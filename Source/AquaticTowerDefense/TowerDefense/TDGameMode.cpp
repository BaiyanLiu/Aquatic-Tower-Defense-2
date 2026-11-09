// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDGameMode.h"
#include "TDAudio.h"
#include "TDGrid.h"
#include "TDTower.h"
#include "TDEnemy.h"
#include "TDProjectile.h"
#include "TDPickup.h"
#include "TDFx.h"
#include "TDModels.h"
#include "TDSaveGame.h"
#include "TDMastery.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "HAL/IConsoleManager.h"
#include "TDHUD.h"
#include "TDCameraPawn.h"
#include "TDPlayerController.h"
#include "AquaticTowerDefense.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float SpawnGroupGap = 1.6f;
}

ATDGameMode::ATDGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	DefaultPawnClass = ATDCameraPawn::StaticClass();
	PlayerControllerClass = ATDPlayerController::StaticClass();
	HUDClass = ATDHUD::StaticClass();
}

void ATDGameMode::BeginPlay()
{
	Super::BeginPlay();

	SetupEnvironment();
	CaptureDepthBase();
	const double PrewarmStart = FPlatformTime::Seconds();
	TD::PrewarmModels();
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("Merged the tower, creature, shot and drop models in %.0f ms"), (FPlatformTime::Seconds() - PrewarmStart) * 1000.0);

	// the board behind the main menu is already a random map
	int32 CommandLineMap = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("ATDMap="), CommandLineMap))
	{
		ForcedMap = CommandLineMap;
	}
	TD::SetMap(ForcedMap >= 0 ? ForcedMap : FMath::RandRange(0, TD::MapCount - 1));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Grid = GetWorld()->SpawnActor<ATDGrid>(ATDGrid::StaticClass(), FTransform::Identity, Params);

	// capture the ambient light once the board exists
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		if (USkyLightComponent* Light = It->GetLightComponent())
		{
			if (!Light->bRealTimeCapture)
			{
				Light->RecaptureSky();
			}
		}
	}

	UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	LoadSettings();
	// the run's slot starts as the most recently saved one
	RefreshSlotInfo();
	for (int32 Slot = 0; Slot < SaveSlotCount; ++Slot)
	{
		if (SlotInfos[Slot].bUsed && (!SlotInfos[CurrentSlot].bUsed || SlotInfos[Slot].SavedAt > SlotInfos[CurrentSlot].SavedAt))
		{
			CurrentSlot = Slot;
		}
	}
	ResetRunStats();
	TDAudio::UpdateMusic(this);
	Toast(TEXT("Build a maze, then release wave 1 (Space)"), FLinearColor(0.6f, 1.0f, 0.95f), ETDToast::Info);

	if (!FParse::Param(FCommandLine::Get(), TEXT("ATDSkipMenu")))
	{
		OpenMenu(ETDMenu::Main);
	}
}

void ATDGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FlushMastery();
	Super::EndPlay(EndPlayReason);
}

void ATDGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer || NewPlayer->IsPendingKillPending())
	{
		return;
	}
	RestartPlayerAtTransform(NewPlayer, FTransform::Identity);
}

// ---------------------------------------------------------------------------------------------
// Environment
// ---------------------------------------------------------------------------------------------

void ATDGameMode::SetupEnvironment()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (!TActorIterator<ADirectionalLight>(World))
	{
		ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0, 0, 1000), FRotator(-55, 25, 0), Params);
		// GetComponent() is editor-only on ADirectionalLight; the light component works in every build
		if (ULightComponent* Light = Sun ? Sun->GetLightComponent() : nullptr)
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(5.0f);
			Light->SetLightColor(FLinearColor(0.7f, 0.95f, 1.0f));
			if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Light))
			{
				SunLight->SetShadowAmount(0.75f);
				SunLight->SetForwardShadingPriority(1);
			}
		}

		// light bounced off the floor: shines up and away from the camera, so it reaches the sides and undersides
		// of towers and walls and misses the floor. No shadows, so it only costs one cheap lighting pass.
		ADirectionalLight* Fill = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector(0, 0, 1000), FRotator(15, 45, 0), Params);
		if (ULightComponent* Light = Fill ? Fill->GetLightComponent() : nullptr)
		{
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetIntensity(1.5f);
			Light->SetLightColor(FLinearColor(0.55f, 0.85f, 0.9f));
			Light->SetCastShadows(false);
			Light->SetSpecularScale(0.0f);
			Light->SetIndirectLightingIntensity(0.0f);
		}
	}

	if (!TActorIterator<ASkyLight>(World))
	{
		ASkyLight* Sky = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector(0, 0, 500), FRotator::ZeroRotator, Params);
		if (Sky && Sky->GetLightComponent())
		{
			USkyLightComponent* Light = Sky->GetLightComponent();
			Light->SetMobility(EComponentMobility::Movable);
			// captured once from the underwater scene (fog) instead of real-time capture, which needs a sky
			// atmosphere and otherwise shows an on-screen warning and lights nothing
			Light->SourceType = ESkyLightSourceType::SLS_CapturedScene;
			Light->bLowerHemisphereIsBlack = true;
			Light->SetLowerHemisphereColor(FLinearColor(0.05f, 0.25f, 0.3f));
			Light->SetLightColor(FLinearColor(0.6f, 0.9f, 1.0f));
			Light->SetIntensity(1.5f);
			Light->SetRealTimeCapture(false);
		}
	}

	if (!TActorIterator<AExponentialHeightFog>(World))
	{
		AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector(0, 0, -200), FRotator::ZeroRotator, Params);
		if (Fog && Fog->GetComponent())
		{
			UExponentialHeightFogComponent* FogComp = Fog->GetComponent();
			FogComp->SetFogDensity(0.015f);
			FogComp->SetFogHeightFalloff(0.2f);
			FogComp->SetFogInscatteringColor(FLinearColor(0.015f, 0.09f, 0.13f));
			FogComp->SetFogMaxOpacity(0.5f);
			FogComp->SetStartDistance(0.0f);
		}
	}

	bool bHasUnboundVolume = false;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		bHasUnboundVolume |= It->bUnbound != 0;
	}
	if (!bHasUnboundVolume)
	{
		APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FTransform::Identity, Params);
		if (Volume)
		{
			Volume->bUnbound = true;
			Volume->Priority = 1.0f;
			Volume->Settings.bOverride_SceneColorTint = true;
			// a light aqua tint that still lets reds and corals through, with richer colour and a touch more contrast
			Volume->Settings.SceneColorTint = FLinearColor(0.9f, 0.99f, 1.0f);
			Volume->Settings.bOverride_ColorSaturation = true;
			Volume->Settings.ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, 1.25f);
			Volume->Settings.bOverride_ColorContrast = true;
			Volume->Settings.ColorContrast = FVector4(1.0f, 1.0f, 1.0f, 1.08f);
			Volume->Settings.bOverride_VignetteIntensity = true;
			Volume->Settings.VignetteIntensity = 0.55f;
			Volume->Settings.bOverride_AutoExposureBias = true;
			Volume->Settings.AutoExposureBias = -0.6f;
			Volume->Settings.bOverride_BloomIntensity = true;
			Volume->Settings.BloomIntensity = 0.45f;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Waves
// ---------------------------------------------------------------------------------------------

void ATDGameMode::BuildWave(int32 N, TArray<FPendingSpawn>& OutSpawns)
{
	OutSpawns.Reset();
	float T = 0.0f;

	// mixed waves start every group together (a little staggered) so the schools swim in side by side
	const bool bMixed = IsMixedWave(N);
	const FTDRunRules& Rules = TD::Run();
	int32 GroupIndex = 0;
	auto Group = [&OutSpawns, &T, &GroupIndex, bMixed, &Rules](ETDEnemyType Type, int32 Count, float Gap)
	{
		if (Count <= 0)
		{
			return;
		}
		// mutators: Swimmer Frenzy doubles swimmer (flying) groups, Swarm doubles every regular group at half health each
		// (the gap shrinks to match, so a group takes as long to arrive as before)
		float HealthScale = 1.0f;
		if (TD::Enemy(Type).bFlying && Rules.FlyerGroupScale() > 1)
		{
			Count *= Rules.FlyerGroupScale();
			Gap /= Rules.FlyerGroupScale();
		}
		if (Rules.IsSwarm() && !TD::Enemy(Type).bBoss)
		{
			Count *= 2;
			Gap *= 0.5f;
			HealthScale = 0.5f;
		}
		const float Start = bMixed ? GroupIndex * 0.35f : T;
		const float Spread = bMixed ? 1.6f : 1.0f;
		for (int32 i = 0; i < Count; ++i)
		{
			OutSpawns.Add({ Type, Start + i * Gap * Spread, 0, HealthScale });
		}
		T += (Count - 1) * Gap + SpawnGroupGap;
		++GroupIndex;
	};

	// Abyss 3: from wave 10 a Manta group joins every wave that has no flyers
	auto AddAbyssFlyers = [&OutSpawns, &Group, &Rules, N]()
	{
		if (!Rules.AddsFlyerGroups() || N < TD::AbyssFlyerWave)
		{
			return;
		}
		for (const FPendingSpawn& Spawn : OutSpawns)
		{
			if (TD::Enemy(Spawn.Type).bFlying)
			{
				return;
			}
		}
		Group(ETDEnemyType::Manta, 2 + N / 4, 0.9f);
	};

	if (TD::IsTitanWave(N))
	{
		// the final wave (and every 50th endless wave): a big escort, then the Abyssal Kraken alone at the end
		Group(ETDEnemyType::Crab, 6 + N, 0.5f);
		Group(ETDEnemyType::Cuttlefish, 4, 1.0f);
		Group(ETDEnemyType::ShockEel, 3, 1.5f);
		Group(ETDEnemyType::Kraken, N / 25, 5.0f);
		AddAbyssFlyers();
		float Last = 0.0f;
		for (const FPendingSpawn& Spawn : OutSpawns)
		{
			Last = FMath::Max(Last, Spawn.Time);
		}
		OutSpawns.StableSort([](const FPendingSpawn& A, const FPendingSpawn& B) { return A.Time < B.Time; });
		OutSpawns.Add({ ETDEnemyType::Kraken, Last + 3.0f * SpawnGroupGap, TDMod::Titan });
		return;
	}

	if (N % 10 == 0)
	{
		Group(ETDEnemyType::Crab, 6 + N, 0.55f);
		if (N >= 20)
		{
			Group(ETDEnemyType::Cuttlefish, 2, 1.0f);	// escorts that armor the boss
		}
		Group(ETDEnemyType::Kraken, N / 10, 5.0f);
		AddAbyssFlyers();
		return;
	}

	Group(ETDEnemyType::Crab, 5 + FMath::FloorToInt(1.2f * N), FMath::Max(0.42f, 1.0f - 0.02f * N));
	if (N >= 3 && N % 2 == 1)
	{
		Group(ETDEnemyType::Piranha, 4 + N, 0.3f);
	}
	if (N >= 4 && N % 3 == 1)
	{
		Group(ETDEnemyType::Manta, 2 + N / 2, 0.9f);
	}
	if (N >= 5 && N % 2 == 0)
	{
		Group(ETDEnemyType::Turtle, 1 + N / 3, 1.6f);
	}
	if (N >= 6 && N % 3 == 0)
	{
		Group(ETDEnemyType::SeaStar, 2 + N / 4, 1.2f);
	}
	if (N >= 7 && N % 4 == 3)
	{
		Group(ETDEnemyType::Cuttlefish, 1 + N / 8, 2.0f);
	}
	if (N >= 8 && N % 4 == 0)
	{
		Group(ETDEnemyType::ShockEel, 1 + N / 8, 2.0f);
	}
	AddAbyssFlyers();

	if (bMixed)
	{
		OutSpawns.StableSort([](const FPendingSpawn& A, const FPendingSpawn& B) { return A.Time < B.Time; });
	}

	// every 5th wave ends with an elite mini-boss
	if (IsMiniBossWave(N))
	{
		const ETDEnemyType Elites[] = { ETDEnemyType::Turtle, ETDEnemyType::Cuttlefish, ETDEnemyType::ShockEel, ETDEnemyType::SeaStar };
		const float Last = OutSpawns.Num() > 0 ? OutSpawns.Last().Time : 0.0f;
		OutSpawns.Add({ Elites[(N / 5) % UE_ARRAY_COUNT(Elites)], Last + SpawnGroupGap, TDMod::Elite });
	}
}

