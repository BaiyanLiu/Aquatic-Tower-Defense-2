// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "TDTypes.h"

/**
 *  Tower mastery: lifetime kills per tower type across runs (kept in the settings slot). Ranks add a cosmetic
 *  trim to every tower of that type and show on the Mastery page. No effect on gameplay.
 */
namespace TD
{
	/** Ranks above none: Bronze, Silver, Gold, Abyssal */
	constexpr int32 MasteryRankCount = 4;

	/** Kills needed for a rank (1..MasteryRankCount); 0 for rank 0 */
	int32 MasteryThreshold(int32 Rank);

	/** Rank reached with this many kills (0 = none yet) */
	int32 MasteryRank(int32 Kills);

	const TCHAR* MasteryName(int32 Rank);

	/** Trim colour of a rank (bronze, silver, gold, deep abyss blue: clear of the teal and violet form badges) */
	FLinearColor MasteryColor(int32 Rank);

	/** Adds one kill for a tower type when the run counts (bCounts). Returns the rank reached when it just went up, else 0. */
	int32 AddMasteryKill(TArray<int32>& Kills, ETDTowerType Type, bool bCounts);

	/** Lifetime kills of every tower type (set from the settings; read by towers for their trim) */
	const TArray<int32>& LifetimeKills();
	void SetLifetimeKills(const TArray<int32>& Kills);
	TArray<int32>& MutableLifetimeKills();
	int32 LifetimeKills(ETDTowerType Type);
	inline int32 TowerMasteryRank(ETDTowerType Type) { return MasteryRank(LifetimeKills(Type)); }
}
