// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDGrid.h"
#include "TDTower.h"
#include "TDEnemy.h"
#include "TDGameMode.h"
#include "TDRules.h"
#include "TDRun.h"
#include "TDModels.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 RingSegments = 72;
	constexpr float RingWidth = 8.0f;		// uu (was 15; the user found the range border too thick)
	constexpr float RingHeight = 4.0f;
	constexpr float RingZ = 7.0f;
	constexpr float DiscZ = 3.0f;
	constexpr float TintZ = 1.2f;
	const TCHAR* const RangeMaterialPath = TEXT("/Game/AquaticTowerDefense/M_ATD_Range.M_ATD_Range");

	// small seafloor decor around the board, beside the kelp
	enum class EDecor : uint8 { Rocks, Coral, BrainCoral, Shell, Starfish, Anemone, Count };
	constexpr int32 DecorVariants = 3;
	constexpr uint64 DecorKeyBase = 50000;
	constexpr int32 CausticRings = 5;	// nested discs per patch of light, so its edge fades instead of ending in a hard line	// merged mesh keys, clear of the towers' (up to ~11000) and the unit shapes' (1000s)

	/** Footprint radius of a decor piece at scale 1, for spacing and for keeping off the board's slab */
	float DecorRadius(EDecor Kind)
	{
		switch (Kind)
		{
		case EDecor::Rocks:			return 26.0f;
		case EDecor::Coral:			return 24.0f;
		case EDecor::BrainCoral:	return 16.0f;
		case EDecor::Shell:			return 18.0f;
		case EDecor::Starfish:		return 14.0f;
		case EDecor::Anemone:		return 17.0f;
		default:					return 20.0f;
		}
	}

	/** Parts of one decor variant, standing on Z 0. Soft reef colours, a step quieter than towers and creatures so the seabed never pulls the eye. */
	TArray<FTDMergedPart> MakeDecorParts(EDecor Kind, int32 Variant)
	{
		TArray<FTDMergedPart> Parts;
		FRandomStream R(9100 + (int32)Kind * 31 + Variant * 7);
		auto Add = [&Parts](ETDShape Shape, const FVector& Loc, const FVector& Size, const FQuat& Rot, const FLinearColor& Color)
		{
			FTDMergedPart& P = Parts.AddDefaulted_GetRef();
			P.Shape = Shape;
			P.Location = Loc;
			P.Size = Size;
			P.Rotation = Rot;
			P.Color = Color;
		};
		auto Yaw = [](float Deg) { return FQuat(FVector::UpVector, FMath::DegreesToRadians(Deg)); };
		auto Along = [](const FVector& Dir) { return FQuat::FindBetweenNormals(FVector::UpVector, Dir); };
		auto Lighter = [](const FLinearColor& C, float F) { return FLinearColor(FMath::Min(C.R * F, 1.0f), FMath::Min(C.G * F, 1.0f), FMath::Min(C.B * F, 1.0f), 1.0f); };

		switch (Kind)
		{
		case EDecor::Rocks:
		{
			// two to four half-buried stones, a moss cap on the biggest and a few pebbles
			const FLinearColor Stone = Variant == 1 ? FLinearColor(0.18f, 0.19f, 0.21f) : FLinearColor(0.15f, 0.18f, 0.2f);
			const FLinearColor Stone2(0.22f, 0.22f, 0.24f);
			const int32 Count = 2 + Variant;
			for (int32 i = 0; i < Count; ++i)
			{
				const float S = i == 0 ? R.FRandRange(26.0f, 32.0f) : R.FRandRange(12.0f, 20.0f);
				const float A = R.FRandRange(0.0f, 2.0f * PI);
				const float Dist = i == 0 ? 0.0f : R.FRandRange(14.0f, 20.0f);
				const FVector Size(S, S * R.FRandRange(0.75f, 1.0f), S * R.FRandRange(0.5f, 0.68f));
				Add(ETDShape::Sphere, FVector(FMath::Cos(A) * Dist, FMath::Sin(A) * Dist, Size.Z * 0.3f), Size, Yaw(R.FRandRange(0, 360)), i % 2 ? Stone2 : Stone);
				if (i == 0)
				{
					Add(ETDShape::Sphere, FVector(-2.0f, 1.0f, Size.Z * 0.62f), FVector(S * 0.55f, S * 0.5f, S * 0.14f), Yaw(R.FRandRange(0, 360)), FLinearColor(0.12f, 0.33f, 0.16f));
				}
			}
			for (int32 i = 0; i < 3; ++i)
			{
				const float A = R.FRandRange(0.0f, 2.0f * PI);
				const float Dist = R.FRandRange(20.0f, 26.0f);
				const float S = R.FRandRange(4.0f, 7.0f);
				Add(ETDShape::Sphere, FVector(FMath::Cos(A) * Dist, FMath::Sin(A) * Dist, S * 0.2f), FVector(S, S * 0.8f, S * 0.55f), FQuat::Identity, Stone2);
			}
			break;
		}
		case EDecor::Coral:
		{
			// a branching coral: tilted stems with rounded tips and a few twigs
			const FLinearColor Colors[DecorVariants] = { FLinearColor(0.62f, 0.22f, 0.26f), FLinearColor(0.62f, 0.37f, 0.16f), FLinearColor(0.4f, 0.22f, 0.56f) };
			const FLinearColor Body = Colors[Variant % DecorVariants];
			const FLinearColor Tip = Lighter(Body, 1.3f);
			Add(ETDShape::Sphere, FVector(0, 0, 2.0f), FVector(13.0f, 13.0f, 7.0f), FQuat::Identity, Body);
			const int32 Branches = 5 + Variant;
			for (int32 i = 0; i < Branches; ++i)
			{
				const float A = 2.0f * PI * i / Branches + R.FRandRange(-0.3f, 0.3f);
				const float Tilt = FMath::DegreesToRadians(R.FRandRange(8.0f, 40.0f));
				const FVector Dir(FMath::Cos(A) * FMath::Sin(Tilt), FMath::Sin(A) * FMath::Sin(Tilt), FMath::Cos(Tilt));
				const float Len = R.FRandRange(14.0f, 28.0f);
				const float Thick = R.FRandRange(3.5f, 4.5f);
				const FVector Start(FMath::Cos(A) * 3.0f, FMath::Sin(A) * 3.0f, 3.0f);
				Add(ETDShape::Cylinder, Start + Dir * Len * 0.5f, FVector(Thick, Thick, Len), Along(Dir), Body);
				Add(ETDShape::Sphere, Start + Dir * Len, FVector(Thick * 1.35f), FQuat::Identity, Tip);
				if (R.FRand() < 0.6f)
				{
					// a twig off the stem's middle, leaning further out
					const float TwigTilt = Tilt + FMath::DegreesToRadians(25.0f);
					const float TwigA = A + R.FRandRange(-0.5f, 0.5f);
					const FVector TwigDir(FMath::Cos(TwigA) * FMath::Sin(TwigTilt), FMath::Sin(TwigA) * FMath::Sin(TwigTilt), FMath::Cos(TwigTilt));
					const FVector From = Start + Dir * Len * 0.55f;
					const float TwigLen = R.FRandRange(7.0f, 11.0f);
					Add(ETDShape::Cylinder, From + TwigDir * TwigLen * 0.5f, FVector(Thick * 0.75f, Thick * 0.75f, TwigLen), Along(TwigDir), Body);
					Add(ETDShape::Sphere, From + TwigDir * TwigLen, FVector(Thick * 1.05f), FQuat::Identity, Tip);
				}
			}
			break;
		}
		case EDecor::BrainCoral:
		{
			// a low dome ringed with grooves
			const FLinearColor Colors[DecorVariants] = { FLinearColor(0.57f, 0.5f, 0.18f), FLinearColor(0.62f, 0.32f, 0.27f), FLinearColor(0.27f, 0.52f, 0.34f) };
			const FLinearColor Body = Colors[Variant % DecorVariants];
			const FLinearColor Groove = Lighter(Body, 0.72f);
			const float S = 26.0f + Variant * 2.0f;
			Add(ETDShape::Sphere, FVector(0, 0, S * 0.22f), FVector(S, S * 0.92f, S * 0.6f), FQuat::Identity, Body);
			for (int32 Ring = 0; Ring < 2; ++Ring)
			{
				const int32 Bumps = Ring == 0 ? 9 : 5;
				const float Dist = Ring == 0 ? S * 0.36f : S * 0.17f;
				const float Z = Ring == 0 ? S * 0.38f : S * 0.5f;
				for (int32 i = 0; i < Bumps; ++i)
				{
					const float A = 2.0f * PI * (i + 0.5f * Ring) / Bumps;
					Add(ETDShape::Sphere, FVector(FMath::Cos(A) * Dist, FMath::Sin(A) * Dist, Z), FVector(S * 0.22f, S * 0.12f, S * 0.1f), Yaw(FMath::RadiansToDegrees(A) + 90.0f), Groove);
				}
			}
			break;
		}
		case EDecor::Shell:
		{
			// scallop shells: a flat fan with ribs and a hinge; the third variant is a pair
			const FLinearColor Colors[DecorVariants] = { FLinearColor(0.68f, 0.58f, 0.45f), FLinearColor(0.7f, 0.42f, 0.37f), FLinearColor(0.61f, 0.56f, 0.63f) };
			const FLinearColor Body = Colors[Variant % DecorVariants];
			const FLinearColor Rib = Lighter(Body, 0.75f);
			auto Scallop = [&](const FVector& Center, float YawDeg, float Scale)
			{
				const FQuat Q = Yaw(YawDeg);
				Add(ETDShape::Sphere, Center + Q.RotateVector(FVector(1.0f, 0, 2.0f)) * Scale, FVector(16.0f, 15.0f, 4.5f) * Scale, Q, Body);
				for (int32 i = 0; i < 5; ++i)
				{
					const float A = FMath::DegreesToRadians(-48.0f + 24.0f * i);
					const FVector Dir(FMath::Cos(A), FMath::Sin(A), 0.0f);
					Add(ETDShape::Cube, Center + Q.RotateVector(FVector(-5.0f, 0, 3.6f) + Dir * 6.0f) * Scale, FVector(10.0f, 0.9f, 0.9f) * Scale,
						Q * Yaw(FMath::RadiansToDegrees(A)), Rib);
				}
				Add(ETDShape::Cube, Center + Q.RotateVector(FVector(-7.0f, 0, 1.6f)) * Scale, FVector(3.5f, 6.0f, 2.6f) * Scale, Q, Rib);
			};
			Scallop(FVector::ZeroVector, 0.0f, 1.0f);
			if (Variant == 2)
			{
				Scallop(FVector(10.0f, 12.0f, 0.0f), 110.0f, 0.7f);
			}
			break;
		}
		case EDecor::Starfish:
		{
			// five flat arms around a small disc, with a row of light dots
			const FLinearColor Colors[DecorVariants] = { FLinearColor(0.7f, 0.31f, 0.13f), FLinearColor(0.64f, 0.19f, 0.3f), FLinearColor(0.64f, 0.5f, 0.16f) };
			const FLinearColor Body = Colors[Variant % DecorVariants];
			const FLinearColor Dot = Lighter(Body, 1.45f);
			Add(ETDShape::Sphere, FVector(0, 0, 1.4f), FVector(9.0f, 9.0f, 3.2f), FQuat::Identity, Body);
			const float Spin = R.FRandRange(0.0f, 72.0f);
			for (int32 i = 0; i < 5; ++i)
			{
				const float Deg = Spin + 72.0f * i + R.FRandRange(-6.0f, 6.0f);
				const FVector Dir(FMath::Cos(FMath::DegreesToRadians(Deg)), FMath::Sin(FMath::DegreesToRadians(Deg)), 0.0f);
				const float Len = R.FRandRange(13.0f, 16.0f);
				Add(ETDShape::Sphere, Dir * Len * 0.45f + FVector(0, 0, 1.2f), FVector(Len, 5.0f, 2.6f), Yaw(Deg), Body);
				Add(ETDShape::Sphere, Dir * Len * 0.3f + FVector(0, 0, 2.3f), FVector(1.8f), FQuat::Identity, Dot);
				Add(ETDShape::Sphere, Dir * Len * 0.6f + FVector(0, 0, 2.0f), FVector(1.5f), FQuat::Identity, Dot);
			}
			break;
		}
		case EDecor::Anemone:
		{
			// a short stalk and an oral disc ringed with tentacles; the whole piece sways gently
			const FLinearColor Stalks[DecorVariants] = { FLinearColor(0.36f, 0.18f, 0.27f), FLinearColor(0.33f, 0.29f, 0.16f), FLinearColor(0.16f, 0.3f, 0.33f) };
			const FLinearColor Tentacles[DecorVariants] = { FLinearColor(0.68f, 0.32f, 0.51f), FLinearColor(0.65f, 0.56f, 0.26f), FLinearColor(0.27f, 0.62f, 0.59f) };
			const FLinearColor Stalk = Stalks[Variant % DecorVariants];
			const FLinearColor Tentacle = Tentacles[Variant % DecorVariants];
			const FLinearColor Tip = Lighter(Tentacle, 1.3f);
			Add(ETDShape::Cylinder, FVector(0, 0, 5.0f), FVector(11.0f, 11.0f, 10.0f), FQuat::Identity, Stalk);
			Add(ETDShape::Cylinder, FVector(0, 0, 10.5f), FVector(17.0f, 17.0f, 2.0f), FQuat::Identity, Stalk);
			const int32 Count = 12 + Variant * 2;
			for (int32 i = 0; i < Count; ++i)
			{
				const float A = 2.0f * PI * i / Count + R.FRandRange(-0.1f, 0.1f);
				const float Tilt = FMath::DegreesToRadians(R.FRandRange(25.0f, 50.0f));
				const FVector Dir(FMath::Cos(A) * FMath::Sin(Tilt), FMath::Sin(A) * FMath::Sin(Tilt), FMath::Cos(Tilt));
				const FVector Start(FMath::Cos(A) * 6.5f, FMath::Sin(A) * 6.5f, 11.0f);
				const float Len = R.FRandRange(9.0f, 12.5f);
				Add(ETDShape::Cylinder, Start + Dir * Len * 0.5f, FVector(2.0f, 2.0f, Len), Along(Dir), Tentacle);
				Add(ETDShape::Sphere, Start + Dir * Len, FVector(2.8f), FQuat::Identity, Tip);
			}
			break;
		}
		default:
			break;
		}
		return Parts;
	}
}

