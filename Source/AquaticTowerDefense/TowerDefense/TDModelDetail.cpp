// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDModelDetail.h"

namespace
{
	float Luminance(const FLinearColor& C)
	{
		return C.R * 0.3f + C.G * 0.59f + C.B * 0.11f;
	}

	FLinearColor Shade(const FLinearColor& C, float Factor)
	{
		return FLinearColor(FMath::Clamp(C.R * Factor, 0.0f, 1.0f), FMath::Clamp(C.G * Factor, 0.0f, 1.0f), FMath::Clamp(C.B * Factor, 0.0f, 1.0f), C.A);
	}

	FLinearColor Towards(const FLinearColor& C, const FLinearColor& Target, float Amount)
	{
		FLinearColor Out = FMath::Lerp(C, Target, Amount);
		Out.A = C.A;
		return Out;
	}

	bool IsLit(const FTDMergedPart& Part)
	{
		return Part.Mat == ETDPartMaterial::Lit;
	}

	/** Whether a part's own Z axis points (nearly) straight up or down */
	bool IsUpright(const FTDMergedPart& Part)
	{
		return FMath::Abs(Part.Rotation.GetAxisZ().Z) > 0.97f;
	}

	FTDMergedPart& AddPart(TArray<FTDMergedPart>& Parts, const FTDMergedPart& From, ETDShape Shape, const FVector& Location, const FVector& Size, const FQuat& Rotation, const FLinearColor& Color)
	{
		FTDMergedPart& Part = Parts.AddDefaulted_GetRef();
		Part.Shape = Shape;
		Part.Mat = From.Mat;
		Part.Flags = From.Flags;
		Part.Location = Location;
		Part.Size = Size;
		Part.Rotation = Rotation;
		Part.Color = Color;
		Part.Surface = From.Surface;
		return Part;
	}

	/** Small dark spheres are eyes and pupils: a white glint on the upper outer side makes them catch the light */
	void AddEyeGlint(const FTDMergedPart& Eye, TArray<FTDMergedPart>& Parts)
	{
		const float Across = Eye.Size.GetMin();
		if (Eye.Shape != ETDShape::Sphere || !IsLit(Eye) || Across < 1.6f || Eye.Size.GetMax() > 14.0f
			|| Eye.Size.GetMax() > Across * 1.6f || Luminance(Eye.Color) > 0.07f)
		{
			return;
		}
		// up and out from the pivot's centre line
		const FVector Out = FVector(Eye.Location.X, Eye.Location.Y, 0.0f).GetSafeNormal();
		const FVector Dir = (FVector(0.0f, 0.0f, 1.0f) + Out * 0.55f + FVector(0.25f, 0.0f, 0.0f)).GetSafeNormal();
		FTDMergedPart& Glint = AddPart(Parts, Eye, ETDShape::Sphere, Eye.Location + Dir * (Across * 0.5f * 0.82f), FVector(Across * 0.3f), FQuat::Identity, FLinearColor(0.95f, 0.97f, 1.0f, 1.0f));
		Glint.Mat = ETDPartMaterial::Glow;
		Glint.Segments = 12;
	}

	/** Spikes, horns and claws: a paler tip shades them along their length */
	void AddConeTip(const FTDMergedPart& Cone, TArray<FTDMergedPart>& Parts)
	{
		if (Cone.Shape != ETDShape::Cone || !IsLit(Cone) || Cone.Size.Z < 4.0f)
		{
			return;
		}
		constexpr float TipShare = 0.38f;
		const FVector Axis = Cone.Rotation.GetAxisZ();
		const FVector Size(Cone.Size.X * TipShare * 1.04f, Cone.Size.Y * TipShare * 1.04f, Cone.Size.Z * TipShare);
		FTDMergedPart& Tip = AddPart(Parts, Cone, ETDShape::Cone, Cone.Location + Axis * (Cone.Size.Z * (0.5f - TipShare * 0.5f) + 0.05f), Size, Cone.Rotation,
			Towards(Cone.Color, FLinearColor(0.96f, 0.93f, 0.84f), 0.4f));
		Tip.bSculpt = false;
	}

