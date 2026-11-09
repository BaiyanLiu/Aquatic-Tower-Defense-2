// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDDevTools.h"
#include "TDPlayerController.h"
#include "TDGameMode.h"
#include "TDGrid.h"
#include "TDTower.h"
#include "TDEnemy.h"
#include "TDHUD.h"
#include "TDPickup.h"
#include "TDIcons.h"
#include "TDRun.h"
#include "TDSaveGame.h"
#include "AquaticTowerDefense.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "GameFramework/GameUserSettings.h"
#include "RHIStats.h"
#include "DynamicRHI.h"
#include "RenderTimer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

// ---------------------------------------------------------------------------------------------
// Console commands (on the player controller)
// ---------------------------------------------------------------------------------------------

#if !UE_BUILD_SHIPPING
namespace
{
	void UseFirstMap(ATDGameMode* GameMode);
}
#endif

void ATDPlayerController::ATDUITest()
{
#if !UE_BUILD_SHIPPING
	UseFirstMap(GetTDGameMode());
	DevTask = MakeShared<FTDUITest>(this);
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("UITEST started"));
#endif
}

void ATDPlayerController::ATDHoldAlt(int32 bHold)
{
#if !UE_BUILD_SHIPPING
	bDevHoldAlt = bHold != 0;
#endif
}

void ATDPlayerController::ATDMap(int32 Map)
{
#if !UE_BUILD_SHIPPING
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->ForceMap(Map);
		GameMode->StartNewGame();
	}
#endif
}

#if !UE_BUILD_SHIPPING
namespace
{
	/** Scripted tools expect the first map's layout */
	void UseFirstMap(ATDGameMode* GameMode)
	{
		if (!GameMode)
		{
			return;
		}
		GameMode->ForceMap(0);
		if (TD::GetMap() != 0 && GameMode->GetGrid())
		{
			TD::SetMap(0);
			GameMode->GetGrid()->ResetBoard();
		}
	}
}
#endif

void ATDPlayerController::ATDSelect(int32 X, int32 Y)
{
#if !UE_BUILD_SHIPPING
	const ATDGameMode* GameMode = GetTDGameMode();
	if (ATDTower* Tower = (GameMode && GameMode->GetGrid()) ? GameMode->GetGrid()->GetTowerAt(FIntPoint(X, Y)) : nullptr)
	{
		SelectPlacedTower(Tower);
	}
#endif
}

void ATDPlayerController::ATDTab(int32 Tab)
{
#if !UE_BUILD_SHIPPING
	SetPanelTab(Tab);
#endif
}

void ATDPlayerController::ATDCursor(float FractionX, float FractionY)
{
#if !UE_BUILD_SHIPPING
	int32 SizeX = 0, SizeY = 0;
	GetViewportSize(SizeX, SizeY);
	if (FractionX < 0.0f || FractionY < 0.0f)
	{
		SimSetCursor(FVector2D(5.0f, 300.0f));
		return;
	}
	SimSetCursor(FVector2D(FractionX * SizeX, FractionY * SizeY));
#endif
}

void ATDPlayerController::ATDSellCard()
{
#if !UE_BUILD_SHIPPING
	// with nothing selected, picks the first tower that levels (walls sell at once, without arming the button)
	if (!SelectedTower.IsValid())
	{
		const ATDGameMode* GameMode = GetTDGameMode();
		if (const ATDGrid* Grid = GameMode ? GameMode->GetGrid() : nullptr)
		{
			for (ATDTower* Tower : Grid->GetTowers())
			{
				if (Tower && Tower->CanProgress())
				{
					SelectPlacedTower(Tower);
					break;
				}
			}
		}
	}
	if (SelectedTower.IsValid() && SelectedTower->CanProgress() && !IsSellConfirmOpen())
	{
		SellSelected();
	}
#endif
}

void ATDPlayerController::ATDExportIcons()
{
#if !UE_BUILD_SHIPPING
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("Icons");
	const bool bOk = TDIcons::Export(Directory) && TDIcons::ExportCompare(Directory / TEXT("Compare"));
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("ICONS exported to %s: %s"), *Directory, bOk ? TEXT("ok") : TEXT("failed"));
#endif
}

void ATDPlayerController::ATDSim(int32 Strategy, int32 Waves, float TimeScale)
{
#if !UE_BUILD_SHIPPING
	// -ATDMap=N on the command line picks the map (the game mode already forces it); otherwise map 0
	int32 CommandLineMap = -1;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ATDMap="), CommandLineMap))
	{
		UseFirstMap(GetTDGameMode());
	}
	DevTask = MakeShared<FTDSim>(this, Strategy, Waves, TimeScale);
#endif
}

void ATDPlayerController::ATDPerf(int32 Count)
{
#if !UE_BUILD_SHIPPING
	UseFirstMap(GetTDGameMode());
	DevTask = MakeShared<FTDPerf>(this, Count);
#endif
}

void ATDPlayerController::ATDTour()
{
#if !UE_BUILD_SHIPPING
	DevTask = MakeShared<FTDTour>(this);
#endif
}

#if !UE_BUILD_SHIPPING

// ---------------------------------------------------------------------------------------------
// Screenshot tour
// ---------------------------------------------------------------------------------------------

bool FTDTour::Tick(float RealDeltaTime)
{
	// each entry runs, then the next one follows a second and a half later
	static const TCHAR* const Commands[] = {
		TEXT("ATDDemo 8 0"), TEXT("Shot showui"),						// tower stats with storage
		TEXT("ATDTab 1"), TEXT("Shot showui"),							// items
		TEXT("ATDTab 2"), TEXT("Shot showui"),							// skills
		TEXT("ATDPreview 3 10 10"), TEXT("Shot showui"),				// tower about to be built
		TEXT("ATDMenu 1"), TEXT("Shot showui"),
		TEXT("ATDMenu 2"), TEXT("Shot showui"),
		TEXT("ATDMenu 3"), TEXT("Shot showui"),
		TEXT("ATDMenu 4"), TEXT("Shot showui"),
		// with -ATDTourFull: the victory menu, a dock tooltip and the armed Sell button too
		TEXT("ATDMenu 5"), TEXT("Shot showui"),
		TEXT("ATDMenu 0"), TEXT("ATDCursor 0.17 0.93"), TEXT("Shot showui"),	// tooltip on a dock card
		TEXT("ATDCursor -1 -1"), TEXT("ATDSellCard"), TEXT("Shot showui"),
		TEXT("ATDMenu 6"), TEXT("Shot showui"),							// new game setup
		TEXT("ATDBoons 1"), TEXT("Shot showui"),						// boon offer with a cursed card (left open: it waits for a pick)
	};
	// -ATDTourRun: only the run screens (setup, run strip with boons and mutators, mastery, boon offer)
	static const TCHAR* const RunCommands[] = {
		TEXT("ATDMenu 6"), TEXT("Shot showui"),
		TEXT("ATDAbyss 4"), TEXT("ATDBoons 0"), TEXT("ATDPickBoon"), TEXT("ATDBoons 1"), TEXT("ATDPickBoon"), TEXT("ATDBoons 0"), TEXT("ATDPickBoon"),
		TEXT("ATDMenu 0"), TEXT("Shot showui"),
		TEXT("ATDMastery 0 30"), TEXT("ATDMastery 1 900"), TEXT("ATDMastery 2 4200"), TEXT("ATDMastery 3 99999"), TEXT("ATDMenu 8"), TEXT("Shot showui"),
		TEXT("ATDMenu 0"), TEXT("ATDBoons 1"), TEXT("Shot showui"),
	};
	const bool bRunOnly = FParse::Param(FCommandLine::Get(), TEXT("ATDTourRun"));
	const TCHAR* const* List = bRunOnly ? RunCommands : Commands;
	const int32 CommandCount = bRunOnly ? UE_ARRAY_COUNT(RunCommands) : (FParse::Param(FCommandLine::Get(), TEXT("ATDTourFull")) ? UE_ARRAY_COUNT(Commands) : 16);

	ATDPlayerController* P = PC.Get();
	if (!P)
	{
		return false;
	}
	Timer -= RealDeltaTime;
	if (Timer > 0.0f)
	{
		return true;
	}
	Timer = 1.5f;
	if (Index == 0)
	{
		P->SimSetCursor(FVector2D(5.0f, 300.0f));	// away from the buttons, so no tooltip covers the shots
		// -ATDTourScale=130: shoot at another interface size (not saved: the settings only save from the menu)
		float Scale = 0.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("ATDTourScale="), Scale) && Scale > 0.0f)
		{
			if (ATDGameMode* GameMode = P->GetWorld() ? P->GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr)
			{
				GameMode->SetUIScalePercent(Scale);
			}
		}
	}
	if (Index >= CommandCount)
	{
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("TOUR DONE"));
		P->ConsoleCommand(TEXT("Quit"));
		return false;
	}
	P->ConsoleCommand(List[Index++]);
	return true;
}

namespace
{
	ATDGameMode* GM(ATDPlayerController* PC)
	{
		return PC ? PC->GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
	}

	ATDHUD* HUDOf(ATDPlayerController* PC)
	{
		return PC ? Cast<ATDHUD>(PC->GetHUD()) : nullptr;
	}

	FVector2D CellToScreen(ATDPlayerController* PC, const FIntPoint& Cell)
	{
		FVector2D Screen(-1, -1);
		PC->ProjectWorldLocationToScreen(TD::CellToWorld(Cell), Screen);
		return Screen;
	}
}

// ---------------------------------------------------------------------------------------------
// UI test
// ---------------------------------------------------------------------------------------------

FTDUITest::FTDUITest(ATDPlayerController* InPC)
	: PC(InPC)
{
	BuildScript();
}

void FTDUITest::Do(const FString& Name, TFunction<void()> Action)
{
	Steps.Add({ EKind::Do, Name, [Action]() { Action(); return true; }, 0.0f });
}

void FTDUITest::Check(const FString& Name, TFunction<bool()> Condition)
{
	Steps.Add({ EKind::Check, Name, MoveTemp(Condition), 0.0f });
}

void FTDUITest::WaitFor(const FString& Name, TFunction<bool()> Condition, float Timeout)
{
	Steps.Add({ EKind::WaitFor, Name, MoveTemp(Condition), Timeout });
}

void FTDUITest::Wait(float Seconds)
{
	Steps.Add({ EKind::Wait, TEXT("wait"), nullptr, Seconds });
}

void FTDUITest::Report(const FString& Name, bool bPassed, const FString& Extra)
{
	if (bPassed)
	{
		++Passed;
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("UITEST PASS: %s"), *Name);
	}
	else
	{
		++Failed;
		Failures.Add(Name);
		UE_LOG(LogAquaticTowerDefense, Error, TEXT("UITEST FAIL: %s %s"), *Name, *Extra);
	}
}

bool FTDUITest::Tick(float RealDeltaTime)
{
	ATDPlayerController* P = PC.Get();
	if (!P)
	{
		return false;
	}

	if (FramesToSkip > 0)
	{
		--FramesToSkip;
		return true;
	}

	if (!Steps.IsValidIndex(Index))
	{
		P->SimClearCursor();
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("UITEST SUMMARY: %d passed, %d failed%s%s"), Passed, Failed,
			Failures.Num() > 0 ? TEXT(" -> ") : TEXT(""), *FString::Join(Failures, TEXT(", ")));
		return false;
	}

	FStep& Step = Steps[Index];
	StepTime += RealDeltaTime;

	switch (Step.Kind)
	{
	case EKind::Do:
		Step.Fn();
		FramesToSkip = 2;	// let the HUD redraw with the new state
		break;
	case EKind::Check:
		Report(Step.Name, Step.Fn());
		break;
	case EKind::WaitFor:
		if (!Step.Fn())
		{
			if (StepTime < Step.Timeout)
			{
				return true;
			}
			Report(Step.Name, false, FString::Printf(TEXT("(timed out after %.0fs)"), Step.Timeout));
		}
		else
		{
			Report(Step.Name, true);
		}
		break;
	case EKind::Wait:
		if (StepTime < Step.Timeout)
		{
			return true;
		}
		break;
	}

	++Index;
	StepTime = 0.0f;
	return true;
}

