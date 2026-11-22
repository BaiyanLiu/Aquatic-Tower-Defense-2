// Aquatic Tower Defense - isometric underwater tower defense with mazing.
// Painted board surfaces for the detailed look: sand tiles with ripples, pebbles and shell bits, the layered stone slab under the
// board, and a mottled patch of seafloor round it. Each is one vertex-coloured mesh on the detail material.

#pragma once

#include "CoreMinimal.h"
#include "TDSculpt.h"

namespace TDBoardSurface
{
	/** How one map's seabed is dressed */
	struct FStyle
	{
		FLinearColor Tint = FLinearColor::White;
		int32 Seed = 1;
		float Algae = 1.0f;		// green film creeping in from tile edges
		float Shells = 1.0f;	// shell fragments
		float Pebbles = 1.0f;
		bool bRuins = false;	// carved flagstone grooves and worn centres
	};

	/** The style of a map (its tint already folded in) */
	FStyle StyleFor(int32 Map, const FLinearColor& Tint);

	/** Tile tops for every cell OnBoard says is on the board, top at Z 0 in the grid's space: one 97 uu tile per cell with a rounded
	 *  lip and short sides, coloured like today's tiles (checker and small random step) with the painted detail on top */
	void BuildTiles(const FStyle& Style, TFunctionRef<bool(const FIntPoint&)> OnBoard, TFunctionRef<FVector(const FIntPoint&)> CellCentre,
		int32 NumCols, int32 NumRows, FTDSculptMesh& Out);

	/** The slab under a full board: a box Size across, top at TopZ and Height tall, with strata, moss along its top and barnacles */
	void BuildSlab(const FStyle& Style, const FVector2D& Size, float TopZ, float Height, FTDSculptMesh& Out);

	/** A mottled, rippled patch of open seafloor Size across, centred on the board at height Z */
	void BuildFloor(const FStyle& Style, float Size, float Z, FTDSculptMesh& Out);
}
