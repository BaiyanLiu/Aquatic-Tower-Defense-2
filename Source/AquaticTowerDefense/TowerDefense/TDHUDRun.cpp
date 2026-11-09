// Aquatic Tower Defense - isometric underwater tower defense with mazing.

// Run setup and replayability UI: the new game setup screen, the boon offer and the score / boon strip shown during play.

#include "TDHUD.h"
#include "TDGameMode.h"
#include "TDPlayerController.h"
#include "TDRun.h"
#include "TDMastery.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "TDHUDStyle.h"
#include "TDIcons.h"

using namespace TDHUDStyle;

namespace
{
	/** Rim and tag color of cursed boons */
	const FLinearColor CursedColor = SRGB(255, 92, 132);

	FString FormatScore(int32 Score)
	{
		return FText::AsNumber(Score).ToString();
	}

	/** "Abyss 1-3: rule; rule; rule" for the tier's cumulative rules ("Normal rules" at 0) */
	FString AbyssSummary(int32 Tier)
	{
		if (Tier <= 0)
		{
			return TEXT("Normal rules. Win wave 100 on a map to unlock Abyss 1 there.");
		}
		TArray<FString> Rules;
		for (int32 t = 1; t <= Tier; ++t)
		{
			Rules.Add(TD::AbyssRule(t));
		}
		return FString::Printf(TEXT("Abyss %d: "), Tier) + FString::Join(Rules, TEXT("; ")) + TEXT(".");
	}
}

UTexture2D* ATDHUD::BoonIcon(int32 Boon)
{
	// every boon and curse has its own painted icon (TDIcons, Boon family)
	return TDIcons::Get(ETDIcon::Boon, Boon);
}

// ---------------------------------------------------------------------------------------------
// Score and boons during play
// ---------------------------------------------------------------------------------------------

void ATDHUD::DrawRunStrip(ATDGameMode* GameMode)
{
	// under the top-left cluster; the storage panel takes that corner while it's open
	const ETDMenu Menu = GameMode->GetMenu();
	if (!TopLeftRect.bIsValid || StorageRect.bIsValid || Menu == ETDMenu::Main || Menu == ETDMenu::Setup)
	{
		return;
	}
	const FTDRunRules& Rules = GameMode->GetRunRules();
	const float S = UIScale;
	const float H = 36.0f * S;
	const float Pad = 14.0f * S;
	const float IconW = 26.0f * S;
	const float ChipPad = 9.0f * S;	// inside a chip, each side
	const float Gap = 10.0f * S;		// between the score, the chips and the boon icons
	const float Y0 = TopLeftRect.Max.Y + 6.0f * S;
	const float MidY = Y0 + H * 0.5f;

	const FString ScoreText = FormatScore(GameMode->GetScore());
	const FString MultText = TD::MultiplierText(GameMode->GetScoreMultiplier());
	TArray<int32> Taken;
	for (int32 b = 0; b < TD::BoonCount; ++b)
	{
		if (Rules.Has((ETDBoon)b))
		{
			Taken.Add(b);
		}
	}
	TArray<FString> MutatorNames;
	for (int32 m = 0; m < TD::MutatorCount; ++m)
	{
		if (Rules.Has((ETDMutator)m))
		{
			MutatorNames.Add(TD::MutatorName((ETDMutator)m));
		}
	}
	const FString TierText = Rules.Tier > 0 ? FString::Printf(TEXT("Abyss %d"), Rules.Tier) : FString();
	const FString MutText = MutatorNames.Num() > 0 ? FString::Printf(TEXT("%d %s"), MutatorNames.Num(), MutatorNames.Num() == 1 ? TEXT("mutator") : TEXT("mutators")) : FString();

	// measure, then draw
	const float ChipSize = SizeSmall;
	const float LabelW = MeasureText(TEXT("SCORE"), SizeSmall).X;
	const float ScoreW = MeasureText(ScoreText, SizeBody).X;
	const float MultW = MeasureText(MultText, SizeSmall).X;
	float W = Pad + LabelW + 8.0f * S + ScoreW + 8.0f * S + MultW + Pad;
	if (!TierText.IsEmpty())
	{
		W += Gap + MeasureText(TierText, ChipSize).X + ChipPad * 2.0f;
	}
	if (!MutText.IsEmpty())
	{
		W += Gap + MeasureText(MutText, ChipSize).X + ChipPad * 2.0f;
	}
	if (Taken.Num() > 0)
	{
		W += Gap + 1.0f * S + Gap + Taken.Num() * IconW + (Taken.Num() - 1) * 6.0f * S;
	}
	const FBox2D Rect(FVector2D(TopLeftRect.Min.X, Y0), FVector2D(TopLeftRect.Min.X + W, Y0 + H));
	FramedPanel(Rect, PanelEdge);

	float X = Rect.Min.X + Pad;
	const float X0 = X;
	Text(TEXT("SCORE"), X, MidY, DimText, SizeSmall, false, true);
	X += LabelW + 8.0f * S;
	Text(ScoreText, X, MidY, PearlColor, SizeBody, false, true);
	X += ScoreW + 8.0f * S;
	Text(MultText, X, MidY, GameMode->GetScoreMultiplier() > 1.0f ? PearlColor : DimText, SizeSmall, false, true);
	X += MultW;
	if (FBox2D(FVector2D(X0, Y0), FVector2D(X, Y0 + H)).IsInside(MousePos))
	{
		SetTip({ { FString::Printf(TEXT("Score %s"), *ScoreText), PearlColor },
			{ FString::Printf(TEXT("(waves cleared x 100 + lives left x 25 once wave %d is beaten) x the score multiplier %s"), TD::FinalWave, *MultText), FLinearColor::White },
			{ TEXT("The multiplier comes from the Abyss tier and mutators chosen for this run, and from cursed boons taken."), DimText } });
	}

	// a small chip: Abyss tier and mutators, each with its rules in a tooltip
	auto Chip = [this, &X, MidY, S, ChipPad, ChipSize, Gap](const FString& Label, const FLinearColor& Color, const TArray<TPair<FString, FLinearColor>>& Tip)
	{
		X += Gap;
		const FVector2D LabelSize = MeasureText(Label, ChipSize);
		const float ChipH = 24.0f * S;
		const FBox2D ChipRect(FVector2D(X, MidY - ChipH * 0.5f), FVector2D(X + LabelSize.X + ChipPad * 2.0f, MidY + ChipH * 0.5f));
		RoundFill(ChipRect, WithAlpha(Color * 0.3f, 0.9f), ChipH * 0.5f);
		RoundFrame(ChipRect, WithAlpha(Color, 0.45f), ChipH * 0.5f, 1.0f * S);
		Text(Label, ChipRect.GetCenter().X, MidY, Lighten(Color, 0.25f), ChipSize, true, true);
		if (ChipRect.IsInside(MousePos))
		{
			SetTip(Tip);
		}
		X = ChipRect.Max.X;
	};
	if (!TierText.IsEmpty())
	{
		TArray<TPair<FString, FLinearColor>> Tip = { { TierText, VioletColor } };
		for (int32 t = 1; t <= Rules.Tier; ++t)
		{
			Tip.Add({ TD::AbyssRule(t), FLinearColor::White });
		}
		Chip(TierText, VioletColor, Tip);
	}
	if (!MutText.IsEmpty())
	{
		TArray<TPair<FString, FLinearColor>> Tip = { { TEXT("Mutators"), LifeColor } };
		for (int32 m = 0; m < TD::MutatorCount; ++m)
		{
			if (Rules.Has((ETDMutator)m))
			{
				Tip.Add({ FString::Printf(TEXT("%s: %s"), TD::MutatorName((ETDMutator)m), TD::MutatorDescription((ETDMutator)m)), FLinearColor::White });
			}
		}
		Chip(MutText, LifeColor, Tip);
	}

	// taken boons as icons
	if (Taken.Num() > 0)
	{
		// a thin divider between the rules and the boons
		X += Gap;
		Fill(FBox2D(FVector2D(X, MidY - 10.0f * S), FVector2D(X + 1.0f * S, MidY + 10.0f * S)), WithAlpha(PanelEdge, 0.5f));
		X += 1.0f * S + Gap;
		for (const int32 Boon : Taken)
		{
			const FBox2D IconRect(FVector2D(X, MidY - IconW * 0.5f), FVector2D(X + IconW, MidY + IconW * 0.5f));
			const bool bCursed = TD::IsCursed((ETDBoon)Boon);
			if (bCursed)
			{
				RoundFill(IconRect.ExpandBy(1.0f * S), WithAlpha(CursedColor * 0.3f, 0.8f), IconW * 0.5f);
				RoundFrame(IconRect.ExpandBy(1.0f * S), WithAlpha(CursedColor, 0.85f), IconW * 0.5f, 1.0f * S);
			}
			Icon(BoonIcon(Boon), IconRect);
			if (IconRect.ExpandBy(2.0f * S).IsInside(MousePos))
			{
				if (bCursed)
				{
					SetTip({ { FString::Printf(TEXT("%s  (cursed, +%d%% score)"), TD::BoonName((ETDBoon)Boon), FMath::RoundToInt(TD::BoonScore((ETDBoon)Boon) * 100)), CursedColor },
						{ TD::BoonDescription((ETDBoon)Boon), FLinearColor::White }, { TD::BoonDrawback((ETDBoon)Boon), LifeColor } });
				}
				else
				{
					SetTip({ { TD::BoonName((ETDBoon)Boon), VioletColor }, { TD::BoonDescription((ETDBoon)Boon), FLinearColor::White } });
				}
			}
			X += IconW + 6.0f * S;
		}
	}
}