ATDGrid::ATDGrid()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ATDGrid::BeginPlay()
{
	Super::BeginPlay();

	TD::MakeStartingCells(Cells);

	RouteMarkers = MakeRouteMarkers(TD::SharedColorMaterial(FLinearColor(0.55f, 0.95f, 1.0f)));
	PreviewMarkers = MakeRouteMarkers(TD::SharedColorMaterial(FLinearColor(1.0f, 0.78f, 0.15f)));

	BuildBoardVisuals();
	RebuildField();
}

// ---------------------------------------------------------------------------------------------
// Board visuals
// ---------------------------------------------------------------------------------------------

void ATDGrid::BuildMapVisuals()
{
	if (BuiltMap == TD::GetMap() && BuiltTier == TD::Run().Tier && MapRoot)
	{
		return;
	}
	BuiltMap = TD::GetMap();
	BuiltTier = TD::Run().Tier;

	// clear the previous map's pieces
	if (MapRoot)
	{
		TArray<USceneComponent*> OldParts;
		MapRoot->GetChildrenComponents(true, OldParts);
		for (USceneComponent* Child : OldParts)
		{
			Child->DestroyComponent();
		}
		MapRoot->DestroyComponent();
	}
	Kelp.Reset();
	Anemones.Reset();
	Shafts.Reset();
	ShaftMats.Reset();
	Caustics.Reset();
	CausticMats.Reset();
	CausticBase.Reset();
	Snow.Reset();
	VentBubbles.Reset();
	Schools.Reset();
	Fish.Reset();
	SnowISM = nullptr;
	FishISM = nullptr;
	BubbleISM = nullptr;
	Manta = nullptr;
	MapRoot = TD::AddPivot(this, GetRootComponent(), FVector::ZeroVector);
	USceneComponent* Root = MapRoot;
	FRandomStream Rand(1337 + BuiltMap * 101);

	FTDModelOptions PropOptions;
	PropOptions.MinSize = 2.5f;

	// seabed: every map has its own faint tint, and some maps leave cells of the grid out (open water)
	{
		// (Abyss tiers deepen the seabed towards blue-violet)
		const FLinearColor Tint = TD::MapTint(BuiltMap) * TD::AbyssTint(BuiltTier);
		auto Tinted = [&Tint](const FLinearColor& Color)
		{
			return FLinearColor(Color.R * Tint.R, Color.G * Tint.G, Color.B * Tint.B, Color.A);
		};
		const float BoardW = TD::Rows * TD::TileSize;	// world X extent
		const float BoardH = TD::Cols * TD::TileSize;	// world Y extent

		// open seafloor around the board
		TD::AddShape(this, Root, ETDShape::Cube, FVector(0, 0, -95), FVector(9000, 9000, 20), Tinted(FLinearColor(0.035f, 0.11f, 0.12f)));

		// darker slab under the board: one piece for a full rectangle, otherwise one per cell so it follows the outline
		const FLinearColor SlabColor = Tinted(FLinearColor(0.05f, 0.09f, 0.1f));
		bool bFullBoard = true;
		for (int32 Y = 0; Y < TD::Rows && bFullBoard; ++Y)
		{
			for (int32 X = 0; X < TD::Cols && bFullBoard; ++X)
			{
				bFullBoard = TD::IsOnBoard(FIntPoint(X, Y));
			}
		}
		if (bFullBoard)
		{
			TD::AddShape(this, Root, ETDShape::Cube, FVector(0, 0, -45), FVector(BoardW + 70, BoardH + 70, 80), SlabColor);
		}

		// seabed tiles in slightly varying sand colors
		FRandomStream TileRand(1337);
		for (int32 Y = 0; Y < TD::Rows; ++Y)
		{
			for (int32 X = 0; X < TD::Cols; ++X)
			{
				const FIntPoint Cell(X, Y);
				const float Checker = ((X + Y) % 2 == 0) ? 0.0f : 0.025f;
				const float Noise = TileRand.FRandRange(-0.025f, 0.025f);
				if (!TD::IsOnBoard(Cell))
				{
					continue;
				}
				if (!bFullBoard)
				{
					TD::AddShape(this, Root, ETDShape::Cube, TD::CellToWorld(Cell, -45.0f), FVector(170, 170, 80), SlabColor);
				}
				const FLinearColor Sand(0.2f + Noise + Checker, 0.3f + Noise + Checker, 0.27f + Noise * 0.5f);
				TD::AddShape(this, Root, ETDShape::Cube, TD::CellToWorld(Cell, -6.0f), FVector(97, 97, 12), Tinted(Sand));
			}
		}
	}

	// rocks: a barnacled boulder cluster, turned and sized a little differently on each cell
	for (const FIntPoint& Rock : TD::StartingRocks())
	{
		USceneComponent* Pivot = TD::AddPivot(this, Root, TD::CellToWorld(Rock));
		Pivot->SetRelativeRotation(FRotator(0, Rand.FRandRange(0, 360), 0));
		Pivot->SetRelativeScale3D(FVector(Rand.FRandRange(0.9f, 1.08f)));
		TD::BuildModel(this, Pivot, ETDModel::Boulders, PropOptions);
	}

	// spawn trench: dark recessed pit with a rim of dark stones
	{
		const FIntPoint SpawnCell = TD::SpawnCell();
		const FVector Spawn = TD::CellToWorld(SpawnCell);
		// the trench runs off the board on whichever edge the spawn sits
		const FIntPoint Out = SpawnCell.X == 0 ? FIntPoint(-1, 0) : (SpawnCell.X == TD::Cols - 1 ? FIntPoint(1, 0) : (SpawnCell.Y == 0 ? FIntPoint(0, -1) : FIntPoint(0, 1)));
		// the model's pit runs off towards its -X
		USceneComponent* Pivot = TD::AddPivot(this, Root, Spawn + FVector(0, 0, 0.5f));
		Pivot->SetRelativeRotation(FRotator(0, (-TD::GridDirToWorld(Out)).Rotation().Yaw, 0));
		TD::BuildModel(this, Pivot, ETDModel::Trench, PropOptions);
	}

	// goal: giant clam holding a glowing pearl
	{
		const FVector Goal = TD::CellToWorld(TD::GoalCell());
		USceneComponent* Pivot = TD::AddPivot(this, Root, Goal);
		const FTDModelInstance Clam = TD::BuildModel(this, Pivot, ETDModel::Goal, PropOptions);
		GoalPearl = Clam.Find(TEXT("Pearl"));
		GoalPearlBase = GoalPearl ? GoalPearl->GetRelativeLocation() : FVector::ZeroVector;

		GoalLight = NewObject<UPointLightComponent>(this);
		GoalLight->SetupAttachment(MapRoot);
		GoalLight->SetRelativeLocation(Goal + FVector(0, 0, 70));
		GoalLight->SetIntensity(9000.0f);
		GoalLight->SetAttenuationRadius(420.0f);
		GoalLight->SetLightColor(FLinearColor(0.7f, 1.0f, 0.95f));
		GoalLight->SetCastShadows(false);
		GoalLight->RegisterComponent();
		AddInstanceComponent(GoalLight);
	}

	// swaying kelp behind the two far edges (row -1 and column Cols)
	// Every stalk stands on the open seafloor (top of the floor slab at Z -95, 20 thick). The board's own slab only reaches
	// 35 units past the outer tiles and the edge kelp sits 80+ units out, so planting it at tile height (Z 0) left it floating.
	const float SeabedZ = -85.0f;
	TArray<FVector> KelpSpots;	// so the smaller seafloor decor below keeps clear of the stalks
	auto AddKelp = [this, Root, &Rand, &PropOptions, &KelpSpots](const FVector& Base)
	{
		KelpSpots.Add(Base);
		USceneComponent* Stalk = TD::AddPivot(this, Root, Base);
		Stalk->SetRelativeRotation(FRotator(0, Rand.FRandRange(0, 360), 0));
		const ETDModel Models[] = { ETDModel::KelpShort, ETDModel::KelpMid, ETDModel::KelpTall };
		TD::BuildModel(this, Stalk, Models[Rand.RandRange(0, 2)], PropOptions);
		Kelp.Add(Stalk);
	};

	// about 27% less kelp than before (was 0.75 and 0.4); the same random numbers are drawn, so the layout stays fixed per map
	const float KelpEdgeChance = 0.55f;
	const float KelpOpenChance = 0.29f;
	auto NearEnds = [](const FIntPoint& Cell)
	{
		return FVector2D::Distance(FVector2D(Cell), FVector2D(TD::GoalCell())) < 2.0 || FVector2D::Distance(FVector2D(Cell), FVector2D(TD::SpawnCell())) < 2.0;
	};
	for (int32 X = -1; X <= TD::Cols; ++X)
	{
		if (Rand.FRand() < KelpEdgeChance && !NearEnds(FIntPoint(X, -1)))
		{
			AddKelp(TD::CellToWorld(FIntPoint(X, -1), SeabedZ) + FVector(Rand.FRandRange(-20, 40), Rand.FRandRange(-30, 30), 0));
		}
	}
	for (int32 Y = 0; Y < TD::Rows; ++Y)
	{
		if (NearEnds(FIntPoint(TD::Cols, Y)))
		{
			continue;	// keep the trench and the clam visible
		}
		if (Rand.FRand() < KelpEdgeChance)
		{
			AddKelp(TD::CellToWorld(FIntPoint(TD::Cols, Y), SeabedZ) + FVector(Rand.FRandRange(-30, 30), Rand.FRandRange(-20, 40), 0));
		}
	}

	// open water cut out of a shaped map: sparse kelp growing up from the deep floor, so the outline reads as intended
	for (int32 Y = 0; Y < TD::Rows; ++Y)
	{
		for (int32 X = 0; X < TD::Cols; ++X)
		{
			const FIntPoint Cell(X, Y);
			if (!TD::IsOnBoard(Cell) && !NearEnds(Cell) && Rand.FRand() < KelpOpenChance)
			{
				AddKelp(TD::CellToWorld(Cell, SeabedZ) + FVector(Rand.FRandRange(-12, 12), Rand.FRandRange(-12, 12), 0));
			}
		}
	}

	// seafloor decor beside the kelp: rocks, coral, shells, starfish and anemones, only on the open floor off the board
	// (never on a tile, the slab under it, the trench or the clam), seeded per map so every map keeps its own layout
	{
		FRandomStream DecorRand(4242 + BuiltMap * 131);
		TArray<FVector> BoardCells;
		for (int32 Y = 0; Y < TD::Rows; ++Y)
		{
			for (int32 X = 0; X < TD::Cols; ++X)
			{
				if (TD::IsOnBoard(FIntPoint(X, Y)))
				{
					BoardCells.Add(TD::CellToWorld(FIntPoint(X, Y)));
				}
			}
		}
		// the slab reaches 35 units past each board tile (85 from its centre), so anything closer would sit inside it
		auto ClearOfBoard = [&BoardCells](const FVector& P, float Radius)
		{
			for (const FVector& C : BoardCells)
			{
				if (FMath::Abs(P.X - C.X) < 85.0f + Radius && FMath::Abs(P.Y - C.Y) < 85.0f + Radius)
				{
					return false;
				}
			}
			return true;
		};
		const FVector SpawnWorld = TD::CellToWorld(TD::SpawnCell());
		const FVector GoalWorld = TD::CellToWorld(TD::GoalCell());

		struct FSpot { FVector Pos; float Radius; };
		TArray<FSpot> Placed;
		for (const FVector& K : KelpSpots)
		{
			Placed.Add({ K, 22.0f });
		}

		// three bubbling vents a little way off the board (placed before the decor, which keeps clear of them)
		TArray<FVector> Vents;
		const float HalfXV = TD::Rows * TD::TileSize * 0.5f;
		const float HalfYV = TD::Cols * TD::TileSize * 0.5f;
		for (int32 Attempt = 0; Attempt < 400 && Vents.Num() < 3; ++Attempt)
		{
			const FVector P(DecorRand.FRandRange(-HalfXV - 220.0f, HalfXV + 220.0f), DecorRand.FRandRange(-HalfYV - 220.0f, HalfYV + 220.0f), SeabedZ);
			bool bOk = ClearOfBoard(P, 40.0f) && FVector::Dist2D(P, SpawnWorld) > 320.0f && FVector::Dist2D(P, GoalWorld) > 240.0f;
			for (const FSpot& S : Placed)
			{
				bOk &= FVector::Dist2D(P, S.Pos) > S.Radius + 50.0f;
			}
			for (const FVector& V : Vents)
			{
				bOk &= FVector::Dist2D(P, V) > 500.0f;	// spread round the board
			}
			if (!bOk)
			{
				continue;
			}
			Vents.Add(P);
			Placed.Add({ P, 34.0f });
			// a low dark mound with a ring of stones round its mouth
			TArray<FTDMergedPart> Parts;
			auto Part = [&Parts](ETDShape Shape, const FVector& Loc, const FVector& Size, const FLinearColor& Color)
			{
				FTDMergedPart& Q = Parts.AddDefaulted_GetRef();
				Q.Shape = Shape;
				Q.Location = Loc;
				Q.Size = Size;
				Q.Color = Color;
			};
			Part(ETDShape::Sphere, FVector(0, 0, 2.0f), FVector(46.0f, 44.0f, 18.0f), FLinearColor(0.1f, 0.11f, 0.12f));
			Part(ETDShape::Cylinder, FVector(0, 0, 8.5f), FVector(12.0f, 12.0f, 4.0f), FLinearColor(0.02f, 0.03f, 0.04f));
			for (int32 i = 0; i < 7; ++i)
			{
				const float A = 2.0f * PI * i / 7.0f;
				Part(ETDShape::Sphere, FVector(FMath::Cos(A) * 11.0f, FMath::Sin(A) * 11.0f, 7.5f), FVector(7.0f, 6.0f, 4.5f), FLinearColor(0.16f, 0.15f, 0.14f));
			}
			USceneComponent* VentPivot = TD::AddPivot(this, Root, P);
			VentPivot->SetRelativeRotation(FRotator(0, DecorRand.FRandRange(0, 360), 0));
			TD::AddMergedMesh(this, VentPivot, TD::GetDetailedMergedMesh(DecorKeyBase + 100, Parts), true);
		}

		// rocks and coral most often, the flat pieces and anemones scattered between
		const EDecor Weighted[] = { EDecor::Rocks, EDecor::Rocks, EDecor::Rocks, EDecor::Coral, EDecor::Coral, EDecor::Coral, EDecor::BrainCoral,
			EDecor::Shell, EDecor::Shell, EDecor::Starfish, EDecor::Starfish, EDecor::Anemone, EDecor::Anemone };
		const float HalfX = TD::Rows * TD::TileSize * 0.5f;
		const float HalfY = TD::Cols * TD::TileSize * 0.5f;
		const float Band = 260.0f;	// how far past the board's edge decor may go (the camera pans only 150 past it)
		constexpr int32 Wanted = 64;
		int32 Count = 0;
		for (int32 Attempt = 0; Attempt < 2000 && Count < Wanted; ++Attempt)
		{
			const FVector P(DecorRand.FRandRange(-HalfX - Band, HalfX + Band), DecorRand.FRandRange(-HalfY - Band, HalfY + Band), SeabedZ);
			const EDecor Kind = Weighted[DecorRand.RandRange(0, UE_ARRAY_COUNT(Weighted) - 1)];
			const int32 Variant = DecorRand.RandRange(0, DecorVariants - 1);
			const float Scale = DecorRand.FRandRange(1.1f, 1.55f);
			const float Yaw = DecorRand.FRandRange(0.0f, 360.0f);
			const float Radius = DecorRadius(Kind) * Scale;

			if (!ClearOfBoard(P, Radius) || FVector::Dist2D(P, SpawnWorld) < 280.0f || FVector::Dist2D(P, GoalWorld) < 200.0f)
			{
				continue;
			}
			bool bCrowded = false;
			for (const FSpot& S : Placed)
			{
				bCrowded |= FVector::Dist2D(P, S.Pos) < S.Radius + Radius + 10.0f;
			}
			if (bCrowded)
			{
				continue;
			}
			Placed.Add({ P, Radius });
			++Count;

			USceneComponent* Pivot = TD::AddPivot(this, Root, P);
			Pivot->SetRelativeRotation(FRotator(0, Yaw, 0));
			Pivot->SetRelativeScale3D(FVector(Scale));
			const uint64 Key = DecorKeyBase + (uint64)Kind * 8 + Variant;
			const bool bShadows = Kind != EDecor::Shell && Kind != EDecor::Starfish;
			TD::AddMergedMesh(this, Pivot, TD::GetDetailedMergedMesh(Key, MakeDecorParts(Kind, Variant)), bShadows);
			if (Kind == EDecor::Anemone)
			{
				Anemones.Add(Pivot);
			}
		}

		BuildAmbience(Root, ClearOfBoard, Vents);
	}
}

