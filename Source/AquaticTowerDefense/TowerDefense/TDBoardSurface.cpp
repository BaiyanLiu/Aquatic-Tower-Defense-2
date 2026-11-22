// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDBoardSurface.h"

namespace
{
	// ---------------------------------------------------------------- noise

	float Hash2(int32 X, int32 Y, int32 Seed)
	{
		uint32 H = (uint32)X * 374761393u + (uint32)Y * 668265263u + (uint32)Seed * 1274126177u;
		H = (H ^ (H >> 13)) * 1274126177u;
		return (float)((H ^ (H >> 16)) & 0xFFFFFF) / (float)0xFFFFFF;
	}

	/** Smooth value noise in 0..1 with features about Cell units apart */
	float ValueNoise(float X, float Y, float Cell, int32 Seed)
	{
		const float FX = X / Cell, FY = Y / Cell;
		const int32 IX = FMath::FloorToInt(FX), IY = FMath::FloorToInt(FY);
		float TX = FX - IX, TY = FY - IY;
		TX = TX * TX * (3.0f - 2.0f * TX);
		TY = TY * TY * (3.0f - 2.0f * TY);
		const float A = Hash2(IX, IY, Seed), B = Hash2(IX + 1, IY, Seed), C = Hash2(IX, IY + 1, Seed), D = Hash2(IX + 1, IY + 1, Seed);
		return FMath::Lerp(FMath::Lerp(A, B, TX), FMath::Lerp(C, D, TX), TY);
	}

	float Fbm(float X, float Y, float Cell, int32 Octaves, int32 Seed)
	{
		float Sum = 0.0f, Amp = 1.0f, Total = 0.0f;
		for (int32 o = 0; o < Octaves; ++o)
		{
			Sum += Amp * ValueNoise(X, Y, FMath::Max(0.5f, Cell / (float)(1 << o)), Seed + o * 31);
			Total += Amp;
			Amp *= 0.5f;
		}
		return Sum / Total;
	}

