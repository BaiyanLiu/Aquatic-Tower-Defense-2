// Aquatic Tower Defense - isometric underwater tower defense with mazing.
// The detail pass of the detailed models (atd.DetailedModels): extra parts added on top of the generated part tables.

#pragma once

#include "TDModels.h"

namespace TDModelDetail
{
	/** Adds the detail parts of one pivot of a model to Parts, which holds every part of that pivot (solid and see-through) */
	void AddDetail(ETDModel Id, int32 PivotIndex, float MinSize, TArray<FTDMergedPart>& Parts);

	/** The same pass for a mesh built outside the model tables (e.g. Coral Walls); Seed picks the speckle layout.
	 *  Also marks every part detailed, so it gets rounded edges and smoother curves. */
	void AddDetail(int32 Seed, float MinSize, TArray<FTDMergedPart>& Parts);
}
