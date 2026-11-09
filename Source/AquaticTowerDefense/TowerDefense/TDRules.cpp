// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDRules.h"

namespace
{
	const FIntPoint GDirs[4] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };
}

// ---------------------------------------------------------------------------------------------
// Pathing
// ---------------------------------------------------------------------------------------------

const TArray<FIntPoint>& TD::StartingRocks()
{
	return MapRocks(GetMap());
}

void TD::MakeStartingCells(TArray<uint8>& OutCells)
{
	OutCells.Init(TDCell::Empty, Cols * Rows);
	for (int32 Y = 0; Y < Rows; ++Y)
	{
		for (int32 X = 0; X < Cols; ++X)
		{
			if (!IsOnBoard(FIntPoint(X, Y)))
			{
				OutCells[CellIndex(FIntPoint(X, Y))] = TDCell::Void;
			}
		}
	}
	for (const FIntPoint& Rock : StartingRocks())
	{
		OutCells[CellIndex(Rock)] = TDCell::Rock;
	}
}

void TD::ComputeField(const TArray<uint8>& Cells, TArray<int32>& OutDist)
{
	OutDist.Init(-1, Cols * Rows);

	TArray<FIntPoint> Queue;
	Queue.Reserve(Cols * Rows);

	const FIntPoint Goal = GoalCell();
	OutDist[CellIndex(Goal)] = 0;
	Queue.Add(Goal);

	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const FIntPoint Cell = Queue[Head];
		const int32 Dist = OutDist[CellIndex(Cell)];
		for (const FIntPoint& Dir : GDirs)
		{
			const FIntPoint Next = Cell + Dir;
			if (!InBounds(Next))
			{
				continue;
			}
			const int32 Index = CellIndex(Next);
			if (Cells[Index] != TDCell::Empty || OutDist[Index] >= 0)
			{
				continue;
			}
			OutDist[Index] = Dist + 1;
			Queue.Add(Next);
		}
	}
}

bool TD::StepOnField(const TArray<int32>& Field, const FIntPoint& From, const FIntPoint& PrevDir, FIntPoint& OutNext)
{
	if (!InBounds(From) || !Field.IsValidIndex(CellIndex(From)))
	{
		return false;
	}

	const int32 Dist = Field[CellIndex(From)];
	if (Dist <= 0)
	{
		return false;
	}

	// prefer continuing straight on ties
	const FIntPoint Straight = From + PrevDir;
	if (PrevDir != FIntPoint::ZeroValue && InBounds(Straight) && Field[CellIndex(Straight)] == Dist - 1)
	{
		OutNext = Straight;
		return true;
	}

	for (const FIntPoint& Dir : GDirs)
	{
		const FIntPoint Next = From + Dir;
		if (InBounds(Next) && Field[CellIndex(Next)] == Dist - 1)
		{
			OutNext = Next;
			return true;
		}
	}
	return false;
}

void TD::TraceRoute(const TArray<int32>& Field, TArray<FIntPoint>& OutRoute)
{
	OutRoute.Reset();

	FIntPoint Cell = SpawnCell();
	if (!Field.IsValidIndex(CellIndex(Cell)) || Field[CellIndex(Cell)] < 0)
	{
		return;
	}

	FIntPoint Dir(1, 0);
	OutRoute.Add(Cell);
	for (int32 Guard = 0; Guard < Cols * Rows && Cell != GoalCell(); ++Guard)
	{
		FIntPoint Next;
		if (!StepOnField(Field, Cell, Dir, Next))
		{
			break;
		}
		Dir = Next - Cell;
		Cell = Next;
		OutRoute.Add(Cell);
	}
}