	/** Round bodies, shells and rocks: flattened spots of lighter and darker tone on the side that faces up, like a texture */
	void AddSpeckles(const FTDMergedPart& Body, int32 Seed, float MinSize, TArray<FTDMergedPart>& Parts)
	{
		if (Body.Shape != ETDShape::Sphere || !IsLit(Body) || Body.Size.GetMin() < 12.0f || Luminance(Body.Color) < 0.03f)
		{
			return;
		}
		FRandomStream Rand(Seed);
		const FVector Half = Body.Size * 0.5f;
		const int32 Count = FMath::Clamp(FMath::RoundToInt(Body.Size.GetMax() / 2.8f), 6, 22);
		const float SpotSize = FMath::Max(Body.Size.GetMin() * 0.14f, MinSize);
		for (int32 i = 0; i < Count; ++i)
		{
			// a direction on the upper part of the body (in the part's own space), then out to its surface
			FVector Dir = Rand.GetUnitVector();
			FVector World = Body.Rotation.RotateVector(Dir);
			if (World.Z < -0.15f)
			{
				Dir = -Dir;
				World = -World;
			}
			const FVector Surface = Body.Location + Body.Rotation.RotateVector(Dir * Half);
			const FVector Normal = Body.Rotation.RotateVector(FVector(Dir.X / Half.X, Dir.Y / Half.Y, Dir.Z / Half.Z)).GetSafeNormal();
			const float Size = SpotSize * Rand.FRandRange(0.7f, 1.25f);
			const bool bLight = (i % 3) != 0;
			FTDMergedPart& Spot = AddPart(Parts, Body, ETDShape::Sphere, Surface - Normal * (Size * 0.08f), FVector(Size, Size * Rand.FRandRange(0.7f, 1.0f), Size * 0.3f),
				FQuat::FindBetweenNormals(FVector::UpVector, Normal) * FQuat(FVector::UpVector, Rand.FRandRange(0.0f, PI)),
				bLight ? Towards(Body.Color, FLinearColor::White, 0.2f) : Shade(Body.Color, 0.74f));
			Spot.Segments = 12;
			Spot.bSculpt = false;
		}
	}

	/** Wide upright cylinders (bases, drums, platforms): a darker lip around the top rim, and studs on the rim of flat ones */
	void AddRimDetail(const FTDMergedPart& Drum, TArray<FTDMergedPart>& Parts)
	{
		const float Across = FMath::Min(Drum.Size.X, Drum.Size.Y);
		if (Drum.Shape != ETDShape::Cylinder || !IsLit(Drum) || Across < 26.0f || Drum.Size.Z < 3.0f || !IsUpright(Drum))
		{
			return;
		}
		const float Up = Drum.Rotation.GetAxisZ().Z > 0.0f ? 1.0f : -1.0f;
		const float Lip = FMath::Min(1.8f, Drum.Size.Z * 0.3f);
		const FVector Top = Drum.Location + FVector(0.0f, 0.0f, Up * Drum.Size.Z * 0.5f);
		AddPart(Parts, Drum, ETDShape::Cylinder, Top - FVector(0.0f, 0.0f, Lip * 0.5f + 0.1f), FVector(Drum.Size.X + 1.6f, Drum.Size.Y + 1.6f, Lip), Drum.Rotation,
			Shade(Drum.Color, 0.72f)).bSculpt = false;
		if (Drum.Size.Z > 12.0f)
		{
			AddPart(Parts, Drum, ETDShape::Cylinder, Drum.Location - FVector(0.0f, 0.0f, Up * (Drum.Size.Z * 0.5f - Lip * 0.5f - 0.1f)),
				FVector(Drum.Size.X + 1.6f, Drum.Size.Y + 1.6f, Lip), Drum.Rotation, Shade(Drum.Color, 0.72f)).bSculpt = false;
		}

		// studs around the top of flat ones (platforms)
		if (Drum.Size.Z <= 14.0f && Across >= 38.0f)
		{
			const int32 Count = FMath::Clamp(FMath::RoundToInt(Across / 7.0f), 8, 20);
			const float Stud = FMath::Clamp(Across * 0.045f, 1.6f, 3.4f);
			const FLinearColor Color = Towards(Shade(Drum.Color, 0.85f), FLinearColor(0.85f, 0.78f, 0.6f), 0.35f);
			for (int32 i = 0; i < Count; ++i)
			{
				const float A = 2.0f * PI * (i + 0.5f) / Count;
				const FVector Local(FMath::Cos(A) * (Drum.Size.X * 0.5f - Stud * 1.4f), FMath::Sin(A) * (Drum.Size.Y * 0.5f - Stud * 1.4f), 0.0f);
				FTDMergedPart& Part = AddPart(Parts, Drum, ETDShape::Sphere, Top + Drum.Rotation.RotateVector(Local) * FVector(1.0f, 1.0f, 0.0f) + FVector(0.0f, 0.0f, Up * Stud * 0.12f),
					FVector(Stud, Stud, Stud * 0.6f), FQuat::Identity, Color);
				Part.Segments = 12;
				Part.Surface = ETDSurface::Metal;
				Part.bSculpt = false;
			}
		}
	}

