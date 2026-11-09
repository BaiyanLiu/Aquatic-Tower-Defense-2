// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDTower.h"
#include "TDAudio.h"
#include "TDEnemy.h"
#include "TDGameMode.h"
#include "TDProjectile.h"
#include "TDFx.h"
#include "TDRules.h"
#include "TDRun.h"
#include "TDMastery.h"
#include "TDModels.h"
#include "TDModelDetail.h"
#include "AquaticTowerDefense.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"

namespace
{
	/** Ghost previews are washed out towards a pale aqua; real towers keep the meshes' own materials */
	TFunction<FLinearColor(const FLinearColor&, uint8)> GhostRecolor(bool bGhost)
	{
		if (!bGhost)
		{
			return nullptr;
		}
		return [](const FLinearColor& Color, uint8 Flags) { return FMath::Lerp(Color, FLinearColor(0.75f, 1.0f, 0.95f), 0.5f); };
	}

	constexpr float EelJumpRange = 1.9f;	// tiles

	/** Round stone platform every tower stands on */
	constexpr float PlinthHeight = 34.0f;

	// merged mesh keys
	constexpr uint64 PlatformKey = 1;
	constexpr uint64 PipsKeyBase = 10;
	constexpr uint64 FormBadgeKeyBase = 9000;	// + form (1 or 2), clear of the pips, the enemies' shadow blob (200) and the walls (10000+)
	constexpr uint64 MasteryKeyBase = 9100;		// + mastery rank (1-4)
	constexpr uint64 WallKeyBase = 10000;	// clear of the other merged keys: there are 256 link masks
	constexpr uint64 DetailedWallKeyBase = 20000;	// the walls with the detailed shapes (atd.DetailedModels)
	constexpr int32 WallVariants = 4;

	static_assert((int32)ETDModel::AcidVentBlack - (int32)ETDModel::HarpoonSwordfish + 1 == ((int32)ETDTowerType::AcidVent - (int32)ETDTowerType::Harpoon + 1) * TD::FormsPerTower,
		"two form models per tower, in tower order");

	/** The tower's model, or once it has specialised its form's (two per tower, in TD::Form order after the base models) */
	ETDModel TowerModel(ETDTowerType Type, int32 Form)
	{
		if (Form >= 1 && Form <= TD::FormsPerTower && Type >= ETDTowerType::Harpoon && Type <= ETDTowerType::AcidVent)
		{
			const int32 Index = (int32)ETDModel::HarpoonSwordfish + ((int32)Type - (int32)ETDTowerType::Harpoon) * TD::FormsPerTower + (Form - 1);
			return (ETDModel)Index;
		}
		switch (Type)
		{
		case ETDTowerType::Jelly:		return ETDModel::Jelly;
		case ETDTowerType::Puffer:		return ETDModel::Puffer;
		case ETDTowerType::Eel:			return ETDModel::Eel;
		case ETDTowerType::Mantis:		return ETDModel::Mantis;
		case ETDTowerType::Octopus:		return ETDModel::Octopus;
		case ETDTowerType::Lionfish:		return ETDModel::Lionfish;
		case ETDTowerType::Sawfish:		return ETDModel::Sawfish;
		case ETDTowerType::Humpback:	return ETDModel::Humpback;
		case ETDTowerType::AcidVent:	return ETDModel::AcidVent;
		default:						return ETDModel::Harpoon;
		}
	}

	/** Gold pips in front of a tower show its level tier */
	constexpr int32 LevelsPerPip = 4;
	constexpr int32 PipCount = TD::MaxTowerLevel / LevelsPerPip;
	constexpr float EelJumpFalloff = 0.85f;
	constexpr float JellySlowDuration = 1.3f;
	constexpr float MantisStun = 0.3f;
	constexpr float AuraPulseInterval = 3.0f;
	constexpr int32 VentMainPuffs = 5;

	// skill indices per tower type (see TDItems.cpp)
	constexpr int32 HarpoonPiercing = 0, HarpoonRapid = 1, HarpoonPoisonTip = 2;
	constexpr int32 JellyDeepSting = 0, JellyNeurotoxin = 1, JellyWideBloom = 2;
	constexpr int32 PufferBigSpines = 0, PufferShrapnel = 1, PufferConcussion = 2;
	constexpr int32 EelOvercharge = 0, EelParalyze = 1, EelConductor = 2;
	constexpr int32 MantisClubs = 0, MantisShell = 1, MantisEyes = 2;
	constexpr int32 OctopusRough = 0, OctopusArms = 1, OctopusThrow = 2;
	constexpr int32 LionfishVenom = 0, LionfishLingering = 1, LionfishForager = 2;
	constexpr int32 SawfishSerrated = 0, SawfishTeeth = 1, SawfishSnout = 2;
	constexpr int32 HumpbackMelody = 0, HumpbackChannel = 1, HumpbackSongs = 2;
	constexpr int32 VentAcid = 0, VentSilt = 1, VentGrowth = 2;
}

ATDTower::ATDTower()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ATDTower::InitTower(ETDTowerType InType, const FIntPoint& InCell, bool bInGhost)
{
	Type = InType;
	Cell = InCell;
	bGhost = bInGhost;
	Level = 1;
	Xp = 0.0f;
	SkillPoints = CanProgress() ? 1 : 0;
	SkillRanks.Init(0, TD::SkillsPerTower);
	Equipped.SetNum((int32)ETDItemSlot::Count);
	Cooldown = 0.25f;
	AnimTime = FMath::FRandRange(0.0f, 10.0f);
	PulseCooldown = FMath::FRandRange(0.0f, AuraPulseInterval);

	BuildVisuals();
	RecalculateStats();
	UpdateLevelPips();
	UpdateMasteryTrim();

	// walls have no animation or attack
	if (Type == ETDTowerType::Wall)
	{
		SetActorTickEnabled(false);
	}
}

// ---------------------------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------------------------

namespace
{
	/** Each map's stone for the tower platforms */
	struct FPlatformStyle
	{
		FLinearColor Stone;
		FLinearColor Top;
		FLinearColor Moss;
		FLinearColor MossDark;
		int32 Map = 0;
	};

	/** 0 Reef Channel: pale sandstone, 1 Twin Trench: dark basalt, 2 Kelp Crossing: coral-crusted, 3 Sunken Ruins: ancient tiles.
	 *  Abyss tiers darken it in step with the water (TD::AbyssTint and a little less light). */
	FPlatformStyle PlatformStyle(int32 Map, int32 Tier)
	{
		FPlatformStyle Style;
		Style.Map = Map;
		switch (Map)
		{
		case 0:
			Style.Stone = FLinearColor(0.62f, 0.54f, 0.4f);
			Style.Top = FLinearColor(0.74f, 0.66f, 0.5f);
			Style.Moss = FLinearColor(0.42f, 0.47f, 0.28f);
			Style.MossDark = FLinearColor(0.33f, 0.39f, 0.22f);
			break;
		case 1:
			Style.Stone = FLinearColor(0.13f, 0.14f, 0.16f);
			Style.Top = FLinearColor(0.21f, 0.22f, 0.24f);
			Style.Moss = FLinearColor(0.1f, 0.26f, 0.24f);
			Style.MossDark = FLinearColor(0.07f, 0.19f, 0.18f);
			break;
		case 2:
			Style.Stone = FLinearColor(0.33f, 0.37f, 0.34f);
			Style.Top = FLinearColor(0.43f, 0.46f, 0.42f);
			Style.Moss = FLinearColor(0.2f, 0.42f, 0.22f);
			Style.MossDark = FLinearColor(0.14f, 0.32f, 0.17f);
			break;
		default:
			Style.Stone = FLinearColor(0.4f, 0.36f, 0.44f);
			Style.Top = FLinearColor(0.52f, 0.48f, 0.56f);
			Style.Moss = FLinearColor(0.22f, 0.38f, 0.3f);
			Style.MossDark = FLinearColor(0.16f, 0.29f, 0.23f);
			break;
		}
		const FLinearColor Depth = TD::AbyssTint(Tier) * (1.0f - 0.15f * TD::AbyssDepth(Tier));
		for (FLinearColor* Color : { &Style.Stone, &Style.Top, &Style.Moss, &Style.MossDark })
		{
			*Color = FLinearColor(Color->R * Depth.R, Color->G * Depth.G, Color->B * Depth.B, 1.0f);
		}
		return Style;
	}

	/** The detailed tower platform: two courses of dressed stone blocks around the side, flagstones on top, moss and pebbles at the foot,
	 *  plus each map's own touch (coral crust, inlaid tiles) */
	void AddPlatformTexture(TArray<FTDMergedPart>& Platform, const FPlatformStyle& Style, float Shade)
	{
		FRandomStream Rand(4127);
		auto Add = [&Platform](ETDShape Shape, const FVector& At, const FVector& Size, const FLinearColor& Color, float Yaw = 0.0f, float Pitch = 0.0f)
		{
			FTDMergedPart& Part = Platform.AddDefaulted_GetRef();
			Part.Shape = Shape;
			Part.Location = At;
			Part.Size = Size;
			Part.Color = Color;
			Part.Rotation = FRotator(Pitch, Yaw, 0.0f).Quaternion();
		};
		auto Tone = [&Rand](const FLinearColor& C, float Low, float High)
		{
			const float F = Rand.FRandRange(Low, High);
			return FLinearColor(C.R * F, C.G * F * Rand.FRandRange(0.97f, 1.03f), C.B * F, 1.0f);
		};

		// side: two courses of blocks, the upper one offset by half a block
		constexpr int32 Blocks = 12;
		const float Courses[2][2] = { { 4.5f, 17.0f }, { 17.5f, 29.5f } };
		for (int32 c = 0; c < 2; ++c)
		{
			const float Bottom = Courses[c][0], Top = Courses[c][1];
			for (int32 i = 0; i < Blocks; ++i)
			{
				const float A = 2.0f * PI * (i + (c == 1 ? 0.5f : 0.0f)) / Blocks + Rand.FRandRange(-0.02f, 0.02f);
				const float Width = 2.0f * PI * 45.0f / Blocks - Rand.FRandRange(1.0f, 2.2f);
				const float Proud = Rand.FRandRange(0.0f, 0.8f);
				Add(ETDShape::Cube, FVector(FMath::Cos(A) * (44.6f + Proud), FMath::Sin(A) * (44.6f + Proud), (Bottom + Top) * 0.5f),
					FVector(3.0f, Width, Top - Bottom - Rand.FRandRange(0.4f, 1.0f)), Tone(Style.Stone, 0.82f, 1.12f), FMath::RadiansToDegrees(A));
			}
		}

		// top: flagstones in two rings around the tower's footprint
		const struct { int32 Count; float Radius; float Depth; } Rings[] = { { 10, 37.5f, 13.5f }, { 7, 23.0f, 13.0f } };
		for (const auto& Ring : Rings)
		{
			const float Offset = Rand.FRand();
			for (int32 i = 0; i < Ring.Count; ++i)
			{
				const float A = 2.0f * PI * (i + Offset) / Ring.Count;
				const float Width = 2.0f * PI * Ring.Radius / Ring.Count - Rand.FRandRange(1.3f, 2.4f);
				Add(ETDShape::Cube, FVector(FMath::Cos(A) * Ring.Radius, FMath::Sin(A) * Ring.Radius, PlinthHeight + Rand.FRandRange(0.2f, 0.6f)),
					FVector(Ring.Depth - Rand.FRandRange(0.5f, 1.5f), Width, 1.6f), Tone(Style.Top, 0.86f, 1.1f), FMath::RadiansToDegrees(A),
					Rand.FRandRange(-1.5f, 1.5f));
			}
		}

		// moss creeping over the rim and down the side
		const FLinearColor Moss = Style.Moss, MossDark = Style.MossDark;
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = 2.0f * PI * (i + Rand.FRandRange(0.0f, 0.6f)) / 4.0f;
			const FLinearColor Color = (i % 2) ? Moss : MossDark;
			Add(ETDShape::Sphere, FVector(FMath::Cos(A) * 44.0f, FMath::Sin(A) * 44.0f, PlinthHeight + 0.3f), FVector(9.0f, 15.0f, 2.6f), Color, FMath::RadiansToDegrees(A));
			Add(ETDShape::Sphere, FVector(FMath::Cos(A) * 46.6f, FMath::Sin(A) * 46.6f, PlinthHeight - 5.0f), FVector(3.0f, 11.0f, 10.0f), Color, FMath::RadiansToDegrees(A));
			Add(ETDShape::Sphere, FVector(FMath::Cos(A + 0.08f) * 47.0f, FMath::Sin(A + 0.08f) * 47.0f, PlinthHeight - 12.0f), FVector(2.6f, 6.0f, 6.0f), Color, FMath::RadiansToDegrees(A));
		}