void ATDGameMode::ReleaseNextWave()
{
	if (bGameOver || !Grid)
	{
		return;
	}

	// the final wave has to be beaten (and endless mode chosen) before anything follows it
	if (Wave >= TD::FinalWave && !bEndless)
	{
		Toast(IsWaveActive() ? FString::Printf(TEXT("Wave %d is the final wave: defeat the Abyssal Kraken!"), TD::FinalWave) : FString(TEXT("Choose endless mode to keep going")),
			FLinearColor(1.0f, 0.6f, 0.4f), ETDToast::Refusal);
		return;
	}

	// a boon offer waits for its pick
	if (HasBoonOffer())
	{
		Toast(TEXT("Choose a boon first"), FLinearColor(1.0f, 0.6f, 0.4f), ETDToast::Refusal);
		if (Menu == ETDMenu::None)
		{
			OpenMenu(ETDMenu::Boon);
		}
		return;
	}

	if (IsWaveActive())
	{
		const int32 Bonus = GetEarlyBonus();
		Pearls += Bonus;
		Income.EarlySend += Bonus;
		Toast(FString::Printf(TEXT("Wave %d sent early  +%d pearls"), Wave + 1, Bonus), FLinearColor(1.0f, 0.85f, 0.4f), ETDToast::Info);
	}
	else
	{
		// the build phase ends: full refunds expire
		++BuildPhaseId;
		Toast(TD::IsTitanWave(Wave + 1) ? FString::Printf(TEXT("Wave %d: the Abyssal Kraken rises!"), Wave + 1) : FString::Printf(TEXT("Wave %d incoming!"), Wave + 1),
			TD::IsTitanWave(Wave + 1) ? FLinearColor(1.0f, 0.4f, 0.45f) : FLinearColor(0.6f, 1.0f, 0.95f), (TD::IsTitanWave(Wave + 1) || (Wave + 1) % 10 == 0) ? ETDToast::Important : ETDToast::Info);
		TDAudio::Play(this, ETDSound::WaveStart, 0.8f);
	}

	++Wave;

	FWaveRun& Run = ActiveWaves.AddDefaulted_GetRef();
	Run.Number = Wave;
	Run.HealthMultiplier = TD::WaveHealthMultiplier(Wave);
	BuildWave(Wave, Run.Spawns);
}

void ATDGameMode::SpawnEnemy(const FPendingSpawn& Spawn, FWaveRun& Run)
{
	const ETDEnemyType Type = Spawn.Type;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATDEnemy* Enemy = GetWorld()->SpawnActor<ATDEnemy>(ATDEnemy::StaticClass(), TD::CellToWorld(TD::SpawnCell()), FRotator::ZeroRotator, Params);
	if (Enemy)
	{
		Enemy->InitEnemy(Type, Run.HealthMultiplier * EnemyHealthPercent / 100.0f * Spawn.HealthScale, Run.Number, Grid);
		// Frenzied Currents (cursed boon) and Rising Tide (mutator)
		const float SpeedScale = TD::Run().EnemySpeedScale() * TD::Run().RisingTideScale(Run.Number);
		if (SpeedScale != 1.0f)
		{
			Enemy->ScaleSpeed(SpeedScale);
		}
		if (Spawn.HealthScale < 1.0f)
		{
			Enemy->SetRewardScale(Spawn.HealthScale);
		}

		// endless mode: creatures get faster every wave
		if (Run.Number > TD::FinalWave)
		{
			Enemy->ScaleSpeed(TD::EndlessSpeedMultiplier(Run.Number));
		}

		// from wave 6 some regular creatures come fast or shielded (much more often in endless mode)
		uint8 Mods = Spawn.Mods;
		if (Run.Number >= 6 && !Enemy->GetDef().bBoss && !(Mods & TDMod::Elite))
		{
			const float Chance = TD::ModifierChance(Run.Number);
			if (FMath::FRand() < Chance)
			{
				Mods |= TDMod::Fast;
			}
			if (FMath::FRand() < Chance)
			{
				Mods |= TDMod::Shielded;
			}
		}
		if (Mods != 0)
		{
			Enemy->ApplyModifiers(Mods);
		}
		Enemies.Add(Enemy);
		++Run.Alive;
	}
}

void ATDGameMode::DebugSpawnEnemy(ETDEnemyType Type, float HealthMultiplier, float SpeedScale)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ATDEnemy* Enemy = GetWorld()->SpawnActor<ATDEnemy>(ATDEnemy::StaticClass(), TD::CellToWorld(TD::SpawnCell()), FRotator::ZeroRotator, Params))
	{
		Enemy->InitEnemy(Type, HealthMultiplier, FMath::Max(1, Wave), Grid);
		Enemy->ScaleSpeed(SpeedScale);
		Enemies.Add(Enemy);
	}
}

void ATDGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// the boss loop plays while a Kraken wave (every 10th, the Abyssal Kraken's included) is out
	bool bBossWave = false;
	for (const FWaveRun& Active : ActiveWaves)
	{
		bBossWave |= Active.Number % 10 == 0;
	}
	TDAudio::SetBossMusic(bBossWave && !bGameOver);

	if (bGameOver)
	{
		return;
	}

	// a boon offer comes back after the pause menu (or anything else) closes
	if (Menu == ETDMenu::None && HasBoonOffer())
	{
		OpenMenu(ETDMenu::Boon);
	}

	// keep the pause in sync with open menus (the controller may not exist yet when the main menu opens)
	if (Menu != ETDMenu::None && GetWorld()->GetTimeSeconds() >= 0.3f)
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (!PC->IsPaused())
			{
				PC->SetPause(true);
			}
		}
	}

	// play time for the run summary: real seconds, only while no menu is open
	if (Menu == ETDMenu::None)
	{
		PlayTime += FApp::GetDeltaTime();
	}

	UpdateAuras();

	// XP and pearl trickle while waves are running
	if (IsWaveActive() && Grid)
	{
		for (ATDTower* Tower : Grid->GetTowers())
		{
			if (Tower)
			{
				const int32 Trickle = Tower->TickTrickle(DeltaTime);
				Pearls += Trickle;
				Income.Trickle += Trickle;
			}
		}
	}

	for (int32 i = 0; i < ActiveWaves.Num(); ++i)
	{
		FWaveRun& Run = ActiveWaves[i];
		Run.Elapsed += DeltaTime;
		while (Run.NextSpawn < Run.Spawns.Num() && Run.Spawns[Run.NextSpawn].Time <= Run.Elapsed)
		{
			SpawnEnemy(Run.Spawns[Run.NextSpawn], Run);
			++Run.NextSpawn;
		}
	}

	for (int32 i = ActiveWaves.Num() - 1; i >= 0; --i)
	{
		const FWaveRun& Run = ActiveWaves[i];
		if (Run.NextSpawn >= Run.Spawns.Num() && Run.Alive <= 0)
		{
			const int32 Cleared = Run.Number;
			const FTDRunRules& Rules = TD::Run();
			// Abyss 4 and Poor Waters take the clear bonus away; Deep Pockets pays interest on what's left unspent
			const int32 Bonus = Rules.HasWaveClearPearls() ? TD::WaveClearPearlsBase + TD::WaveClearPearlsPerWave * Cleared : 0;
			const int32 Interest = Rules.Has(ETDBoon::DeepPockets) ? TD::Interest(Pearls) : 0;
			Pearls += Bonus + Interest;
			Income.WaveClear += Bonus + Interest;
			int32 LivesGained = 0;
			if (Rules.Has(ETDBoon::ThickReef) && Cleared % TD::ThickReefEvery == 0)
			{
				++Lives;
				LivesGained = 1;
			}
			// unique legendaries: Heart of Coral counts the wave, Gilded Conch pays its tower's level
			int32 ConchPearls = 0;
			if (Grid)
			{
				for (ATDTower* Tower : Grid->GetTowers())
				{
					if (Tower)
					{
						Tower->AddHeartWave();
						ConchPearls += Tower->HasUnique(ETDUnique::GildedConch) ? Tower->GetLevel() : 0;
					}
				}
			}
			Pearls += ConchPearls;
			Income.WaveClear += ConchPearls;
			// a small card at the reef instead of a toast
			ShowWaveClearCard(Cleared, Bonus + ConchPearls, Interest, FMath::RoundToInt(TD::WaveClearXp), LivesGained);
			UE_LOG(LogAquaticTowerDefense, Log, TEXT("Wave %d cleared: +%d pearls, +%d interest, +%d lives"), Cleared, Bonus, Interest, LivesGained);
			if (Grid)
			{
				for (ATDTower* Tower : Grid->GetTowers())
				{
					if (Tower)
					{
						Tower->AddXp(TD::WaveClearXp);
					}
				}
			}
			WavesCleared = FMath::Max(WavesCleared, Cleared);
			UpdateRecords(false);
			ActiveWaves.RemoveAt(i);

			// boon offers after waves 25, 50 and 75 (made before the autosave below, so loading keeps the same offer)
			if (bBoonsEnabled && TD::IsBoonMilestone(Cleared) && !HasBoonOffer())
			{
				OfferBoons();
			}

			// autosave between waves so "Continue" picks up from here
			if (ActiveWaves.Num() == 0)
			{
				SaveGame(true);

				// the final wave is beaten: victory, with the choice of endless mode
				if (HasWonAwaitingChoice())
				{
					UpdateRecords(true);
					Toast(TEXT("The reef is safe!"), FLinearColor(1.0f, 0.85f, 0.4f), ETDToast::Important);
					TDAudio::Play(this, ETDSound::WaveStart, 1.0f);
					OpenMenu(ETDMenu::Victory);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Enemies
// ---------------------------------------------------------------------------------------------

void ATDGameMode::RemoveEnemy(ATDEnemy* Enemy, bool bDying)
{
	for (FWaveRun& Run : ActiveWaves)
	{
		if (Run.Number == Enemy->GetWave())
		{
			--Run.Alive;
			break;
		}
	}
	// out of the list straight away, so nothing targets it, paths around it or waits for it to clear the wave
	Enemies.Remove(Enemy);
	if (bDying)
	{
		Enemy->StartDying();
	}
	else
	{
		Enemy->Destroy();
	}
}

void ATDGameMode::OnEnemyKilled(ATDEnemy* Enemy, ATDTower* Killer)
{
	if (!Enemy)
	{
		return;
	}
	if (Enemy->IsTitan())
	{
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("TITAN killed (wave %d): max shield %.0f  max health %.0f  alive %.1f s"),
			Enemy->GetWave(), Enemy->GetMaxShield(), Enemy->GetMaxHealth(), Enemy->GetGameTimeSinceCreation());
	}

	const FTDEnemyDef& Def = Enemy->GetDef();
	const FTDRunRules& Rules = TD::Run();

	// Swarm creatures give half, Bounty Hunters 25% more; the fractions carry over to the next kill
	const float RewardScale = Enemy->GetRewardScale() * Rules.KillPearlScale();
	int32 Reward = Enemy->GetReward();
	if (RewardScale != 1.0f)
	{
		const float Exact = Reward * RewardScale + RewardCarry;
		Reward = FMath::FloorToInt(Exact);
		RewardCarry = Exact - Reward;
	}
	Income.Kills += Reward;
	float DropChance = Enemy->GetDropChance() * Enemy->GetRewardScale() * Rules.DropChanceScale();
	if (Killer)
	{
		// kill record, XP bonus, pearl bounty and item luck from the killing tower
		Killer->AddKill();
		// tower mastery: lifetime kills per tower type, only from runs that count
		if (CountsForRecords())
		{
			const ETDTowerType KillerType = Killer->GetTowerType();
			bMasteryDirty = true;
			if (const int32 NewRank = TD::AddMasteryKill(TD::MutableLifetimeKills(), KillerType, true))
			{
				Toast(FString::Printf(TEXT("%s mastery: %s (%s kills)"), Killer->GetDef().Name, TD::MasteryName(NewRank), *FText::AsNumber(TD::MasteryThreshold(NewRank)).ToString()), TD::MasteryColor(NewRank), ETDToast::Important);
				RefreshMasteryTrims(KillerType);
				TDAudio::Play(this, ETDSound::LevelUp);
				FlushMastery();
			}
		}
		Killer->AddXp(Def.Reward * TD::KillXpPerReward);
		// (Gilded Conch, a unique charm: one more pearl per kill)
		const int32 Bounty = FMath::RoundToInt(Def.Reward * Killer->GetStats().BountyPct * RewardScale) + (Killer->HasUnique(ETDUnique::GildedConch) ? TD::ConchKillPearls : 0);
		Reward += Bounty;
		Income.Bounty += Bounty;
		DropChance *= 1.0f + Killer->GetStats().ItemChance;
	}

	Pearls += Reward;
	FloatingText(Enemy->GetAimLocation() + FVector(0, 0, 30), FString::Printf(TEXT("+%d"), Reward), FLinearColor(1.0f, 0.88f, 0.35f));
	ATDFx::SpawnSplash(GetWorld(), Enemy->GetAimLocation(), TD::EnemyIconColor(Enemy->GetEnemyType()), Def.bBoss ? 2.2f : 1.0f);
	TDAudio::Play(this, ETDSound::Kill, Def.bBoss || Enemy->IsElite() ? 1.0f : 0.6f);

	// item drops: a chance above 1 gives several rolls (original rule); item level follows the wave
	const int32 ItemLevel = Enemy->GetWave() + Def.ItemLevelBonus;
	int32 Dropped = 0;
	while (DropChance > 0.0f)
	{
		if (FMath::FRand() < FMath::Min(1.0f, DropChance))
		{
			ETDRarity Rarity = TD::RollRarity(ItemLevel, (Def.bBoss && Dropped == 0) ? ETDRarity::Epic : ETDRarity::Common);
			// Lucky Find and Hungry Deep roll better drops
			Rarity = (ETDRarity)FMath::Min((int32)ETDRarity::Legendary, (int32)Rarity + Rules.DropRarityBonus());
			const FVector Offset = FRotator(0, Dropped * 120.0f, 0).RotateVector(FVector(Dropped > 0 ? 40.0f : 0.0f, 0, 0));

			// auto-salvage (settings): low-rarity drops turn straight into pearls
			if ((int32)Rarity < AutoSalvage)
			{
				const int32 Value = TD::SalvageValue(Rarity);
				Pearls += Value;
				Income.Salvage += Value;
				FloatingText(Enemy->GetAimLocation() + Offset + FVector(0, 0, 60), FString::Printf(TEXT("+%d salvaged"), Value), TD::RarityColor(Rarity));
				++Dropped;
				DropChance -= 1.0f;
				continue;
			}

			const FTDItem Item = MakeDropItem(ItemLevel, Rarity, false);
			ATDPickup::Spawn(GetWorld(), Enemy->GetActorLocation() + Offset, Item);
			TDAudio::Play(this, ETDSound::ItemDrop);
			UE_LOG(LogAquaticTowerDefense, Log, TEXT("Drop from %s (wave %d): %s %s (item level %d)"), Def.Name, Enemy->GetWave(), TD::RarityName(Rarity), *Item.Name, Item.Level);
			++Dropped;
		}
		DropChance -= 1.0f;
	}

	// unique legendaries from bosses: a Kraken has an extra chance of one, the Abyssal Kraken always brings one (while any are left)
	if (Def.bBoss && TD::CountUniques(UniquesFound) < TD::UniqueCount && (Enemy->IsTitan() || FMath::FRand() < TD::UniqueBossChance))
	{
		const FTDItem Item = MakeDropItem(ItemLevel, ETDRarity::Legendary, true);
		ATDPickup::Spawn(GetWorld(), Enemy->GetActorLocation() + FVector(0, -50.0f, 0), Item);
		TDAudio::Play(this, ETDSound::ItemDrop);
	}

	// Leviathan's Jaw: the kill bursts over the creatures around it
	const FVector DeathAt = Enemy->GetActorLocation();
	const float BurstDamage = Enemy->GetMaxHealth() * TD::JawBurstShare;
	RemoveEnemy(Enemy, true);
	if (Killer && Killer->HasUnique(ETDUnique::LeviathanJaw))
	{
		ATDFx::SpawnSplash(GetWorld(), DeathAt + FVector(0, 0, 30), FLinearColor(0.9f, 0.3f, 0.35f), 1.6f);
		const TArray<TObjectPtr<ATDEnemy>> Around = Enemies;
		for (ATDEnemy* Other : Around)
		{
			if (Other && Other->IsAlive() && FVector::Dist2D(Other->GetActorLocation(), DeathAt) <= TD::JawBurstRadius * TD::TileSize)
			{
				Other->DealDamage(BurstDamage, Killer, TD::XpScaleArea);
			}
		}
	}
}

FTDItem ATDGameMode::MakeDropItem(int32 ItemLevel, ETDRarity Rarity, bool bForceUnique)
{
	if (Rarity == ETDRarity::Legendary && (bForceUnique || FMath::FRand() < TD::UniqueFromLegendaryChance))
	{
		FRandomStream Random(FMath::Rand());
		const ETDUnique Unique = TD::PickUnique(UniquesFound, Random);
		if (Unique != ETDUnique::None)
		{
			UniquesFound |= TD::UniqueBit(Unique);
			const FTDItem Item = TD::MakeItemOfKind(NextItemId++, TD::UniqueKind(Unique), ItemLevel);
			UE_LOG(LogAquaticTowerDefense, Log, TEXT("Unique dropped: %s (%d/%d found)"), *Item.Name, TD::CountUniques(UniquesFound), TD::UniqueCount);
			return Item;
		}
	}
	// (none left: an ordinary Legendary)
	return TD::GenerateItem(NextItemId++, ItemLevel, Rarity);
}

void ATDGameMode::DebugAddUniques()
{
	bRunCheated = true;
	for (int32 u = 1; u <= TD::UniqueCount; ++u)
	{
		if (GetStorageFree() > 0)
		{
			UniquesFound |= TD::UniqueBit((ETDUnique)u);
			AddToStorage(TD::MakeItemOfKind(NextItemId++, TD::UniqueKind((ETDUnique)u), FMath::Max(1, Wave)));
		}
	}
}

void ATDGameMode::OnEnemyLeaked(ATDEnemy* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	if (Enemy->IsTitan())
	{
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("TITAN leaked (wave %d): shield %.0f / %.0f  health %.0f / %.0f  alive %.1f s"),
			Enemy->GetWave(), Enemy->GetShield(), Enemy->GetMaxShield(), Enemy->GetHealth(), Enemy->GetMaxHealth(), Enemy->GetGameTimeSinceCreation());
	}

	// the Abyssal Kraken takes the whole reef with it
	// (Abyss 2: leaks cost double; Blood Tide: one more)
	const int32 Lost = Enemy->IsTitan() ? Lives : TD::Run().LeakLives(Enemy->GetDef().LivesLost);
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("LEAK wave %d: %s cost %d lives, health %.0f / %.0f"),
		Enemy->GetWave(), *Enemy->GetDisplayName(), Lost, Enemy->GetHealth(), Enemy->GetMaxHealth());
	Lives = FMath::Max(0, Lives - Lost);
	if (LeaksByType.IsValidIndex((int32)Enemy->GetEnemyType()))
	{
		++LeaksByType[(int32)Enemy->GetEnemyType()];
	}
	TDAudio::Play(this, ETDSound::Leak);
	if (ATDHUD* HUD = GetTDHUD())
	{
		HUD->AddFloatingText(TD::CellToWorld(TD::GoalCell(), 90.0f), FString::Printf(TEXT("-%d"), Lost), FLinearColor(1.0f, 0.3f, 0.3f));
		HUD->FlashLivesLost(Lost);
	}
	RemoveEnemy(Enemy);

	if (Lives <= 0 && !bGameOver)
	{
		bGameOver = true;
		TDAudio::SetBossMusic(false);
		FlushMastery();
		Toast(TEXT("The reef has fallen!"), FLinearColor(1.0f, 0.35f, 0.3f), ETDToast::Important);
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->SetPause(true);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Player actions
// ---------------------------------------------------------------------------------------------

ATDTower* ATDGameMode::TryPlaceTower(const FIntPoint& Cell, ETDTowerType Type, bool bQuiet, bool bOntoWall)
{
	if (bGameOver || !Grid || Type == ETDTowerType::Count)
	{
		return nullptr;
	}

	const int32 Cost = TD::TowerCost(Type);
	const FLinearColor Red(1.0f, 0.4f, 0.35f);

	if (Type == ETDTowerType::Wall && !TD::Run().CanBuildWalls())
	{
		Toast(TEXT("No Walls: build the maze from towers"), Red, ETDToast::Refusal);
		return nullptr;
	}

	// building onto a Coral Wall: the wall is sold and the tower takes its tile, so the path doesn't change
	if (bOntoWall && Grid->CanBuildOnWall(Cell, Type))
	{
		ATDTower* Wall = Grid->GetTowerAt(Cell);
		const int32 Refund = GetSellValue(Wall);
		const int32 NetCost = FMath::Max(0, Cost - Refund);
		if (Pearls < NetCost)
		{
			Toast(FString::Printf(TEXT("Need %d pearls"), NetCost), Red, ETDToast::Refusal);
			return nullptr;
		}
		ATDTower* Tower = Grid->ReplaceWall(Cell, Type);
		if (Tower)
		{
			// the same targeting the player last chose for this tower type, as on an empty tile
			if (LastTargeting.IsValidIndex((int32)Type))
			{
				Tower->SetTargeting((ETDTargeting)LastTargeting[(int32)Type]);
			}
			Pearls += Refund;
			Income.Sell += Refund;
			Pearls -= Cost;
			Tower->SetPurchaseInfo(Cost, BuildPhaseId, !IsWaveActive());
			Tower->AddSkillPoints(TD::Run().FreeSkillPoints());
			TDAudio::Play(this, ETDSound::Build);
		}
		return Tower;
	}

	const ETDPlaceResult Result = Grid->EvaluatePlacement(Cell);
	switch (Result)
	{
	case ETDPlaceResult::Ok:
		break;
	case ETDPlaceResult::OutOfBounds:
		return nullptr;
	case ETDPlaceResult::Occupied:
		if (!bQuiet)
		{
			Toast(TEXT("That tile is taken"), Red, ETDToast::Refusal);
		}
		return nullptr;
	case ETDPlaceResult::SpawnOrGoal:
		Toast(TEXT("Can't build on the trench or the reef"), Red, ETDToast::Refusal);
		return nullptr;
	case ETDPlaceResult::EnemyInWay:
		Toast(TEXT("A creature is in the way"), Red, ETDToast::Refusal);
		return nullptr;
	case ETDPlaceResult::BlocksPath:
		Toast(TEXT("That would block the path!"), Red, ETDToast::Refusal);
		return nullptr;
	case ETDPlaceResult::TrapsEnemy:
		Toast(TEXT("That would trap a creature!"), Red, ETDToast::Refusal);
		return nullptr;
	}

	if (Pearls < Cost)
	{
		Toast(FString::Printf(TEXT("Need %d pearls"), Cost), Red, ETDToast::Refusal);
		return nullptr;
	}

	ATDTower* Tower = Grid->PlaceTower(Cell, Type);
	if (Tower)
	{
		if (LastTargeting.IsValidIndex((int32)Type))
		{
			Tower->SetTargeting((ETDTargeting)LastTargeting[(int32)Type]);
		}
		Pearls -= Cost;
		Tower->SetPurchaseInfo(Cost, BuildPhaseId, !IsWaveActive());
		if (Tower->CanProgress())
		{
			Tower->AddSkillPoints(TD::Run().FreeSkillPoints());
		}
		TDAudio::Play(this, ETDSound::Build);
	}
	return Tower;
}

bool ATDGameMode::TryLearnSkill(ATDTower* Tower, int32 SkillIndex)
{
	if (bGameOver || !Tower)
	{
		return false;
	}

	const FLinearColor Red(1.0f, 0.4f, 0.35f);
	FString Reason;
	if (!Tower->CanLearnSkill(SkillIndex, &Reason))
	{
		Toast(Reason, Red, ETDToast::Refusal);
		return false;
	}

	const int32 NextRank = Tower->GetSkillRank(SkillIndex) + 1;
	const int32 Cost = TD::SkillCost(Tower->GetTowerType(), SkillIndex, NextRank);
	if (Pearls < Cost)
	{
		Toast(FString::Printf(TEXT("Need %d pearls"), Cost), Red, ETDToast::Refusal);
		return false;
	}

	Pearls -= Cost;
	Tower->LearnSkill(SkillIndex, Cost);
	UE_LOG(LogAquaticTowerDefense, Log, TEXT("Skill learned: %s"), *Tower->DescribeStats());
	TDAudio::Play(this, ETDSound::Upgrade);
	return true;
}

bool ATDGameMode::TryChooseForm(ATDTower* Tower, int32 Form)
{
	if (bGameOver || !Tower)
	{
		return false;
	}
	FString Reason;
	const FTDFormDef* Def = TD::Form(Tower->GetTowerType(), Form);
	if (!Def || !Tower->CanChooseForm(&Reason))
	{
		Toast(Def ? Reason : FString(TEXT("No such form")), FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		return false;
	}
	Tower->ChooseForm(Form);
	FloatingText(Tower->GetActorLocation() + FVector(0, 0, 170), FString(Def->Name).ToUpper(), FLinearColor(0.6f, 1.0f, 0.95f));
	ATDFx::SpawnRing(GetWorld(), Tower->GetActorLocation() + FVector(0, 0, 36), 90.0f, FLinearColor(0.6f, 1.0f, 0.95f));
	TDAudio::Play(this, ETDSound::Upgrade);
	UE_LOG(LogAquaticTowerDefense, Log, TEXT("Form chosen: %s -> %s"), Def->Name, *Tower->DescribeStats());
	return true;
}

int32 ATDGameMode::GetBuildOnWallCost(const FIntPoint& Cell, ETDTowerType Type) const
{
	if (!Grid || !Grid->CanBuildOnWall(Cell, Type))
	{
		return -1;
	}
	return FMath::Max(0, TD::TowerCost(Type) - GetSellValue(Grid->GetTowerAt(Cell)));
}

int32 ATDGameMode::GetSellValue(const ATDTower* Tower) const
{
	return Tower ? Tower->GetSellValue(BuildPhaseId, !IsWaveActive()) : 0;
}

bool ATDGameMode::CanSellTower(const ATDTower* Tower, FString* OutReason) const
{
	if (!Tower)
	{
		return false;
	}
	if (!TD::Run().CanSellLater() && !Tower->IsInRefundWindow(BuildPhaseId, !IsWaveActive()))
	{
		if (OutReason)
		{
			*OutReason = TEXT("Fixed Towers: only in the build phase it was bought in");
		}
		return false;
	}
	return true;
}

bool ATDGameMode::SellTower(ATDTower* Tower)
{
	if (bGameOver || !Tower || !Grid)
	{
		return false;
	}
	FString Reason;
	if (!CanSellTower(Tower, &Reason))
	{
		Toast(Reason, FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		return false;
	}

	// equipped items go back to storage
	const int32 ItemCount = Tower->GetEquippedCount();
	if (ItemCount > GetStorageFree())
	{
		Toast(TEXT("Storage full - free space for this tower's items first"), FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		return false;
	}
	for (int32 Slot = 0; Slot < (int32)ETDItemSlot::Count; ++Slot)
	{
		const FTDItem Item = Tower->Unequip((ETDItemSlot)Slot);
		if (Item.IsValid())
		{
			Storage.Add(Item);
		}
	}

	const int32 Value = GetSellValue(Tower);
	Pearls += Value;
	Income.Sell += Value;
	if (ATDHUD* HUD = GetTDHUD())
	{
		HUD->AddFloatingText(Tower->GetActorLocation() + FVector(0, 0, 80), FString::Printf(TEXT("+%d"), Value), FLinearColor(1.0f, 0.88f, 0.35f));
		TDAudio::Play(this, ETDSound::Sell);
	}
	Grid->RemoveTower(Tower);
	return true;
}

void ATDGameMode::CycleGameSpeed()
{
	GameSpeed = (GameSpeed % 3) + 1;
	UGameplayStatics::SetGlobalTimeDilation(this, (float)GameSpeed);
}

void ATDGameMode::RestartGame()
{
	ResetForRun(FTDRunRules(), false);
}

void ATDGameMode::ResetForRun(const FTDRunRules& Rules, bool bBoons)
{
	UWorld* World = GetWorld();
	FlushMastery();
	TD::SetRun(Rules);
	ApplyAbyssDepth();
	TDAudio::SetMusicPitch(TD::AbyssMusicPitch(TD::Run().Tier));
	TDAudio::SetBossMusic(false);
	bBoonsEnabled = bBoons;
	BoonOffer.Reset();
	RewardCarry = 0.0f;
	LastTargeting.Init((uint8)ETDTargeting::First, TD::TowerTypeCount);
	UniquesFound = 0;
	PendingConfirm = ETDConfirm::None;

	for (ATDEnemy* Enemy : Enemies)
	{
		if (Enemy)
		{
			Enemy->Destroy();
		}
	}
	Enemies.Reset();
	ActiveWaves.Reset();

	// creatures still playing their death animation
	for (TActorIterator<ATDEnemy> It(World); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<ATDProjectile> It(World); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<ATDFx> It(World); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<ATDPickup> It(World); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<ATDSpinePatch> It(World); It; ++It)
	{
		It->Destroy();
	}
	Storage.Reset();

	if (Grid)
	{
		Grid->ResetBoard();
	}

	Pearls = TD::StartPearls;
	Income = FTDIncome();
	Lives = TD::Run().StartLives();
	Wave = 0;
	BuildPhaseId = 0;
	GameSpeed = 1;
	bGameOver = false;
	bEndless = false;
	UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	ResetRunStats();

	Menu = ETDMenu::None;
	if (ATDPlayerController* PC = Cast<ATDPlayerController>(World->GetFirstPlayerController()))
	{
		PC->ClearSelection();
		PC->SetPause(false);
	}
}

void ATDGameMode::ResetRunStats()
{
	LeaksByType.Init(0, TD::EnemyTypeCount);
	PlayTime = 0.0f;
	WavesCleared = 0;
	bRunCheated = false;
	RecordAtStart = GetBestWave(TD::GetMap());
	bRecordToasted = false;
}

int32 ATDGameMode::GetTotalLeaks() const
{
	int32 Total = 0;
	for (const int32 Count : LeaksByType)
	{
		Total += Count;
	}
	return Total;
}

bool ATDGameMode::CountsForRecords() const
{
	if (bRunCheated)
	{
		return false;
	}
	const ATDPlayerController* PC = Cast<ATDPlayerController>(GetWorld()->GetFirstPlayerController());
	return !PC || !PC->IsRunningDevTool();
}

void ATDGameMode::UpdateRecords(bool bWon)
{
	if (!CountsForRecords())
	{
		return;
	}
	const int32 Map = TD::GetMap();
	if (Map < 0 || Map >= TD::MapCount)
	{
		return;
	}
	BestWave.SetNumZeroed(TD::MapCount);
	BestWinLives.SetNumZeroed(TD::MapCount);
	BestScore.SetNumZeroed(TD::MapCount);
	while (BestAbyss.Num() < TD::MapCount)
	{
		BestAbyss.Add(-1);
	}

	bool bChanged = false;
	const int32 Score = GetScore();
	if (Score > BestScore[Map])
	{
		BestScore[Map] = Score;
		bChanged = true;
	}
	// beating the final wave at an Abyss tier unlocks the next one on this map
	const int32 Tier = TD::Run().Tier;
	if (bWon && Tier > BestAbyss[Map])
	{
		BestAbyss[Map] = Tier;
		bChanged = true;
		if (Tier < TD::MaxAbyssTier)
		{
			Toast(FString::Printf(TEXT("Abyss %d unlocked on %s"), Tier + 1, TD::MapName(Map)), FLinearColor(0.75f, 0.6f, 1.0f), ETDToast::Important);
			TDAudio::Play(this, ETDSound::TierUnlock);
		}
	}
	if (WavesCleared > BestWave[Map])
	{
		BestWave[Map] = WavesCleared;
		bChanged = true;
		// announce once per game, and only when there was a record to beat
		if (RecordAtStart > 0 && !bRecordToasted)
		{
			bRecordToasted = true;
			Toast(FString::Printf(TEXT("New best on %s: past wave %d!"), TD::MapName(Map), RecordAtStart), FLinearColor(1.0f, 0.85f, 0.4f), ETDToast::Important);
		}
	}
	if (bWon && Lives > BestWinLives[Map])
	{
		BestWinLives[Map] = Lives;
		bChanged = true;
	}
	if (bChanged || bMasteryDirty)
	{
		SaveSettings();
	}
}

void ATDGameMode::SetupDemo(int32 Waves)
{
	if (!Grid || bGameOver)
	{
		return;
	}
	bRunCheated = true;

	CloseMenu();
	Pearls += 3000;

	// serpentine walls, alternating gaps top and bottom
	const int32 WallCols[] = { 3, 7, 11, 15 };
	for (int32 i = 0; i < UE_ARRAY_COUNT(WallCols); ++i)
	{
		const bool bGapBottom = (i % 2 == 0);
		for (int32 Row = bGapBottom ? 0 : 2; Row <= (bGapBottom ? 10 : 12); ++Row)
		{
			TryPlaceTower(FIntPoint(WallCols[i], Row), ETDTowerType::Wall, true);
		}
	}

	ATDTower* Harpoon = TryPlaceTower(FIntPoint(5, 8), ETDTowerType::Harpoon, true);
	TryPlaceTower(FIntPoint(2, 9), ETDTowerType::Harpoon, true);
	ATDTower* Jelly = TryPlaceTower(FIntPoint(9, 4), ETDTowerType::Jelly, true);
	ATDTower* Puffer = TryPlaceTower(FIntPoint(5, 3), ETDTowerType::Puffer, true);
	ATDTower* Eel = TryPlaceTower(FIntPoint(13, 8), ETDTowerType::Eel, true);
	TryPlaceTower(FIntPoint(16, 9), ETDTowerType::Harpoon, true);
	ATDTower* Mantis = TryPlaceTower(FIntPoint(2, 3), ETDTowerType::Mantis, true);
	ATDTower* Octopus = TryPlaceTower(FIntPoint(9, 8), ETDTowerType::Octopus, true);
	ATDTower* Lionfish = TryPlaceTower(FIntPoint(6, 6), ETDTowerType::Lionfish, true);
	ATDTower* Sawfish = TryPlaceTower(FIntPoint(10, 3), ETDTowerType::Sawfish, true);
	TryPlaceTower(FIntPoint(6, 8), ETDTowerType::Humpback, true);
	TryPlaceTower(FIntPoint(4, 6), ETDTowerType::AcidVent, true);
	TryPlaceTower(FIntPoint(14, 5), ETDTowerType::AcidVent, true);
	TryPlaceTower(FIntPoint(12, 7), ETDTowerType::Lionfish, true);
	TryPlaceTower(FIntPoint(16, 4), ETDTowerType::Sawfish, true);
	if (Octopus)
	{
		TryLearnSkill(Octopus, 1);
	}
	if (Lionfish)
	{
		TryLearnSkill(Lionfish, 2);
	}
	if (Sawfish)
	{
		TryLearnSkill(Sawfish, 0);
	}
	if (Mantis)
	{
		TryLearnSkill(Mantis, 1);
	}

	// grant one item of each rarity plus a couple of extras, and equip some
	for (int32 r = 0; r < 4; ++r)
	{
		AddToStorage(CreateItem((ETDRarity)r));
	}
	AddToStorage(CreateItem(ETDRarity::Rare));
	AddToStorage(CreateItem(ETDRarity::Epic));

	if (Harpoon)
	{
		const FString Before = Harpoon->DescribeStats();
		EquipFromStorage(Harpoon, Storage.Num() - 1);
		EquipFromStorage(Harpoon, 3);
		UE_LOG(LogAquaticTowerDefense, Log, TEXT("Demo equip before: %s"), *Before);
		UE_LOG(LogAquaticTowerDefense, Log, TEXT("Demo equip after:  %s"), *Harpoon->DescribeStats());
		TryLearnSkill(Harpoon, 0);
	}
	if (Jelly)
	{
		TryLearnSkill(Jelly, 1);
	}
	if (Puffer)
	{
		TryLearnSkill(Puffer, 1);
	}
	if (Eel)
	{
		TryLearnSkill(Eel, 0);
		EquipFromStorage(Eel, 0);
	}

	// round-trip the board through a save game to exercise saving and loading
	if (SaveGame(true))
	{
		LoadGame();
	}

	// one item on the seabed to show the pickup flow
	ATDPickup::Spawn(GetWorld(), TD::CellToWorld(FIntPoint(1, 6)), CreateItem(TD::RollRarity(1)));

	for (int32 i = 0; i < Waves; ++i)
	{
		ReleaseNextWave();
	}
}

// ---------------------------------------------------------------------------------------------
// Auras, trickle and splitting
// ---------------------------------------------------------------------------------------------

void ATDGameMode::UpdateAuras()
{
	if (!Grid)
	{
		return;
	}

	const TArray<TObjectPtr<ATDTower>>& Towers = Grid->GetTowers();
	for (ATDTower* Tower : Towers)
	{
		if (Tower)
		{
			Tower->ResetAuras();
		}
	}
	for (ATDEnemy* Enemy : Enemies)
	{
		if (Enemy)
		{
			Enemy->ResetAuras();
		}
	}

	// tower -> tower and tower -> enemy auras
	for (ATDTower* Source : Towers)
	{
		if (!Source || !Source->IsSupportTower())
		{
			continue;
		}
		const FTDTowerStats& S = Source->GetStats();
		const float RangeUU = S.Range * TD::TileSize;
		const FVector At = Source->GetActorLocation();

		if (Source->GetTowerType() == ETDTowerType::Humpback)
		{
			for (ATDTower* Other : Towers)
			{
				if (Other && Other != Source && Other->CanProgress() && FVector::Dist2D(Other->GetActorLocation(), At) <= RangeUU)
				{
					// Sonar Whale range only reaches attack towers (auras don't grow each other)
					Other->ApplyTowerAura(S.AuraRate, S.AuraDamage, S.AuraXpTrickle, Other->IsAttackTower() ? S.AuraRange : 0.0f);
				}
			}
		}
		else if (Source->GetTowerType() == ETDTowerType::AcidVent)
		{
			for (ATDEnemy* Enemy : Enemies)
			{
				if (Enemy && Enemy->IsAlive() && FVector::Dist2D(Enemy->GetActorLocation(), At) <= RangeUU)
				{
					Enemy->ApplyArmorAura(S.AuraArmor);
					if (S.AuraSlow < 1.0f)
					{
						Enemy->ApplySlowAura(S.AuraSlow);
					}
				}
			}
		}
	}

	// Kraken's Heart (unique charm): the 8 tiles around it
	for (ATDTower* Source : Towers)
	{
		if (!Source || !Source->HasUnique(ETDUnique::KrakensHeart))
		{
			continue;
		}
		for (ATDTower* Other : Towers)
		{
			const FIntPoint D = Other ? Other->GetCell() - Source->GetCell() : FIntPoint::ZeroValue;
			if (Other && Other != Source && Other->IsAttackTower() && FMath::Abs(D.X) <= 1 && FMath::Abs(D.Y) <= 1)
			{
				Other->ApplyNeighbourBonus(TD::KrakenHeartBonus);
			}
		}
	}

	// enemy -> enemy and enemy -> tower auras
	for (ATDEnemy* Source : Enemies)
	{
		if (!Source || !Source->IsAlive())
		{
			continue;
		}
		const FVector At = Source->GetActorLocation();
		const int32 Level = Source->GetWave();

		if (Source->GetEnemyType() == ETDEnemyType::Cuttlefish)
		{
			const float Armor = FMath::Min(30.0f, 10.0f + 1.0f * (Level - 1));
			for (ATDEnemy* Other : Enemies)
			{
				if (Other && Other != Source && Other->IsAlive() && FVector::Dist2D(Other->GetActorLocation(), At) <= CuttlefishAuraRange * TD::TileSize)
				{
					Other->ApplyArmorAura(Armor);
				}
			}
		}
		else if (Source->GetEnemyType() == ETDEnemyType::ShockEel)
		{
			for (ATDTower* Tower : Towers)
			{
				if (Tower && Tower->IsAttackTower() && FVector::Dist2D(Tower->GetActorLocation(), At) <= ShockEelAuraRange * TD::TileSize)
				{
					Tower->ApplyDamageDebuff(-0.10f);
				}
			}
		}
	}
}

void ATDGameMode::SpawnSplit(ATDEnemy* Parent)
{
	if (!Parent)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATDEnemy* Copy = GetWorld()->SpawnActor<ATDEnemy>(ATDEnemy::StaticClass(), Parent->GetActorLocation(), Parent->GetActorRotation(), Params);
	if (!Copy)
	{
		return;
	}
	Copy->InitAsSplit(Parent);
	Enemies.Add(Copy);
	for (FWaveRun& Run : ActiveWaves)
	{
		if (Run.Number == Parent->GetWave())
		{
			++Run.Alive;
			break;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Wave info
// ---------------------------------------------------------------------------------------------

void ATDGameMode::GetWaveComposition(int32 N, TArray<TPair<ETDEnemyType, int32>>& Out)
{
	Out.Reset();
	TArray<FPendingSpawn> Spawns;
	BuildWave(N, Spawns);
	// one entry per creature type, in order of first appearance (mixed waves interleave their schools)
	for (const FPendingSpawn& Spawn : Spawns)
	{
		TPair<ETDEnemyType, int32>* Entry = Out.FindByPredicate([&Spawn](const TPair<ETDEnemyType, int32>& E) { return E.Key == Spawn.Type; });
		if (Entry)
		{
			++Entry->Value;
		}
		else
		{
			Out.Add(TPair<ETDEnemyType, int32>(Spawn.Type, 1));
		}
	}
}

float ATDGameMode::GetCurrentWaveProgress() const
{
	if (ActiveWaves.Num() == 0)
	{
		return 0.0f;
	}
	const FWaveRun& Run = ActiveWaves.Last();
	return Run.Spawns.Num() > 0 ? (float)Run.NextSpawn / Run.Spawns.Num() : 1.0f;
}

int32 ATDGameMode::GetCurrentWaveNumber() const
{
	return ActiveWaves.Num() > 0 ? ActiveWaves.Last().Number : Wave;
}

// ---------------------------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------------------------

void ATDGameMode::OpenMenu(ETDMenu NewMenu)
{
	Menu = NewMenu;

	// let the camera settle on the board for a moment before pausing behind the start menu
	if (GetWorld()->GetTimeSeconds() < 0.3f && Menu != ETDMenu::None)
	{
		return;
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetPause(Menu != ETDMenu::None || bGameOver);
	}
}

void ATDGameMode::CloseMenu()
{
	OpenMenu(ETDMenu::None);
}

void ATDGameMode::OpenSettings()
{
	SettingsReturn = Menu;
	OpenMenu(ETDMenu::Settings);
}

void ATDGameMode::CloseSettings()
{
	SaveSettings();
	OpenMenu(SettingsReturn == ETDMenu::Settings ? ETDMenu::None : SettingsReturn);
}

void ATDGameMode::StartNewGame()
{
	TD::SetMap(PickMap());
	RestartGame();
	RunId = NewRunId();
	CloseMenu();
}

void ATDGameMode::OpenNewGameSetup()
{
	if (Menu != ETDMenu::Setup)
	{
		SetupReturn = Menu;
	}
	// the last choices stay, limited to what the chosen map has unlocked; the slot is the first empty one, else the run's own
	SetSetupMap(Setup.Map);
	RefreshSlotInfo();
	Setup.Slot = CurrentSlot;
	for (int32 Slot = SaveSlotCount - 1; Slot >= 0; --Slot)
	{
		if (!SlotInfos[Slot].bUsed)
		{
			Setup.Slot = Slot;
		}
	}
	OpenMenu(ETDMenu::Setup);
}

void ATDGameMode::CloseNewGameSetup()
{
	OpenMenu(SetupReturn == ETDMenu::Setup ? ETDMenu::Main : SetupReturn);
}

void ATDGameMode::SetSetupMap(int32 Map)
{
	Setup.Map = (Map >= 0 && Map < TD::MapCount) ? Map : -1;
	SetSetupTier(Setup.Tier);
}

void ATDGameMode::SetSetupTier(int32 Tier)
{
	// a random map plays at tier 0: its tier can't depend on a map that isn't picked yet
	const int32 MaxTier = Setup.Map >= 0 ? GetUnlockedTier(Setup.Map) : 0;
	Setup.Tier = FMath::Clamp(Tier, 0, MaxTier);
	Setup.Mutators = TD::CleanMutators(Setup.Mutators, Setup.Tier);
}

void ATDGameMode::ToggleSetupMutator(int32 Mutator)
{
	if (Mutator < 0 || Mutator >= TD::MutatorCount || TD::IsMutatorIncluded((ETDMutator)Mutator, Setup.Tier))
	{
		return;
	}
	Setup.Mutators ^= 1u << (uint32)Mutator;
}

void ATDGameMode::StartSetupGame()
{
	SetSetupMap(Setup.Map);
	TD::SetMap(Setup.Map >= 0 ? Setup.Map : PickMap());
	FTDRunRules Rules;
	Rules.Tier = Setup.Tier;
	Rules.Mutators = Setup.Mutators;
	ResetForRun(Rules, true);
	CurrentSlot = Setup.Slot;
	RunId = NewRunId();
	CloseMenu();
	OfferBoons();
}

void ATDGameMode::RequestStartSetupGame()
{
	RefreshSlotInfo();
	if (SlotInfos[Setup.Slot].bUsed)
	{
		PendingConfirm = ETDConfirm::OverwriteForNewGame;
		ConfirmSlot = Setup.Slot;
		return;
	}
	StartSetupGame();
}

void ATDGameMode::RequestRetry()
{
	RefreshSlotInfo();
	// the slot normally holds this very run (its autosaves), which Retry may replace without asking
	const FTDSlotInfo& Info = SlotInfos[CurrentSlot];
	if (Info.bUsed && Info.RunId != RunId)
	{
		PendingConfirm = ETDConfirm::OverwriteForRetry;
		ConfirmSlot = CurrentSlot;
		return;
	}
	RetryRun();
}

void ATDGameMode::RetryRun()
{
	// the map actually played (a Random pick stays the same map), the same tier and mutators, fresh boons; the same run id
	const FTDRunRules Rules = TD::RetryRules(TD::Run());
	ResetForRun(Rules, true);
	CloseMenu();
	OfferBoons();
}

void ATDGameMode::FillSlotInfo(const UTDSaveGame* Save, FTDSlotInfo& Out)
{
	Out = FTDSlotInfo();
	if (!Save)
	{
		return;
	}
	Out.bUsed = true;
	Out.Map = Save->MapIndex;
	Out.Tier = Save->AbyssTier;
	Out.MutatorCount = FMath::CountBits64((uint64)(uint32)Save->Mutators);
	Out.Wave = Save->Wave;
	Out.bEndless = Save->bEndless;
	Out.PlayTime = Save->PlayTime;
	Out.SavedAt = Save->SavedAt;
	Out.RunId = Save->RunId;
	const int32 Cleared = FMath::Max(Save->WavesCleared, Save->Wave);
	Out.Score = TD::RunScore(Cleared, Save->Lives, Cleared >= TD::FinalWave, TD::ScoreMultiplier(Save->AbyssTier, (uint32)Save->Mutators, (uint32)Save->Boons));
}

void ATDGameMode::RefreshSlotInfo()
{
	for (int32 Slot = 0; Slot < SaveSlotCount; ++Slot)
	{
		const FString Name = SlotName(Slot);
		FillSlotInfo(UGameplayStatics::DoesSaveGameExist(Name, 0) ? Cast<UTDSaveGame>(UGameplayStatics::LoadGameFromSlot(Name, 0)) : nullptr, SlotInfos[Slot]);
	}
}

bool ATDGameMode::HasAnySave() const
{
	for (const FTDSlotInfo& Info : SlotInfos)
	{
		if (Info.bUsed)
		{
			return true;
		}
	}
	return false;
}

void ATDGameMode::OpenSlotPicker()
{
	RefreshSlotInfo();
	if (Menu != ETDMenu::Slots)
	{
		SlotReturn = Menu;
	}
	OpenMenu(ETDMenu::Slots);
}

void ATDGameMode::CloseSlotPicker()
{
	PendingConfirm = ETDConfirm::None;
	OpenMenu(SlotReturn == ETDMenu::Slots ? ETDMenu::Main : SlotReturn);
}

void ATDGameMode::RequestDeleteSlot(int32 Slot)
{
	if (Slot >= 0 && Slot < SaveSlotCount && SlotInfos[Slot].bUsed)
	{
		PendingConfirm = ETDConfirm::DeleteSlot;
		ConfirmSlot = Slot;
	}
}

FString ATDGameMode::GetConfirmText() const
{
	const FTDSlotInfo& Info = SlotInfos[FMath::Clamp(ConfirmSlot, 0, SaveSlotCount - 1)];
	switch (PendingConfirm)
	{
	case ETDConfirm::OverwriteForNewGame:
	case ETDConfirm::OverwriteForRetry:
		return FString::Printf(TEXT("Slot %d holds a run on %s at wave %d. This replaces your run at wave %d once the new run saves."), ConfirmSlot + 1, TD::MapName(Info.Map), Info.Wave, Info.Wave);
	case ETDConfirm::DeleteSlot:
		return FString::Printf(TEXT("Delete the run in slot %d (%s, wave %d)? This can't be undone."), ConfirmSlot + 1, TD::MapName(Info.Map), Info.Wave);
	default:
		return FString();
	}
}

void ATDGameMode::AnswerConfirm(bool bYes)
{
	const ETDConfirm Question = PendingConfirm;
	PendingConfirm = ETDConfirm::None;
	if (!bYes)
	{
		return;
	}
	switch (Question)
	{
	case ETDConfirm::OverwriteForNewGame:
		StartSetupGame();
		break;
	case ETDConfirm::OverwriteForRetry:
		RetryRun();
		break;
	case ETDConfirm::DeleteSlot:
		UGameplayStatics::DeleteGameInSlot(SlotName(ConfirmSlot), 0);
		RefreshSlotInfo();
		break;
	default:
		break;
	}
}

void ATDGameMode::SetTowerTargeting(ATDTower* Tower, ETDTargeting Mode)
{
	if (!Tower || !Tower->CanChooseTargeting())
	{
		return;
	}
	Tower->SetTargeting(Mode);
	if (LastTargeting.IsValidIndex((int32)Tower->GetTowerType()))
	{
		LastTargeting[(int32)Tower->GetTowerType()] = (uint8)Mode;
	}
}

int32 ATDGameMode::GetScore() const
{
	return TD::RunScore(WavesCleared, Lives, WavesCleared >= TD::FinalWave, GetScoreMultiplier());
}

void ATDGameMode::OfferBoons()
{
	FRandomStream Random(FMath::Rand());
	TD::RollBoonOffer(TD::Run(), Random, BoonOffer, TD::BoonOfferSize, Lives);
	if (BoonOffer.Num() > 0)
	{
		OpenMenu(ETDMenu::Boon);
	}
}

bool ATDGameMode::PickBoon(int32 Boon)
{
	if (Boon < 0 || Boon >= TD::BoonCount || !BoonOffer.Contains(Boon))
	{
		return false;
	}
	FTDRunRules Rules = TD::Run();
	Rules.Boons |= 1u << (uint32)Boon;
	TD::SetRun(Rules);
	BoonOffer.Reset();

	const ETDBoon Picked = (ETDBoon)Boon;
	if (Picked == ETDBoon::ThickReef)
	{
		Lives += TD::ThickReefLives;
	}
	if (Picked == ETDBoon::KrakensPact)
	{
		Lives = FMath::Max(1, Lives - TD::PactLives);
		if (ATDHUD* HUD = GetTDHUD())
		{
			HUD->FlashLivesLost(TD::PactLives);
		}
	}
	if (Picked == ETDBoon::SirensDebt && Grid)
	{
		for (ATDTower* Tower : Grid->GetTowers())
		{
			if (Tower && Tower->CanProgress())
			{
				Tower->AddSkillPoints(TD::Run().FreeSkillPoints());
			}
		}
	}
	RefreshTowerStats();
	// Kraken's Pact: a second pick of a normal boon follows at once
	if (Picked == ETDBoon::KrakensPact)
	{
		FRandomStream Random(FMath::Rand());
		TD::RollBoonOffer(TD::Run(), Random, BoonOffer, TD::BoonOfferSize, Lives, false);
	}
	if (HasBoonOffer())
	{
		OpenMenu(ETDMenu::Boon);
	}
	else
	{
		CloseMenu();
	}

	// keep the pick between waves (a new game's first pick waits for the wave 1 autosave, so the last save stays until then)
	if (Wave > 0 && CanSaveGame())
	{
		SaveGame(true);
	}
	return true;
}

void ATDGameMode::DebugOfferBoons(bool bForceCursed)
{
	bRunCheated = true;
	BoonOffer.Reset();
	OfferBoons();
	if (bForceCursed && HasBoonOffer() && !BoonOffer.ContainsByPredicate([](int32 Boon) { return TD::IsCursed((ETDBoon)Boon); }))
	{
		for (int32 b = 0; b < TD::BoonCount; ++b)
		{
			if (TD::IsCursed((ETDBoon)b) && TD::BoonFits((ETDBoon)b, TD::Run(), Lives))
			{
				BoonOffer.Last() = b;
				break;
			}
		}
	}
	if (!HasBoonOffer())
	{
		Toast(TEXT("Every boon is taken"), FLinearColor(1.0f, 0.6f, 0.4f), ETDToast::Refusal);
	}
}

void ATDGameMode::DebugSetLastOffer(int32 Boon)
{
	if (BoonOffer.Num() > 0 && Boon >= 0 && Boon < TD::BoonCount && !TD::Run().Has((ETDBoon)Boon))
	{
		BoonOffer.Remove(Boon);
		BoonOffer.Add(Boon);
	}
}

void ATDGameMode::RefreshMasteryTrims(ETDTowerType Type)
{
	if (!Grid)
	{
		return;
	}
	for (ATDTower* Tower : Grid->GetTowers())
	{
		if (Tower && Tower->GetTowerType() == Type)
		{
			Tower->UpdateMasteryTrim();
		}
	}
}

void ATDGameMode::FlushMastery()
{
	if (bMasteryDirty)
	{
		SaveSettings();
	}
}

void ATDGameMode::DebugSetMasteryKills(ETDTowerType Type, int32 Kills)
{
	TArray<int32>& All = TD::MutableLifetimeKills();
	if (All.IsValidIndex((int32)Type))
	{
		All[(int32)Type] = FMath::Max(0, Kills);
		RefreshMasteryTrims(Type);
		SaveSettings();
	}
}

void ATDGameMode::DebugGameOver()
{
	bRunCheated = true;
	CloseMenu();
	Lives = 0;
	bGameOver = true;
	TDAudio::SetBossMusic(false);
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetPause(true);
	}
}

void ATDGameMode::DebugStartAbyss(int32 Tier)
{
	TD::SetMap(PickMap());
	FTDRunRules Rules;
	Rules.Tier = Tier;
	ResetForRun(Rules, false);
	bRunCheated = true;
	CloseMenu();
}

void ATDGameMode::CaptureDepthBase()
{
	UWorld* World = GetWorld();
	DepthLights.Reset();
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (ULightComponent* Light = It->GetLightComponent())
		{
			DepthLights.Add({ Light, Light->Intensity });
		}
	}
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		if (USkyLightComponent* Light = It->GetLightComponent())
		{
			DepthSky = Light;
			DepthSkyIntensity = Light->Intensity;
			break;
		}
	}
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		if (UExponentialHeightFogComponent* Fog = It->GetComponent())
		{
			DepthFog = Fog;
			DepthFogDensity = Fog->FogDensity;
			DepthFogColor = Fog->FogInscatteringLuminance;
			break;
		}
	}
	// the strongest unbound post process volume carries the look
	APostProcessVolume* Best = nullptr;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (It->bUnbound && (!Best || It->Priority > Best->Priority))
		{
			Best = *It;
		}
	}
	DepthVolume = Best;
	if (Best)
	{
		bDepthTintOverride = Best->Settings.bOverride_SceneColorTint != 0;
		DepthTint = bDepthTintOverride ? Best->Settings.SceneColorTint : FLinearColor::White;
		bDepthExposureOverride = Best->Settings.bOverride_AutoExposureBias != 0;
		DepthExposure = bDepthExposureOverride ? Best->Settings.AutoExposureBias : 0.0f;
		bDepthVignetteOverride = Best->Settings.bOverride_VignetteIntensity != 0;
		DepthVignette = bDepthVignetteOverride ? Best->Settings.VignetteIntensity : 0.4f;
	}
}

void ATDGameMode::ApplyAbyssDepth()
{
	// D = 0 puts every value back exactly as captured; Abyss 5 is noticeably deeper but the board stays readable
	const float D = TD::AbyssDepth(TD::Run().Tier);
	for (const FDepthLight& Entry : DepthLights)
	{
		if (ULightComponent* Light = Entry.Light.Get())
		{
			Light->SetIntensity(Entry.Intensity * (1.0f - 0.22f * D));
		}
	}
	if (USkyLightComponent* Sky = DepthSky.Get())
	{
		Sky->SetIntensity(DepthSkyIntensity * (1.0f - 0.25f * D));
	}
	if (UExponentialHeightFogComponent* Fog = DepthFog.Get())
	{
		Fog->SetFogDensity(DepthFogDensity * (1.0f + 0.7f * D));
		Fog->SetFogInscatteringColor(DepthFogColor * FLinearColor(1.0f - 0.45f * D, 1.0f - 0.4f * D, 1.0f - 0.2f * D, 1.0f));
	}
	if (APostProcessVolume* Volume = DepthVolume.Get())
	{
		FPostProcessSettings& Settings = Volume->Settings;
		Settings.bOverride_SceneColorTint = bDepthTintOverride || D > 0.0f;
		Settings.SceneColorTint = DepthTint * FLinearColor(1.0f - 0.14f * D, 1.0f - 0.1f * D, 1.0f, 1.0f);
		Settings.bOverride_AutoExposureBias = bDepthExposureOverride || D > 0.0f;
		Settings.AutoExposureBias = DepthExposure - 0.35f * D;
		Settings.bOverride_VignetteIntensity = bDepthVignetteOverride || D > 0.0f;
		Settings.VignetteIntensity = DepthVignette + 0.15f * D;
	}
}

void ATDGameMode::RefreshTowerStats()
{
	if (!Grid)
	{
		return;
	}
	for (ATDTower* Tower : Grid->GetTowers())
	{
		if (Tower)
		{
			Tower->RefreshStats();
		}
	}
}

void ATDGameMode::ReturnToMainMenu()
{
	RestartGame();
	OpenMenu(ETDMenu::Main);
}

void ATDGameMode::ContinueEndless()
{
	if (bGameOver || bEndless || Wave < TD::FinalWave)
	{
		return;
	}
	bEndless = true;
	CloseMenu();
	Toast(TEXT("Endless mode: every wave is much stronger and faster than the last"), FLinearColor(1.0f, 0.6f, 0.4f), ETDToast::Important);
	SaveGame(true);
}

void ATDGameMode::QuitGame()
{
	FlushMastery();
	UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
}

// ---------------------------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------------------------

FString TDBind::ActionName(ETDBind Bind)
{
	if (Bind >= ETDBind::TowerFirst && Bind <= ETDBind::TowerLast)
	{
		return FString::Printf(TEXT("Build %s"), TD::Tower((ETDTowerType)((int32)Bind - (int32)ETDBind::TowerFirst)).Name);
	}
	switch (Bind)
	{
	case ETDBind::Wave:		return TEXT("Release / send wave");
	case ETDBind::Skills:	return TEXT("Skills tab");
	case ETDBind::Storage:	return TEXT("Storage");
	case ETDBind::Sell:		return TEXT("Sell tower");
	case ETDBind::Speed:	return TEXT("Game speed");
	case ETDBind::Pause:	return TEXT("Pause menu");
	case ETDBind::Items:	return TEXT("Items tab");
	case ETDBind::Targeting:	return TEXT("Tower targeting");
	case ETDBind::Info:		return TEXT("Ranges and levels (hold)");
	default:				return TEXT("");
	}
}

FKey TDBind::DefaultKey(ETDBind Bind)
{
	if (Bind >= ETDBind::TowerFirst && Bind <= ETDBind::TowerLast)
	{
		static const FKey TowerKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero, EKeys::Hyphen };
		static_assert(UE_ARRAY_COUNT(TowerKeys) == (int32)ETDTowerType::Count, "one default key per tower");
		return TowerKeys[(int32)Bind - (int32)ETDBind::TowerFirst];
	}
	switch (Bind)
	{
	case ETDBind::Wave:		return EKeys::SpaceBar;
	case ETDBind::Skills:	return EKeys::E;
	case ETDBind::Storage:	return EKeys::I;
	case ETDBind::Sell:		return EKeys::X;
	case ETDBind::Speed:	return EKeys::F;
	case ETDBind::Pause:	return EKeys::P;
	case ETDBind::Items:	return EKeys::Q;
	case ETDBind::Targeting:	return EKeys::T;
	case ETDBind::Info:		return EKeys::LeftAlt;
	default:				return EKeys::Invalid;
	}
}

FString TDBind::KeyName(const FKey& Key)
{
	if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt)
	{
		return TEXT("Alt");
	}
	// punctuation as the symbol on the key, so it fits a small key cap
	static const TPair<FKey, const TCHAR*> Symbols[] =
	{
		{ EKeys::Hyphen, TEXT("-") }, { EKeys::Equals, TEXT("=") }, { EKeys::Comma, TEXT(",") }, { EKeys::Period, TEXT(".") },
		{ EKeys::Slash, TEXT("/") }, { EKeys::Backslash, TEXT("\\") }, { EKeys::Semicolon, TEXT(";") }, { EKeys::Apostrophe, TEXT("'") },
		{ EKeys::LeftBracket, TEXT("[") }, { EKeys::RightBracket, TEXT("]") }, { EKeys::Tilde, TEXT("`") },
	};
	for (const TPair<FKey, const TCHAR*>& Symbol : Symbols)
	{
		if (Symbol.Key == Key)
		{
			return Symbol.Value;
		}
	}
	return Key.GetDisplayName().ToString();
}

const TArray<FKey>& TDBind::BindableKeys()
{
	static const TArray<FKey> Keys =
	{
		EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero, EKeys::Hyphen,
		EKeys::LeftAlt,	// (Right Alt is captured as Left Alt, and either one holds an Alt binding)
		EKeys::B, EKeys::C, EKeys::E, EKeys::F, EKeys::G, EKeys::H, EKeys::I, EKeys::J, EKeys::K, EKeys::L, EKeys::M, EKeys::N,
		EKeys::O, EKeys::P, EKeys::Q, EKeys::R, EKeys::T, EKeys::U, EKeys::V, EKeys::X, EKeys::Y, EKeys::Z,
		EKeys::SpaceBar, EKeys::Tab, EKeys::Enter, EKeys::BackSpace, EKeys::LeftShift, EKeys::RightShift, EKeys::LeftControl, EKeys::RightControl,
		EKeys::Tilde, EKeys::Comma, EKeys::Period, EKeys::Slash, EKeys::Semicolon, EKeys::Apostrophe, EKeys::LeftBracket, EKeys::RightBracket,
		EKeys::Backslash, EKeys::Equals, EKeys::Insert, EKeys::Delete, EKeys::End, EKeys::PageUp, EKeys::PageDown,
		EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5, EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12
	};
	return Keys;
}

void ATDGameMode::SetKey(ETDBind Bind, const FKey& Key)
{
	const int32 Index = (int32)Bind;
	if (!KeyBindings.IsValidIndex(Index))
	{
		return;
	}
	const FKey Old = KeyBindings[Index];
	for (int32 i = 0; i < KeyBindings.Num(); ++i)
	{
		if (i != Index && KeyBindings[i] == Key)
		{
			KeyBindings[i] = Old;
		}
	}
	KeyBindings[Index] = Key;
	++BindingsVersion;
	SaveSettings();
}

void ATDGameMode::ResetKeys()
{
	for (int32 i = 0; i < (int32)ETDBind::Count; ++i)
	{
		KeyBindings[i] = TDBind::DefaultKey((ETDBind)i);
	}
	++BindingsVersion;
	SaveSettings();
}

void ATDGameMode::SetSfxVolume(float Percent)
{
	SfxVolume = FMath::Clamp(FMath::RoundToFloat(Percent / 5.0f) * 5.0f, 0.0f, 100.0f);
	TDAudio::SetVolumes(SfxVolume / 100.0f, MusicVolume / 100.0f);
}

void ATDGameMode::SetMusicVolume(float Percent)
{
	MusicVolume = FMath::Clamp(FMath::RoundToFloat(Percent / 5.0f) * 5.0f, 0.0f, 100.0f);
	TDAudio::SetVolumes(SfxVolume / 100.0f, MusicVolume / 100.0f);
	TDAudio::UpdateMusic(this);
}

int32 ATDGameMode::PickMap() const
{
	if (ForcedMap >= 0)
	{
		return FMath::Clamp(ForcedMap, 0, TD::MapCount - 1);
	}
	const int32 Offset = FMath::RandRange(1, TD::MapCount - 1);
	return (TD::GetMap() + Offset) % TD::MapCount;
}

void ATDGameMode::SetSharpAA(bool bSharp)
{
	bSharpAA = bSharp;
	ApplyAntiAliasing();
	SaveSettings();
}

void ATDGameMode::ApplyAntiAliasing() const
{
	if (IConsoleVariable* AA = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
	{
		AA->Set(bSharpAA ? 4 : 1, ECVF_SetByGameSetting);
	}
}

void ATDGameMode::LoadSettings()
{
	KeyBindings.SetNum((int32)ETDBind::Count);
	for (int32 i = 0; i < (int32)ETDBind::Count; ++i)
	{
		KeyBindings[i] = TDBind::DefaultKey((ETDBind)i);
	}

	if (UTDSettingsSave* Loaded = Cast<UTDSettingsSave>(UGameplayStatics::LoadGameFromSlot(SettingsSlot, 0)))
	{
		bSharpAA = Loaded->bSharpAA;
		for (int32 i = 0; i < Loaded->KeyBindings.Num() && i < KeyBindings.Num(); ++i)
		{
			const FKey Key(*Loaded->KeyBindings[i]);
			if (Key.IsValid() && TDBind::BindableKeys().Contains(Key))
			{
				KeyBindings[i] = Key;
			}
		}
		// the defaults moved (Skills U -> E, the new Items tab E -> Q): a save still on the old ones (Skills on U, and Items
		// either not saved yet or on E) moves to the new ones, as long as nothing else holds E or Q
		{
			const int32 SkillsIndex = (int32)ETDBind::Skills;
			const int32 ItemsIndex = (int32)ETDBind::Items;
			const bool bItemsOld = Loaded->KeyBindings.Num() <= ItemsIndex || KeyBindings[ItemsIndex] == EKeys::E;
			auto HeldByOther = [this, SkillsIndex, ItemsIndex](const FKey& Key)
			{
				for (int32 j = 0; j < KeyBindings.Num(); ++j)
				{
					if (j != SkillsIndex && j != ItemsIndex && KeyBindings[j] == Key)
					{
						return true;
					}
				}
				return false;
			};
			if (KeyBindings[SkillsIndex] == EKeys::U && bItemsOld && !HeldByOther(EKeys::E) && !HeldByOther(EKeys::Q))
			{
				KeyBindings[SkillsIndex] = EKeys::E;
				KeyBindings[ItemsIndex] = EKeys::Q;
			}
		}
		// a saved key that's no longer bindable (S, the old Sell key, now only moves the camera) fell back to its default,
		// which another action may have taken: give the clashing action its own default, or else the first free key
		for (int32 i = 0; i < KeyBindings.Num(); ++i)
		{
			auto IsTaken = [this, i](const FKey& Key)
			{
				for (int32 j = 0; j < KeyBindings.Num(); ++j)
				{
					if (j != i && KeyBindings[j] == Key)
					{
						return true;
					}
				}
				return false;
			};
			if (!IsTaken(KeyBindings[i]) || KeyBindings[i] != TDBind::DefaultKey((ETDBind)i))
			{
				continue;
			}
			// this action fell back to a default someone else is using: move it to the first free key
			for (const FKey& Key : TDBind::BindableKeys())
			{
				if (!IsTaken(Key))
				{
					KeyBindings[i] = Key;
					break;
				}
			}
		}
		UIScalePercent = FMath::Clamp(Loaded->UIScalePercent, 80.0f, 150.0f);
		FrameRateStep = FMath::Clamp(Loaded->FrameRateStep, 0, TD::FrameRateStepCount - 1);
		bShowFps = Loaded->bShowFps;
		bVSync = Loaded->bVSync;
		// (the saved all-ranges switch is ignored: the setting was removed, ranges show for the selection and while Alt is held)
		SfxVolume = FMath::Clamp(Loaded->SfxVolume, 0.0f, 100.0f);
		MusicVolume = FMath::Clamp(Loaded->MusicVolume, 0.0f, 100.0f);
		AutoSalvage = FMath::Clamp(Loaded->AutoSalvage, 0, 2);
		BestWave = Loaded->BestWave;
		BestWinLives = Loaded->BestWinLives;
		BestAbyss = Loaded->BestAbyss;
		BestScore = Loaded->BestScore;
		TD::SetLifetimeKills(Loaded->TowerKills);
	}
	else
	{
		TD::SetLifetimeKills({});
	}
	BestWave.SetNumZeroed(TD::MapCount);
	BestWinLives.SetNumZeroed(TD::MapCount);
	BestScore.SetNumZeroed(TD::MapCount);
	// settings from before Abyss tiers: a map won there counts as won at tier 0
	while (BestAbyss.Num() < TD::MapCount)
	{
		BestAbyss.Add(-1);
	}
	BestAbyss.SetNum(TD::MapCount);
	for (int32 Map = 0; Map < TD::MapCount; ++Map)
	{
		BestAbyss[Map] = FMath::Clamp(BestAbyss[Map], BestWinLives[Map] > 0 ? 0 : -1, TD::MaxAbyssTier);
	}
	TDAudio::SetVolumes(SfxVolume / 100.0f, MusicVolume / 100.0f);
	++BindingsVersion;
	ApplyDisplaySettings();
	ApplyAntiAliasing();
}

void ATDGameMode::ApplyDisplaySettings()
{
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		UserSettings->SetFrameRateLimit((float)GetFrameRateLimit());
		UserSettings->SetVSyncEnabled(bVSync);
		UserSettings->ApplyNonResolutionSettings();
		UserSettings->SaveSettings();
	}
}

void ATDGameMode::SetFrameRateStep(int32 Step)
{
	FrameRateStep = FMath::Clamp(Step, 0, TD::FrameRateStepCount - 1);
	ApplyDisplaySettings();
}

int32 ATDGameMode::GetGraphicsQuality() const
{
	const UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	return UserSettings ? FMath::Clamp(UserSettings->GetGlobalIlluminationQuality(), 0, GraphicsQualityLevels - 1) : GraphicsQualityLevels - 1;
}

void ATDGameMode::SetGraphicsQuality(int32 Level)
{
	Level = FMath::Clamp(Level, 0, GraphicsQualityLevels - 1);
	if (UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		// only the groups Lumen and the shadow maps read: the board keeps its native resolution and textures
		UserSettings->SetGlobalIlluminationQuality(Level);
		UserSettings->SetReflectionQuality(Level);
		UserSettings->SetShadowQuality(Level);
		UserSettings->ApplyNonResolutionSettings();
		UserSettings->SaveSettings();
	}
}

const TCHAR* ATDGameMode::GraphicsQualityName(int32 Level)
{
	switch (Level)
	{
	case 0:		return TEXT("Low (no Lumen, fastest)");
	case 1:		return TEXT("Medium");
	case 2:		return TEXT("High");
	default:	return TEXT("Epic");
	}
}

void ATDGameMode::SetVSync(bool bEnabled)
{
	bVSync = bEnabled;
	ApplyDisplaySettings();
}

void ATDGameMode::SaveSettings()
{
	UTDSettingsSave* Save = Cast<UTDSettingsSave>(UGameplayStatics::CreateSaveGameObject(UTDSettingsSave::StaticClass()));
	if (Save)
	{
		Save->UIScalePercent = UIScalePercent;
		Save->FrameRateStep = FrameRateStep;
		Save->bShowFps = bShowFps;
		Save->bVSync = bVSync;
		Save->bShowAllRanges = false;	// kept in the save format only so old settings files still load
		Save->bSharpAA = bSharpAA;
		Save->SfxVolume = SfxVolume;
		Save->MusicVolume = MusicVolume;
		Save->AutoSalvage = AutoSalvage;
		Save->BestWave = BestWave;
		Save->BestWinLives = BestWinLives;
		Save->BestAbyss = BestAbyss;
		Save->BestScore = BestScore;
		Save->TowerKills = TD::LifetimeKills();
		bMasteryDirty = false;
		for (const FKey& Key : KeyBindings)
		{
			Save->KeyBindings.Add(Key.GetFName().ToString());
		}
		UGameplayStatics::SaveGameToSlot(Save, SettingsSlot, 0);
	}
}

void ATDGameMode::SetUIScalePercent(float Value)
{
	UIScalePercent = FMath::Clamp(FMath::RoundToFloat(Value / 5.0f) * 5.0f, 80.0f, 150.0f);
}

// ---------------------------------------------------------------------------------------------
// Save / load
// ---------------------------------------------------------------------------------------------

bool ATDGameMode::CanSaveGame(FString* OutReason) const
{
	if (bGameOver)
	{
		if (OutReason)
		{
			*OutReason = TEXT("Can't save after the reef has fallen");
		}
		return false;
	}
	if (IsWaveActive())
	{
		if (OutReason)
		{
			*OutReason = TEXT("Save between waves");
		}
		return false;
	}
	return true;
}

bool ATDGameMode::HasSaveGame() const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName(CurrentSlot), 0);
}

