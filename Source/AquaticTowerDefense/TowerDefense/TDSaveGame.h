// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "TDItems.h"
#include "TDTypes.h"
#include "TDSaveGame.generated.h"

USTRUCT()
struct FTDAffixSave
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Type = 0;

	UPROPERTY()
	float Value = 0.0f;
};

USTRUCT()
struct FTDItemSave
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	UPROPERTY()
	FString Name;

	/** Item type (see TD::ItemKind); 0 for random-affix items saved before item types, which load with their saved affixes */
	UPROPERTY()
	int32 Kind = 0;

	UPROPERTY()
	uint8 Rarity = 0;

	UPROPERTY()
	uint8 Slot = 0;

	UPROPERTY()
	int32 Level = 1;

	UPROPERTY()
	TArray<FTDAffixSave> Affixes;

	/** 0 for items saved before range affixes were halved; ToItem scales those down */
	UPROPERTY()
	int32 RangeRevision = 0;

	/** Locked by the player (Sort and Salvage skip it) */
	UPROPERTY()
	bool bLocked = false;

	static FTDItemSave FromItem(const FTDItem& Item);
	FTDItem ToItem() const;
};

USTRUCT()
struct FTDTowerSave
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Type = 0;

	UPROPERTY()
	FIntPoint Cell = FIntPoint::ZeroValue;

	UPROPERTY()
	int32 Level = 1;

	UPROPERTY()
	float Xp = 0.0f;

	UPROPERTY()
	int32 SkillPoints = 0;

	UPROPERTY()
	TArray<int32> SkillRanks;

	UPROPERTY()
	TArray<FTDItemSave> Items;

	UPROPERTY()
	int32 TotalSpent = 0;

	UPROPERTY()
	int32 SkillSpent = 0;

	UPROPERTY()
	float DamageDone = 0.0f;

	UPROPERTY()
	int32 Kills = 0;

	/** Specialisation: 0 = none, 1 or 2 (see TD::Form) */
	UPROPERTY()
	int32 Form = 0;

	/** Targeting priority (ETDTargeting; 0 = First, also for older saves) */
	UPROPERTY()
	uint8 Targeting = 0;

	/** Heart of Coral: waves cleared while it was equipped */
	UPROPERTY()
	int32 HeartWaves = 0;
};

/** A full game snapshot, taken between waves */
UCLASS()
class UTDSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY()
	int32 Pearls = 0;

	UPROPERTY()
	int32 Lives = 0;

	UPROPERTY()
	int32 Wave = 0;

	UPROPERTY()
	int32 NextItemId = 1;

	UPROPERTY()
	int32 BuildPhaseId = 0;

	UPROPERTY()
	TArray<FTDTowerSave> Towers;

	UPROPERTY()
	TArray<FTDItemSave> Storage;

	UPROPERTY()
	FDateTime SavedAt;

	UPROPERTY()
	int32 MapIndex = 0;

	/** Endless mode was chosen after beating the final wave */
	UPROPERTY()
	bool bEndless = false;

	// ---- run summary so far ----

	/** Seconds played (unpaused, no menu open) */
	UPROPERTY()
	float PlayTime = 0.0f;

	/** Creatures that reached the reef, by ETDEnemyType */
	UPROPERTY()
	TArray<int32> LeaksByType;

	UPROPERTY()
	int32 WavesCleared = 0;

	/** The map's best wave when this game started (-1 in saves made before records) */
	UPROPERTY()
	int32 RecordAtStart = -1;

	/** Dev tools or debug helpers touched this game, so it doesn't count for records */
	UPROPERTY()
	bool bRunCheated = false;

	/** Pearls earned by source: kills, bounty, trickle, wave clear, early send, sell, salvage */
	UPROPERTY()
	TArray<int32> Income;

	// ---- run rules (saves made before them load as Abyss 0, no mutators, no boons) ----

	/** Abyss tier of the run, 0 = normal */
	UPROPERTY()
	int32 AbyssTier = 0;

	/** Mutator bits (ETDMutator) */
	UPROPERTY()
	int32 Mutators = 0;

	/** Taken boon bits (ETDBoon) */
	UPROPERTY()
	int32 Boons = 0;

	/** Boons on offer and not picked yet (ETDBoon values), so loading doesn't reroll them */
	UPROPERTY()
	TArray<int32> BoonOffer;

	/** Scripted runs (tests, dev tools) never get boon offers */
	UPROPERTY()
	bool bNoBoons = false;

	/** Auto-salvage of drops for this run: 0 = off, 1 = Common, 2 = up to Rare, 3 = up to Epic (older saves load as off) */
	UPROPERTY()
	int32 AutoSalvage = 0;

	/** Identifies the run across its saves (0 in older saves), so Retry knows whether a slot holds this run */
	UPROPERTY()
	int32 RunId = 0;

	/** Unique legendaries found this run (bits by ETDUnique; older saves: worked out from the items on hand) */
	UPROPERTY()
	int32 UniquesFound = 0;
};

/** Player settings, kept in their own slot */
UCLASS()
class UTDSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:

	/** HUD text and panel size in percent, 80-150 */
	UPROPERTY()
	float UIScalePercent = 100.0f;

	/** Index into TD::FrameRateSteps */
	UPROPERTY()
	int32 FrameRateStep = TD::DefaultFrameRateStep;

	UPROPERTY()
	bool bShowFps = false;

	UPROPERTY()
	bool bVSync = false;

	UPROPERTY()
	bool bShowAllRanges = false;

	/** Sharper temporal anti-aliasing (TSR) instead of FXAA */
	UPROPERTY()
	bool bSharpAA = false;

	/** Effects and music volume in percent, 0-100 */
	UPROPERTY()
	float SfxVolume = 100.0f;

	UPROPERTY()
	float MusicVolume = 100.0f;


	/** Best wave cleared on each map (by map index) */
	UPROPERTY()
	TArray<int32> BestWave;

	/** Most lives left when winning on each map (0 = not won there yet) */
	UPROPERTY()
	TArray<int32> BestWinLives;

	/** Highest Abyss tier won on each map (-1 = never won; missing in older settings, which load it from BestWinLives) */
	UPROPERTY()
	TArray<int32> BestAbyss;

	/** Best score on each map (0 = none yet) */
	UPROPERTY()
	TArray<int32> BestScore;

	/** Tower mastery: lifetime kills by tower type, from runs that count for records */
	UPROPERTY()
	TArray<int32> TowerKills;

	/** No longer used: auto-salvage is now per run (in the run save) and starts off in every new game.
	 *  Kept only so old settings files still load. */
	UPROPERTY()
	int32 AutoSalvage = 0;

	/** Rebindable keys by ETDBind, as key names (empty = default) */
	UPROPERTY()
	TArray<FString> KeyBindings;
};
