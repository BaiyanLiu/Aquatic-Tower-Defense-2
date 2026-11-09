// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDTypes.h"
#include "TDModels.h"
#include "TDRun.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

namespace
{
	const FTDTowerDef GTowers[] =
	{
		// Name					Description											Key			Cost	Range	Damage	Rate	Damage type				Proj	Attacks	Flying	Support
		{ TEXT("Coral Wall"),		TEXT("Cheap blocker. Shapes the maze, never attacks."),	TEXT("1"),	4,		0.0f,	0.0f,	0.0f,	ETDDamageType::None,	0.0f,	false,	false,	false },
		{ TEXT("Harpoon"),			TEXT("Long-range homing harpoon. Hits swimming."),		TEXT("2"),	22,		5.0f,	22.0f,	0.55f,	ETDDamageType::Normal,	12.0f,	true,	true,	false },
		{ TEXT("Jellyfish"),			TEXT("Pulse hits everything in range and slows it."),	TEXT("3"),	28,		2.3f,	3.0f,	1.1f,	ETDDamageType::Light,	0.0f,	true,	true,	false },
		{ TEXT("Puffer Mortar"),		TEXT("Lobbed bomb with splash. Crawling only."),			TEXT("4"),	40,		3.8f,	22.0f,	0.55f,	ETDDamageType::Heavy,	4.5f,	true,	false,	false },
		{ TEXT("Electric Eel"),		TEXT("Chain lightning that ignores armor."),			TEXT("5"),	65,		3.0f,	16.0f,	0.8f,	ETDDamageType::Light,	0.0f,	true,	true,	false },
		{ TEXT("Mantis Shrimp"),		TEXT("Instant heavy punch that stuns. Short range, crawling only."),	TEXT("6"),	30,		1.6f,	34.0f,	0.5f,	ETDDamageType::Heavy,	0.0f,	true,	false,	false },
		{ TEXT("Octopus"),		TEXT("Two arms take turns throwing light stones, very fast. Hits swimming."),			TEXT("7"),	15,		2.2f,	3.0f,	4.0f,	ETDDamageType::Light,	10.0f,	true,	true,	false },
		{ TEXT("Lionfish"),		TEXT("Venom spines poison targets: damage every half second."),	TEXT("8"),	20,		2.6f,	3.0f,	1.0f,	ETDDamageType::Normal,	7.0f,	true,	true,	false },
		{ TEXT("Sawfish"),		TEXT("Thrown teeth bleed targets for a share of their health."),	TEXT("9"),	30,		2.2f,	6.0f,	1.0f,	ETDDamageType::Light,	8.0f,	true,	true,	false },
		{ TEXT("Humpback"),			TEXT("Aura: whale song makes nearby towers attack faster."),				TEXT("0"),	30,		2.5f,	0.0f,	0.0f,	ETDDamageType::None,	0.0f,	false,	false,	true },
		{ TEXT("Acid Vent"),		TEXT("Aura: acid plume lowers armor of nearby enemies."),			TEXT("-"),	40,		2.0f,	0.0f,	0.0f,	ETDDamageType::None,	0.0f,	false,	false,	true },
	};
	static_assert(UE_ARRAY_COUNT(GTowers) == (int32)ETDTowerType::Count, "Tower table out of sync");

