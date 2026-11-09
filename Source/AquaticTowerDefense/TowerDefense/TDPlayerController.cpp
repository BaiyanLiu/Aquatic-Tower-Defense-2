// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDPlayerController.h"
#include "TDGameMode.h"
#include "TDGrid.h"
#include "TDTower.h"
#include "TDHUD.h"
#include "TDCameraPawn.h"
#include "TDEnemy.h"
#include "TDDevTools.h"
#include "TDAudio.h"
#include "TDMastery.h"
#include "AquaticTowerDefense.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "SceneView.h"
#include "TimerManager.h"
#include "Misc/App.h"

ATDPlayerController::ATDPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
	bShouldPerformFullTickWhenPaused = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ATDPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (MappingContext)
			{
				Subsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}
}

UInputAction* ATDPlayerController::MakeAction(const TCHAR* Name, bool bAxis)
{
	UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
	Action->ValueType = bAxis ? EInputActionValueType::Axis1D : EInputActionValueType::Boolean;
	Action->bTriggerWhenPaused = true;
	Actions.Add(Action);
	return Action;
}

void ATDPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent);
	if (!Input)
	{
		UE_LOG(LogAquaticTowerDefense, Error, TEXT("Aquatic Tower Defense needs the Enhanced Input component class (see DefaultInput.ini)"));
		return;
	}

	// build the actions and mapping context in code so no input assets are required
	Actions.Reset();
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_AquaticTowerDefense"));

	// the tower keys share one action; UpdateKeyBindings maps the player's keys (and their numpad twins) to it
	UInputAction* Select = MakeAction(TEXT("IA_TD_Select"));
	SelectAction = Select;
	SelectKeys.Reset();
	for (int32 i = 0; i < (int32)ETDTowerType::Count; ++i)
	{
		for (const FKey& Key : TowerKeys(TDBind::DefaultKey(TDBind::TowerBind((ETDTowerType)i))))
		{
			MappingContext->MapKey(Select, Key);
			SelectKeys.Add(Key);
		}
	}

	UInputAction* Click = MakeAction(TEXT("IA_TD_Click"));
	MappingContext->MapKey(Click, EKeys::LeftMouseButton);

	UInputAction* Cancel = MakeAction(TEXT("IA_TD_Cancel"));
	MappingContext->MapKey(Cancel, EKeys::RightMouseButton);

	UInputAction* MenuKey = MakeAction(TEXT("IA_TD_Menu"));
	MappingContext->MapKey(MenuKey, EKeys::Escape);

	// rebindable actions start on their default keys; UpdateKeyBindings applies the player's choices
	UInputAction* Wave = MakeAction(TEXT("IA_TD_Wave"));
	UInputAction* Skills = MakeAction(TEXT("IA_TD_Skills"));
	UInputAction* StorageAction = MakeAction(TEXT("IA_TD_Storage"));
	UInputAction* Sell = MakeAction(TEXT("IA_TD_Sell"));
	UInputAction* Speed = MakeAction(TEXT("IA_TD_Speed"));
	UInputAction* Pause = MakeAction(TEXT("IA_TD_Pause"));
	UInputAction* ItemsAction = MakeAction(TEXT("IA_TD_Items"));
	UInputAction* TargetingAction = MakeAction(TEXT("IA_TD_Targeting"));
	BindActions = { Wave, Skills, StorageAction, Sell, Speed, Pause, ItemsAction, TargetingAction };	// same order as ETDBind
	MappedKeys.Reset();
	for (int32 i = 0; i < BindActions.Num(); ++i)
	{
		const FKey Key = TDBind::DefaultKey((ETDBind)i);
		MappingContext->MapKey(BindActions[i], Key);
		MappedKeys.Add(Key);
	}
	AppliedBindingsVersion = -1;

	UInputAction* Zoom = MakeAction(TEXT("IA_TD_Zoom"), true);
	MappingContext->MapKey(Zoom, EKeys::MouseWheelAxis);

	Input->BindAction(Select, ETriggerEvent::Started, this, &ATDPlayerController::OnSelectSlot);
	Input->BindAction(Click, ETriggerEvent::Started, this, &ATDPlayerController::OnClickStarted);
	Input->BindAction(Click, ETriggerEvent::Completed, this, &ATDPlayerController::OnClickCompleted);
	Input->BindAction(Cancel, ETriggerEvent::Started, this, &ATDPlayerController::OnCancel);
	Input->BindAction(Wave, ETriggerEvent::Started, this, &ATDPlayerController::OnWave);
	Input->BindAction(Skills, ETriggerEvent::Started, this, &ATDPlayerController::OnSkills);
	Input->BindAction(StorageAction, ETriggerEvent::Started, this, &ATDPlayerController::OnStorage);
	Input->BindAction(Sell, ETriggerEvent::Started, this, &ATDPlayerController::OnSell);
	Input->BindAction(Speed, ETriggerEvent::Started, this, &ATDPlayerController::OnSpeed);
	Input->BindAction(Pause, ETriggerEvent::Started, this, &ATDPlayerController::OnPause);
	Input->BindAction(ItemsAction, ETriggerEvent::Started, this, &ATDPlayerController::OnItems);
	Input->BindAction(TargetingAction, ETriggerEvent::Started, this, &ATDPlayerController::OnTargeting);
	Input->BindAction(MenuKey, ETriggerEvent::Started, this, &ATDPlayerController::OnMenuKey);
	Input->BindAction(Zoom, ETriggerEvent::Triggered, this, &ATDPlayerController::OnZoom);
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

