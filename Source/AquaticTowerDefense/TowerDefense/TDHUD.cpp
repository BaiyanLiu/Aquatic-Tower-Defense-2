// Aquatic Tower Defense - isometric underwater tower defense with mazing.

// Core: input hit-testing, drawing helpers, top bar, dock, overlays, tooltips and toasts.

#include "TDHUD.h"
#include "TDAudio.h"
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
#include "Misc/App.h"
#include "TDHUDStyle.h"
#include "TDIcons.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "HAL/IConsoleManager.h"

using namespace TDHUDStyle;

// ---------------------------------------------------------------------------------------------
// Shape textures for rounded panels, outlines and glows
// ---------------------------------------------------------------------------------------------

namespace
{
	/** Texture size and corner size in texels: two straight texels in the middle get stretched */
	constexpr int32 ShapeSize = 64;
	constexpr float ShapeCorner = 31.0f;

	enum class EShape : uint8
	{
		Fill,	// solid rounded rectangle
		Glow,	// opaque inside, fading to nothing at the edge
		Ring	// outline; one texture per outline-to-corner ratio
	};

	/** Alpha of a shape texel (signed distance to a rounded square filling the texture) */
	float ShapeAlpha(EShape Shape, int32 X, int32 Y, float RingWidth)
	{
		const float Half = ShapeSize * 0.5f;
		const float Straight = Half - ShapeCorner;
		const float PX = FMath::Max(FMath::Abs(X + 0.5f - Half) - Straight, 0.0f);
		const float PY = FMath::Max(FMath::Abs(Y + 0.5f - Half) - Straight, 0.0f);
		const float Dist = FMath::Sqrt(PX * PX + PY * PY) - ShapeCorner;	// negative inside
		switch (Shape)
		{
		case EShape::Fill:
			return FMath::Clamp(0.5f - Dist, 0.0f, 1.0f);
		case EShape::Glow:
			return FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(-Dist / ShapeCorner, 0.0f, 1.0f));
		case EShape::Ring:
		default:
			return FMath::Clamp(0.5f - Dist, 0.0f, 1.0f) * FMath::Clamp(Dist + RingWidth + 0.5f, 0.0f, 1.0f);
		}
	}

	UTexture2D* MakeShapeTexture(EShape Shape, float RingWidth)
	{
		const FName Name = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName(TEXT("TDHUDShape")));
		UTexture2D* Texture = UTexture2D::CreateTransient(ShapeSize, ShapeSize, PF_B8G8R8A8, Name);
		if (!Texture)
		{
			return nullptr;
		}
		Texture->SRGB = false;
		Texture->Filter = TF_Trilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;

		// white texels, the shape lives in alpha (straight alpha, tinted when drawn)
		TArray<float> Alpha;
		Alpha.SetNumUninitialized(ShapeSize * ShapeSize);
		for (int32 Y = 0; Y < ShapeSize; ++Y)
		{
			for (int32 X = 0; X < ShapeSize; ++X)
			{
				Alpha[Y * ShapeSize + X] = ShapeAlpha(Shape, X, Y, RingWidth);
			}
		}

		// box-filtered mips so small corners stay smooth
		FTexturePlatformData* Data = Texture->GetPlatformData();
		int32 MipSize = ShapeSize;
		for (int32 Level = 0; MipSize >= 4; ++Level)
		{
			FTexture2DMipMap* MipMap = nullptr;
			if (Level == 0)
			{
				MipMap = &Data->Mips[0];
			}
			else
			{
				MipMap = new FTexture2DMipMap(MipSize, MipSize, 1);
				Data->Mips.Add(MipMap);
			}
			TArray<FColor> Pixels;
			Pixels.SetNumUninitialized(MipSize * MipSize);
			for (int32 i = 0; i < Pixels.Num(); ++i)
			{
				Pixels[i] = FColor(255, 255, 255, (uint8)FMath::RoundToInt(FMath::Clamp(Alpha[i], 0.0f, 1.0f) * 255.0f));
			}
			const int64 Bytes = (int64)Pixels.Num() * sizeof(FColor);
			MipMap->BulkData.Lock(LOCK_READ_WRITE);
			void* Dest = MipMap->BulkData.Realloc(Bytes);
			FMemory::Memcpy(Dest, Pixels.GetData(), Bytes);
			MipMap->BulkData.Unlock();

			if (MipSize > 4)
			{
				const int32 Next = MipSize / 2;
				TArray<float> Smaller;
				Smaller.SetNumUninitialized(Next * Next);
				for (int32 Y = 0; Y < Next; ++Y)
				{
					for (int32 X = 0; X < Next; ++X)
					{
						Smaller[Y * Next + X] = 0.25f * (Alpha[(Y * 2) * MipSize + X * 2] + Alpha[(Y * 2) * MipSize + X * 2 + 1]
							+ Alpha[(Y * 2 + 1) * MipSize + X * 2] + Alpha[(Y * 2 + 1) * MipSize + X * 2 + 1]);
					}
				}
				Alpha = MoveTemp(Smaller);
			}
			MipSize /= 2;
		}

		Texture->UpdateResource();
		Texture->AddToRoot();
		return Texture;
	}

	/** Cached shape texture. Ring textures are keyed by the outline width in texels (quarter-texel steps). */
	UTexture2D* ShapeTexture(EShape Shape, float RingWidth = 0.0f)
	{
		static TMap<int32, UTexture2D*> Cache;
		const int32 RingKey = Shape == EShape::Ring ? FMath::Clamp(FMath::RoundToInt(RingWidth * 4.0f), 2, (int32)(ShapeCorner * 4.0f)) : 0;
		const int32 Key = (int32)Shape * 1000 + RingKey;
		if (UTexture2D** Found = Cache.Find(Key))
		{
			return *Found;
		}
		UTexture2D* Texture = MakeShapeTexture(Shape, RingKey / 4.0f);
		Cache.Add(Key, Texture);
		return Texture;
	}

	/** Wave texture: two gentle sine periods across, tiling sideways */
	constexpr int32 WaveW = 128;
	constexpr int32 WaveH = 8;

	float WaveCurve(float X)
	{
		return WaveH * 0.5f + 2.4f * FMath::Sin(X * 2.0f * PI * 2.0f / WaveW);
	}

	UTexture2D* MakeWaveTexture(bool bFilled)
	{
		const FName Name = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName(TEXT("TDHUDWave")));
		UTexture2D* Texture = UTexture2D::CreateTransient(WaveW, WaveH, PF_B8G8R8A8, Name);
		if (!Texture)
		{
			return nullptr;
		}
		Texture->SRGB = false;
		Texture->Filter = TF_Bilinear;
		Texture->AddressX = TA_Wrap;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;

		// 4x4 supersampled coverage of a 1.5-texel line, or of everything above the curve
		TArray<FColor> Pixels;
		Pixels.SetNumUninitialized(WaveW * WaveH);
		for (int32 Y = 0; Y < WaveH; ++Y)
		{
			for (int32 X = 0; X < WaveW; ++X)
			{
				float Coverage = 0.0f;
				for (int32 SY = 0; SY < 4; ++SY)
				{
					for (int32 SX = 0; SX < 4; ++SX)
					{
						const float PX = X + (SX + 0.5f) / 4.0f;
						const float PY = Y + (SY + 0.5f) / 4.0f;
						const float Curve = WaveCurve(PX);
						Coverage += (bFilled ? PY < Curve : FMath::Abs(PY - Curve) < 0.75f) ? 1.0f / 16.0f : 0.0f;
					}
				}
				Pixels[Y * WaveW + X] = FColor(255, 255, 255, (uint8)FMath::RoundToInt(Coverage * 255.0f));
			}
		}
		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		const int64 Bytes = (int64)Pixels.Num() * sizeof(FColor);
		Mip.BulkData.Lock(LOCK_READ_WRITE);
		void* Dest = Mip.BulkData.Realloc(Bytes);
		FMemory::Memcpy(Dest, Pixels.GetData(), Bytes);
		Mip.BulkData.Unlock();
		Texture->UpdateResource();
		Texture->AddToRoot();
		return Texture;
	}

	UTexture2D* WaveTexture(bool bFilled)
	{
		static UTexture2D* Line = nullptr;
		static UTexture2D* Filled = nullptr;
		UTexture2D*& Slot = bFilled ? Filled : Line;
		if (!Slot)
		{
			Slot = MakeWaveTexture(bFilled);
		}
		return Slot;
	}

	// ---------------------------------------------------------------- detailed look (atd.DetailedUI): painted rims and lips

	TAutoConsoleVariable<int32> CVarDetailedUI(
		TEXT("atd.DetailedUI"),
		1,
		TEXT("1 draws the interface with the detailed look: nacre rims, glass buttons and meters, recessed item wells tinted by rarity; 0 the plain shapes."));

	bool DetailedUI()
	{
		return CVarDetailedUI.GetValueOnGameThread() != 0;
	}

	/** A per-point function (linear colour, straight alpha) 4x supersampled into premultiplied texels, so filtering never darkens edges */
	TArray<FLinearColor> PaintPremultiplied(int32 Size, TFunctionRef<FLinearColor(float X, float Y)> Paint)
	{
		TArray<FLinearColor> Texels;
		Texels.SetNumUninitialized(Size * Size);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				FLinearColor Sum(0, 0, 0, 0);
				for (int32 S = 0; S < 4; ++S)
				{
					const FLinearColor C = Paint(X + 0.25f + 0.5f * (S & 1), Y + 0.25f + 0.5f * (S >> 1));
					const float A = FMath::Clamp(C.A, 0.0f, 1.0f);
					Sum += FLinearColor(C.R * A, C.G * A, C.B * A, A);
				}
				Texels[Y * Size + X] = Sum * 0.25f;
			}
		}
		return Texels;
	}

	/** A coloured texture from a per-point function (linear colour, straight alpha), 4x supersampled, stored sRGB with box-filtered mips */
	UTexture2D* MakePaintedTexture(int32 Size, TFunctionRef<FLinearColor(float X, float Y)> Paint, const TCHAR* BaseName)
	{
		const FName Name = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName(BaseName));
		UTexture2D* Texture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8, Name);
		if (!Texture)
		{
			return nullptr;
		}
		Texture->SRGB = true;
		Texture->Filter = TF_Trilinear;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->NeverStream = true;

		TArray<FLinearColor> Texels = PaintPremultiplied(Size, Paint);
		FTexturePlatformData* Data = Texture->GetPlatformData();
		int32 MipSize = Size;
		for (int32 Level = 0; MipSize >= 4; ++Level)
		{
			FTexture2DMipMap* MipMap = Level == 0 ? &Data->Mips[0] : new FTexture2DMipMap(MipSize, MipSize, 1);
			if (Level > 0)
			{
				Data->Mips.Add(MipMap);
			}
			TArray<FColor> Pixels;
			Pixels.SetNumUninitialized(MipSize * MipSize);
			for (int32 i = 0; i < Pixels.Num(); ++i)
			{
				const FLinearColor& P = Texels[i];
				const FLinearColor Straight = P.A > 1e-4f ? FLinearColor(P.R / P.A, P.G / P.A, P.B / P.A, P.A) : FLinearColor(0, 0, 0, 0);
				Pixels[i] = Straight.GetClamped().ToFColor(true);
			}
			const int64 Bytes = (int64)Pixels.Num() * sizeof(FColor);
			MipMap->BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(MipMap->BulkData.Realloc(Bytes), Pixels.GetData(), Bytes);
			MipMap->BulkData.Unlock();
			if (MipSize > 4)
			{
				const int32 Next = MipSize / 2;
				TArray<FLinearColor> Smaller;
				Smaller.SetNumUninitialized(Next * Next);
				for (int32 Y = 0; Y < Next; ++Y)
				{
					for (int32 X = 0; X < Next; ++X)
					{
						Smaller[Y * Next + X] = (Texels[(Y * 2) * MipSize + X * 2] + Texels[(Y * 2) * MipSize + X * 2 + 1]
							+ Texels[(Y * 2 + 1) * MipSize + X * 2] + Texels[(Y * 2 + 1) * MipSize + X * 2 + 1]) * 0.25f;
					}
				}
				Texels = MoveTemp(Smaller);
			}
			MipSize /= 2;
		}
		Texture->UpdateResource();
		Texture->AddToRoot();
		return Texture;
	}

	/** The rounded square of the shape textures at a texel: signed distance (negative inside) and the outward direction */
	float ShapeDistance(float X, float Y, FVector2f& Outward)
	{
		const float Half = ShapeSize * 0.5f;
		const float Straight = Half - ShapeCorner;
		const float DX = X - Half, DY = Y - Half;
		const float PX = FMath::Max(FMath::Abs(DX) - Straight, 0.0f);
		const float PY = FMath::Max(FMath::Abs(DY) - Straight, 0.0f);
		const FVector2f P(PX * FMath::Sign(DX), PY * FMath::Sign(DY));
		Outward = P.SizeSquared() > 1e-6f ? P.GetSafeNormal() : (FMath::Abs(DX) > FMath::Abs(DY) ? FVector2f(FMath::Sign(DX), 0.0f) : FVector2f(0.0f, FMath::Sign(DY)));
		return FMath::Sqrt(PX * PX + PY * PY) - ShapeCorner;
	}

	float SmoothStep01(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	/** Light from the upper left, as in the painted previews */
	const FVector3f LightDir = FVector3f(-0.55f, -0.83f, 0.75f).GetSafeNormal();

	/** Mother-of-pearl: pale with pastel pink, mint and lilac shifting round the shape */
	FLinearColor Nacre(float Angle, float Along, float Amount)
	{
		const float T = FMath::Sin(Angle * 2.0f + Along * 0.1f) * 0.5f + 0.5f;
		const FLinearColor Pink(1.0f, 0.65f, 0.78f), Mint(0.58f, 1.0f, 0.87f), Lilac(0.64f, 0.67f, 1.0f);
		const FLinearColor Shift = T < 0.5f ? FMath::Lerp(Pink, Mint, T * 2.0f) : FMath::Lerp(Mint, Lilac, (T - 0.5f) * 2.0f);
		return FMath::Lerp(FLinearColor::White, Shift, Amount);
	}

	/** The nacre frame's colour at a texel for a band W texels wide (see RimTexture) */
	FLinearColor PaintRim(float W, float X, float Y)
	{
		FVector2f Out;
		const float D = ShapeDistance(X, Y, Out);
		const float Ink = FMath::Clamp(W * 0.18f, 0.6f, 1.2f);		// keyline width
		const float Alpha = FMath::Clamp(0.5f - (D - Ink * 0.5f), 0.0f, 1.0f) * FMath::Clamp(D + W + Ink * 0.5f + 0.5f, 0.0f, 1.0f);
		if (Alpha <= 0.0f)
		{
			return FLinearColor(0, 0, 0, 0);
		}
		const float T = FMath::Clamp(-D / W, 0.0f, 1.0f);		// 0 at the outer edge, 1 at the inner
		// a rounded band: its surface tilts outward on the outer half and inward on the inner half
		const float Slope = FMath::Cos(T * PI) * 0.9f;
		const FVector3f N = FVector3f(Out.X * Slope, Out.Y * Slope, 1.0f).GetSafeNormal();
		const float Diffuse = FMath::Max(0.0f, FVector3f::DotProduct(N, LightDir));
		const FVector3f H = (LightDir + FVector3f(0, 0, 1)).GetSafeNormal();
		const float Spec = FMath::Pow(FMath::Max(0.0f, FVector3f::DotProduct(N, H)), 28.0f) * 0.55f;
		const float Shade = 0.5f + 0.5f * Diffuse;
		const float Angle = FMath::Atan2(Y - ShapeSize * 0.5f, X - ShapeSize * 0.5f);
		const float Grain = 1.0f + 0.07f * FMath::Sin(T * 9.0f * PI);
		FLinearColor C = Nacre(Angle, X + Y, 0.3f) * (Shade * Grain) + FLinearColor(Spec, Spec, Spec, 0.0f);
		// dark keylines on both edges
		const float Key0 = SmoothStep01(Ink, 0.0f, FMath::Abs(D));
		const float Key1 = SmoothStep01(Ink, 0.0f, FMath::Abs(D + W));
		C = FMath::Lerp(C, FLinearColor(0.004f, 0.01f, 0.015f), FMath::Max(Key0 * 0.85f, Key1 * 0.7f));
		C.A = Alpha;
		return C;
	}

	/** A nacre frame for NineSlice: a rounded band Width texels wide, lit from the upper left, with fine growth layers along it and
	 *  a dark keyline either side. Near white: the draw colour tints it. */
	UTexture2D* RimTexture(float Width)
	{
		static TMap<int32, UTexture2D*> Cache;
		const int32 Key = FMath::Clamp(FMath::RoundToInt(Width * 4.0f), 8, (int32)(ShapeCorner * 2.0f));
		if (UTexture2D** Found = Cache.Find(Key))
		{
			return *Found;
		}
		const float W = Key / 4.0f;
		UTexture2D* Texture = MakePaintedTexture(ShapeSize, [W](float X, float Y) { return PaintRim(W, X, Y); }, TEXT("TDHUDRim"));
		Cache.Add(Key, Texture);
		return Texture;
	}

	/** The inner lip's colour at a texel, fading inward over W texels (see LipTexture) */
	FLinearColor PaintLip(float W, float X, float Y)
	{
		FVector2f Out;
		const float D = ShapeDistance(X, Y, Out);
		if (D > 0.0f)
		{
			return FLinearColor(0, 0, 0, 0);
		}
		const float Fade = SmoothStep01(W, 0.0f, -D);
		const float Facing = FVector2f::DotProduct(Out, FVector2f(LightDir.X, LightDir.Y).GetSafeNormal());
		return Facing > 0.0f ? FLinearColor(0.85f, 0.98f, 1.0f, Fade * Facing) : FLinearColor(0.0f, 0.0f, 0.0f, Fade * -Facing);
	}

	/** The pearl's colour at a texel of its 64 texel tile (see PearlTexture) */
	FLinearColor PaintPearl(float X, float Y)
	{
		const float R = 30.0f;
		const FVector2f P(X - 32.0f, Y - 32.0f);
		const float D = P.Size() - R;
		const float Alpha = FMath::Clamp(0.5f - D, 0.0f, 1.0f);
		if (Alpha <= 0.0f)
		{
			return FLinearColor(0, 0, 0, 0);
		}
		const float Q = FMath::Min(P.Size() / R, 1.0f);
		const FVector3f N = FVector3f(P.X / R, P.Y / R, FMath::Sqrt(FMath::Max(0.0f, 1.0f - Q * Q))).GetSafeNormal();
		const float Diffuse = FMath::Max(0.0f, FVector3f::DotProduct(N, LightDir));
		const FVector3f H = (LightDir + FVector3f(0, 0, 1)).GetSafeNormal();
		const float Spec = FMath::Pow(FMath::Max(0.0f, FVector3f::DotProduct(N, H)), 28.0f) * 0.7f;
		const FLinearColor Base(0.89f, 0.9f, 0.94f), Dark(0.27f, 0.31f, 0.48f);
		FLinearColor C = FMath::Lerp(Dark, Base, FMath::Clamp((0.55f + 0.45f * Diffuse - 0.35f) / 0.65f, 0.0f, 1.0f));
		C = FMath::Lerp(C, Nacre(FMath::Atan2(P.Y, P.X), 0.0f, 0.9f) * C.GetMax(), SmoothStep01(0.3f, 1.0f, Q) * 0.28f);
		C += FLinearColor(Spec, Spec, Spec, 0.0f);
		C = FMath::Lerp(C, FLinearColor(0, 0, 0), SmoothStep01(1.6f, 0.0f, FMath::Abs(D)) * 0.35f);
		C.A = Alpha;
		return C;
	}

	/** The bubble's colour at a texel of its 64 texel tile (see BubbleTexture) */
	FLinearColor PaintBubble(float X, float Y)
	{
		const float R = 29.0f;
		const FVector2f P(X - 32.0f, Y - 32.0f);
		const float D = P.Size() - R;
		if (D > 1.0f)
		{
			return FLinearColor(0, 0, 0, 0);
		}
		const float Q = FMath::Min(P.Size() / R, 1.0f);
		const float Lit = FMath::Clamp(-(P.X * LightDir.X + P.Y * LightDir.Y) / R * 0.5f + 0.5f, 0.0f, 1.0f);
		// the body is nearly clear, thickening toward the rim
		float A = 0.08f + 0.22f * FMath::Pow(Q, 6.0f);
		FLinearColor C(0.75f, 0.95f, 1.0f, 1.0f);
		// the rim: a crisp edge, brighter on the lit side
		const float Rim = SmoothStep01(4.8f, 0.0f, FMath::Abs(D + 2.6f));
		A = FMath::Max(A, Rim * (0.55f + 0.4f * Lit));
		C = FMath::Lerp(C, FLinearColor::White, Rim * Lit);
		// a curved window glint on the upper left and a small reflection lower right
		const float Ang = FMath::Atan2(P.Y, P.X);
		const float Arc = SmoothStep01(4.5f, 0.0f, FMath::Abs(P.Size() - R * 0.66f)) * SmoothStep01(0.55f, 0.0f, FMath::Abs(Ang + 2.25f));
		const float Dot = SmoothStep01(5.5f, 2.5f, (P - FVector2f(R * 0.4f, R * 0.42f)).Size());
		A = FMath::Max(A, FMath::Max(Arc * 0.95f, Dot * 0.6f));
		C = FMath::Lerp(C, FLinearColor::White, FMath::Max(Arc, Dot));
		C.A = A * FMath::Clamp(0.5f - D, 0.0f, 1.0f);
		return C;
	}

	/** The inner lip of a glass panel for NineSlice: a light edge along the top left and a shadow along the bottom right, fading
	 *  inward over Width texels. Drawn untinted. */
	UTexture2D* LipTexture(float Width)
	{
		static TMap<int32, UTexture2D*> Cache;
		const int32 Key = FMath::Clamp(FMath::RoundToInt(Width * 4.0f), 4, (int32)(ShapeCorner * 3.0f));
		if (UTexture2D** Found = Cache.Find(Key))
		{
			return *Found;
		}
		const float W = Key / 4.0f;
		UTexture2D* Texture = MakePaintedTexture(ShapeSize, [W](float X, float Y) { return PaintLip(W, X, Y); }, TEXT("TDHUDLip"));
		Cache.Add(Key, Texture);
		return Texture;
	}

	/** A pearl: a lit white ball with a pastel orient toward its rim, a glint and a faint dark outline. Drawn tinted for beads. */
	UTexture2D* PearlTexture()
	{
		static UTexture2D* Texture = nullptr;
		if (!Texture)
		{
			Texture = MakePaintedTexture(64, [](float X, float Y) { return PaintPearl(X, Y); }, TEXT("TDHUDPearl"));
		}
		return Texture;
	}

	/** A soap-clear bubble: a faint tinted body, a thin rim brighter on the lit side, a curved glint and a small back reflection.
	 *  Near white: the draw colour tints it. */
	UTexture2D* BubbleTexture()
	{
		static UTexture2D* Texture = nullptr;
		if (!Texture)
		{
			Texture = MakePaintedTexture(64, [](float X, float Y) { return PaintBubble(X, Y); }, TEXT("TDHUDBubble"));
		}
		return Texture;
	}

	// ---------------------------------------------------------------- one atlas for every interface shape
	//
	// The canvas starts a new draw (a render pass of its own) whenever the texture changes, and the detailed look layers shadow,
	// fill, sheen, lip, rim and pearls from different textures, so a frame came to several hundred draws. Every shape above is
	// painted into cells of one texture instead, with the same texels and mips as its own texture had, so runs of shapes batch.
	// Each cell sits in a 96 texel slot with a gutter that repeats its edge texels (or wraps, for the wave), which keeps
	// filtering exactly as it was with clamped textures, down to the 4x4 mip of a 64 texel shape.

	TAutoConsoleVariable<int32> CVarHUDAtlas(
		TEXT("atd.HUDAtlas"),
		1,
		TEXT("1 draws the interface's shapes from one atlas texture so they batch into few draws; 0 from a texture per shape (the same pixels, many more draws)."));

	/** Cell keys: kinds below 1000 are single cells, outline widths add their quarter-texel key to the kind's base */
	enum EAtlasKey : int32
	{
		AtlasWhite = 1, AtlasFill, AtlasGlow, AtlasPearl, AtlasBubble, AtlasWaveLine, AtlasWaveFilled,
		AtlasRingBase = 1000, AtlasRimBase = 2000, AtlasLipBase = 3000
	};

	int32 RingWidthKey(float RingWidth) { return FMath::Clamp(FMath::RoundToInt(RingWidth * 4.0f), 2, (int32)(ShapeCorner * 4.0f)); }
	int32 RimWidthKey(float Width) { return FMath::Clamp(FMath::RoundToInt(Width * 4.0f), 8, (int32)(ShapeCorner * 2.0f)); }
	int32 LipWidthKey(float Width) { return FMath::Clamp(FMath::RoundToInt(Width * 4.0f), 4, (int32)(ShapeCorner * 3.0f)); }

	/** A cell's content: premultiplied linear texels, W x H */
	struct FAtlasImage
	{
		int32 W = 0, H = 0;
		bool bWrapX = false;
		TArray<FLinearColor> Texels;
	};

	FAtlasImage MakeAtlasImage(int32 Key)
	{
		FAtlasImage Image;
		Image.W = Image.H = ShapeSize;
		auto Shape = [&Image](EShape Shape, float RingWidth)
		{
			Image.Texels.SetNumUninitialized(ShapeSize * ShapeSize);
			for (int32 Y = 0; Y < ShapeSize; ++Y)
			{
				for (int32 X = 0; X < ShapeSize; ++X)
				{
					const float A = FMath::Clamp(ShapeAlpha(Shape, X, Y, RingWidth), 0.0f, 1.0f);
					Image.Texels[Y * ShapeSize + X] = FLinearColor(A, A, A, A);
				}
			}
		};
		if (Key == AtlasWhite)
		{
			Image.Texels.Init(FLinearColor::White, ShapeSize * ShapeSize);
		}
		else if (Key == AtlasFill)
		{
			Shape(EShape::Fill, 0.0f);
		}
		else if (Key == AtlasGlow)
		{
			Shape(EShape::Glow, 0.0f);
		}
		else if (Key == AtlasPearl)
		{
			Image.Texels = PaintPremultiplied(64, [](float X, float Y) { return PaintPearl(X, Y); });
		}
		else if (Key == AtlasBubble)
		{
			Image.Texels = PaintPremultiplied(64, [](float X, float Y) { return PaintBubble(X, Y); });
		}
		else if (Key == AtlasWaveLine || Key == AtlasWaveFilled)
		{
			// the same 4x4 supersampled coverage as MakeWaveTexture
			const bool bFilled = Key == AtlasWaveFilled;
			Image.W = WaveW;
			Image.H = WaveH;
			Image.bWrapX = true;
			Image.Texels.SetNumUninitialized(WaveW * WaveH);
			for (int32 Y = 0; Y < WaveH; ++Y)
			{
				for (int32 X = 0; X < WaveW; ++X)
				{
					float Coverage = 0.0f;
					for (int32 SY = 0; SY < 4; ++SY)
					{
						for (int32 SX = 0; SX < 4; ++SX)
						{
							const float PX = X + (SX + 0.5f) / 4.0f;
							const float PY = Y + (SY + 0.5f) / 4.0f;
							const float Curve = WaveCurve(PX);
							Coverage += (bFilled ? PY < Curve : FMath::Abs(PY - Curve) < 0.75f) ? 1.0f / 16.0f : 0.0f;
						}
					}
					const float A = FMath::RoundToInt(Coverage * 255.0f) / 255.0f;
					Image.Texels[Y * WaveW + X] = FLinearColor(A, A, A, A);
				}
			}
		}
		else if (Key >= AtlasLipBase)
		{
			const float W = (Key - AtlasLipBase) / 4.0f;
			Image.Texels = PaintPremultiplied(ShapeSize, [W](float X, float Y) { return PaintLip(W, X, Y); });
		}
		else if (Key >= AtlasRimBase)
		{
			const float W = (Key - AtlasRimBase) / 4.0f;
			Image.Texels = PaintPremultiplied(ShapeSize, [W](float X, float Y) { return PaintRim(W, X, Y); });
		}
		else if (Key >= AtlasRingBase)
		{
			Shape(EShape::Ring, (Key - AtlasRingBase) / 4.0f);
		}
		return Image;
	}

	class FHUDAtlas
	{
	public:
		static constexpr int32 Slot = 96;
		static constexpr int32 PerRow = 16;
		static constexpr int32 Size = Slot * PerRow;
		static constexpr int32 Mips = 5;		// down to 4x4 for a 64 texel cell, as its own texture had

		/** The atlas UVs of a cell's content, made on first use; false when the atlas could not take it */
		bool Cell(int32 Key, FBox2f& OutUV)
		{
			if (CVarHUDAtlas.GetValueOnGameThread() == 0)
			{
				return false;
			}
			if (const FBox2f* Found = Cells.Find(Key))
			{
				OutUV = *Found;
				return true;
			}
			if (bFull || !Init())
			{
				return false;
			}
			const FAtlasImage Image = MakeAtlasImage(Key);
			const int32 Span = FMath::DivideAndRoundUp(Image.W + 32, Slot);
			if (Image.Texels.Num() != Image.W * Image.H || Image.H + 32 > Slot || Span > PerRow)
			{
				return false;
			}
			if (NextSlot % PerRow + Span > PerRow)
			{
				NextSlot += PerRow - NextSlot % PerRow;
			}
			if (NextSlot + Span > PerRow * PerRow)
			{
				bFull = true;
				return false;
			}
			const int32 SX = (NextSlot % PerRow) * Slot, SY = (NextSlot / PerRow) * Slot;
			NextSlot += Span;
			Write(Image, SX, SY, Span * Slot, Slot);
			const int32 OX = (Span * Slot - Image.W) / 2, OY = (Slot - Image.H) / 2;
			const FBox2f UV(FVector2f((float)(SX + OX) / Size, (float)(SY + OY) / Size), FVector2f((float)(SX + OX + Image.W) / Size, (float)(SY + OY + Image.H) / Size));
			Cells.Add(Key, UV);
			OutUV = UV;
			return true;
		}

		const FTexture* Resource() const
		{
			return Texture ? Texture->GetResource() : nullptr;
		}

	private:
		bool Init()
		{
			if (Texture)
			{
				return true;
			}
			const FName Name = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName(TEXT("TDHUDAtlas")));
			UTexture2D* New = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8, Name);
			if (!New)
			{
				bFull = true;
				return false;
			}
			New->SRGB = true;
			New->Filter = TF_Trilinear;
			New->AddressX = TA_Clamp;
			New->AddressY = TA_Clamp;
			New->NeverStream = true;
			FTexturePlatformData* Data = New->GetPlatformData();
			for (int32 Level = 0; Level < Mips; ++Level)
			{
				const int32 MipSize = Size >> Level;
				// the copies stay for the atlas's lifetime: region updates read them on the render thread
				MipData[Level].SetNumZeroed(MipSize * MipSize);
				FTexture2DMipMap* MipMap = Level == 0 ? &Data->Mips[0] : new FTexture2DMipMap(MipSize, MipSize, 1);
				if (Level > 0)
				{
					Data->Mips.Add(MipMap);
				}
				const int64 Bytes = (int64)MipData[Level].Num() * sizeof(FColor);
				MipMap->BulkData.Lock(LOCK_READ_WRITE);
				FMemory::Memcpy(MipMap->BulkData.Realloc(Bytes), MipData[Level].GetData(), Bytes);
				MipMap->BulkData.Unlock();
			}
			New->UpdateResource();
			New->AddToRoot();
			Texture = New;
			return true;
		}

		/** Pads the image to its slot (edges repeated, or wrapped sideways), box-filters it down the mips premultiplied, and uploads
		 *  the slot's region of every mip */
		void Write(const FAtlasImage& Image, int32 SX, int32 SY, int32 RW, int32 RH)
		{
			const int32 OX = (RW - Image.W) / 2, OY = (RH - Image.H) / 2;
			TArray<FLinearColor> Level;
			Level.SetNumUninitialized(RW * RH);
			for (int32 Y = 0; Y < RH; ++Y)
			{
				const int32 CY = FMath::Clamp(Y - OY, 0, Image.H - 1);
				for (int32 X = 0; X < RW; ++X)
				{
					const int32 CX = Image.bWrapX ? (((X - OX) % Image.W) + Image.W) % Image.W : FMath::Clamp(X - OX, 0, Image.W - 1);
					Level[Y * RW + X] = Image.Texels[CY * Image.W + CX];
				}
			}
			int32 W = RW, H = RH;
			for (int32 Mip = 0; Mip < Mips; ++Mip)
			{
				const int32 MipSize = Size >> Mip;
				const int32 DX = SX >> Mip, DY = SY >> Mip;
				for (int32 Y = 0; Y < H; ++Y)
				{
					for (int32 X = 0; X < W; ++X)
					{
						const FLinearColor& P = Level[Y * W + X];
						FColor& Out = MipData[Mip][(DY + Y) * MipSize + DX + X];
						if (P.A > 1e-4f)
						{
							Out = FLinearColor(P.R / P.A, P.G / P.A, P.B / P.A, 1.0f).GetClamped().ToFColor(true);
							Out.A = (uint8)FMath::RoundToInt(FMath::Clamp(P.A, 0.0f, 1.0f) * 255.0f);
						}
						else
						{
							Out = FColor(0, 0, 0, 0);
						}
					}
				}
				FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(DX, DY, DX, DY, W, H);
				Texture->UpdateTextureRegions(Mip, 1, Region, MipSize * sizeof(FColor), sizeof(FColor), (uint8*)MipData[Mip].GetData(),
					[](uint8*, const FUpdateTextureRegion2D* Regions) { delete Regions; });
				if (Mip + 1 < Mips)
				{
					const int32 NW = W / 2, NH = H / 2;
					TArray<FLinearColor> Smaller;
					Smaller.SetNumUninitialized(NW * NH);
					for (int32 Y = 0; Y < NH; ++Y)
					{
						for (int32 X = 0; X < NW; ++X)
						{
							Smaller[Y * NW + X] = (Level[(Y * 2) * W + X * 2] + Level[(Y * 2) * W + X * 2 + 1] + Level[(Y * 2 + 1) * W + X * 2] + Level[(Y * 2 + 1) * W + X * 2 + 1]) * 0.25f;
						}
					}
					Level = MoveTemp(Smaller);
					W = NW;
					H = NH;
				}
			}
		}

		UTexture2D* Texture = nullptr;
		TArray<FColor> MipData[Mips];
		TMap<int32, FBox2f> Cells;
		int32 NextSlot = 0;
		bool bFull = false;
	};

	FHUDAtlas& HUDAtlas()
	{
		static FHUDAtlas Atlas;
		return Atlas;
	}

	/** One quad from the atlas (corners clockwise from the top left), batched with the canvas's other atlas draws */
	void AtlasQuad(UCanvas* Canvas, const FVector2D (&P)[4], const FVector2f (&UV)[4], const FLinearColor& Color)
	{
		const FTexture* Resource = HUDAtlas().Resource();
		if (!Canvas || !Canvas->Canvas || !Resource)
		{
			return;
		}
		FCanvas* C = Canvas->Canvas;
		FBatchedElements* Batch = C->GetBatchedElements(FCanvas::ET_Triangle, nullptr, Resource, SE_BLEND_Translucent);
		const FHitProxyId HitProxy = C->GetHitProxyId();
		FLinearColor Tint = Color;
		Tint.A *= C->AlphaModulate;
		int32 V[4];
		for (int32 i = 0; i < 4; ++i)
		{
			V[i] = Batch->AddVertexf(FVector4f((float)P[i].X, (float)P[i].Y, 0.0f, 1.0f), UV[i], Tint, HitProxy);
		}
		Batch->AddTriangleExtensive(V[0], V[1], V[2], nullptr, Resource, SE_BLEND_Translucent);
		Batch->AddTriangleExtensive(V[0], V[2], V[3], nullptr, Resource, SE_BLEND_Translucent);
	}

	/** An axis-aligned rectangle of a cell: U0..U1 and V0..V1 in the cell's own 0-1 space */
	void AtlasRect(UCanvas* Canvas, const FBox2f& Cell, float X, float Y, float W, float H, float U0, float V0, float U1, float V1, const FLinearColor& Color)
	{
		const FVector2f CellSize = Cell.GetSize();
		const FVector2f A(Cell.Min.X + U0 * CellSize.X, Cell.Min.Y + V0 * CellSize.Y), B(Cell.Min.X + U1 * CellSize.X, Cell.Min.Y + V1 * CellSize.Y);
		const FVector2D P[4] = { FVector2D(X, Y), FVector2D(X + W, Y), FVector2D(X + W, Y + H), FVector2D(X, Y + H) };
		const FVector2f UV[4] = { A, FVector2f(B.X, A.Y), B, FVector2f(A.X, B.Y) };
		AtlasQuad(Canvas, P, UV, Color);
	}

	/** A whole cell over a rectangle turned AngleDeg about its centre, as a rotated canvas tile would draw it */
	void AtlasRotated(UCanvas* Canvas, const FBox2f& Cell, const FVector2D& Center, const FVector2D& Size, float AngleDeg, const FLinearColor& Color)
	{
		float Sin = 0.0f, Cos = 1.0f;
		FMath::SinCos(&Sin, &Cos, FMath::DegreesToRadians(AngleDeg));
		const FVector2D Half = Size * 0.5f;
		const FVector2D Corners[4] = { FVector2D(-Half.X, -Half.Y), FVector2D(Half.X, -Half.Y), FVector2D(Half.X, Half.Y), FVector2D(-Half.X, Half.Y) };
		FVector2D P[4];
		for (int32 i = 0; i < 4; ++i)
		{
			P[i] = Center + FVector2D(Corners[i].X * Cos - Corners[i].Y * Sin, Corners[i].X * Sin + Corners[i].Y * Cos);
		}
		const FVector2f UV[4] = { Cell.Min, FVector2f(Cell.Max.X, Cell.Min.Y), Cell.Max, FVector2f(Cell.Min.X, Cell.Max.Y) };
		AtlasQuad(Canvas, P, UV, Color);
	}

	/** A point inside the atlas's white cell, for flat colour */
	bool AtlasWhiteUV(FVector2f& OutUV)
	{
		FBox2f Cell;
		if (!HUDAtlas().Cell(AtlasWhite, Cell))
		{
			return false;
		}
		OutUV = Cell.GetCenter();
		return true;
	}

	/** Points flat-coloured triangles at the atlas's white cell, so they batch with the shapes; returns the texture to draw them with */
	const FTexture* WhiteTriangles(TArray<FCanvasUVTri>& Tris)
	{
		FVector2f UV;
		const FTexture* Resource = AtlasWhiteUV(UV) ? HUDAtlas().Resource() : nullptr;
		if (!Resource)
		{
			return GWhiteTexture;
		}
		for (FCanvasUVTri& Tri : Tris)
		{
			Tri.V0_UV = Tri.V1_UV = Tri.V2_UV = FVector2D(UV);
		}
		return Resource;
	}

	/** ATDHUD::NineSlice from an atlas cell; false (nothing drawn) when the atlas can't take the cell */
	bool NineSliceAtlas(UCanvas* Canvas, int32 Key, const FBox2D& Rect, float Corner, const FLinearColor& Color)
	{
		FBox2f Cell;
		if (!Canvas || !HUDAtlas().Cell(Key, Cell))
		{
			return false;
		}
		const FBox2D Box = Snap(Rect);
		const float MinX = (float)Box.Min.X;
		const float MinY = (float)Box.Min.Y;
		const float MaxX = (float)Box.Max.X;
		const float MaxY = (float)Box.Max.Y;
		const float SizeX = MaxX - MinX;
		const float SizeY = MaxY - MinY;
		if (SizeX < 1.0f || SizeY < 1.0f)
		{
			return true;
		}
		const float C = FMath::Max(0.0f, FMath::Min(FMath::RoundToFloat(Corner), FMath::FloorToFloat(FMath::Min(SizeX, SizeY) * 0.5f)));
		const float U = ShapeCorner / ShapeSize;
		const float Xs[4] = { MinX, MinX + C, MaxX - C, MaxX };
		const float Ys[4] = { MinY, MinY + C, MaxY - C, MaxY };
		const float Us[4] = { 0.0f, U, 1.0f - U, 1.0f };
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 Col = 0; Col < 3; ++Col)
			{
				const float W = Xs[Col + 1] - Xs[Col];
				const float H = Ys[Row + 1] - Ys[Row];
				if (W <= 0.0f || H <= 0.0f)
				{
					continue;
				}
				AtlasRect(Canvas, Cell, Xs[Col], Ys[Row], W, H, Us[Col], Us[Row], Us[Col + 1], Us[Row + 1], Color);
			}
		}
		return true;
	}

	/** Repeatable pseudo-random number in [0, 1) for decoration layout */
	float Hash01(int32 I, int32 K)
	{
		return FMath::Frac(FMath::Sin(I * 12.9898f + K * 78.233f) * 43758.5453f);
	}
}

