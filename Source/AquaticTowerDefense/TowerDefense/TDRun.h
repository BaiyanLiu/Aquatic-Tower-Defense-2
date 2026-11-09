// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "TDTypes.h"
#include "TDItems.h"

/**
 *  Replayability rules chosen per run: Abyss tiers (per-map difficulty ladder), mutators (free toggles that raise
 *  the score multiplier) and run boons (picked from offers of three during a run). Pure rules with no actors,
 *  shared by the game code, the HUD and the automation tests. The rules of the run in play live in TD::Run().
 */

/** Optional handicaps chosen on the new game screen (bit index = enum value; saves store the bits) */
enum class ETDMutator : uint8
{
	GlassReef,		// start with 1 life
	Swarm,			// twice the creatures, half the health each
	ArmoredTide,	// creatures +50% armor
	FlyerFrenzy,	// flyer groups doubled
	NoWalls,		// Coral Walls can't be built
	PoorWaters,		// no wave-clear pearls
	MurkyWater,		// the next-wave preview is hidden
	RisingTide,		// creatures 1% faster per wave (up to +100%)
	FixedTowers,	// towers and walls can only be sold in the build phase they were bought in
	LeanReef,		// only 2 item slots per tower (the Charm slot is locked)
	Count
};

/** Run boons (bit index = enum value; append only, saves store the bits) */
enum class ETDBoon : uint8
{
	SharpenedReef,		// all towers +15% damage
	TidalFury,			// all towers +12% attack speed
	LongSight,			// all towers +8% range
	ScholarsTide,		// towers gain +30% XP
	BountyHunters,		// +25% pearls from kills
	DeepPockets,		// interest on unspent pearls at each wave clear
	CoralMasons,		// Coral Walls cost 2 pearls
	CheapUpgrades,		// skill ranks and item upgrades cost 15% less
	TreasureCurrents,	// items drop 40% more often
	LuckyFind,			// drops roll one rarity higher but 30% less often
	ThickReef,			// +5 lives now, +1 life every 10 waves cleared
	GiantSlayer,		// +40% damage to bosses and elites
	VenomBloom,			// poison and bleed deal 30% more
	StormCoil,			// Eels get +1 chain target
	// cursed boons: a strong upside, a lasting drawback, and extra score multiplier
	BloodTide,			// towers +30% damage; leaks cost +1 life each
	KrakensPact,		// another normal boon offer right away; lose 5 lives now
	GildedShackles,		// +60% pearls from kills; sell refunds halved
	FrenziedCurrents,	// towers +25% attack speed; creatures 10% faster
	BrittleCoral,		// Coral Walls cost 1 pearl; towers can't be built onto walls
	HungryDeep,			// drops roll one rarity higher; storage holds 15 fewer items
	SirensDebt,			// +3 skill points on every tower (now and built later); skill ranks cost 25% more
	Eclipse,			// towers +20% range; the next-wave preview is hidden
	Count
};

/** The rules of one run: Abyss tier, mutator bits and taken boon bits */
struct FTDRunRules
{
	int32 Tier = 0;
	uint32 Mutators = 0;
	uint32 Boons = 0;

	bool Has(ETDMutator Mutator) const { return (Mutators & (1u << (uint32)Mutator)) != 0; }
	bool Has(ETDBoon Boon) const { return (Boons & (1u << (uint32)Boon)) != 0; }

	// ---- Abyss tiers (cumulative: tier N has every rule of the tiers below it) ----

	/** A1: extra chance of each fast / shielded roll (the cap rises by the same amount) */
	float ModifierChanceBonus() const { return Tier >= 1 ? 0.10f : 0.0f; }
	/** A2: leaks cost double lives */
	int32 LeakLivesScale() const { return Tier >= 2 ? 2 : 1; }
	/** Lives a leak costs: the creature's lives, doubled at Abyss 2+, plus one with Blood Tide */
	int32 LeakLives(int32 CreatureLives) const { return CreatureLives * LeakLivesScale() + (Has(ETDBoon::BloodTide) ? 1 : 0); }
	/** A3: every wave from TD::AbyssFlyerWave without flyers gets a Manta group */
	bool AddsFlyerGroups() const { return Tier >= 3; }
	/** A4 or Poor Waters: no pearls for clearing a wave */
	bool HasWaveClearPearls() const { return Tier < 4 && !Has(ETDMutator::PoorWaters); }
	/** A5 and Gilded Shackles: sell refunds halved, a quarter with both (the full refund in the build phase a tower was bought stays) */
	float SellRefundScale() const { return (Tier >= 5 ? 0.5f : 1.0f) * (Has(ETDBoon::GildedShackles) ? 0.5f : 1.0f); }

	// ---- mutators ----