// ---------------------------------------------------------------------------------------------
// New game setup
// ---------------------------------------------------------------------------------------------

void ATDHUD::DrawMapThumbnail(int32 Map, const FBox2D& Rect, float Alpha)
{
	const float Cell = FMath::Min(Rect.GetSize().X / TD::Cols, Rect.GetSize().Y / TD::Rows);
	const FVector2D Origin = Rect.GetCenter() - FVector2D(Cell * TD::Cols, Cell * TD::Rows) * 0.5f;
	const FLinearColor Tint = TD::MapTint(Map);
	const FLinearColor Sand = FLinearColor(0.14f * Tint.R, 0.25f * Tint.G, 0.28f * Tint.B, Alpha);
	const FLinearColor Sand2 = FLinearColor(0.17f * Tint.R, 0.29f * Tint.G, 0.31f * Tint.B, Alpha);
	auto CellRect = [&Origin, Cell](const FIntPoint& C, float Inset)
	{
		const FVector2D Min = Origin + FVector2D(C.X * Cell + Inset, C.Y * Cell + Inset);
		return FBox2D(Min, Min + FVector2D(Cell - Inset * 2.0f, Cell - Inset * 2.0f));
	};
	for (int32 Row = 0; Row < TD::Rows; ++Row)
	{
		for (int32 Col = 0; Col < TD::Cols; ++Col)
		{
			const FIntPoint C(Col, Row);
			if (TD::IsMapCell(Map, C))
			{
				Fill(CellRect(C, 0.0f), ((Col + Row) % 2 == 0) ? Sand : Sand2);
			}
		}
	}
	for (const FIntPoint& Rock : TD::MapRocks(Map))
	{
		Fill(CellRect(Rock, Cell * 0.12f), FLinearColor(0.42f, 0.46f, 0.5f, Alpha));
	}
	Fill(CellRect(TD::MapSpawn(Map), -Cell * 0.15f), WithAlpha(LifeColor, Alpha));
	Fill(CellRect(TD::MapGoal(Map), -Cell * 0.15f), WithAlpha(KelpColor, Alpha));
}

