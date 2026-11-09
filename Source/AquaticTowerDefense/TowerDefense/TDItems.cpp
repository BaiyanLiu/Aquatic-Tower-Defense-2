// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDItems.h"
#include "TDRun.h"

namespace
{
	// skills per tower type (row = tower type - 1); walls have none. Ideas ported from the original upgrades.
	const FTDSkillDef GSkills[TD::TowerTypeCount - 1][3] =
	{
		{	// Harpoon
			{ TEXT("Piercing Bolt"),		TEXT("Harpoons pass through +1 enemy per rank"),				30 },
			{ TEXT("Rapid Reload"),			TEXT("+15% attack speed per rank"),								25 },
			{ TEXT("Poisoned Tip"),			TEXT("Poison: 3 damage every 0.5s for 2s per rank"),			30 },
		},
		{	// Jellyfish
			{ TEXT("Deep Sting"),			TEXT("Slows enemies 4% more per rank"),							30 },
			{ TEXT("Neurotoxin"),			TEXT("+50% damage to slowed enemies per rank"),					40 },
			{ TEXT("Wide Bloom"),			TEXT("+0.15 tiles range per rank"),								25 },
		},
		{	// Puffer
			{ TEXT("Big Spines"),			TEXT("+0.25 tiles splash radius per rank"),						30 },
			{ TEXT("Shrapnel"),				TEXT("Spine patch deals 25% damage over 2s per rank"),			45 },
			{ TEXT("Concussion"),			TEXT("7% chance per rank to stun for 0.6s"),					40 },
		},
		{	// Eel
			{ TEXT("Overcharge"),			TEXT("+1 chain target per rank"),								50 },
			{ TEXT("Paralyzing Shock"),		TEXT("6% chance per rank to stun each enemy hit"),				40 },
			{ TEXT("Conductor"),			TEXT("+20% damage to armored enemies per rank"),				45 },
		},
		{	// Mantis Shrimp
			{ TEXT("Heavy Clubs"),		TEXT("+15% damage per rank"),									15 },
			{ TEXT("Moulted Shell"),	TEXT("+10% sell value per rank"),								10 },
			{ TEXT("Keen Eyes"),		TEXT("+20% XP gain per rank"),									15 },
		},
		{	// Octopus
			{ TEXT("Rough Edges"),			TEXT("+15% damage per rank"),									15 },
			{ TEXT("Eight Arms"),		TEXT("Each volley hits +1 more enemy per rank"),				25 },
			{ TEXT("Strong Throw"),			TEXT("+0.1 tiles range per rank"),								10 },
		},
		{	// Lionfish
			{ TEXT("Potent Venom"),		TEXT("+30% poison damage per rank"),							25 },
			{ TEXT("Lingering Venom"),		TEXT("Poison lasts +1 second per rank"),						25 },
			{ TEXT("Reef Forager"),					TEXT("+15% item drop chance per rank"),							15 },
		},
		{	// Sawfish
			{ TEXT("Serrated Teeth"),		TEXT("Bleed takes +0.5% more health per tick per rank"),		25 },
			{ TEXT("Tooth Spray"),		TEXT("Teeth also hit +1 nearby enemy per rank"),				30 },
			{ TEXT("Electric Snout"),	TEXT("+0.15 tiles range per rank"),								15 },
		},
		{	// Humpback
			{ TEXT("Melody"),				TEXT("Aura also gives +5% damage per rank"),					20 },
			{ TEXT("Deep Channel"),		TEXT("+0.15 tiles aura range per rank"),						20 },
			{ TEXT("Passed-On Songs"),	TEXT("Towers in the aura gain +1.5 XP per second per rank"),	25 },
		},
		{	// Acid Vent
			{ TEXT("Stronger Acid"),		TEXT("Aura lowers armor by 5 more per rank"),					25 },
			{ TEXT("Silt Plume"),	TEXT("Aura slows enemies by 6% per rank"),						30 },
			{ TEXT("Mineral Growth"),		TEXT("+25% XP gain per rank"),									15 },
		},
	};

