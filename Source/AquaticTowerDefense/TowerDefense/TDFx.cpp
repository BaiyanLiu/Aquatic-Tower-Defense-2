// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDFx.h"
#include "TDModels.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 RingParts = 28;
	constexpr int32 RingGlints = 14;
	constexpr int32 SplashGlints = 5;

	void MoveDroplets(UInstancedStaticMeshComponent* Droplets, TArrayView<const FTransform> Transforms)
	{
		Droplets->BatchUpdateInstancesTransforms(0, Transforms, false, false, true);
		// instances reach the renderer by themselves, but the bounds that keep them from being culled don't
		Droplets->UpdateBounds();
		Droplets->MarkRenderTransformDirty();
	}
}

ATDFx::ATDFx()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ATDFx* ATDFx::SpawnAt(UWorld* World, const FVector& Location, float InLife)
{
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATDFx* Fx = World->SpawnActor<ATDFx>(ATDFx::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (Fx)
	{
		Fx->Life = InLife;
		Fx->SetLifeSpan(InLife + 0.05f);
	}
	return Fx;
}

UInstancedStaticMeshComponent* ATDFx::AddDroplets(UMaterialInterface* Material, int32 Count)
{
	UInstancedStaticMeshComponent* Droplets = NewObject<UInstancedStaticMeshComponent>(this);
	Droplets->SetStaticMesh(TD::UnitShapeMesh(ETDShape::Sphere, 14.0f));
	Droplets->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Droplets->SetCastShadow(false);
	Droplets->SetMaterial(0, Material);
	Droplets->SetupAttachment(GetRootComponent());
	Droplets->RegisterComponent();
	AddInstanceComponent(Droplets);
	TArray<FTransform> Start;
	Start.Init(FTransform::Identity, Count);
	Droplets->AddInstances(Start, false, false, false);
	return Droplets;
}

ATDFx* ATDFx::SpawnRing(UWorld* World, const FVector& Center, float InRadius, const FLinearColor& Color)
{
	ATDFx* Fx = SpawnAt(World, Center, 0.4f);
	if (Fx)
	{
		Fx->Kind = EKind::Ring;
		Fx->Radius = InRadius;
		Fx->Drops = Fx->AddDroplets(TD::SharedColorMaterial(Color), RingParts);
		if (TD::UseDetailedModels())
		{
			// a few bright glints riding just inside the wave
			Fx->Glints = Fx->AddDroplets(TD::SharedFxMaterial(FMath::Lerp(Color, FLinearColor::White, 0.7f), 0.85f), RingGlints);
		}
		Fx->Tick(0.0f);
	}
	return Fx;
}

ATDFx* ATDFx::SpawnBurst(UWorld* World, const FVector& Center, float InRadius, const FLinearColor& Color)
{
	ATDFx* Fx = SpawnAt(World, Center, 0.35f);
	if (Fx)
	{
		Fx->Kind = EKind::Burst;
		Fx->Radius = InRadius;
		if (TD::UseDetailedModels())
		{
			// a see-through shell of churned water round a bright, smaller core
			UStaticMeshComponent* Shell = TD::AddShape(Fx, Fx->GetRootComponent(), ETDShape::Sphere, FVector::ZeroVector, FVector(10, 10, 10), Color,
				FRotator::ZeroRotator, TD::SharedFxMaterial(FMath::Lerp(Color, FLinearColor::White, 0.2f), 0.45f));
			Shell->SetCastShadow(false);
			Fx->Parts.Add(Shell);
			Fx->PartWidths.Add(1.0f);
			UStaticMeshComponent* Core = TD::AddShape(Fx, Fx->GetRootComponent(), ETDShape::Sphere, FVector::ZeroVector, FVector(10, 10, 10), FMath::Lerp(Color, FLinearColor::White, 0.35f));
			Core->SetCastShadow(false);
			Fx->Parts.Add(Core);
			Fx->PartWidths.Add(0.5f);
		}
		else
		{
			UStaticMeshComponent* Part = TD::AddShape(Fx, Fx->GetRootComponent(), ETDShape::Sphere, FVector::ZeroVector, FVector(10, 10, 10), Color);
			Part->SetCastShadow(false);
			Fx->Parts.Add(Part);
			Fx->PartWidths.Add(1.0f);
		}
		Fx->Tick(0.0f);
	}
	return Fx;
}

ATDFx* ATDFx::SpawnBolt(UWorld* World, const TArray<FVector>& Points, const FLinearColor& Color)
{
	if (Points.Num() < 2)
	{
		return nullptr;
	}

	ATDFx* Fx = SpawnAt(World, Points[0], 0.18f);
	if (!Fx)
	{
		return nullptr;
	}

	Fx->Kind = EKind::Bolt;
	UMaterialInterface* Mat = TD::SharedColorMaterial(Color);

	if (TD::UseDetailedModels())
	{
		// a white-hot core inside a soft glow, more kinks per jump, and a short fork off some of them
		UMaterialInterface* CoreMat = TD::SharedColorMaterial(FMath::Lerp(Color, FLinearColor::White, 0.65f));
		UMaterialInterface* GlowMat = TD::SharedFxMaterial(Color, 0.35f);
		auto Segment = [Fx, &Points, CoreMat, GlowMat, &Color](const FVector& From, const FVector& To, float Width)
		{
			const FVector Delta = To - From;
			const float Length = Delta.Size();
			if (Length <= 1.0f)
			{
				return;
			}
			const FVector Mid = (From + To) * 0.5f - Points[0];
			const FRotator Rot = FRotationMatrix::MakeFromZ(Delta / Length).Rotator();
			UStaticMeshComponent* Glow = TD::AddShape(Fx, Fx->GetRootComponent(), ETDShape::Cylinder, Mid, FVector(5, 5, Length), Color, Rot, GlowMat);
			Glow->SetCastShadow(false);
			Fx->Parts.Add(Glow);
			Fx->PartWidths.Add(0.16f * Width);
			UStaticMeshComponent* Core = TD::AddShape(Fx, Fx->GetRootComponent(), ETDShape::Cylinder, Mid, FVector(5, 5, Length), Color, Rot, CoreMat);
			Core->SetCastShadow(false);
			Fx->Parts.Add(Core);
			Fx->PartWidths.Add(0.035f * Width);
		};
		for (int32 i = 0; i + 1 < Points.Num(); ++i)
		{
			const FVector A = Points[i];
			const FVector B = Points[i + 1];
			FVector Prev = A;
			const int32 Pieces = 5;
			for (int32 p = 1; p <= Pieces; ++p)
			{
				FVector Next = FMath::Lerp(A, B, (float)p / Pieces);
				if (p < Pieces)
				{
					Next += FMath::VRand() * 10.0f;
				}
				Segment(Prev, Next, 1.0f);
				if (p < Pieces && FMath::FRand() < 0.5f)
				{
					// a short fork jumping off the kink and fading out
					const FVector Fork = Next + ((B - A).GetSafeNormal() * 0.5f + FMath::VRand()).GetSafeNormal() * FMath::FRandRange(14.0f, 26.0f);
					Segment(Next, Fork, 0.55f);
				}
				Prev = Next;
			}
		}
		return Fx;
	}

	// each jump becomes a few jagged cylinder segments
	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		const FVector A = Points[i];
		const FVector B = Points[i + 1];
		FVector Prev = A;
		const int32 Pieces = 3;
		for (int32 p = 1; p <= Pieces; ++p)
		{
			FVector Next = FMath::Lerp(A, B, (float)p / Pieces);
			if (p < Pieces)
			{
				Next += FMath::VRand() * 12.0f;
			}
			const FVector Delta = Next - Prev;
			const float Length = Delta.Size();
			if (Length > 1.0f)
			{
				const FVector Mid = (Prev + Next) * 0.5f - Points[0];
				UStaticMeshComponent* Part = TD::AddShape(Fx, Fx->GetRootComponent(), ETDShape::Cylinder, Mid, FVector(5, 5, Length), Color,
					FRotationMatrix::MakeFromZ(Delta / Length).Rotator(), Mat);
				Part->SetCastShadow(false);
				Fx->Parts.Add(Part);
				Fx->PartWidths.Add(0.05f);
			}
			Prev = Next;
		}
	}
	return Fx;
}

