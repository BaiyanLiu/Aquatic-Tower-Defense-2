// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDSculpt.h"

#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<float> CVarSculptRockCells(TEXT("atd.SculptRockCells"), 210.0f,
	TEXT("Cells across a sculpt with stone in it (other sculpts use 140)"));
static TAutoConsoleVariable<int32> CVarSculptRock(TEXT("atd.SculptRock"), 1,
	TEXT("Sculpted stone (the Coral Walls' rocks) gets lumps, cracks and chips, on finer cells (0 = the old smooth rock). Takes effect for meshes built afterwards."));

namespace
{
	/** One part as a signed distance (negative inside), in the pivot's space */
	struct FSculptShape
	{
		ETDShape Shape = ETDShape::Sphere;
		FVector3f Location;
		FQuat4f InvRotation;
		FVector3f Half;		// half size
		float Blend = 1.0f;	// how far (uu) it melts into the shapes before it
		float Rock = 0.0f;	// stone parts: how deep (uu) the rock's lumps, cracks and chips go
		FVector4f Color;
		FBox3f Bounds;

		/** Distance with the rock's surface relief, worked out only near the surface where it changes anything */
		float RockDistance(const FVector3f& World, float Shell) const;

		float Distance(const FVector3f& World) const
		{
			const FVector3f P = InvRotation.RotateVector(World - Location);
			switch (Shape)
			{
			case ETDShape::Sphere:
			{
				// ellipsoid (a close approximation of its true distance)
				const FVector3f A(P.X / Half.X, P.Y / Half.Y, P.Z / Half.Z);
				const FVector3f B(A.X / Half.X, A.Y / Half.Y, A.Z / Half.Z);
				const float K0 = A.Size();
				const float K1 = B.Size();
				return K1 < 1e-6f ? -Half.GetMin() : K0 * (K0 - 1.0f) / K1;
			}
			case ETDShape::Cube:
			{
				const float R = FMath::Min(Half.GetMin() * 0.35f, 3.0f);
				const FVector3f Q(FMath::Abs(P.X) - Half.X + R, FMath::Abs(P.Y) - Half.Y + R, FMath::Abs(P.Z) - Half.Z + R);
				const FVector3f Outside(FMath::Max(Q.X, 0.0f), FMath::Max(Q.Y, 0.0f), FMath::Max(Q.Z, 0.0f));
				return Outside.Size() + FMath::Min(FMath::Max3(Q.X, Q.Y, Q.Z), 0.0f) - R;
			}
			case ETDShape::Cylinder:
			{
				const float Mean = (Half.X + Half.Y) * 0.5f;
				const float D0 = FMath::Sqrt(FMath::Square(P.X * Mean / Half.X) + FMath::Square(P.Y * Mean / Half.Y)) - Mean;
				const float D1 = FMath::Abs(P.Z) - Half.Z;
				return FMath::Min(FMath::Max(D0, D1), 0.0f) + FMath::Sqrt(FMath::Square(FMath::Max(D0, 0.0f)) + FMath::Square(FMath::Max(D1, 0.0f)));
			}
			case ETDShape::Cone:
			{
				// capped cone: base radius at -Half.Z, point at +Half.Z
				const float Mean = (Half.X + Half.Y) * 0.5f;
				const float H = Half.Z;
				const float QX = FMath::Sqrt(FMath::Square(P.X * Mean / Half.X) + FMath::Square(P.Y * Mean / Half.Y));
				const float QY = P.Z;
				const float K2X = -Mean, K2Y = 2.0f * H;
				const float CAX = QX - FMath::Min(QX, QY < 0.0f ? Mean : 0.0f);
				const float CAY = FMath::Abs(QY) - H;
				const float T = FMath::Clamp(((0.0f - QX) * K2X + (H - QY) * K2Y) / (K2X * K2X + K2Y * K2Y), 0.0f, 1.0f);
				const float CBX = QX + K2X * T;
				const float CBY = QY - H + K2Y * T;
				const float Sign = (CBX < 0.0f && CAY < 0.0f) ? -1.0f : 1.0f;
				return Sign * FMath::Sqrt(FMath::Min(CAX * CAX + CAY * CAY, CBX * CBX + CBY * CBY));
			}
			default:
				return 1e3f;
			}
		}
	};

