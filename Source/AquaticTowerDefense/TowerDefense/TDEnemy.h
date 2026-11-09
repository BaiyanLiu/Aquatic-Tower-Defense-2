// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDTypes.h"
#include "TDItems.h"
#include "TDRules.h"
#include "TDCrowd.h"
#include "TDModels.h"
#include "TDEnemy.generated.h"

class ATDGrid;
class UMaterialInterface;
class UStaticMeshComponent;
class ATDTower;

/** Active status effects, shown as colored chips above creatures */
enum class ETDStatus : uint8
{
	Slow,
	Poison,
	Bleed,
	Stun,
	ArmorUp,		// cuttlefish aura
	ArmorDown,		// acid vent aura
	Count
};

/** Wave modifier bits */
namespace TDMod
{
	constexpr uint8 Fast = 1;
	constexpr uint8 Shielded = 2;
	constexpr uint8 Elite = 4;
	constexpr uint8 Titan = 8;		// the Abyssal Kraken of the final wave (and every 50th endless wave)
}

namespace TD
{
	FLinearColor StatusColor(ETDStatus Status);
	const TCHAR* StatusName(ETDStatus Status);
	const TCHAR* StatusDescription(ETDStatus Status);
}

/**
 *  A sea creature walking (or swimming above) the board towards the reef.
 *  Ground enemies follow the grid's distance field tile by tile and re-route from their next tile.
 *  Flyers (mantas) swim in a straight line from spawn to goal, ignoring walls.
 *  Armor is a percentage reduction modified by damage type vs armor type, auras and pierce.
 */
UCLASS()
class ATDEnemy : public AActor
{
	GENERATED_BODY()

public:

	ATDEnemy();

	/** Sets up stats and visuals. Must be called right after spawning. */
	void InitEnemy(ETDEnemyType InType, float HealthMultiplier, int32 InWave, ATDGrid* InGrid);

	/** Turns this enemy into a smaller, faster copy of a splitting parent, continuing from its position */
	void InitAsSplit(const ATDEnemy* Parent);

	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	ETDEnemyType GetEnemyType() const { return Type; }
	const FTDEnemyDef& GetDef() const { return TD::Enemy(Type); }
	FString GetDisplayName() const;

	/** Wave modifiers (TDMod bits): fast and shielded creatures, and the elite mini-boss */
	void ApplyModifiers(uint8 InMods);
	bool IsElite() const { return (Mods & TDMod::Elite) != 0; }
	bool IsTitan() const { return (Mods & TDMod::Titan) != 0; }

	/** Shield of the Shielded modifier, as a share of max health (shields block bleed) */
	static constexpr float ShieldedShieldFraction = 0.5f;

	/** Abyssal Kraken: health multiplier over a regular kraken of the same wave, its shield (share of max health) and extra armor */
	static constexpr float TitanHealthFactor = 3.2f;
	static constexpr float TitanShieldFraction = 0.05f;
	static constexpr float TitanArmorBonus = 20.0f;

	/** Bleed strength against bosses and elites, and against the Abyssal Kraken (bleed takes a share of health, so it ignores how big they are) */
	static constexpr float BossBleedScale = 0.5f;
	static constexpr float TitanBleedScale = 0.25f;

	/** Scales the creature's base speed (endless mode, mutators, dev speed) */
	void ScaleSpeed(float Scale) { SpeedMultiplier *= Scale; }

	/** Pearls and item drop chance for killing this creature, before the killing tower's bonuses */
	int32 GetReward() const;
	/** Share of the reward and drop chance this creature gives (Swarm creatures give half) */
	float GetRewardScale() const { return RewardScale; }
	void SetRewardScale(float Scale) { RewardScale = Scale; }
	float GetDropChance() const;

	float GetShield() const { return Shield; }
	float GetMaxShield() const { return MaxShield; }
	bool IsFlying() const { return GetDef().bFlying; }
	bool IsAlive() const { return !bFinished && Health > 0.0f; }

