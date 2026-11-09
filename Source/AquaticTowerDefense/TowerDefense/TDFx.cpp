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
		UStaticMeshComponent* Part = TD::AddShape(Fx, Fx->GetRootComponent(), ETDShape::Sphere, FVector::ZeroVector, FVector(10, 10, 10), Color);
		Part->SetCastShadow(false);
		Fx->Parts.Add(Part);
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
	Fx->Drops = Fx->AddDroplets(TD::SharedColorMaterial(Color), Drops);
	Fx->Bubbles = Fx->AddDroplets(TD::SharedColorMaterial(FLinearColor(0.75f, 0.95f, 1.0f)), Bubbles);
	for (int32 i = 0; i < Drops + Bubbles; ++i)
	{
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
		for (int32 i = 0; i < RingParts; ++i)
		{
			const float Angle = (2.0f * PI * i) / RingParts;
			Transforms.Add(FTransform(FQuat::Identity, FVector(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R, 0.0f), Scale));
		}
		MoveDroplets(Drops, Transforms);
		break;
	}
	case EKind::Burst:
	{
		// grows quickly then collapses
		const float Grow = FMath::Sin(Alpha * PI);
		const float Diameter = FMath::Max(1.0f, Radius * 2.0f * Grow * 0.8f);
		if (Parts.Num() > 0)
		{
			Parts[0]->SetRelativeScale3D_Direct(FVector(Diameter, Diameter, Diameter * 0.55f) / 100.0f);
		}
		break;
	}
	case EKind::Splash:
	{
		// water drag slows the droplets, they sink and shrink; bubbles wobble upwards
		const float Drag = FMath::Exp(-3.0f * DeltaTime);
		const int32 DropCount = Drops->GetInstanceCount();
		TArray<FTransform> Transforms;
		Transforms.Reserve(Motes.Num());
		for (FMote& Mote : Motes)
		{
			Mote.Velocity = Mote.Velocity * Drag + FVector(0, 0, Mote.Buoyancy * DeltaTime);
			Mote.Location += Mote.Velocity * DeltaTime;
			Transforms.Add(FTransform(FQuat::Identity, Mote.Location, FVector(Mote.Size * (1.0f - Alpha * Alpha) / 100.0f)));
		}
		MoveDroplets(Drops, TArrayView<const FTransform>(Transforms).Left(DropCount));
		MoveDroplets(Bubbles, TArrayView<const FTransform>(Transforms).RightChop(DropCount));
		break;
	}
	case EKind::Bolt:
	{
		const float Flicker = (FMath::Fmod(Age * 60.0f, 2.0f) < 1.0f) ? 1.0f : 0.6f;
		for (UStaticMeshComponent* Part : Parts)
		{
			FVector Scale = Part->GetRelativeScale3D();
			Scale.X = Scale.Y = 0.05f * Flicker * (1.0f - Alpha * 0.6f);
			Part->SetRelativeScale3D_Direct(Scale);
		}
		break;
	}
	}

	GetRootComponent()->UpdateChildTransforms();
}