		// pebbles and a little sand at the foot
		for (int32 i = 0; i < 14; ++i)
		{
			const float A = Rand.FRand() * 2.0f * PI;
			const float R = Rand.FRandRange(46.5f, 49.0f);
			const float Size = Rand.FRandRange(2.5f, 4.5f);
			Add(ETDShape::Sphere, FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Size * 0.3f), FVector(Size, Size * Rand.FRandRange(0.7f, 1.0f), Size * 0.6f),
				Tone(Style.Stone, 0.75f, 1.25f), Rand.FRand() * 180.0f);
		}

		if (Style.Map == 2)
		{
			// coral crust: little knobs of pink, orange and violet coral over the side and rim
			const FLinearColor Corals[] = { FLinearColor(0.95f, 0.42f, 0.5f), FLinearColor(1.0f, 0.58f, 0.28f), FLinearColor(0.62f, 0.4f, 0.9f), FLinearColor(0.95f, 0.75f, 0.6f) };
			for (int32 i = 0; i < 18; ++i)
			{
				const float A = Rand.FRand() * 2.0f * PI;
				const bool bRim = i % 3 == 0;
				const float R = bRim ? Rand.FRandRange(38.0f, 44.0f) : 46.2f;
				const float Z = bRim ? PlinthHeight + 0.8f : Rand.FRandRange(6.0f, PlinthHeight - 4.0f);
				const float Size = Rand.FRandRange(3.0f, 5.5f);
				Add(ETDShape::Sphere, FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z), FVector(Size, Size, Size * 0.8f), Corals[i % 4] * Shade, FMath::RadiansToDegrees(A));
			}
		}
		else if (Style.Map == 3)
		{
			// ancient tiles: an inlaid ring of alternating teal and gold tiles, and a thin gold band inside it
			constexpr int32 Tiles = 16;
			for (int32 i = 0; i < Tiles; ++i)
			{
				const float A = 2.0f * PI * i / Tiles;
				Add(ETDShape::Cube, FVector(FMath::Cos(A) * 31.0f, FMath::Sin(A) * 31.0f, PlinthHeight + 1.25f), FVector(5.0f, 9.0f, 0.9f),
					(i % 2 ? FLinearColor(0.2f, 0.55f, 0.55f) : FLinearColor(0.78f, 0.62f, 0.3f)) * Shade, FMath::RadiansToDegrees(A));
			}
			for (int32 i = 0; i < 24; ++i)
			{
				const float A = 2.0f * PI * i / 24;
				Add(ETDShape::Cube, FVector(FMath::Cos(A) * 25.5f, FMath::Sin(A) * 25.5f, PlinthHeight + 1.25f), FVector(1.4f, 7.0f, 0.8f), FLinearColor(0.78f, 0.62f, 0.3f) * Shade, FMath::RadiansToDegrees(A));
			}
		}
		else if (Style.Map == 1)
		{
			// basalt: a few pale mineral glints in the dark stone
			for (int32 i = 0; i < 10; ++i)
			{
				const float A = Rand.FRand() * 2.0f * PI;
				Add(ETDShape::Sphere, FVector(FMath::Cos(A) * 45.8f, FMath::Sin(A) * 45.8f, Rand.FRandRange(6.0f, PlinthHeight - 4.0f)), FVector(1.6f, 1.6f, 1.6f), FLinearColor(0.55f, 0.6f, 0.65f) * Shade);
			}
		}

		for (FTDMergedPart& Part : Platform)
		{
			Part.bDetailed = true;
		}
	}
}

void ATDTower::BuildVisuals()
{
	USceneComponent* Root = GetRootComponent();
	const bool G = bGhost;

	switch (Type)
	{
	case ETDTowerType::Wall:
	{
		SetWallLinks(0);
		break;
	}

	default:
	{
		// every tower stands on a round stone platform, and its model sits on top
		TArray<FTDMergedPart> Platform;
		auto Layer = [&Platform](float Z, float Diameter, float Height, const FLinearColor& Color)
		{
			FTDMergedPart& Part = Platform.AddDefaulted_GetRef();
			Part.Shape = ETDShape::Cylinder;
			Part.Location = FVector(0, 0, Z);
			Part.Size = FVector(Diameter, Diameter, Height);
			Part.Color = Color;
		};
		// each map has its own stone, a little darker per Abyss tier; one cached mesh per map and tier
		const int32 Map = FMath::Clamp(TD::GetMap(), 0, TD::MapCount - 1);
		const int32 Tier = FMath::Clamp(TD::Run().Tier, 0, TD::MaxAbyssTier);
		const FPlatformStyle Style = PlatformStyle(Map, Tier);
		const float Shade = 1.0f - 0.15f * TD::AbyssDepth(Tier);
		Layer(2.0f, 94.0f, 4.0f, Style.Stone);
		Layer((PlinthHeight - 3.0f) * 0.5f, 90.0f, PlinthHeight - 3.0f, Style.Stone);
		Layer(PlinthHeight - 2.0f, 93.0f, 4.0f, Style.Top);
		const uint64 StyleBits = ((uint64)(Map + 1) << 42) | ((uint64)Tier << 46);
		if (TD::UseDetailedModels())
		{
			AddPlatformTexture(Platform, Style, Shade);
			TD::AddMergedMesh(this, Root, TD::GetMergedMesh(TD::MakeMergedKey(PlatformKey | (1ull << 41) | StyleBits), Platform), !G, GhostRecolor(G));
		}
		else
		{
			TD::AddMergedMesh(this, Root, TD::GetMergedMesh(TD::MakeMergedKey(PlatformKey | StyleBits), Platform), !G, GhostRecolor(G));
		}
		BuildTowerModel();
		break;
	}
	}
}

void ATDTower::BuildTowerModel()
{
	const bool G = bGhost;
	ModelBase = TD::AddPivot(this, GetRootComponent(), FVector(0, 0, PlinthHeight));
	ModelForm = G ? 0 : Form;
	FTDModelOptions Options;
	Options.MinSize = 2.5f;
	Options.Recolor = GhostRecolor(G);
	Options.bShadows = !G;
	const FTDModelInstance Model = TD::BuildModel(this, ModelBase, TowerModel(Type, ModelForm), Options);
	AnimPivot = Model.Find(TEXT("Anim"));
	Spinner = Model.Find(TEXT("Spinner"));
	Muzzle = Model.Find(TEXT("Muzzle"));
	Tail = Model.Find(TEXT("Tail"));
	for (USceneComponent* Thrower : Model.FindAll(TEXT("Thrower")))
	{
		Throwers.Add(Thrower);
		ThrowKicks.Add(0.0f);
	}
	Blade = Model.Find(TEXT("Blade"));
	for (USceneComponent* Tentacle : Model.FindAll(TEXT("Tentacle")))
	{
		Tentacles.Add(Tentacle);
	}
	for (USceneComponent* Club : Model.FindAll(TEXT("Club")))
	{
		Clubs.Add(Club);
	}
	for (USceneComponent* Puff : Model.FindAll(TEXT("Puff")))
	{
		Puffs.Add(Puff);
		PuffBases.Add(Puff->GetRelativeLocation());
	}

	if (Type == ETDTowerType::Eel && !bGhost && AnimPivot)
	{
		Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(AnimPivot);
		Light->SetRelativeLocation(FVector(10, 0, 116));
		Light->SetIntensity(2500.0f);
		Light->SetAttenuationRadius(260.0f);
		// the Storm Eel glows cyan and the Overload Eel orange
		Light->SetLightColor(ModelForm == 1 ? FLinearColor(0.5f, 0.9f, 1.0f) : ModelForm == 2 ? FLinearColor(1.0f, 0.5f, 0.15f) : FLinearColor(1.0f, 0.95f, 0.35f));
		Light->SetCastShadows(false);
		Light->RegisterComponent();
		AddInstanceComponent(Light);
	}
	if (AnimPivot)
	{
		AnimBase = AnimPivot->GetRelativeLocation();
	}
}

void ATDTower::RebuildTowerModel()
{
	if (!ModelBase || bGhost || ModelForm == Form)
	{
		return;
	}

	// the old model's pivots and meshes (and the eel's light) all hang off ModelBase
	TArray<USceneComponent*> Old;
	ModelBase->GetChildrenComponents(true, Old);
	for (USceneComponent* Comp : Old)
	{
		if (Comp)
		{
			Comp->DestroyComponent();
		}
	}
	ModelBase->DestroyComponent();
	ModelBase = nullptr;
	AnimPivot = nullptr;
	Spinner = nullptr;
	Muzzle = nullptr;
	Tail = nullptr;
	Blade = nullptr;
	Light = nullptr;
	Throwers.Reset();
	ThrowKicks.Reset();
	NextThrower = 0;
	Tentacles.Reset();
	Clubs.Reset();
	Puffs.Reset();
	PuffBases.Reset();

	BuildTowerModel();
}

