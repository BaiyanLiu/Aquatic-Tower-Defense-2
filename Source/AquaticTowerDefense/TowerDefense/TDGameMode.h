// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/GameModeBase.h"
#include "TDTypes.h"
#include "TDItems.h"
#include "TDRun.h"
#include "TDRules.h"
#include "TDGameMode.generated.h"

class ATDGrid;
class ATDTower;
class ATDEnemy;
class ATDHUD;

/** Full-screen menus drawn by the HUD */
enum class ETDMenu : uint8
{
	None,
	Main,
	Pause,
	Settings,
	Controls,
	Victory,	// shown after beating the final wave: continue in endless mode or start over
	Setup,		// new game setup: map, Abyss tier and mutators
	Boon,		// pick one of the boons on offer (the run waits for it)
	Mastery,	// tower mastery: lifetime kills and ranks per tower type
	Slots		// save slot picker: load or delete one of the three runs
};

/** A question the HUD shows over the current screen (Yes / No) */
enum class ETDConfirm : uint8
{
	None,
	OverwriteForNewGame,	// Start on the setup screen would replace the run in the chosen slot
	OverwriteForRetry,		// Retry would replace a different run in the run's slot
	DeleteSlot
};

/** What a save slot holds, read once when slots change (not every frame) */
struct FTDSlotInfo
{
	bool bUsed = false;
	int32 Map = 0;
	int32 Tier = 0;
	int32 MutatorCount = 0;
	int32 Wave = 0;
	int32 Score = 0;
	int32 RunId = 0;
	bool bEndless = false;
	float PlayTime = 0.0f;
	FDateTime SavedAt;
};

/** Keys the player can rebind */
enum class ETDBind : uint8
{
	Wave,
	Skills,
	Storage,
	Sell,
	Speed,
	Pause,
	Items,		// added last, so the saved keys of the actions above keep their places
	Targeting,	// cycles the selected tower's targeting priority (appended after Items for the same reason)
	TowerFirst,	// picks a tower to build: one per ETDTowerType, in dock order (Wall .. Acid Vent)
	TowerLast = TowerFirst + (int32)ETDTowerType::Count - 1,
	Info,		// held: every tower's range and level (Alt by default)
	Count
};

namespace TDBind
{
	FString ActionName(ETDBind Bind);
	FKey DefaultKey(ETDBind Bind);

	/** The build hotkey of a tower type */
	inline ETDBind TowerBind(ETDTowerType Type) { return (ETDBind)((int32)ETDBind::TowerFirst + (int32)Type); }
	/** Actions that have their own input action (the tower keys share one, Info is polled) */
	constexpr int32 ActionCount = (int32)ETDBind::TowerFirst;
	/** Short name for menus and key caps (either Alt shows as "Alt") */
	FString KeyName(const FKey& Key);

	/** Keys that may be bound (Esc, mouse and camera keys are reserved) */
	const TArray<FKey>& BindableKeys();
}

/** Pearls earned this game by source (for the balance sim; reset on a new game) */
struct FTDIncome
{
	int32 Kills = 0;
	int32 Bounty = 0;
	int32 Trickle = 0;
	int32 WaveClear = 0;
	int32 EarlySend = 0;
	int32 Sell = 0;
	int32 Salvage = 0;
};

/**
 *  Aquatic Tower Defense game mode.
 *  Owns the economy (pearls, reef lives), wave spawning, game speed and game over / restart.
 *  Also dresses the level with underwater lighting if it lacks it, and spawns the board.
 */
