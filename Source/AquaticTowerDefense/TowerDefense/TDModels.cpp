// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDModels.h"
#include "AquaticTowerDefense.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "HAL/IConsoleManager.h"
#include "Async/ParallelFor.h"
#include "Misc/ScopeLock.h"
#include "TDModelDetail.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Compression.h"
#include "TDSculpt.h"
#include "Tasks/Task.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Misc/CoreDelegates.h"
#include <atomic>

namespace
{
	/** A basic shape of the same size and pivot as the engine's /Engine/BasicShapes meshes (100 units across) */
	struct FUnitShape
	{
		TArray<FVector3f> Positions;
		TArray<FVector3f> Normals;
		TArray<int32> Triangles;

		int32 AddVertex(const FVector3f& P, const FVector3f& N)
		{
			Positions.Add(P);
			Normals.Add(N);
			return Positions.Num() - 1;
		}

		/** Adds a triangle wound so that it faces along its vertex normals */
		void AddTriangle(int32 A, int32 B, int32 C)
		{
			const FVector3f Face = FVector3f::CrossProduct(Positions[C] - Positions[A], Positions[B] - Positions[A]);
			const bool bFacing = FVector3f::DotProduct(Face, Normals[A] + Normals[B] + Normals[C]) >= 0.0f;
			Triangles.Append({ A, bFacing ? B : C, bFacing ? C : B });
		}

		void AddQuad(int32 A, int32 B, int32 C, int32 D)
		{
			AddTriangle(A, B, C);
			AddTriangle(A, C, D);
		}

		/** A flat disc at height Z, facing up or down */
		void AddCap(int32 Segments, float Z, float NormalZ)
		{
			const FVector3f N(0.0f, 0.0f, NormalZ);
			const int32 Center = AddVertex(FVector3f(0.0f, 0.0f, Z), N);
			const int32 First = Positions.Num();
			for (int32 i = 0; i < Segments; ++i)
			{
				const float A = 2.0f * PI * i / Segments;
				AddVertex(FVector3f(FMath::Cos(A) * 50.0f, FMath::Sin(A) * 50.0f, Z), N);
			}
			for (int32 i = 0; i < Segments; ++i)
			{
				AddTriangle(Center, First + i, First + (i + 1) % Segments);
			}
		}
	};