	/** Specialisations of every tower but the wall, Harpoon to Acid Vent (see ATDTower::RecalculateStats, the Fire functions and UpdateAuras) */
	const FTDFormDef GForms[TD::TowerTypeCount - 1][TD::FormsPerTower] =
	{
		{	// Harpoon
			{ TEXT("Swordfish"),		TEXT("Fires two harpoons per shot, at two targets"),		TEXT("Each harpoon deals 65% damage") },
			{ TEXT("Narwhal"),			TEXT("Harpoons pass through 2 more enemies, ignore 20 armor"),	TEXT("-15% attack speed") },
		},
		{	// Jellyfish
			{ TEXT("Box Jelly"),		TEXT("Pulses poison: 50% of damage every 0.5s for 2s"),		TEXT("Slows 10% less") },
			{ TEXT("Moon Jelly"),		TEXT("Every 3rd pulse stuns everything in range for 0.5s"),	TEXT("-15% attack speed") },
		},
		{	// Puffer
			{ TEXT("Cluster Puffer"),	TEXT("Each bomb bursts into 3 bomblets around the target"),	TEXT("Bomblets deal 45% damage, smaller splash") },
			{ TEXT("Deep Lobber"),		TEXT("+2 tiles range, aims at the toughest creature"),		TEXT("-20% attack speed") },
		},
		{	// Eel
			{ TEXT("Storm Eel"),		TEXT("Chain jumps twice as far and doesn't weaken"),		TEXT("-20% damage") },
			{ TEXT("Overload Eel"),		TEXT("+80% damage, every hit stuns for 0.4s"),				TEXT("Chains to half as many enemies") },
		},
		{	// Mantis Shrimp
			{ TEXT("Smasher"),			TEXT("Punches send a shockwave: 50% damage within 1 tile"),	TEXT("The shockwave doesn't stun") },
			{ TEXT("Spearer"),			TEXT("+1.5 tiles range, +30% damage, hits swimming"),			TEXT("No stun") },
		},
		{	// Octopus
			{ TEXT("Juggler"),			TEXT("Each volley throws at 2 more enemies"),				TEXT("-30% damage") },
			{ TEXT("Blue-ringed"),		TEXT("Stones poison: 100% of damage every 0.5s for 2s"),	TEXT("-25% attack speed") },
		},
		{	// Lionfish
			{ TEXT("Venom Cloud"),		TEXT("Spines spread their poison within 1 tile"),			TEXT("The cloud deals no direct damage") },
			{ TEXT("Paralytic Venom"),	TEXT("Poisoned creatures are slowed by 25%"),				TEXT("-15% poison damage") },
		},
		{	// Sawfish
			{ TEXT("Ripper"),			TEXT("+100% damage to bleeding creatures, bleed lasts 5s"),	TEXT("-20% attack speed") },
			{ TEXT("Sweeper"),			TEXT("Every throw also slashes all creatures within 1.2 tiles"),	TEXT("The slash only bleeds, no direct damage") },
		},
		{	// Humpback
			{ TEXT("Sonar Whale"),		TEXT("Aura gives attack towers +0.5 tiles range"),	TEXT("No attack speed bonus") },
			{ TEXT("Chorus Whale"),		TEXT("+1.5 tiles aura range"),								TEXT("Aura bonuses are 35% weaker") },
		},
		{	// Acid Vent
			{ TEXT("Cold Seep"),		TEXT("Aura slows creatures by 25%"),	TEXT("No armor reduction") },
			{ TEXT("Black Smoker"),		TEXT("Aura cuts 50% more armor"),							TEXT("-0.5 tiles aura range") },
		},
	};

	using IA = ETDAffix;
	constexpr ETDItemSlot IShell = ETDItemSlot::Shell;
	constexpr ETDItemSlot IPearl = ETDItemSlot::Pearl;
	constexpr ETDItemSlot ICharm = ETDItemSlot::Charm;
	constexpr ETDRarity ICommon = ETDRarity::Common;
	constexpr ETDRarity IRare = ETDRarity::Rare;
	constexpr ETDRarity IEpic = ETDRarity::Epic;
	constexpr ETDRarity ILegendary = ETDRarity::Legendary;