	bool CanBuildWalls() const { return !Has(ETDMutator::NoWalls); }
	int32 StartLives() const { return Has(ETDMutator::GlassReef) ? 1 : TD::StartLives; }
	/** Swarm: creature count factor of a regular group (and the health share of each creature) */
	bool IsSwarm() const { return Has(ETDMutator::Swarm); }
	float ArmorScale() const { return Has(ETDMutator::ArmoredTide) ? 1.5f : 1.0f; }
	/** Frenzied Currents: creatures move faster */
	float EnemySpeedScale() const { return Has(ETDBoon::FrenziedCurrents) ? 1.1f : 1.0f; }
	/** Rising Tide: creatures of wave N move 1 + 0.01 N times as fast, at most twice as fast */
	float RisingTideScale(int32 Wave) const { return Has(ETDMutator::RisingTide) ? 1.0f + FMath::Min(1.0f, 0.01f * FMath::Max(0, Wave)) : 1.0f; }
	/** Fixed Towers: no selling after the build phase a tower was bought in */
	bool CanSellLater() const { return !Has(ETDMutator::FixedTowers); }
	/** Lean Reef: the Charm slot is locked */
	bool IsItemSlotLocked(ETDItemSlot Slot) const { return Has(ETDMutator::LeanReef) && Slot == ETDItemSlot::Charm; }
	int32 FlyerGroupScale() const { return Has(ETDMutator::FlyerFrenzy) ? 2 : 1; }

	// ---- boons ----

	float DamageScale() const { return (Has(ETDBoon::SharpenedReef) ? 1.15f : 1.0f) * (Has(ETDBoon::BloodTide) ? 1.3f : 1.0f); }
	float RateScale() const { return (Has(ETDBoon::TidalFury) ? 1.12f : 1.0f) * (Has(ETDBoon::FrenziedCurrents) ? 1.25f : 1.0f); }
	float RangeScale() const { return (Has(ETDBoon::LongSight) ? 1.08f : 1.0f) * (Has(ETDBoon::Eclipse) ? 1.2f : 1.0f); }
	float XpBonus() const { return Has(ETDBoon::ScholarsTide) ? 0.30f : 0.0f; }
	float KillPearlScale() const { return (Has(ETDBoon::BountyHunters) ? 1.25f : 1.0f) * (Has(ETDBoon::GildedShackles) ? 1.6f : 1.0f); }
	/** Item workbench upgrades (Cheap Upgrades) */
	float UpgradeCostScale() const { return Has(ETDBoon::CheapUpgrades) ? 0.85f : 1.0f; }
	/** Skill ranks (Cheap Upgrades, Siren's Debt) */
	float SkillCostScale() const { return UpgradeCostScale() * (Has(ETDBoon::SirensDebt) ? 1.25f : 1.0f); }
	/** Rarity steps added to every drop (Lucky Find, Hungry Deep) */
	int32 DropRarityBonus() const { return (Has(ETDBoon::LuckyFind) ? 1 : 0) + (Has(ETDBoon::HungryDeep) ? 1 : 0); }
	/** Storage slots taken away (Hungry Deep) */
	int32 StorageCut() const { return Has(ETDBoon::HungryDeep) ? 15 : 0; }
	/** Brittle Coral: towers can't replace a Coral Wall */
	bool CanBuildOntoWalls() const { return !Has(ETDBoon::BrittleCoral); }
	/** Siren's Debt: free skill points for every tower */
	int32 FreeSkillPoints() const { return Has(ETDBoon::SirensDebt) ? 3 : 0; }
	/** Eclipse (cursed boon) and Murky Water (mutator): the next-wave preview is hidden */
	bool ShowsWavePreview() const { return !Has(ETDBoon::Eclipse) && !Has(ETDMutator::MurkyWater); }
	float DropChanceScale() const { return (Has(ETDBoon::TreasureCurrents) ? 1.4f : 1.0f) * (Has(ETDBoon::LuckyFind) ? 0.7f : 1.0f); }
	float BossDamageBonus() const { return Has(ETDBoon::GiantSlayer) ? 0.40f : 0.0f; }
	float DotScale() const { return Has(ETDBoon::VenomBloom) ? 1.30f : 1.0f; }
	int32 EelChainBonus() const { return Has(ETDBoon::StormCoil) ? 1 : 0; }
};

namespace TD
{
	constexpr int32 MaxAbyssTier = 5;
	constexpr int32 MutatorCount = (int32)ETDMutator::Count;
	constexpr int32 BoonCount = (int32)ETDBoon::Count;

	/** First wave that gets an extra flyer group at Abyss 3 */
	constexpr int32 AbyssFlyerWave = 10;

	/** Chance that an offer has one cursed card (never more than one) */
	constexpr float CursedOfferChance = 0.35f;

	/** Kraken's Pact: lives lost when taken, and the lives needed to be offered it */
	constexpr int32 PactLives = 5;
	constexpr int32 PactMinLives = 7;

	/** Brittle Coral: the wall's price */
	constexpr int32 BrittleWallCost = 1;

	/** Score multiplier added per Abyss tier */
	constexpr float AbyssScorePerTier = 0.20f;

	/** Boons offered at a time */
	constexpr int32 BoonOfferSize = 3;

