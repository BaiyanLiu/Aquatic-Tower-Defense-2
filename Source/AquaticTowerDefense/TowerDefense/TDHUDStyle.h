// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Fonts/SlateFontInfo.h"
#include "TDItems.h"

/**
 *  Colors, sizes and font helpers shared by the HUD source files.
 *  "Abyssal Glass" look: rounded, translucent deep-teal panels, bioluminescent cyan accents,
 *  pearl gold for currency and the wave call, coral for lives and danger.
 */
namespace TDHUDStyle
{
	/** Color from 8-bit sRGB values (how the palette was picked), converted to the linear color the canvas expects */
	inline FLinearColor SRGB(uint8 R, uint8 G, uint8 B, float A = 1.0f)
	{
		auto ToLinear = [](uint8 C)
		{
			const float V = C / 255.0f;
			return V <= 0.04045f ? V / 12.92f : FMath::Pow((V + 0.055f) / 1.055f, 2.4f);
		};
		return FLinearColor(ToLinear(R), ToLinear(G), ToLinear(B), A);
	}

	// surfaces (glass)
	inline const FLinearColor PanelColor = SRGB(6, 26, 38, 0.68f);	// see-through glass; text keeps its outline and shadow
	inline const FLinearColor PanelEdge = SRGB(120, 236, 255, 0.34f);
	inline const FLinearColor ModalColor = SRGB(4, 20, 30, 0.86f);
	inline const FLinearColor TipColor = SRGB(4, 18, 28, 0.9f);
	inline const FLinearColor TrackColor = SRGB(0, 6, 12, 0.7f);
	inline const FLinearColor InsetColor = SRGB(0, 10, 18, 0.45f);	// stat cards, empty tiles, segmented tracks
	inline const FLinearColor InfoCardColor = SRGB(2, 14, 22, 0.76f);	// behind rows of small text on a panel, so the floor can't wash them out
	inline const FLinearColor ShadowColor(0.0f, 0.0f, 0.0f, 0.4f);

	// buttons: dark enough that white labels stay readable (the gold call-to-action gets dark text)
	inline const FLinearColor ButtonColor = SRGB(14, 64, 92, 0.8f);
	inline const FLinearColor ActiveButton = SRGB(0, 112, 146, 0.88f);
	inline const FLinearColor GreenButton = SRGB(0, 128, 104, 0.88f);
	inline const FLinearColor SupportButton = SRGB(76, 46, 140, 0.84f);
	inline const FLinearColor AmberButton = SRGB(255, 202, 112, 0.98f);
	inline const FLinearColor DangerButton = SRGB(140, 28, 52, 0.86f);
	inline const FLinearColor DisabledButton = SRGB(14, 30, 38, 0.62f);
	inline const FLinearColor ButtonEdge = SRGB(120, 236, 255, 0.42f);
	inline const FLinearColor DisabledEdge = SRGB(150, 240, 255, 0.08f);

	// text and accents
	inline const FLinearColor AccentColor = SRGB(92, 242, 255);
	inline const FLinearColor PearlColor = SRGB(255, 217, 138);
	inline const FLinearColor LifeColor = SRGB(255, 122, 142);
	inline const FLinearColor KelpColor = SRGB(107, 242, 176);
	inline const FLinearColor VioletColor = SRGB(176, 140, 255);
	inline const FLinearColor DimText = SRGB(150, 206, 224);
	inline const FLinearColor DisabledText = SRGB(98, 126, 134);
	inline const FLinearColor InfoText = SRGB(169, 218, 255);
	inline const FLinearColor ErrorText = SRGB(255, 107, 107);
	inline const FLinearColor DarkText = SRGB(42, 23, 0);
	inline const FLinearColor UniqueColor = SRGB(255, 176, 64);	// unique legendaries' rim, tag and name

	/**
	 *  One color per kind of tower / item stat, used everywhere that stat is shown (tower cards, item tooltips,
	 *  comparisons), so the same attribute is recognizable at a glance. Bright enough to read on the dark glass.
	 */
	inline const FLinearColor StatDamage = SRGB(255, 132, 96);		// coral orange: damage, bonus damage vs kinds of creature
	inline const FLinearColor StatRate = SRGB(255, 232, 92);		// lemon: attack speed
	inline const FLinearColor StatRange = SRGB(112, 172, 255);		// cornflower: range, aura range
	inline const FLinearColor StatCrit = SRGB(255, 112, 196);		// pink: critical hits
	inline const FLinearColor StatControl = SRGB(132, 236, 255);	// ice: slow, stun
	inline const FLinearColor StatArea = SRGB(188, 150, 255);		// violet: splash, chain targets
	inline const FLinearColor StatDot = SRGB(170, 230, 80);			// lime: poison, bleed
	inline const FLinearColor StatPierce = SRGB(204, 214, 228);		// steel: armor pierce, projectile
	inline const FLinearColor StatXp = SRGB(96, 240, 186);			// mint: XP gain
	inline const FLinearColor StatPearls = SRGB(255, 217, 138);		// pearl (same as PearlColor): pearls, item drop chance
	inline const FLinearColor StatLoss = SRGB(255, 112, 104);		// drawbacks and losses
	inline const FLinearColor StatGain = SRGB(120, 255, 140);		// gains in comparisons