bool ATDGameMode::SaveGame(bool bQuiet)
{
	FString Reason;
	if (!CanSaveGame(&Reason))
	{
		if (!bQuiet)
		{
			Toast(Reason, FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		}
		return false;
	}

	UTDSaveGame* Save = Cast<UTDSaveGame>(UGameplayStatics::CreateSaveGameObject(UTDSaveGame::StaticClass()));
	if (!Save || !Grid)
	{
		return false;
	}

	Save->Pearls = Pearls;
	Save->Lives = Lives;
	Save->Wave = Wave;
	Save->NextItemId = NextItemId;
	Save->BuildPhaseId = BuildPhaseId;
	Save->SavedAt = FDateTime::Now();
	Save->MapIndex = TD::GetMap();
	Save->bEndless = bEndless;
	Save->PlayTime = PlayTime;
	Save->LeaksByType = LeaksByType;
	Save->WavesCleared = WavesCleared;
	Save->RecordAtStart = RecordAtStart;
	Save->bRunCheated = bRunCheated;
	Save->Income = { Income.Kills, Income.Bounty, Income.Trickle, Income.WaveClear, Income.EarlySend, Income.Sell, Income.Salvage };
	Save->AbyssTier = TD::Run().Tier;
	Save->Mutators = (int32)TD::Run().Mutators;
	Save->Boons = (int32)TD::Run().Boons;
	Save->BoonOffer = BoonOffer;
	Save->bNoBoons = !bBoonsEnabled;
	Save->RunId = RunId;
	Save->UniquesFound = (int32)UniquesFound;
	for (const FTDItem& Item : Storage)
	{
		Save->Storage.Add(FTDItemSave::FromItem(Item));
	}
	for (const ATDTower* Tower : Grid->GetTowers())
	{
		if (!Tower)
		{
			continue;
		}
		FTDTowerSave& T = Save->Towers.AddDefaulted_GetRef();
		T.Type = (uint8)Tower->GetTowerType();
		T.Cell = Tower->GetCell();
		T.Level = Tower->GetLevel();
		T.Xp = Tower->GetXp();
		T.SkillPoints = Tower->GetSkillPoints();
		for (int32 i = 0; i < TD::SkillsPerTower; ++i)
		{
			T.SkillRanks.Add(Tower->GetSkillRank(i));
		}
		for (int32 Slot = 0; Slot < (int32)ETDItemSlot::Count; ++Slot)
		{
			const FTDItem& Item = Tower->GetEquipped((ETDItemSlot)Slot);
			if (Item.IsValid())
			{
				T.Items.Add(FTDItemSave::FromItem(Item));
			}
		}
		T.TotalSpent = Tower->GetTotalSpent();
		T.Form = Tower->GetForm();
		T.Targeting = (uint8)Tower->GetTargeting();
		T.HeartWaves = Tower->GetHeartWaves();
		T.SkillSpent = Tower->GetSkillSpent();
		T.DamageDone = Tower->GetDamageDone();
		T.Kills = Tower->GetKills();
	}

	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, SlotName(CurrentSlot), 0);
	if (bOk)
	{
		FillSlotInfo(Save, SlotInfos[CurrentSlot]);
	}
	if (!bQuiet)
	{
		if (!bOk)
		{
			Toast(TEXT("Saving failed"), FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		}
	}
	UE_LOG(LogAquaticTowerDefense, Log, TEXT("Save game: %s (%d towers, %d stored items, wave %d)"), bOk ? TEXT("ok") : TEXT("FAILED"), Save->Towers.Num(), Save->Storage.Num(), Wave);
	return bOk;
}

bool ATDGameMode::LoadGame()
{
	return LoadGame(CurrentSlot);
}

bool ATDGameMode::LoadGame(int32 Slot)
{
	Slot = FMath::Clamp(Slot, 0, SaveSlotCount - 1);
	UTDSaveGame* Save = Cast<UTDSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName(Slot), 0));
	if (!Save || !Grid)
	{
		Toast(TEXT("No saved game found"), FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		return false;
	}
	CurrentSlot = Slot;

	TD::SetMap(Save->MapIndex);
	FTDRunRules Rules;
	Rules.Tier = Save->AbyssTier;
	Rules.Mutators = (uint32)Save->Mutators;
	Rules.Boons = (uint32)Save->Boons;
	ResetForRun(Rules, !Save->bNoBoons);
	// saves from before run ids get one now (kept from here on)
	RunId = Save->RunId != 0 ? Save->RunId : NewRunId();
	SlotInfos[CurrentSlot].RunId = RunId;
	// a pending offer comes back as it was (anything unknown or already taken is dropped)
	for (const int32 Boon : Save->BoonOffer)
	{
		if (Boon >= 0 && Boon < TD::BoonCount && TD::BoonFits((ETDBoon)Boon, TD::Run(), Save->Lives) && !BoonOffer.Contains(Boon))
		{
			BoonOffer.Add(Boon);
		}
	}

	Pearls = Save->Pearls;
	Lives = FMath::Max(1, Save->Lives);
	Wave = Save->Wave;
	NextItemId = Save->NextItemId;
	BuildPhaseId = Save->BuildPhaseId + 1;		// loaded towers count as bought earlier (no full refund)
	bEndless = Save->bEndless || Wave > TD::FinalWave;

	// run summary so far (saves made before it start from zero, with the waves already behind them)
	PlayTime = FMath::Max(0.0f, Save->PlayTime);
	for (int32 i = 0; i < LeaksByType.Num() && i < Save->LeaksByType.Num(); ++i)
	{
		LeaksByType[i] = FMath::Max(0, Save->LeaksByType[i]);
	}
	WavesCleared = FMath::Max(Save->WavesCleared, Wave);
	RecordAtStart = Save->RecordAtStart >= 0 ? FMath::Min(Save->RecordAtStart, GetBestWave(TD::GetMap())) : GetBestWave(TD::GetMap());
	bRunCheated = Save->bRunCheated;
	if (Save->Income.Num() == 7)
	{
		const TArray<int32>& In = Save->Income;
		Income.Kills = In[0]; Income.Bounty = In[1]; Income.Trickle = In[2]; Income.WaveClear = In[3];
		Income.EarlySend = In[4]; Income.Sell = In[5]; Income.Salvage = In[6];
	}

	for (const FTDItemSave& Item : Save->Storage)
	{
		Storage.Add(Item.ToItem());
	}

	int32 Restored = 0;
	for (const FTDTowerSave& T : Save->Towers)
	{
		if (T.Type >= (uint8)ETDTowerType::Count || !TD::InBounds(T.Cell) || Grid->IsBlocked(T.Cell))
		{
			continue;
		}
		ATDTower* Tower = Grid->PlaceTower(T.Cell, (ETDTowerType)T.Type);
		if (!Tower)
		{
			continue;
		}
		TArray<FTDItem> Items;
		for (const FTDItemSave& Item : T.Items)
		{
			Items.Add(Item.ToItem());
		}
		Tower->SetPurchaseInfo(T.TotalSpent, -1, false);
		Tower->RestoreState(T.Level, T.Xp, T.SkillPoints, T.SkillRanks, Items, T.TotalSpent, T.SkillSpent, T.DamageDone, T.Kills, T.Form);
		Tower->SetTargeting((ETDTargeting)T.Targeting);
		if (T.HeartWaves > 0)
		{
			Tower->SetHeartWaves(T.HeartWaves);
		}
		++Restored;
	}

	// uniques found: the saved record plus any unique on hand (saves from before uniques had no record)
	{
		TArray<FTDItem> OnHand = Storage;
		for (const ATDTower* Tower : Grid->GetTowers())
		{
			for (int32 ItemSlot = 0; Tower && ItemSlot < (int32)ETDItemSlot::Count; ++ItemSlot)
			{
				OnHand.Add(Tower->GetEquipped((ETDItemSlot)ItemSlot));
			}
		}
		UniquesFound = (uint32)Save->UniquesFound | TD::InferUniquesFound(OnHand);
	}

	// a save made right after the final wave goes back to the victory screen
	OpenMenu(HasWonAwaitingChoice() ? ETDMenu::Victory : HasBoonOffer() ? ETDMenu::Boon : ETDMenu::None);
	UE_LOG(LogAquaticTowerDefense, Log, TEXT("Load game: %d towers, %d stored items, wave %d, %d pearls"), Restored, Storage.Num(), Wave, Pearls);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Storage
// ---------------------------------------------------------------------------------------------

FTDItem ATDGameMode::CreateItem(ETDRarity Rarity)
{
	return TD::GenerateItem(NextItemId++, FMath::Max(1, Wave), Rarity);	// item level = current wave
}

bool ATDGameMode::AddToStorage(const FTDItem& Item)
{
	if (!Item.IsValid() || GetStorageFree() <= 0)
	{
		return false;
	}
	Storage.Add(Item);
	return true;
}

bool ATDGameMode::EquipFromStorage(ATDTower* Tower, int32 StorageIndex)
{
	if (!Tower || !Storage.IsValidIndex(StorageIndex))
	{
		return false;
	}
	if (!Tower->CanProgress())
	{
		Toast(TEXT("Coral Walls can't hold items"), FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		return false;
	}

	if (TD::Run().IsItemSlotLocked(Storage[StorageIndex].Slot))
	{
		Toast(FString::Printf(TEXT("Lean Reef: the %s slot is locked"), TD::SlotName(Storage[StorageIndex].Slot)), FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		return false;
	}

	const FTDItem Item = Storage[StorageIndex];
	Storage.RemoveAt(StorageIndex);
	const FTDItem Previous = Tower->Equip(Item);
	if (Previous.IsValid())
	{
		Storage.Insert(Previous, FMath::Min(StorageIndex, Storage.Num()));
	}
	UE_LOG(LogAquaticTowerDefense, Log, TEXT("Equipped %s -> %s"), *Item.Name, *Tower->DescribeStats());
	return true;
}

bool ATDGameMode::UnequipToStorage(ATDTower* Tower, ETDItemSlot Slot)
{
	if (!Tower || !Tower->GetEquipped(Slot).IsValid())
	{
		return false;
	}
	if (GetStorageFree() <= 0)
	{
		Toast(TEXT("Storage full"), FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		return false;
	}
	Storage.Add(Tower->Unequip(Slot));
	UE_LOG(LogAquaticTowerDefense, Log, TEXT("Unequipped -> %s"), *Tower->DescribeStats());
	return true;
}

bool ATDGameMode::SalvageItem(int32 StorageIndex)
{
	if (!Storage.IsValidIndex(StorageIndex))
	{
		return false;
	}
	const FTDItem Item = Storage[StorageIndex];
	if (Item.bLocked)
	{
		Toast(FString::Printf(TEXT("%s is locked - unlock it to salvage it"), *Item.Name), FLinearColor(1.0f, 0.6f, 0.4f), ETDToast::Refusal);
		return false;
	}
	const int32 Value = TD::SalvageValue(Item.Rarity);
	Storage.RemoveAt(StorageIndex);
	Pearls += Value;
	Income.Salvage += Value;
	return true;
}

bool ATDGameMode::UpgradeStorageItem(int32 StorageIndex)
{
	if (!Storage.IsValidIndex(StorageIndex))
	{
		return false;
	}
	FTDItem& Item = Storage[StorageIndex];
	if (Item.Level >= TD::MaxItemLevel)
	{
		Toast(TEXT("This item is already at the highest level"), FLinearColor(1.0f, 0.6f, 0.4f), ETDToast::Refusal);
		return false;
	}
	const int32 Cost = TD::UpgradeCost(Item);
	if (Pearls < Cost)
	{
		Toast(FString::Printf(TEXT("Upgrading costs %d pearls"), Cost), FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		return false;
	}
	Pearls -= Cost;
	TD::UpgradeItem(Item);
	TDAudio::Play(this, ETDSound::Upgrade);
	return true;
}

bool ATDGameMode::RerollStorageItem(int32 StorageIndex)
{
	if (!Storage.IsValidIndex(StorageIndex))
	{
		return false;
	}
	FTDItem& Item = Storage[StorageIndex];
	if (TD::IsUnique(Item))
	{
		Toast(TEXT("Uniques can't be rerolled"), FLinearColor(1.0f, 0.6f, 0.4f), ETDToast::Refusal);
		return false;
	}
	const int32 Cost = TD::RerollCost(Item);
	if (Pearls < Cost)
	{
		Toast(FString::Printf(TEXT("Rerolling costs %d pearls"), Cost), FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		return false;
	}
	Pearls -= Cost;
	TD::RerollItem(Item);
	TDAudio::Play(this, ETDSound::Upgrade);
	return true;
}

void ATDGameMode::SortStorage()
{
	// locked items stay put; the rest are sorted into the remaining places
	TArray<FTDItem> Loose;
	for (const FTDItem& Item : Storage)
	{
		if (!Item.bLocked)
		{
			Loose.Add(Item);
		}
	}
	Loose.StableSort([](const FTDItem& A, const FTDItem& B)
	{
		if (A.Rarity != B.Rarity)
		{
			return A.Rarity > B.Rarity;
		}
		return A.Slot < B.Slot;
	});
	int32 Next = 0;
	for (FTDItem& Item : Storage)
	{
		if (!Item.bLocked)
		{
			Item = Loose[Next++];
		}
	}
}

bool ATDGameMode::ToggleItemLock(int32 StorageIndex)
{
	if (!Storage.IsValidIndex(StorageIndex))
	{
		return false;
	}
	FTDItem& Item = Storage[StorageIndex];
	Item.bLocked = !Item.bLocked;
	return true;
}

const TCHAR* ATDGameMode::AutoSalvageName(int32 Level)
{
	switch (Level)
	{
	case 1:		return TEXT("Common");
	case 2:		return TEXT("Common and Rare");
	default:	return TEXT("Off");
	}
}

bool ATDGameMode::TryClickPickup(const FVector& WorldPoint)
{
	ATDPickup* Closest = nullptr;
	float Best = 70.0f;
	for (TActorIterator<ATDPickup> It(GetWorld()); It; ++It)
	{
		if (It->IsActorBeingDestroyed())
		{
			continue;
		}
		const float Distance = FVector::Dist2D(It->GetActorLocation(), WorldPoint);
		if (Distance < Best)
		{
			Best = Distance;
			Closest = *It;
		}
	}

	if (!Closest)
	{
		return false;
	}
	Closest->TryCollect(true);
	return true;
}

// ---------------------------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------------------------

void ATDGameMode::ShowWaveClearCard(int32 WaveNumber, int32 PearlsGained, int32 Interest, int32 Xp, int32 LivesGained) const
{
	ATDHUD* HUD = GetTDHUD();
	if (HUD)
	{
		HUD->ShowWaveClear(WaveNumber, PearlsGained, Interest, Xp, LivesGained);
	}
}

void ATDGameMode::FloatingText(const FVector& WorldLocation, const FString& Text, const FLinearColor& Color) const
{
	if (ATDHUD* HUD = GetTDHUD())
	{
		HUD->AddFloatingText(WorldLocation, Text, Color);
	}
}

ATDHUD* ATDGameMode::GetTDHUD() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? Cast<ATDHUD>(PC->GetHUD()) : nullptr;
}

void ATDGameMode::Toast(const FString& Message, const FLinearColor& Color, ETDToast Kind) const
{
	ATDHUD* HUD = GetTDHUD();
	if (HUD && TD::ShouldShowToast(Kind))
	{
		HUD->ShowToast(Message, Color);
	}
	UE_LOG(LogAquaticTowerDefense, Log, TEXT("Aquatic Tower Defense: %s"), *Message);
}