void FTDUITest::BuildScript()
{
	using EB = ATDHUD::EButton;
	TWeakObjectPtr<ATDPlayerController> WeakPC = PC;
	auto P = [WeakPC]() { return WeakPC.Get(); };
	auto G = [WeakPC]() { return GM(WeakPC.Get()); };
	auto Grid = [WeakPC]() { ATDGameMode* M = GM(WeakPC.Get()); return M ? M->GetGrid() : nullptr; };

	// moves the cursor to a HUD button and clicks it (fails the next check if the button isn't drawn)
	auto Click = [WeakPC](EB Id, int32 Param)
	{
		ATDPlayerController* C = WeakPC.Get();
		FVector2D Center;
		if (C && HUDOf(C) && HUDOf(C)->FindButton(Id, Param, Center))
		{
			C->SimSetCursor(Center);
			C->SimLeftPress();
			C->SimLeftRelease();
		}
		else
		{
			UE_LOG(LogAquaticTowerDefense, Warning, TEXT("UITEST: button %d/%d not on screen"), (int32)Id, Param);
		}
	};
	auto RightClick = [WeakPC](EB Id, int32 Param)
	{
		ATDPlayerController* C = WeakPC.Get();
		FVector2D Center;
		if (C && HUDOf(C) && HUDOf(C)->FindButton(Id, Param, Center))
		{
			C->SimSetCursor(Center);
			C->SimRightClick();
		}
	};
	auto Hover = [WeakPC](EB Id, int32 Param)
	{
		ATDPlayerController* C = WeakPC.Get();
		FVector2D Center;
		if (C && HUDOf(C) && HUDOf(C)->FindButton(Id, Param, Center))
		{
			C->SimSetCursor(Center);
		}
	};
	auto ClickCell = [WeakPC](const FIntPoint& Cell)
	{
		ATDPlayerController* C = WeakPC.Get();
		C->SimSetCursor(CellToScreen(C, Cell));
		C->SimLeftPress();
		C->SimLeftRelease();
	};
	auto TowerAt = [Grid](const FIntPoint& Cell) { return Grid() ? Grid()->GetTowerAt(Cell) : nullptr; };
	auto TowerCount = [Grid]() { return Grid() ? Grid()->GetTowers().Num() : -1; };
	auto ButtonCenter = [WeakPC](EB Id, int32 Param, FVector2D& Out) { return HUDOf(WeakPC.Get()) && HUDOf(WeakPC.Get())->FindButton(Id, Param, Out); };

	// the player's way into a game: setup screen, Start, then the boon offer before wave 1
	auto PassSetup = [this, Click, G](const FString& What)
	{
		Check(What + TEXT(": setup screen opens"), [G]() { return G()->GetMenu() == ETDMenu::Setup; });
		// the script's runs always use save slot 2 (slot 1 may hold the player's run)
		Do(What + TEXT(": pick save slot 2"), [Click]() { Click(EB::SetupSlot, 1); });
		Do(What + TEXT(": click Start"), [Click]() { Click(EB::SetupStart, 0); });
		Do(What + TEXT(": replace the slot's run if asked"), [Click, G]() { if (G()->GetPendingConfirm() == ETDConfirm::OverwriteForNewGame) { Click(EB::ConfirmYes, 0); } });
		Check(What + TEXT(": boon offer before wave 1"), [G]() { return G()->GetMenu() == ETDMenu::Boon && G()->GetBoonOffer().Num() == TD::BoonOfferSize && G()->GetWave() == 0; });
		// (Kraken's Pact would open a second offer, so the script takes another card)
		Do(What + TEXT(": take the first boon"), [Click, G]()
		{
			for (const int32 Boon : G()->GetBoonOffer())
			{
				if (Boon != (int32)ETDBoon::KrakensPact)
				{
					Click(EB::PickBoon, Boon);
					return;
				}
			}
		});
		Check(What + TEXT(": boon taken, game on"), [G]() { return G()->GetMenu() == ETDMenu::None && !G()->HasBoonOffer() && G()->GetRunRules().Boons != 0; });
		Do(What + TEXT(": plain rules again for the scripted checks"), [G]() { TD::SetRun(FTDRunRules()); G()->DebugSetLives(TD::StartLives); });
	};

	// ---------------- main menu and settings ----------------
	Do(TEXT("open main menu"), [G]() { G()->OpenMenu(ETDMenu::Main); });
	Check(TEXT("main menu is open"), [G]() { return G()->GetMenu() == ETDMenu::Main; });
	Do(TEXT("click Settings"), [Click]() { Click(EB::MenuSettings, 0); });
	Check(TEXT("settings menu opens"), [G]() { return G()->GetMenu() == ETDMenu::Settings; });

	// drag the effects volume slider from the middle to the end
	Do(TEXT("press effects volume slider"), [WeakPC, ButtonCenter]()
	{
		FVector2D Center;
		if (ButtonCenter(EB::SettingSlider, 3, Center))
		{
			WeakPC->SimSetCursor(Center);	// pressed in the middle (starts the drag)
			WeakPC->SimLeftPress();
		}
	});
	for (int32 i = 1; i <= 6; ++i)
	{
		Do(FString::Printf(TEXT("drag slider %d"), i), [WeakPC, i]()
		{
			FVector2D Cursor;
			if (WeakPC->GetCursorPosition(Cursor))
			{
				WeakPC->SimSetCursor(Cursor + FVector2D(18.0f * i, 0.0f));
			}
		});
	}
	Do(TEXT("release slider"), [WeakPC]() { WeakPC->SimLeftRelease(); });
	Check(TEXT("slider drag raised effects volume"), [G]() { return G()->GetSfxVolume() > 80.0f; });
	Do(TEXT("click effects volume -"), [Click]() { Click(EB::SettingAdjust, 6); });
	Check(TEXT("effects volume - lowers it"), [G]() { return G()->GetSfxVolume() < 100.0f; });
	Do(TEXT("restore effects volume"), [G]() { G()->SetSfxVolume(100.0f); });
	Check(TEXT("no enemy health or reduce motion settings"), [G]() { return G()->GetEnemyHealthPercent() == 100.0f; });
	Do(TEXT("click interface +"), [Click]() { Click(EB::SettingAdjust, 3); });
	Check(TEXT("interface + raises size"), [G]() { return G()->GetUIScalePercent() > 100.0f; });
	Do(TEXT("click interface -"), [Click]() { Click(EB::SettingAdjust, 2); });
	Check(TEXT("interface - restores size"), [G]() { return FMath::IsNearlyEqual(G()->GetUIScalePercent(), 100.0f); });
	// frame rate cap slider and the FPS / VSync toggles
	Do(TEXT("press frame rate slider"), [WeakPC, ButtonCenter]()
	{
		FVector2D Center;
		if (ButtonCenter(EB::SettingSlider, 2, Center))
		{
			WeakPC->SimSetCursor(Center);
			WeakPC->SimLeftPress();
		}
	});
	for (int32 i = 1; i <= 6; ++i)
	{
		Do(FString::Printf(TEXT("drag frame rate slider %d"), i), [WeakPC, i]()
		{
			FVector2D Cursor;
			if (WeakPC->GetCursorPosition(Cursor))
			{
				WeakPC->SimSetCursor(Cursor + FVector2D(20.0f * i, 0.0f));
			}
		});
	}
	Do(TEXT("release frame rate slider"), [WeakPC]() { WeakPC->SimLeftRelease(); });
	Check(TEXT("dragging to the end means unlimited"), [G]() { return G()->GetFrameRateLimit() == 0 && GEngine->GetGameUserSettings()->GetFrameRateLimit() == 0.0f; });
	Do(TEXT("click frame rate -"), [Click]() { Click(EB::SettingAdjust, 4); });
	Check(TEXT("frame rate limit 240 applied to the engine"), [G]() { return G()->GetFrameRateLimit() == 240 && FMath::IsNearlyEqual(GEngine->GetGameUserSettings()->GetFrameRateLimit(), 240.0f); });
	Do(TEXT("click Show FPS"), [Click]() { Click(EB::ToggleShowFps, 0); });
	Check(TEXT("FPS counter on"), [G]() { return G()->IsShowingFps(); });
	Do(TEXT("click Show FPS again"), [Click]() { Click(EB::ToggleShowFps, 0); });
	Check(TEXT("FPS counter off"), [G]() { return !G()->IsShowingFps(); });
	Do(TEXT("click VSync"), [Click]() { Click(EB::ToggleVSync, 0); });
	Check(TEXT("VSync on"), [G]() { return G()->IsVSyncEnabled() && GEngine->GetGameUserSettings()->IsVSyncEnabled(); });
	Do(TEXT("click VSync again"), [Click]() { Click(EB::ToggleVSync, 0); });
	Check(TEXT("VSync off"), [G]() { return !G()->IsVSyncEnabled(); });
	Do(TEXT("restore 60 FPS"), [G]() { G()->SetFrameRateStep(TD::DefaultFrameRateStep); });
	Do(TEXT("click Anti-aliasing"), [Click]() { Click(EB::ToggleAA, 0); });
	Check(TEXT("sharp anti-aliasing on"), [G]() { return G()->IsSharpAA(); });
	Do(TEXT("click Anti-aliasing again"), [Click]() { Click(EB::ToggleAA, 0); });
	Check(TEXT("smooth anti-aliasing back"), [G]() { return !G()->IsSharpAA(); });
	Do(TEXT("clear a wave"), [G]() { G()->ShowWaveClearCard(3, 7, 0, 4, 0); });
	Check(TEXT("the wave-clear card shows"), [WeakPC]() { return HUDOf(WeakPC.Get())->IsWaveClearCardVisible(); });

	Do(TEXT("click Controls"), [Click]() { Click(EB::MenuControls, 0); });
	Check(TEXT("controls menu open"), [G]() { return G()->GetMenu() == ETDMenu::Controls; });
	Do(TEXT("click the wave key"), [Click]() { Click(EB::BindKey, 0); });
	Check(TEXT("waiting for a key"), [G]() { return G()->GetRebinding() == 0; });
	Do(TEXT("bind the wave to the storage key"), [G]() { G()->SetKey(ETDBind::Wave, EKeys::I); G()->StopRebinding(); });
	Check(TEXT("keys swapped"), [G]() { return G()->GetKey(ETDBind::Wave) == EKeys::I && G()->GetKey(ETDBind::Storage) == EKeys::SpaceBar; });
	Do(TEXT("click Reset to defaults"), [Click]() { Click(EB::ResetKeys, 0); });
	Check(TEXT("default keys back"), [G]() { return G()->GetKey(ETDBind::Wave) == EKeys::SpaceBar && G()->GetKey(ETDBind::Storage) == EKeys::I; });
	Do(TEXT("bind the Harpoon to the Alt key"), [G]() { G()->SetKey(TDBind::TowerBind(ETDTowerType::Harpoon), EKeys::LeftAlt); });
	Check(TEXT("Harpoon and info keys swapped"), [G]() { return G()->GetKey(TDBind::TowerBind(ETDTowerType::Harpoon)) == EKeys::LeftAlt && G()->GetKey(ETDBind::Info) == EKeys::Two; });
	Do(TEXT("reset keys again"), [Click]() { Click(EB::ResetKeys, 0); });
	Check(TEXT("tower and info keys back"), [G]() { return G()->GetKey(TDBind::TowerBind(ETDTowerType::Harpoon)) == EKeys::Two && G()->GetKey(ETDBind::Info) == EKeys::LeftAlt; });
	Do(TEXT("click Back in controls"), [Click]() { Click(EB::ControlsBack, 0); });
	Check(TEXT("back to settings"), [G]() { return G()->GetMenu() == ETDMenu::Settings; });

	Do(TEXT("click Back"), [Click]() { Click(EB::SettingsBack, 0); });
	Check(TEXT("back returns to the main menu"), [G]() { return G()->GetMenu() == ETDMenu::Main; });
	Do(TEXT("click Tower mastery"), [Click]() { Click(EB::MenuMastery, 0); });
	Check(TEXT("mastery page opens"), [G]() { return G()->GetMenu() == ETDMenu::Mastery; });
	Do(TEXT("mastery: click Back"), [Click]() { Click(EB::MasteryBack, 0); });
	Check(TEXT("mastery Back returns to the main menu"), [G]() { return G()->GetMenu() == ETDMenu::Main; });
	Do(TEXT("click Continue"), [Click]() { Click(EB::MenuLoad, 0); });
	Check(TEXT("Continue opens the slot picker (when there is a save)"), [G]() { return !G()->HasAnySave() || G()->GetMenu() == ETDMenu::Slots; });
	Do(TEXT("slot picker: click Back"), [Click, G]() { if (G()->GetMenu() == ETDMenu::Slots) { Click(EB::SlotBack, 0); } });
	Check(TEXT("slot picker Back returns to the main menu"), [G]() { return G()->GetMenu() == ETDMenu::Main; });
	Do(TEXT("click Start new game"), [Click]() { Click(EB::MenuNewGame, 0); });
	Check(TEXT("new game opens the setup screen"), [G]() { return G()->GetMenu() == ETDMenu::Setup; });
	Do(TEXT("setup: click Back"), [Click]() { Click(EB::SetupBack, 0); });
	Check(TEXT("setup Back returns to the main menu"), [G]() { return G()->GetMenu() == ETDMenu::Main; });
	Do(TEXT("click Start new game again"), [Click]() { Click(EB::MenuNewGame, 0); });
	Do(TEXT("setup: pick the third map"), [Click]() { Click(EB::SetupMap, 2); });
	Check(TEXT("setup: map picked"), [G]() { return G()->GetSetup().Map == 2; });
	Do(TEXT("setup: Glass Reef on"), [Click]() { Click(EB::SetupMutator, (int32)ETDMutator::GlassReef); });
	Check(TEXT("setup: Glass Reef adds 100% score"), [G]() { return FMath::IsNearlyEqual(G()->GetSetupMultiplier(), TD::ScoreMultiplier(G()->GetSetup().Tier, 0) + 1.0f); });
	Do(TEXT("setup: Glass Reef off"), [Click]() { Click(EB::SetupMutator, (int32)ETDMutator::GlassReef); });
	Do(TEXT("setup: Murky Water and Lean Reef on"), [Click]() { Click(EB::SetupMutator, (int32)ETDMutator::MurkyWater); Click(EB::SetupMutator, (int32)ETDMutator::LeanReef); });
	Check(TEXT("setup: the new toggles add 25% and 30%"), [G]() { return FMath::IsNearlyEqual(G()->GetSetupMultiplier(), TD::ScoreMultiplier(G()->GetSetup().Tier, 0) + 0.55f); });
	Do(TEXT("setup: Murky Water and Lean Reef off"), [Click]() { Click(EB::SetupMutator, (int32)ETDMutator::MurkyWater); Click(EB::SetupMutator, (int32)ETDMutator::LeanReef); });
	Do(TEXT("setup: pick Random (the test's forced map)"), [Click]() { Click(EB::SetupMap, -1); });
	Check(TEXT("setup: Random plays at normal rules"), [G]() { return G()->GetSetup().Map == -1 && G()->GetSetup().Tier == 0 && G()->GetSetup().Mutators == 0; });
	PassSetup(TEXT("new game"));
	Check(TEXT("new game plays the forced map"), []() { return TD::GetMap() == 0; });

	// ---------------- dock, click placing and placement rules ----------------
	Do(TEXT("add test pearls"), [G]() { G()->DebugAddPearls(2000); });
	Do(TEXT("hover dock slot 5"), [Hover]() { Hover(EB::Slot, 5); });
	Check(TEXT("dock tooltip shows Mantis Shrimp"), [WeakPC]() { return HUDOf(WeakPC.Get())->GetLastTipTitle().Contains(TEXT("Mantis")); });
	Do(TEXT("click dock slot 1 (Coral Wall)"), [Click]() { Click(EB::Slot, 0); });
	Check(TEXT("wall selected"), [P]() { return P()->GetSelectedType() == ETDTowerType::Wall; });

	// dragging no longer builds a line: holding the button and moving places only the first wall
	Do(TEXT("press on (6,2)"), [WeakPC]() { WeakPC->SimSetCursor(CellToScreen(WeakPC.Get(), FIntPoint(6, 2))); WeakPC->SimLeftPress(); });
	Do(TEXT("move to (6,3) with the button held"), [WeakPC]() { WeakPC->SimSetCursor(CellToScreen(WeakPC.Get(), FIntPoint(6, 3))); });
	Do(TEXT("release"), [WeakPC]() { WeakPC->SimLeftRelease(); });
	Check(TEXT("a drag places one wall only"), [TowerAt]() { return TowerAt(FIntPoint(6, 2)) && !TowerAt(FIntPoint(6, 3)); });
	for (int32 Row = 3; Row <= 7; ++Row)
	{
		Do(FString::Printf(TEXT("click (6,%d)"), Row), [WeakPC, Row]()
		{
			WeakPC->SimSetCursor(CellToScreen(WeakPC.Get(), FIntPoint(6, Row)));
			WeakPC->SimLeftPress();
			WeakPC->SimLeftRelease();
		});
	}
	Check(TEXT("clicks placed a line of 6 walls"), [TowerAt]()
	{
		for (int32 Row = 2; Row <= 7; ++Row)
		{
			if (!TowerAt(FIntPoint(6, Row)))
			{
				return false;
			}
		}
		return true;
	});

	Do(TEXT("place a wall diagonal to the line"), [G]() { G()->TryPlaceTower(FIntPoint(7, 8), ETDTowerType::Wall, true); });
	Check(TEXT("diagonal walls link to each other"), [TowerAt]()
	{
		return TowerAt(FIntPoint(7, 8)) && TowerAt(FIntPoint(6, 7))->GetWallLinks() == (8 | 16) && TowerAt(FIntPoint(7, 8))->GetWallLinks() == 128;
	});
	Do(TEXT("remove the diagonal wall"), [Grid, TowerAt]() { Grid()->RemoveTower(TowerAt(FIntPoint(7, 8))); });
	Check(TEXT("removing a wall unlinks its diagonal neighbor"), [TowerAt]() { return !TowerAt(FIntPoint(7, 8)) && TowerAt(FIntPoint(6, 7))->GetWallLinks() == 8; });

	Do(TEXT("wall off column 9 except row 6"), [G]()
	{
		for (int32 Row = 0; Row < TD::Rows; ++Row)
		{
			if (Row != 6)
			{
				G()->TryPlaceTower(FIntPoint(9, Row), ETDTowerType::Wall, true);
			}
		}
	});
	Do(TEXT("click the last gap"), [ClickCell]() { ClickCell(FIntPoint(9, 6)); });
	Check(TEXT("blocking placement is rejected"), [TowerAt, Grid]() { return !TowerAt(FIntPoint(9, 6)) && Grid()->GetRouteLength() > 0; });
	Check(TEXT("hover preview reports the block"), [Grid]() { return Grid()->GetPreviewResult() == ETDPlaceResult::BlocksPath; });
	Do(TEXT("right click cancels"), [WeakPC]() { WeakPC->SimRightClick(); });
	Check(TEXT("selection cleared"), [P]() { return !P()->HasSelectedType(); });
	Check(TEXT("range indicator hidden with nothing selected"), [Grid]() { return Grid()->GetVisibleRangeCount() == 0 && Grid()->GetTintedTileCount() == 0; });

	// ---------------- tower panel: tabs, skills ----------------
	Do(TEXT("click dock slot 2 (Harpoon)"), [Click]() { Click(EB::Slot, 1); });
	Do(TEXT("hover (11,4) with the harpoon"), [WeakPC]() { WeakPC->SimSetCursor(CellToScreen(WeakPC.Get(), FIntPoint(11, 4))); });
	Check(TEXT("placement ghost shows its range"), [Grid]()
	{
		return Grid()->GetVisibleRangeCount() == 1 && FMath::IsNearlyEqual(Grid()->GetFirstRangeTiles(), TD::TowerRange(ETDTowerType::Harpoon, 1), 0.05f)
			&& Grid()->GetTintedTileCount() > 0;
	});
	Do(TEXT("place harpoon at (11,4)"), [ClickCell]() { ClickCell(FIntPoint(11, 4)); });
	Check(TEXT("harpoon placed"), [TowerAt]() { return TowerAt(FIntPoint(11, 4)) && TowerAt(FIntPoint(11, 4))->GetTowerType() == ETDTowerType::Harpoon; });

	// ---------------- building a tower straight onto a wall ----------------
	static int32 RouteBeforeWallBuild = 0;
	static int32 PearlsBeforeWallBuild = 0;
	static int32 WallBuildCost = 0;
	Do(TEXT("hover the wall at (6,5) with the harpoon"), [WeakPC]() { WeakPC->SimSetCursor(CellToScreen(WeakPC.Get(), FIntPoint(6, 5))); });
	Check(TEXT("preview offers building onto the wall"), [Grid]()
	{
		return Grid()->IsPreviewOnWall() && Grid()->GetPreviewResult() == ETDPlaceResult::Ok && Grid()->GetPreviewDelta() == 0;
	});
	Do(TEXT("click the wall at (6,5)"), [G, Grid, ClickCell]()
	{
		RouteBeforeWallBuild = Grid()->GetRouteLength();
		PearlsBeforeWallBuild = G()->GetPearls();
		WallBuildCost = G()->GetBuildOnWallCost(FIntPoint(6, 5), ETDTowerType::Harpoon);
		ClickCell(FIntPoint(6, 5));
	});
	Check(TEXT("harpoon replaced the wall, path unchanged"), [G, Grid, TowerAt]()
	{
		const ATDTower* T = TowerAt(FIntPoint(6, 5));
		return T && T->GetTowerType() == ETDTowerType::Harpoon && Grid()->GetRouteLength() == RouteBeforeWallBuild
			&& WallBuildCost >= 0 && G()->GetPearls() == PearlsBeforeWallBuild - WallBuildCost;
	});
	Check(TEXT("walls next to it unlink from the new tower"), [TowerAt]()
	{
		return TowerAt(FIntPoint(6, 4)) && !(TowerAt(FIntPoint(6, 4))->GetWallLinks() & 4) && TowerAt(FIntPoint(6, 6)) && !(TowerAt(FIntPoint(6, 6))->GetWallLinks() & 8);
	});
	Do(TEXT("hover a plain wall with the Coral Wall picked"), [Click, WeakPC]() { Click(EB::Slot, 0); WeakPC->SimSetCursor(CellToScreen(WeakPC.Get(), FIntPoint(6, 3))); });
	Check(TEXT("a wall can't be built onto a wall"), [Grid]() { return !Grid()->IsPreviewOnWall() && Grid()->GetPreviewResult() == ETDPlaceResult::Occupied; });
	Do(TEXT("pick the harpoon again"), [Click]() { Click(EB::Slot, 1); });
	Do(TEXT("click the harpoon"), [ClickCell]() { ClickCell(FIntPoint(11, 4)); });
	Check(TEXT("harpoon selected"), [P, TowerAt]() { return P()->GetSelectedTower() == TowerAt(FIntPoint(11, 4)); });
	Do(TEXT("move the cursor off the board"), [WeakPC]() { WeakPC->SimSetCursor(FVector2D(5.0f, 400.0f)); });
	Check(TEXT("selected tower shows its live range"), [Grid, TowerAt]()
	{
		return Grid()->GetVisibleRangeCount() == 1 && FMath::IsNearlyEqual(Grid()->GetFirstRangeTiles(), TowerAt(FIntPoint(11, 4))->GetRange(), 0.05f)
			&& Grid()->GetTintedTileCount() > 0;
	});
	Do(TEXT("click Items tab"), [Click]() { Click(EB::Tab, 1); });
	Check(TEXT("items tab shown"), [P]() { return P()->GetPanelTab() == 1; });
	Do(TEXT("click Skills tab"), [Click]() { Click(EB::Tab, 2); });
	Check(TEXT("skills tab shown"), [P]() { return P()->GetPanelTab() == 2; });
	Do(TEXT("click Learn on skill 1"), [Click]() { Click(EB::LearnSkill, 0); });
	Check(TEXT("skill rank 1 learned"), [TowerAt]() { return TowerAt(FIntPoint(11, 4))->GetSkillRank(0) == 1; });
	Check(TEXT("learn spent the skill point"), [TowerAt]() { return TowerAt(FIntPoint(11, 4))->GetSkillPoints() == 0; });

	// ---------------- specialisation: locked before level 15, two clicks to pick ----------------
	Check(TEXT("form buttons shown but locked at level 1"), [ButtonCenter, TowerAt]()
	{
		FVector2D C;
		return ButtonCenter(EB::ChooseForm, 1, C) && ButtonCenter(EB::ChooseForm, 2, C) && !TowerAt(FIntPoint(11, 4))->CanChooseForm();
	});
	Do(TEXT("level the harpoon to 15"), [TowerAt]()
	{
		ATDTower* T = TowerAt(FIntPoint(11, 4));
		for (int32 Guard = 0; Guard < 200 && T->GetLevel() < TD::FormLevel; ++Guard)
		{
			T->AddXp(1.0e6f);
		}
	});
	Check(TEXT("harpoon can pick a form"), [TowerAt]() { return TowerAt(FIntPoint(11, 4))->GetLevel() >= TD::FormLevel && TowerAt(FIntPoint(11, 4))->CanChooseForm(); });
	Do(TEXT("click Swordfish once"), [Click]() { Click(EB::ChooseForm, 1); });
	Check(TEXT("first click only arms the form"), [P, TowerAt]() { return TowerAt(FIntPoint(11, 4))->GetForm() == 0 && P()->GetArmedForm() == 1; });
	Do(TEXT("click Swordfish again"), [Click]() { Click(EB::ChooseForm, 1); });
	Check(TEXT("harpoon became a Swordfish"), [TowerAt]() { return TowerAt(FIntPoint(11, 4))->GetForm() == 1 && !TowerAt(FIntPoint(11, 4))->CanChooseForm(); });
	Do(TEXT("click Stats tab"), [Click]() { Click(EB::Tab, 0); });
	Check(TEXT("stats tab shown"), [P]() { return P()->GetPanelTab() == 0; });
	Do(TEXT("add a jellyfish, deselect, show all ranges"), [G, WeakPC]()
	{
		G()->TryPlaceTower(FIntPoint(13, 9), ETDTowerType::Jelly, true);
		WeakPC->ClearSelection();
		G()->SetShowAllRanges(true);
	});
	Check(TEXT("show all ranges draws every tower's range"), [Grid]()
	{
		int32 Ranged = 0;
		for (const ATDTower* T : Grid()->GetTowers())
		{
			Ranged += (T && T->GetRange() > 0.0f) ? 1 : 0;
		}
		return Ranged >= 2 && Grid()->GetVisibleRangeCount() == Ranged;
	});
	Do(TEXT("turn show all off, reselect the harpoon"), [G, ClickCell]() { G()->SetShowAllRanges(false); ClickCell(FIntPoint(11, 4)); });

	// ---------------- hotkeys on aura towers (they have skills and items like the attackers) ----------------
	Do(TEXT("turn show all off, place a Humpback and an Acid Vent"), [G]()
	{
		G()->SetShowAllRanges(false);
		G()->TryPlaceTower(FIntPoint(14, 12), ETDTowerType::Humpback, true);
		G()->TryPlaceTower(FIntPoint(15, 12), ETDTowerType::AcidVent, true);
	});
	for (const ETDTowerType AuraType : { ETDTowerType::Humpback, ETDTowerType::AcidVent })
	{
		const FIntPoint AuraCell(AuraType == ETDTowerType::Humpback ? 14 : 15, 12);
		const FString AuraName = TD::Tower(AuraType).Name;
		Do(FString::Printf(TEXT("click the %s"), *AuraName), [ClickCell, AuraCell]() { ClickCell(AuraCell); });
		Check(FString::Printf(TEXT("%s selected"), *AuraName), [P, TowerAt, AuraCell, AuraType]()
		{
			return TowerAt(AuraCell) && TowerAt(AuraCell)->GetTowerType() == AuraType && P()->GetSelectedTower() == TowerAt(AuraCell);
		});
		Do(FString::Printf(TEXT("press the Skills key on the %s"), *AuraName), [P]() { P()->SetPanelTab(0); P()->ToggleSkillsTab(); });
		Check(FString::Printf(TEXT("Skills key opens the %s skills tab"), *AuraName), [P]() { return P()->GetPanelTab() == 2; });
		Do(FString::Printf(TEXT("click Learn on %s skill 1"), *AuraName), [Click]() { Click(EB::LearnSkill, 0); });
		Check(FString::Printf(TEXT("%s skill learned"), *AuraName), [TowerAt, AuraCell]() { return TowerAt(AuraCell)->GetSkillRank(0) == 1; });
		Do(FString::Printf(TEXT("press the Storage key on the %s"), *AuraName), [P]() { if (P()->IsStorageOpen()) { P()->ToggleStorage(); } P()->ToggleStorage(); });
		Check(FString::Printf(TEXT("Storage key opens storage with the %s selected"), *AuraName), [P]() { return P()->IsStorageOpen(); });
		Do(TEXT("close storage, press the Skills key again"), [P]() { P()->ToggleStorage(); P()->ToggleSkillsTab(); });
		Check(FString::Printf(TEXT("Skills key toggles the %s back to stats"), *AuraName), [P]() { return P()->GetPanelTab() == 0; });
		Do(FString::Printf(TEXT("press the Sell key on the %s"), *AuraName), [P]() { P()->SellSelected(); });
		Check(FString::Printf(TEXT("Sell key asks to confirm selling the %s"), *AuraName), [P, TowerAt, AuraCell]()
		{
			return TowerAt(AuraCell) && P()->IsSellConfirmOpen() && P()->GetPendingSell() == TowerAt(AuraCell);
		});
		Check(TEXT("the Sell button itself asks to confirm (no popup)"), [P, ButtonCenter]() { FVector2D C; return ButtonCenter(EB::Sell, 0, C) && P()->IsSellConfirmOpen(); });
		Do(TEXT("press Esc"), [WeakPC]() { WeakPC->SimMenuKey(); });
		Check(FString::Printf(TEXT("Esc cancels: the %s is kept"), *AuraName), [P, TowerAt, AuraCell]() { return TowerAt(AuraCell) && !P()->IsSellConfirmOpen() && P()->GetSelectedTower() == TowerAt(AuraCell); });
		Do(FString::Printf(TEXT("press the Sell key twice on the %s"), *AuraName), [P]() { P()->SellSelected(); P()->SellSelected(); });
		Check(FString::Printf(TEXT("Sell key sold the %s"), *AuraName), [P, TowerAt, AuraCell]() { return !TowerAt(AuraCell) && !P()->GetSelectedTower() && !P()->IsSellConfirmOpen(); });
	}
	Do(TEXT("click a Coral Wall, press the Skills key"), [ClickCell, P]() { ClickCell(FIntPoint(6, 3)); P()->ToggleSkillsTab(); });
	Check(TEXT("Skills key does nothing on a Coral Wall"), [P, TowerAt]() { return P()->GetSelectedTower() == TowerAt(FIntPoint(6, 3)) && P()->GetPanelTab() == 0; });
	Do(TEXT("reselect the harpoon"), [ClickCell]() { ClickCell(FIntPoint(11, 4)); });

	// ---------------- storage: toggle, equip, unequip, tooltip, salvage, sort ----------------
	Do(TEXT("click Storage button"), [Click]() { Click(EB::StorageToggle, 0); });
	Check(TEXT("storage open"), [P]() { return P()->IsStorageOpen(); });
	Do(TEXT("add items"), [G]()
	{
		G()->AddToStorage(G()->CreateItem(ETDRarity::Common));
		G()->AddToStorage(G()->CreateItem(ETDRarity::Legendary));
		G()->AddToStorage(G()->CreateItem(ETDRarity::Rare));
	});
	Do(TEXT("click storage item 1"), [Click]() { Click(EB::StorageItem, 0); });
	Check(TEXT("item picked"), [P]() { return P()->GetWorkbenchIndex() == 0; });
	Do(TEXT("click Equip"), [Click]() { Click(EB::ItemEquip, 0); });
	Check(TEXT("item equipped from storage"), [G, TowerAt]() { return TowerAt(FIntPoint(11, 4))->GetEquippedCount() == 1 && G()->GetStorage().Num() == 2; });
	Do(TEXT("click the equipped item"), [Click, TowerAt]()
	{
		for (int32 Slot = 0; Slot < (int32)ETDItemSlot::Count; ++Slot)
		{
			if (TowerAt(FIntPoint(11, 4))->GetEquipped((ETDItemSlot)Slot).IsValid())
			{
				Click(EB::EquippedItem, Slot);
			}
		}
	});
	Check(TEXT("item unequipped to storage"), [G, TowerAt]() { return TowerAt(FIntPoint(11, 4))->GetEquippedCount() == 0 && G()->GetStorage().Num() == 3; });
	Do(TEXT("hover storage item 1"), [Hover]() { Hover(EB::StorageItem, 0); });
	Check(TEXT("item tooltip shows its name"), [G, WeakPC]() { return HUDOf(WeakPC.Get())->GetLastTipTitle() == G()->GetStorage()[0].Name; });
	Do(TEXT("click Sort"), [Click]() { Click(EB::Sort, 0); });
	Check(TEXT("sorted by rarity"), [G]()
	{
		const TArray<FTDItem>& S = G()->GetStorage();
		for (int32 i = 1; i < S.Num(); ++i)
		{
			if (S[i].Rarity > S[i - 1].Rarity)
			{
				return false;
			}
		}
		return S.Num() == 3 && S[0].Rarity == ETDRarity::Legendary;
	});
	Do(TEXT("right click storage item 3"), [RightClick]() { RightClick(EB::StorageItem, 2); });
	Check(TEXT("salvaged"), [G]() { return G()->GetStorage().Num() == 2; });

	// ---------------- item workbench ----------------
	Do(TEXT("deselect and pick an item"), [P, G]() { P()->ClearSelection(); G()->DebugAddPearls(500); });
	Do(TEXT("wait out the double-click window"), []() {});
	Wait(0.5f);
	Do(TEXT("click storage item 1 to pick it"), [Click]() { Click(EB::StorageItem, 0); });
	Check(TEXT("item picked for the workbench"), [P]() { return P()->GetWorkbenchIndex() == 0; });
	TSharedRef<int32> LevelBefore = MakeShared<int32>(0);
	Do(TEXT("click Upgrade"), [Click, G, LevelBefore]() { *LevelBefore = G()->GetStorage()[0].Level; Click(EB::ItemUpgrade, 0); });
	Check(TEXT("item upgraded"), [G, LevelBefore]() { return G()->GetStorage()[0].Level == FMath::Min(TD::MaxItemLevel, *LevelBefore + TD::ItemUpgradeLevels); });
	Do(TEXT("click Reroll"), [Click]() { Click(EB::ItemReroll, 0); });
	Check(TEXT("item rerolled and still stored"), [G, P]() { return G()->GetStorage().Num() == 2 && P()->GetWorkbenchIndex() == 0; });

	// ---------------- item lock: Salvage refuses it, Sort leaves it in place ----------------
	static int32 LockedId = 0;
	Do(TEXT("click Lock"), [Click, G]() { LockedId = G()->GetStorage()[0].Id; Click(EB::ItemLock, 0); });
	Check(TEXT("item locked"), [G]() { return G()->GetStorage()[0].bLocked; });
	Do(TEXT("salvage the locked item"), [G]() { G()->SalvageItem(0); });
	Check(TEXT("locked item can't be salvaged"), [G]() { return G()->GetStorage().Num() == 2 && G()->GetStorage()[0].Id == LockedId; });
	Do(TEXT("add a Legendary and sort"), [G]() { G()->AddToStorage(G()->CreateItem(ETDRarity::Legendary)); G()->SortStorage(); });
	Check(TEXT("sort keeps the locked item in its place"), [G]() { return G()->GetStorage()[0].Id == LockedId && G()->GetStorage()[1].Rarity == ETDRarity::Legendary; });
	Do(TEXT("salvage the Legendary, click Unlock"), [G, Click]() { G()->SalvageItem(1); Click(EB::ItemLock, 0); });
	Check(TEXT("item unlocked"), [G]() { return G()->GetStorage().Num() == 2 && G()->GetStorage()[0].Id == LockedId && !G()->GetStorage()[0].bLocked; });
	Do(TEXT("reselect the tower"), [P, TowerAt]() { P()->SelectPlacedTower(TowerAt(FIntPoint(11, 4))); });
	Check(TEXT("harpoon targets First"), [TowerAt]() { return TowerAt(FIntPoint(11, 4))->GetTargeting() == ETDTargeting::First; });
	Do(TEXT("click the targeting arrow"), [Click]() { Click(EB::TargetCycle, 1); });
	Check(TEXT("targeting Last"), [TowerAt]() { return TowerAt(FIntPoint(11, 4))->GetTargeting() == ETDTargeting::Last; });
	Do(TEXT("click the other arrow twice"), [Click]() { Click(EB::TargetCycle, -1); Click(EB::TargetCycle, -1); });
	Check(TEXT("targeting wraps round to Closest"), [TowerAt]() { return TowerAt(FIntPoint(11, 4))->GetTargeting() == ETDTargeting::Closest; });
	Do(TEXT("targeting key"), [P]() { P()->CycleTargeting(1); });
	Check(TEXT("the key steps on to First"), [TowerAt]() { return TowerAt(FIntPoint(11, 4))->GetTargeting() == ETDTargeting::First; });

	// ---------------- sell refused with full storage ----------------
	Do(TEXT("pick an item"), [Click]() { Click(EB::StorageItem, 0); });
	Do(TEXT("equip it"), [Click]() { Click(EB::ItemEquip, 0); });
	Do(TEXT("fill storage"), [G]() { while (G()->GetStorageFree() > 0) { G()->AddToStorage(G()->CreateItem(ETDRarity::Common)); } });
	Do(TEXT("click Sell"), [Click]() { Click(EB::Sell, 0); });
	Check(TEXT("sell refused while storage is full"), [TowerAt]() { return TowerAt(FIntPoint(11, 4)) != nullptr; });

	// ---------------- floor drop with full storage ----------------
	static TWeakObjectPtr<ATDPickup> Drop;
	Do(TEXT("drop an item on the floor"), [G, WeakPC]() { Drop = ATDPickup::Spawn(WeakPC->GetWorld(), TD::CellToWorld(FIntPoint(13, 7)), G()->CreateItem(ETDRarity::Epic)); });
	Wait(1.5f);
	Check(TEXT("drop waits while storage is full"), []() { return Drop.IsValid(); });
	Do(TEXT("click the drop"), [ClickCell]() { ClickCell(FIntPoint(13, 7)); });
	Check(TEXT("clicking a drop with full storage keeps it"), []() { return Drop.IsValid(); });
	Do(TEXT("right click to salvage one"), [RightClick]() { RightClick(EB::StorageItem, 39); });
	WaitFor(TEXT("drop collected once there is room"), []() { return !Drop.IsValid(); }, 3.0f);
	Do(TEXT("salvage another"), [RightClick]() { RightClick(EB::StorageItem, 39); });
	Do(TEXT("click Sell again"), [Click]() { Click(EB::Sell, 0); });
	Check(TEXT("sell asks to confirm"), [P, TowerAt]() { return TowerAt(FIntPoint(11, 4)) != nullptr && P()->IsSellConfirmOpen(); });
	Do(TEXT("click the armed Sell button"), [Click]() { Click(EB::Sell, 0); });
	Check(TEXT("sell works with room for the item"), [TowerAt]() { return TowerAt(FIntPoint(11, 4)) == nullptr; });
	Do(TEXT("close storage"), [WeakPC]() { WeakPC->SimStorageKey(); });
	Check(TEXT("storage closed with I"), [P]() { return !P()->IsStorageOpen(); });

	// ---------------- pause menu ----------------
	static int32 SavedTowers = 0;
	Do(TEXT("press Esc"), [WeakPC]() { WeakPC->ClearSelection(); WeakPC->SimMenuKey(); });
	Check(TEXT("pause menu opens"), [G]() { return G()->GetMenu() == ETDMenu::Pause; });
	Do(TEXT("click Save game"), [Click, TowerCount]() { SavedTowers = TowerCount(); Click(EB::MenuSave, 0); });
	Check(TEXT("save exists"), [G]() { return G()->HasSaveGame(); });
	Do(TEXT("click Settings"), [Click]() { Click(EB::MenuSettings, 0); });
	Check(TEXT("settings from pause"), [G]() { return G()->GetMenu() == ETDMenu::Settings; });
	Do(TEXT("click Back"), [Click]() { Click(EB::SettingsBack, 0); });
	Check(TEXT("back to pause"), [G]() { return G()->GetMenu() == ETDMenu::Pause; });
	Do(TEXT("click Resume"), [Click]() { Click(EB::MenuResume, 0); });
	Check(TEXT("resumed"), [G, WeakPC]() { return G()->GetMenu() == ETDMenu::None && !WeakPC->IsPaused(); });
	Do(TEXT("change the board"), [G]() { G()->TryPlaceTower(FIntPoint(12, 10), ETDTowerType::Wall, true); });
	Do(TEXT("press Esc"), [WeakPC]() { WeakPC->SimMenuKey(); });
	Do(TEXT("click Load"), [Click]() { Click(EB::MenuLoad, 0); });
	Check(TEXT("Load opens the slot picker"), [G]() { return G()->GetMenu() == ETDMenu::Slots; });
	Do(TEXT("click Load on the run's slot"), [Click, G]() { Click(EB::SlotLoad, G()->GetCurrentSlot()); });
	Check(TEXT("load restored the saved board"), [G, TowerCount]() { return G()->GetMenu() == ETDMenu::None && TowerCount() == SavedTowers; });
	Do(TEXT("press Esc"), [WeakPC]() { WeakPC->SimMenuKey(); });
	Do(TEXT("click Restart"), [Click]() { Click(EB::Restart, 0); });
	PassSetup(TEXT("restart"));
	Check(TEXT("restart clears the board"), [G, TowerCount]() { return TowerCount() == 0 && G()->GetPearls() == TD::StartPearls && G()->GetMenu() == ETDMenu::None; });
	Do(TEXT("press Esc"), [WeakPC]() { WeakPC->SimMenuKey(); });
	Do(TEXT("click Main menu"), [Click]() { Click(EB::MenuMainMenu, 0); });
	Check(TEXT("main menu from pause"), [G]() { return G()->GetMenu() == ETDMenu::Main; });
	Do(TEXT("click Start new game"), [Click]() { Click(EB::MenuNewGame, 0); });
	PassSetup(TEXT("new game from the main menu"));

	// ---------------- waves, creatures, preview ----------------
	Do(TEXT("hover wave preview"), [Hover]() { Hover(EB::PreviewEntry, 0); });
	Check(TEXT("wave preview tooltip"), [WeakPC]() { return HUDOf(WeakPC.Get())->GetLastTipTitle().Contains(TEXT("Crab")); });
	Do(TEXT("click Speed twice"), [Click]() { Click(EB::Speed, 0); });
	Do(TEXT("click Speed"), [Click]() { Click(EB::Speed, 0); });
	Check(TEXT("speed 3x"), [G]() { return G()->GetGameSpeed() == 3; });
	Do(TEXT("click Release wave"), [Click]() { Click(EB::Wave, 0); });
	Check(TEXT("wave started"), [G]() { return G()->IsWaveActive(); });
	// a killed creature leaves the game at once and only its remnant plays the death animation
	static TWeakObjectPtr<ATDEnemy> Dying;
	WaitFor(TEXT("a creature to kill"), [G]() { return G()->GetEnemies().Num() > 0; }, 10.0f);
	Do(TEXT("kill a creature"), [G]()
	{
		ATDEnemy* Victim = G()->GetEnemies()[0];
		Dying = Victim;
		Victim->DealDamage(Victim->GetMaxHealth() * 10.0f + Victim->GetMaxShield() * 10.0f, nullptr);
	});
	Check(TEXT("the dying creature is out of the creature list (no targeting, no blocking the wave) and plays its death"), [G]()
	{
		return Dying.IsValid() && Dying->IsDying() && !Dying->IsAlive() && !G()->GetEnemies().Contains(Dying.Get());
	});
	WaitFor(TEXT("the remnant goes after its animation"), []() { return !Dying.IsValid(); }, 3.0f);
	WaitFor(TEXT("hover a creature"), [WeakPC, G]()
	{
		for (ATDEnemy* Enemy : G()->GetEnemies())
		{
			FVector2D Screen;
			if (Enemy && Enemy->IsAlive() && WeakPC->ProjectWorldLocationToScreen(Enemy->GetAimLocation(), Screen))
			{
				WeakPC->SimSetCursor(Screen);
				return WeakPC->GetHoveredEnemy() == Enemy;
			}
		}
		return false;
	}, 15.0f);
	Do(TEXT("click the creature"), [WeakPC]() { WeakPC->SimLeftPress(); WeakPC->SimLeftRelease(); });
	Check(TEXT("creature selected for details"), [P]() { return P()->GetSelectedEnemy() != nullptr; });

	// ---------------- game over buttons ----------------
	Do(TEXT("one life left"), [G]() { G()->DebugSetLives(1); });
	WaitFor(TEXT("game over when a creature leaks"), [G]() { return G()->IsGameOver(); }, 60.0f);
	Do(TEXT("click Load last save"), [Click]() { Click(EB::MenuLoad, 1); });
	Check(TEXT("load from game over"), [G, TowerCount]() { return !G()->IsGameOver() && TowerCount() == SavedTowers; });
	Do(TEXT("one life, release a wave"), [G]() { G()->DebugSetLives(1); G()->ReleaseNextWave(); });
	WaitFor(TEXT("game over again"), [G]() { return G()->IsGameOver(); }, 90.0f);
	Do(TEXT("click Restart"), [Click]() { Click(EB::Restart, 0); });
	PassSetup(TEXT("restart from game over"));
	Check(TEXT("restart from game over"), [G]() { return !G()->IsGameOver() && G()->GetWave() == 0; });
	Do(TEXT("no towers, one life, release"), [G]() { G()->DebugSetLives(1); G()->ReleaseNextWave(); });
	WaitFor(TEXT("game over a third time"), [G]() { return G()->IsGameOver(); }, 60.0f);
	static int32 RetryMap = -1;
	Do(TEXT("click Retry"), [Click]() { RetryMap = TD::GetMap(); Click(EB::Retry, 0); });
	Do(TEXT("replace the slot's other run if asked"), [Click, G]() { if (G()->GetPendingConfirm() == ETDConfirm::OverwriteForRetry) { Click(EB::ConfirmYes, 0); } });
	Check(TEXT("retry starts over on the same map with a boon offer"), [G]() { return !G()->IsGameOver() && G()->GetWave() == 0 && TD::GetMap() == RetryMap && G()->GetMenu() == ETDMenu::Boon; });
	Do(TEXT("retry: take a boon"), [Click, G]()
	{
		for (const int32 Boon : G()->GetBoonOffer())
		{
			if (Boon != (int32)ETDBoon::KrakensPact)
			{
				Click(EB::PickBoon, Boon);
				return;
			}
		}
	});
	Check(TEXT("retry: game on"), [G]() { return G()->GetMenu() == ETDMenu::None; });
	Do(TEXT("dev: all nine uniques into storage"), [G]() { G()->DebugAddUniques(); });
	Check(TEXT("nine uniques found and stored"), [G]()
	{
		int32 Stored = 0;
		for (const FTDItem& Item : G()->GetStorage())
		{
			Stored += TD::IsUnique(Item) ? 1 : 0;
		}
		return TD::CountUniques(G()->GetUniquesFound()) == TD::UniqueCount && Stored == TD::UniqueCount;
	});
	Check(TEXT("a unique can't be rerolled at the workbench"), [G]()
	{
		const int32 Index = G()->GetStorage().IndexOfByPredicate([](const FTDItem& Item) { return TD::IsUnique(Item); });
		G()->DebugAddPearls(100000);
		const FString Before = Index >= 0 ? G()->GetStorage()[Index].Name : FString();
		return Index >= 0 && !G()->RerollStorageItem(Index) && G()->GetStorage()[Index].Name == Before;
	});
	Do(TEXT("retry: plain rules, one life, release"), [G]() { TD::SetRun(FTDRunRules()); G()->DebugSetLives(1); G()->ReleaseNextWave(); });
	WaitFor(TEXT("game over a fourth time"), [G]() { return G()->IsGameOver(); }, 60.0f);
	Do(TEXT("click Main menu"), [Click]() { Click(EB::MenuMainMenu, 0); });
	Check(TEXT("main menu from game over"), [G]() { return !G()->IsGameOver() && G()->GetMenu() == ETDMenu::Main; });
	// delete the script's slot through the picker: No keeps it, Yes deletes it
	Do(TEXT("open the slot picker"), [G]() { G()->OpenSlotPicker(); });
	Do(TEXT("click Delete on slot 2"), [Click]() { Click(EB::SlotDelete, 1); });
	Check(TEXT("delete asks first"), [G]() { return G()->GetPendingConfirm() == ETDConfirm::DeleteSlot; });
	Do(TEXT("click Cancel"), [Click]() { Click(EB::ConfirmNo, 0); });
	Check(TEXT("Cancel keeps the slot"), [G]() { return G()->GetPendingConfirm() == ETDConfirm::None && G()->GetSlotInfo(1).bUsed; });
	Do(TEXT("click Delete again"), [Click]() { Click(EB::SlotDelete, 1); });
	Do(TEXT("click Delete in the question"), [Click]() { Click(EB::ConfirmYes, 0); });
	Check(TEXT("slot 2 deleted"), [G]() { return !G()->GetSlotInfo(1).bUsed && !UGameplayStatics::DoesSaveGameExist(ATDGameMode::SlotName(1), 0); });
	Do(TEXT("leave the slot picker"), [Click]() { Click(EB::SlotBack, 0); });
	Do(TEXT("leave the main menu"), [G]() { G()->StartNewGame(); });
}