void ATDHUD::DrawSetupMenu(ATDGameMode* GameMode)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	Panel(FBox2D(FVector2D(0, 0), FVector2D(W, H)), FLinearColor(0.0f, 0.01f, 0.02f, 0.78f));
	DrawSeaBackdrop(0.75f);

	const float BaseW = 1000.0f;
	const float CardH = 272.0f;	// room for three lines of map description above the records
	const float ContentH = 28.0f + 52.0f + 28.0f + CardH + 20.0f + 28.0f + 42.0f + 46.0f + 28.0f + 3.0f * 46.0f + 10.0f + 18.0f + 72.0f + 16.0f + 52.0f + 50.0f + 26.0f;	// 52: the save slot row
	const float SavedScale = UIScale;
	UIScale = FMath::Min3(UIScale, H * 0.97f / ContentH, W * 0.97f / BaseW);
	const float S = UIScale;

	const ATDGameMode::FTDRunSetup& Setup = GameMode->GetSetup();
	const float BoxW = BaseW * S;
	const float BoxH = ContentH * S;
	const float CX = W * 0.5f;
	const FBox2D Box(FVector2D(CX - BoxW * 0.5f, (H - BoxH) * 0.5f), FVector2D(CX + BoxW * 0.5f, (H + BoxH) * 0.5f));
	Glow(Box, WithAlpha(AccentColor, 0.08f), RadiusPanel * S, 24.0f * S);
	Panel(Box, ModalColor, false);
	RoundFrame(Box, WithAlpha(AccentColor, 0.35f), RadiusPanel * S, 1.0f * S);
	PanelDecor(Box, AccentColor);

	const float Left = Box.Min.X + 30.0f * S;
	const float Right = Box.Max.X - 30.0f * S;
	const float InnerW = Right - Left;
	float Y = Box.Min.Y + 28.0f * S;
	Text(TEXT("New game"), CX, Y + 16.0f * S, AccentColor, 34.0f, true, true);
	Y += 52.0f * S;

	// ---- map cards: the four maps and Random ----
	Text(TEXT("Map"), Left, Y + 10.0f * S, AccentColor, SizeHeading, false, true);
	Y += 28.0f * S;
	{
		const int32 Cards = TD::MapCount + 1;
		const float Gap = 12.0f * S;
		const float CardW = (InnerW - Gap * (Cards - 1)) / Cards;
		for (int32 i = 0; i < Cards; ++i)
		{
			const int32 Map = i < TD::MapCount ? i : -1;
			const bool bSelected = Setup.Map == Map;
			const FBox2D Card(FVector2D(Left + i * (CardW + Gap), Y), FVector2D(Left + i * (CardW + Gap) + CardW, Y + CardH * S));
			Button(Card, TEXT(""), EButton::SetupMap, Map, bSelected ? ActiveButton : ButtonColor, true, bSelected, SizeBody);

			const float Pad = 10.0f * S;
			const float CW = CardW - Pad * 2.0f;
			const FString Name = Map >= 0 ? FString(TD::MapName(Map)) : FString(TEXT("Random"));
			Text(Name, Card.GetCenter().X, Card.Min.Y + 18.0f * S, bSelected ? FLinearColor::White : FLinearColor(0.9f, 0.97f, 1.0f), FitSize(Name, SizeBody, CW), true, true);

			const FBox2D Thumb(FVector2D(Card.Min.X + Pad, Card.Min.Y + 34.0f * S), FVector2D(Card.Max.X - Pad, Card.Min.Y + 34.0f * S + CW * TD::Rows / TD::Cols));
			RoundFill(Thumb.ExpandBy(2.0f * S), InsetColor, RadiusTile * S);
			if (Map >= 0)
			{
				DrawMapThumbnail(Map, Thumb, 1.0f);
			}
			else
			{
				Text(TEXT("?"), Thumb.GetCenter().X, Thumb.GetCenter().Y, WithAlpha(AccentColor, 0.8f), 46.0f, true, true);
			}

			const float DescY = Thumb.Max.Y + 9.0f * S;
			const FString Desc = Map >= 0 ? FString(TD::MapDescription(Map)) : FString(TEXT("A different map from the last one, at normal rules."));
			WrappedTextCentered(Desc, Card.GetCenter().X, DescY, CW, InfoText, 13.5f, 17.0f * S);

			// records: best wave or win, then the highest Abyss tier won and the best score
			if (Map >= 0)
			{
				const int32 Best = GameMode->GetBestWave(Map);
				const int32 WinLives = GameMode->GetBestWinLives(Map);
				const int32 Abyss = GameMode->GetBestAbyss(Map);
				const int32 BestScore = GameMode->GetBestScore(Map);
				const FString Line1 = WinLives > 0 ? FString::Printf(TEXT("Won, best %d lives"), WinLives) : (Best > 0 ? FString::Printf(TEXT("Best wave %d"), Best) : FString(TEXT("Not cleared yet")));
				FString Line2 = Abyss >= 1 ? FString::Printf(TEXT("Abyss %d"), Abyss) : FString();
				if (BestScore > 0)
				{
					Line2 += (Line2.IsEmpty() ? TEXT("") : TEXT("  |  ")) + FString::Printf(TEXT("%s pts"), *FormatScore(BestScore));
				}
				// a hairline above the records, then one or two centred lines
				const float RecordsY = Card.Max.Y - (Line2.IsEmpty() ? 30.0f : 48.0f) * S;
				Fill(FBox2D(FVector2D(Card.Min.X + Pad * 2.0f, RecordsY - 8.0f * S), FVector2D(Card.Max.X - Pad * 2.0f, RecordsY - 7.0f * S)), WithAlpha(PanelEdge, 0.35f));
				Text(Line1, Card.GetCenter().X, RecordsY + 8.0f * S, WinLives > 0 ? PearlColor : (Best > 0 ? FLinearColor::White : DimText), FitSize(Line1, SizeSmall, CW), true, true);
				if (!Line2.IsEmpty())
				{
					Text(Line2, Card.GetCenter().X, RecordsY + 27.0f * S, Abyss >= 1 ? VioletColor : DimText, FitSize(Line2, SizeSmall, CW), true, true);
				}
			}
			else
			{
				const FString Note = TEXT("Normal rules only");
				const float RecordsY = Card.Max.Y - 30.0f * S;
				Fill(FBox2D(FVector2D(Card.Min.X + Pad * 2.0f, RecordsY - 8.0f * S), FVector2D(Card.Max.X - Pad * 2.0f, RecordsY - 7.0f * S)), WithAlpha(PanelEdge, 0.35f));
				Text(Note, Card.GetCenter().X, RecordsY + 8.0f * S, DimText, FitSize(Note, SizeSmall, CW), true, true);
			}
			if (Card.IsInside(MousePos) && Map >= 0)
			{
				SetTip({ { TD::MapName(Map), AccentColor }, { TD::MapDescription(Map), FLinearColor::White },
					{ FString::Printf(TEXT("Abyss unlocked up to %d"), GameMode->GetUnlockedTier(Map)), VioletColor } });
			}
		}
		Y += CardH * S + 20.0f * S;
	}

	// ---- Abyss tier ----
	Text(TEXT("Abyss tier"), Left, Y + 10.0f * S, AccentColor, SizeHeading, false, true);
	{
		const FString Hint = Setup.Map >= 0 ? FString::Printf(TEXT("Win wave %d at a tier to unlock the next one on that map"), TD::FinalWave) : FString(TEXT("Choose a map to play an Abyss tier"));
		const float HintSize = FitSize(Hint, SizeSmall, InnerW * 0.65f);
		Text(Hint, Right - MeasureText(Hint, HintSize).X, Y + 10.0f * S, DimText, HintSize, false, true);
	}
	Y += 28.0f * S;
	{
		const int32 Tiers = TD::MaxAbyssTier + 1;
		const float Gap = 8.0f * S;
		const float BtnW = (InnerW - Gap * (Tiers - 1)) / Tiers;
		const int32 Unlocked = Setup.Map >= 0 ? GameMode->GetUnlockedTier(Setup.Map) : 0;
		for (int32 t = 0; t < Tiers; ++t)
		{
			const bool bOpen = t <= Unlocked;
			const bool bSelected = Setup.Tier == t;
			const FString Label = t == 0 ? FString(TEXT("Normal")) : FString::Printf(TEXT("Abyss %d"), t);
			const FBox2D Rect(FVector2D(Left + t * (BtnW + Gap), Y), FVector2D(Left + t * (BtnW + Gap) + BtnW, Y + 42.0f * S));
			Button(Rect, bOpen ? Label : FString(), EButton::SetupTier, t, bSelected ? (t > 0 ? SupportButton : ActiveButton) : ButtonColor, bOpen, bSelected, SizeField);
			if (!bOpen)
			{
				// a padlock before the name, the pair centred on the button
				const float LockW = 13.0f * S;
				const float LockH = 16.0f * S;
				const float LabelW = MeasureText(Label, SizeField).X;
				const float GroupX = Rect.GetCenter().X - (LockW + 7.0f * S + LabelW) * 0.5f;
				const float MidY = Rect.GetCenter().Y;
				Padlock(FBox2D(FVector2D(GroupX, MidY - LockH * 0.5f - 1.0f * S), FVector2D(GroupX + LockW, MidY + LockH * 0.5f - 1.0f * S)), DisabledText);
				Text(Label, GroupX + LockW + 7.0f * S, MidY, DisabledText, SizeField, false, true);
			}
			if (Rect.IsInside(MousePos) && t > 0)
			{
				TArray<TPair<FString, FLinearColor>> Tip = { { FString::Printf(TEXT("Abyss %d  (+%d%% score)"), t, FMath::RoundToInt(TD::AbyssScorePerTier * 100 * t)), VioletColor } };
				for (int32 r = 1; r <= t; ++r)
				{
					Tip.Add({ TD::AbyssRule(r), FLinearColor::White });
				}
				if (!bOpen)
				{
					Tip.Add({ Setup.Map >= 0 ? FString::Printf(TEXT("Win wave %d at Abyss %d on this map to unlock it"), TD::FinalWave, t - 1) : FString(TEXT("Choose a map first")), LifeColor });
				}
				SetTip(Tip);
			}
		}
		Y += 42.0f * S + 10.0f * S;
		WrappedTextCentered(AbyssSummary(Setup.Tier), CX, Y, InnerW, Setup.Tier > 0 ? Lighten(VioletColor, 0.2f) : InfoText, SizeField, 20.0f * S);
		Y += 36.0f * S;
	}

	// ---- mutators: four per row ----
	Text(TEXT("Mutators"), Left, Y + 10.0f * S, AccentColor, SizeHeading, false, true);
	Y += 28.0f * S;
	{
		const int32 PerRow = 4;
		const float Gap = 10.0f * S;
		const float ToggleW = (InnerW - Gap * (PerRow - 1)) / PerRow;
		const float ToggleH = 40.0f * S;
		for (int32 m = 0; m < TD::MutatorCount; ++m)
		{
			const ETDMutator Mutator = (ETDMutator)m;
			const bool bIncluded = TD::IsMutatorIncluded(Mutator, Setup.Tier);
			const bool bOn = bIncluded || (Setup.Mutators & (1u << (uint32)m)) != 0;
			// a short last row is centred under the full ones
			const int32 InRow = FMath::Min(PerRow, TD::MutatorCount - (m / PerRow) * PerRow);
			const float RowX = Left + (InnerW - (InRow * ToggleW + (InRow - 1) * Gap)) * 0.5f;
			const float TX = RowX + (m % PerRow) * (ToggleW + Gap);
			const float TY = Y + (m / PerRow) * (ToggleH + 6.0f * S);
			const FBox2D Rect(FVector2D(TX, TY), FVector2D(TX + ToggleW, TY + ToggleH));
			Button(Rect, TEXT(""), EButton::SetupMutator, m, ButtonColor, !bIncluded, false, SizeField);

			const float SwitchW = 38.0f * S;
			const float SwitchH = 22.0f * S;
			const FBox2D SwitchRect(FVector2D(Rect.Max.X - 12.0f * S - SwitchW, Rect.GetCenter().Y - SwitchH * 0.5f), FVector2D(Rect.Max.X - 12.0f * S, Rect.GetCenter().Y + SwitchH * 0.5f));
			const FString Bonus = bIncluded ? FString(TEXT("in Abyss 4")) : FString::Printf(TEXT("+%d%%"), FMath::RoundToInt(TD::MutatorScore(Mutator) * 100));
			const float BonusW = MeasureText(Bonus, SizeSmall).X;
			Text(Bonus, SwitchRect.Min.X - 10.0f * S - BonusW, Rect.GetCenter().Y, bIncluded ? DimText : PearlColor, SizeSmall, false, true);
			const FString Name = TD::MutatorName(Mutator);
			Text(Name, Rect.Min.X + 14.0f * S, Rect.GetCenter().Y, bIncluded ? DimText : FLinearColor::White, FitSize(Name, SizeField, SwitchRect.Min.X - BonusW - Rect.Min.X - 34.0f * S), false, true);
			Switch(SwitchRect, bOn);
			if (Rect.IsInside(MousePos))
			{
				TArray<TPair<FString, FLinearColor>> Tip = { { Name, LifeColor }, { TD::MutatorDescription(Mutator), FLinearColor::White } };
				Tip.Add({ bIncluded ? FString(TEXT("Already part of Abyss 4 and up")) : FString::Printf(TEXT("+%d%% score"), FMath::RoundToInt(TD::MutatorScore(Mutator) * 100)), bIncluded ? DimText : PearlColor });
				SetTip(Tip);
			}
		}
		const int32 Rows = (TD::MutatorCount + PerRow - 1) / PerRow;
		Y += Rows * ToggleH + (Rows - 1) * 6.0f * S + 18.0f * S;
	}

	// ---- the resulting score multiplier ----
	{
		const FString Mult = FString::Printf(TEXT("Score multiplier  %s"), *TD::MultiplierText(GameMode->GetSetupMultiplier()));
		Text(Mult, CX, Y + 14.0f * S, PearlColor, 26.0f, true, true);
		const FString Formula = TEXT("Score = (waves cleared x 100 + lives left x 25 on a win) x multiplier");
		const FString BoonNote = TEXT("A boon is offered before wave 1 and after waves 25, 50 and 75; cursed boons raise the multiplier.");
		Text(Formula, CX, Y + 42.0f * S, InfoText, FitSize(Formula, SizeSmall, InnerW), true, true);
		Text(BoonNote, CX, Y + 62.0f * S, DimText, FitSize(BoonNote, SizeSmall, InnerW), true, true);
		Y += 72.0f * S + 16.0f * S;
	}

	// ---- save slot for the new run ----
	{
		const float LabelW = MeasureText(TEXT("Save slot"), SizeBody).X + 16.0f * S;
		Text(TEXT("Save slot"), Left, Y + 20.0f * S, AccentColor, SizeBody, false, true);
		const float Gap = 8.0f * S;
		const float SlotW = (InnerW - LabelW - Gap * (ATDGameMode::SaveSlotCount - 1)) / ATDGameMode::SaveSlotCount;
		for (int32 Slot = 0; Slot < ATDGameMode::SaveSlotCount; ++Slot)
		{
			const FTDSlotInfo& Info = GameMode->GetSlotInfo(Slot);
			const bool bSelected = Setup.Slot == Slot;
			const FBox2D Rect(FVector2D(Left + LabelW + Slot * (SlotW + Gap), Y), FVector2D(Left + LabelW + Slot * (SlotW + Gap) + SlotW, Y + 40.0f * S));
			const FString Label = Info.bUsed ? FString::Printf(TEXT("%d: %s, wave %d"), Slot + 1, TD::MapName(Info.Map), Info.Wave) : FString::Printf(TEXT("%d: empty"), Slot + 1);
			Button(Rect, Label, EButton::SetupSlot, Slot, bSelected ? ActiveButton : ButtonColor, true, bSelected, SizeSmall);
			if (Rect.IsInside(MousePos))
			{
				SetTip({ { FString::Printf(TEXT("Save slot %d"), Slot + 1), AccentColor },
					{ Info.bUsed ? SlotSummary(Info) + TEXT(". Starting here replaces that run (you'll be asked).") : FString(TEXT("Empty: the new run saves here.")), FLinearColor::White } });
			}
		}
		Y += 52.0f * S;
	}

	// ---- Back / Start ----
	{
		const float BtnW = 260.0f * S;
		const float BtnH = 50.0f * S;
		Button(FBox2D(FVector2D(CX - BtnW - 8.0f * S, Y), FVector2D(CX - 8.0f * S, Y + BtnH)), TEXT("Back"), EButton::SetupBack, 0, ButtonColor, true, false, 19.0f);
		Button(FBox2D(FVector2D(CX + 8.0f * S, Y), FVector2D(CX + 8.0f * S + BtnW, Y + BtnH)), TEXT("Start"), EButton::SetupStart, 0, AmberButton, true, true, 19.0f);
	}

	UIScale = SavedScale;
}

