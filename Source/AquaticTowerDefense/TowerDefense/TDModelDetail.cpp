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
		AddPart(Parts, Cone, ETDShape::Cone, Cone.Location + Axis * (Cone.Size.Z * (0.5f - TipShare * 0.5f) + 0.05f), Size, Cone.Rotation,
			Towards(Cone.Color, FLinearColor(0.96f, 0.93f, 0.84f), 0.4f));
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
			Shade(Drum.Color, 0.72f));
		if (Drum.Size.Z > 12.0f)
		{
			AddPart(Parts, Drum, ETDShape::Cylinder, Drum.Location - FVector(0.0f, 0.0f, Up * (Drum.Size.Z * 0.5f - Lip * 0.5f - 0.1f)),
				FVector(Drum.Size.X + 1.6f, Drum.Size.Y + 1.6f, Lip), Drum.Rotation, Shade(Drum.Color, 0.72f));
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
			}
		}
	}
}

void TDModelDetail::AddDetail(ETDModel Id, int32 PivotIndex, float MinSize, TArray<FTDMergedPart>& Parts)
{
	const int32 SeedBase = ((int32)Id * 64 + PivotIndex) * 4096;
	const int32 NumOriginal = Parts.Num();
	for (int32 i = 0; i < NumOriginal; ++i)
	{
		const FTDMergedPart Part = Parts[i];	// a copy: adding parts may move the array
		AddEyeGlint(Part, Parts);
		AddConeTip(Part, Parts);
		AddSpeckles(Part, SeedBase + i, MinSize, Parts);
		AddRimDetail(Part, Parts);
	}
}

void TDModelDetail::AddDetail(int32 Seed, float MinSize, TArray<FTDMergedPart>& Parts)
{
	const int32 NumOriginal = Parts.Num();
	for (int32 i = 0; i < NumOriginal; ++i)
	{
		const FTDMergedPart Part = Parts[i];
		AddEyeGlint(Part, Parts);
		AddConeTip(Part, Parts);
		AddSpeckles(Part, Seed * 4096 + i, MinSize, Parts);
		AddRimDetail(Part, Parts);
	}
	for (FTDMergedPart& Part : Parts)
	{
		Part.bDetailed = true;
	}
}
