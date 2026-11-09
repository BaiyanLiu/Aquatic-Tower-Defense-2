// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDRun.h"
#include "TDRules.h"

namespace
{
	FTDRunRules GRunRules;

	struct FTDMutatorDef
	{
		const TCHAR* Name;
		const TCHAR* Description;
		float Score;
	};

	const FTDMutatorDef GMutators[] =
	{
		{ TEXT("Glass Reef"),	TEXT("Start with 1 life."),											1.00f },
		{ TEXT("Swarm"),			TEXT("Twice as many creatures, each with half the health and pearls."),	0.25f },
		{ TEXT("Armored Tide"),	TEXT("Creatures have 50% more armor."),								0.30f },
		{ TEXT("Swimmer Frenzy"),	TEXT("Swimmer groups are twice as big."),								0.30f },
		{ TEXT("No Walls"),		TEXT("Coral Walls can't be built: make the maze from towers."),		0.50f },
		{ TEXT("Poor Waters"),	TEXT("No pearls for clearing a wave."),								0.30f },
		{ TEXT("Murky Water"),	TEXT("The next wave is hidden."),										0.25f },
		{ TEXT("Rising Tide"),	TEXT("Creatures get 1% faster every wave (at most twice as fast)."),	0.35f },
		{ TEXT("Fixed Towers"),	TEXT("Towers and walls can only be sold in the build phase they were bought in."),	0.20f },
		{ TEXT("Lean Reef"),		TEXT("Towers hold only 2 items: the Charm slot is locked."),			0.30f },
	};
	static_assert(UE_ARRAY_COUNT(GMutators) == (int32)ETDMutator::Count, "Mutator table out of sync");

	struct FTDBoonDef
	{
		const TCHAR* Name;
		const TCHAR* Description;
		const TCHAR* Drawback = TEXT("");	// cursed boons only
		float Score = 0.0f;					// cursed boons only
	};

	const FTDBoonDef GBoons[] =
	{
		{ TEXT("Sharpened Reef"),	TEXT("All towers deal 15% more damage.") },
		{ TEXT("Tidal Fury"),		TEXT("All towers attack 12% faster.") },
		{ TEXT("Long Sight"),		TEXT("All towers get 8% more range.") },
		{ TEXT("Scholar's Tide"),	TEXT("Towers gain 30% more XP.") },
		{ TEXT("Bounty Hunters"),	TEXT("Kills give 25% more pearls.") },
		{ TEXT("Deep Pockets"),		TEXT("Each wave clear pays 3% interest on unspent pearls (up to 30).") },
		{ TEXT("Coral Masons"),		TEXT("Coral Walls cost 2 pearls.") },
		{ TEXT("Cheap Upgrades"),	TEXT("Skill ranks and item upgrades cost 15% less.") },
		{ TEXT("Treasure Currents"),	TEXT("Items drop 40% more often.") },
		{ TEXT("Lucky Find"),		TEXT("Drops roll one rarity higher, but come 30% less often.") },
		{ TEXT("Thick Reef"),		TEXT("+5 lives now, and +1 life every 10 waves cleared.") },
		{ TEXT("Giant Slayer"),		TEXT("Towers deal 40% more damage to bosses and elites.") },
		{ TEXT("Venom Bloom"),		TEXT("Poison and bleed deal 30% more damage.") },
		{ TEXT("Storm Coil"),		TEXT("Electric Eels chain to 1 more target.") },
		// cursed
		{ TEXT("Blood Tide"),		TEXT("All towers deal 30% more damage."),	TEXT("Every leak costs 1 more life."),			0.15f },
		{ TEXT("Kraken's Pact"),		TEXT("Pick another boon right away."),		TEXT("Lose 5 lives now."),						0.10f },
		{ TEXT("Gilded Shackles"),	TEXT("Kills give 60% more pearls."),		TEXT("Sell refunds are halved."),				0.15f },
		{ TEXT("Frenzied Currents"),	TEXT("All towers attack 25% faster."),		TEXT("Creatures move 10% faster."),				0.20f },
		{ TEXT("Brittle Coral"),		TEXT("Coral Walls cost 1 pearl."),			TEXT("Towers can't be built onto walls."),		0.10f },
		{ TEXT("Hungry Deep"),		TEXT("Drops roll one rarity higher."),		TEXT("Storage holds 15 fewer items."),			0.15f },
		{ TEXT("Siren's Debt"),		TEXT("+3 skill points on every tower, now and for every tower built later."),	TEXT("Skill ranks cost 25% more."),	0.10f },
		{ TEXT("Eclipse"),			TEXT("All towers get 20% more range."),	TEXT("The next wave is hidden."),				0.20f },
	};
	static_assert(UE_ARRAY_COUNT(GBoons) == (int32)ETDBoon::Count, "Boon table out of sync");
	static_assert((int32)ETDBoon::Count <= 32 && (int32)ETDMutator::Count <= 32, "Bits must fit a uint32");
}