// ---------------------------------------------------------------------------------------------
// Boon offer
// ---------------------------------------------------------------------------------------------

void ATDHUD::DrawBoonMenu(ATDGameMode* GameMode)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	Panel(FBox2D(FVector2D(0, 0), FVector2D(W, H)), FLinearColor(0.0f, 0.01f, 0.02f, 0.7f));
	DrawSeaBackdrop(0.6f);

	const TArray<int32>& Offer = GameMode->GetBoonOffer();
	const float BaseW = 880.0f;
	const float CardH = 280.0f;	// room for a cursed boon's drawback line
	const float ContentH = 28.0f + 92.0f + CardH + 20.0f + 26.0f + 24.0f;
	const float SavedScale = UIScale;
	UIScale = FMath::Min3(UIScale, H * 0.97f / ContentH, W * 0.97f / BaseW);
	const float S = UIScale;

	const float BoxW = BaseW * S;
	const float BoxH = ContentH * S;
	const float CX = W * 0.5f;
	const FBox2D Box(FVector2D(CX - BoxW * 0.5f, (H - BoxH) * 0.5f), FVector2D(CX + BoxW * 0.5f, (H + BoxH) * 0.5f));
	Glow(Box, WithAlpha(VioletColor, 0.1f), RadiusPanel * S, 24.0f * S);
	Panel(Box, ModalColor, false);
	RoundFrame(Box, WithAlpha(VioletColor, 0.45f), RadiusPanel * S, 1.0f * S);
	PanelDecor(Box, VioletColor);

	float Y = Box.Min.Y + 28.0f * S;
	Text(TEXT("Choose a boon"), CX, Y + 16.0f * S, VioletColor, 34.0f, true, true);
	const int32 Cleared = GameMode->GetWavesCleared();
	FString Sub = TEXT("One of these stays for the rest of the run");
	if (GameMode->GetWave() == 0)
	{
		Sub = TEXT("Before wave 1: ") + Sub.ToLower();
	}
	else if (TD::IsBoonMilestone(Cleared))
	{
		Sub = FString::Printf(TEXT("Wave %d cleared: "), Cleared) + Sub.ToLower();
	}
	Text(Sub, CX, Y + 58.0f * S, DimText, FitSize(Sub, SizeField, BoxW - 60.0f * S), true, true);
	Y += 92.0f * S;

	const float Left = Box.Min.X + 30.0f * S;
	const float InnerW = BoxW - 60.0f * S;
	if (Offer.Num() == 0)
	{
		Text(TEXT("No boon on offer"), CX, Y + CardH * S * 0.4f, DimText, SizeBody, true, true);
		const float BtnW = 260.0f * S;
		Button(FBox2D(FVector2D(CX - BtnW * 0.5f, Y + CardH * S * 0.55f), FVector2D(CX + BtnW * 0.5f, Y + CardH * S * 0.55f + 50.0f * S)), TEXT("Continue"), EButton::MenuResume, 0, ActiveButton, true, true, 19.0f);
	}
	else
	{
		const float Gap = 18.0f * S;
		const int32 Count = Offer.Num();
		const float CardW = (InnerW - Gap * (TD::BoonOfferSize - 1)) / TD::BoonOfferSize;
		const float RowW = Count * CardW + (Count - 1) * Gap;
		const float X0 = CX - RowW * 0.5f;
		for (int32 i = 0; i < Count; ++i)
		{
			const int32 Boon = Offer[i];
			const bool bCursed = TD::IsCursed((ETDBoon)Boon);
			const FBox2D Card(FVector2D(X0 + i * (CardW + Gap), Y), FVector2D(X0 + i * (CardW + Gap) + CardW, Y + CardH * S));
			const bool bHovered = Card.IsInside(MousePos);
			const FLinearColor CursedFill = SRGB(56, 16, 40, 0.94f);
			if (bCursed)
			{
				Glow(Card, WithAlpha(CursedColor, bHovered ? 0.35f : 0.2f), RadiusButton * S, 12.0f * S);
			}
			Button(Card, TEXT(""), EButton::PickBoon, Boon, bCursed ? CursedFill : (bHovered ? SupportButton : ButtonColor), true, bHovered && !bCursed, SizeBody);
			if (bCursed)
			{
				// dark red rim, a CURSED tag on the left and the score bonus on the right
				RoundFrame(Card, WithAlpha(CursedColor, bHovered ? 1.0f : 0.75f), RadiusButton * S, 2.0f * S);
				const FString Tag = TEXT("CURSED");
				const float TagH = 24.0f * S;
				const float TagPad = 9.0f * S;
				const float TagY = Card.Min.Y + 10.0f * S;
				const float TagW = MeasureText(Tag, SizeSmall).X + TagPad * 2.0f;
				const FBox2D TagRect(FVector2D(Card.Min.X + 10.0f * S, TagY), FVector2D(Card.Min.X + 10.0f * S + TagW, TagY + TagH));
				RoundFill(TagRect, WithAlpha(CursedColor * 0.4f, 0.95f), TagH * 0.5f);
				Text(Tag, TagRect.GetCenter().X, TagRect.GetCenter().Y, Lighten(CursedColor, 0.3f), SizeSmall, true, true);
				const FString Bonus = FString::Printf(TEXT("+%d%% score"), FMath::RoundToInt(TD::BoonScore((ETDBoon)Boon) * 100));
				const float BonusW = MeasureText(Bonus, SizeSmall).X + TagPad * 2.0f;
				const FBox2D BonusRect(FVector2D(Card.Max.X - 10.0f * S - BonusW, TagY), FVector2D(Card.Max.X - 10.0f * S, TagY + TagH));
				RoundFill(BonusRect, WithAlpha(PearlColor * 0.3f, 0.95f), TagH * 0.5f);
				Text(Bonus, BonusRect.GetCenter().X, BonusRect.GetCenter().Y, Lighten(PearlColor, 0.2f), SizeSmall, true, true);
			}

			const float IconSize = 76.0f * S;
			const FVector2D IconMin(Card.GetCenter().X - IconSize * 0.5f, Card.Min.Y + 26.0f * S);
			Glow(FBox2D(IconMin, IconMin + FVector2D(IconSize, IconSize)).ExpandBy(-10.0f * S), WithAlpha(VioletColor, bHovered ? 0.35f : 0.18f), IconSize * 0.5f, 14.0f * S);
			Icon(BoonIcon(Boon), FBox2D(IconMin, IconMin + FVector2D(IconSize, IconSize)));

			const FString Name = TD::BoonName((ETDBoon)Boon);
			Text(Name, Card.GetCenter().X, Card.Min.Y + 128.0f * S, FLinearColor::White, FitSize(Name, SizeHeading, CardW - 20.0f * S), true, true);
			const float TextW = CardW - 36.0f * S;
			const FString Desc = TD::BoonDescription((ETDBoon)Boon);
			const float DescY = Card.Min.Y + 152.0f * S;
			const float DescH = WrappedTextCentered(Desc, Card.GetCenter().X, DescY, TextW, InfoText, SizeField, 21.0f * S);
			if (bCursed)
			{
				WrappedTextCentered(TD::BoonDrawback((ETDBoon)Boon), Card.GetCenter().X, DescY + DescH + 6.0f * S, TextW, Lighten(LifeColor, 0.15f), SizeField, 21.0f * S);
			}
			Text(TEXT("Click to take"), Card.GetCenter().X, Card.Max.Y - 20.0f * S, bHovered ? AccentColor : DimText, SizeSmall, true, true);
		}
	}
	Y += CardH * S + 20.0f * S;

	// boons taken so far
	TArray<FString> Taken;
	for (int32 b = 0; b < TD::BoonCount; ++b)
	{
		if (GameMode->GetRunRules().Has((ETDBoon)b))
		{
			Taken.Add(TD::IsCursed((ETDBoon)b) ? FString(TD::BoonName((ETDBoon)b)) + TEXT(" (cursed)") : FString(TD::BoonName((ETDBoon)b)));
		}
	}
	const FString TakenText = Taken.Num() > 0 ? TEXT("Taken: ") + FString::Join(Taken, TEXT(", ")) : FString(TEXT("No boons taken yet.  Esc opens the pause menu."));
	Text(TakenText, CX, Y + 10.0f * S, DimText, FitSize(TakenText, SizeSmall, InnerW), true, true);

	UIScale = SavedScale;
}