double ATDHUD::Now() const
{
	return GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
}

// ---------------------------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------------------------

void ATDHUD::ShowToast(const FString& Message, const FLinearColor& Color)
{
	// refresh instead of stacking duplicates (e.g. clicking a spot that can't be built on again)
	for (FToast& Toast : Toasts)
	{
		if (Toast.Text == Message)
		{
			Toast.Start = Now();
			Toast.Color = Color;
			return;
		}
	}

	Toasts.Add({ Message, Color, Now() });
	while (Toasts.Num() > 3)
	{
		Toasts.RemoveAt(0);
	}
}

void ATDHUD::FlashLivesLost(int32 Amount)
{
	const bool bRecent = Now() - LivesFlashStart < 1.5;
	LivesFlashAmount = bRecent ? LivesFlashAmount + Amount : Amount;
	LivesFlashStart = Now();
}

void ATDHUD::AddFloatingText(const FVector& WorldLocation, const FString& InText, const FLinearColor& Color)
{
	// stack texts that pop up at nearly the same place and time
	const double Time = Now();
	int32 Nearby = 0;
	for (const FFloater& F : Floaters)
	{
		if (Time - F.Start < 0.5 && FVector::Dist2D(F.World, WorldLocation) < 90.0f)
		{
			++Nearby;
		}
	}
	Floaters.Add({ WorldLocation + FVector(0, 0, 32.0f * FMath::Min(Nearby, 4)), InText, Color, Time });
}

bool ATDHUD::IsOverUI(const FVector2D& ScreenPos) const
{
	for (const FBox2D& Rect : Panels)
	{
		if (Rect.IsInside(ScreenPos))
		{
			return true;
		}
	}
	return false;
}

bool ATDHUD::HandleClick(const FVector2D& ScreenPos)
{
	ATDPlayerController* PC = Cast<ATDPlayerController>(PlayerOwner);
	ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	const bool bGameOver = GameMode && GameMode->IsGameOver();
	const bool bMenu = GameMode && GameMode->GetMenu() != ETDMenu::None;
	// an armed Sell button disarms on any click that isn't on it
	const bool bSellArmed = PC && PC->IsSellConfirmOpen() && !bGameOver && !bMenu;

	// later buttons are drawn on top, so test them first
	for (int32 i = Buttons.Num() - 1; i >= 0; --i)
	{
		const FButton& Btn = Buttons[i];
		if (!Btn.Rect.IsInside(ScreenPos))
		{
			continue;
		}
		// menu and dialog buttons (Lock, auto-salvage and the form buttons sit after them in the list but belong to the board UI)
		const bool bBoardButton = Btn.Id == EButton::ItemLock || Btn.Id == EButton::ChooseForm || Btn.Id == EButton::CycleAutoSalvage || Btn.Id == EButton::TargetCycle;
		const bool bMenuButton = (Btn.Id >= EButton::MenuNewGame && !bBoardButton) || Btn.Id == EButton::Restart;
		// an open question only takes its own Yes / No
		if (GameMode && GameMode->GetPendingConfirm() != ETDConfirm::None && Btn.Id != EButton::ConfirmYes && Btn.Id != EButton::ConfirmNo)
		{
			return true;
		}
		if (bSellArmed && Btn.Id != EButton::Sell)
		{
			PC->CancelSell();
		}
		if (!PC || !GameMode || !Btn.bEnabled || ((bGameOver || bMenu) && !bMenuButton))
		{
			return true;
		}

		if (Btn.Id != EButton::StorageItem && Btn.Id != EButton::SettingSlider)
		{
			TDAudio::Play(this, ETDSound::Click, 0.6f);
		}

		switch (Btn.Id)
		{
		case EButton::Speed:			PC->CycleSpeed(); break;
		case EButton::Pause:			PC->TogglePause(); break;
		case EButton::Wave:				PC->CallWave(); break;
		case EButton::Slot:				PC->SelectTowerType((ETDTowerType)Btn.Param); break;
		case EButton::Sell:				PC->SellSelected(true); break;
		case EButton::Restart:			GameMode->OpenNewGameSetup(); break;
		case EButton::Tab:				PC->SetPanelTab(Btn.Param); break;
		case EButton::LearnSkill:		PC->LearnSkill(Btn.Param); break;
		case EButton::EquippedItem:		PC->UnequipSlot((ETDItemSlot)Btn.Param); break;
		case EButton::StorageItem:		PC->EquipStorageItem(Btn.Param); break;
		case EButton::StorageToggle:	PC->ToggleStorage(); break;
		case EButton::Sort:				PC->SortStorage(); break;
		case EButton::ItemEquip:		PC->EquipWorkbenchItem(); break;
		case EButton::ItemUpgrade:		PC->UpgradeWorkbenchItem(); break;
		case EButton::ItemReroll:		PC->RerollWorkbenchItem(); break;
		case EButton::ItemSalvage:		PC->SalvageWorkbenchItem(); break;
		case EButton::MenuNewGame:		GameMode->OpenNewGameSetup(); break;
		case EButton::SetupMap:			GameMode->SetSetupMap(Btn.Param); break;
		case EButton::SetupTier:		GameMode->SetSetupTier(Btn.Param); break;
		case EButton::SetupMutator:		GameMode->ToggleSetupMutator(Btn.Param); break;
		case EButton::SetupStart:		GameMode->RequestStartSetupGame(); break;
		case EButton::SetupSlot:		GameMode->SetSetupSlot(Btn.Param); break;
		case EButton::SlotLoad:			GameMode->LoadGame(Btn.Param); break;
		case EButton::SlotDelete:		GameMode->RequestDeleteSlot(Btn.Param); break;
		case EButton::SlotBack:			GameMode->CloseSlotPicker(); break;
		case EButton::ConfirmYes:		GameMode->AnswerConfirm(true); break;
		case EButton::ConfirmNo:		GameMode->AnswerConfirm(false); break;
		case EButton::Retry:			GameMode->RequestRetry(); break;
		case EButton::TargetCycle:		PC->CycleTargeting(Btn.Param); break;
		case EButton::SetupBack:		GameMode->CloseNewGameSetup(); break;
		case EButton::PickBoon:			GameMode->PickBoon(Btn.Param); break;
		case EButton::MenuMastery:		GameMode->OpenMenu(ETDMenu::Mastery); break;
		case EButton::MasteryBack:		GameMode->OpenMenu(ETDMenu::Main); break;
		// Param 0: the slot picker (Continue, Load); 1: the run's own slot at once (game over)
		case EButton::MenuLoad:			if (Btn.Param == 1) { GameMode->LoadGame(); } else { GameMode->OpenSlotPicker(); } break;
		case EButton::MenuSave:			GameMode->SaveGame(); break;
		case EButton::MenuSettings:		GameMode->OpenSettings(); break;
		case EButton::MenuQuit:			GameMode->QuitGame(); break;
		case EButton::MenuResume:		GameMode->CloseMenu(); break;
		case EButton::MenuMainMenu:		GameMode->ReturnToMainMenu(); break;
		case EButton::MenuEndless:		GameMode->ContinueEndless(); break;
		case EButton::ItemLock:			PC->ToggleLockWorkbenchItem(); break;
		case EButton::CycleAutoSalvage:	GameMode->CycleAutoSalvage(); break;
		case EButton::ChooseForm:		PC->ClickForm(Btn.Param); break;
		case EButton::SettingsBack:		GameMode->CloseSettings(); break;
		case EButton::ToggleShowFps:	GameMode->SetShowFps(!GameMode->IsShowingFps()); break;
		case EButton::ToggleVSync:		GameMode->SetVSync(!GameMode->IsVSyncEnabled()); break;
		case EButton::ToggleAA:			GameMode->SetSharpAA(!GameMode->IsSharpAA()); break;
		case EButton::ToggleHDIcons:	GameMode->SetHDIcons(!GameMode->IsHDIcons()); break;
		case EButton::ToggleHDInterface:	GameMode->SetHDInterface(!GameMode->IsHDInterface()); break;

		case EButton::MenuControls:		GameMode->OpenControls(); break;
		case EButton::BindKey:			GameMode->StartRebinding(Btn.Param); break;
		case EButton::ResetKeys:		GameMode->ResetKeys(); break;
		case EButton::ControlsBack:		GameMode->CloseControls(); break;
		case EButton::SettingAdjust:
		{
			const float Delta = (Btn.Param % 2 == 0) ? -10.0f : 10.0f;
			switch (Btn.Param / 2)
			{
			case 1:	GameMode->SetUIScalePercent(GameMode->GetUIScalePercent() + Delta * 0.5f); break;
			case 2:	GameMode->SetFrameRateStep(GameMode->GetFrameRateStep() + (Delta > 0.0f ? 1 : -1)); break;
			case 3:	GameMode->SetSfxVolume(GameMode->GetSfxVolume() + Delta); break;
			case 4:	GameMode->SetMusicVolume(GameMode->GetMusicVolume() + Delta); break;
			case 5:	GameMode->SetGraphicsQuality(GameMode->GetGraphicsQuality() + (Delta > 0.0f ? 1 : -1)); break;
			default: break;
			}
			break;
		}
		case EButton::SettingSlider:
			// remember the bar: hit boxes are rebuilt every frame, the drag outlives them
			ActiveSlider = Btn.Param;
			ActiveSliderRect = Btn.Rect;
			DragSlider(GameMode, ScreenPos);
			break;
		}
		return true;
	}

	if (bSellArmed)
	{
		PC->CancelSell();
	}
	return bGameOver || bMenu || IsOverUI(ScreenPos);
}