// ---------------------------------------------------------------------------------------------
// Living seabed: everything off the board, no collision, slow, muted and cheap (a few instanced components)
// ---------------------------------------------------------------------------------------------

float ATDGrid::MaxAmbientZ(const FVector& P)
{
	// the camera looks from the -X / -Y side: something raised on that side is drawn further up the screen, over the board,
	// so it must stay below the line from its spot to the board's near edge (about tan 35 = 0.7 of its distance out)
	const float HalfX = TD::Rows * TD::TileSize * 0.5f;
	const float HalfY = TD::Cols * TD::TileSize * 0.5f;
	const bool bNearSide = P.X < -HalfX || P.Y < -HalfY;
	if (!bNearSide)
	{
		return 400.0f;
	}
	const float Out = FMath::Max(-P.X - HalfX, -P.Y - HalfY);
	return FMath::Min(400.0f, 0.7f * Out - 20.0f);
}

void ATDGrid::BuildAmbience(USceneComponent* Root, const TFunctionRef<bool(const FVector&, float)>& ClearOfBoard, const TArray<FVector>& Vents)
{
	FRandomStream R(777 + BuiltMap * 53);
	const float HalfX = TD::Rows * TD::TileSize * 0.5f;
	const float HalfY = TD::Cols * TD::TileSize * 0.5f;
	const float FloorZ = -85.0f;	// top of the open seafloor
	const FVector SpawnWorld = TD::CellToWorld(TD::SpawnCell());
	const FLinearColor Tint = TD::MapTint(BuiltMap) * TD::AbyssTint(BuiltTier);
	auto Tinted = [&Tint](const FLinearColor& C) { return FLinearColor(C.R * Tint.R, C.G * Tint.G, C.B * Tint.B, C.A); };
	// distance outside the board's rectangle (negative inside)
	auto OutOfRect = [HalfX, HalfY](const FVector& P) { return FMath::Max(FMath::Abs(P.X) - HalfX, FMath::Abs(P.Y) - HalfY); };

	// ---- dunes: broad, low swells of sand further out, darker the further they are, so the floor fades into the deep ----
	for (int32 Attempt = 0, Count = 0; Attempt < 300 && Count < 26; ++Attempt)
	{
		const FVector P(R.FRandRange(-HalfX - 1700.0f, HalfX + 1700.0f), R.FRandRange(-HalfY - 1700.0f, HalfY + 1700.0f), 0.0f);
		const float Out = OutOfRect(P);
		if (Out < 340.0f || FVector::Dist2D(P, SpawnWorld) < 600.0f)
		{
			continue;
		}
		++Count;
		const float Len = R.FRandRange(320.0f, 720.0f);
		const float H = R.FRandRange(40.0f, 70.0f);	// mostly sunk into the floor: a gentle swell 4-7 units high, not a boulder
		const float Far = FMath::Clamp((Out - 340.0f) / 1200.0f, 0.0f, 1.0f);
		const FLinearColor Sand = Tinted(FMath::Lerp(FLinearColor(0.045f, 0.125f, 0.13f), FLinearColor(0.028f, 0.09f, 0.1f), Far));
		UStaticMeshComponent* Dune = TD::AddShape(this, Root, ETDShape::Sphere, FVector(P.X, P.Y, FloorZ - H * 0.4f),
			FVector(Len, Len * R.FRandRange(0.45f, 0.8f), H), Sand, FRotator(0, R.FRandRange(0, 180), 0));
		Dune->SetCastShadow(false);
	}

	// ---- light shafts from the surface, standing beyond the far edges and leaning away from the board ----
	for (int32 i = 0; i < 7; ++i)
	{
		const bool bAlongX = i % 2 == 0;	// on the +X (far) side, else the +Y side
		const float Along = R.FRandRange(-0.8f, 1.1f) * (bAlongX ? HalfY : HalfX);
		const float Out = R.FRandRange(170.0f, 460.0f);
		const FVector Base = bAlongX ? FVector(HalfX + Out, Along, FloorZ) : FVector(Along, HalfY + Out, FloorZ);
		if (FVector::Dist2D(Base, SpawnWorld) < 320.0f)
		{
			continue;
		}
		USceneComponent* Pivot = TD::AddPivot(this, Root, Base);
		const float Lean = R.FRandRange(10.0f, 18.0f);
		const FRotator BaseRot = bAlongX ? FRotator(-Lean, 0, 0) : FRotator(0, 0, -Lean);
		Pivot->SetRelativeRotation(BaseRot);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(RangeMaterial(), this);
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.55f, 0.88f, 1.0f));
		Mat->SetScalarParameterValue(TEXT("Opacity"), 0.03f);
		// nested faint beams, so the shaft fades out towards its sides; its foot sits under the floor so no end cap shows
		const float Radius = R.FRandRange(45.0f, 100.0f);
		for (int32 Layer = 0; Layer < 4; ++Layer)
		{
			const float W = Radius * (2.2f - Layer * 0.5f);
			UStaticMeshComponent* Beam = TD::AddShape(this, Pivot, ETDShape::Cylinder, FVector(0, 0, 775.0f), FVector(W, W, 1600.0f), FLinearColor::White, FRotator::ZeroRotator, Mat);
			Beam->SetCastShadow(false);
		}
		Shafts.Add(Pivot);
		ShaftMats.Add(Mat);
		ShaftBase.Add(BaseRot);
	}

	// ---- caustics: soft patches of light drifting across the open floor (three nested discs each) ----
	for (int32 Attempt = 0, Count = 0; Attempt < 400 && Count < 16; ++Attempt)
	{
		const float Radius = R.FRandRange(70.0f, 160.0f);
		const FVector P(R.FRandRange(-HalfX - 600.0f, HalfX + 600.0f), R.FRandRange(-HalfY - 600.0f, HalfY + 600.0f), FloorZ + 0.8f);
		if (!ClearOfBoard(P, Radius + 40.0f) || FVector::Dist2D(P, SpawnWorld) < 300.0f)
		{
			continue;
		}
		++Count;
		USceneComponent* Pivot = TD::AddPivot(this, Root, P);
		Pivot->SetRelativeRotation(FRotator(0, R.FRandRange(0, 180), 0));
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(RangeMaterial(), this);
		Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.6f, 0.95f, 1.0f));
		Mat->SetScalarParameterValue(TEXT("Opacity"), 0.03f);
		const float Squash = R.FRandRange(0.55f, 0.85f);
		for (int32 Ring = 0; Ring < CausticRings; ++Ring)
		{
			const float D = Radius * 2.0f * (1.0f - Ring * 0.17f);
			UStaticMeshComponent* Disc = TD::AddShape(this, Pivot, ETDShape::Cylinder, FVector(0, 0, Ring * 0.2f), FVector(D, D * Squash, 1.0f), FLinearColor::White, FRotator::ZeroRotator, Mat);
			Disc->SetCastShadow(false);
			Caustics.Add(Disc);
		}
		CausticMats.Add(Mat);
		CausticBase.Add(P);
	}

	auto MakeInstanced = [this, Root](UStaticMesh* Mesh, UMaterialInterface* Material)
	{
		UInstancedStaticMeshComponent* Comp = NewObject<UInstancedStaticMeshComponent>(this);
		Comp->SetStaticMesh(Mesh);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetCastShadow(false);
		if (Material)
		{
			Comp->SetMaterial(0, Material);
		}
		Comp->SetupAttachment(Root);
		Comp->RegisterComponent();
		AddInstanceComponent(Comp);
		return Comp;
	};

	// ---- marine snow: faint specks sinking slowly round the board (never over it) ----
	for (int32 Attempt = 0; Attempt < 1200 && Snow.Num() < 240; ++Attempt)
	{
		const FVector P(R.FRandRange(-HalfX - 800.0f, HalfX + 800.0f), R.FRandRange(-HalfY - 800.0f, HalfY + 800.0f), 0.0f);
		if (OutOfRect(P) < 60.0f)
		{
			continue;
		}
		const float Top = FMath::Min(300.0f, MaxAmbientZ(P));
		if (Top < FloorZ + 40.0f)
		{
			continue;
		}
		Snow.Add({ FVector(P.X, P.Y, Top), (float)R.FRandRange(5.0f, 11.0f), Top - FloorZ, (float)R.FRand(), (float)R.FRandRange(2.0f, 3.6f) });
	}
	SnowISM = MakeInstanced(TD::UnitShapeMesh(ETDShape::Sphere, 4.0f), TD::SharedFxMaterial(FLinearColor(0.75f, 0.92f, 1.0f), 0.5f));

	// ---- bubbles rising from the vents ----
	for (const FVector& Vent : Vents)
	{
		const float Top = FMath::Clamp(MaxAmbientZ(Vent), FloorZ + 120.0f, 320.0f);
		for (int32 i = 0; i < 12; ++i)
		{
			VentBubbles.Add({ Vent + FVector(0, 0, 9.0f), (float)R.FRandRange(22.0f, 34.0f), (float)(Top - (Vent.Z + 9.0f)), (float)((i + R.FRand() * 0.5f) / 12.0f), (float)R.FRandRange(2.5f, 5.5f) });
		}
	}
	BubbleISM = MakeInstanced(TD::UnitShapeMesh(ETDShape::Sphere, 6.0f), TD::SharedFxMaterial(FLinearColor(0.7f, 0.95f, 1.0f), 0.38f));

	// ---- small schools of dark fish circling far round the board ----
	{
		TArray<FTDMergedPart> Parts;
		auto Part = [&Parts](ETDShape Shape, const FVector& Loc, const FVector& Size, const FLinearColor& Color)
		{
			FTDMergedPart& Q = Parts.AddDefaulted_GetRef();
			Q.Shape = Shape;
			Q.Location = Loc;
			Q.Size = Size;
			Q.Color = Color;
		};
		const FLinearColor Body(0.06f, 0.17f, 0.2f);
		Part(ETDShape::Sphere, FVector(0, 0, 0), FVector(18.0f, 6.0f, 7.0f), Body);
		Part(ETDShape::Sphere, FVector(-11.0f, 0, 0), FVector(7.0f, 1.5f, 8.0f), Body);
		Part(ETDShape::Sphere, FVector(1.0f, 0, 3.5f), FVector(6.0f, 1.2f, 3.0f), Body);
		const FTDMergedMesh& FishMesh = TD::GetDetailedMergedMesh(DecorKeyBase + 101, Parts);
		FishISM = MakeInstanced(FishMesh.Mesh, nullptr);
		for (int32 s = 0; s < 3; ++s)
		{
			const float Grow = 330.0f + 90.0f * s;
			const float Dir = s % 2 == 0 ? 1.0f : -1.0f;
			Schools.Add({ FVector2D(HalfX + Grow, HalfY + Grow), 20.0f + 30.0f * s, (float)(Dir * R.FRandRange(0.012f, 0.018f)), (float)R.FRandRange(0.0f, 2.0f * PI) });
			for (int32 i = 0; i < 12; ++i)
			{
				Fish.Add({ s, FVector(R.FRandRange(-60.0f, 60.0f), R.FRandRange(-40.0f, 40.0f), R.FRandRange(-12.0f, 12.0f)), (float)R.FRandRange(0.0f, 2.0f * PI) });
			}
		}
	}

	// ---- a manta gliding slowly past beyond the far edge, now and then ----
	{
		TArray<FTDMergedPart> Parts;
		auto Part = [&Parts](ETDShape Shape, const FVector& Loc, const FVector& Size, const FQuat& Rot)
		{
			FTDMergedPart& Q = Parts.AddDefaulted_GetRef();
			Q.Shape = Shape;
			Q.Location = Loc;
			Q.Size = Size;
			Q.Rotation = Rot;
			Q.Color = FLinearColor(0.03f, 0.09f, 0.11f);
		};
		Part(ETDShape::Sphere, FVector(0, 0, 2.0f), FVector(70.0f, 46.0f, 14.0f), FQuat::Identity);
		Part(ETDShape::Sphere, FVector(-10.0f, 0, 0), FVector(90.0f, 230.0f, 8.0f), FQuat::Identity);
		Part(ETDShape::Sphere, FVector(34.0f, 14.0f, 0), FVector(16.0f, 8.0f, 6.0f), FQuat::Identity);
		Part(ETDShape::Sphere, FVector(34.0f, -14.0f, 0), FVector(16.0f, 8.0f, 6.0f), FQuat::Identity);
		Part(ETDShape::Cylinder, FVector(-100.0f, 0, 0), FVector(2.5f, 2.5f, 110.0f), FQuat(FVector::RightVector, PI * 0.5f));
		Manta = TD::AddPivot(this, Root, FVector::ZeroVector);
		Manta->SetRelativeScale3D(FVector(1.6f));
		TD::AddMergedMesh(this, Manta, TD::GetDetailedMergedMesh(DecorKeyBase + 102, Parts), false);
		MantaX = HalfX + 520.0f;
		MantaSpan = HalfY + 1500.0f;
	}

	// first placement, so nothing pops in on the first frame
	TickAmbience();
}