// ---------------------------------------------------------------------------------------------
// Tower mastery
// ---------------------------------------------------------------------------------------------

void ATDHUD::Padlock(const FBox2D& Rect, const FLinearColor& Color)
{
	const float W = Rect.GetSize().X;
	const float H = Rect.GetSize().Y;
	const FVector2D C = Rect.GetCenter();
	const float BodyW = W * 0.7f;
	const float BodyH = H * 0.5f;
	const FBox2D Body(FVector2D(C.X - BodyW * 0.5f, Rect.Max.Y - BodyH), FVector2D(C.X + BodyW * 0.5f, Rect.Max.Y));
	const float ShackleW = W * 0.46f;
	const FBox2D Shackle(FVector2D(C.X - ShackleW * 0.5f, Rect.Min.Y), FVector2D(C.X + ShackleW * 0.5f, Body.Min.Y + H * 0.2f));
	RoundFrame(Shackle, Color, ShackleW * 0.5f, FMath::Max(1.5f, W * 0.1f));
	RoundFill(Body, Color, W * 0.1f);
	RoundFill(FBox2D(FVector2D(C.X - W * 0.05f, Body.Min.Y + BodyH * 0.3f), FVector2D(C.X + W * 0.05f, Body.Min.Y + BodyH * 0.7f)), FLinearColor(0.02f, 0.06f, 0.08f, 0.9f), W * 0.04f);
}

