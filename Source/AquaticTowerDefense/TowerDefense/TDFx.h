// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDFx.generated.h"

class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 *  Short-lived shape-based effects: jellyfish pulse ring, mortar burst, eel lightning and the death splash.
 */
UCLASS()
class ATDFx : public AActor
{
	GENERATED_BODY()

public:

	ATDFx();

	static ATDFx* SpawnRing(UWorld* World, const FVector& Center, float Radius, const FLinearColor& Color);
	static ATDFx* SpawnBurst(UWorld* World, const FVector& Center, float Radius, const FLinearColor& Color);
	static ATDFx* SpawnBolt(UWorld* World, const TArray<FVector>& Points, const FLinearColor& Color);

	/** Droplets of the creature's color spraying out and sinking, plus a few bubbles rising */
	static ATDFx* SpawnSplash(UWorld* World, const FVector& Center, const FLinearColor& Color, float Size = 1.0f);

	virtual void Tick(float DeltaTime) override;

protected:

	static ATDFx* SpawnAt(UWorld* World, const FVector& Location, float Life);

	enum class EKind : uint8 { Ring, Burst, Bolt, Splash };

	EKind Kind = EKind::Ring;
	float Radius = 100.0f;
	float Life = 0.4f;
	float Age = 0.0f;

	/** Adds one component that draws many droplets, so a ring or a splash doesn't register a component for each */
	UInstancedStaticMeshComponent* AddDroplets(UMaterialInterface* Material, int32 Count);

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Drops;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Bubbles;

	/** Burst and bolt pieces */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	/** One droplet or bubble of a splash, drops first; negative buoyancy sinks */
	struct FMote
	{
		FVector Location;
		FVector Velocity;	// uu/s
		float Size;
		float Buoyancy;
	};
	TArray<FMote> Motes;
};
