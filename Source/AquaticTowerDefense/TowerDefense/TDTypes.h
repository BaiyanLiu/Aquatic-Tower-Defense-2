// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Tower archetypes, in hotkey order (1-9, 0, -) */
enum class ETDTowerType : uint8
{
	Wall,
	Harpoon,		// long range (original "Harpoon Launcher")
	Jelly,			// slow
	Puffer,			// splash
	Eel,			// chain
	Mantis,			// close-range heavy punch that stuns
	Octopus,		// very fast, low damage
	Lionfish,		// poison
	Sawfish,		// bleed
	Humpback,		// aura: attack speed / damage to towers
	AcidVent,		// aura: armor reduction / slow to enemies
	Count
};

/** Enemy archetypes */
enum class ETDEnemyType : uint8
{
	Crab,
	Piranha,
	Turtle,
	Manta,
	Kraken,			// boss
	SeaStar,		// splits when hurt
	Cuttlefish,		// armor aura for nearby enemies
	ShockEel,		// weakens nearby towers
	Count
};

/** Damage type of a tower, compared against the enemy's armor type */
enum class ETDDamageType : uint8
{
	Light,
	Normal,
	Heavy,
	None
};

enum class ETDArmorType : uint8
{
	Light,
	Normal,
	Heavy
};

/** Result of checking whether a tower may be built on a tile */
enum class ETDPlaceResult : uint8
{
	Ok,
	OutOfBounds,
	Occupied,
	SpawnOrGoal,
	EnemyInWay,
	BlocksPath,
	TrapsEnemy
};

/** Basic shapes every visual in the game is built from */
enum class ETDShape : uint8
{
	Cube,
	Sphere,
	Cylinder,
	Cone,
	Plane
};

/** Static tower stats (level 1) */
struct FTDTowerDef
{
	const TCHAR* Name;
	const TCHAR* Description;
	const TCHAR* Hotkey;
	int32 Cost;
	float Range;			// tiles (aura radius for support towers)
	float Damage;
	float Rate;				// shots per second
	ETDDamageType DamageType;
	float ProjectileSpeed;	// tiles per second (0 = instant / no projectile)
	bool bAttacks;
	bool bHitsFlying;
	bool bSupport;			// aura tower, no attack
};

/** Static enemy stats (before wave scaling) */
struct FTDEnemyDef
{
	const TCHAR* Name;
	float Health;
	float Speed;			// tiles per second
	int32 Reward;			// pearls
	float Armor;			// percent damage reduction at wave 1
	float ArmorGain;		// extra armor per wave
	ETDArmorType ArmorType;
	int32 LivesLost;
	bool bFlying;
	bool bBoss;
	float DropChance;		// expected item drops (values above 1 give several)
	int32 ItemLevelBonus;
	int32 Splits;			// sea star: copies spawned as it gets hurt
	const TCHAR* Description;
};

/** What a toast is, for the Toasts setting: Info (routine news), Important (big events), Refusal (why a click did nothing) */
enum class ETDToast : uint8
{
	Info,
	Important,
	Refusal
};

/** Shared constants, rules and helpers */
namespace TD
{
	constexpr int32 Cols = 18;
	constexpr int32 Rows = 13;
	constexpr float TileSize = 100.0f;
	constexpr int32 MaxTowerLevel = 30;
	constexpr int32 StartPearls = 110;
	constexpr int32 StartLives = 20;

	/** Frame rate cap choices for the settings slider (0 = unlimited) */
	inline constexpr int32 FrameRateSteps[] = { 30, 60, 90, 120, 144, 165, 240, 0 };
	constexpr int32 FrameRateStepCount = UE_ARRAY_COUNT(FrameRateSteps);
	constexpr int32 DefaultFrameRateStep = 1;	// 60 FPS

	/** Pearls for clearing wave N: base + per wave * N */
	constexpr int32 WaveClearPearlsBase = 4;
	constexpr int32 WaveClearPearlsPerWave = 1;
	constexpr int32 TowerTypeCount = (int32)ETDTowerType::Count;
	constexpr int32 EnemyTypeCount = (int32)ETDEnemyType::Count;

	// ---- maps: each has its own trench (spawn), reef (goal) and starting rocks ----
	constexpr int32 MapCount = 4;
	const TCHAR* MapName(int32 Index);
	const TCHAR* MapDescription(int32 Index);
	const TArray<FIntPoint>& MapRocks(int32 Index);
	FIntPoint MapSpawn(int32 Index);
	FIntPoint MapGoal(int32 Index);

	/** Faint per-map seabed tint (multiplies the sand, slab and open seafloor colors) */
	FLinearColor MapTint(int32 Index);