// ---------------------------------------------------------------------------------------------
// Balance and performance simulation
// ---------------------------------------------------------------------------------------------

namespace
{
	struct FPlanStep
	{
		FIntPoint Cell;
		ETDTowerType Type;
	};

	/** A reasonable player's build: early towers, then a serpentine maze filled with a mix of towers */
	TArray<FPlanStep> MakeMazePlan()
	{
		TArray<FPlanStep> Plan;
		auto Add = [&Plan](int32 X, int32 Y, ETDTowerType T) { Plan.Add({ FIntPoint(X, Y), T }); };
		auto Column = [&Plan](int32 X, int32 From, int32 To)
		{
			for (int32 Y = From; Y <= To; ++Y)
			{
				Plan.Add({ FIntPoint(X, Y), ETDTowerType::Wall });
			}
		};

		// opening: towers in what become the corridors of the final maze (columns 5, 9, 13),
		// then a serpentine of walls on columns 7, 3, 11 and 15, then more towers along the corridors
		Add(5, 5, ETDTowerType::Mantis);
		Add(9, 7, ETDTowerType::Mantis);
		Add(5, 7, ETDTowerType::Lionfish);
		Add(9, 5, ETDTowerType::Octopus);
		Column(7, 2, 12);
		Add(9, 3, ETDTowerType::Jelly);
		Column(3, 0, 10);
		Add(5, 2, ETDTowerType::Mantis);
		Column(11, 0, 10);
		Add(13, 6, ETDTowerType::Puffer);
		Add(6, 6, ETDTowerType::Humpback);
		Column(15, 2, 12);
		Add(10, 6, ETDTowerType::AcidVent);
		Add(13, 8, ETDTowerType::Harpoon);
		Add(13, 4, ETDTowerType::Sawfish);
		Add(9, 10, ETDTowerType::Eel);
		Add(1, 8, ETDTowerType::Lionfish);
		Add(16, 8, ETDTowerType::Harpoon);
		Add(5, 9, ETDTowerType::Puffer);
		Add(12, 6, ETDTowerType::Humpback);
		Add(13, 11, ETDTowerType::Eel);
		Add(1, 4, ETDTowerType::Octopus);
		Add(16, 4, ETDTowerType::Sawfish);
		Add(4, 6, ETDTowerType::AcidVent);
		Add(8, 6, ETDTowerType::Jelly);
		Add(14, 6, ETDTowerType::Lionfish);
		Add(10, 2, ETDTowerType::Mantis);
		Add(6, 11, ETDTowerType::Harpoon);
		Add(12, 9, ETDTowerType::Puffer);
		Add(2, 6, ETDTowerType::Eel);
		return Plan;
	}

