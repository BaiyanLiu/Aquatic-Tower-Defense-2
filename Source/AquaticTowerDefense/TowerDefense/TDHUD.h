// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TDTypes.h"
#include "TDItems.h"
#include "TDHUD.generated.h"

class ATDGameMode;
class ATDPlayerController;
class ATDTower;
class ATDEnemy;
class UFont;
class UTexture2D;

/** Same as TDHUDStyle::SizeField (the style header isn't included here) */
constexpr float TDHUDFieldSize = 16.0f;

/**
 *  Canvas-drawn HUD: top bar, tower dock, tower panel (Stats | Items | Skills), master storage,
 *  item tooltips, toasts, health bars, floating texts and the game-over overlay.
 *  Buttons are plain rectangles recorded while drawing and hit-tested by the player controller on click.
 */
UCLASS()
class ATDHUD : public AHUD
{
	GENERATED_BODY()

public:

	virtual void DrawHUD() override;

	/** Handles a left click at a screen position. Returns true if the HUD consumed it. */
	bool HandleClick(const FVector2D& ScreenPos);

	/** Handles a right click (salvage storage items). Returns true if the HUD consumed it. */
	bool HandleRightClick(const FVector2D& ScreenPos);

	/** True if the position is over any HUD panel (so the board should ignore the mouse) */
	bool IsOverUI(const FVector2D& ScreenPos) const;

	void ShowToast(const FString& Message, const FLinearColor& Color);

	/** The wave-clear card at the reef (replaces one still showing) */
	void ShowWaveClear(int32 WaveNumber, int32 PearlsGained, int32 Interest, int32 Xp, int32 LivesGained);
	bool IsWaveClearCardVisible() const { return Now() - WaveCard.Start < WaveCardSeconds; }
	static constexpr double WaveCardSeconds = 2.0;
	void AddFloatingText(const FVector& WorldLocation, const FString& Text, const FLinearColor& Color);

	/** Flashes "-N" next to the reef lives */
	void FlashLivesLost(int32 Amount);

	/** Ends a settings slider drag (left button released) */
	void EndDrag() { ActiveSlider = -1; }

	enum class EButton : uint8
	{
		Speed,
		Pause,
		Wave,
		Slot,
		Sell,
		Restart,
		Tab,
		LearnSkill,
		EquippedItem,
		StorageItem,
		StorageToggle,
		Sort,
		ItemEquip,
		ItemUpgrade,
		ItemReroll,
		ItemSalvage,
		MenuNewGame,
		MenuLoad,
		MenuSave,
		MenuSettings,
		MenuQuit,
		MenuResume,
		MenuMainMenu,
		SettingAdjust,
		SettingSlider,
		SettingsBack,
		PreviewEntry,
		ToggleShowFps,
		ToggleVSync,
		ToggleAA,

		MenuControls,
		BindKey,
		ResetKeys,
		ControlsBack,
		MenuEndless,
		ItemLock,
		CycleAutoSalvage,
		ChooseForm,
		SetupMap,		// Param: map index, -1 = random
		SetupTier,		// Param: Abyss tier
		SetupMutator,	// Param: ETDMutator
		SetupStart,
		SetupBack,
		PickBoon,		// Param: ETDBoon
		MenuMastery,
		MasteryBack,
		SlotLoad,		// Param: slot
		SlotDelete,		// Param: slot
		SlotBack,
		SetupSlot,		// Param: slot
		ConfirmYes,
		ConfirmNo,
		Retry,
		TargetCycle	// Param: -1 previous, 1 next
	};

	/** For the UI test: centre of a button drawn last frame */
	bool FindButton(EButton Id, int32 Param, FVector2D& OutCenter) const;

	/** For the UI test: title of the tooltip shown last frame (empty if none) */
	const FString& GetLastTipTitle() const { return LastTipTitle; }

protected:

	struct FButton
	{
		FBox2D Rect;
		EButton Id;
		int32 Param;
		bool bEnabled;
	};

	struct FToast
	{
		FString Text;
		FLinearColor Color;
		double Start;
	};

