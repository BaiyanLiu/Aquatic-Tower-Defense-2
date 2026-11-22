// Aquatic Tower Defense - isometric underwater tower defense with mazing.
// Sculpted bodies for the detailed models: a pivot's organic parts (spheres, tubes, cones, boxes) melted into one smooth surface.

#pragma once

#include "TDModels.h"

/** A sculpted surface in the pivot's space: one vertex colour per vertex, blended from the nearby parts' colours */
struct FTDSculptMesh
{
	TArray<FVector3f> Positions;
	TArray<FVector3f> Normals;
	TArray<FVector4f> Colors;
	TArray<int32> Triangles;
};

namespace TDSculpt
{
	/** Melts Parts together (a smooth union of their shapes, rounded where they meet) and meshes the result.
	 *  CellScale coarsens the surface for the lower levels of detail (1 = full). Safe on worker threads. False when there is nothing to sculpt. */
	bool Build(const TArray<FTDMergedPart>& Parts, float CellScale, FTDSculptMesh& Out);
}
