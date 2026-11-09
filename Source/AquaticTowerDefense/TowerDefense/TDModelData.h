// Aquatic Tower Defense - isometric underwater tower defense with mazing.
// GENERATED from the model previews (tower, monster and world redesigns). Edit the preview builders and regenerate.
// Private to the model code: each group file holds the part tables of some models, in ETDModel order.

#pragma once

#include "TDModels.h"

namespace TDModelData
{
	/** Models of the towers: harpoon to mantis shrimp */
	TConstArrayView<FTDModel> Towers1();
	/** Models of the towers: octopus to acid vent */
	TConstArrayView<FTDModel> Towers2();
	/** Models of the monsters */
	TConstArrayView<FTDModel> Monsters();
	/** Models of the world props, projectiles and item drops */
	TConstArrayView<FTDModel> World();
	/** Models of the tower forms: harpoon to mantis shrimp (TDModelDataForms1.cpp, generated from the form builders) */
	TConstArrayView<FTDModel> Forms1();
	/** Models of the tower forms: octopus to acid vent */
	TConstArrayView<FTDModel> Forms2();
}
