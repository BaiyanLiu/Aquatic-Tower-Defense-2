// Aquatic Tower Defense - isometric underwater tower defense with mazing.

// Full-screen menus: main, pause, settings, victory and game over.

#include "TDHUD.h"
#include "TDGameMode.h"
#include "TDPlayerController.h"
#include "TDGrid.h"
#include "TDTower.h"
#include "TDEnemy.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "TDHUDStyle.h"
#include "TDIcons.h"
#include "TDRun.h"

using namespace TDHUDStyle;

void ATDHUD::DrawGameOver(ATDGameMode* GameMode)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;

	Panel(FBox2D(FVector2D(0, 0), FVector2D(W, H)), FLinearColor(0.0f, 0.01f, 0.02f, 0.8f));
	DrawSeaBackdrop(0.8f);

	// title, subtitle, run summary, three buttons; shrinks to fit short screens
	const float ContentH = 30.0f + 100.0f + RunSummaryHeight + 4.0f * 62.0f + 18.0f;
	const float SavedScale = UIScale;
	UIScale = FMath::Min(UIScale, H * 0.97f / ContentH);
	const float S = UIScale;

	const float CardW = 680.0f * S;
	const float CardH = ContentH * S;
	const FBox2D Card(FVector2D(W * 0.5f - CardW * 0.5f, (H - CardH) * 0.5f), FVector2D(W * 0.5f + CardW * 0.5f, (H + CardH) * 0.5f));
	Panel(Card, ModalColor, false);
	RoundFrame(Card, WithAlpha(LifeColor, 0.45f), RadiusPanel * S, 1.0f * S);
	PanelDecor(Card, LifeColor);

	float Y = Card.Min.Y + 30.0f * S;
	Text(TEXT("The reef has fallen"), W * 0.5f, Y + 20.0f * S, FLinearColor(1.0f, 0.5f, 0.55f), 46.0f, true, true);
	Text(FString::Printf(TEXT("You held out until wave %d on %s"), GameMode->GetWave(), TD::MapName(TD::GetMap())), W * 0.5f, Y + 68.0f * S, FLinearColor::White, 20.0f, true, true);
	Y += 100.0f * S;

	Y += DrawRunSummary(GameMode, Card.Min.X + 30.0f * S, Y, CardW - 60.0f * S);

	const float BtnW = 280.0f * S;
	const float BtnH = 50.0f * S;
	const bool bHasSave = GameMode->HasSaveGame();
	Button(FBox2D(FVector2D(W * 0.5f - BtnW * 0.5f, Y), FVector2D(W * 0.5f + BtnW * 0.5f, Y + BtnH)), TEXT("Retry"), EButton::Retry, 0, ActiveButton, true, true, 20.0f);
	if (FBox2D(FVector2D(W * 0.5f - BtnW * 0.5f, Y), FVector2D(W * 0.5f + BtnW * 0.5f, Y + BtnH)).IsInside(MousePos))
	{
		SetTip({ { TEXT("Retry"), AccentColor }, { FString::Printf(TEXT("A new run on %s with the same Abyss tier and mutators, in save slot %d. Boons start over."), TD::MapName(TD::GetMap()), GameMode->GetCurrentSlot() + 1), FLinearColor::White } });
	}
	Y += BtnH + 12.0f * S;
	Button(FBox2D(FVector2D(W * 0.5f - BtnW * 0.5f, Y), FVector2D(W * 0.5f + BtnW * 0.5f, Y + BtnH)), bHasSave ? FString::Printf(TEXT("Load last save (slot %d)"), GameMode->GetCurrentSlot() + 1) : FString(TEXT("No save to load")),
		EButton::MenuLoad, 1, ButtonColor, bHasSave, false, 20.0f);
	Y += BtnH + 12.0f * S;
	Button(FBox2D(FVector2D(W * 0.5f - BtnW * 0.5f, Y), FVector2D(W * 0.5f + BtnW * 0.5f, Y + BtnH)), TEXT("New game"), EButton::Restart, 0, ButtonColor, true, false, 20.0f);
	Y += BtnH + 12.0f * S;
	Button(FBox2D(FVector2D(W * 0.5f - BtnW * 0.5f, Y), FVector2D(W * 0.5f + BtnW * 0.5f, Y + BtnH)), TEXT("Main menu"), EButton::MenuMainMenu, 0, ButtonColor, true, false, 20.0f);

	UIScale = SavedScale;
}