void ATDHUD::DragSlider(ATDGameMode* GameMode, const FVector2D& Position)
{
	if (ActiveSlider < 0 || !ActiveSliderRect.bIsValid)
	{
		return;
	}
	const float Fraction = FMath::Clamp((Position.X - ActiveSliderRect.Min.X) / FMath::Max(1.0f, ActiveSliderRect.GetSize().X), 0.0f, 1.0f);
	switch (ActiveSlider)
	{
	case 1:	GameMode->SetUIScalePercent(FMath::Lerp(80.0f, 150.0f, Fraction)); break;
	case 3:	GameMode->SetSfxVolume(Fraction * 100.0f); break;
	case 4:	GameMode->SetMusicVolume(Fraction * 100.0f); break;
	case 5:
	{
		const int32 Level = FMath::RoundToInt(Fraction * (ATDGameMode::GraphicsQualityLevels - 1));
		if (Level != GameMode->GetGraphicsQuality())
		{
			GameMode->SetGraphicsQuality(Level);
		}
		break;
	}
	case 2:
	{
		const int32 Step = FMath::RoundToInt(Fraction * (TD::FrameRateStepCount - 1));
		if (Step != GameMode->GetFrameRateStep())
		{
			GameMode->SetFrameRateStep(Step);
		}
		break;
	}
	default: break;
	}
}

bool ATDHUD::FindButton(EButton Id, int32 Param, FVector2D& OutCenter) const
{
	for (const FButton& Btn : Buttons)
	{
		if (Btn.Id == Id && Btn.Param == Param)
		{
			OutCenter = Btn.Rect.GetCenter();
			return true;
		}
	}
	return false;
}

bool ATDHUD::HandleRightClick(const FVector2D& ScreenPos)
{
	ATDPlayerController* PC = Cast<ATDPlayerController>(PlayerOwner);
	for (int32 i = Buttons.Num() - 1; i >= 0; --i)
	{
		const FButton& Btn = Buttons[i];
		if (Btn.Id == EButton::StorageItem && Btn.bEnabled && Btn.Rect.IsInside(ScreenPos))
		{
			if (PC)
			{
				PC->SalvageStorageItem(Btn.Param);
			}
			return true;
		}
	}
	return IsOverUI(ScreenPos);
}

// ---------------------------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------------------------

void ATDHUD::Fill(const FBox2D& Rect, const FLinearColor& Color)
{
	const FBox2D Box = Snap(Rect);
	FBox2f White;
	if (Canvas && HUDAtlas().Cell(AtlasWhite, White))
	{
		// the middle of the white cell: flat colour, batched with the shapes
		const FVector2f Mid = White.GetCenter();
		const FVector2D P[4] = { Box.Min, FVector2D(Box.Max.X, Box.Min.Y), Box.Max, FVector2D(Box.Min.X, Box.Max.Y) };
		const FVector2f UV[4] = { Mid, Mid, Mid, Mid };
		AtlasQuad(Canvas, P, UV, Color);
		return;
	}
	DrawRect(Color, Box.Min.X, Box.Min.Y, Box.GetSize().X, Box.GetSize().Y);
}

void ATDHUD::NineSlice(UTexture2D* Texture, const FBox2D& Rect, float Corner, const FLinearColor& Color)
{
	if (!Texture || !Canvas)
	{
		return;
	}
	const FBox2D Box = Snap(Rect);
	const float MinX = (float)Box.Min.X;
	const float MinY = (float)Box.Min.Y;
	const float MaxX = (float)Box.Max.X;
	const float MaxY = (float)Box.Max.Y;
	const float SizeX = MaxX - MinX;
	const float SizeY = MaxY - MinY;
	if (SizeX < 1.0f || SizeY < 1.0f)
	{
		return;
	}
	const float C = FMath::Max(0.0f, FMath::Min(FMath::RoundToFloat(Corner), FMath::FloorToFloat(FMath::Min(SizeX, SizeY) * 0.5f)));
	const float U = ShapeCorner / ShapeSize;
	const float Xs[4] = { MinX, MinX + C, MaxX - C, MaxX };
	const float Ys[4] = { MinY, MinY + C, MaxY - C, MaxY };
	const float Us[4] = { 0.0f, U, 1.0f - U, 1.0f };
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Col = 0; Col < 3; ++Col)
		{
			const float W = Xs[Col + 1] - Xs[Col];
			const float H = Ys[Row + 1] - Ys[Row];
			if (W <= 0.0f || H <= 0.0f)
			{
				continue;
			}
			DrawTexture(Texture, Xs[Col], Ys[Row], W, H, Us[Col], Us[Row], Us[Col + 1] - Us[Col], Us[Row + 1] - Us[Row], Color, BLEND_Translucent);
		}
	}
}

void ATDHUD::RoundFill(const FBox2D& Rect, const FLinearColor& Color, float Radius)
{
	if (!NineSliceAtlas(Canvas, AtlasFill, Rect, Radius, Color))
	{
		NineSlice(ShapeTexture(EShape::Fill), Rect, Radius, Color);
	}
}

void ATDHUD::RoundFrame(const FBox2D& Rect, const FLinearColor& Color, float Radius, float Thickness)
{
	const FVector2D Size = Snap(Rect).GetSize();
	const float Shorter = (float)FMath::Min(Size.X, Size.Y);
	const float C = FMath::Max(1.0f, FMath::Min(FMath::RoundToFloat(Radius), FMath::FloorToFloat(Shorter * 0.5f)));
	const float T = FMath::Max(1.0f, FMath::RoundToFloat(Thickness));
	// the outline width in texels that lands on T pixels at this corner size
	if (!NineSliceAtlas(Canvas, AtlasRingBase + RingWidthKey(T * ShapeCorner / C), Rect, C, Color))
	{
		NineSlice(ShapeTexture(EShape::Ring, T * ShapeCorner / C), Rect, C, Color);
	}
}

void ATDHUD::NacreRim(const FBox2D& Rect, const FLinearColor& Edge, float Radius, float Width, float Alpha)
{
	const FVector2D Size = Snap(Rect).GetSize();
	const float C = FMath::Max(1.0f, FMath::Min(FMath::RoundToFloat(Radius), FMath::FloorToFloat((float)FMath::Min(Size.X, Size.Y) * 0.5f)));
	// a faint dark line just outside, so the rim stands off whatever is behind the panel
	RoundFrame(Rect.ExpandBy(FMath::Max(1.0f, UIScale)), FLinearColor(0.0f, 0.0f, 0.0f, 0.3f * Alpha), C + UIScale, FMath::Max(1.0f, UIScale));
	const FLinearColor Tint = FMath::Lerp(FLinearColor(Edge.R, Edge.G, Edge.B, 1.0f), FLinearColor::White, 0.42f);
	if (!NineSliceAtlas(Canvas, AtlasRimBase + RimWidthKey(Width * ShapeCorner / C), Rect, C, FLinearColor(Tint.R, Tint.G, Tint.B, Alpha)))
	{
		NineSlice(RimTexture(Width * ShapeCorner / C), Rect, C, FLinearColor(Tint.R, Tint.G, Tint.B, Alpha));
	}
}

void ATDHUD::DetailedCardEdge(const FBox2D& Rect, const FLinearColor& Edge, float Radius, float Alpha)
{
	InnerLip(Rect, Radius, 3.0f * UIScale, 0.3f * Alpha);
	NacreRim(Rect, Edge, Radius, 1.8f * UIScale, Alpha);
}

void ATDHUD::InnerLip(const FBox2D& Rect, float Radius, float Width, float Strength)
{
	const FVector2D Size = Snap(Rect).GetSize();
	const float C = FMath::Max(1.0f, FMath::Min(FMath::RoundToFloat(Radius), FMath::FloorToFloat((float)FMath::Min(Size.X, Size.Y) * 0.5f)));
	if (!NineSliceAtlas(Canvas, AtlasLipBase + LipWidthKey(Width * ShapeCorner / C), Rect, C, FLinearColor(1.0f, 1.0f, 1.0f, Strength)))
	{
		NineSlice(LipTexture(Width * ShapeCorner / C), Rect, C, FLinearColor(1.0f, 1.0f, 1.0f, Strength));
	}
}

void ATDHUD::Pearl(const FVector2D& Center, float Radius, const FLinearColor& Tint)
{
	if (!Canvas || Radius < 0.5f)
	{
		return;
	}
	FBox2f Cell;
	if (HUDAtlas().Cell(AtlasPearl, Cell))
	{
		AtlasRect(Canvas, Cell, Center.X - Radius, Center.Y - Radius, Radius * 2.0f, Radius * 2.0f, 0.0f, 0.0f, 1.0f, 1.0f, Tint);
		return;
	}
	UTexture2D* Texture = PearlTexture();
	if (!Texture)
	{
		return;
	}
	DrawTexture(Texture, Center.X - Radius, Center.Y - Radius, Radius * 2.0f, Radius * 2.0f, 0.0f, 0.0f, 1.0f, 1.0f, Tint, BLEND_Translucent);
}

void ATDHUD::Glow(const FBox2D& Rect, const FLinearColor& Color, float Radius, float Spread)
{
	const FBox2D Outer = Rect.ExpandBy(Spread);
	if (!NineSliceAtlas(Canvas, AtlasGlow, Outer, FMath::Max(Spread * 2.0f, Radius + Spread), Color))
	{
		NineSlice(ShapeTexture(EShape::Glow), Outer, FMath::Max(Spread * 2.0f, Radius + Spread), Color);
	}
}

void ATDHUD::Panel(const FBox2D& Rect, const FLinearColor& Color, bool bBlocksMouse)
{
	// full-screen overlays stay square, everything else is a rounded glass panel with a soft shadow
	const bool bFullScreen = Canvas && Rect.GetSize().X >= Canvas->ClipX - 1.0f && Rect.GetSize().Y >= Canvas->ClipY - 1.0f;
	if (bFullScreen)
	{
		Fill(Rect, Color);
	}
	else
	{
		const float R = RadiusPanel * UIScale;
		Glow(FBox2D(Rect.Min + FVector2D(0, 6.0f * UIScale), Rect.Max + FVector2D(0, 6.0f * UIScale)), ShadowColor, R, 14.0f * UIScale);
		RoundFill(Rect, Color, R);
		// faint sheen on the upper part, like light from the surface; its lower edge ripples, drifting very slowly
		const FLinearColor SheenColor(0.55f, 0.9f, 1.0f, 0.035f);
		const float SheenH = FMath::Min(Rect.GetSize().Y, 46.0f * UIScale);
		RoundFill(FBox2D(Rect.Min, FVector2D(Rect.Max.X, Rect.Min.Y + SheenH)), SheenColor, R);
		const float RippleH = 6.0f * UIScale;
		if (Rect.GetSize().Y >= SheenH + RippleH)
		{
			// under the straight part of the sheen's bottom edge, so the two meet without overlapping
			const FBox2D Sheen = Snap(FBox2D(Rect.Min, FVector2D(Rect.Max.X, Rect.Min.Y + SheenH)));
			const float C = FMath::Min(FMath::RoundToFloat(R), FMath::FloorToFloat((float)FMath::Min(Sheen.GetSize().X, Sheen.GetSize().Y) * 0.5f));
			WaveStrip((float)Sheen.Min.X + C, (float)Sheen.Max.X - C, (float)Sheen.Max.Y, RippleH, true, SheenColor, (float)FMath::Fmod(Now() * 0.015, 1.0));
		}
		if (DetailedUI())
		{
			// the glass's lit inner lip and a thin nacre rim (framed panels draw a slightly wider one over it)
			InnerLip(Rect, R, 3.5f * UIScale, 0.35f);
			NacreRim(Rect, PanelEdge, R, 1.8f * UIScale);
		}
	}
	if (bBlocksMouse)
	{
		Panels.Add(Rect);
	}
}

void ATDHUD::FramedPanel(const FBox2D& Rect, const FLinearColor& Edge)
{
	const float S = UIScale;
	const float R = RadiusPanel * S;
	Panel(Rect, PanelColor);
	// the panel's own colour washes faintly up from the bottom (no accent bar on top: the user removed it)
	RoundFill(FBox2D(FVector2D(Rect.Min.X, Rect.Max.Y - FMath::Min((float)Rect.GetSize().Y, 110.0f * S)), Rect.Max), WithAlpha(Edge, 0.1f), R);
	if (DetailedUI())
	{
		// a nacre rim and light caught just inside the edge (no corner pearls: the user removed them)
		NacreRim(Rect, Edge, R, 2.4f * S);
		RoundFrame(Rect.ExpandBy(-3.0f * S), WithAlpha(FMath::Lerp(Edge, FLinearColor::White, 0.3f), 0.06f), FMath::Max(1.0f, R - 3.0f * S), 6.0f * S);
	}
	else
	{
		RoundFrame(Rect, WithAlpha(Edge, FMath::Max(0.7f, FMath::Min(Edge.A, 0.85f))), R, 2.0f * S);
	}
	PanelDecor(Rect, Edge);
}

void ATDHUD::WaveStrip(float X0, float X1, float Y, float Height, bool bFilled, const FLinearColor& Color, float Phase)
{
	if (!Canvas || X1 - X0 < 1.0f || Height < 1.0f)
	{
		return;
	}
	// texels stay square, so the wave keeps its shape at every interface size
	const float TexelPx = Height / WaveH;
	const float Width = X1 - X0;
	FBox2f Cell;
	if (HUDAtlas().Cell(bFilled ? AtlasWaveFilled : AtlasWaveLine, Cell))
	{
		// the atlas can't wrap, so one quad per period: each runs from U to the end of its period (or the strip)
		const float PeriodPx = TexelPx * WaveW;
		float X = X0;
		float U = FMath::Frac(Phase);
		while (X1 - X > 0.01f)
		{
			const float Px = FMath::Min((1.0f - U) * PeriodPx, X1 - X);
			const float U1 = U + Px / PeriodPx;
			AtlasRect(Canvas, Cell, X, Y, Px, Height, U, 0.0f, U1, 1.0f, Color);
			X += Px;
			U = 0.0f;
		}
		return;
	}
	UTexture2D* Texture = WaveTexture(bFilled);
	if (!Texture)
	{
		return;
	}
	DrawTexture(Texture, X0, Y, Width, Height, Phase, 0.0f, Width / (TexelPx * WaveW), 1.0f, Color, BLEND_Translucent);
}

void ATDHUD::Blob(const FVector2D& Center, const FVector2D& Size, float AngleDeg, const FLinearColor& Color, bool bSoft)
{
	if (!Canvas || Size.X < 0.5f || Size.Y < 0.5f)
	{
		return;
	}
	FBox2f Cell;
	if (HUDAtlas().Cell(bSoft ? AtlasGlow : AtlasFill, Cell))
	{
		// turned by hand rather than by the canvas, whose transform would start a new draw for every turned blob
		if (AngleDeg == 0.0f)
		{
			AtlasRect(Canvas, Cell, Center.X - Size.X * 0.5f, Center.Y - Size.Y * 0.5f, Size.X, Size.Y, 0.0f, 0.0f, 1.0f, 1.0f, Color);
		}
		else
		{
			AtlasRotated(Canvas, Cell, Center, Size, AngleDeg, Color);
		}
		return;
	}
	UTexture2D* Texture = ShapeTexture(bSoft ? EShape::Glow : EShape::Fill);
	if (!Texture)
	{
		return;
	}
	DrawTexture(Texture, Center.X - Size.X * 0.5f, Center.Y - Size.Y * 0.5f, Size.X, Size.Y, 0.0f, 0.0f, 1.0f, 1.0f, Color, BLEND_Translucent,
		1.0f, false, AngleDeg, FVector2D(0.5f, 0.5f));
}

void ATDHUD::Bubble(const FVector2D& Center, float Radius, const FLinearColor& Tint, float Alpha)
{
	if (Radius < 0.5f || Alpha <= 0.0f)
	{
		return;
	}
	if (DetailedUI())
	{
		FBox2f Cell;
		if (Canvas && HUDAtlas().Cell(AtlasBubble, Cell))
		{
			const FLinearColor Color = FMath::Lerp(FLinearColor::White, FLinearColor(Tint.R, Tint.G, Tint.B, 1.0f), 0.25f);
			AtlasRect(Canvas, Cell, Center.X - Radius, Center.Y - Radius, Radius * 2.0f, Radius * 2.0f, 0.0f, 0.0f, 1.0f, 1.0f, WithAlpha(Color, FMath::Min(Alpha, 1.0f)));
			return;
		}
		if (UTexture2D* Texture = BubbleTexture())
		{
			// the painted bubble, tinted only slightly toward the caller's colour
			const FLinearColor Color = FMath::Lerp(FLinearColor::White, FLinearColor(Tint.R, Tint.G, Tint.B, 1.0f), 0.25f);
			DrawTexture(Texture, Center.X - Radius, Center.Y - Radius, Radius * 2.0f, Radius * 2.0f, 0.0f, 0.0f, 1.0f, 1.0f, WithAlpha(Color, FMath::Min(Alpha, 1.0f)), BLEND_Translucent);
			return;
		}
	}
	const FBox2D Box(Center - FVector2D(Radius, Radius), Center + FVector2D(Radius, Radius));
	RoundFill(Box, WithAlpha(Tint, FMath::Min(0.1f * Alpha, 0.2f)), Radius);
	RoundFrame(Box, WithAlpha(Tint, FMath::Min(0.45f * Alpha, 0.85f)), Radius, (Radius > 3.0f * UIScale ? 1.5f : 1.0f) * UIScale);
	if (Radius > 2.5f * UIScale)
	{
		Blob(Center + FVector2D(-0.35f, -0.35f) * Radius, FVector2D(Radius * 0.35f, Radius * 0.35f), 0.0f, FLinearColor(1.0f, 1.0f, 1.0f, FMath::Min(0.5f * Alpha, 0.7f)));
	}
}

void ATDHUD::Starfish(const FVector2D& Center, float Radius, float AngleDeg, const FLinearColor& Color)
{
	if (DetailedUI())
	{
		if (UTexture2D* Texture = TDIcons::Decor(ETDDecorIcon::Starfish))
		{
			// the painted sea star fills about 0.86 of its tile
			const float Size = Radius * 2.3f;
			DrawTexture(Texture, Center.X - Size * 0.5f, Center.Y - Size * 0.5f, Size, Size, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor(1.0f, 1.0f, 1.0f, Color.A), BLEND_Translucent,
				1.0f, false, AngleDeg, FVector2D(0.5f, 0.5f));
			return;
		}
	}
	for (int32 i = 0; i < 5; ++i)
	{
		const float Deg = AngleDeg + 72.0f * i;
		const FVector2D Dir(FMath::Sin(FMath::DegreesToRadians(Deg)), -FMath::Cos(FMath::DegreesToRadians(Deg)));
		Blob(Center + Dir * Radius * 0.5f, FVector2D(Radius * 0.42f, Radius), Deg, Color);
	}
	Blob(Center, FVector2D(Radius * 0.6f, Radius * 0.6f), 0.0f, Color);
}

void ATDHUD::Scallop(const FVector2D& Center, float Radius, const FLinearColor& Color)
{
	if (DetailedUI())
	{
		if (UTexture2D* Texture = TDIcons::Decor(ETDDecorIcon::Scallop))
		{
			// the painted shell is wider than tall and fills its tile's width; centred where the plain one's ribs are
			const float Size = Radius * 2.0f;
			const FVector2D Mid = Center - FVector2D(0.0f, Radius * 0.42f);
			DrawTexture(Texture, Mid.X - Size * 0.5f, Mid.Y - Size * 0.5f, Size, Size, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor(1.0f, 1.0f, 1.0f, Color.A), BLEND_Translucent);
			return;
		}
	}
	// ribs fanning up from the hinge, a faint body behind them and the hinge's little ears
	Blob(Center - FVector2D(0.0f, Radius * 0.5f), FVector2D(Radius * 1.7f, Radius * 1.1f), 0.0f, WithAlpha(Color, Color.A * 0.35f));
	for (int32 i = 0; i < 5; ++i)
	{
		const float Deg = -52.0f + 26.0f * i;
		const FVector2D Dir(FMath::Sin(FMath::DegreesToRadians(Deg)), -FMath::Cos(FMath::DegreesToRadians(Deg)));
		Blob(Center + Dir * Radius * 0.52f, FVector2D(Radius * 0.26f, Radius * 1.04f), Deg, Color);
	}
	Blob(Center + FVector2D(0.0f, Radius * 0.08f), FVector2D(Radius * 0.7f, Radius * 0.28f), 0.0f, Color);
}

void ATDHUD::CoralSprig(const FVector2D& Base, float Height, const FLinearColor& Color)
{
	if (DetailedUI())
	{
		if (UTexture2D* Texture = TDIcons::Decor(ETDDecorIcon::Coral))
		{
			// the painted sprig is taller than wide, so its height spans 0.86 of the tile and its foot sits at 0.93
			const float Size = Height * 1.25f / 0.86f;
			DrawTexture(Texture, Base.X - Size * 0.5f, Base.Y - Size * 0.93f, Size, Size, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor(1.0f, 1.0f, 1.0f, Color.A), BLEND_Translucent);
			return;
		}
	}
	// a stem with two side branches, each ending in a round tip
	const float Thick = FMath::Max(1.5f, Height * 0.16f);
	auto Branch = [this, Thick, &Color](const FVector2D& From, float Deg, float Len)
	{
		const FVector2D Dir(FMath::Sin(FMath::DegreesToRadians(Deg)), -FMath::Cos(FMath::DegreesToRadians(Deg)));
		Blob(From + Dir * Len * 0.5f, FVector2D(Thick, Len), Deg, Color);
		Blob(From + Dir * Len, FVector2D(Thick * 1.5f, Thick * 1.5f), 0.0f, Color);
		return From + Dir * Len;
	};
	Branch(Base, 0.0f, Height);
	Branch(Base + FVector2D(0.0f, -Height * 0.35f), -42.0f, Height * 0.5f);
	Branch(Base + FVector2D(0.0f, -Height * 0.55f), 38.0f, Height * 0.42f);
}