ATDGameMode* ATDPlayerController::GetTDGameMode() const
{
	return GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
}

ATDHUD* ATDPlayerController::GetTDHUD() const
{
	return Cast<ATDHUD>(GetHUD());
}

bool ATDPlayerController::IsMenuOpen() const
{
	const ATDGameMode* GameMode = GetTDGameMode();
	return GameMode && GameMode->GetMenu() != ETDMenu::None;
}

ATDEnemy* ATDPlayerController::PickEnemyAtMouse() const
{
	const ATDGameMode* GameMode = GetTDGameMode();
	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	FVector2D Cursor;
	if (!GameMode || GameMode->GetEnemies().Num() == 0 || !LocalPlayer || !LocalPlayer->ViewportClient || !GetCursorPosition(Cursor))
	{
		return nullptr;
	}

	// the view is worked out once: this runs every frame over every creature
	FSceneViewProjectionData Projection;
	if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, Projection))
	{
		return nullptr;
	}
	const FMatrix ViewProjection = Projection.ComputeViewProjectionMatrix();
	const FIntRect ViewRect = Projection.GetConstrainedViewRect();

	int32 ViewX = 0, ViewY = 0;
	GetViewportSize(ViewX, ViewY);
	const float PickRadius = FMath::Max(22.0f, ViewY / 1080.0f * 34.0f);

	ATDEnemy* Best = nullptr;
	float BestDistance = PickRadius;
	for (ATDEnemy* Enemy : GameMode->GetEnemies())
	{
		FVector2D Screen;
		if (Enemy && Enemy->IsAlive() && FSceneView::ProjectWorldToScreen(Enemy->GetAimLocation(), ViewRect, ViewProjection, Screen))
		{
			const float Distance = FVector2D::Distance(Screen, Cursor);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Enemy;
			}
		}
	}
	return Best;
}

bool ATDPlayerController::GetCursorPosition(FVector2D& OutPosition) const
{
	if (SimCursor.IsSet())
	{
		OutPosition = SimCursor.GetValue();
		return true;
	}
	float X = 0.0f, Y = 0.0f;
	if (GetMousePosition(X, Y))
	{
		OutPosition = FVector2D(X, Y);
		return true;
	}
	return false;
}

bool ATDPlayerController::IsLeftMouseHeld() const
{
	return SimCursor.IsSet() ? bSimLeftDown : IsInputKeyDown(EKeys::LeftMouseButton);
}

void ATDPlayerController::SimLeftPress()
{
	bSimLeftDown = true;
	OnClickStarted(FInputActionValue());
}

void ATDPlayerController::SimLeftRelease()
{
	bSimLeftDown = false;
	OnClickCompleted(FInputActionValue());
}

void ATDPlayerController::SimRightClick()
{
	OnCancel(FInputActionValue());
}