void ATDTower::SetWallLinks(uint8 Mask)
{
	if (Type != ETDTowerType::Wall || Mask == WallLinks)
	{
		return;
	}
	WallLinks = Mask;

	for (UStaticMeshComponent* Part : WallParts)
	{
		TD::DestroyMergedMesh(Part);
	}
	WallParts.Reset();

	USceneComponent* Root = GetRootComponent();
	const bool G = bGhost;
	TArray<FTDMergedPart> Parts;
	auto Shape = [&Parts](ETDShape S, const FVector& Loc, const FVector& Size, const FLinearColor& Color, const FRotator& Rot = FRotator::ZeroRotator)
	{
		FTDMergedPart& Part = Parts.AddDefaulted_GetRef();
		Part.Shape = S;
		Part.Location = Loc;
		Part.Size = Size;
		Part.Color = Color;
		Part.Rotation = Rot.Quaternion();
	};

	// grid column is world +Y, grid row is world -X
	const FVector Dirs[] = { FVector(0, 1, 0), FVector(0, -1, 0), FVector(-1, 0, 0), FVector(1, 0, 0) };
	// same order as the diagonal link bits: next column + next row, next column + previous row, and so on
	const FVector DiagonalDirs[] = { FVector(-1, 1, 0), FVector(1, 1, 0), FVector(-1, -1, 0), FVector(1, -1, 0) };

	// rounded reef rock on the seabed with a brain-coral cap, moss, barnacles and tube sponges, plus a ridge and coral towards each linked wall
	// and a lower ridge towards each diagonal one
	const FVector B = FVector::ZeroVector;
	const FLinearColor RockColors[] = { FLinearColor(0.3f, 0.25f, 0.24f), FLinearColor(0.4f, 0.32f, 0.28f), FLinearColor(0.26f, 0.23f, 0.25f), FLinearColor(0.35f, 0.3f, 0.3f) };
	const FLinearColor Moss(0.2f, 0.45f, 0.22f);
	// a few looks per link pattern, so every wall on the board shares one of a small set of meshes
	const int32 Variant = FMath::Abs(Cell.X * 3 + Cell.Y * 7) % WallVariants;	// the ghost's cell is negative
	FRandomStream Rand(Variant * 131 + (Mask & 0x0F) * 17);	// a diagonal neighbor adds its ridge without re-rolling the rest of the rock
	auto Dir =[](const FVector& V) { return FRotationMatrix::MakeFromZ(V.GetSafeNormal()).Rotator(); };
	auto Tube = [&Shape, &Dir](const FVector& A, const FVector& C, float Diameter, const FLinearColor& Color)
	{
		Shape(ETDShape::Cylinder, (A + C) * 0.5f, FVector(Diameter, Diameter, FVector::Dist(A, C)), Color, Dir(C - A));
	};
	auto Barnacle = [&Shape, &Dir](const FVector& P, float S, const FVector& D)
	{
		Shape(ETDShape::Cone, P, FVector(S, S, S * 0.8f), FLinearColor(0.86f, 0.84f, 0.76f), Dir(D));
		Shape(ETDShape::Sphere, P + D * S * 0.35f, FVector(S * 0.35f, S * 0.35f, S * 0.2f), FLinearColor(0.25f, 0.22f, 0.2f), Dir(D));
	};

	const float Rocks[4][7] = { { 0, 0, 16, 60, 58, 32, 0 }, { 3, -3, 40, 50, 48, 26, 1 }, { -14, 12, 10, 28, 26, 20, 2 }, { 16, 14, 8, 24, 22, 16, 3 } };
	for (const float* R : Rocks)
	{
		Shape(ETDShape::Sphere, B + FVector(R[0], R[1], R[2]), FVector(R[3], R[4], R[5]), RockColors[(int32)R[6]], FRotator(0, Rand.FRand() * 90.0f, 0));
	}
	Shape(ETDShape::Sphere, B + FVector(0, 0, 60), FVector(44, 44, 30), FLinearColor::FromSRGBColor(FColor(0xFF, 0x7C, 0x80)));
	for (int32 g = 0; g < 4; ++g)
	{
		Shape(ETDShape::Sphere, B + FVector(0, 0, 60 + g * 3.5f), FVector(45 - g * 7, 45 - g * 7, 10), FLinearColor::FromSRGBColor(FColor(0xE0, 0x55, 0x6A)));
	}
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = 0.3f + i / 10.0f * 2.0f * PI;
		Shape(ETDShape::Sphere, B + FVector(FMath::Cos(A) * 17, FMath::Sin(A) * 17, 66), FVector(6, 6, 5), FLinearColor::FromSRGBColor(FColor(0xFF, 0x9A, 0xA0)));
	}
	for (int32 i = 0; i < 5; ++i)
	{
		const float A = Rand.FRand() * 2.0f * PI;
		const float Z = 30 + Rand.FRand() * 10;
		Shape(ETDShape::Sphere, B + FVector(FMath::Cos(A) * 22, FMath::Sin(A) * 20, Z), FVector(16, 14, 5), Moss, Dir(FVector(FMath::Cos(A) * 0.6f, FMath::Sin(A) * 0.6f, 1)));
	}
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = Rand.FRand() * 2.0f * PI;
		const float Z = 12 + Rand.FRand() * 24;
		const float S = 6 + Rand.FRand() * 3;
		Barnacle(B + FVector(FMath::Cos(A) * 28, FMath::Sin(A) * 27, Z), S, FVector(FMath::Cos(A), FMath::Sin(A), 0.4f));
	}
	{
		const float A = Rand.FRand() * 2.0f * PI;
		for (int32 k = 0; k < 3; ++k)
		{
			const float X = FMath::Cos(A) * 18 + k * 6, Y = FMath::Sin(A) * 18 - k * 4, H = 18.0f + k * 6;
			Shape(ETDShape::Cylinder, B + FVector(X, Y, 44 + H * 0.5f), FVector(8, 8, H), FLinearColor(1.0f, 0.55f, 0.2f));
			Shape(ETDShape::Cylinder, B + FVector(X, Y, 44 + H + 0.5f), FVector(5, 5, 2), FLinearColor(0.4f, 0.15f, 0.05f));
		}
	}

	for (int32 d = 0; d < 4; ++d)
	{
		if (!(Mask & (1 << d)))
		{
			continue;
		}
		const float DX = Dirs[d].X, DY = Dirs[d].Y;
		const bool bAlongX = FMath::Abs(DX) > 0.5f;
		const float Ridge[4][6] = { { 30, 16, 40, 32, 2, 0 }, { 46, 17, 34, 30, 0, 6 }, { 38, 40, 32, 24, 1, -4 }, { 50, 36, 22, 18, 3, 3 } };
		for (const float* R : Ridge)
		{
			Shape(ETDShape::Sphere, B + FVector(DX * R[0] + DY * R[5], DY * R[0] + DX * R[5], R[1]), bAlongX ? FVector(R[2], 46, R[3]) : FVector(46, R[2], R[3]), RockColors[(int32)R[4]]);
		}
		for (int32 i = 0; i < 4; ++i)
		{
			const float R = 28 + Rand.FRand() * 22;
			const float Z = 14 + Rand.FRand() * 20;
			const float S = 5 + Rand.FRand() * 2;
			Barnacle(B + FVector(DX * R + DY * 22, DY * R + DX * 22, Z), S, FVector(DY, DX, 0.3f));
		}
		Shape(ETDShape::Sphere, B + FVector(DX * 40, DY * 40, 50), FVector(20, 18, 5), Moss);

		const float Kind = Rand.FRand();
		const float BX = 34 + Rand.FRand() * 10;
		const float BY = 34 + Rand.FRand() * 10;
		const FVector Base = B + FVector(DX * BX, DY * BY, 50);
		if (Kind < 0.4f)
		{
			// sea fan: radiating twigs in one plane
			for (int32 k = 0; k < 7; ++k)
			{
				const float T = (k / 6.0f - 0.5f) * 1.6f, H = 22 + FMath::Cos(T) * 8;
				const FVector TipPos = Base + FVector(DY * FMath::Sin(T) * H, DX * FMath::Sin(T) * H, FMath::Cos(T) * H);
				Tube(Base, TipPos, 2.4f, FLinearColor(0.85f, 0.22f, 0.6f));
				Shape(ETDShape::Sphere, TipPos, FVector(4, 4, 4), FLinearColor(1.0f, 0.4f, 0.75f));
			}
		}
		else if (Kind < 0.75f)
		{
			// staghorn with sub-branches
			const FLinearColor Col(1.0f, 0.55f, 0.2f), TipCol(1.0f, 0.72f, 0.4f);
			for (int32 k = 0; k < 3; ++k)
			{
				const float Pitch = (Rand.FRand() - 0.5f) * 50;
				const float Yaw = Rand.FRand() * 360;
				const float Roll = (Rand.FRand() - 0.5f) * 40;
				const FVector Up = FRotator(Pitch, Yaw, Roll).RotateVector(FVector::UpVector);
				const float H = 18 + Rand.FRand() * 12;
				const FVector TipPos = Base + Up * H;
				Tube(Base, TipPos, 5, Col);
				Shape(ETDShape::Sphere, TipPos, FVector(7, 7, 7), TipCol);
				const FVector Mid = Base + Up * H * 0.5f;
				const float TX = (Rand.FRand() - 0.5f) * 14;
				const float TY = (Rand.FRand() - 0.5f) * 14;
				const FVector Twig = Mid + FVector(TX, TY, 8);
				Tube(Mid, Twig, 3.5f, Col);
				Shape(ETDShape::Sphere, Twig, FVector(5, 5, 5), TipCol);
			}
		}
		else
		{
			// anemone
			Shape(ETDShape::Cylinder, Base + FVector(0, 0, 5), FVector(12, 12, 10), FLinearColor(0.55f, 0.2f, 0.55f));
			for (int32 k = 0; k < 12; ++k)
			{
				const float A = k / 12.0f * 2.0f * PI;
				Shape(ETDShape::Cone, Base + FVector(FMath::Cos(A) * 6, FMath::Sin(A) * 6, 15), FVector(3, 3, 12), FLinearColor(0.95f, 0.5f, 0.9f), Dir(FVector(FMath::Cos(A) * 0.5f, FMath::Sin(A) * 0.5f, 1)));
			}
		}
	}

	for (int32 d = 0; d < 4; ++d)
	{
		if (!(Mask & (16 << d)))
		{
			continue;
		}
		// each wall draws its half of the ridge, out to the shared tile corner about 71 units away.
		// It stays narrow enough to miss the platforms on the other two tiles at that corner.
		const FVector Out = DiagonalDirs[d].GetSafeNormal();
		const FVector Side(-Out.Y, Out.X, 0);
		const FRotator Along(0, Out.Rotation().Yaw, 0);
		const float Ridge[5][7] = { { 32, 15, 46, 30, 2, 0 }, { 52, 14, 42, 28, 0, 3 }, { 66, 13, 28, 26, 3, -2 }, { 40, 35, 34, 22, 1, 0 }, { 58, 32, 28, 18, 3, 1 } };
		for (const float* R : Ridge)
		{
			Shape(ETDShape::Sphere, B + Out * R[0] + Side * R[5] + FVector(0, 0, R[1]), FVector(R[2], 34, R[3]), RockColors[(int32)R[4]], Along);
		}
		Shape(ETDShape::Sphere, B + Out * 48 + FVector(0, 0, 45), FVector(20, 14, 4), Moss, Along);
		for (int32 i = 0; i < 2; ++i)
		{
			const float SideSign = i == 0 ? 1.0f : -1.0f;
			Barnacle(B + Out * (36 + Rand.FRand() * 24) + Side * SideSign * 14 + FVector(0, 0, 14 + Rand.FRand() * 14), 5 + Rand.FRand() * 2, Side * SideSign + FVector(0, 0, 0.3f));
		}
	}

	// the detailed look: rounded, smoother shapes plus speckled rock, pale barnacle and anemone tips
	const bool bDetailed = TD::UseDetailedModels();
	if (bDetailed)
	{
		TDModelDetail::AddDetail(Mask * WallVariants + Variant, 0.0f, Parts);
	}
	const FTDMergedMesh& Merged = TD::GetMergedMesh(TD::MakeMergedKey((bDetailed ? DetailedWallKeyBase : WallKeyBase) + Mask * WallVariants + Variant), Parts);
	if (Merged.Mesh)
	{
		WallParts.Add(TD::AddMergedMesh(this, Root, Merged, !G, GhostRecolor(G)));
	}
}

void ATDTower::UpdateLevelPips()
{
	// one pip per tier of LevelsPerPip levels along the front rim of the platform; the tower also grows a little with each level
	SetActorScale3D(FVector(1.0f + 0.005f * (Level - 1)));
	const int32 Tiers = FMath::Min(PipCount, 1 + (Level - 1) / LevelsPerPip);
	if (Type == ETDTowerType::Wall || bGhost || Tiers == ShownPips)
	{
		return;
	}
	ShownPips = Tiers;

	TArray<FTDMergedPart> Parts;
	for (int32 i = 0; i < Tiers; ++i)
	{
		const float Angle = 180.0f + (i - (PipCount - 1) * 0.5f) * 15.0f;
		FTDMergedPart& Pip = Parts.AddDefaulted_GetRef();
		Pip.Shape = ETDShape::Sphere;
		Pip.Location = FRotator(0, Angle, 0).RotateVector(FVector(40, 0, PlinthHeight + 2.0f));
		Pip.Size = FVector(9, 9, 9);
		Pip.Color = FLinearColor(1.0f, 0.8f, 0.25f);
	}
	const FTDMergedMesh& Merged = TD::GetDetailedMergedMesh(PipsKeyBase + Tiers, Parts);
	if (LevelPips)
	{
		LevelPips->SetStaticMesh(Merged.Mesh);
	}
	else
	{
		LevelPips = TD::AddMergedMesh(this, GetRootComponent(), Merged, false);
	}
}