	/** Stat color of an item effect */
	inline FLinearColor AffixColor(ETDAffix Affix)
	{
		switch (Affix)
		{
		case ETDAffix::DamagePct:
		case ETDAffix::DamageFlat:
		case ETDAffix::ArmoredDamage:
		case ETDAffix::BossDamage:
		case ETDAffix::Execute:
		case ETDAffix::AirDamage:		return StatDamage;
		case ETDAffix::RatePct:
		case ETDAffix::RetiredProjectileSpeed:	return StatRate;
		case ETDAffix::Range:
		case ETDAffix::RangePct:		return StatRange;
		case ETDAffix::Crit:
		case ETDAffix::CritDamage:		return StatCrit;
		case ETDAffix::SlowStrength:
		case ETDAffix::OnHitSlow:
		case ETDAffix::Stun:			return StatControl;
		case ETDAffix::Chain:
		case ETDAffix::Splash:			return StatArea;
		case ETDAffix::Poison:
		case ETDAffix::Bleed:			return StatDot;
		case ETDAffix::ArmorPierce:		return StatPierce;
		case ETDAffix::XpPct:
		case ETDAffix::XpTrickle:		return StatXp;
		case ETDAffix::BountyPct:
		case ETDAffix::GoldTrickle:
		case ETDAffix::ItemChance:		return StatPearls;
		default:						return FLinearColor::White;
		}
	}

	/** Stat color of an effect word in a description ("chain", "stuns", "poison", "damage"...), or transparent for any other word */
	inline FLinearColor EffectWordColor(const FString& InWord)
	{
		FString Word = InWord.ToLower();
		// drop punctuation around the word ("(stun", "slows,", "damage.")
		while (Word.Len() > 0 && !FChar::IsAlnum(Word[0]))
		{
			Word.RightChopInline(1);
		}
		while (Word.Len() > 0 && !FChar::IsAlnum(Word[Word.Len() - 1]))
		{
			Word.LeftChopInline(1);
		}
		static const TMap<FString, int32> Words = []()
		{
			TMap<FString, int32> M;
			auto Add = [&M](int32 Kind, std::initializer_list<const TCHAR*> List) { for (const TCHAR* W : List) { M.Add(W, Kind); } };
			Add(0, { TEXT("damage"), TEXT("damages"), TEXT("execute"), TEXT("executes") });
			Add(1, { TEXT("crit"), TEXT("crits"), TEXT("critical") });
			Add(2, { TEXT("range"), TEXT("reach") });
			Add(3, { TEXT("faster"), TEXT("rate"), TEXT("speed"), TEXT("attacks") });
			Add(4, { TEXT("slow"), TEXT("slows"), TEXT("slowed"), TEXT("slowing"), TEXT("chill"), TEXT("chills"), TEXT("chilled"), TEXT("chilling"), TEXT("freeze"), TEXT("freezes"),
				TEXT("frozen"), TEXT("stun"), TEXT("stuns"), TEXT("stunned"), TEXT("stunning"), TEXT("paralyze"), TEXT("paralyzes"), TEXT("paralyzed"), TEXT("paralyzing"),
				TEXT("concussion"), TEXT("knockback"), TEXT("root"), TEXT("roots"), TEXT("rooted") });
			Add(5, { TEXT("splash"), TEXT("splashes"), TEXT("chain"), TEXT("chains"), TEXT("chained"), TEXT("chaining"), TEXT("bounce"), TEXT("bounces"), TEXT("targets"),
				TEXT("blast"), TEXT("explodes"), TEXT("explosion") });
			Add(6, { TEXT("poison"), TEXT("poisons"), TEXT("poisoned"), TEXT("bleed"), TEXT("bleeds"), TEXT("bleeding"), TEXT("acid"), TEXT("toxin"), TEXT("toxins"),
				TEXT("venom"), TEXT("venomous") });
			Add(7, { TEXT("pierce"), TEXT("pierces"), TEXT("piercing"), TEXT("armor") });
			Add(8, { TEXT("xp"), TEXT("experience") });
			Add(9, { TEXT("pearls"), TEXT("pearl"), TEXT("bounty") });
			return M;
		}();
		const int32* Kind = Words.Find(Word);
		if (!Kind)
		{
			return FLinearColor::Transparent;
		}
		const FLinearColor Colors[] = { StatDamage, StatCrit, StatRange, StatRate, StatControl, StatArea, StatDot, StatPierce, StatXp, StatPearls };
		return Colors[*Kind];
	}

