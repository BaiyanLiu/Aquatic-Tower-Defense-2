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

/** What a lit part is made of in the detailed models: picks its roughness, metalness and surface relief (M_ATD_Detail).
 *  Auto lets the detail pass decide from the part's shape, colour and place in the model. */
enum class ETDSurface : uint8
{
	Auto,
	Plain,		// painted, smooth
	Skin,		// creature skin: wet sheen and fine pores
	Eye,		// glossy
	Stone,		// rough, chiselled; round stones get a lumpy outline
	Sand,
	Wood,		// grain
	Metal,
	Brass,
	Moss,		// fuzzy
	Shell,		// ridged, glossy
	Coral,
	Brain,		// brain coral grooves
	Count
};

struct FTDSculptMesh;

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
	/** What the part is made of in the detailed models */
	ETDSurface Surface = ETDSurface::Auto;
	/** Melted together with the pivot's other sculpted parts into one smooth surface (creature bodies, see TDSculpt) */
	bool bSculpt = false;
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
		/** Plain for the original look; anything else uses the detail material */
		ETDSurface Surface = ETDSurface::Auto;
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

/** A painted surface (see TD::MakeSurfaceMesh) whose geometry is made on a worker thread */
struct FTDSurfaceJob
{
	/** Fills the surface; runs on a worker thread, so it may only read data that stays put */
	TFunction<void(FTDSculptMesh&)> Build;
	ETDSurface Look = ETDSurface::Sand;
	/** Gets the finished mesh (null when the surface came out empty or there is no detail material), on the game thread */
	TFunction<void(UStaticMesh*)> OnBuilt;
};

/** Meshes outside the model tables that TD::PrewarmModels makes on all cores along with the models, so the first board needs no slow
 *  work of its own */
struct FTDPrewarmExtras
{
	/** Sculpted props baked under Content/ATDModels (the names TD::GetBakedPropMesh takes) */
	TArray<FString> BakedProps;
	TArray<FTDSurfaceJob> Surfaces;
};

namespace TD
{
	const FTDModel& GetModel(ETDModel Id);

	/** One shared lit material per colour and surface from M_ATD_Detail (vertex colour times Color, with the surface's roughness,
	 *  metalness and relief), or null when that material asset is missing. Never modify them. */
	UMaterialInterface* SharedDetailMaterial(const FLinearColor& Color, ETDSurface Surface);

	/** One shared unlit material per colour/opacity (glowing and see-through parts). Never modify them. */
	UMaterialInterface* SharedFxMaterial(const FLinearColor& Color, float Opacity);

	/** Builds a model's pivots and parts under Parent: up to two shared merged meshes per pivot rather than a component per part */
	FTDModelInstance BuildModel(AActor* Owner, USceneComponent* Parent, ETDModel Id, const FTDModelOptions& Options = FTDModelOptions());

	/** Visits each merged mesh of a model with its pivot's index and whether it is the pivot's solid mesh. BuildModel makes a component of each.
	 *  Detail: -1 follows atd.DetailedModels, 0 the original shapes, 1 the detailed ones. */
	void ForEachModelMesh(ETDModel Id, float MinSize, const FVector& RootSpread, TFunctionRef<void(int32 PivotIndex, bool bSolid, const FTDMergedMesh& Merged)> Visit, int32 Detail = -1);

	/** Whether models load the sculpted meshes baked from the way-more-detail preview (atd.SculptedModels, Content/ATDModels) where one exists.
	 *  They replace the detailed shapes, so they need atd.DetailedModels on as well. */
	bool UseSculptedModels();

	/** Whether models build with the detailed shapes: rounded box edges, smoother curves, rimmed cylinders and the extra detail parts (atd.DetailedModels) */
	bool UseDetailedModels();

	/** Name of a model for labels and logs */
	const TCHAR* ModelName(ETDModel Id);

	/** Merges every tower, creature, projectile, drop and board prop model once, so the first one of a kind in a wave doesn't hitch,
	 *  together with Extras, all on every core */
	void PrewarmModels(const FTDPrewarmExtras& Extras = FTDPrewarmExtras());

	/** Waits for the models PrewarmModels left to the worker threads and finishes them now (at once when they're done). Building a model
	 *  that isn't merged yet does this by itself; leaving the main menu does it so the first builds never wait. */
	void FinishModelPrewarm();

	/** The cached mesh for Key, merged from Parts the first time. Keys from MakeMergedKey never clash with the models' own. */
	const FTDMergedMesh& GetMergedMesh(uint64 Key, const TArray<FTDMergedPart>& Parts);
	constexpr uint64 MakeMergedKey(uint64 Id) { return (1ull << 63) | Id; }

	/** The cached mesh for Key, or null when it hasn't been merged yet */
	const FTDMergedMesh* FindMergedMesh(uint64 Key);

	/** Merges Parts into Key's mesh in the background: the geometry (and any sculpting, which can take hundreds of milliseconds) on a worker
	 *  thread, then the mesh itself on the game thread, at most one per frame. OnReady runs on the game thread once the mesh is cached
	 *  (at once when it already is). A second request for a key on its way only adds its OnReady. */
	void RequestMergedMeshAsync(uint64 Key, TArray<FTDMergedPart>&& Parts, TFunction<void()> OnReady = nullptr);

	/** Builds a merged mesh's shadow copy now instead of when the first shadow-casting component shows it */
	void PrewarmShadowMesh(const FTDMergedMesh& Merged);

	/** GetMergedMesh(MakeMergedKey(Id), Parts) for meshes built outside the model tables (platforms, badges, seabed decor), but with the
	 *  detailed shapes and detail parts while atd.DetailedModels is on (cached under its own key) */
	const FTDMergedMesh& GetDetailedMergedMesh(uint64 Id, const TArray<FTDMergedPart>& Parts);

	/** A seafloor prop sculpted as one connected surface and baked under Content/ATDModels (e.g. "DecorRocks0" for DecorRocks0.atdm),
	 *  while atd.DetailedModels and atd.SculptedModels are on; null when they are off or the file is missing, so the caller builds its shapes */
	const FTDMergedMesh* GetBakedPropMesh(const TCHAR* Name);

	/** A static mesh of a vertex-coloured surface (TDSculpt's layout: colours already decoded for the mesh build) on the detail material
	 *  with the given surface, one level of detail; null without the detail material */
	UStaticMesh* MakeSurfaceMesh(const FTDSculptMesh& Surface, ETDSurface Look);

	/** Runs surface jobs on all cores and finishes their meshes here (their OnBuilt run before this returns) */
	void MakeSurfaceMeshes(TArray<FTDSurfaceJob>&& Jobs);

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
