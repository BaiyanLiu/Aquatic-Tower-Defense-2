// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDCameraPawn.h"
#include "TDPlayerController.h"
#include "TDTypes.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Misc/App.h"

namespace
{
	constexpr float CameraYaw = 45.0f;
	constexpr float CameraPitch = -35.0f;

	/** How far past the board edge the view centre may go (the board is 1300 x 1800 units) */
	constexpr float PanMargin = 150.0f;
}

ATDCameraPawn::ATDCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->SetUsingAbsoluteRotation(true);
	SpringArm->SetRelativeRotation(FRotator(CameraPitch, CameraYaw, 0.0f));
	SpringArm->TargetArmLength = 2600.0f;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->SetProjectionMode(ECameraProjectionMode::Orthographic);
	Camera->SetOrthoWidth(DefaultOrthoWidth);
	TargetOrthoWidth = DefaultOrthoWidth;

	SetCanBeDamaged(false);
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void ATDCameraPawn::AddZoom(float Amount, bool bTowardsCursor)
{
	TargetOrthoWidth = FMath::Clamp(TargetOrthoWidth * (1.0f - Amount * 0.1f), MinOrthoWidth, MaxOrthoWidth);
	bZoomAtCursor = bTowardsCursor;
}

void ATDCameraPawn::ResetView()
{
	TargetOrthoWidth = HomeOrthoWidth;
	bZoomAtCursor = false;
	SetActorLocation(HomeLocation);
}

void ATDCameraPawn::FrameMapEnds()
{
	// same zoom on every map; the view centres between the trench and the reef
	const FVector Spawn = TD::CellToWorld(TD::SpawnCell());
	const FVector Goal = TD::CellToWorld(TD::GoalCell());
	HomeOrthoWidth = DefaultOrthoWidth;
	HomeLocation = (Spawn + Goal) * 0.5f;
	HomeLocation.Z = 0.0f;
}

void ATDCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// real time, unaffected by game speed and pause
	const float RealDelta = FMath::Min((float)FApp::GetDeltaTime(), 0.1f);

	// a new board (new game, restart, load): start looking at its trench and reef
	if (FramedEpoch != TD::BoardEpoch() && GetController())
	{
		FramedEpoch = TD::BoardEpoch();
		FrameMapEnds();
		ResetView();
		Camera->SetOrthoWidth(HomeOrthoWidth);
	}

	ATDPlayerController* PC = Cast<ATDPlayerController>(GetController());
	const bool bInputAllowed = PC && !PC->IsMenuOpen() && !PC->IsSimulatingInput();

	const FVector Forward = FRotator(0, CameraYaw, 0).Vector();
	const FVector Right = FRotator(0, CameraYaw + 90.0f, 0).Vector();
	const float GroundPerScreenY = 1.0f / FMath::Sin(FMath::DegreesToRadians(-CameraPitch));

	int32 ViewX = 0, ViewY = 0;
	float MouseX = 0.0f, MouseY = 0.0f;
	bool bHasMouse = false;
	if (PC)
	{
		PC->GetViewportSize(ViewX, ViewY);
		bHasMouse = PC->GetMousePosition(MouseX, MouseY);
	}

	FVector Location = GetActorLocation();

	// smooth zoom; wheel zoom moves the view so the point under the cursor stays put
	const float OldWidth = Camera->OrthoWidth;
	const float NewWidth = FMath::FInterpTo(OldWidth, TargetOrthoWidth, RealDelta, 12.0f);
	Camera->SetOrthoWidth(NewWidth);
	TD::SetViewWidth(NewWidth);
	if (bZoomAtCursor && bHasMouse && ViewX > 0 && ViewY > 0 && OldWidth > 0.0f)
	{
		const float OldUnitsPerPixel = OldWidth / ViewX;
		const FVector CursorOffset = Right * (MouseX - ViewX * 0.5f) * OldUnitsPerPixel - Forward * (MouseY - ViewY * 0.5f) * OldUnitsPerPixel * GroundPerScreenY;
		Location += CursorOffset * (1.0f - NewWidth / OldWidth);
	}

	if (!bInputAllowed)
	{
		ClampAndApply(Location);
		return;
	}

	FVector Move = FVector::ZeroVector;

	// keyboard pan (WASD and the arrows only move the camera; no rebindable action may use them)
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))		{ Move += Forward; }
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))	{ Move -= Forward; }
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right))	{ Move += Right; }
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))	{ Move -= Right; }

	// edge pan
	if (bHasMouse && ViewX > 0 && ViewY > 0)
	{
		if (MouseX <= EdgePanMargin)				{ Move -= Right; }
		if (MouseX >= ViewX - 1 - EdgePanMargin)	{ Move += Right; }
		if (MouseY <= EdgePanMargin)				{ Move += Forward; }
		if (MouseY >= ViewY - 1 - EdgePanMargin)	{ Move -= Forward; }
	}

	if (!Move.IsNearlyZero())
	{
		Location += Move.GetSafeNormal() * Camera->OrthoWidth * 0.75f * RealDelta;
	}

	ClampAndApply(Location);
}

void ATDCameraPawn::ClampAndApply(FVector Location)
{
	// the view centre stays over (or just past) the board
	const float LimitX = TD::Rows * TD::TileSize * 0.5f + PanMargin;
	const float LimitY = TD::Cols * TD::TileSize * 0.5f + PanMargin;
	Location.X = FMath::Clamp(Location.X, -LimitX, LimitX);
	Location.Y = FMath::Clamp(Location.Y, -LimitY, LimitY);
	Location.Z = 0.0f;
	SetActorLocation(Location);
}