	FUnitShape MakeUnitShape(ETDShape Shape, int32 Segments)
	{
		FUnitShape Out;
		switch (Shape)
		{
		case ETDShape::Sphere:
		{
			// as many rings as half the sides, so the faces come out about square (more rings add triangles, not a rounder outline)
			const int32 Rings = FMath::Max(4, (Segments + 1) / 2);
			for (int32 R = 0; R <= Rings; ++R)
			{
				const float Polar = PI * R / Rings;
				for (int32 i = 0; i <= Segments; ++i)
				{
					const float A = 2.0f * PI * i / Segments;
					const FVector3f N(FMath::Sin(Polar) * FMath::Cos(A), FMath::Sin(Polar) * FMath::Sin(A), FMath::Cos(Polar));
					Out.AddVertex(N * 50.0f, N);
				}
			}
			for (int32 R = 0; R < Rings; ++R)
			{
				for (int32 i = 0; i < Segments; ++i)
				{
					const int32 A = R * (Segments + 1) + i;
					const int32 B = A + Segments + 1;
					if (R > 0)
					{
						Out.AddTriangle(A, A + 1, B);
					}
					if (R < Rings - 1)
					{
						Out.AddTriangle(A + 1, B + 1, B);
					}
				}
			}
			break;
		}
		case ETDShape::Cylinder:
		{
			for (int32 i = 0; i <= Segments; ++i)
			{
				const float A = 2.0f * PI * i / Segments;
				const FVector3f N(FMath::Cos(A), FMath::Sin(A), 0.0f);
				Out.AddVertex(N * 50.0f + FVector3f(0.0f, 0.0f, 50.0f), N);
				Out.AddVertex(N * 50.0f - FVector3f(0.0f, 0.0f, 50.0f), N);
			}
			for (int32 i = 0; i < Segments; ++i)
			{
				Out.AddQuad(i * 2, i * 2 + 1, i * 2 + 3, i * 2 + 2);
			}
			Out.AddCap(Segments, 50.0f, 1.0f);
			Out.AddCap(Segments, -50.0f, -1.0f);
			break;
		}
		case ETDShape::Cone:
		{
			// the point at the top; side normals lean up by the slope of a cone as wide as it is tall
			for (int32 i = 0; i <= Segments; ++i)
			{
				const float A = 2.0f * PI * i / Segments;
				const float Mid = 2.0f * PI * (i + 0.5f) / Segments;
				const FVector3f Rim(FMath::Cos(A), FMath::Sin(A), 0.0f);
				const FVector3f TipDir(FMath::Cos(Mid), FMath::Sin(Mid), 0.0f);
				Out.AddVertex(Rim * 50.0f - FVector3f(0.0f, 0.0f, 50.0f), (Rim * 2.0f + FVector3f(0.0f, 0.0f, 1.0f)).GetSafeNormal());
				Out.AddVertex(FVector3f(0.0f, 0.0f, 50.0f), (TipDir * 2.0f + FVector3f(0.0f, 0.0f, 1.0f)).GetSafeNormal());
			}
			for (int32 i = 0; i < Segments; ++i)
			{
				Out.AddTriangle(i * 2, i * 2 + 2, i * 2 + 1);
			}
			Out.AddCap(Segments, -50.0f, -1.0f);
			break;
		}
		case ETDShape::Plane:
		{
			const FVector3f N(0.0f, 0.0f, 1.0f);
			const int32 A = Out.AddVertex(FVector3f(-50.0f, -50.0f, 0.0f), N);
			const int32 B = Out.AddVertex(FVector3f(50.0f, -50.0f, 0.0f), N);
			const int32 C = Out.AddVertex(FVector3f(50.0f, 50.0f, 0.0f), N);
			const int32 D = Out.AddVertex(FVector3f(-50.0f, 50.0f, 0.0f), N);
			Out.AddQuad(A, B, C, D);
			break;
		}
		default:
		{
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				for (float Sign : { 1.0f, -1.0f })
				{
					FVector3f N(0.0f), U(0.0f), V(0.0f);
					N[Axis] = Sign;
					U[(Axis + 1) % 3] = 50.0f;
					V[(Axis + 2) % 3] = 50.0f;
					const FVector3f C = N * 50.0f;
					const int32 A0 = Out.AddVertex(C - U - V, N);
					const int32 A1 = Out.AddVertex(C + U - V, N);
					const int32 A2 = Out.AddVertex(C + U + V, N);
					const int32 A3 = Out.AddVertex(C - U + V, N);
					Out.AddQuad(A0, A1, A2, A3);
				}
			}
			break;
		}
		}
		return Out;
	}

	/** Only as many sides as a shape of this size shows on screen: the engine's shapes spend the same triangles on a barnacle as on a boulder */
	int32 SegmentsForSize(float Size)
	{
		if (Size < 8.0f)
		{
			return 8;
		}
		if (Size < 24.0f)
		{
			return 12;
		}
		return Size < 60.0f ? 16 : 24;
	}

	/** Shared unit shapes; safe to call from the threads that prepare merged meshes (each shape is allocated once and never moves) */
	const FUnitShape& GetUnitShape(ETDShape Shape, int32 Segments)
	{
		static FCriticalSection Lock;
		static TMap<uint32, TUniquePtr<FUnitShape>> Shapes;
		const uint32 Key = ((uint32)Shape << 8) | (uint32)Segments;
		FScopeLock Scope(&Lock);
		if (const TUniquePtr<FUnitShape>* Found = Shapes.Find(Key))
		{
			return **Found;
		}
		return *Shapes.Add(Key, MakeUnique<FUnitShape>(MakeUnitShape(Shape, Segments)));
	}

	/** Merged meshes get coarser levels of detail for zoomed-out views. Each level keeps every part and only cuts the sides of round
	 *  shapes, so far that its outline is never further off (in pixels) than the full mesh is at the closest zoom. */
	constexpr int32 MaxMergedLods = 3;
	constexpr float LodSegmentScale[MaxMergedLods] = { 1.0f, 0.8f, 0.63f };
	/** Camera ortho width beyond which each level shows on a 2.4:1 screen (the default view is 1300 wide, the closest 800).
	 *  The engine measures screen size against the view height, so other screen shapes switch at the same pixel density per screen height. */
	constexpr float LodSwitchOrthoWidth[MaxMergedLods] = { 0.0f, 1250.0f, 2000.0f };
	constexpr float LodSwitchAspect = 2.4f;

	/** Sides of a round shape at a level of detail: the coarsest shapes (8 sides) stay as they are, the others drop to an even count */
	int32 ScaleSegments(int32 Segments, float Scale)
	{
		return Segments <= 8 || Scale >= 1.0f ? Segments : FMath::Max(8, 2 * FMath::RoundToInt(Segments * Scale * 0.5f));
	}

	/** Sides of a round shape in the detailed models, from the width of its round section */
	int32 DetailedSegments(float Across)
	{
		if (Across < 4.0f)
		{
			return 12;
		}
		if (Across < 10.0f)
		{
			return 18;
		}
		if (Across < 24.0f)
		{
			return 28;
		}
		return Across < 60.0f ? 40 : 56;
	}

	/** A box of half size H with edges rounded to radius R, made at its final size so every edge gets the same rounding.
	 *  Fine boxes round each edge in four steps, the others in two; normals are smooth across the rounding. */
	FUnitShape MakeRoundedBox(const FVector3f& H, float R, bool bFine)
	{
		FUnitShape Out;
		const FVector3f Inner = H - FVector3f(R);
		const float Step = R * 0.41421356f;	// tan(22.5 degrees): the middle row of a fine edge sits at 22.5 degrees
		auto Coords = [&](int32 Axis)
		{
			TArray<float, TInlineAllocator<6>> C;
			if (bFine)
			{
				C = { -H[Axis], -Inner[Axis] - Step, -Inner[Axis], Inner[Axis], Inner[Axis] + Step, H[Axis] };
			}
			else
			{
				C = { -H[Axis], -Inner[Axis], Inner[Axis], H[Axis] };
			}
			return C;
		};
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const int32 U = (Axis + 1) % 3;
			const int32 V = (Axis + 2) % 3;
			const auto CU = Coords(U);
			const auto CV = Coords(V);
			for (float Sign : { 1.0f, -1.0f })
			{
				const int32 First = Out.Positions.Num();
				for (int32 i = 0; i < CU.Num(); ++i)
				{
					for (int32 j = 0; j < CV.Num(); ++j)
					{
						FVector3f P;
						P[Axis] = Sign * H[Axis];
						P[U] = CU[i];
						P[V] = CV[j];
						const FVector3f Core(FMath::Clamp(P.X, -Inner.X, Inner.X), FMath::Clamp(P.Y, -Inner.Y, Inner.Y), FMath::Clamp(P.Z, -Inner.Z, Inner.Z));
						const FVector3f N = (P - Core).GetSafeNormal();
						Out.AddVertex(Core + N * R, N);
					}
				}
				const int32 Row = CV.Num();
				for (int32 i = 0; i + 1 < CU.Num(); ++i)
				{
					for (int32 j = 0; j + 1 < CV.Num(); ++j)
					{
						const int32 A = First + i * Row + j;
						Out.AddQuad(A, A + Row, A + Row + 1, A + 1);
					}
				}
			}
		}
		return Out;
	}

	/** A cylinder (round or oval) of radii RX, RY and half height HZ whose rims are rounded to radius R, at its final size */
	FUnitShape MakeRoundedCylinder(float RX, float RY, float HZ, float R, int32 Segments)
	{
		FUnitShape Out;
		// the profile from the bottom centre's rim up to the top's: (angle of the normal above the horizontal, height of the rounding centre)
		struct FRing { float Angle; float Z; };
		const float Low = -HZ + R;
		const float High = HZ - R;
		const FRing Rings[] = {
			{ -90.0f, Low }, { -60.0f, Low }, { -30.0f, Low }, { 0.0f, Low },
			{ 0.0f, High }, { 30.0f, High }, { 60.0f, High }, { 90.0f, High } };
		const int32 NumRings = UE_ARRAY_COUNT(Rings);
		for (const FRing& Ring : Rings)
		{
			const float Up = FMath::DegreesToRadians(Ring.Angle);
			const float Out_ = R * FMath::Cos(Up) - R;	// how far inside the outer wall this ring sits
			for (int32 i = 0; i <= Segments; ++i)
			{
				const float A = 2.0f * PI * i / Segments;
				const float C = FMath::Cos(A);
				const float S = FMath::Sin(A);
				const FVector3f Across = FVector3f(C / FMath::Max(RX, 1e-3f), S / FMath::Max(RY, 1e-3f), 0.0f).GetSafeNormal();
				const FVector3f P(C * (RX + Out_), S * (RY + Out_), Ring.Z + R * FMath::Sin(Up));
				const FVector3f N = Across * FMath::Cos(Up) + FVector3f(0.0f, 0.0f, FMath::Sin(Up));
				Out.AddVertex(P, N.GetSafeNormal());
			}
		}
		const int32 Row = Segments + 1;
		for (int32 r = 0; r + 1 < NumRings; ++r)
		{
			for (int32 i = 0; i < Segments; ++i)
			{
				const int32 A = r * Row + i;
				Out.AddQuad(A, A + 1, A + Row + 1, A + Row);
			}
		}
		// flat caps fanned from the rounded rims
		for (const bool bTop : { false, true })
		{
			const FVector3f N(0.0f, 0.0f, bTop ? 1.0f : -1.0f);
			const int32 Center = Out.AddVertex(FVector3f(0.0f, 0.0f, bTop ? HZ : -HZ), N);
			const int32 RimRow = bTop ? NumRings - 1 : 0;
			for (int32 i = 0; i < Segments; ++i)
			{
				Out.AddTriangle(Center, RimRow * Row + i, RimRow * Row + i + 1);
			}
		}
		return Out;
	}

	/** A round stone at its final size: a sphere with a lumpy, chiselled outline. Normals come from the displaced surface itself. */
	FUnitShape MakeLumpySphere(const FVector3f& Size, int32 Segments, const FVector3f& Seed)
	{
		FUnitShape Out;
		const FVector3f Half = Size * 0.5f;
		const float Amp = FMath::Min(0.075f, 0.075f * 26.0f / FMath::Max(Size.GetMax(), 1.0f) + 0.02f);
		auto Surface = [&](const FVector3f& Dir)
		{
			const FVector P = FVector(Dir) * 2.4 + FVector(Seed);
			const float Lumps = FMath::PerlinNoise3D(P) * 0.6f + FMath::PerlinNoise3D(P * 2.03) * 0.3f + FMath::PerlinNoise3D(P * 4.1) * 0.15f;
			const float Chips = 0.6f * FMath::Abs(FMath::PerlinNoise3D(FVector(Dir) * 6.0 + FVector(Seed) * 1.7)) - 0.3f;
			return Dir * Half * (1.0f + Amp * (Lumps + Chips));
		};
		const int32 Rings = FMath::Max(4, (Segments + 1) / 2);
		for (int32 R = 0; R <= Rings; ++R)
		{
			const float Polar = PI * R / Rings;
			for (int32 i = 0; i <= Segments; ++i)
			{
				const float A = 2.0f * PI * i / Segments;
				const FVector3f Dir(FMath::Sin(Polar) * FMath::Cos(A), FMath::Sin(Polar) * FMath::Sin(A), FMath::Cos(Polar));
				// normal from two nearby points on the surface
				const FVector3f T1 = FVector3f(-FMath::Sin(A), FMath::Cos(A), 0.0f);
				const FVector3f T2 = FVector3f::CrossProduct(Dir, T1).GetSafeNormal();
				const float E = 0.02f;
				const FVector3f P0 = Surface(Dir);
				const FVector3f P1 = Surface((Dir + T1 * E).GetSafeNormal());
				const FVector3f P2 = Surface((Dir + T2 * E).GetSafeNormal());
				FVector3f N = FVector3f::CrossProduct(P1 - P0, P2 - P0).GetSafeNormal();
				if (FVector3f::DotProduct(N, Dir) < 0.0f)
				{
					N = -N;
				}
				if (N.IsNearlyZero() || R == 0 || R == Rings)
				{
					N = Dir;
				}
				Out.AddVertex(P0, N);
			}
		}
		for (int32 R = 0; R < Rings; ++R)
		{
			for (int32 i = 0; i < Segments; ++i)
			{
				const int32 A = R * (Segments + 1) + i;
				const int32 B = A + Segments + 1;
				if (R > 0)
				{
					Out.AddTriangle(A, A + 1, B);
				}
				if (R < Rings - 1)
				{
					Out.AddTriangle(A + 1, B + 1, B);
				}
			}
		}
		return Out;
	}

	/** Whether M_ATD_Detail loaded (checked on the game thread before any mesh is prepared): without it the detailed models
	 *  keep the plain material and skip the sculpted bodies */
	std::atomic<int32> GDetailMaterialState{ -1 };

	bool HasDetailMaterial()
	{
		return GDetailMaterialState.load() == 1;
	}

	/** The detailed version of a part, at its final size (before rotation), or false when the plain unit shape is used scaled */
	bool MakeDetailedShape(const FTDMergedPart& Part, FUnitShape& Out, int32& OutSegments, float SegmentScale)
	{
		const FVector3f Size(Part.Size);
		switch (Part.Shape)
		{
		case ETDShape::Cube:
		{
			const float MinSide = Size.GetMin();
			const float R = FMath::Min(MinSide * 0.3f, 6.0f);
			if (R < 0.25f)
			{
				return false;
			}
			Out = MakeRoundedBox(Size * 0.5f, R, R >= 1.5f);
			return true;
		}
		case ETDShape::Cylinder:
		{
			const float Across = FMath::Min(Size.X, Size.Y);
			const float R = FMath::Min3(Across * 0.22f, Size.Z * 0.45f, 5.0f);
			OutSegments = ScaleSegments(DetailedSegments(FMath::Max(Size.X, Size.Y)), SegmentScale);
			if (R < 0.25f)
			{
				return false;
			}
			Out = MakeRoundedCylinder(Size.X * 0.5f, Size.Y * 0.5f, Size.Z * 0.5f, R, OutSegments);
			return true;
		}
		case ETDShape::Cone:
			OutSegments = ScaleSegments(DetailedSegments(FMath::Max(Size.X, Size.Y)), SegmentScale);
			return false;
		case ETDShape::Sphere:
			OutSegments = ScaleSegments(DetailedSegments(Size.GetMax()), SegmentScale);
			if (Part.Surface == ETDSurface::Stone && Size.GetMin() > 5.0f && HasDetailMaterial())
			{
				const FVector3f Seed = FVector3f(Part.Location) * 0.037f + FVector3f(Size.X * 0.011f, 3.1f, 7.7f);
				Out = MakeLumpySphere(Size, FMath::Max(OutSegments, 16), Seed);
				return true;
			}
			return false;
		default:
			return false;
		}
	}

	UMaterialInterface* SlotMaterial(const FLinearColor& Color, ETDPartMaterial Mat, float Opacity, ETDSurface Surface = ETDSurface::Auto)
	{
		if (Mat == ETDPartMaterial::Lit && Surface != ETDSurface::Auto)
		{
			if (UMaterialInterface* Detail = TD::SharedDetailMaterial(FLinearColor(Color.R, Color.G, Color.B, 1.0f), Surface))
			{
				return Detail;
			}
		}
		return Mat == ETDPartMaterial::Lit ? TD::SharedColorMaterial(FLinearColor(Color.R, Color.G, Color.B, 1.0f))
			: TD::SharedFxMaterial(FLinearColor(Color.R, Color.G, Color.B, 1.0f), Mat == ETDPartMaterial::Glass ? Opacity : 1.0f);
	}

	/** Adds a sculpted surface (TDSculpt) to a mesh description, already in the pivot's space, with its vertex colours */
	void AppendSculpt(FMeshDescription& Description, FStaticMeshAttributes& Attributes, FPolygonGroupID Group, const FTDSculptMesh& Sculpt, TArray<FVertexInstanceID>& Instances)
	{
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
		Instances.Reset(Sculpt.Positions.Num());
		for (int32 i = 0; i < Sculpt.Positions.Num(); ++i)
		{
			const FVertexID Vertex = Description.CreateVertex();
			Positions[Vertex] = Sculpt.Positions[i];
			const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
			const FVector3f Normal = Sculpt.Normals[i];
			const FVector3f Across = FMath::Abs(Normal.Z) < 0.9f ? FVector3f(0.0f, 0.0f, 1.0f) : FVector3f(1.0f, 0.0f, 0.0f);
			Normals[Instance] = Normal;
			Tangents[Instance] = FVector3f::CrossProduct(Across, Normal).GetSafeNormal();
			BinormalSigns[Instance] = 1.0f;
			Colors[Instance] = Sculpt.Colors[i];
			UVs.Set(Instance, 0, FVector2f::ZeroVector);
			Instances.Add(Instance);
		}
		for (int32 i = 0; i + 2 < Sculpt.Triangles.Num(); i += 3)
		{
			const FVertexInstanceID Corners[] = { Instances[Sculpt.Triangles[i]], Instances[Sculpt.Triangles[i + 1]], Instances[Sculpt.Triangles[i + 2]] };
			Description.CreateTriangle(Group, Corners);
		}
	}

	/** Adds one shape to a mesh description: Shape scaled by Scale, then placed by the part's rotation and location */
	void AppendShape(FMeshDescription& Description, FStaticMeshAttributes& Attributes, FPolygonGroupID Group, const FUnitShape& Shape, const FVector3f& Scale,
		const FTDMergedPart& Part, TArray<FVertexInstanceID>& Instances)
	{
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
		TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
		const FQuat4f Rotation(Part.Rotation);
		const FVector3f Location(Part.Location);
		Instances.Reset(Shape.Positions.Num());
		for (int32 i = 0; i < Shape.Positions.Num(); ++i)
		{
			const FVertexID Vertex = Description.CreateVertex();
			Positions[Vertex] = Location + Rotation.RotateVector(Shape.Positions[i] * Scale);
			const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
			// normals on a stretched shape follow the inverse of the stretch
			const FVector3f Stretched(Shape.Normals[i].X / FMath::Max(Scale.X, 1e-4f), Shape.Normals[i].Y / FMath::Max(Scale.Y, 1e-4f), Shape.Normals[i].Z / FMath::Max(Scale.Z, 1e-4f));
			const FVector3f Normal = Rotation.RotateVector(Stretched.GetSafeNormal());
			const FVector3f Across = FMath::Abs(Normal.Z) < 0.9f ? FVector3f(0.0f, 0.0f, 1.0f) : FVector3f(1.0f, 0.0f, 0.0f);
			Normals[Instance] = Normal;
			Tangents[Instance] = FVector3f::CrossProduct(Across, Normal).GetSafeNormal();
			BinormalSigns[Instance] = 1.0f;
			Colors[Instance] = FVector4f(1.0f, 1.0f, 1.0f, 1.0f);
			UVs.Set(Instance, 0, FVector2f::ZeroVector);
			Instances.Add(Instance);
		}
		for (int32 i = 0; i + 2 < Shape.Triangles.Num(); i += 3)
		{
			const FVertexInstanceID Corners[] = { Instances[Shape.Triangles[i]], Instances[Shape.Triangles[i + 1]], Instances[Shape.Triangles[i + 2]] };
			Description.CreateTriangle(Group, Corners);
		}
	}

	UStaticMesh* NewRuntimeMesh()
	{
		UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage());
		Mesh->bSupportRayTracing = false;
		return Mesh;
	}

	/** The levels switch by zoom (see LodSwitchOrthoWidth): with an ortho view the engine compares the bounds radius times
	 *  aspect / ortho width against half of each level's screen size */
	void SetLodScreenSizes(UStaticMesh* Mesh, int32 NumLods)
	{
		if (FStaticMeshRenderData* RenderData = Mesh->GetRenderData())
		{
			const float Radius = RenderData->Bounds.SphereRadius;
			for (int32 Lod = 1; Lod < NumLods && Lod < MaxMergedLods; ++Lod)
			{
				const float Wanted = 2.0f * Radius * LodSwitchAspect / LodSwitchOrthoWidth[Lod];
				RenderData->ScreenSize[Lod].Default = FMath::Min(Wanted, RenderData->ScreenSize[Lod - 1].Default * 0.9f);
			}
		}
	}

	void FinishRuntimeMesh(UStaticMesh* Mesh, const TArray<const FMeshDescription*>& Descriptions)
	{
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions(Descriptions, Params);
		Mesh->AddToRoot();
		SetLodScreenSizes(Mesh, Descriptions.Num());
	}

	/** The shadow copy of a merged mesh: one section, the plainer sides of SegmentsForSize and the coarse rounding on detailed boxes and
	 *  cylinders. Every shape stays inside the full one's surface, so the full mesh never falls into its copy's shadow. */
	void PrepareShadowDescription(const TArray<FTDMergedPart>& Parts, FMeshDescription& Description)
	{
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();
		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Shadow");

		TArray<FVertexInstanceID> Instances;
		for (const FTDMergedPart& Part : Parts)
		{
			const float Largest = Part.Size.GetMax();
			const int32 Segments = Part.Segments > 0 ? FMath::Min(Part.Segments, SegmentsForSize(Largest)) : SegmentsForSize(Largest);
			FUnitShape Own;
			bool bOwnShape = false;
			if (Part.bDetailed && Part.Shape == ETDShape::Cube)
			{
				const float R = FMath::Min((float)Part.Size.GetMin() * 0.3f, 6.0f);
				if (R >= 0.25f)
				{
					Own = MakeRoundedBox(FVector3f(Part.Size) * 0.5f, R, false);
					bOwnShape = true;
				}
			}
			else if (Part.bDetailed && Part.Shape == ETDShape::Cylinder)
			{
				const FVector3f Size(Part.Size);
				const float R = FMath::Min3(FMath::Min(Size.X, Size.Y) * 0.22f, Size.Z * 0.45f, 5.0f);
				if (R >= 0.25f)
				{
					Own = MakeRoundedCylinder(Size.X * 0.5f, Size.Y * 0.5f, Size.Z * 0.5f, R, Segments);
					bOwnShape = true;
				}
			}
			const FUnitShape& Shape = bOwnShape ? Own : GetUnitShape(Part.Shape, Segments);
			// lumpy stones dip up to a few percent inside their sphere: the copy stays inside them
			const float Shrink = Part.Surface == ETDSurface::Stone && Part.Shape == ETDShape::Sphere ? 0.92f : 1.0f;
			AppendShape(Description, Attributes, Group, Shape, bOwnShape ? FVector3f(1.0f) : FVector3f(Part.Size / 100.0f) * Shrink, Part, Instances);
		}
	}

	UStaticMesh* NewShadowMesh()
	{
		UStaticMesh* Mesh = NewRuntimeMesh();
		Mesh->GetStaticMaterials().Add(FStaticMaterial(TD::SharedColorMaterial(FLinearColor(0.5f, 0.5f, 0.5f)), TEXT("Shadow")));
		return Mesh;
	}

	UStaticMesh* BuildShadowMesh(const TArray<FTDMergedPart>& Parts)
	{
		FMeshDescription Description;
		PrepareShadowDescription(Parts, Description);
		UStaticMesh* Mesh = NewShadowMesh();
		FinishRuntimeMesh(Mesh, { &Description });
		return Mesh;
	}

	/** A merged part's shape at a level of detail (scaled by OutScale), or the plain unit shape scaled to the part's size */
	const FUnitShape& PartShape(const FTDMergedPart& Part, int32 Lod, FUnitShape& Own, FVector3f& OutScale)
	{
		// detailed parts with rounded edges are made at their final size; the rest scale a shared unit shape
		const float SegmentScale = LodSegmentScale[Lod];
		int32 Segments = Part.Segments > 0 ? Part.Segments : SegmentsForSize(Part.Size.GetMax());
		if (Part.bDetailed && MakeDetailedShape(Part, Own, Segments, SegmentScale))
		{
			OutScale = FVector3f(1.0f);
			return Own;
		}
		if (!Part.bDetailed)	// detailed round shapes got their level's sides from MakeDetailedShape
		{
			Segments = ScaleSegments(Segments, SegmentScale);
		}
		OutScale = FVector3f(Part.Size / 100.0f);
		return GetUnitShape(Part.Shape, Segments);
	}

	/** Whether a coarser level would change any part (only round shapes with more than 8 sides do) */
	bool NeedsLods(const TArray<FTDMergedPart>& Parts)
	{
		for (const FTDMergedPart& Part : Parts)
		{
			if (Part.Shape == ETDShape::Sphere || Part.Shape == ETDShape::Cylinder || Part.Shape == ETDShape::Cone)
			{
				const int32 Segments = Part.Segments > 0 ? Part.Segments : (Part.bDetailed ? 9 : SegmentsForSize(Part.Size.GetMax()));
				if (Segments > 8)
				{
					return true;
				}
			}
		}
		return false;
	}

	/** The geometry of a merged mesh, made without touching any UObject so that many can be prepared at once on worker threads */
	struct FPreparedMesh
	{
		int32 NumLods = 0;
		FMeshDescription Descriptions[MaxMergedLods];
		TArray<FTDMergedMesh::FSlot> Slots;
		TArray<FTDMergedPart> ShadowParts;
		/** The sculpted models bring their shadow copy as a finished mesh (a coarse level shrunk a little inside the surface) */
		bool bHasShadowDescription = false;
		FMeshDescription ShadowDescription;
		/** The meshes' vertex and index buffers, when PrebuildRenderData built them on the worker thread too (null otherwise) */
		TUniquePtr<FStaticMeshRenderData> RenderData;
		FBoxSphereBounds Bounds;
		TUniquePtr<FStaticMeshRenderData> ShadowRenderData;
		FBoxSphereBounds ShadowBounds;
	};

	/** Empty meshes with FinishMergedMesh's slot names ("Slot0", "Slot1", ...) and the shadow copy's ("Shadow"), so worker threads can build
	 *  render data with UStaticMesh::BuildFromMeshDescription, which only reads the mesh's material slots to number the sections.
	 *  Made on the game thread (MakeSlotTemplates) and never changed. */
	constexpr int32 TemplateSlots = 512;
	std::atomic<const UStaticMesh*> GSlotTemplate{ nullptr };
	std::atomic<const UStaticMesh*> GShadowTemplate{ nullptr };

	void MakeSlotTemplates()
	{
		check(IsInGameThread());
		if (GSlotTemplate.load())
		{
			return;
		}
		UStaticMesh* Shadow = NewObject<UStaticMesh>(GetTransientPackage());
		Shadow->AddToRoot();
		Shadow->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Shadow")));
		GShadowTemplate.store(Shadow);
		UStaticMesh* Slots = NewObject<UStaticMesh>(GetTransientPackage());
		Slots->AddToRoot();
		for (int32 i = 0; i < TemplateSlots; ++i)
		{
			Slots->GetStaticMaterials().Add(FStaticMaterial(nullptr, FName(*FString::Printf(TEXT("Slot%d"), i))));
		}
		GSlotTemplate.store(Slots);
	}

	TUniquePtr<FStaticMeshRenderData> BuildRenderData(const UStaticMesh* Template, const FMeshDescription* Descriptions, int32 NumLods, FBoxSphereBounds& OutBounds)
	{
		TUniquePtr<FStaticMeshRenderData> RenderData = MakeUnique<FStaticMeshRenderData>();
		RenderData->AllocateLODResources(NumLods);
		for (int32 Lod = 0; Lod < NumLods; ++Lod)
		{
			const_cast<UStaticMesh*>(Template)->BuildFromMeshDescription(Descriptions[Lod], RenderData->LODResources[Lod]);
		}
		OutBounds = Descriptions[0].GetBounds();
		return RenderData;
	}

	/** Builds a prepared mesh's render data (the slow part of making a big mesh: ~35 ms for a 120k-triangle sculpted wall) and its shadow
	 *  copy off the game thread, the same way UStaticMesh::BuildFromMeshDescriptions' fast build does. Safe on worker threads once
	 *  MakeSlotTemplates ran. */
	void PrebuildRenderData(FPreparedMesh& Prepared)
	{
		const UStaticMesh* Template = GSlotTemplate.load();
		if (!Template || Prepared.NumLods == 0 || Prepared.Slots.Num() > TemplateSlots)
		{
			return;
		}
		Prepared.RenderData = BuildRenderData(Template, Prepared.Descriptions, Prepared.NumLods, Prepared.Bounds);
		// the shadow copy, which AddMergedMesh would otherwise build on the game thread the first time the mesh is shown
		if (!Prepared.bHasShadowDescription && Prepared.ShadowParts.Num() > 0)
		{
			PrepareShadowDescription(Prepared.ShadowParts, Prepared.ShadowDescription);
			Prepared.ShadowParts.Reset();
			Prepared.bHasShadowDescription = true;
		}
		if (Prepared.bHasShadowDescription)
		{
			Prepared.ShadowRenderData = BuildRenderData(GShadowTemplate.load(), &Prepared.ShadowDescription, 1, Prepared.ShadowBounds);
		}
	}

	/** Hands render data built on a worker to its mesh and starts the upload: what's left of the fast build for the game thread */
	void FinishPrebuiltMesh(UStaticMesh* Mesh, TUniquePtr<FStaticMeshRenderData>&& RenderData, const FBoxSphereBounds& Bounds, int32 NumLods)
	{
		Mesh->NeverStream = true;
		Mesh->bDoFastBuild = true;
		RenderData->Bounds = Bounds;
		for (int32 Lod = 0; Lod < NumLods; ++Lod)
		{
			RenderData->ScreenSize[Lod].Default = FMath::Pow(0.75f, Lod);	// as the fast build sets them
		}
		Mesh->SetRenderData(MoveTemp(RenderData));
		Mesh->InitResources();
		Mesh->CalculateExtendedBounds();
		Mesh->AddToRoot();
		SetLodScreenSizes(Mesh, NumLods);
	}

	/** Coarser sculpted surfaces for the lower levels of detail */
	constexpr float LodSculptCell[MaxMergedLods] = { 1.0f, 1.6f, 2.5f };

	void PrepareMergedMesh(const TArray<FTDMergedPart>& InParts, FPreparedMesh& Out)
	{
		if (InParts.Num() == 0)
		{
			return;
		}

		// the detailed models' creature bodies: their organic parts melt into one sculpted surface per level, the rest stay shapes
		const bool bHD = HasDetailMaterial();
		TArray<FTDMergedPart> Parts;
		TArray<FTDMergedPart> SculptParts;
		Parts.Reserve(InParts.Num());
		for (const FTDMergedPart& Part : InParts)
		{
			(bHD && Part.bSculpt && Part.Mat == ETDPartMaterial::Lit ? SculptParts : Parts).Add(Part);
		}
		FTDSculptMesh Sculpts[MaxMergedLods];
		bool bHasSculpt = false;
		if (SculptParts.Num() >= 2)
		{
			bHasSculpt = TDSculpt::Build(SculptParts, LodSculptCell[0], Sculpts[0]);
		}
		if (!bHasSculpt)
		{
			Parts.Append(SculptParts);
			SculptParts.Reset();
		}
		if (!bHD)
		{
			for (FTDMergedPart& Part : Parts)
			{
				Part.Surface = ETDSurface::Auto;
			}
		}
		if (Parts.Num() == 0 && !bHasSculpt)
		{
			return;
		}
		static const IConsoleVariable* LodsVar = IConsoleManager::Get().FindConsoleVariable(TEXT("atd.ModelLods"));
		Out.NumLods = (!LodsVar || LodsVar->GetInt() != 0) && (NeedsLods(Parts) || bHasSculpt) ? MaxMergedLods : 1;
		for (int32 Lod = 1; bHasSculpt && Lod < Out.NumLods; ++Lod)
		{
			if (!TDSculpt::Build(SculptParts, LodSculptCell[Lod], Sculpts[Lod]))
			{
				Sculpts[Lod] = Sculpts[Lod - 1];
			}
		}

		// every part's shape on every level first, so each description can reserve its memory once
		struct FPlaced { const FUnitShape* Shape; FVector3f Scale; int32 Own; };
		TArray<FUnitShape> Owned;
		Owned.Reserve(Parts.Num() * Out.NumLods);
		TArray<FPlaced> Placed[MaxMergedLods];
		for (int32 Lod = 0; Lod < Out.NumLods; ++Lod)
		{
			int32 Vertices = 0, Triangles = 0;
			Placed[Lod].Reserve(Parts.Num());
			for (const FTDMergedPart& Part : Parts)
			{
				FUnitShape Own;
				FVector3f Scale;
				const FUnitShape& Shape = PartShape(Part, Lod, Own, Scale);
				if (&Shape == &Own)
				{
					Placed[Lod].Add({ nullptr, Scale, Owned.Num() });
					Owned.Add(MoveTemp(Own));
				}
				else
				{
					Placed[Lod].Add({ &Shape, Scale, INDEX_NONE });
				}
				Vertices += Shape.Positions.Num();
				Triangles += Shape.Triangles.Num() / 3;
			}
			if (bHasSculpt)
			{
				Vertices += Sculpts[Lod].Positions.Num();
				Triangles += Sculpts[Lod].Triangles.Num() / 3;
			}
			FMeshDescription& Description = Out.Descriptions[Lod];
			FStaticMeshAttributes(Description).Register();
			Description.ReserveNewVertices(Vertices);
			Description.ReserveNewVertexInstances(Vertices);
			Description.ReserveNewTriangles(Triangles);
			Description.ReserveNewEdges(Triangles * 3 / 2 + Parts.Num() * 4);
		}

		// one section per distinct look, in the same order on every level
		TMap<uint64, int32> SlotOfLook;
		TArray<FPolygonGroupID> Groups[MaxMergedLods];
		TArray<FVertexInstanceID> Instances;
		for (int32 PartIndex = 0; PartIndex < Parts.Num(); ++PartIndex)
		{
			const FTDMergedPart& Part = Parts[PartIndex];
			auto Q = [](float V) { return (uint64)FMath::Clamp(FMath::RoundToInt(V * 128.0f), 0, 0xFFF); };
			const uint64 Look = Q(Part.Color.R) | (Q(Part.Color.G) << 12) | (Q(Part.Color.B) << 24) | (Q(Part.Color.A) << 36) | ((uint64)Part.Mat << 48) | ((uint64)Part.Flags << 52)
				| ((uint64)Part.Surface << 60);
			int32* Slot = SlotOfLook.Find(Look);
			if (!Slot)
			{
				const FName SlotName(*FString::Printf(TEXT("Slot%d"), Out.Slots.Num()));
				for (int32 Lod = 0; Lod < Out.NumLods; ++Lod)
				{
					const FPolygonGroupID NewGroup = Out.Descriptions[Lod].CreatePolygonGroup();
					FStaticMeshAttributes(Out.Descriptions[Lod]).GetPolygonGroupMaterialSlotNames()[NewGroup] = SlotName;
					Groups[Lod].Add(NewGroup);
				}
				Out.Slots.Add({ Part.Color, Part.Mat, Part.Flags, Part.Surface });
				Slot = &SlotOfLook.Add(Look, Out.Slots.Num() - 1);
			}

			for (int32 Lod = 0; Lod < Out.NumLods; ++Lod)
			{
				const FPlaced& Where = Placed[Lod][PartIndex];
				const FUnitShape& Shape = Where.Shape ? *Where.Shape : Owned[Where.Own];
				FStaticMeshAttributes Attributes(Out.Descriptions[Lod]);
				AppendShape(Out.Descriptions[Lod], Attributes, Groups[Lod][*Slot], Shape, Where.Scale, Part, Instances);
			}

			// the shadow copy keeps the lit parts that are big enough to show in a shadow
			if (Part.Mat == ETDPartMaterial::Lit && Part.Size.GetMax() >= TD::ShadowMinPartSize)
			{
				Out.ShadowParts.Add(Part);
			}
		}

		// the sculpted surface: its own section, white times its vertex colours
		if (bHasSculpt)
		{
			const FName SlotName(*FString::Printf(TEXT("Slot%d"), Out.Slots.Num()));
			// a reef melted from rocks (the Coral Walls) reads as stone; creature bodies as skin
			double StoneVolume = 0.0, Volume = 0.0;
			for (const FTDMergedPart& Part : SculptParts)
			{
				const double V = Part.Size.X * Part.Size.Y * Part.Size.Z;
				Volume += V;
				StoneVolume += Part.Surface == ETDSurface::Stone ? V : 0.0;
			}
			Out.Slots.Add({ FLinearColor::White, ETDPartMaterial::Lit, 0, StoneVolume * 2.0 >= Volume ? ETDSurface::Stone : ETDSurface::Skin });
			for (int32 Lod = 0; Lod < Out.NumLods; ++Lod)
			{
				FStaticMeshAttributes Attributes(Out.Descriptions[Lod]);
				const FPolygonGroupID Group = Out.Descriptions[Lod].CreatePolygonGroup();
				Attributes.GetPolygonGroupMaterialSlotNames()[Group] = SlotName;
				AppendSculpt(Out.Descriptions[Lod], Attributes, Group, Sculpts[Lod], Instances);
			}
			// its shapes cast the shadow: the sculpted surface only rounds them off where they meet
			for (const FTDMergedPart& Part : SculptParts)
			{
				if (Part.Size.GetMax() >= TD::ShadowMinPartSize)
				{
					Out.ShadowParts.Add(Part);
				}
			}
		}
	}

	/** Makes the static mesh of a prepared merged mesh (game thread: it creates objects and materials) */
	FTDMergedMesh FinishMergedMesh(FPreparedMesh& Prepared)
	{
		FTDMergedMesh Out;
		if (Prepared.NumLods == 0)
		{
			return Out;
		}
		UStaticMesh* Mesh = NewRuntimeMesh();
		for (int32 i = 0; i < Prepared.Slots.Num(); ++i)
		{
			const FTDMergedMesh::FSlot& Slot = Prepared.Slots[i];
			Mesh->GetStaticMaterials().Add(FStaticMaterial(SlotMaterial(Slot.Color, Slot.Mat, Slot.Color.A, Slot.Surface), FName(*FString::Printf(TEXT("Slot%d"), i))));
		}
		if (Prepared.RenderData)
		{
			FinishPrebuiltMesh(Mesh, MoveTemp(Prepared.RenderData), Prepared.Bounds, Prepared.NumLods);
		}
		else
		{
			TArray<const FMeshDescription*> Levels;
			for (int32 Lod = 0; Lod < Prepared.NumLods; ++Lod)
			{
				Levels.Add(&Prepared.Descriptions[Lod]);
			}
			FinishRuntimeMesh(Mesh, Levels);
		}
		Out.Mesh = Mesh;
		Out.Slots = MoveTemp(Prepared.Slots);
		Out.ShadowParts = MoveTemp(Prepared.ShadowParts);
		if (Prepared.bHasShadowDescription)
		{
			UStaticMesh* Shadow = NewShadowMesh();
			if (Prepared.ShadowRenderData)
			{
				FinishPrebuiltMesh(Shadow, MoveTemp(Prepared.ShadowRenderData), Prepared.ShadowBounds, 1);
			}
			else
			{
				FinishRuntimeMesh(Shadow, { &Prepared.ShadowDescription });
			}
			Out.ShadowMesh = Shadow;
		}
		return Out;
	}

	// ---------------------------------------------------------------- sculpted models baked from the way-more-detail preview

	/** File names under Content/ATDModels, in ETDModel order */
	const TCHAR* const BakedModelFiles[] = {
		TEXT("Harpoon"), TEXT("Jelly"), TEXT("Puffer"), TEXT("Eel"), TEXT("Mantis"), TEXT("Octopus"), TEXT("Lionfish"), TEXT("Sawfish"),
		TEXT("Humpback"), TEXT("AcidVent"), TEXT("Crab"), TEXT("Piranha"), TEXT("Turtle"), TEXT("Manta"), TEXT("Kraken"), TEXT("SeaStar"),
		TEXT("Cuttlefish"), TEXT("ShockEel"), TEXT("Boulders"), TEXT("Trench"), TEXT("Goal"), TEXT("KelpShort"), TEXT("KelpMid"), TEXT("KelpTall"),
		TEXT("ShotHarpoon"), TEXT("ShotRock"), TEXT("ShotLionfish"), TEXT("ShotSawfish"), TEXT("ShotBomb"), TEXT("Spines"),
		TEXT("DropShell"), TEXT("DropPearl"), TEXT("DropCharm"),
		TEXT("HarpoonSwordfish"), TEXT("HarpoonNarwhal"), TEXT("JellyBox"), TEXT("JellyMoon"), TEXT("PufferCluster"), TEXT("PufferLobber"),
		TEXT("EelStorm"), TEXT("EelOverload"), TEXT("MantisSmasher"), TEXT("MantisSpearer"), TEXT("OctopusJuggler"), TEXT("OctopusBlueRinged"),
		TEXT("LionfishCloud"), TEXT("LionfishParalytic"), TEXT("SawfishRipper"), TEXT("SawfishSweeper"), TEXT("HumpbackSonar"), TEXT("HumpbackChorus"),
		TEXT("AcidVentCold"), TEXT("AcidVentBlack") };
	static_assert(UE_ARRAY_COUNT(BakedModelFiles) == (int32)ETDModel::Count, "one file name per model");

	struct FBakedLook
	{
		FLinearColor Color;
		ETDPartMaterial Mat;
		uint8 Flags;
		bool bVertex;	// plain lit surface coloured per vertex
	};

	struct FBakedLod
	{
		TArray<FVector3f> Positions;
		TArray<FVector3f> Normals;
		TArray<FColor> Colors;	// sRGB colour, look index in A
		TArray<uint32> Indices;
	};

	/** One pivot's solid or see-through mesh */
	struct FBakedMesh
	{
		int32 Pivot = 0;
		bool bSolid = true;
		TArray<FBakedLook> Looks;
		TArray<FLinearColor> Palette;	// stands in for the vertex colours when there is no vertex-colour material
		TArray<FBakedLod> Lods;
	};

	struct FBakedModel
	{
		bool bLoaded = false;
		TArray<FBakedMesh> Meshes;
	};

	/** Reads a baked model file: "ATDM", version 1, the unpacked size, then a zlib stream (written by the preview's pack.mjs) */
	bool ReadBakedModel(const FString& Path, TArray<FBakedMesh>& Out)
	{
		TArray<uint8> File;
		if (!FFileHelper::LoadFileToArray(File, *Path, FILEREAD_Silent) || File.Num() < 12 || FMemory::Memcmp(File.GetData(), "ATDM", 4) != 0)
		{
			return false;
		}
		const uint32 Version = *(const uint32*)(File.GetData() + 4);
		const uint32 RawSize = *(const uint32*)(File.GetData() + 8);
		if (Version != 1 || RawSize == 0 || RawSize > 256u * 1024u * 1024u)
		{
			return false;
		}
		TArray<uint8> Raw;
		Raw.SetNumUninitialized(RawSize);
		if (!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), (int32)RawSize, File.GetData() + 12, File.Num() - 12))
		{
			return false;
		}

		int64 At = 0;
		bool bOk = true;
		auto Take = [&](void* Dest, int64 Bytes)
		{
			if (!bOk || Bytes < 0 || At + Bytes > Raw.Num())
			{
				bOk = false;
				return;
			}
			FMemory::Memcpy(Dest, Raw.GetData() + At, Bytes);
			At += Bytes;
		};
		auto U8 = [&]() { uint8 V = 0; Take(&V, 1); return V; };
		auto U16 = [&]() { uint16 V = 0; Take(&V, 2); return V; };
		auto U32 = [&]() { uint32 V = 0; Take(&V, 4); return V; };
		auto F32 = [&]() { float V = 0.0f; Take(&V, 4); return V; };

		const int32 NumMeshes = U16();
		for (int32 m = 0; m < NumMeshes && bOk; ++m)
		{
			FBakedMesh& Mesh = Out.AddDefaulted_GetRef();
			Mesh.Pivot = U8();
			Mesh.bSolid = U8() != 0;
			const int32 NumLods = U8();
			const int32 NumLooks = U8();
			for (int32 l = 0; l < NumLooks && bOk; ++l)
			{
				FBakedLook& Look = Mesh.Looks.AddDefaulted_GetRef();
				Look.Color.R = F32();
				Look.Color.G = F32();
				Look.Color.B = F32();
				Look.Color.A = F32();
				const uint8 Mat = U8();
				Look.Mat = Mat == 1 ? ETDPartMaterial::Glow : (Mat == 2 ? ETDPartMaterial::Glass : ETDPartMaterial::Lit);
				Look.Flags = U8();
				Look.bVertex = U8() != 0;
				U8();
			}
			const int32 NumPalette = U8();
			for (int32 p = 0; p < NumPalette && bOk; ++p)
			{
				const float R = F32(), G = F32(), B = F32();
				Mesh.Palette.Add(FLinearColor(R, G, B, 1.0f));
			}
			for (int32 l = 0; l < NumLods && bOk; ++l)
			{
				FBakedLod& Lod = Mesh.Lods.AddDefaulted_GetRef();
				const uint32 NumVerts = U32();
				const uint32 NumIndices = U32();
				if (!bOk || NumVerts > 4000000u || NumIndices > 12000000u || NumIndices % 3 != 0)
				{
					bOk = false;
					break;
				}
				Lod.Positions.SetNumUninitialized(NumVerts);
				Take(Lod.Positions.GetData(), (int64)NumVerts * 12);
				TArray<int8> Packed;
				Packed.SetNumUninitialized(NumVerts * 4);
				Take(Packed.GetData(), (int64)NumVerts * 4);
				Lod.Normals.SetNumUninitialized(NumVerts);
				for (uint32 v = 0; v < NumVerts && bOk; ++v)
				{
					Lod.Normals[v] = FVector3f(Packed[v * 4], Packed[v * 4 + 1], Packed[v * 4 + 2]).GetSafeNormal();
				}
				TArray<uint8> Colors;
				Colors.SetNumUninitialized(NumVerts * 4);
				Take(Colors.GetData(), (int64)NumVerts * 4);
				Lod.Colors.SetNumUninitialized(NumVerts);
				for (uint32 v = 0; v < NumVerts && bOk; ++v)
				{
					Lod.Colors[v] = FColor(Colors[v * 4], Colors[v * 4 + 1], Colors[v * 4 + 2], Colors[v * 4 + 3]);
				}
				Lod.Indices.SetNumUninitialized(NumIndices);
				Take(Lod.Indices.GetData(), (int64)NumIndices * 4);
				for (uint32 i = 0; i < NumIndices && bOk; ++i)
				{
					bOk = Lod.Indices[i] < NumVerts;
				}
				for (uint32 v = 0; v < NumVerts && bOk; ++v)
				{
					bOk = Lod.Colors[v].A < NumLooks;
				}
			}
			bOk = bOk && Mesh.Lods.Num() > 0;
		}
		return bOk;
	}

	/** The baked model of Id, read the first time it is asked for (safe from the threads that prepare meshes) */
	const FBakedModel& GetBakedModel(ETDModel Id)
	{
		// one lock per model, so a thread reading one file never holds up another model's lookups
		static FCriticalSection Locks[(int32)ETDModel::Count];
		static FBakedModel Models[(int32)ETDModel::Count];
		const int32 Index = FMath::Clamp((int32)Id, 0, (int32)ETDModel::Count - 1);
		FScopeLock Scope(&Locks[Index]);
		FBakedModel& Model = Models[Index];
		if (!Model.bLoaded)
		{
			Model.bLoaded = true;
			const FString Path = FPaths::ProjectContentDir() / TEXT("ATDModels") / FString(BakedModelFiles[Index]) + TEXT(".atdm");
			if (FPaths::FileExists(Path) && !ReadBakedModel(Path, Model.Meshes))
			{
				UE_LOG(LogAquaticTowerDefense, Warning, TEXT("Could not read the sculpted model %s, using the detailed shapes"), *Path);
				Model.Meshes.Reset();
			}
		}
		return Model;
	}

	const FBakedMesh* FindBakedMesh(ETDModel Id, int32 PivotIndex, bool bSolid)
	{
		for (const FBakedMesh& Mesh : GetBakedModel(Id).Meshes)
		{
			if (Mesh.Pivot == PivotIndex && Mesh.bSolid == bSolid)
			{
				return &Mesh;
			}
		}
		return nullptr;
	}

	/** The spike field (Spines) is laid out here rather than baked whole, because the game spreads it to different sizes: its file
	 *  holds one spine standing on its base (pivot 0), the floor patch (pivot 1) and one venom puddle (pivot 2). A copy of the
	 *  spine goes where each of the model's own spines is, tilted like that cone, with the shaft above the collar stretched to
	 *  the cone's length; a puddle goes on each of its puddles; the floor patch stays in the middle. */
	constexpr float SpineCollarTop = 6.0f;
	constexpr float SpineBakedLength = 15.5f;

	struct FBakedPlacement
	{
		FVector3f Base;
		FQuat4f Rotation;
		float Stretch;	// along the spine above the collar; 0 leaves the copy unstretched
	};

	TUniquePtr<FBakedMesh> BuildSpinesField(const FVector& RootSpread, bool bSolid)
	{
		TUniquePtr<FBakedMesh> Out = MakeUnique<FBakedMesh>();
		Out->Pivot = 0;
		Out->bSolid = bSolid;
		const FTDModel& Def = TD::GetModel(ETDModel::Spines);
		TArray<FBakedPlacement> Spines;
		TArray<FBakedPlacement> Puddles;
		for (int32 i = 0; i < Def.NumParts; ++i)
		{
			const FTDModelPart& P = Def.Parts[i];
			if (P.Pivot != 0)
			{
				continue;
			}
			const FVector3f Location(FVector(P.L[0], P.L[1], P.L[2]) * RootSpread);
			if ((ETDShape)P.Shape == ETDShape::Cone && (ETDPartMaterial)P.Mat == ETDPartMaterial::Lit)
			{
				const FQuat4f Rotation(P.Q[0], P.Q[1], P.Q[2], P.Q[3]);
				const float Length = P.S[2];
				Spines.Add({ Location - Rotation.RotateVector(FVector3f::UnitZ()) * (Length * 0.5f), Rotation, FMath::Max(0.1f, (Length - SpineCollarTop) / (SpineBakedLength - SpineCollarTop)) });
			}
			else if ((ETDShape)P.Shape == ETDShape::Sphere && (ETDPartMaterial)P.Mat == ETDPartMaterial::Glass)
			{
				Puddles.Add({ Location - FVector3f(0.0f, 0.0f, 1.0f), FQuat4f::Identity, 0.0f });
			}
		}
		const FBakedPlacement Middle{ FVector3f::ZeroVector, FQuat4f::Identity, 0.0f };

		for (const FBakedMesh& Src : GetBakedModel(ETDModel::Spines).Meshes)
		{
			const TArrayView<const FBakedPlacement> Where = Src.Pivot == 0 ? TArrayView<const FBakedPlacement>(Spines)
				: (Src.Pivot == 2 ? TArrayView<const FBakedPlacement>(Puddles) : TArrayView<const FBakedPlacement>(&Middle, 1));
			if (Src.bSolid != bSolid || Where.Num() == 0 || Src.Lods.Num() == 0)
			{
				continue;
			}
			const int32 LookOffset = Out->Looks.Num();
			Out->Looks.Append(Src.Looks);
			Out->Palette.Append(Src.Palette);
			const int32 NumLods = Out->Lods.Num() == 0 ? Src.Lods.Num() : FMath::Min(Out->Lods.Num(), Src.Lods.Num());
			Out->Lods.SetNum(NumLods);
			for (int32 l = 0; l < NumLods; ++l)
			{
				const FBakedLod& From = Src.Lods[l];
				FBakedLod& To = Out->Lods[l];
				To.Positions.Reserve(To.Positions.Num() + From.Positions.Num() * Where.Num());
				To.Normals.Reserve(To.Normals.Num() + From.Normals.Num() * Where.Num());
				To.Colors.Reserve(To.Colors.Num() + From.Colors.Num() * Where.Num());
				To.Indices.Reserve(To.Indices.Num() + From.Indices.Num() * Where.Num());
				for (const FBakedPlacement& At : Where)
				{
					const uint32 First = (uint32)To.Positions.Num();
					for (int32 v = 0; v < From.Positions.Num(); ++v)
					{
						FVector3f Position = From.Positions[v];
						FVector3f Normal = From.Normals[v];
						if (At.Stretch > 0.0f && Position.Z > SpineCollarTop)
						{
							Position.Z = SpineCollarTop + (Position.Z - SpineCollarTop) * At.Stretch;
							Normal.Z /= At.Stretch;
						}
						To.Positions.Add(At.Base + At.Rotation.RotateVector(Position));
						To.Normals.Add(At.Rotation.RotateVector(Normal.GetSafeNormal()));
						FColor Color = From.Colors[v];
						Color.A = (uint8)FMath::Min(255, Color.A + LookOffset);
						To.Colors.Add(Color);
					}
					for (const uint32 Index : From.Indices)
					{
						To.Indices.Add(First + Index);
					}
				}
			}
		}
		if (Out->Looks.Num() > 255)
		{
			Out->Lods.Reset();
		}
		return Out;
	}

	/** The laid-out spike field for one spread, made once and kept (safe from the threads that prepare meshes) */
	const FBakedMesh* SpinesFieldMesh(const FVector& RootSpread, bool bSolid)
	{
		static FCriticalSection Lock;
		static TMap<uint64, TUniquePtr<FBakedMesh>> Fields;
		const uint64 Key = ((uint64)GetTypeHash(RootSpread) << 1) | (bSolid ? 1 : 0);
		FScopeLock Scope(&Lock);
		if (const TUniquePtr<FBakedMesh>* Found = Fields.Find(Key))
		{
			return Found->Get();
		}
		return Fields.Add(Key, BuildSpinesField(RootSpread, bSolid)).Get();
	}

	/** The baked mesh a sculpted model pivot shows at a spread: the laid-out field for Spines, the file's own mesh otherwise */
	const FBakedMesh* FindBakedMeshAt(ETDModel Id, int32 PivotIndex, bool bSolid, const FVector& RootSpread)
	{
		if (Id == ETDModel::Spines)
		{
			return PivotIndex == 0 ? SpinesFieldMesh(RootSpread, bSolid) : nullptr;
		}
		return FindBakedMesh(Id, PivotIndex, bSolid);
	}

	/** The detail material surface of a sculpted model's vertex-coloured section: skin for creatures, plain for the props, shots and drops */
	ETDSurface BakedSurface(ETDModel Id)
	{
		switch (Id)
		{
		case ETDModel::Harpoon: case ETDModel::Boulders: case ETDModel::Trench: case ETDModel::Goal:
		case ETDModel::ShotHarpoon: case ETDModel::ShotRock: case ETDModel::ShotLionfish: case ETDModel::ShotSawfish: case ETDModel::ShotBomb:
		case ETDModel::Spines: case ETDModel::DropShell: case ETDModel::DropPearl: case ETDModel::DropCharm:
			return ETDSurface::Plain;
		default:
			return ETDSurface::Skin;
		}
	}

	/** Fills a prepared mesh from a baked one: one section for every vertex-coloured surface (M_ATD_Detail; one per palette colour
	 *  without it) and one per other look, the baked levels of detail, and a shadow copy made from the coarsest level */
	void PrepareBakedMesh(const FBakedMesh& Baked, bool bVertexMaterial, ETDSurface VertexSurface, FPreparedMesh& Out)
	{
		static const IConsoleVariable* LodsVar = IConsoleManager::Get().FindConsoleVariable(TEXT("atd.ModelLods"));
		Out.NumLods = FMath::Min(Baked.Lods.Num(), (!LodsVar || LodsVar->GetInt() != 0) ? MaxMergedLods : 1);
		if (Out.NumLods == 0)
		{
			return;
		}

		// slots in a fixed order: the vertex-coloured surface (or its palette), then every other look
		const bool bPalette = !bVertexMaterial && Baked.Palette.Num() > 0;
		TArray<int32> SlotOfLook;
		SlotOfLook.Init(INDEX_NONE, Baked.Looks.Num());
		int32 VertexSlot = INDEX_NONE;
		int32 FirstPaletteSlot = INDEX_NONE;
		for (int32 l = 0; l < Baked.Looks.Num(); ++l)
		{
			const FBakedLook& Look = Baked.Looks[l];
			if (Look.bVertex && bVertexMaterial)
			{
				if (VertexSlot == INDEX_NONE)
				{
					VertexSlot = Out.Slots.Add({ FLinearColor::White, ETDPartMaterial::Lit, 0, VertexSurface });
				}
				SlotOfLook[l] = VertexSlot;
			}
			else if (Look.bVertex && bPalette)
			{
				if (FirstPaletteSlot == INDEX_NONE)
				{
					FirstPaletteSlot = Out.Slots.Num();
					for (const FLinearColor& Color : Baked.Palette)
					{
						Out.Slots.Add({ Color, ETDPartMaterial::Lit, 0 });
					}
				}
			}
			else
			{
				SlotOfLook[l] = Out.Slots.Add({ Look.Color, Look.Mat, Look.Flags, bVertexMaterial && Look.Mat == ETDPartMaterial::Lit ? ETDSurface::Plain : ETDSurface::Auto });
			}
		}

		auto NearestPalette = [&Baked](const FLinearColor& C)
		{
			int32 Best = 0;
			float BestDist = MAX_flt;
			for (int32 p = 0; p < Baked.Palette.Num(); ++p)
			{
				const FLinearColor& P = Baked.Palette[p];
				const float Dist = FMath::Square(P.R - C.R) + FMath::Square(P.G - C.G) + FMath::Square(P.B - C.B);
				if (Dist < BestDist)
				{
					BestDist = Dist;
					Best = p;
				}
			}
			return Best;
		};

		auto DecodeSRGB = [](float X) { X = FMath::Clamp(X, 0.0f, 1.0f); return X <= 0.04045f ? X / 12.92f : FMath::Pow((X + 0.055f) / 1.055f, 2.4f); };
		auto Build = [&](const FBakedLod& Lod, FMeshDescription& Description, bool bShadow)
		{
			FStaticMeshAttributes Attributes(Description);
			Attributes.Register();
			TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
			TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
			TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
			TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
			TVertexInstanceAttributesRef<FVector4f> Colors = Attributes.GetVertexInstanceColors();
			TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
			const int32 NumVerts = Lod.Positions.Num();
			Description.ReserveNewVertices(NumVerts);
			Description.ReserveNewVertexInstances(NumVerts);
			Description.ReserveNewTriangles(Lod.Indices.Num() / 3);
			Description.ReserveNewEdges(Lod.Indices.Num() / 2 + 16);

			TArray<FPolygonGroupID> Groups;
			if (bShadow)
			{
				const FPolygonGroupID Group = Description.CreatePolygonGroup();
				Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Shadow");
				Groups.Add(Group);
			}
			else
			{
				for (int32 s = 0; s < Out.Slots.Num(); ++s)
				{
					const FPolygonGroupID Group = Description.CreatePolygonGroup();
					Attributes.GetPolygonGroupMaterialSlotNames()[Group] = FName(*FString::Printf(TEXT("Slot%d"), s));
					Groups.Add(Group);
				}
			}

			TArray<FVertexInstanceID> Instances;
			Instances.SetNumUninitialized(NumVerts);
			for (int32 v = 0; v < NumVerts; ++v)
			{
				const FVector3f& N = Lod.Normals[v];
				const FVertexID Vertex = Description.CreateVertex();
				// the shadow copy sits a little inside the surface, so the coarser shape never shades the full one
				Positions[Vertex] = bShadow ? Lod.Positions[v] - N * 0.5f : Lod.Positions[v];
				const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
				const FVector3f Across = FMath::Abs(N.Z) < 0.9f ? FVector3f(0.0f, 0.0f, 1.0f) : FVector3f(1.0f, 0.0f, 0.0f);
				Normals[Instance] = N;
				Tangents[Instance] = FVector3f::CrossProduct(Across, N).GetSafeNormal();
				BinormalSigns[Instance] = 1.0f;
				// the mesh build sRGB-encodes vertex colours and the material reads them raw, so the linear colour is decoded once more
				// (as TDSculpt does) to arrive as itself
				const FLinearColor Linear = FLinearColor::FromSRGBColor(FColor(Lod.Colors[v].R, Lod.Colors[v].G, Lod.Colors[v].B, 255));
				Colors[Instance] = FVector4f(DecodeSRGB(Linear.R), DecodeSRGB(Linear.G), DecodeSRGB(Linear.B), 1.0f);
				UVs.Set(Instance, 0, FVector2f::ZeroVector);
				Instances[v] = Instance;
			}
			for (int32 i = 0; i + 2 < Lod.Indices.Num(); i += 3)
			{
				const uint32 A = Lod.Indices[i], B = Lod.Indices[i + 1], C = Lod.Indices[i + 2];
				if (A == B || B == C || A == C)
				{
					continue;
				}
				int32 Slot = 0;
				if (!bShadow)
				{
					const int32 LookIndex = Lod.Colors[A].A;
					Slot = SlotOfLook[LookIndex];
					if (Slot == INDEX_NONE)
					{
						if (FirstPaletteSlot == INDEX_NONE)
						{
							continue;
						}
						const FLinearColor Avg = (FLinearColor::FromSRGBColor(Lod.Colors[A]) + FLinearColor::FromSRGBColor(Lod.Colors[B]) + FLinearColor::FromSRGBColor(Lod.Colors[C])) / 3.0f;
						Slot = FirstPaletteSlot + NearestPalette(Avg);
					}
				}
				const FVertexInstanceID Corners[] = { Instances[A], Instances[B], Instances[C] };
				Description.CreateTriangle(Groups[Slot], Corners);
			}
		};

		for (int32 Lod = 0; Lod < Out.NumLods; ++Lod)
		{
			Build(Baked.Lods[Lod], Out.Descriptions[Lod], false);
		}
		if (Baked.bSolid)
		{
			Out.bHasShadowDescription = true;
			Build(Baked.Lods.Last(), Out.ShadowDescription, true);
		}
	}

	FTDMergedMesh BuildMergedMesh(const TArray<FTDMergedPart>& Parts)
	{
		FPreparedMesh Prepared;
		PrepareMergedMesh(Parts, Prepared);
		return FinishMergedMesh(Prepared);
	}

	TMap<uint64, FTDMergedMesh>& MergedMeshes()
	{
		static TMap<uint64, FTDMergedMesh> Meshes;
		return Meshes;
	}

	/** The merged mesh of one model pivot: its solid parts, or its see-through parts, which sort apart from the solid ones */
	uint64 ModelPivotKey(ETDModel Id, int32 PivotIndex, bool bSolid, float MinSize, const FVector& RootSpread, bool bDetailed)
	{
		const uint64 Options = ((uint64)FMath::RoundToInt(MinSize * 10.0f) << 24) | (((uint64)GetTypeHash(RootSpread) << 32) & ~(3ull << 62));
		return (uint64)Id | ((uint64)PivotIndex << 8) | ((uint64)(bSolid ? 1 : 0) << 16) | Options | ((uint64)(bDetailed ? 1 : 0) << 62);
	}

	/** The cached mesh of a sculpted model pivot: one per pivot and kind whatever the minimum part size (bit 17 is never set in the other keys) */
	uint64 SculptedPivotKey(ETDModel Id, int32 PivotIndex, bool bSolid, const FVector& RootSpread = FVector::OneVector)
	{
		// only the spike field is laid out per spread (see SpinesFieldMesh); bits 24-61 tell its spreads apart
		const uint64 Spread = RootSpread.Equals(FVector::OneVector) ? 0 : (((uint64)GetTypeHash(RootSpread) << 24) & ((1ull << 62) - 1));
		return (uint64)Id | ((uint64)PivotIndex << 8) | ((uint64)(bSolid ? 1 : 0) << 16) | (1ull << 17) | Spread | (1ull << 62);
	}

	/** The parts of one model pivot: its solid parts, or its see-through parts, which sort apart from the solid ones */
	void ModelPivotParts(ETDModel Id, int32 PivotIndex, bool bSolid, float MinSize, const FVector& RootSpread, bool bDetailed, TArray<FTDMergedPart>& Parts)
	{
		{
			const FTDModel& Model = TD::GetModel(Id);
			for (int32 i = 0; i < Model.NumParts; ++i)
			{
				const FTDModelPart& P = Model.Parts[i];
				const FVector Size(P.S[0], P.S[1], P.S[2]);
				// the detail pass looks at every part of the pivot, so both kinds are gathered when it runs
				if (P.Pivot != PivotIndex || Size.GetMax() < MinSize || (!bDetailed && (((ETDPartMaterial)P.Mat == ETDPartMaterial::Lit) != bSolid)))
				{
					continue;
				}
				const FVector Location(P.L[0], P.L[1], P.L[2]);
				FTDMergedPart& Part = Parts.AddDefaulted_GetRef();
				Part.Shape = (ETDShape)P.Shape;
				Part.Mat = (ETDPartMaterial)P.Mat;
				Part.Flags = P.Flags;
				Part.Location = P.Pivot == 0 ? Location * RootSpread : Location;
				Part.Size = Size;
				Part.Rotation = FQuat(P.Q[0], P.Q[1], P.Q[2], P.Q[3]);
				Part.Color = FLinearColor(P.C[0], P.C[1], P.C[2], P.C[3]);
			}
			if (bDetailed)
			{
				TDModelDetail::AddDetail(Id, PivotIndex, MinSize, Parts);
				Parts.RemoveAll([bSolid](const FTDMergedPart& Part) { return (Part.Mat == ETDPartMaterial::Lit) != bSolid; });
				for (FTDMergedPart& Part : Parts)
				{
					Part.bDetailed = true;
				}
			}
		}
	}

	/** The merged mesh of one model pivot (see ModelPivotParts), or its sculpted mesh */
}