void ATDHUD::DrawAmbient()
{
	if (!Canvas)
	{
		return;
	}
	// under every panel and never registered for input; it all drifts on periods of tens of seconds
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float S = UIScale;
	const double T = Now();

	// deep water closing in at the edges
	const FLinearColor Deep(0.0f, 0.03f, 0.06f, 0.07f);
	Blob(FVector2D(W * 0.5f, -H * 0.02f), FVector2D(W * 1.5f, H * 0.3f), 0.0f, Deep, true);
	Blob(FVector2D(W * 0.5f, H * 1.02f), FVector2D(W * 1.5f, H * 0.34f), 0.0f, Deep, true);
	Blob(FVector2D(-W * 0.02f, H * 0.5f), FVector2D(W * 0.2f, H * 1.5f), 0.0f, Deep, true);
	Blob(FVector2D(W * 1.02f, H * 0.5f), FVector2D(W * 0.2f, H * 1.5f), 0.0f, Deep, true);

	// caustic light near the surface: a row of soft, slowly drifting and breathing streaks
	for (int32 i = 0; i < 7; ++i)
	{
		const float Seed = i * 2.13f;
		const float X = W * (i + 0.5f) / 7.0f + (float)FMath::Sin(T * 0.045 + Seed) * 70.0f * S;
		const float Y = (34.0f + 30.0f * Hash01(i, 11)) * S + (float)FMath::Sin(T * 0.06 + Seed * 1.7f) * 10.0f * S;
		const float Pulse = 0.5f + 0.5f * (float)FMath::Sin(T * 0.21 + Seed * 3.0f);
		const float Angle = -8.0f + 16.0f * Hash01(i, 12) + (float)FMath::Sin(T * 0.05 + Seed) * 4.0f;
		Blob(FVector2D(X, Y), FVector2D((200.0f + 120.0f * Hash01(i, 13)) * S, (26.0f + 18.0f * Hash01(i, 14)) * S), Angle,
			FLinearColor(0.55f, 0.92f, 1.0f, 0.025f + 0.03f * Pulse), true);
	}
}

float ATDHUD::WrappedColoredText(const TArray<TPair<FString, FLinearColor>>& Parts, float X, float Y, float W, float Size, float LineH, bool bDraw)
{
	// split every piece into words that keep their trailing spaces, then lay them out line by line
	struct FToken { FString Text; FString Word; FLinearColor Color; };
	TArray<FToken> Tokens;
	for (const TPair<FString, FLinearColor>& Part : Parts)
	{
		const FString& Str = Part.Key;
		int32 i = 0;
		while (i < Str.Len())
		{
			int32 j = i;
			while (j < Str.Len() && Str[j] != TCHAR(' '))
			{
				++j;
			}
			int32 k = j;
			while (k < Str.Len() && Str[k] == TCHAR(' '))
			{
				++k;
			}
			Tokens.Add({ Str.Mid(i, k - i), Str.Mid(i, j - i), Part.Value });
			i = k;
		}
	}
	if (Tokens.Num() == 0)
	{
		return 0.0f;
	}
	float PX = X;
	float LineY = Y;
	int32 Lines = 1;
	for (const FToken& Token : Tokens)
	{
		const float WordW = MeasureText(Token.Word, Size).X;
		if (PX > X && PX - X + WordW > W)
		{
			PX = X;
			LineY += LineH;
			++Lines;
		}
		if (PX == X && Token.Word.IsEmpty())
		{
			continue;	// no spaces at the start of a line
		}
		if (bDraw)
		{
			Text(Token.Text, PX, LineY, Token.Color, Size);
		}
		PX += MeasureText(Token.Text, Size).X;
	}
	return Lines * LineH;
}

float ATDHUD::WrappedText(const FString& InText, float X, float Y, float W, const FLinearColor& Color, float Size, float LineH, bool bDraw)
{
	return WrappedColoredText({ { InText, Color } }, X, Y, W, Size, LineH, bDraw);
}

float ATDHUD::WrappedTextCentered(const FString& InText, float CX, float Y, float W, const FLinearColor& Color, float Size, float LineH, bool bDraw)
{
	TArray<FString> Words;
	InText.ParseIntoArray(Words, TEXT(" "), true);
	TArray<FString> Lines;
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		if (!Line.IsEmpty() && MeasureText(Candidate, Size).X > W)
		{
			Lines.Add(Line);
			Line = Word;
		}
		else
		{
			Line = Candidate;
		}
	}
	if (!Line.IsEmpty())
	{
		Lines.Add(Line);
	}
	if (bDraw)
	{
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			Text(Lines[i], CX, Y + i * LineH, Color, Size, true, false);
		}
	}
	return Lines.Num() * LineH;
}

void ATDHUD::PanelDecor(const FBox2D& Rect, const FLinearColor& Tint)
{
	// short panels (the top clusters, the next-wave card) have no spare padding for it
	const float S = UIScale;
	const float Height = (float)Rect.GetSize().Y;
	if (Height < 100.0f * S)
	{
		return;
	}
	const float R = RadiusPanel * S;

	// a faint wave in the bottom padding, drifting so slowly it only shows when you look for it; it stops short of the
	// bottom-right corner, where a few bubbles rise from its end
	const bool bBubbles = Height >= 150.0f * S;
	const bool bCorals = Height >= 260.0f * S && Rect.GetSize().X >= 240.0f * S;
	const bool bDetailed = DetailedUI();
	{
		// the detailed look draws a finer line with a pale crest above it
		const float StripH = (bDetailed ? 8.0f : 13.0f) * S;
		const float WaveX0 = Rect.Min.X + R + (bCorals ? (bDetailed ? 36.0f : 24.0f) * S : 0.0f);
		const float WaveX1 = Rect.Max.X - R - (bBubbles ? 30.0f * S : 0.0f);
		const float Drift = (float)FMath::Fmod(Now() * 0.02, 1.0);
		WaveStrip(WaveX0, WaveX1, Rect.Max.Y - (bDetailed ? 12.0f : 15.0f) * S, StripH, false, WithAlpha(Tint, bDetailed ? 0.32f : 0.3f), Drift);
		if (bDetailed)
		{
			WaveStrip(WaveX0, WaveX1, Rect.Max.Y - 12.0f * S - FMath::Max(1.0f, S), StripH, false, FLinearColor(0.85f, 1.0f, 1.0f, 0.12f), Drift);
		}
	}

	// a starfish resting on the bottom-left corner of tall panels and a scallop beside it on the rim, both straddling
	// the frame so they never cover the panel's content
	if (bCorals)
	{
		const float Sway = (float)FMath::Sin(Now() * 0.22 + Rect.Min.X * 0.01) * 5.0f;
		Starfish(FVector2D(Rect.Min.X + 5.0f * S, Rect.Max.Y - 3.0f * S), 9.0f * S, -14.0f + Sway, FLinearColor(1.0f, 0.5f, 0.34f, 0.92f));
		Scallop(FVector2D(Rect.Min.X + 22.0f * S, Rect.Max.Y + 4.0f * S), 7.0f * S, FLinearColor(1.0f, 0.78f, 0.58f, 0.9f));
		if (bDetailed)
		{
			// a coral sprig growing up beside them, as in the painted preview
			CoralSprig(FVector2D(Rect.Min.X + 34.0f * S, Rect.Max.Y + 1.0f * S), 15.0f * S, FLinearColor(1.0f, 0.45f, 0.55f, 0.95f));
		}
	}

	// three small bubbles in the bottom padding, clear of the content above it
	if (!bBubbles)
	{
		return;
	}
	struct FBubble { float X; float Y; float Diameter; };
	static const FBubble Bubbles[] = { { RadiusPanel + 21.0f, 10.0f, 6.0f }, { RadiusPanel + 11.0f, 13.0f, 4.5f }, { RadiusPanel + 3.0f, 16.0f, 3.5f } };
	for (const FBubble& Bubble : Bubbles)
	{
		const float Radius = Bubble.Diameter * 0.5f * S;
		const FVector2D Center(Rect.Max.X - Bubble.X * S, Rect.Max.Y - Bubble.Y * S);
		if (bDetailed)
		{
			ATDHUD::Bubble(Center, Radius * 1.15f, Tint, 0.95f);
			continue;
		}
		const FBox2D Box(Center - FVector2D(Radius, Radius), Center + FVector2D(Radius, Radius));
		RoundFill(Box, WithAlpha(Tint, 0.1f), Radius);
		RoundFrame(Box, WithAlpha(Tint, 0.5f), Radius, 1.5f * S);
		// a tiny glint in each bubble
		Blob(Center + FVector2D(-0.35f, -0.35f) * Radius, FVector2D(Radius * 0.35f, Radius * 0.35f), 0.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.45f));
	}
}

void ATDHUD::DrawSeaBackdrop(float Strength)
{
	if (!Canvas)
	{
		return;
	}
	if (DetailedUI())
	{
		DrawSeaBackdropDetailed(Strength);
		return;
	}
	// everything moves on periods of tens of seconds, and stays dim, so it reads as a picture and not as motion
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float S = UIScale;
	const double T = Now();

	// light shafts from the surface, leaning and breathing slowly
	for (int32 i = 0; i < 5; ++i)
	{
		const float Seed = i * 1.37f;
		const float X = W * (0.1f + 0.2f * i) + (float)FMath::Sin(T * 0.07 + Seed) * 40.0f * S;
		const float Angle = 20.0f + 4.0f * (float)FMath::Sin(T * 0.05 + Seed * 2.0f);
		const float BeamW = (70.0f + 60.0f * Hash01(i, 1)) * S;
		const float Pulse = 0.5f + 0.5f * (float)FMath::Sin(T * 0.11 + Seed * 4.0f);
		Blob(FVector2D(X, H * 0.2f), FVector2D(BeamW, H * 1.4f), Angle, FLinearColor(0.45f, 0.85f, 1.0f, (0.02f + 0.02f * Pulse) * Strength), true);
	}

	// two small schools of fish crossing far away, one each way
	for (int32 School = 0; School < 2; ++School)
	{
		const float Dir = School == 0 ? 1.0f : -1.0f;
		const float FishScale = (School == 0 ? 1.0f : 0.75f) * S;
		const float Travel = W + 400.0f * S;
		const float Speed = (School == 0 ? 22.0f : 15.0f) * S;
		const float Lead = (float)FMath::Fmod(T * Speed + School * Travel * 0.5f, (double)Travel) - 200.0f * S;
		const float LeadX = Dir > 0.0f ? Lead : W - Lead;
		const float BaseY = H * (School == 0 ? 0.3f : 0.18f) + (float)FMath::Sin(T * 0.13 + School) * 20.0f * S;
		const FLinearColor FishColor(0.02f, 0.1f, 0.14f, 0.5f * Strength);
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector2D Pos(LeadX - Dir * (20.0f + 26.0f * i + 14.0f * Hash01(i, School + 3)) * FishScale,
				BaseY + (Hash01(i, School + 7) - 0.5f) * 50.0f * FishScale + (float)FMath::Sin(T * 0.8 + i * 1.9) * 3.0f * FishScale);
			const float Wiggle = (float)FMath::Sin(T * 3.0 + i * 2.3) * 12.0f;
			Blob(Pos, FVector2D(18.0f, 7.0f) * FishScale, 0.0f, FishColor);
			// forked tail behind the body
			const FVector2D Tail = Pos - FVector2D(Dir * 10.0f * FishScale, 0.0f);
			Blob(Tail + FVector2D(0.0f, -2.0f * FishScale), FVector2D(7.0f, 3.0f) * FishScale, Dir * (-30.0f + Wiggle), FishColor);
			Blob(Tail + FVector2D(0.0f, 2.0f * FishScale), FVector2D(7.0f, 3.0f) * FishScale, Dir * (30.0f + Wiggle), FishColor);
		}
	}

	// kelp swaying in the bottom corners
	struct FKelp { float X; float Height; float Phase; };
	static const FKelp Kelp[] = { { 0.025f, 0.36f, 0.0f }, { 0.065f, 0.25f, 1.7f }, { 0.105f, 0.31f, 3.1f }, { 0.9f, 0.28f, 0.9f }, { 0.94f, 0.4f, 2.4f }, { 0.975f, 0.23f, 4.0f } };
	const FLinearColor KelpShade(0.03f, 0.22f, 0.1f, 0.65f * Strength);
	for (const FKelp& Stalk : Kelp)
	{
		constexpr int32 Segments = 16;
		const float StalkH = Stalk.Height * H;
		const float SegLen = StalkH / Segments;
		auto PointAt = [&](float F)
		{
			const float Sway = (float)FMath::Sin(T * 0.35 + Stalk.Phase + F * 2.2f) * F * F * 30.0f * S;
			return FVector2D(Stalk.X * W + Sway, H + 8.0f * S - F * StalkH);
		};
		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			const float F0 = (float)Seg / Segments;
			const FVector2D A = PointAt(F0);
			const FVector2D B = PointAt(F0 + 1.0f / Segments);
			const FVector2D Mid = (A + B) * 0.5f;
			const float Angle = FMath::RadiansToDegrees(FMath::Atan2((float)(B.X - A.X), (float)(A.Y - B.Y)));
			const float Thick = FMath::Lerp(9.0f, 4.0f, F0) * S;
			Blob(Mid, FVector2D(Thick, SegLen * 1.5f), Angle, KelpShade);
			// a leaf on alternating sides every few segments
			if (Seg % 3 == 1)
			{
				const float Side = (Seg / 3) % 2 == 0 ? 1.0f : -1.0f;
				const float LeafLen = FMath::Lerp(34.0f, 18.0f, F0) * S;
				const float LeafAngle = Angle + Side * 38.0f;
				const FVector2D Out(FMath::Sin(FMath::DegreesToRadians(LeafAngle)), -FMath::Cos(FMath::DegreesToRadians(LeafAngle)));
				Blob(Mid + Out * LeafLen * 0.5f, FVector2D(LeafLen * 0.32f, LeafLen), LeafAngle, KelpShade);
			}
		}
	}

	// bubbles drifting up, fading out toward the surface
	for (int32 i = 0; i < 26; ++i)
	{
		const float Speed = (14.0f + 22.0f * Hash01(i, 2)) * S;
		const float Span = H + 40.0f * S;
		const float Age = (float)FMath::Fmod(T * Speed + Hash01(i, 3) * Span, (double)Span);
		const float Y = H + 20.0f * S - Age;
		const float X = Hash01(i, 1) * W + (float)FMath::Sin(T * 0.6 + i) * 10.0f * S * Hash01(i, 4);
		const float Radius = (1.5f + 3.5f * Hash01(i, 5)) * S;
		const float Fade = FMath::Clamp(Y / (0.3f * H), 0.0f, 1.0f) * Strength;
		const FBox2D Box(FVector2D(X - Radius, Y - Radius), FVector2D(X + Radius, Y + Radius));
		RoundFill(Box, FLinearColor(0.6f, 0.95f, 1.0f, 0.04f * Fade), Radius);
		RoundFrame(Box, FLinearColor(0.6f, 0.95f, 1.0f, 0.2f * Fade), Radius, 1.0f * S);
	}
}

void ATDHUD::DrawSeaBackdropDetailed(float Strength)
{
	// the same slow scene as the plain backdrop, painted with more depth: everything still moves on periods of tens of seconds
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float S = UIScale;
	const double T = Now();

	// light shafts from the surface: a broad soft beam with a brighter core, leaning and breathing slowly
	for (int32 i = 0; i < 7; ++i)
	{
		const float Seed = i * 1.37f;
		const float X = W * (0.06f + 0.145f * i) + (float)FMath::Sin(T * 0.07 + Seed) * 40.0f * S;
		const float Angle = 19.0f + 4.0f * (float)FMath::Sin(T * 0.05 + Seed * 2.0f);
		const float BeamW = (80.0f + 70.0f * Hash01(i, 1)) * S;
		const float Pulse = 0.5f + 0.5f * (float)FMath::Sin(T * 0.11 + Seed * 4.0f);
		Blob(FVector2D(X, H * 0.15f), FVector2D(BeamW, H * 1.5f), Angle, FLinearColor(0.45f, 0.85f, 1.0f, (0.025f + 0.025f * Pulse) * Strength), true);
		Blob(FVector2D(X + 6.0f * S, H * 0.1f), FVector2D(BeamW * 0.3f, H * 1.3f), Angle, FLinearColor(0.7f, 0.95f, 1.0f, (0.02f + 0.03f * Pulse) * Strength), true);
	}

	// marine snow: tiny pale specks sinking and drifting
	for (int32 i = 0; i < 60; ++i)
	{
		const float Span = H + 20.0f * S;
		const float Fall = (6.0f + 8.0f * Hash01(i, 21)) * S;
		const float Y = (float)FMath::Fmod(T * Fall + Hash01(i, 22) * Span, (double)Span) - 10.0f * S;
		const float X = Hash01(i, 23) * W + (float)FMath::Sin(T * 0.3 + i * 1.7) * 8.0f * S;
		const float D = (1.2f + 1.6f * Hash01(i, 24)) * S;
		Blob(FVector2D(X, Y), FVector2D(D, D), 0.0f, FLinearColor(0.8f, 0.95f, 1.0f, (0.12f + 0.14f * Hash01(i, 25)) * Strength));
	}

	// schools of fish far away, with pale bellies and flicking tails
	for (int32 School = 0; School < 3; ++School)
	{
		const float Dir = School == 1 ? -1.0f : 1.0f;
		const float FishScale = (School == 0 ? 1.0f : (School == 1 ? 0.75f : 0.6f)) * S;
		const float Travel = W + 400.0f * S;
		const float Speed = (School == 0 ? 22.0f : (School == 1 ? 15.0f : 11.0f)) * S;
		const float Lead = (float)FMath::Fmod(T * Speed + School * Travel * 0.37f, (double)Travel) - 200.0f * S;
		const float LeadX = Dir > 0.0f ? Lead : W - Lead;
		const float BaseY = H * (School == 0 ? 0.3f : (School == 1 ? 0.18f : 0.42f)) + (float)FMath::Sin(T * 0.13 + School) * 20.0f * S;
		const FLinearColor Back(0.02f, 0.1f, 0.14f, 0.55f * Strength);
		const FLinearColor Belly(0.3f, 0.5f, 0.55f, 0.4f * Strength);
		const int32 FishCount = School == 2 ? 9 : 7;
		for (int32 i = 0; i < FishCount; ++i)
		{
			const FVector2D Pos(LeadX - Dir * (20.0f + 24.0f * i + 14.0f * Hash01(i, School + 3)) * FishScale,
				BaseY + (Hash01(i, School + 7) - 0.5f) * 56.0f * FishScale + (float)FMath::Sin(T * 0.8 + i * 1.9) * 3.0f * FishScale);
			const float Wiggle = (float)FMath::Sin(T * 3.0 + i * 2.3) * 14.0f;
			const FVector2D Tail = Pos - FVector2D(Dir * 10.0f * FishScale, 0.0f);
			Blob(Tail + FVector2D(0.0f, -2.0f * FishScale), FVector2D(7.0f, 3.0f) * FishScale, Dir * (-30.0f + Wiggle), Back);
			Blob(Tail + FVector2D(0.0f, 2.0f * FishScale), FVector2D(7.0f, 3.0f) * FishScale, Dir * (30.0f + Wiggle), Back);
			Blob(Pos, FVector2D(19.0f, 7.0f) * FishScale, 0.0f, Back);
			Blob(Pos + FVector2D(Dir * 1.0f, 1.6f) * FishScale, FVector2D(14.0f, 3.0f) * FishScale, 0.0f, Belly);
			Blob(Pos + FVector2D(Dir * 6.0f, -0.8f) * FishScale, FVector2D(1.6f, 1.6f) * FishScale, 0.0f, FLinearColor(0.75f, 0.9f, 0.95f, 0.5f * Strength));
		}
	}

	// reef ridges along the bottom: a paler far line and a darker near one, soft-edged so they read as depth, not shapes
	struct FRidge { float X; float Height; float Width; };
	static const FRidge FarRidges[] = { { 0.0f, 0.2f, 0.22f }, { 0.16f, 0.14f, 0.2f }, { 0.34f, 0.18f, 0.24f }, { 0.52f, 0.12f, 0.2f }, { 0.68f, 0.19f, 0.22f }, { 0.86f, 0.15f, 0.2f }, { 1.0f, 0.21f, 0.2f } };
	for (const FRidge& Ridge : FarRidges)
	{
		Blob(FVector2D(W * Ridge.X, H * 1.0f), FVector2D(W * Ridge.Width * 1.4f, H * Ridge.Height * 2.0f), 0.0f, FLinearColor(0.02f, 0.13f, 0.17f, 0.5f * Strength), true);
	}
	static const FRidge NearRidges[] = { { 0.06f, 0.12f, 0.2f }, { 0.28f, 0.08f, 0.22f }, { 0.5f, 0.1f, 0.18f }, { 0.74f, 0.07f, 0.22f }, { 0.95f, 0.13f, 0.2f } };
	for (const FRidge& Ridge : NearRidges)
	{
		Blob(FVector2D(W * Ridge.X, H * 1.02f), FVector2D(W * Ridge.Width * 1.3f, H * Ridge.Height * 2.0f), 0.0f, FLinearColor(0.004f, 0.05f, 0.07f, 0.7f * Strength), true);
	}

	// kelp swaying in the bottom corners: a dark stalk with a lit edge, leaves with a pale midrib
	struct FKelp { float X; float Height; float Phase; };
	static const FKelp Kelp[] = { { 0.02f, 0.4f, 0.0f }, { 0.055f, 0.27f, 1.7f }, { 0.09f, 0.34f, 3.1f }, { 0.125f, 0.22f, 5.2f },
		{ 0.88f, 0.25f, 0.9f }, { 0.915f, 0.31f, 4.6f }, { 0.945f, 0.42f, 2.4f }, { 0.98f, 0.26f, 4.0f } };
	const FLinearColor StalkC(0.04f, 0.27f, 0.13f, 0.85f * Strength);
	const FLinearColor LeafC(0.06f, 0.36f, 0.16f, 0.8f * Strength);
	const FLinearColor LitC(0.35f, 0.75f, 0.45f, 0.35f * Strength);
	for (const FKelp& Stalk : Kelp)
	{
		constexpr int32 Segments = 18;
		const float StalkH = Stalk.Height * H;
		const float SegLen = StalkH / Segments;
		auto PointAt = [&](float F)
		{
			const float Sway = (float)FMath::Sin(T * 0.35 + Stalk.Phase + F * 2.2f) * F * F * 30.0f * S;
			return FVector2D(Stalk.X * W + Sway, H + 8.0f * S - F * StalkH);
		};
		for (int32 Seg = 0; Seg < Segments; ++Seg)
		{
			const float F0 = (float)Seg / Segments;
			const FVector2D A = PointAt(F0);
			const FVector2D B = PointAt(F0 + 1.0f / Segments);
			const FVector2D Mid = (A + B) * 0.5f;
			const float Angle = FMath::RadiansToDegrees(FMath::Atan2((float)(B.X - A.X), (float)(A.Y - B.Y)));
			const float Thick = FMath::Lerp(8.0f, 3.5f, F0) * S;
			Blob(Mid, FVector2D(Thick, SegLen * 1.5f), Angle, StalkC);
			Blob(Mid - FVector2D(Thick * 0.22f, 0.0f), FVector2D(Thick * 0.3f, SegLen * 1.3f), Angle, LitC);
			// a leaf every few segments, alternating sides, each with a pale midrib
			if (Seg % 3 == 1)
			{
				const float Side = (Seg / 3) % 2 == 0 ? 1.0f : -1.0f;
				const float LeafLen = FMath::Lerp(36.0f, 18.0f, F0) * S;
				const float LeafAngle = Angle + Side * (36.0f + 6.0f * (float)FMath::Sin(T * 0.5 + Stalk.Phase + Seg));
				const FVector2D Out(FMath::Sin(FMath::DegreesToRadians(LeafAngle)), -FMath::Cos(FMath::DegreesToRadians(LeafAngle)));
				Blob(Mid + Out * LeafLen * 0.5f, FVector2D(LeafLen * 0.34f, LeafLen), LeafAngle, LeafC);
				Blob(Mid + Out * LeafLen * 0.48f, FVector2D(FMath::Max(1.0f, LeafLen * 0.05f), LeafLen * 0.8f), LeafAngle, LitC);
			}
		}
	}

	// bubbles drifting up in loose strings, fading toward the surface
	for (int32 i = 0; i < 30; ++i)
	{
		const float Speed = (14.0f + 22.0f * Hash01(i, 2)) * S;
		const float Span = H + 40.0f * S;
		const float Age = (float)FMath::Fmod(T * Speed + Hash01(i, 3) * Span, (double)Span);
		const float Y = H + 20.0f * S - Age;
		const float X = Hash01(i / 3, 1) * W + (i % 3) * 6.0f * S + (float)FMath::Sin(T * 0.6 + i) * 10.0f * S * Hash01(i, 4);
		const float Radius = (1.8f + 3.6f * Hash01(i, 5)) * S;
		const float Fade = FMath::Clamp(Y / (0.3f * H), 0.0f, 1.0f) * Strength;
		Bubble(FVector2D(X, Y), Radius, FLinearColor(0.6f, 0.95f, 1.0f, 1.0f), 0.55f * Fade);
	}
}