	/**
	 *  Every item type in the game. Values are at item level 1; positive ones grow with item level (TD::ItemLevelScale),
	 *  drawbacks (negative values), chain targets and stun chance stay fixed. Reach, splash, on-hit slow and pearl trickle
	 *  grow at a quarter of the rate (x1.5 at item level 100 instead of x3), see AffixLevelScale.
	 *  APPEND ONLY: saves store the 1-based index into this table. Each slot needs at least two types per rarity so rerolls can change it.
	 *  Shells hit harder, pearls add on-hit effects, charms help progress, range and support towers.
	 */
	const FTDItemKindDef GItemKinds[] =
	{
		// ---------------- Shells ----------------
		{ TEXT("Sharpened Cowrie"),		IShell, ICommon,		TEXT("Filed to a wicked edge on the reef rocks."),				{ { IA::DamagePct, 0.30f } } },
		{ TEXT("Spiral Whelk"),			IShell, ICommon,		TEXT("Its coils hum when the current runs fast."),				{ { IA::RatePct, 0.24f } } },
		{ TEXT("Barnacle Plate"),		IShell, ICommon,		TEXT("Crusted with barnacles that bite through any hide."),		{ { IA::ArmorPierce, 14.0f } } },
		{ TEXT("Scallop Fan"),			IShell, ICommon,		TEXT("Every blow spreads out like a ripple."),					{ { IA::Splash, 0.5f } } },
		{ TEXT("Hermit's Hammer"),		IShell, IRare,		TEXT("Heavy, slow, and borrowed from a very angry crab."),		{ { IA::DamagePct, 0.60f }, { IA::RatePct, -0.15f } } },
		{ TEXT("Razor Clam"),			IShell, IRare,		TEXT("Thin as paper, and it leaves a trail."),					{ { IA::Bleed, 1.0f }, { IA::RatePct, 0.15f } } },
		{ TEXT("Ironclad Nautilus"),	IShell, IRare,		TEXT("Armor is just a shell it hasn't cracked yet."),			{ { IA::ArmoredDamage, 0.25f }, { IA::ArmorPierce, 10.0f } } },
		{ TEXT("Titan Clam"),			IShell, IEpic,		TEXT("Built to crack the biggest things in the deep."),			{ { IA::BossDamage, 0.70f }, { IA::DamagePct, 0.25f } } },
		{ TEXT("Triton's Trumpet"),		IShell, IEpic,		TEXT("Its call leaps from one creature to the next."),			{ { IA::Chain, 1.0f }, { IA::DamagePct, 0.20f }, { IA::RatePct, 0.15f } } },
		{ TEXT("Murex of Endings"),		IShell, IEpic,		TEXT("Finishes what others start."),							{ { IA::Execute, 0.80f }, { IA::Crit, 0.15f } } },
		{ TEXT("Heart of the Leviathan"), IShell, ILegendary, TEXT("Still beating, slow and enormous."),						{ { IA::DamagePct, 0.80f }, { IA::BossDamage, 0.50f }, { IA::ArmorPierce, 20.0f } } },
		{ TEXT("Crown of Brine"),		IShell, ILegendary,	TEXT("Rules the currents, not the blow."),						{ { IA::RatePct, 0.45f }, { IA::Chain, 2.0f }, { IA::DamagePct, -0.20f } } },

		// ---------------- Pearls ----------------
		{ TEXT("Lantern Pearl"),		IPearl, ICommon,		TEXT("Glows brightest over a weak spot."),						{ { IA::Crit, 0.20f } } },
		{ TEXT("Black Pearl"),			IPearl, ICommon,		TEXT("Grown in a venomous oyster."),							{ { IA::Poison, 5.0f } } },
		{ TEXT("Cold Seep Pearl"),		IPearl, ICommon,		TEXT("Cold water seeps from it and stiffens fins."),			{ { IA::OnHitSlow, 0.25f } } },
		{ TEXT("Seafoam Pearl"),		IPearl, ICommon,		TEXT("Light as foam, it rises after whatever swims above."),	{ { IA::AirDamage, 0.55f } } },
		{ TEXT("Baroque Pearl"),		IPearl, IRare,		TEXT("Lumpy, ugly, and it hits exactly where it hurts."),		{ { IA::CritDamage, 0.50f }, { IA::Crit, 0.10f } } },
		{ TEXT("Cone Snail Pearl"),		IPearl, IRare,		TEXT("The strongest venom on the reef. Slow to load."),			{ { IA::Poison, 10.0f }, { IA::RatePct, -0.12f } } },
		{ TEXT("Torpedo Ray Pearl"),	IPearl, IRare,		TEXT("Crackles when you hold it."),								{ { IA::Stun, 0.08f }, { IA::DamagePct, 0.12f } } },
		{ TEXT("Moon Pearl"),			IPearl, IEpic,		TEXT("Pulls on hits the way the moon pulls the tide."),			{ { IA::Crit, 0.25f }, { IA::CritDamage, 0.60f } } },
		{ TEXT("Siren's Tear"),			IPearl, IEpic,		TEXT("Creatures stop to listen, and some never move again."),	{ { IA::OnHitSlow, 0.30f }, { IA::Stun, 0.06f }, { IA::Range, 0.08f } } },
		{ TEXT("Red Tide Pearl"),		IPearl, IEpic,		TEXT("Turns the water around it rust red."),					{ { IA::Poison, 8.0f }, { IA::Bleed, 1.0f } } },
		{ TEXT("Pearl of the Abyss"),	IPearl, ILegendary,	TEXT("It has seen the bottom, and it sees through you."),		{ { IA::Crit, 0.35f }, { IA::CritDamage, 0.80f }, { IA::DamagePct, 0.20f } } },
		{ TEXT("Kraken's Eye"),			IPearl, ILegendary,	TEXT("Every hit bursts into a cloud of ink."),					{ { IA::Splash, 0.6f }, { IA::OnHitSlow, 0.25f }, { IA::Stun, 0.12f } } },

		// ---------------- Charms ----------------
		{ TEXT("Driftwood Totem"),		ICharm, ICommon,		TEXT("Carved with lessons from a hundred tides."),				{ { IA::XpPct, 0.45f } } },
		{ TEXT("Sand Dollar"),			ICharm, ICommon,		TEXT("Currency of the shallows."),								{ { IA::BountyPct, 0.225f } } },
		{ TEXT("Lucky Starfish"),		ICharm, ICommon,		TEXT("Five arms, five chances."),								{ { IA::ItemChance, 0.45f } } },
		{ TEXT("Kelp Braid"),			ICharm, ICommon,		TEXT("Lets a tower reach a little further into the current."),	{ { IA::Range, 0.15f } } },
		{ TEXT("Sunken Coin Purse"),	ICharm, IRare,		TEXT("Still leaking coins from a ship long gone."),				{ { IA::GoldTrickle, 0.25f }, { IA::BountyPct, 0.125f } } },
		{ TEXT("Scholar's Coral"),		ICharm, IRare,		TEXT("Grows a new ring with every wave it watches."),			{ { IA::XpTrickle, 3.0f }, { IA::XpPct, 0.25f } } },
		{ TEXT("Clearwater Lens"),		ICharm, IRare,		TEXT("Polished from glass that washed ashore."),				{ { IA::Range, 0.15f }, { IA::RangePct, 0.03f } } },
		{ TEXT("Pirate's Doubloon"),	ICharm, IEpic,		TEXT("Cursed, of course. Worth it."),							{ { IA::BountyPct, 0.45f }, { IA::ItemChance, 0.60f }, { IA::DamagePct, -0.15f } } },
		{ TEXT("Whalesong Charm"),		ICharm, IEpic,		TEXT("Hums a song that carries for miles."),					{ { IA::Range, 0.18f }, { IA::XpTrickle, 4.0f }, { IA::XpPct, 0.40f } } },
		{ TEXT("Flying Fish Fin"),		ICharm, IEpic,		TEXT("Knows exactly where things leap and land."),				{ { IA::AirDamage, 0.60f }, { IA::Range, 0.12f }, { IA::Crit, 0.12f } } },
		{ TEXT("Tidecaller's Conch"),	ICharm, ILegendary,	TEXT("Blow it and the whole sea leans closer."),				{ { IA::Range, 0.25f }, { IA::RangePct, 0.05f }, { IA::XpPct, 0.60f } } },
		{ TEXT("Siren's Locket"),		ICharm, ILegendary,	TEXT("Sailors gave it everything they had."),					{ { IA::GoldTrickle, 0.75f }, { IA::BountyPct, 0.5f }, { IA::ItemChance, 1.0f } } },

		// ---------------- Uniques: one of each per run, never from a rarity roll alone (see TD::PickUnique) ----------------
		{ TEXT("Trident of the Deep King"),	IShell, ILegendary,	TEXT("Every third thrust finds two more."),					{ { IA::DamagePct, 0.30f } },								ETDUnique::Trident },
		{ TEXT("Leviathan's Jaw"),			IShell, ILegendary,	TEXT("What it bites, bursts."),								{ { IA::DamagePct, 0.25f } },								ETDUnique::LeviathanJaw },
		{ TEXT("Abyssal Anchor"),			IShell, ILegendary,	TEXT("Heavy enough to pin anything to the seabed."),		{ { IA::DamagePct, 0.40f }, { IA::RatePct, -0.10f } },		ETDUnique::AbyssalAnchor },
		{ TEXT("Tear of the Moon"),			IPearl, ILegendary,	TEXT("Moonlight never strikes only once."),					{ { IA::Crit, 0.15f }, { IA::CritDamage, 0.30f } },			ETDUnique::TearOfMoon },
		{ TEXT("Heart of Coral"),			IPearl, ILegendary,	TEXT("Grows a little stronger with every tide it survives."),	{ { IA::XpPct, 0.20f } },									ETDUnique::HeartOfCoral },
		{ TEXT("Black Tide Pearl"),			IPearl, ILegendary,	TEXT("Its venom never dies with the poisoned."),			{ { IA::Poison, 6.0f } },									ETDUnique::BlackTide },
		{ TEXT("Compass of Tides"),			ICharm, ILegendary,	TEXT("Points at whatever swims, wherever it swims."),		{ { IA::AirDamage, 0.20f } },								ETDUnique::CompassOfTides },
		{ TEXT("Gilded Conch"),				ICharm, ILegendary,	TEXT("Hums with the sound of falling coins."),				{ { IA::BountyPct, 0.20f } },								ETDUnique::GildedConch },
		{ TEXT("Kraken's Heart"),			ICharm, ILegendary,	TEXT("Its slow beat drives every tower beside it."),		{ { IA::XpPct, 0.20f } },									ETDUnique::KrakensHeart },
	};