// ---------------------------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------------------------

void ATDTower::RecalculateStats()
{
	FTDItemBonus Bonus;
	for (const FTDItem& Item : Equipped)
	{
		if (Item.IsValid())
		{
			for (const FTDAffix& Affix : Item.Affixes)
			{
				if (TD::AffixApplies(Affix.Type, Type))
				{
					Bonus.Add(Affix);
				}
			}
		}
	}

	auto R = [this](int32 Index) { return (float)GetSkillRank(Index); };
	const bool bHarpoon = Type == ETDTowerType::Harpoon;
	const bool bJelly = Type == ETDTowerType::Jelly;
	const bool bPuffer = Type == ETDTowerType::Puffer;
	const bool bEel = Type == ETDTowerType::Eel;
	const bool bMantis = Type == ETDTowerType::Mantis;
	const bool bOctopus = Type == ETDTowerType::Octopus;
	const bool bLionfish = Type == ETDTowerType::Lionfish;
	const bool bSawfish = Type == ETDTowerType::Sawfish;
	const bool bHumpback = Type == ETDTowerType::Humpback;
	const bool bVent = Type == ETDTowerType::AcidVent;

	Stats = FTDTowerStats();

	// unique legendaries equipped (Heart of Coral's count starts over once it's taken off)
	UniqueBits = 0;
	for (const FTDItem& Item : Equipped)
	{
		if (Item.IsValid() && TD::IsUnique(Item))
		{
			UniqueBits |= TD::UniqueBit(TD::ItemUnique(Item));
		}
	}
	if (!HasUnique(ETDUnique::HeartOfCoral))
	{
		HeartWaves = 0;
	}

	const float SkillDamage = (bMantis ? 0.15f * R(MantisClubs) : 0.0f) + (bOctopus ? 0.15f * R(OctopusRough) : 0.0f);
	Stats.Damage = (TD::TowerDamage(Type, Level) + Bonus.Get(ETDAffix::DamageFlat)) * (1.0f + Bonus.Get(ETDAffix::DamagePct) + SkillDamage);
	Stats.Rate = TD::TowerRate(Type, Level) * (1.0f + Bonus.Get(ETDAffix::RatePct) + (bHarpoon ? 0.15f * R(HarpoonRapid) : 0.0f));

	const float SkillRange = (bJelly ? 0.15f * R(JellyWideBloom) : 0.0f) + (bOctopus ? 0.1f * R(OctopusThrow) : 0.0f)
		+ (bSawfish ? 0.15f * R(SawfishSnout) : 0.0f) + (bHumpback ? 0.15f * R(HumpbackChannel) : 0.0f);
	Stats.Range = (Type != ETDTowerType::Wall)
		? (TD::TowerRange(Type, Level) + Bonus.Get(ETDAffix::Range) + SkillRange) * (1.0f + Bonus.Get(ETDAffix::RangePct))
		: 0.0f;

	Stats.Crit = FMath::Clamp(Bonus.Get(ETDAffix::Crit), 0.0f, 0.75f);
	Stats.CritMultiplier = 2.0f + FMath::Max(0.0f, Bonus.Get(ETDAffix::CritDamage));
	Stats.SlowFactor = bJelly ? FMath::Max(TD::MinSlowFactor, TD::BaseSlowFactor - Bonus.Get(ETDAffix::SlowStrength) - 0.04f * R(JellyDeepSting)) : TD::BaseSlowFactor;
	Stats.Chain = TD::BaseChainCount + (bEel ? FMath::RoundToInt(Bonus.Get(ETDAffix::Chain)) + GetSkillRank(EelOvercharge) : 0);
	Stats.Splash = TD::BaseSplash + (bPuffer ? Bonus.Get(ETDAffix::Splash) + 0.25f * R(PufferBigSpines) : 0.0f);
	Stats.XpMult = 1.0f + Bonus.Get(ETDAffix::XpPct) + (bMantis ? 0.2f * R(MantisEyes) : 0.0f) + (bVent ? 0.25f * R(VentGrowth) : 0.0f);
	Stats.BountyPct = Bonus.Get(ETDAffix::BountyPct);
	Stats.ArmorPierce = Bonus.Get(ETDAffix::ArmorPierce);
	Stats.ProjectileSpeed = GetDef().ProjectileSpeed;
	Stats.ItemChance = Bonus.Get(ETDAffix::ItemChance) + (bLionfish ? 0.15f * R(LionfishForager) : 0.0f);
	Stats.GoldTrickle = Bonus.Get(ETDAffix::GoldTrickle);
	Stats.XpTrickle = Bonus.Get(ETDAffix::XpTrickle);
	Stats.SellBonus = bMantis ? 0.1f * R(MantisShell) : 0.0f;

	// poison: lionfish base, harpoon "Poisoned Tip", item affix
	if (bLionfish)
	{
		Stats.PoisonPerTick = (4.0f + 0.4f * (Level - 1)) * (1.0f + 0.3f * R(LionfishVenom));
		Stats.PoisonDuration = 2.0f + R(LionfishLingering);
	}
	if (bHarpoon && R(HarpoonPoisonTip) > 0.0f)
	{
		Stats.PoisonPerTick += 3.0f * R(HarpoonPoisonTip);
		Stats.PoisonDuration = FMath::Max(Stats.PoisonDuration, 2.0f);
	}
	if (Bonus.Get(ETDAffix::Poison) > 0.0f)
	{
		Stats.PoisonPerTick += Bonus.Get(ETDAffix::Poison);
		Stats.PoisonDuration = FMath::Max(Stats.PoisonDuration, 2.0f);
	}

	// bleed: sawfish (percent of current health every 0.5s), item affix
	if (bSawfish)
	{
		Stats.BleedPercent = 1.5f + 0.1f * (Level - 1) + 0.5f * R(SawfishSerrated);
		Stats.BleedDuration = 3.0f;
	}
	if (Bonus.Get(ETDAffix::Bleed) > 0.0f)
	{
		Stats.BleedPercent += Bonus.Get(ETDAffix::Bleed);
		Stats.BleedDuration = FMath::Max(Stats.BleedDuration, 3.0f);
	}

	Stats.OnHitSlow = FMath::Min(0.6f, Bonus.Get(ETDAffix::OnHitSlow));
	Stats.ExtraSplash = bPuffer ? 0.0f : Bonus.Get(ETDAffix::Splash);
	Stats.ExtraChain = (bEel ? 0 : FMath::RoundToInt(Bonus.Get(ETDAffix::Chain))) + (bSawfish ? GetSkillRank(SawfishTeeth) : 0);
	Stats.StunChance = (bEel ? 0.06f * R(EelParalyze) : 0.0f) + (bPuffer ? 0.07f * R(PufferConcussion) : 0.0f) + (bMantis ? 1.0f : 0.0f)
		+ FMath::Max(0.0f, Bonus.Get(ETDAffix::Stun));
	Stats.ArmoredBonus = (bEel ? 0.2f * R(EelConductor) : 0.0f) + FMath::Max(0.0f, Bonus.Get(ETDAffix::ArmoredDamage));
	Stats.BossBonus = FMath::Max(0.0f, Bonus.Get(ETDAffix::BossDamage));
	Stats.ExecuteBonus = FMath::Max(0.0f, Bonus.Get(ETDAffix::Execute));
	Stats.AirBonus = FMath::Max(0.0f, Bonus.Get(ETDAffix::AirDamage));

	// specialisation (see TD::Form); behaviour changes live in MakeHit and the Fire functions
	if (Form != 0)
	{
		const bool bFirst = Form == 1;
		switch (Type)
		{
		case ETDTowerType::Harpoon:
			if (!bFirst)	// Narwhal
			{
				Stats.ArmorPierce += 20.0f;
				Stats.Rate *= 0.85f;
			}
			break;
		case ETDTowerType::Jelly:
			if (bFirst)		// Box Jelly
			{
				Stats.PoisonPerTick += 0.5f * Stats.Damage;
				Stats.PoisonDuration = FMath::Max(Stats.PoisonDuration, 2.0f);
				Stats.SlowFactor = FMath::Min(0.95f, Stats.SlowFactor + 0.1f);
			}
			else			// Moon Jelly
			{
				Stats.Rate *= 0.85f;
			}
			break;
		case ETDTowerType::Puffer:
			if (!bFirst)	// Deep Lobber
			{
				Stats.Range += 2.0f;
				Stats.Rate *= 0.8f;
			}
			break;
		case ETDTowerType::Eel:
			if (bFirst)		// Storm Eel
			{
				Stats.Damage *= 0.8f;
			}
			else			// Overload Eel
			{
				Stats.Damage *= 1.8f;
				Stats.Chain = FMath::Max(2, Stats.Chain / 2);
				Stats.StunChance = 1.0f;
			}
			break;
		case ETDTowerType::Mantis:
			if (!bFirst)	// Spearer
			{
				Stats.Range += 1.5f;
				Stats.Damage *= 1.3f;
				Stats.StunChance = FMath::Max(0.0f, Stats.StunChance - 1.0f);
			}
			break;
		case ETDTowerType::Octopus:
			if (bFirst)		// Juggler
			{
				Stats.Damage *= 0.7f;
			}
			else			// Blue-ringed
			{
				Stats.PoisonPerTick += Stats.Damage;
				Stats.PoisonDuration = FMath::Max(Stats.PoisonDuration, 2.0f);
				Stats.Rate *= 0.75f;
			}
			break;
		case ETDTowerType::Lionfish:
			if (!bFirst)	// Paralytic Venom
			{
				Stats.PoisonPerTick *= 0.85f;
			}
			break;
		case ETDTowerType::Sawfish:
			if (bFirst)		// Ripper
			{
				Stats.Rate *= 0.8f;
				Stats.BleedDuration = FMath::Max(Stats.BleedDuration, 5.0f);
			}
			break;
		default:
			break;
		}
	}

	if (bHumpback)
	{
		Stats.AuraRate = 0.10f + 0.01f * (Level - 1);
		Stats.AuraDamage = 0.05f * R(HumpbackMelody);
		Stats.AuraXpTrickle = TD::SongsXpPerRank * R(HumpbackSongs);
	}
	if (bVent)
	{
		Stats.AuraArmor = -(10.0f + 1.0f * (Level - 1) + 5.0f * R(VentAcid));
		Stats.AuraSlow = 1.0f - 0.06f * R(VentSilt);
	}

	// aura tower forms
	if (bHumpback && Form == 1)		// Sonar Whale: range instead of attack speed
	{
		Stats.AuraRange = 0.5f;
		Stats.AuraRate = 0.0f;
	}
	if (bHumpback && Form == 2)		// Chorus Whale: a wider, weaker song
	{
		Stats.Range += 1.5f;
		Stats.AuraRate *= 0.65f;
		Stats.AuraDamage *= 0.65f;
		Stats.AuraXpTrickle *= 0.65f;
	}
	if (bVent && Form == 1)			// Cold Seep: slows instead of cutting armor
	{
		Stats.AuraArmor = 0.0f;
		Stats.AuraSlow *= 0.75f;
	}
	if (bVent && Form == 2)			// Black Smoker: stronger acid, smaller plume
	{
		Stats.AuraArmor *= 1.5f;
		Stats.Range = FMath::Max(0.5f, Stats.Range - 0.5f);
	}

	// unique legendaries with stat effects
	if (HasUnique(ETDUnique::CompassOfTides) && Type != ETDTowerType::Wall)
	{
		Stats.Range += TD::CompassRange;
	}
	Stats.Damage *= 1.0f + TD::HeartOfCoralBonus(HeartWaves);

	// run boons (see TDRun.h) apply on top of everything else
	const FTDRunRules& Run = TD::Run();
	Stats.Damage *= Run.DamageScale();
	Stats.Rate *= Run.RateScale();
	Stats.Range *= Run.RangeScale();
	Stats.XpMult += Run.XpBonus();
	Stats.BossBonus += Run.BossDamageBonus();
	Stats.PoisonPerTick *= Run.DotScale();
	Stats.BleedPercent *= Run.DotScale();
	if (bEel)
	{
		Stats.Chain += Run.EelChainBonus();
	}
}

