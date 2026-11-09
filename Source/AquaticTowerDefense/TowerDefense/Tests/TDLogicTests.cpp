// Aquatic Tower Defense - isometric underwater tower defense with mazing.
// Automation tests for the pure game rules. Run with: Automation RunTests ATD

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TDTypes.h"
#include "TDItems.h"
#include "TDRules.h"
#include "TDRun.h"
#include "TDMastery.h"
#include "TDSaveGame.h"
#include "TDIcons.h"
#include "TDGameMode.h"
#include "TDEnemy.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"

namespace
{
	constexpr EAutomationTestFlags ATDTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	/** Puts a full wall in a column, optionally leaving one row open */
	void FillColumn(TArray<uint8>& Cells, int32 Column, int32 GapRow = -1)
	{
		for (int32 Row = 0; Row < TD::Rows; ++Row)
		{
			if (Row != GapRow)
			{
				Cells[TD::CellIndex(FIntPoint(Column, Row))] = TDCell::Tower;
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Pathing
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDFieldTest, "ATD.Logic.Pathing.DistanceField", ATDTestFlags)
bool FATDFieldTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Cells;
	TD::MakeStartingCells(Cells);
	TestEqual(TEXT("Starting board has 11 rocks"), TD::StartingRocks().Num(), 11);

	TArray<int32> Field;
	TD::ComputeField(Cells, Field);
	TestEqual(TEXT("Goal distance is 0"), Field[TD::CellIndex(TD::GoalCell())], 0);
	TestEqual(TEXT("Open board: spawn is 17 steps from the goal"), Field[TD::CellIndex(TD::SpawnCell())], 17);
	TestEqual(TEXT("Rocks are unreachable"), Field[TD::CellIndex(FIntPoint(4, 2))], -1);

	TArray<FIntPoint> Route;
	TD::TraceRoute(Field, Route);
	TestEqual(TEXT("Route has 18 cells"), Route.Num(), 18);
	TestEqual(TEXT("Route starts at the spawn"), Route[0], TD::SpawnCell());
	TestEqual(TEXT("Route ends at the goal"), Route.Last(), TD::GoalCell());

	// straight-ahead tie break: the open board route stays on the spawn row
	bool bStraight = true;
	for (const FIntPoint& Cell : Route)
	{
		bStraight &= Cell.Y == TD::SpawnCell().Y;
	}
	TestTrue(TEXT("Open board route goes straight"), bStraight);

	// a wall with a gap forces a detour through the gap
	FillColumn(Cells, 8, 0);
	TD::ComputeField(Cells, Field);
	TD::TraceRoute(Field, Route);
	TestTrue(TEXT("Detour is longer"), Route.Num() > 18);
	TestTrue(TEXT("Detour passes the gap"), Route.Contains(FIntPoint(8, 0)));

	// fully sealed column
	FillColumn(Cells, 8);
	TD::ComputeField(Cells, Field);
	TestEqual(TEXT("Sealed board: spawn unreachable"), Field[TD::CellIndex(TD::SpawnCell())], -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDPlacementTest, "ATD.Logic.Pathing.Placement", ATDTestFlags)
bool FATDPlacementTest::RunTest(const FString& Parameters)
{
	TArray<uint8> Cells;
	TD::MakeStartingCells(Cells);
	const TArray<FTDWalker> NoWalkers;

	TestEqual(TEXT("Out of bounds"), TD::EvaluatePlacement(Cells, FIntPoint(-1, 0), NoWalkers), ETDPlaceResult::OutOfBounds);
	TestEqual(TEXT("Spawn tile"), TD::EvaluatePlacement(Cells, TD::SpawnCell(), NoWalkers), ETDPlaceResult::SpawnOrGoal);
	TestEqual(TEXT("Goal tile"), TD::EvaluatePlacement(Cells, TD::GoalCell(), NoWalkers), ETDPlaceResult::SpawnOrGoal);
	TestEqual(TEXT("Rock tile"), TD::EvaluatePlacement(Cells, FIntPoint(4, 2), NoWalkers), ETDPlaceResult::Occupied);
	TestEqual(TEXT("Open tile"), TD::EvaluatePlacement(Cells, FIntPoint(7, 7), NoWalkers), ETDPlaceResult::Ok);

	// the last gap of a wall would block the path
	FillColumn(Cells, 8, 5);
	TestEqual(TEXT("Closing the last gap blocks"), TD::EvaluatePlacement(Cells, FIntPoint(8, 5), NoWalkers), ETDPlaceResult::BlocksPath);

	// creatures on or heading to the tile
	const TArray<FTDWalker> Walkers = { { FIntPoint(3, 6), FIntPoint(4, 6) } };
	TestEqual(TEXT("Creature heading to tile"), TD::EvaluatePlacement(Cells, FIntPoint(4, 6), Walkers), ETDPlaceResult::EnemyInWay);
	TestEqual(TEXT("Creature on tile"), TD::EvaluatePlacement(Cells, FIntPoint(3, 6), Walkers), ETDPlaceResult::EnemyInWay);

	// a creature boxed in on the far side of a new wall is trapped
	TArray<uint8> Box;
	TD::MakeStartingCells(Box);
	Box[TD::CellIndex(FIntPoint(11, 4))] = TDCell::Tower;
	Box[TD::CellIndex(FIntPoint(11, 6))] = TDCell::Tower;
	Box[TD::CellIndex(FIntPoint(10, 5))] = TDCell::Tower;
	const TArray<FTDWalker> Boxed = { { FIntPoint(10, 4), FIntPoint(11, 5) } };
	TestEqual(TEXT("Sealing a creature in is refused"), TD::EvaluatePlacement(Box, FIntPoint(12, 5), Boxed), ETDPlaceResult::TrapsEnemy);

	TArray<int32> NewField;
	TestEqual(TEXT("Ok placement returns a field"), TD::EvaluatePlacement(Box, FIntPoint(2, 2), Boxed, &NewField), ETDPlaceResult::Ok);
	TestEqual(TEXT("Returned field size"), NewField.Num(), TD::Cols * TD::Rows);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Combat math
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDDamageTableTest, "ATD.Logic.Combat.DamageTypes", ATDTestFlags)
bool FATDDamageTableTest::RunTest(const FString& Parameters)
{
	using D = ETDDamageType;
	using A = ETDArmorType;
	// original table
	TestEqual(TEXT("Light vs Light"), TD::DamageTypeMultiplier(D::Light, A::Light), 1.0f);
	TestEqual(TEXT("Normal vs Light"), TD::DamageTypeMultiplier(D::Normal, A::Light), 1.1f);
	TestEqual(TEXT("Heavy vs Light"), TD::DamageTypeMultiplier(D::Heavy, A::Light), 1.2f);
	TestEqual(TEXT("Light vs Normal"), TD::DamageTypeMultiplier(D::Light, A::Normal), 0.8f);
	TestEqual(TEXT("Normal vs Normal"), TD::DamageTypeMultiplier(D::Normal, A::Normal), 1.0f);
	TestEqual(TEXT("Heavy vs Normal"), TD::DamageTypeMultiplier(D::Heavy, A::Normal), 1.1f);
	TestEqual(TEXT("Light vs Heavy"), TD::DamageTypeMultiplier(D::Light, A::Heavy), 0.6f);
	TestEqual(TEXT("Normal vs Heavy"), TD::DamageTypeMultiplier(D::Normal, A::Heavy), 0.8f);
	TestEqual(TEXT("Heavy vs Heavy"), TD::DamageTypeMultiplier(D::Heavy, A::Heavy), 1.0f);
	TestEqual(TEXT("No damage type"), TD::DamageTypeMultiplier(D::None, A::Heavy), 1.0f);

	// percentage armor
	TestEqual(TEXT("0 armor"), TD::ArmorMultiplier(0.0f), 1.0f);
	TestEqual(TEXT("25 armor"), TD::ArmorMultiplier(25.0f), 0.75f);
	TestEqual(TEXT("Negative armor adds damage"), TD::ArmorMultiplier(-20.0f), 1.2f);
	TestEqual(TEXT("Armor capped at 90%"), TD::ArmorMultiplier(150.0f), 0.1f);

	TestEqual(TEXT("Hit: heavy vs heavy 25 armor"), TD::ComputeHitDamage(100.0f, D::Heavy, A::Heavy, 25.0f, 0.0f, false, 0.0f), 75.0f);
	TestEqual(TEXT("Hit: pierce 5"), TD::ComputeHitDamage(100.0f, D::Normal, A::Normal, 25.0f, 5.0f, false, 0.0f), 80.0f);
	TestEqual(TEXT("Hit: ignores armor"), TD::ComputeHitDamage(100.0f, D::Light, A::Heavy, 50.0f, 0.0f, true, 0.0f), 60.0f);
	TestEqual(TEXT("Hit: armored bonus"), TD::ComputeHitDamage(100.0f, D::Normal, A::Normal, 10.0f, 0.0f, true, 0.2f), 120.0f);
	TestEqual(TEXT("Hit: armored bonus skipped with no armor"), TD::ComputeHitDamage(100.0f, D::Normal, A::Normal, 0.0f, 0.0f, true, 0.2f), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDDamageRangeTest, "ATD.Logic.Combat.DamageRange", ATDTestFlags)
bool FATDDamageRangeTest::RunTest(const FString& Parameters)
{
	// every attack tower has a damage range; walls and aura towers have none
	for (int32 i = 0; i < TD::TowerTypeCount; ++i)
	{
		const ETDTowerType Type = (ETDTowerType)i;
		const FTDTowerDef& Def = TD::Tower(Type);
		const float Spread = TD::TowerDamageSpread(Type);
		if (Def.bAttacks)
		{
			TestTrue(FString::Printf(TEXT("%s has a damage range"), Def.Name), Spread > 0.0f && Spread < 1.0f);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s has no damage range"), Def.Name), Spread, 0.0f);
		}
	}

	// precise towers are tight, chaotic ones wide
	TestTrue(TEXT("Harpoon tighter than Octopus"), TD::TowerDamageSpread(ETDTowerType::Harpoon) < TD::TowerDamageSpread(ETDTowerType::Octopus));
	TestTrue(TEXT("Mantis tighter than Puffer"), TD::TowerDamageSpread(ETDTowerType::Mantis) < TD::TowerDamageSpread(ETDTowerType::Puffer));
	TestTrue(TEXT("Eel is the widest"), TD::TowerDamageSpread(ETDTowerType::Eel) >= TD::TowerDamageSpread(ETDTowerType::Puffer));

	// the roll covers [avg x (1 - spread), avg x (1 + spread)] evenly, so the average damage is unchanged
	TestEqual(TEXT("Lowest roll"), TD::RollDamage(20.0f, 0.25f, 0.0f), 15.0f, 0.001f);
	TestEqual(TEXT("Middle roll is the average"), TD::RollDamage(20.0f, 0.25f, 0.5f), 20.0f, 0.001f);
	TestEqual(TEXT("Highest roll"), TD::RollDamage(20.0f, 0.25f, 1.0f), 25.0f, 0.001f);
	TestEqual(TEXT("Out-of-range unit is clamped"), TD::RollDamage(20.0f, 0.25f, 2.0f), 25.0f, 0.001f);
	for (int32 i = 0; i < TD::TowerTypeCount; ++i)
	{
		const ETDTowerType Type = (ETDTowerType)i;
		const float Spread = TD::TowerDamageSpread(Type);
		double Sum = 0.0;
		constexpr int32 Steps = 1000;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			Sum += TD::RollDamage(100.0f, Spread, (Step + 0.5f) / Steps);
		}
		TestEqual(FString::Printf(TEXT("%s keeps its average damage"), TD::Tower(Type).Name), (float)(Sum / Steps), 100.0f, 0.01f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDDotTest, "ATD.Logic.Combat.PoisonAndBleed", ATDTestFlags)
bool FATDDotTest::RunTest(const FString& Parameters)
{
	// poison: 4 per tick for 2s = 4 ticks = 16 damage
	{
		FTDDotSet Dots;
		float Health = 100.0f;
		float Dealt = 0.0f;
		Dots.Apply(ETDDot::Poison, 4.0f, 2.0f, 1);
		for (int32 i = 0; i < 60; ++i)
		{
			Dots.Tick(0.1f, [&]() { return Health; }, [&](uint64, float D) { Health -= D; Dealt += D; });
		}
		TestEqual(TEXT("Poison total"), Dealt, 16.0f);
		TestFalse(TEXT("Poison expired"), Dots.Has(ETDDot::Poison));
	}

	// same source refreshes, different sources stack
	{
		FTDDotSet Dots;
		Dots.Apply(ETDDot::Poison, 4.0f, 2.0f, 1);
		Dots.Apply(ETDDot::Poison, 3.0f, 3.0f, 1);
		TestEqual(TEXT("Same source keeps one stack"), Dots.Entries.Num(), 1);
		TestEqual(TEXT("Keeps the stronger amount"), Dots.Entries[0].Amount, 4.0f);
		TestEqual(TEXT("Keeps the longer duration"), Dots.Entries[0].Remaining, 3.0f);
		Dots.Apply(ETDDot::Poison, 2.0f, 1.0f, 2);
		TestEqual(TEXT("Second source adds a stack"), Dots.Entries.Num(), 2);
	}

	// bleed: 10% of current health per tick, compounding
	{
		FTDDotSet Dots;
		float Health = 1000.0f;
		Dots.Apply(ETDDot::Bleed, 10.0f, 1.0f, 7);
		for (int32 i = 0; i < 20; ++i)
		{
			Dots.Tick(0.1f, [&]() { return Health; }, [&](uint64 Source, float D) { Health -= D; TestEqual(TEXT("Bleed source id"), Source, (uint64)7); });
		}
		TestTrue(TEXT("Bleed: two ticks of 10% current health"), FMath::IsNearlyEqual(Health, 810.0f, 0.01f));
	}

	// a big frame doesn't overrun the duration
	{
		FTDDotSet Dots;
		float Dealt = 0.0f;
		Dots.Apply(ETDDot::Poison, 1.0f, 1.0f, 3);
		Dots.Tick(5.0f, []() { return 100.0f; }, [&](uint64, float D) { Dealt += D; });
		TestEqual(TEXT("Large delta: only 2 ticks"), Dealt, 2.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDAuraTest, "ATD.Logic.Combat.AuraStacking", ATDTestFlags)
bool FATDAuraTest::RunTest(const FString& Parameters)
{
	FTDAuraStack Stack;
	Stack.Add(10.0f);
	Stack.Add(25.0f);
	Stack.Add(5.0f);
	Stack.Add(-10.0f);
	Stack.Add(-15.0f);
	TestEqual(TEXT("Strongest positive"), Stack.Up, 25.0f);
	TestEqual(TEXT("Strongest negative"), Stack.Down, -15.0f);
	TestEqual(TEXT("Total"), Stack.Total(), 10.0f);
	Stack.Reset();
	TestEqual(TEXT("Reset"), Stack.Total(), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDSplitTest, "ATD.Logic.Combat.SeaStarSplit", ATDTestFlags)
bool FATDSplitTest::RunTest(const FString& Parameters)
{
	const FTDEnemyDef& Star = TD::Enemy(ETDEnemyType::SeaStar);
	TestEqual(TEXT("Sea star splits twice"), Star.Splits, 2);

	const float Max = 120.0f;
	const float SplitHealth = Max / Star.Splits;
	TestFalse(TEXT("No split above half"), TD::CrossesSplitThreshold(120.0f, 70.0f, SplitHealth, 2));
	TestTrue(TEXT("Split when crossing half"), TD::CrossesSplitThreshold(70.0f, 50.0f, SplitHealth, 2));
	TestFalse(TEXT("No split on death"), TD::CrossesSplitThreshold(50.0f, -5.0f, SplitHealth, 2));

	float CopyMax = 0.0f;
	int32 CopySplits = 0;
	TD::SplitCopyStats(Max, 2, CopyMax, CopySplits);
	TestEqual(TEXT("Copy health is a quarter"), CopyMax, 30.0f);
	TestEqual(TEXT("Copy has one split left"), CopySplits, 1);
	TestFalse(TEXT("Copies never split again"), TD::CrossesSplitThreshold(30.0f, 1.0f, CopyMax, CopySplits));
	return true;
}

// ---------------------------------------------------------------------------------------------
// Progression and economy
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDXpCurveTest, "ATD.Logic.Progression.XpCurve", ATDTestFlags)
bool FATDXpCurveTest::RunTest(const FString& Parameters)
{
	int32 Previous = 0;
	int32 Total = 0;
	for (int32 Level = 1; Level < TD::MaxTowerLevel; ++Level)
	{
		const int32 Need = TD::XpToNextLevel(Level);
		TestTrue(FString::Printf(TEXT("Level %d needs more than level %d"), Level, Level - 1), Need > Previous);
		Previous = Need;
		Total += Need;
	}
	TestEqual(TEXT("Level 1 -> 2"), TD::XpToNextLevel(1), TD::XpBase);
	TestEqual(TEXT("Level 2 -> 3"), TD::XpToNextLevel(2), FMath::RoundToInt(TD::XpBase * TD::XpGrowth));
	TestEqual(TEXT("Level 19 -> 20 keeps the steep curve"), TD::XpToNextLevel(19), FMath::RoundToInt(TD::XpBase * FMath::Pow(TD::XpGrowth, 18.0f)));
	TestEqual(TEXT("Last level"), TD::XpToNextLevel(TD::MaxTowerLevel - 1), FMath::RoundToInt(TD::XpBase * FMath::Pow(TD::XpGrowth, 18.0f) * FMath::Pow(TD::XpGrowthLate, (float)(TD::MaxTowerLevel - 20))));
	AddInfo(FString::Printf(TEXT("Total XP to level %d: %d"), TD::MaxTowerLevel, Total));
	TestEqual(TEXT("Skill rank 1 needs level 1"), TD::SkillLevelRequirement(1), 1);
	TestEqual(TEXT("Skill rank 2 needs level 5"), TD::SkillLevelRequirement(2), 5);
	TestEqual(TEXT("Top skill rank reachable"), TD::SkillLevelRequirement(TD::MaxSkillRank) <= TD::MaxTowerLevel, true);
	TestTrue(TEXT("Enough skill points for every rank"), TD::MaxTowerLevel >= TD::SkillsPerTower * TD::MaxSkillRank);
	TestEqual(TEXT("Skill cost"), TD::SkillCost(ETDTowerType::Harpoon, 0, 7), FMath::RoundToInt(TD::Skill(ETDTowerType::Harpoon, 0)->BaseCost * 7 * 7 * TD::SkillCostRankScale));
	TestTrue(TEXT("Skill ranks get dearer faster than linearly"), TD::SkillCost(ETDTowerType::Harpoon, 0, 7) > 7 * TD::SkillCost(ETDTowerType::Harpoon, 0, 1));
	FTDItem Item;
	Item.Level = 95;
	Item.Rarity = ETDRarity::Legendary;
	TestEqual(TEXT("Item upgrade cost"), TD::UpgradeCost(Item), 630);
	TestEqual(TEXT("Range at level 20"), TD::TowerRange(ETDTowerType::Harpoon, 20), 5.95f, 0.001f);
	TestEqual(TEXT("Range at the top level"), TD::TowerRange(ETDTowerType::Harpoon, TD::MaxTowerLevel), 6.2f, 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDSellValueTest, "ATD.Logic.Economy.SellValue", ATDTestFlags)
bool FATDSellValueTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Full refund in build phase"), TD::SellValue(40, 1, 0.0f, 0, 55, true), 55);
	TestEqual(TEXT("70% at level 1"), TD::SellValue(40, 1, 0.0f, 0, 40, false), 28);
	TestEqual(TEXT("+10% per level"), TD::SellValue(40, 6, 0.0f, 0, 40, false), 42);
	TestEqual(TEXT("Half of skill spend"), TD::SellValue(40, 1, 0.0f, 30, 70, false), 43);
	TestEqual(TEXT("Recycled Materials bonus"), TD::SellValue(40, 1, 0.2f, 0, 40, false), 33);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDItemTest, "ATD.Logic.Items.Generation", ATDTestFlags)
bool FATDItemTest::RunTest(const FString& Parameters)
{
	FMath::RandInit(12345);

	// the item table: every type is complete, and every slot has at least two types per rarity so rerolls can change it
	TestTrue(TEXT("Many item types"), TD::ItemKindCount() >= 30);
	TestNull(TEXT("Kind 0 is no type"), TD::ItemKind(0));
	TestNull(TEXT("Kind past the end is no type"), TD::ItemKind(TD::ItemKindCount() + 1));
	TSet<FString> Names;
	int32 PerBucket[(int32)ETDItemSlot::Count][4] = {};
	for (int32 Kind = 1; Kind <= TD::ItemKindCount(); ++Kind)
	{
		const FTDItemKindDef* Def = TD::ItemKind(Kind);
		TestNotNull(TEXT("Kind has a definition"), Def);
		if (!Def)
		{
			continue;
		}
		TestFalse(TEXT("Unique name"), Names.Contains(Def->Name));
		Names.Add(Def->Name);
		++PerBucket[(int32)Def->Slot][(int32)Def->Rarity];

		const FTDItem Item = TD::MakeItemOfKind(Kind, Kind, 1);
		TestTrue(TEXT("Typed item valid"), Item.IsValid());
		TestEqual(TEXT("Kind kept"), Item.Kind, Kind);
		TestEqual(TEXT("Name from type"), Item.Name, FString(Def->Name));
		TestFalse(TEXT("Has flavor text"), TD::ItemFlavor(Item).IsEmpty());
		TestTrue(TEXT("At least one effect"), Item.Affixes.Num() >= 1);

		TSet<ETDAffix> Seen;
		int32 Gains = 0;
		for (const FTDAffix& Affix : Item.Affixes)
		{
			TestFalse(TEXT("No duplicate affixes"), Seen.Contains(Affix.Type));
			Seen.Add(Affix.Type);
			TestTrue(TEXT("Retired affix unused"), Affix.Type != ETDAffix::RetiredProjectileSpeed);
			TestFalse(TEXT("Affix has text"), TD::AffixText(Affix).IsEmpty());
			Gains += Affix.Value > 0.0f ? 1 : 0;
			if (Affix.Type == ETDAffix::Chain)
			{
				TestEqual(TEXT("Chain is a whole number"), Affix.Value, FMath::RoundToFloat(Affix.Value));
			}
		}
		TestTrue(TEXT("Every type has an upside"), Gains >= 1);
	}
	for (int32 Slot = 0; Slot < (int32)ETDItemSlot::Count; ++Slot)
	{
		for (int32 r = 0; r < 4; ++r)
		{
			TestTrue(FString::Printf(TEXT("Two or more %s %s types"), TD::RarityName((ETDRarity)r), TD::SlotName((ETDItemSlot)Slot)), PerBucket[Slot][r] >= 2);
		}
	}

	// generated items follow the asked rarity and slot, and a type always rolls the same effects
	for (int32 r = 0; r < 4; ++r)
	{
		for (int32 i = 0; i < 50; ++i)
		{
			const FTDItem Item = TD::GenerateItem(100 + i, 7, (ETDRarity)r, ETDItemSlot::Pearl);
			TestEqual(TEXT("Rarity kept"), Item.Rarity, (ETDRarity)r);
			TestEqual(TEXT("Slot kept"), Item.Slot, ETDItemSlot::Pearl);
			TestEqual(TEXT("Item level kept"), Item.Level, 7);
			TestTrue(TEXT("Generated items are typed"), TD::ItemKind(Item.Kind) != nullptr);

			const FTDItem Twin = TD::MakeItemOfKind(1, Item.Kind, 7);
			TestEqual(TEXT("Same effects count"), Twin.Affixes.Num(), Item.Affixes.Num());
			for (int32 k = 0; k < FMath::Min(Twin.Affixes.Num(), Item.Affixes.Num()); ++k)
			{
				TestEqual(TEXT("No variance within a type"), Twin.Affixes[k].Value, Item.Affixes[k].Value);
			}
		}
	}

	// item level grows bonuses, but drawbacks, chain targets and stun chance stay fixed
	for (int32 Kind = 1; Kind <= TD::ItemKindCount(); ++Kind)
	{
		const FTDItem Low = TD::MakeItemOfKind(1, Kind, 1);
		const FTDItem High = TD::MakeItemOfKind(1, Kind, 30);
		for (int32 k = 0; k < Low.Affixes.Num(); ++k)
		{
			const FTDAffix& L = Low.Affixes[k];
			const FTDAffix& H = High.Affixes[k];
			if (L.Value < 0.0f || L.Type == ETDAffix::Chain || L.Type == ETDAffix::Stun)
			{
				TestEqual(TEXT("Fixed effect doesn't scale"), H.Value, L.Value);
			}
			else
			{
				TestTrue(TEXT("Bonus grows with item level"), H.Value > L.Value);
			}
		}
	}

	// damage-type bonuses grow x5 by item level 100; reach, splash, slow and pearl trickle only x2
	{
		auto KindNamed = [](const TCHAR* Name)
		{
			for (int32 Kind = 1; Kind <= TD::ItemKindCount(); ++Kind)
			{
				if (FCString::Strcmp(TD::ItemKind(Kind)->Name, Name) == 0)
				{
					return Kind;
				}
			}
			return 0;
		};
		auto Growth = [](int32 Kind, int32 Index)
		{
			return TD::MakeItemOfKind(1, Kind, 100).Affixes[Index].Value / TD::MakeItemOfKind(1, Kind, 1).Affixes[Index].Value;
		};
		const int32 Cowrie = KindNamed(TEXT("Sharpened Cowrie"));
		const int32 Kelp = KindNamed(TEXT("Kelp Braid"));
		const int32 Scallop = KindNamed(TEXT("Scallop Fan"));
		const int32 ColdSeep = KindNamed(TEXT("Cold Seep Pearl"));
		const int32 Purse = KindNamed(TEXT("Sunken Coin Purse"));
		if (TestTrue(TEXT("Scaling test items exist"), Cowrie && Kelp && Scallop && ColdSeep && Purse))
		{
			TestEqual(TEXT("Damage grows with the full item level scale"), Growth(Cowrie, 0), TD::ItemLevelScale(100), 0.001f);
			const float Slow = 1.0f + 0.25f * (TD::ItemLevelScale(100) - 1.0f);
			TestEqual(TEXT("Range grows slowly"), Growth(Kelp, 0), Slow, 0.001f);
			TestEqual(TEXT("Splash grows slowly"), Growth(Scallop, 0), Slow, 0.001f);
			TestEqual(TEXT("On-hit slow grows slowly"), Growth(ColdSeep, 0), Slow, 0.001f);
			TestEqual(TEXT("Pearl trickle grows slowly"), Growth(Purse, 0), Slow, 0.001f);
			TestEqual(TEXT("Bounty on the same item grows fully"), Growth(Purse, 1), TD::ItemLevelScale(100), 0.001f);
		}
	}

	// upgrading a typed item matches a fresh item of the new level
	FTDItem Upgraded = TD::MakeItemOfKind(5, 1, 10);
	TestTrue(TEXT("Upgrade works"), TD::UpgradeItem(Upgraded));
	TestEqual(TEXT("Upgrade adds levels"), Upgraded.Level, 10 + TD::ItemUpgradeLevels);
	TestEqual(TEXT("Upgraded effects match the type at that level"), Upgraded.Affixes[0].Value, TD::MakeItemOfKind(5, 1, 10 + TD::ItemUpgradeLevels).Affixes[0].Value);

	// rerolling changes the type, never the slot, rarity, level or id
	for (int32 i = 0; i < 50; ++i)
	{
		FTDItem Item = TD::GenerateItem(77, 12, (ETDRarity)(i % 4));
		const FTDItem Before = Item;
		TD::RerollItem(Item);
		TestNotEqual(TEXT("Reroll changes the type"), Item.Kind, Before.Kind);
		TestEqual(TEXT("Reroll keeps the slot"), Item.Slot, Before.Slot);
		TestEqual(TEXT("Reroll keeps the rarity"), Item.Rarity, Before.Rarity);
		TestEqual(TEXT("Reroll keeps the level"), Item.Level, Before.Level);
		TestEqual(TEXT("Reroll keeps the id"), Item.Id, Before.Id);
	}

	// old random-affix items (no type) still upgrade by scaling what they rolled
	FTDItem Legacy;
	Legacy.Id = 9;
	Legacy.Level = 1;
	Legacy.Affixes = { { ETDAffix::DamagePct, 0.2f }, { ETDAffix::Chain, 1.0f } };
	TD::UpgradeItem(Legacy);
	TestEqual(TEXT("Old item damage scaled"), Legacy.Affixes[0].Value, 0.2f * TD::ItemLevelScale(1 + TD::ItemUpgradeLevels) / TD::ItemLevelScale(1), 0.0001f);
	TestEqual(TEXT("Old item chain fixed"), Legacy.Affixes[1].Value, 1.0f);

	// projectile speed is fixed per tower, so no new item can change it
	bool bRolledSpeed = false;
	for (int32 i = 0; i < 2000; ++i)
	{
		bRolledSpeed |= TD::GenerateItem(1, 30, ETDRarity::Legendary).Affixes.ContainsByPredicate([](const FTDAffix& Affix) { return Affix.Type == ETDAffix::RetiredProjectileSpeed; });
	}
	TestFalse(TEXT("Projectile speed affix never rolls"), bRolledSpeed);

	// rarity rolls respect the minimum and shift up with level
	int32 Low = 0, High = 0;
	for (int32 i = 0; i < 2000; ++i)
	{
		TestTrue(TEXT("Minimum epic respected"), TD::RollRarity(1, ETDRarity::Epic) >= ETDRarity::Epic);
		Low += TD::RollRarity(1) >= ETDRarity::Epic ? 1 : 0;
		High += TD::RollRarity(30) >= ETDRarity::Epic ? 1 : 0;
	}
	TestTrue(TEXT("Higher levels roll more epics"), High > Low);

	// affix applicability
	TestFalse(TEXT("Walls take nothing"), TD::AffixApplies(ETDAffix::DamagePct, ETDTowerType::Wall));
	TestTrue(TEXT("Slow strength on jellyfish"), TD::AffixApplies(ETDAffix::SlowStrength, ETDTowerType::Jelly));
	TestFalse(TEXT("Slow strength not on harpoon"), TD::AffixApplies(ETDAffix::SlowStrength, ETDTowerType::Harpoon));
	TestFalse(TEXT("A puffer's bomb doesn't chain"), TD::AffixApplies(ETDAffix::Chain, ETDTowerType::Puffer));
	TestFalse(TEXT("Support towers ignore damage"), TD::AffixApplies(ETDAffix::DamagePct, ETDTowerType::Humpback));
	TestTrue(TEXT("Support towers take range"), TD::AffixApplies(ETDAffix::Range, ETDTowerType::AcidVent));
	TestFalse(TEXT("Mantis punches already stun"), TD::AffixApplies(ETDAffix::Stun, ETDTowerType::Mantis));
	TestTrue(TEXT("Stun on a puffer"), TD::AffixApplies(ETDAffix::Stun, ETDTowerType::Puffer));
	TestFalse(TEXT("No anti-air bonus for a ground-only tower"), TD::AffixApplies(ETDAffix::AirDamage, ETDTowerType::Mantis));
	TestTrue(TEXT("Anti-air bonus on a harpoon"), TD::AffixApplies(ETDAffix::AirDamage, ETDTowerType::Harpoon));
	TestFalse(TEXT("Support towers ignore boss damage"), TD::AffixApplies(ETDAffix::BossDamage, ETDTowerType::Humpback));

	// conditional damage bonuses add up, and execute only counts below the threshold
	FTDHit Hit;
	Hit.BossBonus = 0.5f;
	Hit.AirBonus = 0.25f;
	Hit.ExecuteBonus = 1.0f;
	TestEqual(TEXT("No bonus on a healthy ground creature"), TD::ConditionalDamageMultiplier(Hit, false, false, 1.0f), 1.0f);
	TestEqual(TEXT("Boss bonus"), TD::ConditionalDamageMultiplier(Hit, true, false, 1.0f), 1.5f);
	TestEqual(TEXT("Flying boss"), TD::ConditionalDamageMultiplier(Hit, true, true, 0.9f), 1.75f);
	TestEqual(TEXT("Execute below the threshold"), TD::ConditionalDamageMultiplier(Hit, false, false, TD::ExecuteThreshold - 0.01f), 2.0f);
	TestEqual(TEXT("No execute at the threshold"), TD::ConditionalDamageMultiplier(Hit, false, false, TD::ExecuteThreshold), 1.0f);
	TestFalse(TEXT("Mantis shrimp can't reach flying enemies"), TD::Tower(ETDTowerType::Mantis).bHitsFlying);
	TestTrue(TEXT("Mantis shrimp deals heavy damage"), TD::Tower(ETDTowerType::Mantis).DamageType == ETDDamageType::Heavy);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDItemCompareTest, "ATD.Logic.Items.Compare", ATDTestFlags)
bool FATDItemCompareTest::RunTest(const FString& Parameters)
{
	FTDItem New;
	New.Id = 1;
	New.Affixes = { { ETDAffix::DamagePct, 0.30f }, { ETDAffix::RatePct, 0.10f }, { ETDAffix::SlowStrength, 0.2f } };
	FTDItem Old;
	Old.Id = 2;
	Old.Affixes = { { ETDAffix::DamagePct, 0.10f }, { ETDAffix::Range, 0.25f }, { ETDAffix::RatePct, 0.10f } };

	TArray<FTDAffix> Changes;
	TD::CompareItems(New, Old, ETDTowerType::Harpoon, Changes);
	TestEqual(TEXT("Damage gain and range loss; equal speed and slow (no effect on a harpoon) left out"), Changes.Num(), 2);
	if (Changes.Num() == 2)
	{
		TestTrue(TEXT("Damage first (affix order)"), Changes[0].Type == ETDAffix::DamagePct && FMath::IsNearlyEqual(Changes[0].Value, 0.2f, 0.001f));
		TestTrue(TEXT("Range lost"), Changes[1].Type == ETDAffix::Range && FMath::IsNearlyEqual(Changes[1].Value, -0.25f, 0.001f));
		TestEqual(TEXT("Gain text"), TD::AffixDeltaText(Changes[0]), FString(TEXT("+20% damage")));
		TestEqual(TEXT("Loss text"), TD::AffixDeltaText(Changes[1]), FString(TEXT("-0.25 tiles range")));
	}

	TD::CompareItems(New, FTDItem(), ETDTowerType::Jelly, Changes);
	TestEqual(TEXT("Empty slot: every affix that applies is a gain"), Changes.Num(), 3);

	TestEqual(TEXT("A loss without a plus sign"), TD::AffixDeltaText({ ETDAffix::ArmorPierce, -5.0f }), FString(TEXT("Loses: Ignores 5 armor")));

	// drawbacks read with a minus sign, and taking one off reads as a gain
	TestEqual(TEXT("Drawback text"), TD::AffixText({ ETDAffix::RatePct, -0.15f }), FString(TEXT("-15% attack speed")));
	FTDItem Hammer;
	Hammer.Id = 3;
	Hammer.Affixes = { { ETDAffix::DamagePct, 0.6f }, { ETDAffix::RatePct, -0.15f } };
	TD::CompareItems(Hammer, FTDItem(), ETDTowerType::Harpoon, Changes);
	TestEqual(TEXT("Drawback listed as a change"), Changes.Num(), 2);
	if (Changes.Num() == 2)
	{
		TestEqual(TEXT("Drawback delta text"), TD::AffixDeltaText(Changes[1]), FString(TEXT("-15% attack speed")));
	}
	FTDItem Plain;
	Plain.Id = 4;
	TD::CompareItems(Plain, Hammer, ETDTowerType::Harpoon, Changes);
	TestTrue(TEXT("Removing a drawback is a gain"), Changes.ContainsByPredicate([](const FTDAffix& C) { return C.Type == ETDAffix::RatePct && C.Value > 0.0f; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDSaveRoundTripTest, "ATD.Logic.Save.RoundTrip", ATDTestFlags)
bool FATDSaveRoundTripTest::RunTest(const FString& Parameters)
{
	FMath::RandInit(777);
	const FTDItem Original = TD::GenerateItem(42, 12, ETDRarity::Legendary);

	// item <-> save struct
	const FTDItem Back = FTDItemSave::FromItem(Original).ToItem();
	TestEqual(TEXT("Item id"), Back.Id, Original.Id);
	TestEqual(TEXT("Item type"), Back.Kind, Original.Kind);
	TestEqual(TEXT("Item name"), Back.Name, Original.Name);
	TestEqual(TEXT("Item rarity"), Back.Rarity, Original.Rarity);
	TestEqual(TEXT("Item slot"), Back.Slot, Original.Slot);
	TestEqual(TEXT("Item level"), Back.Level, Original.Level);
	TestFalse(TEXT("Item unlocked by default"), Back.bLocked);
	{
		FTDItem Locked = Original;
		Locked.bLocked = true;
		TestTrue(TEXT("Item lock survives a save"), FTDItemSave::FromItem(Locked).ToItem().bLocked);
		TD::RerollItem(Locked);
		TestTrue(TEXT("Item lock survives a reroll"), Locked.bLocked);
	}
	TestEqual(TEXT("Affix count"), Back.Affixes.Num(), Original.Affixes.Num());
	for (int32 i = 0; i < FMath::Min(Back.Affixes.Num(), Original.Affixes.Num()); ++i)
	{
		TestEqual(TEXT("Affix type"), Back.Affixes[i].Type, Original.Affixes[i].Type);
		TestEqual(TEXT("Affix value"), Back.Affixes[i].Value, Original.Affixes[i].Value);
	}

	// typed items load from their type, whatever affixes were saved with them
	FTDItemSave Tampered = FTDItemSave::FromItem(Original);
	Tampered.Affixes.Reset();
	Tampered.Name = TEXT("Old name");
	const FTDItem Rebuilt = Tampered.ToItem();
	TestEqual(TEXT("Typed item rebuilt from its type"), Rebuilt.Affixes.Num(), Original.Affixes.Num());
	TestEqual(TEXT("Typed item name from its type"), Rebuilt.Name, Original.Name);

	// items from older saves get their range affixes halved, once
	FTDItemSave OldSave;
	auto AddOldAffix = [&OldSave](ETDAffix Type, float Value)
	{
		FTDAffixSave& A = OldSave.Affixes.AddDefaulted_GetRef();
		A.Type = (uint8)Type;
		A.Value = Value;
	};
	AddOldAffix(ETDAffix::Range, 1.0f);
	AddOldAffix(ETDAffix::RangePct, 0.4f);
	AddOldAffix(ETDAffix::DamagePct, 0.5f);
	const FTDItem Migrated = OldSave.ToItem();
	TestEqual(TEXT("Old flat range halved"), Migrated.Affixes[0].Value, 0.5f);
	TestEqual(TEXT("Old percent range halved"), Migrated.Affixes[1].Value, 0.2f);
	TestEqual(TEXT("Old damage untouched"), Migrated.Affixes[2].Value, 0.5f);
	TestEqual(TEXT("Resaved range not halved again"), FTDItemSave::FromItem(Migrated).ToItem().Affixes[0].Value, 0.5f);

	// old projectile speed affixes turn into attack speed, and later affix numbers stay put
	TestEqual(TEXT("Item chance keeps its saved number"), (int32)ETDAffix::ItemChance, 17);
	TestEqual(TEXT("New affixes come after the old ones"), (int32)ETDAffix::Stun, 18);
	OldSave.Affixes.Reset();
	AddOldAffix(ETDAffix::RetiredProjectileSpeed, 0.5f);
	const FTDItem OnlySpeed = OldSave.ToItem();
	TestEqual(TEXT("Speed-only item keeps one affix"), OnlySpeed.Affixes.Num(), 1);
	TestEqual(TEXT("Old speed becomes attack speed"), OnlySpeed.Affixes[0].Type, ETDAffix::RatePct);
	TestEqual(TEXT("Old speed value converted"), OnlySpeed.Affixes[0].Value, 0.5f * TD::RetiredSpeedToRate);
	TestFalse(TEXT("Converted affix has text"), TD::AffixText(OnlySpeed.Affixes[0]).IsEmpty());

	AddOldAffix(ETDAffix::RatePct, 0.1f);
	AddOldAffix(ETDAffix::ItemChance, 0.3f);
	const FTDItem Merged = OldSave.ToItem();
	TestEqual(TEXT("Attack speed merged into one affix"), Merged.Affixes.Num(), 2);
	TestEqual(TEXT("Merged attack speed adds up"), Merged.Affixes[0].Value, 0.5f * TD::RetiredSpeedToRate + 0.1f);
	TestEqual(TEXT("Item chance still item chance"), Merged.Affixes[1].Type, ETDAffix::ItemChance);

	// full save object through binary serialization
	UTDSaveGame* Save = NewObject<UTDSaveGame>();
	Save->Pearls = 321;
	Save->Lives = 7;
	Save->Wave = 14;
	Save->NextItemId = 99;
	Save->BuildPhaseId = 15;
	Save->bEndless = true;
	Save->AbyssTier = 3;
	Save->Mutators = 0x5;
	Save->Boons = 0x102;
	Save->BoonOffer = { 4, 9, 13 };
	Save->Storage.Add(FTDItemSave::FromItem(Original));
	FTDTowerSave& Tower = Save->Towers.AddDefaulted_GetRef();
	Tower.Type = (uint8)ETDTowerType::Lionfish;
	Tower.Cell = FIntPoint(6, 3);
	Tower.Level = 5;
	Tower.Xp = 12.5f;
	Tower.SkillPoints = 2;
	Tower.SkillRanks = { 1, 0, 2 };
	Tower.Items.Add(FTDItemSave::FromItem(Original));
	Tower.TotalSpent = 75;
	Tower.SkillSpent = 55;
	Tower.DamageDone = 1234.5f;
	Tower.Kills = 17;

	TArray<uint8> Bytes;
	TestTrue(TEXT("Serialize"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	const UTDSaveGame* Loaded = Cast<UTDSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Deserialize"), Loaded))
	{
		return false;
	}

	TestEqual(TEXT("Pearls"), Loaded->Pearls, 321);
	TestEqual(TEXT("Lives"), Loaded->Lives, 7);
	TestEqual(TEXT("Wave"), Loaded->Wave, 14);
	TestEqual(TEXT("Next item id"), Loaded->NextItemId, 99);
	TestEqual(TEXT("Build phase"), Loaded->BuildPhaseId, 15);
	TestTrue(TEXT("Endless mode"), Loaded->bEndless);
	TestEqual(TEXT("Abyss tier"), Loaded->AbyssTier, 3);
	TestEqual(TEXT("Mutators"), Loaded->Mutators, 0x5);
	TestEqual(TEXT("Boons"), Loaded->Boons, 0x102);
	TestEqual(TEXT("Pending boon offer"), Loaded->BoonOffer, TArray<int32>({ 4, 9, 13 }));
	TestFalse(TEXT("Boons on by default"), Loaded->bNoBoons);
	{
		// a save from before run rules loads as a plain run
		const UTDSaveGame* Fresh = GetDefault<UTDSaveGame>();
		TestEqual(TEXT("Old saves: Abyss 0"), Fresh->AbyssTier, 0);
		TestEqual(TEXT("Old saves: no mutators"), Fresh->Mutators, 0);
		TestEqual(TEXT("Old saves: no boons"), Fresh->Boons, 0);
		TestEqual(TEXT("Old saves: no offer"), Fresh->BoonOffer.Num(), 0);
	}
	TestEqual(TEXT("Storage size"), Loaded->Storage.Num(), 1);
	TestEqual(TEXT("Tower count"), Loaded->Towers.Num(), 1);
	if (Loaded->Towers.Num() == 1)
	{
		const FTDTowerSave& T = Loaded->Towers[0];
		TestEqual(TEXT("Tower type"), T.Type, (uint8)ETDTowerType::Lionfish);
		TestEqual(TEXT("Tower cell"), T.Cell, FIntPoint(6, 3));
		TestEqual(TEXT("Tower level"), T.Level, 5);
		TestEqual(TEXT("Tower xp"), T.Xp, 12.5f);
		TestEqual(TEXT("Tower skill points"), T.SkillPoints, 2);
		TestEqual(TEXT("Tower ranks"), T.SkillRanks, TArray<int32>({ 1, 0, 2 }));
		TestEqual(TEXT("Tower items"), T.Items.Num(), 1);
		TestEqual(TEXT("Tower spent"), T.TotalSpent, 75);
		TestEqual(TEXT("Tower skill spend"), T.SkillSpent, 55);
		TestEqual(TEXT("Tower damage"), T.DamageDone, 1234.5f);
		TestEqual(TEXT("Tower kills"), T.Kills, 17);
		TestEqual(TEXT("Equipped item survives"), T.Items[0].ToItem().Name, Original.Name);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Final wave and endless mode
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDFinalWaveTest, "ATD.Logic.Waves.FinalAndEndless", ATDTestFlags)
bool FATDFinalWaveTest::RunTest(const FString& Parameters)
{
	// the Abyssal Kraken comes on the final wave, then every 50th endless wave
	TestEqual(TEXT("Final wave"), TD::FinalWave, 100);
	TestTrue(TEXT("Titan on wave 100"), TD::IsTitanWave(100));
	TestFalse(TEXT("No titan on wave 50"), TD::IsTitanWave(50));
	TestFalse(TEXT("No titan on wave 90"), TD::IsTitanWave(90));
	TestFalse(TEXT("No titan on wave 110"), TD::IsTitanWave(110));
	TestTrue(TEXT("Titan on wave 150"), TD::IsTitanWave(150));
	TestTrue(TEXT("Titan on wave 200"), TD::IsTitanWave(200));

	auto Krakens = [](int32 N)
	{
		TArray<TPair<ETDEnemyType, int32>> Groups;
		ATDGameMode::GetWaveComposition(N, Groups);
		int32 Count = 0;
		for (const TPair<ETDEnemyType, int32>& Group : Groups)
		{
			Count += Group.Key == ETDEnemyType::Kraken ? Group.Value : 0;
		}
		return Count;
	};
	TestEqual(TEXT("Regular boss wave 90 krakens"), Krakens(90), 9);
	TestEqual(TEXT("Wave 100: four krakens and the Abyssal Kraken"), Krakens(100), 5);
	TestEqual(TEXT("Wave 200: eight krakens and the Abyssal Kraken"), Krakens(200), 9);
	TestTrue(TEXT("The final boss is far tougher than a kraken"), ATDEnemy::TitanHealthFactor >= 3.0f);
	TestTrue(TEXT("The final boss has a shield to break"), ATDEnemy::TitanShieldFraction > 0.0f && ATDEnemy::TitanShieldFraction <= 1.0f);
	TestTrue(TEXT("The final boss resists bleed more than other bosses"), ATDEnemy::TitanBleedScale < ATDEnemy::BossBleedScale && ATDEnemy::BossBleedScale < 1.0f);

	// the normal health curve is unchanged up to the final wave
	TestEqual(TEXT("No endless factor before the final wave"), TD::EndlessHealthFactor(100), 1.0f);
	TestEqual(TEXT("Wave 1 health"), TD::WaveHealthMultiplier(1), 1.0f, 0.001f);
	const float Wave100 = (1.0f + 99 * 0.2f + 99 * 99 * 0.016f + 99.0f * 99 * 99 * TD::WaveHealthCubic) * TD::WavePressure(100);
	TestEqual(TEXT("Wave 100 health"), TD::WaveHealthMultiplier(100), Wave100, Wave100 * 0.0001f);
	TestEqual(TEXT("No wave pressure before it starts"), TD::WavePressure(TD::WavePressureMidFrom), 1.0f);
	TestEqual(TEXT("Wave pressure holds in endless mode"), TD::WavePressure(150), TD::WavePressure(100));
	TestEqual(TEXT("No late pressure before it starts"), TD::WavePressureLatePart(TD::WavePressureLateFrom), 1.0f);
	TestTrue(TEXT("Late pressure is part of the whole"), TD::WavePressureLatePart(100) <= TD::WavePressure(100));

	// endless waves grow several times faster than the normal curve did
	const float NormalGrowth = TD::WaveHealthMultiplier(100) / TD::WaveHealthMultiplier(99);
	const float EndlessGrowth = TD::WaveHealthMultiplier(131) / TD::WaveHealthMultiplier(130);
	TestTrue(FString::Printf(TEXT("Normal growth (%.3f) is below endless growth"), NormalGrowth), NormalGrowth < EndlessGrowth);
	TestTrue(FString::Printf(TEXT("Endless growth is steep (%.3f)"), EndlessGrowth), EndlessGrowth > 1.08f);
	TestTrue(TEXT("Wave 150 is far beyond the normal curve"), TD::WaveHealthMultiplier(150) > 25.0f * (1.0f + 149 * 0.2f + 149 * 149 * 0.016f));
	TestTrue(TEXT("Health stays finite deep into endless"), FMath::IsFinite(TD::WaveHealthMultiplier(5000)));

	// speed, armor and modifier chance only climb in endless mode
	TestEqual(TEXT("No endless speed at 100"), TD::EndlessSpeedMultiplier(100), 1.0f);
	TestEqual(TEXT("Endless speed at 110"), TD::EndlessSpeedMultiplier(110), 1.15f, 0.001f);
	TestEqual(TEXT("Endless speed caps"), TD::EndlessSpeedMultiplier(400), 1.75f, 0.001f);
	TestEqual(TEXT("No endless armor at 100"), TD::EndlessArmorBonus(100), 0.0f);
	TestEqual(TEXT("Endless armor at 120"), TD::EndlessArmorBonus(120), 10.0f, 0.001f);
	TestEqual(TEXT("No modifiers before wave 6"), TD::ModifierChance(5), 0.0f);
	TestEqual(TEXT("Wave 6 modifiers"), TD::ModifierChance(6), TD::ModifierChanceBase + 6 * TD::ModifierChancePerWave, 0.001f);
	TestEqual(TEXT("Modifier chance caps in the mid game"), TD::ModifierChance(TD::ModifierChanceLateFrom), TD::ModifierChanceCap, 0.001f);
	const float Late100 = TD::ModifierChanceLatePerWave * (TD::FinalWave - TD::ModifierChanceLateFrom);
	TestEqual(TEXT("Modifier chance climbs again late"), TD::ModifierChance(100), TD::ModifierChanceCap + Late100, 0.001f);
	TestEqual(TEXT("Endless modifier chance at 110"), TD::ModifierChance(110), TD::ModifierChanceCap + Late100 + 0.1f, 0.001f);
	TestEqual(TEXT("Endless modifier chance caps"), TD::ModifierChance(200), 0.75f, 0.001f);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Maps
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDMapsTest, "ATD.Logic.Pathing.Maps", ATDTestFlags)
bool FATDMapsTest::RunTest(const FString& Parameters)
{
	const int32 Previous = TD::GetMap();
	int32 ShapedMaps = 0;
	for (int32 Map = 0; Map < TD::MapCount; ++Map)
	{
		TD::SetMap(Map);
		const FIntPoint Spawn = TD::SpawnCell();
		const FIntPoint Goal = TD::GoalCell();
		const bool bSpawnOnEdge = Spawn.X == 0 || Spawn.Y == 0 || Spawn.X == TD::Cols - 1 || Spawn.Y == TD::Rows - 1;
		const bool bGoalOnEdge = Goal.X == 0 || Goal.Y == 0 || Goal.X == TD::Cols - 1 || Goal.Y == TD::Rows - 1;
		TestTrue(FString::Printf(TEXT("%s: trench and reef on the board edge"), TD::MapName(Map)), TD::InBounds(Spawn) && TD::InBounds(Goal) && bSpawnOnEdge && bGoalOnEdge);

		TArray<uint8> Cells;
		TD::MakeStartingCells(Cells);
		TestEqual(FString::Printf(TEXT("%s: trench is open"), TD::MapName(Map)), Cells[TD::CellIndex(Spawn)], TDCell::Empty);
		TestEqual(FString::Printf(TEXT("%s: reef is open"), TD::MapName(Map)), Cells[TD::CellIndex(Goal)], TDCell::Empty);

		// outline: rocks sit on the seabed, open-water cells are void and can't be built on
		for (const FIntPoint& Rock : TD::MapRocks(Map))
		{
			TestTrue(FString::Printf(TEXT("%s: rock (%d, %d) is on the board"), TD::MapName(Map), Rock.X, Rock.Y), TD::IsOnBoard(Rock));
		}
		int32 VoidCells = 0;
		for (int32 Y = 0; Y < TD::Rows; ++Y)
		{
			for (int32 X = 0; X < TD::Cols; ++X)
			{
				const FIntPoint Cell(X, Y);
				if (TD::IsOnBoard(Cell))
				{
					continue;
				}
				++VoidCells;
				TestEqual(FString::Printf(TEXT("%s: (%d, %d) is open water"), TD::MapName(Map), X, Y), Cells[TD::CellIndex(Cell)], TDCell::Void);
				TestEqual(FString::Printf(TEXT("%s: can't build on open water"), TD::MapName(Map)), TD::EvaluatePlacement(Cells, Cell, TArray<FTDWalker>()), ETDPlaceResult::OutOfBounds);
			}
		}
		ShapedMaps += VoidCells > 0 ? 1 : 0;
		TestTrue(FString::Printf(TEXT("%s: most of the grid is seabed"), TD::MapName(Map)), VoidCells < TD::Cols * TD::Rows / 4);
		const FLinearColor Tint = TD::MapTint(Map);
		TestTrue(FString::Printf(TEXT("%s: seabed tint stays faint"), TD::MapName(Map)), FMath::Abs(Tint.R - 1.0f) <= 0.2f && FMath::Abs(Tint.G - 1.0f) <= 0.2f && FMath::Abs(Tint.B - 1.0f) <= 0.2f);

		TArray<int32> Field;
		TD::ComputeField(Cells, Field);
		TestTrue(FString::Printf(TEXT("%s: a path exists"), TD::MapName(Map)), Field[TD::CellIndex(Spawn)] > 0);
		AddInfo(FString::Printf(TEXT("%s: open path %d tiles"), TD::MapName(Map), Field[TD::CellIndex(Spawn)]));
	}
	TestTrue(TEXT("some maps are not rectangular"), ShapedMaps > 0 && ShapedMaps < TD::MapCount);
	TD::SetMap(Previous);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Icons
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDIconTest, "ATD.Logic.UI.Icons", ATDTestFlags)
bool FATDIconTest::RunTest(const FString& Parameters)
{
	// every tower, creature, skill, item slot and top bar stat has a visible icon
	const ETDIcon Kinds[] = { ETDIcon::Tower, ETDIcon::Enemy, ETDIcon::Skill, ETDIcon::Item, ETDIcon::Misc };
	int32 Rendered = 0;
	for (ETDIcon Kind : Kinds)
	{
		for (int32 Id = 0; Id < TDIcons::Count(Kind); ++Id)
		{
			if (Kind == ETDIcon::Skill && !TD::Skill((ETDTowerType)(Id / TD::SkillsPerTower), Id % TD::SkillsPerTower))
			{
				TArray<FColor> Unused;
				TestFalse(TEXT("No icon for a missing skill"), TDIcons::Render(Kind, Id, 32, Unused));
				continue;
			}
			TArray<FColor> Pixels;
			if (!TestTrue(FString::Printf(TEXT("Icon %d/%d renders"), (int32)Kind, Id), TDIcons::Render(Kind, Id, 32, Pixels)))
			{
				continue;
			}
			int32 Opaque = 0;
			for (const FColor& Px : Pixels)
			{
				Opaque += Px.A > 128 ? 1 : 0;
			}
			// at least 8% of the tile is drawn, and the border isn't filled edge to edge
			TestTrue(FString::Printf(TEXT("Icon %d/%d has content (%d px)"), (int32)Kind, Id, Opaque), Opaque > 32 * 32 * 0.08f);
			TestTrue(FString::Printf(TEXT("Icon %d/%d leaves a margin"), (int32)Kind, Id), Pixels[0].A < 64 && Pixels[31].A < 64);
			++Rendered;
		}
	}
	TestEqual(TEXT("Icons rendered"), Rendered, TD::TowerTypeCount + TD::EnemyTypeCount + (TD::TowerTypeCount - 1) * TD::SkillsPerTower + 3 + (int32)ETDMiscIcon::Count);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Specialisation
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDFormTableTest, "ATD.Logic.Towers.Forms", ATDTestFlags)
bool FATDFormTableTest::RunTest(const FString& Parameters)
{
	// every tower but the wall has exactly two named forms with both lines filled
	for (int32 t = 0; t < TD::TowerTypeCount; ++t)
	{
		const ETDTowerType Type = (ETDTowerType)t;
		const FTDTowerDef& Def = TD::Tower(Type);
		TestNull(TEXT("No form 0"), TD::Form(Type, 0));
		TestNull(TEXT("No form 3"), TD::Form(Type, TD::FormsPerTower + 1));
		for (int32 f = 1; f <= TD::FormsPerTower; ++f)
		{
			const FTDFormDef* Form = TD::Form(Type, f);
			if (Type == ETDTowerType::Wall)
			{
				TestNull(FString::Printf(TEXT("%s has no forms"), Def.Name), Form);
				continue;
			}
			if (TestNotNull(FString::Printf(TEXT("%s form %d"), Def.Name, f), Form))
			{
				TestTrue(FString::Printf(TEXT("%s form %d is described"), Def.Name, f), FCString::Strlen(Form->Name) > 0 && FCString::Strlen(Form->Line1) > 0 && FCString::Strlen(Form->Line2) > 0);
			}
		}
		if (Type != ETDTowerType::Wall)
		{
			TestNotEqual(FString::Printf(TEXT("%s forms differ"), Def.Name), FString(TD::Form(Type, 1)->Name), FString(TD::Form(Type, 2)->Name));
		}
	}
	TestTrue(TEXT("Forms come after the first skill ranks"), TD::FormLevel > TD::SkillLevelRequirement(3) && TD::FormLevel < TD::MaxTowerLevel);
	return true;
}

// ---------------------------------------------------------------------------------------------
// Run rules: score, Abyss tiers, mutators and boons
// ---------------------------------------------------------------------------------------------

namespace
{
	/** Puts the run rules back after a test changes them */
	struct FRunRulesGuard
	{
		FTDRunRules Saved = TD::Run();
		~FRunRulesGuard() { TD::SetRun(Saved); }
	};

	uint32 Bit(ETDMutator Mutator) { return 1u << (uint32)Mutator; }
	uint32 Bit(ETDBoon Boon) { return 1u << (uint32)Boon; }

	int32 CountInWave(int32 Wave, ETDEnemyType Type)
	{
		TArray<TPair<ETDEnemyType, int32>> Groups;
		ATDGameMode::GetWaveComposition(Wave, Groups);
		int32 Count = 0;
		for (const TPair<ETDEnemyType, int32>& Group : Groups)
		{
			Count += Group.Key == Type ? Group.Value : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDScoreMultiplierTest, "ATD.Logic.Run.ScoreMultiplier", ATDTestFlags)
bool FATDScoreMultiplierTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Plain run x1"), TD::ScoreMultiplier(0, 0), 1.0f);
	TestEqual(TEXT("Glass Reef +100%"), TD::ScoreMultiplier(0, Bit(ETDMutator::GlassReef)), 2.0f);
	TestEqual(TEXT("Swarm +25%"), TD::ScoreMultiplier(0, Bit(ETDMutator::Swarm)), 1.25f);
	TestEqual(TEXT("Armored Tide +30%"), TD::ScoreMultiplier(0, Bit(ETDMutator::ArmoredTide)), 1.3f, 0.0001f);
	TestEqual(TEXT("Swimmer Frenzy +30%"), TD::ScoreMultiplier(0, Bit(ETDMutator::FlyerFrenzy)), 1.3f, 0.0001f);
	TestEqual(TEXT("No Walls +50%"), TD::ScoreMultiplier(0, Bit(ETDMutator::NoWalls)), 1.5f);
	TestEqual(TEXT("Poor Waters +30%"), TD::ScoreMultiplier(0, Bit(ETDMutator::PoorWaters)), 1.3f, 0.0001f);
	TestEqual(TEXT("Abyss adds 20% per tier"), TD::ScoreMultiplier(3, 0), 1.6f, 0.0001f);
	TestEqual(TEXT("Bonuses add up"), TD::ScoreMultiplier(2, Bit(ETDMutator::GlassReef) | Bit(ETDMutator::NoWalls)), 1.0f + 0.4f + 1.0f + 0.5f, 0.0001f);

	const uint32 All = (1u << (uint32)TD::MutatorCount) - 1u;
	const float Later = 0.25f + 0.35f + 0.2f + 0.3f;	// Murky Water, Rising Tide, Fixed Towers, Lean Reef
	TestEqual(TEXT("Every mutator at tier 0"), TD::ScoreMultiplier(0, All), 1.0f + 1.0f + 0.25f + 0.3f + 0.3f + 0.5f + 0.3f + Later, 0.0001f);
	TestEqual(TEXT("Poor Waters adds nothing at Abyss 4+ (it's part of the tier)"), TD::ScoreMultiplier(4, All), 1.0f + 0.8f + 1.0f + 0.25f + 0.3f + 0.3f + 0.5f + Later, 0.0001f);
	TestEqual(TEXT("Every mutator at tier 5"), TD::ScoreMultiplier(5, All), 1.0f + 1.0f + 1.0f + 0.25f + 0.3f + 0.3f + 0.5f + Later, 0.0001f);
	TestEqual(TEXT("Murky Water +25%"), TD::ScoreMultiplier(0, Bit(ETDMutator::MurkyWater)), 1.25f);
	TestEqual(TEXT("Rising Tide +35%"), TD::ScoreMultiplier(0, Bit(ETDMutator::RisingTide)), 1.35f, 0.0001f);
	TestEqual(TEXT("Fixed Towers +20%"), TD::ScoreMultiplier(0, Bit(ETDMutator::FixedTowers)), 1.2f, 0.0001f);
	TestEqual(TEXT("Lean Reef +30%"), TD::ScoreMultiplier(0, Bit(ETDMutator::LeanReef)), 1.3f, 0.0001f);
	TestEqual(TEXT("The first six mutators keep their bits (old saves)"), (int32)ETDMutator::PoorWaters, 5);
	TestEqual(TEXT("Tiers past the last count as the last"), TD::ScoreMultiplier(9, 0), TD::ScoreMultiplier(TD::MaxAbyssTier, 0));
	TestEqual(TEXT("Unknown mutator bits add nothing"), TD::ScoreMultiplier(0, 1u << 30), 1.0f);

	TestTrue(TEXT("Poor Waters included at Abyss 4"), TD::IsMutatorIncluded(ETDMutator::PoorWaters, 4));
	TestFalse(TEXT("Poor Waters free to pick at Abyss 3"), TD::IsMutatorIncluded(ETDMutator::PoorWaters, 3));
	TestEqual(TEXT("Clean drops Poor Waters at Abyss 4"), TD::CleanMutators(Bit(ETDMutator::PoorWaters) | Bit(ETDMutator::Swarm), 4), Bit(ETDMutator::Swarm));

	TestEqual(TEXT("Multiplier text x2"), TD::MultiplierText(2.0f), FString(TEXT("x2")));
	TestEqual(TEXT("Multiplier text x1.5"), TD::MultiplierText(1.5f), FString(TEXT("x1.5")));
	TestEqual(TEXT("Multiplier text x1.75"), TD::MultiplierText(1.75f), FString(TEXT("x1.75")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDRunScoreTest, "ATD.Logic.Run.Score", ATDTestFlags)
bool FATDRunScoreTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("100 per wave cleared"), TD::RunScore(37, 12, false, 1.0f), 3700);
	TestEqual(TEXT("Lives only count on a win"), TD::RunScore(37, 12, false, 2.0f), 7400);
	TestEqual(TEXT("A win adds 25 per life, then the multiplier"), TD::RunScore(100, 12, true, 1.5f), 15450);
	TestEqual(TEXT("Rounded"), TD::RunScore(3, 0, false, 1.3f), 390);
	TestEqual(TEXT("Rounded to the nearest point"), TD::RunScore(1, 1, true, 1.25f), 156);
	TestEqual(TEXT("Nothing cleared, nothing scored"), TD::RunScore(0, 20, false, 3.0f), 0);
	TestEqual(TEXT("Never negative"), TD::RunScore(-5, -3, true, 1.0f), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDAbyssTiersTest, "ATD.Logic.Run.AbyssTiers", ATDTestFlags)
bool FATDAbyssTiersTest::RunTest(const FString& Parameters)
{
	FRunRulesGuard Guard;

	// each tier keeps every rule of the tiers below it
	for (int32 Tier = 0; Tier <= TD::MaxAbyssTier; ++Tier)
	{
		FTDRunRules Rules;
		Rules.Tier = Tier;
		const FString At = FString::Printf(TEXT(" at Abyss %d"), Tier);
		TestEqual(TEXT("A1 modifier chance") + At, Rules.ModifierChanceBonus(), Tier >= 1 ? 0.1f : 0.0f);
		TestEqual(TEXT("A2 leak cost") + At, Rules.LeakLivesScale(), Tier >= 2 ? 2 : 1);
		TestEqual(TEXT("A3 flyer groups") + At, Rules.AddsFlyerGroups(), Tier >= 3);
		TestEqual(TEXT("A4 wave-clear pearls") + At, Rules.HasWaveClearPearls(), Tier < 4);
		TestEqual(TEXT("A5 sell refunds") + At, Rules.SellRefundScale(), Tier >= 5 ? 0.5f : 1.0f);
		TestFalse(TEXT("Every tier rule is described") + At, Tier > 0 && FCString::Strlen(TD::AbyssRule(Tier)) == 0);
	}
	FTDRunRules Poor;
	Poor.Mutators = Bit(ETDMutator::PoorWaters);
	TestFalse(TEXT("Poor Waters removes wave-clear pearls"), Poor.HasWaveClearPearls());

	// A1: +10 points on the modifier chance and its caps
	TD::SetRun(FTDRunRules());
	const float Normal10 = TD::ModifierChance(10);
	const float Normal100 = TD::ModifierChance(TD::FinalWave);
	FTDRunRules A1;
	A1.Tier = 1;
	TD::SetRun(A1);
	TestEqual(TEXT("A1 mid-game chance +10 points"), TD::ModifierChance(10), Normal10 + 0.1f, 0.0001f);
	TestEqual(TEXT("A1 raises the cap by 10 points"), TD::ModifierChance(TD::FinalWave), Normal100 + 0.1f, 0.0001f);
	TestEqual(TEXT("A1 still no modifiers before wave 6"), TD::ModifierChance(5), 0.0f);

	// A3: a Manta group from wave 10 in waves without flyers, boss waves included
	FTDRunRules A2;
	A2.Tier = 2;
	TD::SetRun(A2);
	TestEqual(TEXT("Wave 12 has no Mantas below A3"), CountInWave(12, ETDEnemyType::Manta), 0);
	const int32 Wave13Mantas = CountInWave(13, ETDEnemyType::Manta);
	FTDRunRules A3;
	A3.Tier = 3;
	TD::SetRun(A3);
	TestEqual(TEXT("A3 adds Mantas to wave 12"), CountInWave(12, ETDEnemyType::Manta), 2 + 12 / 4);
	TestEqual(TEXT("A3 adds Mantas to the wave 10 boss wave"), CountInWave(10, ETDEnemyType::Manta), 2 + 10 / 4);
	TestEqual(TEXT("A3 leaves waves before 10 alone"), CountInWave(9, ETDEnemyType::Manta), 0);
	TestEqual(TEXT("A3 leaves waves that have flyers alone"), CountInWave(13, ETDEnemyType::Manta), Wave13Mantas);
	TestTrue(TEXT("A3 adds Mantas to the final wave"), CountInWave(TD::FinalWave, ETDEnemyType::Manta) > 0);
	TestTrue(TEXT("The final wave still has the Abyssal Kraken"), CountInWave(TD::FinalWave, ETDEnemyType::Kraken) > 0);
	FTDRunRules A5;
	A5.Tier = 5;
	TD::SetRun(A5);
	TestEqual(TEXT("A5 has the A3 flyers too"), CountInWave(12, ETDEnemyType::Manta), 2 + 12 / 4);

	// mutators that change waves
	TD::SetRun(FTDRunRules());
	const int32 Crabs1 = CountInWave(1, ETDEnemyType::Crab);
	const int32 Mantas4 = CountInWave(4, ETDEnemyType::Manta);
	const int32 Krakens20 = CountInWave(20, ETDEnemyType::Kraken);
	FTDRunRules Swarm;
	Swarm.Mutators = Bit(ETDMutator::Swarm);
	TD::SetRun(Swarm);
	TestEqual(TEXT("Swarm doubles creatures"), CountInWave(1, ETDEnemyType::Crab), Crabs1 * 2);
	TestEqual(TEXT("Swarm leaves bosses alone"), CountInWave(20, ETDEnemyType::Kraken), Krakens20);
	FTDRunRules Frenzy;
	Frenzy.Mutators = Bit(ETDMutator::FlyerFrenzy);
	TD::SetRun(Frenzy);
	TestEqual(TEXT("Swimmer Frenzy doubles flyer groups"), CountInWave(4, ETDEnemyType::Manta), Mantas4 * 2);
	TestEqual(TEXT("Swimmer Frenzy leaves ground groups alone"), CountInWave(1, ETDEnemyType::Crab), Crabs1);
	FTDRunRules Both;
	Both.Mutators = Bit(ETDMutator::FlyerFrenzy) | Bit(ETDMutator::Swarm);
	TD::SetRun(Both);
	TestEqual(TEXT("Swarm and Swimmer Frenzy stack"), CountInWave(4, ETDEnemyType::Manta), Mantas4 * 4);

	FTDRunRules Glass;
	Glass.Mutators = Bit(ETDMutator::GlassReef) | Bit(ETDMutator::NoWalls) | Bit(ETDMutator::ArmoredTide);
	TestEqual(TEXT("Glass Reef starts with 1 life"), Glass.StartLives(), 1);
	TestEqual(TEXT("Normal start lives"), FTDRunRules().StartLives(), TD::StartLives);
	TestFalse(TEXT("No Walls bans walls"), Glass.CanBuildWalls());
	TestEqual(TEXT("Armored Tide +50% armor"), Glass.ArmorScale(), 1.5f);

	// boons that change prices
	FTDRunRules Masons;
	Masons.Boons = Bit(ETDBoon::CoralMasons) | Bit(ETDBoon::CheapUpgrades);
	TD::SetRun(FTDRunRules());
	const int32 SkillCost = TD::SkillCost(ETDTowerType::Harpoon, 0, 3);
	TD::SetRun(Masons);
	TestEqual(TEXT("Coral Masons: walls cost 2"), TD::TowerCost(ETDTowerType::Wall), TD::MasonWallCost);
	TestEqual(TEXT("Coral Masons leaves towers alone"), TD::TowerCost(ETDTowerType::Harpoon), TD::Tower(ETDTowerType::Harpoon).Cost);
	TestTrue(TEXT("Cheap Upgrades lowers skill costs"), TD::SkillCost(ETDTowerType::Harpoon, 0, 3) < SkillCost);
	TestEqual(TEXT("Deep Pockets: 3% interest"), TD::Interest(500), 15);
	TestEqual(TEXT("Deep Pockets: capped at 30"), TD::Interest(5000), TD::InterestCap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDBoonOfferTest, "ATD.Logic.Run.BoonOffers", ATDTestFlags)
bool FATDBoonOfferTest::RunTest(const FString& Parameters)
{
	FRandomStream Random(1234);
	TArray<int32> Offer;

	// a fresh run: three different boons
	const FTDRunRules Fresh;
	TSet<int32> Seen;
	bool bThreeDifferent = true;
	for (int32 Roll = 0; Roll < 300; ++Roll)
	{
		TD::RollBoonOffer(Fresh, Random, Offer);
		bThreeDifferent &= Offer.Num() == TD::BoonOfferSize && TSet<int32>(Offer).Num() == Offer.Num();
		Seen.Append(Offer);
	}
	TestTrue(TEXT("Offers hold three different boons"), bThreeDifferent);
	TestEqual(TEXT("Every boon turns up in a fresh run"), Seen.Num(), TD::BoonCount);

	// taken boons and boons that don't fit the run never show up
	FTDRunRules Rules;
	Rules.Mutators = Bit(ETDMutator::NoWalls) | Bit(ETDMutator::GlassReef);
	Rules.Boons = Bit(ETDBoon::SharpenedReef) | Bit(ETDBoon::StormCoil);
	bool bClean = true;
	for (int32 Roll = 0; Roll < 300; ++Roll)
	{
		TD::RollBoonOffer(Rules, Random, Offer);
		for (const int32 Boon : Offer)
		{
			bClean &= Boon != (int32)ETDBoon::SharpenedReef && Boon != (int32)ETDBoon::StormCoil
				&& Boon != (int32)ETDBoon::CoralMasons && Boon != (int32)ETDBoon::ThickReef;
		}
	}
	TestTrue(TEXT("No taken boons, wall price under No Walls or lives under Glass Reef"), bClean);
	TestFalse(TEXT("Coral Masons doesn't fit No Walls"), TD::BoonFits(ETDBoon::CoralMasons, Rules));
	TestFalse(TEXT("Thick Reef doesn't fit Glass Reef"), TD::BoonFits(ETDBoon::ThickReef, Rules));
	TestFalse(TEXT("A taken boon doesn't fit"), TD::BoonFits(ETDBoon::SharpenedReef, Rules));
	TestTrue(TEXT("Coral Masons fits a run with walls"), TD::BoonFits(ETDBoon::CoralMasons, Fresh));
	TestTrue(TEXT("Thick Reef fits a run with lives"), TD::BoonFits(ETDBoon::ThickReef, Fresh));

	// fewer boons left than an offer holds, then none
	FTDRunRules Late;
	Late.Boons = ((1u << (uint32)TD::BoonCount) - 1u) & ~(Bit(ETDBoon::LongSight) | Bit(ETDBoon::VenomBloom));
	TD::RollBoonOffer(Late, Random, Offer);
	TestEqual(TEXT("Only two left: offer of two"), Offer.Num(), 2);
	TestTrue(TEXT("The two that are left"), Offer.Contains((int32)ETDBoon::LongSight) && Offer.Contains((int32)ETDBoon::VenomBloom));
	Late.Boons = (1u << (uint32)TD::BoonCount) - 1u;
	TD::RollBoonOffer(Late, Random, Offer);
	TestEqual(TEXT("All taken: no offer"), Offer.Num(), 0);

	// the same seed gives the same offer
	FRandomStream A(99);
	FRandomStream B(99);
	TArray<int32> OfferA;
	TArray<int32> OfferB;
	TD::RollBoonOffer(Fresh, A, OfferA);
	TD::RollBoonOffer(Fresh, B, OfferB);
	TestEqual(TEXT("Same seed, same offer"), OfferA, OfferB);

	TestTrue(TEXT("Milestones at 25, 50 and 75"), TD::IsBoonMilestone(25) && TD::IsBoonMilestone(50) && TD::IsBoonMilestone(75));
	TestFalse(TEXT("No offer at 100 or other waves"), TD::IsBoonMilestone(100) || TD::IsBoonMilestone(24) || TD::IsBoonMilestone(0));
	for (int32 b = 0; b < TD::BoonCount; ++b)
	{
		TestTrue(FString::Printf(TEXT("Boon %d is named and described"), b), FCString::Strlen(TD::BoonName((ETDBoon)b)) > 0 && FCString::Strlen(TD::BoonDescription((ETDBoon)b)) > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDCursedBoonTest, "ATD.Logic.Run.CursedBoons", ATDTestFlags)
bool FATDCursedBoonTest::RunTest(const FString& Parameters)
{
	FRunRulesGuard Guard;
	FRandomStream Random(4321);
	TArray<int32> Offer;

	// at most one cursed card per offer, at about the set chance, and never without normal boons
	const FTDRunRules Fresh;
	int32 WithCurse = 0;
	bool bAtMostOne = true;
	bool bThree = true;
	const int32 Rolls = 2000;
	for (int32 Roll = 0; Roll < Rolls; ++Roll)
	{
		TD::RollBoonOffer(Fresh, Random, Offer);
		int32 Cursed = 0;
		for (const int32 Boon : Offer)
		{
			Cursed += TD::IsCursed((ETDBoon)Boon) ? 1 : 0;
		}
		bAtMostOne &= Cursed <= 1;
		bThree &= Offer.Num() == TD::BoonOfferSize && TSet<int32>(Offer).Num() == Offer.Num();
		WithCurse += Cursed;
	}
	TestTrue(TEXT("Never more than one cursed card"), bAtMostOne);
	TestTrue(TEXT("Still three different cards"), bThree);
	const float Share = (float)WithCurse / Rolls;
	TestTrue(FString::Printf(TEXT("About 35%% of offers have a cursed card (got %.0f%%)"), Share * 100.0f), Share > 0.30f && Share < 0.40f);

	bool bNoCurse = true;
	for (int32 Roll = 0; Roll < 300; ++Roll)
	{
		TD::RollBoonOffer(Fresh, Random, Offer, TD::BoonOfferSize, MAX_int32, false);
		for (const int32 Boon : Offer)
		{
			bNoCurse &= !TD::IsCursed((ETDBoon)Boon);
		}
	}
	TestTrue(TEXT("Kraken's Pact follow-up offers are never cursed"), bNoCurse);

	FTDRunRules NormalsGone;
	for (int32 b = 0; b < TD::BoonCount; ++b)
	{
		if (!TD::IsCursed((ETDBoon)b))
		{
			NormalsGone.Boons |= 1u << (uint32)b;
		}
	}
	bool bEmpty = true;
	for (int32 Roll = 0; Roll < 100; ++Roll)
	{
		TD::RollBoonOffer(NormalsGone, Random, Offer);
		bEmpty &= Offer.Num() == 0;
	}
	TestTrue(TEXT("No offer of only cursed boons"), bEmpty);

	// exclusions
	FTDRunRules Glass;
	Glass.Mutators = Bit(ETDMutator::GlassReef);
	TestFalse(TEXT("Blood Tide not under Glass Reef"), TD::BoonFits(ETDBoon::BloodTide, Glass));
	TestFalse(TEXT("Kraken's Pact not under Glass Reef"), TD::BoonFits(ETDBoon::KrakensPact, Glass));
	FTDRunRules NoWalls;
	NoWalls.Mutators = Bit(ETDMutator::NoWalls);
	TestFalse(TEXT("Brittle Coral not under No Walls"), TD::BoonFits(ETDBoon::BrittleCoral, NoWalls));
	TestFalse(TEXT("Kraken's Pact not with 6 lives"), TD::BoonFits(ETDBoon::KrakensPact, Fresh, 6));
	TestTrue(TEXT("Kraken's Pact with 7 lives"), TD::BoonFits(ETDBoon::KrakensPact, Fresh, 7));
	FTDRunRules Brittle;
	Brittle.Boons = Bit(ETDBoon::BrittleCoral);
	TestFalse(TEXT("Coral Masons adds nothing after Brittle Coral"), TD::BoonFits(ETDBoon::CoralMasons, Brittle));
	TestFalse(TEXT("A taken cursed boon isn't offered again"), TD::BoonFits(ETDBoon::BrittleCoral, Brittle));
	bool bPactClean = true;
	for (int32 Roll = 0; Roll < 500; ++Roll)
	{
		TD::RollBoonOffer(Fresh, Random, Offer, TD::BoonOfferSize, 6);
		bPactClean &= !Offer.Contains((int32)ETDBoon::KrakensPact);
	}
	TestTrue(TEXT("Low lives: Kraken's Pact never offered"), bPactClean);

	// the multiplier counts cursed boons only
	TestEqual(TEXT("Normal boons add no score"), TD::ScoreMultiplier(0, 0, Bit(ETDBoon::SharpenedReef) | Bit(ETDBoon::StormCoil)), 1.0f);
	TestEqual(TEXT("Cursed boons add their bonus"), TD::ScoreMultiplier(0, 0, Bit(ETDBoon::BloodTide) | Bit(ETDBoon::Eclipse)), 1.35f, 0.0001f);
	TestEqual(TEXT("Cursed boons add to tier and mutators"), TD::ScoreMultiplier(2, Bit(ETDMutator::NoWalls), Bit(ETDBoon::FrenziedCurrents)), 1.0f + 0.4f + 0.5f + 0.2f, 0.0001f);
	float AllCursed = 1.0f;
	uint32 CursedBits = 0;
	for (int32 b = 0; b < TD::BoonCount; ++b)
	{
		const bool bCursed = TD::IsCursed((ETDBoon)b);
		TestEqual(FString::Printf(TEXT("Boon %d has a drawback only if cursed"), b), FCString::Strlen(TD::BoonDrawback((ETDBoon)b)) > 0, bCursed);
		TestEqual(FString::Printf(TEXT("Boon %d scores only if cursed"), b), TD::BoonScore((ETDBoon)b) > 0.0f, bCursed);
		if (bCursed)
		{
			AllCursed += TD::BoonScore((ETDBoon)b);
			CursedBits |= 1u << (uint32)b;
		}
	}
	TestEqual(TEXT("Every cursed boon adds up"), TD::ScoreMultiplier(0, 0, CursedBits), AllCursed, 0.0001f);
	TestEqual(TEXT("Eight cursed boons, +115% together"), AllCursed, 2.15f, 0.0001f);

	// Blood Tide: one more life per leak, after the Abyss 2 doubling
	FTDRunRules BloodTide;
	BloodTide.Boons = Bit(ETDBoon::BloodTide);
	TestEqual(TEXT("A crab leak costs 1 normally"), Fresh.LeakLives(1), 1);
	TestEqual(TEXT("Blood Tide: a crab leak costs 2"), BloodTide.LeakLives(1), 2);
	BloodTide.Tier = 2;
	TestEqual(TEXT("Blood Tide at Abyss 2: a turtle leak costs 2 x 2 + 1"), BloodTide.LeakLives(2), 5);

	// Gilded Shackles: half the refund, a quarter with Abyss 5; the build-phase full refund stays
	const int32 Plain = TD::SellValue(40, 1, 0.0f, 0, 40, false);
	FTDRunRules Gilded;
	Gilded.Boons = Bit(ETDBoon::GildedShackles);
	TD::SetRun(Gilded);
	TestEqual(TEXT("Gilded Shackles halves the refund"), TD::RunSellValue(40, 1, 0.0f, 0, 40, false), FMath::FloorToInt(Plain * 0.5f));
	Gilded.Tier = 5;
	TD::SetRun(Gilded);
	TestEqual(TEXT("Gilded Shackles with Abyss 5: a quarter"), TD::RunSellValue(40, 1, 0.0f, 0, 40, false), FMath::FloorToInt(Plain * 0.25f));
	TestEqual(TEXT("The build-phase full refund stays"), TD::RunSellValue(40, 1, 0.0f, 0, 40, true), 40);
	TestEqual(TEXT("Gilded Shackles: +60% kill pearls"), Gilded.KillPearlScale(), 1.6f);

	// the other upsides and drawbacks
	FTDRunRules Rest;
	Rest.Boons = Bit(ETDBoon::BrittleCoral) | Bit(ETDBoon::HungryDeep) | Bit(ETDBoon::SirensDebt) | Bit(ETDBoon::Eclipse) | Bit(ETDBoon::FrenziedCurrents);
	TD::SetRun(FTDRunRules());
	const int32 SkillCost = TD::SkillCost(ETDTowerType::Harpoon, 0, 4);
	TD::SetRun(Rest);
	TestEqual(TEXT("Brittle Coral: walls cost 1"), TD::TowerCost(ETDTowerType::Wall), TD::BrittleWallCost);
	TestFalse(TEXT("Brittle Coral: no building onto walls"), Rest.CanBuildOntoWalls());
	TestEqual(TEXT("Hungry Deep: one rarity higher"), Rest.DropRarityBonus(), 1);
	TestEqual(TEXT("Hungry Deep: 15 fewer storage slots"), Rest.StorageCut(), 15);
	TestEqual(TEXT("Siren's Debt: 3 free skill points"), Rest.FreeSkillPoints(), 3);
	TestTrue(TEXT("Siren's Debt: skill ranks cost 25% more"), FMath::Abs(TD::SkillCost(ETDTowerType::Harpoon, 0, 4) - FMath::RoundToInt(SkillCost * 1.25f)) <= 1);
	TestFalse(TEXT("Eclipse hides the wave preview"), Rest.ShowsWavePreview());
	TestEqual(TEXT("Eclipse: +20% range"), Rest.RangeScale(), 1.2f);
	TestEqual(TEXT("Frenzied Currents: +25% attack speed"), Rest.RateScale(), 1.25f);
	TestEqual(TEXT("Frenzied Currents: creatures 10% faster"), Rest.EnemySpeedScale(), 1.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDLaterMutatorsTest, "ATD.Logic.Run.MoreMutators", ATDTestFlags)
bool FATDLaterMutatorsTest::RunTest(const FString& Parameters)
{
	const FTDRunRules Plain;
	TestTrue(TEXT("Plain run shows the preview"), Plain.ShowsWavePreview());
	TestTrue(TEXT("Plain run sells any time"), Plain.CanSellLater());
	TestFalse(TEXT("Plain run has every item slot"), Plain.IsItemSlotLocked(ETDItemSlot::Charm));
	TestEqual(TEXT("Plain run: no Rising Tide"), Plain.RisingTideScale(60), 1.0f);

	FTDRunRules Murky;
	Murky.Mutators = Bit(ETDMutator::MurkyWater);
	TestFalse(TEXT("Murky Water hides the preview"), Murky.ShowsWavePreview());
	TestFalse(TEXT("Eclipse isn't offered under Murky Water"), TD::BoonFits(ETDBoon::Eclipse, Murky));
	TestTrue(TEXT("Eclipse is offered otherwise"), TD::BoonFits(ETDBoon::Eclipse, Plain));

	// Rising Tide: 1 + 0.01 x wave, at most twice as fast
	FTDRunRules Tide;
	Tide.Mutators = Bit(ETDMutator::RisingTide);
	TestEqual(TEXT("Rising Tide wave 0"), Tide.RisingTideScale(0), 1.0f);
	TestEqual(TEXT("Rising Tide wave 1"), Tide.RisingTideScale(1), 1.01f, 0.0001f);
	TestEqual(TEXT("Rising Tide wave 37"), Tide.RisingTideScale(37), 1.37f, 0.0001f);
	TestEqual(TEXT("Rising Tide wave 100: twice as fast"), Tide.RisingTideScale(100), 2.0f, 0.0001f);
	TestEqual(TEXT("Rising Tide is capped (endless)"), Tide.RisingTideScale(250), 2.0f, 0.0001f);

	FTDRunRules Fixed;
	Fixed.Mutators = Bit(ETDMutator::FixedTowers);
	TestFalse(TEXT("Fixed Towers: no selling later"), Fixed.CanSellLater());
	TestFalse(TEXT("Gilded Shackles isn't offered under Fixed Towers"), TD::BoonFits(ETDBoon::GildedShackles, Fixed));

	FTDRunRules Lean;
	Lean.Mutators = Bit(ETDMutator::LeanReef);
	TestTrue(TEXT("Lean Reef locks the Charm slot"), Lean.IsItemSlotLocked(ETDItemSlot::Charm));
	TestFalse(TEXT("Lean Reef keeps the Shell slot"), Lean.IsItemSlotLocked(ETDItemSlot::Shell));
	TestFalse(TEXT("Lean Reef keeps the Pearl slot"), Lean.IsItemSlotLocked(ETDItemSlot::Pearl));

	// Abyss depth: Normal stays exactly as it was, deeper tiers darken and lower the music
	TestEqual(TEXT("Normal seabed tint is white"), TD::AbyssTint(0), FLinearColor::White);
	TestEqual(TEXT("Normal music pitch"), TD::AbyssMusicPitch(0), 1.0f);
	TestEqual(TEXT("Abyss 5 music 15% lower"), TD::AbyssMusicPitch(5), 0.85f, 0.0001f);
	bool bDeeper = true;
	for (int32 Tier = 1; Tier <= TD::MaxAbyssTier; ++Tier)
	{
		const FLinearColor A = TD::AbyssTint(Tier - 1);
		const FLinearColor B = TD::AbyssTint(Tier);
		bDeeper &= B.R < A.R && B.G < A.G && B.B <= A.B && B.B > B.R;
	}
	TestTrue(TEXT("Each tier is a little deeper and bluer"), bDeeper);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDMasteryTest, "ATD.Logic.Progression.Mastery", ATDTestFlags)
bool FATDMasteryTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Bronze at 500"), TD::MasteryThreshold(1), 500);
	TestEqual(TEXT("Silver at 2,500"), TD::MasteryThreshold(2), 2500);
	TestEqual(TEXT("Gold at 10,000"), TD::MasteryThreshold(3), 10000);
	TestEqual(TEXT("Abyssal at 40,000"), TD::MasteryThreshold(4), 40000);
	const int32 Cases[][2] = { { 0, 0 }, { 499, 0 }, { 500, 1 }, { 2499, 1 }, { 2500, 2 }, { 9999, 2 }, { 10000, 3 }, { 39999, 3 }, { 40000, 4 }, { 5000000, 4 } };
	for (const int32* Case : Cases)
	{
		TestEqual(FString::Printf(TEXT("%d kills"), Case[0]), TD::MasteryRank(Case[0]), Case[1]);
	}
	TestEqual(TEXT("Rank names"), FString(TD::MasteryName(4)), FString(TEXT("Abyssal")));

	// kills only count from runs that count for records
	TArray<int32> Kills;
	TestEqual(TEXT("A cheated run adds nothing"), TD::AddMasteryKill(Kills, ETDTowerType::Harpoon, false), 0);
	TestEqual(TEXT("Nothing stored for a cheated run"), Kills.Num() > 0 ? Kills[(int32)ETDTowerType::Harpoon] : 0, 0);
	TestEqual(TEXT("A counted kill below a rank"), TD::AddMasteryKill(Kills, ETDTowerType::Harpoon, true), 0);
	TestEqual(TEXT("One kill stored"), Kills[(int32)ETDTowerType::Harpoon], 1);
	Kills[(int32)ETDTowerType::Harpoon] = 499;
	TestEqual(TEXT("The 500th kill reaches Bronze"), TD::AddMasteryKill(Kills, ETDTowerType::Harpoon, true), 1);
	TestEqual(TEXT("The 501st kill reaches nothing new"), TD::AddMasteryKill(Kills, ETDTowerType::Harpoon, true), 0);
	TestEqual(TEXT("Other towers untouched"), Kills[(int32)ETDTowerType::Eel], 0);
	Kills[(int32)ETDTowerType::Eel] = 39999;
	TestEqual(TEXT("A cheated kill doesn't reach Abyssal"), TD::AddMasteryKill(Kills, ETDTowerType::Eel, false), 0);
	TestEqual(TEXT("The counted one does"), TD::AddMasteryKill(Kills, ETDTowerType::Eel, true), 4);

	// trims stay clear of the form badge colours
	for (int32 Rank = 1; Rank <= TD::MasteryRankCount; ++Rank)
	{
		for (int32 Form = 1; Form <= TD::FormsPerTower; ++Form)
		{
			const FLinearColor A = TD::MasteryColor(Rank);
			const FLinearColor B = TD::FormColor(Form);
			const float Distance = FMath::Abs(A.R - B.R) + FMath::Abs(A.G - B.G) + FMath::Abs(A.B - B.B);
			TestTrue(FString::Printf(TEXT("Rank %d trim differs from form %d"), Rank, Form), Distance > 0.5f);
		}
	}

	// the settings slot keeps the lifetime kills
	UTDSettingsSave* Settings = NewObject<UTDSettingsSave>();
	Settings->TowerKills = { 0, 1234, 0, 0, 40000 };
	TArray<uint8> Bytes;
	TestTrue(TEXT("Settings serialize"), UGameplayStatics::SaveGameToMemory(Settings, Bytes));
	const UTDSettingsSave* Loaded = Cast<UTDSettingsSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (TestNotNull(TEXT("Settings load"), Loaded))
	{
		TestEqual(TEXT("Lifetime kills survive"), Loaded->TowerKills, TArray<int32>({ 0, 1234, 0, 0, 40000 }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDBossMusicTest, "ATD.Logic.Audio.BossLoop", ATDTestFlags)
bool FATDBossMusicTest::RunTest(const FString& Parameters)
{
	// the boss loop crossfades with the ambient loop in step, so both must exist and be just as long
	const USoundWave* Ambient = LoadObject<USoundWave>(nullptr, TEXT("/Game/AquaticTowerDefense/Audio/S_Music.S_Music"));
	const USoundWave* Boss = LoadObject<USoundWave>(nullptr, TEXT("/Game/AquaticTowerDefense/Audio/S_MusicBoss.S_MusicBoss"));
	if (!TestNotNull(TEXT("Ambient loop"), Ambient) || !TestNotNull(TEXT("Boss loop"), Boss))
	{
		return false;
	}
	TestEqual(TEXT("Same length, so the loops stay in step"), Boss->Duration, Ambient->Duration, 0.01f);
	TestEqual(TEXT("48 second loop"), Boss->Duration, 48.0f, 0.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDTargetingTest, "ATD.Logic.Combat.Targeting", ATDTestFlags)
bool FATDTargetingTest::RunTest(const FString& Parameters)
{
	// five creatures in range: (tiles to the reef, health + shield, distance to the tower, swimming)
	TArray<FTDTargetInfo> In;
	auto Add = [&In](float Goal, float Tough, float Near, bool bSwim)
	{
		FTDTargetInfo& Info = In.AddDefaulted_GetRef();
		Info.GoalDistance = Goal;
		Info.Toughness = Tough;
		Info.TowerDistance = Near;
		Info.bSwimming = bSwim;
	};
	Add(12.0f, 300.0f, 2.0f, false);	// 0
	Add(4.0f, 80.0f, 3.5f, false);		// 1: closest to the reef
	Add(20.0f, 900.0f, 1.0f, false);	// 2: furthest, toughest, closest to the tower
	Add(9.0f, 20.0f, 2.5f, true);		// 3: weakest, swimming
	Add(15.0f, 150.0f, 3.0f, true);		// 4: swimming
	TestEqual(TEXT("First: closest to the reef"), TD::PickTarget(ETDTargeting::First, In), 1);
	TestEqual(TEXT("Last: furthest from the reef"), TD::PickTarget(ETDTargeting::Last, In), 2);
	TestEqual(TEXT("Strongest"), TD::PickTarget(ETDTargeting::Strongest, In), 2);
	TestEqual(TEXT("Weakest"), TD::PickTarget(ETDTargeting::Weakest, In), 3);
	TestEqual(TEXT("Swimming first: the swimmer closest to the reef"), TD::PickTarget(ETDTargeting::SwimmingFirst, In), 3);
	TestEqual(TEXT("Closest to the tower"), TD::PickTarget(ETDTargeting::Closest, In), 2);

	// no swimmers: Swimming first falls back to First
	TArray<FTDTargetInfo> Crawlers = { In[0], In[1], In[2] };
	TestEqual(TEXT("Swimming first without swimmers is First"), TD::PickTarget(ETDTargeting::SwimmingFirst, Crawlers), 1);
	TestEqual(TEXT("No creatures, no target"), TD::PickTarget(ETDTargeting::First, TArray<FTDTargetInfo>()), -1);

	// cycling wraps both ways, and every priority is named
	TestEqual(TEXT("Next after First"), TD::CycleTargeting(ETDTargeting::First, 1), ETDTargeting::Last);
	TestEqual(TEXT("Previous before First"), TD::CycleTargeting(ETDTargeting::First, -1), ETDTargeting::Closest);
	TestEqual(TEXT("Next after Closest"), TD::CycleTargeting(ETDTargeting::Closest, 1), ETDTargeting::First);
	for (int32 m = 0; m < TD::TargetingCount; ++m)
	{
		TestTrue(FString::Printf(TEXT("Priority %d named"), m), FCString::Strlen(TD::TargetingName((ETDTargeting)m)) > 0 && FCString::Strlen(TD::TargetingDescription((ETDTargeting)m)) > 0);
	}
	TestEqual(TEXT("First is 0, so older saves load as First"), (int32)ETDTargeting::First, 0);
	TestEqual(TEXT("The targeting key comes after the Items key (saved keys keep their places)"), (int32)ETDBind::Targeting, (int32)ETDBind::Items + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDSaveSlotsTest, "ATD.Logic.Save.Slots", ATDTestFlags)
bool FATDSaveSlotsTest::RunTest(const FString& Parameters)
{
	// slot 1 keeps the old single save's name, so an existing save is slot 1 with nothing to move
	TestEqual(TEXT("Slot 1 is the old save"), ATDGameMode::SlotName(0), FString(TEXT("ATDSave")));
	TestEqual(TEXT("Slot 2"), ATDGameMode::SlotName(1), FString(TEXT("ATDSave2")));
	TestEqual(TEXT("Slot 3"), ATDGameMode::SlotName(2), FString(TEXT("ATDSave3")));
	TestEqual(TEXT("Three slots"), ATDGameMode::SaveSlotCount, 3);

	// a run's save through a test slot and back, and what the slot picker reads from it
	UTDSaveGame* Save = NewObject<UTDSaveGame>();
	Save->MapIndex = 2;
	Save->Wave = 37;
	Save->WavesCleared = 36;
	Save->Lives = 11;
	Save->AbyssTier = 2;
	Save->Mutators = 0x5;
	Save->RunId = 4242;
	Save->PlayTime = 1234.0f;
	Save->SavedAt = FDateTime(2026, 11, 8, 3, 7, 0);
	FTDTowerSave& Tower = Save->Towers.AddDefaulted_GetRef();
	Tower.Type = (uint8)ETDTowerType::Harpoon;
	Tower.Targeting = (uint8)ETDTargeting::Strongest;
	const FString TestSlot = TEXT("ATDSlotTest");
	TestTrue(TEXT("Saved to a slot"), UGameplayStatics::SaveGameToSlot(Save, TestSlot, 0));
	const UTDSaveGame* Loaded = Cast<UTDSaveGame>(UGameplayStatics::LoadGameFromSlot(TestSlot, 0));
	UGameplayStatics::DeleteGameInSlot(TestSlot, 0);
	if (!TestNotNull(TEXT("Loaded from the slot"), Loaded))
	{
		return false;
	}
	TestEqual(TEXT("Run id survives"), Loaded->RunId, 4242);
	TestEqual(TEXT("Tower priority survives"), Loaded->Towers[0].Targeting, (uint8)ETDTargeting::Strongest);
	TestEqual(TEXT("Older saves: towers target First, no run id"), (int32)GetDefault<UTDSaveGame>()->RunId, 0);
	TestEqual(TEXT("Older tower saves target First"), FTDTowerSave().Targeting, (uint8)0);

	FTDSlotInfo Info;
	ATDGameMode::FillSlotInfo(Loaded, Info);
	TestTrue(TEXT("Slot used"), Info.bUsed);
	TestEqual(TEXT("Slot map"), Info.Map, 2);
	TestEqual(TEXT("Slot wave"), Info.Wave, 37);
	TestEqual(TEXT("Slot tier"), Info.Tier, 2);
	TestEqual(TEXT("Slot mutator count"), Info.MutatorCount, 2);
	TestEqual(TEXT("Slot run id"), Info.RunId, 4242);
	TestEqual(TEXT("Slot score"), Info.Score, TD::RunScore(37, 11, false, TD::ScoreMultiplier(2, 0x5)));
	FTDSlotInfo Empty;
	ATDGameMode::FillSlotInfo(nullptr, Empty);
	TestFalse(TEXT("No save, empty slot"), Empty.bUsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDRetryTest, "ATD.Logic.Run.Retry", ATDTestFlags)
bool FATDRetryTest::RunTest(const FString& Parameters)
{
	FTDRunRules Played;
	Played.Tier = 3;
	Played.Mutators = Bit(ETDMutator::Swarm) | Bit(ETDMutator::NoWalls);
	Played.Boons = Bit(ETDBoon::TidalFury) | Bit(ETDBoon::BloodTide);
	const FTDRunRules Again = TD::RetryRules(Played);
	TestEqual(TEXT("Retry keeps the Abyss tier"), Again.Tier, 3);
	TestEqual(TEXT("Retry keeps the mutators"), Again.Mutators, Played.Mutators);
	TestEqual(TEXT("Retry starts without boons"), Again.Boons, 0u);
	TestEqual(TEXT("Retry keeps the score multiplier before boons"), TD::ScoreMultiplier(Again.Tier, Again.Mutators, Again.Boons), TD::ScoreMultiplier(3, Played.Mutators));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDMotionTest, "ATD.Logic.UI.Motion", ATDTestFlags)
bool FATDMotionTest::RunTest(const FString& Parameters)
{
	// death: crawlers roll onto their backs and sink, swimmers spiral down to the floor
	const TD::FTDDeathPose Start = TD::DeathPose(0.0f, false);
	TestEqual(TEXT("A death starts where the creature was"), Start.Sink + Start.Roll + Start.Drop + Start.Spin, 0.0f);
	const TD::FTDDeathPose Crawler = TD::DeathPose(1.0f, false);
	TestTrue(TEXT("A crawler ends on its back"), Crawler.Roll > 150.0f);
	TestTrue(TEXT("A crawler ends under the sand"), Crawler.Sink >= 40.0f);
	const TD::FTDDeathPose Swimmer = TD::DeathPose(1.0f, true);
	TestEqual(TEXT("A swimmer ends on the floor"), Swimmer.Drop, 1.0f);
	TestTrue(TEXT("A swimmer spirals"), Swimmer.Spin >= 360.0f);
	TestTrue(TEXT("Past the end it holds the last pose"), TD::DeathPose(3.0f, false).Sink == Crawler.Sink);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDToastFilterTest, "ATD.Logic.UI.Toasts", ATDTestFlags)
bool FATDToastFilterTest::RunTest(const FString& Parameters)
{
	// important only: routine news stays quiet, big events and refusals (why a click did nothing) show
	TestFalse(TEXT("Routine news is quiet"), TD::ShouldShowToast(ETDToast::Info));
	TestTrue(TEXT("Big events show"), TD::ShouldShowToast(ETDToast::Important));
	TestTrue(TEXT("Refusals show"), TD::ShouldShowToast(ETDToast::Refusal));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDBoonArtTest, "ATD.Logic.UI.BoonIcons", ATDTestFlags)
bool FATDBoonArtTest::RunTest(const FString& Parameters)
{
	// every boon and curse paints its own icon in the Boon family (no more borrowed skill or tower icons)
	TestEqual(TEXT("One boon icon per boon"), TDIcons::Count(ETDIcon::Boon), TD::BoonCount);
	TArray<uint32> Hashes;
	for (int32 b = 0; b < TD::BoonCount; ++b)
	{
		TArray<FColor> Pixels;
		const bool bOk = TDIcons::Render(ETDIcon::Boon, b, 48, Pixels);
		TestTrue(FString::Printf(TEXT("Boon %d has an icon"), b), bOk && Pixels.Num() == 48 * 48);
		int32 Opaque = 0;
		for (const FColor& Px : Pixels)
		{
			Opaque += Px.A > 128 ? 1 : 0;
		}
		TestTrue(FString::Printf(TEXT("Boon %d icon isn't empty"), b), Opaque > 48 * 48 / 10);
		// the visible art is centred in its tile, so a row of boon icons lines up
		FIntPoint Min(48, 48), Max(-1, -1);
		for (int32 i = 0; i < Pixels.Num(); ++i)
		{
			if (Pixels[i].A > 128)
			{
				Min = FIntPoint(FMath::Min(Min.X, i % 48), FMath::Min(Min.Y, i / 48));
				Max = FIntPoint(FMath::Max(Max.X, i % 48), FMath::Max(Max.Y, i / 48));
			}
		}
		const float CentreX = (Min.X + Max.X + 1) * 0.5f / 48.0f, CentreY = (Min.Y + Max.Y + 1) * 0.5f / 48.0f;
		TestTrue(FString::Printf(TEXT("Boon %d icon is centred"), b), FMath::Abs(CentreX - 0.5f) < 0.04f && FMath::Abs(CentreY - 0.5f) < 0.04f);
		Hashes.Add(FCrc::MemCrc32(Pixels.GetData(), Pixels.Num() * sizeof(FColor)));
	}
	TestEqual(TEXT("Every boon icon is different"), TSet<uint32>(Hashes).Num(), TD::BoonCount);

	// the Abyss unlock sound exists; boons, curses and records have no sound of their own
	const USoundWave* Unlock = LoadObject<USoundWave>(nullptr, TEXT("/Game/AquaticTowerDefense/Audio/S_TierUnlock.S_TierUnlock"));
	if (TestNotNull(TEXT("Tier unlock sound"), Unlock))
	{
		TestTrue(TEXT("Tier unlock sound is short"), Unlock->Duration > 0.3f && Unlock->Duration < 1.5f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FATDUniqueTest, "ATD.Logic.Items.Uniques", ATDTestFlags)
bool FATDUniqueTest::RunTest(const FString& Parameters)
{
	// nine uniques, three per slot, each its own Legendary item type with a description and an icon
	TestEqual(TEXT("Nine uniques"), TD::UniqueCount, 9);
	int32 PerSlot[(int32)ETDItemSlot::Count] = {};
	for (int32 u = 1; u <= TD::UniqueCount; ++u)
	{
		const ETDUnique Unique = (ETDUnique)u;
		const int32 Kind = TD::UniqueKind(Unique);
		const FTDItemKindDef* Def = TD::ItemKind(Kind);
		if (!TestNotNull(FString::Printf(TEXT("Unique %d has an item type"), u), Def))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s is Legendary"), Def->Name), Def->Rarity, ETDRarity::Legendary);
		TestTrue(FString::Printf(TEXT("%s is described"), Def->Name), FCString::Strlen(TD::UniqueDescription(Unique)) > 0);
		TestTrue(FString::Printf(TEXT("%s is unique"), Def->Name), TD::IsUnique(TD::MakeItemOfKind(1, Kind, 10)));
		TArray<FColor> Pixels;
		TestTrue(FString::Printf(TEXT("%s has an icon"), Def->Name), TDIcons::Render(ETDIcon::ItemKind, Kind, 48, Pixels));
		++PerSlot[(int32)Def->Slot];
	}
	TestTrue(TEXT("Three per slot"), PerSlot[0] == 3 && PerSlot[1] == 3 && PerSlot[2] == 3);

	// a run never finds the same unique twice; once all are found the pick falls back (an ordinary Legendary)
	FRandomStream Random(2024);
	bool bNoRepeats = true;
	for (int32 Run = 0; Run < 200; ++Run)
	{
		uint32 Found = 0;
		for (int32 i = 0; i < TD::UniqueCount; ++i)
		{
			const ETDUnique Pick = TD::PickUnique(Found, Random);
			bNoRepeats &= Pick != ETDUnique::None && !(Found & TD::UniqueBit(Pick));
			Found |= TD::UniqueBit(Pick);
		}
		bNoRepeats &= TD::PickUnique(Found, Random) == ETDUnique::None && TD::CountUniques(Found) == TD::UniqueCount;
	}
	TestTrue(TEXT("No unique twice in a run, and none left after nine"), bNoRepeats);

	// uniques never come from a roll by rarity or from a reroll; a unique can't be rerolled
	FMath::RandInit(99);
	bool bNeverRolled = true;
	for (int32 i = 0; i < 2000; ++i)
	{
		bNeverRolled &= !TD::IsUnique(TD::GenerateItem(i + 1, 50, ETDRarity::Legendary));
	}
	TestTrue(TEXT("Legendary rolls never give a unique by themselves"), bNeverRolled);
	bool bRerollNever = true;
	for (int32 i = 0; i < 500; ++i)
	{
		FTDItem Item = TD::GenerateItem(1, 30, ETDRarity::Legendary);
		TD::RerollItem(Item);
		bRerollNever &= !TD::IsUnique(Item);
	}
	TestTrue(TEXT("Rerolls never give a unique"), bRerollNever);
	FTDItem Trident = TD::MakeItemOfKind(7, TD::UniqueKind(ETDUnique::Trident), 20);
	TD::RerollItem(Trident);
	TestEqual(TEXT("A unique can't be rerolled"), TD::ItemUnique(Trident), ETDUnique::Trident);
	TestTrue(TEXT("A unique can be upgraded"), TD::UpgradeItem(Trident) && Trident.Level == 20 + TD::ItemUpgradeLevels && TD::IsUnique(Trident));
	for (int32 Level = 0; Level <= 2; ++Level)
	{
		TestFalse(FString::Printf(TEXT("Auto-salvage level %d never takes a Legendary (so never a unique)"), Level), (int32)ETDRarity::Legendary < Level);
	}

	// old saves: the found record comes from the uniques on hand
	TArray<FTDItem> OnHand = { TD::GenerateItem(1, 10, ETDRarity::Epic), TD::MakeItemOfKind(2, TD::UniqueKind(ETDUnique::GildedConch), 10), FTDItem(), TD::MakeItemOfKind(3, TD::UniqueKind(ETDUnique::TearOfMoon), 10) };
	TestEqual(TEXT("Uniques inferred from items"), TD::InferUniquesFound(OnHand), TD::UniqueBit(ETDUnique::GildedConch) | TD::UniqueBit(ETDUnique::TearOfMoon));

	// save round-trip
	UTDSaveGame* Save = NewObject<UTDSaveGame>();
	Save->UniquesFound = (int32)(TD::UniqueBit(ETDUnique::Trident) | TD::UniqueBit(ETDUnique::KrakensHeart));
	FTDTowerSave& Tower = Save->Towers.AddDefaulted_GetRef();
	Tower.HeartWaves = 17;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Serialize"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	const UTDSaveGame* Loaded = Cast<UTDSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (TestNotNull(TEXT("Deserialize"), Loaded))
	{
		TestEqual(TEXT("Uniques found survive"), Loaded->UniquesFound, Save->UniquesFound);
		TestEqual(TEXT("Heart of Coral waves survive"), Loaded->Towers[0].HeartWaves, 17);
	}
	TestEqual(TEXT("Older saves: no record (inferred on load)"), GetDefault<UTDSaveGame>()->UniquesFound, 0);

	// mechanics
	TestFalse(TEXT("Trident: not the 1st attack"), TD::TridentStrikes(1));
	TestFalse(TEXT("Trident: not the 2nd"), TD::TridentStrikes(2));
	TestTrue(TEXT("Trident: the 3rd"), TD::TridentStrikes(3));
	TestTrue(TEXT("Trident: the 6th"), TD::TridentStrikes(6));
	TestEqual(TEXT("Heart of Coral: +1% per wave"), TD::HeartOfCoralBonus(10), 0.1f, 0.0001f);
	TestEqual(TEXT("Heart of Coral: at most +60%"), TD::HeartOfCoralBonus(200), 0.6f, 0.0001f);
	TestTrue(TEXT("Anchor ready after its cooldown"), TD::AnchorReady(10.0, 10.0) && !TD::AnchorReady(9.9, 10.0));
	TestEqual(TEXT("Leviathan's Jaw: 15% of max health"), TD::JawBurstShare, 0.15f);
	TestEqual(TEXT("Tear of the Moon: half the damage"), TD::TearArcShare, 0.5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
