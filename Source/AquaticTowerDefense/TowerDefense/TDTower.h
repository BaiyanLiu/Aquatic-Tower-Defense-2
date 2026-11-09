// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDTypes.h"
#include "TDItems.h"
#include "TDRules.h"
#include "TDTower.generated.h"

class ATDEnemy;
class UStaticMeshComponent;
class UPointLightComponent;

/** Final tower stats after level, items and skills (auras from other towers / enemies are applied on top) */
struct FTDTowerStats
{
	float Damage = 0.0f;
	float Rate = 0.0f;
	float Range = 0.0f;
	float Crit = 0.0f;
	float CritMultiplier = 2.0f;			// damage multiplier on a critical hit
	float SlowFactor = TD::BaseSlowFactor;	// jellyfish pulse
	int32 Chain = TD::BaseChainCount;		// eel
	float Splash = TD::BaseSplash;			// puffer
	float XpMult = 1.0f;
	float BountyPct = 0.0f;
	float ArmorPierce = 0.0f;
	float ProjectileSpeed = 0.0f;			// tiles per second
	float ItemChance = 0.0f;				// bonus drop chance on kills
	float GoldTrickle = 0.0f;				// pearls per second during waves
	float XpTrickle = 0.0f;					// XP per second during waves
	float SellBonus = 0.0f;

	// on-hit effects
	float PoisonPerTick = 0.0f;
	float PoisonDuration = 0.0f;
	float BleedPercent = 0.0f;
	float BleedDuration = 0.0f;
	float OnHitSlow = 0.0f;					// slow amount (0.1 = 10% slower)
	float ExtraSplash = 0.0f;				// tiles, for towers without native splash
	int32 ExtraChain = 0;					// extra targets for towers without native chain
	float StunChance = 0.0f;

	// conditional damage bonuses (items)
	float ArmoredBonus = 0.0f;				// vs enemies with armor
	float BossBonus = 0.0f;					// vs bosses and elites
	float ExecuteBonus = 0.0f;				// vs enemies below TD::ExecuteThreshold health
	float AirBonus = 0.0f;					// vs flying enemies

	// support tower auras
	float AuraRate = 0.0f;
	float AuraDamage = 0.0f;
	float AuraXpTrickle = 0.0f;
	float AuraArmor = 0.0f;
	float AuraSlow = 1.0f;
	float AuraRange = 0.0f;					// Sonar Whale: range given to attack towers, in tiles
};

/**
 *  A tower occupying one tile. Built entirely from basic shapes.
 *  Attack towers gain XP from damage and kills (up to TD::MaxTowerLevel), equip one item per slot and learn skills.
 *  Support towers (Humpback, Acid Vent) do the same but project auras and learn through XP trickle.
 *  Also used (with bGhost) as the placement preview, in which case it never attacks.
 */
UCLASS()
class ATDTower : public AActor
{
	GENERATED_BODY()

public:

	ATDTower();

	/** Builds visuals and sets up stats. Must be called right after spawning. */
	void InitTower(ETDTowerType InType, const FIntPoint& InCell, bool bInGhost);

	virtual void Tick(float DeltaTime) override;

	ETDTowerType GetTowerType() const { return Type; }
	const FTDTowerDef& GetDef() const { return TD::Tower(Type); }
	FIntPoint GetCell() const { return Cell; }
	bool IsAttackTower() const { return GetDef().bAttacks; }
	bool IsSupportTower() const { return GetDef().bSupport; }

	/** Walls don't level, hold items or learn skills */
	bool CanProgress() const { return Type != ETDTowerType::Wall; }

	/** Recomputes the stats (after the run's boons changed) */
	void RefreshStats() { RecalculateStats(); }

	/** Mastery trim: a band at the plinth's foot in the tower type's mastery rank colour (none below Bronze) */
	void UpdateMasteryTrim();

