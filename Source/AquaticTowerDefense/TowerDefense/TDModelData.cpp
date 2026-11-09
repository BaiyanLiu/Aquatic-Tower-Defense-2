// Aquatic Tower Defense - isometric underwater tower defense with mazing.
// GENERATED from the model previews (tower, monster and world redesigns). Edit the preview builders and regenerate.
// The part tables are split over TDModelData<Group>.cpp so a change to one group, or to any other game file
// compiled next to it, does not recompile all 600 KB of tables.

#include "TDModelData.h"

const FTDModel& TD::GetModel(ETDModel Id)
{
	static const TArray<FTDModel> Models = []()
	{
		TArray<FTDModel> All;
		All.Append(TDModelData::Towers1());
		All.Append(TDModelData::Towers2());
		All.Append(TDModelData::Monsters());
		All.Append(TDModelData::World());
		All.Append(TDModelData::Forms1());
		All.Append(TDModelData::Forms2());
		checkf(All.Num() == (int32)ETDModel::Count, TEXT("one table per model: %d tables for %d models"), All.Num(), (int32)ETDModel::Count);
		return All;
	}();
	return Models[FMath::Clamp((int32)Id, 0, (int32)ETDModel::Count - 1)];
}