	TArray<FPlanStep> MakeUnpreparedPlan()
	{
		return { { FIntPoint(15, 5), ETDTowerType::Mantis }, { FIntPoint(15, 7), ETDTowerType::Mantis } };
	}

	// ---- adaptive builder (strategy 1) ----

	/** Order the adaptive builder buys towers in; it repeats after the last one */
	const ETDTowerType BuildRotation[] = {
		ETDTowerType::Mantis, ETDTowerType::Lionfish, ETDTowerType::Octopus, ETDTowerType::Mantis, ETDTowerType::Jelly,
		ETDTowerType::Harpoon, ETDTowerType::Puffer, ETDTowerType::Sawfish, ETDTowerType::Eel, ETDTowerType::Lionfish,
		ETDTowerType::Harpoon, ETDTowerType::Humpback, ETDTowerType::Puffer, ETDTowerType::Sawfish, ETDTowerType::AcidVent,
		ETDTowerType::Eel, ETDTowerType::Mantis, ETDTowerType::Harpoon, ETDTowerType::Octopus, ETDTowerType::Sawfish,
		ETDTowerType::Eel, ETDTowerType::Humpback, ETDTowerType::Puffer, ETDTowerType::Harpoon, ETDTowerType::AcidVent,
		ETDTowerType::Eel, ETDTowerType::Sawfish, ETDTowerType::Lionfish, ETDTowerType::Puffer, ETDTowerType::Eel,
	};
	constexpr int32 BuildRotationCount = UE_ARRAY_COUNT(BuildRotation);