	/** Killed: plays the death animation as a visual-only remnant (already out of the game's creature list), then goes */
	void StartDying();
	bool IsDying() const { return bDying; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsSlowed() const { return SlowTime > 0.0f || AuraSlow < 1.0f; }
	int32 GetWave() const { return Wave; }

	/** Base armor at this wave (without auras) */
	float GetBaseArmor() const;

	/** Armor including auras, in percent */
	float GetArmor() const { return GetBaseArmor() + ArmorAura.Total(); }

	/** Current movement speed in tiles per second */
	float GetCurrentSpeed() const;

	FIntPoint GetCurrentCell() const { return CurrentCell; }
	FIntPoint GetNextCell() const { return NextCell; }

	/** Remaining distance to the goal, in tiles (used for "closest to goal" targeting) */
	float GetGoalDistance() const;

	/** Where projectiles should aim */
	FVector GetAimLocation() const;

	/** Height above the actor for the health bar */
	float GetBarHeight() const;

	/** Active statuses for UI chips */
	void GetStatuses(TArray<ETDStatus>& OutStatuses) const;

	/** Remaining time of a status (0 for auras / none) */
	float GetStatusTime(ETDStatus Status) const;

	// ---- Combat ----

	/** Applies a hit to Target, then its splash / chain extras to nearby enemies */
	static void DeliverHit(ATDEnemy* Target, const FTDHit& Hit);

	/** Applies a tower hit: armor, type multiplier, effects and stun. Credits XP to the source tower. */
	void ApplyHit(const FTDHit& Hit);

	/** Raw damage (no armor), credited to Source for XP and kill bounty. Kills at zero health. */
	void DealDamage(float Amount, ATDTower* Source, float XpScale = 1.0f);

	/** Poison: flat damage every Interval for Duration. One stack per source tower (re-applying refreshes it). */
	void ApplyPoison(float DamagePerTick, float Duration, ATDTower* Source);

	/** Bleed: Percent of current health every 0.5s for Duration. One stack per source. */
	void ApplyBleed(float Percent, float Duration, ATDTower* Source);

	void ApplyStun(float Duration);

	/** Slows movement to Factor of normal speed for Duration seconds (bosses take half the effect) */
	void ApplySlow(float Factor, float Duration);

	// ---- Auras (set every frame by the game mode) ----

	void ResetAuras() { ArmorAura.Reset(); AuraSlow = 1.0f; }
	void ApplyArmorAura(float Amount) { ArmorAura.Add(Amount); }
	void ApplySlowAura(float Factor) { AuraSlow = FMath::Min(AuraSlow, Factor); }

protected:

	void BuildVisuals();
	void TickGround(float DeltaTime);
	void TickFlying(float DeltaTime);
	void TickEffects(float DeltaTime);
	void ReachGoal();

	ETDEnemyType Type = ETDEnemyType::Crab;
	int32 Wave = 0;
	float Health = 1.0f;
	float MaxHealth = 1.0f;

	uint8 Mods = 0;
	float Shield = 0.0f;
	float MaxShield = 0.0f;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ShieldBubble;
	bool bFinished = false;

	// sea star splitting
	int32 SplitsLeft = 0;
	float SplitHealth = 0.0f;
	float SpeedMultiplier = 1.0f;
	bool bSplitCopy = false;
	float RewardScale = 1.0f;

	float SlowFactor = 1.0f;
	float SlowTime = 0.0f;
	float StunTime = 0.0f;

	FTDAuraStack ArmorAura;
	float AuraSlow = 1.0f;

	/** Poison and bleed stacks, keyed by the source tower's unique id */
	FTDDotSet Dots;
	TMap<uint64, TWeakObjectPtr<ATDTower>> DotSources;

	void AddDot(ETDDot Kind, float Amount, float Duration, ATDTower* Source);

	UPROPERTY()
	TObjectPtr<ATDGrid> Grid;

	/** Logical position on the path, without the visual lateral offset */
	FVector PathPos = FVector::ZeroVector;

	/** Where the actor goes at the end of this frame's tick */
	FVector VisualLocation = FVector::ZeroVector;
	FRotator VisualRotation = FRotator::ZeroRotator;

	FIntPoint CurrentCell = FIntPoint(-1, -1);
	FIntPoint NextCell = FIntPoint(-1, -1);
	FIntPoint MoveDir = FIntPoint(1, 0);

	/** Random sideways offset so a crowd doesn't walk in single file */
	float Lateral = 0.0f;

	float AnimTime = 0.0f;

	/** Crab scuttle, which follows the current speed */
	float WalkPhase = 0.0f;

	/** The model's pivots as plain transforms, in model order: pivot 0 is the actor, and each pivot's parent comes before it */
	TArray<int32> PivotParents;
	TArray<FTransform> PivotLocal;
	/** Each pivot relative to the actor, worked out from PivotLocal once per tick */
	TArray<FTransform> PivotInActor;

	int32 BodyPivot = 0;
	TArray<int32> LimbPivots;
	/** Which way each limb points at rest, which the kraken's sway keeps */
	TArray<float> LimbYaws;

	/** Rest position of the body pivot, which the idle animation bobs around */
	FVector BodyBase = FVector::ZeroVector;

	/** Solid parts, which live in the crowd */
	struct FSolidPart
	{
		FTDCrowdPart Crowd;
		int32 Pivot = 0;
		int32 MeshIndex = 0;
	};
	TArray<FSolidPart> SolidParts;
	/** The merged meshes of SolidParts */
	TArray<FTDMergedMesh> SolidMeshes;

	UPROPERTY()
	TObjectPtr<ATDCrowd> Crowd;

	/** See-through and glowing parts, attached straight to the root (the ground blob isn't one of them) */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ShellParts;
	TArray<int32> ShellPivots;

	/** Moves the actor and every part to where this tick left them */
	void UpdateVisuals();

	// death animation and flinch
	bool bDying = false;
	float DyingTime = 0.0f;
	FVector DeathLocation = FVector::ZeroVector;
	FRotator DeathRotation = FRotator::ZeroRotator;
	float FlinchTime = 0.0f;
	void TickDying(float DeltaTime);

	/** Abyssal Anchor (unique): world time when it may be held again */
	double AnchorReadyAt = -100.0;

	/** Black Tide Pearl (unique): its tower's poison and bleed jump to the nearest creature when this one dies */
	void SpreadBlackTide();
};