namespace
{
	/** A baked prop's cache key: from its file name, clear of the models' and the parts' own keys */
	uint64 BakedPropKey(const FString& Name)
	{
		return TD::MakeMergedKey((3ull << 44) | (uint64)GetTypeHash(Name));
	}

	FString BakedPropPath(const FString& Name)
	{
		return FPaths::ProjectContentDir() / TEXT("ATDModels") / Name + TEXT(".atdm");
	}
}

namespace
{
	/** One mesh for PrewarmModels to merge */
	struct FPrewarmRequest
	{
		uint64 Key = 0;
		TArray<FTDMergedPart> Parts;
		const FBakedMesh* Baked = nullptr;	// a sculpted pivot mesh instead of the parts
		ETDSurface Surface = ETDSurface::Skin;
		FString BakedProp;	// a baked seafloor prop's file instead (read when it is prepared)
	};

	/** The pivot meshes of models First to Last as BuildModel makes them (unscaled, at MinSize). Safe on a worker thread: the console
	 *  variables come in as bDetailed and bSculptOn, and the baked model files load under their own lock. */
	void GatherModelRequests(ETDModel First, ETDModel Last, float MinSize, bool bDetailed, bool bSculptOn, TArray<FPrewarmRequest>& Requests)
	{
		for (int32 Id = (int32)First; Id <= (int32)Last; ++Id)
		{
			// as ShowSculpted at an unscaled spread
			const bool bSculpted = bDetailed && bSculptOn && GetBakedModel((ETDModel)Id).Meshes.Num() > 0;
			for (int32 Pivot = 0; Pivot < TD::GetModel((ETDModel)Id).NumPivots; ++Pivot)
			{
				for (const bool bSolid : { false, true })
				{
					const uint64 Key = bSculpted ? SculptedPivotKey((ETDModel)Id, Pivot, bSolid) : ModelPivotKey((ETDModel)Id, Pivot, bSolid, MinSize, FVector::OneVector, bDetailed);
					if (Requests.ContainsByPredicate([Key](const FPrewarmRequest& R) { return R.Key == Key; }))
					{
						continue;
					}
					FPrewarmRequest& Request = Requests.AddDefaulted_GetRef();
					Request.Key = Key;
					if (bSculpted)
					{
						Request.Baked = FindBakedMeshAt((ETDModel)Id, Pivot, bSolid, FVector::OneVector);
						Request.Surface = BakedSurface((ETDModel)Id);
					}
					else
					{
						ModelPivotParts((ETDModel)Id, Pivot, bSolid, MinSize, FVector::OneVector, bDetailed, Request.Parts);
					}
				}
			}
		}
	}