	/** False for cells of the 18 x 13 grid that a map's outline leaves out (open water: not buildable, not walkable) */
	bool IsMapCell(int32 Index, const FIntPoint& Cell);

	/** IsMapCell for the map in play */
	bool IsOnBoard(const FIntPoint& Cell);

	/** The map in play (changed on New Game and Load) */
	void SetMap(int32 Index);
	int32 GetMap();

	FIntPoint SpawnCell();
	FIntPoint GoalCell();

	/** Counts board resets (new game, restart, load) so views can react */
	int32 BoardEpoch();
	void BumpBoardEpoch();
	inline bool InBounds(const FIntPoint& C) { return C.X >= 0 && C.Y >= 0 && C.X < Cols && C.Y < Rows; }
	inline int32 CellIndex(const FIntPoint& C) { return C.Y * Cols + C.X; }

	/** Grid column grows towards the upper right of the screen, row towards the lower right (classic iso diamond) */
	FVector CellToWorld(const FIntPoint& Cell, float Z = 0.0f);
	FIntPoint WorldToCell(const FVector& World);

	/** Converts a grid direction (dx, dy) into a normalized world direction */
	FVector GridDirToWorld(const FIntPoint& Dir);

	const FTDTowerDef& Tower(ETDTowerType Type);
	const FTDEnemyDef& Enemy(ETDEnemyType Type);

	const TCHAR* DamageTypeName(ETDDamageType Type);
	const TCHAR* ArmorTypeName(ETDArmorType Type);

	/** Damage multiplier of a damage type against an armor type (same table as the original game) */
	float DamageTypeMultiplier(ETDDamageType Damage, ETDArmorType Armor);

	/** Percentage armor: each point is 1% damage reduction (negative armor increases damage) */
	inline float ArmorMultiplier(float Armor) { return 1.0f - FMath::Clamp(Armor, -100.0f, 90.0f) / 100.0f; }

	// Per-level tower stats (before items, skills and auras)
	float TowerDamage(ETDTowerType Type, int32 Level);

	/**
	 *  How far one hit's damage can stray from the tower's damage, as a fraction either way: a hit rolls evenly
	 *  between Damage x (1 - spread) and Damage x (1 + spread), so the average stays the listed damage.
	 *  Precise towers (Harpoon) are tight, chaotic ones (Puffer bombs, Eel lightning) wide.
	 */
	float TowerDamageSpread(ETDTowerType Type);

	/** One hit's damage: Unit in [0, 1] picks a point in the range (0 = lowest, 0.5 = average, 1 = highest) */
	inline float RollDamage(float Average, float Spread, float Unit) { return Average * (1.0f + Spread * (2.0f * FMath::Clamp(Unit, 0.0f, 1.0f) - 1.0f)); }
	float TowerRange(ETDTowerType Type, int32 Level);
	float TowerRate(ETDTowerType Type, int32 Level);

	/** Attack speed gained per tower level (4%: x2.16 at level 30) */
	constexpr float TowerRatePerLevel = 0.04f;

	constexpr float BaseSlowFactor = 0.5f;	// speed multiplier applied by jellyfish
	constexpr float MinSlowFactor = 0.2f;
	constexpr int32 BaseChainCount = 4;
	constexpr float BaseSplash = 1.1f;		// tiles

	/** Wave HP multiplier: (1 + 0.2 N + 0.016 N^2 + WaveHealthCubic N^3) x WavePressure x EndlessHealthFactor, N = wave - 1
	 *  (grows much faster past the final wave, in endless mode) */
	float WaveHealthMultiplier(int32 Wave);

	/** Cubic term of the wave HP curve: barely changes early waves and makes mid and late waves harder
	 *  (0.0001: wave 30 +12%, wave 50 +24%, wave 99 +56%) */
	constexpr float WaveHealthCubic = 0.0001f;

	/** Towers (levels, items, skills) outgrow the polynomial curve, so health also compounds per wave:
	 *  x WavePressureMid per wave after wave WavePressureMidFrom, and x WavePressureLate more per wave after
	 *  WavePressureLateFrom, held at the final wave's value in endless mode. The Abyssal Kraken leaves it out, and
	 *  flyers leave out the late part: they skip the maze, so the board has far less to spare against them.
	 *  (1.008 / 1.038 from 38: wave 30 x1.2, wave 50 x2.15, wave 80 x8.4, wave 100 x21) */
	constexpr float WavePressureMid = 1.008f;
	constexpr int32 WavePressureMidFrom = 10;
	constexpr float WavePressureLate = 1.038f;
	constexpr int32 WavePressureLateFrom = 38;
	float WavePressure(int32 Wave);
	float WavePressureLatePart(int32 Wave);