bool ATDPlayerController::GetMouseGroundPoint(FVector& OutPoint) const
{
	FVector2D Cursor;
	FVector Origin, Direction;
	if (!GetCursorPosition(Cursor) || !DeprojectScreenPositionToWorld(Cursor.X, Cursor.Y, Origin, Direction) || Direction.Z > -KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// intersect with the seabed plane (z = 0)
	const float T = -Origin.Z / Direction.Z;
	OutPoint = Origin + Direction * T;
	return true;
}

bool ATDPlayerController::GetHoveredCell(FIntPoint& OutCell) const
{
	FVector Point;
	if (!GetMouseGroundPoint(Point))
	{
		return false;
	}
	OutCell = TD::WorldToCell(Point);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------------------------

void ATDPlayerController::SelectTowerType(ETDTowerType Type)
{
	// No Walls (mutator): the wall can't be picked at all
	ATDGameMode* GameMode = GetTDGameMode();
	if (Type == ETDTowerType::Wall && !TD::Run().CanBuildWalls())
	{
		if (GameMode)
		{
			GameMode->Toast(TEXT("No Walls: build the maze from towers"), FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		}
		SelectedType = ETDTowerType::Count;
		return;
	}
	// pressing the same hotkey again deselects
	SelectedType = (SelectedType == Type) ? ETDTowerType::Count : Type;
	SelectedTower = nullptr;
}

void ATDPlayerController::SelectPlacedTower(ATDTower* Tower)
{
	SelectedTower = Tower;
	SelectedType = ETDTowerType::Count;
	if (Tower && !Tower->CanProgress())
	{
		PanelTab = 0;
	}
}

void ATDPlayerController::ClearSelection()
{
	SelectedType = ETDTowerType::Count;
	SelectedTower = nullptr;
	SelectedEnemy = nullptr;
	PendingSell = nullptr;
}

// ---------------------------------------------------------------------------------------------
// Tick: hover preview and range ring
// ---------------------------------------------------------------------------------------------

void ATDPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// the boss music crossfade runs in real time, also while paused or at another game speed
	TDAudio::TickMusic(FApp::GetDeltaTime());

	if (DevTask.IsValid() && !DevTask->Tick(FApp::GetDeltaTime()))
	{
		DevTask.Reset();
	}

	if (PendingShotTime > 0.0f)
	{
		PendingShotTime -= FApp::GetDeltaTime();
		if (PendingShotTime <= 0.0f)
		{
			ConsoleCommand(TEXT("Shot showui"));
		}
	}

	UpdateKeyBindings();

	ATDGameMode* GameMode = GetTDGameMode();
	ATDGrid* Grid = GameMode ? GameMode->GetGrid() : nullptr;
	if (!Grid)
	{
		return;
	}

	FVector2D Cursor;
	const bool bHasMouse = GetCursorPosition(Cursor);
	const float MouseX = Cursor.X, MouseY = Cursor.Y;
	const ATDHUD* HUD = GetTDHUD();
	const bool bOverUI = bHasMouse && HUD && HUD->IsOverUI(FVector2D(MouseX, MouseY));

	FIntPoint Cell;
	const bool bHover = bHasMouse && !bOverUI && !GameMode->IsGameOver() && !IsMenuOpen() && GetHoveredCell(Cell);
	HoveredEnemy = (bHasMouse && !bOverUI && !IsMenuOpen()) ? PickEnemyAtMouse() : nullptr;
	if (SelectedEnemy.IsValid() && !SelectedEnemy->IsAlive())
	{
		SelectedEnemy = nullptr;
	}

	if (HasSelectedType() && bHover)
	{
		Grid->UpdatePreview(true, Cell, SelectedType);
	}
	else
	{
		Grid->UpdatePreview(false, FIntPoint(-1, -1), SelectedType);
	}

	UpdateRangeIndicators(GameMode, Grid);
}

void ATDPlayerController::UpdateRangeIndicators(ATDGameMode* GameMode, ATDGrid* Grid)
{
	// main indicator: the placement ghost, else the selected tower (live range, so levels / items / skills show at once)
	TArray<FTDRangeView> Views;
	FTDRangeView Main;
	const ATDTower* Selected = SelectedTower.Get();
	Grid->SetSelectedCell(Selected ? Selected->GetCell() : FIntPoint(-1, -1));
	if (Grid->GetPreviewRangeView(Main))
	{
		Views.Add(Main);
	}
	else if (Selected && Selected->GetRange() > 0.0f)
	{
		Main.Center = Selected->GetActorLocation();
		Main.RangeTiles = Selected->GetRange();
		Main.Color = Selected->IsSupportTower() ? TDRangeColors::Aura : TDRangeColors::Selected;
		Main.bTintTiles = true;
		Views.Add(Main);
	}

	// every tower's range while Alt is held or the setting is on
	const bool bShowAll = !IsMenuOpen() && (GameMode->IsShowingAllRanges() || IsAltHeld());
	if (bShowAll)
	{
		for (const ATDTower* Tower : Grid->GetTowers())
		{
			if (Tower && Tower != Selected && Tower->GetRange() > 0.0f)
			{
				FTDRangeView View;
				View.Center = Tower->GetActorLocation();
				View.RangeTiles = Tower->GetRange();
				View.Color = Tower->IsSupportTower() ? TDRangeColors::Aura : TDRangeColors::Other;
				Views.Add(View);
			}
		}
	}

	Grid->SetRangeViews(Views);
}

// ---------------------------------------------------------------------------------------------
// Input handlers
// ---------------------------------------------------------------------------------------------

void ATDPlayerController::OnSelectSlot(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}

	ATDGameMode* GameMode = GetTDGameMode();
	if (!GameMode)
	{
		return;
	}
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 i = 0; i < (int32)ETDTowerType::Count; ++i)
		{
			for (const FKey& Key : TowerKeys(GameMode->GetKey(TDBind::TowerBind((ETDTowerType)i))))
			{
				if (Pass == 0 ? WasInputKeyJustPressed(Key) : IsInputKeyDown(Key))
				{
					SelectTowerType((ETDTowerType)i);
					return;
				}
			}
		}
	}
}