	/** Deep Pockets: share of unspent pearls paid at each wave clear, and its cap per wave */
	constexpr float InterestRate = 0.03f;
	constexpr int32 InterestCap = 30;

	/** Coral Masons: the wall's price */
	constexpr int32 MasonWallCost = 2;

	/** Thick Reef: lives when taken, and one more every this many waves cleared */
	constexpr int32 ThickReefLives = 5;
	constexpr int32 ThickReefEvery = 10;

	/** The rules of the run in play (set on a new game, a load and when a boon is picked) */
	const FTDRunRules& Run();
	void SetRun(const FTDRunRules& Rules);

	// ---- mutators ----
	const TCHAR* MutatorName(ETDMutator Mutator);
	const TCHAR* MutatorDescription(ETDMutator Mutator);
	/** Score multiplier the mutator adds (1.0 = +100%) */
	float MutatorScore(ETDMutator Mutator);
	/** Poor Waters is part of Abyss 4+, so it can't be picked (and adds nothing) there */
	bool IsMutatorIncluded(ETDMutator Mutator, int32 Tier);
	/** Drops mutator bits that are unknown or already part of the tier */
	uint32 CleanMutators(uint32 Mutators, int32 Tier);

	// ---- Abyss ----
	/** The rule a tier adds on top of the tiers below it (empty for 0) */
	const TCHAR* AbyssRule(int32 Tier);

	/** Depth look: 0 at Normal, 1 at Abyss 5 */
	inline float AbyssDepth(int32 Tier) { return FMath::Clamp(Tier, 0, MaxAbyssTier) / (float)MaxAbyssTier; }
	/** Seabed tint multiplier for a tier: white at Normal, a deeper blue-violet below */
	inline FLinearColor AbyssTint(int32 Tier)
	{
		const float D = AbyssDepth(Tier);
		return FLinearColor(1.0f - 0.24f * D, 1.0f - 0.2f * D, 1.0f - 0.06f * D, 1.0f);
	}
	/** Music pitch for a tier: 3% lower per tier */
	inline float AbyssMusicPitch(int32 Tier) { return 1.0f - 0.03f * FMath::Clamp(Tier, 0, MaxAbyssTier); }

	// ---- score ----
	/** 1 + each chosen mutator's bonus + AbyssScorePerTier per tier + each taken cursed boon's bonus */
	float ScoreMultiplier(int32 Tier, uint32 Mutators, uint32 Boons = 0);
	/** (waves cleared x 100 + lives left x 25 on a win) x multiplier, rounded */
	int32 RunScore(int32 WavesCleared, int32 LivesLeft, bool bWon, float Multiplier);
	/** "x1.75" */
	FString MultiplierText(float Multiplier);

	// ---- boons ----
	const TCHAR* BoonName(ETDBoon Boon);
	const TCHAR* BoonDescription(ETDBoon Boon);
	/** Cursed boons: the drawback (empty for normal boons) and the score multiplier they add (0 for normal boons) */
	bool IsCursed(ETDBoon Boon);
	const TCHAR* BoonDrawback(ETDBoon Boon);
	float BoonScore(ETDBoon Boon);
	/** Boon offers come before wave 1 and after clearing these waves */
	inline bool IsBoonMilestone(int32 WaveCleared) { return WaveCleared == 25 || WaveCleared == 50 || WaveCleared == 75; }
	/** False when the boon was taken or doesn't fit the run (wall price under No Walls, extra lives under Glass Reef,
	 *  Kraken's Pact with fewer than PactMinLives lives) */
	bool BoonFits(ETDBoon Boon, const FTDRunRules& Rules, int32 Lives = MAX_int32);
	/** Up to Count different normal boons that fit, in random order; with CursedOfferChance (when bAllowCursed) one card
	 *  is a cursed boon instead. An offer is never only cursed. */
	void RollBoonOffer(const FTDRunRules& Rules, FRandomStream& Random, TArray<int32>& OutOffer, int32 Count = BoonOfferSize, int32 Lives = MAX_int32, bool bAllowCursed = true);

	/** Rules for Retry: the same Abyss tier and mutators, no boons yet */
	inline FTDRunRules RetryRules(const FTDRunRules& Current)
	{
		FTDRunRules Rules;
		Rules.Tier = Current.Tier;
		Rules.Mutators = Current.Mutators;
		return Rules;
	}

	/** Sell refund in this run: TD::SellValue with the run's refund cut, which spares the full build-phase refund */
	int32 RunSellValue(int32 TowerCost, int32 Level, float SellBonus, int32 SkillSpent, int32 TotalSpent, bool bFullRefund);

	/** Deep Pockets payout for a pearl balance */
	inline int32 Interest(int32 Pearls) { return FMath::Clamp(FMath::FloorToInt(Pearls * InterestRate), 0, InterestCap); }

	/** A tower's price in this run (Coral Masons makes walls cheaper) */
	int32 TowerCost(ETDTowerType Type);
}