	/** Worker thread: a request's geometry and render data */
	void PrepareRequest(const FPrewarmRequest& Request, bool bVertexMaterial, FPreparedMesh& Out)
	{
		if (Request.Baked)
		{
			PrepareBakedMesh(*Request.Baked, bVertexMaterial, Request.Surface, Out);
		}
		else if (!Request.BakedProp.IsEmpty())
		{
			// a missing or unreadable file leaves the mesh empty, which GetBakedPropMesh remembers as missing
			TArray<FBakedMesh> Baked;
			if (ReadBakedModel(BakedPropPath(Request.BakedProp), Baked) && Baked.Num() > 0)
			{
				PrepareBakedMesh(Baked[0], bVertexMaterial, ETDSurface::Plain, Out);
			}
		}
		else
		{
			PrepareMergedMesh(Request.Parts, Out);
		}
		PrebuildRenderData(Out);
	}

	/** The tower, creature, shot and drop models PrewarmModels hands to the worker threads: prepared there a batch at a time, turned into
	 *  meshes here a few milliseconds per frame (about 0.05 ms each) */
	struct FModelPrewarm
	{
		bool bActive = false;	// game thread
		double Start = 0.0;
		UE::Tasks::FTask Task;
		FCriticalSection Lock;
		TArray<TPair<uint64, TUniquePtr<FPreparedMesh>>> Ready;	// under Lock
		std::atomic<bool> bCancel{ false };
		std::atomic<int32> NumRequests{ 0 };
	};