void ATDPlayerController::OnClickStarted(const FInputActionValue& Value)
{
	FVector2D Cursor;
	if (!GetCursorPosition(Cursor))
	{
		return;
	}

	// HUD buttons first
	if (ATDHUD* HUD = GetTDHUD())
	{
		if (HUD->HandleClick(Cursor))
		{
			return;
		}
	}

	ATDGameMode* GameMode = GetTDGameMode();
	if (!GameMode || !GameMode->GetGrid() || GameMode->IsGameOver() || IsMenuOpen())
	{
		return;
	}

	FVector GroundPoint;
	if (GetMouseGroundPoint(GroundPoint) && GameMode->TryClickPickup(GroundPoint))
	{
		return;
	}

	// clicking a creature (when not building) shows its details
	if (!HasSelectedType())
	{
		if (ATDEnemy* Enemy = PickEnemyAtMouse())
		{
			SelectedEnemy = Enemy;
			SelectedTower = nullptr;
			return;
		}
	}
	SelectedEnemy = nullptr;

	FIntPoint Cell;
	if (!GetHoveredCell(Cell) || !TD::InBounds(Cell))
	{
		SelectedTower = nullptr;
		return;
	}

	// with a tower picked, clicking a wall builds the tower onto it
	if (HasSelectedType() && GameMode->GetGrid()->CanBuildOnWall(Cell, SelectedType))
	{
		GameMode->TryPlaceTower(Cell, SelectedType, false, true);
		return;
	}

	ATDTower* Existing = GameMode->GetGrid()->GetTowerAt(Cell);
	if (Existing)
	{
		SelectPlacedTower(Existing);
		return;
	}

	if (HasSelectedType())
	{
		GameMode->TryPlaceTower(Cell, SelectedType, false);	// one click, one tower (no dragging out a line)
	}
	else
	{
		SelectedTower = nullptr;
	}
}

void ATDPlayerController::OnClickCompleted(const FInputActionValue& Value)
{
	if (ATDHUD* HUD = GetTDHUD())
	{
		HUD->EndDrag();
	}
}

void ATDPlayerController::OnCancel(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}

	// right click disarms the Sell button
	if (IsSellConfirmOpen())
	{
		CancelSell();
		return;
	}

	FVector2D Cursor;
	if (GetCursorPosition(Cursor))
	{
		if (ATDHUD* HUD = GetTDHUD())
		{
			if (HUD->HandleRightClick(Cursor))
			{
				return;
			}
		}
	}

	if (bStorageOpen && !HasSelectedType() && !SelectedTower.IsValid())
	{
		bStorageOpen = false;
		return;
	}
	ClearSelection();
}

void ATDPlayerController::OnWave(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}
	CallWave();
}

void ATDPlayerController::OnSkills(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}
	ToggleSkillsTab();
}

void ATDPlayerController::OnTargeting(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}
	CycleTargeting(1);
}

void ATDPlayerController::CycleTargeting(int32 Step)
{
	ATDGameMode* GameMode = GetTDGameMode();
	ATDTower* Tower = SelectedTower.Get();
	if (!GameMode || !Tower || !Tower->CanChooseTargeting())
	{
		return;
	}
	const ETDTargeting Mode = TD::CycleTargeting(Tower->GetTargeting(), Step);
	// (no toast: the tower panel's selector shows the new priority)
	GameMode->SetTowerTargeting(Tower, Mode);
}

void ATDPlayerController::OnItems(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}
	ToggleItemsTab();
}