	float Smooth(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Asymmetric sand ripples (gentle stoss, steep lee) whose crests wander and fork a little; 0..1 */
	float Ripples(float X, float Y, float Scale, int32 Seed, float DirX = 0.83f, float DirY = 0.55f)
	{
		const float Warp = (Fbm(X, Y, Scale * 2.2f, 3, Seed) - 0.5f) * Scale * 2.6f;
		const float T = (X * DirX + Y * DirY + Warp) / Scale;
		const float F = T - FMath::FloorToFloat(T);
		const float Profile = Smooth(0.0f, 1.0f, F < 0.72f ? F / 0.72f : (1.0f - F) / 0.28f);
		return Profile * (0.55f + 0.45f * Fbm(X, Y, Scale * 4.0f, 2, Seed + 5));
	}

	FLinearColor Mul(const FLinearColor& A, const FLinearColor& B) { return FLinearColor(A.R * B.R, A.G * B.G, A.B * B.B, 1.0f); }

	/** The mesh build sRGB-encodes vertex colours and the detail material reads them raw, so linear colours are decoded once first (as TDSculpt does) */
	FVector4f Encode(const FLinearColor& C)
	{
		auto Decode = [](float X) { X = FMath::Clamp(X, 0.0f, 1.0f); return X <= 0.04045f ? X / 12.92f : FMath::Pow((X + 0.055f) / 1.055f, 2.4f); };
		return FVector4f(Decode(C.R), Decode(C.G), Decode(C.B), 1.0f);
	}

	/** A grid of quads, wound like TDSculpt's (cross(C - A, B - A) along the normal) */
	void AddGridTriangles(FTDSculptMesh& Out, int32 First, int32 NU, int32 NV, bool bFlip)
	{
		for (int32 j = 0; j < NV - 1; ++j)
		{
			for (int32 i = 0; i < NU - 1; ++i)
			{
				const int32 A = First + j * NU + i, B = A + 1, C = A + NU + 1, D = A + NU;
				// pick the winding from the first corner's normal so every face points out
				const FVector3f Face = FVector3f::CrossProduct(Out.Positions[C] - Out.Positions[A], Out.Positions[B] - Out.Positions[A]);
				const bool bOut = (FVector3f::DotProduct(Face, Out.Normals[A]) >= 0.0f) != bFlip;
				if (bOut)
				{
					Out.Triangles.Append({ A, B, C, A, C, D });
				}
				else
				{
					Out.Triangles.Append({ A, C, B, A, D, C });
				}
			}
		}
	}

	struct FPebble
	{
		FVector2f P;
		float RX, RY, Angle, Height;
		FLinearColor Color;
	};

	struct FShellBit
	{
		FVector2f P;
		float R, Angle, Span;
		FLinearColor Color;
	};
}

TDBoardSurface::FStyle TDBoardSurface::StyleFor(int32 Map, const FLinearColor& Tint)
{
	FStyle S;
	S.Tint = Tint;
	S.Seed = 1 + FMath::Max(Map, 0);
	switch (Map)
	{
	case 0: S.Algae = 0.6f; S.Shells = 2.2f; S.Pebbles = 0.8f; break;			// Reef Channel: shelly sand
	case 1: S.Algae = 0.4f; S.Shells = 0.5f; S.Pebbles = 1.6f; break;			// Twin Trench: gravelly
	case 2: S.Algae = 2.2f; S.Shells = 0.8f; S.Pebbles = 1.0f; break;			// Kelp Crossing: green film at the edges
	case 3: S.Algae = 0.8f; S.Shells = 0.6f; S.Pebbles = 0.7f; S.bRuins = true; break;	// Sunken Ruins: carved flagstones
	default: break;
	}
	return S;
}

void TDBoardSurface::BuildTiles(const FStyle& Style, TFunctionRef<bool(const FIntPoint&)> OnBoard, TFunctionRef<FVector(const FIntPoint&)> CellCentre,
	int32 NumCols, int32 NumRows, FTDSculptMesh& Out)
{
	constexpr float Tile = 97.0f;		// as today's tile cubes
	constexpr float Depth = 12.0f;
	constexpr int32 N = 33;				// vertices along a tile side (3 uu apart)
	constexpr float Lip = 4.5f;			// rounded, worn edge
	const float Step = Tile / (N - 1);
	const int32 Seed = Style.Seed;

	const FLinearColor PebbleColors[] = { FLinearColor(0.26f, 0.32f, 0.3f), FLinearColor(0.18f, 0.21f, 0.23f), FLinearColor(0.45f, 0.42f, 0.32f), FLinearColor(0.11f, 0.13f, 0.15f) };
	FRandomStream TileRand(1337);	// the same per-tile colour steps as today's tiles
	FRandomStream Scatter(Seed + 9);

	for (int32 Y = 0; Y < NumRows; ++Y)
	{
		for (int32 X = 0; X < NumCols; ++X)
		{
			const FIntPoint Cell(X, Y);
			const float Checker = ((X + Y) % 2 == 0) ? 0.0f : 0.025f;
			const float Noise = TileRand.FRandRange(-0.025f, 0.025f);
			if (!OnBoard(Cell))
			{
				continue;
			}
			const FVector C3 = CellCentre(Cell);
			const FVector2f Centre((float)C3.X, (float)C3.Y);
			const FLinearColor Base = Mul(FLinearColor(0.2f + Noise + Checker, 0.3f + Noise + Checker, 0.27f + Noise * 0.5f), Style.Tint);

			// this tile's pebbles and shell bits, kept off the worn edge where they would hang over
			TArray<FPebble> Pebbles;
			TArray<FShellBit> Shells;
			const int32 Small = FMath::RoundToInt(3.0f * Style.Pebbles * Scatter.FRandRange(0.6f, 1.4f));
			const int32 Large = Scatter.FRand() < 0.5f * Style.Pebbles ? 1 : 0;
			for (int32 i = 0; i < Small + Large; ++i)
			{
				const bool bLarge = i >= Small;
				const float R = bLarge ? Scatter.FRandRange(2.6f, 4.0f) : Scatter.FRandRange(1.4f, 2.6f);
				const float Margin = (bLarge ? 14.0f : 8.0f) + R;
				FPebble& P = Pebbles.AddDefaulted_GetRef();
				P.P = Centre + FVector2f(Scatter.FRandRange(-Tile * 0.5f + Margin, Tile * 0.5f - Margin), Scatter.FRandRange(-Tile * 0.5f + Margin, Tile * 0.5f - Margin));
				P.RX = R * Scatter.FRandRange(0.8f, 1.25f);
				P.RY = R * Scatter.FRandRange(0.7f, 1.0f);
				P.Angle = Scatter.FRandRange(0.0f, PI);
				P.Height = R * 0.35f;
				P.Color = Mul(PebbleColors[Scatter.RandRange(0, 3)], Style.Tint) * Scatter.FRandRange(0.85f, 1.12f);
			}
			const int32 NumShells = FMath::RoundToInt(0.5f * Style.Shells * Scatter.FRandRange(0.4f, 1.6f));
			for (int32 i = 0; i < NumShells; ++i)
			{
				FShellBit& S = Shells.AddDefaulted_GetRef();
				S.R = 4.0f * Scatter.FRandRange(0.7f, 1.3f);
				S.P = Centre + FVector2f(Scatter.FRandRange(-Tile * 0.5f + 10.0f, Tile * 0.5f - 10.0f), Scatter.FRandRange(-Tile * 0.5f + 10.0f, Tile * 0.5f - 10.0f));
				S.Angle = Scatter.FRandRange(0.0f, 2.0f * PI);
				S.Span = Scatter.FRandRange(0.6f, 1.0f);
				S.Color = Scatter.FRand() < 0.6f ? FLinearColor(0.91f, 0.71f, 0.6f) : FLinearColor(0.89f, 0.52f, 0.48f);
			}

			// the top: height and colour per vertex
			const int32 First = Out.Positions.Num();
			TArray<float> Heights;
			Heights.SetNumUninitialized(N * N);
			TArray<FLinearColor> Colors;
			Colors.SetNumUninitialized(N * N);
			for (int32 j = 0; j < N; ++j)
			{
				for (int32 i = 0; i < N; ++i)
				{
					const float LX = -Tile * 0.5f + i * Step, LY = -Tile * 0.5f + j * Step;
					const float WX = Centre.X + LX, WY = Centre.Y + LY;
					const float U = (LX + Tile * 0.5f) / Tile, V = (LY + Tile * 0.5f) / Tile;
					const float Edge = FMath::Min(FMath::Min(U, 1.0f - U), FMath::Min(V, 1.0f - V)) * Tile;	// uu to the nearest edge

					const float Mott = Fbm(WX, WY, 60.0f, 4, Seed) - 0.5f;
					const float Grain = ValueNoise(WX, WY, 2.0f, Seed + 1) - 0.5f;
					const float Rip = Ripples(WX, WY, 16.0f, Seed + 2);
					float H = Rip * 0.5f + Mott * 1.0f;
					// broad mottling, fine grain and ripple crests (in linear light, so about twice the painted sheet's strength)
					FLinearColor Col = Base * (1.0f + Mott * 0.32f + Grain * 0.12f + (Rip - 0.5f) * 0.16f) * (1.0f - 0.12f * (1.0f - Rip));
					// an algae film creeping in from some tile edges
					const float Alg = Smooth(0.62f, 0.8f, Fbm(WX, WY, 35.0f, 3, Seed + 3)) * Smooth(22.0f, 2.0f, Edge) * FMath::Min(Style.Algae, 2.5f);
					Col = FMath::Lerp(Col, Mul(FLinearColor(0.11f, 0.24f, 0.1f), Style.Tint) * (Col.GetLuminance() * 4.0f), FMath::Clamp(Alg * 0.25f, 0.0f, 0.6f));
					// the worn edge: rounded down and a touch paler where the stone shows
					const float LipT = Smooth(Lip, 0.0f, Edge);
					H -= FMath::Pow(LipT, 1.6f) * 2.5f;
					Col *= 1.0f + LipT * 0.18f;
					if (Style.bRuins)
					{
						// sunken flagstones: a softly carved groove framing the tile, paler worn stone in the middle
						const float Inset = FMath::Abs(FMath::Max(FMath::Abs(U - 0.5f), FMath::Abs(V - 0.5f)) - 0.36f);
						const float Groove = Smooth(0.03f, 0.008f, Inset);
						H -= Groove * 1.6f;
						Col *= 1.0f - Groove * 0.32f;
						Col *= 1.0f + Smooth(0.3f, 0.0f, FMath::Sqrt(FMath::Square(U - 0.5f) + FMath::Square(V - 0.5f))) * 0.12f;
					}
					// pebbles, half buried, with a contact shadow on the sand away from the light
					for (const FPebble& P : Pebbles)
					{
						const float DX = WX - P.P.X, DY = WY - P.P.Y;
						const float CA = FMath::Cos(P.Angle), SA = FMath::Sin(P.Angle);
						const float QX = (DX * CA + DY * SA) / P.RX, QY = (-DX * SA + DY * CA) / P.RY;
						const float Rr = FMath::Sqrt(QX * QX + QY * QY);
						const float SX = (DX - P.RX * 0.35f), SY = (DY - P.RY * 0.45f);
						const float Shadow = FMath::Clamp(1.0f - (FMath::Sqrt(SX * SX + SY * SY) / (P.RX * 1.3f)), 0.0f, 1.0f);
						if (Rr >= 1.0f)
						{
							Col *= 1.0f - 0.45f * Shadow * Shadow;
							continue;
						}
						const float Dome = FMath::Sqrt(1.0f - Rr * Rr);
						H = FMath::Max(H, Dome * P.Height + H * Rr);
						const float Speck = ValueNoise(WX, WY, 1.2f, Seed + 7) - 0.5f;
						const float Sand = Smooth(0.65f, 1.0f, Rr);	// sand drifted up round its foot
						Col = FMath::Lerp(P.Color * (1.0f + Speck * 0.5f), Col, Sand * 0.6f);
					}
					// broken scallop fragments
					for (const FShellBit& S : Shells)
					{
						const float DX = WX - S.P.X, DY = WY - S.P.Y;
						const float CA = FMath::Cos(S.Angle), SA = FMath::Sin(S.Angle);
						const float QX = DX * CA + DY * SA, QY = -DX * SA + DY * CA;
						const float Th = FMath::Atan2(QX, -QY), Rad = FMath::Sqrt(QX * QX + QY * QY);
						if (Rad < S.R * (1.0f + 0.06f * FMath::Cos(Th * 14.0f)) && FMath::Abs(Th) < S.Span && QY < S.R * 0.05f)
						{
							Col = S.Color * (0.8f + 0.2f * FMath::Cos(Th * 14.0f));
							H += 0.4f * S.R * 0.25f;
						}
					}
					Heights[j * N + i] = H;
					Colors[j * N + i] = Col;
				}
			}
			for (int32 j = 0; j < N; ++j)
			{
				for (int32 i = 0; i < N; ++i)
				{
					const float LX = -Tile * 0.5f + i * Step, LY = -Tile * 0.5f + j * Step;
					const float HL = Heights[j * N + FMath::Max(i - 1, 0)], HR = Heights[j * N + FMath::Min(i + 1, N - 1)];
					const float HD = Heights[FMath::Max(j - 1, 0) * N + i], HU = Heights[FMath::Min(j + 1, N - 1) * N + i];
					const float SpanX = (FMath::Min(i + 1, N - 1) - FMath::Max(i - 1, 0)) * Step, SpanY = (FMath::Min(j + 1, N - 1) - FMath::Max(j - 1, 0)) * Step;
					Out.Positions.Add(FVector3f(Centre.X + LX, Centre.Y + LY, Heights[j * N + i]));
					Out.Normals.Add(FVector3f(-(HR - HL) / SpanX, -(HU - HD) / SpanY, 1.0f).GetSafeNormal());
					Out.Colors.Add(Encode(Colors[j * N + i]));
				}
			}
			AddGridTriangles(Out, First, N, N, false);

			// short sides down to the slab, darker towards the bottom
			auto Side = [&](int32 I0, int32 J0, int32 DI, int32 DJ, const FVector3f& Normal)
			{
				const int32 SideFirst = Out.Positions.Num();
				for (int32 Row = 0; Row < 2; ++Row)
				{
					for (int32 k = 0; k < N; ++k)
					{
						const int32 i = I0 + DI * k, j = J0 + DJ * k;
						const FVector3f Top = Out.Positions[First + j * N + i];
						Out.Positions.Add(Row == 0 ? Top : FVector3f(Top.X, Top.Y, -Depth));
						Out.Normals.Add(Normal);
						Out.Colors.Add(Encode(Colors[j * N + i] * (Row == 0 ? 0.8f : 0.45f)));
					}
				}
				AddGridTriangles(Out, SideFirst, N, 2, false);
			};
			Side(0, 0, 1, 0, FVector3f(0, -1, 0));
			Side(0, N - 1, 1, 0, FVector3f(0, 1, 0));
			Side(0, 0, 0, 1, FVector3f(-1, 0, 0));
			Side(N - 1, 0, 0, 1, FVector3f(1, 0, 0));
		}
	}
}

void TDBoardSurface::BuildSlab(const FStyle& Style, const FVector2D& Size, float TopZ, float Height, FTDSculptMesh& Out)
{
	const int32 Seed = Style.Seed + 6;
	const FLinearColor Base = Mul(FLinearColor(0.05f, 0.09f, 0.1f), Style.Tint);
	const FLinearColor Moss = Mul(FLinearColor(0.05f, 0.16f, 0.08f), Style.Tint);
	FRandomStream Rand(Seed + 9);
	// barnacles on the sides: positions along the perimeter (uu) and depth below the top
	struct FBarnacle { float Along, Down, R; };
	TArray<FBarnacle> Barnacles;
	const float Perimeter = 2.0f * (Size.X + Size.Y);
	for (int32 i = 0; i < (int32)(Perimeter / 40.0f); ++i)
	{
		Barnacles.Add({ (float)Rand.FRandRange(0.0f, Perimeter), (float)Rand.FRandRange(8.0f, Height - 8.0f), (float)Rand.FRandRange(1.6f, 3.2f) });
	}

	auto SideColor = [&](float Along, float Down)
	{
		const float Warp = (Fbm(Along, Down, 40.0f, 3, Seed) - 0.5f) * 20.0f;
		const float Strata = FMath::Sin((Down + Warp) / 9.0f) * 0.5f + 0.5f;
		const float Bands = Fbm(Along, Down, 12.0f, 2, Seed + 1);
		FLinearColor Col = Base * (0.72f + Strata * 0.5f + (Bands - 0.5f) * 0.7f);
		const float M = Smooth(26.0f, 0.0f, Down + (Fbm(Along, 0.0f, 14.0f, 3, Seed + 4) - 0.5f) * 30.0f);
		Col = FMath::Lerp(Col, Moss, M * 0.6f);
		for (const FBarnacle& B : Barnacles)
		{
			const float D = FMath::Sqrt(FMath::Square(Along - B.Along) + FMath::Square(Down - B.Down));
			if (D < B.R)
			{
				// a pale cone with a dark opening
				Col = D < B.R * 0.4f ? FLinearColor(0.02f, 0.03f, 0.03f) : FLinearColor(0.14f, 0.17f, 0.16f) * (1.2f - (Down - B.Down) / B.R * 0.4f);
			}
		}
		return Col;
	};

	const float HX = (float)Size.X * 0.5f, HY = (float)Size.Y * 0.5f;
	const float Bottom = TopZ - Height;
	constexpr float Spacing = 4.0f;
	const int32 NV = FMath::Max(2, FMath::CeilToInt(Height / Spacing) + 1);
	// four sides, walked round the perimeter so strata and barnacles carry across the corners
	const FVector3f Corners[5] = { FVector3f(-HX, -HY, 0), FVector3f(HX, -HY, 0), FVector3f(HX, HY, 0), FVector3f(-HX, HY, 0), FVector3f(-HX, -HY, 0) };
	float AlongStart = 0.0f;
	for (int32 s = 0; s < 4; ++s)
	{
		const FVector3f A = Corners[s], B = Corners[s + 1];
		const float Len = (B - A).Size();
		const FVector3f Dir = (B - A) / Len;
		const FVector3f Normal(Dir.Y, -Dir.X, 0.0f);
		const int32 NU = FMath::Max(2, FMath::CeilToInt(Len / Spacing) + 1);
		const int32 First = Out.Positions.Num();
		for (int32 j = 0; j < NV; ++j)
		{
			const float Down = Height * j / (NV - 1);
			for (int32 i = 0; i < NU; ++i)
			{
				const float T = Len * i / (NU - 1);
				const FVector3f P = A + Dir * T;
				// a little relief so the strata catch the light
				const float Bump = (Fbm(AlongStart + T, Down, 10.0f, 2, Seed + 2) - 0.5f) * 1.6f;
				Out.Positions.Add(FVector3f(P.X, P.Y, TopZ - Down) + Normal * Bump);
				Out.Normals.Add(Normal);
				Out.Colors.Add(Encode(SideColor(AlongStart + T, Down)));
			}
		}
		AddGridTriangles(Out, First, NU, NV, false);
		AlongStart += Len;
	}
	// the top rim that shows round the tiles: mossy stone
	{
		const int32 NU = FMath::CeilToInt(Size.X / 8.0f) + 1, NW = FMath::CeilToInt(Size.Y / 8.0f) + 1;
		const int32 First = Out.Positions.Num();
		for (int32 j = 0; j < NW; ++j)
		{
			for (int32 i = 0; i < NU; ++i)
			{
				const float X = -HX + Size.X * i / (NU - 1), Y = -HY + Size.Y * j / (NW - 1);
				const float M = Smooth(0.45f, 0.75f, Fbm(X, Y, 30.0f, 3, Seed + 5));
				const FLinearColor Col = FMath::Lerp(Base * (0.85f + (Fbm(X, Y, 9.0f, 2, Seed + 3) - 0.5f) * 0.6f), Moss, M * 0.55f);
				Out.Positions.Add(FVector3f(X, Y, TopZ));
				Out.Normals.Add(FVector3f(0, 0, 1));
				Out.Colors.Add(Encode(Col));
			}
		}
		AddGridTriangles(Out, First, NU, NW, false);
	}
	(void)Bottom;
}

void TDBoardSurface::BuildFloor(const FStyle& Style, float Size, float Z, FTDSculptMesh& Out)
{
	const int32 Seed = Style.Seed + 4;
	const FLinearColor Base = Mul(FLinearColor(0.035f, 0.11f, 0.12f), Style.Tint);
	const FLinearColor Silt = Mul(FLinearColor(0.008f, 0.019f, 0.014f), Style.Tint);
	constexpr float Spacing = 16.0f;
	const int32 N = FMath::CeilToInt(Size / Spacing) + 1;
	const float Half = Size * 0.5f;
	const int32 First = Out.Positions.Num();
	for (int32 j = 0; j < N; ++j)
	{
		for (int32 i = 0; i < N; ++i)
		{
			const float X = -Half + Size * i / (N - 1), Y = -Half + Size * j / (N - 1);
			const float Mott = Fbm(X, Y, 260.0f, 5, Seed) - 0.5f;
			const float Rip = Ripples(X, Y, 42.0f, Seed + 1, 0.9f, 0.42f);
			const float S = Smooth(0.55f, 0.75f, Fbm(X, Y, 360.0f, 3, Seed + 3));
			FLinearColor Col = Base * (1.0f + Mott * 1.0f + (Rip - 0.5f) * 0.24f);
			Col = FMath::Lerp(Col, Silt, S * 0.3f);
			// fade into the plain floor at the patch's edge so it never shows a border
			const float EdgeFade = Smooth(Half, Half * 0.75f, FMath::Max(FMath::Abs(X), FMath::Abs(Y)));
			Col = FMath::Lerp(Base, Col, EdgeFade);
			Out.Positions.Add(FVector3f(X, Y, Z + (Rip * 1.5f + Mott * 3.0f) * EdgeFade));
			Out.Normals.Add(FVector3f(0, 0, 1));
			Out.Colors.Add(Encode(Col));
		}
	}
	AddGridTriangles(Out, First, N, N, false);
}
