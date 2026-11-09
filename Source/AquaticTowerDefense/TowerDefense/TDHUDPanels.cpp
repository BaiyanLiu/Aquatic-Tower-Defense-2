// Aquatic Tower Defense - isometric underwater tower defense with mazing.

// Side panels: tower to build, placed tower (Stats | Items | Skills), creature details, storage and next-wave preview.

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

using namespace TDHUDStyle;

void ATDHUD::DrawInfoPanel(ATDGameMode* GameMode, ATDPlayerController* PC)
{
	// a clicked creature stays pinned (creatures swim past the mouse too fast to read on hover),
	// then a creature under the mouse, then the selected tower
	if (ATDEnemy* Selected = PC->GetSelectedEnemy())
	{
		DrawEnemyPanel(GameMode, Selected);
		return;
	}
	if (ATDEnemy* Hovered = PC->GetHoveredEnemy())
	{
		DrawEnemyPanel(GameMode, Hovered);
		return;
	}
	if (ATDTower* Tower = PC->GetSelectedTower())
	{
		DrawTowerPanel(GameMode, PC, Tower);
		return;
	}
	if (!PC->HasSelectedType())
	{
		return;
	}

	// info about the tower type about to be built
	const float S = UIScale;
	const float PanelW = 420.0f * S;
	const float X = Canvas->ClipX - PanelW - 14.0f * S;
	const float Y = SideTop * S;
	const float Pad = 16.0f * S;
	const float Line = 27.0f * S;
	const float InnerW = PanelW - Pad * 2.0f;

	const ETDTowerType Type = PC->GetSelectedType();
	const FTDTowerDef& Def = TD::Tower(Type);
	const int32 Lines = (Def.bAttacks || Def.bSupport) ? 5 : 2;
	const float IconSize = HeaderIcon * S;
	// the description wraps (effect words in their stat colors), and the panel grows by any extra lines
	const TArray<TPair<FString, FLinearColor>> DescParts = ColorizeEffects(Def.Description, DimText);
	const float DescLineH = 21.0f * S;
	const float DescH = FMath::Max(Line, WrappedColoredText(DescParts, 0, 0, InnerW, SizeField, DescLineH, false) + 6.0f * S);
	const FBox2D Rect(FVector2D(X, Y), FVector2D(X + PanelW, Y + Pad * 2.0f + IconSize + 6.0f * S + (Lines - 1) * Line + DescH - 6.0f * S));

	FramedPanel(Rect, PanelEdge);

	float CY = Y + Pad;
	PanelHeader(TDIcons::Tower(Type), X + Pad, CY, InnerW, Def.Name, AccentColor, FString::Printf(TEXT("%d pearls"), TD::TowerCost(Type)), PearlColor);
	CY += IconSize + 6.0f * S;
	WrappedColoredText(DescParts, X + Pad, CY, InnerW, SizeField, DescLineH);
	CY += DescH;
	if (Def.bAttacks)
	{
		FieldRow(X + Pad, CY, InnerW, Line, TEXT("Damage"), FString::Printf(TEXT("%s (%s)"), *DamageRangeText(TD::TowerDamage(Type, 1), TD::TowerDamageSpread(Type)), TD::DamageTypeName(Def.DamageType)),
			TEXT("Range"), FString::Printf(TEXT("%.2f tiles"), TD::TowerRange(Type, 1)), StatDamage, StatRange);
		FieldRow(X + Pad, CY, InnerW, Line, TEXT("Attack speed"), FString::Printf(TEXT("%.2f/s"), TD::TowerRate(Type, 1)),
			TEXT("Targets"), Def.bHitsFlying ? TEXT("All creatures") : TEXT("Crawling only"), StatRate);
	}
	else if (Def.bSupport)
	{
		Field(X + Pad, CY, InnerW, TEXT("Aura range"), FString::Printf(TEXT("%.1f tiles"), TD::TowerRange(Type, 1)), StatRange);
		CY += Line;
		Field(X + Pad, CY, InnerW, TEXT("XP during waves"), FString::Printf(TEXT("%.1f per second"), TD::SupportXpRate(Type)), StatXp);
		CY += Line;
	}
	if (Def.bAttacks || Def.bSupport)
	{
		Text(FString::Printf(TEXT("Levels up to %d, holds items, learns skills"), TD::MaxTowerLevel), X + Pad, CY, DimText, SizeSmall);
		CY += Line;
	}
	Text(TEXT("Click to place"), X + Pad, CY, InfoText, SizeField);
}

void ATDHUD::DrawEnemyPanel(ATDGameMode* GameMode, ATDEnemy* Enemy)
{
	const float S = UIScale;
	const float PanelW = 420.0f * S;
	const float X = Canvas->ClipX - PanelW - 14.0f * S;
	const float Y = SideTop * S;
	const float Pad = 16.0f * S;
	const float Line = 27.0f * S;
	const float InnerW = PanelW - Pad * 2.0f;
	const FTDEnemyDef& Def = Enemy->GetDef();

	TArray<ETDStatus> Statuses;
	Enemy->GetStatuses(Statuses);

	const float IconSize = HeaderIcon * S;
	const FBox2D Rect(FVector2D(X, Y), FVector2D(X + PanelW, Y + Pad * 2.0f + Line * 9.0f + IconSize + 6.0f * S));
	FramedPanel(Rect, TD::EnemyIconColor(Enemy->GetEnemyType()));

	float CY = Y + Pad;
	PanelHeader(TDIcons::Enemy(Enemy->GetEnemyType()), X + Pad, CY, InnerW, Enemy->GetDisplayName(), TD::EnemyIconColor(Enemy->GetEnemyType()) * 1.2f,
		FString::Printf(TEXT("Wave %d"), Enemy->GetWave()), DimText);
	CY += IconSize + 6.0f * S;
	const FString Description = Def.Description;
	Text(Description, X + Pad, CY, DimText, FitSize(Description, SizeSmall, InnerW));
	CY += Line;

	// health bar
	const float BarH = 18.0f * S;
	const float Fraction = FMath::Clamp(Enemy->GetHealth() / FMath::Max(1.0f, Enemy->GetMaxHealth()), 0.0f, 1.0f);
	Meter(FBox2D(FVector2D(X + Pad, CY), FVector2D(X + Pad + InnerW, CY + BarH)), Fraction, FMath::Lerp(FLinearColor(0.6f, 0.08f, 0.05f), FLinearColor(0.08f, 0.45f, 0.14f), Fraction),
		FString::Printf(TEXT("Health: %s / %s"), *ShortNumber(Enemy->GetHealth()), *ShortNumber(Enemy->GetMaxHealth())));
	CY += BarH + 8.0f * S;

	const float Armor = Enemy->GetArmor();
	const float AuraArmor = Armor - Enemy->GetBaseArmor();
	Field(X + Pad, CY, InnerW, TEXT("Armor"), FString::Printf(TEXT("%.0f%% (%s)%s"), Armor, TD::ArmorTypeName(Def.ArmorType),
		FMath::Abs(AuraArmor) > 0.5f ? *FString::Printf(TEXT("  [%+.0f from auras]"), AuraArmor) : TEXT("")), FLinearColor::White);
	CY += Line;
	Field(X + Pad, CY, InnerW, TEXT("Speed"), FString::Printf(TEXT("%.2f tiles/s%s"), Enemy->GetCurrentSpeed(), Def.bFlying ? TEXT(" (swimming)") : TEXT("")), FLinearColor::White);
	CY += Line;
	Field(X + Pad, CY, InnerW, TEXT("Reward"), FString::Printf(TEXT("%d pearls, %d XP"), Enemy->GetReward(), FMath::RoundToInt(Def.Reward * TD::KillXpPerReward)), PearlColor);
	CY += Line;
	Field(X + Pad, CY, InnerW, TEXT("Lives lost if it gets through"), FString::Printf(TEXT("%d"), Def.LivesLost), LifeColor);
	CY += Line;
	Field(X + Pad, CY, InnerW, TEXT("Item drop chance"), FString::Printf(TEXT("%g%%%s"), FMath::RoundToFloat(Enemy->GetDropChance() * 10000.0f) / 100.0f, Def.ItemLevelBonus > 0 ? *FString::Printf(TEXT(" (item level +%d)"), Def.ItemLevelBonus) : TEXT("")),
		FLinearColor::White);
	CY += Line;

	Text(Statuses.Num() > 0 ? TEXT("Active effects (hover for details):") : TEXT("No active effects"), X + Pad, CY, DimText, SizeSmall);
	CY += Line * 0.9f;

	// effect chips with names
	float CX = X + Pad;
	for (ETDStatus Status : Statuses)
	{
		const FString Label = TD::StatusName(Status);
		const FVector2D Extent = MeasureText(Label, SizeSmall);
		const FBox2D Chip(FVector2D(CX, CY), FVector2D(CX + Extent.X + 16.0f * S, CY + 26.0f * S));
		if (Chip.Max.X > X + PanelW - Pad)
		{
			break;
		}
		RoundFill(Chip, WithAlpha(TD::StatusColor(Status) * 0.3f, 0.9f), Chip.GetSize().Y * 0.5f);
		RoundFrame(Chip, WithAlpha(TD::StatusColor(Status), 0.8f), Chip.GetSize().Y * 0.5f, 1.0f * S);
		Text(Label, Chip.GetCenter().X, Chip.GetCenter().Y, FLinearColor::White, SizeSmall, true, true);
		if (Chip.IsInside(MousePos))
		{
			const float Time = Enemy->GetStatusTime(Status);
			SetTip({ { Label, TD::StatusColor(Status) }, { TD::StatusDescription(Status), FLinearColor::White },
				{ Time > 0.0f ? FString::Printf(TEXT("%.1f seconds left"), Time) : FString(TEXT("Lasts while the aura source is near")), DimText } }, true);
		}
		CX = Chip.Max.X + 6.0f * S;
	}
}