	/** Sold in the build phase it was bought in (the full-refund window) */
	bool IsInRefundWindow(int32 CurrentBuildPhaseId, bool bInBuildPhase) const { return bInBuildPhase && bPurchasedInBuildPhase && PurchasePhase == CurrentBuildPhaseId; }

	/** Free skill points (Siren's Debt) */
	void AddSkillPoints(int32 Count) { SkillPoints += FMath::Max(0, Count); }

	// ---- Stats (final values) ----

	const FTDTowerStats& GetStats() const { return Stats; }

	/** Damage including auras */
	float GetDamage() const { return Stats.Damage * FMath::Max(0.1f, 1.0f + AuraDamageUp + AuraDamageDown) * (1.0f + NeighbourBonus); }

	/** Attack speed including auras */
	float GetRate() const { return Stats.Rate * (1.0f + AuraRateUp) * (1.0f + NeighbourBonus); }
	/** Range including the Sonar Whale aura (support towers' aura radius isn't boosted) */
	float GetRange() const { return Stats.Range + AuraRangeUp; }
	float GetAuraRangeUp() const { return AuraRangeUp; }

	float GetAuraDamageUp() const { return AuraDamageUp; }
	float GetAuraDamageDown() const { return AuraDamageDown; }
	float GetAuraRateUp() const { return AuraRateUp; }

	/** Short description of the special effect with current stats */
	FString GetSpecialText() const;

	/** One-line stat summary for logs */
	FString DescribeStats() const;

	// ---- Auras (set every frame by the game mode) ----

	void ResetAuras() { AuraRateUp = 0.0f; AuraDamageUp = 0.0f; AuraDamageDown = 0.0f; AuraXpTrickle = 0.0f; AuraRangeUp = 0.0f; NeighbourBonus = 0.0f; }

	/** Kraken's Heart (unique) on a tower next to this one: +damage and +attack speed (the strongest one counts) */
	void ApplyNeighbourBonus(float Bonus) { NeighbourBonus = FMath::Max(NeighbourBonus, Bonus); }

	/** A unique legendary this tower has equipped */
	bool HasUnique(ETDUnique Unique) const { return (UniqueBits & TD::UniqueBit(Unique)) != 0; }

	/** Heart of Coral: waves cleared while equipped (saved with the tower) */
	int32 GetHeartWaves() const { return HeartWaves; }
	void SetHeartWaves(int32 Waves) { HeartWaves = FMath::Clamp(Waves, 0, TD::HeartMaxWaves); RecalculateStats(); }
	void AddHeartWave() { if (HasUnique(ETDUnique::HeartOfCoral) && HeartWaves < TD::HeartMaxWaves) { ++HeartWaves; RecalculateStats(); } }

	/** Strongest positive and strongest negative aura apply (original rule) */
	void ApplyTowerAura(float Rate, float Damage, float XpTrickle, float Range = 0.0f);
	void ApplyDamageDebuff(float Amount) { AuraDamageDown = FMath::Min(AuraDamageDown, Amount); }

	// ---- XP / levels / record ----

	int32 GetLevel() const { return Level; }
	float GetXp() const { return Xp; }
	int32 GetXpToNext() const { return TD::XpToNextLevel(Level); }
	bool IsMaxLevel() const { return Level >= TD::MaxTowerLevel; }
	float GetDamageDone() const { return DamageDone; }
	int32 GetKills() const { return Kills; }

	/** Adds XP (scaled by XP gain bonuses) and handles level-ups */
	void AddXp(float Amount);

	/** Credits damage for the record and XP */
	void OnDamageDealt(float Amount, float XpScale = 1.0f);
	void AddKill() { ++Kills; }

	/** XP and pearl trickle during waves. Returns whole pearls earned this frame. */
	int32 TickTrickle(float DeltaTime);

	// ---- Skills ----

	int32 GetSkillPoints() const { return SkillPoints; }
	int32 GetSkillRank(int32 Index) const { return SkillRanks.IsValidIndex(Index) ? SkillRanks[Index] : 0; }