	const FTDEnemyDef GEnemies[] =
	{
		// Name				Hp		Speed	Reward	Armor	Gain	Armor type				Lives	Flying	Boss	Drop	ItemLv	Splits	Description
		{ TEXT("Crab"),			28.0f,	1.15f,	2,		5.0f,	0.3f,	ETDArmorType::Normal,	1,		false,	false,	0.005f,	0,		0,		TEXT("Common scuttler.") },
		{ TEXT("Piranha"),		16.0f,	2.1f,	1,		0.0f,	0.1f,	ETDArmorType::Light,	1,		false,	false,	0.0025f,	0,		0,		TEXT("Fast and fragile.") },
		{ TEXT("Sea Turtle"),	110.0f,	0.65f,	5,		25.0f,	0.5f,	ETDArmorType::Heavy,	2,		false,	false,	0.015f,	0,		0,		TEXT("Slow, heavily armored.") },
		{ TEXT("Manta"),			40.0f,	1.35f,	3,		5.0f,	0.2f,	ETDArmorType::Light,	1,		true,	false,	0.01f,	0,		0,		TEXT("Swims straight over walls.") },
		{ TEXT("Kraken"),		800.0f,	0.45f,	40,		30.0f,	1.0f,	ETDArmorType::Heavy,	5,		false,	true,	1.5f,	5,		0,		TEXT("Boss. Resists slows, stuns and bleed. Drops several high-level items.") },
		{ TEXT("Sea Star"),		60.0f,	1.0f,	3,		5.0f,	0.5f,	ETDArmorType::Normal,	1,		false,	false,	0.00625f,	0,		2,		TEXT("Splits into faster copies when hurt.") },
		{ TEXT("Cuttlefish"),	70.0f,	1.0f,	4,		10.0f,	0.5f,	ETDArmorType::Normal,	1,		false,	false,	0.0075f,	0,		0,		TEXT("Aura: +armor to nearby creatures.") },
		{ TEXT("Shock Eel"),		70.0f,	1.0f,	4,		10.0f,	0.5f,	ETDArmorType::Normal,	1,		false,	false,	0.0075f,	0,		0,		TEXT("Aura: nearby towers deal 10% less damage.") },
	};
	static_assert(UE_ARRAY_COUNT(GEnemies) == (int32)ETDEnemyType::Count, "Enemy table out of sync");

	/**
	 *  Round shapes come from TDModels with only the sides their size shows. Cubes and planes stay the engine's:
	 *  those are the floor and slabs, whose distance fields feed the ambient light.
	 */
	UStaticMesh* GetShapeMesh(ETDShape Shape, float Size)
	{
		if (Shape == ETDShape::Sphere || Shape == ETDShape::Cylinder || Shape == ETDShape::Cone)
		{
			return TD::UnitShapeMesh(Shape, Size);
		}
		static TStrongObjectPtr<UStaticMesh> Cube(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		static TStrongObjectPtr<UStaticMesh> Plane(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
		return Shape == ETDShape::Plane ? Plane.Get() : Cube.Get();
	}

	UMaterialInterface* GetBaseMaterial()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}

	const FName ColorParam(TEXT("Color"));
}

namespace
{
	struct FTDMapDef
	{
		const TCHAR* Name;
		const TCHAR* Description;
		FIntPoint Spawn;
		FIntPoint Goal;
		TArray<FIntPoint> Rocks;
		/** Seabed tint multiplier: faint, so each map has its own mood without fighting the towers for attention */
		FLinearColor Tint;
		/** Board outline, one string per row ('#' = seabed, '-' = open water off the board); empty = full rectangle */
		TArray<const TCHAR*> Shape;
	};