FString ATDTower::GetSpecialText() const
{
	// attack forms describe themselves; aura forms keep the live aura numbers after the form's name
	const FTDFormDef* FormDef = TD::Form(Type, Form);
	if (FormDef && !IsSupportTower())
	{
		return FString::Printf(TEXT("%s: %s"), FormDef->Name, FormDef->Line1);
	}
	if (FormDef)
	{
		FString Aura;
		if (Type == ETDTowerType::Humpback)
		{
			Aura = Stats.AuraRange > 0.0f
				? FString::Printf(TEXT("+%.1f tiles range, +%d%% damage"), Stats.AuraRange, FMath::RoundToInt(Stats.AuraDamage * 100))
				: FString::Printf(TEXT("+%d%% attack speed, +%d%% damage"), FMath::RoundToInt(Stats.AuraRate * 100), FMath::RoundToInt(Stats.AuraDamage * 100));
		}
		else
		{
			Aura = FString::Printf(TEXT("%d armor, %d%% slower"), FMath::RoundToInt(Stats.AuraArmor), FMath::RoundToInt((1.0f - Stats.AuraSlow) * 100));
		}
		return FString::Printf(TEXT("%s aura: %s"), FormDef->Name, *Aura);
	}
	switch (Type)
	{
	case ETDTowerType::Wall:		return TEXT("Blocks the path. No attack.");
	case ETDTowerType::Harpoon:		return GetSkillRank(HarpoonPiercing) > 0 ? FString::Printf(TEXT("Harpoons pass through %d extra %s"), GetSkillRank(HarpoonPiercing), GetSkillRank(HarpoonPiercing) == 1 ? TEXT("enemy") : TEXT("enemies")) : FString(TEXT("Long range, one enemy at a time"));
	case ETDTowerType::Jelly:		return FString::Printf(TEXT("Slows enemies to %d%% speed for %.1f seconds"), FMath::RoundToInt(Stats.SlowFactor * 100), JellySlowDuration);
	case ETDTowerType::Puffer:		return FString::Printf(TEXT("Splash radius: %.2f tiles"), Stats.Splash);
	case ETDTowerType::Eel:			return FString::Printf(TEXT("Chains to %d enemies, ignores armor"), Stats.Chain);
	case ETDTowerType::Mantis:		return FString::Printf(TEXT("Punch: stuns for %.1fs. Crawling only"), MantisStun);
	case ETDTowerType::Octopus:		return FString::Printf(TEXT("Throws at %d enemies per volley"), 1 + GetSkillRank(OctopusArms));
	case ETDTowerType::Lionfish:		return FString::Printf(TEXT("Poison: %.1f damage every 0.5s for %.1fs"), Stats.PoisonPerTick, Stats.PoisonDuration);
	case ETDTowerType::Sawfish:		return FString::Printf(TEXT("Bleed: %.1f%% of current health every 0.5s for %.0fs"), Stats.BleedPercent, Stats.BleedDuration);
	case ETDTowerType::Humpback:	return FString::Printf(TEXT("Aura: +%d%% attack speed, +%d%% damage"), FMath::RoundToInt(Stats.AuraRate * 100), FMath::RoundToInt(Stats.AuraDamage * 100));
	case ETDTowerType::AcidVent:	return FString::Printf(TEXT("Aura: %d armor%s"), FMath::RoundToInt(Stats.AuraArmor),
		Stats.AuraSlow < 1.0f ? *FString::Printf(TEXT(", %d%% slower"), FMath::RoundToInt((1.0f - Stats.AuraSlow) * 100)) : TEXT(""));
	default:						return FString();
	}
}

FString ATDTower::DescribeStats() const
{
	return FString::Printf(TEXT("%s Lv%d: dmg %.1f rate %.2f range %.2f crit %d%% proj %.1f poison %.1f bleed %.1f%% xp x%.2f bounty +%d%% pierce %.0f items %d dealt %.0f kills %d"),
		GetDef().Name, Level, GetDamage(), GetRate(), Stats.Range, FMath::RoundToInt(Stats.Crit * 100), Stats.ProjectileSpeed, Stats.PoisonPerTick,
		Stats.BleedPercent, Stats.XpMult, FMath::RoundToInt(Stats.BountyPct * 100), Stats.ArmorPierce, GetEquippedCount(), DamageDone, Kills);
}

// ---------------------------------------------------------------------------------------------
// Auras, XP and trickle
// ---------------------------------------------------------------------------------------------

void ATDTower::ApplyTowerAura(float Rate, float Damage, float XpTrickle, float Range)
{
	AuraRangeUp = FMath::Max(AuraRangeUp, Range);
	AuraRateUp = FMath::Max(AuraRateUp, Rate);
	AuraDamageUp = FMath::Max(AuraDamageUp, Damage);
	AuraXpTrickle = FMath::Max(AuraXpTrickle, XpTrickle);
}

void ATDTower::OnDamageDealt(float Amount, float XpScale)
{
	if (Amount <= 0.0f || bGhost)
	{
		return;
	}
	DamageDone += Amount;
	AddXp(Amount * TD::XpPerDamage * XpScale);
}

int32 ATDTower::TickTrickle(float DeltaTime)
{
	if (bGhost || !CanProgress())
	{
		return 0;
	}

	// support towers learn by being active during waves
	const float XpPerSecond = Stats.XpTrickle + AuraXpTrickle + TD::SupportXpRate(Type);
	if (XpPerSecond > 0.0f)
	{
		AddXp(XpPerSecond * DeltaTime);
	}

	GoldAccumulator += Stats.GoldTrickle * DeltaTime;
	const int32 Whole = FMath::FloorToInt(GoldAccumulator);
	GoldAccumulator -= Whole;
	return Whole;
}

void ATDTower::AddXp(float Amount)
{
	if (bGhost || !CanProgress() || IsMaxLevel() || Amount <= 0.0f)
	{
		return;
	}

	const int32 OldLevel = Level;
	Xp += Amount * Stats.XpMult;
	bool bLeveled = false;
	while (!IsMaxLevel() && Xp >= GetXpToNext())
	{
		Xp -= GetXpToNext();
		++Level;
		++SkillPoints;
		bLeveled = true;
	}
	if (IsMaxLevel())
	{
		Xp = 0.0f;
	}

	if (bLeveled)
	{
		RecalculateStats();
		UpdateLevelPips();

		const FVector Top = GetActorLocation() + FVector(0, 0, 130);
		ATDFx::SpawnRing(GetWorld(), GetActorLocation() + FVector(0, 0, 36), 70.0f, FLinearColor(1.0f, 0.82f, 0.25f));
		TDAudio::Play(this, ETDSound::LevelUp);
		if (const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
		{
			GameMode->FloatingText(Top + FVector(0, 0, 30), FString::Printf(TEXT("LEVEL %d"), Level), FLinearColor(1.0f, 0.85f, 0.3f));
			if (CanSpecialise() && Form == 0 && OldLevel < TD::FormLevel && Level >= TD::FormLevel)
			{
				GameMode->FloatingText(Top + FVector(0, 0, 70), TEXT("PICK A FORM"), FLinearColor(0.6f, 1.0f, 0.95f));
				GameMode->Toast(FString::Printf(TEXT("%s reached level %d: pick its form in the Skills tab"), GetDef().Name, TD::FormLevel), FLinearColor(0.6f, 1.0f, 0.95f), ETDToast::Important);
			}
		}
		UE_LOG(LogAquaticTowerDefense, Log, TEXT("Level up at (%d,%d): %s"), Cell.X, Cell.Y, *DescribeStats());
	}
}

// ---------------------------------------------------------------------------------------------
// Skills
// ---------------------------------------------------------------------------------------------

bool ATDTower::CanLearnSkill(int32 Index, FString* OutReason) const
{
	auto Fail = [OutReason](const FString& Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
		return false;
	};

	if (!CanProgress() || !TD::Skill(Type, Index))
	{
		return Fail(TEXT("No skills"));
	}
	const int32 NextRank = GetSkillRank(Index) + 1;
	if (NextRank > TD::MaxSkillRank)
	{
		return Fail(TEXT("Max rank"));
	}
	if (Level < TD::SkillLevelRequirement(NextRank))
	{
		return Fail(FString::Printf(TEXT("Requires level %d"), TD::SkillLevelRequirement(NextRank)));
	}
	if (SkillPoints < 1)
	{
		return Fail(TEXT("No skill points"));
	}
	return true;
}

bool ATDTower::CanChooseForm(FString* OutReason) const
{
	auto Fail = [OutReason](const FString& Reason)
	{
		if (OutReason)
		{
			*OutReason = Reason;
		}
		return false;
	};
	if (!CanSpecialise())
	{
		return Fail(TEXT("This tower has no forms"));
	}
	if (Form != 0)
	{
		return Fail(TEXT("Form already chosen"));
	}
	if (Level < TD::FormLevel)
	{
		return Fail(FString::Printf(TEXT("Needs level %d"), TD::FormLevel));
	}
	return true;
}

void ATDTower::ChooseForm(int32 InForm)
{
	if (!CanChooseForm() || !TD::Form(Type, InForm))
	{
		return;
	}
	Form = InForm;
	PulseCount = 0;
	RecalculateStats();
	RebuildTowerModel();
	UpdateFormBadge();
}

void ATDTower::UpdateFormBadge()
{
	if (bGhost || Form == 0)
	{
		if (FormBadge)
		{
			FormBadge->SetVisibility(false);
		}
		return;
	}

	// a glowing band around the platform and a gem on its back rim, in the form's colour
	const FLinearColor Color = TD::FormColor(Form);
	TArray<FTDMergedPart> Parts;
	FTDMergedPart& Band = Parts.AddDefaulted_GetRef();
	Band.Shape = ETDShape::Cylinder;
	Band.Mat = ETDPartMaterial::Glow;
	Band.Location = FVector(0, 0, PlinthHeight - 9.0f);
	Band.Size = FVector(92.0f, 92.0f, 4.0f);
	Band.Color = Color;
	FTDMergedPart& Gem = Parts.AddDefaulted_GetRef();
	Gem.Shape = ETDShape::Sphere;
	Gem.Mat = ETDPartMaterial::Glow;
	Gem.Location = FVector(-40.0f, 0.0f, PlinthHeight + 8.0f);
	Gem.Size = FVector(11.0f, 11.0f, 17.0f);
	Gem.Color = Color;

	const FTDMergedMesh& Merged = TD::GetDetailedMergedMesh(FormBadgeKeyBase + Form, Parts);
	if (FormBadge)
	{
		FormBadge->SetStaticMesh(Merged.Mesh);
		FormBadge->SetVisibility(true);
	}
	else
	{
		FormBadge = TD::AddMergedMesh(this, GetRootComponent(), Merged, false);
	}
}

void ATDTower::UpdateMasteryTrim()
{
	const int32 Rank = (bGhost || !CanProgress()) ? 0 : TD::TowerMasteryRank(Type);
	if (Rank <= 0)
	{
		if (MasteryTrim)
		{
			MasteryTrim->SetVisibility(false);
		}
		return;
	}

	// a metal band round the foot of the plinth (two from Silver up, with four studs from Gold up); Abyssal glows.
	// Low on the plinth, below the form badge's band, and in colours clear of the teal and violet forms.
	const FLinearColor Color = TD::MasteryColor(Rank);
	const ETDPartMaterial Mat = Rank >= 4 ? ETDPartMaterial::Glow : ETDPartMaterial::Lit;
	TArray<FTDMergedPart> Parts;
	auto Band = [&Parts, &Color, Mat](float Z, float Diameter, float Height)
	{
		FTDMergedPart& Part = Parts.AddDefaulted_GetRef();
		Part.Shape = ETDShape::Cylinder;
		Part.Mat = Mat;
		Part.Location = FVector(0, 0, Z);
		Part.Size = FVector(Diameter, Diameter, Height);
		Part.Color = Color;
	};
	{
		// a dark seat behind the bands, so they read on pale sandstone as well as on basalt
		FTDMergedPart& Seat = Parts.AddDefaulted_GetRef();
		Seat.Shape = ETDShape::Cylinder;
		Seat.Mat = ETDPartMaterial::Lit;
		Seat.Location = FVector(0, 0, Rank >= 2 ? 9.5f : 5.0f);
		Seat.Size = FVector(98.6f, 98.6f, Rank >= 2 ? 17.5f : 10.0f);
		Seat.Color = FLinearColor(0.04f, 0.05f, 0.06f);
	}
	Band(5.0f, 100.0f, 7.0f);
	if (Rank >= 2)
	{
		Band(15.0f, 98.0f, 4.0f);
	}
	if (Rank >= 3)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			FTDMergedPart& Stud = Parts.AddDefaulted_GetRef();
			Stud.Shape = ETDShape::Sphere;
			Stud.Mat = Mat;
			Stud.Location = FRotator(0, 45.0f + 90.0f * i, 0).RotateVector(FVector(50.0f, 0, 10.0f));
			Stud.Size = FVector(10.0f, 10.0f, 10.0f);
			Stud.Color = Rank >= 4 ? FLinearColor(0.55f, 0.75f, 1.0f) : Color;
		}
	}

	const FTDMergedMesh& Merged = TD::GetDetailedMergedMesh(MasteryKeyBase + Rank, Parts);
	if (MasteryTrim)
	{
		MasteryTrim->SetStaticMesh(Merged.Mesh);
		MasteryTrim->SetVisibility(true);
	}
	else
	{
		MasteryTrim = TD::AddMergedMesh(this, GetRootComponent(), Merged, false);
	}
}