bool ATDHUD::IsDetailedUI()
{
	return DetailedUI();
}

UTexture2D* ATDHUD::MapThumbnailTexture(int32 Map)
{
	static TMap<int32, UTexture2D*> Cache;
	if (UTexture2D** Found = Cache.Find(Map))
	{
		return *Found;
	}
	// the texture is square; each texel maps to a spot in the Cols x Rows board, and the draw stretches it back to the board's shape
	TSet<FIntPoint> Rocks;
	for (const FIntPoint& Rock : TD::MapRocks(Map))
	{
		Rocks.Add(Rock);
	}
	const FIntPoint Spawn = TD::MapSpawn(Map), Goal = TD::MapGoal(Map);
	const FLinearColor Tint = TD::MapTint(Map);
	constexpr int32 Size = 256;
	UTexture2D* Texture = MakePaintedTexture(Size, [Map, &Rocks, Spawn, Goal, Tint](float X, float Y)
	{
		const float BX = X / Size * TD::Cols, BY = Y / Size * TD::Rows;
		const FIntPoint C(FMath::FloorToInt(BX), FMath::FloorToInt(BY));
		if (!TD::IsMapCell(Map, C))
		{
			return FLinearColor(0, 0, 0, 0);
		}
		const float FX = BX - C.X, FY = BY - C.Y;
		// sand with soft ripples and grain, a faint checker, darker grout between the cells
		const float Ripple = FMath::Sin((BX * 0.8f + BY * 1.3f) * 6.0f + FMath::Sin(BX * 2.1f) * 1.5f) * 0.5f + 0.5f;
		const float Grain = Hash01(FMath::FloorToInt(X * 2.0f), FMath::FloorToInt(Y * 2.0f));
		float Light = 0.92f + 0.08f * Ripple + 0.06f * (Grain - 0.5f) + ((C.X + C.Y) % 2 == 0 ? 0.0f : 0.04f);
		const float Edge = FMath::Min(FMath::Min(FX, 1.0f - FX), FMath::Min(FY, 1.0f - FY));
		Light *= 0.82f + 0.18f * SmoothStep01(0.0f, 0.08f, Edge);
		// a lit top-left bevel on every tile
		Light *= 1.0f + 0.08f * SmoothStep01(0.12f, 0.0f, FMath::Min(FX, FY));
		// the board's outer edge falls away into shadow
		const bool bLeft = !TD::IsMapCell(Map, C - FIntPoint(1, 0)), bRight = !TD::IsMapCell(Map, C + FIntPoint(1, 0));
		const bool bUp = !TD::IsMapCell(Map, C - FIntPoint(0, 1)), bDown = !TD::IsMapCell(Map, C + FIntPoint(0, 1));
		const float Outer = FMath::Min(FMath::Min(bLeft ? FX : 1.0f, bRight ? 1.0f - FX : 1.0f), FMath::Min(bUp ? FY : 1.0f, bDown ? 1.0f - FY : 1.0f));
		Light *= 0.6f + 0.4f * SmoothStep01(0.0f, 0.18f, Outer);
		FLinearColor Col(0.075f * Tint.R * Light, 0.14f * Tint.G * Light, 0.16f * Tint.B * Light, 1.0f);
		if (Rocks.Contains(C))
		{
			// a rounded boulder, lit from the upper left, with a contact shadow on the sand
			const FVector2f P(FX - 0.5f, FY - 0.52f);
			const float R = 0.42f + 0.03f * FMath::Sin(FMath::Atan2(P.Y, P.X) * 3.0f + C.X);
			const float D = P.Size();
			Col *= 1.0f - 0.45f * SmoothStep01(0.5f, 0.32f, (P - FVector2f(-0.05f, -0.07f)).Size());
			if (D < R)
			{
				const float Q = D / R;
				const FVector3f N = FVector3f(P.X / R, P.Y / R, FMath::Sqrt(FMath::Max(0.0f, 1.0f - Q * Q))).GetSafeNormal();
				const float Diffuse = FMath::Max(0.0f, FVector3f::DotProduct(N, LightDir));
				const float Spec = FMath::Pow(FMath::Max(0.0f, FVector3f::DotProduct(N, (LightDir + FVector3f(0, 0, 1)).GetSafeNormal())), 20.0f) * 0.25f;
				const float Speck = Hash01(FMath::FloorToInt(X * 3.0f), FMath::FloorToInt(Y * 3.0f)) * 0.1f;
				const float V = 0.3f + 0.55f * Diffuse + Speck;
				Col = FLinearColor(0.2f * V + Spec, 0.23f * V + Spec, 0.26f * V + Spec, 1.0f);
				Col *= 1.0f - 0.5f * SmoothStep01(R - 0.04f, R, D);
				Col.A = 1.0f;
			}
		}
		// the trench (red glow) where creatures come from and the reef (green glow) they head for
		const auto GlowAt = [BX, BY](const FIntPoint& At) { return FVector2f(BX - (At.X + 0.5f), BY - (At.Y + 0.5f)).Size(); };
		const float SpawnD = GlowAt(Spawn), GoalD = GlowAt(Goal);
		Col = FMath::Lerp(Col, FLinearColor(0.95f, 0.35f, 0.38f, 1.0f), SmoothStep01(1.1f, 0.2f, SpawnD) * 0.85f);
		Col = FMath::Lerp(Col, FLinearColor(0.45f, 0.95f, 0.55f, 1.0f), SmoothStep01(1.1f, 0.2f, GoalD) * 0.85f);
		const float Ring = FMath::Min(FMath::Abs(SpawnD - 0.55f), FMath::Abs(GoalD - 0.55f));
		Col = FMath::Lerp(Col, FLinearColor::White, SmoothStep01(0.07f, 0.0f, Ring) * 0.5f);
		Col.A = 1.0f;
		return Col;
	}, TEXT("TDHUDMapThumb"));
	Cache.Add(Map, Texture);
	return Texture;
}

void ATDHUD::GlassBar(const FBox2D& Rect, float Fraction, const FLinearColor& Color, bool bTicks)
{
	const float H = Rect.GetSize().Y;
	const float R = H * 0.5f;
	// a dark keyline round the empty tube
	RoundFill(Rect.ExpandBy(1.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.72f), R + 1.0f);
	if (Fraction > 0.0f)
	{
		const FBox2D Bar(Rect.Min, FVector2D(Rect.Min.X + FMath::Max(H, Rect.GetSize().X * FMath::Clamp(Fraction, 0.0f, 1.0f)), Rect.Max.Y));
		RoundFill(Bar, Color, R);
		// the lit top of the liquid: a pale streak inset from the ends
		RoundFill(FBox2D(FVector2D(Bar.Min.X + R * 0.6f, Bar.Min.Y + FMath::Max(1.0f, H * 0.12f)), FVector2D(Bar.Max.X - R * 0.6f, Bar.Min.Y + H * 0.45f)),
			FLinearColor(1.0f, 1.0f, 1.0f, 0.4f), H * 0.2f);
	}
	if (bTicks)
	{
		// quarter marks across the whole tube
		for (int32 q = 1; q < 4; ++q)
		{
			const float X = FMath::RoundToFloat(Rect.Min.X + Rect.GetSize().X * q / 4.0f);
			Fill(FBox2D(FVector2D(X, Rect.Min.Y + 1.0f), FVector2D(X + 1.0f, Rect.Max.Y - 1.0f)), FLinearColor(0.0f, 0.0f, 0.0f, 0.4f));
		}
	}
}

void ATDHUD::Frame(const FBox2D& Rect, const FLinearColor& Color, float Thickness)
{
	const FBox2D Box = Snap(Rect);
	const FVector2D Size = Box.GetSize();
	const float T = FMath::Max(1.0f, FMath::RoundToFloat(Thickness));
	// Fill snaps to whole pixels again, which leaves these already whole edges unchanged
	Fill(FBox2D(Box.Min, Box.Min + FVector2D(Size.X, T)), Color);
	Fill(FBox2D(FVector2D(Box.Min.X, Box.Max.Y - T), FVector2D(Box.Min.X + Size.X, Box.Max.Y)), Color);
	Fill(FBox2D(FVector2D(Box.Min.X, Box.Min.Y + T), FVector2D(Box.Min.X + T, Box.Max.Y - T)), Color);
	Fill(FBox2D(FVector2D(Box.Max.X - T, Box.Min.Y + T), FVector2D(Box.Max.X, Box.Max.Y - T)), Color);
}

FVector2D ATDHUD::MeasureText(const FString& InText, float Size) const
{
	if (InText.IsEmpty() || !HUDFont() || !FSlateApplication::IsInitialized())
	{
		return FVector2D::ZeroVector;
	}
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return Measure->Measure(InText, MakeFont(Size, UIScale, bBoldText));
}

float ATDHUD::FitSize(const FString& InText, float Size, float MaxWidth) const
{
	while (Size > SizeMin && MeasureText(InText, Size).X > MaxWidth)
	{
		Size -= 1.0f;
	}
	return Size;
}

void ATDHUD::PanelHeader(UTexture2D* Texture, float X, float Y, float W, const FString& Title, const FLinearColor& TitleColor, const FString& Note, const FLinearColor& NoteColor)
{
	// icon on a round plate with an accent rim
	const float IconSize = HeaderIcon * UIScale;
	const float CenterY = Y + IconSize * 0.5f;
	const FBox2D Plate(FVector2D(X, Y), FVector2D(X + IconSize, Y + IconSize));
	if (DetailedUI())
	{
		// a porthole: a dark glass window in a thin nacre ring
		RoundFill(Plate, FLinearColor(0.003f, 0.035f, 0.06f, 0.85f), IconSize * 0.5f);
		NacreRim(Plate, TitleColor, IconSize * 0.5f, 2.4f * UIScale);
	}
	else
	{
		RoundFill(Plate, InsetColor, IconSize * 0.5f);
		RoundFrame(Plate, WithAlpha(TitleColor, 0.7f), IconSize * 0.5f, 1.5f * UIScale);
	}
	Icon(Texture, Plate.ExpandBy(-5.0f * UIScale));

	const float NoteW = MeasureText(Note, SizeSmall).X;
	Text(Note, X + W - NoteW, CenterY, NoteColor, SizeSmall, false, true);

	const float TitleX = X + IconSize + 10.0f * UIScale;
	Text(Title, TitleX, CenterY, TitleColor, FitSize(Title, SizeHeading, X + W - NoteW - 10.0f * UIScale - TitleX), false, true);
}

void ATDHUD::Field(float X, float Y, float W, const FString& Label, const FString& Value, const FLinearColor& ValueColor, float Size)
{
	const float LabelW = Text(Label, X, Y, DimText, Size).X;
	const float ValueSize = FitSize(Value, Size, W - LabelW - 8.0f * UIScale);
	// a value shrunk to fit stays centred on the label's line instead of riding high
	const FVector2D ValueExtent = MeasureText(Value, ValueSize);
	const float DropY = ValueSize < Size ? (MeasureText(Value, Size).Y - ValueExtent.Y) * 0.5f : 0.0f;
	Text(Value, X + W - ValueExtent.X, Y + DropY, ValueColor, ValueSize);
}

void ATDHUD::FieldRow(float X, float& Y, float W, float Line, const FString& Label1, const FString& Value1, const FString& Label2, const FString& Value2,
	const FLinearColor& Color1, const FLinearColor& Color2)
{
	const float Gap = 18.0f * UIScale;
	const float ColW = (W - Gap) * 0.5f;
	Field(X, Y, ColW, Label1, Value1, Color1);
	Field(X + ColW + Gap, Y, ColW, Label2, Value2, Color2);
	// hairline under the row
	Fill(FBox2D(FVector2D(X, Y + Line - 5.0f * UIScale), FVector2D(X + W, Y + Line - 5.0f * UIScale + FMath::Max(1.0f, FMath::RoundToFloat(UIScale)))), WithAlpha(PanelEdge, 0.08f));
	Y += Line;
}

void ATDHUD::Meter(const FBox2D& Rect, float Fraction, const FLinearColor& Color, const FString& Label)
{
	const float R = Rect.GetSize().Y * 0.5f;
	RoundFill(Rect, TrackColor, R);
	if (Fraction > 0.0f)
	{
		// never thinner than its own rounded ends
		const float FillW = FMath::Max(Rect.GetSize().Y, Rect.GetSize().X * FMath::Clamp(Fraction, 0.0f, 1.0f));
		const FBox2D Bar(Rect.Min, FVector2D(Rect.Min.X + FillW, Rect.Max.Y));
		RoundFill(Bar, Color, R);
		// lighter top half for a glassy tube
		RoundFill(FBox2D(Bar.Min + FVector2D(2.0f * UIScale, 1.0f * UIScale), FVector2D(Bar.Max.X - 2.0f * UIScale, Bar.Min.Y + Rect.GetSize().Y * 0.45f)), FLinearColor(1.0f, 1.0f, 1.0f, 0.12f), R * 0.5f);
		if (DetailedUI() && Fraction < 0.995f)
		{
			// the liquid's meniscus: a bright edge where it ends
			const float S = UIScale;
			Fill(FBox2D(FVector2D(Bar.Max.X - 1.5f * S, Rect.Min.Y + 1.5f * S), FVector2D(Bar.Max.X, Rect.Max.Y - 1.5f * S)), FLinearColor(1.0f, 1.0f, 1.0f, 0.35f));
		}
		if (DetailedUI() && FillW > 40.0f * UIScale)
		{
			// a few tiny bubbles drifting up through the liquid
			const float S = UIScale;
			for (int32 i = 0; i < 3; ++i)
			{
				const float Rise = (float)FMath::Fmod(Now() * (0.25 + 0.1 * i) + Hash01(i, 31), 1.0);
				const FVector2D P(Rect.Min.X + FillW * (0.2f + 0.28f * i), Rect.Max.Y - 2.0f * S - Rise * (Rect.GetSize().Y - 4.0f * S));
				const float BR = (1.0f + 0.4f * i) * S;
				RoundFrame(FBox2D(P - FVector2D(BR), P + FVector2D(BR)), FLinearColor(1.0f, 1.0f, 1.0f, 0.35f * (1.0f - Rise)), BR, FMath::Max(1.0f, 0.8f * S));
			}
		}
	}
	if (DetailedUI())
	{
		// a glass tube: tick marks at the quarters, a highlight along the top and a dark keyline
		const float S = UIScale;
		for (int32 q = 1; q < 4; ++q)
		{
			const float X = Rect.Min.X + Rect.GetSize().X * q / 4.0f;
			Fill(FBox2D(FVector2D(X, Rect.Max.Y - Rect.GetSize().Y * 0.35f), FVector2D(X + FMath::Max(1.0f, S), Rect.Max.Y - 1.0f * S)), FLinearColor(0.0f, 0.0f, 0.0f, 0.3f));
		}
		RoundFill(FBox2D(Rect.Min + FVector2D(R * 0.6f, 1.0f * S), FVector2D(Rect.Max.X - R * 0.6f, Rect.Min.Y + FMath::Max(2.0f, Rect.GetSize().Y * 0.22f))), FLinearColor(1.0f, 1.0f, 1.0f, 0.1f), R * 0.3f);
		RoundFrame(Rect.ExpandBy(FMath::Max(1.0f, S)), FLinearColor(0.0f, 0.0f, 0.0f, 0.4f), R + S, FMath::Max(1.0f, S));
	}
	RoundFrame(Rect, WithAlpha(PanelEdge, 0.12f), R, 1.0f);
	const FVector2D Center = Rect.GetCenter();
	const float BarSize = (float)Rect.GetSize().Y / UIScale - 2.0f;
	Text(Label, Center.X, Center.Y, FLinearColor::White, FMath::Min(SizeSmall, FMath::Max(SizeMin, BarSize)), true, true);
}

FVector2D ATDHUD::Text(const FString& InText, float X, float Y, const FLinearColor& Color, float Size, bool bCenterX, bool bCenterY)
{
	UFont* Font = HUDFont();
	if (!Canvas || !Font || InText.IsEmpty())
	{
		return FVector2D::ZeroVector;
	}

	const FVector2D Extent = MeasureText(InText, Size);
	if (bCenterX)
	{
		X -= Extent.X * 0.5f;
	}
	if (bCenterY)
	{
		Y -= Extent.Y * 0.5f;
	}

	// dark text (on the gold button) gets no outline or drop shadow, which would read as a doubled label
	const bool bDarkText = Color.GetLuminance() < 0.08f;
	FSlateFontInfo FontInfo = MakeFont(Size, UIScale, bBoldText);
	if (bDarkText)
	{
		FontInfo.OutlineSettings.OutlineSize = 0;
	}
	FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(X), FMath::RoundToFloat(Y)), FText::FromString(InText), FontInfo, Color);
	if (!bDarkText)
	{
		Item.EnableShadow(FLinearColor(0, 0, 0, 0.7f * Color.A), FVector2D(1.0f, 2.0f) * FMath::Max(1.0f, UIScale));
	}
	Canvas->DrawItem(Item);
	return Extent;
}

void ATDHUD::Button(const FBox2D& Rect, const FString& Label, EButton Id, int32 Param, const FLinearColor& Base, bool bEnabled, bool bHighlighted, float TextSize)
{
	const float S = UIScale;
	const float R = RadiusButton * S;
	const bool bHovered = Rect.IsInside(MousePos);
	FLinearColor Color = bEnabled ? Base : DisabledButton;
	if (bEnabled && bHovered)
	{
		Color = Lighten(Base, 0.12f);
	}

	// selected buttons glow in the accent color, the gold call-to-action glows gold
	if (bEnabled && (bHighlighted || IsLightFill(Base)))
	{
		Glow(Rect, WithAlpha(IsLightFill(Base) ? Base : AccentColor, bHovered ? 0.4f : 0.28f), R, 10.0f * S);
	}
	const bool bDetailed = DetailedUI();
	if (bDetailed)
	{
		// a dark keyline round the sea glass
		RoundFrame(Rect.ExpandBy(FMath::Max(1.0f, S)), FLinearColor(0.0f, 0.0f, 0.0f, 0.45f), R + S, FMath::Max(1.0f, S));
	}
	RoundFill(Rect, Color, R);
	if (bDetailed)
	{
		// sea glass: a soft gloss over the upper part, inset from the edge (a disabled button stays matte), and the lit inner lip
		if (bEnabled)
		{
			const FBox2D Gloss(Rect.Min + FVector2D(2.0f * S, 2.0f * S), FVector2D(Rect.Max.X - 2.0f * S, Rect.Min.Y + Rect.GetSize().Y * 0.46f));
			RoundFill(Gloss, FLinearColor(1.0f, 1.0f, 1.0f, IsLightFill(Base) ? 0.2f : 0.1f), FMath::Max(1.0f, R - 2.0f * S));
		}
		InnerLip(Rect, R, 3.0f * S, bEnabled ? 0.5f : 0.2f);
	}
	else
	{
		// top sheen
		RoundFill(FBox2D(Rect.Min, FVector2D(Rect.Max.X, Rect.Min.Y + Rect.GetSize().Y * 0.5f)), FLinearColor(1.0f, 1.0f, 1.0f, IsLightFill(Base) ? 0.16f : 0.07f), R);
	}
	// a darker lip along the bottom gives the button some depth
	RoundFill(FBox2D(FVector2D(Rect.Min.X, Rect.Max.Y - FMath::Max(2.0f, 3.0f * S)), Rect.Max), FLinearColor(0.0f, 0.0f, 0.0f, 0.22f), FMath::Min(R, 3.0f * S));
	// a clickable button drawn in the disabled colour (an option that is off, like Auto-salvage) gets the disabled edge too
	const FLinearColor Edge = (!bEnabled || (Base == DisabledButton && !bHovered)) ? DisabledEdge : (bHighlighted ? AccentColor : (bHovered ? WithAlpha(ButtonEdge, 0.5f) : ButtonEdge));
	RoundFrame(Rect, Edge, R, (bHighlighted ? 1.5f : 1.0f) * S);

	const FVector2D Center = Rect.GetCenter();
	const FLinearColor LabelColor = !bEnabled ? DisabledText : (IsLightFill(Base) ? DarkText : FLinearColor::White);
	Text(Label, Center.X, Center.Y, LabelColor, FitSize(Label, TextSize, Rect.GetSize().X - 14.0f * S), true, true);

	Buttons.Add({ Rect, Id, Param, bEnabled });
}

float ATDHUD::KeyCap(const FString& Key, float X, float Y, bool bOnLight)
{
	if (Key.IsEmpty())
	{
		return 0.0f;
	}
	const float S = UIScale;
	const FVector2D Extent = MeasureText(Key, SizeCaption);
	const float H = 20.0f * S;
	const float W = FMath::Max(H, Extent.X + 10.0f * S);
	const FBox2D Cap(FVector2D(X, Y - H * 0.5f), FVector2D(X + W, Y + H * 0.5f));
	RoundFill(Cap, bOnLight ? FLinearColor(0.1f, 0.04f, 0.0f, 0.14f) : FLinearColor(0.3f, 0.75f, 0.85f, 0.10f), 5.0f * S);
	RoundFrame(Cap, bOnLight ? FLinearColor(0.1f, 0.04f, 0.0f, 0.3f) : WithAlpha(PanelEdge, 0.3f), 5.0f * S, 1.0f);
	Text(Key, Cap.GetCenter().X, Cap.GetCenter().Y, bOnLight ? DarkText : DimText, SizeCaption, true, true);
	return W;
}

void ATDHUD::Switch(const FBox2D& Rect, bool bOn)
{
	const float R = Rect.GetSize().Y * 0.5f;
	RoundFill(Rect, bOn ? WithAlpha(AccentColor, 0.4f) : TrackColor, R);
	RoundFrame(Rect, bOn ? WithAlpha(AccentColor, 0.8f) : ButtonEdge, R, 1.0f);
	const float Knob = Rect.GetSize().Y - 6.0f * UIScale;
	const float KX = bOn ? Rect.Max.X - 3.0f * UIScale - Knob : Rect.Min.X + 3.0f * UIScale;
	const FBox2D KnobRect(FVector2D(KX, Rect.Min.Y + 3.0f * UIScale), FVector2D(KX + Knob, Rect.Min.Y + 3.0f * UIScale + Knob));
	if (bOn)
	{
		Glow(KnobRect, WithAlpha(AccentColor, 0.5f), Knob * 0.5f, 4.0f * UIScale);
	}
	if (DetailedUI())
	{
		Pearl(KnobRect.GetCenter(), Knob * 0.5f, bOn ? FLinearColor::White : FLinearColor(0.45f, 0.55f, 0.6f, 1.0f));
	}
	else
	{
		RoundFill(KnobRect, bOn ? FLinearColor::White : DisabledText, Knob * 0.5f);
	}
}

void ATDHUD::Icon(UTexture2D* Texture, const FBox2D& Rect, const FLinearColor& Tint)
{
	if (Texture && Canvas)
	{
		// whole pixels, so the icon isn't resampled across pixel boundaries
		const FVector2D Min(FMath::RoundToFloat(Rect.Min.X), FMath::RoundToFloat(Rect.Min.Y));
		const FVector2D Size(FMath::RoundToFloat(Rect.GetSize().X), FMath::RoundToFloat(Rect.GetSize().Y));
		DrawTexture(Texture, Min.X, Min.Y, Size.X, Size.Y, 0.0f, 0.0f, 1.0f, 1.0f, Tint, BLEND_Translucent);
	}
}

