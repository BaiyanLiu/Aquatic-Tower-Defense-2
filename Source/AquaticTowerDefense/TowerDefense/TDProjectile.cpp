// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDProjectile.h"
#include "TDModels.h"
#include "TDEnemy.h"
#include "TDTower.h"
#include "TDGameMode.h"
#include "TDFx.h"
#include "TDTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

namespace
{
	constexpr float BombArcHeight = 160.0f;
	constexpr float PierceRange = 2.0f * TD::TileSize;
	constexpr float PatchDuration = 2.0f;
}

ATDProjectile::ATDProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	InitialLifeSpan = 6.0f;
}

ATDProjectile* ATDProjectile::LaunchBolt(UWorld* World, const FVector& InStart, ATDEnemy* InTarget, const FTDHit& InHit, int32 Pierce, float InSpeed, ETDTowerType Style)
{
	if (!World || !InTarget)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATDProjectile* Bolt = World->SpawnActor<ATDProjectile>(ATDProjectile::StaticClass(), InStart, FRotator::ZeroRotator, Params);
	if (Bolt)
	{
		Bolt->bBomb = false;
		Bolt->Hit = InHit;
		Bolt->PierceLeft = Pierce;
		Bolt->Target = InTarget;
		Bolt->LastTargetLocation = InTarget->GetAimLocation();
		Bolt->Speed = FMath::Max(2.0f, InSpeed) * TD::TileSize;

		USceneComponent* Root = Bolt->GetRootComponent();
		// crits get a gold head
		FTDModelOptions Options;
		Options.bShadows = false;
		if (InHit.bCrit)
		{
			Options.Recolor = [](const FLinearColor& Color, uint8 Flags)
			{
				return (Flags & TDPartFlag::Tip) ? FLinearColor(1.0f, 0.8f, 0.2f) : Color;
			};
		}
		ETDModel Model = ETDModel::ShotHarpoon;
		switch (Style)
		{
		case ETDTowerType::Octopus:	Model = ETDModel::ShotRock; Bolt->bSpin = true; break;
		case ETDTowerType::Lionfish:	Model = ETDModel::ShotLionfish; break;
		case ETDTowerType::Sawfish:	Model = ETDModel::ShotSawfish; Bolt->bSpin = true; break;
		default:					break;
		}
		TD::BuildModel(Bolt, Root, Model, Options);
	}
	return Bolt;
}

ATDProjectile* ATDProjectile::LaunchBomb(UWorld* World, const FVector& InStart, const FVector& InLand, const FTDHit& InHit, float SplashTiles, float InShrapnel, float InSpeed)
{
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATDProjectile* Bomb = World->SpawnActor<ATDProjectile>(ATDProjectile::StaticClass(), InStart, FRotator::ZeroRotator, Params);
	if (Bomb)
	{
		Bomb->bBomb = true;
		Bomb->Hit = InHit;
		Bomb->Splash = SplashTiles;
		Bomb->ShrapnelFraction = InShrapnel;
		Bomb->Start = InStart;
		Bomb->Land = InLand;
		Bomb->FlightTime = FMath::Clamp(FVector::Dist2D(InStart, InLand) / (FMath::Max(1.0f, InSpeed) * TD::TileSize), 0.35f, 1.2f);

		// an angry baby puffer with a lit fuse
		FTDModelOptions Options;
		Options.bShadows = false;
		TD::BuildModel(Bomb, Bomb->GetRootComponent(), ETDModel::ShotBomb, Options);
	}
	return Bomb;
}

void ATDProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bBomb)
	{
		TickBomb(DeltaTime);
	}
	else
	{
		TickBolt(DeltaTime);
	}
}