	/** Affixes whose value is a count or a chance that should not grow with item level */
	bool AffixScalesWithLevel(ETDAffix Affix)
	{
		return Affix != ETDAffix::Chain && Affix != ETDAffix::Stun;
	}

	/**
	 *  Item level multiplier for one affix. Reach (range, splash radius), on-hit slow and pearl trickle only get a quarter
	 *  of the growth: at the full rate a level 100 item stretched towers across the board, put a 3-tile splash on every tower,
	 *  hit the slow cap on a common pearl and paid more pearls per wave than the wave itself.
	 */
	float AffixLevelScale(ETDAffix Affix, float LevelScale)
	{
		switch (Affix)
		{
		case ETDAffix::Range:
		case ETDAffix::RangePct:
		case ETDAffix::Splash:
		case ETDAffix::OnHitSlow:
		case ETDAffix::GoldTrickle:
			return 1.0f + 0.25f * (LevelScale - 1.0f);
		default:
			return LevelScale;
		}
	}

	/** Highest value one item can give, so text never promises more than the tower stat allows */
	float AffixCap(ETDAffix Affix)
	{
		switch (Affix)
		{
		case ETDAffix::Crit:		return 0.75f;	// tower crit chance cap
		case ETDAffix::OnHitSlow:	return 0.6f;	// tower on-hit slow cap
		default:					return TNumericLimits<float>::Max();
		}
	}