ETDPlaceResult TD::EvaluatePlacement(const TArray<uint8>& Cells, const FIntPoint& Cell, const TArray<FTDWalker>& Walkers, TArray<int32>* OutField)
{
	if (!InBounds(Cell) || Cells[CellIndex(Cell)] == TDCell::Void)
	{
		return ETDPlaceResult::OutOfBounds;
	}
	if (Cell == SpawnCell() || Cell == GoalCell())
	{
		return ETDPlaceResult::SpawnOrGoal;
	}
	if (Cells[CellIndex(Cell)] != TDCell::Empty)
	{
		return ETDPlaceResult::Occupied;
	}
	for (const FTDWalker& Walker : Walkers)
	{
		if (Walker.Current == Cell || Walker.Next == Cell)
		{
			return ETDPlaceResult::EnemyInWay;
		}
	}

	TArray<uint8> Blocked = Cells;
	Blocked[CellIndex(Cell)] = TDCell::Tower;

	TArray<int32> NewField;
	ComputeField(Blocked, NewField);

	if (NewField[CellIndex(SpawnCell())] < 0)
	{
		return ETDPlaceResult::BlocksPath;
	}

	// creatures re-route from the tile they are heading to, so that tile must still reach the goal
	for (const FTDWalker& Walker : Walkers)
	{
		if (InBounds(Walker.Next) && NewField[CellIndex(Walker.Next)] < 0)
		{
			return ETDPlaceResult::TrapsEnemy;
		}
	}

	if (OutField)
	{
		*OutField = MoveTemp(NewField);
	}
	return ETDPlaceResult::Ok;
}

// ---------------------------------------------------------------------------------------------
// Combat math
// ---------------------------------------------------------------------------------------------

float TD::ComputeHitDamage(float Damage, ETDDamageType DamageType, ETDArmorType ArmorType, float Armor, float ArmorPierce, bool bIgnoreArmor, float ArmoredBonus)
{
	float Result = Damage * DamageTypeMultiplier(DamageType, ArmorType);
	if (Armor > 0.0f && ArmoredBonus > 0.0f)
	{
		Result *= 1.0f + ArmoredBonus;
	}
	if (!bIgnoreArmor)
	{
		Result *= ArmorMultiplier(Armor - ArmorPierce);
	}
	return Result;
}

int32 TD::SellValue(int32 TowerCost, int32 Level, float SellBonus, int32 SkillSpent, int32 TotalSpent, bool bFullRefund)
{
	if (bFullRefund)
	{
		return TotalSpent;
	}
	const float Base = TowerCost * 0.7f * (1.0f + 0.1f * (Level - 1)) * (1.0f + SellBonus);
	return FMath::FloorToInt(Base + SkillSpent * 0.5f);
}

void TD::SplitCopyStats(float ParentMaxHealth, int32 ParentSplits, float& OutMaxHealth, int32& OutSplits)
{
	OutMaxHealth = ParentMaxHealth / FMath::Max(1, ParentSplits * 2);
	OutSplits = ParentSplits / 2;
}

bool TD::CrossesSplitThreshold(float OldHealth, float NewHealth, float SplitHealth, int32 SplitsLeft)
{
	return SplitsLeft > 1 && SplitHealth > 0.0f && NewHealth > 0.0f
		&& FMath::CeilToInt(OldHealth / SplitHealth) > FMath::CeilToInt(NewHealth / SplitHealth);
}

// ---------------------------------------------------------------------------------------------
// Damage over time
// ---------------------------------------------------------------------------------------------

void FTDDotSet::Apply(ETDDot Kind, float Amount, float Duration, uint64 SourceId)
{
	if (Amount <= 0.0f || Duration <= 0.0f)
	{
		return;
	}
	for (FEntry& Entry : Entries)
	{
		if (Entry.Kind == Kind && Entry.SourceId == SourceId)
		{
			Entry.Amount = FMath::Max(Entry.Amount, Amount);
			Entry.Remaining = FMath::Max(Entry.Remaining, Duration);
			return;
		}
	}
	FEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Kind = Kind;
	Entry.Amount = Amount;
	Entry.Remaining = Duration;
	Entry.SourceId = SourceId;
}

