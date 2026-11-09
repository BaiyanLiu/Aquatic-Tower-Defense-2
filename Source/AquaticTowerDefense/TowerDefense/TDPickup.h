// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDItems.h"
#include "TDPickup.generated.h"

class UPointLightComponent;

/**
 *  An item dropped by a creature: a bobbing glowing sphere colored by rarity.
 *  After a short float it is collected into master storage automatically.
 *  If storage is full it waits on the seabed until it washes away after 12 seconds.
 */
UCLASS()
class ATDPickup : public AActor
{
	GENERATED_BODY()

public:

	ATDPickup();

	static ATDPickup* Spawn(UWorld* World, const FVector& Location, const FTDItem& Item);

	virtual void Tick(float DeltaTime) override;


	/** Tries to move the item into storage. Returns true if collected (the pickup is destroyed). */
	bool TryCollect(bool bFromClick);

protected:

	FTDItem Item;
	FVector Base = FVector::ZeroVector;
	float Age = 0.0f;

	UPROPERTY()
	TObjectPtr<USceneComponent> Orb;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;
};