bool ATDTower::HitsFlying() const
{
	// (Compass of Tides, a unique charm, lets any tower hit swimming creatures)
	return GetDef().bHitsFlying || (Type == ETDTowerType::Mantis && Form == 2) || HasUnique(ETDUnique::CompassOfTides);
}

void ATDTower::LearnSkill(int32 Index, int32 PaidCost)
{
	if (!CanLearnSkill(Index))
	{
		return;
	}
	++SkillRanks[Index];
	--SkillPoints;
	TotalSpent += PaidCost;
	SkillSpent += PaidCost;
	RecalculateStats();
}

// ---------------------------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------------------------

int32 ATDTower::GetEquippedCount() const
{
	int32 Count = 0;
	for (const FTDItem& Item : Equipped)
	{
		Count += Item.IsValid() ? 1 : 0;
	}
	return Count;
}

FTDItem ATDTower::Equip(const FTDItem& Item)
{
	if (!CanProgress() || !Item.IsValid())
	{
		return FTDItem();
	}
	FTDItem Previous = Equipped[(int32)Item.Slot];
	Equipped[(int32)Item.Slot] = Item;
	RecalculateStats();
	return Previous;
}

FTDItem ATDTower::Unequip(ETDItemSlot Slot)
{
	FTDItem Previous = Equipped[(int32)Slot];
	Equipped[(int32)Slot] = FTDItem();
	RecalculateStats();
	return Previous;
}

// ---------------------------------------------------------------------------------------------
// Economy
// ---------------------------------------------------------------------------------------------

void ATDTower::SetPurchaseInfo(int32 Cost, int32 BuildPhaseId, bool bBoughtInBuildPhase)
{
	TotalSpent = Cost;
	PurchasePhase = BuildPhaseId;
	bPurchasedInBuildPhase = bBoughtInBuildPhase;
}

int32 ATDTower::GetSellValue(int32 CurrentBuildPhaseId, bool bInBuildPhase) const
{
	const bool bFullRefund = IsInRefundWindow(CurrentBuildPhaseId, bInBuildPhase);
	// the refund follows the price actually paid: a wall bought before Coral Masons or Brittle Coral keeps its old value
	// (capped at the listed price, for saves from before skill spending was tracked)
	const int32 Paid = TotalSpent - SkillSpent;
	const int32 Price = Paid > 0 ? FMath::Min(Paid, GetDef().Cost) : TD::TowerCost(Type);
	// Abyss 5 and Gilded Shackles cut every refund except the full one in the build phase the tower was bought
	return TD::RunSellValue(Price, Level, Stats.SellBonus, SkillSpent, TotalSpent, bFullRefund);
}

void ATDTower::RestoreState(int32 InLevel, float InXp, int32 InSkillPoints, const TArray<int32>& InRanks, const TArray<FTDItem>& InItems,
	int32 InTotalSpent, int32 InSkillSpent, float InDamageDone, int32 InKills, int32 InForm)
{
	Level = FMath::Clamp(InLevel, 1, TD::MaxTowerLevel);
	Form = (CanSpecialise() && Level >= TD::FormLevel && TD::Form(Type, InForm)) ? InForm : 0;
	Xp = InXp;
	SkillPoints = InSkillPoints;
	for (int32 i = 0; i < SkillRanks.Num() && i < InRanks.Num(); ++i)
	{
		SkillRanks[i] = InRanks[i];
	}
	for (const FTDItem& Item : InItems)
	{
		if (Item.IsValid())
		{
			Equipped[(int32)Item.Slot] = Item;
		}
	}
	TotalSpent = InTotalSpent;
	SkillSpent = InSkillSpent;
	DamageDone = InDamageDone;
	Kills = InKills;
	bPurchasedInBuildPhase = false;
	RecalculateStats();
	UpdateLevelPips();
	RebuildTowerModel();
	UpdateFormBadge();
}

// ---------------------------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------------------------

FTDHit ATDTower::MakeHit(float DamageScale) const
{
	FTDHit Hit;
	Hit.Damage = TD::RollDamage(GetDamage(), TD::TowerDamageSpread(Type), FMath::FRand()) * DamageScale;	// average stays GetDamage()
	Hit.DamageType = GetDef().DamageType;
	Hit.ArmorPierce = Stats.ArmorPierce;
	Hit.Source = const_cast<ATDTower*>(this);
	Hit.bArcOnCrit = HasUnique(ETDUnique::TearOfMoon);
	Hit.bAnchor = HasUnique(ETDUnique::AbyssalAnchor);
	if (Stats.Crit > 0.0f && FMath::FRand() < Stats.Crit)
	{
		Hit.Damage *= Stats.CritMultiplier;
		Hit.bCrit = true;
	}

	Hit.PoisonPerTick = Stats.PoisonPerTick;
	Hit.PoisonDuration = Stats.PoisonDuration;
	Hit.BleedPercent = Stats.BleedPercent;
	Hit.BleedDuration = Stats.BleedDuration;
	if (Stats.OnHitSlow > 0.0f)
	{
		Hit.SlowFactor = 1.0f - Stats.OnHitSlow;
		Hit.SlowDuration = 1.0f;
	}
	Hit.SplashRadius = Stats.ExtraSplash;
	Hit.ChainExtra = Stats.ExtraChain;
	Hit.StunChance = Stats.StunChance;
	Hit.ArmoredBonus = Stats.ArmoredBonus;
	Hit.BossBonus = Stats.BossBonus;
	Hit.ExecuteBonus = Stats.ExecuteBonus;
	Hit.AirBonus = Stats.AirBonus;
	Hit.StunDuration = (Type == ETDTowerType::Eel) ? 0.5f : (Type == ETDTowerType::Mantis ? MantisStun : 0.6f);

	if (Type == ETDTowerType::Eel)
	{
		Hit.bIgnoreArmor = true;
		if (Form == 2)	// Overload Eel
		{
			Hit.StunDuration = 0.4f;
		}
	}
	if (Type == ETDTowerType::Lionfish && Form == 1)
	{
		// Venom Cloud: the poison spreads around the target (the cloud itself does no damage unless an item adds splash)
		Hit.SplashFraction = Hit.SplashRadius > 0.0f ? Hit.SplashFraction : 0.0f;
		Hit.SplashRadius += 1.0f;
	}
	if (Type == ETDTowerType::Lionfish && Form == 2 && Hit.PoisonDuration > 0.0f)
	{
		// Paralytic Venom: slowed for as long as the poison lasts
		Hit.SlowFactor = FMath::Min(Hit.SlowFactor, 0.75f);
		Hit.SlowDuration = FMath::Max(Hit.SlowDuration, Hit.PoisonDuration);
	}
	if (Type == ETDTowerType::Sawfish && Form == 1)
	{
		Hit.BleedingBonus = 1.0f;	// Ripper
	}
	return Hit;
}

ATDEnemy* ATDTower::FindTarget(bool bAllowFlying, const TArray<ATDEnemy*>* Exclude) const
{
	const ETDTargeting Mode = GetEffectiveTargeting();
	const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	if (!GameMode)
	{
		return nullptr;
	}

	const float RangeUU = GetRange() * TD::TileSize;
	const FVector Here = GetActorLocation();

	ATDEnemy* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (ATDEnemy* Enemy : GameMode->GetEnemies())
	{
		if (!Enemy || !Enemy->IsAlive() || (!bAllowFlying && Enemy->IsFlying()) || (Exclude && Exclude->Contains(Enemy)))
		{
			continue;
		}
		if (FVector::Dist2D(Here, Enemy->GetActorLocation()) > RangeUU)
		{
			continue;
		}
		// lower is better (TD::TargetScore); only what the priority looks at is worked out
		FTDTargetInfo Info;
		switch (Mode)
		{
		case ETDTargeting::Strongest:
		case ETDTargeting::Weakest:
			Info.Toughness = Enemy->GetHealth() + Enemy->GetShield();
			break;
		case ETDTargeting::Closest:
			Info.TowerDistance = FVector::Dist2D(Here, Enemy->GetActorLocation());
			break;
		case ETDTargeting::SwimmingFirst:
			Info.bSwimming = Enemy->IsFlying();
			Info.GoalDistance = Enemy->GetGoalDistance();
			break;
		default:
			Info.GoalDistance = Enemy->GetGoalDistance();
			break;
		}
		const float Score = TD::TargetScore(Mode, Info);
		if (Score < BestDistance)
		{
			BestDistance = Score;
			Best = Enemy;
		}
	}
	return Best;
}