	/** Rough reef rock, about -1..1: broad lumps, then sharp creases and chipped edges (thread safe) */
	float RockNoise(const FVector3f& P)
	{
		const FVector Q = FVector(P) * 0.085;
		const float Lumps = FMath::PerlinNoise3D(Q) * 0.55f + FMath::PerlinNoise3D(Q * 2.03 + FVector(5.2, 1.3, 7.7)) * 0.3f
			+ FMath::PerlinNoise3D(Q * 4.4 + FVector(2.1, 9.4, 3.3)) * 0.15f;
		const float Cracks = 0.7f * FMath::Abs(FMath::PerlinNoise3D(Q * 2.6 + FVector(11.0, 3.0, 6.0))) - 0.25f;
		const float Chips = 0.35f * FMath::Abs(FMath::PerlinNoise3D(Q * 7.5 + FVector(4.0, 8.0, 1.0))) - 0.12f;
		return Lumps + Cracks + Chips;
	}

	float FSculptShape::RockDistance(const FVector3f& World, float Shell) const
	{
		const float D = Distance(World);
		return Rock > 0.0f && FMath::Abs(D) < Rock * 2.0f + Shell ? D + Rock * RockNoise(World) : D;
	}

	/** Polynomial smooth minimum: melts two distances together over K units */
	float SmoothMin(float A, float B, float K)
	{
		const float H = FMath::Max(K - FMath::Abs(A - B), 0.0f) / K;
		return FMath::Min(A, B) - H * H * K * 0.25f;
	}
}