	struct FFloater
	{
		FVector World;
		FString Text;
		FLinearColor Color;
		double Start;
	};

	void DrawWorldOverlays(ATDGameMode* GameMode);
	void DrawTopBar(ATDGameMode* GameMode, ATDPlayerController* PC);
	void DrawDock(ATDGameMode* GameMode, ATDPlayerController* PC);
	void DrawInfoPanel(ATDGameMode* GameMode, ATDPlayerController* PC);
	void DrawTowerPanel(ATDGameMode* GameMode, ATDPlayerController* PC, ATDTower* Tower);
	void DrawStorage(ATDGameMode* GameMode, ATDPlayerController* PC);
	void DrawTooltip();
	void DrawToasts();

	/** The wave-clear card: slides in beside the reef, holds, fades */
	void DrawWaveClearCard(ATDGameMode* GameMode);
	struct FWaveCard
	{
		int32 Wave = 0;
		int32 Pearls = 0;
		int32 Interest = 0;
		int32 Xp = 0;
		int32 Lives = 0;
		double Start = -100.0;
	};
	FWaveCard WaveCard;
	void DrawCursorHint(ATDGameMode* GameMode, ATDPlayerController* PC);
	void DrawGameOver(ATDGameMode* GameMode);

	/** Run summary (time, top towers, pearls by source, leaks, map record) in a box of width W from (X, Y). Returns its height. */
	float DrawRunSummary(ATDGameMode* GameMode, float X, float Y, float W);
	static constexpr float RunSummaryHeight = 306.0f;	// before UI scale: fits three top towers or all seven pearl sources, and the score

	void DrawMenu(ATDGameMode* GameMode);

	/** New game setup: map cards, Abyss tier, mutators, score multiplier, Start / Back */
	void DrawSetupMenu(ATDGameMode* GameMode);

	/** Boon offer: three cards, one to pick */
	void DrawBoonMenu(ATDGameMode* GameMode);

	/** Tower mastery: lifetime kills, rank and progress per attack tower */
	void DrawMasteryMenu(ATDGameMode* GameMode);

	/** Save slot picker: three cards with what each slot holds, Load and Delete */
	void DrawSlotMenu(ATDGameMode* GameMode);

	/** Yes / No question over whatever is on screen (overwriting or deleting a slot) */
	void DrawConfirm(ATDGameMode* GameMode);

	/** One line about a save slot ("Reef Channel, wave 34, Abyss 2") */
	static FString SlotSummary(const struct FTDSlotInfo& Info);

	/** Small padlock drawn inside a rectangle */
	void Padlock(const FBox2D& Rect, const FLinearColor& Color);

	/** Small board outline of a map (seabed, rocks, trench and reef) inside a rectangle */
	void DrawMapThumbnail(int32 Map, const FBox2D& Rect, float Alpha);

	/** Score, multiplier and taken boons under the top-left cluster during play */
	void DrawRunStrip(ATDGameMode* GameMode);

	/** Icon shown for a boon (reuses tower, skill, item and misc icons) */
	static UTexture2D* BoonIcon(int32 Boon);
	void DrawEnemyPanel(ATDGameMode* GameMode, ATDEnemy* Enemy);
	void DrawWavePreview(ATDGameMode* GameMode);

	/** Top of the tower dock (for laying out panels above it) */
	float DockTop() const;

	/** Generic tooltip shown at the mouse this frame (first line is the title) */
	void SetTip(const TArray<TPair<FString, FLinearColor>>& Lines, bool bColorizeEffects = false);

	/** The current tip's description rows (after the title) color their effect words in their stat colors */
	bool bTipColorize = false;

	/** True while the tower panel is redrawn at a smaller scale so it fits above the dock */
	bool bFittingPanel = false;
	float FittedPanelTop = 0.0f;