void ATDTower::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AnimTime += DeltaTime;
	FireKick = FMath::Max(0.0f, FireKick - DeltaTime * 3.0f);

	// idle life: eased down while the tower is firing, and still when the camera is
	// zoomed far out (where the models drop to their lowest detail). Nothing here turns the tower's facing.
	const bool bBusy = FireKick > 0.0f;
	IdleBlend = FMath::FInterpTo(IdleBlend, bBusy ? 0.35f : 1.0f, DeltaTime, 4.0f);
	const float IdleAmp = TD::ViewWidth() > TD::IdleLodViewWidth ? 0.0f : IdleBlend;
	auto IdleSin = [IdleAmp](float X) { return IdleAmp * FMath::Sin(X); };
	auto IdleCos = [IdleAmp](float X) { return IdleAmp * FMath::Cos(X); };

	// idle animation
	if (AnimPivot)
	{
		switch (Type)
		{
		case ETDTowerType::Jelly:
		{
			AnimPivot->SetRelativeLocation(AnimBase + FVector(0, 0, IdleSin(AnimTime * 2.2f) * 6.0f));
			const float Squash = 1.0f + FireKick * 0.25f;
			AnimPivot->SetRelativeScale3D(FVector(Squash, Squash, 1.0f / Squash));
			for (int32 i = 0; i < Tentacles.Num(); ++i)
			{
				Tentacles[i]->SetRelativeRotation(FRotator(IdleSin(AnimTime * 2.6f + i) * 10.0f, 0, IdleCos(AnimTime * 2.2f + i * 1.3f) * 8.0f));
			}
			break;
		}
		case ETDTowerType::Puffer:
		case ETDTowerType::Lionfish:
		{
			const float Puff = 1.0f + FireKick * (Type == ETDTowerType::Puffer ? 0.4f : 0.15f) + IdleSin(AnimTime * 1.6f) * 0.03f;
			AnimPivot->SetRelativeScale3D(FVector(Puff));
			break;
		}
		case ETDTowerType::Eel:
		{
			AnimPivot->SetRelativeRotation(FRotator(IdleSin(AnimTime * 1.7f) * 4.0f, AnimPivot->GetRelativeRotation().Yaw, IdleCos(AnimTime * 1.3f) * 4.0f));
			if (Light)
			{
				Light->SetIntensity(2000.0f + FireKick * 12000.0f + IdleSin(AnimTime * 9.0f) * 300.0f);
			}
			break;
		}
		case ETDTowerType::Humpback:
		{
			// bob, turn, and swell with an aura beat every second and a half
			AnimPivot->SetRelativeLocation(AnimBase + FVector(0, 0, IdleSin(AnimTime * 2.0f) * 4.0f));
			AnimPivot->SetRelativeRotation(FRotator(0, AnimTime * 25.0f, 0));
			const float Beat = FMath::Pow(FMath::Max(0.0f, IdleSin(AnimTime * PI / 1.5f)), 12.0f);
			AnimPivot->SetRelativeScale3D(FVector(1.0f + 0.04f * Beat));
			break;
		}
		default:
			break;
		}

		// turrets kick back along their aim when they fire and breathe gently while idle
		const bool bTurret = Type == ETDTowerType::Harpoon || Type == ETDTowerType::Mantis || Type == ETDTowerType::Octopus
			|| Type == ETDTowerType::Sawfish || Type == ETDTowerType::Puffer || Type == ETDTowerType::Lionfish || Type == ETDTowerType::Eel;
		if (bTurret)
		{
			const FRotator Rot = AnimPivot->GetRelativeRotation();
			const FVector Back = -FRotator(0, Rot.Yaw, 0).Vector();
			const float Kick = FireKick * FireKick;
			// the mantis shrimp lunges into its punch instead of recoiling
			const float KickBack = Type == ETDTowerType::Mantis ? -5.0f : (Type == ETDTowerType::Puffer ? 4.0f : (Type == ETDTowerType::Octopus ? 1.5f : 9.0f));
			AnimPivot->SetRelativeLocation(AnimBase + Back * KickBack * Kick + FVector(0, 0, IdleSin(AnimTime * 1.8f) * 1.5f));
			if (Type == ETDTowerType::Harpoon)
			{
				// a slow nod of the launcher while it waits
				AnimPivot->SetRelativeRotation(FRotator(Kick * 10.0f + IdleSin(AnimTime * 0.9f) * 1.5f, Rot.Yaw, 0));
			}
			if (Type == ETDTowerType::Mantis)
			{
				// a slight side-to-side sway on its legs
				AnimPivot->SetRelativeRotation(FRotator(0, Rot.Yaw, IdleSin(AnimTime * 1.1f) * 3.0f));
			}
			if (Type == ETDTowerType::Eel)
			{
				// the eel lunges as it zaps
				AnimPivot->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f + 0.18f * Kick));
			}
		}
	}
	if (Spinner)
	{
		Spinner->AddRelativeRotation(FRotator(0, (Type == ETDTowerType::AcidVent ? -40.0f : 40.0f) * DeltaTime, 0));
		Spinner->SetRelativeScale3D(FVector(1.0f + 0.05f * IdleSin(AnimTime * 3.0f)));
	}
	if (Tail)
	{
		if (Type == ETDTowerType::Humpback)
		{
			// a whale's fluke beats up and down
			Tail->SetRelativeRotation(FQuat(FVector::YAxisVector, -0.6f + IdleSin(AnimTime * 1.4f) * 0.22f));
		}
		else
		{
			Tail->SetRelativeRotation(FRotator(0, IdleSin(AnimTime * 4.0f) * 20.0f, 0));
		}
	}
	for (int32 i = 0; i < Throwers.Num(); ++i)
	{
		// cocked behind the head, whipped forward on its throw and eased back
		ThrowKicks[i] = FMath::Max(0.0f, ThrowKicks[i] - DeltaTime * 5.0f);
		// arms curl a little between throws
		Throwers[i]->SetRelativeRotation(FQuat(FVector::YAxisVector, FMath::Clamp(ThrowKicks[i] * 1.5f, 0.0f, 1.0f) * 1.25f + IdleSin(AnimTime * 1.7f + i * 1.9f) * 0.12f));
	}
	if (Type != ETDTowerType::Jelly)
	{
		for (int32 i = 0; i < Tentacles.Num(); ++i)
		{
			const float Side = i == 0 ? -1.0f : 1.0f;
			FQuat Sway = FQuat::Identity;
			switch (Type)
			{
			case ETDTowerType::Mantis:		// stalked eyes wander independently
				Sway = FQuat(FVector::XAxisVector, IdleSin(AnimTime * 1.9f + i * 2.4f) * 0.3f) * FQuat(FVector::ZAxisVector, IdleSin(AnimTime * 1.3f + i * 4.0f) * 0.5f);
				break;
			case ETDTowerType::Lionfish:	// fans held out to the sides, flaring on each shot
				Sway = FQuat(FVector::XAxisVector, FMath::DegreesToRadians(Side * (-14.0f + IdleSin(AnimTime * 1.5f) * 5.0f)))
					* FQuat(FVector::ZAxisVector, FMath::DegreesToRadians(Side * (-58.0f - FireKick * 16.0f + IdleSin(AnimTime * 1.5f + 1.0f) * 5.0f)));
				break;
			case ETDTowerType::Sawfish:
				Sway = FQuat(FVector::XAxisVector, Side * IdleSin(AnimTime * 1.3f) * 0.12f);
				break;
			case ETDTowerType::Humpback:	// slow flipper strokes
				Sway = FQuat(FVector::XAxisVector, Side * IdleSin(AnimTime * 1.4f) * 0.26f);
				break;
			case ETDTowerType::Octopus:	// sprawled arms shift a little
				Sway = FQuat(FVector::XAxisVector, IdleSin(AnimTime * 1.6f + i * 1.1f) * 0.05f) * FQuat(FVector::YAxisVector, IdleCos(AnimTime * 1.3f + i) * 0.05f);
				break;
			case ETDTowerType::AcidVent:	// tube worm plumes in the current
				Sway = FQuat(FVector::XAxisVector, IdleSin(AnimTime * 1.5f + i) * 0.22f) * FQuat(FVector::YAxisVector, IdleCos(AnimTime * 1.2f + i * 1.3f) * 0.22f);
				break;
			default:
				break;
			}
			Tentacles[i]->SetRelativeRotation(Sway);
		}
	}
	for (USceneComponent* Club : Clubs)
	{
		// folded under the head, whipped out by a punch and eased back
		const float Whip = FMath::Clamp(FireKick * 1.6f, 0.0f, 1.0f);
		Club->SetRelativeRotation(FQuat(FVector::YAxisVector, FMath::DegreesToRadians(-118.0f + Whip * 128.0f)));
	}
	if (Blade)
	{
		// the snout slashes side to side as a tooth flies off
		Blade->SetRelativeRotation(FRotator(0, FMath::Sin(FireKick * 2.0f * PI) * 14.0f, 0));
	}
	for (int32 i = 0; i < Puffs.Num(); ++i)
	{
		// smoke rises and swells from the chimney mouth, then thins away
		const float U = FMath::Frac(AnimTime * 0.2f + (float)i / (i < VentMainPuffs ? VentMainPuffs : Puffs.Num() - VentMainPuffs));
		const float Rise = i < VentMainPuffs ? 74.0f : 30.0f;
		const float Wander = 2.0f + U * (i < VentMainPuffs ? 11.0f : 6.0f);
		Puffs[i]->SetRelativeLocation(PuffBases[i] + FVector(IdleSin(AnimTime * 1.2f + i * 2.1f) * Wander + U * 5.0f, IdleCos(AnimTime * 0.9f + i * 1.7f) * Wander, U * Rise));
		Puffs[i]->SetRelativeScale3D(FVector((0.55f + U * 1.5f) * FMath::Min(1.0f, U * 6.0f) * FMath::Min(1.0f, (1.0f - U) * 5.0f)));
	}
	if (!bGhost && IsSupportTower())
	{
		// a ring of bubbles shows the aura working during a wave
		PulseCooldown -= DeltaTime;
		if (PulseCooldown <= 0.0f)
		{
			PulseCooldown = AuraPulseInterval;
			const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
			if (GameMode && GameMode->IsWaveActive())
			{
				const FLinearColor Color = Type == ETDTowerType::Humpback ? FLinearColor(0.75f, 0.92f, 1.0f) : FLinearColor(0.8f, 1.0f, 0.2f);
				ATDFx::SpawnRing(GetWorld(), GetActorLocation() + FVector(0, 0, PlinthHeight + 6.0f), 70.0f, Color);
			}
		}
	}

	if (bGhost || !IsAttackTower())
	{
		return;
	}

	Cooldown -= DeltaTime;

	ATDEnemy* Target = FindTarget(HitsFlying());

	// turn the turret / head towards the target
	if (Target && AnimPivot && Type != ETDTowerType::Jelly)
	{
		const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
		const float DesiredYaw = ToTarget.Rotation().Yaw;
		FRotator Rot = AnimPivot->GetRelativeRotation();
		Rot.Yaw = FMath::FixedTurn(Rot.Yaw, DesiredYaw, 540.0f * DeltaTime);
		AnimPivot->SetRelativeRotation(Rot);
	}

	if (!Target)
	{
		// nothing in range: hold the current facing
		Cooldown = FMath::Max(Cooldown, 0.0f);
		return;
	}
	if (Cooldown > 0.0f)
	{
		return;
	}

	// carry the overshoot so fast towers keep their rate at high game speed / low frame rate
	Cooldown = FMath::Max(Cooldown + 1.0f / FMath::Max(0.01f, GetRate()), 0.0f);
	FireKick = 1.0f;
	TDAudio::PlayShot(this, Type);

	switch (Type)
	{
	case ETDTowerType::Harpoon:	FireHarpoon(Target); break;
	case ETDTowerType::Sawfish:	FireSawfish(Target); break;
	case ETDTowerType::Jelly:	FireJelly(); break;
	case ETDTowerType::Puffer:	FirePuffer(Target); break;
	case ETDTowerType::Eel:		FireEel(Target); break;
	case ETDTowerType::Octopus:	FireOctopus(Target); break;
	case ETDTowerType::Mantis:	FireMantis(Target); break;
	default:					FireProjectile(Target, 0); break;
	}

	// Trident of the Deep King: every 3rd attack also strikes two more
	++AttackCount;
	if (HasUnique(ETDUnique::Trident) && TD::TridentStrikes(AttackCount) && Type != ETDTowerType::Jelly)
	{
		TridentStrike(Target);
	}
}

