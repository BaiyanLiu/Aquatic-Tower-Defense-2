// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "TDTypes.h"

class ATDTower;

// ---------------------------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------------------------

enum class ETDRarity : uint8
{
	Common,
	Rare,
	Epic,
	Legendary
};

/** Each attack tower has one equip slot of each type */
enum class ETDItemSlot : uint8
{
	Shell,
	Pearl,
	Charm,
	Count
};

enum class ETDAffix : uint8
{
	DamagePct,
	DamageFlat,
	RatePct,
	Range,
	RangePct,
	Crit,
	SlowStrength,
	Chain,
	Splash,
	XpPct,
	BountyPct,
	ArmorPierce,
	GoldTrickle,
	XpTrickle,
	Poison,
	OnHitSlow,
	RetiredProjectileSpeed,	// never rolled; keeps saved affix numbers valid, and FTDItemSave::ToItem converts old ones
	ItemChance,
	// effects added with fixed item types (append only: saves store these numbers)
	Stun,				// chance per hit to stun for 0.6s
	Bleed,				// % of current health every 0.5s for 3s
	ArmoredDamage,		// extra damage vs enemies that have armor
	BossDamage,			// extra damage vs bosses and elites
	Execute,			// extra damage vs enemies below TD::ExecuteThreshold health
	AirDamage,			// extra damage vs flying enemies
	CritDamage,			// added to the critical hit multiplier (base x2)
	Count
};

struct FTDAffix
{
	ETDAffix Type = ETDAffix::DamagePct;
	float Value = 0.0f;
};

struct FTDItem
{
	/** 0 means "no item" */
	int32 Id = 0;

	/** Item type (1-based index into the item table, see TD::ItemKind). 0 = an old random-affix item from a save made before item types. */
	int32 Kind = 0;

	FString Name;
	ETDRarity Rarity = ETDRarity::Common;
	ETDItemSlot Slot = ETDItemSlot::Shell;
	int32 Level = 1;
	TArray<FTDAffix> Affixes;

	/** Locked by the player: Sort leaves it where it is and Salvage (and auto-salvage) never takes it */
	bool bLocked = false;

	bool IsValid() const { return Id != 0; }
};

/** Unique legendaries: each has its own mechanic and can be found at most once per run (append only: saves store these) */
enum class ETDUnique : uint8
{
	None,
	Trident,			// shell: every 3rd attack also strikes the 2 nearest other creatures in range
	LeviathanJaw,		// shell: its kills burst for 15% of the creature's max health within 1 tile
	AbyssalAnchor,		// shell: hits hold crawlers in place for 0.4 s (each creature once per 3 s); -10% attack speed
	TearOfMoon,			// pearl: critical hits arc to one more creature for 50% of the damage
	HeartOfCoral,		// pearl: +1% damage per wave cleared while equipped, up to +60%
	BlackTide,			// pearl: this tower's poison and bleed jump to the nearest creature when the carrier dies
	CompassOfTides,		// charm: +1 tile range, hits swimming creatures
	GildedConch,		// charm: +1 pearl per kill; on wave clear, pearls equal to the tower's level
	KrakensHeart,		// charm: towers on the 8 tiles around get +15% damage and attack speed
	Count
};

/** Effect of an item type at item level 1. Positive values grow with item level; negative values (drawbacks) don't. */
struct FTDKindAffix
{
	ETDAffix Type;
	float Value;
};

/** A fixed item type: every copy has the same name and effects, only item level scales them */
struct FTDItemKindDef
{
	const TCHAR* Name;
	ETDItemSlot Slot;
	ETDRarity Rarity;
	const TCHAR* Flavor;
	FTDKindAffix Affixes[3];	// unused entries have Value 0
	ETDUnique Unique = ETDUnique::None;
};

/** Summed affix bonuses of everything a tower has equipped */
struct FTDItemBonus
{
	float Values[(int32)ETDAffix::Count] = {};