	// drawing helpers
	void Panel(const FBox2D& Rect, const FLinearColor& Color, bool bBlocksMouse = true);
	void Fill(const FBox2D& Rect, const FLinearColor& Color);
	void Frame(const FBox2D& Rect, const FLinearColor& Color, float Thickness);
	FVector2D Text(const FString& InText, float X, float Y, const FLinearColor& Color, float Size, bool bCenterX = false, bool bCenterY = false);
	FVector2D MeasureText(const FString& InText, float Size) const;

	/** Largest size up to Size at which the text fits the width */
	float FitSize(const FString& InText, float Size, float MaxWidth) const;

	/** Panel background with its standard one-pixel edge */
	void FramedPanel(const FBox2D& Rect, const FLinearColor& Edge);

	/** Quiet underwater touches on a tall panel: a slowly drifting wave line along the bottom and a few bubbles in the corner */
	void PanelDecor(const FBox2D& Rect, const FLinearColor& Tint);

	/** Slowly animated sea behind the full-screen menus: light shafts, swaying kelp, passing fish and rising bubbles */
	void DrawSeaBackdrop(float Strength);

	/** Tiled gentle wave between X0 and X1 (a thin line, or filled above the curve), Phase in wavelengths */
	void WaveStrip(float X0, float X1, float Y, float Height, bool bFilled, const FLinearColor& Color, float Phase);

	/** Ellipse (or soft glow when bSoft) centred on Center, rotated by AngleDeg */
	void Blob(const FVector2D& Center, const FVector2D& Size, float AngleDeg, const FLinearColor& Color, bool bSoft = false);

	/** Small see-through bubble: faint fill and a thin rim (Alpha scales both) */
	void Bubble(const FVector2D& Center, float Radius, const FLinearColor& Tint, float Alpha = 1.0f);

	/** Tiny five-armed starfish emblem */
	void Starfish(const FVector2D& Center, float Radius, float AngleDeg, const FLinearColor& Color);

	/** Tiny scallop shell emblem: a fan of ribs opening upwards from its hinge at Center */
	void Scallop(const FVector2D& Center, float Radius, const FLinearColor& Color);

	/** Tiny branching coral standing on Base */
	void CoralSprig(const FVector2D& Base, float Height, const FLinearColor& Color);

	/** Faint deep-water vignette at the screen edges and a slow caustic shimmer along the top, under everything else */
	void DrawAmbient();

	/** The game's brand (logo on its plate, "AQUATIC / TOWER DEFENSE", wave and coral) from X, centred on MidY, laid out at Scale
	 *  (UIScale in the top bar). Returns its width; pass bDraw false to only measure. */
	float DrawBrand(float X, float MidY, float Scale, bool bDraw = true);

	/** Draws text word-wrapped to W, starting at (X, Y); returns the height used. Pass bDraw false to only measure. */
	float WrappedText(const FString& InText, float X, float Y, float W, const FLinearColor& Color, float Size, float LineH, bool bDraw = true);

	/** Draws text word-wrapped to W, each line centred on CX, starting at Y; returns the height used. Pass bDraw false to only measure. */
	float WrappedTextCentered(const FString& InText, float CX, float Y, float W, const FLinearColor& Color, float Size, float LineH, bool bDraw = true);

	/** Word-wraps colored pieces to W (a piece keeps its color across lines); returns the height used. Pass bDraw false to only measure. */
	float WrappedColoredText(const TArray<TPair<FString, FLinearColor>>& Parts, float X, float Y, float W, float Size, float LineH, bool bDraw = true);

	/** Panel title row: icon and title on the left, a short note aligned right */
	void PanelHeader(UTexture2D* Texture, float X, float Y, float W, const FString& Title, const FLinearColor& TitleColor, const FString& Note, const FLinearColor& NoteColor);

	/** Dim label on the left, value aligned right */
	void Field(float X, float Y, float W, const FString& Label, const FString& Value, const FLinearColor& ValueColor, float Size = TDHUDFieldSize);

	/** Two fields side by side; moves Y down one line */
	void FieldRow(float X, float& Y, float W, float Line, const FString& Label1, const FString& Value1, const FString& Label2, const FString& Value2,
		const FLinearColor& Color1 = FLinearColor::White, const FLinearColor& Color2 = FLinearColor::White);