	/** The builder stops adding walls once this share of the board is walls, or the route is this many times its natural length */
	constexpr float MaxWallShare = 0.35f;
	constexpr float MazeTarget = 2.5f;

	/** Share of the tower and wall spending the builder puts into walls while they still lengthen the route */
	constexpr float WallSpendShare = 0.35f;

	/** Towers (attack and support) the builder places at most, like a player would; after that pearls go to skills and items.
	 *  -ATDSimTowers=N overrides it. */
	constexpr int32 DefaultPlannerTowers = 30;
	int32 PlannerTowerCap()
	{
		int32 Cap = DefaultPlannerTowers;
		FParse::Value(FCommandLine::Get(), TEXT("ATDSimTowers="), Cap);
		return FMath::Max(1, Cap);
	}

	/** One letter per tower type for the board log (Wall first, same order as ETDTowerType) */
	const TCHAR BoardLetters[] = TEXT("#HJPEMOLSWA");

	/** True when a wall here would touch another wall, a rock or the board edge */
	bool ExtendsLine(const ATDGrid* Grid, const FIntPoint& Cell)
	{
		static const FIntPoint Dirs[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) };
		for (const FIntPoint& D : Dirs)
		{
			const FIntPoint N = Cell + D;
			if (!TD::InBounds(N))
			{
				return true;
			}
			const ATDTower* Tower = Grid->GetTowerAt(N);
			if ((Grid->IsBlocked(N) && !Tower) || (Tower && Tower->GetTowerType() == ETDTowerType::Wall))
			{
				return true;
			}
		}
		return false;
	}

	/** An attack tower as the coverage score sees it */
	struct FCoverTower
	{
		FIntPoint Cell;
		float Range;
	};

	inline float CellDistance(const FIntPoint& A, const FIntPoint& B)
	{
		return FMath::Sqrt((float)FMath::Square(A.X - B.X) + (float)FMath::Square(A.Y - B.Y));
	}

	int32 CountInRange(const TArray<FIntPoint>& Route, const FIntPoint& Cell, float Range)
	{
		int32 Count = 0;
		for (const FIntPoint& R : Route)
		{
			Count += CellDistance(R, Cell) <= Range ? 1 : 0;
		}
		return Count;
	}

	/** Sum over attack towers of the route cells each one reaches */
	int32 RouteCoverage(const TArray<FIntPoint>& Route, const TArray<FCoverTower>& Towers)
	{
		int32 Total = 0;
		for (const FCoverTower& T : Towers)
		{
			Total += CountInRange(Route, T.Cell, T.Range);
		}
		return Total;
	}

	void CurrentRoute(const ATDGrid* Grid, TArray<FIntPoint>& OutRoute)
	{
		TArray<int32> Field;
		Field.SetNum(TD::Cols * TD::Rows);
		for (int32 Y = 0; Y < TD::Rows; ++Y)
		{
			for (int32 X = 0; X < TD::Cols; ++X)
			{
				Field[TD::CellIndex(FIntPoint(X, Y))] = Grid->GetDistance(FIntPoint(X, Y));
			}
		}
		TD::TraceRoute(Field, OutRoute);
	}
}