void ATDHUD::DrawMasteryMenu(ATDGameMode* GameMode)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	Panel(FBox2D(FVector2D(0, 0), FVector2D(W, H)), FLinearColor(0.0f, 0.01f, 0.02f, 0.78f));
	DrawSeaBackdrop(0.75f);

	// the attack towers: aura towers and walls make no kills
	TArray<ETDTowerType> Types;
	for (int32 t = 0; t < TD::TowerTypeCount; ++t)
	{
		if (TD::Tower((ETDTowerType)t).bAttacks)
		{
			Types.Add((ETDTowerType)t);
		}
	}
	const float RowH = 58.0f;
	const float BaseW = 780.0f;
	const float ContentH = 28.0f + 104.0f + Types.Num() * (RowH + 6.0f) + 14.0f + 50.0f + 28.0f;
	const float SavedScale = UIScale;
	UIScale = FMath::Min3(UIScale, H * 0.97f / ContentH, W * 0.97f / BaseW);
	const float S = UIScale;

	const float BoxW = BaseW * S;
	const float BoxH = ContentH * S;
	const float CX = W * 0.5f;
	const FBox2D Box(FVector2D(CX - BoxW * 0.5f, (H - BoxH) * 0.5f), FVector2D(CX + BoxW * 0.5f, (H + BoxH) * 0.5f));
	Glow(Box, WithAlpha(AccentColor, 0.08f), RadiusPanel * S, 24.0f * S);
	Panel(Box, ModalColor, false);
	RoundFrame(Box, WithAlpha(AccentColor, 0.35f), RadiusPanel * S, 1.0f * S);
	PanelDecor(Box, AccentColor);

	float Y = Box.Min.Y + 28.0f * S;
	Text(TEXT("Tower mastery"), CX, Y + 16.0f * S, AccentColor, 34.0f, true, true);
	const FString Sub = FString::Printf(TEXT("Lifetime kills from games that count for records. Ranks at %s, %s, %s and %s kills add a trim to every tower of that type."),
		*FText::AsNumber(TD::MasteryThreshold(1)).ToString(), *FText::AsNumber(TD::MasteryThreshold(2)).ToString(), *FText::AsNumber(TD::MasteryThreshold(3)).ToString(), *FText::AsNumber(TD::MasteryThreshold(4)).ToString());
	const float Left = Box.Min.X + 30.0f * S;
	const float InnerW = BoxW - 60.0f * S;
	WrappedTextCentered(Sub, CX, Y + 44.0f * S, InnerW, InfoText, SizeSmall, 20.0f * S);
	Y += 104.0f * S;

	float ChipColumnW = 0.0f;
	for (int32 r = 0; r <= TD::MasteryRankCount; ++r)
	{
		ChipColumnW = FMath::Max(ChipColumnW, MeasureText(TD::MasteryName(r), SizeSmall).X + 22.0f * S);
	}
	for (const ETDTowerType Type : Types)
	{
		const int32 Kills = TD::LifetimeKills(Type);
		const int32 Rank = TD::MasteryRank(Kills);
		const FLinearColor RankColor = TD::MasteryColor(Rank);
		const FBox2D Row(FVector2D(Left, Y), FVector2D(Left + InnerW, Y + RowH * S));
		RoundFill(Row, InsetColor, RadiusButton * S);
		if (Rank > 0)
		{
			RoundFill(FBox2D(Row.Min, FVector2D(Row.Min.X + 4.0f * S, Row.Max.Y)), WithAlpha(RankColor, 0.9f), 2.0f * S);
		}

		const float IconSize = 44.0f * S;
		const FVector2D IconMin(Row.Min.X + 12.0f * S, Row.GetCenter().Y - IconSize * 0.5f);
		Icon(TDIcons::Tower(Type), FBox2D(IconMin, IconMin + FVector2D(IconSize, IconSize)));
		const float NameX = IconMin.X + IconSize + 12.0f * S;
		Text(TD::Tower(Type).Name, NameX, Row.Min.Y + 18.0f * S, FLinearColor::White, SizeBody, false, true);
		Text(FString::Printf(TEXT("%s kills"), *FText::AsNumber(Kills).ToString()), NameX, Row.Min.Y + 40.0f * S, DimText, SizeSmall, false, true);

		// rank chip: one width for every rank, so the chips line up in a column
		const FString RankText = TD::MasteryName(Rank);
		const float ChipW = ChipColumnW;
		const float ChipH = 26.0f * S;
		const float ChipX = Row.Min.X + 250.0f * S;
		const FBox2D Chip(FVector2D(ChipX, Row.GetCenter().Y - ChipH * 0.5f), FVector2D(ChipX + ChipW, Row.GetCenter().Y + ChipH * 0.5f));
		RoundFill(Chip, WithAlpha(RankColor * (Rank > 0 ? 0.22f : 0.12f), 0.95f), Chip.GetSize().Y * 0.5f);
		RoundFrame(Chip, WithAlpha(RankColor, Rank > 0 ? 0.55f : 0.2f), Chip.GetSize().Y * 0.5f, 1.0f * S);
		if (Rank >= TD::MasteryRankCount)
		{
			Glow(Chip, WithAlpha(RankColor, 0.4f), Chip.GetSize().Y * 0.5f, 6.0f * S);
		}
		Text(RankText, Chip.GetCenter().X, Chip.GetCenter().Y, Rank > 0 ? Lighten(RankColor, 0.2f) : DimText, SizeSmall, true, true);

		// progress to the next rank
		// fills are darkened so the white label stays readable on the bright silver and abyssal colors
		const FBox2D Bar(FVector2D(Chip.Max.X + 24.0f * S, Row.GetCenter().Y - 12.0f * S), FVector2D(Row.Max.X - 16.0f * S, Row.GetCenter().Y + 12.0f * S));
		auto BarColor = [](const FLinearColor& C)
		{
			// scale every color to about the same dim brightness, so silver and abyssal get no brighter than bronze
			const float Scale = FMath::Min(0.6f, 0.16f / FMath::Max(0.01f, C.GetLuminance()));
			return FLinearColor(C.R * Scale, C.G * Scale, C.B * Scale, 1.0f);
		};
		if (Rank >= TD::MasteryRankCount)
		{
			Meter(Bar, 1.0f, BarColor(RankColor), TEXT("Highest rank"));
		}
		else
		{
			const int32 From = TD::MasteryThreshold(Rank);
			const int32 To = TD::MasteryThreshold(Rank + 1);
			const float Fraction = FMath::Clamp((float)(Kills - From) / FMath::Max(1, To - From), 0.0f, 1.0f);
			Meter(Bar, Fraction, BarColor(TD::MasteryColor(Rank + 1)), FString::Printf(TEXT("%s / %s to %s"), *FText::AsNumber(Kills).ToString(), *FText::AsNumber(To).ToString(), TD::MasteryName(Rank + 1)));
		}
		Y += (RowH + 6.0f) * S;
	}
	Y += 14.0f * S;

	const float BtnW = 260.0f * S;
	Button(FBox2D(FVector2D(CX - BtnW * 0.5f, Y), FVector2D(CX + BtnW * 0.5f, Y + 50.0f * S)), TEXT("Back"), EButton::MasteryBack, 0, ActiveButton, true, true, 19.0f);

	UIScale = SavedScale;
}

