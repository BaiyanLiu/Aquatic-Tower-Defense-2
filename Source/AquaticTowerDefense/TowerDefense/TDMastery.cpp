// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDMastery.h"

namespace
{
	TArray<int32> GLifetimeKills;

	constexpr int32 Thresholds[TD::MasteryRankCount + 1] = { 0, 500, 2500, 10000, 40000 };
}

int32 TD::MasteryThreshold(int32 Rank)
{
	return Thresholds[FMath::Clamp(Rank, 0, MasteryRankCount)];
}

int32 TD::MasteryRank(int32 Kills)
{
	int32 Rank = 0;
	while (Rank < MasteryRankCount && Kills >= Thresholds[Rank + 1])
	{
		++Rank;
	}
	return Rank;
}

const TCHAR* TD::MasteryName(int32 Rank)
{
	switch (Rank)
	{
	case 1:		return TEXT("Bronze");
	case 2:		return TEXT("Silver");
	case 3:		return TEXT("Gold");
	case 4:		return TEXT("Abyssal");
	default:	return TEXT("No rank");
	}
}

FLinearColor TD::MasteryColor(int32 Rank)
{
	switch (Rank)
	{
	case 1:		return FLinearColor(0.95f, 0.5f, 0.2f);
	case 2:		return FLinearColor(0.92f, 0.95f, 1.0f);
	case 3:		return FLinearColor(1.0f, 0.76f, 0.22f);
	case 4:		return FLinearColor(0.22f, 0.36f, 1.0f);
	default:	return FLinearColor(0.4f, 0.45f, 0.5f);
	}
}

int32 TD::AddMasteryKill(TArray<int32>& Kills, ETDTowerType Type, bool bCounts)
{
	const int32 Index = (int32)Type;
	if (!bCounts || Index < 0 || Index >= TowerTypeCount)
	{
		return 0;
	}
	if (Kills.Num() < TowerTypeCount)
	{
		Kills.SetNumZeroed(TowerTypeCount);
	}
	const int32 Before = MasteryRank(Kills[Index]);
	Kills[Index] = Kills[Index] < MAX_int32 ? Kills[Index] + 1 : Kills[Index];
	const int32 After = MasteryRank(Kills[Index]);
	return After > Before ? After : 0;
}

const TArray<int32>& TD::LifetimeKills()
{
	return GLifetimeKills;
}

TArray<int32>& TD::MutableLifetimeKills()
{
	if (GLifetimeKills.Num() < TowerTypeCount)
	{
		GLifetimeKills.SetNumZeroed(TowerTypeCount);
	}
	return GLifetimeKills;
}

void TD::SetLifetimeKills(const TArray<int32>& Kills)
{
	GLifetimeKills = Kills;
	GLifetimeKills.SetNumZeroed(TowerTypeCount);
	for (int32& Count : GLifetimeKills)
	{
		Count = FMath::Max(0, Count);
	}
}

int32 TD::LifetimeKills(ETDTowerType Type)
{
	return GLifetimeKills.IsValidIndex((int32)Type) ? GLifetimeKills[(int32)Type] : 0;
}
