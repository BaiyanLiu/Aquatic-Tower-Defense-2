// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TDCameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 *  Isometric orthographic camera rig.
 *  Pans with WASD / arrows / screen edges and zooms with the mouse wheel (towards the cursor). Each new board starts
 *  at the same view (ResetView). Panning is limited so part of the board always stays in view.
 *  Uses real (undilated) time so it keeps working at any game speed and while paused.
 */
UCLASS()
class ATDCameraPawn : public APawn
{
	GENERATED_BODY()

public:

	ATDCameraPawn();

	virtual void Tick(float DeltaTime) override;

	/** Positive zooms in. Wheel zoom keeps the ground point under the cursor in place. */
	void AddZoom(float Amount, bool bTowardsCursor = true);

	/** Back to the start view of the current map */
	void ResetView();

	/** Start view of the current map: normal zoom, centred between the trench and the reef */
	void FrameMapEnds();

protected:

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MinOrthoWidth = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxOrthoWidth = 3000.0f;

	/** Starting zoom: closer than the whole board */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float DefaultOrthoWidth = 1300.0f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float EdgePanMargin = 6.0f;

	void ClampAndApply(FVector Location);

	float TargetOrthoWidth = 1300.0f;

	FVector HomeLocation = FVector::ZeroVector;
	float HomeOrthoWidth = 1300.0f;
	int32 FramedEpoch = -1;
	bool bZoomAtCursor = false;

};