	/** Item types of a rarity (and slot, unless Count) */
	void KindsMatching(ETDRarity Rarity, ETDItemSlot Slot, TArray<int32>& OutKinds)
	{
		OutKinds.Reset();
		for (int32 i = 0; i < UE_ARRAY_COUNT(GItemKinds); ++i)
		{
			// (uniques never come from a roll by rarity, nor from a reroll)
			if (GItemKinds[i].Rarity == Rarity && GItemKinds[i].Unique == ETDUnique::None && (Slot == ETDItemSlot::Count || GItemKinds[i].Slot == Slot))
			{
				OutKinds.Add(i + 1);
			}
		}
	}
}

FLinearColor TD::RarityColor(ETDRarity Rarity)
{
	switch (Rarity)
	{
	case ETDRarity::Rare:		return FLinearColor(0.25f, 0.55f, 1.0f);
	case ETDRarity::Epic:		return FLinearColor(0.72f, 0.3f, 1.0f);
	case ETDRarity::Legendary:	return FLinearColor(1.0f, 0.55f, 0.1f);
	default:					return FLinearColor(0.62f, 0.64f, 0.66f);
	}
}

const TCHAR* TD::RarityName(ETDRarity Rarity)
{
	switch (Rarity)
	{
	case ETDRarity::Rare:		return TEXT("Rare");
	case ETDRarity::Epic:		return TEXT("Epic");
	case ETDRarity::Legendary:	return TEXT("Legendary");
	default:					return TEXT("Common");
	}
}

FLinearColor TD::SlotColor(ETDItemSlot Slot)
{
	// kept away from the rarity colours (grey, blue, purple, orange) so a badge never reads as a rarity
	switch (Slot)
	{
	case ETDItemSlot::Pearl:	return FLinearColor(1.0f, 0.42f, 0.66f);	// pink
	case ETDItemSlot::Charm:	return FLinearColor(0.72f, 0.95f, 0.22f);	// lime
	default:					return FLinearColor(0.2f, 0.82f, 0.95f);	// cyan
	}
}

const TCHAR* TD::SlotName(ETDItemSlot Slot)
{
	switch (Slot)
	{
	case ETDItemSlot::Pearl:	return TEXT("Pearl");
	case ETDItemSlot::Charm:	return TEXT("Charm");
	default:					return TEXT("Shell");
	}
}

int32 TD::SalvageValue(ETDRarity Rarity)
{
	switch (Rarity)
	{
	case ETDRarity::Rare:		return 8;
	case ETDRarity::Epic:		return 20;
	case ETDRarity::Legendary:	return 50;
	default:					return 3;
	}
}

FString TD::AffixText(const FTDAffix& Affix)
{
	// drawbacks are stored as negative values and read "-15% attack speed"
	const TCHAR* Sign = Affix.Value < 0.0f ? TEXT("-") : TEXT("+");
	const float V = FMath::Abs(Affix.Value);
	const int32 Pct = FMath::RoundToInt(V * 100);
	switch (Affix.Type)
	{
	case ETDAffix::DamagePct:		return FString::Printf(TEXT("%s%d%% damage"), Sign, Pct);
	case ETDAffix::DamageFlat:		return FString::Printf(TEXT("%s%.1f damage"), Sign, V);
	case ETDAffix::RatePct:			return FString::Printf(TEXT("%s%d%% attack speed"), Sign, Pct);
	case ETDAffix::Range:			return FString::Printf(TEXT("%s%.2f tiles range"), Sign, V);
	case ETDAffix::RangePct:		return FString::Printf(TEXT("%s%d%% range"), Sign, Pct);
	case ETDAffix::Crit:			return FString::Printf(TEXT("%s%d%% critical hit chance"), Sign, Pct);
	case ETDAffix::SlowStrength:	return FString::Printf(TEXT("%s%d%% slow strength"), Sign, Pct);
	case ETDAffix::Chain:			return FString::Printf(TEXT("%s%d chain target%s"), Sign, FMath::RoundToInt(V), FMath::RoundToInt(V) == 1 ? TEXT("") : TEXT("s"));
	case ETDAffix::Splash:			return FString::Printf(TEXT("%s%.2f tiles splash radius"), Sign, V);
	case ETDAffix::XpPct:			return FString::Printf(TEXT("%s%d%% XP gain"), Sign, Pct);
	case ETDAffix::BountyPct:		return FString::Printf(TEXT("%s%d%% pearls from kills"), Sign, Pct);
	case ETDAffix::ArmorPierce:		return FString::Printf(TEXT("Ignores %d armor"), FMath::RoundToInt(V));
	case ETDAffix::GoldTrickle:		return FString::Printf(TEXT("%s%.2f pearls per second during waves"), Sign, V);
	case ETDAffix::XpTrickle:		return FString::Printf(TEXT("%s%.1f XP per second during waves"), Sign, V);
	case ETDAffix::Poison:			return FString::Printf(TEXT("Hits poison for %.1f damage every 0.5s (2s)"), V);
	case ETDAffix::OnHitSlow:		return FString::Printf(TEXT("Hits slow enemies by %d%% for 1s"), Pct);
	case ETDAffix::ItemChance:		return FString::Printf(TEXT("%s%d%% item drop chance"), Sign, Pct);
	case ETDAffix::Stun:			return FString::Printf(TEXT("%s%d%% chance to stun for 0.6s"), Sign, Pct);
	case ETDAffix::Bleed:			return FString::Printf(TEXT("Hits bleed %.1f%% of current health every 0.5s (3s)"), V);
	case ETDAffix::ArmoredDamage:	return FString::Printf(TEXT("%s%d%% damage to armored enemies"), Sign, Pct);
	case ETDAffix::BossDamage:		return FString::Printf(TEXT("%s%d%% damage to bosses and elites"), Sign, Pct);
	case ETDAffix::Execute:			return FString::Printf(TEXT("%s%d%% damage to enemies below %d%% health"), Sign, Pct, FMath::RoundToInt(ExecuteThreshold * 100));
	case ETDAffix::AirDamage:		return FString::Printf(TEXT("%s%d%% damage to swimming enemies"), Sign, Pct);
	case ETDAffix::CritDamage:		return FString::Printf(TEXT("%s%d%% critical hit damage"), Sign, Pct);
	default:						return FString();
	}
}