void ATDGrid::TickAmbience()
{
	const float T = AnimTime;

	// shafts lean a few degrees and breathe in brightness, each on its own slow cycle
	for (int32 i = 0; i < Shafts.Num(); ++i)
	{
		const float Sway = FMath::Sin(T * 0.11f + i * 2.1f) * 3.0f;
		const FRotator& Base = ShaftBase[i];
		Shafts[i]->SetRelativeRotation(FRotator(Base.Pitch + (Base.Pitch != 0.0f ? Sway : 0.0f), 0, Base.Roll + (Base.Roll != 0.0f ? Sway : 0.0f)));
		ShaftMats[i]->SetScalarParameterValue(TEXT("Opacity"), 0.005f + 0.007f * (0.5f + 0.5f * FMath::Sin(T * 0.17f + i * 1.9f)));	// per layer; four stack in the core
	}

	// caustic patches drift in small circles and pulse
	for (int32 i = 0; i < CausticMats.Num(); ++i)
	{
		const float A = T * (0.12f + 0.03f * (i % 3)) + i * 1.7f;
		const FVector Offset(FMath::Cos(A) * 28.0f, FMath::Sin(A * 0.8f) * 28.0f, 0.0f);
		for (int32 Ring = 0; Ring < CausticRings; ++Ring)
		{
			if (UStaticMeshComponent* Disc = Caustics.IsValidIndex(i * CausticRings + Ring) ? Caustics[i * CausticRings + Ring].Get() : nullptr)
			{
				// inner rings wander a little further, so the patch's shape keeps changing
				Disc->GetAttachParent()->SetRelativeLocation(CausticBase[i] + Offset * 0.5f);
				Disc->SetRelativeLocation(FVector(Offset.X * 0.15f * Ring, Offset.Y * 0.15f * Ring, Ring * 0.2f));
			}
		}
		CausticMats[i]->SetScalarParameterValue(TEXT("Opacity"), 0.004f + 0.007f * (0.5f + 0.5f * FMath::Sin(T * 0.23f + i * 2.3f)));	// per ring; five stack in the middle
	}

	auto Drift = [&](UInstancedStaticMeshComponent* Comp, const TArray<FDrifter>& Items, bool bRising, float Wobble, float WobbleRate)
	{
		if (!Comp)
		{
			return;
		}
		AmbientScratch.Reset(Items.Num());
		for (int32 i = 0; i < Items.Num(); ++i)
		{
			const FDrifter& D = Items[i];
			const float F = FMath::Frac(D.Phase + T * D.Speed / FMath::Max(1.0f, D.Span));
			const float Z = bRising ? D.Base.Z + F * D.Span : D.Base.Z - F * D.Span;
			const float Seed = D.Phase * 37.0f;
			const FVector P(D.Base.X + FMath::Sin(T * WobbleRate + Seed) * Wobble, D.Base.Y + FMath::Cos(T * WobbleRate * 0.8f + Seed) * Wobble, Z);
			// bubbles swell a little as they rise
			const float Size = D.Size * (bRising ? 0.8f + 0.4f * F : 1.0f);
			AmbientScratch.Add(FTransform(FRotator::ZeroRotator, P, FVector(Size / 100.0f)));
		}
		if (Comp->GetInstanceCount() != AmbientScratch.Num())
		{
			Comp->ClearInstances();
			Comp->AddInstances(AmbientScratch, false, false, false);
		}
		else
		{
			Comp->BatchUpdateInstancesTransforms(0, AmbientScratch, false, false, true);
		}
		Comp->UpdateBounds();
	};
	Drift(SnowISM, Snow, false, 14.0f, 0.12f);
	Drift(BubbleISM, VentBubbles, true, 5.0f, 1.3f);

	// fish: each school swims round its ellipse, every fish holding its place in the school with a little wiggle
	if (FishISM)
	{
		AmbientScratch.Reset(Fish.Num());
		for (const FFish& F : Fish)
		{
			const FSchool& School = Schools[F.School];
			const float A = School.Phase + T * School.Speed;
			const FVector Center(School.Radii.X * FMath::Cos(A), School.Radii.Y * FMath::Sin(A), School.Z);
			const FVector2D Tangent = FVector2D(-School.Radii.X * FMath::Sin(A), School.Radii.Y * FMath::Cos(A)).GetSafeNormal() * FMath::Sign(School.Speed);
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X));
			const FRotator Heading(0, Yaw, 0);
			const FVector Bob(0, 0, FMath::Sin(T * 0.9f + F.Phase) * 4.0f);
			const FVector P = Center + Heading.RotateVector(F.Offset) + Bob;
			AmbientScratch.Add(FTransform(FRotator(0, Yaw + FMath::Sin(T * 3.0f + F.Phase) * 8.0f, 0), P, FVector(1.3f)));
		}
		if (FishISM->GetInstanceCount() != AmbientScratch.Num())
		{
			FishISM->ClearInstances();
			FishISM->AddInstances(AmbientScratch, false, false, false);
		}
		else
		{
			FishISM->BatchUpdateInstancesTransforms(0, AmbientScratch, false, false, true);
		}
		FishISM->UpdateBounds();
	}

	// the manta crosses beyond the far edge in about two minutes, then comes round again (both ends are far off screen)
	if (Manta && MantaSpan > 0.0f)
	{
		const float Speed = 35.0f;
		const float Y = -MantaSpan + FMath::Fmod(T * Speed, 2.0f * MantaSpan);
		Manta->SetRelativeLocation(FVector(MantaX + FMath::Sin(T * 0.07f) * 60.0f, Y, 180.0f + FMath::Sin(T * 0.4f) * 18.0f));
		Manta->SetRelativeRotation(FRotator(0, 90.0f, FMath::Sin(T * 0.6f) * 7.0f));
	}
}