void ATDHUD::ItemTile(const FBox2D& Rect, const FTDItem* Item, EButton Id, int32 Param)
{
	const float R = RadiusTile * UIScale;
	const bool bHovered = Rect.IsInside(MousePos);
	const bool bDetailed = DetailedUI();
	const float TileW = (float)Rect.GetSize().X, TileH = (float)Rect.GetSize().Y;
	if (!Item || !Item->IsValid())
	{
		RoundFill(Rect, InsetColor, R);
		if (bDetailed)
		{
			// a recessed glass well: shadowed under its top edge, a faint cool light along its bottom
			RoundFill(Rect, FLinearColor(0.0f, 0.02f, 0.035f, 0.25f), R);
			Blob(FVector2D(Rect.GetCenter().X, Rect.Min.Y + TileH * 0.14f), FVector2D(TileW * 0.92f, TileH * 0.34f), 0.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), true);
			RoundFrame(Rect, FLinearColor(0.5f, 0.9f, 1.0f, 0.12f), R, 1.0f);
		}
		else
		{
			RoundFrame(Rect, WithAlpha(PanelEdge, 0.1f), R, 1.0f);
		}
		if (Id == EButton::EquippedItem)
		{
			// faded silhouette of the slot's item kind
			Icon(TDIcons::Item((ETDItemSlot)Param), Rect.ExpandBy(-6.0f * UIScale), FLinearColor(0.7f, 0.8f, 0.85f, 0.3f));
		}
		return;
	}

	const bool bUnique = TD::IsUnique(*Item);
	TArray<FCanvasUVTri, TInlineAllocator<24>> Tris;	// unique corner jewels, drawn over the icon
	if (bUnique)
	{
		// uniques get their own border, unlike any rarity's: a slowly breathing pale-gold halo, a thick gold rim with a thin
		// bright inner rim (a double frame), and a small jewel on each corner
		const float S = UIScale;
		const float Pulse = 0.5f + 0.5f * FMath::Sin((float)Now() * 2.4f);
		Glow(Rect, WithAlpha(UniqueColor, (bHovered ? 0.55f : 0.28f) + 0.14f * Pulse), R, (bHovered ? 8.0f : 5.0f) * S);
		RoundFill(Rect, InsetColor, R);
		RoundFill(Rect.ExpandBy(-1.0f * S), WithAlpha(UniqueColor * (bHovered ? 0.42f : 0.26f), 0.88f), R);
		const FBox2D Outer = Rect.ExpandBy(0.5f * S);
		RoundFrame(Outer, UniqueColor, R + 0.5f * S, 1.5f * S);
		RoundFrame(Rect.ExpandBy(-2.0f * S), WithAlpha(FMath::Lerp(UniqueColor, FLinearColor::White, 0.6f), 0.55f + 0.3f * Pulse), FMath::Max(1.0f, R - 2.0f * S), 1.0f);

		const float Inset = (R + 0.5f * S) * 0.3f;
		const float H = FMath::Max(2.5f, 3.0f * S);
		const FLinearColor Dark(0.03f, 0.04f, 0.06f, 0.95f);
		const FLinearColor Shine = FMath::Lerp(UniqueColor, FLinearColor::White, 0.75f);
		const FVector2D Corners[4] = { Outer.Min + FVector2D(Inset, Inset), FVector2D(Outer.Max.X - Inset, Outer.Min.Y + Inset),
			Outer.Max - FVector2D(Inset, Inset), FVector2D(Outer.Min.X + Inset, Outer.Max.Y - Inset) };
		auto Gem = [&Tris](const FVector2D& C, float Half, const FLinearColor& Color)
		{
			const FVector2D P[4] = { C + FVector2D(0, -Half), C + FVector2D(Half, 0), C + FVector2D(0, Half), C + FVector2D(-Half, 0) };
			for (int32 i = 0; i < 2; ++i)
			{
				FCanvasUVTri& Tri = Tris.AddDefaulted_GetRef();
				Tri.V0_Pos = P[0]; Tri.V1_Pos = P[i + 1]; Tri.V2_Pos = P[i + 2];
				Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
			}
		};
		for (const FVector2D& C : Corners)
		{
			Gem(C, H + 1.0f, Dark);
			Gem(C, H, UniqueColor);
			Gem(C, H * 0.45f, Shine);
		}
	}
	else
	{
		// rarity-tinted well that glows on hover
		const FLinearColor Color = TD::RarityColor(Item->Rarity);
		if (bHovered)
		{
			Glow(Rect, WithAlpha(Color, 0.45f), R, 8.0f * UIScale);
		}
		if (bDetailed)
		{
			// a recessed well: dark at the rim, the rarity colour glowing up from the bottom like light through deep water, a soft
			// bloom behind the icon and a shadow under the top edge; then a bevelled rim in the rarity colour
			const float S = UIScale;
			const FVector2D Mid = Rect.GetCenter();
			RoundFill(Rect, FLinearColor(0.0f, 0.006f, 0.012f, 0.8f), R);
			// a subtle wash of the rarity colour over the whole well (no corner beads or caps: the user removed them)
			RoundFill(Rect.ExpandBy(-1.0f * S), WithAlpha(Color * 0.5f, bHovered ? 0.3f : 0.2f), R);
			Blob(FVector2D(Mid.X, Rect.Max.Y - TileH * 0.26f), FVector2D(TileW * 0.94f, TileH * 0.62f), 0.0f, WithAlpha(Color * 0.75f, bHovered ? 0.6f : 0.45f), true);
			Blob(Mid, FVector2D(TileH * 0.66f, TileH * 0.66f), 0.0f, WithAlpha(FMath::Lerp(Color, FLinearColor::White, 0.3f), 0.16f), true);
			Blob(FVector2D(Mid.X, Rect.Min.Y + TileH * 0.12f), FVector2D(TileW * 0.92f, TileH * 0.3f), 0.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.4f), true);
			NacreRim(Rect, FMath::Lerp(Color, FLinearColor::White, 0.1f), R, 2.0f * S);
		}
		else
		{
			RoundFill(Rect, InsetColor, R);
			RoundFill(Rect.ExpandBy(-1.0f * UIScale), WithAlpha(Color * (bHovered ? 0.5f : 0.3f), 0.85f), R);
			RoundFrame(Rect, WithAlpha(Color, bHovered ? 1.0f : 0.8f), R, 1.5f * UIScale);
		}
	}
	Icon(TDIcons::ForItem(*Item), Rect.ExpandBy(-5.0f * UIScale));
	if (Tris.Num() > 0 && Canvas)
	{
		TArray<FCanvasUVTri> GemTris(Tris);
		FCanvasTriangleItem Gems(GemTris, WhiteTriangles(GemTris));
		Gems.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Gems);
	}

	// slot badge in the bottom left corner: a coloured shape per slot (Shell cyan circle, Pearl pink diamond, Charm lime triangle),
	// ringed in dark so it stands off the icon; shape and colour both differ, so it reads without relying on colour alone
	if (Id == EButton::StorageItem)
	{
		const float S = UIScale;
		const float B = FMath::Max(11.0f, 15.0f * S);
		const FVector2D C(Rect.Min.X + 3.0f * S + B * 0.5f, Rect.Max.Y - 3.0f * S - B * 0.5f);
		const FLinearColor Dark(0.02f, 0.05f, 0.08f, 0.95f);
		const float Ring = FMath::Max(1.5f, 1.8f * S);	// the same width of dark setting round every shape
		// a convex polygon (3 or 4 corners) as a triangle fan around its centre; drawn for every stored item each frame, so no heap use
		using FCorners = TArray<FVector2D, TInlineAllocator<4>>;
		auto Poly = [this, C](const FCorners& Points, const FLinearColor& Color)
		{
			TArray<FCanvasUVTri> Tris;
			Tris.Reserve(Points.Num());
			for (int32 i = 0; i < Points.Num(); ++i)
			{
				FCanvasUVTri& Tri = Tris.AddDefaulted_GetRef();
				Tri.V0_Pos = C; Tri.V1_Pos = Points[i]; Tri.V2_Pos = Points[(i + 1) % Points.Num()];
				Tri.V0_Color = Tri.V1_Color = Tri.V2_Color = Color;
			}
			FCanvasTriangleItem Fan(Tris, WhiteTriangles(Tris));
			Fan.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Fan);
		};
		const FLinearColor SlotCol = TD::SlotColor(Item->Slot);
		switch (Item->Slot)
		{
		case ETDItemSlot::Pearl:
		{
			// diamond
			// all three shapes fill the same box (about 0.95 B tall) on the same centre
			auto Diamond = [C](float H) { return FCorners{ C + FVector2D(0, -H), C + FVector2D(H, 0), C + FVector2D(0, H), C + FVector2D(-H, 0) }; };
			Poly(Diamond(B * 0.5f + Ring * 1.414f), Dark);
			Poly(Diamond(B * 0.5f), SlotCol);
			break;
		}
		case ETDItemSlot::Charm:
		{
			// upward triangle around the centre
			// centred on its box rather than its centroid, so it sits level with the circle and diamond
			const FVector2D TC = C + FVector2D(0.0f, B * 0.6f * 0.25f);
			auto Triangle = [TC](float H) { return FCorners{ TC + FVector2D(0, -H), TC + FVector2D(H * 0.866f, H * 0.5f), TC + FVector2D(-H * 0.866f, H * 0.5f) }; };
			Poly(Triangle(B * 0.6f + Ring * 2.0f), Dark);
			Poly(Triangle(B * 0.6f), SlotCol);
			break;
		}
		default:
		{
			// circle
			const FBox2D Dot(C - FVector2D(B * 0.47f), C + FVector2D(B * 0.47f));
			RoundFill(Dot.ExpandBy(Ring), Dark, B * 0.47f + Ring);
			RoundFill(Dot, SlotCol, B * 0.47f);
			break;
		}
		}
		if (bDetailed)
		{
			// an enamel cabochon: a glint on its upper left
			Blob(C + FVector2D(-B * 0.13f, -B * 0.13f), FVector2D(B * 0.24f, B * 0.17f), -35.0f, FLinearColor(1.0f, 1.0f, 1.0f, 0.5f), true);
		}
	}

	// locked: a small padlock badge in the top right corner
	if (Item->bLocked)
	{
		const float S = UIScale;
		const float B = FMath::Max(10.0f, 15.0f * S);
		const FBox2D Badge(FVector2D(Rect.Max.X - B - 2.0f * S, Rect.Min.Y + 2.0f * S), FVector2D(Rect.Max.X - 2.0f * S, Rect.Min.Y + 2.0f * S + B));
		RoundFill(Badge, FLinearColor(0.02f, 0.05f, 0.08f, 0.9f), B * 0.3f);
		const FVector2D C = Badge.GetCenter();
		const FBox2D Shackle(FVector2D(C.X - B * 0.2f, Badge.Min.Y + B * 0.16f), FVector2D(C.X + B * 0.2f, C.Y + B * 0.1f));
		RoundFrame(Shackle, PearlColor, B * 0.2f, FMath::Max(1.0f, 1.5f * S));
		RoundFill(FBox2D(FVector2D(C.X - B * 0.3f, C.Y - B * 0.02f), FVector2D(C.X + B * 0.3f, Badge.Max.Y - B * 0.16f)), PearlColor, B * 0.08f);
	}

	Buttons.Add({ Rect, Id, Param, true });

	if (bHovered)
	{
		TooltipItem = *Item;
		bTooltipFromStorage = (Id == EButton::StorageItem);
	}
}

// ---------------------------------------------------------------------------------------------
// Main draw
// ---------------------------------------------------------------------------------------------

void ATDHUD::DrawHUD()
{
	Super::DrawHUD();

	Buttons.Reset();
	Panels.Reset();
	TooltipItem = FTDItem();
	TipLines.Reset();
	StorageRect = FBox2D(ForceInit);
	TopLeftRect = FBox2D(ForceInit);
	PreviewPillBottom = 0.0f;
	TopRightRect = FBox2D(ForceInit);

	if (!Canvas || !HUDFont())
	{
		return;
	}

	ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	ATDPlayerController* PC = Cast<ATDPlayerController>(PlayerOwner);

	const float UserScale = GameMode ? GameMode->GetUIScalePercent() / 100.0f : 1.0f;
	UIScale = FMath::Clamp(Canvas->ClipY / 1080.0f, 0.8f, 2.5f) * UserScale;

	FVector2D Cursor;
	MousePos = (PC && PC->GetCursorPosition(Cursor)) ? Cursor : FVector2D(-1, -1);

	// keep dragging a settings slider while the button is held
	if (ActiveSlider >= 0 && GameMode && PC && PC->IsLeftMouseHeld())
	{
		DragSlider(GameMode, MousePos);
	}

	if (!GameMode || !PC)
	{
		return;
	}

	TooltipContext = PC->GetSelectedTower() ? PC->GetSelectedTower()->GetTowerType() : ETDTowerType::Count;
	TooltipTower = PC->GetSelectedTower();

	DrawAmbient();
	DrawWorldOverlays(GameMode);
	DrawTopBar(GameMode, PC);
	DrawDock(GameMode, PC);
	DrawInfoPanel(GameMode, PC);
	if (PC->IsStorageOpen())
	{
		DrawStorage(GameMode, PC);
	}
	DrawWavePreview(GameMode);
	DrawRunStrip(GameMode);
	DrawWaveClearCard(GameMode);
	DrawToasts();
	DrawCursorHint(GameMode, PC);

	// (the setup screen opened from the game over card draws over it)
	if (GameMode->IsGameOver() && GameMode->GetMenu() != ETDMenu::Setup)
	{
		PC->CancelSell();
		DrawGameOver(GameMode);
	}
	else if (GameMode->GetMenu() != ETDMenu::None)
	{
		DrawMenu(GameMode);
	}
	if (GameMode->GetPendingConfirm() != ETDConfirm::None)
	{
		TipLines.Reset();	// nothing under the question shows a tooltip
		DrawConfirm(GameMode);
	}

	DrawTooltip();
}

void ATDHUD::DrawWorldOverlays(ATDGameMode* GameMode)
{
	const float S = UIScale;
	TArray<ETDStatus> Statuses;
	// whole pixels, with room for the dark rim between neighbouring dots
	const float Chip = FMath::Max(6.0f, FMath::RoundToFloat(8.0f * S));
	const float Gap = FMath::Max(3.0f, FMath::RoundToFloat(2.0f * S) + 2.0f);
	struct FChipRow
	{
		TArray<ETDStatus, TInlineAllocator<(int32)ETDStatus::Count>> Statuses;
		FBox2D Rect;
	};
	TArray<FChipRow, TInlineAllocator<64>> ChipRows;
	const bool bDetailedUI = DetailedUI();

	for (const ATDEnemy* Enemy : GameMode->GetEnemies())
	{
		if (!Enemy || !Enemy->IsAlive())
		{
			continue;
		}

		Enemy->GetStatuses(Statuses);
		const bool bDamaged = Enemy->GetHealth() < Enemy->GetMaxHealth();
		if (!bDamaged && Statuses.Num() == 0 && Enemy->GetShield() <= 0.0f && !Enemy->IsElite())
		{
			continue;
		}

		const FVector Screen = Project(Enemy->GetActorLocation() + FVector(0, 0, Enemy->GetBarHeight()));
		const bool bBoss = Enemy->GetDef().bBoss || Enemy->IsElite();
		const float W = (bBoss ? 70.0f : 34.0f) * S;
		const float H = (bBoss ? 7.0f : 5.0f) * S;
		const float X = Screen.X - W * 0.5f;
		const float Y = Screen.Y - H * 0.5f;

		// rounded health pill on damaged enemies: coral when low, kelp green when healthy
		if (bDamaged && bDetailedUI)
		{
			const float Fraction = FMath::Clamp(Enemy->GetHealth() / Enemy->GetMaxHealth(), 0.0f, 1.0f);
			GlassBar(FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)), Fraction, FMath::Lerp(LifeColor, KelpColor, Fraction), bBoss);
		}
		else if (bDamaged)
		{
			const float Fraction = FMath::Clamp(Enemy->GetHealth() / Enemy->GetMaxHealth(), 0.0f, 1.0f);
			const FBox2D Track(FVector2D(X - 1, Y - 1), FVector2D(X + W + 1, Y + H + 1));
			RoundFill(Track, FLinearColor(0, 0, 0, 0.7f), Track.GetSize().Y * 0.5f);
			if (Fraction > 0.0f)
			{
				RoundFill(FBox2D(FVector2D(X, Y), FVector2D(X + FMath::Max(H, W * Fraction), Y + H)), FMath::Lerp(LifeColor, KelpColor, Fraction), H * 0.5f);
			}
		}
		// shield on top of the health bar
		if (Enemy->GetShield() > 0.0f)
		{
			const float ShieldFraction = FMath::Clamp(Enemy->GetShield() / FMath::Max(1.0f, Enemy->GetMaxShield()), 0.0f, 1.0f);
			if (bDetailedUI)
			{
				GlassBar(FBox2D(FVector2D(X, Y - H - 2), FVector2D(X + W, Y - 3)), ShieldFraction, AccentColor, false);
			}
			else
			{
				const FBox2D Track(FVector2D(X - 1, Y - H - 2), FVector2D(X + W + 1, Y - 1));
				RoundFill(Track, FLinearColor(0, 0, 0, 0.7f), Track.GetSize().Y * 0.5f);
				if (ShieldFraction > 0.0f)
				{
					RoundFill(FBox2D(FVector2D(X, Y - H - 1), FVector2D(X + FMath::Max(H, W * ShieldFraction), Y - 2)), AccentColor, (H - 1) * 0.5f);
				}
			}
		}

		// remember where this creature's effect dots would sit; they are laid out together below
		if (Statuses.Num() > 0)
		{
			const float RowW = Statuses.Num() * (Chip + Gap) - Gap;
			const float CY = FMath::RoundToFloat(Y - Chip - 4.0f * S - (Enemy->GetShield() > 0.0f ? H : 0.0f));
			const float CX = FMath::RoundToFloat(Screen.X - RowW * 0.5f);
			FChipRow& Row = ChipRows.AddDefaulted_GetRef();
			Row.Statuses = Statuses;
			Row.Rect = FBox2D(FVector2D(CX, CY), FVector2D(CX + RowW, CY + Chip));
		}
	}

	// one colored dot per active effect, above the bar. Crowded creatures would stack their dots on top of each
	// other, so a row that hits one already placed moves up a step (then sideways) until it is clear
	TArray<FBox2D, TInlineAllocator<64>> Placed;
	const float Step = Chip + Gap + 2.0f;	// the dark rim adds a pixel on each side
	for (const FChipRow& Row : ChipRows)
	{
		auto IsFree = [&Placed](const FBox2D& Rect)
		{
			for (const FBox2D& Other : Placed)
			{
				if (Rect.Min.X < Other.Max.X && Rect.Max.X > Other.Min.X && Rect.Min.Y < Other.Max.Y && Rect.Max.Y > Other.Min.Y)
				{
					return false;
				}
			}
			return true;
		};
		// rects are compared with their rim and a pixel of air around them
		const FBox2D Want = Row.Rect.ExpandBy(FVector2D(Gap * 0.5f + 1.0f, Gap * 0.5f + 1.0f));
		FVector2D Offset = FVector2D::ZeroVector;
		const FVector2D Tries[] = { { 0, 0 }, { 0, -1 }, { 0, -2 }, { -0.5f, -1 }, { 0.5f, -1 }, { 0, -3 }, { -0.5f, -2 }, { 0.5f, -2 }, { 0, -4 } };
		for (const FVector2D& Try : Tries)
		{
			const FVector2D Candidate(FMath::RoundToFloat(Try.X * Row.Rect.GetSize().X), Try.Y * Step);
			if (IsFree(Want.ShiftBy(Candidate)))
			{
				Offset = Candidate;
				break;
			}
		}
		Placed.Add(Want.ShiftBy(Offset));

		float CX = Row.Rect.Min.X + Offset.X;
		const float CY = Row.Rect.Min.Y + Offset.Y;
		for (ETDStatus Status : Row.Statuses)
		{
			const FBox2D Dot(FVector2D(CX, CY), FVector2D(CX + Chip, CY + Chip));
			RoundFill(Dot.ExpandBy(1.0f), FLinearColor(0, 0, 0, 0.75f), Chip);
			if (bDetailedUI)
			{
				// a lit gem in the effect's colour
				Pearl(Dot.GetCenter(), Chip * 0.5f, TD::StatusColor(Status));
			}
			else
			{
				RoundFill(Dot, TD::StatusColor(Status), Chip);
			}
			CX += Chip + Gap;
		}
	}

	// tower levels while Alt is held
	const ATDPlayerController* PC = Cast<ATDPlayerController>(PlayerOwner);
	if (PC && PC->IsAltHeld() && GameMode->GetMenu() == ETDMenu::None)
	{
		if (const ATDGrid* Grid = GameMode->GetGrid())
		{
			for (const ATDTower* Tower : Grid->GetTowers())
			{
				if (!Tower || !Tower->CanProgress())
				{
					continue;
				}
				const FVector Screen = Project(Tower->GetActorLocation() + FVector(0, 0, 115));
				const FString Label = Tower->IsMaxLevel() ? FString(TEXT("MAX")) : FString::Printf(TEXT("Lv %d"), Tower->GetLevel());
				const FVector2D Extent = MeasureText(Label, 16.0f);
				const float W = Extent.X + 16.0f * S;
				const float H = Extent.Y + 4.0f * S;
				const FBox2D Badge(FVector2D(Screen.X - W * 0.5f, Screen.Y - H * 0.5f), FVector2D(Screen.X + W * 0.5f, Screen.Y + H * 0.5f));
				// pill badge, gold rimmed when the tower has skill points to spend
				const bool bPoints = Tower->GetSkillPoints() > 0;
				RoundFill(Badge, TipColor, H * 0.5f);
				if (bDetailedUI)
				{
					// dark glass with light along its top and a brighter rim; a twinkle on the corner when there are points to spend
					RoundFill(FBox2D(Badge.Min + FVector2D(H * 0.3f, 1.5f * S), FVector2D(Badge.Max.X - H * 0.3f, Badge.Min.Y + H * 0.42f)), FLinearColor(1.0f, 1.0f, 1.0f, 0.1f), H * 0.2f);
					RoundFrame(Badge.ExpandBy(1.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), H * 0.5f + 1.0f, 1.0f);
					RoundFrame(Badge, bPoints ? PearlColor : WithAlpha(AccentColor, 0.85f), H * 0.5f, 1.5f * S);
					if (bPoints)
					{
						const FVector2D Star(Badge.Max.X - H * 0.3f, Badge.Min.Y + 1.0f * S);
						const float Pulse = 0.75f + 0.25f * (float)FMath::Sin(Now() * 3.0 + Tower->GetActorLocation().X * 0.01);
						Blob(Star, FVector2D(2.0f, 11.0f) * S * Pulse, 0.0f, FLinearColor(1.0f, 0.97f, 0.85f, 0.95f));
						Blob(Star, FVector2D(11.0f, 2.0f) * S * Pulse, 0.0f, FLinearColor(1.0f, 0.97f, 0.85f, 0.95f));
						Blob(Star, FVector2D(5.0f, 5.0f) * S, 0.0f, WithAlpha(PearlColor, 0.6f), true);
					}
				}
				else
				{
					RoundFrame(Badge, bPoints ? PearlColor : WithAlpha(AccentColor, 0.6f), H * 0.5f, 1.0f * S);
				}
				Text(Label, Screen.X, Screen.Y, Tower->GetSkillPoints() > 0 ? PearlColor : AccentColor, 16.0f, true, true);
			}
		}
	}

	// floating texts ("+N", "LEVEL N")
	const double Time = Now();
	Floaters.RemoveAll([Time](const FFloater& F) { return (float)(Time - F.Start) > FloaterDuration; });
	// all the halos first, then all the texts: alternating glow and text would start two draws per floater
	for (int32 Pass = bDetailedUI ? 0 : 1; Pass < 2; ++Pass)
	{
		for (int32 i = Floaters.Num() - 1; i >= 0; --i)
		{
			const FFloater& F = Floaters[i];
			const float Age = (float)(Time - F.Start);
			const float Alpha = 1.0f - Age / FloaterDuration;
			const FVector Screen = Project(F.World + FVector(0, 0, Age * 60.0f));
			const float FloatSize = F.Text.StartsWith(TEXT("LEVEL")) ? 26.0f : 22.0f;
			if (Pass == 0)
			{
				// a soft halo of the text's own colour behind it, so it glows off the board
				const FVector2D Extent = MeasureText(F.Text, FloatSize);
				const FBox2D Halo(FVector2D(Screen.X, Screen.Y) - Extent * 0.5f, FVector2D(Screen.X, Screen.Y) + Extent * 0.5f);
				Glow(Halo.ExpandBy(FVector2D(-Extent.X * 0.1f, -Extent.Y * 0.2f)), WithAlpha(F.Color, 0.3f * Alpha), Extent.Y * 0.3f, 10.0f * UIScale);
			}
			else
			{
				Text(F.Text, Screen.X, Screen.Y, WithAlpha(F.Color, Alpha), FloatSize, true, true);
			}
		}
	}
}

