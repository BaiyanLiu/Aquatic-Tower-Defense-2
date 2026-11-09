// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDCrowd.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
struct FTDMergedMesh;

/** One creature part in the crowd */
struct FTDCrowdPart
{
	int32 Batch = INDEX_NONE;
	int32 Slot = INDEX_NONE;
};

/**
 *  The solid parts of every creature, one instanced component per mesh. A wave of 300 creatures would otherwise be
 *  thousands of components that all move every frame, each with its own transform, bounds and render update.
 *  See-through parts stay ordinary components: their material isn't flagged for instancing, so a cooked game wouldn't draw them.
 */
UCLASS()
class ATDCrowd : public AActor
{
	GENERATED_BODY()

public:

	ATDCrowd();

	/** This world's crowd, spawned the first time it is asked for */
	static ATDCrowd* Get(UWorld* World);

	/** Takes a slot for one copy of Merged */
	FTDCrowdPart AddPart(const FTDMergedMesh& Merged);

	/** Hides the part and frees its slot */
	void RemovePart(FTDCrowdPart& Part);

	/** Takes effect at the end of this frame's ticks, when every creature has moved */
	void SetPartTransform(const FTDCrowdPart& Part, const FTransform& Transform);

	virtual void Tick(float DeltaTime) override;

private:

	struct FBatch
	{
		TArray<FTransform> Transforms;
		TArray<int32> FreeSlots;
		bool bDirty = false;
	};

	int32 FindBatch(const FTDMergedMesh& Merged);

	TMap<const UStaticMesh*, int32> BatchIndex;
	TArray<FBatch> Batches;

	/** Same order as Batches */
	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Instanced;
};
