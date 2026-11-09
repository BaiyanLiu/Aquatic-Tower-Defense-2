// Aquatic Tower Defense - isometric underwater tower defense with mazing.
// Detailed models built from basic shapes. The part tables in TDModelData.cpp are generated from the preview builders.

#pragma once

#include "CoreMinimal.h"
#include "TDTypes.h"

class UStaticMesh;
class UStaticMeshComponent;

enum class ETDModel : uint8
{
	Harpoon,
	Jelly,
	Puffer,
	Eel,
	Mantis,
	Octopus,
	Lionfish,
	Sawfish,
	Humpback,
	AcidVent,
	Crab,
	Piranha,
	Turtle,
	Manta,
	Kraken,
	SeaStar,
	Cuttlefish,
	ShockEel,
	Boulders,
	Trench,
	Goal,
	KelpShort,
	KelpMid,
	KelpTall,
	ShotHarpoon,
	ShotRock,
	ShotLionfish,
	ShotSawfish,
	ShotBomb,
	Spines,
	DropShell,
	DropPearl,
	DropCharm,
	// tower forms, two per tower in TD::Form order (see TD::TowerFormModel)
	HarpoonSwordfish,
	HarpoonNarwhal,
	JellyBox,
	JellyMoon,
	PufferCluster,
	PufferLobber,
	EelStorm,
	EelOverload,
	MantisSmasher,
	MantisSpearer,
	OctopusJuggler,
	OctopusBlueRinged,
	LionfishCloud,
	LionfishParalytic,
	SawfishRipper,
	SawfishSweeper,
	HumpbackSonar,
	HumpbackChorus,
	AcidVentCold,
	AcidVentBlack,
	Count
};

/** How a part is shaded */
enum class ETDPartMaterial : uint8
{
	Lit,	// regular shape colour
	Glow,	// unlit, so it reads as glowing
	Glass	// unlit and see-through, opacity in Color.A
};

namespace TDPartFlag
{
	constexpr uint8 Tip = 1;	// projectile head, gold on crits
	constexpr uint8 Tint = 2;	// takes the item rarity colour
}

struct FTDModelPivot
{
	const TCHAR* Name;
	int32 Parent;
	float L[3];
	float Q[4];
};

struct FTDModelPart
{
	uint8 Pivot;
	uint8 Shape;
	uint8 Mat;
	uint8 Flags;
	float L[3];
	float S[3];
	float Q[4];
	float C[4];
};

struct FTDModel
{
	const FTDModelPivot* Pivots;
	int32 NumPivots;
	const FTDModelPart* Parts;
	int32 NumParts;
};

struct FTDModelOptions
{
	/** Parts smaller than this (largest dimension, uu) are skipped */
	float MinSize = 0.0f;
	bool bShadows = true;
	/** Scales part and pivot offsets on the model root (not part sizes) */
	FVector RootSpread = FVector::OneVector;
	/** Optional recolour, e.g. ghost tint or crit tip */
	TFunction<FLinearColor(const FLinearColor& Color, uint8 Flags)> Recolor;
	/** Called for every created part */
	TFunction<void(UStaticMeshComponent*)> OnPart;
	/** -1 follows the atd.DetailedModels setting, 0 the original shapes, 1 the detailed ones */
	int8 Detail = -1;
};

/** One basic shape going into a merged mesh */
struct FTDMergedPart
{
	ETDShape Shape = ETDShape::Cube;
	ETDPartMaterial Mat = ETDPartMaterial::Lit;
	uint8 Flags = 0;
	FVector Location = FVector::ZeroVector;
	FVector Size = FVector(100.0f);
	FQuat Rotation = FQuat::Identity;
	FLinearColor Color = FLinearColor::White;
	/** Sides of a round shape; 0 picks them from its size */
	int32 Segments = 0;
	/** Rounded edges and smoother curves (see TD::UseDetailedModels) */
	bool bDetailed = false;
};

/** A cached mesh of many basic shapes, with one material section per colour */
struct FTDMergedMesh
{
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** What each material slot stands for, so a recoloured copy can override its materials */
	struct FSlot
	{
		FLinearColor Color;
		ETDPartMaterial Mat;
		uint8 Flags;
	};
	TArray<FSlot> Slots;