// ---------------------------------------------------------------------------------------------
// Save slots and questions
// ---------------------------------------------------------------------------------------------

FString ATDHUD::SlotSummary(const FTDSlotInfo& Info)
{
	if (!Info.bUsed)
	{
		return TEXT("Empty");
	}
	FString Text = FString::Printf(TEXT("%s, wave %d%s"), TD::MapName(Info.Map), Info.Wave, Info.bEndless ? TEXT(" (endless)") : TEXT(""));
	if (Info.Tier > 0)
	{
		Text += FString::Printf(TEXT(", Abyss %d"), Info.Tier);
	}
	if (Info.MutatorCount > 0)
	{
		Text += FString::Printf(TEXT(", %d %s"), Info.MutatorCount, Info.MutatorCount == 1 ? TEXT("mutator") : TEXT("mutators"));
	}
	return Text;
}

void ATDHUD::DrawSlotMenu(ATDGameMode* GameMode)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	Panel(FBox2D(FVector2D(0, 0), FVector2D(W, H)), FLinearColor(0.0f, 0.01f, 0.02f, 0.78f));
	DrawSeaBackdrop(0.75f);

	const float CardH = 372.0f;	// map outline, five facts and the Load / Delete row
	const float BaseW = 960.0f;
	const float ContentH = 28.0f + 92.0f + CardH + 24.0f + 50.0f + 28.0f;
	const float SavedScale = UIScale;
	UIScale = FMath::Min3(UIScale, H * 0.97f / ContentH, W * 0.97f / BaseW);
	const float S = UIScale;

	const float BoxW = BaseW * S;
	const float BoxH = ContentH * S;
	const float CX = W * 0.5f;
	const FBox2D Box(FVector2D(CX - BoxW * 0.5f, (H - BoxH) * 0.5f), FVector2D(CX + BoxW * 0.5f, (H + BoxH) * 0.5f));
	Glow(Box, WithAlpha(AccentColor, 0.08f), RadiusPanel * S, 24.0f * S);
	Panel(Box, ModalColor, false);
	RoundFrame(Box, WithAlpha(AccentColor, 0.35f), RadiusPanel * S, 1.0f * S);
	PanelDecor(Box, AccentColor);

	float Y = Box.Min.Y + 28.0f * S;
	Text(TEXT("Saved games"), CX, Y + 16.0f * S, AccentColor, 34.0f, true, true);
	Text(TEXT("Each slot holds one run; autosaves go to the run's own slot"), CX, Y + 58.0f * S, DimText, SizeField, true, true);
	Y += 92.0f * S;

	const float Left = Box.Min.X + 30.0f * S;
	const float InnerW = BoxW - 60.0f * S;
	const float Gap = 16.0f * S;
	const float CardW = (InnerW - Gap * (ATDGameMode::SaveSlotCount - 1)) / ATDGameMode::SaveSlotCount;
	for (int32 Slot = 0; Slot < ATDGameMode::SaveSlotCount; ++Slot)
	{
		const FTDSlotInfo& Info = GameMode->GetSlotInfo(Slot);
		const bool bCurrent = Slot == GameMode->GetCurrentSlot() && Info.bUsed;
		const FBox2D Card(FVector2D(Left + Slot * (CardW + Gap), Y), FVector2D(Left + Slot * (CardW + Gap) + CardW, Y + CardH * S));
		RoundFill(Card, InsetColor, RadiusButton * S);
		RoundFrame(Card, bCurrent ? WithAlpha(AccentColor, 0.7f) : WithAlpha(PanelEdge, 0.4f), RadiusButton * S, (bCurrent ? 1.5f : 1.0f) * S);
		const float Pad = 14.0f * S;
		const float CW = CardW - Pad * 2.0f;
		float TY = Card.Min.Y + 22.0f * S;
		Text(FString::Printf(TEXT("Slot %d"), Slot + 1), Card.Min.X + Pad, TY, AccentColor, SizeHeading, false, true);
		if (bCurrent)
		{
			const FString Tag = TEXT("current run");
			Text(Tag, Card.Max.X - Pad - MeasureText(Tag, SizeCaption).X, TY, AccentColor, SizeCaption, false, true);
		}
		TY += 34.0f * S;
		if (!Info.bUsed)
		{
			Text(TEXT("Empty"), Card.GetCenter().X, Card.Min.Y + CardH * S * 0.45f, DimText, SizeBody, true, true);
		}
		else
		{
			// map outline, then the facts
			const FBox2D Thumb(FVector2D(Card.Min.X + Pad, TY), FVector2D(Card.Max.X - Pad, TY + CW * TD::Rows / TD::Cols * 0.6f));
			const FBox2D ThumbCentered(FVector2D(Thumb.GetCenter().X - Thumb.GetSize().Y * TD::Cols / TD::Rows * 0.5f, Thumb.Min.Y), FVector2D(Thumb.GetCenter().X + Thumb.GetSize().Y * TD::Cols / TD::Rows * 0.5f, Thumb.Max.Y));
			RoundFill(ThumbCentered.ExpandBy(2.0f * S), WithAlpha(InsetColor, 0.8f), RadiusTile * S);
			DrawMapThumbnail(Info.Map, ThumbCentered, 1.0f);
			TY = Thumb.Max.Y + 18.0f * S;
			const FString MapLine = TD::MapName(Info.Map);
			Text(MapLine, Card.Min.X + Pad, TY, FLinearColor::White, FitSize(MapLine, SizeBody, CW), false, true);
			TY += 24.0f * S;
			FString Rules = Info.Tier > 0 ? FString::Printf(TEXT("Abyss %d"), Info.Tier) : FString(TEXT("Normal"));
			if (Info.MutatorCount > 0)
			{
				Rules += FString::Printf(TEXT(", %d %s"), Info.MutatorCount, Info.MutatorCount == 1 ? TEXT("mutator") : TEXT("mutators"));
			}
			Text(Rules, Card.Min.X + Pad, TY, Info.Tier > 0 ? VioletColor : DimText, FitSize(Rules, SizeSmall, CW), false, true);
			TY += 22.0f * S;
			const int32 Seconds = FMath::FloorToInt(Info.PlayTime);
			const FString Time = Seconds >= 3600 ? FString::Printf(TEXT("%dh %02dm"), Seconds / 3600, (Seconds / 60) % 60) : FString::Printf(TEXT("%dm %02ds"), Seconds / 60, Seconds % 60);
			Field(Card.Min.X + Pad, TY, CW, TEXT("Wave"), FString::Printf(TEXT("%d%s"), Info.Wave, Info.bEndless ? TEXT(" (endless)") : TEXT("")), FLinearColor::White, SizeSmall);
			TY += 20.0f * S;
			Field(Card.Min.X + Pad, TY, CW, TEXT("Score"), FText::AsNumber(Info.Score).ToString(), PearlColor, SizeSmall);
			TY += 20.0f * S;
			Field(Card.Min.X + Pad, TY, CW, TEXT("Played"), Time, FLinearColor::White, SizeSmall);
			TY += 20.0f * S;
			Field(Card.Min.X + Pad, TY, CW, TEXT("Saved"), Info.SavedAt.ToString(TEXT("%Y-%m-%d %H:%M")), DimText, SizeSmall);
		}
		// Load and Delete along the bottom
		const float BtnH = 36.0f * S;
		const float BY = Card.Max.Y - Pad - BtnH;
		const float LoadW = (CW - 8.0f * S) * 0.62f;
		Button(FBox2D(FVector2D(Card.Min.X + Pad, BY), FVector2D(Card.Min.X + Pad + LoadW, BY + BtnH)), TEXT("Load"), EButton::SlotLoad, Slot, ActiveButton, Info.bUsed, Info.bUsed, SizeField);
		Button(FBox2D(FVector2D(Card.Min.X + Pad + LoadW + 8.0f * S, BY), FVector2D(Card.Max.X - Pad, BY + BtnH)), TEXT("Delete"), EButton::SlotDelete, Slot, DangerButton, Info.bUsed, false, SizeField);
	}
	Y += CardH * S + 24.0f * S;

	const float BtnW = 260.0f * S;
	Button(FBox2D(FVector2D(CX - BtnW * 0.5f, Y), FVector2D(CX + BtnW * 0.5f, Y + 50.0f * S)), TEXT("Back"), EButton::SlotBack, 0, ButtonColor, true, false, 19.0f);

	UIScale = SavedScale;
}

