// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "TDTypes.h"
#include "TDItems.h"

class UTexture2D;

/** Icon families. Ids inside a family are the matching enum value (skills: tower * SkillsPerTower + index). */
enum class ETDIcon : uint8
{
	Tower,
	Enemy,
	Skill,
	Item,
	Misc,
	ItemKind,	// one per item type (id = TD::ItemKind index)
	Boon		// one per run boon, cursed ones included (id = ETDBoon)
};

enum class ETDMiscIcon : uint8
{
	Pearl,
	Life,
	Wave,
	SkillPoint,
	Logo,
	Count
};

/**
 *  Icons painted in code from signed distance shapes (circles, capsules, polygons, arcs). Each shape is
 *  lit as a rounded form with a tinted ink line, details are clipped inside their parent shape, and a dark
 *  outline around the whole icon makes it read on any panel.
 *  Textures are generated on first use, with a full mip chain so they stay crisp at any UI scale.
 */
namespace TDIcons
{
	constexpr int32 TextureSize = 256;

	/** Number of ids in a family */
	int32 Count(ETDIcon Kind);

	/** True when atd.DetailedIcons asks for the detailed look */
	bool UseDetailed();

	/** Paints an icon (straight alpha, sRGB). Returns false if the id has no icon. Detail: -1 follows atd.DetailedIcons, 0 original, 1 detailed. */
	bool Render(ETDIcon Kind, int32 Id, int32 Size, TArray<FColor>& OutPixels, int32 Detail = -1);

	/** Cached texture for an icon, or null if the id has no icon */
	UTexture2D* Get(ETDIcon Kind, int32 Id);

	inline UTexture2D* Tower(ETDTowerType Type) { return Get(ETDIcon::Tower, (int32)Type); }
	inline UTexture2D* Enemy(ETDEnemyType Type) { return Get(ETDIcon::Enemy, (int32)Type); }
	inline UTexture2D* Skill(ETDTowerType Type, int32 Index) { return Get(ETDIcon::Skill, (int32)Type * TD::SkillsPerTower + Index); }
	inline UTexture2D* Item(ETDItemSlot Slot) { return Get(ETDIcon::Item, (int32)Slot); }

	/** The item type's own icon, or its slot's icon for old untyped items */
	inline UTexture2D* ForItem(const FTDItem& It)
	{
		UTexture2D* Texture = TD::ItemKind(It.Kind) ? Get(ETDIcon::ItemKind, It.Kind) : nullptr;
		return Texture ? Texture : Item(It.Slot);
	}
	inline UTexture2D* Misc(ETDMiscIcon Icon) { return Get(ETDIcon::Misc, (int32)Icon); }

#if !UE_BUILD_SHIPPING
	/** Writes contact sheets of every icon (full size and HUD size) and the Windows application icon into a folder */
	bool Export(const FString& Directory);

	/** Writes before/after sheets (original left, detailed right) of every icon family into a folder */
	bool ExportCompare(const FString& Directory);
#endif
}