void ATDPlayerController::OnStorage(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}
	ToggleStorage();
}

void ATDPlayerController::OnSell(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}
	SellSelected();
}

void ATDPlayerController::OnSpeed(const FInputActionValue& Value)
{
	if (IsMenuOpen())
	{
		return;
	}
	CycleSpeed();
}

void ATDPlayerController::OnPause(const FInputActionValue& Value)
{
	TogglePause();
}

TArray<FKey, TInlineAllocator<2>> ATDPlayerController::TowerKeys(const FKey& Key)
{
	// a tower on a number row key also answers to the same key on the numpad
	static const TPair<FKey, FKey> Twins[] =
	{
		{ EKeys::One, EKeys::NumPadOne }, { EKeys::Two, EKeys::NumPadTwo }, { EKeys::Three, EKeys::NumPadThree },
		{ EKeys::Four, EKeys::NumPadFour }, { EKeys::Five, EKeys::NumPadFive }, { EKeys::Six, EKeys::NumPadSix },
		{ EKeys::Seven, EKeys::NumPadSeven }, { EKeys::Eight, EKeys::NumPadEight }, { EKeys::Nine, EKeys::NumPadNine },
		{ EKeys::Zero, EKeys::NumPadZero }, { EKeys::Hyphen, EKeys::Subtract },
	};
	TArray<FKey, TInlineAllocator<2>> Keys;
	Keys.Add(Key);
	for (const TPair<FKey, FKey>& Twin : Twins)
	{
		if (Twin.Key == Key)
		{
			Keys.Add(Twin.Value);
		}
	}
	return Keys;
}

bool ATDPlayerController::IsAltHeld() const
{
	if (bDevHoldAlt)
	{
		return true;
	}
	const ATDGameMode* GameMode = GetTDGameMode();
	const FKey Key = GameMode ? GameMode->GetKey(ETDBind::Info) : EKeys::LeftAlt;
	if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt)
	{
		return IsInputKeyDown(EKeys::LeftAlt) || IsInputKeyDown(EKeys::RightAlt);
	}
	return IsInputKeyDown(Key);
}

void ATDPlayerController::UpdateKeyBindings()
{
	ATDGameMode* GameMode = GetTDGameMode();
	if (!GameMode || !MappingContext || BindActions.Num() != TDBind::ActionCount)
	{
		return;
	}

	// controls menu waiting for a key
	if (GameMode->GetRebinding() >= 0)
	{
		for (const FKey& Key : TDBind::BindableKeys())
		{
			if (WasInputKeyJustPressed(Key) || (Key == EKeys::LeftAlt && WasInputKeyJustPressed(EKeys::RightAlt)))
			{
				GameMode->SetKey((ETDBind)GameMode->GetRebinding(), Key);
				GameMode->StopRebinding();
				break;
			}
		}
	}

	if (GameMode->GetBindingsVersion() == 0 || AppliedBindingsVersion == GameMode->GetBindingsVersion())
	{
		return;
	}
	AppliedBindingsVersion = GameMode->GetBindingsVersion();
	for (int32 i = 0; i < BindActions.Num(); ++i)
	{
		const FKey Wanted = GameMode->GetKey((ETDBind)i);
		if (MappedKeys[i] != Wanted)
		{
			MappingContext->UnmapKey(BindActions[i], MappedKeys[i]);
			MappingContext->MapKey(BindActions[i], Wanted);
			MappedKeys[i] = Wanted;
		}
	}
	if (SelectAction)
	{
		for (const FKey& Key : SelectKeys)
		{
			MappingContext->UnmapKey(SelectAction, Key);
		}
		SelectKeys.Reset();
		for (int32 i = 0; i < (int32)ETDTowerType::Count; ++i)
		{
			for (const FKey& Key : TowerKeys(GameMode->GetKey(TDBind::TowerBind((ETDTowerType)i))))
			{
				MappingContext->MapKey(SelectAction, Key);
				SelectKeys.Add(Key);
			}
		}
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->RequestRebuildControlMappings();
	}
}