float ATDHUD::DrawBrand(float X, float MidY, float Scale, bool bDraw)
{
	// laid out in Scale units; text sizes are relative to UIScale, so they get the ratio
	const float S = Scale;
	const float K = Scale / FMath::Max(UIScale, 0.01f);
	const FString Title1 = TEXT("AQUATIC");
	const FString Title2 = TEXT("TOWER DEFENSE");
	const float LogoSize = 40.0f * S;
	float Title1W = 0.0f;
	{
		TGuardValue<bool> Bold(bBoldText, true);
		Title1W = MeasureText(Title1, 19.0f * K).X;
	}
	const float TitleColW = FMath::Max(Title1W, (float)MeasureText(Title2, SizeCaption * K).X);
	// logo plate rings (4 each side), bubbles past its shoulder, and a coral sprig after the wave line
	const float Width = 4.0f * S + LogoSize + 16.0f * S + TitleColW + 19.0f * S;
	if (!bDraw)
	{
		return Width;
	}

	X += 4.0f * S;	// room for the plate's outer ring
	const FBox2D LogoRect(FVector2D(X, MidY - LogoSize * 0.5f), FVector2D(X + LogoSize, MidY + LogoSize * 0.5f));
	const double T = Now();
	// bioluminescent halo behind the logo, breathing slowly
	const float Breath = 0.5f + 0.5f * (float)FMath::Sin(T * 0.6);
	Glow(LogoRect.ExpandBy(-6.0f * S), WithAlpha(AccentColor, 0.1f + 0.08f * Breath), LogoSize * 0.5f, 10.0f * S);
	// round inset plate with an accent rim and a fainter outer ring, like the panel headers' icon plates
	{
		const FBox2D Plate = LogoRect.ExpandBy(1.0f * S);
		const FBox2D Outer = LogoRect.ExpandBy(4.0f * S);
		const float PlateR = Plate.GetSize().X * 0.5f;
		const float OuterR = Outer.GetSize().X * 0.5f;
		RoundFill(Plate, InsetColor, PlateR);
		RoundFrame(Plate, WithAlpha(AccentColor, 0.55f), PlateR, 1.5f * S);
		RoundFrame(Outer, WithAlpha(AccentColor, 0.14f + 0.06f * Breath), OuterR, 1.0f * S);
		// a glint gliding round the outer ring, one lap every 20 s
		const FVector2D C = LogoRect.GetCenter();
		const float A = (float)FMath::Fmod(T * 0.314, 2.0 * PI);
		for (int32 Tail = 0; Tail < 3; ++Tail)
		{
			const float TA = A - Tail * 0.16f;
			const FVector2D P = C + FVector2D(FMath::Cos(TA), FMath::Sin(TA)) * (OuterR - 0.5f * S);
			const float Size = (4.5f - Tail * 1.2f) * S;
			Blob(P, FVector2D(Size, Size), 0.0f, FLinearColor(0.8f, 1.0f, 1.0f, 0.5f - Tail * 0.15f), true);
		}
	}
	Icon(TDIcons::Misc(ETDMiscIcon::Logo), LogoRect);
	// a few bubbles rising from the logo's shoulder, bobbing very slightly
	{
		struct FBubbleSpot { float DX; float DY; float Diameter; };
		static const FBubbleSpot Bubbles[] = { { 6.0f, -14.0f, 7.0f }, { 12.0f, -23.0f, 5.0f }, { 8.0f, -30.0f, 3.5f } };
		for (int32 i = 0; i < UE_ARRAY_COUNT(Bubbles); ++i)
		{
			const FBubbleSpot& Spot = Bubbles[i];
			const float Bob = (float)FMath::Sin(T * 0.5 + i * 1.3) * 1.0f * S;
			Bubble(FVector2D(LogoRect.Max.X + Spot.DX * S, MidY + Spot.DY * S + Bob), Spot.Diameter * 0.5f * S, AccentColor);
		}
	}
	const float TX = X + LogoSize + 16.0f * S;
	// the name in bold over a faint glow, with a lighter sheen pass; the subtitle above a slowly drifting wave
	Blob(FVector2D(TX + Title1W * 0.5f, MidY - 9.0f * S), FVector2D(Title1W + 24.0f * S, 26.0f * S), 0.0f, WithAlpha(AccentColor, 0.07f), true);
	{
		TGuardValue<bool> Bold(bBoldText, true);
		Text(Title1, TX, MidY - 9.0f * S, AccentColor, 19.0f * K, false, true);
		// the sheen breathes with the halo
		Text(Title1, TX, MidY - 9.0f * S - FMath::Max(1.0f, FMath::RoundToFloat(S)), FLinearColor(0.85f, 1.0f, 1.0f, 0.12f + 0.12f * Breath), 19.0f * K, false, true);
	}
	Text(Title2, TX, MidY + 11.0f * S, DimText, SizeCaption * K, false, true);
	WaveStrip(TX, TX + TitleColW, MidY + 19.0f * S, 6.0f * S, false, WithAlpha(AccentColor, 0.4f), (float)FMath::Fmod(T * 0.04, 1.0));
	// a little coral sprig where the wave ends
	// (far enough right that its branches clear the subtitle's last letter at small interface sizes)
	CoralSprig(FVector2D(TX + TitleColW + 15.0f * S, MidY + 26.0f * S), 22.0f * S, FLinearColor(1.0f, 0.45f, 0.5f, 0.8f));
	Bubble(FVector2D(TX + TitleColW + 21.0f * S, MidY - 2.0f * S), 3.0f * S, AccentColor, 1.6f);
	return Width;
}

void ATDHUD::DrawTopBar(ATDGameMode* GameMode, ATDPlayerController* PC)
{
	// three floating glass clusters: brand and resources (left), next wave (centre, see DrawWavePreview), controls (right)
	const float S = UIScale;
	const float W = Canvas->ClipX;
	const float Margin = 14.0f * S;
	const float Top = 12.0f * S;
	const float ClusterH = TopClusterH * S;
	const float MidY = Top + ClusterH * 0.5f;
	const float Inner = 10.0f * S;

	// a narrow screen drops the key caps from the buttons
	const bool bCompact = W / S < 1300.0f;

	// ---- right cluster: storage, speed, menu and the gold wave button ----
	struct FBarButton
	{
		FString Label;
		FString Key;
		EButton Id;
		FLinearColor Base;
		bool bHighlighted;
		float Width;
	};
	TArray<FBarButton> BarButtons;
	{
		const int32 Next = GameMode->GetWave() + 1;
		const bool bFinalWaveOut = !GameMode->IsEndless() && GameMode->GetWave() >= TD::FinalWave;
		const FString WaveLabel = bFinalWaveOut
			? FString(TEXT("Final wave"))
			: GameMode->IsWaveActive()
			? FString::Printf(TEXT("Send wave %d early  +%d"), Next, GameMode->GetEarlyBonus())
			: FString::Printf(TEXT("Release wave %d"), Next);
		BarButtons.Add({ FString::Printf(TEXT("Storage  %d/%d"), GameMode->GetStorage().Num(), GameMode->GetStorageCapacity()), GameMode->GetKeyName(ETDBind::Storage), EButton::StorageToggle, ButtonColor, PC->IsStorageOpen(), 0.0f });
		BarButtons.Add({ FString::Printf(TEXT("Speed %dx"), GameMode->GetGameSpeed()), GameMode->GetKeyName(ETDBind::Speed), EButton::Speed, ButtonColor, GameMode->GetGameSpeed() > 1, 0.0f });
		BarButtons.Add({ TEXT("Menu"), FString::Printf(TEXT("Esc/%s"), *GameMode->GetKeyName(ETDBind::Pause)), EButton::Pause, ButtonColor, PC->IsPaused(), 0.0f });
		BarButtons.Add({ WaveLabel, GameMode->GetKeyName(ETDBind::Wave), EButton::Wave, bFinalWaveOut ? ButtonColor : AmberButton, false, 0.0f });
	}
	const float BtnH = 42.0f * S;
	const float BtnGap = 8.0f * S;
	const float BtnPad = 14.0f * S;
	float RightW = Inner * 2.0f - BtnGap;
	for (FBarButton& Btn : BarButtons)
	{
		const float KeyW = bCompact ? 0.0f : FMath::Max(20.0f * S, MeasureText(Btn.Key, SizeCaption).X + 10.0f * S) + 8.0f * S;
		// the wave call stands out by a slightly larger regular face (full bold read as too heavy)
		Btn.Width = BtnPad * 2.0f + MeasureText(Btn.Label, Btn.Id == EButton::Wave ? SizeBody + 1.0f : SizeBody).X + KeyW;
		RightW += Btn.Width + BtnGap;
	}
	TopRightRect = FBox2D(FVector2D(W - Margin - RightW, Top), FVector2D(W - Margin, Top + ClusterH));
	FramedPanel(TopRightRect, PanelEdge);
	{
		float BX = TopRightRect.Min.X + Inner;
		const float BY = MidY - BtnH * 0.5f;
		for (const FBarButton& Btn : BarButtons)
		{
			const FBox2D Rect(FVector2D(BX, BY), FVector2D(BX + Btn.Width, BY + BtnH));
			Button(Rect, TEXT(""), Btn.Id, 0, Btn.Base, true, Btn.bHighlighted, SizeBody);
			const bool bLight = IsLightFill(Btn.Base);
			float LabelW = 0.0f;
			{
				const FLinearColor LabelColor = bLight ? DarkText : FLinearColor::White;
				LabelW = Text(Btn.Label, BX + BtnPad, MidY, LabelColor, Btn.Id == EButton::Wave ? SizeBody + 1.0f : SizeBody, false, true).X;
			}
			if (!bCompact)
			{
				KeyCap(Btn.Key, BX + BtnPad + LabelW + 8.0f * S, MidY, bLight);
			}
			BX += Btn.Width + BtnGap;
		}
	}

	// ---- left cluster: logo, then pearls, lives and wave ----
	const float StatIcon = 30.0f * S;
	const float ValueSize = SizeHeading;
	const FString PearlText = FString::Printf(TEXT("%d"), GameMode->GetPearls());
	const FString LivesText = FString::Printf(TEXT("%d"), GameMode->GetLives());
	const bool bWaveActive = GameMode->IsWaveActive();
	const int32 WaveNumber = bWaveActive ? GameMode->GetCurrentWaveNumber() : GameMode->GetWave();
	const FString WaveText = FString::Printf(TEXT("%d"), WaveNumber);
	// "/ 100" until the final wave is beaten, then "endless"
	const FString WaveGoal = GameMode->IsEndless() ? FString(TEXT(" endless")) : FString::Printf(TEXT(" / %d"), TD::FinalWave);
	const float WaveBarW = 120.0f * S;

	if (GameMode->IsShowingFps())
	{
		// smoothed real frame rate, refreshed twice a second so it stays readable
		const float Delta = FMath::Max((float)FApp::GetDeltaTime(), 0.0001f);
		FpsSmoothed = (FpsSmoothed <= 0.0f) ? 1.0f / Delta : FMath::Lerp(FpsSmoothed, 1.0f / Delta, 0.1f);
		if (Now() - FpsShownAt > 0.5)
		{
			FpsShown = FpsSmoothed;
			FpsShownAt = Now();
		}
	}
	const FString FpsText = FString::Printf(TEXT("%d"), FMath::RoundToInt(FpsShown));

	const float StatPad = 14.0f * S;
	auto StatWidth = [this, StatIcon, ValueSize, S](const FString& Value, const FString& Caption, float MinValueW)
	{
		const float ValueW = FMath::Max(MinValueW, FMath::Max(MeasureText(Value, ValueSize).X, MeasureText(Caption, SizeCaption).X));
		return StatIcon + 8.0f * S + ValueW;
	};
	// the progress percent sits at the right end of the number's row, so room is kept for "100%" after the
	// goal (always, so the panel doesn't change width when a wave starts; " endless" at wave 100+ needs it)
	const float WavePercentRoom = 10.0f * S + MeasureText(TEXT("100%"), SizeCaption).X;
	const float WaveValueW = FMath::Max(WaveBarW, MeasureText(WaveText, ValueSize).X + MeasureText(WaveGoal, SizeSmall).X + WavePercentRoom);
	float StatsW = StatWidth(PearlText, TEXT("PEARLS"), 0.0f) + StatWidth(LivesText, TEXT("REEF LIVES"), 0.0f) + StatIcon + 8.0f * S + WaveValueW + StatPad * 2.0f;
	if (GameMode->IsShowingFps())
	{
		StatsW += StatPad + FMath::Max(MeasureText(FpsText, ValueSize).X, MeasureText(TEXT("FPS"), SizeCaption).X) + 4.0f * S;
	}

	// the title goes first when the bar gets crowded (small window or large interface size)
	const float BrandW = DrawBrand(0.0f, MidY, S, false) + 16.0f * S;	// + room for the divider
	const bool bShowBrand = Margin + Inner * 2.0f + BrandW + StatsW + 24.0f * S < TopRightRect.Min.X;
	// the same 4 px of extra padding on both ends, plus the gap after the brand divider
	const float LeftW = Inner * 2.0f + 8.0f * S + (bShowBrand ? BrandW + 6.0f * S : 0.0f) + StatsW;

	TopLeftRect = FBox2D(FVector2D(Margin, Top), FVector2D(Margin + LeftW, Top + ClusterH));
	FramedPanel(TopLeftRect, PanelEdge);
	float X = TopLeftRect.Min.X + Inner + 4.0f * S;

	if (bShowBrand)
	{
		DrawBrand(X, MidY, S);
		X += BrandW;
		// divider
		Fill(FBox2D(FVector2D(X - 8.0f * S, Top + 14.0f * S), FVector2D(X - 8.0f * S + FMath::Max(1.0f, FMath::RoundToFloat(S)), Top + ClusterH - 14.0f * S)), WithAlpha(PanelEdge, 0.25f));
		X += 6.0f * S;
	}

	// icon, value and a small caption underneath; hovering shows what it means
	auto Stat = [this, &X, MidY, StatIcon, ValueSize, S](ETDMiscIcon Which, const FString& Value, const FLinearColor& Color, const FString& Caption, float MinValueW, const TCHAR* Title, const FString& Help)
	{
		const float X0 = X;
		Icon(TDIcons::Misc(Which), FBox2D(FVector2D(X, MidY - StatIcon * 0.5f), FVector2D(X + StatIcon, MidY + StatIcon * 0.5f)));
		X += StatIcon + 8.0f * S;
		const float ValueW = Text(Value, X, MidY - 8.0f * S, Color, ValueSize, false, true).X;
		Text(Caption, X, MidY + 13.0f * S, DimText, SizeCaption, false, true);
		X += FMath::Max(MinValueW, FMath::Max(ValueW, MeasureText(Caption, SizeCaption).X));
		if (FBox2D(FVector2D(X0, MidY - StatIcon * 0.6f), FVector2D(X, MidY + StatIcon * 0.6f)).IsInside(MousePos))
		{
			SetTip({ { Title, Color }, { Help, FLinearColor::White } });
		}
	};

	Stat(ETDMiscIcon::Pearl, PearlText, PearlColor, TEXT("PEARLS"), 0.0f, TEXT("Pearls"), TEXT("Spend them on towers and skills. Earned from kills and waves."));
	X += StatPad;
	{
		const float FlashAge = (float)(Now() - LivesFlashStart);
		const bool bFlash = FlashAge < 1.5f;
		const FLinearColor LivesColor = bFlash ? FMath::Lerp(FLinearColor(1.0f, 0.15f, 0.15f), LifeColor, FlashAge / 1.5f) : LifeColor;
		const float LivesX = X + StatIcon + 8.0f * S + MeasureText(LivesText, ValueSize).X;
		Stat(ETDMiscIcon::Life, LivesText, LivesColor, TEXT("REEF LIVES"), 0.0f, TEXT("Reef lives"), TEXT("Lost when creatures reach the reef. The game ends at zero."));
		if (bFlash)
		{
			Text(FString::Printf(TEXT("-%d"), LivesFlashAmount), LivesX + 6.0f * S, MidY - 8.0f * S - FlashAge * 8.0f * S, WithAlpha(FLinearColor(1.0f, 0.25f, 0.25f), 1.0f - FlashAge / 1.5f), SizeBody, false, true);
		}
		X += StatPad;
	}

	// wave: number, goal and a progress bar while creatures are out
	{
		const float X0 = X;
		Icon(TDIcons::Misc(ETDMiscIcon::Wave), FBox2D(FVector2D(X, MidY - StatIcon * 0.5f), FVector2D(X + StatIcon, MidY + StatIcon * 0.5f)));
		X += StatIcon + 8.0f * S;
		const float NumW = Text(WaveText, X, MidY - 8.0f * S, FLinearColor::White, ValueSize, false, true).X;
		Text(WaveGoal, X + NumW, MidY - 6.0f * S, DimText, SizeSmall, false, true);
		if (bWaveActive)
		{
			const float Progress = FMath::Clamp(GameMode->GetCurrentWaveProgress(), 0.0f, 1.0f);
			const FString Percent = FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Progress * 100));
			Text(Percent, X + WaveValueW - MeasureText(Percent, SizeCaption).X, MidY - 7.0f * S, AccentColor, SizeCaption, false, true);
			const FBox2D Track(FVector2D(X, MidY + 11.0f * S), FVector2D(X + WaveValueW, MidY + 17.0f * S));
			RoundFill(Track, TrackColor, 3.0f * S);
			if (Progress > 0.0f)
			{
				RoundFill(FBox2D(Track.Min, FVector2D(Track.Min.X + FMath::Max(6.0f * S, Track.GetSize().X * Progress), Track.Max.Y)), AccentColor, 3.0f * S);
			}
		}
		else
		{
			Text(TEXT("WAVE"), X, MidY + 13.0f * S, DimText, SizeCaption, false, true);
		}
		X += WaveValueW;
		if (FBox2D(FVector2D(X0, MidY - StatIcon * 0.6f), FVector2D(X, MidY + StatIcon * 0.6f)).IsInside(MousePos))
		{
			SetTip({ { TEXT("Wave"), FLinearColor::White }, { TEXT("Waves released so far, with progress through the latest one. Every 10th wave brings a boss, and beating wave 100 wins the game. Endless mode follows, where creatures grow much stronger every wave."), FLinearColor::White } });
		}
	}

	if (GameMode->IsShowingFps())
	{
		X += StatPad;
		Text(FpsText, X, MidY - 8.0f * S, KelpColor, ValueSize, false, true);
		Text(TEXT("FPS"), X, MidY + 13.0f * S, DimText, SizeCaption, false, true);
	}
}

float ATDHUD::DockTop() const
{
	// top of the dock tray (cards start 10 below it, and sit 18 above the screen edge)
	return Canvas ? Canvas->ClipY - DockSlotHeight * UIScale - 28.0f * UIScale : 0.0f;
}

void ATDHUD::DrawDock(ATDGameMode* GameMode, ATDPlayerController* PC)
{
	const float S = UIScale;
	const int32 Count = TD::TowerTypeCount;
	const float Gap = 7.0f * S;
	// support towers get their own group, set apart by a divider
	int32 FirstSupport = -1;
	for (int32 i = 0; i < Count; ++i)
	{
		if (TD::Tower((ETDTowerType)i).bSupport)
		{
			FirstSupport = i;
			break;
		}
	}
	const float GroupGap = FirstSupport > 0 ? 14.0f * S : 0.0f;
	const float SlotW = FMath::Min(150.0f * S, (Canvas->ClipX - 60.0f * S - GroupGap) / Count - Gap);
	const float SlotH = DockSlotHeight * S;
	const float TotalW = Count * SlotW + (Count - 1) * Gap + GroupGap;
	const float X0 = (Canvas->ClipX - TotalW) * 0.5f;
	const float Y0 = Canvas->ClipY - SlotH - 18.0f * S;

	const FBox2D DockRect(FVector2D(X0 - 10.0f * S, Y0 - 10.0f * S), FVector2D(X0 + TotalW + 10.0f * S, Y0 + SlotH + 10.0f * S));
	FramedPanel(DockRect, PanelEdge);

	for (int32 i = 0; i < Count; ++i)
	{
		const ETDTowerType Type = (ETDTowerType)i;
		const FTDTowerDef& Def = TD::Tower(Type);
		const int32 Cost = TD::TowerCost(Type);
		const bool bBanned = Type == ETDTowerType::Wall && !TD::Run().CanBuildWalls();	// No Walls (mutator)
		const bool bAffordable = GameMode->GetPearls() >= Cost && !bBanned;
		const bool bSelected = PC->GetSelectedType() == Type;

		float SX = X0 + i * (SlotW + Gap);
		if (FirstSupport > 0 && i >= FirstSupport)
		{
			SX += GroupGap;
		}
		if (i == FirstSupport && FirstSupport > 0)
		{
			// thin divider between the attack and support groups
			const float DX = FMath::RoundToFloat(SX - Gap * 0.5f - GroupGap * 0.5f);
			Fill(FBox2D(FVector2D(DX, Y0 + 14.0f * S), FVector2D(DX + FMath::Max(1.0f, FMath::RoundToFloat(S)), Y0 + SlotH - 14.0f * S)), WithAlpha(VioletColor, 0.35f));
		}

		// the selected card lifts a little
		const float Lift = bSelected ? 5.0f * S : 0.0f;
		const FBox2D Rect(FVector2D(SX, Y0 - Lift), FVector2D(SX + SlotW, Y0 + SlotH - Lift));
		const FLinearColor Base = !bAffordable ? DisabledButton : (Def.bSupport ? SupportButton : ButtonColor);
		Button(Rect, TEXT(""), EButton::Slot, i, Base, true, bSelected, SizeField);

		const FLinearColor NameColor = bAffordable ? FLinearColor::White : DimText;
		// hotkey in a small key cap
		{
			const FString Key = GameMode->GetKeyName(TDBind::TowerBind(Type));
			const float KeyH = 18.0f * S;
			const float KeyW = FMath::Max(KeyH, MeasureText(Key, SizeCaption).X + 8.0f * S);
			const FBox2D Cap(FVector2D(Rect.Min.X + 6.0f * S, Rect.Min.Y + 6.0f * S), FVector2D(Rect.Min.X + 6.0f * S + KeyW, Rect.Min.Y + 6.0f * S + KeyH));
			RoundFill(Cap, bSelected ? WithAlpha(AccentColor, 0.25f) : FLinearColor(0.3f, 0.75f, 0.85f, 0.10f), 5.0f * S);
			Text(Key, Cap.GetCenter().X, Cap.GetCenter().Y, bSelected ? AccentColor : (bAffordable ? DimText : DisabledText), SizeCaption, true, true);
		}
		const float IconSize = 44.0f * S;
		const FVector2D IconMin(Rect.GetCenter().X - IconSize * 0.5f, Rect.Min.Y + 7.0f * S);
		Icon(TDIcons::Tower(Type), FBox2D(IconMin, IconMin + FVector2D(IconSize, IconSize)), bAffordable ? FLinearColor::White : FLinearColor(0.55f, 0.55f, 0.55f, 0.7f));
		const FString Name = Def.Name;
		Text(Name, Rect.GetCenter().X, Rect.Min.Y + 64.0f * S, NameColor, FitSize(Name, SizeField, SlotW - 10.0f * S), true, true);

		// pearl icon + cost, centered as a group
		const FString CostText = bBanned ? FString(TEXT("-")) : FString::Printf(TEXT("%d"), Cost);
		const float CostIcon = 16.0f * S;
		const float CostW = CostIcon + 4.0f * S + MeasureText(CostText, SizeSmall).X;
		const float CostX = Rect.GetCenter().X - CostW * 0.5f;
		const float CostY = Rect.Min.Y + 87.0f * S;
		Icon(TDIcons::Misc(ETDMiscIcon::Pearl), FBox2D(FVector2D(CostX, CostY - CostIcon * 0.5f), FVector2D(CostX + CostIcon, CostY + CostIcon * 0.5f)), bAffordable ? FLinearColor::White : FLinearColor(0.55f, 0.55f, 0.55f, 0.8f));
		Text(CostText, CostX + CostIcon + 4.0f * S, CostY, bAffordable ? PearlColor : LifeColor, SizeSmall, false, true);

		if (Rect.IsInside(MousePos))
		{
			// stats one per line, each in its stat color
			TArray<TPair<FString, FLinearColor>> Tip = { { Def.Name, AccentColor }, { Def.Description, FLinearColor::White } };
			if (Def.bSupport)
			{
				Tip.Add({ FString::Printf(TEXT("Aura range %.1f tiles"), Def.Range), StatRange });
			}
			else if (Def.bAttacks)
			{
				Tip.Add({ FString::Printf(TEXT("Damage %s per hit (%s)"), *DamageRangeText(Def.Damage, TD::TowerDamageSpread(Type)), TD::DamageTypeName(Def.DamageType)), StatDamage });
				Tip.Add({ FString::Printf(TEXT("Range %.1f tiles"), Def.Range), StatRange });
				Tip.Add({ FString::Printf(TEXT("%.2f attacks per second"), Def.Rate), StatRate });
			}
			else
			{
				Tip.Add({ TEXT("Blocks the path"), DimText });
			}
			if (bBanned)
			{
				Tip.Add({ TEXT("No Walls: can't be built this run"), LifeColor });
			}
			else
			{
				Tip.Add({ bAffordable ? FString::Printf(TEXT("Cost: %d pearls  [%s]"), Cost, *GameMode->GetKeyName(TDBind::TowerBind(Type))) : FString::Printf(TEXT("Cost: %d pearls (need %d more)"), Cost, Cost - GameMode->GetPearls()), PearlColor });
			}
			SetTip(Tip, true);
		}
	}
	// (no controls hint above the dock: the full list is in the Controls menu)

	// a starfish and a scallop resting on the dock's top rim, clear of the cards, and a couple of bubbles above the shell
	{
		const double T = Now();
		const float Sway = (float)FMath::Sin(T * 0.25) * 4.0f;
		Starfish(FVector2D(DockRect.Min.X + 20.0f * S, DockRect.Min.Y + 1.0f * S), 11.0f * S, 12.0f + Sway, FLinearColor(1.0f, 0.5f, 0.32f, 0.9f));
		Scallop(FVector2D(DockRect.Max.X - 22.0f * S, DockRect.Min.Y + 4.0f * S), 11.0f * S, FLinearColor(1.0f, 0.8f, 0.62f, 0.85f));
		CoralSprig(FVector2D(DockRect.Min.X + 38.0f * S, DockRect.Min.Y + 2.0f * S), 17.0f * S, FLinearColor(1.0f, 0.4f, 0.6f, 0.75f));
		for (int32 i = 0; i < 2; ++i)
		{
			// rise 16 px over 9 s, then start again
			const float Age = (float)FMath::Fmod(T / 9.0 + i * 0.5, 1.0);
			const float Fade = FMath::Sin(Age * PI);
			Bubble(FVector2D(DockRect.Max.X - (20.0f - 4.0f * i) * S, DockRect.Min.Y - (9.0f + 16.0f * Age) * S), (2.0f + i * 1.0f) * S, AccentColor, Fade * 1.6f);
		}
	}
}