void TD::CompareItems(const FTDItem& New, const FTDItem& Old, ETDTowerType Tower, TArray<FTDAffix>& OutChanges)
{
	OutChanges.Reset();
	FTDItemBonus Delta;
	for (const FTDAffix& Affix : New.Affixes)
	{
		Delta.Add(Affix);
	}
	if (Old.IsValid())
	{
		for (const FTDAffix& Affix : Old.Affixes)
		{
			Delta.Add({ Affix.Type, -Affix.Value });
		}
	}
	for (int32 i = 0; i < (int32)ETDAffix::Count; ++i)
	{
		const ETDAffix Type = (ETDAffix)i;
		const float Value = Delta.Get(Type);
		if (FMath::Abs(Value) > 0.005f && AffixApplies(Type, Tower))
		{
			OutChanges.Add({ Type, Value });
		}
	}
}

FString TD::AffixDeltaText(const FTDAffix& Change)
{
	if (Change.Value >= 0.0f)
	{
		return AffixText(Change);
	}
	const FString Text = AffixText({ Change.Type, -Change.Value });
	return Text.StartsWith(TEXT("+")) ? TEXT("-") + Text.RightChop(1) : TEXT("Loses: ") + Text;
}

bool TD::AffixApplies(ETDAffix Affix, ETDTowerType Tower)
{
	if (Tower == ETDTowerType::Wall || Tower == ETDTowerType::Count)
	{
		return false;
	}

	const FTDTowerDef& Def = TD::Tower(Tower);
	if (Def.bSupport)
	{
		// aura towers only care about reach and progression
		return Affix == ETDAffix::Range || Affix == ETDAffix::RangePct || Affix == ETDAffix::XpPct
			|| Affix == ETDAffix::XpTrickle || Affix == ETDAffix::GoldTrickle;
	}

	switch (Affix)
	{
	case ETDAffix::SlowStrength:	return Tower == ETDTowerType::Jelly;
	case ETDAffix::Stun:			return Tower != ETDTowerType::Mantis;	// every punch already stuns
	case ETDAffix::AirDamage:		return Def.bHitsFlying;
	case ETDAffix::Chain:			return Tower != ETDTowerType::Jelly && Tower != ETDTowerType::Puffer;	// a bomb's splash doesn't chain
	case ETDAffix::Splash:			return Tower != ETDTowerType::Jelly;
	case ETDAffix::ArmorPierce:		return Tower != ETDTowerType::Eel;	// eels already ignore armor
	default:						return true;
	}
}

ETDRarity TD::RollRarity(int32 ItemLevel, ETDRarity Minimum)
{
	const float W = (float)FMath::Max(0, ItemLevel - 1);
	float Weights[4] =
	{
		FMath::Max(40.0f, 70.0f - W * 1.0f),
		22.0f + W * 0.5f,
		7.0f + W * 0.35f,
		1.0f + W * 0.15f
	};
	for (int32 i = 0; i < (int32)Minimum; ++i)
	{
		Weights[i] = 0.0f;
	}

	const float Total = Weights[0] + Weights[1] + Weights[2] + Weights[3];
	float Roll = FMath::FRand() * Total;
	for (int32 i = 0; i < 4; ++i)
	{
		if (Roll < Weights[i])
		{
			return (ETDRarity)i;
		}
		Roll -= Weights[i];
	}
	return ETDRarity::Legendary;
}

float TD::ItemLevelScale(int32 ItemLevel)
{
	return FMath::Min(ItemLevelScaleCap, 1.0f + ItemLevelScalePerLevel * (ItemLevel - 1));
}