void ATDGrid::BuildBoardVisuals()
{
	USceneComponent* Root = GetRootComponent();

	// seafloor, board slab and tiles depend on the map (tint and outline), so they're built with the map's other pieces
	BuildMapVisuals();

	// hover highlight
	Highlight = TD::AddShape(this, Root, ETDShape::Cube, FVector::ZeroVector, FVector(94, 94, 2), FLinearColor::Green);
	Highlight->SetVisibility(false);

	// selected tower's tile: walls have no range ring, so this frame is what shows the selection
	SelectionMarker = TD::AddPivot(this, Root, FVector::ZeroVector);
	{
		const FLinearColor Edge(0.45f, 1.0f, 1.0f);
		const float Half = TD::TileSize * 0.5f - 3.0f;
		for (int32 i = 0; i < 4; ++i)
		{
			const bool bAlongY = i < 2;
			const float Side = (i % 2) ? -Half : Half;
			const FVector Pos = bAlongY ? FVector(Side, 0, 5) : FVector(0, Side, 5);
			const FVector Size = bAlongY ? FVector(9, TD::TileSize - 2.0f, 12) : FVector(TD::TileSize - 2.0f, 9, 12);
			UStaticMeshComponent* Bar = TD::AddShape(this, SelectionMarker, ETDShape::Cube, Pos, Size, Edge * 1.6f);
			Bar->SetCastShadow(false);
		}
		UStaticMeshComponent* Fill = TD::AddShape(this, SelectionMarker, ETDShape::Plane, FVector(0, 0, 2.0f), FVector(TD::TileSize - 8.0f, TD::TileSize - 8.0f, 1.0f), Edge);
		SelectionFillMat = UMaterialInstanceDynamic::Create(RangeMaterial(), this);
		SelectionFillMat->SetVectorParameterValue(TEXT("Color"), Edge);
		Fill->SetMaterial(0, SelectionFillMat);
		Fill->SetCastShadow(false);

		// a bobbing pointer above the tower, visible even when walls hide the tile
		SelectionPointer = TD::AddPivot(this, SelectionMarker, FVector(0, 0, 140));
		UStaticMeshComponent* Tip = TD::AddShape(this, SelectionPointer, ETDShape::Cone, FVector::ZeroVector, FVector(46, 46, 52), Edge * 1.6f, FRotator(180, 0, 0));
		Tip->SetCastShadow(false);
	}
	SelectionMarker->SetVisibility(false, true);

	// tinted tiles inside the main range indicator (instanced quads)
	TileTint = NewObject<UInstancedStaticMeshComponent>(this);
	TileTint->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
	TileTint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TileTint->SetCastShadow(false);
	TileTintMat = UMaterialInstanceDynamic::Create(RangeMaterial(), this);
	TileTint->SetMaterial(0, TileTintMat);
	TileTint->SetupAttachment(Root);
	TileTint->RegisterComponent();
	AddInstanceComponent(TileTint);
}