	/** Filled bar with a centered label */
	void Meter(const FBox2D& Rect, float Fraction, const FLinearColor& Color, const FString& Label);

	void Button(const FBox2D& Rect, const FString& Label, EButton Id, int32 Param, const FLinearColor& Base, bool bEnabled, bool bHighlighted, float TextSize);

	// rounded shapes: nine-slices of small generated textures, so corners stay smooth at any size
	/** Filled rounded rectangle (Radius in pixels, clamped to half the shorter side) */
	void RoundFill(const FBox2D& Rect, const FLinearColor& Color, float Radius);
	/** Rounded outline of the given thickness in pixels */
	void RoundFrame(const FBox2D& Rect, const FLinearColor& Color, float Radius, float Thickness);
	/** Soft glow or drop shadow around a rounded rectangle, fading out over Spread pixels */
	void Glow(const FBox2D& Rect, const FLinearColor& Color, float Radius, float Spread);
	/** Draws a shape texture as a nine-slice: corners keep their shape, edges and centre stretch */
	void NineSlice(UTexture2D* Texture, const FBox2D& Rect, float Corner, const FLinearColor& Color);

	/** Small key cap ("I", "Space") starting at X, centred on Y; returns its width */
	float KeyCap(const FString& Key, float X, float Y, bool bOnLight = false);

	/** On/off switch drawn inside a rectangle */
	void Switch(const FBox2D& Rect, bool bOn);

	/** Draws an icon texture stretched over a rectangle (does nothing if the texture is missing) */
	void Icon(UTexture2D* Texture, const FBox2D& Rect, const FLinearColor& Tint = FLinearColor::White);

	/** Draws an item tile (or an empty slot) as a button and registers it for the tooltip */
	void ItemTile(const FBox2D& Rect, const FTDItem* Item, EButton Id, int32 Param);

	double Now() const;

	/** Rebuilt every frame while drawing */
	TArray<FButton> Buttons;
	TArray<FBox2D> Panels;

	TArray<FToast> Toasts;
	TArray<FFloater> Floaters;

	/** Creatures of the next wave, rebuilt only when the wave number changes */
	int32 PreviewWave = -1;
	int32 PreviewRulesKey = 0;	// Abyss tier and mutators the preview was built for
	TArray<TPair<ETDEnemyType, int32>> PreviewGroups;

	/** Item under the mouse this frame (tooltip), and the tower type used to grey out affixes */
	FTDItem TooltipItem;
	bool bTooltipFromStorage = false;
	ETDTowerType TooltipContext = ETDTowerType::Count;

	/** Selected tower this frame: a hovered storage item is compared with what it has in that slot */
	TWeakObjectPtr<const ATDTower> TooltipTower;
	TArray<TPair<FString, FLinearColor>> TipLines;

	FString LastTipTitle;

	/** Settings slider being dragged (-1 = none) */
	int32 ActiveSlider = -1;
	FBox2D ActiveSliderRect = FBox2D(ForceInit);
	void DragSlider(ATDGameMode* GameMode, const FVector2D& Position);

	/** Smoothed frames per second for the optional counter */
	float FpsSmoothed = 0.0f;
	float FpsShown = 0.0f;
	double FpsShownAt = 0.0;

	int32 LivesFlashAmount = 0;
	double LivesFlashStart = -100.0;

	/** Storage panel drawn this frame (invalid when closed), so other panels can keep clear of it */
	FBox2D StorageRect = FBox2D(ForceInit);

	/** Floating top clusters drawn this frame, so the next-wave card can sit between them */
	FBox2D TopLeftRect = FBox2D(ForceInit);
	FBox2D TopRightRect = FBox2D(ForceInit);

	/** UI scale relative to a 1080p viewport */
	float UIScale = 1.0f;

	/** Text and MeasureText use the bold face at any size while this is set (see TGuardValue in DrawTopBar) */
	bool bBoldText = false;

	FVector2D MousePos = FVector2D(-1, -1);
};