FTDSim::FTDSim(ATDPlayerController* InPC, int32 InStrategy, int32 InWaves, float InTimeScale)
	: PC(InPC), Strategy(InStrategy), Waves(FMath::Max(1, InWaves)), TimeScale(FMath::Clamp(InTimeScale, 1.0f, 12.0f))
{
}

void FTDSim::Start()
{
	ATDGameMode* M = GM(PC.Get());
	M->StartNewGame();
	WaveLivesStart = M->GetLives();
	if (Strategy == 2)
	{
		// stress test: everything built, late waves
		M->DebugAddPearls(200000);
		M->DebugSetWave(24);
	}
	UGameplayStatics::SetGlobalTimeDilation(PC.Get(), TimeScale);
	StartTime = FPlatformTime::Seconds();
	bStarted = true;
	// -ATDSimHealth=N plays with enemy health at N% (past the settings range) to measure how much headroom a board has
	float HealthPercent = 0.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("ATDSimHealth="), HealthPercent))
	{
		M->DebugSetEnemyHealthPercent(HealthPercent);
	}
	// boon offers like a set-up game (before wave 1, then after waves 25, 50 and 75); -ATDSimNoBoons plays without them
	if (!FParse::Param(FCommandLine::Get(), TEXT("ATDSimNoBoons")))
	{
		M->DebugEnableBoons();
	}
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM start: strategy %d, %d waves, time scale %.1f, enemy health %.0f%%"), Strategy, Waves, TimeScale, M->GetEnemyHealthPercent());
}

void FTDSim::PickBoon()
{
	// a typical player's order of preference: raw tower power first, then economy and safety; cursed boons are skipped
	static const ETDBoon Preference[] = {
		ETDBoon::SharpenedReef, ETDBoon::TidalFury, ETDBoon::GiantSlayer, ETDBoon::VenomBloom, ETDBoon::ScholarsTide,
		ETDBoon::BountyHunters, ETDBoon::LongSight, ETDBoon::CheapUpgrades, ETDBoon::ThickReef, ETDBoon::StormCoil,
		ETDBoon::DeepPockets, ETDBoon::TreasureCurrents, ETDBoon::LuckyFind, ETDBoon::CoralMasons,
	};
	ATDGameMode* M = GM(PC.Get());
	const TArray<int32> Offer = M->GetBoonOffer();
	int32 Pick = INDEX_NONE;
	for (const ETDBoon Boon : Preference)
	{
		if (Offer.Contains((int32)Boon))
		{
			Pick = (int32)Boon;
			break;
		}
	}
	if (Pick == INDEX_NONE)
	{
		for (const int32 Boon : Offer)
		{
			if (!TD::IsCursed((ETDBoon)Boon))
			{
				Pick = Boon;
				break;
			}
		}
	}
	if (Pick != INDEX_NONE && M->PickBoon(Pick))
	{
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM boon: %s (wave %d)"), TD::BoonName((ETDBoon)Pick), M->GetWave());
	}
}

void FTDSim::SampleDanger()
{
	ATDGameMode* M = GM(PC.Get());
	const ATDGrid* Grid = M->GetGrid();
	const float RouteLength = FMath::Max(1.0f, (float)Grid->GetRouteLength());
	const float FlightLength = FMath::Max(1.0f, FVector::Dist2D(TD::CellToWorld(TD::SpawnCell()), TD::CellToWorld(TD::GoalCell())) / TD::TileSize);
	for (const ATDEnemy* Enemy : M->GetEnemies())
	{
		if (Enemy && !Enemy->IsDying())
		{
			const float Share = 1.0f - Enemy->GetGoalDistance() / (Enemy->IsFlying() ? FlightLength : RouteLength);
			WaveDanger = FMath::Max(WaveDanger, FMath::Clamp(Share, 0.0f, 1.0f));
		}
	}
}

void FTDSim::BuildAdaptive()
{
	ATDGameMode* M = GM(PC.Get());
	ATDGrid* Grid = M->GetGrid();
	const float HumpbackRange = TD::TowerRange(ETDTowerType::Humpback, 1);
	const float VentRange = TD::TowerRange(ETDTowerType::AcidVent, 1);

	if (NaturalPath == 0)
	{
		NaturalPath = Grid->GetRouteLength();
	}

	// the tiles under the swimmers' straight line from the trench to the reef
	TArray<FIntPoint> Flight;
	{
		const FVector2D From(TD::SpawnCell()), To(TD::GoalCell());
		const int32 Steps = FMath::CeilToInt(FVector2D::Distance(From, To) * 2.0f);
		for (int32 i = 0; i <= Steps; ++i)
		{
			const FVector2D P = FMath::Lerp(From, To, Steps > 0 ? (float)i / Steps : 0.0f);
			Flight.AddUnique(FIntPoint(FMath::RoundToInt(P.X), FMath::RoundToInt(P.Y)));
		}
	}
	constexpr float FlightWeight = 1.0f;

	// a few purchases per tick at most; anything left waits for the next tick
	for (int32 Purchase = 0; Purchase < 8; ++Purchase)
	{
		TArray<FIntPoint> Route;
		CurrentRoute(Grid, Route);
		TArray<FCoverTower> Attackers;
		int32 Walls = 0;
		int32 PlannerTowers = 0;
		for (const ATDTower* Tower : Grid->GetTowers())
		{
			if (Tower && Tower->IsAttackTower())
			{
				Attackers.Add({ Tower->GetCell(), Tower->GetRange() });
			}
			const bool bWallTile = Tower && Tower->GetTowerType() == ETDTowerType::Wall;
			Walls += bWallTile ? 1 : 0;
			PlannerTowers += (Tower && !bWallTile) ? 1 : 0;
		}
		const int32 BaseCoverage = RouteCoverage(Route, Attackers);
		TSet<FIntPoint> OnRoute(Route);

		ETDTowerType NextType = BuildRotation[RotationIndex % BuildRotationCount];
		// -ATDSimNoSawfish builds Harpoons instead, to measure how much of the board's power is percentage bleed
		if (NextType == ETDTowerType::Sawfish && FParse::Param(FCommandLine::Get(), TEXT("ATDSimNoSawfish")))
		{
			NextType = ETDTowerType::Harpoon;
		}
		const FTDTowerDef& Def = TD::Tower(NextType);

		// best tile for the next tower; ties go to tiles off the route so the maze isn't cut short
		FIntPoint TowerCell(-1, -1);
		float TowerScore = -1.0f;
		TArray<int32> NewField;
		TArray<FIntPoint> NewRoute;
		for (int32 Y = 0; Y < TD::Rows; ++Y)
		{
			for (int32 X = 0; X < TD::Cols; ++X)
			{
				const FIntPoint Cell(X, Y);
				const bool bOnRoute = OnRoute.Contains(Cell);
				if (Grid->EvaluatePlacement(Cell, bOnRoute ? &NewField : nullptr) != ETDPlaceResult::Ok)
				{
					continue;
				}
				// a tile off the route doesn't change it
				const TArray<FIntPoint>* After = &Route;
				if (bOnRoute)
				{
					TD::TraceRoute(NewField, NewRoute);
					After = &NewRoute;
				}

				float Score = 0.0f;
				if (Def.bAttacks)
				{
					TArray<FCoverTower> With = Attackers;
					With.Add({ Cell, TD::TowerRange(NextType, 1) });
					Score = (float)(RouteCoverage(*After, With) - BaseCoverage);
					// once the opening towers stand, a player also covers the swimmers' straight line with towers that hit them
					if (Def.bHitsFlying && Attackers.Num() >= 3)
					{
						Score += FlightWeight * CountInRange(Flight, Cell, TD::TowerRange(NextType, 1));
					}
				}
				else
				{
					int32 NearTowers = 0;
					for (const FCoverTower& T : Attackers)
					{
						NearTowers += CellDistance(T.Cell, Cell) <= (NextType == ETDTowerType::Humpback ? HumpbackRange : VentRange) ? 1 : 0;
					}
					const int32 RouteNear = CountInRange(*After, Cell, NextType == ETDTowerType::Humpback ? HumpbackRange : VentRange);
					Score = (NextType == ETDTowerType::Humpback) ? NearTowers + 0.01f * RouteNear : RouteNear + 0.25f * NearTowers;
					// keep auras from walling the route unless it pays off
					Score += (float)(RouteCoverage(*After, Attackers) - BaseCoverage);
				}
				Score -= bOnRoute ? 0.1f : 0.0f;
				if (Score > TowerScore)
				{
					TowerScore = Score;
					TowerCell = Cell;
				}
			}
		}

		// best wall: score = coverage gain + 3 x route tiles added, +1 when it extends a wall line, rock or the
		// board edge (so walls form serpentines instead of scattered blocks). Only walls that lengthen the
		// route count; a tile off the route can't change it, so only route tiles are tried.
		const int32 PathBefore = FMath::Max(0, Route.Num() - 1);
		FIntPoint WallCell(-1, -1);
		float WallScore = 0.0f;
		if (Attackers.Num() >= 3 && Walls < MaxWallShare * TD::Cols * TD::Rows && PathBefore < MazeTarget * NaturalPath)
		{
			for (const FIntPoint& Cell : Route)
			{
				if (Grid->EvaluatePlacement(Cell, &NewField) != ETDPlaceResult::Ok)
				{
					continue;
				}
				TD::TraceRoute(NewField, NewRoute);
				const int32 PathGain = FMath::Max(0, NewRoute.Num() - 1) - PathBefore;
				const int32 CoverGain = RouteCoverage(NewRoute, Attackers) - BaseCoverage;
				const float Score = CoverGain + 3.0f * PathGain + (ExtendsLine(Grid, Cell) ? 1.0f : 0.0f);
				if (PathGain > 0 && Score > WallScore)
				{
					WallScore = Score;
					WallCell = Cell;
				}
			}
		}

		// walls first while they are under their share of the spending, otherwise the better coverage per pearl;
		// a tower may be worth saving for
		const bool bTowerOpen = PlannerTowers < PlannerTowerCap() && TowerCell.X >= 0;
		const int32 WallCost = TD::Tower(ETDTowerType::Wall).Cost;
		const float TowerValue = bTowerOpen ? FMath::Max(TowerScore, 0.0f) / Def.Cost : 0.0f;
		const float WallValue = WallScore / WallCost;
		const bool bUnderShare = WallSpent < WallSpendShare * (WallSpent + TowerSpent);
		const bool bWall = WallCell.X >= 0 && (bUnderShare || WallValue >= TowerValue || !bTowerOpen);
		if (!bWall && !bTowerOpen)
		{
			if (!bMazeLogged && PlannerTowers >= PlannerTowerCap())
			{
				bMazeLogged = true;
				LogBoard(TEXT("tower cap reached"));
			}
			return;
		}
		const FIntPoint Cell = bWall ? WallCell : TowerCell;
		const ETDTowerType Type = bWall ? ETDTowerType::Wall : NextType;
		const int32 Cost = TD::Tower(Type).Cost;
		if (M->GetPearls() < Cost)
		{
			return;
		}
		if (!M->TryPlaceTower(Cell, Type, true))
		{
			return;	// a creature stepped in the way: try again next tick
		}
		if (bWall)
		{
			WallSpent += Cost;
		}
		else
		{
			TowerSpent += Cost;
			++RotationIndex;
		}
		CurrentRoute(Grid, Route);
		Attackers.Reset();
		for (const ATDTower* Tower : Grid->GetTowers())
		{
			if (Tower && Tower->IsAttackTower())
			{
				Attackers.Add({ Tower->GetCell(), Tower->GetRange() });
			}
		}
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM build: %s at (%d,%d)  path %d  coverage %d  pearls %d"),
			TD::Tower(Type).Name, Cell.X, Cell.Y, Grid->GetRouteLength(), RouteCoverage(Route, Attackers), M->GetPearls());
	}
}