	const TArray<FTDMapDef>& Maps()
	{
		static const TArray<FTDMapDef> Defs =
		{
			{ TEXT("Reef Channel"), TEXT("Straight across: trench on the left, reef on the right."), FIntPoint(0, 6), FIntPoint(17, 6),
				{ FIntPoint(4, 2), FIntPoint(4, 3), FIntPoint(5, 10), FIntPoint(8, 9), FIntPoint(9, 9), FIntPoint(12, 3),
				  FIntPoint(13, 3), FIntPoint(14, 10), FIntPoint(15, 1), FIntPoint(2, 11), FIntPoint(10, 0) },
				// warm coral sand
				FLinearColor(1.14f, 0.97f, 0.88f), {} },
			{ TEXT("Twin Trench"), TEXT("Corner to corner along a diagonal shelf: the longest natural path."), FIntPoint(0, 1), FIntPoint(17, 11),
				{ FIntPoint(3, 3), FIntPoint(4, 3), FIntPoint(6, 8), FIntPoint(7, 8), FIntPoint(9, 4), FIntPoint(10, 5),
				  FIntPoint(12, 9), FIntPoint(13, 9), FIntPoint(15, 4), FIntPoint(2, 9), FIntPoint(11, 1), FIntPoint(14, 11) },
				// cold deep blue; the far corners drop off into open water
				FLinearColor(0.86f, 0.94f, 1.16f),
				{ TEXT("############------"),
				  TEXT("##############----"),
				  TEXT("###############---"),
				  TEXT("################--"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("-#################"),
				  TEXT("---###############"),
				  TEXT("----##############"),
				  TEXT("------############") } },
			{ TEXT("Kelp Crossing"), TEXT("Top to bottom across the short side: build wide."), FIntPoint(8, 0), FIntPoint(9, 12),
				{ FIntPoint(3, 4), FIntPoint(4, 4), FIntPoint(13, 4), FIntPoint(14, 4), FIntPoint(6, 7), FIntPoint(11, 7),
				  FIntPoint(8, 9), FIntPoint(2, 10), FIntPoint(15, 10), FIntPoint(5, 1), FIntPoint(12, 1) },
				// kelp green; a rounded clearing with the corners overgrown
				FLinearColor(0.88f, 1.06f, 0.84f),
				{ TEXT("----##########----"),
				  TEXT("--##############--"),
				  TEXT("-################-"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("-################-"),
				  TEXT("--##############--"),
				  TEXT("----##########----") } },
			{ TEXT("Sunken Ruins"), TEXT("Two stone pillars split the middle; the edges have crumbled away."), FIntPoint(0, 10), FIntPoint(17, 2),
				{ FIntPoint(6, 5), FIntPoint(6, 6), FIntPoint(6, 7), FIntPoint(11, 5), FIntPoint(11, 6), FIntPoint(11, 7),
				  FIntPoint(3, 2), FIntPoint(14, 10), FIntPoint(9, 1), FIntPoint(8, 11), FIntPoint(15, 1), FIntPoint(2, 11) },
				// dusky violet stone; broken edges
				FLinearColor(1.08f, 0.9f, 1.1f),
				{ TEXT("--###---##########"),
				  TEXT("-####---##########"),
				  TEXT("######-###########"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("##################"),
				  TEXT("###########-######"),
				  TEXT("##########---####-"),
				  TEXT("##########---###--") } },
		};
		return Defs;
	}

	int32 GCurrentMap = 0;
	int32 GBoardEpoch = 0;

	const FTDMapDef& MapDef(int32 Index)
	{
		return Maps()[FMath::Clamp(Index, 0, Maps().Num() - 1)];
	}
}

const TCHAR* TD::MapName(int32 Index) { return MapDef(Index).Name; }
const TCHAR* TD::MapDescription(int32 Index) { return MapDef(Index).Description; }
const TArray<FIntPoint>& TD::MapRocks(int32 Index) { return MapDef(Index).Rocks; }
FIntPoint TD::MapSpawn(int32 Index) { return MapDef(Index).Spawn; }
FIntPoint TD::MapGoal(int32 Index) { return MapDef(Index).Goal; }
FLinearColor TD::MapTint(int32 Index) { return MapDef(Index).Tint; }

bool TD::IsMapCell(int32 Index, const FIntPoint& Cell)
{
	if (!InBounds(Cell))
	{
		return false;
	}
	const TArray<const TCHAR*>& Shape = MapDef(Index).Shape;
	if (!Shape.IsValidIndex(Cell.Y) || !Shape[Cell.Y])
	{
		return true;
	}
	const TCHAR* Row = Shape[Cell.Y];
	return FCString::Strlen(Row) <= Cell.X || Row[Cell.X] != TEXT('-');
}

bool TD::IsOnBoard(const FIntPoint& Cell) { return IsMapCell(GCurrentMap, Cell); }
void TD::SetMap(int32 Index) { GCurrentMap = FMath::Clamp(Index, 0, MapCount - 1); }
int32 TD::GetMap() { return GCurrentMap; }
FIntPoint TD::SpawnCell() { return MapDef(GCurrentMap).Spawn; }
int32 TD::BoardEpoch() { return GBoardEpoch; }
void TD::BumpBoardEpoch() { ++GBoardEpoch; }
FIntPoint TD::GoalCell() { return MapDef(GCurrentMap).Goal; }

FVector TD::CellToWorld(const FIntPoint& Cell, float Z)
{
	// board is centered on the world origin
	const float X = (Rows * 0.5f - Cell.Y - 0.5f) * TileSize;
	const float Y = (Cell.X - Cols * 0.5f + 0.5f) * TileSize;
	return FVector(X, Y, Z);
}

FIntPoint TD::WorldToCell(const FVector& World)
{
	const int32 Col = FMath::FloorToInt(World.Y / TileSize + Cols * 0.5f);
	const int32 Row = FMath::FloorToInt(Rows * 0.5f - World.X / TileSize);
	return FIntPoint(Col, Row);
}

FVector TD::GridDirToWorld(const FIntPoint& Dir)
{
	return FVector(-Dir.Y, Dir.X, 0.0f).GetSafeNormal();
}

const FTDTowerDef& TD::Tower(ETDTowerType Type)
{
	return GTowers[FMath::Clamp((int32)Type, 0, (int32)ETDTowerType::Count - 1)];
}

const FTDEnemyDef& TD::Enemy(ETDEnemyType Type)
{
	return GEnemies[(int32)Type];
}

float TD::TowerDamage(ETDTowerType Type, int32 Level)
{
	return Tower(Type).Damage * (1.0f + 0.10f * (Level - 1));
}

float TD::TowerDamageSpread(ETDTowerType Type)
{
	switch (Type)
	{
	case ETDTowerType::Harpoon:		return 0.10f;	// a sniper: almost every hit the same
	case ETDTowerType::Jelly:		return 0.15f;
	case ETDTowerType::Mantis:		return 0.15f;	// a trained punch
	case ETDTowerType::Lionfish:	return 0.20f;	// the venom does the steady work
	case ETDTowerType::Sawfish:		return 0.25f;
	case ETDTowerType::Octopus:		return 0.30f;	// thrown stones of any size
	case ETDTowerType::Puffer:		return 0.40f;	// bombs land anywhere
	case ETDTowerType::Eel:			return 0.50f;	// lightning is wild
	default:						return 0.0f;
	}
}

float TD::TowerRange(ETDTowerType Type, int32 Level)
{
	// levels past 20 add half as much, so the higher cap doesn't undo the range cut
	return Tower(Type).Range + 0.05f * (FMath::Min(Level, 20) - 1) + 0.025f * FMath::Max(Level - 20, 0);
}

float TD::TowerRate(ETDTowerType Type, int32 Level)
{
	return Tower(Type).Rate * (1.0f + TowerRatePerLevel * (Level - 1));
}

const TCHAR* TD::DamageTypeName(ETDDamageType Type)
{
	switch (Type)
	{
	case ETDDamageType::Light:	return TEXT("Light");
	case ETDDamageType::Normal:	return TEXT("Normal");
	case ETDDamageType::Heavy:	return TEXT("Heavy");
	default:					return TEXT("None");
	}
}

const TCHAR* TD::ArmorTypeName(ETDArmorType Type)
{
	switch (Type)
	{
	case ETDArmorType::Light:	return TEXT("Light");
	case ETDArmorType::Heavy:	return TEXT("Heavy");
	default:					return TEXT("Normal");
	}
}

float TD::DamageTypeMultiplier(ETDDamageType Damage, ETDArmorType Armor)
{
	// rows: armor Light / Normal / Heavy, columns: damage Light / Normal / Heavy
	static const float Table[3][3] =
	{
		{ 1.0f, 1.1f, 1.2f },
		{ 0.8f, 1.0f, 1.1f },
		{ 0.6f, 0.8f, 1.0f },
	};
	if (Damage == ETDDamageType::None)
	{
		return 1.0f;
	}
	return Table[(int32)Armor][(int32)Damage];
}

FLinearColor TD::EnemyIconColor(ETDEnemyType Type)
{
	switch (Type)
	{
	case ETDEnemyType::Crab:		return FLinearColor(1.0f, 0.45f, 0.15f);
	case ETDEnemyType::Piranha:		return FLinearColor(0.85f, 0.25f, 0.2f);
	case ETDEnemyType::Turtle:		return FLinearColor(0.3f, 0.6f, 0.25f);
	case ETDEnemyType::Manta:		return FLinearColor(0.35f, 0.4f, 0.55f);
	case ETDEnemyType::Kraken:		return FLinearColor(0.6f, 0.25f, 0.8f);
	case ETDEnemyType::SeaStar:		return FLinearColor(1.0f, 0.6f, 0.3f);
	case ETDEnemyType::Cuttlefish:	return FLinearColor(0.9f, 0.75f, 0.55f);
	case ETDEnemyType::ShockEel:	return FLinearColor(0.3f, 0.75f, 0.85f);
	default:						return FLinearColor::White;
	}
}

float TD::WaveHealthMultiplier(int32 Wave)
{
	// endless mode keeps the normal curve going and compounds on top of it (the factor is 1 up to the final wave)
	const float N = (float)(Wave - 1);
	return (1.0f + N * 0.2f + N * N * 0.016f + N * N * N * WaveHealthCubic) * WavePressure(Wave) * EndlessHealthFactor(Wave);
}

float TD::WavePressure(int32 Wave)
{
	const int32 W = FMath::Min(Wave, FinalWave);
	return FMath::Pow(WavePressureMid, (float)FMath::Max(0, W - WavePressureMidFrom)) * WavePressureLatePart(Wave);
}

float TD::WavePressureLatePart(int32 Wave)
{
	const int32 W = FMath::Min(Wave, FinalWave);
	return FMath::Pow(WavePressureLate, (float)FMath::Max(0, W - WavePressureLateFrom));
}

float TD::EndlessHealthFactor(int32 Wave)
{
	return FMath::Pow(1.08f, (float)FMath::Min(EndlessWaves(Wave), 800));	// capped well short of float overflow
}

float TD::EndlessSpeedMultiplier(int32 Wave)
{
	return 1.0f + FMath::Min(0.75f, 0.015f * EndlessWaves(Wave));
}

float TD::EndlessArmorBonus(int32 Wave)
{
	return 0.5f * EndlessWaves(Wave);
}

namespace
{
	float GViewWidth = 1300.0f;
}

void TD::SetViewWidth(float Width) { GViewWidth = Width; }
float TD::ViewWidth() { return GViewWidth; }

TD::FTDDeathPose TD::DeathPose(float Alpha, bool bSwimming)
{
	const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
	FTDDeathPose Pose;
	if (bSwimming)
	{
		// spiral down to the floor, then a little way into it
		Pose.Drop = FMath::InterpEaseIn(0.0f, 1.0f, FMath::Min(1.0f, A / 0.8f), 1.6f);
		Pose.Spin = 540.0f * A;
		Pose.Roll = 35.0f * FMath::Min(1.0f, A / 0.3f);
		Pose.Sink = 30.0f * FMath::SmoothStep(0.75f, 1.0f, A);
	}
	else
	{
		// roll over onto the back in the first 40%, then sink into the sand
		Pose.Roll = 160.0f * FMath::InterpEaseOut(0.0f, 1.0f, FMath::Min(1.0f, A / 0.4f), 2.0f);
		Pose.Sink = 48.0f * FMath::SmoothStep(0.25f, 1.0f, A);
	}
	return Pose;
}

float TD::ModifierChance(int32 Wave)
{
	if (Wave < 6)
	{
		return 0.0f;
	}
	const float Bonus = Run().ModifierChanceBonus();
	const float Late = ModifierChanceLatePerWave * FMath::Clamp(Wave - ModifierChanceLateFrom, 0, FinalWave - ModifierChanceLateFrom);
	const float Normal = FMath::Min(ModifierChanceCap + Bonus, ModifierChanceBase + Bonus + ModifierChancePerWave * Wave) + Late;
	return FMath::Min(0.75f + Bonus, Normal + 0.01f * EndlessWaves(Wave));
}

UMaterialInstanceDynamic* TD::MakeColorMaterial(UObject* Outer, const FLinearColor& Color)
{
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(GetBaseMaterial(), Outer);
	if (MID)
	{
		MID->SetVectorParameterValue(ColorParam, Color);
	}
	return MID;
}

UMaterialInterface* TD::SharedColorMaterial(const FLinearColor& Color)
{
	static TMap<uint64, TStrongObjectPtr<UMaterialInstanceDynamic>> Cache;

	// quantize to 1/128 so slightly different random tints share a material
	auto Q = [](float V) { return (uint64)FMath::Clamp(FMath::RoundToInt(V * 128.0f), 0, 0xFFFF); };
	const uint64 Key = Q(Color.R) | (Q(Color.G) << 16) | (Q(Color.B) << 32) | (Q(Color.A) << 48);

	if (const TStrongObjectPtr<UMaterialInstanceDynamic>* Found = Cache.Find(Key))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}

	const FLinearColor Quantized(Q(Color.R) / 128.0f, Q(Color.G) / 128.0f, Q(Color.B) / 128.0f, Q(Color.A) / 128.0f);
	UMaterialInstanceDynamic* MID = MakeColorMaterial(GetTransientPackage(), Quantized);
	Cache.Add(Key, TStrongObjectPtr<UMaterialInstanceDynamic>(MID));
	return MID;
}

UStaticMeshComponent* TD::AddShape(AActor* Owner, USceneComponent* Parent, ETDShape Shape, const FVector& Location, const FVector& Size,
	const FLinearColor& Color, const FRotator& Rotation, UMaterialInterface* SharedMaterial)
{
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
	Comp->SetStaticMesh(GetShapeMesh(Shape, Size.GetMax()));
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
	Comp->SetRelativeLocationAndRotation(Location, Rotation);
	Comp->SetRelativeScale3D(Size / 100.0f);
	// tiny details (eyes, tips, pips) don't need their own shadow draws
	Comp->SetCastShadow(Size.GetMax() >= 16.0f);
	Comp->SetMaterial(0, SharedMaterial ? SharedMaterial : SharedColorMaterial(Color));
	Comp->RegisterComponent();
	Owner->AddInstanceComponent(Comp);
	return Comp;
}

void TD::SetShapeColor(UStaticMeshComponent* Shape, const FLinearColor& Color)
{
	if (!Shape)
	{
		return;
	}

	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Shape->GetMaterial(0));
	if (!MID || MID->GetOuter() != Shape)
	{
		MID = UMaterialInstanceDynamic::Create(GetBaseMaterial(), Shape);
		Shape->SetMaterial(0, MID);
	}
	MID->SetVectorParameterValue(ColorParam, Color);
}

USceneComponent* TD::AddPivot(AActor* Owner, USceneComponent* Parent, const FVector& Location)
{
	USceneComponent* Pivot = NewObject<USceneComponent>(Owner);
	Pivot->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
	Pivot->SetRelativeLocation(Location);
	Pivot->RegisterComponent();
	Owner->AddInstanceComponent(Pivot);
	return Pivot;
}