	// ---- the higher-detail look: what each part is made of, the creature bodies to sculpt, and a few extra parts ----

	/** Hue (0-1), saturation and lightness of a linear colour, measured in sRGB like a colour picker */
	void ToHsl(const FLinearColor& Linear, float& H, float& S, float& L)
	{
		const FColor C = Linear.ToFColorSRGB();
		const float R = C.R / 255.0f, G = C.G / 255.0f, B = C.B / 255.0f;
		const float Max = FMath::Max3(R, G, B), Min = FMath::Min3(R, G, B);
		L = (Max + Min) * 0.5f;
		H = S = 0.0f;
		if (Max - Min < 1e-5f)
		{
			return;
		}
		const float D = Max - Min;
		S = L > 0.5f ? D / (2.0f - Max - Min) : D / (Max + Min);
		if (Max == R)
		{
			H = (G - B) / D + (G < B ? 6.0f : 0.0f);
		}
		else if (Max == G)
		{
			H = (B - R) / D + 2.0f;
		}
		else
		{
			H = (R - G) / D + 4.0f;
		}
		H /= 6.0f;
	}

	struct FDetailContext
	{
		bool bCreature = false;	// every part belongs to a creature (no metal or wooden fittings)
		bool bProp = false;		// scenery: every round grey part is rock
		bool bRoot = false;		// the model's base pivot (rocks, sand and fittings around the tower)
	};

	/** Small or flattened spheres stay crisp on top of a sculpted body: spots, scales, beads, suckers */
	bool IsDecal(const FTDMergedPart& Part)
	{
		const float Max = Part.Size.GetMax();
		return Part.Shape == ETDShape::Sphere && (Max < 4.0f || (Part.Size.GetMin() < 0.55f * Max && Max <= 12.0f));
	}

	ETDSurface ClassifySurface(const FTDMergedPart& Part, const FDetailContext& Ctx)
	{
		float H, S, L;
		ToHsl(Part.Color, H, S, L);
		FVector Sorted = Part.Size;
		if (Sorted.X > Sorted.Y) Swap(Sorted.X, Sorted.Y);
		if (Sorted.Y > Sorted.Z) Swap(Sorted.Y, Sorted.Z);
		if (Sorted.X > Sorted.Y) Swap(Sorted.X, Sorted.Y);
		const float Min = Sorted.X, Max = Sorted.Z;
		const bool bHard = Part.Shape == ETDShape::Cube || Part.Shape == ETDShape::Cylinder;

		if (Part.Shape == ETDShape::Sphere && Max <= 14.0f && Max <= Min * 1.6f && Luminance(Part.Color) <= 0.07f)
		{
			return ETDSurface::Eye;
		}
		if (Part.Shape == ETDShape::Plane)
		{
			return ETDSurface::Skin;
		}
		if (bHard && H >= 0.09f && H <= 0.17f && S > 0.45f && L > 0.35f && L < 0.8f)
		{
			return ETDSurface::Brass;
		}
		if (bHard && H >= 0.02f && H <= 0.12f && S > 0.25f && L < 0.42f && L > 0.06f)
		{
			return ETDSurface::Wood;
		}
		if (bHard && S < 0.16f && L > 0.12f && L < 0.82f)
		{
			return ETDSurface::Metal;
		}
		const bool bRoot = Ctx.bRoot || Ctx.bProp;
		if (bRoot && Part.Shape == ETDShape::Sphere && S < 0.3f && Min > 5.0f)
		{
			return ETDSurface::Stone;
		}
		if (bRoot && (Part.Shape == ETDShape::Sphere || Part.Shape == ETDShape::Cylinder) && H > 0.08f && H < 0.17f && S < 0.6f && L > 0.55f && Sorted.X < Sorted.Y * 0.4f)
		{
			return ETDSurface::Sand;
		}
		if (!bHard || Ctx.bCreature)
		{
			return ETDSurface::Skin;
		}
		return ETDSurface::Plain;
	}

