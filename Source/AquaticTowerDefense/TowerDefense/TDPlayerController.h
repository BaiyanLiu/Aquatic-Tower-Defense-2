// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "TDTypes.h"
#include "TDItems.h"
#include "TDPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ATDTower;
class ATDEnemy;
class ATDGameMode;
class ATDHUD;

/**
 *  Mouse-driven builder controls.
 *  Input actions and the mapping context are created in code at runtime (Enhanced Input), so no assets are needed.
 *
 *  1-9, 0, - select tower type, LMB place / select a placed tower or creature, RMB cancel,
 *  Esc cancel or pause menu, Space release wave, E skills tab, Q items tab, I storage, X sell, F speed, P pause menu.
 *  Camera keys are polled by the camera pawn.
 */
UCLASS()
class ATDPlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	ATDPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// ---- Selection ----

	bool HasSelectedType() const { return SelectedType != ETDTowerType::Count; }
	ETDTowerType GetSelectedType() const { return SelectedType; }
	ATDTower* GetSelectedTower() const { return SelectedTower.Get(); }

	void SelectTowerType(ETDTowerType Type);
	void SelectPlacedTower(ATDTower* Tower);
	void ClearSelection();

	/** Creature under the mouse (for the details panel) and the one clicked last */
	ATDEnemy* GetHoveredEnemy() const { return HoveredEnemy.Get(); }
	ATDEnemy* GetSelectedEnemy() const { return SelectedEnemy.Get(); }

	/** True while a full-screen menu is open */
	bool IsMenuOpen() const;

	/** Tile under the mouse cursor (may be out of bounds) */
	bool GetHoveredCell(FIntPoint& OutCell) const;

	/** Point on the seabed under the mouse cursor */
	bool GetMouseGroundPoint(FVector& OutPoint) const;

	/** Mouse position in viewport pixels (the simulated cursor while a UI test runs) */
	bool GetCursorPosition(FVector2D& OutPosition) const;

	/** True while the left button is held (real or simulated) */
	bool IsLeftMouseHeld() const;

	// ---- Input simulation for the scripted UI test (same handlers as real input) ----

	void SimSetCursor(const FVector2D& Position) { SimCursor = Position; }
	void SimClearCursor() { SimCursor.Reset(); bSimLeftDown = false; }
	void SimLeftPress();
	void SimLeftRelease();
	void SimRightClick();
	void SimMenuKey() { OnMenuKey(FInputActionValue()); }
	void SimStorageKey() { OnStorage(FInputActionValue()); }
	bool IsSimulatingInput() const { return SimCursor.IsSet(); }

	/** A UI test, simulation, benchmark or tour is running (its games don't count for records) */
	bool IsRunningDevTool() const { return DevTask.IsValid(); }

	// ---- Tower panel / storage UI state ----

	/** 0 = Stats, 1 = Items, 2 = Skills */
	int32 GetPanelTab() const { return PanelTab; }
	/** Info key held (Alt by default, rebindable): show every tower's range and level */
	bool IsAltHeld() const;

	void SetPanelTab(int32 Tab) { PanelTab = FMath::Clamp(Tab, 0, 2); }
	bool IsStorageOpen() const { return bStorageOpen; }
	void ToggleStorage() { bStorageOpen = !bStorageOpen; }

	// ---- Commands (also used by HUD buttons) ----

	void ToggleSkillsTab();
	void ToggleItemsTab();

	/** Asks to sell the selected tower: arms the Sell button's own confirmation (walls sell at once). Pressed again while armed, sells.
	 *  bFromClick: armed by clicking the button, so moving the mouse off it cancels. */
	void SellSelected(bool bFromClick = false);

	/** Tower whose Sell button is armed, or null. It disarms after SellConfirmWindow seconds or when the selection changes. */
	ATDTower* GetPendingSell() const { return IsSellConfirmOpen() ? PendingSell.Get() : nullptr; }
	bool IsSellConfirmOpen() const
	{
		return PendingSell.IsValid() && PendingSell == SelectedTower && FPlatformTime::Seconds() - PendingSellTime <= SellConfirmWindow;
	}
	bool IsSellArmedByClick() const { return bSellArmedByClick; }
	void ConfirmSell();
	void CancelSell() { PendingSell = nullptr; }
	void LearnSkill(int32 SkillIndex);

	/** Specialisation buttons: the first click arms a form, a second click on it within a few seconds picks it (it's permanent) */
	void ClickForm(int32 Form);
	/** Form armed on the selected tower and waiting for the second click, or 0 */
	int32 GetArmedForm() const;
	void EquipStorageItem(int32 StorageIndex);
	void UnequipSlot(ETDItemSlot Slot);
	void SalvageStorageItem(int32 StorageIndex);

	/** Storage item picked for the workbench, or -1 */
	int32 GetWorkbenchIndex() const;
	void EquipWorkbenchItem();
	void UpgradeWorkbenchItem();
	void RerollWorkbenchItem();
	void SalvageWorkbenchItem();
	void ToggleLockWorkbenchItem();
	void SortStorage();
	void TogglePause();
	void CycleSpeed();
	void CallWave();

	/** Steps the selected tower's targeting priority (1 next, -1 previous) */
	void CycleTargeting(int32 Step);

	// ---- Dev console commands (no-ops in shipping builds) ----

	/** Console cheat: builds a sample maze and releases waves. Optionally takes a screenshot after a delay. */
	UFUNCTION(Exec)
	void ATDDemo(int32 Waves, float ScreenshotDelay);

	/** Dev: offers three boons now (marks the run as not counting for records); ATDBoons 1 makes one card cursed */
	UFUNCTION(Exec)
	void ATDBoons(int32 ForceCursed = 0);

	/** Dev: takes the last boon on offer (the cursed one after ATDBoons 1); ATDPickBoon N takes boon N (ETDBoon) instead */
	UFUNCTION(Exec)
	void ATDPickBoon(int32 Boon = -1);

	/** Dev: puts every unique legendary into storage */
	UFUNCTION(Exec)
	void ATDUniques();

	/** Dev: shows the wave-clear card (for screenshots) */
	UFUNCTION(Exec)
	void ATDWaveCard();

	/** Dev: ends the run at once, showing the game over card */
	UFUNCTION(Exec)
	void ATDGameOver();

	/** Dev: starts a plain game at an Abyss tier (0-5), for the depth look */
	UFUNCTION(Exec)
	void ATDAbyss(int32 Tier);

	/** Dev: jumps to a wave (100+ turns endless mode on), optionally releasing the next one, for HUD checks */
	UFUNCTION(Exec)
	void ATDSetWave(int32 Wave, int32 bRelease = 0);

	/** Dev: sets a tower type's lifetime kills (mastery trim and the Mastery page) */
	UFUNCTION(Exec)
	void ATDMastery(int32 TowerType, int32 Kills);

	/** Console helper: opens a menu (0 none, 1 main, 2 pause, 3 settings, 4 controls, 5 victory, 6 new game setup, 7 boon offer, 8 mastery) */
	UFUNCTION(Exec)
	void ATDMenu(int32 Menu);

	/** Console helper: takes a screenshot after a delay (real seconds, works while paused) */
	UFUNCTION(Exec)
	void ATDShot(float Delay);

	/** Console helper: selects a tower type and holds the cursor over a tile (shows the placement ghost) */
	UFUNCTION(Exec)
	void ATDPreview(int32 TowerType, int32 X, int32 Y);

	/** Console helper: drives every HUD path through simulated clicks and logs PASS / FAIL */
	UFUNCTION(Exec)
	void ATDUITest();

	/** Console helper: acts as if Alt were held (1) or not (0), for screenshots */
	UFUNCTION(Exec)
	void ATDHoldAlt(int32 bHold);

	/** Console helper: starts a new game on a map and keeps using it for new games (instead of random maps) */
	UFUNCTION(Exec)
	void ATDMap(int32 Map);

	/** Console helper: selects the tower on a tile */
	UFUNCTION(Exec)
	void ATDSelect(int32 X, int32 Y);

	/** Console helper: switches the tower panel tab (0 stats, 1 items, 2 skills) */
	UFUNCTION(Exec)
	void ATDTab(int32 Tab);

	/** Dev: puts the simulated cursor at a fraction of the viewport (for tooltip screenshots); negative clears it */
	UFUNCTION(Exec)
	void ATDCursor(float FractionX, float FractionY);

	/** Dev: arms the Sell button of the selected tower (or the first tower that levels) */
	UFUNCTION(Exec)
	void ATDSellCard();

	/** Console helper: writes a contact sheet of all icons and the Windows app icon to Saved/Icons */
	UFUNCTION(Exec)
	void ATDExportIcons();

	/** Console helper: plays a scripted game and logs balance and performance numbers.
	 *  Strategy 0 = unprepared (no maze), 1 = adaptive maze on the normal economy (any map), 2 = stress test from wave 25,
	 *  3 = the old fixed maze plan (map 0 only). Plays map 0 unless -ATDMap=N is on the command line. */
	UFUNCTION(Exec)
	void ATDSim(int32 Strategy, int32 Waves, float TimeScale);

	/** Console helper: builds the maze, sends Count slow, tough creatures through it and logs frame times uncapped */
	UFUNCTION(Exec)
	void ATDPerf(int32 Count);

	/** Console helper: screenshots every panel and menu, then quits */
	UFUNCTION(Exec)
	void ATDTour();

	/** Dev: screenshots models First to Last (ETDModel order; -1 for all), each as built now and with the detailed shapes side by side,
	 *  PairsPerPage at a time, into Saved/Gallery. Quits afterwards with -ATDGalleryQuit. */
	UFUNCTION(Exec)
	void ATDGallery(int32 PairsPerPage = 8, int32 First = -1, int32 Last = -1);