const FTDRunRules& TD::Run()
{
	return GRunRules;
}

void TD::SetRun(const FTDRunRules& Rules)
{
	GRunRules = Rules;
	GRunRules.Tier = FMath::Clamp(Rules.Tier, 0, MaxAbyssTier);
	GRunRules.Mutators = CleanMutators(Rules.Mutators, GRunRules.Tier);
	GRunRules.Boons = Rules.Boons & ((1u << (uint32)BoonCount) - 1u);
}

const TCHAR* TD::MutatorName(ETDMutator Mutator)
{
	return (int32)Mutator < MutatorCount ? GMutators[(int32)Mutator].Name : TEXT("");
}

const TCHAR* TD::MutatorDescription(ETDMutator Mutator)
{
	return (int32)Mutator < MutatorCount ? GMutators[(int32)Mutator].Description : TEXT("");
}

float TD::MutatorScore(ETDMutator Mutator)
{
	return (int32)Mutator < MutatorCount ? GMutators[(int32)Mutator].Score : 0.0f;
}

bool TD::IsMutatorIncluded(ETDMutator Mutator, int32 Tier)
{
	return Mutator == ETDMutator::PoorWaters && Tier >= 4;
}

uint32 TD::CleanMutators(uint32 Mutators, int32 Tier)
{
	uint32 Result = Mutators & ((1u << (uint32)MutatorCount) - 1u);
	for (int32 m = 0; m < MutatorCount; ++m)
	{
		if (IsMutatorIncluded((ETDMutator)m, Tier))
		{
			Result &= ~(1u << (uint32)m);
		}
	}
	return Result;
}

const TCHAR* TD::AbyssRule(int32 Tier)
{
	switch (Tier)
	{
	case 1:		return TEXT("Fast and shielded creatures 10% more often");
	case 2:		return TEXT("Leaks cost double lives");
	case 3:		return TEXT("A Manta group joins every wave from wave 10");
	case 4:		return TEXT("No pearls for clearing a wave");
	case 5:		return TEXT("Sell refunds halved");
	default:	return TEXT("");
	}
}

float TD::ScoreMultiplier(int32 Tier, uint32 Mutators, uint32 Boons)
{
	const int32 ClampedTier = FMath::Clamp(Tier, 0, MaxAbyssTier);
	const uint32 Clean = CleanMutators(Mutators, ClampedTier);
	float Multiplier = 1.0f + AbyssScorePerTier * ClampedTier;
	for (int32 m = 0; m < MutatorCount; ++m)
	{
		if (Clean & (1u << (uint32)m))
		{
			Multiplier += MutatorScore((ETDMutator)m);
		}
	}
	for (int32 b = 0; b < BoonCount; ++b)
	{
		if (Boons & (1u << (uint32)b))
		{
			Multiplier += BoonScore((ETDBoon)b);
		}
	}
	return Multiplier;
}

int32 TD::RunScore(int32 WavesCleared, int32 LivesLeft, bool bWon, float Multiplier)
{
	const int32 Base = FMath::Max(0, WavesCleared) * 100 + (bWon ? FMath::Max(0, LivesLeft) * 25 : 0);
	return FMath::RoundToInt(Base * Multiplier);
}

FString TD::MultiplierText(float Multiplier)
{
	FString Text = FString::Printf(TEXT("%.2f"), Multiplier);
	// "x2.00" reads as "x2", "x1.50" as "x1.5"
	while (Text.EndsWith(TEXT("0")))
	{
		Text.LeftChopInline(1);
	}
	if (Text.EndsWith(TEXT(".")))
	{
		Text.LeftChopInline(1);
	}
	return TEXT("x") + Text;
}