bool TDSculpt::Build(const TArray<FTDMergedPart>& Parts, float CellScale, FTDSculptMesh& Out)
{
	TArray<FSculptShape> Shapes;
	FBox3f Bounds(ForceInit);
	float MinDim = TNumericLimits<float>::Max();
	bool bRock = false;
	for (const FTDMergedPart& Part : Parts)
	{
		if (Part.Shape == ETDShape::Plane)
		{
			continue;
		}
		FSculptShape& S = Shapes.AddDefaulted_GetRef();
		S.Shape = Part.Shape;
		S.Location = FVector3f(Part.Location);
		S.InvRotation = FQuat4f(Part.Rotation).Inverse();
		S.Half = FVector3f(FMath::Max(Part.Size.X, 0.1), FMath::Max(Part.Size.Y, 0.1), FMath::Max(Part.Size.Z, 0.1)) * 0.5f;
		S.Blend = FMath::Clamp(S.Half.GetMin() * 1.1f, 0.6f, 4.5f);
		if (Part.Surface == ETDSurface::Stone && CVarSculptRock.GetValueOnAnyThread() != 0)
		{
			S.Rock = FMath::Clamp(S.Half.GetMin() * 0.14f, 0.8f, 2.6f);
			bRock = true;
		}
		S.Color = FVector4f(Part.Color.R, Part.Color.G, Part.Color.B, 1.0f);
		const float Radius = S.Half.GetMax();
		S.Bounds = FBox3f(S.Location - FVector3f(Radius), S.Location + FVector3f(Radius));
		Bounds += S.Bounds;
		MinDim = FMath::Min(MinDim, S.Half.GetMin() * 2.0f);
	}
	if (Shapes.Num() < 2)
	{
		return false;
	}
	Bounds = Bounds.ExpandBy(2.0f);
	const FVector3f Extent = Bounds.GetSize();

	// cells small enough for the thinnest part, but no more than about 140 across the whole pivot
	// (rock needs finer cells, or its cracks and chips smear into soft blobs)
	float Cell = FMath::Clamp(FMath::Min(Extent.GetMax() / (bRock ? CVarSculptRockCells.GetValueOnAnyThread() : 140.0f), MinDim / 3.2f), 0.35f, 1.0f) * CellScale;
	auto CellCount = [&Extent](float C) { return (int64)(Extent.X / C + 2) * (int64)(Extent.Y / C + 2) * (int64)(Extent.Z / C + 2); };
	while (CellCount(Cell) > 4000000)
	{
		Cell *= 1.15f;
	}
	const int32 NX = FMath::CeilToInt(Extent.X / Cell) + 1;
	const int32 NY = FMath::CeilToInt(Extent.Y / Cell) + 1;
	const int32 NZ = FMath::CeilToInt(Extent.Z / Cell) + 1;
	auto Index = [NX, NY](int32 I, int32 J, int32 K) { return (K * NY + J) * NX + I; };
	auto GridPoint = [&Bounds, Cell](float I, float J, float K) { return Bounds.Min + FVector3f(I, J, K) * Cell; };

	// the field: each part melted into the ones before it, only near the part
	TArray<float> Field;
	Field.Init(1e3f, NX * NY * NZ);
	for (const FSculptShape& S : Shapes)
	{
		const FBox3f Near = S.Bounds.ExpandBy(S.Blend + S.Rock + Cell * 2.0f);
		const int32 I0 = FMath::Max(0, FMath::FloorToInt((Near.Min.X - Bounds.Min.X) / Cell)), I1 = FMath::Min(NX - 1, FMath::CeilToInt((Near.Max.X - Bounds.Min.X) / Cell));
		const int32 J0 = FMath::Max(0, FMath::FloorToInt((Near.Min.Y - Bounds.Min.Y) / Cell)), J1 = FMath::Min(NY - 1, FMath::CeilToInt((Near.Max.Y - Bounds.Min.Y) / Cell));
		const int32 K0 = FMath::Max(0, FMath::FloorToInt((Near.Min.Z - Bounds.Min.Z) / Cell)), K1 = FMath::Min(NZ - 1, FMath::CeilToInt((Near.Max.Z - Bounds.Min.Z) / Cell));
		for (int32 K = K0; K <= K1; ++K)
		{
			for (int32 J = J0; J <= J1; ++J)
			{
				for (int32 I = I0; I <= I1; ++I)
				{
					float& F = Field[Index(I, J, K)];
					F = SmoothMin(F, S.RockDistance(GridPoint(I, J, K), S.Blend + Cell * 2.0f), S.Blend);
				}
			}
		}
	}

	// surface nets: one vertex per cell the surface passes through, at the mean of its edge crossings
	TArray<int32> VertexOfCell;
	VertexOfCell.Init(INDEX_NONE, NX * NY * NZ);
	static const int32 Edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
	for (int32 K = 0; K < NZ - 1; ++K)
	{
		for (int32 J = 0; J < NY - 1; ++J)
		{
			for (int32 I = 0; I < NX - 1; ++I)
			{
				float V[8];
				int32 Inside = 0;
				for (int32 n = 0; n < 8; ++n)
				{
					V[n] = Field[Index(I + (n & 1), J + ((n >> 1) & 1), K + ((n >> 2) & 1))];
					Inside += V[n] < 0.0f ? 1 : 0;
				}
				if (Inside == 0 || Inside == 8)
				{
					continue;
				}
				FVector3f Sum(0.0f);
				int32 Crossings = 0;
				for (const auto& E : Edges)
				{
					const float A = V[E[0]], B = V[E[1]];
					if ((A < 0.0f) == (B < 0.0f))
					{
						continue;
					}
					const float T = A / (A - B);
					const FVector3f PA((float)(E[0] & 1), (float)((E[0] >> 1) & 1), (float)((E[0] >> 2) & 1));
					const FVector3f PB((float)(E[1] & 1), (float)((E[1] >> 1) & 1), (float)((E[1] >> 2) & 1));
					Sum += PA + (PB - PA) * T;
					++Crossings;
				}
				const FVector3f Local = Sum / (float)Crossings;
				VertexOfCell[Index(I, J, K)] = Out.Positions.Num();
				Out.Positions.Add(GridPoint(I + Local.X, J + Local.Y, K + Local.Z));
				// normal from the field's gradient across the cell
				const FVector3f Gradient(
					(V[1] + V[3] + V[5] + V[7]) - (V[0] + V[2] + V[4] + V[6]),
					(V[2] + V[3] + V[6] + V[7]) - (V[0] + V[1] + V[4] + V[5]),
					(V[4] + V[5] + V[6] + V[7]) - (V[0] + V[1] + V[2] + V[3]));
				Out.Normals.Add(Gradient.GetSafeNormal());
			}
		}
	}

	// a quad around every grid edge the surface crosses, wound to face along the vertex normals
	auto AddTriangle = [&Out](int32 A, int32 B, int32 C)
	{
		const FVector3f Face = FVector3f::CrossProduct(Out.Positions[C] - Out.Positions[A], Out.Positions[B] - Out.Positions[A]);
		const bool bFacing = FVector3f::DotProduct(Face, Out.Normals[A] + Out.Normals[B] + Out.Normals[C]) >= 0.0f;
		Out.Triangles.Append({ A, bFacing ? B : C, bFacing ? C : B });
	};
	auto AddQuad = [&](int32 A, int32 B, int32 C, int32 D)
	{
		const int32 VA = VertexOfCell[A], VB = VertexOfCell[B], VC = VertexOfCell[C], VD = VertexOfCell[D];
		if (VA == INDEX_NONE || VB == INDEX_NONE || VC == INDEX_NONE || VD == INDEX_NONE)
		{
			return;
		}
		AddTriangle(VA, VB, VC);
		AddTriangle(VA, VC, VD);
	};
	for (int32 K = 1; K < NZ - 1; ++K)
	{
		for (int32 J = 1; J < NY - 1; ++J)
		{
			for (int32 I = 1; I < NX - 1; ++I)
			{
				const bool bIn = Field[Index(I, J, K)] < 0.0f;
				if ((Field[Index(I + 1, J, K)] < 0.0f) != bIn)
				{
					AddQuad(Index(I, J - 1, K - 1), Index(I, J, K - 1), Index(I, J, K), Index(I, J - 1, K));
				}
				if ((Field[Index(I, J + 1, K)] < 0.0f) != bIn)
				{
					AddQuad(Index(I - 1, J, K - 1), Index(I - 1, J, K), Index(I, J, K), Index(I, J, K - 1));
				}
				if ((Field[Index(I, J, K + 1)] < 0.0f) != bIn)
				{
					AddQuad(Index(I - 1, J - 1, K), Index(I, J - 1, K), Index(I, J, K), Index(I - 1, J, K));
				}
			}
		}
	}
	if (Out.Triangles.Num() == 0)
	{
		return false;
	}

	// colours: a soft blend of the nearest parts' colours, so painted areas meet in a narrow gradient
	Out.Colors.SetNumUninitialized(Out.Positions.Num());
	for (int32 v = 0; v < Out.Positions.Num(); ++v)
	{
		const FVector3f& P = Out.Positions[v];
		FVector4f Sum(0.0f);
		float Weights = 0.0f;
		float Best = TNumericLimits<float>::Max();
		FVector4f Nearest(1.0f);
		float NearestRock = 0.0f;
		for (const FSculptShape& S : Shapes)
		{
			if (S.Bounds.ComputeSquaredDistanceToPoint(P) > FMath::Square(S.Blend + 3.0f))
			{
				continue;
			}
			const float D = S.Distance(P);
			if (D < Best)
			{
				Best = D;
				Nearest = S.Color;
				NearestRock = S.Rock;
			}
			const float W = FMath::Exp(-FMath::Max(D, 0.0f) * 3.0f - FMath::Max(-D, 0.0f) * 0.2f);
			if (W > 1e-4f)
			{
				Sum += S.Color * W;
				Weights += W;
			}
		}
		FVector4f C = Weights > 0.0f ? Sum / Weights : Nearest;
		if (NearestRock > 0.0f)
		{
			// rock: lighter on its lumps and chipped edges, darker down in its cracks
			const float Shade = FMath::Clamp(1.0f - 0.32f * RockNoise(P), 0.55f, 1.35f);
			C = FVector4f(C.X * Shade, C.Y * Shade, C.Z * Shade, C.W);
		}
		// the mesh build sRGB-encodes vertex colours but the material reads them raw: decode first so they arrive linear
		// (the exact sRGB curve, not a 2.2 power: the build encodes with the exact curve, and a 2.2 power darkens the darker channels)
		auto Decode = [](float X) { X = FMath::Clamp(X, 0.0f, 1.0f); return X <= 0.04045f ? X / 12.92f : FMath::Pow((X + 0.055f) / 1.055f, 2.4f); };
		Out.Colors[v] = FVector4f(Decode(C.X), Decode(C.Y), Decode(C.Z), 1.0f);
	}
	return true;
}