	void Add(const FTDAffix& Affix) { Values[(int32)Affix.Type] += Affix.Value; }
	float Get(ETDAffix Type) const { return Values[(int32)Type]; }
};

// ---------------------------------------------------------------------------------------------
// Skills
// ---------------------------------------------------------------------------------------------

struct FTDSkillDef
{
	const TCHAR* Name;
	const TCHAR* Description;	// per rank
	int32 BaseCost;
};

/** A specialisation: at TD::FormLevel an attack tower picks one of two forms that change how it attacks */
struct FTDFormDef
{
	const TCHAR* Name;
	const TCHAR* Line1;		// what it does
	const TCHAR* Line2;		// the trade-off
};

/** Hit description passed from towers / projectiles to enemies */
struct FTDHit
{
	float Damage = 0.0f;
	ETDDamageType DamageType = ETDDamageType::Normal;
	bool bIgnoreArmor = false;
	bool bCrit = false;
	float ArmorPierce = 0.0f;		// armor points ignored
	float ArmoredBonus = 0.0f;		// extra damage multiplier vs enemies with armor
	float BossBonus = 0.0f;			// extra damage multiplier vs bosses and elites
	float ExecuteBonus = 0.0f;		// extra damage multiplier vs enemies below TD::ExecuteThreshold health
	float AirBonus = 0.0f;			// extra damage multiplier vs flying enemies
	float BleedingBonus = 0.0f;		// extra damage multiplier vs enemies that are already bleeding (Ripper sawfish)
	float StunChance = 0.0f;
	float StunDuration = 0.6f;

	// effects applied on hit
	float PoisonPerTick = 0.0f;		// flat damage every 0.5s
	float PoisonDuration = 0.0f;
	float BleedPercent = 0.0f;		// % of current health every 0.5s
	float BleedDuration = 0.0f;
	float SlowFactor = 1.0f;		// speed multiplier
	float SlowDuration = 0.0f;

	// extra targets
	float SplashRadius = 0.0f;		// tiles, secondary splash around the target
	float SplashFraction = 0.5f;
	int32 ChainExtra = 0;			// extra nearby enemies hit
	float ChainFraction = 0.5f;
	bool bSecondary = false;		// splash / chain hits don't spread further
	float XpScale = 1.0f;			// share of damage turned into XP for the source tower
	bool bArcOnCrit = false;		// Tear of the Moon: a critical hit arcs to one more creature
	bool bAnchor = false;			// Abyssal Anchor: holds a crawler in place

	TWeakObjectPtr<ATDTower> Source;
};

namespace TD
{
	constexpr int32 StorageSlots = 40;
	constexpr int32 SkillsPerTower = 3;
	constexpr int32 MaxSkillRank = 7;

	/** Item upgrades cost this many times their base price */
	constexpr float UpgradeCostScale = 1.5f;

	/** Skill rank N costs base price x N^2 x this (rank 1 0.75x, rank 7 36.75x), so full skill trees come late */
	constexpr float SkillCostRankScale = 0.75f;

	// item helpers
	FLinearColor RarityColor(ETDRarity Rarity);
	const TCHAR* RarityName(ETDRarity Rarity);
	const TCHAR* SlotName(ETDItemSlot Slot);
	int32 SalvageValue(ETDRarity Rarity);
	FString AffixText(const FTDAffix& Affix);
	bool AffixApplies(ETDAffix Affix, ETDTowerType Tower);

	/**
	 *  What swapping Old for New would change on a tower: summed value per affix type (New minus Old), in affix order,
	 *  skipping affixes with no effect on the tower and changes too small to show. Old may be an empty item.
	 */
	void CompareItems(const FTDItem& New, const FTDItem& Old, ETDTowerType Tower, TArray<FTDAffix>& OutChanges);

	/** Affix text for a change: gains read like AffixText, losses as "-12% damage" */
	FString AffixDeltaText(const FTDAffix& Change);

	// ---- item types ----