	FModelPrewarm& ModelPrewarm()
	{
		static FModelPrewarm Prewarm;
		return Prewarm;
	}

	bool FinishOnePrewarmed(FModelPrewarm& Prewarm)
	{
		TPair<uint64, TUniquePtr<FPreparedMesh>> Item;
		{
			FScopeLock Scope(&Prewarm.Lock);
			if (Prewarm.Ready.Num() == 0)
			{
				return false;
			}
			Item = MoveTemp(Prewarm.Ready.Last());
			Prewarm.Ready.Pop(EAllowShrinking::No);
		}
		if (!MergedMeshes().Contains(Item.Key))
		{
			MergedMeshes().Add(Item.Key, FinishMergedMesh(*Item.Value));
		}
		return true;
	}

	void EndModelPrewarm(FModelPrewarm& Prewarm, double WaitedMs)
	{
		Prewarm.bActive = false;
		Prewarm.Ready.Empty();
		UE_LOG(LogAquaticTowerDefense, Log, TEXT("Merged %d tower, creature, shot and drop meshes in the background in %.0f ms (waited %.0f ms for them)"),
			Prewarm.NumRequests.load(), (FPlatformTime::Seconds() - Prewarm.Start) * 1000.0, WaitedMs);
	}

	/** Game thread, every frame while the prewarm runs */
	bool TickModelPrewarm(float)
	{
		FModelPrewarm& Prewarm = ModelPrewarm();
		if (!Prewarm.bActive)
		{
			return true;	// (stays registered for the next prewarm)
		}
		const double Until = FPlatformTime::Seconds() + 0.002;
		while (FPlatformTime::Seconds() < Until && FinishOnePrewarmed(Prewarm))
		{
		}
		if (Prewarm.Task.IsCompleted())
		{
			while (FinishOnePrewarmed(Prewarm))
			{
			}
			EndModelPrewarm(Prewarm, 0.0);
		}
		return true;
	}