ATDFx* ATDFx::SpawnSplash(UWorld* World, const FVector& Center, const FLinearColor& Color, float Size)
{
	ATDFx* Fx = SpawnAt(World, Center, 0.65f);
	if (!Fx)
	{
		return nullptr;
	}
	Fx->Kind = EKind::Splash;
	const int32 Drops = 9;
	const int32 Bubbles = 4;
	const bool bDetailed = TD::UseDetailedModels();
	const int32 Sparks = bDetailed ? SplashGlints : 0;
	Fx->Drops = Fx->AddDroplets(TD::SharedColorMaterial(Color), Drops);
	// the detailed look: clear, glassy bubbles and a few bright sparks thrown out fastest
	Fx->Bubbles = Fx->AddDroplets(bDetailed ? TD::SharedFxMaterial(FLinearColor(0.8f, 0.97f, 1.0f), 0.5f) : TD::SharedColorMaterial(FLinearColor(0.75f, 0.95f, 1.0f)), Bubbles);
	if (bDetailed)
	{
		Fx->Glints = Fx->AddDroplets(TD::SharedColorMaterial(FMath::Lerp(Color, FLinearColor::White, 0.75f)), Sparks);
	}
	for (int32 i = 0; i < Drops + Bubbles + Sparks; ++i)
	{
		if (i >= Drops + Bubbles)
		{
			const FVector Dir = FRotator(FMath::FRandRange(25.0f, 70.0f), FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector();
			Fx->Motes.Add({ FVector::ZeroVector, Dir * FMath::FRandRange(220.0f, 320.0f) * Size, FMath::FRandRange(3.5f, 5.0f) * Size, -300.0f });
			continue;
		}
		const bool bBubble = i >= Drops;
		const FVector Dir = FRotator(FMath::FRandRange(15.0f, 60.0f), FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector();
		Fx->Motes.Add({ FVector::ZeroVector,
			bBubble ? FVector(FMath::FRandRange(-20.0f, 20.0f), FMath::FRandRange(-20.0f, 20.0f), FMath::FRandRange(60.0f, 110.0f)) : Dir * FMath::FRandRange(120.0f, 220.0f) * Size,
			(bBubble ? FMath::FRandRange(7.0f, 11.0f) : FMath::FRandRange(8.0f, 14.0f)) * Size,
			bBubble ? 40.0f : -420.0f });
	}
	Fx->Tick(0.0f);
	return Fx;
}

void ATDFx::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Age += DeltaTime;
	const float Alpha = FMath::Clamp(Age / FMath::Max(Life, 0.01f), 0.0f, 1.0f);

	// parts only set their relative transforms here, and update together at the end

	switch (Kind)
	{
	case EKind::Ring:
	{
		// expanding ring of droplets that shrink as it reaches the edge of the range
		const float R = FMath::Lerp(Radius * 0.15f, Radius, FMath::Sqrt(Alpha));
		const FVector Scale = FVector(0.14f, 0.14f, 0.06f) * FMath::Lerp(1.0f, 0.3f, Alpha);
		TArray<FTransform> Transforms;
		Transforms.Reserve(RingParts);
		const bool bSmooth = Glints != nullptr;
		for (int32 i = 0; i < RingParts; ++i)
		{
			const float Angle = (2.0f * PI * i) / RingParts;
			if (bSmooth)
			{
				// long droplets laid along the circle, overlapping into one smooth wave
				const float Along = 1.35f * 2.0f * PI * R / RingParts / 100.0f;
				Transforms.Add(FTransform(FQuat(FVector::UpVector, Angle), FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 0.0f),
					FVector(Scale.X * 0.75f, FMath::Max(Scale.X, Along), Scale.Z)));
			}
			else
			{
				Transforms.Add(FTransform(FQuat::Identity, FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 0.0f), Scale));
			}
		}
		MoveDroplets(Drops, Transforms);
		if (bSmooth)
		{
			TArray<FTransform> Sparkles;
			Sparkles.Reserve(RingGlints);
			for (int32 i = 0; i < RingGlints; ++i)
			{
				const float Angle = (2.0f * PI * (i + 0.37f)) / RingGlints + Age * 1.5f;
				const float Twinkle = 0.5f + 0.5f * FMath::Sin(Age * 30.0f + i * 1.7f);
				Sparkles.Add(FTransform(FQuat::Identity, FVector(FMath::Cos(Angle) * R * 0.92f, FMath::Sin(Angle) * R * 0.92f, 3.0f),
					FVector(0.05f + 0.04f * Twinkle) * (1.0f - Alpha)));
			}
			MoveDroplets(Glints, Sparkles);
		}
		break;
	}
	case EKind::Burst:
	{
		// grows quickly then collapses
		const float Grow = FMath::Sin(Alpha * PI);
		const float Diameter = FMath::Max(1.0f, Radius * 2.0f * Grow * 0.8f);
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			const float W = PartWidths.IsValidIndex(i) ? PartWidths[i] : 1.0f;
			Parts[i]->SetRelativeScale3D_Direct(FVector(Diameter, Diameter, Diameter * 0.55f) * W / 100.0f);
		}
		break;
	}
	case EKind::Splash:
	{
		// water drag slows the droplets, they sink and shrink; bubbles wobble upwards
		const float Drag = FMath::Exp(-3.0f * DeltaTime);
		const int32 DropCount = Drops->GetInstanceCount();
		const int32 BubbleCount = Bubbles->GetInstanceCount();
		const bool bStretch = Glints != nullptr;
		TArray<FTransform> Transforms;
		Transforms.Reserve(Motes.Num());
		for (int32 i = 0; i < Motes.Num(); ++i)
		{
			FMote& Mote = Motes[i];
			Mote.Velocity = Mote.Velocity * Drag + FVector(0, 0, Mote.Buoyancy * DeltaTime);
			Mote.Location += Mote.Velocity * DeltaTime;
			const float S = Mote.Size * (1.0f - Alpha * Alpha) / 100.0f;
			const float Speed = Mote.Velocity.Size();
			if (bStretch && (i < DropCount || i >= DropCount + BubbleCount) && Speed > 1.0f)
			{
				// fast droplets and sparks stretch along their flight
				const float Stretch = 1.0f + FMath::Min(Speed / 160.0f, 1.6f);
				Transforms.Add(FTransform(FRotationMatrix::MakeFromZ(Mote.Velocity / Speed).ToQuat(), Mote.Location, FVector(S, S, S * Stretch)));
			}
			else
			{
				Transforms.Add(FTransform(FQuat::Identity, Mote.Location, FVector(S)));
			}
		}
		MoveDroplets(Drops, TArrayView<const FTransform>(Transforms).Left(DropCount));
		MoveDroplets(Bubbles, TArrayView<const FTransform>(Transforms).Mid(DropCount, BubbleCount));
		if (Glints)
		{
			MoveDroplets(Glints, TArrayView<const FTransform>(Transforms).RightChop(DropCount + BubbleCount));
		}
		break;
	}
	case EKind::Bolt:
	{
		const float Flicker = (FMath::Fmod(Age * 60.0f, 2.0f) < 1.0f) ? 1.0f : 0.6f;
		for (int32 i = 0; i < Parts.Num(); ++i)
		{
			UStaticMeshComponent* Part = Parts[i];
			FVector Scale = Part->GetRelativeScale3D();
			Scale.X = Scale.Y = (PartWidths.IsValidIndex(i) ? PartWidths[i] : 0.05f) * Flicker * (1.0f - Alpha * 0.6f);
			Part->SetRelativeScale3D_Direct(Scale);
		}
		break;
	}
	}

	GetRootComponent()->UpdateChildTransforms();
}