	/**
	 *  Splits a description into colored pieces: an effect word that is part of a stat (a number close by in the same phrase)
	 *  is drawn in its stat color together with that number (see EffectWordColor); everything else in Base.
	 */
	inline TArray<TPair<FString, FLinearColor>> ColorizeEffects(const FString& InText, const FLinearColor& Base)
	{
		// words with their trailing spaces
		TArray<FString> Tokens;
		TArray<FString> Bare;
		for (int32 i = 0; i < InText.Len();)
		{
			int32 j = i;
			while (j < InText.Len() && InText[j] != TCHAR(' '))
			{
				++j;
			}
			int32 k = j;
			while (k < InText.Len() && InText[k] == TCHAR(' '))
			{
				++k;
			}
			Tokens.Add(InText.Mid(i, k - i));
			Bare.Add(InText.Mid(i, j - i));
			i = k;
		}
		auto HasDigit = [](const FString& Str)
		{
			for (TCHAR C : Str)
			{
				if (FChar::IsDigit(C))
				{
					return true;
				}
			}
			return false;
		};
		// a phrase ends at a word followed by punctuation ("Poison:", "targets,"), so a number never reaches across it
		auto EndsPhrase = [](const FString& Word)
		{
			if (Word.IsEmpty())
			{
				return false;
			}
			const TCHAR Last = Word[Word.Len() - 1];
			return Last == TCHAR(',') || Last == TCHAR('.') || Last == TCHAR(';') || Last == TCHAR(':') || Last == TCHAR(')');
		};
		auto SamePhrase = [&Bare, &EndsPhrase](int32 A, int32 B)
		{
			for (int32 k = FMath::Min(A, B); k < FMath::Max(A, B); ++k)
			{
				if (EndsPhrase(Bare[k]))
				{
					return false;
				}
			}
			return true;
		};
		// an effect word is only colored when it is part of a stat: a number within two words of it in the same phrase
		// ("+20% damage", "stuns for 1.5s", "chains to 3 targets", "x2 crit damage"); the number and the words between go with it.
		// Plain mentions ("deals damage to groups", "stunned creatures") keep the base color.
		TArray<FLinearColor> Colors;
		Colors.Init(Base, Tokens.Num());
		TArray<bool> bColored;
		bColored.Init(false, Tokens.Num());
		for (int32 i = 0; i < Tokens.Num(); ++i)
		{
			const FLinearColor C = EffectWordColor(Bare[i]);
			if (C.A <= 0.0f)
			{
				continue;
			}
			int32 Number = INDEX_NONE;
			for (int32 Dist = 1; Dist <= 2 && Number == INDEX_NONE; ++Dist)
			{
				for (const int32 j : { i - Dist, i + Dist })
				{
					if (j >= 0 && j < Tokens.Num() && HasDigit(Bare[j]) && SamePhrase(i, j))
					{
						Number = j;
						break;
					}
				}
			}
			if (Number == INDEX_NONE)
			{
				continue;
			}
			for (int32 k = FMath::Min(i, Number); k <= FMath::Max(i, Number); ++k)
			{
				// a word between that another stat already took keeps its color
				if (!bColored[k] || k == i)
				{
					Colors[k] = C;
					bColored[k] = true;
				}
			}
		}
		// merge neighbours of the same color
		TArray<TPair<FString, FLinearColor>> Parts;
		for (int32 i = 0; i < Tokens.Num(); ++i)
		{
			if (Parts.Num() > 0 && Parts.Last().Value.Equals(Colors[i]))
			{
				Parts.Last().Key += Tokens[i];
			}
			else
			{
				Parts.Add({ Tokens[i], Colors[i] });
			}
		}
		return Parts;
	}

	/** Text sizes (before UI scale): headings at 20 and above are bold */
	constexpr float SizeHeading = 21.0f;
	constexpr float SizeBody = 17.0f;
	constexpr float SizeField = 16.0f;
	constexpr float SizeSmall = 15.0f;
	constexpr float SizeCaption = 12.0f;
	constexpr float SizeMin = 11.0f;