	// ---- the final wave and endless mode ----

	/** Beating this wave wins the game; the player may then continue in endless mode */
	constexpr int32 FinalWave = 100;

	/** Waves that bring the Abyssal Kraken: the final wave, then every 50th endless wave */
	inline bool IsTitanWave(int32 Wave) { return Wave == FinalWave || (Wave > FinalWave && Wave % 50 == 0); }

	/** Waves past the final one (0 before endless mode) */
	inline int32 EndlessWaves(int32 Wave) { return FMath::Max(0, Wave - FinalWave); }

	/** Extra HP factor on top of the normal curve: +8% compounding per endless wave */
	float EndlessHealthFactor(int32 Wave);

	/** Speed multiplier for endless creatures: +1.5% per endless wave, up to +75% */
	float EndlessSpeedMultiplier(int32 Wave);

	/** Extra armor for endless creatures: +0.5 per endless wave on top of the normal gain */
	float EndlessArmorBonus(int32 Wave);

	/** Chance of each fast / shielded roll on a regular creature: ModifierChanceBase + ModifierChancePerWave x wave from wave 6,
	 *  up to ModifierChanceCap by wave 37, then the late climb below (and to 75% in endless mode). Abyss 1+ adds TD::Run().ModifierChanceBonus()
	 *  on top, caps included. */
	constexpr float ModifierChanceBase = 0.08f;
	constexpr float ModifierChancePerWave = 0.006f;
	constexpr float ModifierChanceCap = 0.3f;
	/** Past ModifierChanceLateFrom the chance climbs again by ModifierChanceLatePerWave a wave, above the cap, up to the
	 *  final wave (0.3 at wave 50, 0.45 at wave 100), so the last stretch brings more fast and shielded creatures */
	constexpr int32 ModifierChanceLateFrom = 50;
	constexpr float ModifierChanceLatePerWave = 0.003f;
	float ModifierChance(int32 Wave);

	// Shape helpers
	UMaterialInstanceDynamic* MakeColorMaterial(UObject* Outer, const FLinearColor& Color);

	/**
	 *  One shared material instance per (quantized) color. Shapes with the same mesh and material can be
	 *  drawn as instances by the renderer, so sharing these cuts draw calls dramatically. Never modify them.
	 */
	UMaterialInterface* SharedColorMaterial(const FLinearColor& Color);

	/** Creates, attaches and registers a basic shape. Size is in unreal units (shapes are 100uu). */
	UStaticMeshComponent* AddShape(AActor* Owner, USceneComponent* Parent, ETDShape Shape, const FVector& Location, const FVector& Size,
		const FLinearColor& Color, const FRotator& Rotation = FRotator::ZeroRotator, UMaterialInterface* SharedMaterial = nullptr);

	/** Recolors a shape created by AddShape */
	void SetShapeColor(UStaticMeshComponent* Shape, const FLinearColor& Color);

	/** Creates and registers an empty scene component used as an animation pivot */
	USceneComponent* AddPivot(AActor* Owner, USceneComponent* Parent, const FVector& Location);

	// ---- motion: the camera's zoom and the creature death animation ----

	/** The camera's ortho width this frame; zoomed far out (past IdleLodViewWidth, where models drop to their lowest LOD) towers skip idle loops */
	void SetViewWidth(float Width);
	float ViewWidth();
	constexpr float IdleLodViewWidth = 2000.0f;

	/** Creature death: crawlers roll over and sink into the seabed, swimmers spiral down, over DeathSeconds */
	constexpr float DeathSeconds = 0.8f;
	struct FTDDeathPose
	{
		float Sink = 0.0f;		// uu below where it died
		float Drop = 0.0f;		// share of a swimmer's height above the floor lost so far
		float Roll = 0.0f;		// degrees onto its side / back
		float Spin = 0.0f;		// degrees of spiral (swimmers)
	};
	/** Pose at Alpha (0..1 through the animation) */
	FTDDeathPose DeathPose(float Alpha, bool bSwimming);

	/** Only important toasts and refusals show; routine news (wave released or sent early, common finds, the startup hint) stays quiet */
	inline bool ShouldShowToast(ETDToast Kind) { return Kind != ETDToast::Info; }

	/** A hit this share of max health or more makes a creature flinch */
	constexpr float FlinchHealthShare = 0.15f;
	constexpr float FlinchSeconds = 0.15f;

	/** Icon color used for an enemy type in the wave preview */
	FLinearColor EnemyIconColor(ETDEnemyType Type);
}