const TCHAR* TD::BoonName(ETDBoon Boon)
{
	return (int32)Boon < BoonCount ? GBoons[(int32)Boon].Name : TEXT("");
}

const TCHAR* TD::BoonDescription(ETDBoon Boon)
{
	return (int32)Boon < BoonCount ? GBoons[(int32)Boon].Description : TEXT("");
}

bool TD::IsCursed(ETDBoon Boon)
{
	return Boon >= ETDBoon::BloodTide && Boon < ETDBoon::Count;
}

const TCHAR* TD::BoonDrawback(ETDBoon Boon)
{
	return (int32)Boon < BoonCount ? GBoons[(int32)Boon].Drawback : TEXT("");
}

float TD::BoonScore(ETDBoon Boon)
{
	return (int32)Boon < BoonCount ? GBoons[(int32)Boon].Score : 0.0f;
}

bool TD::BoonFits(ETDBoon Boon, const FTDRunRules& Rules, int32 Lives)
{
	if ((int32)Boon >= BoonCount || Rules.Has(Boon))
	{
		return false;
	}
	switch (Boon)
	{
	// (Brittle Coral's walls are cheaper still, so Coral Masons adds nothing after it)
	case ETDBoon::CoralMasons:		return Rules.CanBuildWalls() && !Rules.Has(ETDBoon::BrittleCoral);
	case ETDBoon::ThickReef:		return !Rules.Has(ETDMutator::GlassReef);
	case ETDBoon::BrittleCoral:		return Rules.CanBuildWalls();
	// a drawback that means nothing when one leak already ends the game
	case ETDBoon::BloodTide:		return !Rules.Has(ETDMutator::GlassReef);
	case ETDBoon::KrakensPact:		return !Rules.Has(ETDMutator::GlassReef) && Lives >= PactMinLives;
	// the preview is already hidden; with Fixed Towers every refund left is the full build-phase one
	case ETDBoon::Eclipse:			return !Rules.Has(ETDMutator::MurkyWater);
	case ETDBoon::GildedShackles:	return Rules.CanSellLater();
	default:						return true;
	}
}

void TD::RollBoonOffer(const FTDRunRules& Rules, FRandomStream& Random, TArray<int32>& OutOffer, int32 Count, int32 Lives, bool bAllowCursed)
{
	OutOffer.Reset();
	TArray<int32> Pool;
	TArray<int32> Cursed;
	for (int32 b = 0; b < BoonCount; ++b)
	{
		if (BoonFits((ETDBoon)b, Rules, Lives))
		{
			(IsCursed((ETDBoon)b) ? Cursed : Pool).Add(b);
		}
	}
	while (OutOffer.Num() < Count && Pool.Num() > 0)
	{
		const int32 Pick = Random.RandRange(0, Pool.Num() - 1);
		OutOffer.Add(Pool[Pick]);
		Pool.RemoveAtSwap(Pick);
	}
	// sometimes one card (at a random place) is cursed; with no normal boon left the offer stays empty
	const float Roll = Random.GetFraction();
	if (bAllowCursed && OutOffer.Num() > 0 && Cursed.Num() > 0 && Roll < CursedOfferChance)
	{
		const int32 Curse = Cursed[Random.RandRange(0, Cursed.Num() - 1)];
		// a short offer gains the card (at any place, the end included); a full one has a card replaced
		if (OutOffer.Num() < Count)
		{
			OutOffer.Insert(Curse, Random.RandRange(0, OutOffer.Num()));
		}
		else
		{
			OutOffer[Random.RandRange(0, OutOffer.Num() - 1)] = Curse;
		}
	}
}

int32 TD::RunSellValue(int32 TowerCost, int32 Level, float SellBonus, int32 SkillSpent, int32 TotalSpent, bool bFullRefund)
{
	const int32 Value = SellValue(TowerCost, Level, SellBonus, SkillSpent, TotalSpent, bFullRefund);
	return bFullRefund ? Value : FMath::FloorToInt(Value * Run().SellRefundScale());
}

int32 TD::TowerCost(ETDTowerType Type)
{
	if (Type == ETDTowerType::Wall && Run().Has(ETDBoon::BrittleCoral))
	{
		return BrittleWallCost;
	}
	if (Type == ETDTowerType::Wall && Run().Has(ETDBoon::CoralMasons))
	{
		return MasonWallCost;
	}
	return Tower(Type).Cost;
}