UCLASS()
class ATDGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ATDGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	/** Keeps mastery kills from the wave in progress when the window is closed */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Always spawns the camera pawn over the board, so the level needs no PlayerStart */
	virtual void RestartPlayer(AController* NewPlayer) override;

	// ---- State ----

	ATDGrid* GetGrid() const { return Grid; }
	const TArray<TObjectPtr<ATDEnemy>>& GetEnemies() const { return Enemies; }

	int32 GetPearls() const { return Pearls; }
	const FTDIncome& GetIncome() const { return Income; }
	int32 GetLives() const { return Lives; }
	int32 GetWave() const { return Wave; }
	bool IsWaveActive() const { return ActiveWaves.Num() > 0; }
	bool IsGameOver() const { return bGameOver; }

	/** Endless mode: chosen on the victory screen after the final wave; creatures then scale much faster */
	bool IsEndless() const { return bEndless; }
	/** The final wave has been beaten and the victory screen hasn't been answered yet */
	bool HasWonAwaitingChoice() const { return !bEndless && Wave >= TD::FinalWave && !IsWaveActive(); }
	void ContinueEndless();
	int32 GetGameSpeed() const { return GameSpeed; }

	/** Bonus for calling the next wave while one is running */
	int32 GetEarlyBonus() const { return 2 + (Wave + 1); }

	/** Enemy groups (type, count) of wave N, in spawn order */
	static void GetWaveComposition(int32 N, TArray<TPair<ETDEnemyType, int32>>& Out);

	/** Waves that bring an elite mini-boss (every 5th wave that isn't a boss wave) */
	static bool IsMiniBossWave(int32 N) { return N % 5 == 0 && N % 10 != 0; }

	/** Waves where the groups arrive together instead of one after another */
	static bool IsMixedWave(int32 N) { return N >= 5 && N % 3 == 2; }

	/** Fraction of the newest running wave that has spawned */
	float GetCurrentWaveProgress() const;
	int32 GetCurrentWaveNumber() const;

	// ---- Menus, settings and saving ----

	ETDMenu GetMenu() const { return Menu; }
	void OpenMenu(ETDMenu NewMenu);
	void CloseMenu();
	void OpenSettings();
	void CloseSettings();
	/** Starts a new game at once on a random map (or the forced one) with no Abyss tier, mutators or boons (tests, dev tools) */
	void StartNewGame();

	// ---- new game setup (the player's way to start a game) ----

	/** Choices on the setup screen. Map = -1 picks a random map (as StartNewGame does). */
	struct FTDRunSetup
	{
		int32 Map = -1;
		int32 Tier = 0;
		uint32 Mutators = 0;
		int32 Slot = 0;		// save slot the new run will use
	};

	/** Opens the setup screen; Back returns to the menu that was open */
	void OpenNewGameSetup();
	void CloseNewGameSetup();
	const FTDRunSetup& GetSetup() const { return Setup; }
	void SetSetupMap(int32 Map);
	void SetSetupTier(int32 Tier);
	void ToggleSetupMutator(int32 Mutator);
	float GetSetupMultiplier() const { return TD::ScoreMultiplier(Setup.Tier, Setup.Mutators); }

	/** Starts the game chosen on the setup screen, with a boon offer before wave 1 */
	void StartSetupGame();

	/** The setup screen's Start: asks first when the chosen slot holds a run */
	void RequestStartSetupGame();
	void SetSetupSlot(int32 Slot) { Setup.Slot = FMath::Clamp(Slot, 0, SaveSlotCount - 1); }

	// ---- save slots ----

	static constexpr int32 SaveSlotCount = 3;
	/** Slot 1 keeps the old single save's name, so an existing save simply is slot 1 */
	static FString SlotName(int32 Slot) { return Slot <= 0 ? FString(TEXT("ATDSave")) : FString::Printf(TEXT("ATDSave%d"), Slot + 1); }
	static void FillSlotInfo(const class UTDSaveGame* Save, FTDSlotInfo& Out);
	const FTDSlotInfo& GetSlotInfo(int32 Slot) const { return SlotInfos[FMath::Clamp(Slot, 0, SaveSlotCount - 1)]; }
	void RefreshSlotInfo();
	/** The run's slot (autosave and Save game write here) */
	int32 GetCurrentSlot() const { return CurrentSlot; }
	bool HasAnySave() const;
	/** Load / Continue: the slot picker; Back returns to the menu that opened it */
	void OpenSlotPicker();
	void CloseSlotPicker();
	bool LoadGame(int32 Slot);
	/** Asks, then deletes a slot */
	void RequestDeleteSlot(int32 Slot);

	// ---- retry ----

	/** Game over / victory: a new run with the same map, Abyss tier and mutators in the same slot (asks only if the slot holds another run) */
	void RequestRetry();
	void RetryRun();

	// ---- confirm dialog ----

	ETDConfirm GetPendingConfirm() const { return PendingConfirm; }
	FString GetConfirmText() const;
	void AnswerConfirm(bool bYes);

	// ---- targeting ----

	/** Sets a tower's priority; new towers of that type in this run start with it */
	void SetTowerTargeting(ATDTower* Tower, ETDTargeting Mode);

	// ---- run rules, score and boons ----

	const FTDRunRules& GetRunRules() const { return TD::Run(); }
	float GetScoreMultiplier() const { return TD::ScoreMultiplier(TD::Run().Tier, TD::Run().Mutators, TD::Run().Boons); }
	/** Score of this run so far: (waves cleared x 100 + lives x 25 once the final wave is beaten) x multiplier */
	int32 GetScore() const;

	/** Boons on offer (ETDBoon values), empty when none is waiting */
	const TArray<int32>& GetBoonOffer() const { return BoonOffer; }
	bool HasBoonOffer() const { return BoonOffer.Num() > 0; }
	/** Takes a boon from the offer */
	bool PickBoon(int32 Boon);
	/** Fixed Towers (mutator): a tower may only be sold in the build phase it was bought in */
	bool CanSellTower(const ATDTower* Tower, FString* OutReason = nullptr) const;

	/** Unique legendaries found this run (bits by ETDUnique; saved with the run) */
	uint32 GetUniquesFound() const { return UniquesFound; }

	/** Dev tools: puts every unique legendary into storage (the run doesn't count) */
	void DebugAddUniques();

	/** Dev tools: ends the run at once (the game over card; the run doesn't count) */
	void DebugGameOver();

	/** Dev tools: starts a plain game at an Abyss tier, unlocked or not (the run doesn't count) */
	void DebugStartAbyss(int32 Tier);

	/** Dev tools: sets a tower type's lifetime kills (not a cheat for the run: mastery is lifetime) */
	void DebugSetMasteryKills(ETDTowerType Type, int32 Kills);

	/** Dev tools: offers boons now (even in a scripted run); bForceCursed makes one card cursed */
	void DebugOfferBoons(bool bForceCursed = false);

	/** Dev: makes boon Boon the last card of the current offer (for screenshots of a given boon) */
	void DebugSetLastOffer(int32 Boon);
	void ReturnToMainMenu();
	void QuitGame();

	float GetEnemyHealthPercent() const { return EnemyHealthPercent; }
	float GetUIScalePercent() const { return UIScalePercent; }
	void SetUIScalePercent(float Value);

	/** Frame rate cap as an index into TD::FrameRateSteps; applied and saved through the engine's user settings */
	int32 GetFrameRateStep() const { return FrameRateStep; }
	void SetFrameRateStep(int32 Step);
	int32 GetFrameRateLimit() const { return TD::FrameRateSteps[FrameRateStep]; }

	/**
	 *  Lighting quality, 0-3 (Low, Medium, High, Epic). Drives only the engine's global illumination, reflection
	 *  and shadow scalability groups (Lumen's cost), never resolution or textures. Saved in GameUserSettings.ini.
	 */
	int32 GetGraphicsQuality() const;
	void SetGraphicsQuality(int32 Level);
	static const TCHAR* GraphicsQualityName(int32 Level);
	static constexpr int32 GraphicsQualityLevels = 4;

	bool IsShowingFps() const { return bShowFps; }
	void SetShowFps(bool bShow) { bShowFps = bShow; }
	bool IsVSyncEnabled() const { return bVSync; }
	void SetVSync(bool bEnabled);
	void SaveSettings();

	/** Anti-aliasing: false = FXAA (steady moving creatures), true = TSR (sharper, can ghost) */
	bool IsSharpAA() const { return bSharpAA; }

	float GetSfxVolume() const { return SfxVolume; }
	void SetSfxVolume(float Percent);
	float GetMusicVolume() const { return MusicVolume; }
	void SetMusicVolume(float Percent);

	/** Every new game picks a random map (a different one from the last), unless a map is forced (tests, -ATDMap=N) */
	void ForceMap(int32 Map) { ForcedMap = Map; }
	int32 PickMap() const;
	void SetSharpAA(bool bSharp);

	// ---- key bindings ----
	FKey GetKey(ETDBind Bind) const { return KeyBindings.IsValidIndex((int32)Bind) ? KeyBindings[(int32)Bind] : TDBind::DefaultKey(Bind); }
	FString GetKeyName(ETDBind Bind) const { return TDBind::KeyName(GetKey(Bind)); }
	/** Sets a key; an action already using it swaps to the old key */
	void SetKey(ETDBind Bind, const FKey& Key);
	void ResetKeys();
	int32 GetBindingsVersion() const { return BindingsVersion; }

	/** Action waiting for a key press in the controls menu, or -1 */
	int32 GetRebinding() const { return Rebinding; }
	void StartRebinding(int32 Index) { Rebinding = Index; }
	void StopRebinding() { Rebinding = -1; }
	void OpenControls() { OpenMenu(ETDMenu::Controls); }
	void CloseControls() { Rebinding = -1; SaveSettings(); OpenMenu(ETDMenu::Settings); }

	/** Saving is only allowed between waves */
	bool CanSaveGame(FString* OutReason = nullptr) const;
	bool HasSaveGame() const;
	bool SaveGame(bool bQuiet = false);
	/** Loads the run's slot */
	bool LoadGame();

	/** Spawns a smaller, faster copy of a splitting sea star */
	void SpawnSplit(ATDEnemy* Parent);

	// ---- Player actions (return false and toast the reason on failure) ----

	/** Places a tower. With bOntoWall a non-wall tower may replace the Coral Wall on the tile (the player's click does this). */
	ATDTower* TryPlaceTower(const FIntPoint& Cell, ETDTowerType Type, bool bQuiet, bool bOntoWall = false);

	/** Pearls needed to build a tower straight onto the wall on a tile: its cost minus the wall's refund (-1 if there's no wall) */
	int32 GetBuildOnWallCost(const FIntPoint& Cell, ETDTowerType Type) const;
	bool TryLearnSkill(ATDTower* Tower, int32 SkillIndex);

	/** Picks a tower's specialisation (form 1 or 2, free, once, from TD::FormLevel) */
	bool TryChooseForm(ATDTower* Tower, int32 Form);

	/** Sells a tower, returning its items to storage (refused if storage can't hold them) */
	bool SellTower(ATDTower* Tower);
	int32 GetSellValue(const ATDTower* Tower) const;

	/** Releases the next wave, or sends it early (with a bonus) if a wave is already running */
	void ReleaseNextWave();

	void CycleGameSpeed();

	/** Resets the whole game in place */
	void RestartGame();

	/** Debug helper: builds a sample maze and releases a few waves */
	void SetupDemo(int32 Waves);

	// ---- Debug helpers for the dev tools ----
	// (each one marks the run as not counting for the best-wave records)
	void DebugAddPearls(int32 Amount) { Pearls += Amount; bRunCheated = true; }
	void DebugSetLives(int32 Amount) { Lives = FMath::Max(1, Amount); bRunCheated = true; }
	void DebugSetWave(int32 Number) { Wave = FMath::Max(0, Number); bEndless = Wave >= TD::FinalWave; bRunCheated = true; }
	/** Sets enemy health past the settings slider range, for balance probes (not saved unless settings are) */
	void DebugSetEnemyHealthPercent(float Value) { EnemyHealthPercent = FMath::Max(1.0f, Value); bRunCheated = true; }
	/** Simulator: turn boon offers on for the run in play (before wave 1 and at the milestones), like a set-up game */
	void DebugEnableBoons() { bBoonsEnabled = true; OfferBoons(); }
	/** Spawns one creature outside any wave, for benchmarks */
	void DebugSpawnEnemy(ETDEnemyType Type, float HealthMultiplier, float SpeedScale);

	// ---- Master storage ----

	const TArray<FTDItem>& GetStorage() const { return Storage; }
	/** Storage slots in this run (Hungry Deep takes some away; items past the limit stay, but nothing new comes in) */
	int32 GetStorageCapacity() const { return TD::StorageSlots - TD::Run().StorageCut(); }
	int32 GetStorageFree() const { return GetStorageCapacity() - Storage.Num(); }

	/** Adds an item to storage; false if full */
	bool AddToStorage(const FTDItem& Item);

	/** Creates a new random item with a unique id */
	FTDItem CreateItem(ETDRarity Rarity);

	/** Equips a storage item on a tower, swapping the old item in that slot back to storage */
	bool EquipFromStorage(ATDTower* Tower, int32 StorageIndex);

	/** Moves an equipped item back to storage */
	bool UnequipToStorage(ATDTower* Tower, ETDItemSlot Slot);

	/** Destroys a storage item for pearls */
	bool SalvageItem(int32 StorageIndex);

	/** Item workbench: spend pearls to raise a stored item's level, or reroll its stats */
	bool UpgradeStorageItem(int32 StorageIndex);
	bool RerollStorageItem(int32 StorageIndex);

	/** Sorts storage by rarity (best first), then slot. Locked items keep their places. */
	void SortStorage();

	/** Locks or unlocks a stored item (locked items can't be salvaged and aren't moved by Sort) */
	bool ToggleItemLock(int32 StorageIndex);

	/** Auto-salvage of drops: 0 = off, 1 = Common, 2 = Common and Rare */
	int32 GetAutoSalvage() const { return AutoSalvage; }
	void CycleAutoSalvage() { AutoSalvage = (AutoSalvage + 1) % 3; SaveSettings(); }
	static const TCHAR* AutoSalvageName(int32 Level);

	/** Collects a dropped item near a world point (when clicked). Returns true if there was one. */
	bool TryClickPickup(const FVector& WorldPoint);

	// ---- Enemy callbacks ----

	void OnEnemyKilled(ATDEnemy* Enemy, ATDTower* Killer);
	void OnEnemyLeaked(ATDEnemy* Enemy);

	// ---- Run summary and best-wave records ----

	/** Creatures that reached the reef this game, by ETDEnemyType */
	const TArray<int32>& GetLeaksByType() const { return LeaksByType; }
	int32 GetTotalLeaks() const;

	/** Time spent playing this game (real seconds while unpaused, saved with the game) */
	float GetPlayTime() const { return PlayTime; }

	/** Highest wave cleared this game */
	int32 GetWavesCleared() const { return WavesCleared; }

	/** Highest Abyss tier won on a map (-1 = never won), the highest tier that may be chosen there, and the best score */
	int32 GetBestAbyss(int32 Map) const { return BestAbyss.IsValidIndex(Map) ? BestAbyss[Map] : -1; }
	int32 GetUnlockedTier(int32 Map) const { return FMath::Clamp(GetBestAbyss(Map) + 1, 0, TD::MaxAbyssTier); }
	int32 GetBestScore(int32 Map) const { return BestScore.IsValidIndex(Map) ? BestScore[Map] : 0; }

	/** Best wave cleared on a map in any game, and the most lives left on a win there (0 = never won) */
	int32 GetBestWave(int32 Map) const { return BestWave.IsValidIndex(Map) ? BestWave[Map] : 0; }
	int32 GetBestWinLives(int32 Map) const { return BestWinLives.IsValidIndex(Map) ? BestWinLives[Map] : 0; }

	/** This game beat the map's previous best wave */
	bool IsNewRecord() const { return WavesCleared > RecordAtStart && !bRunCheated; }

	/** Records only count games played normally (not dev tools, demos or debug cheats) */
	bool CountsForRecords() const;

	/** Shows a toast message on the HUD */
	void Toast(const FString& Message, const FLinearColor& Color = FLinearColor::White, ETDToast Kind = ETDToast::Important) const;

	/** The card that slides in at the reef when a wave is cleared */
	void ShowWaveClearCard(int32 WaveNumber, int32 PearlsGained, int32 Interest, int32 Xp, int32 LivesGained) const;

	/** Shows floating text in the world (e.g. "+3", "LEVEL 4") */
	void FloatingText(const FVector& WorldLocation, const FString& Text, const FLinearColor& Color) const;