void ATDHUD::DrawWavePreview(ATDGameMode* GameMode)
{
	const float S = UIScale;
	const int32 Next = GameMode->GetWave() + 1;
	if (!GameMode->GetRunRules().ShowsWavePreview())
	{
		// Eclipse (cursed boon): only a dark card in the preview's place
		const FString Hidden = GameMode->GetRunRules().Has(ETDMutator::MurkyWater) ? FString::Printf(TEXT("Wave %d: hidden in murky water"), Next)
			: FString::Printf(TEXT("Wave %d: hidden by the Eclipse"), Next);
		const float HW = MeasureText(Hidden, SizeField).X + 32.0f * S;
		const float HH = 40.0f * S;
		const float HX = Canvas->ClipX * 0.5f - HW * 0.5f;
		const float HY = 12.0f * S + (TopClusterH * S - HH) * 0.5f;
		if (!TopLeftRect.bIsValid || HX > TopLeftRect.Max.X)
		{
			const FBox2D Card(FVector2D(HX, HY), FVector2D(HX + HW, HY + HH));
			FramedPanel(Card, WithAlpha(VioletColor, 0.5f));
			Text(Hidden, Card.GetCenter().X, Card.GetCenter().Y, VioletColor, SizeField, true, true);
		}
		return;
	}
	const int32 RulesKey = (int32)GameMode->GetRunRules().Mutators | (GameMode->GetRunRules().Tier << 16);
	if (PreviewWave != Next || PreviewRulesKey != RulesKey)
	{
		PreviewWave = Next;
		PreviewRulesKey = RulesKey;
		ATDGameMode::GetWaveComposition(Next, PreviewGroups);
	}
	const TArray<TPair<ETDEnemyType, int32>>& Groups = PreviewGroups;
	const float HealthMul = TD::WaveHealthMultiplier(Next) * GameMode->GetEnemyHealthPercent() / 100.0f;
	const TCHAR* Tag = Next % 10 == 0 ? TEXT("BOSS") : (ATDGameMode::IsMiniBossWave(Next) ? TEXT("MINI-BOSS") : (ATDGameMode::IsMixedWave(Next) ? TEXT("MIXED") : TEXT("")));
	const FString TagText = Tag;

	// one creature group: tinted icon well and count, with details on hover
	auto Entry = [this, &Groups, Next, HealthMul, S](int32 i, const FBox2D& Rect, float IconW)
	{
		const ETDEnemyType Type = Groups[i].Key;
		const FTDEnemyDef& Def = TD::Enemy(Type);
		const FLinearColor Tint = TD::EnemyIconColor(Type);
		RoundFill(Rect, WithAlpha(Tint * 0.22f, 0.8f), RadiusButton * S);
		RoundFrame(Rect, WithAlpha(Tint, 0.4f), RadiusButton * S, 1.0f * S);
		const float IY = Rect.GetCenter().Y - IconW * 0.5f;
		const FBox2D IconRect(FVector2D(Rect.Min.X + 6.0f * S, IY), FVector2D(Rect.Min.X + 6.0f * S + IconW, IY + IconW));
		Icon(TDIcons::Enemy(Type), IconRect);
		Text(FString::Printf(TEXT("x%d"), Groups[i].Value), IconRect.Max.X + 4.0f * S, Rect.GetCenter().Y, FLinearColor::White, SizeField, false, true);

		Buttons.Add({ Rect, EButton::PreviewEntry, i, false });
		if (Rect.IsInside(MousePos))
		{
			TArray<TPair<FString, FLinearColor>> Lines;
			Lines.Add({ FString::Printf(TEXT("%s  x%d"), Def.Name, Groups[i].Value), Tint * 1.2f });
			Lines.Add({ Def.Description, FLinearColor::White });
			// the same numbers the creatures spawn with: Swarm halves health and reward, flyers skip part of the wave
			// pressure, Armored Tide raises armor, Rising Tide and Frenzied Currents speed them up, Abyss 2 doubles leaks
			const FTDRunRules& Rules = TD::Run();
			const float Share = Rules.IsSwarm() && !Def.bBoss ? 0.5f : 1.0f;
			const float Health = Def.Health * HealthMul * Share / (Def.bFlying ? TD::WavePressureLatePart(Next) : 1.0f);
			float Armor = Def.Armor + Def.ArmorGain * (Next - 1) + TD::EndlessArmorBonus(Next);
			Armor = Armor > 0.0f ? Armor * Rules.ArmorScale() : Armor;
			const float Speed = Def.Speed * Rules.EnemySpeedScale() * Rules.RisingTideScale(Next) * (Next > TD::FinalWave ? TD::EndlessSpeedMultiplier(Next) : 1.0f);
			Lines.Add({ FString::Printf(TEXT("Health %s, armor %.0f%% (%s), speed %.2f"), *ShortNumber(Health), Armor, TD::ArmorTypeName(Def.ArmorType), Speed), DimText });
			Lines.Add({ FString::Printf(TEXT("Reward %g pearls, costs %d lives, %g%% item chance"), FMath::RoundToFloat(Def.Reward * Share * Rules.KillPearlScale() * 10.0f) / 10.0f,
				Rules.LeakLives(Def.LivesLost), FMath::RoundToFloat(Def.DropChance * TD::DropRateScale * Share * Rules.DropChanceScale() * 10000.0f) / 100.0f), DimText });
			SetTip(Lines);
		}
	};
	auto EntryWidth = [this, &Groups, S](int32 i, float IconW)
	{
		// icon 6 in from the left edge, count 8 in from the right
		return 6.0f * S + IconW + 4.0f * S + MeasureText(FString::Printf(TEXT("x%d"), Groups[i].Value), SizeField).X + 8.0f * S;
	};
	auto TagPill = [this, &TagText, S](float X, float CY)
	{
		const FVector2D Extent = MeasureText(TagText, SizeCaption);
		const FBox2D Pill(FVector2D(X, CY - Extent.Y * 0.5f - 3.0f * S), FVector2D(X + Extent.X + 14.0f * S, CY + Extent.Y * 0.5f + 3.0f * S));
		RoundFill(Pill, WithAlpha(VioletColor * 0.25f, 0.9f), 6.0f * S);
		RoundFrame(Pill, WithAlpha(VioletColor, 0.6f), 6.0f * S, 1.0f * S);
		Text(TagText, Pill.GetCenter().X, Pill.GetCenter().Y, VioletColor, SizeCaption, true, true);
		return Pill.GetSize().X;
	};

	// ---- preferred: a slim card in the top row, between the two top clusters ----
	{
		const float Top = 12.0f * S;
		const float H = TopClusterH * S;
		const float MidY = Top + H * 0.5f;
		const float Inner = 14.0f * S;
		const float IconW = 32.0f * S;
		const float EntryGap = 6.0f * S;
		const FString Caption = TEXT("INCOMING");
		const FString WaveLabel = FString::Printf(TEXT("Wave %d"), Next);
		const float TitleW = FMath::Max(MeasureText(Caption, SizeCaption).X, MeasureText(WaveLabel, SizeBody).X);
		float RowW = Inner * 2.0f + TitleW;
		for (int32 i = 0; i < Groups.Num(); ++i)
		{
			RowW += EntryGap + EntryWidth(i, IconW);
		}
		if (!TagText.IsEmpty())
		{
			RowW += 10.0f * S + MeasureText(TagText, SizeCaption).X + 14.0f * S;
		}

		const float Left = TopLeftRect.bIsValid ? TopLeftRect.Max.X + 12.0f * S : 14.0f * S;
		const float Right = TopRightRect.bIsValid ? TopRightRect.Min.X - 12.0f * S : Canvas->ClipX - 14.0f * S;
		if (Right - Left >= RowW)
		{
			const float X = FMath::Clamp((Canvas->ClipX - RowW) * 0.5f, Left, Right - RowW);
			const FBox2D Rect(FVector2D(X, Top), FVector2D(X + RowW, Top + H));
			FramedPanel(Rect, PanelEdge);
			Text(Caption, X + Inner, MidY - 10.0f * S, DimText, SizeCaption, false, true);
			Text(WaveLabel, X + Inner, MidY + 9.0f * S, FLinearColor::White, SizeBody, false, true);
			float EX = X + Inner + TitleW;
			const float EntryH = IconW + 10.0f * S;
			for (int32 i = 0; i < Groups.Num(); ++i)
			{
				EX += EntryGap;
				const float EW = EntryWidth(i, IconW);
				Entry(i, FBox2D(FVector2D(EX, MidY - EntryH * 0.5f), FVector2D(EX + EW, MidY + EntryH * 0.5f)), IconW);
				EX += EW;
			}
			if (!TagText.IsEmpty())
			{
				TagPill(EX + 10.0f * S, MidY);
			}
			return;
		}
	}

	// ---- fallback (narrow screens): a card above the dock on the left ----
	const float IconW = 34.0f * S;
	const float Pad = 12.0f * S;
	const float EntryW = 84.0f * S;
	const float EntryH = IconW + 8.0f * S;
	const int32 PerRow = 4;
	const int32 Rows = FMath::DivideAndRoundUp(Groups.Num(), PerRow);
	const FString Title = FString::Printf(TEXT("Next: wave %d"), Next);
	const float TitleW = MeasureText(Title, SizeBody).X + (TagText.IsEmpty() ? 0.0f : 10.0f * S + MeasureText(TagText, SizeCaption).X + 14.0f * S);
	// only as wide as this wave needs
	const float W = Pad * 2.0f + FMath::Max(FMath::Min(Groups.Num(), PerRow) * EntryW - 6.0f * S, TitleW);
	const float H = Pad * 2.0f + 28.0f * S + Rows * (EntryH + 6.0f * S) - 6.0f * S;
	float X = 14.0f * S;
	const float Y = DockTop() - 14.0f * S - H;	// (the controls hint that sat above the dock is gone)

	// slide out from under an open storage panel
	if (StorageRect.bIsValid && StorageRect.Intersect(FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H))))
	{
		X = StorageRect.Max.X + 14.0f * S;
	}

	const FBox2D Rect(FVector2D(X, Y), FVector2D(X + W, Y + H));
	FramedPanel(Rect, PanelEdge);
	const float TitleY = Y + Pad + 10.0f * S;
	const float TW = Text(Title, X + Pad, TitleY, AccentColor, SizeBody, false, true).X;
	if (!TagText.IsEmpty())
	{
		TagPill(X + Pad + TW + 10.0f * S, TitleY);
	}

	for (int32 i = 0; i < Groups.Num(); ++i)
	{
		const float EX = X + Pad + (i % PerRow) * EntryW;
		const float EY = Y + Pad + 28.0f * S + (i / PerRow) * (EntryH + 6.0f * S);
		Entry(i, FBox2D(FVector2D(EX, EY), FVector2D(EX + EntryW - 6.0f * S, EY + EntryH)), IconW);
	}
}