void ATDHUD::DrawConfirm(ATDGameMode* GameMode)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float S = UIScale;
	Panel(FBox2D(FVector2D(0, 0), FVector2D(W, H)), FLinearColor(0.0f, 0.01f, 0.02f, 0.55f));

	const bool bDelete = GameMode->GetPendingConfirm() == ETDConfirm::DeleteSlot;
	const FString Title = bDelete ? TEXT("Delete this run?") : TEXT("Replace this run?");
	const FString Body = GameMode->GetConfirmText();
	const float BoxW = FMath::Min(560.0f * S, W * 0.92f);
	const float TextW = BoxW - 60.0f * S;
	const float BodyH = WrappedText(Body, 0.0f, 0.0f, TextW, FLinearColor::White, SizeBody, 24.0f * S, false);
	const float BoxH = 30.0f * S + 44.0f * S + BodyH + 24.0f * S + 48.0f * S + 26.0f * S;
	const FBox2D Box(FVector2D((W - BoxW) * 0.5f, (H - BoxH) * 0.5f), FVector2D((W + BoxW) * 0.5f, (H + BoxH) * 0.5f));
	Glow(Box, WithAlpha(LifeColor, 0.12f), RadiusPanel * S, 20.0f * S);
	Panel(Box, ModalColor, false);
	RoundFrame(Box, WithAlpha(LifeColor, 0.55f), RadiusPanel * S, 1.0f * S);

	float Y = Box.Min.Y + 30.0f * S;
	Text(Title, Box.GetCenter().X, Y + 12.0f * S, LifeColor, 28.0f, true, true);
	Y += 44.0f * S;
	WrappedText(Body, Box.Min.X + 30.0f * S, Y, TextW, FLinearColor::White, SizeBody, 24.0f * S);
	Y += BodyH + 24.0f * S;
	const float BtnW = (BoxW - 60.0f * S - 12.0f * S) * 0.5f;
	Button(FBox2D(FVector2D(Box.Min.X + 30.0f * S, Y), FVector2D(Box.Min.X + 30.0f * S + BtnW, Y + 48.0f * S)), TEXT("Cancel"), EButton::ConfirmNo, 0, ButtonColor, true, false, 19.0f);
	Button(FBox2D(FVector2D(Box.Max.X - 30.0f * S - BtnW, Y), FVector2D(Box.Max.X - 30.0f * S, Y + 48.0f * S)), bDelete ? TEXT("Delete") : TEXT("Replace"), EButton::ConfirmYes, 0, DangerButton, true, true, 19.0f);
}