void ATDPlayerController::OnMenuKey(const FInputActionValue& Value)
{
	ATDGameMode* GameMode = GetTDGameMode();
	if (!GameMode)
	{
		return;
	}
	// Esc answers an open question with No
	if (GameMode->GetPendingConfirm() != ETDConfirm::None)
	{
		GameMode->AnswerConfirm(false);
		return;
	}
	if (GameMode->GetRebinding() >= 0)
	{
		GameMode->StopRebinding();
		return;
	}

	// Esc backs out of things step by step: sell confirmation -> settings -> menu -> selection -> opens the pause menu
	if (IsSellConfirmOpen() && GameMode->GetMenu() == ETDMenu::None)
	{
		CancelSell();
		return;
	}
	switch (GameMode->GetMenu())
	{
	case ETDMenu::Settings:	GameMode->CloseSettings(); return;
	case ETDMenu::Controls:	GameMode->CloseControls(); return;
	case ETDMenu::Pause:	GameMode->CloseMenu(); return;
	case ETDMenu::Main:		return;
	case ETDMenu::Victory:	return;		// answered with its buttons
	case ETDMenu::Setup:	GameMode->CloseNewGameSetup(); return;
	case ETDMenu::Boon:		GameMode->OpenMenu(ETDMenu::Pause); return;	// the offer comes back when the pause menu closes
	case ETDMenu::Mastery:	GameMode->OpenMenu(ETDMenu::Main); return;
	case ETDMenu::Slots:	GameMode->CloseSlotPicker(); return;
	default: break;
	}

	if (HasSelectedType() || SelectedTower.IsValid() || SelectedEnemy.IsValid())
	{
		ClearSelection();
		return;
	}
	TogglePause();
}

void ATDPlayerController::OnZoom(const FInputActionValue& Value)
{
	if (ATDCameraPawn* CameraPawn = Cast<ATDCameraPawn>(GetPawn()))
	{
		CameraPawn->AddZoom(Value.Get<float>());
	}
}

// ---------------------------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------------------------

void ATDPlayerController::ToggleSkillsTab()
{
	// aura towers have skills too; only Coral Walls have no tabs
	if (SelectedTower.IsValid() && SelectedTower->CanProgress())
	{
		PanelTab = (PanelTab == 2) ? 0 : 2;
	}
}

void ATDPlayerController::ToggleItemsTab()
{
	// same as the Skills key: towers that hold items only, back to Stats when Items is already showing
	if (SelectedTower.IsValid() && SelectedTower->CanProgress())
	{
		PanelTab = (PanelTab == 1) ? 0 : 1;
	}
}

void ATDPlayerController::LearnSkill(int32 SkillIndex)
{
	ATDGameMode* GameMode = GetTDGameMode();
	if (GameMode && SelectedTower.IsValid())
	{
		GameMode->TryLearnSkill(SelectedTower.Get(), SkillIndex);
	}
}

int32 ATDPlayerController::GetArmedForm() const
{
	const bool bLive = ArmedForm != 0 && ArmedFormTower.IsValid() && ArmedFormTower == SelectedTower
		&& FPlatformTime::Seconds() - ArmedFormTime <= FormConfirmWindow;
	return bLive ? ArmedForm : 0;
}

void ATDPlayerController::ClickForm(int32 Form)
{
	ATDGameMode* GameMode = GetTDGameMode();
	ATDTower* Tower = SelectedTower.Get();
	if (!GameMode || !Tower)
	{
		return;
	}

	// the choice is permanent, so the first click only arms it
	if (GetArmedForm() != Form)
	{
		ArmedForm = Form;
		ArmedFormTower = Tower;
		ArmedFormTime = FPlatformTime::Seconds();
		return;
	}
	ArmedForm = 0;
	ArmedFormTower = nullptr;
	GameMode->TryChooseForm(Tower, Form);
}

void ATDPlayerController::EquipStorageItem(int32 StorageIndex)
{
	ATDGameMode* GameMode = GetTDGameMode();
	if (!GameMode)
	{
		return;
	}
	const TArray<FTDItem>& Storage = GameMode->GetStorage();
	if (!Storage.IsValidIndex(StorageIndex))
	{
		return;
	}

	// a click picks the item for the workbench; a quick second click on it equips it
	const double Now = GetWorld()->GetRealTimeSeconds();
	const bool bDoubleClick = WorkbenchItemId == Storage[StorageIndex].Id && Now - LastStorageClickTime < 0.35;
	LastStorageClickTime = Now;
	WorkbenchItemId = Storage[StorageIndex].Id;
	if (bDoubleClick)
	{
		EquipWorkbenchItem();
	}
}

void ATDPlayerController::UnequipSlot(ETDItemSlot Slot)
{
	ATDGameMode* GameMode = GetTDGameMode();
	if (GameMode && SelectedTower.IsValid())
	{
		GameMode->UnequipToStorage(SelectedTower.Get(), Slot);
	}
}

void ATDPlayerController::SalvageStorageItem(int32 StorageIndex)
{
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->SalvageItem(StorageIndex);
	}
}