// ---------------------------------------------------------------------------------------------
// Range indicators
// ---------------------------------------------------------------------------------------------

UMaterialInterface* ATDGrid::RangeMaterial() const
{
	// unlit translucent material with Color / Opacity; falls back to the basic shape material
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, RangeMaterialPath))
	{
		return Mat;
	}
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
}

ATDGrid::FRangeIndicator& ATDGrid::GetIndicator(int32 Index)
{
	while (Indicators.Num() <= Index)
	{
		FRangeIndicator& New = Indicators.AddDefaulted_GetRef();

		New.Ring = NewObject<UInstancedStaticMeshComponent>(this);
		New.Ring->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		New.Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		New.Ring->SetCastShadow(false);
		// solid (lit) ring reads best over the seabed; the disc and tile tint stay see-through
		New.RingMat = TD::MakeColorMaterial(this, FLinearColor::White);
		New.Ring->SetMaterial(0, New.RingMat);
		New.Ring->SetupAttachment(GetRootComponent());
		New.Ring->RegisterComponent();
		AddInstanceComponent(New.Ring);
		New.Ring->SetVisibility(false);

		New.Disc = TD::AddShape(this, GetRootComponent(), ETDShape::Cylinder, FVector::ZeroVector, FVector(100, 100, 1), FLinearColor::White);
		New.Disc->SetCastShadow(false);
		New.DiscMat = UMaterialInstanceDynamic::Create(RangeMaterial(), this);
		New.Disc->SetMaterial(0, New.DiscMat);
		New.Disc->SetVisibility(false);
	}
	return Indicators[Index];
}