bool FTDDotSet::Has(ETDDot Kind) const
{
	return Entries.ContainsByPredicate([Kind](const FEntry& Entry) { return Entry.Kind == Kind; });
}

float FTDDotSet::LongestRemaining(ETDDot Kind) const
{
	float Longest = 0.0f;
	for (const FEntry& Entry : Entries)
	{
		if (Entry.Kind == Kind)
		{
			Longest = FMath::Max(Longest, Entry.Remaining);
		}
	}
	return Longest;
}

void FTDDotSet::Tick(float DeltaTime, TFunctionRef<float()> CurrentHealth, TFunctionRef<void(uint64, float)> OnDamage)
{
	for (int32 i = Entries.Num() - 1; i >= 0; --i)
	{
		if (CurrentHealth() <= 0.0f)
		{
			return;
		}

		FEntry& Entry = Entries[i];
		// ticks only happen while the effect is still running
		const float Active = FMath::Min(DeltaTime, FMath::Max(0.0f, Entry.Remaining));
		Entry.Remaining -= DeltaTime;
		Entry.Timer += Active;

		float Damage = 0.0f;
		while (Entry.Timer >= TickInterval - KINDA_SMALL_NUMBER)
		{
			Entry.Timer -= TickInterval;
			Damage += (Entry.Kind == ETDDot::Bleed) ? (CurrentHealth() - Damage) * Entry.Amount / 100.0f : Entry.Amount;
		}

		const uint64 SourceId = Entry.SourceId;
		if (Entry.Remaining <= 0.0f)
		{
			Entries.RemoveAtSwap(i);
		}
		if (Damage > 0.0f)
		{
			OnDamage(SourceId, Damage);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Targeting priority
// ---------------------------------------------------------------------------------------------

const TCHAR* TD::TargetingName(ETDTargeting Mode)
{
	switch (Mode)
	{
	case ETDTargeting::First:			return TEXT("First");
	case ETDTargeting::Last:			return TEXT("Last");
	case ETDTargeting::Strongest:		return TEXT("Strongest");
	case ETDTargeting::Weakest:			return TEXT("Weakest");
	case ETDTargeting::SwimmingFirst:	return TEXT("Swimming first");
	case ETDTargeting::Closest:			return TEXT("Closest");
	default:							return TEXT("");
	}
}

const TCHAR* TD::TargetingDescription(ETDTargeting Mode)
{
	switch (Mode)
	{
	case ETDTargeting::First:			return TEXT("The creature closest to the reef");
	case ETDTargeting::Last:			return TEXT("The creature furthest from the reef");
	case ETDTargeting::Strongest:		return TEXT("The creature with the most health and shield");
	case ETDTargeting::Weakest:			return TEXT("The creature with the least health and shield");
	case ETDTargeting::SwimmingFirst:	return TEXT("Swimming creatures first, then the one closest to the reef");
	case ETDTargeting::Closest:			return TEXT("The creature closest to the tower");
	default:							return TEXT("");
	}
}

float TD::TargetScore(ETDTargeting Mode, const FTDTargetInfo& Info)
{
	switch (Mode)
	{
	case ETDTargeting::Last:			return -Info.GoalDistance;
	case ETDTargeting::Strongest:		return -Info.Toughness;
	case ETDTargeting::Weakest:			return Info.Toughness;
	case ETDTargeting::SwimmingFirst:	return (Info.bSwimming ? 0.0f : 1.0e7f) + Info.GoalDistance;
	case ETDTargeting::Closest:			return Info.TowerDistance;
	default:							return Info.GoalDistance;
	}
}

int32 TD::PickTarget(ETDTargeting Mode, const TArray<FTDTargetInfo>& Candidates)
{
	int32 Best = -1;
	float BestScore = TNumericLimits<float>::Max();
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		const float Score = TargetScore(Mode, Candidates[i]);
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = i;
		}
	}
	return Best;
}