void FTDSim::Build()
{
	ATDGameMode* M = GM(PC.Get());
	if (Strategy == 1)
	{
		BuildAdaptive();
		return;
	}
	const TArray<FPlanStep> Plan = (Strategy == 0) ? MakeUnpreparedPlan() : MakeMazePlan();
	while (Plan.IsValidIndex(PlanIndex))
	{
		const FPlanStep& Step = Plan[PlanIndex];
		if (M->GetGrid()->IsBlocked(Step.Cell))
		{
			++PlanIndex;	// rock, or already built
			continue;
		}
		if (M->GetPearls() < TD::Tower(Step.Type).Cost)
		{
			return;
		}
		if (!M->TryPlaceTower(Step.Cell, Step.Type, true) && M->GetGrid()->EvaluatePlacement(Step.Cell) != ETDPlaceResult::EnemyInWay)
		{
			++PlanIndex;	// blocked for good: skip
			continue;
		}
		if (M->GetGrid()->GetTowerAt(Step.Cell))
		{
			++PlanIndex;
		}
		else
		{
			return;	// a creature is in the way: try again later
		}
	}

	// stress test: fill every free tile that keeps a route with towers
	if (Strategy == 2)
	{
		for (int32 Y = 0; Y < TD::Rows; ++Y)
		{
			for (int32 X = 0; X < TD::Cols; ++X)
			{
				const ETDTowerType Type = (ETDTowerType)(1 + (X * 7 + Y * 3) % (TD::TowerTypeCount - 1));
				M->TryPlaceTower(FIntPoint(X, Y), Type, true);
			}
		}
	}
}

void FTDSim::ManageTowers()
{
	ATDGameMode* M = GM(PC.Get());
	int32 PlannerTowers = 0;
	for (const ATDTower* Tower : M->GetGrid()->GetTowers())
	{
		PlannerTowers += (Tower && Tower->GetTowerType() != ETDTowerType::Wall) ? 1 : 0;
	}
	const bool bPlanDone = (Strategy == 1) ? PlannerTowers >= PlannerTowerCap()
		: (Strategy != 3) || PlanIndex >= MakeMazePlan().Num();

	for (ATDTower* Tower : M->GetGrid()->GetTowers())
	{
		if (!Tower || !Tower->CanProgress())
		{
			continue;
		}

		// learn the cheapest available skill when there is spare money
		while (Tower->GetSkillPoints() > 0)
		{
			int32 Best = -1, BestCost = MAX_int32;
			for (int32 i = 0; i < TD::SkillsPerTower; ++i)
			{
				const int32 Cost = TD::SkillCost(Tower->GetTowerType(), i, Tower->GetSkillRank(i) + 1);
				if (Tower->CanLearnSkill(i) && Cost < BestCost)
				{
					Best = i;
					BestCost = Cost;
				}
			}
			const int32 Reserve = bPlanDone ? 0 : 40;
			if (Best < 0 || M->GetPearls() < BestCost + Reserve || !M->TryLearnSkill(Tower, Best))
			{
				break;
			}
			SkillSpent += BestCost;
		}

		// equip the best stored item for each empty slot
		for (int32 Slot = 0; Slot < (int32)ETDItemSlot::Count; ++Slot)
		{
			if (Tower->GetEquipped((ETDItemSlot)Slot).IsValid())
			{
				continue;
			}
			int32 Best = -1;
			for (int32 i = 0; i < M->GetStorage().Num(); ++i)
			{
				const FTDItem& Item = M->GetStorage()[i];
				if ((int32)Item.Slot == Slot && (Best < 0 || Item.Rarity > M->GetStorage()[Best].Rarity))
				{
					Best = i;
				}
			}
			if (Best >= 0)
			{
				M->EquipFromStorage(Tower, Best);
			}
		}
	}

	while (M->GetStorage().Num() > 30)
	{
		M->SalvageItem(M->GetStorage().Num() - 1);
	}

	if (Strategy == 1 && bPlanDone)
	{
		UpgradeItems();
	}
}

void FTDSim::UpgradeItems()
{
	// one upgrade per tick: the cheapest equipped item on the five towers that dealt the most damage,
	// through the workbench (unequip to storage, upgrade, equip again)
	ATDGameMode* M = GM(PC.Get());
	if (M->GetStorageFree() <= 0)
	{
		return;
	}
	TArray<ATDTower*> Top;
	for (ATDTower* Tower : M->GetGrid()->GetTowers())
	{
		if (Tower && Tower->IsAttackTower())
		{
			Top.Add(Tower);
		}
	}
	Top.Sort([](const ATDTower& A, const ATDTower& B) { return A.GetDamageDone() > B.GetDamageDone(); });
	Top.SetNum(FMath::Min(Top.Num(), 5));

	ATDTower* BestTower = nullptr;
	ETDItemSlot BestSlot = ETDItemSlot::Shell;
	int32 BestCost = MAX_int32;
	for (ATDTower* Tower : Top)
	{
		for (int32 Slot = 0; Slot < (int32)ETDItemSlot::Count; ++Slot)
		{
			const FTDItem& Item = Tower->GetEquipped((ETDItemSlot)Slot);
			if (Item.IsValid() && Item.Level < TD::MaxItemLevel && TD::UpgradeCost(Item) < BestCost)
			{
				BestTower = Tower;
				BestSlot = (ETDItemSlot)Slot;
				BestCost = TD::UpgradeCost(Item);
			}
		}
	}
	if (!BestTower || M->GetPearls() < BestCost)
	{
		return;
	}
	const int32 Id = BestTower->GetEquipped(BestSlot).Id;
	if (!M->UnequipToStorage(BestTower, BestSlot))
	{
		return;
	}
	const int32 Index = M->GetStorage().IndexOfByPredicate([Id](const FTDItem& Item) { return Item.Id == Id; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	if (M->UpgradeStorageItem(Index))
	{
		ItemSpent += BestCost;
	}
	const int32 After = M->GetStorage().IndexOfByPredicate([Id](const FTDItem& Item) { return Item.Id == Id; });
	if (After != INDEX_NONE)
	{
		M->EquipFromStorage(BestTower, After);
	}
}

void FTDSim::LogBoard(const TCHAR* Label)
{
	ATDGameMode* M = GM(PC.Get());
	const ATDGrid* Grid = M->GetGrid();
	TArray<FIntPoint> Route;
	CurrentRoute(Grid, Route);
	const TSet<FIntPoint> OnRoute(Route);
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM board (%s, wave %d): path %d (natural %d). # wall, R rock, X spawn, G goal, . route, letters H J P E M O L S W A = Harpoon Jelly Puffer Eel Mantis Octopus Lionfish Sawfish Humpback AcidVent"),
		Label, M->GetWave(), Grid->GetRouteLength(), NaturalPath);
	for (int32 Y = 0; Y < TD::Rows; ++Y)
	{
		FString Row;
		for (int32 X = 0; X < TD::Cols; ++X)
		{
			const FIntPoint Cell(X, Y);
			const ATDTower* Tower = Grid->GetTowerAt(Cell);
			TCHAR C = TEXT(' ');
			if (Cell == TD::SpawnCell())
			{
				C = TEXT('X');
			}
			else if (Cell == TD::GoalCell())
			{
				C = TEXT('G');
			}
			else if (Tower)
			{
				C = BoardLetters[(int32)Tower->GetTowerType()];
			}
			else if (Grid->IsBlocked(Cell))
			{
				C = TEXT('R');
			}
			else if (OnRoute.Contains(Cell))
			{
				C = TEXT('.');
			}
			Row.AppendChar(C);
		}
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM board |%s|"), *Row);
	}
}

void FTDSim::LogTopTowers()
{
	ATDGameMode* M = GM(PC.Get());
	TArray<ATDTower*> Top;
	for (ATDTower* Tower : M->GetGrid()->GetTowers())
	{
		if (Tower && Tower->IsAttackTower())
		{
			Top.Add(Tower);
		}
	}
	Top.Sort([](const ATDTower& A, const ATDTower& B) { return A.GetDamageDone() > B.GetDamageDone(); });
	for (int32 i = 0; i < FMath::Min(Top.Num(), 8); ++i)
	{
		FString Items;
		for (int32 Slot = 0; Slot < (int32)ETDItemSlot::Count; ++Slot)
		{
			const FTDItem& Item = Top[i]->GetEquipped((ETDItemSlot)Slot);
			if (Item.IsValid())
			{
				Items += FString::Printf(TEXT(" [%s %s L%d]"), TD::RarityName(Item.Rarity), *Item.Name, Item.Level);
			}
		}
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM top %d at (%d,%d): %s  items:%s"),
			i + 1, Top[i]->GetCell().X, Top[i]->GetCell().Y, *Top[i]->DescribeStats(), Items.IsEmpty() ? TEXT(" none") : *Items);
	}
}

void FTDSim::LogWave(int32 WaveNumber)
{
	if (Strategy == 1 && WaveNumber == 30 && !bMazeLogged)
	{
		bMazeLogged = true;
		LogBoard(TEXT("wave 30"));
	}
	if (Strategy == 1 && WaveNumber % 10 == 0)
	{
		const FTDIncome& In = GM(PC.Get())->GetIncome();
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM income to wave %d: kills %d  bounty %d  trickle %d  wave clear %d  early send %d  sell %d  salvage %d | spent: towers %d  walls %d  skills %d  item upgrades %d"),
			WaveNumber, In.Kills, In.Bounty, In.Trickle, In.WaveClear, In.EarlySend, In.Sell, In.Salvage, TowerSpent, WallSpent, SkillSpent, ItemSpent);
	}

	ATDGameMode* M = GM(PC.Get());
	int32 Attack = 0, MaxLevel = 0, Kills = 0;
	float LevelSum = 0.0f, Damage = 0.0f;
	for (const ATDTower* Tower : M->GetGrid()->GetTowers())
	{
		if (Tower && Tower->CanProgress())
		{
			++Attack;
			LevelSum += Tower->GetLevel();
			MaxLevel = FMath::Max(MaxLevel, Tower->GetLevel());
			Kills += Tower->GetKills();
			Damage += Tower->GetDamageDone();
			if (Strategy != 2 && WaveNumber <= 4)
			{
				UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM   tower %s"), *Tower->DescribeStats());
			}
		}
	}
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM   totals: kills %d  damage %.0f"), Kills, Damage);
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM wave %2d: lives %2d  pearls %5d  towers %2d (+%d walls)  avg level %.1f  max level %d  path %d  danger %.0f%%  lost %d"),
		WaveNumber, M->GetLives(), M->GetPearls(), Attack, M->GetGrid()->GetTowers().Num() - Attack, Attack > 0 ? LevelSum / Attack : 0.0f, MaxLevel,
		M->GetGrid()->GetRouteLength(), 100.0f * WaveDanger, WaveLivesStart >= 0 ? FMath::Max(0, WaveLivesStart - M->GetLives()) : 0);
	WaveDanger = 0.0f;
	WaveLivesStart = M->GetLives();
}