float ATDHUD::DrawRunSummary(ATDGameMode* GameMode, float X, float Y, float W)
{
	const float S = UIScale;
	const float Line = 22.0f * S;
	const float Top = Y;

	// headline: play time, waves cleared, leaks
	const int32 Seconds = FMath::FloorToInt(GameMode->GetPlayTime());
	const FString Time = Seconds >= 3600 ? FString::Printf(TEXT("%dh %02dm"), Seconds / 3600, (Seconds / 60) % 60) : FString::Printf(TEXT("%dm %02ds"), Seconds / 60, Seconds % 60);
	const int32 Leaks = GameMode->GetTotalLeaks();
	const FString Headline = FString::Printf(TEXT("%s played   |   %d waves cleared   |   %d %s reached the reef"), *Time, GameMode->GetWavesCleared(), Leaks, Leaks == 1 ? TEXT("creature") : TEXT("creatures"));
	Text(Headline, X + W * 0.5f, Y + 10.0f * S, PearlColor, FitSize(Headline, SizeField, W), true, true);
	Y += 34.0f * S;

	const float ColGap = 24.0f * S;
	const float ColW = (W - ColGap) * 0.5f;
	const float RightX = X + ColW + ColGap;
	const float ColTop = Y;

	// left: the three towers that dealt the most damage
	Text(TEXT("Top towers"), X, Y, AccentColor, SizeBody);
	Y += Line + 4.0f * S;
	TArray<const ATDTower*> Ranked;
	if (const ATDGrid* Grid = GameMode->GetGrid())
	{
		for (const ATDTower* Tower : Grid->GetTowers())
		{
			if (Tower && Tower->IsAttackTower() && Tower->GetDamageDone() > 0.0f)
			{
				Ranked.Add(Tower);
			}
		}
	}
	Ranked.Sort([](const ATDTower& A, const ATDTower& B) { return A.GetDamageDone() > B.GetDamageDone(); });
	if (Ranked.Num() == 0)
	{
		Text(TEXT("No tower dealt damage"), X, Y, DimText, SizeSmall);
		Y += Line;
	}
	for (int32 i = 0; i < FMath::Min(3, Ranked.Num()); ++i)
	{
		const ATDTower* Tower = Ranked[i];
		const FTDFormDef* Form = TD::Form(Tower->GetTowerType(), Tower->GetForm());
		const FString Name = FString::Printf(TEXT("%d. %s%s  (level %d)"), i + 1, Form ? Form->Name : Tower->GetDef().Name, Form ? *FString::Printf(TEXT(" %s"), Tower->GetDef().Name) : TEXT(""), Tower->GetLevel());
		Text(Name, X, Y, i == 0 ? PearlColor : FLinearColor::White, FitSize(Name, SizeSmall, ColW));
		Y += Line - 2.0f * S;
		const FString Record = FString::Printf(TEXT("    %s damage, %d kills"), *ShortNumber(Tower->GetDamageDone()), Tower->GetKills());
		Text(Record, X, Y, DimText, FitSize(Record, SizeCaption + 1.0f, ColW));
		Y += Line;
	}
	float LeftBottom = Y;

	// right: pearls by source
	Y = ColTop;
	Text(TEXT("Pearls earned"), RightX, Y, AccentColor, SizeBody);
	Y += Line + 4.0f * S;
	const FTDIncome& In = GameMode->GetIncome();
	struct FSource
	{
		const TCHAR* Label;
		int32 Value;
	};
	const FSource Sources[] = {
		{ TEXT("Kills"), In.Kills }, { TEXT("Kill bounties"), In.Bounty }, { TEXT("Wave clears"), In.WaveClear }, { TEXT("Early sends"), In.EarlySend },
		{ TEXT("Trickle"), In.Trickle }, { TEXT("Selling"), In.Sell }, { TEXT("Salvage"), In.Salvage } };
	int32 Total = 0;
	for (const FSource& Source : Sources)
	{
		Total += Source.Value;
		if (Source.Value > 0)
		{
			Field(RightX, Y, ColW, Source.Label, ShortNumber(Source.Value), FLinearColor::White, SizeSmall);
			Y += Line - 2.0f * S;
		}
	}
	Field(RightX, Y, ColW, TEXT("Total"), ShortNumber(Total), PearlColor, SizeSmall);
	Y = FMath::Max(LeftBottom, Y + Line) + 6.0f * S;

	// leaks by creature, most first
	TArray<TPair<int32, int32>> LeakList;
	const TArray<int32>& ByType = GameMode->GetLeaksByType();
	for (int32 t = 0; t < ByType.Num(); ++t)
	{
		if (ByType[t] > 0)
		{
			LeakList.Add(TPair<int32, int32>(ByType[t], t));
		}
	}
	LeakList.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B) { return A.Key > B.Key; });
	TArray<FString> LeakParts;
	for (const TPair<int32, int32>& Leak : LeakList)
	{
		LeakParts.Add(FString::Printf(TEXT("%s %d"), TD::Enemy((ETDEnemyType)Leak.Value).Name, Leak.Key));
	}
	const FString LeakText = LeakParts.Num() > 0 ? TEXT("Leaks: ") + FString::Join(LeakParts, TEXT(", ")) : FString(TEXT("Leaks: none"));
	Text(LeakText, X, Y, LeakList.Num() > 0 ? FLinearColor(1.0f, 0.65f, 0.6f) : DimText, FitSize(LeakText, SizeSmall, W));
	Y += Line + 4.0f * S;

	// score of the run
	{
		const int32 Score = GameMode->GetScore();
		FString ScoreText = FString::Printf(TEXT("Score %s  (%s)"), *FText::AsNumber(Score).ToString(), *TD::MultiplierText(GameMode->GetScoreMultiplier()));
		const FTDRunRules& Rules = GameMode->GetRunRules();
		if (Rules.Tier > 0)
		{
			ScoreText += FString::Printf(TEXT("   Abyss %d"), Rules.Tier);
		}
		int32 CursedCount = 0;
		for (int32 b = 0; b < TD::BoonCount; ++b)
		{
			CursedCount += (Rules.Has((ETDBoon)b) && TD::IsCursed((ETDBoon)b)) ? 1 : 0;
		}
		if (CursedCount > 0)
		{
			ScoreText += FString::Printf(TEXT("   %d cursed"), CursedCount);
		}
		if (GameMode->CountsForRecords() && Score > 0 && Score >= GameMode->GetBestScore(TD::GetMap()))
		{
			ScoreText += TEXT("   - best score on this map");
		}
		Text(ScoreText, X + W * 0.5f, Y + 8.0f * S, PearlColor, FitSize(ScoreText, SizeField, W), true, true);
		Y += Line + 2.0f * S;
	}

	// the map's record
	const int32 Map = TD::GetMap();
	FString RecordText;
	FLinearColor RecordColor = DimText;
	if (!GameMode->CountsForRecords())
	{
		RecordText = TEXT("This game used dev tools, so it doesn't count for records");
	}
	else if (GameMode->IsNewRecord())
	{
		RecordText = FString::Printf(TEXT("New best on %s: wave %d!"), TD::MapName(Map), GameMode->GetBestWave(Map));
		RecordColor = PearlColor;
	}
	else if (GameMode->GetBestWave(Map) > 0)
	{
		RecordText = FString::Printf(TEXT("Best on %s: wave %d"), TD::MapName(Map), GameMode->GetBestWave(Map));
	}
	else
	{
		RecordText = FString::Printf(TEXT("No waves cleared on %s yet"), TD::MapName(Map));
	}
	Text(RecordText, X + W * 0.5f, Y + 8.0f * S, RecordColor, FitSize(RecordText, SizeField, W), true, true);

	// keep a fixed height so the buttons below don't move between runs
	return FMath::Max(Y + 30.0f * S - Top, RunSummaryHeight * S);
}