void ATDHUD::DrawTowerPanel(ATDGameMode* GameMode, ATDPlayerController* PC, ATDTower* Tower)
{
	const float S = UIScale;
	const float PanelW = 420.0f * S;
	const float X = Canvas->ClipX - PanelW - 14.0f * S;
	const float Y = bFittingPanel ? FittedPanelTop : SideTop * S;
	const float Pad = 16.0f * S;
	const float Line = 27.0f * S;
	const float InnerW = PanelW - Pad * 2.0f;
	const float StatCardH = 50.0f * S;
	const float StatCardGap = 6.0f * S;

	const ETDTowerType Type = Tower->GetTowerType();
	const FTDTowerDef& Def = TD::Tower(Type);
	const FTDTowerStats& Stats = Tower->GetStats();
	const bool bAttack = Tower->CanProgress();	// towers with levels, items and skills
	const int32 Tab = bAttack ? PC->GetPanelTab() : 0;

	// Skills tab layout, measured here so the panel grows to fit long descriptions instead of letting them run out of their boxes
	const float SkillIcon = 50.0f * S;
	const float SkillTextW = InnerW - SkillIcon - 10.0f * S;
	const float SkillLineH = 19.0f * S;
	const float SkillBtnH = 30.0f * S;
	auto SkillRowH = [this, Type, SkillTextW, SkillLineH, SkillBtnH, S](int32 i)
	{
		const FTDSkillDef* Skill = TD::Skill(Type, i);
		if (!Skill)
		{
			return 0.0f;
		}
		// name, wrapped description, then the Learn button under it (one description line gives the old 84)
		const float DescH = WrappedText(Skill->Description, 0, 0, SkillTextW, DimText, SizeSmall, SkillLineH, false);
		return FMath::Max(84.0f * S, 26.0f * S + DescH + 5.0f * S + SkillBtnH + 4.0f * S);
	};

	// form section: heading, then one wide card (chosen) or two side by side, each sized to its wrapped text
	const FTDFormDef* ChosenForm = Tower->CanSpecialise() ? TD::Form(Type, Tower->GetForm()) : nullptr;
	const FString FormHeading = ChosenForm ? FString(TEXT("Form")) : FString::Printf(TEXT("Form (pick one at level %d, it's permanent)"), TD::FormLevel);
	const float FormHeadingLineH = 22.0f * S;
	const float FormCardGap = 8.0f * S;
	const float FormCardW = ChosenForm ? InnerW : (InnerW - FormCardGap) * 0.5f;
	const float FormTextW = FormCardW - (ChosenForm ? 24.0f : 20.0f) * S;
	const float FormLineH = (ChosenForm ? 20.0f : 16.0f) * S;
	const float FormTextSize = ChosenForm ? SizeSmall : SizeCaption;
	const float FormNameBase = ChosenForm ? SizeHeading : SizeBody;
	const float FormNameLineH = (ChosenForm ? 28.0f : 22.0f) * S;
	auto FormNameSize = [this, FormNameBase, FormTextW](const FString& Name)
	{
		// shrink a long name a little, but wrap it rather than go below body text
		return FMath::Max(SizeSmall, FitSize(Name, FormNameBase, FormTextW));
	};
	auto FormCardH = [&](const FTDFormDef* FormDef)
	{
		const FString Name = FormDef->Name;
		const float NameH = WrappedText(Name, 0, 0, FormTextW, FLinearColor::White, FormNameSize(Name), FormNameLineH, false);
		const float LinesH = WrappedText(FormDef->Line1, 0, 0, FormTextW, FLinearColor::White, FormTextSize, FormLineH, false)
			+ WrappedText(FormDef->Line2, 0, 0, FormTextW, FLinearColor::White, FormTextSize, FormLineH, false);
		return ChosenForm
			? FMath::Max(100.0f * S, 10.0f * S + NameH + 4.0f * S + LinesH + 10.0f * S)
			: FMath::Max(100.0f * S, 6.0f * S + NameH + 2.0f * S + LinesH + 6.0f * S + SkillBtnH + 8.0f * S);
	};
	auto FormSectionHeight = [&]()
	{
		if (!Tower->CanSpecialise())
		{
			return 0.0f;
		}
		float CardH = 0.0f;
		if (ChosenForm)
		{
			CardH = FormCardH(ChosenForm);
		}
		else
		{
			for (int32 FormIndex = 1; FormIndex <= TD::FormsPerTower; ++FormIndex)
			{
				if (const FTDFormDef* FormDef = TD::Form(Type, FormIndex))
				{
					CardH = FMath::Max(CardH, FormCardH(FormDef));
				}
			}
		}
		const float HeadingH = WrappedText(FormHeading, 0, 0, InnerW, PearlColor, SizeBody, FormHeadingLineH, false);
		return HeadingH + 6.0f * S + CardH + 8.0f * S;
	};

	// Stats tab text rows (special ability, on-hit effects, auras), colored by effect and wrapped to the panel
	auto Percent = [](float Fraction) { return FString::Printf(TEXT("+%d%%"), FMath::RoundToInt(Fraction * 100)); };
	const float StatLineH = 21.0f * S;
	// the info rows below the stat cards sit on their own darker card (the glass alone lets the bright floor wash them out),
	// with the text inset like the stat cards' text
	const float InfoInset = 12.0f * S;
	const float InfoW = InnerW - 2.0f * InfoInset;
	auto StatRowH = [this, InfoW, StatLineH, Line, S](const TArray<TPair<FString, FLinearColor>>& Parts)
	{
		return FMath::Max(Line, WrappedColoredText(Parts, 0, 0, InfoW, SizeField, StatLineH, false) + 6.0f * S);
	};
	TArray<TArray<TPair<FString, FLinearColor>>> StatRows;
	if (bAttack && Tab == 0)
	{
		if (Tower->IsSupportTower())
		{
			StatRows.Add(ColorizeEffects(Tower->GetSpecialText(), FLinearColor::White));
			StatRows.Add(ColorizeEffects(Stats.AuraXpTrickle > 0.0f ? FString::Printf(TEXT("Towers in the aura gain +%.1f XP per second"), Stats.AuraXpTrickle) : FString(TEXT("Strongest aura of each kind applies")), InfoText));
		}
		else
		{
			StatRows.Add(ColorizeEffects(Tower->GetSpecialText(), InfoText));

			// lead in, then each piece in its stat color, or a dim line when there are none
			auto Listed = [](const TCHAR* Lead, const TArray<TPair<FString, FLinearColor>>& Pieces, const TCHAR* NoneText)
			{
				TArray<TPair<FString, FLinearColor>> Parts;
				if (Pieces.Num() == 0)
				{
					Parts.Add({ NoneText, DimText });
					return Parts;
				}
				Parts.Add({ Lead, InfoText });
				for (int32 i = 0; i < Pieces.Num(); ++i)
				{
					Parts.Add({ i + 1 < Pieces.Num() ? Pieces[i].Key + TEXT(", ") : Pieces[i].Key, Pieces[i].Value });
				}
				return Parts;
			};
			TArray<TPair<FString, FLinearColor>> Effects;
			if (Stats.PoisonPerTick > 0.0f && Type != ETDTowerType::Lionfish) Effects.Add({ FString::Printf(TEXT("poison %.1f"), Stats.PoisonPerTick), StatDot });
			if (Stats.OnHitSlow > 0.0f) Effects.Add({ FString::Printf(TEXT("slow %d%%"), FMath::RoundToInt(Stats.OnHitSlow * 100)), StatControl });
			if (Stats.ExtraSplash > 0.0f) Effects.Add({ FString::Printf(TEXT("splash %.2f tiles"), Stats.ExtraSplash), StatArea });
			if (Stats.ExtraChain > 0) Effects.Add({ FString::Printf(TEXT("chain +%d targets"), Stats.ExtraChain), StatArea });
			if (Stats.StunChance > 0.0f) Effects.Add({ FString::Printf(TEXT("stun %d%%"), FMath::RoundToInt(Stats.StunChance * 100)), StatControl });
			StatRows.Add(Listed(TEXT("On hit: "), Effects, TEXT("No extra on-hit effects")));

			TArray<TPair<FString, FLinearColor>> Auras;
			if (Tower->GetAuraRateUp() > 0.0f) Auras.Add({ FString::Printf(TEXT("+%d%% speed"), FMath::RoundToInt(Tower->GetAuraRateUp() * 100)), StatRate });
			if (Tower->GetAuraDamageUp() > 0.0f) Auras.Add({ FString::Printf(TEXT("+%d%% damage"), FMath::RoundToInt(Tower->GetAuraDamageUp() * 100)), StatDamage });
			if (Tower->GetAuraDamageDown() < 0.0f) Auras.Add({ FString::Printf(TEXT("%d%% damage (Shock Eel)"), FMath::RoundToInt(Tower->GetAuraDamageDown() * 100)), StatLoss });
			StatRows.Add(Listed(TEXT("Auras: "), Auras, TEXT("No auras affecting this tower")));
		}
	}
	float StatRowsH = 0.0f;
	for (const TArray<TPair<FString, FLinearColor>>& Parts : StatRows)
	{
		StatRowsH += StatRowH(Parts);
	}

	// content height per tab
	float ContentH = 2.0f * Line;
	if (bAttack)
	{
		// attack towers: three rows of stat cards, then three field rows and the text rows; support towers: two field rows and two text rows
		const float StatsH = Tower->IsSupportTower() ? 3.0f * Line + StatRowsH : 3.0f * (StatCardH + StatCardGap) + 3.0f * Line + StatRowsH + 12.0f * S;
		float SkillsH = 0.0f;
		if (Tab == 2)
		{
			for (int32 i = 0; i < TD::SkillsPerTower; ++i)
			{
				SkillsH += SkillRowH(i);
			}
			SkillsH += FormSectionHeight();
		}
		ContentH = (Tab == 0) ? StatsH : (Tab == 1 ? 3.0f * 62.0f * S + Line : SkillsH);
	}
	const float TitleH = HeaderIcon * S + 6.0f * S;
	const float HeaderH = bAttack ? TitleH + 18.0f * S + 8.0f * S + 38.0f * S + 12.0f * S : TitleH;
	// the targeting priority sits above Sell for towers that aim at one creature
	const bool bTargeting = Tower->CanChooseTargeting() || (Tower->IsAttackTower() && Tower->GetTowerType() == ETDTowerType::Puffer && Tower->GetForm() == 2);
	const float TargetRowH = 46.0f * S;
	const float FooterH = 56.0f * S + (bTargeting ? TargetRowH : 0.0f);
	const FBox2D Rect(FVector2D(X, Y), FVector2D(X + PanelW, Y + Pad * 2.0f + HeaderH + ContentH + FooterH));

	// a large interface size can make the panel reach down over the dock: draw it once more at a scale that fits above it
	const float FitBottom = DockTop() - 10.0f * S;
	if (!bFittingPanel && Rect.Max.Y > FitBottom)
	{
		TGuardValue<bool> Fitting(bFittingPanel, true);
		// keep the top level with the other side panels and shrink only what hangs below it
		TGuardValue<float> Top(FittedPanelTop, Y);
		TGuardValue<float> Scale(UIScale, UIScale * FMath::Max(0.6f, (FitBottom - Y) / (float)(Rect.Max.Y - Y) * 0.98f));
		DrawTowerPanel(GameMode, PC, Tower);
		return;
	}

	FramedPanel(Rect, PanelEdge);

	float CY = Y + Pad;

	// header: name, level, XP bar
	PanelHeader(TDIcons::Tower(Type), X + Pad, CY, InnerW, Def.Name, AccentColor,
		bAttack ? FString::Printf(TEXT("Level %d of %d"), Tower->GetLevel(), TD::MaxTowerLevel) : FString(), DimText);
	CY += TitleH;

	if (bAttack)
	{
		const float BarH = 18.0f * S;
		const float Fraction = Tower->IsMaxLevel() ? 1.0f : FMath::Clamp(Tower->GetXp() / FMath::Max(1, Tower->GetXpToNext()), 0.0f, 1.0f);
		const FString XpText = Tower->IsMaxLevel() ? FString(TEXT("MAX LEVEL"))
			: FString::Printf(TEXT("XP %d / %d"), FMath::FloorToInt(Tower->GetXp()), Tower->GetXpToNext());
		Meter(FBox2D(FVector2D(X + Pad, CY), FVector2D(X + Pad + InnerW, CY + BarH)), Fraction, Tower->IsMaxLevel() ? WithAlpha(PearlColor * 0.6f, 0.95f) : SRGB(24, 150, 172, 0.95f), XpText);
		CY += BarH + 8.0f * S;

		// tabs; unspent skill points show on the Skills tab
		const float TabW = (InnerW - 2.0f * 6.0f * S) / 3.0f;
		const float TabH = 38.0f * S;
		const int32 SkillPoints = Tower->GetSkillPoints();
		// segmented control: one dark track, the active tab lit inside it
		RoundFill(FBox2D(FVector2D(X + Pad, CY), FVector2D(X + Pad + InnerW, CY + TabH)).ExpandBy(3.0f * S), InsetColor, (RadiusButton + 3.0f) * S);
		for (int32 i = 0; i < 3; ++i)
		{
			const FBox2D TabRect(FVector2D(X + Pad + i * (TabW + 6.0f * S), CY), FVector2D(X + Pad + i * (TabW + 6.0f * S) + TabW, CY + TabH));
			FString TabLabel = TabNames[i];
			if (i == 1)
			{
				TabLabel = FString::Printf(TEXT("%s [%s]"), TabNames[i], *GameMode->GetKeyName(ETDBind::Items));
			}
			else if (i == 2)
			{
				TabLabel = SkillPoints > 0 ? FString::Printf(TEXT("  %s (%d) [%s]"), TabNames[i], SkillPoints, *GameMode->GetKeyName(ETDBind::Skills))
					: FString::Printf(TEXT("%s [%s]"), TabNames[i], *GameMode->GetKeyName(ETDBind::Skills));
			}
			Button(TabRect, TabLabel, EButton::Tab, i, i == Tab ? ActiveButton : WithAlpha(ButtonColor, 0.3f), true, i == Tab, SizeBody);
			if (i == 2 && SkillPoints > 0)
			{
				const float StarSize = 16.0f * S;
				const FVector2D StarMin(TabRect.Min.X + 4.0f * S, TabRect.GetCenter().Y - StarSize * 0.5f);
				Icon(TDIcons::Misc(ETDMiscIcon::SkillPoint), FBox2D(StarMin, StarMin + FVector2D(StarSize, StarSize)));
			}
		}
		// a thin, slowly drifting wave under the tab row instead of a straight rule
		WaveStrip(X + Pad + RadiusButton * S, X + Pad + InnerW - RadiusButton * S, CY + TabH + 4.0f * S, 5.0f * S, false, WithAlpha(AccentColor, 0.22f), (float)FMath::Fmod(Now() * 0.02, 1.0));
		CY += TabH + 12.0f * S;
	}

	// tab content
	if (!bAttack)
	{
		const FString Description = Def.Description;
		Text(Description, X + Pad, CY, DimText, FitSize(Description, SizeField, InnerW));
		CY += Line;
		Text(TEXT("Walls don't level up, hold items or learn skills"), X + Pad, CY, DimText, SizeSmall);
		CY += Line;
	}
	else if (Tab == 0)
	{
		// a text row that wraps instead of running past the panel (one line takes the usual Line)
		const float IX = X + Pad + InfoInset;
		auto Row = [this, &CY, &StatRowH, IX, InfoW, StatLineH](const TArray<TPair<FString, FLinearColor>>& Parts)
		{
			WrappedColoredText(Parts, IX, CY, InfoW, SizeField, StatLineH);
			CY += StatRowH(Parts);
		};
		// the darker card behind the info rows, from the current line down by Height
		auto InfoCard = [this, X, Pad, InnerW, S](float Top, float Height)
		{
			const FBox2D Card(FVector2D(X + Pad, Top), FVector2D(X + Pad + InnerW, Top + Height));
			RoundFill(Card, InfoCardColor, RadiusButton * S);
			RoundFrame(Card, WithAlpha(PanelEdge, 0.1f), RadiusButton * S, 1.0f);
		};

		if (Tower->IsSupportTower())
		{
			InfoCard(CY, 2.0f * Line + StatRowsH + 6.0f * S);
			CY += 6.0f * S;
			Row(StatRows[0]);
			FieldRow(IX, CY, InfoW, Line, TEXT("Aura range"), FString::Printf(TEXT("%.2f tiles"), Stats.Range), TEXT("Bonus XP"), Percent(Stats.XpMult - 1.0f), StatRange, StatXp);
			FieldRow(IX, CY, InfoW, Line, TEXT("XP in waves"), FString::Printf(TEXT("%.1f/s"), TD::SupportXpRate(Type) + Stats.XpTrickle), TEXT("Pearls"), FString::Printf(TEXT("%.2f/s"), Stats.GoldTrickle), StatXp, StatPearls);
			Row(StatRows[1]);
		}
		else
		{
			// headline stats as small cards: caption on top, big value below, each in its stat color with an accent bar
			{
				const FLinearColor CardColors[6] = { StatDamage, StatRange, StatRate, StatCrit, DimText, StatPierce };
				const FString Cards[6][2] = {
					{ TEXT("DAMAGE PER HIT"), FString::Printf(TEXT("%s (%s)"), *DamageRangeText(Tower->GetDamage(), TD::TowerDamageSpread(Type)), TD::DamageTypeName(Def.DamageType)) },
					{ TEXT("RANGE"), Tower->GetAuraRangeUp() > 0.0f ? FString::Printf(TEXT("%.2f tiles (+%.1f sonar)"), Tower->GetRange(), Tower->GetAuraRangeUp()) : FString::Printf(TEXT("%.2f tiles"), Tower->GetRange()) },
					{ TEXT("ATTACK SPEED"), FString::Printf(TEXT("%.2f/s"), Tower->GetRate()) },
					{ TEXT("CRITICAL HIT"), Stats.CritMultiplier > 2.0f ? FString::Printf(TEXT("%d%% (x%.1f)"), FMath::RoundToInt(Stats.Crit * 100), Stats.CritMultiplier) : FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Stats.Crit * 100)) },
					{ TEXT("PROJECTILE"), Stats.ProjectileSpeed > 0.0f ? FString::Printf(TEXT("%.1f tiles/s"), Stats.ProjectileSpeed) : FString(TEXT("Instant")) },
					{ TEXT("ARMOR PIERCE"), FString::Printf(TEXT("%.0f"), Stats.ArmorPierce) } };
				const float CardW = (InnerW - StatCardGap) * 0.5f;
				for (int32 i = 0; i < 6; ++i)
				{
					const float CX = X + Pad + (i % 2) * (CardW + StatCardGap);
					const float CardY = CY + (i / 2) * (StatCardH + StatCardGap);
					const FBox2D Card(FVector2D(CX, CardY), FVector2D(CX + CardW, CardY + StatCardH));
					const FLinearColor& StatColor = CardColors[i];
					RoundFill(Card, InsetColor, RadiusButton * S);
					RoundFill(Card, WithAlpha(StatColor, 0.05f), RadiusButton * S);
					RoundFrame(Card, WithAlpha(StatColor, 0.16f), RadiusButton * S, 1.0f);
					RoundFill(Snap(FBox2D(FVector2D(CX + 4.0f * S, CardY + 9.0f * S), FVector2D(CX + 7.0f * S, CardY + StatCardH - 9.0f * S))), WithAlpha(StatColor, 0.85f), 1.5f * S);
					Text(Cards[i][0], CX + 14.0f * S, CardY + 14.0f * S, FMath::Lerp(DimText, StatColor, 0.55f), SizeCaption, false, true);
					Text(Cards[i][1], CX + 14.0f * S, CardY + 34.0f * S, i == 4 ? FLinearColor::White : StatColor, FitSize(Cards[i][1], 19.0f, CardW - 24.0f * S), false, true);
					if (i == 0 && Card.IsInside(MousePos))
					{
						const float Spread = TD::TowerDamageSpread(Type);
						SetTip({ { TEXT("Damage per hit"), StatDamage },
							{ FString::Printf(TEXT("Each hit rolls between %s, %s on average (%s%d%%)"), *DamageRangeText(Tower->GetDamage(), Spread), *ShortNumber(Tower->GetDamage(), 1), TEXT("+/-"), FMath::RoundToInt(Spread * 100)), FLinearColor::White },
							{ TEXT("Critical hits multiply the rolled damage"), DimText } });
					}
				}
				CY += 3.0f * (StatCardH + StatCardGap);
			}
			InfoCard(CY, 3.0f * Line + StatRowsH + 6.0f * S);
			CY += 6.0f * S;
			FieldRow(IX, CY, InfoW, Line, TEXT("Damage dealt"), ShortNumber(Tower->GetDamageDone()), TEXT("Kills"), FString::Printf(TEXT("%d"), Tower->GetKills()), StatDamage);
			FieldRow(IX, CY, InfoW, Line, TEXT("Bonus XP"), Percent(Stats.XpMult - 1.0f), TEXT("Bonus pearls"), Percent(Stats.BountyPct), StatXp, StatPearls);
			FieldRow(IX, CY, InfoW, Line, TEXT("Item chance"), Percent(Stats.ItemChance), TEXT("Targets"), Tower->HitsFlying() ? TEXT("All creatures") : TEXT("Crawling only"), StatPearls);
			// special ability, on-hit effects and auras: each effect in its stat color
			for (const TArray<TPair<FString, FLinearColor>>& Parts : StatRows)
			{
				Row(Parts);
			}
		}
	}
	else if (Tab == 1)
	{
		const float TileSize = 50.0f * S;
		for (int32 SlotIndex = 0; SlotIndex < (int32)ETDItemSlot::Count; ++SlotIndex)
		{
			const ETDItemSlot Slot = (ETDItemSlot)SlotIndex;
			const FTDItem& Item = Tower->GetEquipped(Slot);
			const FBox2D TileRect(FVector2D(X + Pad, CY), FVector2D(X + Pad + TileSize, CY + TileSize));
			const float TX = X + Pad + TileSize + 10.0f * S;
			const float TextW = X + PanelW - Pad - TX;
			if (GameMode->GetRunRules().IsItemSlotLocked(Slot) && !Item.IsValid())
			{
				// Lean Reef (mutator): a greyed, locked slot
				RoundFill(TileRect, WithAlpha(InsetColor, 0.35f), RadiusTile * S);
				RoundFrame(TileRect, WithAlpha(DisabledText, 0.4f), RadiusTile * S, 1.0f);
				Padlock(TileRect.ExpandBy(-14.0f * S), DisabledText);
				const FString LockedText = FString::Printf(TEXT("%s slot locked (Lean Reef)"), TD::SlotName(Slot));
				Text(LockedText, TX, CY + TileSize * 0.5f, DisabledText, FitSize(LockedText, SizeField, TextW), false, true);
				CY += 62.0f * S;
				continue;
			}
			ItemTile(TileRect, &Item, EButton::EquippedItem, SlotIndex);

			if (Item.IsValid())
			{
				Text(Item.Name, TX, CY + 3.0f * S, TD::RarityColor(Item.Rarity) * 1.15f, FitSize(Item.Name, SizeBody, TextW));
				const FString ItemLine = FString::Printf(TEXT("%s %s  (click to unequip)"), TD::RarityName(Item.Rarity), TD::SlotName(Slot));
				Text(ItemLine, TX, CY + 27.0f * S, DimText, FitSize(ItemLine, SizeSmall, TextW));
			}
			else
			{
				Text(FString::Printf(TEXT("Empty %s slot"), TD::SlotName(Slot)), TX, CY + TileSize * 0.5f, DimText, SizeField, false, true);
			}
			CY += 62.0f * S;
		}
		const FString EquipHint = PC->IsStorageOpen() ? FString(TEXT("Double-click an item in storage to equip it")) : FString::Printf(TEXT("Open storage [%s] and double-click an item to equip it"), *GameMode->GetKeyName(ETDBind::Storage));
		Text(EquipHint, X + Pad, CY, InfoText, FitSize(EquipHint, SizeSmall, InnerW));
		CY += Line;
	}
	else
	{
		const float BtnW = 170.0f * S;
		const float BtnH = SkillBtnH;
		for (int32 i = 0; i < TD::SkillsPerTower; ++i)
		{
			const FTDSkillDef* Skill = TD::Skill(Type, i);
			if (!Skill)
			{
				continue;
			}
			const int32 Rank = Tower->GetSkillRank(i);
			const float RowH = SkillRowH(i);

			// icon on a small plate, gold rimmed once learned
			const FBox2D Plate(FVector2D(X + Pad, CY + 2.0f * S), FVector2D(X + Pad + SkillIcon, CY + 2.0f * S + SkillIcon));
			// the rim grows with the rank: a dim edge until learned, then an accent rim that brightens and thickens,
			// and a pearl-gold rim with a soft glow at the top rank
			const bool bMaxRank = Rank >= TD::MaxSkillRank;
			const float RankF = TD::MaxSkillRank > 1 ? FMath::Clamp((Rank - 1) / (float)(TD::MaxSkillRank - 1), 0.0f, 1.0f) : 1.0f;
			FLinearColor Rim = WithAlpha(ButtonEdge, 0.18f);
			float RimW = 1.0f;
			if (bMaxRank)
			{
				Rim = PearlColor;
				RimW = 2.0f;
				Glow(Plate, WithAlpha(PearlColor, 0.3f), RadiusButton * S, 7.0f * S);
			}
			else if (Rank > 0)
			{
				Rim = WithAlpha(FMath::Lerp(AccentColor * 0.8f, AccentColor, RankF), FMath::Lerp(0.6f, 0.95f, RankF));
				RimW = FMath::Lerp(1.0f, 1.8f, RankF);
				Glow(Plate, WithAlpha(AccentColor, 0.06f + 0.12f * RankF), RadiusButton * S, 5.0f * S);
			}
			RoundFill(Plate, InsetColor, RadiusButton * S);
			RoundFrame(Plate, Rim, RadiusButton * S, RimW * S);
			Icon(TDIcons::Skill(Type, i), Plate.ExpandBy(-3.0f * S), Rank > 0 ? FLinearColor::White : FLinearColor(0.75f, 0.78f, 0.8f, 0.9f));
			// one small pip per rank under the plate, lit for each rank learned
			{
				const float Pip = FMath::Max(3.0f, FMath::RoundToFloat(4.0f * S));
				const float PipGap = FMath::Max(2.0f, FMath::RoundToFloat(2.5f * S));
				const float RowW = TD::MaxSkillRank * Pip + (TD::MaxSkillRank - 1) * PipGap;
				float PX = FMath::RoundToFloat(Plate.GetCenter().X - RowW * 0.5f);
				const float PY = FMath::RoundToFloat(Plate.Max.Y + 4.0f * S);
				for (int32 r = 0; r < TD::MaxSkillRank; ++r)
				{
					const FBox2D PipRect(FVector2D(PX, PY), FVector2D(PX + Pip, PY + Pip));
					const bool bLit = r < Rank;
					RoundFill(PipRect, bLit ? (bMaxRank ? PearlColor : AccentColor) : WithAlpha(ButtonEdge, 0.25f), Pip * 0.5f);
					PX += Pip + PipGap;
				}
			}

			const float TX = Plate.Max.X + 10.0f * S;
			const float TextW = X + PanelW - Pad - TX;
			const FString RankText = FString::Printf(TEXT("Rank %d of %d"), Rank, TD::MaxSkillRank);
			const float RankW = MeasureText(RankText, SizeSmall).X;
			const FString SkillName = Skill->Name;
			// name and rank share one centre line, so the smaller rank text sits level with the name
			const float TitleMidY = CY + 11.0f * S;
			Text(SkillName, TX, TitleMidY, Rank > 0 ? PearlColor : FLinearColor::White, FitSize(SkillName, SizeBody, TextW - RankW - 10.0f * S), false, true);
			Text(RankText, X + PanelW - Pad - RankW, TitleMidY, Rank > 0 ? PearlColor : DimText, SizeSmall, false, true);
			// description wrapped under the name, effect words in their stat colors
			const float DescH = WrappedColoredText(ColorizeEffects(Skill->Description, DimText), TX, CY + 26.0f * S, TextW, SizeSmall, SkillLineH);

			FString Reason;
			const bool bCanLearn = Tower->CanLearnSkill(i, &Reason);
			FString Label;
			bool bEnabled = false;
			if (bCanLearn)
			{
				const int32 Cost = TD::SkillCost(Type, i, Rank + 1);
				bEnabled = GameMode->GetPearls() >= Cost;
				Label = FString::Printf(TEXT("Learn (%d pearls)"), Cost);
			}
			else
			{
				Label = Reason.Replace(TEXT("Requires level"), TEXT("Needs level"));
			}
			const float BtnY = CY + FMath::Max(50.0f * S, 26.0f * S + DescH + 5.0f * S);
			const FBox2D BtnRect(FVector2D(X + PanelW - Pad - BtnW, BtnY), FVector2D(X + PanelW - Pad, BtnY + BtnH));
			Button(BtnRect, Label, EButton::LearnSkill, i, GreenButton, bEnabled, false, SizeSmall);
			CY += RowH;
		}

		// specialisation: two forms side by side until one is picked, then the chosen one
		if (Tower->CanSpecialise())
		{
			// heading, wrapped if a large interface size makes it too long for one line
			CY += WrappedText(FormHeading, X + Pad, CY, InnerW, PearlColor, SizeBody, FormHeadingLineH) + 6.0f * S;

			float CardH = 0.0f;
			if (ChosenForm)
			{
				CardH = FormCardH(ChosenForm);
			}
			else
			{
				for (int32 FormIndex = 1; FormIndex <= TD::FormsPerTower; ++FormIndex)
				{
					if (const FTDFormDef* FormDef = TD::Form(Type, FormIndex))
					{
						CardH = FMath::Max(CardH, FormCardH(FormDef));
					}
				}
			}

			if (ChosenForm)
			{
				const FBox2D Card(FVector2D(X + Pad, CY), FVector2D(X + Pad + InnerW, CY + CardH));
				const FLinearColor FormColor = TD::FormColor(Tower->GetForm());
				Glow(Card, WithAlpha(FormColor, 0.18f), RadiusButton * S, 6.0f * S);
				RoundFill(Card, InsetColor, RadiusButton * S);
				RoundFrame(Card, FormColor, RadiusButton * S, 1.5f * S);
				const float TX = Card.Min.X + 12.0f * S;
				const FString Name = ChosenForm->Name;
				float TY = CY + 10.0f * S;
				TY += WrappedText(Name, TX, TY, FormTextW, FormColor, FormNameSize(Name), FormNameLineH) + 4.0f * S;
				TY += WrappedColoredText(ColorizeEffects(ChosenForm->Line1, FLinearColor::White), TX, TY, FormTextW, FormTextSize, FormLineH);
				WrappedColoredText(ColorizeEffects(ChosenForm->Line2, DimText), TX, TY, FormTextW, FormTextSize, FormLineH);
			}
			else
			{
				FString Reason;
				const bool bCanChoose = Tower->CanChooseForm(&Reason);
				const int32 Armed = PC->GetArmedForm();
				for (int32 FormIndex = 1; FormIndex <= TD::FormsPerTower; ++FormIndex)
				{
					const FTDFormDef* FormDef = TD::Form(Type, FormIndex);
					if (!FormDef)
					{
						continue;
					}
					const float CX0 = X + Pad + (FormIndex - 1) * (FormCardW + FormCardGap);
					const FBox2D Card(FVector2D(CX0, CY), FVector2D(CX0 + FormCardW, CY + CardH));
					const bool bArmed = Armed == FormIndex;
					RoundFill(Card, InsetColor, RadiusButton * S);
					const FLinearColor FormColor = TD::FormColor(FormIndex);
					RoundFrame(Card, bArmed ? FormColor : WithAlpha(FormColor, 0.35f), RadiusButton * S, (bArmed ? 1.5f : 1.0f) * S);
					const float TX = Card.Min.X + 10.0f * S;
					const FString Name = FormDef->Name;
					float TY = CY + 6.0f * S;
					TY += WrappedText(Name, TX, TY, FormTextW, bCanChoose ? FormColor : DimText, FormNameSize(Name), FormNameLineH) + 2.0f * S;
					TY += WrappedColoredText(ColorizeEffects(FormDef->Line1, FLinearColor::White), TX, TY, FormTextW, FormTextSize, FormLineH);
					WrappedColoredText(ColorizeEffects(FormDef->Line2, DimText), TX, TY, FormTextW, FormTextSize, FormLineH);

					// the button label must stay readable: a long "Become ..." falls back to a short label
					const FBox2D BtnRect(FVector2D(TX, Card.Max.Y - 8.0f * S - BtnH), FVector2D(Card.Max.X - 10.0f * S, Card.Max.Y - 8.0f * S));
					FString Label = !bCanChoose ? Reason : (bArmed ? FString(TEXT("Click again to confirm")) : FString::Printf(TEXT("Become %s"), FormDef->Name));
					const float LabelRoom = (float)BtnRect.GetSize().X - 14.0f * S;
					if (FitSize(Label, SizeSmall, LabelRoom) < SizeCaption)
					{
						Label = !bCanChoose ? FString(TEXT("Not yet")) : (bArmed ? FString(TEXT("Confirm")) : FString(TEXT("Choose")));
					}
					Button(BtnRect, Label, EButton::ChooseForm, FormIndex, bArmed ? ActiveButton : GreenButton, bCanChoose, bArmed, SizeSmall);
					if (Card.IsInside(MousePos))
					{
						SetTip({ { FormDef->Name, PearlColor }, { FormDef->Line1, FLinearColor::White }, { FormDef->Line2, DimText },
							{ TEXT("Free, but permanent: the tower keeps this form until it's sold"), InfoText } }, true);
					}
				}
			}
			CY += CardH + 8.0f * S;
		}
	}

	// footer: targeting priority ([<] name [>] and its key), then sell
	if (bTargeting)
	{
		const float RowY = Rect.Max.Y - Pad - 40.0f * S - TargetRowH;
		const bool bChoose = Tower->CanChooseTargeting();
		const ETDTargeting Mode = Tower->GetEffectiveTargeting();
		Text(TEXT("Target"), X + Pad, RowY + 18.0f * S, DimText, SizeField, false, true);
		const float LabelW = 74.0f * S;
		const float ArrowW = 34.0f * S;
		const FString KeyName = GameMode->GetKeyName(ETDBind::Targeting);
		const float KeyW = bChoose ? MeasureText(KeyName, SizeCaption).X + 24.0f * S : 0.0f;
		const float SelX = X + Pad + LabelW;
		const float SelW = PanelW - Pad * 2.0f - LabelW - KeyW;
		const FBox2D Prev(FVector2D(SelX, RowY), FVector2D(SelX + ArrowW, RowY + 36.0f * S));
		const FBox2D Next(FVector2D(SelX + SelW - ArrowW, RowY), FVector2D(SelX + SelW, RowY + 36.0f * S));
		const FBox2D Middle(FVector2D(Prev.Max.X + 6.0f * S, RowY), FVector2D(Next.Min.X - 6.0f * S, RowY + 36.0f * S));
		Button(Prev, TEXT("<"), EButton::TargetCycle, -1, ButtonColor, bChoose, false, SizeBody);
		Button(Next, TEXT(">"), EButton::TargetCycle, 1, ButtonColor, bChoose, false, SizeBody);
		RoundFill(Middle, InsetColor, RadiusButton * S);
		const FString ModeText = bChoose ? FString(TD::TargetingName(Mode)) : FString(TEXT("Toughest (Deep Lobber)"));
		Text(ModeText, Middle.GetCenter().X, Middle.GetCenter().Y, bChoose ? AccentColor : DimText, FitSize(ModeText, SizeBody, Middle.GetSize().X - 12.0f * S), true, true);
		if (bChoose)
		{
			KeyCap(KeyName, SelX + SelW + 10.0f * S, Middle.GetCenter().Y);
		}
		if (FBox2D(FVector2D(X + Pad, RowY), FVector2D(X + PanelW - Pad, RowY + 36.0f * S)).IsInside(MousePos))
		{
			TArray<TPair<FString, FLinearColor>> Tip = { { TEXT("Targeting priority"), AccentColor } };
			if (!bChoose)
			{
				Tip.Add({ TEXT("The Deep Lobber always lobs at the toughest creature in range."), FLinearColor::White });
			}
			else
			{
				for (int32 m = 0; m < TD::TargetingCount; ++m)
				{
					const ETDTargeting Each = (ETDTargeting)m;
					Tip.Add({ FString::Printf(TEXT("%s: %s"), TD::TargetingName(Each), TD::TargetingDescription(Each)), Each == Mode ? AccentColor : DimText });
				}
				Tip.Add({ FString::Printf(TEXT("Click the arrows or press %s. New %s towers start with the last choice."), *KeyName, Tower->GetDef().Name), FLinearColor::White });
			}
			SetTip(Tip);
		}
	}

	// footer: sell
	// the first press arms the button itself (no popup): it lights up and asks to be pressed again, for 3 seconds
	CY = Rect.Max.Y - Pad - 40.0f * S;
	const FBox2D SellRect(FVector2D(X + Pad, CY), FVector2D(X + PanelW - Pad, CY + 40.0f * S));
	const bool bArmed = PC->GetPendingSell() == Tower;
	if (bArmed && PC->IsSellArmedByClick() && !SellRect.IsInside(MousePos))
	{
		PC->CancelSell();	// armed by a click, then the mouse moved away
	}
	const FString SellKey = GameMode->GetKeyName(ETDBind::Sell);
	const FString SellLabel = PC->GetPendingSell() == Tower
		? FString::Printf(TEXT("Confirm sell? +%d pearls [%s]"), GameMode->GetSellValue(Tower), *SellKey)
		: FString::Printf(TEXT("Sell [%s] for %d pearls%s"), *SellKey, GameMode->GetSellValue(Tower), Tower->GetEquippedCount() > 0 ? TEXT(" (items go to storage)") : TEXT(""));
	const bool bShowArmed = PC->GetPendingSell() == Tower;
	FString SellBlocked;
	if (!GameMode->CanSellTower(Tower, &SellBlocked))
	{
		// Fixed Towers (mutator): no selling after the tower's own build phase
		Button(SellRect, TEXT("Can't sell (Fixed Towers)"), EButton::Sell, 0, DangerButton, false, false, SizeField);
		if (SellRect.IsInside(MousePos))
		{
			SetTip({ { TEXT("Fixed Towers"), LifeColor }, { TEXT("Towers and walls can only be sold in the build phase they were bought in."), FLinearColor::White } });
		}
		return;
	}
	Button(SellRect, SellLabel, EButton::Sell, 0, bShowArmed ? SRGB(156, 34, 52, 0.96f) : DangerButton, true, bShowArmed, SizeField);
	if (bShowArmed)
	{
		// a coral rim over the accent highlight, so it reads as a warning
		RoundFrame(SellRect, LifeColor, RadiusButton * S, 1.5f * S);
	}
}

