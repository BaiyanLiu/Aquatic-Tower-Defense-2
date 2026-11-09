// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDTypes.h"
#include "TDGrid.generated.h"

class ATDTower;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UInstancedStaticMeshComponent;

/** One range indicator to draw: a ring at the range, a soft disc inside, optionally tinted tiles */
struct FTDRangeView
{
	FVector Center = FVector::ZeroVector;
	float RangeTiles = 0.0f;
	FLinearColor Color = FLinearColor::White;
	bool bTintTiles = false;
};

namespace TDRangeColors
{
	inline const FLinearColor Selected(1.0f, 0.78f, 0.2f);		// gold
	inline const FLinearColor ValidGhost(0.3f, 1.0f, 0.45f);	// green
	inline const FLinearColor InvalidGhost(1.0f, 0.25f, 0.2f);	// red
	inline const FLinearColor Aura(0.7f, 0.45f, 1.0f);			// purple
	inline const FLinearColor Other(0.45f, 0.9f, 1.0f);			// cyan, "show all ranges"
}

/**
 *  The seabed board.
 *  Owns cell occupancy, the BFS distance field ground enemies follow,
 *  placement validation, the board visuals and the route / placement previews.
 */
UCLASS()
class ATDGrid : public AActor
{
	GENERATED_BODY()

public:

	ATDGrid();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ---- Cells ----

	bool IsBlocked(const FIntPoint& Cell) const;
	ATDTower* GetTowerAt(const FIntPoint& Cell) const;
	const TArray<TObjectPtr<ATDTower>>& GetTowers() const { return Towers; }

	/** Checks whether a tower can go on the given tile. Optionally returns the resulting distance field. */
	ETDPlaceResult EvaluatePlacement(const FIntPoint& Cell, TArray<int32>* OutField = nullptr) const;

	/** Spawns a tower on an already validated tile and re-routes everything */
	ATDTower* PlaceTower(const FIntPoint& Cell, ETDTowerType Type);

	/** Removes and destroys a tower, then re-routes */
	void RemoveTower(ATDTower* Tower);

	/** True if a tower of this type may be built straight onto the Coral Wall on this tile (the path doesn't change) */
	bool CanBuildOnWall(const FIntPoint& Cell, ETDTowerType Type) const;

	/** Swaps the wall on a tile for a new tower (validated with CanBuildOnWall). The tile stays blocked, so nothing re-routes. */
	ATDTower* ReplaceWall(const FIntPoint& Cell, ETDTowerType Type);

	/** Destroys every tower and restores the starting board */
	void ResetBoard();

	/** Which neighbors of a tile are walls (see ATDTower::SetWallLinks) */
	uint8 GetWallLinks(const FIntPoint& Cell) const;

	// ---- Flow field ----

	/** Steps to the goal from this cell, -1 if unreachable */
	int32 GetDistance(const FIntPoint& Cell) const;

	/** Picks the next cell towards the goal, preferring to continue straight on ties */
	bool GetNextStep(const FIntPoint& From, const FIntPoint& PrevDir, FIntPoint& OutNext) const;

	/** Current spawn-to-goal route, in tiles */
	int32 GetRouteLength() const { return FMath::Max(0, Route.Num() - 1); }

	// ---- Previews ----

	/** Updates the hover highlight, ghost tower, range ring and route preview */
	void UpdatePreview(bool bActive, const FIntPoint& Cell, ETDTowerType Type);

	/** Draws these range indicators this frame (the first may tint the tiles in range); an empty list hides them */
	void SetRangeViews(const TArray<FTDRangeView>& Views);

	/** Marks the selected tower's tile with a glowing frame (Cell outside the board hides it) */
	void SetSelectedCell(const FIntPoint& Cell);

	/** Range indicator of the placement ghost, if one should be shown */
	bool GetPreviewRangeView(FTDRangeView& OutView) const;

	/** For the UI test: how many range indicators are visible, and the radius (tiles) of the first */
	int32 GetVisibleRangeCount() const;
	float GetFirstRangeTiles() const;
	int32 GetTintedTileCount() const;

	bool IsPreviewActive() const { return bPreviewActive; }
	ETDPlaceResult GetPreviewResult() const { return PreviewResult; }
	int32 GetPreviewDelta() const { return PreviewDelta; }

	/** The placement preview is over a wall the selected tower would be built onto */
	bool IsPreviewOnWall() const { return bPreviewActive && bPreviewOnWall; }

protected:

	/** Re-joins the wall on a tile and its neighbors after a change */
	void RefreshWalls(const FIntPoint& Cell);

	/** Recomputes the distance field, the route and the route markers */
	void RebuildField();

	void BuildBoardVisuals();

	/** One component draws every dot of a route */
	UInstancedStaticMeshComponent* MakeRouteMarkers(UMaterialInterface* Material);