void ATDHUD::DrawMenu(ATDGameMode* GameMode)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const ETDMenu Menu = GameMode->GetMenu();
	if (Menu == ETDMenu::Setup)
	{
		DrawSetupMenu(GameMode);
		return;
	}
	if (Menu == ETDMenu::Boon)
	{
		DrawBoonMenu(GameMode);
		return;
	}
	if (Menu == ETDMenu::Mastery)
	{
		DrawMasteryMenu(GameMode);
		return;
	}
	if (Menu == ETDMenu::Slots)
	{
		DrawSlotMenu(GameMode);
		return;
	}

	Panel(FBox2D(FVector2D(0, 0), FVector2D(W, H)), FLinearColor(0.0f, 0.01f, 0.02f, Menu == ETDMenu::Main ? 0.6f : 0.78f));
	DrawSeaBackdrop(Menu == ETDMenu::Main ? 1.0f : 0.75f);

	// height of each menu before scaling: title block, rows and bottom margin
	float ContentH = 0.0f;
	switch (Menu)
	{
	case ETDMenu::Main:		ContentH = 76.0f + 406.0f + 62.0f + 40.0f + 26.0f * TD::MapCount; break;	// 76: the logo above the title; 62: the Mastery button
	case ETDMenu::Pause:	ContentH = 520.0f + 44.0f - 26.0f + 30.0f; break;	// +44: the brand above "PAUSED"; -26: no resume hint under it; +30: the score line
	case ETDMenu::Settings:	ContentH = 764.0f; break;	// (auto-salvage moved to the storage panel, the all-ranges switch and the Alt note are gone)
	case ETDMenu::Controls:	ContentH = 30.0f + 96.0f + 30.0f + 50.0f * FMath::Max(TDBind::ActionCount + 3, (int32)ETDTowerType::Count) + 20.0f + 50.0f + 30.0f; break;	// two columns: actions, Info and the fixed mouse rows; tower keys
	case ETDMenu::Victory:	ContentH = 30.0f + 196.0f + RunSummaryHeight + 4.0f * 62.0f + 18.0f; break;
	default: break;
	}

	// a large interface size must not push a menu off the screen
	const float SavedScale = UIScale;
	if (ContentH > 0.0f)
	{
		UIScale = FMath::Min(UIScale, H * 0.97f / ContentH);
	}
	const float S = UIScale;

	const float BoxW = (Menu == ETDMenu::Victory ? 680.0f : Menu == ETDMenu::Controls ? 900.0f : 520.0f) * S;
	const float BtnW = 380.0f * S;
	const float BtnH = 50.0f * S;
	const float Step = BtnH + 12.0f * S;
	const float CX = W * 0.5f;

	auto MenuButton = [this, CX, BtnW, BtnH](float Y, const FString& Label, EButton Id, bool bEnabled, bool bPrimary)
	{
		Button(FBox2D(FVector2D(CX - BtnW * 0.5f, Y), FVector2D(CX + BtnW * 0.5f, Y + BtnH)), Label, Id, 0,
			bPrimary ? ActiveButton : ButtonColor, bEnabled, bPrimary, 19.0f);
	};

	const float BoxH = ContentH * S;
	const float BoxY = (H - BoxH) * 0.5f;
	const FBox2D Box(FVector2D(CX - BoxW * 0.5f, BoxY), FVector2D(CX + BoxW * 0.5f, BoxY + BoxH));
	// rounded glass modal with a soft accent rim and a faint glow
	Glow(Box, WithAlpha(AccentColor, 0.08f), RadiusPanel * S, 24.0f * S);
	Panel(Box, ModalColor, false);
	RoundFrame(Box, WithAlpha(AccentColor, 0.35f), RadiusPanel * S, 1.0f * S);
	PanelDecor(Box, AccentColor);

	float Y = BoxY + 30.0f * S;
	if (Menu == ETDMenu::Main)
	{
		const double T = Now();
		const float Breath = 0.5f + 0.5f * (float)FMath::Sin(T * 0.5);	// one breath every 12.5 s

		// the logo on a round plate, with a breathing halo, a faint outer ring, a slow glint and a few bubbles
		{
			const float LogoSize = 64.0f * S;
			const FVector2D LC(CX, Y + 32.0f * S);
			const FBox2D LogoRect(LC - FVector2D(LogoSize, LogoSize) * 0.5f, LC + FVector2D(LogoSize, LogoSize) * 0.5f);
			Glow(LogoRect.ExpandBy(-8.0f * S), WithAlpha(AccentColor, 0.12f + 0.1f * Breath), LogoSize * 0.5f, 18.0f * S);
			const FBox2D Plate = LogoRect.ExpandBy(2.0f * S);
			const FBox2D Outer = LogoRect.ExpandBy(7.0f * S);
			RoundFill(Plate, InsetColor, Plate.GetSize().X * 0.5f);
			RoundFrame(Plate, WithAlpha(AccentColor, 0.6f), Plate.GetSize().X * 0.5f, 1.5f * S);
			RoundFrame(Outer, WithAlpha(AccentColor, 0.14f + 0.08f * Breath), Outer.GetSize().X * 0.5f, 1.0f * S);
			const float A = (float)FMath::Fmod(T * 0.25, 2.0 * PI);	// one lap every 25 s
			for (int32 Tail = 0; Tail < 4; ++Tail)
			{
				const float TA = A - Tail * 0.12f;
				const FVector2D P = LC + FVector2D(FMath::Cos(TA), FMath::Sin(TA)) * (Outer.GetSize().X * 0.5f - 0.5f * S);
				const float Size = (6.0f - Tail * 1.3f) * S;
				Blob(P, FVector2D(Size, Size), 0.0f, FLinearColor(0.8f, 1.0f, 1.0f, 0.55f - Tail * 0.12f), true);
			}
			Icon(TDIcons::Misc(ETDMiscIcon::Logo), LogoRect);
			struct FSpot { float DX; float DY; float R; };
			static const FSpot Spots[] = { { 44.0f, -10.0f, 3.5f }, { 50.0f, -20.0f, 2.5f }, { 46.0f, -28.0f, 1.8f }, { -46.0f, 4.0f, 2.5f }, { -51.0f, -6.0f, 1.8f } };
			for (int32 i = 0; i < UE_ARRAY_COUNT(Spots); ++i)
			{
				const float Bob = (float)FMath::Sin(T * 0.4 + i * 1.7) * 1.5f * S;
				Bubble(LC + FVector2D(Spots[i].DX, Spots[i].DY) * S + FVector2D(0.0f, Bob), Spots[i].R * S, AccentColor);
			}
		}
		Y += 76.0f * S;

		// the name over a soft glow with a lighter sheen pass, a drifting wave flourish with bubbles at its ends, then the subtitle
		const FString Title = TEXT("AQUATIC TOWER DEFENSE");
		const float TitleW = MeasureText(Title, 38.0f).X;
		Blob(FVector2D(CX, Y + 16.0f * S), FVector2D(TitleW + 70.0f * S, 64.0f * S), 0.0f, WithAlpha(AccentColor, 0.05f + 0.03f * Breath), true);
		Text(Title, CX, Y + 16.0f * S, AccentColor, 38.0f, true, true);
		Text(Title, CX, Y + 16.0f * S - FMath::Max(1.0f, FMath::RoundToFloat(S)), FLinearColor(0.85f, 1.0f, 1.0f, 0.1f + 0.1f * Breath), 38.0f, true, true);
		{
			const float Half = FMath::Min(TitleW * 0.36f, 170.0f * S);
			const float WY = Y + 41.0f * S;
			WaveStrip(CX - Half, CX + Half, WY, 8.0f * S, false, WithAlpha(AccentColor, 0.42f), (float)FMath::Fmod(T * 0.035, 1.0));
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				Bubble(FVector2D(CX + Side * (Half + 7.0f * S), WY + 4.0f * S), 3.0f * S, AccentColor);
				Bubble(FVector2D(CX + Side * (Half + 14.0f * S), WY - 1.0f * S), 1.8f * S, AccentColor);
			}
		}
		Text(TEXT("Build a maze on the seabed and protect the reef"), CX, Y + 62.0f * S, DimText, 17.0f, true, true);
		// separator: two hairlines with a tiny starfish between them
		{
			const float SY = FMath::RoundToFloat(Y + 89.0f * S);
			const float Thick = FMath::Max(1.0f, FMath::RoundToFloat(S));
			Fill(FBox2D(FVector2D(CX - 90.0f * S, SY), FVector2D(CX - 14.0f * S, SY + Thick)), WithAlpha(AccentColor, 0.22f));
			Fill(FBox2D(FVector2D(CX + 14.0f * S, SY), FVector2D(CX + 90.0f * S, SY + Thick)), WithAlpha(AccentColor, 0.22f));
			Starfish(FVector2D(CX, SY + Thick * 0.5f), 7.0f * S, (float)FMath::Sin(T * 0.2) * 8.0f, FLinearColor(1.0f, 0.52f, 0.34f, 0.9f));
		}
		Y += 110.0f * S;

		MenuButton(Y, TEXT("Start new game"), EButton::MenuNewGame, true, true);
		Y += Step;
		const bool bHasSave = GameMode->HasAnySave();
		MenuButton(Y, bHasSave ? TEXT("Continue saved game") : TEXT("Continue (no save yet)"), EButton::MenuLoad, bHasSave, false);
		Y += Step;
		MenuButton(Y, TEXT("Tower mastery"), EButton::MenuMastery, true, false);
		Y += Step;
		MenuButton(Y, TEXT("Settings"), EButton::MenuSettings, true, false);
		Y += Step;
		MenuButton(Y, TEXT("Quit"), EButton::MenuQuit, true, false);
		Y += Step + 8.0f * S;

		// best-wave records per map
		Text(TEXT("Best waves"), CX, Y + 10.0f * S, AccentColor, SizeBody, true, true);
		Y += 32.0f * S;
		const float Left = CX - BoxW * 0.5f + 40.0f * S;
		const float RowW = BoxW - 80.0f * S;
		for (int32 Map = 0; Map < TD::MapCount; ++Map)
		{
			const int32 Best = GameMode->GetBestWave(Map);
			const int32 WinLives = GameMode->GetBestWinLives(Map);
			FString Value = WinLives > 0 ? FString::Printf(TEXT("Won (%d lives left)%s"), WinLives, Best > TD::FinalWave ? *FString::Printf(TEXT(", wave %d"), Best) : TEXT(""))
				: (Best > 0 ? FString::Printf(TEXT("Wave %d"), Best) : FString(TEXT("-")));
			if (GameMode->GetBestAbyss(Map) >= 1)
			{
				Value += FString::Printf(TEXT(", Abyss %d"), GameMode->GetBestAbyss(Map));
			}
			Field(Left, Y, RowW, TD::MapName(Map), Value, WinLives > 0 ? PearlColor : (Best > 0 ? FLinearColor::White : DimText), SizeSmall);
			Y += 26.0f * S;
		}
	}
	else if (Menu == ETDMenu::Victory)
	{
		Text(TEXT("THE REEF IS SAFE"), CX, Y + 16.0f * S, FLinearColor(1.0f, 0.85f, 0.4f), 38.0f, true, true);
		Text(FString::Printf(TEXT("You beat the Abyssal Kraken and held all %d waves"), TD::FinalWave), CX, Y + 64.0f * S, FLinearColor::White, 18.0f, true, true);
		Text(FString::Printf(TEXT("%d lives left"), GameMode->GetLives()), CX, Y + 96.0f * S, DimText, 17.0f, true, true);
		Text(TEXT("Endless mode: the waves never stop, and every one"), CX, Y + 136.0f * S, FLinearColor(1.0f, 0.65f, 0.45f), 16.0f, true, true);
		Text(TEXT("is much stronger and faster than the last"), CX, Y + 160.0f * S, FLinearColor(1.0f, 0.65f, 0.45f), 16.0f, true, true);
		Y += 196.0f * S;
		Y += DrawRunSummary(GameMode, CX - BoxW * 0.5f + 30.0f * S, Y, BoxW - 60.0f * S);

		MenuButton(Y, TEXT("Continue in endless mode"), EButton::MenuEndless, true, true);
		Y += Step;
		MenuButton(Y, TEXT("Retry (same map, tier and mutators)"), EButton::Retry, true, false);
		Y += Step;
		MenuButton(Y, TEXT("Start new game"), EButton::MenuNewGame, true, false);
		Y += Step;
		MenuButton(Y, TEXT("Main menu"), EButton::MenuMainMenu, true, false);
	}
	else if (Menu == ETDMenu::Pause)
	{
		// the same brand as the top bar, a little larger and centred
		{
			const float BrandScale = S * 1.4f;
			const float BrandW = DrawBrand(0.0f, 0.0f, BrandScale, false);
			DrawBrand(CX - BrandW * 0.5f, Y + 30.0f * S, BrandScale);
		}
		const float PY = Y + 44.0f * S;	// everything below moved down by the brand's extra height
		{
			// "PAUSED" in a gold pill
			const FVector2D Extent = MeasureText(TEXT("PAUSED"), SizeSmall);
			const FBox2D Pill(FVector2D(CX - Extent.X * 0.5f - 14.0f * S, PY + 30.0f * S), FVector2D(CX + Extent.X * 0.5f + 14.0f * S, PY + 30.0f * S + Extent.Y + 8.0f * S));
			RoundFill(Pill, WithAlpha(PearlColor * 0.2f, 0.9f), Pill.GetSize().Y * 0.5f);
			RoundFrame(Pill, WithAlpha(PearlColor, 0.6f), Pill.GetSize().Y * 0.5f, 1.0f * S);
			Text(TEXT("PAUSED"), CX, Pill.GetCenter().Y, PearlColor, SizeSmall, true, true);
		}
		// the run's score and multiplier
		{
			const FTDRunRules& Rules = GameMode->GetRunRules();
			FString ScoreLine = FString::Printf(TEXT("Score %s  (%s)"), *FText::AsNumber(GameMode->GetScore()).ToString(), *TD::MultiplierText(GameMode->GetScoreMultiplier()));
			if (Rules.Tier > 0)
			{
				ScoreLine += FString::Printf(TEXT("   Abyss %d"), Rules.Tier);
			}
			Text(ScoreLine, CX, PY + 82.0f * S, PearlColor, FitSize(ScoreLine, SizeField, BoxW - 60.0f * S), true, true);
		}
		Y = PY + 104.0f * S;	// the buttons follow the score line with the same gap as between buttons
		MenuButton(Y, TEXT("Resume"), EButton::MenuResume, true, true);
		Y += Step;
		FString Reason;
		const bool bCanSave = GameMode->CanSaveGame(&Reason);
		MenuButton(Y, bCanSave ? FString::Printf(TEXT("Save game (slot %d)"), GameMode->GetCurrentSlot() + 1) : FString::Printf(TEXT("Save game (%s)"), *Reason.ToLower()), EButton::MenuSave, bCanSave, false);
		Y += Step;
		MenuButton(Y, GameMode->HasAnySave() ? TEXT("Load saved game") : TEXT("Load (no save yet)"), EButton::MenuLoad, GameMode->HasAnySave(), false);
		Y += Step;
		MenuButton(Y, TEXT("Restart"), EButton::Restart, true, false);
		Y += Step;
		MenuButton(Y, TEXT("Settings"), EButton::MenuSettings, true, false);
		Y += Step;
		MenuButton(Y, TEXT("Main menu"), EButton::MenuMainMenu, true, false);
	}
	else if (Menu == ETDMenu::Settings)
	{
		Text(TEXT("Settings"), CX, Y + 16.0f * S, AccentColor, 34.0f, true, true);
		Y += 80.0f * S;

		auto SettingRow = [this, &Y, CX, BoxW, S](const FString& Label, const FString& ValueText, float Value, float Min, float Max, int32 Index)
		{
			const float Left = CX - BoxW * 0.5f + 30.0f * S;
			const float Right = CX + BoxW * 0.5f - 30.0f * S;
			Field(Left, Y, Right - Left, Label, ValueText, FLinearColor::White, 18.0f);
			Y += 30.0f * S;

			const float Btn = 40.0f * S;
			Button(FBox2D(FVector2D(Left, Y), FVector2D(Left + Btn, Y + Btn)), TEXT("-"), EButton::SettingAdjust, Index * 2, ButtonColor, Value > Min, false, 22.0f);
			Button(FBox2D(FVector2D(Right - Btn, Y), FVector2D(Right, Y + Btn)), TEXT("+"), EButton::SettingAdjust, Index * 2 + 1, ButtonColor, Value < Max, false, 22.0f);

			// clickable slider bar between the buttons
			const FBox2D Bar(FVector2D(Left + Btn + 16.0f * S, Y + Btn * 0.5f - 4.0f * S), FVector2D(Right - Btn - 16.0f * S, Y + Btn * 0.5f + 4.0f * S));
			const float Fraction = FMath::Clamp((Value - Min) / (Max - Min), 0.0f, 1.0f);
			// thin rounded track, cyan fill and a round glowing knob
			RoundFill(Bar, TrackColor, 4.0f * S);
			RoundFill(FBox2D(Bar.Min, FVector2D(Bar.Min.X + FMath::Max(8.0f * S, Bar.GetSize().X * Fraction), Bar.Max.Y)), AccentColor * 0.85f, 4.0f * S);
			const float Knob = 20.0f * S;
			const FVector2D KnobCenter(Bar.Min.X + Bar.GetSize().X * Fraction, Bar.GetCenter().Y);
			const FBox2D KnobRect(KnobCenter - FVector2D(Knob, Knob) * 0.5f, KnobCenter + FVector2D(Knob, Knob) * 0.5f);
			Glow(KnobRect, WithAlpha(AccentColor, 0.35f), Knob * 0.5f, 5.0f * S);
			RoundFill(KnobRect, FLinearColor::White, Knob * 0.5f);
			Buttons.Add({ FBox2D(Bar.Min - FVector2D(0, 10.0f * S), Bar.Max + FVector2D(0, 10.0f * S)), EButton::SettingSlider, Index, true });
			Y += Btn + 22.0f * S;
		};

		auto Percent = [](float Value) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value)); };
		SettingRow(TEXT("Interface size"), Percent(GameMode->GetUIScalePercent()), GameMode->GetUIScalePercent(), 80.0f, 150.0f, 1);

		const int32 Limit = GameMode->GetFrameRateLimit();
		const FString FpsText = Limit > 0 ? FString::Printf(TEXT("%d FPS"), Limit) : FString(TEXT("Unlimited"));
		SettingRow(TEXT("Frame rate limit"), FpsText, (float)GameMode->GetFrameRateStep(), 0.0f, (float)(TD::FrameRateStepCount - 1), 2);
		const int32 Quality = GameMode->GetGraphicsQuality();
		SettingRow(TEXT("Lighting quality"), ATDGameMode::GraphicsQualityName(Quality), (float)Quality, 0.0f, (float)(ATDGameMode::GraphicsQualityLevels - 1), 5);
		SettingRow(TEXT("Effects volume"), Percent(GameMode->GetSfxVolume()), GameMode->GetSfxVolume(), 0.0f, 100.0f, 3);
		SettingRow(TEXT("Music volume"), Percent(GameMode->GetMusicVolume()), GameMode->GetMusicVolume(), 0.0f, 100.0f, 4);

		// on/off toggles side by side
		{
			const float Left = CX - BoxW * 0.5f + 30.0f * S;
			const float Right = CX + BoxW * 0.5f - 30.0f * S;
			const float Gap = 14.0f * S;
			const float ToggleW = (Right - Left - Gap) * 0.5f;
			const float ToggleH = 42.0f * S;
			const bool bFps = GameMode->IsShowingFps();
			const bool bVSync = GameMode->IsVSyncEnabled();
			// label on the left, on/off switch on the right
			auto Toggle = [this, S](const FBox2D& Rect, const FString& Label, EButton Id, bool bOn)
			{
				Button(Rect, TEXT(""), Id, 0, ButtonColor, true, false, 18.0f);
				const float SwitchW = 38.0f * S;
				const float SwitchH = 22.0f * S;
				const FBox2D SwitchRect(FVector2D(Rect.Max.X - 12.0f * S - SwitchW, Rect.GetCenter().Y - SwitchH * 0.5f), FVector2D(Rect.Max.X - 12.0f * S, Rect.GetCenter().Y + SwitchH * 0.5f));
				Text(Label, Rect.Min.X + 14.0f * S, Rect.GetCenter().Y, FLinearColor::White, FitSize(Label, 17.0f, SwitchRect.Min.X - Rect.Min.X - 22.0f * S), false, true);
				Switch(SwitchRect, bOn);
			};
			Toggle(FBox2D(FVector2D(Left, Y), FVector2D(Left + ToggleW, Y + ToggleH)), TEXT("Show FPS"), EButton::ToggleShowFps, bFps);
			Toggle(FBox2D(FVector2D(Left + ToggleW + Gap, Y), FVector2D(Right, Y + ToggleH)), TEXT("VSync"), EButton::ToggleVSync, bVSync);
			Y += ToggleH + 12.0f * S;

			// anti-aliasing and the controls share the last row
			const bool bSharp = GameMode->IsSharpAA();
			const FBox2D AARect(FVector2D(Left, Y), FVector2D(Left + ToggleW, Y + ToggleH));
			Button(AARect, bSharp ? TEXT("Anti-aliasing: Sharp") : TEXT("Anti-aliasing: Smooth"), EButton::ToggleAA, 0, ButtonColor, true, false, 18.0f);
			if (AARect.IsInside(MousePos))
			{
				SetTip({ { TEXT("Anti-aliasing"), AccentColor }, { TEXT("Smooth (FXAA): moving creatures stay crisp"), FLinearColor::White },
					{ TEXT("Sharp (TSR): cleaner edges, but fast movement can leave ghost trails"), FLinearColor::White } });
			}
			Button(FBox2D(FVector2D(Left + ToggleW + Gap, Y), FVector2D(Right, Y + ToggleH)), TEXT("Controls"), EButton::MenuControls, 0, ButtonColor, true, false, 18.0f);
			Y += ToggleH + 18.0f * S;
		}
		MenuButton(Y, TEXT("Back"), EButton::SettingsBack, true, true);
	}
	else if (Menu == ETDMenu::Controls)
	{
		Text(TEXT("Controls"), CX, Y + 16.0f * S, AccentColor, 34.0f, true, true);
		Text(TEXT("Click a key, then press the new one"), CX, Y + 58.0f * S, DimText, SizeField, true, true);
		Y += 96.0f * S;

		const float Left = CX - BoxW * 0.5f + 30.0f * S;
		const float Right = CX + BoxW * 0.5f - 30.0f * S;
		const float ColGap = 40.0f * S;
		const float ColW = (Right - Left - ColGap) * 0.5f;
		const float KeyW = 150.0f * S;
		const float RowH = 42.0f * S;
		const float RowStep = RowH + 8.0f * S;

		// left column: the actions and the hold-for-info key; right column: one key per tower, in dock order
		Text(TEXT("Actions"), Left, Y + 12.0f * S, DimText, SizeField, false, true);
		Text(TEXT("Build towers"), Left + ColW + ColGap, Y + 12.0f * S, DimText, SizeField, false, true);
		Y += 30.0f * S;
		const float RowsY = Y;
		auto BindRow = [&](int32 Index, const FString& Label, float ColX, float RowY)
		{
			const bool bWaiting = GameMode->GetRebinding() == Index;
			const float KeyX = ColX + ColW - KeyW;
			Text(Label, ColX, RowY + RowH * 0.5f, FLinearColor::White, FitSize(Label, 18.0f, KeyX - ColX - 10.0f * S), false, true);
			Button(FBox2D(FVector2D(KeyX, RowY), FVector2D(ColX + ColW, RowY + RowH)), bWaiting ? FString(TEXT("Press a key")) : GameMode->GetKeyName((ETDBind)Index),
				EButton::BindKey, Index, bWaiting ? ActiveButton : ButtonColor, true, bWaiting, 18.0f);
		};
		int32 LeftRows = 0;
		for (int32 i = 0; i < TDBind::ActionCount; ++i)
		{
			BindRow(i, TDBind::ActionName((ETDBind)i), Left, RowsY + RowStep * LeftRows++);
		}
		BindRow((int32)ETDBind::Info, TDBind::ActionName(ETDBind::Info), Left, RowsY + RowStep * LeftRows++);
		// the mouse buttons can't be changed: same row and key box, greyed out like a disabled button, with no click or hover
		auto FixedRow = [&](const FString& Label, const FString& Key, float ColX, float RowY)
		{
			const float KeyX = ColX + ColW - KeyW;
			const float R = RadiusButton * S;
			const FBox2D Rect(FVector2D(KeyX, RowY), FVector2D(ColX + ColW, RowY + RowH));
			Text(Label, ColX, RowY + RowH * 0.5f, DimText, FitSize(Label, 18.0f, KeyX - ColX - 10.0f * S), false, true);
			RoundFill(Rect, DisabledButton, R);
			RoundFill(FBox2D(Rect.Min, FVector2D(Rect.Max.X, Rect.Min.Y + Rect.GetSize().Y * 0.5f)), FLinearColor(1.0f, 1.0f, 1.0f, 0.07f), R);
			RoundFill(FBox2D(FVector2D(Rect.Min.X, Rect.Max.Y - FMath::Max(2.0f, 3.0f * S)), Rect.Max), FLinearColor(0.0f, 0.0f, 0.0f, 0.22f), FMath::Min(R, 3.0f * S));
			RoundFrame(Rect, DisabledEdge, R, 1.0f * S);
			Text(Key, Rect.GetCenter().X, Rect.GetCenter().Y, DisabledText, FitSize(Key, 18.0f, Rect.GetSize().X - 14.0f * S), true, true);
			if (Rect.IsInside(MousePos))
			{
				SetTip({ { TEXT("This key can't be changed"), DimText } });
			}
		};
		FixedRow(TEXT("Build / select"), TEXT("Click"), Left, RowsY + RowStep * LeftRows++);
		FixedRow(TEXT("Cancel"), TEXT("Right-click"), Left, RowsY + RowStep * LeftRows++);
		for (int32 i = 0; i < (int32)ETDTowerType::Count; ++i)
		{
			BindRow((int32)TDBind::TowerBind((ETDTowerType)i), TD::Tower((ETDTowerType)i).Name, Left + ColW + ColGap, RowsY + RowStep * i);
		}
		Y = RowsY + RowStep * FMath::Max(LeftRows, (int32)ETDTowerType::Count) + 12.0f * S;

		const float Half = (Right - Left - 12.0f * S) * 0.5f;
		Button(FBox2D(FVector2D(Left, Y), FVector2D(Left + Half, Y + BtnH)), TEXT("Reset to defaults"), EButton::ResetKeys, 0, ButtonColor, true, false, 18.0f);
		Button(FBox2D(FVector2D(Right - Half, Y), FVector2D(Right, Y + BtnH)), TEXT("Back"), EButton::ControlsBack, 0, ActiveButton, true, true, 18.0f);
	}

	UIScale = SavedScale;
}