	/** Number of item types; valid kinds are 1..ItemKindCount() */
	int32 ItemKindCount();

	/** Definition of an item type, or nullptr for 0 / out of range */
	const FTDItemKindDef* ItemKind(int32 Kind);

	/** One line of flavor text for typed items, empty for old random-affix items */
	FString ItemFlavor(const FTDItem& Item);

	/** Creates an item of a given type at an item level. Same type and level always give the same effects. */
	FTDItem MakeItemOfKind(int32 Id, int32 Kind, int32 ItemLevel);

	/** Fills Item.Affixes (and name, slot, rarity) from its type at its item level. Does nothing for old untyped items. */
	void ApplyKind(FTDItem& Item);

	// ---- unique legendaries ----

	constexpr int32 UniqueCount = (int32)ETDUnique::Count - 1;
	ETDUnique ItemUnique(const FTDItem& Item);
	inline bool IsUnique(const FTDItem& Item) { return ItemUnique(Item) != ETDUnique::None; }
	/** Item type of a unique (0 for None) */
	int32 UniqueKind(ETDUnique Unique);
	/** What the unique does, for tooltips */
	const TCHAR* UniqueDescription(ETDUnique Unique);
	inline uint32 UniqueBit(ETDUnique Unique) { return 1u << (uint32)Unique; }
	int32 CountUniques(uint32 Found);
	/** A unique not found yet this run (None when all are) */
	ETDUnique PickUnique(uint32 Found, FRandomStream& Random);
	/** Uniques among these items (old saves had no record of found uniques) */
	uint32 InferUniquesFound(const TArray<FTDItem>& Items);

	/** A Legendary drop has this chance to be a unique instead; Kraken bosses have this extra chance of one; the Abyssal Kraken always brings one */
	constexpr float UniqueFromLegendaryChance = 0.25f;
	constexpr float UniqueBossChance = 0.15f;
	/** All creature item drop chances (regular, elite, boss) are scaled by this; halved 2026-11-08 at the user's request */
	constexpr float DropRateScale = 0.5f;
	/** Badge colour that marks an item's slot (Shell, Pearl, Charm) in item tiles */
	FLinearColor SlotColor(ETDItemSlot Slot);

	// mechanic numbers
	constexpr int32 TridentEvery = 3;
	constexpr int32 TridentExtraTargets = 2;
	constexpr float JawBurstShare = 0.15f;
	constexpr float JawBurstRadius = 1.0f;		// tiles
	constexpr float AnchorHold = 0.4f;
	constexpr float AnchorCooldown = 3.0f;
	constexpr float TearArcShare = 0.5f;
	constexpr float TearArcRange = 2.0f;		// tiles
	constexpr float HeartPerWave = 0.01f;
	constexpr int32 HeartMaxWaves = 60;
	constexpr float BlackTideRange = 1.5f;		// tiles
	constexpr float CompassRange = 1.0f;		// tiles
	constexpr int32 ConchKillPearls = 1;
	constexpr float KrakenHeartBonus = 0.15f;
	inline bool TridentStrikes(int32 AttackNumber) { return AttackNumber > 0 && AttackNumber % TridentEvery == 0; }
	inline float HeartOfCoralBonus(int32 Waves) { return HeartPerWave * FMath::Clamp(Waves, 0, HeartMaxWaves); }
	inline bool AnchorReady(double Now, double ReadyAt) { return Now >= ReadyAt; }

	/** Health fraction below which Execute bonus damage applies */
	constexpr float ExecuteThreshold = 0.35f;

	/** Damage multiplier from a hit's conditional bonuses (boss, flying, execute) against a target; bonuses add up */
	float ConditionalDamageMultiplier(const FTDHit& Hit, bool bBossOrElite, bool bFlying, float HealthFraction);

	/** Rolls a rarity, shifted towards better items at higher item levels */
	ETDRarity RollRarity(int32 ItemLevel, ETDRarity Minimum = ETDRarity::Common);