	/** Lays dotted markers along a route */
	void LayoutRouteMarkers(const TArray<FIntPoint>& InRoute, UInstancedStaticMeshComponent* Markers, float Z, float Size);

	void SetGhostType(ETDTowerType Type);

	/** Cell contents (see TDCell) */
	TArray<uint8> Cells;

	/** Distance field towards the goal */
	TArray<int32> Field;

	TArray<FIntPoint> Route;

	UPROPERTY()
	TArray<TObjectPtr<ATDTower>> Towers;

	/** Increments every time the field changes, so the preview knows to refresh */
	int32 FieldVersion = 0;

	// ---- Visual pieces ----

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> RouteMarkers;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> PreviewMarkers;

	/** Pooled range indicators (components are owned by this actor) */
	struct FRangeIndicator
	{
		UInstancedStaticMeshComponent* Ring = nullptr;
		UStaticMeshComponent* Disc = nullptr;
		UMaterialInstanceDynamic* RingMat = nullptr;
		UMaterialInstanceDynamic* DiscMat = nullptr;
		FVector Center = FVector(1e9);
		float Radius = -1.0f;
		FLinearColor Color = FLinearColor::Transparent;
		bool bVisible = false;
	};
	TArray<FRangeIndicator> Indicators;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> TileTint;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> TileTintMat;

	FVector TintCenter = FVector(1e9);
	float TintRadius = -1.0f;

	FRangeIndicator& GetIndicator(int32 Index);
	UMaterialInterface* RangeMaterial() const;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Highlight;

	/** Selected tile: a pulsing translucent fill and four bright edge bars */
	UPROPERTY()
	TObjectPtr<USceneComponent> SelectionMarker;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SelectionFillMat;

	UPROPERTY()
	TObjectPtr<USceneComponent> SelectionPointer;

	FIntPoint SelectedCell = FIntPoint(-1, -1);

	UPROPERTY()
	TObjectPtr<ATDTower> Ghost;

	/** Wall hidden under the ghost while previewing a build onto it */
	TWeakObjectPtr<ATDTower> HiddenWall;
	bool bPreviewOnWall = false;
	void ShowHiddenWall();

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Kelp;

	/** Seafloor anemones, which sway a little like the kelp */
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Anemones;

	// ---- living seabed around the board (all off the board, no collision, built with the map) ----
	/** Builds the light shafts, caustics, dunes, marine snow, fish, the far manta and the vents' bubbles */
	void BuildAmbience(USceneComponent* Root, const TFunctionRef<bool(const FVector&, float)>& ClearOfBoard, const TArray<FVector>& Vents);
	void TickAmbience();

	/** How high above the floor something off the board may float and still not be drawn over the board */
	static float MaxAmbientZ(const FVector& P);

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Shafts;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ShaftMats;
	TArray<FRotator> ShaftBase;
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Caustics;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CausticMats;
	TArray<FVector> CausticBase;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> SnowISM;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> FishISM;
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> BubbleISM;
	UPROPERTY()
	TObjectPtr<USceneComponent> Manta;

	struct FDrifter
	{
		FVector Base;		// start (snow: top of its fall; bubbles: the vent)
		float Speed;		// units per second along its path
		float Span;			// how far it travels before starting again
		float Phase;		// 0..1 start offset
		float Size;
	};
	TArray<FDrifter> Snow;
	TArray<FDrifter> VentBubbles;
	struct FFish
	{
		int32 School;
		FVector Offset;		// from the school's centre, in the school's frame
		float Phase;
	};
	struct FSchool
	{
		FVector2D Radii;	// ellipse the school swims round the board
		float Z;
		float Speed;		// radians per second
		float Phase;
	};
	TArray<FSchool> Schools;
	TArray<FFish> Fish;
	TArray<FTransform> AmbientScratch;
	float MantaSpan = 0.0f;
	float MantaX = 0.0f;

	/** Parent of everything that depends on the map (rocks, trench, reef, kelp); rebuilt on reset */
	UPROPERTY()
	TObjectPtr<USceneComponent> MapRoot;
	int32 BuiltMap = -1;
	int32 BuiltTier = 0;	// Abyss tier the seabed was tinted for
	void BuildMapVisuals();

	UPROPERTY()
	TObjectPtr<USceneComponent> GoalPearl;
	FVector GoalPearlBase = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> GoalLight;

	// ---- Preview state ----

	bool bPreviewActive = false;
	FIntPoint PreviewCell = FIntPoint(-1, -1);
	ETDTowerType PreviewType = ETDTowerType::Count;
	int32 PreviewVersion = -1;
	ETDPlaceResult PreviewResult = ETDPlaceResult::Ok;
	int32 PreviewDelta = 0;
	ETDTowerType GhostType = ETDTowerType::Count;

	float PreviewRefreshTimer = 0.0f;

	float AnimTime = 0.0f;
};