protected:

	struct FPendingSpawn
	{
		ETDEnemyType Type;
		float Time;		// seconds after the wave started
		uint8 Mods = 0;	// TDMod bits fixed by the schedule (the elite); fast / shielded are rolled at spawn
		float HealthScale = 1.0f;	// Swarm creatures come with half health (and half the reward)
	};

	struct FWaveRun
	{
		int32 Number = 0;
		float HealthMultiplier = 1.0f;
		float Elapsed = 0.0f;
		int32 NextSpawn = 0;
		int32 Alive = 0;
		TArray<FPendingSpawn> Spawns;
	};

	/** Builds the spawn schedule for wave N */
	static void BuildWave(int32 N, TArray<FPendingSpawn>& OutSpawns);

	void SpawnEnemy(const FPendingSpawn& Spawn, FWaveRun& Run);
	void UpdateAuras();
	void LoadSettings();
	void ApplyAntiAliasing() const;

	/** Pushes the frame rate cap and VSync to the engine and saves them in GameUserSettings.ini */
	void ApplyDisplaySettings();

	/** Takes a creature out of the game (and its wave's count); bDying plays its death animation instead of removing it at once */
	void RemoveEnemy(ATDEnemy* Enemy, bool bDying = false);

	/** Resets the board for a run with these rules (boons off for scripted runs) */
	void ResetForRun(const FTDRunRules& Rules, bool bBoons);

	/** Rolls a boon offer and opens the boon menu (nothing happens when no boon fits) */
	void OfferBoons();

	/** Recomputes every tower's stats after the run's boons changed */
	void RefreshTowerStats();

	/** Mastery trims of every tower of a type (after its rank changed) */
	void RefreshMasteryTrims(ETDTowerType Type);

	/** Saves the settings slot if mastery kills came in since the last save */
	void FlushMastery();

	/** Darkens the water, fog and light for the run's Abyss tier (Normal = exactly the level's own look) */
	void ApplyAbyssDepth();
	void CaptureDepthBase();

	/** Raises the map's records after a cleared wave or a win, and saves them with the settings */
	void UpdateRecords(bool bWon);
	void ResetRunStats();
	void SetupEnvironment();
	ATDHUD* GetTDHUD() const;

	UPROPERTY()
	TObjectPtr<ATDGrid> Grid;

	UPROPERTY()
	TArray<TObjectPtr<ATDEnemy>> Enemies;

	TArray<FWaveRun> ActiveWaves;

	TArray<FTDItem> Storage;
	int32 NextItemId = 1;

	int32 Pearls = TD::StartPearls;
	FTDIncome Income;
	int32 Lives = TD::StartLives;
	int32 Wave = 0;
	int32 BuildPhaseId = 0;
	int32 GameSpeed = 1;
	bool bGameOver = false;
	bool bEndless = false;

	ETDMenu Menu = ETDMenu::None;
	ETDMenu SettingsReturn = ETDMenu::Main;
	float EnemyHealthPercent = 100.0f;
	float UIScalePercent = 100.0f;
	int32 FrameRateStep = TD::DefaultFrameRateStep;
	bool bShowFps = false;
	bool bVSync = false;
	bool bSharpAA = false;
	float SfxVolume = 100.0f;
	float MusicVolume = 100.0f;
	int32 ForcedMap = -1;
	TArray<FKey> KeyBindings;
	int32 BindingsVersion = 0;
	int32 Rebinding = -1;
	int32 AutoSalvage = 0;

	// run summary (reset on a new game, saved with the game)
	TArray<int32> LeaksByType;
	float PlayTime = 0.0f;
	int32 WavesCleared = 0;
	bool bRunCheated = false;

	// best-wave records per map (kept in the settings slot)
	TArray<int32> BestWave;
	TArray<int32> BestWinLives;
	TArray<int32> BestAbyss;
	TArray<int32> BestScore;

	// new game setup and run boons
	FTDRunSetup Setup;
	ETDMenu SetupReturn = ETDMenu::Main;
	bool bBoonsEnabled = false;
	TArray<int32> BoonOffer;
	/** Fractions of pearls left over from scaled kill rewards (Swarm, Bounty Hunters) */
	float RewardCarry = 0.0f;

	/** Unique legendaries found this run */
	uint32 UniquesFound = 0;

	/** Makes the item for a drop of this rarity: a Legendary may become a unique not found yet (see TD::UniqueFromLegendaryChance) */
	FTDItem MakeDropItem(int32 ItemLevel, ETDRarity Rarity, bool bForceUnique);

	/** Mastery kills came in since the settings were last saved */
	bool bMasteryDirty = false;

	// the environment's own values, which the Abyss depth look scales from
	struct FDepthLight
	{
		TWeakObjectPtr<class ULightComponent> Light;
		float Intensity = 0.0f;
	};
	TArray<FDepthLight> DepthLights;
	TWeakObjectPtr<class USkyLightComponent> DepthSky;
	float DepthSkyIntensity = 0.0f;
	TWeakObjectPtr<class UExponentialHeightFogComponent> DepthFog;
	float DepthFogDensity = 0.0f;
	FLinearColor DepthFogColor = FLinearColor::Black;
	TWeakObjectPtr<class APostProcessVolume> DepthVolume;
	bool bDepthTintOverride = false;
	FLinearColor DepthTint = FLinearColor::White;
	bool bDepthExposureOverride = false;
	float DepthExposure = 0.0f;
	bool bDepthVignetteOverride = false;
	float DepthVignette = 0.0f;
	int32 RecordAtStart = 0;
	bool bRecordToasted = false;

	static constexpr float CuttlefishAuraRange = 2.5f;	// tiles
	static constexpr float ShockEelAuraRange = 1.5f;	// tiles
	// save slots, the run that owns the current slot, and the confirm dialog
	FTDSlotInfo SlotInfos[SaveSlotCount];
	int32 CurrentSlot = 0;
	int32 RunId = 0;
	ETDMenu SlotReturn = ETDMenu::Main;
	ETDConfirm PendingConfirm = ETDConfirm::None;
	int32 ConfirmSlot = 0;

	/** Last targeting priority chosen per tower type in this run (new towers start with it) */
	TArray<uint8> LastTargeting;

	static int32 NewRunId() { return FMath::RandRange(1, MAX_int32 - 1); }
	inline static const FString SettingsSlot = TEXT("ATDSettings");
};