protected:

	bool bDevHoldAlt = false;

	/** Id of the storage item picked for the workbench (0 = none); ids survive sorting */
	int32 WorkbenchItemId = 0;
	double LastStorageClickTime = -1.0;

	void OnSelectSlot(const FInputActionValue& Value);
	void OnClickStarted(const FInputActionValue& Value);
	void OnClickCompleted(const FInputActionValue& Value);
	void OnCancel(const FInputActionValue& Value);
	void OnWave(const FInputActionValue& Value);
	void OnSkills(const FInputActionValue& Value);
	void OnItems(const FInputActionValue& Value);
	void OnTargeting(const FInputActionValue& Value);
	void OnStorage(const FInputActionValue& Value);
	void OnSell(const FInputActionValue& Value);
	void OnSpeed(const FInputActionValue& Value);
	void OnPause(const FInputActionValue& Value);
	void OnMenuKey(const FInputActionValue& Value);
	void OnZoom(const FInputActionValue& Value);

	/** Range indicators: placement ghost or selected tower, plus every tower with Alt / "show all ranges" */
	void UpdateRangeIndicators(class ATDGameMode* GameMode, class ATDGrid* Grid);


	ATDGameMode* GetTDGameMode() const;
	ATDHUD* GetTDHUD() const;

	UInputAction* MakeAction(const TCHAR* Name, bool bAxis = false);

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Actions with rebindable keys (by ETDBind) and the keys currently mapped to them */
	UPROPERTY()
	TArray<TObjectPtr<UInputAction>> BindActions;
	TArray<FKey> MappedKeys;
	int32 AppliedBindingsVersion = -1;

	/** Remaps rebindable actions after the bindings change, and captures a key for the controls menu */
	void UpdateKeyBindings();

	/** The tower keys' shared action and the keys mapped to it */
	UPROPERTY()
	TObjectPtr<UInputAction> SelectAction;
	TArray<FKey> SelectKeys;

	/** A tower hotkey plus its numpad twin, if it has one */
	static TArray<FKey, TInlineAllocator<2>> TowerKeys(const FKey& Key);

	UPROPERTY()
	TArray<TObjectPtr<UInputAction>> Actions;

	ETDTowerType SelectedType = ETDTowerType::Count;
	TWeakObjectPtr<ATDTower> SelectedTower;
	TWeakObjectPtr<ATDTower> PendingSell;
	double PendingSellTime = -100.0;
	bool bSellArmedByClick = false;
	static constexpr double SellConfirmWindow = 3.0;

	TWeakObjectPtr<ATDTower> ArmedFormTower;
	int32 ArmedForm = 0;
	double ArmedFormTime = -100.0;
	static constexpr double FormConfirmWindow = 4.0;
	TWeakObjectPtr<ATDEnemy> HoveredEnemy;
	TWeakObjectPtr<ATDEnemy> SelectedEnemy;

	/** Finds the creature closest to the mouse on screen */
	ATDEnemy* PickEnemyAtMouse() const;

	int32 PanelTab = 0;
	bool bStorageOpen = false;

	float PendingShotTime = 0.0f;

	TOptional<FVector2D> SimCursor;
	bool bSimLeftDown = false;

	/** Running dev tool (UI test or simulation), ticked from PlayerTick (also while paused) */
	TSharedPtr<class FTDDevTask> DevTask;

};