int32 TD::UpgradeCost(const FTDItem& Item)
{
	// Cheap Upgrades (run boon) takes a share off
	return FMath::RoundToInt((10 + Item.Level) * (1 + (int32)Item.Rarity) * UpgradeCostScale * Run().UpgradeCostScale());
}

int32 TD::RerollCost(const FTDItem& Item)
{
	return 15 * (1 + (int32)Item.Rarity) + Item.Level;
}

bool TD::UpgradeItem(FTDItem& Item)
{
	if (Item.Level >= MaxItemLevel)
	{
		return false;
	}
	const int32 NewLevel = FMath::Min(MaxItemLevel, Item.Level + ItemUpgradeLevels);
	if (ItemKind(Item.Kind))
	{
		Item.Level = NewLevel;
		ApplyKind(Item);
		return true;
	}

	// old random-affix item: scale what it rolled
	const float Ratio = ItemLevelScale(NewLevel) / ItemLevelScale(Item.Level);
	for (FTDAffix& Affix : Item.Affixes)
	{
		if (Affix.Type == ETDAffix::Chain)
		{
			continue;
		}
		Affix.Value *= Ratio;
		if (Affix.Type == ETDAffix::ArmorPierce)
		{
			Affix.Value = FMath::RoundToFloat(Affix.Value);
		}
	}
	Item.Level = NewLevel;
	return true;
}

void TD::RerollItem(FTDItem& Item)
{
	// a unique is what it is: it can be upgraded but never rerolled
	if (IsUnique(Item))
	{
		return;
	}
	TArray<int32> Kinds;
	KindsMatching(Item.Rarity, Item.Slot, Kinds);
	Kinds.Remove(Item.Kind);
	if (Kinds.Num() == 0)
	{
		return;
	}
	const bool bLocked = Item.bLocked;
	Item = MakeItemOfKind(Item.Id, Kinds[FMath::RandRange(0, Kinds.Num() - 1)], Item.Level);
	Item.bLocked = bLocked;
}

float TD::ConditionalDamageMultiplier(const FTDHit& Hit, bool bBossOrElite, bool bFlying, float HealthFraction)
{
	float Bonus = 0.0f;
	Bonus += bBossOrElite ? Hit.BossBonus : 0.0f;
	Bonus += bFlying ? Hit.AirBonus : 0.0f;
	Bonus += HealthFraction < ExecuteThreshold ? Hit.ExecuteBonus : 0.0f;
	return 1.0f + Bonus;
}

ETDUnique TD::ItemUnique(const FTDItem& Item)
{
	const FTDItemKindDef* Def = ItemKind(Item.Kind);
	return Def ? Def->Unique : ETDUnique::None;
}

int32 TD::UniqueKind(ETDUnique Unique)
{
	for (int32 i = 0; i < UE_ARRAY_COUNT(GItemKinds); ++i)
	{
		if (Unique != ETDUnique::None && GItemKinds[i].Unique == Unique)
		{
			return i + 1;
		}
	}
	return 0;
}

const TCHAR* TD::UniqueDescription(ETDUnique Unique)
{
	switch (Unique)
	{
	case ETDUnique::Trident:		return TEXT("Every 3rd attack also strikes the 2 nearest other creatures in range for full damage.");
	case ETDUnique::LeviathanJaw:	return TEXT("Creatures this tower kills burst, dealing 15% of their max health to creatures within 1 tile.");
	case ETDUnique::AbyssalAnchor:	return TEXT("Hits hold crawling creatures in place for 0.4s (each creature at most once every 3s).");
	case ETDUnique::TearOfMoon:		return TEXT("Critical hits arc to one more creature for 50% of the damage.");
	case ETDUnique::HeartOfCoral:	return TEXT("+1% damage for each wave cleared while on this tower, up to +60%. Starts over if taken off.");
	case ETDUnique::BlackTide:		return TEXT("When a creature carrying this tower's poison or bleed dies, the effect jumps to the nearest creature within 1.5 tiles.");
	case ETDUnique::CompassOfTides:	return TEXT("+1 tile range, and the tower can hit swimming creatures.");
	case ETDUnique::GildedConch:	return TEXT("Kills give +1 pearl. Each wave cleared, the tower gives pearls equal to its level.");
	case ETDUnique::KrakensHeart:	return TEXT("Towers on the 8 tiles around this one get +15% damage and +15% attack speed.");
	default:						return TEXT("");
	}
}

int32 TD::CountUniques(uint32 Found)
{
	int32 Count = 0;
	for (int32 u = 1; u <= UniqueCount; ++u)
	{
		Count += (Found & UniqueBit((ETDUnique)u)) ? 1 : 0;
	}
	return Count;
}

ETDUnique TD::PickUnique(uint32 Found, FRandomStream& Random)
{
	TArray<ETDUnique> Left;
	for (int32 u = 1; u <= UniqueCount; ++u)
	{
		if (!(Found & UniqueBit((ETDUnique)u)))
		{
			Left.Add((ETDUnique)u);
		}
	}
	return Left.Num() > 0 ? Left[Random.RandRange(0, Left.Num() - 1)] : ETDUnique::None;
}