int32 ATDPlayerController::GetWorkbenchIndex() const
{
	const ATDGameMode* GameMode = GetTDGameMode();
	if (!GameMode || WorkbenchItemId == 0)
	{
		return -1;
	}
	return GameMode->GetStorage().IndexOfByPredicate([this](const FTDItem& Item) { return Item.Id == WorkbenchItemId; });
}

void ATDPlayerController::EquipWorkbenchItem()
{
	ATDGameMode* GameMode = GetTDGameMode();
	if (!GameMode || GetWorkbenchIndex() < 0)
	{
		return;
	}
	if (!SelectedTower.IsValid() || !SelectedTower->CanProgress())
	{
		GameMode->Toast(TEXT("Select a tower to equip items"), FLinearColor(0.7f, 0.9f, 1.0f), ETDToast::Refusal);
		return;
	}
	if (GameMode->EquipFromStorage(SelectedTower.Get(), GetWorkbenchIndex()))
	{
		WorkbenchItemId = 0;
		PanelTab = 1;
	}
}

void ATDPlayerController::UpgradeWorkbenchItem()
{
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->UpgradeStorageItem(GetWorkbenchIndex());
	}
}

void ATDPlayerController::RerollWorkbenchItem()
{
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->RerollStorageItem(GetWorkbenchIndex());
	}
}

void ATDPlayerController::SalvageWorkbenchItem()
{
	ATDGameMode* GameMode = GetTDGameMode();
	if (GameMode && GameMode->SalvageItem(GetWorkbenchIndex()))
	{
		WorkbenchItemId = 0;
	}
}

void ATDPlayerController::ToggleLockWorkbenchItem()
{
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->ToggleItemLock(GetWorkbenchIndex());
	}
}

void ATDPlayerController::SortStorage()
{
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->SortStorage();
	}
}

void ATDPlayerController::SellSelected(bool bFromClick)
{
	// the Sell key (or button) pressed again while the button is armed sells
	if (IsSellConfirmOpen())
	{
		ConfirmSell();
		return;
	}

	ATDGameMode* GameMode = GetTDGameMode();
	ATDTower* Tower = SelectedTower.Get();
	if (!GameMode || !Tower || GameMode->IsGameOver())
	{
		return;
	}
	// Fixed Towers (mutator): only in the build phase it was bought in
	FString Reason;
	if (!GameMode->CanSellTower(Tower, &Reason))
	{
		GameMode->Toast(Reason, FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		return;
	}

	// walls are cheap and have no progress to lose, and a sell that storage can't take is refused (with its toast) straight away
	if (!Tower->CanProgress() || Tower->GetEquippedCount() > GameMode->GetStorageFree())
	{
		if (GameMode->SellTower(Tower))
		{
			SelectedTower = nullptr;
		}
		return;
	}

	PendingSell = Tower;
	PendingSellTime = FPlatformTime::Seconds();
	bSellArmedByClick = bFromClick;
}

void ATDPlayerController::ConfirmSell()
{
	ATDGameMode* GameMode = GetTDGameMode();
	ATDTower* Tower = PendingSell.Get();
	PendingSell = nullptr;
	if (GameMode && Tower && GameMode->SellTower(Tower))
	{
		if (SelectedTower.Get() == Tower)
		{
			SelectedTower = nullptr;
		}
	}
}

void ATDPlayerController::TogglePause()
{
	ATDGameMode* GameMode = GetTDGameMode();
	if (!GameMode || GameMode->IsGameOver())
	{
		return;
	}

	// pausing opens the pause menu (resume, save, load, restart, settings, main menu)
	if (GameMode->GetMenu() == ETDMenu::None)
	{
		GameMode->OpenMenu(ETDMenu::Pause);
	}
	else if (GameMode->GetMenu() == ETDMenu::Pause)
	{
		GameMode->CloseMenu();
	}
}

void ATDPlayerController::CycleSpeed()
{
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->CycleGameSpeed();
	}
}

void ATDPlayerController::CallWave()
{
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->ReleaseNextWave();
	}
}