	void StartModelPrewarm(bool bDetailed, bool bSculptOn, bool bVertexMaterial)
	{
		FModelPrewarm& Prewarm = ModelPrewarm();
		// once per look (play-in-editor sessions keep the merged meshes)
		static int32 PrewarmedLook = -1;
		const int32 Look = (bDetailed ? 1 : 0) | (bSculptOn ? 2 : 0) | (bVertexMaterial ? 4 : 0);
		if (PrewarmedLook == Look)
		{
			return;
		}
		PrewarmedLook = Look;
		static bool bHooked = false;
		if (!bHooked)
		{
			bHooked = true;
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickModelPrewarm));
			FCoreDelegates::OnPreExit.AddLambda([]()
			{
				ModelPrewarm().bCancel = true;
				ModelPrewarm().Task.Wait();
			});
		}
		Prewarm.bActive = true;
		Prewarm.Start = FPlatformTime::Seconds();
		Prewarm.Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [bDetailed, bSculptOn, bVertexMaterial]()
		{
			FModelPrewarm& Prewarm = ModelPrewarm();
			// the towers first (the dock and the first builds), then the creatures, shots and drops, with the minimum part sizes they build with
			TArray<FPrewarmRequest> Requests;
			GatherModelRequests(ETDModel::Harpoon, ETDModel::AcidVent, 2.5f, bDetailed, bSculptOn, Requests);
			GatherModelRequests(ETDModel::HarpoonSwordfish, ETDModel::AcidVentBlack, 2.5f, bDetailed, bSculptOn, Requests);
			GatherModelRequests(ETDModel::Crab, ETDModel::ShockEel, 3.0f, bDetailed, bSculptOn, Requests);
			GatherModelRequests(ETDModel::ShotHarpoon, ETDModel::ShotBomb, 0.0f, bDetailed, bSculptOn, Requests);
			GatherModelRequests(ETDModel::DropShell, ETDModel::DropCharm, 0.0f, bDetailed, bSculptOn, Requests);
			Prewarm.NumRequests = Requests.Num();
			// a batch at a time: prepared meshes are large
			constexpr int32 Batch = 48;
			for (int32 First = 0; First < Requests.Num() && !Prewarm.bCancel; First += Batch)
			{
				const int32 Count = FMath::Min(Batch, Requests.Num() - First);
				TArray<TUniquePtr<FPreparedMesh>> Prepared;
				Prepared.SetNum(Count);
				ParallelFor(Count, [&](int32 i)
				{
					Prepared[i] = MakeUnique<FPreparedMesh>();
					PrepareRequest(Requests[First + i], bVertexMaterial, *Prepared[i]);
				}, EParallelForFlags::Unbalanced);
				FScopeLock Scope(&Prewarm.Lock);
				for (int32 i = Count - 1; i >= 0; --i)	// (taken from the back)
				{
					Prewarm.Ready.Insert(TPair<uint64, TUniquePtr<FPreparedMesh>>(Requests[First + i].Key, MoveTemp(Prepared[i])), 0);
				}
			}
		});
	}
}

namespace
{
	const FTDMergedMesh& ModelPivotMesh(ETDModel Id, int32 PivotIndex, bool bSolid, float MinSize, const FVector& RootSpread, bool bDetailed, bool bSculpted)
	{
		// a model the background prewarm hasn't finished yet: wait for it rather than merge it twice
		if (ModelPrewarm().bActive && !MergedMeshes().Contains(bSculpted ? SculptedPivotKey(Id, PivotIndex, bSolid, RootSpread) : ModelPivotKey(Id, PivotIndex, bSolid, MinSize, RootSpread, bDetailed)))
		{
			TD::FinishModelPrewarm();
		}
		if (bSculpted)
		{
			const uint64 SculptedKey = SculptedPivotKey(Id, PivotIndex, bSolid, RootSpread);
			if (const FTDMergedMesh* Found = MergedMeshes().Find(SculptedKey))
			{
				return *Found;
			}
			FPreparedMesh Prepared;
			if (const FBakedMesh* Baked = FindBakedMeshAt(Id, PivotIndex, bSolid, RootSpread))
			{
				PrepareBakedMesh(*Baked, HasDetailMaterial(), BakedSurface(Id), Prepared);
			}
			return MergedMeshes().Add(SculptedKey, FinishMergedMesh(Prepared));
		}
		const uint64 Key = ModelPivotKey(Id, PivotIndex, bSolid, MinSize, RootSpread, bDetailed);
		TArray<FTDMergedPart> Parts;
		if (!MergedMeshes().Contains(Key))
		{
			ModelPivotParts(Id, PivotIndex, bSolid, MinSize, RootSpread, bDetailed, Parts);
		}
		return TD::GetMergedMesh(Key, Parts);
	}

	const FName& ShadowCopyTag()
	{
		static const FName Tag(TEXT("ATDShadowCopy"));
		return Tag;
	}

	TAutoConsoleVariable<int32> CVarModelLods(
		TEXT("atd.ModelLods"),
		1,
		TEXT("1 gives merged meshes coarser levels of detail for zoomed-out views (round shapes with fewer sides); 0 only the full mesh. Takes effect for meshes merged afterwards."));

	TAutoConsoleVariable<int32> CVarShadowCopies(
		TEXT("atd.ShadowCopies"),
		1,
		TEXT("1 casts model shadows from plainer one-section copies (fewer triangles and draws in the shadow pass); 0 from the full meshes. Takes effect for models built afterwards."));

	TAutoConsoleVariable<int32> CVarSculptedModels(
		TEXT("atd.SculptedModels"),
		1,
		TEXT("1 loads the sculpted models baked from the way-more-detail preview (Content/ATDModels) in place of the detailed shapes, where a model has one; 0 always builds from the shapes. Takes effect for models built afterwards."));

	/** Whether a model shows as its sculpted mesh: only with the detailed models on, at the normal spread (the spike field at any),
	 *  and when its file loaded */
	bool ShowSculpted(ETDModel Id, const FVector& RootSpread, bool bDetailed)
	{
		return bDetailed && TD::UseSculptedModels() && (RootSpread.Equals(FVector::OneVector) || Id == ETDModel::Spines) && GetBakedModel(Id).Meshes.Num() > 0;
	}

	TAutoConsoleVariable<int32> CVarDetailedModels(
		TEXT("atd.DetailedModels"),
		1,
		TEXT("1 builds towers, creatures and props with the detailed shapes (rounded edges, smoother curves, extra detail parts); 0 the original ones. Takes effect for models built afterwards."));
}

bool TD::UseSculptedModels()
{
	return CVarSculptedModels.GetValueOnGameThread() != 0;
}

bool TD::UseDetailedModels()
{
	return CVarDetailedModels.GetValueOnGameThread() != 0;
}

const TCHAR* TD::ModelName(ETDModel Id)
{
	static const TCHAR* const Names[] = {
		TEXT("Harpoon"), TEXT("Jelly"), TEXT("Puffer"), TEXT("Eel"), TEXT("Mantis Shrimp"), TEXT("Octopus"), TEXT("Lionfish"), TEXT("Sawfish"),
		TEXT("Humpback"), TEXT("Acid Vent"), TEXT("Crab"), TEXT("Piranha"), TEXT("Turtle"), TEXT("Manta"), TEXT("Kraken"), TEXT("Sea Star"),
		TEXT("Cuttlefish"), TEXT("Shock Eel"), TEXT("Boulders"), TEXT("Trench"), TEXT("Goal"), TEXT("Kelp Short"), TEXT("Kelp Mid"), TEXT("Kelp Tall"),
		TEXT("Shot Harpoon"), TEXT("Shot Rock"), TEXT("Shot Lionfish"), TEXT("Shot Sawfish"), TEXT("Shot Bomb"), TEXT("Spines"),
		TEXT("Drop Shell"), TEXT("Drop Pearl"), TEXT("Drop Charm"),
		TEXT("Harpoon: Swordfish"), TEXT("Harpoon: Narwhal"), TEXT("Jelly: Box"), TEXT("Jelly: Moon"), TEXT("Puffer: Cluster"), TEXT("Puffer: Lobber"),
		TEXT("Eel: Storm"), TEXT("Eel: Overload"), TEXT("Mantis: Smasher"), TEXT("Mantis: Spearer"), TEXT("Octopus: Juggler"), TEXT("Octopus: Blue-ringed"),
		TEXT("Lionfish: Cloud"), TEXT("Lionfish: Paralytic"), TEXT("Sawfish: Ripper"), TEXT("Sawfish: Sweeper"), TEXT("Humpback: Sonar"), TEXT("Humpback: Chorus"),
		TEXT("Acid Vent: Cold"), TEXT("Acid Vent: Black") };
	static_assert(UE_ARRAY_COUNT(Names) == (int32)ETDModel::Count, "one name per model");
	return Names[FMath::Clamp((int32)Id, 0, (int32)ETDModel::Count - 1)];
}