void ATDHUD::DrawStorage(ATDGameMode* GameMode, ATDPlayerController* PC)
{
	const float S = UIScale;
	const float Tile = 48.0f * S;
	const float Gap = 6.0f * S;
	const float Pad = 16.0f * S;
	const int32 RowCount = FMath::DivideAndRoundUp(TD::StorageSlots, StorageColumns);
	const float GridW = StorageColumns * Tile + (StorageColumns - 1) * Gap;
	const float GridH = RowCount * Tile + (RowCount - 1) * Gap;
	const float HeaderH = 52.0f * S;
	const float FooterH = 176.0f * S;	// the two hint lines and three button rows, so the bottom padding matches the sides

	const float X = 14.0f * S;
	const float Y = SideTop * S;
	const FBox2D Rect(FVector2D(X, Y), FVector2D(X + GridW + Pad * 2.0f, Y + Pad * 2.0f + HeaderH + GridH + FooterH));
	FramedPanel(Rect, PanelEdge);
	StorageRect = Rect;

	const TArray<FTDItem>& Storage = GameMode->GetStorage();
	// header: title, count and a thin capacity bar
	const float TitleW = Text(TEXT("Storage"), X + Pad, Y + Pad + 16.0f * S, AccentColor, SizeHeading, false, true).X;
	const int32 Capacity = GameMode->GetStorageCapacity();	// fewer slots with Hungry Deep (cursed boon)
	const float CountW = Text(FString::Printf(TEXT("%d / %d"), Storage.Num(), Capacity), X + Pad + TitleW + 10.0f * S, Y + Pad + 17.0f * S, Storage.Num() > Capacity ? LifeColor : DimText, SizeSmall, false, true).X;
	// unique legendaries found this run
	{
		const FString UniqueText = FString::Printf(TEXT("Uniques %d/%d"), TD::CountUniques(GameMode->GetUniquesFound()), TD::UniqueCount);
		Text(UniqueText, X + Pad + TitleW + 10.0f * S + CountW + 14.0f * S, Y + Pad + 17.0f * S, TD::CountUniques(GameMode->GetUniquesFound()) > 0 ? UniqueColor : DimText, SizeSmall, false, true);
	}
	{
		const FBox2D Track(FVector2D(X + Pad, Y + Pad + 40.0f * S), FVector2D(X + Pad + GridW, Y + Pad + 44.0f * S));
		const float Used = FMath::Clamp((float)Storage.Num() / FMath::Max(1, Capacity), 0.0f, 1.0f);
		RoundFill(Track, TrackColor, 2.0f * S);
		if (Used > 0.0f)
		{
			RoundFill(FBox2D(Track.Min, FVector2D(Track.Min.X + FMath::Max(4.0f * S, Track.GetSize().X * Used), Track.Max.Y)), Used >= 0.9f ? LifeColor : AccentColor, 2.0f * S);
		}
	}

	const float SortW = 90.0f * S;
	Button(FBox2D(FVector2D(Rect.Max.X - Pad - SortW, Y + Pad), FVector2D(Rect.Max.X - Pad, Y + Pad + 32.0f * S)), TEXT("Sort"), EButton::Sort, 0, ButtonColor, Storage.Num() > 1, false, SizeField);

	const float GX = X + Pad;
	const float GY = Y + Pad + HeaderH;
	for (int32 i = 0; i < TD::StorageSlots; ++i)
	{
		const int32 Col = i % StorageColumns;
		const int32 Row = i / StorageColumns;
		const FVector2D Min(GX + Col * (Tile + Gap), GY + Row * (Tile + Gap));
		if (i >= Capacity && !Storage.IsValidIndex(i))
		{
			// a slot Hungry Deep took away: dimmed and crossed out
			const FBox2D Lost(Min, Min + FVector2D(Tile, Tile));
			RoundFill(Lost, WithAlpha(LifeColor * 0.12f, 0.5f), RadiusTile * S);
			Text(TEXT("x"), Lost.GetCenter().X, Lost.GetCenter().Y, WithAlpha(LifeColor, 0.35f), SizeBody, true, true);
			continue;
		}
		ItemTile(FBox2D(Min, Min + FVector2D(Tile, Tile)), Storage.IsValidIndex(i) ? &Storage[i] : nullptr, EButton::StorageItem, i);
	}

	// workbench item gets a bright double frame
	const int32 Bench = PC->GetWorkbenchIndex();
	if (Bench >= 0)
	{
		const FVector2D Min(GX + (Bench % StorageColumns) * (Tile + Gap), GY + (Bench / StorageColumns) * (Tile + Gap));
		const FBox2D Picked = FBox2D(Min, Min + FVector2D(Tile, Tile)).ExpandBy(3.0f * S);
		Glow(Picked, WithAlpha(AccentColor, 0.35f), (RadiusTile + 3.0f) * S, 6.0f * S);
		RoundFrame(Picked, AccentColor, (RadiusTile + 3.0f) * S, 2.0f * S);
	}

	const bool bCanEquip = PC->GetSelectedTower() && PC->GetSelectedTower()->CanProgress();
	const FString Hint = bCanEquip ? TEXT("Click an item to pick it, double-click to equip it") : TEXT("Click an item to pick it (select a tower to equip)");
	Text(Hint, X + Pad, GY + GridH + 8.0f * S, DimText, FitSize(Hint, SizeSmall, GridW));
	Text(TEXT("Right-click an item: salvage it for pearls (locked items are kept)"), X + Pad, GY + GridH + 30.0f * S,
		DimText, FitSize(TEXT("Right-click an item: salvage it for pearls (locked items are kept)"), SizeSmall, GridW));

	// workbench buttons
	{
		const FTDItem* Item = Storage.IsValidIndex(Bench) ? &Storage[Bench] : nullptr;
		const float BY = GY + GridH + 56.0f * S;
		const float BH = 36.0f * S;
		const float BGap = 6.0f * S;
		const float BW = (GridW - BGap) * 0.5f;
		const bool bCanUpgrade = Item && Item->Level < TD::MaxItemLevel;
		const FString UpgradeLabel = !Item ? FString(TEXT("Upgrade")) : (bCanUpgrade ? FString::Printf(TEXT("Upgrade (%d)"), TD::UpgradeCost(*Item)) : FString(TEXT("Max level")));
		const FString RerollLabel = Item ? FString::Printf(TEXT("Reroll (%d)"), TD::RerollCost(*Item)) : FString(TEXT("Reroll"));
		const FString SalvageLabel = Item ? FString::Printf(TEXT("Salvage (+%d)"), TD::SalvageValue(Item->Rarity)) : FString(TEXT("Salvage"));
		// row 1: equip / salvage, row 2: upgrade / reroll
		const float BY2 = BY + BH + BGap;
		const FBox2D EquipRect(FVector2D(GX, BY), FVector2D(GX + BW, BY + BH));
		const FBox2D SalvageRect(FVector2D(GX + BW + BGap, BY), FVector2D(GX + GridW, BY + BH));
		const FBox2D UpRect(FVector2D(GX, BY2), FVector2D(GX + BW, BY2 + BH));
		const FBox2D RerollRect(FVector2D(GX + BW + BGap, BY2), FVector2D(GX + GridW, BY2 + BH));
		Button(EquipRect, bCanEquip ? TEXT("Equip on tower") : TEXT("Equip (select a tower)"), EButton::ItemEquip, 0, ActiveButton, Item && bCanEquip, false, SizeSmall);
		Button(UpRect, UpgradeLabel, EButton::ItemUpgrade, 0, GreenButton, bCanUpgrade && GameMode->GetPearls() >= TD::UpgradeCost(*Item), false, SizeSmall);
		const bool bUnique = Item && TD::IsUnique(*Item);
		Button(RerollRect, bUnique ? FString(TEXT("Reroll (unique)")) : RerollLabel, EButton::ItemReroll, 0, ButtonColor, Item && !bUnique && GameMode->GetPearls() >= TD::RerollCost(*Item), false, SizeSmall);
		if (bUnique && RerollRect.IsInside(MousePos))
		{
			SetTip({ { TEXT("Uniques can't be rerolled"), UniqueColor }, { TEXT("A unique is one of a kind. It can still be upgraded."), FLinearColor::White } });
		}
		Button(SalvageRect, SalvageLabel, EButton::ItemSalvage, 0, DangerButton, Item && !Item->bLocked, false, SizeSmall);

		// row 3: lock / unlock, and the auto-salvage setting for new drops
		const float BY3 = BY2 + BH + BGap;
		const FBox2D LockRect(FVector2D(GX, BY3), FVector2D(GX + BW, BY3 + BH));
		Button(LockRect, (Item && Item->bLocked) ? TEXT("Unlock item") : TEXT("Lock item"), EButton::ItemLock, 0, ButtonColor, Item != nullptr, false, SizeSmall);
		const FBox2D AutoRect(FVector2D(GX + BW + BGap, BY3), FVector2D(GX + GridW, BY3 + BH));
		const int32 Auto = GameMode->GetAutoSalvage();
		Button(AutoRect, FString::Printf(TEXT("Auto-salvage: %s"), Auto == 2 ? TEXT("Common, Rare") : ATDGameMode::AutoSalvageName(Auto)), EButton::CycleAutoSalvage, 0, ButtonColor, true, Auto > 0, SizeSmall);
		if (AutoRect.IsInside(MousePos))
		{
			SetTip({ { TEXT("Auto-salvage drops"), AccentColor }, { TEXT("Drops of the chosen rarities turn straight into pearls instead of going to storage"), FLinearColor::White },
				{ TEXT("Click to cycle: Off, Common, Common and Rare"), DimText } });
		}
		if (Item && LockRect.IsInside(MousePos))
		{
			SetTip({ { TEXT("Lock"), PearlColor }, { TEXT("A locked item can't be salvaged, and Sort leaves it in its place"), FLinearColor::White } });
		}
		if (Item && UpRect.IsInside(MousePos))
		{
			SetTip({ { TEXT("Upgrade"), PearlColor }, { FString::Printf(TEXT("+%d item levels: its bonuses grow, drawbacks stay the same (item level %d now)"), TD::ItemUpgradeLevels, Item->Level), FLinearColor::White } });
		}
		else if (Item && RerollRect.IsInside(MousePos))
		{
			SetTip({ { TEXT("Reroll"), PearlColor }, { TEXT("Turns it into a different item of the same slot, rarity and level"), FLinearColor::White } });
		}
	}
}
