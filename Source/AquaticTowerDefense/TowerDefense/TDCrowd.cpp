// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDCrowd.h"
#include "TDModels.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 SlotsPerGrowth = 16;

	/** Free slots stay as tiny instances under the floor: taking instances out renumbers the others */
	const FTransform& HiddenTransform()
	{
		static const FTransform Hidden(FQuat::Identity, FVector(0.0f, 0.0f, -500.0f), FVector(0.001f));
		return Hidden;
	}
}

ATDCrowd::ATDCrowd()
{
	PrimaryActorTick.bCanEverTick = true;
	// after every creature's tick; while paused a new game can still remove creatures, and they must disappear
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ATDCrowd* ATDCrowd::Get(UWorld* World)
{
	static TWeakObjectPtr<ATDCrowd> Current;
	if (Current.IsValid() && Current->GetWorld() == World && !Current->IsActorBeingDestroyed())
	{
		return Current.Get();
	}
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Current = World->SpawnActor<ATDCrowd>(ATDCrowd::StaticClass(), FTransform::Identity, Params);
	return Current.Get();
}

int32 ATDCrowd::FindBatch(const FTDMergedMesh& Merged)
{
	const UStaticMesh* Key = Merged.Mesh.Get();
	if (const int32* Found = BatchIndex.Find(Key))
	{
		return *Found;
	}

	UInstancedStaticMeshComponent* Comp = NewObject<UInstancedStaticMeshComponent>(this);
	Comp->SetStaticMesh(Merged.Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetCastShadow(false);
	Comp->SetupAttachment(GetRootComponent());
	Comp->RegisterComponent();
	AddInstanceComponent(Comp);

	Instanced.Add(Comp);
	Batches.AddDefaulted();
	return BatchIndex.Add(Key, Batches.Num() - 1);
}

FTDCrowdPart ATDCrowd::AddPart(const FTDMergedMesh& Merged)
{
	FTDCrowdPart Part;
	Part.Batch = FindBatch(Merged);
	FBatch& Batch = Batches[Part.Batch];
	if (Batch.FreeSlots.Num() == 0)
	{
		TArray<FTransform> New;
		New.Init(HiddenTransform(), SlotsPerGrowth);
		Instanced[Part.Batch]->AddInstances(New, false, false, false);
		for (int32 i = 0; i < SlotsPerGrowth; ++i)
		{
			Batch.FreeSlots.Add(Batch.Transforms.Num() + i);
		}
		Batch.Transforms.Append(New);
	}
	Part.Slot = Batch.FreeSlots.Pop(EAllowShrinking::No);
	return Part;
}

void ATDCrowd::RemovePart(FTDCrowdPart& Part)
{
	if (Batches.IsValidIndex(Part.Batch))
	{
		FBatch& Batch = Batches[Part.Batch];
		Batch.Transforms[Part.Slot] = HiddenTransform();
		Batch.FreeSlots.Add(Part.Slot);
		Batch.bDirty = true;
	}
	Part = FTDCrowdPart();
}

void ATDCrowd::SetPartTransform(const FTDCrowdPart& Part, const FTransform& Transform)
{
	FBatch& Batch = Batches[Part.Batch];
	Batch.Transforms[Part.Slot] = Transform;
	Batch.bDirty = true;
}

void ATDCrowd::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (int32 i = 0; i < Batches.Num(); ++i)
	{
		FBatch& Batch = Batches[i];
		if (Batch.bDirty)
		{
			Batch.bDirty = false;
			UInstancedStaticMeshComponent* Comp = Instanced[i];
			Comp->BatchUpdateInstancesTransforms(0, Batch.Transforms, false, false, true);
			// instances reach the renderer by themselves, but the bounds that keep them from being culled don't
			Comp->UpdateBounds();
			Comp->MarkRenderTransformDirty();
		}
	}
}