const FTDMergedMesh& TD::GetMergedMesh(uint64 Key, const TArray<FTDMergedPart>& Parts)
{
	SharedDetailMaterial(FLinearColor::White, ETDSurface::Plain);	// settles HasDetailMaterial on the game thread
	if (const FTDMergedMesh* Found = MergedMeshes().Find(Key))
	{
		return *Found;
	}
	return MergedMeshes().Add(Key, BuildMergedMesh(Parts));
}

const FTDMergedMesh* TD::FindMergedMesh(uint64 Key)
{
	return MergedMeshes().Find(Key);
}

namespace
{
	/** A merged mesh being prepared in the background, and who waits for it */
	struct FAsyncMergedMesh
	{
		TArray<TFunction<void()>> OnReady;
		TSharedPtr<FPreparedMesh> Prepared;	// set (on the game thread) once the worker is done
	};

	TMap<uint64, FAsyncMergedMesh>& AsyncMergedMeshes()
	{
		static TMap<uint64, FAsyncMergedMesh> Pending;
		return Pending;
	}

	/** Requests waiting for a worker: only a few build at once, so a burst of sculpted walls (each a few hundred ms of work)
	 *  never takes every core from the frame's own work. Game thread only. */
	struct FWaitingMergedMesh
	{
		uint64 Key = 0;
		TArray<FTDMergedPart> Parts;
	};
	TArray<FWaitingMergedMesh>& WaitingMergedMeshes()
	{
		static TArray<FWaitingMergedMesh> Waiting;
		return Waiting;
	}
	int32 RunningMergedMeshes = 0;

	/** The workers' tasks, so quitting waits for any still sculpting rather than tearing the engine down under them */
	TArray<UE::Tasks::FTask>& MergedMeshTasks()
	{
		static TArray<UE::Tasks::FTask> Tasks;
		return Tasks;
	}

	int32 MaxRunningMergedMeshes()
	{
		return FMath::Clamp(FPlatformMisc::NumberOfCoresIncludingHyperthreads() / 6, 2, 6);
	}

	void LaunchMergedMeshes()
	{
		TArray<FWaitingMergedMesh>& Waiting = WaitingMergedMeshes();
		while (Waiting.Num() > 0 && RunningMergedMeshes < MaxRunningMergedMeshes())
		{
			FWaitingMergedMesh Next = MoveTemp(Waiting[0]);
			Waiting.RemoveAt(0);
			++RunningMergedMeshes;
			MergedMeshTasks().RemoveAll([](const UE::Tasks::FTask& Task) { return Task.IsCompleted(); });
			MergedMeshTasks().Add(UE::Tasks::Launch(UE_SOURCE_LOCATION, [Key = Next.Key, Parts = MoveTemp(Next.Parts)]()
			{
				TSharedPtr<FPreparedMesh> Prepared = MakeShared<FPreparedMesh>();
				PrepareMergedMesh(Parts, *Prepared);
				PrebuildRenderData(*Prepared);
				AsyncTask(ENamedThreads::GameThread, [Key, Prepared]()
				{
					--RunningMergedMeshes;
					if (FAsyncMergedMesh* Found = AsyncMergedMeshes().Find(Key))
					{
						Found->Prepared = Prepared;
					}
					LaunchMergedMeshes();
				});
			}, UE::Tasks::ETaskPriority::BackgroundLow));
		}
	}

	/** Finishes one prepared mesh per frame, so a batch landing together doesn't hitch either */
	bool FinishOneAsyncMergedMesh(float)
	{
		TMap<uint64, FAsyncMergedMesh>& Pending = AsyncMergedMeshes();
		TArray<TFunction<void()>> OnReady;
		for (auto It = Pending.CreateIterator(); It; ++It)
		{
			if (It->Value.Prepared)
			{
				if (!MergedMeshes().Contains(It->Key))
				{
					MergedMeshes().Add(It->Key, FinishMergedMesh(*It->Value.Prepared));
				}
				OnReady = MoveTemp(It->Value.OnReady);
				It.RemoveCurrent();
				break;
			}
		}
		// (after the loop: a callback may ask for another mesh)
		for (const TFunction<void()>& Callback : OnReady)
		{
			if (Callback)
			{
				Callback();
			}
		}
		return true;
	}
}

void TD::RequestMergedMeshAsync(uint64 Key, TArray<FTDMergedPart>&& Parts, TFunction<void()> OnReady)
{
	check(IsInGameThread());
	if (MergedMeshes().Contains(Key))
	{
		if (OnReady)
		{
			OnReady();
		}
		return;
	}
	if (FAsyncMergedMesh* Found = AsyncMergedMeshes().Find(Key))
	{
		if (OnReady)
		{
			// now someone is waiting for it: build it next
			TArray<FWaitingMergedMesh>& Waiting = WaitingMergedMeshes();
			const int32 Index = Waiting.IndexOfByPredicate([Key](const FWaitingMergedMesh& W) { return W.Key == Key; });
			if (Index > 0)
			{
				FWaitingMergedMesh Moved = MoveTemp(Waiting[Index]);
				Waiting.RemoveAt(Index);
				Waiting.Insert(MoveTemp(Moved), 0);
			}
		}
		Found->OnReady.Add(MoveTemp(OnReady));
		return;
	}

	static bool bTickerAdded = false;
	if (!bTickerAdded)
	{
		bTickerAdded = true;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&FinishOneAsyncMergedMesh));
		FCoreDelegates::OnPreExit.AddLambda([]()
		{
			// drop what hasn't started and let the running ones finish (each well under a second)
			WaitingMergedMeshes().Empty();
			UE::Tasks::Wait(MergedMeshTasks());
			MergedMeshTasks().Empty();
		});
	}

	SharedDetailMaterial(FLinearColor::White, ETDSurface::Plain);	// settles HasDetailMaterial on the game thread before the worker reads it
	MakeSlotTemplates();
	// a mesh someone is waiting to show (a wall just placed) goes ahead of the prewarm batch
	const int32 At = OnReady ? 0 : WaitingMergedMeshes().Num();
	AsyncMergedMeshes().Add(Key).OnReady.Add(MoveTemp(OnReady));
	WaitingMergedMeshes().Insert(FWaitingMergedMesh{ Key, MoveTemp(Parts) }, At);
	LaunchMergedMeshes();
}

const FTDMergedMesh& TD::GetDetailedMergedMesh(uint64 Id, const TArray<FTDMergedPart>& Parts)
{
	if (!UseDetailedModels())
	{
		return GetMergedMesh(MakeMergedKey(Id), Parts);
	}
	const uint64 Key = MakeMergedKey(Id | (1ull << 40));
	if (const FTDMergedMesh* Found = MergedMeshes().Find(Key))
	{
		return *Found;
	}
	TArray<FTDMergedPart> Detailed = Parts;
	TDModelDetail::AddDetail((int32)(Id & 0x7FFFF), 0.0f, Detailed);
	return GetMergedMesh(Key, Detailed);
}

const FTDMergedMesh* TD::GetBakedPropMesh(const TCHAR* Name)
{
	if (!UseDetailedModels() || !UseSculptedModels())
	{
		return nullptr;
	}
	const uint64 Key = BakedPropKey(FString(Name));
	if (const FTDMergedMesh* Found = MergedMeshes().Find(Key))
	{
		return Found->Mesh ? Found : nullptr;
	}
	const FString Path = BakedPropPath(FString(Name));
	TArray<FBakedMesh> Baked;
	if (!FPaths::FileExists(Path) || !ReadBakedModel(Path, Baked) || Baked.Num() == 0)
	{
		if (FPaths::FileExists(Path))
		{
			UE_LOG(LogAquaticTowerDefense, Warning, TEXT("Could not read the sculpted prop %s, using the detailed shapes"), *Path);
		}
		MergedMeshes().Add(Key, FTDMergedMesh());	// remembered as missing
		return nullptr;
	}
	SharedDetailMaterial(FLinearColor::White, ETDSurface::Plain);	// settles HasDetailMaterial first
	FPreparedMesh Prepared;
	PrepareBakedMesh(Baked[0], HasDetailMaterial(), ETDSurface::Plain, Prepared);
	const FTDMergedMesh& Merged = MergedMeshes().Add(Key, FinishMergedMesh(Prepared));
	return Merged.Mesh ? &Merged : nullptr;
}

namespace
{
	/** A surface's mesh description: one section, "Slot0" */
	void DescribeSurface(const FTDSculptMesh& Surface, FMeshDescription& Description)
	{
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();
		Description.ReserveNewVertices(Surface.Positions.Num());
		Description.ReserveNewVertexInstances(Surface.Positions.Num());
		Description.ReserveNewTriangles(Surface.Triangles.Num() / 3);
		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Slot0");
		TArray<FVertexInstanceID> Instances;
		AppendSculpt(Description, Attributes, Group, Surface, Instances);
	}

	/** A surface job's geometry and render data, made on a worker thread */
	struct FPreparedSurface
	{
		bool bOk = false;
		FMeshDescription Description;
		TUniquePtr<FStaticMeshRenderData> RenderData;
		FBoxSphereBounds Bounds;
	};

	/** Worker thread (after MakeSlotTemplates) */
	void PrepareSurface(const FTDSurfaceJob& Job, FPreparedSurface& Out)
	{
		FTDSculptMesh Surface;
		if (Job.Build)
		{
			Job.Build(Surface);
		}
		if (Surface.Triangles.Num() == 0)
		{
			return;
		}
		DescribeSurface(Surface, Out.Description);
		if (const UStaticMesh* Template = GSlotTemplate.load())
		{
			Out.RenderData = BuildRenderData(Template, &Out.Description, 1, Out.Bounds);
		}
		Out.bOk = true;
	}

	/** Game thread */
	UStaticMesh* FinishSurface(FPreparedSurface& Prepared, ETDSurface Look)
	{
		UMaterialInterface* Material = Prepared.bOk ? TD::SharedDetailMaterial(FLinearColor::White, Look) : nullptr;
		if (!Material)
		{
			return nullptr;
		}
		UStaticMesh* Mesh = NewRuntimeMesh();
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Slot0")));
		if (Prepared.RenderData)
		{
			FinishPrebuiltMesh(Mesh, MoveTemp(Prepared.RenderData), Prepared.Bounds, 1);
		}
		else
		{
			FinishRuntimeMesh(Mesh, { &Prepared.Description });
		}
		return Mesh;
	}

	/** Starts preparing every surface job on the worker threads; wait on the tasks before reading Out */
	TArray<UE::Tasks::FTask> LaunchSurfaces(const TArray<FTDSurfaceJob>& Jobs, TArray<FPreparedSurface>& Out)
	{
		MakeSlotTemplates();
		Out.SetNum(Jobs.Num());
		TArray<UE::Tasks::FTask> Tasks;
		for (int32 i = 0; i < Jobs.Num(); ++i)
		{
			Tasks.Add(UE::Tasks::Launch(UE_SOURCE_LOCATION, [&Jobs, &Out, i]() { PrepareSurface(Jobs[i], Out[i]); }));
		}
		return Tasks;
	}

	void FinishSurfaces(const TArray<FTDSurfaceJob>& Jobs, TArray<FPreparedSurface>& Prepared)
	{
		for (int32 i = 0; i < Jobs.Num(); ++i)
		{
			UStaticMesh* Mesh = FinishSurface(Prepared[i], Jobs[i].Look);
			if (Jobs[i].OnBuilt)
			{
				Jobs[i].OnBuilt(Mesh);
			}
		}
	}
}

UStaticMesh* TD::MakeSurfaceMesh(const FTDSculptMesh& Surface, ETDSurface Look)
{
	if (Surface.Triangles.Num() == 0)
	{
		return nullptr;
	}
	UMaterialInterface* Material = SharedDetailMaterial(FLinearColor::White, Look);
	if (!Material)
	{
		return nullptr;
	}
	FMeshDescription Description;
	DescribeSurface(Surface, Description);
	UStaticMesh* Mesh = NewRuntimeMesh();
	Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Slot0")));
	FinishRuntimeMesh(Mesh, { &Description });
	return Mesh;
}

void TD::MakeSurfaceMeshes(TArray<FTDSurfaceJob>&& Jobs)
{
	SharedDetailMaterial(FLinearColor::White, ETDSurface::Plain);	// settles HasDetailMaterial on the game thread
	TArray<FPreparedSurface> Prepared;
	UE::Tasks::Wait(LaunchSurfaces(Jobs, Prepared));
	FinishSurfaces(Jobs, Prepared);
}

UStaticMesh* TD::UnitShapeMesh(ETDShape Shape, float Size)
{
	FTDMergedPart Part;
	Part.Shape = Shape;
	Part.Segments = SegmentsForSize(Size);
	const FTDMergedMesh& Merged = GetMergedMesh(MakeMergedKey(1000 + (uint64)Shape * 100 + Part.Segments), { Part });
	return Merged.Mesh;
}