void ATDTower::TridentStrike(ATDEnemy* Target)
{
	const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	if (!GameMode)
	{
		return;
	}
	const FVector Here = GetActorLocation();
	const float RangeUU = GetRange() * TD::TileSize;
	TArray<TPair<float, ATDEnemy*>> Near;
	for (ATDEnemy* Enemy : GameMode->GetEnemies())
	{
		if (Enemy && Enemy != Target && Enemy->IsAlive() && (HitsFlying() || !Enemy->IsFlying()))
		{
			const float Distance = FVector::Dist2D(Here, Enemy->GetActorLocation());
			if (Distance <= RangeUU)
			{
				Near.Add(TPair<float, ATDEnemy*>(Distance, Enemy));
			}
		}
	}
	Near.Sort([](const TPair<float, ATDEnemy*>& A, const TPair<float, ATDEnemy*>& B) { return A.Key < B.Key; });
	const FVector Start = Muzzle ? Muzzle->GetComponentLocation() : Here + FVector(0, 0, 90);
	for (int32 i = 0; i < FMath::Min(TD::TridentExtraTargets, Near.Num()); ++i)
	{
		ATDEnemy* Extra = Near[i].Value;
		ATDFx::SpawnBolt(GetWorld(), { Start, Extra->GetAimLocation() }, FLinearColor(1.0f, 0.82f, 0.35f));
		ATDEnemy::DeliverHit(Extra, MakeHit(1.0f));
	}
}

void ATDTower::FireProjectile(ATDEnemy* Target, int32 Pierce, float DamageScale)
{
	const FVector Start = Muzzle ? Muzzle->GetComponentLocation() : GetActorLocation() + FVector(0, 0, 90);
	ATDProjectile::LaunchBolt(GetWorld(), Start, Target, MakeHit(DamageScale), Pierce, Stats.ProjectileSpeed, Type);
}

void ATDTower::FireHarpoon(ATDEnemy* Target)
{
	const int32 Pierce = GetSkillRank(HarpoonPiercing);
	if (Form == 1)
	{
		// Swordfish: a second harpoon at another target (or the same one if it's alone)
		TArray<ATDEnemy*> Taken;
		Taken.Add(Target);
		ATDEnemy* Second = FindTarget(true, &Taken);
		FireProjectile(Target, Pierce, 0.65f);
		FireProjectile(Second ? Second : Target, Pierce, 0.65f);
		return;
	}
	FireProjectile(Target, Pierce + (Form == 2 ? 2 : 0));
}

void ATDTower::FireSawfish(ATDEnemy* Target)
{
	FireProjectile(Target, 0);
	if (Form != 2)
	{
		return;
	}

	// Sweeper: the snout slashes everything close to the sawfish, leaving it bleeding
	const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	if (!GameMode || Stats.BleedPercent <= 0.0f)
	{
		return;
	}
	const FVector Here = GetActorLocation();
	const float RadiusUU = 1.2f * TD::TileSize;
	TArray<ATDEnemy*> Slashed;
	for (ATDEnemy* Enemy : GameMode->GetEnemies())
	{
		if (Enemy && Enemy->IsAlive() && FVector::Dist2D(Here, Enemy->GetActorLocation()) <= RadiusUU)
		{
			Slashed.Add(Enemy);
		}
	}
	for (ATDEnemy* Enemy : Slashed)
	{
		Enemy->ApplyBleed(Stats.BleedPercent, Stats.BleedDuration, this);
	}
	if (Slashed.Num() > 0)
	{
		ATDFx::SpawnRing(GetWorld(), Here + FVector(0, 0, 40), RadiusUU, FLinearColor(1.0f, 0.45f, 0.45f));
	}
}

void ATDTower::FireMantis(ATDEnemy* Target)
{
	// the club lands instantly, with a burst of cavitation bubbles (the hit may destroy the target)
	const FVector Impact = Target->GetAimLocation();
	const FVector Ground = Target->GetActorLocation();
	ATDEnemy::DeliverHit(Target, MakeHit());
	ATDFx::SpawnBurst(GetWorld(), Impact, 34.0f, FLinearColor(0.75f, 0.95f, 1.0f));

	// Smasher: the shockwave hits ground creatures within a tile of the impact
	if (Form == 1)
	{
		const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
		if (!GameMode)
		{
			return;
		}
		TArray<ATDEnemy*> Around;
		for (ATDEnemy* Enemy : GameMode->GetEnemies())
		{
			if (Enemy && Enemy != Target && Enemy->IsAlive() && !Enemy->IsFlying() && FVector::Dist2D(Ground, Enemy->GetActorLocation()) <= TD::TileSize)
			{
				Around.Add(Enemy);
			}
		}
		FTDHit Wave = MakeHit(0.5f);
		Wave.StunChance = 0.0f;
		Wave.bSecondary = true;
		Wave.XpScale = TD::XpScaleArea;
		for (ATDEnemy* Enemy : Around)
		{
			Enemy->ApplyHit(Wave);
		}
		ATDFx::SpawnRing(GetWorld(), FVector(Ground.X, Ground.Y, 20.0f), TD::TileSize, FLinearColor(0.75f, 0.95f, 1.0f));
	}
}

void ATDTower::FireOctopus(ATDEnemy* Target)
{
	if (ThrowKicks.Num() > 0)
	{
		ThrowKicks[NextThrower] = 1.0f;
		NextThrower = (NextThrower + 1) % ThrowKicks.Num();
	}

	// Eight Arms: each extra rank adds another target to the volley
	TArray<ATDEnemy*> Targets;
	Targets.Add(Target);
	const int32 ExtraTargets = GetSkillRank(OctopusArms) + (Form == 1 ? 2 : 0);	// Juggler throws at 2 more
	for (int32 i = 0; i < ExtraTargets; ++i)
	{
		if (ATDEnemy* Extra = FindTarget(true, &Targets))
		{
			Targets.Add(Extra);
		}
	}
	for (ATDEnemy* Enemy : Targets)
	{
		FireProjectile(Enemy, 0);
	}
}

void ATDTower::FireJelly()
{
	const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	if (!GameMode)
	{
		return;
	}

	const float RangeUU = GetRange() * TD::TileSize;
	const FVector Here = GetActorLocation();
	const float NeurotoxinBonus = 0.5f * GetSkillRank(JellyNeurotoxin);

	// Moon Jelly: every third pulse stuns
	const bool bStunPulse = Form == 2 && (++PulseCount % 3) == 0;

	// copy: hits may kill and remove enemies from the list
	TArray<ATDEnemy*> Hits;
	for (ATDEnemy* Enemy : GameMode->GetEnemies())
	{
		if (Enemy && Enemy->IsAlive() && FVector::Dist2D(Here, Enemy->GetActorLocation()) <= RangeUU)
		{
			Hits.Add(Enemy);
		}
	}

	for (ATDEnemy* Enemy : Hits)
	{
		const bool bWasSlowed = Enemy->IsSlowed();
		Enemy->ApplySlow(Stats.SlowFactor, JellySlowDuration);
		AddXp(TD::JellySlowXp * (1.0f - Stats.SlowFactor));
		FTDHit Hit = MakeHit(bWasSlowed ? 1.0f + NeurotoxinBonus : 1.0f);
		Hit.XpScale = TD::XpScaleArea;
		if (bStunPulse)
		{
			Hit.StunChance = 1.0f;
			Hit.StunDuration = 0.5f;
		}
		Enemy->ApplyHit(Hit);
	}

	ATDFx::SpawnRing(GetWorld(), Here + FVector(0, 0, 36), RangeUU, bStunPulse ? FLinearColor(0.85f, 0.9f, 1.0f) : FLinearColor(1.0f, 0.6f, 0.9f));
}

void ATDTower::FirePuffer(ATDEnemy* Target)
{
	const FVector Start = Muzzle ? Muzzle->GetComponentLocation() : GetActorLocation() + FVector(0, 0, 110);
	FVector Land = Target->GetActorLocation();
	Land.Z = 0.0f;
	if (Form == 1)
	{
		// Cluster Puffer: three bomblets in a triangle around the target, each with its own crit roll
		const float Spread = 0.55f * TD::TileSize;
		const float Yaw0 = FMath::FRandRange(0.0f, 120.0f);
		for (int32 i = 0; i < 3; ++i)
		{
			FTDHit Bomblet = MakeHit(0.45f);
			Bomblet.XpScale = TD::XpScaleArea;
			const FVector Offset = FRotator(0.0f, Yaw0 + 120.0f * i, 0.0f).RotateVector(FVector(Spread, 0.0f, 0.0f));
			ATDProjectile::LaunchBomb(GetWorld(), Start, Land + Offset, Bomblet, Stats.Splash * 0.65f, 0.25f * GetSkillRank(PufferShrapnel), Stats.ProjectileSpeed);
		}
		return;
	}
	FTDHit Hit = MakeHit();
	Hit.XpScale = TD::XpScaleArea;
	ATDProjectile::LaunchBomb(GetWorld(), Start, Land, Hit, Stats.Splash, 0.25f * GetSkillRank(PufferShrapnel), Stats.ProjectileSpeed);
}

void ATDTower::FireEel(ATDEnemy* Target)
{
	const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	if (!GameMode)
	{
		return;
	}

	// collect the chain first, then apply damage (kills remove enemies from the list)
	TArray<ATDEnemy*> Chain;
	Chain.Add(Target);
	while (Chain.Num() < Stats.Chain)
	{
		const FVector From = Chain.Last()->GetActorLocation();
		ATDEnemy* Next = nullptr;
		float NextDistance = EelJumpRange * TD::TileSize * (Form == 1 ? 2.0f : 1.0f);	// Storm Eel jumps twice as far
		for (ATDEnemy* Enemy : GameMode->GetEnemies())
		{
			if (!Enemy || !Enemy->IsAlive() || Chain.Contains(Enemy))
			{
				continue;
			}
			const float Distance = FVector::Dist2D(From, Enemy->GetActorLocation());
			if (Distance <= NextDistance)
			{
				NextDistance = Distance;
				Next = Enemy;
			}
		}
		if (!Next)
		{
			break;
		}
		Chain.Add(Next);
	}

	TArray<FVector> Points;
	Points.Add(Muzzle ? Muzzle->GetComponentLocation() : GetActorLocation() + FVector(0, 0, 140));
	for (ATDEnemy* Enemy : Chain)
	{
		Points.Add(Enemy->GetAimLocation());
	}

	float Scale = 1.0f;
	for (ATDEnemy* Enemy : Chain)
	{
		FTDHit Hit = MakeHit(Scale);
		Hit.ChainExtra = 0;		// the eel's own chain replaces the item chain
		Hit.XpScale = (Enemy == Target) ? 1.0f : TD::XpScaleArea;
		ATDEnemy::DeliverHit(Enemy, Hit);
		Scale *= (Form == 1) ? 1.0f : EelJumpFalloff;
	}

	ATDFx::SpawnBolt(GetWorld(), Points, FLinearColor(1.0f, 0.95f, 0.4f));
}