void ATDProjectile::TickBolt(float DeltaTime)
{
	ATDEnemy* Enemy = Target.Get();
	if (Enemy && Enemy->IsAlive())
	{
		LastTargetLocation = Enemy->GetAimLocation();
	}
	else
	{
		Enemy = nullptr;
	}

	const FVector Here = GetActorLocation();
	const FVector ToTarget = LastTargetLocation - Here;
	const float Step = Speed * DeltaTime;

	if (ToTarget.Size() <= Step + 8.0f)
	{
		if (Enemy)
		{
			AlreadyHit.Add(Enemy);
			ATDEnemy::DeliverHit(Enemy, Hit);
		}

		// Piercing Bolt: carry on to the nearest enemy not hit yet
		ATDEnemy* Next = nullptr;
		if (PierceLeft > 0)
		{
			if (const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
			{
				float Best = PierceRange;
				for (ATDEnemy* Candidate : GameMode->GetEnemies())
				{
					if (!Candidate || !Candidate->IsAlive() || AlreadyHit.Contains(Candidate))
					{
						continue;
					}
					const float Distance = FVector::Dist2D(Candidate->GetActorLocation(), LastTargetLocation);
					if (Distance < Best)
					{
						Best = Distance;
						Next = Candidate;
					}
				}
			}
		}

		if (Next)
		{
			--PierceLeft;
			Target = Next;
			LastTargetLocation = Next->GetAimLocation();
			return;
		}

		Destroy();
		return;
	}

	const FVector Dir = ToTarget.GetSafeNormal();
	FRotator Rot = Dir.Rotation();
	if (bSpin)
	{
		Rot.Yaw += GetGameTimeSinceCreation() * 900.0f;
	}
	SetActorLocationAndRotation(Here + Dir * Step, Rot);
}

void ATDProjectile::TickBomb(float DeltaTime)
{
	Elapsed += DeltaTime;
	const float Alpha = FMath::Clamp(Elapsed / FlightTime, 0.0f, 1.0f);

	FVector Pos = FMath::Lerp(Start, Land, Alpha);
	Pos.Z += FMath::Sin(Alpha * PI) * BombArcHeight;
	// face the landing spot and wobble, so the puffer's face stays readable in flight
	const float Heading = (Land - Start).GetSafeNormal2D().Rotation().Yaw;
	SetActorLocationAndRotation(Pos, FRotator(FMath::Sin(Elapsed * 12.0f) * 15.0f, Heading + FMath::Sin(Elapsed * 7.0f) * 20.0f, 0));

	if (Alpha >= 1.0f)
	{
		Explode();
	}
}

void ATDProjectile::Explode()
{
	const float RadiusUU = Splash * TD::TileSize;
	if (const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		// copy: kills remove enemies from the game mode list
		TArray<ATDEnemy*> Hits;
		for (ATDEnemy* Enemy : GameMode->GetEnemies())
		{
			if (Enemy && Enemy->IsAlive() && !Enemy->IsFlying() && FVector::Dist2D(Enemy->GetActorLocation(), Land) <= RadiusUU)
			{
				Hits.Add(Enemy);
			}
		}
		for (ATDEnemy* Enemy : Hits)
		{
			Enemy->ApplyHit(Hit);
		}
	}

	if (ShrapnelFraction > 0.0f)
	{
		ATDSpinePatch::Spawn(GetWorld(), Land, RadiusUU * 0.6f, Hit.Damage * ShrapnelFraction, Hit.Source.Get());
	}

	ATDFx::SpawnRing(GetWorld(), Land + FVector(0, 0, 6), RadiusUU, FLinearColor(0.75f, 0.62f, 0.3f));
	Destroy();
}

// ---------------------------------------------------------------------------------------------
// Spine patch
// ---------------------------------------------------------------------------------------------

ATDSpinePatch::ATDSpinePatch()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ATDSpinePatch* ATDSpinePatch::Spawn(UWorld* World, const FVector& Location, float RadiusUU, float TotalDamage, ATDTower* InSource)
{
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATDSpinePatch* Patch = World->SpawnActor<ATDSpinePatch>(ATDSpinePatch::StaticClass(), FVector(Location.X, Location.Y, 0.0f), FRotator::ZeroRotator, Params);
	if (Patch)
	{
		Patch->Radius = RadiusUU;
		Patch->Dps = TotalDamage / PatchDuration;
		Patch->Source = InSource;
		Patch->SetLifeSpan(PatchDuration);

		// poison-tipped spines stuck in the sand, spread to the patch radius
		FTDModelOptions Options;
		Options.MinSize = 2.5f;
		Options.bShadows = false;
		// each spread is a mesh of its own, so round it: an item's exact splash radius shouldn't make a new one
		const float Spread = FMath::GridSnap(RadiusUU, 5.0f) / 45.0f;
		Options.RootSpread = FVector(Spread, Spread, 1.0f);
		USceneComponent* Pivot = TD::AddPivot(Patch, Patch->GetRootComponent(), FVector::ZeroVector);
		Pivot->SetRelativeRotation(FRotator(0, FMath::FRandRange(0.0f, 360.0f), 0));
		TD::BuildModel(Patch, Pivot, ETDModel::Spines, Options);
	}
	return Patch;
}

void ATDSpinePatch::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TickAccumulator += DeltaTime;
	if (TickAccumulator < 0.25f)
	{
		return;
	}
	const float Interval = TickAccumulator;
	TickAccumulator = 0.0f;

	const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	if (!GameMode)
	{
		return;
	}

	TArray<ATDEnemy*> Hits;
	for (ATDEnemy* Enemy : GameMode->GetEnemies())
	{
		if (Enemy && Enemy->IsAlive() && !Enemy->IsFlying() && FVector::Dist2D(Enemy->GetActorLocation(), GetActorLocation()) <= Radius)
		{
			Hits.Add(Enemy);
		}
	}
	for (ATDEnemy* Enemy : Hits)
	{
		Enemy->DealDamage(Dps * Interval, Source.Get(), TD::XpScaleDot);
	}
}