void TD::PrewarmModels(const FTDPrewarmExtras& Extras)
{
	SharedDetailMaterial(FLinearColor::White, ETDSurface::Plain);	// settles HasDetailMaterial before the worker threads prepare meshes
	MakeSlotTemplates();
	const bool bDetailed = UseDetailedModels();
	const bool bSculptOn = UseSculptedModels();
	const bool bVertexMaterial = HasDetailMaterial();
	FinishModelPrewarm();	// (a second prewarm after a first one)

	// what the first board shows, now: its rocks, trench, clam and kelp (with ATDGrid's PropOptions minimum part size), the baked
	// seafloor props and the painted surfaces
	TArray<FPrewarmRequest> Board;
	GatherModelRequests(ETDModel::Boulders, ETDModel::KelpTall, 2.5f, bDetailed, bSculptOn, Board);
	if (bDetailed && bSculptOn)
	{
		for (const FString& Name : Extras.BakedProps)
		{
			FPrewarmRequest& Request = Board.AddDefaulted_GetRef();
			Request.Key = BakedPropKey(Name);
			Request.BakedProp = Name;
		}
	}
	Board.RemoveAll([](const FPrewarmRequest& Request) { return MergedMeshes().Contains(Request.Key); });

	// the painted surfaces are the biggest single jobs: they run alongside the props
	TArray<FPreparedSurface> PreparedSurfaces;
	const TArray<UE::Tasks::FTask> SurfaceTasks = LaunchSurfaces(Extras.Surfaces, PreparedSurfaces);
	TArray<FPreparedMesh> Prepared;
	Prepared.SetNum(Board.Num());
	ParallelFor(Board.Num(), [&](int32 i) { PrepareRequest(Board[i], bVertexMaterial, Prepared[i]); }, EParallelForFlags::Unbalanced);
	for (int32 i = 0; i < Board.Num(); ++i)
	{
		MergedMeshes().Add(Board[i].Key, FinishMergedMesh(Prepared[i]));
	}
	UE::Tasks::Wait(SurfaceTasks);
	FinishSurfaces(Extras.Surfaces, PreparedSurfaces);

	// the towers, creatures, shots and drops aren't needed behind the main menu: they're merged in the background
	StartModelPrewarm(bDetailed, bSculptOn, bVertexMaterial);
}

void TD::FinishModelPrewarm()
{
	FModelPrewarm& Prewarm = ModelPrewarm();
	if (!Prewarm.bActive)
	{
		return;
	}
	const double Start = FPlatformTime::Seconds();
	Prewarm.Task.Wait();
	while (FinishOnePrewarmed(Prewarm))
	{
	}
	EndModelPrewarm(Prewarm, (FPlatformTime::Seconds() - Start) * 1000.0);
}

void TD::PrewarmShadowMesh(const FTDMergedMesh& Merged)
{
	if (CVarShadowCopies.GetValueOnGameThread() != 0 && !Merged.ShadowMesh && Merged.ShadowParts.Num() > 0)
	{
		Merged.ShadowMesh = BuildShadowMesh(Merged.ShadowParts);
		Merged.ShadowParts.Empty();
	}
}

UStaticMeshComponent* TD::AddMergedMesh(AActor* Owner, USceneComponent* Parent, const FTDMergedMesh& Merged, bool bShadows,
	const TFunction<FLinearColor(const FLinearColor& Color, uint8 Flags)>& Recolor)
{
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
	Comp->SetStaticMesh(Merged.Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
	// the shadow comes from the plainer copy (built the first time one is needed), so the full mesh skips the shadow passes
	const bool bUseCopy = bShadows && CVarShadowCopies.GetValueOnGameThread() != 0;
	if (bUseCopy)
	{
		PrewarmShadowMesh(Merged);
	}
	UStaticMesh* ShadowMesh = bUseCopy ? Merged.ShadowMesh.Get() : nullptr;
	Comp->SetCastShadow(bShadows && !ShadowMesh);
	if (Recolor)
	{
		for (int32 i = 0; i < Merged.Slots.Num(); ++i)
		{
			const FTDMergedMesh::FSlot& Slot = Merged.Slots[i];
			Comp->SetMaterial(i, SlotMaterial(Recolor(Slot.Color, Slot.Flags), Slot.Mat, Slot.Color.A, Slot.Surface));
		}
	}
	Comp->RegisterComponent();
	Owner->AddInstanceComponent(Comp);

	if (ShadowMesh)
	{
		UStaticMeshComponent* Shadow = NewObject<UStaticMeshComponent>(Owner);
		Shadow->SetStaticMesh(ShadowMesh);
		Shadow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shadow->SetGenerateOverlapEvents(false);
		Shadow->SetCanEverAffectNavigation(false);
		Shadow->SetupAttachment(Comp);
		Shadow->SetHiddenInGame(true);
		Shadow->bCastHiddenShadow = true;
		Shadow->SetCastShadow(true);
		Shadow->bAffectDistanceFieldLighting = false;
		Shadow->bVisibleInRayTracing = false;
		Shadow->bVisibleInReflectionCaptures = false;
		Shadow->ComponentTags.Add(ShadowCopyTag());
		Shadow->RegisterComponent();
		Owner->AddInstanceComponent(Shadow);
	}
	return Comp;
}

void TD::DestroyMergedMesh(UStaticMeshComponent* Comp)
{
	if (!Comp)
	{
		return;
	}
	TArray<USceneComponent*> Children;
	Comp->GetChildrenComponents(false, Children);
	for (USceneComponent* Child : Children)
	{
		if (Child && Child->ComponentHasTag(ShadowCopyTag()))
		{
			Child->DestroyComponent();
		}
	}
	Comp->DestroyComponent();
}

void TD::SetShadowCopiesEnabled(AActor* Actor, bool bEnabled)
{
	if (!Actor)
	{
		return;
	}
	TInlineComponentArray<UStaticMeshComponent*> Comps(Actor);
	for (UStaticMeshComponent* Comp : Comps)
	{
		if (Comp->ComponentHasTag(ShadowCopyTag()))
		{
			Comp->SetCastShadow(bEnabled);
		}
	}
}

USceneComponent* FTDModelInstance::Find(FName Name) const
{
	const int32 Index = PivotNames.IndexOfByKey(Name);
	return Index != INDEX_NONE ? Pivots[Index] : nullptr;
}

TArray<USceneComponent*> FTDModelInstance::FindAll(FName Name) const
{
	TArray<USceneComponent*> Found;
	for (int32 i = 0; i < PivotNames.Num(); ++i)
	{
		if (PivotNames[i] == Name)
		{
			Found.Add(Pivots[i]);
		}
	}
	return Found;
}

UMaterialInterface* TD::SharedDetailMaterial(const FLinearColor& Color, ETDSurface Surface)
{
	static TStrongObjectPtr<UMaterialInterface> Base;
	if (GDetailMaterialState.load() < 0)
	{
		Base = TStrongObjectPtr<UMaterialInterface>(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AquaticTowerDefense/M_ATD_Detail.M_ATD_Detail")));
		GDetailMaterialState.store(Base.IsValid() ? 1 : 0);
	}
	if (!Base.IsValid() || Surface == ETDSurface::Auto)
	{
		return nullptr;
	}

	static TMap<uint64, TStrongObjectPtr<UMaterialInstanceDynamic>> Cache;
	auto Q = [](float V) { return (uint64)FMath::Clamp(FMath::RoundToInt(V * 128.0f), 0, 0xFFFF); };
	const uint64 Key = Q(Color.R) | (Q(Color.G) << 16) | (Q(Color.B) << 32) | ((uint64)Surface << 48);
	if (const TStrongObjectPtr<UMaterialInstanceDynamic>* Found = Cache.Find(Key))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}

	// each surface: roughness, metalness, specular, relief strength, relief scale (per uu), relief pattern and how much the relief tints the colour
	struct FPreset { float Roughness, Metallic, Specular, Bump, Scale, Pattern, Tone; };
	static const FPreset Presets[(int32)ETDSurface::Count] = {
		{ 0.5f, 0.0f, 0.5f, 0.15f, 0.5f, 0.0f, 0.0f },		// Auto (unused)
		{ 0.5f, 0.0f, 0.45f, 0.15f, 0.5f, 0.0f, 0.0f },		// Plain
		{ 0.45f, 0.0f, 0.5f, 0.15f, 0.8f, 1.0f, 0.0f },		// Skin
		{ 0.06f, 0.0f, 0.8f, 0.0f, 1.0f, 0.0f, 0.0f },		// Eye
		{ 0.85f, 0.0f, 0.4f, 0.35f, 0.22f, 2.0f, 0.2f },		// Stone
		{ 0.9f, 0.0f, 0.4f, 0.2f, 0.6f, 2.0f, 0.1f },		// Sand
		{ 0.65f, 0.0f, 0.45f, 0.3f, 0.35f, 3.0f, 0.25f },		// Wood
		{ 0.45f, 0.2f, 0.5f, 0.05f, 1.5f, 0.0f, 0.0f },		// Metal
		{ 0.4f, 0.25f, 0.5f, 0.04f, 1.5f, 0.0f, 0.0f },		// Brass
		{ 0.95f, 0.0f, 0.3f, 0.45f, 1.4f, 1.0f, 0.15f },		// Moss
		{ 0.4f, 0.0f, 0.5f, 0.2f, 1.0f, 4.0f, 0.1f },		// Shell
		{ 0.6f, 0.0f, 0.45f, 0.25f, 0.9f, 1.0f, 0.1f },		// Coral
		{ 0.6f, 0.0f, 0.45f, 0.4f, 0.5f, 5.0f, 0.15f },		// Brain
	};
	const FPreset& P = Presets[FMath::Clamp((int32)Surface, 0, (int32)ETDSurface::Count - 1)];
	FLinearColor Tint(Color.R, Color.G, Color.B, 1.0f);
	if (Surface == ETDSurface::Brass)
	{
		Tint = FMath::Lerp(Tint, FLinearColor(0.83f, 0.6f, 0.25f, 1.0f), 0.35f);
	}
	else if (Surface == ETDSurface::Metal)
	{
		Tint = FLinearColor(FMath::Min(Tint.R * 1.15f, 1.0f), FMath::Min(Tint.G * 1.15f, 1.0f), FMath::Min(Tint.B * 1.15f, 1.0f), 1.0f);
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base.Get(), GetTransientPackage());
	MID->SetVectorParameterValue(TEXT("Color"), Tint);
	MID->SetScalarParameterValue(TEXT("Roughness"), P.Roughness);
	MID->SetScalarParameterValue(TEXT("Metallic"), P.Metallic);
	MID->SetScalarParameterValue(TEXT("Specular"), P.Specular);
	MID->SetScalarParameterValue(TEXT("Bump"), P.Bump);
	MID->SetScalarParameterValue(TEXT("BumpScale"), P.Scale);
	MID->SetScalarParameterValue(TEXT("Pattern"), P.Pattern);
	MID->SetScalarParameterValue(TEXT("Tone"), P.Tone);
	Cache.Add(Key, TStrongObjectPtr<UMaterialInstanceDynamic>(MID));
	return MID;
}

UMaterialInterface* TD::SharedFxMaterial(const FLinearColor& Color, float Opacity)
{
	static TMap<uint64, TStrongObjectPtr<UMaterialInstanceDynamic>> Cache;

	auto Q = [](float V) { return (uint64)FMath::Clamp(FMath::RoundToInt(V * 128.0f), 0, 0xFFFF); };
	const uint64 Key = Q(Color.R) | (Q(Color.G) << 16) | (Q(Color.B) << 32) | (Q(Opacity) << 48);
	if (const TStrongObjectPtr<UMaterialInstanceDynamic>* Found = Cache.Find(Key))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}

	// the range-ring material is unlit and translucent with Color and Opacity parameters
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AquaticTowerDefense/M_ATD_Range.M_ATD_Range"));
	if (!Base)
	{
		return SharedColorMaterial(Color);
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	Cache.Add(Key, TStrongObjectPtr<UMaterialInstanceDynamic>(MID));
	return MID;
}

FTDModelInstance TD::BuildModel(AActor* Owner, USceneComponent* Parent, ETDModel Id, const FTDModelOptions& Options)
{
	const FTDModel& Model = GetModel(Id);
	FTDModelInstance Out;
	Out.Pivots.Reserve(Model.NumPivots);
	Out.PivotNames.Reserve(Model.NumPivots);

	for (int32 i = 0; i < Model.NumPivots; ++i)
	{
		const FTDModelPivot& P = Model.Pivots[i];
		USceneComponent* Pivot = Parent;
		if (P.Parent >= 0)
		{
			const FVector Location(P.L[0], P.L[1], P.L[2]);
			Pivot = AddPivot(Owner, Out.Pivots[P.Parent], P.Parent == 0 ? Location * Options.RootSpread : Location);
			Pivot->SetRelativeRotation(FQuat(P.Q[0], P.Q[1], P.Q[2], P.Q[3]));
		}
		Out.Pivots.Add(Pivot);
		Out.PivotNames.Add(FName(P.Name));
	}

	ForEachModelMesh(Id, Options.MinSize, Options.RootSpread, [&](int32 PivotIndex, bool bSolid, const FTDMergedMesh& Merged)
	{
		UStaticMeshComponent* Comp = AddMergedMesh(Owner, Out.Pivots[PivotIndex], Merged, Options.bShadows && bSolid, Options.Recolor);
		if (Options.OnPart)
		{
			Options.OnPart(Comp);
		}
	}, Options.Detail);
	return Out;
}

void TD::ForEachModelMesh(ETDModel Id, float MinSize, const FVector& RootSpread, TFunctionRef<void(int32 PivotIndex, bool bSolid, const FTDMergedMesh& Merged)> Visit, int32 Detail)
{
	SharedDetailMaterial(FLinearColor::White, ETDSurface::Plain);	// settles HasDetailMaterial before any sculpted mesh is made
	const bool bDetailed = Detail < 0 ? UseDetailedModels() : Detail > 0;
	const bool bSculpted = ShowSculpted(Id, RootSpread, bDetailed);
	const int32 NumPivots = GetModel(Id).NumPivots;
	for (int32 PivotIndex = 0; PivotIndex < NumPivots; ++PivotIndex)
	{
		for (const bool bSolid : { false, true })
		{
			const FTDMergedMesh& Merged = ModelPivotMesh(Id, PivotIndex, bSolid, MinSize, RootSpread, bDetailed, bSculpted);
			if (Merged.Mesh)
			{
				Visit(PivotIndex, bSolid, Merged);
			}
		}
	}
}