void ATDPlayerController::ATDDemo(int32 Waves, float ScreenshotDelay)
{
#if !UE_BUILD_SHIPPING
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		// the demo maze is laid out for the first map
		GameMode->ForceMap(0);
		if (TD::GetMap() != 0 && GameMode->GetGrid())
		{
			TD::SetMap(0);
			GameMode->GetGrid()->ResetBoard();
		}
		GameMode->SetupDemo(FMath::Max(0, Waves));

		// show the new UI in the screenshot: select the itemized harpoon and open storage
		if (ATDTower* Tower = GameMode->GetGrid() ? GameMode->GetGrid()->GetTowerAt(FIntPoint(5, 8)) : nullptr)
		{
			SelectPlacedTower(Tower);
			PanelTab = 0;
		}
		bStorageOpen = true;
	}

	if (ScreenshotDelay > 0.0f)
	{
		// first shot: tower stats + storage; second shot: a creature's details panel (the boss if there is one)
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			ConsoleCommand(TEXT("Shot showui"));
		}), ScreenshotDelay, false);

		FTimerHandle Handle2;
		GetWorldTimerManager().SetTimer(Handle2, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			ClearSelection();
			bStorageOpen = false;
			if (const ATDGameMode* GameMode = GetTDGameMode())
			{
				for (ATDEnemy* Enemy : GameMode->GetEnemies())
				{
					if (Enemy && Enemy->IsAlive() && (!SelectedEnemy.IsValid() || Enemy->GetDef().bBoss))
					{
						SelectedEnemy = Enemy;
					}
				}
			}
			ConsoleCommand(TEXT("Shot showui"));
		}), ScreenshotDelay * 2.0f, false);
	}
#endif
}

void ATDPlayerController::ATDBoons(int32 ForceCursed)
{
#if !UE_BUILD_SHIPPING
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->DebugOfferBoons(ForceCursed != 0);
	}
#else
	(void)ForceCursed;
#endif
}

void ATDPlayerController::ATDPickBoon(int32 Boon)
{
#if !UE_BUILD_SHIPPING
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		if (Boon >= 0 && Boon < TD::BoonCount)
		{
			GameMode->DebugOfferBoons(false);
			GameMode->DebugSetLastOffer(Boon);
		}
		if (GameMode->GetBoonOffer().Num() > 0)
		{
			GameMode->PickBoon(GameMode->GetBoonOffer().Last());
		}
	}
#else
	(void)Boon;
#endif
}

void ATDPlayerController::ATDUniques()
{
#if !UE_BUILD_SHIPPING
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->DebugAddUniques();
		bStorageOpen = true;
	}
#endif
}

void ATDPlayerController::ATDWaveCard()
{
#if !UE_BUILD_SHIPPING
	if (const ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->ShowWaveClearCard(FMath::Max(1, GameMode->GetWave()), 16, 9, 4, 1);
	}
#endif
}

void ATDPlayerController::ATDGameOver()
{
#if !UE_BUILD_SHIPPING
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->DebugGameOver();
	}
#endif
}

void ATDPlayerController::ATDAbyss(int32 Tier)
{
#if !UE_BUILD_SHIPPING
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->DebugStartAbyss(Tier);
	}
#endif
}

void ATDPlayerController::ATDSetWave(int32 Wave, int32 bRelease)
{
#if !UE_BUILD_SHIPPING
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->DebugSetWave(Wave);
		if (bRelease)
		{
			GameMode->ReleaseNextWave();
		}
	}
#endif
}

void ATDPlayerController::ATDMastery(int32 TowerType, int32 Kills)
{
#if !UE_BUILD_SHIPPING
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->DebugSetMasteryKills((ETDTowerType)FMath::Clamp(TowerType, 0, TD::TowerTypeCount - 1), Kills);
	}
#endif
}

void ATDPlayerController::ATDShot(float Delay)
{
#if !UE_BUILD_SHIPPING
	// counted down in PlayerTick, which also runs while paused (timers don't)
	PendingShotTime = FMath::Max(0.1f, Delay);
#endif
}

void ATDPlayerController::ATDPreview(int32 TowerType, int32 X, int32 Y)
{
#if !UE_BUILD_SHIPPING
	ClearSelection();
	bStorageOpen = false;
	SelectTowerType((ETDTowerType)FMath::Clamp(TowerType, 0, TD::TowerTypeCount - 1));
	FVector2D Screen;
	if (ProjectWorldLocationToScreen(TD::CellToWorld(FIntPoint(X, Y)), Screen))
	{
		SimSetCursor(Screen);
	}
#endif
}

void ATDPlayerController::ATDMenu(int32 Menu)
{
#if !UE_BUILD_SHIPPING
	if (ATDGameMode* GameMode = GetTDGameMode())
	{
		GameMode->OpenMenu((ETDMenu)FMath::Clamp(Menu, 0, (int32)ETDMenu::Slots));
	}
#endif
}