	/** Sorts the pivot's original parts into surfaces, and marks the creature bodies that melt into one sculpted surface */
	void ClassifyParts(TArray<FTDMergedPart>& Parts, const FDetailContext& Ctx)
	{
		for (FTDMergedPart& Part : Parts)
		{
			if (Part.Mat != ETDPartMaterial::Lit)
			{
				continue;
			}
			const bool bWasAuto = Part.Surface == ETDSurface::Auto;
			if (bWasAuto)
			{
				Part.Surface = ClassifySurface(Part, Ctx);
			}
			const bool bOrganic = Part.Surface == ETDSurface::Skin || Part.Surface == ETDSurface::Coral;
			Part.bSculpt = bOrganic && Part.Flags == 0 && !IsDecal(Part) && Part.Shape != ETDShape::Plane && Part.Size.GetMin() >= 1.6f;
		}
	}

	/** Eyes: a second, softer catch light low on the other side */
	void AddEyeCatchLight(const FTDMergedPart& Eye, TArray<FTDMergedPart>& Parts)
	{
		const float Across = Eye.Size.GetMin();
		if (Eye.Surface != ETDSurface::Eye || Eye.Shape != ETDShape::Sphere || Across <= 2.2f)
		{
			return;
		}
		const FVector Out = FVector(Eye.Location.X, Eye.Location.Y, 0.0f).GetSafeNormal();
		const FVector Dir = (FVector(0.0f, 0.0f, 1.0f) + Out * 0.55f + FVector(0.25f, 0.0f, 0.0f)).GetSafeNormal();
		FTDMergedPart& Light = AddPart(Parts, Eye, ETDShape::Sphere, Eye.Location - Dir * (Across * 0.38f) - FVector(0.0f, 0.0f, Across * 0.1f), FVector(Across * 0.16f),
			FQuat::Identity, FLinearColor(0.7f, 0.85f, 1.0f, 1.0f));
		Light.Mat = ETDPartMaterial::Glow;
		Light.Segments = 8;
		Light.bSculpt = false;
	}

	/** Wooden drums and barrels: two iron bands with rivets */
	void AddIronBands(const FTDMergedPart& Drum, TArray<FTDMergedPart>& Parts)
	{
		if (Drum.Surface != ETDSurface::Wood || Drum.Shape != ETDShape::Cylinder || FMath::Min(Drum.Size.X, Drum.Size.Y) <= 20.0f || Drum.Size.Z <= 14.0f || !IsUpright(Drum))
		{
			return;
		}
		const FLinearColor Iron(0.12f, 0.12f, 0.13f, 1.0f), RivetColor(0.35f, 0.3f, 0.25f, 1.0f);
		for (const float F : { -0.3f, 0.3f })
		{
			const FVector At = Drum.Location + FVector(0.0f, 0.0f, Drum.Size.Z * F);
			FTDMergedPart& Band = AddPart(Parts, Drum, ETDShape::Cylinder, At, FVector(Drum.Size.X + 1.4f, Drum.Size.Y + 1.4f, 2.2f), Drum.Rotation, Iron);
			Band.Surface = ETDSurface::Metal;
			const int32 Count = FMath::RoundToInt(PI * Drum.Size.X / 9.0f);
			for (int32 k = 0; k < Count; ++k)
			{
				const float A = 2.0f * PI * k / Count;
				FTDMergedPart& Rivet = AddPart(Parts, Drum, ETDShape::Sphere, At + FVector(FMath::Cos(A) * (Drum.Size.X * 0.5f + 0.9f), FMath::Sin(A) * (Drum.Size.Y * 0.5f + 0.9f), 0.0f),
					FVector(1.3f), FQuat::Identity, RivetColor);
				Rivet.Surface = ETDSurface::Metal;
				Rivet.Segments = 8;
			}
		}
	}