	/** A plainer copy for shadows only: one section, fewer sides, no parts under TD::ShadowMinPartSize. AddMergedMesh builds it the first
	 *  time a shadow-casting component needs it (from ShadowParts) and shows it as a hidden shadow caster instead of the full mesh. */
	mutable TObjectPtr<UStaticMesh> ShadowMesh = nullptr;
	mutable TArray<FTDMergedPart> ShadowParts;
};

struct FTDModelInstance
{
	/** Pivot 0 is the parent the model was built on */
	TArray<USceneComponent*> Pivots;
	TArray<FName> PivotNames;

	USceneComponent* Find(FName Name) const;
	TArray<USceneComponent*> FindAll(FName Name) const;
};

namespace TD
{
	const FTDModel& GetModel(ETDModel Id);

	/** One shared unlit material per colour/opacity (glowing and see-through parts). Never modify them. */
	UMaterialInterface* SharedFxMaterial(const FLinearColor& Color, float Opacity);

	/** Builds a model's pivots and parts under Parent: up to two shared merged meshes per pivot rather than a component per part */
	FTDModelInstance BuildModel(AActor* Owner, USceneComponent* Parent, ETDModel Id, const FTDModelOptions& Options = FTDModelOptions());

	/** Visits each merged mesh of a model with its pivot's index and whether it is the pivot's solid mesh. BuildModel makes a component of each.
	 *  Detail: -1 follows atd.DetailedModels, 0 the original shapes, 1 the detailed ones. */
	void ForEachModelMesh(ETDModel Id, float MinSize, const FVector& RootSpread, TFunctionRef<void(int32 PivotIndex, bool bSolid, const FTDMergedMesh& Merged)> Visit, int32 Detail = -1);

	/** Whether models build with the detailed shapes: rounded box edges, smoother curves, rimmed cylinders and the extra detail parts (atd.DetailedModels) */
	bool UseDetailedModels();

	/** Name of a model for labels and logs */
	const TCHAR* ModelName(ETDModel Id);

	/** Merges every tower, creature, projectile and drop model once, so the first one of a kind in a wave doesn't hitch */
	void PrewarmModels();

	/** The cached mesh for Key, merged from Parts the first time. Keys from MakeMergedKey never clash with the models' own. */
	const FTDMergedMesh& GetMergedMesh(uint64 Key, const TArray<FTDMergedPart>& Parts);
	constexpr uint64 MakeMergedKey(uint64 Id) { return (1ull << 63) | Id; }

	/** GetMergedMesh(MakeMergedKey(Id), Parts) for meshes built outside the model tables (platforms, badges, seabed decor), but with the
	 *  detailed shapes and detail parts while atd.DetailedModels is on (cached under its own key) */
	const FTDMergedMesh& GetDetailedMergedMesh(uint64 Id, const TArray<FTDMergedPart>& Parts);

	/** A round shape 100 units across with only as many sides as it needs when shown at Size units */
	UStaticMesh* UnitShapeMesh(ETDShape Shape, float Size);

	/** Parts smaller than this (largest side, uu) are left out of the shadow meshes: speckles, studs, pebbles and tips barely show in a shadow */
	constexpr float ShadowMinPartSize = 6.0f;

	/** Adds a component showing a merged mesh. Recolor (optional) overrides its section colours for this component only.
	 *  With bShadows the shadow comes from a plainer hidden copy (FTDMergedMesh::ShadowMesh) attached under the returned component. */
	UStaticMeshComponent* AddMergedMesh(AActor* Owner, USceneComponent* Parent, const FTDMergedMesh& Merged, bool bShadows,
		const TFunction<FLinearColor(const FLinearColor& Color, uint8 Flags)>& Recolor = nullptr);

	/** Destroys a component from AddMergedMesh together with its shadow copy */
	void DestroyMergedMesh(UStaticMeshComponent* Comp);

	/** Turns the shadow copies of an actor's merged meshes on or off (e.g. while the actor is hidden: hidden shadow casters still cast) */
	void SetShadowCopiesEnabled(AActor* Actor, bool bEnabled);
}