	/** Creates an item of a random type with this rarity (and slot, unless Count). Ids must be unique per run. */
	FTDItem GenerateItem(int32 Id, int32 ItemLevel, ETDRarity Rarity, ETDItemSlot Slot = ETDItemSlot::Count);

	/** Affix strength multiplier from item level: 1 + ItemLevelScalePerLevel per level above 1, up to ItemLevelScaleCap (x3 at level 100) */
	constexpr float ItemLevelScalePerLevel = 0.02f;
	constexpr float ItemLevelScaleCap = 3.0f;
	float ItemLevelScale(int32 ItemLevel);

	// item workbench: spend pearls on a stored item
	constexpr int32 ItemUpgradeLevels = 5;
	constexpr int32 MaxItemLevel = 100;
	int32 UpgradeCost(const FTDItem& Item);
	int32 RerollCost(const FTDItem& Item);

	/** Raises the item level by ItemUpgradeLevels and scales its affixes to match. False at max level. */
	bool UpgradeItem(FTDItem& Item);

	/** Turns the item into a different type of the same slot, rarity and level */
	void RerollItem(FTDItem& Item);

	// skill helpers
	const FTDSkillDef* Skill(ETDTowerType Tower, int32 Index);
	int32 SkillCost(ETDTowerType Tower, int32 Index, int32 NextRank);
	int32 SkillLevelRequirement(int32 NextRank);

	// ---- specialisation (forms) ----

	/** Level at which a tower picks its form (one choice, kept until it's sold) */
	constexpr int32 FormLevel = 15;
	constexpr int32 FormsPerTower = 2;

	/** Form 1 or 2 of a tower (nullptr for 0 and walls) */
	const FTDFormDef* Form(ETDTowerType Tower, int32 Form);

	/** Badge colour of the first and second form (the same for every tower, so the board reads at a glance) */
	FLinearColor FormColor(int32 Form);

	// ---- XP balance (all XP sources in one place) ----

	/** XP for level 1 -> 2; each further level needs XpGrowth times more */
	constexpr int32 XpBase = 40;
	constexpr float XpGrowth = 1.3f;

	/** Levels from 20 up grow by XpGrowthLate instead: at 1.3 the last ten levels would need 250k XP */
	constexpr int32 XpLateFromLevel = 19;
	constexpr float XpGrowthLate = 1.12f;

	/** XP per point of damage from a tower's main hit */
	constexpr float XpPerDamage = 0.22f;

	/** Multipliers for damage that isn't a single targeted hit */
	constexpr float XpScaleSecondary = 1.0f;	// item splash / chain extras
	constexpr float XpScaleArea = 1.0f;			// jellyfish pulse, puffer splash, eel jumps
	constexpr float XpScaleDot = 1.0f;			// poison, bleed, shrapnel

	constexpr float KillXpPerReward = 1.1f;		// kill bonus = reward * this
	constexpr float WaveClearXp = 4.0f;		// to every tower
	constexpr float JellySlowXp = 0.85f;			// per creature slowed, scaled by slow strength
	constexpr float SupportXpPerSecond = 0.4f;	// aura towers learn by being active during waves
	constexpr float HumpbackXpPerSecond = 0.7f;	// the Humpback learns faster than the Acid Vent
	constexpr float SongsXpPerRank = 1.5f;		// Passed-On Songs: XP per second per rank to towers in the aura

	/** Passive XP per second during waves for a support tower (0 for attack towers) */
	inline float SupportXpRate(ETDTowerType Type)
	{
		return Type == ETDTowerType::Humpback ? HumpbackXpPerSecond : (Type == ETDTowerType::AcidVent ? SupportXpPerSecond : 0.0f);
	}

	int32 XpToNextLevel(int32 Level);

	// ---- item balance ----

	/** Old projectile speed affixes load as attack speed; this maps the old value range onto the attack speed range */
	constexpr float RetiredSpeedToRate = 0.4f;
}