void ATDGrid::SetRangeViews(const TArray<FTDRangeView>& Views)
{
	for (int32 i = 0; i < FMath::Max(Views.Num(), Indicators.Num()); ++i)
	{
		const bool bUse = Views.IsValidIndex(i) && Views[i].RangeTiles > 0.0f;
		if (!bUse)
		{
			if (Indicators.IsValidIndex(i) && Indicators[i].bVisible)
			{
				Indicators[i].Ring->SetVisibility(false);
				Indicators[i].Disc->SetVisibility(false);
				Indicators[i].bVisible = false;
			}
			continue;
		}

		const FTDRangeView& View = Views[i];
		FRangeIndicator& Ind = GetIndicator(i);
		const float Radius = View.RangeTiles * TD::TileSize;
		const FVector Center(View.Center.X, View.Center.Y, 0.0f);

		// rebuild the ring only when it moved or changed size
		if (!Ind.Center.Equals(Center, 0.5f) || !FMath::IsNearlyEqual(Ind.Radius, Radius, 0.5f))
		{
			Ind.Center = Center;
			Ind.Radius = Radius;

			TArray<FTransform> Segments;
			Segments.Reserve(RingSegments);
			const float SegmentLength = 2.0f * PI * Radius / RingSegments * 1.08f;
			for (int32 Seg = 0; Seg < RingSegments; ++Seg)
			{
				const float Angle = 2.0f * PI * Seg / RingSegments;
				const FVector Pos(Center.X + FMath::Cos(Angle) * Radius, Center.Y + FMath::Sin(Angle) * Radius, RingZ);
				Segments.Add(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f), Pos,
					FVector(SegmentLength, RingWidth, RingHeight) / 100.0f));
			}
			Ind.Ring->ClearInstances();
			Ind.Ring->AddInstances(Segments, false, false, false);

			Ind.Disc->SetRelativeLocation(FVector(Center.X, Center.Y, DiscZ));
			Ind.Disc->SetRelativeScale3D(FVector(Radius * 2.0f / 100.0f, Radius * 2.0f / 100.0f, 0.01f));
		}

		Ind.Color = View.Color;
		if (!Ind.bVisible)
		{
			Ind.Ring->SetVisibility(true);
			Ind.Disc->SetVisibility(true);
			Ind.bVisible = true;
		}
	}

	// tile tint for the first view
	const bool bTint = Views.Num() > 0 && Views[0].bTintTiles && Views[0].RangeTiles > 0.0f;
	if (!bTint)
	{
		if (TileTint->GetInstanceCount() > 0)
		{
			TileTint->ClearInstances();
		}
		TintRadius = -1.0f;
		return;
	}

	const FVector Center(Views[0].Center.X, Views[0].Center.Y, 0.0f);
	const float Radius = Views[0].RangeTiles * TD::TileSize;
	TileTintMat->SetVectorParameterValue(TEXT("Color"), Views[0].Color);
	if (TintCenter.Equals(Center, 0.5f) && FMath::IsNearlyEqual(TintRadius, Radius, 0.5f))
	{
		return;
	}
	TintCenter = Center;
	TintRadius = Radius;

	TArray<FTransform> Tiles;
	for (int32 Y = 0; Y < TD::Rows; ++Y)
	{
		for (int32 X = 0; X < TD::Cols; ++X)
		{
			const FVector Tile = TD::CellToWorld(FIntPoint(X, Y), TintZ);
			if (TD::IsOnBoard(FIntPoint(X, Y)) && FVector::Dist2D(Tile, Center) <= Radius)
			{
				Tiles.Add(FTransform(FRotator::ZeroRotator, Tile, FVector(0.94f, 0.94f, 1.0f)));
			}
		}
	}
	TileTint->ClearInstances();
	TileTint->AddInstances(Tiles, false, false, false);
}

bool ATDGrid::GetPreviewRangeView(FTDRangeView& OutView) const
{
	if (!bPreviewActive || PreviewType == ETDTowerType::Count || !TD::InBounds(PreviewCell))
	{
		return false;
	}
	const float Range = TD::TowerRange(PreviewType, 1);
	if (Range <= 0.0f)
	{
		return false;
	}
	OutView.Center = TD::CellToWorld(PreviewCell);
	OutView.RangeTiles = Range;
	OutView.Color = (PreviewResult == ETDPlaceResult::Ok) ? TDRangeColors::ValidGhost : TDRangeColors::InvalidGhost;
	OutView.bTintTiles = true;
	return true;
}

int32 ATDGrid::GetVisibleRangeCount() const
{
	int32 Count = 0;
	for (const FRangeIndicator& Ind : Indicators)
	{
		Count += Ind.bVisible ? 1 : 0;
	}
	return Count;
}

float ATDGrid::GetFirstRangeTiles() const
{
	return (Indicators.Num() > 0 && Indicators[0].bVisible) ? Indicators[0].Radius / TD::TileSize : 0.0f;
}

int32 ATDGrid::GetTintedTileCount() const
{
	return TileTint ? TileTint->GetInstanceCount() : 0;
}

void ATDGrid::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AnimTime += DeltaTime;

	for (int32 i = 0; i < Kelp.Num(); ++i)
	{
		const float Phase = i * 0.73f;
		const FRotator Rot = Kelp[i]->GetRelativeRotation();
		Kelp[i]->SetRelativeRotation(FRotator(FMath::Sin(AnimTime * 0.9f + Phase) * 7.0f, Rot.Yaw, FMath::Cos(AnimTime * 0.7f + Phase) * 5.0f));
	}
	// anemones lean a little in the same current, slower than the kelp
	for (int32 i = 0; i < Anemones.Num(); ++i)
	{
		const float Phase = i * 1.31f;
		const FRotator Rot = Anemones[i]->GetRelativeRotation();
		Anemones[i]->SetRelativeRotation(FRotator(FMath::Sin(AnimTime * 0.55f + Phase) * 4.0f, Rot.Yaw, FMath::Cos(AnimTime * 0.45f + Phase) * 3.0f));
	}
	TickAmbience();

	if (GoalPearl)
	{
		GoalPearl->SetRelativeLocation(GoalPearlBase + FVector(0, 0, FMath::Sin(AnimTime * 1.6f) * 4.0f));
		GoalPearl->SetRelativeRotation(FRotator(0, AnimTime * 35.0f, 0));
		GoalLight->SetIntensity(8000.0f + FMath::Sin(AnimTime * 2.0f) * 2000.0f);
	}

	// gentle pulse on the range indicators (bright unlit ring, soft disc and tiles)
	const float Pulse = 0.5f + 0.5f * FMath::Sin(AnimTime * 3.0f);

	if (SelectionMarker && TD::InBounds(SelectedCell))
	{
		SelectionFillMat->SetScalarParameterValue(TEXT("Opacity"), 0.35f + 0.25f * Pulse);
		SelectionPointer->SetRelativeLocation(FVector(0, 0, 140.0f + 14.0f * Pulse));
		SelectionPointer->SetRelativeRotation(FRotator(0, AnimTime * 90.0f, 0));
	}
	for (FRangeIndicator& Ind : Indicators)
	{
		if (Ind.bVisible)
		{
			Ind.RingMat->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Ind.Color, FLinearColor::White, 0.15f + 0.35f * Pulse));
			Ind.DiscMat->SetVectorParameterValue(TEXT("Color"), Ind.Color);
			Ind.DiscMat->SetScalarParameterValue(TEXT("Opacity"), 0.10f + 0.06f * Pulse);
		}
	}
	if (TileTintMat && TileTint->GetInstanceCount() > 0)
	{
		TileTintMat->SetScalarParameterValue(TEXT("Opacity"), 0.16f + 0.08f * Pulse);
	}

	// keep the preview fresh as enemies move around
	if (bPreviewActive)
	{
		PreviewRefreshTimer += DeltaTime;
		if (PreviewRefreshTimer > 0.15f)
		{
			PreviewRefreshTimer = 0.0f;
			PreviewVersion = -1;
			UpdatePreview(true, PreviewCell, PreviewType);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Cells
// ---------------------------------------------------------------------------------------------

bool ATDGrid::IsBlocked(const FIntPoint& Cell) const
{
	return !TD::InBounds(Cell) || !Cells.IsValidIndex(TD::CellIndex(Cell)) || Cells[TD::CellIndex(Cell)] != TDCell::Empty;
}

ATDTower* ATDGrid::GetTowerAt(const FIntPoint& Cell) const
{
	for (ATDTower* Tower : Towers)
	{
		if (Tower && Tower->GetCell() == Cell)
		{
			return Tower;
		}
	}
	return nullptr;
}

ETDPlaceResult ATDGrid::EvaluatePlacement(const FIntPoint& Cell, TArray<int32>* OutField) const
{
	TArray<FTDWalker> Walkers;
	if (const ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		for (const ATDEnemy* Enemy : GameMode->GetEnemies())
		{
			if (Enemy && Enemy->IsAlive() && !Enemy->IsFlying())
			{
				Walkers.Add({ Enemy->GetCurrentCell(), Enemy->GetNextCell() });
			}
		}
	}
	return TD::EvaluatePlacement(Cells, Cell, Walkers, OutField);
}

ATDTower* ATDGrid::PlaceTower(const FIntPoint& Cell, ETDTowerType Type)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATDTower* Tower = GetWorld()->SpawnActor<ATDTower>(ATDTower::StaticClass(), TD::CellToWorld(Cell), FRotator::ZeroRotator, Params);
	if (!Tower)
	{
		return nullptr;
	}

	Tower->InitTower(Type, Cell, false);
	Towers.Add(Tower);
	Cells[TD::CellIndex(Cell)] = TDCell::Tower;
	RefreshWalls(Cell);
	RebuildField();
	return Tower;
}

void ATDGrid::RemoveTower(ATDTower* Tower)
{
	if (!Tower)
	{
		return;
	}

	Towers.Remove(Tower);
	const FIntPoint Cell = Tower->GetCell();
	if (TD::InBounds(Cell))
	{
		Cells[TD::CellIndex(Cell)] = TDCell::Empty;
	}
	Tower->Destroy();
	RefreshWalls(Cell);
	RebuildField();
}

bool ATDGrid::CanBuildOnWall(const FIntPoint& Cell, ETDTowerType Type) const
{
	// (Brittle Coral, a cursed boon, turns building onto walls off)
	if (Type == ETDTowerType::Wall || Type == ETDTowerType::Count || !TD::Run().CanBuildOntoWalls())
	{
		return false;
	}
	const ATDTower* Existing = GetTowerAt(Cell);
	if (!Existing || Existing->GetTowerType() != ETDTowerType::Wall)
	{
		return false;
	}
	// building onto a wall sells it: Fixed Towers allows that only while the wall can still be sold
	const ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
	return !GameMode || GameMode->CanSellTower(Existing);
}

ATDTower* ATDGrid::ReplaceWall(const FIntPoint& Cell, ETDTowerType Type)
{
	ATDTower* Wall = GetTowerAt(Cell);
	if (!Wall || !CanBuildOnWall(Cell, Type))
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATDTower* Tower = GetWorld()->SpawnActor<ATDTower>(ATDTower::StaticClass(), TD::CellToWorld(Cell), FRotator::ZeroRotator, Params);
	if (!Tower)
	{
		return nullptr;
	}

	if (HiddenWall.Get() == Wall)
	{
		HiddenWall = nullptr;
	}
	Towers.Remove(Wall);
	Wall->Destroy();

	// the tile stays blocked, so the distance field and route are unchanged
	Tower->InitTower(Type, Cell, false);
	Towers.Add(Tower);
	RefreshWalls(Cell);
	return Tower;
}

void ATDGrid::ShowHiddenWall()
{
	if (ATDTower* Wall = HiddenWall.Get())
	{
		Wall->SetActorHiddenInGame(false);
		TD::SetShadowCopiesEnabled(Wall, true);
	}
	HiddenWall = nullptr;
}

void ATDGrid::SetSelectedCell(const FIntPoint& Cell)
{
	if (!SelectionMarker || Cell == SelectedCell)
	{
		return;
	}
	SelectedCell = Cell;
	const bool bShow = TD::InBounds(Cell);
	SelectionMarker->SetVisibility(bShow, true);
	if (bShow)
	{
		SelectionMarker->SetRelativeLocation(TD::CellToWorld(Cell));
	}
}

uint8 ATDGrid::GetWallLinks(const FIntPoint& Cell) const
{
	const FIntPoint Offsets[] = { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 1), FIntPoint(1, -1), FIntPoint(-1, 1), FIntPoint(-1, -1) };
	uint8 Mask = 0;
	for (int32 d = 0; d < UE_ARRAY_COUNT(Offsets); ++d)
	{
		const ATDTower* Neighbor = GetTowerAt(Cell + Offsets[d]);
		if (Neighbor && Neighbor->GetTowerType() == ETDTowerType::Wall)
		{
			Mask |= 1 << d;
		}
	}
	return Mask;
}

