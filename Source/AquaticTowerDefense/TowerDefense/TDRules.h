// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "TDTypes.h"

/**
 *  Pure game rules with no actors involved, shared by the game code and the automation tests:
 *  pathing (BFS field, routes, placement checks), combat math, damage-over-time effects,
 *  aura stacking, sea star splitting and sell value.
 */

// ---------------------------------------------------------------------------------------------
// Pathing
// ---------------------------------------------------------------------------------------------

/** Board cell contents */
namespace TDCell
{
	constexpr uint8 Empty = 0;
	constexpr uint8 Rock = 1;
	constexpr uint8 Tower = 2;
	constexpr uint8 Void = 3;	// open water outside a non-rectangular map's outline
}

/** A ground creature as seen by the placement check */
struct FTDWalker
{
	FIntPoint Current;
	FIntPoint Next;
};

namespace TD
{
	/** Fills OutDist with BFS steps from the goal over 4-neighbours (-1 = unreachable) */
	void ComputeField(const TArray<uint8>& Cells, TArray<int32>& OutDist);

	/** Next cell towards the goal, preferring to continue straight on ties */
	bool StepOnField(const TArray<int32>& Field, const FIntPoint& From, const FIntPoint& PrevDir, FIntPoint& OutNext);

	/** Route from the spawn to the goal following the field */
	void TraceRoute(const TArray<int32>& Field, TArray<FIntPoint>& OutRoute);

	/** Rocks of the starting board */
	const TArray<FIntPoint>& StartingRocks();

	/** Fresh board with the starting rocks */
	void MakeStartingCells(TArray<uint8>& OutCells);

	/**
	 *  Placement rules: bounds, spawn/goal, occupied, creatures on or heading to the tile,
	 *  spawn must keep a route and every walker's next tile must still reach the goal.
	 */
	ETDPlaceResult EvaluatePlacement(const TArray<uint8>& Cells, const FIntPoint& Cell, const TArray<FTDWalker>& Walkers, TArray<int32>* OutField = nullptr);
}

// ---------------------------------------------------------------------------------------------
// Combat math
// ---------------------------------------------------------------------------------------------

namespace TD
{
	/** Final damage of a hit after the type table, armored bonus and percentage armor */
	float ComputeHitDamage(float Damage, ETDDamageType DamageType, ETDArmorType ArmorType, float Armor, float ArmorPierce, bool bIgnoreArmor, float ArmoredBonus);

	/** Sell refund: everything during the build phase it was bought in, otherwise 70% of the cost growing 10% per level plus half the skill spend */
	int32 SellValue(int32 TowerCost, int32 Level, float SellBonus, int32 SkillSpent, int32 TotalSpent, bool bFullRefund);

	/** Sea star: a split copy gets 1/(2*splits) of the parent's max health and half its splits */
	void SplitCopyStats(float ParentMaxHealth, int32 ParentSplits, float& OutMaxHealth, int32& OutSplits);

	/** True when health crossed a split threshold (original rule: every MaxHealth/Splits) */
	bool CrossesSplitThreshold(float OldHealth, float NewHealth, float SplitHealth, int32 SplitsLeft);
}

// ---------------------------------------------------------------------------------------------
// Auras: the strongest positive and the strongest negative value apply (original rule)
// ---------------------------------------------------------------------------------------------

struct FTDAuraStack
{
	float Up = 0.0f;
	float Down = 0.0f;

	void Reset() { Up = 0.0f; Down = 0.0f; }
	void Add(float Value)
	{
		if (Value > 0.0f)
		{
			Up = FMath::Max(Up, Value);
		}
		else
		{
			Down = FMath::Min(Down, Value);
		}
	}
	float Total() const { return Up + Down; }
};

// ---------------------------------------------------------------------------------------------
// Damage over time
// ---------------------------------------------------------------------------------------------

enum class ETDDot : uint8
{
	Poison,		// flat damage per tick
	Bleed		// percent of current health per tick
};

/** Damage-over-time effects on one creature. One stack per (kind, source); re-applying refreshes it. */
struct FTDDotSet
{
	static constexpr float TickInterval = 0.5f;

	struct FEntry
	{
		ETDDot Kind = ETDDot::Poison;
		float Amount = 0.0f;
		float Remaining = 0.0f;
		float Timer = 0.0f;
		uint64 SourceId = 0;
	};

	TArray<FEntry> Entries;

	/** Adds or refreshes a stack (keeps the stronger amount and the longer duration) */
	void Apply(ETDDot Kind, float Amount, float Duration, uint64 SourceId);

	bool Has(ETDDot Kind) const;
	float LongestRemaining(ETDDot Kind) const;

	/**
	 *  Advances time and reports damage per source. Bleed uses the health at the moment of each tick.
	 *  OnDamage(SourceId, Damage) may reduce health through the callback; CurrentHealth is re-read every tick.
	 */
	void Tick(float DeltaTime, TFunctionRef<float()> CurrentHealth, TFunctionRef<void(uint64, float)> OnDamage);
};

// ---------------------------------------------------------------------------------------------
// Targeting priority
// ---------------------------------------------------------------------------------------------

/** Which creature in range an attack tower aims at (saved per tower: append only) */
enum class ETDTargeting : uint8
{
	First,			// closest to the reef (the default)
	Last,			// furthest from the reef
	Strongest,		// most health and shield
	Weakest,		// least health and shield
	SwimmingFirst,	// swimming creatures first, then First
	Closest,		// closest to the tower
	Count
};

/** What the targeting rules look at for one creature in range */
struct FTDTargetInfo
{
	float GoalDistance = 0.0f;		// tiles along the path to the reef
	float Toughness = 0.0f;			// health + shield
	float TowerDistance = 0.0f;		// to the tower
	bool bSwimming = false;
};

namespace TD
{
	constexpr int32 TargetingCount = (int32)ETDTargeting::Count;
	const TCHAR* TargetingName(ETDTargeting Mode);
	const TCHAR* TargetingDescription(ETDTargeting Mode);

	/** Lower is better */
	float TargetScore(ETDTargeting Mode, const FTDTargetInfo& Info);

	/** Index of the creature a tower with this priority aims at, or -1 for none */
	int32 PickTarget(ETDTargeting Mode, const TArray<FTDTargetInfo>& Candidates);

	/** The next (Step 1) or previous (Step -1) priority, wrapping round */
	inline ETDTargeting CycleTargeting(ETDTargeting Mode, int32 Step) { return (ETDTargeting)(((int32)Mode + Step % TargetingCount + TargetingCount) % TargetingCount); }
}