void FTDSim::SamplePerformance(float RealDeltaTime)
{
	const float FrameMs = RealDeltaTime * 1000.0f;
	FrameMsSum += FrameMs;
	GameMsSum += FPlatformTime::ToMilliseconds(GGameThreadTime);
	RenderMsSum += FPlatformTime::ToMilliseconds(GRenderThreadTime);
	GpuMsSum += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	DrawCallsSum += GNumDrawCallsRHI[0];
	PrimitivesSum += GNumPrimitivesDrawnRHI[0];
	WorstFrameMs = FMath::Max(WorstFrameMs, FrameMs);
	++SampleFrames;

	ATDGameMode* M = GM(PC.Get());
	PeakEnemies = FMath::Max(PeakEnemies, M->GetEnemies().Num());

	SampleTimer += RealDeltaTime;
	if (SampleTimer >= 5.0f && SampleFrames > 0)
	{
		const double N = SampleFrames;
		int32 Actors = 0;
		for (TActorIterator<AActor> It(PC->GetWorld()); It; ++It)
		{
			++Actors;
		}
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM perf: frame %.2f ms  game %.2f ms  render %.2f ms  gpu %.2f ms  draw calls %.0f  primitives %.0f  enemies %d  actors %d"),
			FrameMsSum / N, GameMsSum / N, RenderMsSum / N, GpuMsSum / N, DrawCallsSum / N, PrimitivesSum / N, M->GetEnemies().Num(), Actors);
		TotalFrameMs += FrameMsSum / N;
		TotalGameMs += GameMsSum / N;
		TotalRenderMs += RenderMsSum / N;
		TotalGpuMs += GpuMsSum / N;
		TotalDrawCalls += DrawCallsSum / N;
		++TotalSamples;
		SampleTimer = 0.0f;
		SampleFrames = 0;
		FrameMsSum = GameMsSum = RenderMsSum = GpuMsSum = DrawCallsSum = PrimitivesSum = 0.0;
	}
}

void FTDSim::Finish(const TCHAR* Outcome)
{
	ATDGameMode* M = GM(PC.Get());
	const double N = FMath::Max(1, TotalSamples);
	if (Strategy == 1)
	{
		LogBoard(TEXT("end"));
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM spent: towers %d  walls %d  skills %d  item upgrades %d  total %d"),
			TowerSpent, WallSpent, SkillSpent, ItemSpent, TowerSpent + WallSpent + SkillSpent + ItemSpent);
		LogTopTowers();
	}
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM RESULT strategy %d: %s at wave %d, lives %d, %.0f s real time"), Strategy, Outcome, M->GetWave(), M->GetLives(), FPlatformTime::Seconds() - StartTime);
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM PERF AVERAGE: frame %.2f ms  game %.2f ms  render %.2f ms  gpu %.2f ms  draw calls %.0f  worst frame %.1f ms  peak enemies %d"),
		TotalFrameMs / N, TotalGameMs / N, TotalRenderMs / N, TotalGpuMs / N, TotalDrawCalls / N, WorstFrameMs, PeakEnemies);
	UGameplayStatics::SetGlobalTimeDilation(PC.Get(), 1.0f);
}

bool FTDSim::Tick(float RealDeltaTime)
{
	ATDGameMode* M = GM(PC.Get());
	if (!M || !M->GetGrid())
	{
		return false;
	}
	if (!bStarted)
	{
		Start();
		return true;
	}

	SamplePerformance(RealDeltaTime);
	SampleDanger();
	if (M->HasBoonOffer())
	{
		PickBoon();
	}

	if (M->IsGameOver())
	{
		LogWave(M->GetWave());
		Finish(TEXT("LOST"));
		return false;
	}

	// the sim sends waves one at a time, so a quiet board means the last wave was cleared
	if (!M->IsWaveActive() && M->GetWave() > LastLoggedWave)
	{
		LastLoggedWave = M->GetWave();
		LogWave(LastLoggedWave);
	}

	BuildTimer -= RealDeltaTime;
	if (BuildTimer <= 0.0f)
	{
		BuildTimer = 0.5f;
		Build();
		ManageTowers();
	}

	if (!M->IsWaveActive())
	{
		if (M->GetWave() >= Waves + (Strategy == 2 ? 24 : 0))
		{
			Finish(TEXT("SURVIVED"));
			return false;
		}
		// -ATDSimRamp=G: from wave 11 enemy health grows by G per wave (1.05 = +5%), so the wave the sim
		// breaks at shows how much headroom the board has at that point of the game
		// (-ATDSimRampFrom=W starts the ramp after wave W instead of 10)
		float Ramp = 0.0f;
		if (FParse::Value(FCommandLine::Get(), TEXT("ATDSimRamp="), Ramp) && Ramp > 0.0f)
		{
			int32 From = 10;
			FParse::Value(FCommandLine::Get(), TEXT("ATDSimRampFrom="), From);
			const int32 Next = M->GetWave() + 1;
			M->DebugSetEnemyHealthPercent(100.0f * FMath::Pow(Ramp, (float)FMath::Max(0, Next - From)));
			UE_LOG(LogAquaticTowerDefense, Display, TEXT("SIM ramp: wave %d at enemy health %.0f%%"), Next, M->GetEnemyHealthPercent());
		}
		M->ReleaseNextWave();
		UGameplayStatics::SetGlobalTimeDilation(PC.Get(), TimeScale);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Benchmark
// ---------------------------------------------------------------------------------------------

namespace
{
	/** Where the board's triangles come from: LOD0 triangles times instances, per owner class, split by shadow casting, and the biggest meshes */
	void LogTriangleCensus(UWorld* World)
	{
		struct FEntry { double Tris = 0.0; double ShadowTris = 0.0; int32 Draws = 0; int32 Prims = 0; };
		TMap<FString, FEntry> ByClass;
		TMap<const UStaticMesh*, TPair<FString, double>> ByMesh;
		double Total = 0.0, TotalShadow = 0.0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			for (UActorComponent* C : It->GetComponents())
			{
				UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(C);
				if (!SM || !SM->IsRegistered() || !SM->IsVisible() || !SM->GetStaticMesh() || !SM->GetStaticMesh()->GetRenderData()
					|| SM->GetStaticMesh()->GetRenderData()->LODResources.Num() == 0)
				{
					continue;
				}
				const FStaticMeshLODResources& LOD = SM->GetStaticMesh()->GetRenderData()->LODResources[0];
				const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(SM);
				const int32 Count = ISM ? ISM->GetInstanceCount() : 1;
				const double Tris = (double)LOD.GetNumTriangles() * Count;
				FEntry& E = ByClass.FindOrAdd(It->GetClass()->GetName());
				E.Tris += Tris;
				E.ShadowTris += SM->CastShadow ? Tris : 0.0;
				E.Draws += LOD.Sections.Num();
				E.Prims += 1;
				Total += Tris;
				TotalShadow += SM->CastShadow ? Tris : 0.0;
				TPair<FString, double>& M = ByMesh.FindOrAdd(SM->GetStaticMesh());
				M.Key = FString::Printf(TEXT("%s %d tris %d sections%s"), *It->GetClass()->GetName(), LOD.GetNumTriangles(), LOD.Sections.Num(), SM->CastShadow ? TEXT(" shadow") : TEXT(""));
				M.Value += Tris;
			}
		}
		UE_LOG(LogAquaticTowerDefense, Display, TEXT("CENSUS total %.2fM tris, %.2fM cast shadows"), Total / 1e6, TotalShadow / 1e6);
		ByClass.ValueSort([](const FEntry& A, const FEntry& B) { return A.Tris > B.Tris; });
		for (const TPair<FString, FEntry>& P : ByClass)
		{
			UE_LOG(LogAquaticTowerDefense, Display, TEXT("CENSUS class %-24s %8.0fk tris  %8.0fk shadow  %5d primitives  %6d sections"), *P.Key, P.Value.Tris / 1e3, P.Value.ShadowTris / 1e3, P.Value.Prims, P.Value.Draws);
		}
		ByMesh.ValueSort([](const TPair<FString, double>& A, const TPair<FString, double>& B) { return A.Value > B.Value; });
		int32 Shown = 0;
		for (const TPair<const UStaticMesh*, TPair<FString, double>>& P : ByMesh)
		{
			if (++Shown > 25)
			{
				break;
			}
			UE_LOG(LogAquaticTowerDefense, Display, TEXT("CENSUS mesh %8.0fk  %s"), P.Value.Value / 1e3, *P.Value.Key);
		}
	}
}

void FTDPerf::Sample(float RealDeltaTime)
{
	Frames += 1.0;
	FrameMs += RealDeltaTime * 1000.0;
	GameMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
	RenderMs += FPlatformTime::ToMilliseconds(GRenderThreadTime);
	GpuMs += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
	DrawCalls += GNumDrawCallsRHI[0];
	Primitives += GNumPrimitivesDrawnRHI[0];
	WorstMs = FMath::Max(WorstMs, (double)RealDeltaTime * 1000.0);
	FrameTimes.Add(RealDeltaTime * 1000.0f);
}

void FTDPerf::Report(const TCHAR* Label)
{
	ATDGameMode* M = GM(PC.Get());
	int32 Components = 0;
	for (TActorIterator<AActor> It(PC->GetWorld()); It; ++It)
	{
		Components += It->GetComponents().Num();
	}
	const double N = FMath::Max(1.0, Frames);
	FrameTimes.Sort();
	// the fastest tenth of frames: another program on the GPU inflates the average, much less so this
	const float Best = FrameTimes.Num() > 0 ? FrameTimes[FrameTimes.Num() / 10] : 0.0f;
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("PERF %s: %.0f fps (best tenth %.0f fps)  frame %.2f ms  game %.2f ms  render %.2f ms  gpu %.2f ms  worst %.1f ms  draws %.0f  triangles %.0f  enemies %d  components %d"),
		Label, 1000.0 / (FrameMs / N), Best > 0.0f ? 1000.0f / Best : 0.0f, FrameMs / N, GameMs / N, RenderMs / N, GpuMs / N, WorstMs, DrawCalls / N, Primitives / N, M->GetEnemies().Num(), Components);
	if (FCString::Strcmp(Label, TEXT("WHILE SPAWNING")) != 0)
	{
		LogTriangleCensus(PC->GetWorld());
	}
	Frames = FrameMs = GameMs = RenderMs = GpuMs = DrawCalls = Primitives = WorstMs = 0.0;
	FrameTimes.Reset();
}

bool FTDPerf::Tick(float RealDeltaTime)
{
	ATDGameMode* M = GM(PC.Get());
	if (!M || !M->GetGrid())
	{
		return false;
	}
	GEngine->SetMaxFPS(0.0f);
	PhaseTime += RealDeltaTime;

	switch (Phase)
	{
	case 0:
		// the same board every run: the maze from the sim, no creatures yet
		M->StartNewGame();
		M->DebugAddPearls(200000);
		M->DebugSetLives(100000);
		for (const FPlanStep& Step : MakeMazePlan())
		{
			M->TryPlaceTower(Step.Cell, Step.Type, true);
		}
		Phase = 1;
		PhaseTime = 0.0f;
		break;
	case 1:
		if (PhaseTime > 4.0f)
		{
			// -ATDPerfCsv: per-pass GPU times for every sampled frame (r.GPUCsvStatsEnabled) in Saved/Profiling/CSV, one file per phase
			if (!bCsvRunning && FParse::Param(FCommandLine::Get(), TEXT("ATDPerfCsv")))
			{
				bCsvRunning = true;
				PC->ConsoleCommand(TEXT("r.GPUCsvStatsEnabled 1"));
				PC->ConsoleCommand(TEXT("csvprofile start"));
			}
			Sample(RealDeltaTime);
		}
		if (PhaseTime > 10.0f)
		{
			if (bCsvRunning)
			{
				bCsvRunning = false;
				PC->ConsoleCommand(TEXT("csvprofile stop"));
			}
			Report(TEXT("EMPTY BOARD"));
			Phase = 2;
			PhaseTime = 0.0f;
		}
		break;
	case 2:
	{
		Sample(RealDeltaTime);
		const ETDEnemyType Mix[] = { ETDEnemyType::Crab, ETDEnemyType::Piranha, ETDEnemyType::Turtle, ETDEnemyType::Crab, ETDEnemyType::Manta, ETDEnemyType::SeaStar, ETDEnemyType::Cuttlefish, ETDEnemyType::ShockEel };
		while (Spawned < Count && Spawned * 0.1f <= PhaseTime)
		{
			// tough enough to survive the whole run, slow enough to stay on the board
			const ETDEnemyType Type = (Spawned == Count / 2) ? ETDEnemyType::Kraken : Mix[Spawned % UE_ARRAY_COUNT(Mix)];
			M->DebugSpawnEnemy(Type, 100000.0f, Type == ETDEnemyType::Piranha ? 0.1f : 0.25f);
			++Spawned;
		}
		if (Spawned >= Count)
		{
			Report(TEXT("WHILE SPAWNING"));
			Phase = 3;
			PhaseTime = 0.0f;
		}
		break;
	}
	case 3:
		if (PhaseTime > 2.0f)
		{
			if (!bCsvRunning && !bShotTaken && FParse::Param(FCommandLine::Get(), TEXT("ATDPerfCsv")))
			{
				bCsvRunning = true;
				PC->ConsoleCommand(TEXT("csvprofile start"));
			}
			Sample(RealDeltaTime);
		}
		if (PhaseTime > 12.0f && !bShotTaken)
		{
			if (bCsvRunning)
			{
				bCsvRunning = false;
				PC->ConsoleCommand(TEXT("csvprofile stop"));
			}
			bShotTaken = true;
			PC->ConsoleCommand(TEXT("Shot showui"));
			// where one heavy frame goes, written to the log: game thread call tree and GPU passes
			PC->ConsoleCommand(TEXT("stat dumpframe -ms=0.1"));
			PC->ConsoleCommand(TEXT("ProfileGPU"));
		}
		if (PhaseTime > 22.0f)
		{
			Report(TEXT("HEAVY WAVE"));
			UE_LOG(LogAquaticTowerDefense, Display, TEXT("PERF DONE"));
			return false;
		}
		break;
	default:
		break;
	}
	return true;
}

#endif // !UE_BUILD_SHIPPING