	/** Corner radii (before UI scale) */
	constexpr float RadiusPanel = 16.0f;
	constexpr float RadiusButton = 10.0f;
	constexpr float RadiusTile = 8.0f;

	/** Top of the side panels, below the floating top clusters (before UI scale) */
	constexpr float SideTop = 88.0f;
	/** Height of the floating top clusters (before UI scale) */
	constexpr float TopClusterH = 62.0f;

	constexpr double ToastDuration = 2.6;
	constexpr double FloaterDuration = 1.1;
	constexpr int32 StorageColumns = 8;

	/** Tower dock button height and panel header icon size (before UI scale) */
	constexpr float DockSlotHeight = 104.0f;
	constexpr float HeaderIcon = 40.0f;

	inline const TCHAR* const TabNames[] = { TEXT("Stats"), TEXT("Items"), TEXT("Skills") };

	inline UFont* HUDFont()
	{
		return GEngine ? GEngine->GetMediumFont() : nullptr;
	}

	/** Whole pixels, so edges stay crisp and frames keep an even thickness */
	inline FBox2D Snap(const FBox2D& Rect)
	{
		return FBox2D(FVector2D(FMath::RoundToFloat(Rect.Min.X), FMath::RoundToFloat(Rect.Min.Y)),
			FVector2D(FMath::RoundToFloat(Rect.Max.X), FMath::RoundToFloat(Rect.Max.Y)));
	}

	/**
	 *  Builds a font rasterized at the exact pixel size (crisp at any resolution), bold for headings,
	 *  with a dark outline so text stays readable over the seabed.
	 *  Weight follows the unscaled size, so the hierarchy looks the same at every resolution; bBold forces bold at any size.
	 */
	inline FSlateFontInfo MakeFont(float Size, float Scale, bool bBold = false)
	{
		const float PixelSize = Size * Scale;
		// Slate font sizes are in points at 96 DPI
		const int32 Points = FMath::Max(6, FMath::RoundToInt(PixelSize * 0.75f));
		FSlateFontInfo Info(HUDFont(), Points, (bBold || Size >= 20.0f) ? FName(TEXT("Bold")) : FName(TEXT("Regular")));
		Info.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt(PixelSize / 14.0f));
		Info.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.9f);
		return Info;
	}

	inline FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A = Alpha;
		return Color;
	}

	/**
	 *  Short form for big numbers so they fit panels and tooltips: 9,999 and below are shown in full
	 *  (with Decimals places), larger ones as 12.4K, 3.21M, 1.5B, 2.04T.
	 */
	inline FString ShortNumber(double Value, int32 Decimals = 0)
	{
		const double Abs = FMath::Abs(Value);
		if (Abs < 10000.0)
		{
			return Decimals >= 2 ? FString::Printf(TEXT("%.2f"), Value) : (Decimals == 1 ? FString::Printf(TEXT("%.1f"), Value) : FString::Printf(TEXT("%.0f"), Value));
		}
		static const TCHAR* const Suffixes[] = { TEXT("K"), TEXT("M"), TEXT("B"), TEXT("T") };
		double Scaled = Value / 1000.0;
		int32 Index = 0;
		while (FMath::Abs(Scaled) >= 999.5 && Index < 3)
		{
			Scaled /= 1000.0;
			++Index;
		}
		const double ScaledAbs = FMath::Abs(Scaled);
		const FString Number = ScaledAbs < 10.0 ? FString::Printf(TEXT("%.2f"), Scaled) : (ScaledAbs < 100.0 ? FString::Printf(TEXT("%.1f"), Scaled) : FString::Printf(TEXT("%.0f"), Scaled));
		return Number + Suffixes[Index];
	}

	/** A tower's per-hit damage range, "22-31" (one decimal while the top of the range is below 10) */
	inline FString DamageRangeText(float Average, float Spread)
	{
		const float Low = Average * (1.0f - Spread);
		const float High = Average * (1.0f + Spread);
		const int32 Decimals = High < 10.0f ? 1 : 0;
		if (ShortNumber(Low, Decimals) == ShortNumber(High, Decimals))
		{
			return ShortNumber(Average, Decimals);
		}
		return ShortNumber(Low, Decimals) + TEXT("-") + ShortNumber(High, Decimals);
	}

	/** Moves a color toward white (hover highlight), keeping its alpha */
	inline FLinearColor Lighten(const FLinearColor& Color, float Amount)
	{
		FLinearColor Out = FMath::Lerp(Color, FLinearColor::White, Amount);
		Out.A = Color.A;
		return Out;
	}

	/** True for light fills (the gold button) that need dark text */
	inline bool IsLightFill(const FLinearColor& Color)
	{
		return Color.GetLuminance() > 0.35f;
	}
}