void ATDGrid::RefreshWalls(const FIntPoint& Cell)
{
	const FIntPoint Offsets[] = { FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1), FIntPoint(1, 1), FIntPoint(1, -1), FIntPoint(-1, 1), FIntPoint(-1, -1) };
	for (const FIntPoint& Offset : Offsets)
	{
		ATDTower* Tower = GetTowerAt(Cell + Offset);
		if (Tower && Tower->GetTowerType() == ETDTowerType::Wall)
		{
			Tower->SetWallLinks(GetWallLinks(Cell + Offset));
		}
	}
	// the placement ghost may be standing next to the change
	PreviewVersion = -1;
}

void ATDGrid::ResetBoard()
{
	for (ATDTower* Tower : Towers)
	{
		if (Tower)
		{
			Tower->Destroy();
		}
	}
	Towers.Reset();

	TD::MakeStartingCells(Cells);
	BuildMapVisuals();
	TD::BumpBoardEpoch();

	UpdatePreview(false, FIntPoint(-1, -1), ETDTowerType::Count);
	SetRangeViews({});
	RebuildField();
}

// ---------------------------------------------------------------------------------------------
// Flow field
// ---------------------------------------------------------------------------------------------

int32 ATDGrid::GetDistance(const FIntPoint& Cell) const
{
	return (TD::InBounds(Cell) && Field.IsValidIndex(TD::CellIndex(Cell))) ? Field[TD::CellIndex(Cell)] : -1;
}

bool ATDGrid::GetNextStep(const FIntPoint& From, const FIntPoint& PrevDir, FIntPoint& OutNext) const
{
	return Field.Num() > 0 && TD::StepOnField(Field, From, PrevDir, OutNext);
}

void ATDGrid::RebuildField()
{
	TD::ComputeField(Cells, Field);
	TD::TraceRoute(Field, Route);
	LayoutRouteMarkers(Route, RouteMarkers, 2.5f, 11.0f);
	++FieldVersion;
}

UInstancedStaticMeshComponent* ATDGrid::MakeRouteMarkers(UMaterialInterface* Material)
{
	UInstancedStaticMeshComponent* Markers = NewObject<UInstancedStaticMeshComponent>(this);
	Markers->SetStaticMesh(TD::UnitShapeMesh(ETDShape::Cylinder, 24.0f));
	Markers->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Markers->SetCastShadow(false);
	Markers->SetMaterial(0, Material);
	Markers->SetupAttachment(GetRootComponent());
	Markers->RegisterComponent();
	AddInstanceComponent(Markers);
	return Markers;
}

void ATDGrid::LayoutRouteMarkers(const TArray<FIntPoint>& InRoute, UInstancedStaticMeshComponent* Markers, float Z, float Size)
{
	const FVector Scale(Size / 100.0f, Size / 100.0f, 0.02f);
	TArray<FTransform> Dots;
	Dots.Reserve(InRoute.Num() * 2);
	for (int32 i = 0; i < InRoute.Num(); ++i)
	{
		const FVector Here = TD::CellToWorld(InRoute[i], Z);
		Dots.Add(FTransform(FRotator::ZeroRotator, Here, Scale));
		if (i + 1 < InRoute.Num())
		{
			Dots.Add(FTransform(FRotator::ZeroRotator, (Here + TD::CellToWorld(InRoute[i + 1], Z)) * 0.5f, Scale));
		}
	}
	Markers->ClearInstances();
	Markers->AddInstances(Dots, false, false, false);
}

// ---------------------------------------------------------------------------------------------
// Previews
// ---------------------------------------------------------------------------------------------

void ATDGrid::SetGhostType(ETDTowerType Type)
{
	if (GhostType == Type && Ghost)
	{
		return;
	}

	if (Ghost)
	{
		Ghost->Destroy();
		Ghost = nullptr;
	}

	GhostType = Type;
	if (Type == ETDTowerType::Count)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Ghost = GetWorld()->SpawnActor<ATDTower>(ATDTower::StaticClass(), FVector(0, 0, -1000), FRotator::ZeroRotator, Params);
	if (Ghost)
	{
		Ghost->InitTower(Type, FIntPoint(-1, -1), true);
	}
}

void ATDGrid::UpdatePreview(bool bActive, const FIntPoint& Cell, ETDTowerType Type)
{
	if (!bActive || Type == ETDTowerType::Count || !TD::IsOnBoard(Cell))
	{
		ShowHiddenWall();
		bPreviewOnWall = false;
		bPreviewActive = false;
		PreviewCell = Cell;
		PreviewType = Type;
		PreviewVersion = -1;
		PreviewDelta = 0;
		if (Highlight)
		{
			Highlight->SetVisibility(false);
		}
		if (PreviewMarkers && PreviewMarkers->GetInstanceCount() > 0)
		{
			PreviewMarkers->ClearInstances();
		}
		if (Ghost)
		{
			Ghost->SetActorHiddenInGame(true);
		}
		if (!bActive || Type == ETDTowerType::Count)
		{
			SetGhostType(ETDTowerType::Count);
		}
		return;
	}

	if (bPreviewActive && Cell == PreviewCell && Type == PreviewType && PreviewVersion == FieldVersion)
	{
		return;
	}

	bPreviewActive = true;
	PreviewCell = Cell;
	PreviewType = Type;
	PreviewVersion = FieldVersion;

	TArray<int32> NewField;
	PreviewResult = EvaluatePlacement(Cell, &NewField);

	// a tower can go straight onto a wall: the tile is already blocked, so the path stays as it is
	ShowHiddenWall();
	bPreviewOnWall = PreviewResult == ETDPlaceResult::Occupied && CanBuildOnWall(Cell, Type);
	if (bPreviewOnWall)
	{
		PreviewResult = ETDPlaceResult::Ok;
		NewField = Field;
	}

	const bool bOk = PreviewResult == ETDPlaceResult::Ok;
	const FLinearColor TileColor = bOk ? FLinearColor(0.2f, 0.9f, 0.4f) : FLinearColor(0.95f, 0.15f, 0.12f);

	Highlight->SetVisibility(true);
	Highlight->SetRelativeLocation(TD::CellToWorld(Cell, 1.5f));
	TD::SetShapeColor(Highlight, TileColor);

	// gold route preview and path delta
	if (bOk)
	{
		TArray<FIntPoint> NewRoute;
		TD::TraceRoute(NewField, NewRoute);
		PreviewDelta = (NewRoute.Num() - 1) - GetRouteLength();
		LayoutRouteMarkers(NewRoute, PreviewMarkers, 4.0f, 8.0f);
	}
	else
	{
		PreviewDelta = 0;
		PreviewMarkers->ClearInstances();
	}

	// ghost tower, hidden on occupied tiles so it doesn't clip into a rock or tower
	SetGhostType(Type);
	if (Ghost)
	{
		const bool bShowGhost = PreviewResult != ETDPlaceResult::Occupied && PreviewResult != ETDPlaceResult::SpawnOrGoal;
		Ghost->SetActorHiddenInGame(!bShowGhost);
		Ghost->SetActorLocation(TD::CellToWorld(Cell));
		Ghost->SetWallLinks(GetWallLinks(Cell));
	}

	// hide the wall under the ghost so the preview doesn't clip into the rock
	if (bPreviewOnWall)
	{
		if (ATDTower* Wall = GetTowerAt(Cell))
		{
			Wall->SetActorHiddenInGame(true);
			TD::SetShadowCopiesEnabled(Wall, false);	// hidden shadow casters keep casting
			HiddenWall = Wall;
		}
	}

}

