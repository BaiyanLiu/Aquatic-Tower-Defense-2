// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDItems.h"
#include "TDTypes.h"
#include "TDProjectile.generated.h"

class ATDEnemy;
class ATDTower;

/**
 *  Tower projectiles:
 *  - Bolt: homing harpoon that hits a single target (continues to the last known spot if the target dies).
 *    With Piercing Bolt it continues on to further enemies.
 *  - Bomb: puffer mortar lobbed in an arc, splashing ground enemies on landing and optionally leaving shrapnel.
 */
UCLASS()
class ATDProjectile : public AActor
{
	GENERATED_BODY()

public:

	ATDProjectile();

	/** Homing projectile. Speed is in tiles per second; Style picks the look (harpoon, rock, venom spine, tooth). */
	static ATDProjectile* LaunchBolt(UWorld* World, const FVector& Start, ATDEnemy* Target, const FTDHit& Hit, int32 Pierce, float Speed, ETDTowerType Style);
	static ATDProjectile* LaunchBomb(UWorld* World, const FVector& Start, const FVector& Land, const FTDHit& Hit, float SplashTiles, float ShrapnelFraction, float Speed);

	virtual void Tick(float DeltaTime) override;

protected:

	void TickBolt(float DeltaTime);
	void TickBomb(float DeltaTime);
	void Explode();

	bool bBomb = false;
	FTDHit Hit;
	float Splash = 0.0f;
	float ShrapnelFraction = 0.0f;
	int32 PierceLeft = 0;
	float Speed = 1100.0f;		// uu per second
	bool bSpin = false;

	TWeakObjectPtr<ATDEnemy> Target;
	TArray<TWeakObjectPtr<ATDEnemy>> AlreadyHit;
	FVector LastTargetLocation = FVector::ZeroVector;

	FVector Start = FVector::ZeroVector;
	FVector Land = FVector::ZeroVector;
	float Elapsed = 0.0f;
	float FlightTime = 1.0f;
};

/**
 *  Shrapnel patch left by the puffer mortar: damages ground enemies standing in it for 2 seconds.
 */
UCLASS()
class ATDSpinePatch : public AActor
{
	GENERATED_BODY()

public:

	ATDSpinePatch();

	static ATDSpinePatch* Spawn(UWorld* World, const FVector& Location, float RadiusUU, float TotalDamage, ATDTower* Source);

	virtual void Tick(float DeltaTime) override;

protected:

	float Radius = 60.0f;
	float Dps = 0.0f;
	float TickAccumulator = 0.0f;
	TWeakObjectPtr<ATDTower> Source;
};