uint32 TD::InferUniquesFound(const TArray<FTDItem>& Items)
{
	uint32 Found = 0;
	for (const FTDItem& Item : Items)
	{
		const ETDUnique Unique = ItemUnique(Item);
		if (Unique != ETDUnique::None)
		{
			Found |= UniqueBit(Unique);
		}
	}
	return Found;
}

int32 TD::ItemKindCount()
{
	return UE_ARRAY_COUNT(GItemKinds);
}

const FTDItemKindDef* TD::ItemKind(int32 Kind)
{
	return (Kind >= 1 && Kind <= ItemKindCount()) ? &GItemKinds[Kind - 1] : nullptr;
}

FString TD::ItemFlavor(const FTDItem& Item)
{
	const FTDItemKindDef* Def = ItemKind(Item.Kind);
	return Def ? FString(Def->Flavor) : FString();
}

void TD::ApplyKind(FTDItem& Item)
{
	const FTDItemKindDef* Def = ItemKind(Item.Kind);
	if (!Def)
	{
		return;
	}
	Item.Name = Def->Name;
	Item.Slot = Def->Slot;
	Item.Rarity = Def->Rarity;
	Item.Level = FMath::Clamp(Item.Level, 1, MaxItemLevel);
	Item.Affixes.Reset();

	const float LevelScale = ItemLevelScale(Item.Level);
	for (const FTDKindAffix& Base : Def->Affixes)
	{
		if (Base.Value == 0.0f)
		{
			continue;
		}
		FTDAffix& Affix = Item.Affixes.AddDefaulted_GetRef();
		Affix.Type = Base.Type;
		Affix.Value = (Base.Value > 0.0f && AffixScalesWithLevel(Base.Type)) ? Base.Value * AffixLevelScale(Base.Type, LevelScale) : Base.Value;
		if (Affix.Type == ETDAffix::ArmorPierce)
		{
			Affix.Value = FMath::RoundToFloat(Affix.Value);
		}
		Affix.Value = FMath::Min(Affix.Value, AffixCap(Affix.Type));
	}
}

FTDItem TD::MakeItemOfKind(int32 Id, int32 Kind, int32 ItemLevel)
{
	FTDItem Item;
	if (!ItemKind(Kind))
	{
		return Item;
	}
	Item.Id = Id;
	Item.Kind = Kind;
	Item.Level = ItemLevel;
	ApplyKind(Item);
	return Item;
}

FTDItem TD::GenerateItem(int32 Id, int32 ItemLevel, ETDRarity Rarity, ETDItemSlot Slot)
{
	TArray<int32> Kinds;
	KindsMatching(Rarity, Slot, Kinds);
	if (Kinds.Num() == 0)
	{
		KindsMatching(Rarity, ETDItemSlot::Count, Kinds);
	}
	check(Kinds.Num() > 0);
	return MakeItemOfKind(Id, Kinds[FMath::RandRange(0, Kinds.Num() - 1)], ItemLevel);
}

const FTDSkillDef* TD::Skill(ETDTowerType Tower, int32 Index)
{
	const int32 Row = (int32)Tower - 1;
	if (Row < 0 || Row >= UE_ARRAY_COUNT(GSkills) || Index < 0 || Index >= SkillsPerTower)
	{
		return nullptr;
	}
	return &GSkills[Row][Index];
}

FLinearColor TD::FormColor(int32 InForm)
{
	return InForm == 1 ? FLinearColor(0.25f, 1.0f, 0.8f) : (InForm == 2 ? FLinearColor(0.85f, 0.45f, 1.0f) : FLinearColor::White);
}

const FTDFormDef* TD::Form(ETDTowerType Tower, int32 InForm)
{
	const int32 Row = (int32)Tower - (int32)ETDTowerType::Harpoon;
	if (Row < 0 || Row >= UE_ARRAY_COUNT(GForms) || InForm < 1 || InForm > FormsPerTower)
	{
		return nullptr;
	}
	return &GForms[Row][InForm - 1];
}

int32 TD::SkillCost(ETDTowerType Tower, int32 Index, int32 NextRank)
{
	const FTDSkillDef* Def = Skill(Tower, Index);
	return Def ? FMath::RoundToInt(Def->BaseCost * NextRank * NextRank * SkillCostRankScale * Run().SkillCostScale()) : 0;
}

int32 TD::SkillLevelRequirement(int32 NextRank)
{
	// one more rank every 4 levels: ranks 1-7 need levels 1, 5, 9, 13, 17, 21, 25
	return 4 * NextRank - 3;
}

int32 TD::XpToNextLevel(int32 Level)
{
	const int32 Steps = Level - 1;
	const int32 LateSteps = FMath::Max(0, Level - XpLateFromLevel);
	return FMath::RoundToInt(XpBase * FMath::Pow(XpGrowth, (float)(Steps - LateSteps)) * FMath::Pow(XpGrowthLate, (float)LateSteps));
}