	/** Checks rank cap, level and skill point requirements (pearls are checked by the game mode) */
	bool CanLearnSkill(int32 Index, FString* OutReason = nullptr) const;
	void LearnSkill(int32 Index, int32 PaidCost);

	// ---- Specialisation (form) ----

	/** 0 = not chosen, 1 or 2 = TD::Form(Type, Form) */
	int32 GetForm() const { return Form; }

	/** Targeting priority chosen for the tower (First by default) */
	ETDTargeting GetTargeting() const { return Targeting; }
	void SetTargeting(ETDTargeting Mode) { Targeting = (int32)Mode < TD::TargetingCount ? Mode : ETDTargeting::First; }

	/** The priority it really uses: the Deep Lobber puffer always lobs at the toughest creature */
	ETDTargeting GetEffectiveTargeting() const { return (Type == ETDTowerType::Puffer && Form == 2) ? ETDTargeting::Strongest : Targeting; }

	/** Attack towers that aim at one creature choose a priority (walls, aura towers, the jellyfish pulse and the Deep Lobber don't) */
	bool CanChooseTargeting() const { return IsAttackTower() && Type != ETDTowerType::Jelly && !(Type == ETDTowerType::Puffer && Form == 2); }

	/** Every tower but the wall specialises, once, from TD::FormLevel */
	bool CanSpecialise() const { return !bGhost && CanProgress() && TD::Form(Type, 1) != nullptr; }
	bool CanChooseForm(FString* OutReason = nullptr) const;
	void ChooseForm(int32 InForm);

	/** Swimming and crawling, or crawling only (the Spearer mantis shrimp learns to hit flyers) */
	bool HitsFlying() const;

	// ---- Items ----

	const FTDItem& GetEquipped(ETDItemSlot Slot) const { return Equipped[(int32)Slot]; }
	int32 GetEquippedCount() const;

	/** Equips an item into its slot and returns whatever was there before (may be invalid) */
	FTDItem Equip(const FTDItem& Item);

	/** Removes and returns the item in a slot (may be invalid) */
	FTDItem Unequip(ETDItemSlot Slot);

	// ---- Economy ----

	void SetPurchaseInfo(int32 Cost, int32 BuildPhaseId, bool bBoughtInBuildPhase);

	/** Refund for selling right now */
	int32 GetSellValue(int32 CurrentBuildPhaseId, bool bInBuildPhase) const;

	int32 GetTotalSpent() const { return TotalSpent; }
	int32 GetSkillSpent() const { return SkillSpent; }

	/** Restores progress from a save game */
	void RestoreState(int32 InLevel, float InXp, int32 InSkillPoints, const TArray<int32>& InRanks, const TArray<FTDItem>& InItems,
		int32 InTotalSpent, int32 InSkillSpent, float InDamageDone, int32 InKills, int32 InForm = 0);

	/**
	 *  Walls are reef rocks that join up with neighboring walls.
	 *  Bits: 1 = next column, 2 = previous column, 4 = next row, 8 = previous row,
	 *  then the diagonals: 16 = next column and next row, 32 = next column and previous row,
	 *  64 = previous column and next row, 128 = previous column and previous row.
	 */
	void SetWallLinks(uint8 Mask);
	uint8 GetWallLinks() const { return WallLinks; }

protected:

	void BuildVisuals();

	/** Builds the creature (or harpoon gun) on top of the platform: the base model, or its form's model once a form is chosen */
	void BuildTowerModel();

	/** Swaps the model for the chosen form's (kept if it already shows that form) */
	void RebuildTowerModel();
	void UpdateLevelPips();

	/** Glowing band and gem in the form's colour, so a specialised tower reads at a glance */
	void UpdateFormBadge();
	void RecalculateStats();

	/** Builds a hit with crit rolled and per-tower effects applied */
	FTDHit MakeHit(float DamageScale = 1.0f) const;

	/** Picks the creature in range by the tower's targeting priority (optionally skipping some). Secondary targets
	 *  (Swordfish's second harpoon, the Octopus volley) follow the same priority. */
	ATDEnemy* FindTarget(bool bAllowFlying, const TArray<ATDEnemy*>* Exclude = nullptr) const;