void ATDHUD::SetTip(const TArray<TPair<FString, FLinearColor>>& Lines, bool bColorizeEffects)
{
	TipLines = Lines;
	bTipColorize = bColorizeEffects;
}

void ATDHUD::DrawTooltip()
{
	LastTipTitle.Reset();
	if (MousePos.X < 0.0f)
	{
		return;
	}

	const float S = UIScale;

	/** One tooltip row: stat rows carry a small swatch in their stat color, comparison rows are indented under their heading */
	struct FTipRow
	{
		FString Str;
		FLinearColor Color;
		FLinearColor Mark = FLinearColor::Transparent;	// swatch color (transparent: none)
		bool bIndent = false;
		bool bMarkSpace = false;	// leave room for a swatch (continuation of a wrapped stat row)
		bool bColorize = false;		// effect words in their stat colors
	};
	TArray<FTipRow> Lines;
	auto AddRow = [&Lines](const FString& InStr, const FLinearColor& Color, const FLinearColor& Mark = FLinearColor::Transparent, bool bIndent = false)
	{
		FTipRow& Row = Lines.AddDefaulted_GetRef();
		Row.Str = InStr;
		Row.Color = Color;
		Row.Mark = Mark;
		Row.bIndent = bIndent;
		Row.bMarkSpace = Mark.A > 0.0f;
	};

	FLinearColor Border = AccentColor;
	if (TooltipItem.IsValid())
	{
		const ETDUnique Unique = TD::ItemUnique(TooltipItem);
		Border = Unique != ETDUnique::None ? UniqueColor : TD::RarityColor(TooltipItem.Rarity);
		AddRow(TooltipItem.Name, (Unique != ETDUnique::None ? UniqueColor : TD::RarityColor(TooltipItem.Rarity)) * 1.15f);
		AddRow(FString::Printf(TEXT("%s %s  -  item level %d"), Unique != ETDUnique::None ? TEXT("UNIQUE Legendary") : TD::RarityName(TooltipItem.Rarity), TD::SlotName(TooltipItem.Slot), TooltipItem.Level), Unique != ETDUnique::None ? UniqueColor : DimText);
		if (Unique != ETDUnique::None)
		{
			AddRow(TD::UniqueDescription(Unique), FLinearColor(1.0f, 0.9f, 0.7f));
			AddRow(TEXT("One of a kind: found at most once per run. Can be upgraded, not rerolled."), DimText);
		}
		const FString Flavor = TD::ItemFlavor(TooltipItem);
		if (!Flavor.IsEmpty())
		{
			AddRow(Flavor, FLinearColor(0.6f, 0.78f, 0.82f));
		}
		for (const FTDAffix& Affix : TooltipItem.Affixes)
		{
			// text in the stat's color; drawbacks in red (the swatch still names the stat); grey when it does nothing here
			const bool bApplies = TooltipContext == ETDTowerType::Count || TD::AffixApplies(Affix.Type, TooltipContext);
			const FLinearColor StatColor = AffixColor(Affix.Type);
			const FLinearColor Color = !bApplies ? FLinearColor(0.55f, 0.6f, 0.62f) : (Affix.Value < 0.0f ? StatLoss : StatColor);
			AddRow(TD::AffixText(Affix) + (bApplies ? FString() : FString(TEXT("  (no effect on this tower)"))), Color, bApplies ? StatColor : WithAlpha(StatColor * 0.5f, 0.6f));
		}
		const ATDTower* Tower = TooltipTower.Get();
		if (bTooltipFromStorage && Tower && Tower->CanProgress())
		{
			// what equipping it on the selected tower would change, with the item it replaces taken off
			const FTDItem& Current = Tower->GetEquipped(TooltipItem.Slot);
			TArray<FTDAffix> Changes;
			TD::CompareItems(TooltipItem, Current, Tower->GetTowerType(), Changes);
			AddRow(Current.IsValid() ? FString::Printf(TEXT("Instead of %s:"), *Current.Name) : FString::Printf(TEXT("%s slot is empty on this tower"), TD::SlotName(TooltipItem.Slot)),
				Current.IsValid() ? TD::RarityColor(Current.Rarity) : DimText);
			if (Changes.Num() == 0)
			{
				AddRow(TEXT("No change for this tower"), DimText, FLinearColor::Transparent, true);
			}
			for (const FTDAffix& Change : Changes)
			{
				// swatch: which stat, text: better (green) or worse (red)
				AddRow(TD::AffixDeltaText(Change), Change.Value > 0.0f ? StatGain : StatLoss, AffixColor(Change.Type), true);
			}
		}
		if (bTooltipFromStorage)
		{
			AddRow(FString::Printf(TEXT("Salvage value: %d pearls"), TD::SalvageValue(TooltipItem.Rarity)), PearlColor);
		}
	}
	else if (TipLines.Num() > 0)
	{
		for (int32 i = 0; i < TipLines.Num(); ++i)
		{
			AddRow(TipLines[i].Key, TipLines[i].Value);
			// only plain description rows: lines already in a stat color keep it
			const FLinearColor& C = TipLines[i].Value;
			Lines.Last().bColorize = bTipColorize && i > 0 && (C.Equals(FLinearColor::White) || C.Equals(DimText) || C.Equals(InfoText));
		}
		Border = TipLines[0].Value;
	}
	else
	{
		return;
	}

	LastTipTitle = Lines[0].Str;

	const float Pad = 14.0f * S;
	const float Line = 25.0f * S;
	const float IndentW = 14.0f * S;
	const float MarkSize = 9.0f * S;
	const float MarkW = MarkSize + 8.0f * S;
	auto RowOffset = [IndentW, MarkW](const FTipRow& Row)
	{
		return (Row.bIndent ? IndentW : 0.0f) + (Row.bMarkSpace ? MarkW : 0.0f);
	};

	// wrap long lines at word boundaries so the card stays a readable width
	{
		const float MaxTextW = 420.0f * S;
		TArray<FTipRow> Wrapped;
		for (int32 i = 0; i < Lines.Num(); ++i)
		{
			const float LineSize = i == 0 ? 18.0f : 16.0f;
			const FTipRow& Source = Lines[i];
			const float RowMaxW = MaxTextW - RowOffset(Source);
			if (MeasureText(Source.Str, LineSize).X <= RowMaxW)
			{
				Wrapped.Add(Source);
				continue;
			}
			TArray<FString> Words;
			Source.Str.ParseIntoArray(Words, TEXT(" "), true);
			FString Current;
			bool bFirst = true;
			auto Flush = [&Wrapped, &Source, &bFirst](const FString& InStr)
			{
				FTipRow Row = Source;
				Row.Str = InStr;
				if (!bFirst)
				{
					Row.Mark = FLinearColor::Transparent;	// continuation rows keep the swatch's space, not the swatch
				}
				Wrapped.Add(Row);
				bFirst = false;
			};
			for (const FString& Word : Words)
			{
				const FString Candidate = Current.IsEmpty() ? Word : Current + TEXT(" ") + Word;
				if (!Current.IsEmpty() && MeasureText(Candidate, LineSize).X > RowMaxW)
				{
					Flush(Current);
					Current = Word;
				}
				else
				{
					Current = Candidate;
				}
			}
			if (!Current.IsEmpty())
			{
				Flush(Current);
			}
		}
		Lines = MoveTemp(Wrapped);
	}

	// each row measured at the size it is drawn at, so the right padding matches the left; a small gap under the title
	// holds the wave, and the bottom padding is measured from the last row's text, so it matches the top
	const float TitleGap = Lines.Num() > 1 ? 6.0f * S : 0.0f;
	float W = 0.0f;
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		W = FMath::Max(W, RowOffset(Lines[i]) + MeasureText(Lines[i].Str, i == 0 ? 18.0f : 16.0f).X);
	}
	W += Pad * 2.0f;
	const float LastTextH = MeasureText(TEXT("Ag"), Lines.Num() > 1 ? 16.0f : 18.0f).Y;
	const float H = Pad * 2.0f + (Lines.Num() - 1) * Line + TitleGap + LastTextH;

	float X = MousePos.X + 18.0f * S;
	float Y = MousePos.Y + 18.0f * S;
	X = FMath::Max(4.0f, FMath::Min(X, Canvas->ClipX - W - 4.0f));
	Y = FMath::Max(4.0f, FMath::Min(Y, Canvas->ClipY - H - 4.0f));

	// rounded glass card, rimmed in the title's color (rarity for items), with a tinted title band
	const FBox2D Card(FVector2D(X, Y), FVector2D(X + W, Y + H));
	const float R = 12.0f * S;
	Glow(FBox2D(Card.Min + FVector2D(0, 6.0f * S), Card.Max + FVector2D(0, 6.0f * S)), ShadowColor, R, 14.0f * S);
	RoundFill(Card, TipColor, R);
	RoundFill(FBox2D(Card.Min, FVector2D(Card.Max.X, Y + Pad + Line + TitleGap * 0.5f)), WithAlpha(Border, 0.12f), R);
	if (DetailedUI())
	{
		DetailedCardEdge(Card, Border, R, 1.0f);
	}
	else
	{
		RoundFrame(Card, WithAlpha(Border, 0.85f), R, 1.5f * S);
	}
	// a thin wave under the title band instead of a straight edge
	if (Lines.Num() > 1)
	{
		WaveStrip(X + R, X + W - R, Y + Pad + Line + TitleGap * 0.5f - 2.5f * S, 5.0f * S, false, WithAlpha(Border, 0.35f), (float)FMath::Fmod(Now() * 0.03, 1.0));
	}
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const FTipRow& Row = Lines[i];
		const float Size = i == 0 ? 18.0f : 16.0f;
		const float RowX = X + Pad + (Row.bIndent ? IndentW : 0.0f);
		const float RowY = Y + Pad + i * Line + (i > 0 ? TitleGap : 0.0f);
		if (Row.Mark.A > 0.0f)
		{
			// a small rounded swatch, centered on the text's height
			const float MidY = RowY + MeasureText(Row.Str, Size).Y * 0.5f;
			const FBox2D Swatch(FVector2D(RowX, MidY - MarkSize * 0.5f), FVector2D(RowX + MarkSize, MidY + MarkSize * 0.5f));
			RoundFill(Snap(Swatch), Row.Mark, 2.5f * S);
		}
		if (Row.bColorize)
		{
			float PX = X + Pad + RowOffset(Row);
			for (const TPair<FString, FLinearColor>& Part : ColorizeEffects(Row.Str, Row.Color))
			{
				Text(Part.Key, PX, RowY, Part.Value, Size);
				PX += MeasureText(Part.Key, Size).X;
			}
		}
		else
		{
			Text(Row.Str, X + Pad + RowOffset(Row), RowY, Row.Color, Size);
		}
	}
}

void ATDHUD::ShowWaveClear(int32 WaveNumber, int32 PearlsGained, int32 Interest, int32 Xp, int32 LivesGained)
{
	WaveCard = { WaveNumber, PearlsGained, Interest, Xp, LivesGained, Now() };
}

void ATDHUD::DrawWaveClearCard(ATDGameMode* GameMode)
{
	const float Age = (float)(Now() - WaveCard.Start);
	if (Age >= WaveCardSeconds || Age < 0.0f || GameMode->GetMenu() != ETDMenu::None)
	{
		return;
	}
	const float S = UIScale;

	// what the clear gave: only the parts that apply
	struct FLine
	{
		UTexture2D* IconTexture;
		FString Text;
		FLinearColor Color;
	};
	TArray<FLine> Lines;
	if (WaveCard.Pearls > 0)
	{
		Lines.Add({ TDIcons::Misc(ETDMiscIcon::Pearl), FString::Printf(TEXT("+%d pearls"), WaveCard.Pearls), PearlColor });
	}
	if (WaveCard.Interest > 0)
	{
		Lines.Add({ TDIcons::Item(ETDItemSlot::Pearl), FString::Printf(TEXT("+%d interest"), WaveCard.Interest), PearlColor });
	}
	if (WaveCard.Xp > 0)
	{
		Lines.Add({ TDIcons::Misc(ETDMiscIcon::SkillPoint), FString::Printf(TEXT("+%d XP to every tower"), WaveCard.Xp), StatXp });
	}
	if (WaveCard.Lives > 0)
	{
		Lines.Add({ TDIcons::Misc(ETDMiscIcon::Life), FString::Printf(TEXT("+%d %s"), WaveCard.Lives, WaveCard.Lives == 1 ? TEXT("life") : TEXT("lives")), LifeColor });
	}
	const FString Title = FString::Printf(TEXT("Wave %d cleared"), WaveCard.Wave);
	const float Pad = 10.0f * S;
	const float LineH = 22.0f * S;
	const float IconW = 18.0f * S;
	float TextW = MeasureText(Title, SizeField).X;
	for (const FLine& Line : Lines)
	{
		TextW = FMath::Max(TextW, IconW + 6.0f * S + MeasureText(Line.Text, SizeSmall).X);
	}
	const float CardW = TextW + Pad * 2.0f;
	const float CardH = Pad * 2.0f + 22.0f * S + Lines.Num() * LineH;

	// beside the reef, kept on screen and clear of the top bar, the dock, storage and the side panel
	FVector2D Anchor(Canvas->ClipX * 0.75f, Canvas->ClipY * 0.5f);
	const FVector Projected = Project(TD::CellToWorld(TD::GoalCell(), 60.0f));
	if (Projected.Z > 0.0f)
	{
		Anchor = FVector2D(Projected.X, Projected.Y);
	}
	const float MinX = (StorageRect.bIsValid ? StorageRect.Max.X : 0.0f) + 14.0f * S;
	const float MaxX = Canvas->ClipX - 450.0f * S - CardW;
	const float MinY = (TopLeftRect.bIsValid ? TopLeftRect.Max.Y : 80.0f * S) + 50.0f * S;
	const float MaxY = DockTop() - CardH - 12.0f * S;
	float X = FMath::Clamp(Anchor.X - CardW - 40.0f * S, MinX, FMath::Max(MinX, MaxX));
	const float Y = FMath::Clamp(Anchor.Y - CardH * 0.5f, MinY, FMath::Max(MinY, MaxY));

	// slide in from the right over 0.25 s, hold, fade over the last 0.4 s
	const float In = FMath::Clamp(Age / 0.25f, 0.0f, 1.0f);
	const float Alpha = FMath::Min(In, FMath::Clamp(((float)WaveCardSeconds - Age) / 0.4f, 0.0f, 1.0f));
	X += (1.0f - FMath::InterpEaseOut(0.0f, 1.0f, In, 2.0f)) * 40.0f * S;

	const FBox2D Card(FVector2D(X, Y), FVector2D(X + CardW, Y + CardH));
	RoundFill(Card, WithAlpha(PanelColor, PanelColor.A * Alpha), RadiusButton * S);
	if (DetailedUI())
	{
		DetailedCardEdge(Card, KelpColor, RadiusButton * S, Alpha);
	}
	else
	{
		RoundFrame(Card, WithAlpha(KelpColor, 0.55f * Alpha), RadiusButton * S, 1.0f * S);
	}
	float LY = Y + Pad + 10.0f * S;
	Text(Title, X + Pad, LY, WithAlpha(KelpColor, Alpha), SizeField, false, true);
	LY += 22.0f * S;
	for (const FLine& Line : Lines)
	{
		Icon(Line.IconTexture, FBox2D(FVector2D(X + Pad, LY - IconW * 0.5f), FVector2D(X + Pad + IconW, LY + IconW * 0.5f)), FLinearColor(1.0f, 1.0f, 1.0f, Alpha));
		Text(Line.Text, X + Pad + IconW + 6.0f * S, LY, WithAlpha(Line.Color, Alpha), SizeSmall, false, true);
		LY += LineH;
	}
}

void ATDHUD::DrawToasts()
{
	const float S = UIScale;
	const double Time = Now();
	float Y = 84.0f * S;

	for (int32 i = Toasts.Num() - 1; i >= 0; --i)
	{
		if (Time - Toasts[i].Start > ToastDuration)
		{
			Toasts.RemoveAt(i);
		}
	}

	// below the next-wave card when it sits in the top row
	if (TopLeftRect.bIsValid)
	{
		Y = FMath::Max(Y, TopLeftRect.Max.Y + 12.0f * S);
	}
	Y = FMath::Max(Y, PreviewPillBottom + 8.0f * S);

	for (const FToast& Toast : Toasts)
	{
		const float Age = (float)(Time - Toast.Start);
		const float Alpha = FMath::Clamp((float)(ToastDuration - Age) / 0.5f, 0.0f, 1.0f);
		// slide down a little as it appears
		const float Slide = (1.0f - FMath::Clamp(Age / 0.2f, 0.0f, 1.0f)) * -8.0f * S;
		const FVector2D Extent = MeasureText(Toast.Text, 18.0f);
		const float W = Extent.X + 44.0f * S;
		const float H = Extent.Y + 14.0f * S;
		const float X = (Canvas->ClipX - W) * 0.5f;

		// pill with a rim and a dot in the message color
		const FBox2D Pill(FVector2D(X, Y + Slide), FVector2D(X + W, Y + Slide + H));
		RoundFill(Pill, WithAlpha(TipColor, 0.88f * Alpha), H * 0.5f);
		const bool bDetailedToast = DetailedUI();
		if (bDetailedToast)
		{
			DetailedCardEdge(Pill, Toast.Color, H * 0.5f, Alpha);
		}
		else
		{
			RoundFrame(Pill, WithAlpha(Toast.Color, 0.45f * Alpha), H * 0.5f, 1.0f * S);
		}
		const float Dot = 7.0f * S;
		const FBox2D DotRect(FVector2D(X + 14.0f * S, Pill.GetCenter().Y - Dot * 0.5f), FVector2D(X + 14.0f * S + Dot, Pill.GetCenter().Y + Dot * 0.5f));
		Glow(DotRect, WithAlpha(Toast.Color, 0.4f * Alpha), Dot * 0.5f, 4.0f * S);
		if (bDetailedToast)
		{
			Pearl(DotRect.GetCenter(), Dot * 0.6f, WithAlpha(Toast.Color, Alpha));
		}
		else
		{
			RoundFill(DotRect, WithAlpha(Toast.Color, Alpha), Dot * 0.5f);
		}
		Text(Toast.Text, Canvas->ClipX * 0.5f + 8.0f * S, Pill.GetCenter().Y, WithAlpha(Toast.Color, Alpha), 18.0f, true, true);
		// two tiny bubbles drifting up past the pill's right end
		for (int32 b = 0; b < 2; ++b)
		{
			const float Rise = FMath::Min(1.0f, Age / (2.5f + b * 0.8f));
			const FVector2D P(Pill.Max.X - (12.0f - 6.0f * b) * S, Pill.Min.Y + (2.0f - 10.0f * Rise - 4.0f * b) * S);
			Bubble(P, (2.0f - 0.6f * b) * S, Toast.Color, Alpha * (1.0f - 0.5f * Rise));
		}
		Y += H + 6.0f * S;
	}
}

void ATDHUD::DrawCursorHint(ATDGameMode* GameMode, ATDPlayerController* PC)
{
	const ATDGrid* Grid = GameMode->GetGrid();
	if (!Grid || !Grid->IsPreviewActive() || !PC->HasSelectedType() || MousePos.X < 0.0f)
	{
		return;
	}

	FString Hint;
	FLinearColor Color = PearlColor;
	int32 Cost = TD::TowerCost(PC->GetSelectedType());
	if (Grid->IsPreviewOnWall())
	{
		FIntPoint Cell;
		if (PC->GetHoveredCell(Cell))
		{
			Cost = FMath::Max(0, GameMode->GetBuildOnWallCost(Cell, PC->GetSelectedType()));
		}
		Hint = FString::Printf(TEXT("Build onto the wall: %d pearls"), Cost);
	}
	else switch (Grid->GetPreviewResult())
	{
	case ETDPlaceResult::Ok:
	{
		const int32 Delta = Grid->GetPreviewDelta();
		Hint = Delta > 0 ? FString::Printf(TEXT("Path +%d tiles"), Delta) : (Delta < 0 ? FString::Printf(TEXT("Path %d tiles"), Delta) : TEXT("Path unchanged"));
		break;
	}
	case ETDPlaceResult::BlocksPath:	Hint = TEXT("Would block the path"); Color = ErrorText; break;
	case ETDPlaceResult::TrapsEnemy:	Hint = TEXT("Would trap a creature"); Color = ErrorText; break;
	case ETDPlaceResult::EnemyInWay:	Hint = TEXT("Creature in the way"); Color = FLinearColor(1.0f, 0.55f, 0.35f); break;
	default:							return;
	}

	if (GameMode->GetPearls() < Cost)
	{
		Hint += TEXT("  (not enough pearls)");
	}

	{
		const FVector2D Extent = MeasureText(Hint, 18.0f);
		const float HX = MousePos.X + 22.0f * UIScale;
		const float HY = MousePos.Y + 16.0f * UIScale;
		const FBox2D Pill(FVector2D(HX - 10.0f * UIScale, HY - 4.0f * UIScale), FVector2D(HX + Extent.X + 10.0f * UIScale, HY + Extent.Y + 4.0f * UIScale));
		RoundFill(Pill, TipColor, Pill.GetSize().Y * 0.5f);
		if (DetailedUI())
		{
			DetailedCardEdge(Pill, Color, Pill.GetSize().Y * 0.5f, 1.0f);
		}
		else
		{
			RoundFrame(Pill, WithAlpha(Color, 0.5f), Pill.GetSize().Y * 0.5f, 1.0f * UIScale);
		}
		Text(Hint, HX, HY, Color, 18.0f);
	}
}