	/** Metal and brass tubes with some length: a ring of rivets near each end */
	void AddRivets(const FTDMergedPart& Tube, TArray<FTDMergedPart>& Parts)
	{
		if ((Tube.Surface != ETDSurface::Metal && Tube.Surface != ETDSurface::Brass) || Tube.Shape != ETDShape::Cylinder
			|| FMath::Min(Tube.Size.X, Tube.Size.Y) <= 7.0f || Tube.Size.Z <= 6.0f)
		{
			return;
		}
		const FVector Axis = Tube.Rotation.GetAxisZ();
		const FVector U = Tube.Rotation.GetAxisX();
		const FVector V = Tube.Rotation.GetAxisY();
		const float Radius = FMath::Min(Tube.Size.X, Tube.Size.Y) * 0.5f;
		const int32 Count = FMath::Clamp(FMath::RoundToInt(Radius * 0.9f), 6, 16);
		for (const float F : { -0.41f, 0.41f })
		{
			for (int32 k = 0; k < Count; ++k)
			{
				const float A = 2.0f * PI * k / Count;
				FTDMergedPart& Rivet = AddPart(Parts, Tube, ETDShape::Sphere, Tube.Location + Axis * (Tube.Size.Z * F) + (U * FMath::Cos(A) + V * FMath::Sin(A)) * Radius,
					FVector(1.0f), FQuat::Identity, Shade(Tube.Color, 0.8f));
				Rivet.Segments = 8;
			}
		}
	}

	void AddHighDetail(TArray<FTDMergedPart>& Parts, int32 NumOriginal)
	{
		for (int32 i = 0; i < NumOriginal; ++i)
		{
			const FTDMergedPart Part = Parts[i];
			AddEyeCatchLight(Part, Parts);
			AddIronBands(Part, Parts);
			AddRivets(Part, Parts);
		}
	}

	/** Which kind of model an id is, for the surfaces */
	FDetailContext ContextOf(ETDModel Id, int32 PivotIndex)
	{
		FDetailContext Ctx;
		Ctx.bCreature = Id >= ETDModel::Crab && Id <= ETDModel::ShockEel;
		Ctx.bProp = Id >= ETDModel::Boulders && Id <= ETDModel::KelpTall;
		Ctx.bRoot = PivotIndex == 0;
		return Ctx;
	}
}

void TDModelDetail::AddDetail(ETDModel Id, int32 PivotIndex, float MinSize, TArray<FTDMergedPart>& Parts)
{
	const int32 SeedBase = ((int32)Id * 64 + PivotIndex) * 4096;
	const int32 NumOriginal = Parts.Num();
	ClassifyParts(Parts, ContextOf(Id, PivotIndex));
	for (int32 i = 0; i < NumOriginal; ++i)
	{
		const FTDMergedPart Part = Parts[i];	// a copy: adding parts may move the array
		AddEyeGlint(Part, Parts);
		AddConeTip(Part, Parts);
		AddSpeckles(Part, SeedBase + i, MinSize, Parts);
		AddRimDetail(Part, Parts);
	}
	AddHighDetail(Parts, NumOriginal);
}

void TDModelDetail::AddDetail(int32 Seed, float MinSize, TArray<FTDMergedPart>& Parts)
{
	const int32 NumOriginal = Parts.Num();
	FDetailContext Ctx;
	Ctx.bProp = true;
	ClassifyParts(Parts, Ctx);
	for (int32 i = 0; i < NumOriginal; ++i)
	{
		const FTDMergedPart Part = Parts[i];
		AddEyeGlint(Part, Parts);
		AddConeTip(Part, Parts);
		AddSpeckles(Part, Seed * 4096 + i, MinSize, Parts);
		AddRimDetail(Part, Parts);
	}
	AddHighDetail(Parts, NumOriginal);
	for (FTDMergedPart& Part : Parts)
	{
		Part.bDetailed = true;
	}
}