	void FireProjectile(ATDEnemy* Target, int32 Pierce, float DamageScale = 1.0f);
	void FireHarpoon(ATDEnemy* Target);
	void FireSawfish(ATDEnemy* Target);
	void FireJelly();
	void FirePuffer(ATDEnemy* Target);
	void FireEel(ATDEnemy* Target);
	void FireOctopus(ATDEnemy* Target);
	void FireMantis(ATDEnemy* Target);

	ETDTowerType Type = ETDTowerType::Wall;
	FIntPoint Cell = FIntPoint(-1, -1);
	bool bGhost = false;

	int32 Level = 1;
	float Xp = 0.0f;
	int32 SkillPoints = 1;
	TArray<int32> SkillRanks;
	int32 Form = 0;

	/** Moon Jelly: counts pulses, every third one stuns */
	int32 PulseCount = 0;
	TArray<FTDItem> Equipped;
	FTDTowerStats Stats;

	float DamageDone = 0.0f;
	int32 Kills = 0;
	float GoldAccumulator = 0.0f;

	float AuraRateUp = 0.0f;
	float AuraDamageUp = 0.0f;
	float AuraDamageDown = 0.0f;
	float AuraXpTrickle = 0.0f;
	float AuraRangeUp = 0.0f;

	int32 TotalSpent = 0;
	int32 SkillSpent = 0;
	int32 PurchasePhase = -1;
	bool bPurchasedInBuildPhase = false;

	float Cooldown = 0.0f;
	float AnimTime = 0.0f;
	float FireKick = 0.0f;

	/** Time until a support tower's next aura pulse */
	float PulseCooldown = 0.0f;

	/** Rest position of the animated part */
	FVector AnimBase = FVector::ZeroVector;

	/** Pivot the model is built on, so a form can swap the whole model; and the form it was built for */
	UPROPERTY()
	TObjectPtr<USceneComponent> ModelBase;
	int32 ModelForm = 0;

	/** Rotating / bobbing / puffing part of the tower */
	UPROPERTY()
	TObjectPtr<USceneComponent> AnimPivot;

	/** Spinning detail (orbiting cleaner fish, drifting shell plates) */
	UPROPERTY()
	TObjectPtr<USceneComponent> Spinner;

	/** Projectile spawn point */
	UPROPERTY()
	TObjectPtr<USceneComponent> Muzzle;

	/** Fish tail that wags */
	UPROPERTY()
	TObjectPtr<USceneComponent> Tail;

	/** Octopus arms that take turns throwing, and how far through its throw each one is */
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Throwers;
	TArray<float> ThrowKicks;
	int32 NextThrower = 0;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Light;

	/** One mesh of the pips earned so far */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> LevelPips;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> FormBadge;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> MasteryTrim;

	ETDTargeting Targeting = ETDTargeting::First;

	// unique legendaries
	uint32 UniqueBits = 0;
	int32 HeartWaves = 0;
	int32 AttackCount = 0;
	float NeighbourBonus = 0.0f;
	/** Trident of the Deep King: the extra strikes on the 2 nearest other creatures in range */
	void TridentStrike(ATDEnemy* Target);

	/** Idle loops ease towards a smaller sway while the tower is firing */
	float IdleBlend = 1.0f;
	int32 ShownPips = 0;

	/** Parts that sway on their own: jelly tentacles, shrimp eyes, fins, flippers and tube worms */
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Tentacles;

	/** Mantis shrimp clubs that whip forward on a punch */
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Clubs;

	/** Sawfish snout that slashes when it fires */
	UPROPERTY()
	TObjectPtr<USceneComponent> Blade;

	/** Acid vent smoke puffs and where they start */
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Puffs;
	TArray<FVector> PuffBases;

	/** Wall pieces, rebuilt when neighboring walls change */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> WallParts;

	uint8 WallLinks = 0xFF;
};
