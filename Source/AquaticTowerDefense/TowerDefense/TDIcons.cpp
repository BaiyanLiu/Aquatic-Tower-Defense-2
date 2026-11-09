// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDIcons.h"
#include "TDRun.h"
#include "TDEnemy.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Async/ParallelFor.h"
#include "HAL/IConsoleManager.h"
#include "AquaticTowerDefense.h"

namespace
{
	using FV = FVector2f;

	/** Set only while painting the Windows app icon: its output size, so the logo can drop details too fine to survive at 16-32 px */
	thread_local int32 AppIconOutSize = 0;

	FLinearColor Hex(uint32 RGB, float Alpha = 1.0f)
	{
		FLinearColor Color = FLinearColor::FromSRGBColor(FColor((RGB >> 16) & 0xFF, (RGB >> 8) & 0xFF, RGB & 0xFF));
		Color.A = Alpha;
		return Color;
	}

	/** Scales a color's brightness, keeping its alpha */
	FLinearColor Tone(const FLinearColor& Color, float K)
	{
		return FLinearColor(Color.R * K, Color.G * K, Color.B * K, Color.A);
	}

	// ---- signed distances (negative inside), in icon units where the icon spans 0..1 ----

	float SdCircle(FV P, FV C, float R)
	{
		return (P - C).Size() - R;
	}

	float SdEllipse(FV P, FV C, float Rx, float Ry)
	{
		// cheap approximation, exact enough for small icons
		const FV Q = P - C;
		const float K0 = FV(Q.X / Rx, Q.Y / Ry).Size();
		const float K1 = FV(Q.X / (Rx * Rx), Q.Y / (Ry * Ry)).Size();
		return K1 > 1e-6f ? K0 * (K0 - 1.0f) / K1 : -FMath::Min(Rx, Ry);
	}

	float SdBox(FV P, FV Min, FV Max, float Round)
	{
		const FV C = (Min + Max) * 0.5f;
		const FV H = (Max - Min) * 0.5f;
		const FV Q(FMath::Abs(P.X - C.X) - H.X + Round, FMath::Abs(P.Y - C.Y) - H.Y + Round);
		return FV(FMath::Max(Q.X, 0.0f), FMath::Max(Q.Y, 0.0f)).Size() + FMath::Min(FMath::Max(Q.X, Q.Y), 0.0f) - Round;
	}

	float SdSegment(FV P, FV A, FV B)
	{
		const FV PA = P - A;
		const FV BA = B - A;
		const float H = FMath::Clamp(FV::DotProduct(PA, BA) / FMath::Max(FV::DotProduct(BA, BA), 1e-8f), 0.0f, 1.0f);
		return (PA - BA * H).Size();
	}

	float SdCapsule(FV P, FV A, FV B, float R)
	{
		return SdSegment(P, A, B) - R;
	}

	float SdPolyline(FV P, const TArray<FV>& Pts, float R)
	{
		float D = TNumericLimits<float>::Max();
		for (int32 i = 0; i + 1 < Pts.Num(); ++i)
		{
			D = FMath::Min(D, SdSegment(P, Pts[i], Pts[i + 1]));
		}
		return D - R;
	}

	/** Tube along a path with a radius that tapers from R0 to R1 */
	float SdRibbon(FV P, const TArray<FV>& Pts, float R0, float R1)
	{
		float D = TNumericLimits<float>::Max();
		const int32 Spans = Pts.Num() - 1;
		for (int32 i = 0; i < Spans; ++i)
		{
			const FV PA = P - Pts[i];
			const FV BA = Pts[i + 1] - Pts[i];
			const float H = FMath::Clamp(FV::DotProduct(PA, BA) / FMath::Max(FV::DotProduct(BA, BA), 1e-8f), 0.0f, 1.0f);
			D = FMath::Min(D, (PA - BA * H).Size() - FMath::Lerp(R0, R1, (i + H) / Spans));
		}
		return D;
	}

	float SdPoly(FV P, const TArray<FV>& Pts)
	{
		float D = TNumericLimits<float>::Max();
		bool bInside = false;
		for (int32 i = 0, j = Pts.Num() - 1; i < Pts.Num(); j = i++)
		{
			D = FMath::Min(D, SdSegment(P, Pts[j], Pts[i]));
			const FV A = Pts[i];
			const FV B = Pts[j];
			if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bInside = !bInside;
			}
		}
		return bInside ? -D : D;
	}

	/** Arc of a ring with round caps; angles in degrees, 0 = right, 90 = down (screen space) */
	float SdArc(FV P, FV C, float R, float Thick, float A0, float A1)
	{
		const FV Q = P - C;
		float Angle = FMath::RadiansToDegrees(FMath::Atan2(Q.Y, Q.X));
		while (Angle < A0) Angle += 360.0f;
		while (Angle > A0 + 360.0f) Angle -= 360.0f;
		if (Angle <= A1)
		{
			return FMath::Abs(Q.Size() - R) - Thick * 0.5f;
		}
		auto End = [&C, R](float A) { const float Rad = FMath::DegreesToRadians(A); return C + FV(FMath::Cos(Rad), FMath::Sin(Rad)) * R; };
		return FMath::Min((P - End(A0)).Size(), (P - End(A1)).Size()) - Thick * 0.5f;
	}

	TArray<FV> StarPoints(FV C, float Outer, float Inner, int32 Points, float RotDeg)
	{
		TArray<FV> Pts;
		for (int32 i = 0; i < Points * 2; ++i)
		{
			const float A = FMath::DegreesToRadians(RotDeg + i * 180.0f / Points);
			const float R = (i % 2 == 0) ? Outer : Inner;
			Pts.Add(C + FV(FMath::Cos(A), FMath::Sin(A)) * R);
		}
		return Pts;
	}

	/** Irregular rounded polygon used for rocks */
	TArray<FV> RockPoints(FV C, float R, int32 Seed, int32 Count = 9, float Jitter = 0.22f)
	{
		TArray<FV> Pts;
		for (int32 i = 0; i < Count; ++i)
		{
			const uint32 H = (uint32)(Seed * 7919 + i * 104729) * 2654435761u;
			const float Noise = (float)((H >> 8) & 0xFFFF) / 65535.0f;
			const float A = FMath::DegreesToRadians(i * 360.0f / Count + Seed * 17.0f);
			Pts.Add(C + FV(FMath::Cos(A), FMath::Sin(A) * 0.85f) * R * (1.0f - Jitter + Jitter * Noise));
		}
		return Pts;
	}

	/** Catmull-Rom through the points; bClosed joins the last point back to the first */
	TArray<FV> Smooth(const TArray<FV>& In, bool bClosed = false, int32 Sub = 6)
	{
		TArray<FV> Out;
		const int32 N = In.Num();
		auto At = [&In, N, bClosed](int32 i) { return bClosed ? In[(i + N) % N] : In[FMath::Clamp(i, 0, N - 1)]; };
		const int32 Spans = bClosed ? N : N - 1;
		for (int32 i = 0; i < Spans; ++i)
		{
			const FV P0 = At(i - 1), P1 = At(i), P2 = At(i + 1), P3 = At(i + 2);
			for (int32 s = 0; s < Sub; ++s)
			{
				const float T = (float)s / Sub, T2 = T * T, T3 = T2 * T;
				Out.Add((P1 * 2.0f + (P2 - P0) * T + (P0 * 2.0f - P1 * 5.0f + P2 * 4.0f - P3) * T2 + (P1 * 3.0f - P0 - P2 * 3.0f + P3) * T3) * 0.5f);
			}
		}
		if (!bClosed)
		{
			Out.Add(In.Last());
		}
		return Out;
	}

	/** Position and unit tangent at T (0..1) along a path, by point index */
	void Along(const TArray<FV>& Path, float T, FV& OutPos, FV& OutTan)
	{
		const float F = FMath::Clamp(T, 0.0f, 1.0f) * (Path.Num() - 1);
		const int32 I = FMath::Min((int32)F, Path.Num() - 2);
		OutPos = FMath::Lerp(Path[I], Path[I + 1], F - I);
		OutTan = (Path[I + 1] - Path[I]).GetSafeNormal();
	}

	/** Smooth value noise in -1..1 */
	float Noise2(FV P)
	{
		auto Hash = [](int32 X, int32 Y)
		{
			uint32 H = (uint32)(X * 374761393 + Y * 668265263);
			H = (H ^ (H >> 13)) * 1274126177u;
			return (float)((H ^ (H >> 16)) & 0xFFFF) / 65535.0f;
		};
		const int32 X = FMath::FloorToInt(P.X), Y = FMath::FloorToInt(P.Y);
		const float FX = P.X - X, FY = P.Y - Y;
		const float SX = FX * FX * (3.0f - 2.0f * FX), SY = FY * FY * (3.0f - 2.0f * FY);
		const float V = FMath::Lerp(FMath::Lerp(Hash(X, Y), Hash(X + 1, Y), SX), FMath::Lerp(Hash(X, Y + 1), Hash(X + 1, Y + 1), SX), SY);
		return V * 2.0f - 1.0f;
	}

	/**
	 *  Paints shapes into a premultiplied linear buffer. Shapes are given in local units through a placement
	 *  (translate, rotate, uniform scale) so motifs can be reused at any position and angle. Each filled shape is
	 *  lit as a pillow raised from its own distance field, with a tinted ink line, so simple shapes read as
	 *  rounded painted forms. Mark paints flat detail (lines, spots), and FClip keeps detail inside a parent shape.
	 */
	class FPainter
	{
	public:
		/** bInDetailed paints the detailed look (see atd.DetailedIcons); InSize should then be twice the output size */
		explicit FPainter(int32 InSize, bool bInDetailed = false) : Size(InSize), bDetailed(bInDetailed)
		{
			Pixels.SetNumZeroed(Size * Size);
			Depth.SetNumUninitialized(Size * Size);
			BlockSize = FMath::Max(4, Size / 16);
		}

		struct FXf
		{
			FV T = FV(0.0f, 0.0f);
			float Cos = 1.0f;
			float Sin = 0.0f;
			float Scale = 1.0f;

			FV ToLocal(FV P) const
			{
				const FV V = P - T;
				return FV(V.X * Cos + V.Y * Sin, -V.X * Sin + V.Y * Cos) / Scale;
			}
		};

		/** How filled shapes are lit, in icon units */
		struct FLook
		{
			float Ink = 0.011f;		// tinted outline width
			float Dome = 0.06f;		// width of the rounded edge; large values give a full dome
			float Relief = 1.0f;	// strength of the lighting
			float Spec = 0.3f;		// specular highlight
			float Grain = 0.0f;		// mottled surface amount
			float GrainFreq = 24.0f;
		};

		/** Places local coordinates: local origin at C, rotated by RotDeg, scaled by S */
		struct FPlace
		{
			FPlace(FPainter& InP, FV C, float S, float RotDeg = 0.0f) : P(InP), Old(InP.Xf)
			{
				P.Xf.T = C;
				P.Xf.Cos = FMath::Cos(FMath::DegreesToRadians(RotDeg));
				P.Xf.Sin = FMath::Sin(FMath::DegreesToRadians(RotDeg));
				P.Xf.Scale = S;
			}
			~FPlace() { P.Xf = Old; }
			FPainter& P;
			FXf Old;
		};

		/** Restores the look at the end of a scope */
		struct FWith
		{
			explicit FWith(FPainter& InP) : P(InP), Old(InP.Look) {}
			~FWith() { P.Look = Old; }
			FPainter& P;
			FLook Old;
		};

		/** Keeps everything painted in this scope inside a shape */
		struct FClip
		{
			FClip(FPainter& InP, TFunctionRef<float(FV)> Sdf) : P(InP), Old(MoveTemp(InP.Mask))
			{
				TArray<float> New;
				New.SetNumZeroed(P.Size * P.Size);
				P.Blocks(Sdf, 2.0f, [this, &New](int32 X, int32 Y, float D)
				{
					const int32 I = Y * P.Size + X;
					New[I] = FMath::Clamp(0.5f - D, 0.0f, 1.0f) * (Old.Num() ? Old[I] : 1.0f);
				});
				P.Mask = MoveTemp(New);
			}
			~FClip() { P.Mask = MoveTemp(Old); }
			FPainter& P;
			TArray<float> Old;
		};

		/** Composites a lit shape over the image; bShade false paints it flat with no ink */
		void Fill(TFunctionRef<float(FV)> Sdf, const FLinearColor& Color, bool bShade = true)
		{
			if (!bShade)
			{
				Mark(Sdf, Color);
				return;
			}

			// signed distance of every pixel near the shape, in pixels
			const float Far = 1e6f;
			for (float& D : Depth)
			{
				D = Far;
			}
			const float ToPx = Size * FrameS;
			const bool bRich = bDetailed && !bMeasuring;
			// detailed shapes cast a shadow, so pixels a little further out need their distance too
			const float CastPx = bRich ? 0.018f * ToPx : 0.0f;
			float Deepest = 0.0f;
			int32 TopY = Size, BottomY = 0;
			Blocks(Sdf, 3.0f + CastPx * 2.2f, [this, &Deepest, &TopY, &BottomY](int32 X, int32 Y, float D)
			{
				Depth[Y * Size + X] = D;
				Deepest = FMath::Min(Deepest, D);
				if (D < 0.0f)
				{
					TopY = FMath::Min(TopY, Y);
					BottomY = FMath::Max(BottomY, Y);
				}
			});
			if (Deepest >= 0.0f)
			{
				return;
			}

			if (bRich)
			{
				CastShadow(FMath::Min(CastPx, -Deepest * 0.6f), Color.A);
			}

			const float Bevel = FMath::Max(1.0f, FMath::Min(Look.Dome * ToPx, -Deepest));
			// detailed (modern) ink is thinner and keeps the shape's own hue instead of going near black
			const float InkPx = FMath::Min(Look.Ink * ToPx * (bRich ? 0.7f : 1.0f), -Deepest * 0.55f);
			const FLinearColor InkColor = bRich
				? FLinearColor(Color.R * 0.3f + 0.006f, Color.G * 0.3f + 0.014f, Color.B * 0.34f + 0.03f)
				: FLinearColor(Color.R * 0.14f + 0.004f, Color.G * 0.14f + 0.008f, Color.B * 0.16f + 0.014f);
			// a clean top-to-bottom gradient across the whole shape, and a soft glassy sheen on its upper part
			const float InvSpan = 1.0f / FMath::Max(1, BottomY - TopY);
			const bool bGloss = bRich && -Deepest > 10.0f * (Size / 512.0f) && Look.Relief > 0.5f;
			const FVector3f Light = FVector3f(-0.45f, -0.65f, 0.61f).GetSafeNormal();
			const FVector3f Half = (Light + FVector3f(0.0f, 0.0f, 1.0f)).GetSafeNormal();
			const float InvSize = 1.0f / Size;
			// detailed: cool light bouncing up from the sea floor on the lower right, and a crisp catch light just inside top-left edges
			const FVector3f Bounce = FVector3f(0.55f, 0.75f, 0.25f).GetSafeNormal();
			const FLinearColor BounceColor = Hex(0x6FD6F2);
			const float Px = Size / 512.0f;	// one pixel of the reference 512 px canvas
			const bool bCatch = bRich && -Deepest > InkPx + 3.5f * Px;
			const float CatchAt = InkPx + 2.0f * Px;
			const float CatchWidth = 1.8f * Px;

			for (const FIntPoint& Block : Active)
			{
				const int32 X1 = FMath::Min(Block.X + BlockSize, Size);
				const int32 Y1 = FMath::Min(Block.Y + BlockSize, Size);
				for (int32 Y = Block.Y; Y < Y1; ++Y)
				{
					for (int32 X = Block.X; X < X1; ++X)
					{
						const int32 I = Y * Size + X;
						const float D = Depth[I];
						float Cover = FMath::Clamp(0.5f - D, 0.0f, 1.0f) * FMath::Clamp(Color.A, 0.0f, 1.0f);
						if (Mask.Num())
						{
							Cover *= Mask[I];
						}
						if (Cover <= 0.0f)
						{
							continue;
						}

						// outward direction of the edge, from the neighbouring distances
						auto Near = [this, D, Far](int32 NX, int32 NY)
						{
							const float V = Depth[FMath::Clamp(NY, 0, Size - 1) * Size + FMath::Clamp(NX, 0, Size - 1)];
							return V >= Far ? D + 1.0f : V;
						};
						FV Grad(Near(X + 1, Y) - Near(X - 1, Y), Near(X, Y + 1) - Near(X, Y - 1));
						Grad = Grad.GetSafeNormal();

						// a dome that falls away towards the edge, lit from the top left
						const float Edge = 1.0f - FMath::Clamp(-D / Bevel, 0.0f, 1.0f);
						const FVector3f N(Grad.X * Edge, Grad.Y * Edge, FMath::Sqrt(FMath::Max(0.0f, 1.0f - Edge * Edge)));
						const float Diffuse = FVector3f::DotProduct(N, Light);
						float K = FMath::Max(0.62f + 0.62f * Diffuse, 0.36f);
						K = FMath::Lerp(1.0f, K, Look.Relief);
						const FV Pixel((X + 0.5f) * InvSize, (Y + 0.5f) * InvSize);
						K *= 1.07f - 0.14f * Pixel.Y;
						if (Look.Grain > 0.0f)
						{
							const FV Q = Pixel * Look.GrainFreq;
							K *= 1.0f + Look.Grain * (Noise2(Q) * 0.65f + Noise2(Q * 2.3f + FV(7.1f, 3.3f)) * 0.35f);
						}
						if (bRich)
						{
							// fine painterly tooth on every surface
							const float T = (Y - TopY) * InvSpan;
							K *= FMath::Lerp(1.1f, 0.84f, T);
							const FV Q = Pixel * 70.0f;
							K *= 1.0f + 0.012f * Look.Relief * (Noise2(Q) * 0.6f + Noise2(Q * 2.7f + FV(3.7f, 9.1f)) * 0.4f);
						}

						// shadows lean cool, lights lean towards white
						FLinearColor Lit;
						if (K < 1.0f)
						{
							Lit = FLinearColor(Color.R * FMath::Pow(K, 1.2f), Color.G * K, Color.B * FMath::Pow(K, 0.85f));
						}
						else
						{
							const float W = (K - 1.0f) * 0.8f;
							Lit = FLinearColor(FMath::Lerp(Color.R, 1.0f, W), FMath::Lerp(Color.G, 1.0f, W), FMath::Lerp(Color.B, 1.0f, W));
						}
						const float Gleam = Look.Spec * Look.Relief * FMath::Pow(FMath::Max(FVector3f::DotProduct(N, Half), 0.0f), 42.0f);
						Lit.R += Gleam;
						Lit.G += Gleam;
						Lit.B += Gleam;

						if (bRich)
						{
							const float NH = FMath::Max(FVector3f::DotProduct(N, Half), 0.0f);
							const float Sheen = Look.Spec * Look.Relief * 0.22f * NH * NH * NH * NH * NH * NH;
							const float Up = FMath::Max(FVector3f::DotProduct(N, Bounce), 0.0f) * Edge * Edge * 0.13f * Look.Relief;
							Lit.R += Sheen + BounceColor.R * Up;
							Lit.G += Sheen + BounceColor.G * Up;
							Lit.B += Sheen + BounceColor.B * Up;
							if (bGloss)
							{
								const float T = (Y - TopY) * InvSpan;
								const float G = FMath::Clamp(1.0f - T / 0.42f, 0.0f, 1.0f) * (1.0f - Edge) * 0.09f * Look.Spec / 0.3f;
								Lit.R += G;
								Lit.G += G;
								Lit.B += G;
							}
							if (bCatch)
							{
								const float Facing = FMath::Max(0.0f, -(Grad.X * 0.6f + Grad.Y * 0.8f));
								const float Band = FMath::Max(0.0f, 1.0f - FMath::Abs(-D - CatchAt) / CatchWidth);
								const float Catch = Facing * Facing * Band * 0.6f * Look.Relief;
								Lit.R = FMath::Lerp(Lit.R, 1.0f, Catch);
								Lit.G = FMath::Lerp(Lit.G, 1.0f, Catch);
								Lit.B = FMath::Lerp(Lit.B, 1.0f, Catch);
							}
						}

						if (InkPx > 0.0f)
						{
							// detailed ink swells on the shadow side (lower right) and thins towards the light, like a brush line
							const float InkHere = bRich ? InkPx * (0.75f + 0.6f * FMath::Max(0.0f, Grad.X * 0.6f + Grad.Y * 0.8f)) : InkPx;
							const float InkMix = FMath::Clamp(D + InkHere + 0.5f, 0.0f, 1.0f) * 0.92f;
							Lit.R = FMath::Lerp(Lit.R, InkColor.R, InkMix);
							Lit.G = FMath::Lerp(Lit.G, InkColor.G, InkMix);
							Lit.B = FMath::Lerp(Lit.B, InkColor.B, InkMix);
						}

						FLinearColor& Dst = Pixels[I];
						Dst.R = Lit.R * Cover + Dst.R * (1.0f - Cover);
						Dst.G = Lit.G * Cover + Dst.G * (1.0f - Cover);
						Dst.B = Lit.B * Cover + Dst.B * (1.0f - Cover);
						Dst.A = Cover + Dst.A * (1.0f - Cover);
					}
				}
			}
		}

		/**
		 *  Darkens what is already painted with a soft copy of the shape in Depth, shifted down and to the right,
		 *  so stacked parts look layered. It never adds alpha, so it only lands on earlier shapes.
		 */
		void CastShadow(float ReachPx, float Opacity)
		{
			if (ReachPx < 0.75f)
			{
				return;
			}
			const int32 OX = FMath::RoundToInt(ReachPx * 0.5f);
			const int32 OY = FMath::RoundToInt(ReachPx * 0.85f);
			const float Blur = ReachPx * 0.8f + 1.0f;
			const float Strength = 0.38f * FMath::Clamp(Opacity, 0.0f, 1.0f);
			for (const FIntPoint& Block : Active)
			{
				const int32 X1 = FMath::Min(Block.X + BlockSize, Size);
				const int32 Y1 = FMath::Min(Block.Y + BlockSize, Size);
				for (int32 Y = Block.Y; Y < Y1; ++Y)
				{
					for (int32 X = Block.X; X < X1; ++X)
					{
						const int32 I = Y * Size + X;
						FLinearColor& Dst = Pixels[I];
						const int32 SX = X - OX;
						const int32 SY = Y - OY;
						if (Dst.A <= 0.0f || SX < 0 || SY < 0)
						{
							continue;
						}
						const float D = Depth[SY * Size + SX];
						float S = FMath::Clamp((Blur - D) / (2.0f * Blur), 0.0f, 1.0f);
						S = S * S * (3.0f - 2.0f * S) * Strength * (Mask.Num() ? Mask[I] : 1.0f);
						Dst.R *= 1.0f - S;
						Dst.G *= 1.0f - S * 0.92f;	// shade leans slightly blue-green, like deep water
						Dst.B *= 1.0f - S * 0.85f;
					}
				}
			}
		}

		/** Composites a flat shape; Soft (icon units) feathers its edge */
		void Mark(TFunctionRef<float(FV)> Sdf, const FLinearColor& Color, float Soft = 0.0f)
		{
			const float SoftPx = FMath::Max(1.0f, Soft * Size * FrameS);
			Blocks(Sdf, SoftPx + 2.0f, [this, &Color, SoftPx](int32 X, int32 Y, float D)
			{
				const int32 I = Y * Size + X;
				const float Cover = FMath::Clamp(0.5f - D / SoftPx, 0.0f, 1.0f) * FMath::Clamp(Color.A, 0.0f, 1.0f) * (Mask.Num() ? Mask[I] : 1.0f);
				if (Cover > 0.0f)
				{
					FLinearColor& Dst = Pixels[I];
					Dst.R = Color.R * Cover + Dst.R * (1.0f - Cover);
					Dst.G = Color.G * Cover + Dst.G * (1.0f - Cover);
					Dst.B = Color.B * Cover + Dst.B * (1.0f - Cover);
					Dst.A = Cover + Dst.A * (1.0f - Cover);
				}
			});
		}

		/** Adds light around a shape; Radius is in icon units and it is left out when framing the icon */
		void Glow(TFunctionRef<float(FV)> Sdf, const FLinearColor& Color, float Radius, float Strength = 0.8f)
		{
			if (bMeasuring)
			{
				return;
			}
			const float RadiusPx = FMath::Max(1.0f, Radius * Size * FrameS);
			Blocks(Sdf, RadiusPx + 2.0f, [this, &Color, RadiusPx, Strength](int32 X, int32 Y, float D)
			{
				const int32 I = Y * Size + X;
				const float F = 1.0f - FMath::Clamp(D / RadiusPx, 0.0f, 1.0f);
				const float G = F * F * Strength * (Mask.Num() ? Mask[I] : 1.0f);
				if (G > 0.0f)
				{
					FLinearColor& Dst = Pixels[I];
					Dst.R += Color.R * G;
					Dst.G += Color.G * G;
					Dst.B += Color.B * G;
					Dst.A = G + Dst.A * (1.0f - G);
				}
			});
		}

		/** Cuts a shape out of everything painted so far */
		void Erase(TFunctionRef<float(FV)> Sdf)
		{
			Blocks(Sdf, 2.0f, [this](int32 X, int32 Y, float D)
			{
				Pixels[Y * Size + X] *= 1.0f - FMath::Clamp(0.5f - D, 0.0f, 1.0f);
			});
		}

		// ---- primitives ----

		void Circle(FV C, float R, const FLinearColor& Color) { Fill([=](FV P) { return SdCircle(P, C, R); }, Color); }
		void Ellipse(FV C, float Rx, float Ry, const FLinearColor& Color) { Fill([=](FV P) { return SdEllipse(P, C, Rx, Ry); }, Color); }
		void Box(FV Min, FV Max, float Round, const FLinearColor& Color) { Fill([=](FV P) { return SdBox(P, Min, Max, Round); }, Color); }
		void Capsule(FV A, FV B, float R, const FLinearColor& Color) { Fill([=](FV P) { return SdCapsule(P, A, B, R); }, Color); }
		void Line(const TArray<FV>& Pts, float R, const FLinearColor& Color) { Fill([&Pts, R](FV P) { return SdPolyline(P, Pts, R); }, Color); }
		void Poly(const TArray<FV>& Pts, const FLinearColor& Color) { Fill([&Pts](FV P) { return SdPoly(P, Pts); }, Color); }
		void Arc(FV C, float R, float Thick, float A0, float A1, const FLinearColor& Color) { Fill([=](FV P) { return SdArc(P, C, R, Thick, A0, A1); }, Color); }
		void Ring(FV C, float R, float Thick, const FLinearColor& Color) { Fill([=](FV P) { return FMath::Abs(SdCircle(P, C, R)) - Thick * 0.5f; }, Color); }
		void Star(FV C, float Outer, float Inner, int32 Points, float RotDeg, const FLinearColor& Color) { Poly(StarPoints(C, Outer, Inner, Points, RotDeg), Color); }

		/** A smooth closed outline through the points, optionally puffed outwards */
		void Blob(const TArray<FV>& Pts, const FLinearColor& Color, float Puff = 0.0f)
		{
			const TArray<FV> Outline = Smooth(Pts, true, 5);
			Fill([&Outline, Puff](FV P) { return SdPoly(P, Outline) - Puff; }, Color);
		}

		/** A tube along a path whose radius tapers from R0 to R1 */
		void Ribbon(const TArray<FV>& Path, float R0, float R1, const FLinearColor& Color)
		{
			Fill([&Path, R0, R1](FV P) { return SdRibbon(P, Path, R0, R1); }, Color);
		}

		/** A sphere with a full dome */
		void Ball(FV C, float R, const FLinearColor& Color, float Spec = 0.6f)
		{
			FWith With(*this);
			Look.Dome = 10.0f;
			Look.Spec = Spec;
			Look.Ink = FMath::Min(Look.Ink, R * 0.22f);
			Circle(C, R, Color);
			if (bDetailed && R * Xf.Scale > 0.025f)
			{
				// a sharp sparkle in the highlight and a faint reflected rim along the bottom
				Mark([=](FV P) { return SdCircle(P, C + FV(-0.36f, -0.4f) * R, R * 0.09f); }, FLinearColor(1.0f, 1.0f, 1.0f, 0.85f * FMath::Min(1.0f, Spec * 1.6f)), 0.004f);
				Mark([=](FV P) { return FMath::Max(FMath::Abs(SdCircle(P, C, R * 0.8f)) - R * 0.035f, (C.Y + R * 0.25f) - P.Y); }, FLinearColor(1.0f, 1.0f, 1.0f, 0.18f), 0.006f);
			}
		}

		void MCircle(FV C, float R, const FLinearColor& Color) { Mark([=](FV P) { return SdCircle(P, C, R); }, Color); }
		void MEllipse(FV C, float Rx, float Ry, const FLinearColor& Color, float Soft = 0.0f) { Mark([=](FV P) { return SdEllipse(P, C, Rx, Ry); }, Color, Soft); }
		void MCapsule(FV A, FV B, float R, const FLinearColor& Color) { Mark([=](FV P) { return SdCapsule(P, A, B, R); }, Color); }
		void MLine(const TArray<FV>& Pts, float R, const FLinearColor& Color) { Mark([&Pts, R](FV P) { return SdPolyline(P, Pts, R); }, Color); }
		void MPoly(const TArray<FV>& Pts, const FLinearColor& Color) { Mark([&Pts](FV P) { return SdPoly(P, Pts); }, Color); }
		void MArc(FV C, float R, float Thick, float A0, float A1, const FLinearColor& Color) { Mark([=](FV P) { return SdArc(P, C, R, Thick, A0, A1); }, Color); }

		/** Small white gloss spot */
		void Gloss(FV C, float Rx, float Ry, float Alpha = 0.55f)
		{
			Mark([=](FV P) { return SdEllipse(P, C, Rx, Ry); }, FLinearColor(1.0f, 1.0f, 1.0f, Alpha), 0.012f);
		}

		/** Dark rim around everything painted, then converts to straight-alpha sRGB pixels at OutSize */
		void Finish(TArray<FColor>& Out, float OutlineWidth, int32 OutSize)
		{
			if (bDetailed && Size >= OutSize * 2 && Size % 2 == 0)
			{
				// the detailed canvas is painted at twice the size: average it down first, so the rim below costs the same
				HalveCanvas();
			}

			const int32 R = FMath::Max(1, FMath::RoundToInt(OutlineWidth * Size));
			TArray<float> Rim;
			Rim.SetNumZeroed(Size * Size);
			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					if (Pixels[Y * Size + X].A >= 0.995f)
					{
						continue;
					}
					float Best = 0.0f;
					for (int32 DY = -R; DY <= R && Best < 1.0f; ++DY)
					{
						for (int32 DX = -R; DX <= R; ++DX)
						{
							const float Dist = FMath::Sqrt((float)(DX * DX + DY * DY));
							const int32 SX = X + DX;
							const int32 SY = Y + DY;
							if (Dist > R + 0.5f || SX < 0 || SY < 0 || SX >= Size || SY >= Size)
							{
								continue;
							}
							// soften the rim's own edge
							const float Edge = FMath::Clamp(R + 0.5f - Dist, 0.0f, 1.0f);
							Best = FMath::Max(Best, Pixels[SY * Size + SX].A * Edge);
						}
					}
					Rim[Y * Size + X] = Best;
				}
			}

			// the modern rim is a deep sea teal rather than near black
			const FLinearColor RimColor = bDetailed ? Hex(0x072634) : Hex(0x03121A);
			for (int32 i = 0; i < Size * Size; ++i)
			{
				FLinearColor& Px = Pixels[i];
				const float Under = Rim[i] * 0.95f * (1.0f - Px.A);
				Px.R += RimColor.R * Under;
				Px.G += RimColor.G * Under;
				Px.B += RimColor.B * Under;
				Px.A += Under;
			}

			if (bDetailed)
			{
				Enrich();
				if (bDropShadow)
				{
					AddDropShadow();
				}
			}

			// box filter down in premultiplied linear space, so small icons stay clean
			Out.SetNumUninitialized(OutSize * OutSize);
			for (int32 Y = 0; Y < OutSize; ++Y)
			{
				const int32 Y0 = Y * Size / OutSize;
				const int32 Y1 = FMath::Max(Y0 + 1, (Y + 1) * Size / OutSize);
				for (int32 X = 0; X < OutSize; ++X)
				{
					const int32 X0 = X * Size / OutSize;
					const int32 X1 = FMath::Max(X0 + 1, (X + 1) * Size / OutSize);
					FLinearColor Sum(0, 0, 0, 0);
					for (int32 SY = Y0; SY < Y1; ++SY)
					{
						for (int32 SX = X0; SX < X1; ++SX)
						{
							Sum += Pixels[SY * Size + SX];
						}
					}
					Out[Y * OutSize + X] = Unpremultiply(Sum * (1.0f / ((Y1 - Y0) * (X1 - X0))));
				}
			}
		}

		/** Averages the canvas down to half size (premultiplied, so edges stay clean) */
		void HalveCanvas()
		{
			const int32 Half = Size / 2;
			TArray<FLinearColor> Small;
			Small.SetNumUninitialized(Half * Half);
			for (int32 Y = 0; Y < Half; ++Y)
			{
				for (int32 X = 0; X < Half; ++X)
				{
					const int32 I = Y * 2 * Size + X * 2;
					Small[Y * Half + X] = (Pixels[I] + Pixels[I + 1] + Pixels[I + Size] + Pixels[I + Size + 1]) * 0.25f;
				}
			}
			Pixels = MoveTemp(Small);
			Size = Half;
		}

		/** Detailed finish: a light unsharp mask for crisper surface detail, and a little more colour */
		void Enrich()
		{
			const int32 N = Size * Size;
			TArray<FLinearColor> Blur = Pixels;
			for (int32 Y = 1; Y + 1 < Size; ++Y)
			{
				for (int32 X = 1; X + 1 < Size; ++X)
				{
					const int32 I = Y * Size + X;
					Blur[I] = (Pixels[I] * 4.0f + Pixels[I - 1] * 2.0f + Pixels[I + 1] * 2.0f + Pixels[I - Size] * 2.0f + Pixels[I + Size] * 2.0f
						+ Pixels[I - Size - 1] + Pixels[I - Size + 1] + Pixels[I + Size - 1] + Pixels[I + Size + 1]) * (1.0f / 16.0f);
				}
			}
			for (int32 i = 0; i < N; ++i)
			{
				FLinearColor& Px = Pixels[i];
				if (Px.A < 0.98f || Blur[i].A < 0.98f)
				{
					continue;	// leave the silhouette edge alone, the rim handles it
				}
				FLinearColor C = Px + (Px - Blur[i]) * 0.45f;
				const float Luma = C.R * 0.2126f + C.G * 0.7152f + C.B * 0.0722f;
				C.R = FMath::Max(0.0f, Luma + (C.R - Luma) * 1.12f);
				C.G = FMath::Max(0.0f, Luma + (C.G - Luma) * 1.12f);
				C.B = FMath::Max(0.0f, Luma + (C.B - Luma) * 1.12f);
				Px = FLinearColor(C.R, C.G, C.B, Px.A);
			}
		}

		/** A soft dark shadow under the whole icon, a little below it, so it sits on the panel instead of floating flat */
		void AddDropShadow()
		{
			const int32 N = Size * Size;
			TArray<float> A, Tmp;
			A.SetNumUninitialized(N);
			Tmp.SetNumUninitialized(N);
			const int32 Shift = FMath::Max(1, FMath::RoundToInt(Size * 0.022f));
			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					A[Y * Size + X] = Y >= Shift ? Pixels[(Y - Shift) * Size + X].A : 0.0f;
				}
			}
			// two separable box passes approximate a gaussian
			const int32 Radius = FMath::Max(1, FMath::RoundToInt(Size * 0.018f));
			const float Inv = 1.0f / (2 * Radius + 1);
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				for (int32 Y = 0; Y < Size; ++Y)
				{
					for (int32 X = 0; X < Size; ++X)
					{
						float Sum = 0.0f;
						for (int32 K = -Radius; K <= Radius; ++K)
						{
							Sum += A[Y * Size + FMath::Clamp(X + K, 0, Size - 1)];
						}
						Tmp[Y * Size + X] = Sum * Inv;
					}
				}
				for (int32 Y = 0; Y < Size; ++Y)
				{
					for (int32 X = 0; X < Size; ++X)
					{
						float Sum = 0.0f;
						for (int32 K = -Radius; K <= Radius; ++K)
						{
							Sum += Tmp[FMath::Clamp(Y + K, 0, Size - 1) * Size + X];
						}
						A[Y * Size + X] = Sum * Inv;
					}
				}
			}
			for (int32 i = 0; i < N; ++i)
			{
				FLinearColor& Px = Pixels[i];
				const float Under = A[i] * 0.45f * (1.0f - Px.A);
				Px.A += Under;	// black, so the premultiplied colour gains nothing
			}
		}

		static FColor Unpremultiply(const FLinearColor& Px)
		{
			if (Px.A <= 1e-4f)
			{
				return FColor(0, 0, 0, 0);
			}
			FLinearColor Straight(Px.R / Px.A, Px.G / Px.A, Px.B / Px.A, FMath::Clamp(Px.A, 0.0f, 1.0f));
			Straight.R = FMath::Clamp(Straight.R, 0.0f, 1.0f);
			Straight.G = FMath::Clamp(Straight.G, 0.0f, 1.0f);
			Straight.B = FMath::Clamp(Straight.B, 0.0f, 1.0f);
			return Straight.ToFColor(true);
		}

		/** Bounds of everything painted, in 0..1 units (false if empty) */
		bool GetBounds(FV& OutMin, FV& OutMax) const
		{
			OutMin = FV(1.0f, 1.0f);
			OutMax = FV(0.0f, 0.0f);
			for (int32 Y = 0; Y < Size; ++Y)
			{
				for (int32 X = 0; X < Size; ++X)
				{
					if (Pixels[Y * Size + X].A > 0.05f)
					{
						OutMin = FV(FMath::Min(OutMin.X, (float)X / Size), FMath::Min(OutMin.Y, (float)Y / Size));
						OutMax = FV(FMath::Max(OutMax.X, (X + 1.0f) / Size), FMath::Max(OutMax.Y, (Y + 1.0f) / Size));
					}
				}
			}
			return OutMax.X > OutMin.X;
		}

		int32 Size;
		TArray<FLinearColor> Pixels;
		FXf Xf;
		FLook Look;

		/** Whole-icon placement applied on top of every shape: pixel = world * FrameS + FrameT */
		FV FrameT = FV(0.0f, 0.0f);
		float FrameS = 1.0f;

		/** True for the draft that only measures the icon */
		bool bMeasuring = false;

		/** Detailed look: cast shadows, catch and bounce light, surface tooth, a drop shadow, painted at twice the size */
		bool bDetailed = false;
		bool bDropShadow = true;

	private:
		/** Visits every pixel of the blocks a shape can reach, with its signed distance in pixels. Most shapes touch few blocks. */
		template <typename FVisit>
		void Blocks(TFunctionRef<float(FV)> Sdf, float ReachPx, FVisit&& Visit)
		{
			const float InvSize = 1.0f / Size;
			const float PxPerLocal = Xf.Scale * FrameS * Size;
			auto Eval = [this, &Sdf, InvSize, PxPerLocal](float X, float Y)
			{
				const FV World = (FV((X + 0.5f) * InvSize, (Y + 0.5f) * InvSize) - FrameT) / FrameS;
				return Sdf(Xf.ToLocal(World)) * PxPerLocal;
			};

			Active.Reset();
			// distances from approximate shapes can run a little long, hence the safety factor
			const float Keep = BlockSize * 0.71f * 1.6f + ReachPx;
			for (int32 BY = 0; BY < Size; BY += BlockSize)
			{
				for (int32 BX = 0; BX < Size; BX += BlockSize)
				{
					if (Eval(BX + BlockSize * 0.5f - 0.5f, BY + BlockSize * 0.5f - 0.5f) > Keep)
					{
						continue;
					}
					Active.Add(FIntPoint(BX, BY));
					const int32 X1 = FMath::Min(BX + BlockSize, Size);
					const int32 Y1 = FMath::Min(BY + BlockSize, Size);
					for (int32 Y = BY; Y < Y1; ++Y)
					{
						for (int32 X = BX; X < X1; ++X)
						{
							Visit(X, Y, Eval((float)X, (float)Y));
						}
					}
				}
			}
		}

		int32 BlockSize;
		TArray<float> Depth;
		TArray<float> Mask;
		TArray<FIntPoint> Active;
	};

	using FPlace = FPainter::FPlace;
	using FWith = FPainter::FWith;
	using FClip = FPainter::FClip;

	// ---- colors ----

	const FLinearColor White = Hex(0xF4FAFC);
	const FLinearColor Ink = Hex(0x14232C);
	const FLinearColor Wood = Hex(0xA8743F);
	const FLinearColor WoodDark = Hex(0x6E4A28);
	const FLinearColor WoodLight = Hex(0xCFA26A);
	const FLinearColor RopeColor = Hex(0xD9BE8A);
	const FLinearColor Steel = Hex(0xD6E0E6);
	const FLinearColor SteelDark = Hex(0x8E9CA6);
	const FLinearColor Iron = Hex(0x56646E);
	const FLinearColor IronDark = Hex(0x36424A);
	const FLinearColor Brass = Hex(0xD9A441);
	const FLinearColor Gold = Hex(0xF2C14E);
	const FLinearColor Poison = Hex(0x74D84A);
	const FLinearColor Blood = Hex(0xD9343C);
	const FLinearColor Volt = Hex(0xFFE24A);
	const FLinearColor Frost = Hex(0x8FD8FF);
	const FLinearColor Stone = Hex(0x949CA3);

	FLinearColor Alpha(const FLinearColor& Color, float A)
	{
		return FLinearColor(Color.R, Color.G, Color.B, A);
	}

	FV Spin(FV V, float Deg)
	{
		const float Rad = FMath::DegreesToRadians(Deg);
		const float C = FMath::Cos(Rad), S = FMath::Sin(Rad);
		return FV(V.X * C - V.Y * S, V.X * S + V.Y * C);
	}

	// ---- motifs, drawn around a local origin with unit size about 1 ----

	/** The round stone platform towers stand on, top surface centred at (Cx, Y) */
	void Plinth(FPainter& P, float Cx, float Y, float Rx)
	{
		const float Ry = Rx * 0.27f;
		const float H = 0.055f;
		{
			FWith With(P);
			P.Look.Grain = 0.1f;
			P.Look.Dome = 0.03f;
			P.Fill([=](FV Q) { return FMath::Min(SdEllipse(Q, FV(Cx, Y + H), Rx, Ry), SdBox(Q, FV(Cx - Rx, Y), FV(Cx + Rx, Y + H), 0.0f)); }, Hex(0x5E6A70));
			P.Ellipse(FV(Cx, Y), Rx, Ry, Hex(0x9AA6A8));
		}
		P.Mark([=](FV Q) { return FMath::Abs(SdEllipse(Q, FV(Cx, Y), Rx * 0.8f, Ry * 0.78f)) - 0.004f; }, Hex(0x5E6A70, 0.7f));
	}

	/** A short twisted rope, with its strands drawn across it */
	void Rope(FPainter& P, FV A, FV B, float R)
	{
		P.Capsule(A, B, R, RopeColor);
		const FV D = (B - A).GetSafeNormal();
		const FV N(-D.Y, D.X);
		const float Len = (B - A).Size();
		const int32 Count = FMath::Max(2, FMath::RoundToInt(Len / (R * 1.5f)));
		for (int32 i = 0; i < Count; ++i)
		{
			const FV C = FMath::Lerp(A, B, (i + 0.5f) / Count);
			P.MCapsule(C - N * R * 0.75f - D * R * 0.3f, C + N * R * 0.75f + D * R * 0.3f, R * 0.17f, Hex(0x7E5C30, 0.8f));
		}
	}

	void Rivet(FPainter& P, FV C, float R)
	{
		P.Ball(C, R, Hex(0xC4CDD2), 0.8f);
	}

	void Bubble(FPainter& P, FV C, float R)
	{
		P.Mark([=](FV Q) { return SdCircle(Q, C, R); }, Hex(0x8FE4FF, 0.3f));
		P.Mark([=](FV Q) { return FMath::Abs(SdCircle(Q, C, R * 0.93f)) - R * 0.09f; }, Hex(0xD4F6FF, 0.95f));
		P.Mark([=](FV Q) { return SdArc(Q, C, R * 0.62f, R * 0.14f, 150.0f, 250.0f); }, Hex(0xFFFFFF, 0.95f));
		P.MCircle(C + FV(R * 0.38f, R * 0.4f), R * 0.1f, Hex(0xFFFFFF, 0.7f));
	}

	void Drop(FPainter& P, FV C, float S, const FLinearColor& Color, float Rot = 0.0f)
	{
		FPlace Place(P, C, S, Rot);
		FWith With(P);
		P.Look.Dome = 10.0f;
		P.Look.Spec = 0.8f;
		const TArray<FV> Tip = { FV(0.0f, -0.48f), FV(0.27f, 0.02f), FV(-0.27f, 0.02f) };
		auto Shape = [&Tip](FV Q) { return FMath::Min(SdCircle(Q, FV(0.0f, 0.14f), 0.3f), SdPoly(Q, Tip)); };
		P.Fill(Shape, Color);
		FClip Clip(P, Shape);
		P.MEllipse(FV(0.06f, 0.27f), 0.2f, 0.09f, Alpha(Tone(Color, 1.7f), 0.5f), 0.04f);
		P.Gloss(FV(-0.11f, 0.08f), 0.055f, 0.11f, 0.8f);
		P.Gloss(FV(-0.05f, -0.18f), 0.02f, 0.05f, 0.5f);
	}

	void Bolt(FPainter& P, FV C, float S, const FLinearColor& Color, float Rot = 0.0f)
	{
		FPlace Place(P, C, S, Rot);
		const TArray<FV> Pts = { FV(0.12f, -0.5f), FV(-0.26f, 0.06f), FV(-0.01f, 0.06f), FV(-0.12f, 0.5f), FV(0.28f, -0.1f), FV(0.03f, -0.1f) };
		P.Glow([&Pts](FV Q) { return SdPoly(Q, Pts); }, Alpha(Tone(Color, 0.75f), 1.0f), 0.1f, 0.55f);
		P.Poly(Pts, Color);
		P.MPoly({ FV(0.07f, -0.34f), FV(-0.15f, 0.02f), FV(-0.04f, 0.02f), FV(0.03f, -0.14f) }, Hex(0xFFFFFF, 0.65f));
	}

	void Plus(FPainter& P, FV C, float S, const FLinearColor& Color)
	{
		FPlace Place(P, C, S);
		P.Fill([](FV Q) { return FMath::Min(SdCapsule(Q, FV(-0.4f, 0.0f), FV(0.4f, 0.0f), 0.13f), SdCapsule(Q, FV(0.0f, -0.4f), FV(0.0f, 0.4f), 0.13f)); }, Color);
	}

	/** Arrow pointing up (Rot 180 points down) */
	void Arrow(FPainter& P, FV C, float S, const FLinearColor& Color, float Rot = 0.0f)
	{
		FPlace Place(P, C, S, Rot);
		P.Poly({ FV(0.0f, -0.48f), FV(0.36f, -0.06f), FV(0.14f, -0.06f), FV(0.14f, 0.44f), FV(-0.14f, 0.44f), FV(-0.14f, -0.06f), FV(-0.36f, -0.06f) }, Color);
	}

	void StarIcon(FPainter& P, FV C, float S, const FLinearColor& Color)
	{
		FPlace Place(P, C, S);
		FWith With(P);
		P.Look.Dome = 10.0f;
		const TArray<FV> Pts = StarPoints(FV(0.0f, 0.02f), 0.5f, 0.23f, 5, -90.0f);
		P.Fill([&Pts](FV Q) { return SdPoly(Q, Pts) - 0.015f; }, Color);
	}

	/** Barbed steel head and shaft along +X, ring and rope at the tail */
	void Harpoon(FPainter& P, FV C, float S, float Rot)
	{
		FPlace Place(P, C, S, Rot);
		P.Ring(FV(-0.42f, 0.0f), 0.06f, 0.036f, Hex(0xD9C08A));
		P.Capsule(FV(-0.37f, 0.0f), FV(0.2f, 0.0f), 0.044f, Wood);
		P.MCapsule(FV(-0.34f, -0.016f), FV(0.1f, -0.016f), 0.009f, Alpha(WoodLight, 0.8f));
		// barbs swept back from the head
		P.Poly({ FV(0.24f, -0.02f), FV(0.06f, -0.17f), FV(0.1f, -0.2f), FV(0.33f, -0.05f) }, SteelDark);
		P.Poly({ FV(0.24f, 0.02f), FV(0.06f, 0.17f), FV(0.1f, 0.2f), FV(0.33f, 0.05f) }, SteelDark);
		const TArray<FV> Head = { FV(0.16f, -0.07f), FV(0.3f, -0.08f), FV(0.52f, 0.0f), FV(0.3f, 0.08f), FV(0.16f, 0.07f) };
		P.Poly(Head, Steel);
		P.MPoly({ FV(0.17f, -0.058f), FV(0.3f, -0.066f), FV(0.49f, 0.0f), FV(0.17f, 0.0f) }, Hex(0xFFFFFF, 0.55f));
		P.MCapsule(FV(0.18f, 0.0f), FV(0.48f, 0.0f), 0.006f, Hex(0x5E6C76, 0.9f));
		P.Box(FV(0.08f, -0.058f), FV(0.17f, 0.058f), 0.016f, Hex(0x6B4A2A));
		for (int32 i = 0; i < 3; ++i)
		{
			const float X = 0.095f + i * 0.03f;
			P.MCapsule(FV(X, -0.05f), FV(X + 0.012f, 0.05f), 0.007f, Hex(0xD9BE8A, 0.9f));
		}
	}

	void Rock(FPainter& P, FV C, float R, int32 Seed, const FLinearColor& Color)
	{
		const TArray<FV> Pts = Smooth(RockPoints(C, R, Seed), true, 3);
		FWith With(P);
		P.Look.Grain = 0.16f;
		P.Look.Dome = 10.0f;
		P.Look.Spec = 0.12f;
		P.Fill([&Pts](FV Q) { return SdPoly(Q, Pts) - 0.008f; }, Color);
		FClip Clip(P, [&Pts](FV Q) { return SdPoly(Q, Pts); });
		P.MCapsule(C + FV(-R * 0.1f, R * 0.05f), C + FV(R * 0.35f, R * 0.32f), R * 0.035f, Alpha(Tone(Color, 0.4f), 0.7f));
		P.MCapsule(C + FV(-R * 0.55f, R * 0.25f), C + FV(-R * 0.25f, R * 0.4f), R * 0.03f, Alpha(Tone(Color, 0.4f), 0.6f));
	}

	/** Glowing bell with a scalloped rim, inner lobes, frilled arms and trailing tentacles */
	void Jelly(FPainter& P, FV C, float S, const FLinearColor& Color)
	{
		FPlace Place(P, C, S);
		const FLinearColor Solid = Alpha(Color, 1.0f);
		const FLinearColor Light = Alpha(Tone(Solid, 1.45f), 1.0f);
		const FLinearColor Deep = Alpha(Tone(Solid, 0.55f), 1.0f);
		auto Bell = [](FV Q)
		{
			return FMath::Max(SdEllipse(Q, FV(0.0f, 0.04f), 0.36f, 0.39f), Q.Y - 0.04f);
		};
		P.Glow(Bell, Alpha(Tone(Solid, 0.75f), 1.0f), 0.2f, 0.6f);

		// long thin tentacles, then frilled arms in the middle
		for (int32 i = 0; i < 6; ++i)
		{
			const float X = -0.28f + i * 0.112f;
			const float W = (i % 2) ? -1.0f : 1.0f;
			const TArray<FV> Path = Smooth({ FV(X, 0.03f), FV(X + W * 0.05f, 0.15f), FV(X - W * 0.04f, 0.28f), FV(X + W * 0.04f, 0.41f), FV(X - W * 0.01f, 0.52f) });
			P.Ribbon(Path, 0.024f, 0.01f, Alpha(Tone(Solid, 0.82f), 1.0f));
		}
		for (int32 i = 0; i < 2; ++i)
		{
			const float X = i ? 0.075f : -0.075f;
			const TArray<FV> Path = Smooth({ FV(X, 0.03f), FV(X * 1.4f, 0.14f), FV(X * 0.7f, 0.25f), FV(X * 1.3f, 0.36f) });
			P.Ribbon(Path, 0.042f, 0.014f, Light);
		}

		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.55f;
			P.Fill(Bell, Solid);
		}
		FClip Clip(P, Bell);
		// translucent inner body with four lobes, and veins fanning towards the rim
		P.MEllipse(FV(0.0f, -0.05f), 0.2f, 0.15f, Alpha(Light, 0.6f), 0.05f);
		for (int32 i = 0; i < 4; ++i)
		{
			P.MCircle(FV(-0.135f + i * 0.09f, -0.05f + (i == 0 || i == 3 ? 0.03f : -0.02f)), 0.045f, Alpha(Deep, 0.55f));
			P.MCircle(FV(-0.135f + i * 0.09f, -0.055f + (i == 0 || i == 3 ? 0.03f : -0.02f)), 0.022f, Alpha(White, 0.75f));
		}
		for (int32 i = 0; i < 9; ++i)
		{
			const float X = -0.32f + i * 0.08f;
			P.MCapsule(FV(X * 0.45f, -0.18f), FV(X, 0.04f), 0.006f, Alpha(Deep, 0.5f));
		}
		// scalloped rim
		for (int32 i = 0; i < 8; ++i)
		{
			P.MCircle(FV(-0.315f + i * 0.09f, 0.03f), 0.04f, Alpha(Deep, 0.85f));
			P.MCircle(FV(-0.315f + i * 0.09f, 0.02f), 0.024f, Alpha(Light, 0.9f));
		}
		P.Gloss(FV(-0.15f, -0.21f), 0.1f, 0.05f, 0.85f);
		P.Gloss(FV(0.17f, -0.2f), 0.025f, 0.02f, 0.6f);
	}

	/** Front-on puffer fish: spines, eyes, pouted lips, side fins and a pale belly */
	void Puffer(FPainter& P, FV C, float S, float SpikeLength)
	{
		FPlace Place(P, C, S);
		// side fins
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const float F = (float)Side;
			P.Blob({ FV(F * 0.26f, 0.03f), FV(F * 0.46f, -0.05f), FV(F * 0.5f, 0.06f), FV(F * 0.42f, 0.14f), FV(F * 0.27f, 0.12f) }, Hex(0xF0A030));
			for (int32 i = 0; i < 3; ++i)
			{
				P.MCapsule(FV(F * 0.3f, 0.07f), FV(F * 0.45f, -0.02f + i * 0.06f), 0.006f, Hex(0x9A5A14, 0.8f));
			}
		}
		for (int32 i = 0; i < 18; ++i)
		{
			const FV Dir = Spin(FV(1.0f, 0.0f), i * 20.0f + 10.0f);
			const FV Perp(-Dir.Y, Dir.X);
			P.Poly({ Dir * (0.33f + SpikeLength), Dir * 0.28f + Perp * 0.05f, Dir * 0.28f - Perp * 0.05f }, Hex(0xD2A234));
		}
		const auto Body = [](FV Q) { return SdCircle(Q, FV(0.0f, 0.0f), 0.34f); };
		{
			FWith With(P);
			P.Look.Dome = 0.34f;
			P.Look.Spec = 0.4f;
			P.Fill(Body, Hex(0xF7CF45));
		}
		{
			FClip Clip(P, Body);
			P.MEllipse(FV(0.0f, 0.27f), 0.29f, 0.19f, Hex(0xFFF4C8, 0.95f), 0.03f);
			const FV Spots[] = { FV(-0.2f, -0.2f), FV(0.2f, -0.21f), FV(0.0f, -0.27f), FV(-0.09f, -0.2f), FV(0.1f, -0.17f), FV(-0.27f, -0.05f), FV(0.28f, -0.05f) };
			for (const FV& Spot : Spots)
			{
				P.MCircle(Spot, 0.024f, Hex(0x9A6A18, 0.85f));
			}
			// little belly spines
			for (int32 i = 0; i < 5; ++i)
			{
				const FV Base(-0.16f + i * 0.08f, 0.2f + (i % 2) * 0.04f);
				P.MPoly({ Base + FV(-0.015f, 0.0f), Base + FV(0.015f, 0.0f), Base + FV(0.0f, 0.03f) }, Hex(0xC8A040, 0.9f));
			}
		}
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const float F = (float)Side;
			P.Ball(FV(F * 0.14f, -0.06f), 0.095f, White, 0.2f);
			P.Ball(FV(F * 0.125f, -0.05f), 0.055f, Hex(0x18242C), 0.9f);
			P.MCircle(FV(F * 0.125f - 0.018f, -0.07f), 0.015f, Hex(0xFFFFFF, 0.95f));
			P.MCapsule(FV(F * 0.05f, -0.15f), FV(F * 0.23f, -0.18f), 0.012f, Hex(0xB57E20, 0.95f));
		}
		// pouted lips
		P.Ellipse(FV(0.0f, 0.11f), 0.08f, 0.055f, Hex(0xF2884A));
		P.MEllipse(FV(0.0f, 0.11f), 0.045f, 0.018f, Hex(0x7A2A1A));
		P.Gloss(FV(-0.02f, 0.078f), 0.03f, 0.009f, 0.6f);
	}

	/** Two-part eel body with a head, used by the burrow tower and the creature */
	void EelHead(FPainter& P, FV C, float Rot, float S, const FLinearColor& Body, const FLinearColor& Belly, const FLinearColor& EyeColor)
	{
		FPlace Place(P, C, S, Rot);
		// lower jaw, open mouth, upper jaw
		P.Blob({ FV(-0.14f, 0.03f), FV(0.05f, 0.06f), FV(0.2f, 0.09f), FV(0.05f, 0.13f), FV(-0.14f, 0.11f) }, Belly);
		P.Poly({ FV(-0.08f, -0.01f), FV(0.2f, 0.0f), FV(0.18f, 0.08f), FV(-0.08f, 0.06f) }, Hex(0x5A1422));
		for (int32 i = 0; i < 3; ++i)
		{
			const float X = 0.0f + i * 0.065f;
			P.MPoly({ FV(X, 0.0f), FV(X + 0.035f, 0.0f), FV(X + 0.015f, 0.045f) }, White);
		}
		P.Blob({ FV(-0.17f, -0.09f), FV(0.03f, -0.13f), FV(0.2f, -0.06f), FV(0.24f, 0.01f), FV(0.06f, 0.03f), FV(-0.17f, 0.05f) }, Body);
		P.Glow([](FV Q) { return SdCircle(Q, FV(0.06f, -0.05f), 0.04f); }, Alpha(Tone(EyeColor, 0.9f), 1.0f), 0.1f, 0.85f);
		P.Ball(FV(0.06f, -0.05f), 0.042f, EyeColor, 0.5f);
		P.MCircle(FV(0.07f, -0.05f), 0.015f, Ink);
		P.MCircle(FV(0.21f, -0.015f), 0.008f, Alpha(Ink, 0.8f));
		P.MCapsule(FV(-0.02f, -0.1f), FV(0.1f, -0.085f), 0.008f, Alpha(Tone(Body, 0.3f), 0.8f));
	}

	void Shield(FPainter& P, FV C, float S, const FLinearColor& Color)
	{
		FPlace Place(P, C, S);
		const TArray<FV> Pts = { FV(-0.36f, -0.42f), FV(0.36f, -0.42f), FV(0.36f, 0.02f), FV(0.0f, 0.46f), FV(-0.36f, 0.02f) };
		P.Fill([&Pts](FV Q) { return SdPoly(Q, Pts) - 0.03f; }, Tone(Color, 0.85f));
		const TArray<FV> Inner = { FV(-0.27f, -0.33f), FV(0.27f, -0.33f), FV(0.27f, 0.0f), FV(0.0f, 0.33f), FV(-0.27f, 0.0f) };
		{
			FWith With(P);
			P.Look.Dome = 0.14f;
			P.Fill([&Inner](FV Q) { return SdPoly(Q, Inner); }, Tone(Color, 1.25f));
		}
		const FV Studs[] = { FV(-0.32f, -0.38f), FV(0.32f, -0.38f), FV(-0.32f, 0.0f), FV(0.32f, 0.0f) };
		for (const FV& Stud : Studs)
		{
			Rivet(P, Stud, 0.022f);
		}
		P.Gloss(FV(-0.13f, -0.22f), 0.09f, 0.04f, 0.5f);
	}

	void Book(FPainter& P, FV C, float S, const FLinearColor& Cover)
	{
		FPlace Place(P, C, S);
		P.Poly({ FV(-0.48f, -0.26f), FV(0.0f, -0.18f), FV(0.48f, -0.26f), FV(0.48f, 0.34f), FV(0.0f, 0.42f), FV(-0.48f, 0.34f) }, Cover);
		P.Poly({ FV(-0.42f, -0.32f), FV(-0.02f, -0.22f), FV(-0.02f, 0.34f), FV(-0.42f, 0.26f) }, Hex(0xF6EBD0));
		P.Poly({ FV(0.42f, -0.32f), FV(0.02f, -0.22f), FV(0.02f, 0.34f), FV(0.42f, 0.26f) }, Hex(0xE2D3B0));
		for (int32 i = 0; i < 4; ++i)
		{
			const float Y = -0.16f + i * 0.1f;
			P.MCapsule(FV(-0.34f, Y), FV(-0.1f, Y + 0.05f), 0.011f, Hex(0x8A7A5C));
			P.MCapsule(FV(0.1f, Y + 0.05f), FV(0.34f, Y), 0.011f, Hex(0x8A7A5C));
		}
	}

	void Clock(FPainter& P, FV C, float S)
	{
		FPlace Place(P, C, S);
		P.Ball(FV(0.0f, 0.0f), 0.47f, Hex(0x3C7FA0));
		P.Circle(FV(0.0f, 0.0f), 0.37f, Hex(0xF0F6F8));
		for (int32 i = 0; i < 12; ++i)
		{
			const FV Dir = Spin(FV(1.0f, 0.0f), i * 30.0f);
			P.MCapsule(Dir * 0.3f, Dir * 0.34f, i % 3 ? 0.008f : 0.014f, Alpha(Ink, 0.8f));
		}
		P.MCapsule(FV(0.0f, 0.0f), FV(0.0f, -0.25f), 0.03f, Ink);
		P.MCapsule(FV(0.0f, 0.0f), FV(0.17f, 0.07f), 0.03f, Ink);
		P.MCircle(FV(0.0f, 0.0f), 0.04f, Blood);
	}

	void Eye(FPainter& P, FV C, float S)
	{
		FPlace Place(P, C, S);
		const auto Lid = [](FV Q) { return FMath::Max(SdCircle(Q, FV(0.0f, 0.36f), 0.56f), SdCircle(Q, FV(0.0f, -0.36f), 0.56f)); };
		P.Fill(Lid, White);
		FClip Clip(P, Lid);
		P.Ball(FV(0.0f, 0.0f), 0.21f, Hex(0x2E86CC), 0.2f);
		for (int32 i = 0; i < 14; ++i)
		{
			const FV Dir = Spin(FV(1.0f, 0.0f), i * 360.0f / 14.0f);
			P.MCapsule(Dir * 0.11f, Dir * 0.19f, 0.007f, Hex(0x9CD8FF, 0.75f));
		}
		P.MCircle(FV(0.0f, 0.0f), 0.095f, Ink);
		P.Gloss(FV(-0.07f, -0.08f), 0.05f, 0.04f, 0.95f);
		P.MEllipse(FV(0.0f, -0.22f), 0.42f, 0.08f, Hex(0x0A2030, 0.35f), 0.04f);
	}

	void Snowflake(FPainter& P, FV C, float S, const FLinearColor& Color)
	{
		FPlace Place(P, C, S);
		for (int32 i = 0; i < 3; ++i)
		{
			const FV Dir = Spin(FV(1.0f, 0.0f), 90.0f + i * 60.0f);
			P.Capsule(Dir * -0.46f, Dir * 0.46f, 0.042f, Color);
		}
		for (int32 i = 0; i < 6; ++i)
		{
			const FV Dir = Spin(FV(1.0f, 0.0f), 90.0f + i * 60.0f);
			const FV Perp(-Dir.Y, Dir.X);
			P.Line({ Dir * 0.27f + Perp * 0.13f + Dir * 0.11f, Dir * 0.27f, Dir * 0.27f - Perp * 0.13f + Dir * 0.11f }, 0.032f, Color);
			P.Line({ Dir * 0.13f + Perp * 0.08f + Dir * 0.07f, Dir * 0.13f, Dir * 0.13f - Perp * 0.08f + Dir * 0.07f }, 0.024f, Color);
		}
		P.Ball(FV(0.0f, 0.0f), 0.08f, Tone(Color, 1.3f));
	}

	void MusicNotes(FPainter& P, FV C, float S, const FLinearColor& Color)
	{
		FPlace Place(P, C, S);
		P.Poly({ FV(-0.17f, -0.36f), FV(0.33f, -0.46f), FV(0.33f, -0.32f), FV(-0.17f, -0.22f) }, Color);
		P.Capsule(FV(-0.17f, -0.3f), FV(-0.17f, 0.24f), 0.035f, Color);
		P.Capsule(FV(0.33f, -0.4f), FV(0.33f, 0.14f), 0.035f, Color);
		{
			FPlace Head(P, C + FV(-0.27f, 0.26f) * S, S, -20.0f);
			P.Ellipse(FV(0.0f, 0.0f), 0.13f, 0.095f, Color);
			P.Gloss(FV(-0.04f, -0.03f), 0.05f, 0.022f, 0.7f);
		}
		{
			FPlace Head(P, C + FV(0.23f, 0.16f) * S, S, -20.0f);
			P.Ellipse(FV(0.0f, 0.0f), 0.13f, 0.095f, Color);
			P.Gloss(FV(-0.04f, -0.03f), 0.05f, 0.022f, 0.7f);
		}
	}

	void Clover(FPainter& P, FV C, float S)
	{
		FPlace Place(P, C, S);
		P.Ribbon(Smooth({ FV(0.0f, 0.05f), FV(0.06f, 0.3f), FV(0.2f, 0.46f) }), 0.04f, 0.028f, Hex(0x3C8A3A));
		const FLinearColor Leaf = Hex(0x55C04A);
		for (int32 i = 0; i < 4; ++i)
		{
			const FV Dir = Spin(FV(1.0f, 0.0f), 45.0f + i * 90.0f);
			const FV Perp(-Dir.Y, Dir.X);
			const TArray<FV> Wedge = { FV(0.0f, 0.0f), Dir * 0.24f + Perp * 0.19f, Dir * 0.24f - Perp * 0.19f };
			const auto Heart = [Dir, Perp, &Wedge](FV Q)
			{
				return FMath::Min(FMath::Min(SdCircle(Q, Dir * 0.2f + Perp * 0.09f, 0.13f), SdCircle(Q, Dir * 0.2f - Perp * 0.09f, 0.13f)), SdPoly(Q, Wedge));
			};
			FWith With(P);
			P.Look.Dome = 0.12f;
			P.Fill(Heart, Leaf);
			P.MCapsule(Dir * 0.06f, Dir * 0.26f, 0.008f, Hex(0x2E7A2E, 0.8f));
			P.MEllipse(Dir * 0.2f + Perp * 0.08f, 0.05f, 0.05f, Hex(0xD8FFC0, 0.4f), 0.03f);
		}
		P.Ball(FV(0.0f, 0.0f), 0.05f, Hex(0x3C8A3A));
	}

	/** Lustrous pearl with a pink and teal sheen */
	void PearlBall(FPainter& P, FV C, float R)
	{
		P.Ball(C, R, Hex(0xE6E0F0), 0.5f);
		FClip Clip(P, [C, R](FV Q) { return SdCircle(Q, C, R); });
		P.MEllipse(C + FV(R * 0.34f, R * 0.46f), R * 0.72f, R * 0.46f, Hex(0x8E7CB8, 0.5f), R * 0.4f);
		P.MEllipse(C + FV(-R * 0.5f, R * 0.25f), R * 0.36f, R * 0.5f, Hex(0xFFB8D8, 0.4f), R * 0.3f);
		P.MEllipse(C + FV(R * 0.5f, -R * 0.2f), R * 0.32f, R * 0.46f, Hex(0x9AF0E0, 0.35f), R * 0.3f);
		P.Gloss(C + FV(-R * 0.32f, -R * 0.38f), R * 0.3f, R * 0.2f, 0.95f);
		P.Gloss(C + FV(-R * 0.08f, -R * 0.6f), R * 0.08f, R * 0.05f, 0.7f);
		P.Mark([C, R](FV Q) { return SdArc(Q, C, R * 0.82f, R * 0.08f, 20.0f, 95.0f); }, Hex(0xFFFFFF, 0.5f), R * 0.06f);
	}

	void Heart(FPainter& P, FV C, float S, const FLinearColor& Color)
	{
		FPlace Place(P, C, S);
		const TArray<FV> Tip = { FV(-0.4f, -0.04f), FV(0.4f, -0.04f), FV(0.0f, 0.42f) };
		const auto Shape = [&Tip](FV Q)
		{
			return FMath::Min(FMath::Min(SdCircle(Q, FV(-0.2f, -0.12f), 0.22f), SdCircle(Q, FV(0.2f, -0.12f), 0.22f)), SdPoly(Q, Tip));
		};
		FWith With(P);
		P.Look.Dome = 0.1f;
		P.Look.Spec = 0.8f;
		P.Fill(Shape, Color);
		FClip Clip(P, Shape);
		P.MEllipse(FV(0.1f, 0.2f), 0.26f, 0.14f, Alpha(Tone(Color, 0.4f), 0.4f), 0.06f);
		P.Gloss(FV(-0.22f, -0.2f), 0.09f, 0.055f, 0.85f);
		P.Gloss(FV(0.17f, -0.24f), 0.03f, 0.02f, 0.6f);
	}

	void Burst(FPainter& P, FV C, float S)
	{
		FPlace Place(P, C, S);
		P.Glow([](FV Q) { return SdCircle(Q, FV(0.0f, 0.0f), 0.3f); }, Hex(0xFF7A20), 0.22f, 0.7f);
		P.Star(FV(0.0f, 0.0f), 0.5f, 0.28f, 11, -90.0f, Hex(0xE8561E));
		P.Star(FV(0.0f, 0.0f), 0.36f, 0.2f, 9, -70.0f, Hex(0xFF9A2E));
		P.Star(FV(0.0f, 0.0f), 0.22f, 0.11f, 7, -40.0f, Hex(0xFFE26A));
		P.Mark([](FV Q) { return SdCircle(Q, FV(0.0f, 0.0f), 0.08f); }, Hex(0xFFFFFF, 0.9f), 0.05f);
	}

	// ---- icons by family ----

	void PaintWall(FPainter& P)
	{
		// reef rock with a brain-coral cap, moss, barnacles and tube sponges, as on the board
		const TArray<FV> RockPts = Smooth({ FV(0.06f, 0.82f), FV(0.1f, 0.6f), FV(0.2f, 0.46f), FV(0.38f, 0.4f), FV(0.64f, 0.4f), FV(0.82f, 0.47f), FV(0.93f, 0.62f), FV(0.95f, 0.83f), FV(0.76f, 0.93f), FV(0.5f, 0.95f), FV(0.24f, 0.93f) }, true, 5);
		const auto RockShape = [&RockPts](FV Q) { return SdPoly(Q, RockPts); };
		{
			FWith With(P);
			P.Look.Grain = 0.18f;
			P.Look.GrainFreq = 22.0f;
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.1f;
			P.Fill(RockShape, Hex(0x8A736A));
		}
		{
			FClip Clip(P, RockShape);
			// layered ledges and cracks
			P.MEllipse(FV(0.32f, 0.72f), 0.2f, 0.12f, Hex(0xB39A8C, 0.35f), 0.04f);
			P.MLine(Smooth({ FV(0.1f, 0.68f), FV(0.28f, 0.66f), FV(0.42f, 0.74f), FV(0.62f, 0.7f), FV(0.92f, 0.75f) }), 0.007f, Hex(0x3A2A28, 0.9f));
			P.MLine({ FV(0.42f, 0.74f), FV(0.38f, 0.84f), FV(0.42f, 0.92f) }, 0.006f, Hex(0x3A2A28, 0.85f));
			P.MLine({ FV(0.7f, 0.72f), FV(0.76f, 0.82f), FV(0.72f, 0.9f) }, 0.006f, Hex(0x3A2A28, 0.85f));
			// moss on the shoulders
			P.MEllipse(FV(0.14f, 0.62f), 0.1f, 0.05f, Hex(0x4CA848, 0.95f), 0.012f);
			P.MEllipse(FV(0.86f, 0.62f), 0.1f, 0.05f, Hex(0x4CA848, 0.95f), 0.012f);
			P.MEllipse(FV(0.14f, 0.6f), 0.06f, 0.022f, Hex(0x96DC70, 0.8f), 0.01f);
			P.MEllipse(FV(0.85f, 0.6f), 0.06f, 0.022f, Hex(0x96DC70, 0.8f), 0.01f);
		}
		// barnacles
		const FV Barnacles[] = { FV(0.24f, 0.8f), FV(0.32f, 0.86f), FV(0.17f, 0.74f), FV(0.6f, 0.82f), FV(0.68f, 0.86f), FV(0.82f, 0.78f) };
		for (const FV& B : Barnacles)
		{
			P.Ball(B, 0.032f, Hex(0xEAE4D0), 0.2f);
			P.MCircle(B, 0.012f, Hex(0x3A302C));
		}

		// orange tube sponges
		for (int32 i = 0; i < 3; ++i)
		{
			const float X = 0.76f + i * 0.07f;
			const float Top = 0.14f + i * 0.09f;
			P.Capsule(FV(X, 0.56f), FV(X, Top), 0.032f, Hex(0xFF9A3E));
			P.Ellipse(FV(X, Top), 0.032f, 0.016f, Hex(0x6A2A10));
		}

		// the brain coral cap with its meandering grooves
		const auto Cap = [](FV Q) { return SdEllipse(Q, FV(0.42f, 0.34f), 0.32f, 0.26f); };
		{
			FWith With(P);
			P.Look.Dome = 0.27f;
			P.Look.Spec = 0.3f;
			P.Fill(Cap, Hex(0xFF7C80));
		}
		{
			FClip Clip(P, Cap);
			for (int32 Row = 0; Row < 6; ++Row)
			{
				const float Y = 0.12f + Row * 0.085f;
				TArray<FV> Groove;
				for (int32 i = 0; i <= 24; ++i)
				{
					const float X = 0.07f + i * 0.034f;
					Groove.Add(FV(X, Y + FMath::Sin(X * 24.0f + Row * 1.7f) * 0.024f + FMath::Sin(X * 9.0f + Row) * 0.012f));
				}
				P.MLine(Groove, 0.0085f, Hex(0x9A2A44, 0.95f));
			}
			P.MEllipse(FV(0.32f, 0.19f), 0.15f, 0.07f, Hex(0xFFD8D4, 0.45f), 0.05f);
		}
	}

	void PaintHarpoon(FPainter& P)
	{
		Plinth(P, 0.47f, 0.86f, 0.36f);

		// rope-wrapped wooden piling
		const auto Post = [](FV Q) { return SdBox(Q, FV(0.36f, 0.45f), FV(0.58f, 0.87f), 0.025f); };
		{
			FWith With(P);
			P.Look.Grain = 0.14f;
			P.Look.GrainFreq = 18.0f;
			P.Look.Dome = 0.1f;
			P.Fill(Post, Wood);
		}
		{
			FClip Clip(P, Post);
			for (int32 i = 0; i < 4; ++i)
			{
				P.MLine(Smooth({ FV(0.39f + i * 0.055f, 0.46f), FV(0.4f + i * 0.055f, 0.65f), FV(0.385f + i * 0.055f, 0.87f) }), 0.0055f, Hex(0x4A2E16, 0.75f));
			}
		}
		for (int32 i = 0; i < 4; ++i)
		{
			const float Y = 0.565f + i * 0.052f;
			Rope(P, FV(0.345f, Y + 0.012f), FV(0.595f, Y - 0.012f), 0.026f);
		}
		P.Ellipse(FV(0.47f, 0.45f), 0.125f, 0.04f, WoodLight);
		P.Mark([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.47f, 0.45f), 0.07f, 0.02f)) - 0.004f; }, Hex(0x6E4A28, 0.9f));

		// coiled line hanging from the gun
		P.Ribbon(Smooth({ FV(0.28f, 0.42f), FV(0.22f, 0.52f), FV(0.26f, 0.62f), FV(0.18f, 0.7f), FV(0.24f, 0.78f) }), 0.014f, 0.014f, RopeColor);

		// iron yoke
		P.Poly({ FV(0.39f, 0.45f), FV(0.42f, 0.33f), FV(0.52f, 0.33f), FV(0.55f, 0.45f) }, Iron);

		// gun, aimed up and to the right
		{
			FPlace Place(P, FV(0.46f, 0.33f), 1.0f, -34.0f);
			P.Box(FV(-0.32f, -0.05f), FV(-0.1f, 0.06f), 0.03f, WoodDark);
			P.Box(FV(-0.13f, -0.075f), FV(0.17f, 0.075f), 0.03f, Iron);
			P.MCapsule(FV(-0.1f, -0.042f), FV(0.14f, -0.042f), 0.012f, Hex(0xC8D4DA, 0.6f));
			P.Box(FV(-0.06f, -0.088f), FV(-0.02f, 0.088f), 0.012f, Brass);
			P.Box(FV(0.13f, -0.09f), FV(0.18f, 0.09f), 0.012f, Brass);
			for (int32 i = 0; i < 2; ++i)
			{
				Rivet(P, FV(0.03f + i * 0.05f, 0.04f), 0.013f);
			}
		}
		{
			// the loaded barbed harpoon
			const FV Base = FV(0.46f, 0.33f);
			Harpoon(P, Base + Spin(FV(0.28f, 0.0f), -34.0f), 0.82f, -34.0f);
		}
		P.Ball(FV(0.46f, 0.33f), 0.042f, Brass);
	}

	void PaintEelTower(FPainter& P)
	{
		// the eel coils up out of a dark burrow in a rock
		P.Glow([](FV Q) { return SdCircle(Q, FV(0.68f, 0.26f), 0.1f); }, Hex(0xC8B020), 0.2f, 0.4f);

		const FLinearColor Body = Hex(0x8A8A36);
		{
			FWith With(P);
			P.Look.Grain = 0.16f;
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.1f;
			P.Blob({ FV(0.05f, 0.92f), FV(0.05f, 0.7f), FV(0.2f, 0.52f), FV(0.42f, 0.5f), FV(0.62f, 0.6f), FV(0.72f, 0.78f), FV(0.72f, 0.92f), FV(0.4f, 0.96f) }, Hex(0x75797E));
		}
		P.Ellipse(FV(0.38f, 0.75f), 0.2f, 0.145f, Hex(0x262E34));
		P.MEllipse(FV(0.38f, 0.78f), 0.14f, 0.1f, Hex(0x06090C), 0.02f);
		P.MLine({ FV(0.12f, 0.8f), FV(0.17f, 0.88f), FV(0.28f, 0.92f) }, 0.006f, Hex(0x2A3238, 0.8f));
		P.MLine({ FV(0.62f, 0.72f), FV(0.66f, 0.82f), FV(0.62f, 0.9f) }, 0.006f, Hex(0x2A3238, 0.8f));

		const TArray<FV> Path = Smooth({ FV(0.34f, 0.84f), FV(0.42f, 0.66f), FV(0.72f, 0.66f), FV(0.82f, 0.5f), FV(0.68f, 0.38f), FV(0.42f, 0.42f), FV(0.3f, 0.3f), FV(0.42f, 0.2f), FV(0.58f, 0.2f) }, false, 7);
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.45f;
			P.Ribbon(Path, 0.082f, 0.058f, Body);
		}
		{
			FClip Clip(P, [&Path](FV Q) { return SdRibbon(Q, Path, 0.082f, 0.058f); });
			// spotted back and a pale belly stripe
			for (int32 i = 1; i < 14; ++i)
			{
				FV Pos, Tan;
				Along(Path, i / 14.0f, Pos, Tan);
				const FV Nrm(-Tan.Y, Tan.X);
				P.MCircle(Pos - Nrm * 0.022f, 0.017f, Hex(0x3C3A14, 0.9f));
				P.MCircle(Pos + Nrm * 0.04f, 0.022f, Hex(0xE8D07A, 0.9f));
			}
		}
		// lightning arcing off the back
		const FLinearColor Arc = Volt;
		const TArray<FV> Arc1 = { FV(0.84f, 0.42f), FV(0.9f, 0.34f), FV(0.86f, 0.3f), FV(0.94f, 0.2f) };
		P.Glow([&Arc1](FV Q) { return SdPolyline(Q, Arc1, 0.012f); }, Hex(0xE0C010), 0.05f, 0.7f);
		P.MLine(Arc1, 0.012f, Arc);
		const TArray<FV> Arc2 = { FV(0.25f, 0.28f), FV(0.17f, 0.25f), FV(0.2f, 0.19f), FV(0.1f, 0.15f) };
		P.Glow([&Arc2](FV Q) { return SdPolyline(Q, Arc2, 0.012f); }, Hex(0xE0C010), 0.05f, 0.7f);
		P.MLine(Arc2, 0.012f, Arc);
		EelHead(P, FV(0.64f, 0.2f), -8.0f, 1.15f, Body, Hex(0xE8D07A), Hex(0xFFEE5A));
	}

	void PaintOctopus(FPainter& P)
	{
		Plinth(P, 0.48f, 0.87f, 0.37f);
		P.Ellipse(FV(0.48f, 0.81f), 0.3f, 0.07f, Hex(0xE8D8A0));

		const FLinearColor Skin = Hex(0xE8551E), SkinDark = Hex(0x8E2410), SkinLight = Hex(0xFF8A44), Sucker = Hex(0xFFE0C4), Pebble = Hex(0xA6ADB2);

		// a shell and stones from its midden
		P.Blob({ FV(0.72f, 0.82f), FV(0.76f, 0.73f), FV(0.86f, 0.71f), FV(0.93f, 0.77f), FV(0.86f, 0.84f) }, Hex(0xFFD8C8));
		for (int32 i = 0; i < 4; ++i)
		{
			P.MCapsule(FV(0.76f, 0.8f), FV(0.79f + i * 0.04f, 0.73f + i * 0.012f), 0.004f, Hex(0xB87A68));
		}
		P.Ball(FV(0.2f, 0.8f), 0.038f, Pebble, 0.25f);
		P.Ball(FV(0.13f, 0.82f), 0.028f, Hex(0x8C949A), 0.2f);

		// an arm with its row of suckers
		auto Arm = [&P, Skin, Sucker](const TArray<FV>& Points, float R0, float R1)
		{
			const TArray<FV> Path = Smooth(Points, false, 7);
			P.Ribbon(Path, R0, R1, Skin);
			for (int32 i = 1; i < 9; ++i)
			{
				FV Pos, Tan;
				Along(Path, i / 9.0f, Pos, Tan);
				P.MCircle(Pos + FV(-Tan.Y, Tan.X) * FMath::Lerp(R0, R1, i / 9.0f) * 0.55f, FMath::Lerp(R0, R1, i / 9.0f) * 0.36f, Sucker);
			}
		};

		// arms sprawled over the sand
		Arm({ FV(0.4f, 0.62f), FV(0.28f, 0.72f), FV(0.14f, 0.72f), FV(0.07f, 0.63f) }, 0.042f, 0.014f);
		Arm({ FV(0.44f, 0.66f), FV(0.38f, 0.8f), FV(0.28f, 0.87f), FV(0.2f, 0.87f) }, 0.042f, 0.014f);
		Arm({ FV(0.56f, 0.66f), FV(0.6f, 0.8f), FV(0.7f, 0.86f), FV(0.79f, 0.87f) }, 0.042f, 0.014f);
		Arm({ FV(0.6f, 0.62f), FV(0.72f, 0.68f), FV(0.84f, 0.64f), FV(0.9f, 0.56f) }, 0.042f, 0.014f);

		// the arm cocked behind the head, holding a stone
		Arm({ FV(0.42f, 0.5f), FV(0.24f, 0.46f), FV(0.13f, 0.32f), FV(0.13f, 0.18f) }, 0.04f, 0.018f);
		P.Ball(FV(0.13f, 0.12f), 0.06f, Pebble, 0.35f);

		// bulbous warty mantle over a rounded head
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.4f;
			P.Blob({ FV(0.3f, 0.34f), FV(0.33f, 0.14f), FV(0.5f, 0.05f), FV(0.68f, 0.14f), FV(0.71f, 0.36f), FV(0.68f, 0.6f), FV(0.5f, 0.7f), FV(0.33f, 0.6f) }, Skin);
		}
		for (const FVector Spot : { FVector(0.42f, 0.2f, 0.028f), FVector(0.55f, 0.14f, 0.022f), FVector(0.62f, 0.27f, 0.03f), FVector(0.49f, 0.33f, 0.022f), FVector(0.38f, 0.36f, 0.018f) })
		{
			P.MCircle(FV(Spot.X, Spot.Y), Spot.Z, SkinDark);
		}
		for (const FV Wart : { FV(0.5f, 0.23f), FV(0.42f, 0.29f), FV(0.58f, 0.2f), FV(0.57f, 0.36f) })
		{
			P.MCircle(Wart, 0.012f, SkinLight);
		}
		// gold eyes with flat bar pupils
		for (const float Side : { -1.0f, 1.0f })
		{
			const FV Eye(0.5f + Side * 0.115f, 0.49f);
			P.Ball(Eye, 0.062f, Hex(0xFFC828), 0.5f);
			P.MCapsule(Eye - FV(0.04f, 0.0f), Eye + FV(0.04f, 0.0f), 0.014f, Hex(0x06090C));
		}
		// the throwing arm whipping forward with its stone
		Arm({ FV(0.62f, 0.56f), FV(0.76f, 0.5f), FV(0.82f, 0.36f), FV(0.86f, 0.24f) }, 0.04f, 0.018f);
		P.Ball(FV(0.88f, 0.17f), 0.06f, Hex(0xB8BEC2), 0.35f);
		P.Capsule(FV(0.62f, 0.09f), FV(0.76f, 0.13f), 0.01f, Hex(0xFFFFFF, 0.55f));
	}

	void PaintMantis(FPainter& P)
	{
		Plinth(P, 0.48f, 0.87f, 0.36f);

		const FLinearColor Teal = Hex(0x1FB58C), TealDark = Hex(0x0E7A70), Red = Hex(0xE8342E), Orange = Hex(0xFF8A24), Blue = Hex(0x2E6AE0), Yellow = Hex(0xFFD23C);

		// legs braced on the mound
		for (int32 i = 0; i < 3; ++i)
		{
			P.Line(Smooth({ FV(0.34f + i * 0.09f, 0.58f - i * 0.05f), FV(0.37f + i * 0.1f, 0.7f), FV(0.33f + i * 0.13f, 0.85f) }), 0.014f, i % 2 ? Orange : Red);
		}
		// tail fan
		for (int32 i = 0; i < 5; ++i)
		{
			FPlace Place(P, FV(0.15f, 0.72f), 1.0f, 130.0f + i * 28.0f);
			P.Ellipse(FV(0.08f, 0.0f), 0.09f, 0.032f, i % 2 ? Blue : Orange);
		}
		// arching segmented body
		const TArray<FV> Body = Smooth({ FV(0.15f, 0.72f), FV(0.25f, 0.68f), FV(0.35f, 0.54f), FV(0.46f, 0.4f), FV(0.56f, 0.32f) }, false, 7);
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.5f;
			P.Ribbon(Body, 0.055f, 0.115f, Teal);
		}
		{
			FClip Clip(P, [&Body](FV Q) { return SdRibbon(Q, Body, 0.055f, 0.115f); });
			for (int32 i = 1; i < 8; ++i)
			{
				FV Pos, Tan;
				Along(Body, i / 8.0f, Pos, Tan);
				const FV Nrm(-Tan.Y, Tan.X);
				P.MCapsule(Pos - Nrm * 0.14f, Pos + Nrm * 0.14f, 0.008f, Hex(0x173FB0, 0.95f));
				P.MCircle(Pos - Nrm * 0.03f + Tan * 0.022f, 0.013f, Yellow);
			}
		}
		// head and carapace
		P.Ellipse(FV(0.6f, 0.3f), 0.125f, 0.095f, Teal);
		P.MEllipse(FV(0.57f, 0.26f), 0.08f, 0.04f, Hex(0x62E8B8, 0.8f));
		// feelers and antennal scale
		P.Line(Smooth({ FV(0.68f, 0.26f), FV(0.8f, 0.18f), FV(0.95f, 0.17f) }), 0.008f, Orange);
		P.Line(Smooth({ FV(0.66f, 0.24f), FV(0.74f, 0.12f), FV(0.88f, 0.05f) }), 0.008f, Orange);
		// stalked eyes with the dark midband
		for (int32 i = 0; i < 2; ++i)
		{
			const FV Eye = i ? FV(0.66f, 0.1f) : FV(0.52f, 0.1f);
			P.Capsule(FV(0.58f + i * 0.04f, 0.24f), Eye + FV(0.0f, 0.04f), 0.014f, Orange);
			P.Ball(Eye, 0.058f, Hex(0x48E0D0));
			P.MCapsule(Eye - FV(0.052f, 0.0f), Eye + FV(0.052f, 0.0f), 0.009f, Hex(0x123A70));
			P.MCircle(Eye + FV(0.018f, -0.002f), 0.016f, Hex(0x06090C));
		}
		// the club, whipped forward with a flash of bubbles
		P.Capsule(FV(0.62f, 0.4f), FV(0.72f, 0.52f), 0.03f, Hex(0xE85A8A));
		P.Ball(FV(0.72f, 0.52f), 0.03f, Blue);
		P.Capsule(FV(0.72f, 0.52f), FV(0.8f, 0.48f), 0.024f, Hex(0xE8503C));
		P.Star(FV(0.93f, 0.44f), 0.085f, 0.03f, 6, 10.0f, Hex(0xD8F4FF));
		P.Ball(FV(0.83f, 0.47f), 0.078f, Red);
		P.MCircle(FV(0.885f, 0.455f), 0.034f, Yellow);
		for (const FV Spot : { FV(0.8f, 0.43f), FV(0.82f, 0.51f), FV(0.77f, 0.48f) })
		{
			P.MCircle(Spot, 0.009f, Hex(0x10080C));
		}
		Bubble(P, FV(0.9f, 0.62f), 0.022f);
		Bubble(P, FV(0.96f, 0.3f), 0.016f);
	}

	void PaintLionfish(FPainter& P)
	{
		Plinth(P, 0.5f, 0.87f, 0.32f);
		P.Ellipse(FV(0.5f, 0.82f), 0.2f, 0.07f, Hex(0xB86AB8));
		for (int32 i = 0; i < 5; ++i)
		{
			P.MCircle(FV(0.36f + i * 0.07f, 0.8f), 0.014f, Hex(0xE8F0C8));
		}

		const FLinearColor Maroon = Hex(0x8E1E18), Cream = Hex(0xF8E6C4), Venom = Hex(0x7CF04A);
		const FV Center(0.52f, 0.47f);

		// a banded spine with a venomous tip
		auto Spine = [&P, Maroon, Cream, Venom](FV From, FV To, float R)
		{
			P.Capsule(From, To, R, Cream);
			for (int32 i = 0; i < 3; ++i)
			{
				const FV A = FMath::Lerp(From, To, 0.1f + i * 0.3f), B = FMath::Lerp(From, To, 0.25f + i * 0.3f);
				P.MCapsule(A, B, R, Maroon);
			}
			P.Circle(To, R * 1.5f, Venom);
		};
		// sheer web behind the crown, then the crown of tall spines
		P.Blob({ FV(0.28f, 0.34f), FV(0.3f, 0.13f), FV(0.42f, 0.07f), FV(0.58f, 0.08f), FV(0.66f, 0.3f) }, Hex(0xB84A3C, 0.55f));
		for (int32 i = 0; i < 7; ++i)
		{
			const float X = 0.32f + i * 0.055f;
			Spine(FV(X, 0.34f), FV(X - 0.05f + i * 0.02f, 0.14f - FMath::Sin(i * 0.5f) * 0.1f), 0.011f);
		}
		// wide striped fan toward the viewer's left
		P.Blob({ FV(0.48f, 0.52f), FV(0.22f, 0.36f), FV(0.05f, 0.5f), FV(0.1f, 0.74f), FV(0.3f, 0.78f) }, Hex(0xF4E0C4, 0.75f));
		for (int32 i = 0; i < 7; ++i)
		{
			const float Angle = 138.0f + i * 15.0f;
			Spine(FV(0.46f, 0.53f), FV(0.46f, 0.53f) + Spin(FV(0.42f, 0.0f), Angle), 0.009f);
		}
		// tail fan
		P.Blob({ FV(0.2f, 0.46f), FV(0.07f, 0.34f), FV(0.03f, 0.46f), FV(0.07f, 0.58f) }, Cream);
		for (int32 i = 0; i < 3; ++i)
		{
			P.MCircle(FV(0.1f, 0.4f + i * 0.06f), 0.012f, Maroon);
		}
		// banded body and head
		const TArray<FV> Body = Smooth({ FV(0.17f, 0.46f), FV(0.32f, 0.33f), FV(0.54f, 0.3f), FV(0.74f, 0.37f), FV(0.82f, 0.48f), FV(0.72f, 0.6f), FV(0.5f, 0.66f), FV(0.3f, 0.58f) }, true);
		{
			FWith With(P);
			P.Look.Dome = 0.16f;
			P.Fill([&Body](FV Q) { return SdPoly(Q, Body); }, Cream);
		}
		{
			FClip Clip(P, [&Body](FV Q) { return SdPoly(Q, Body); });
			for (int32 i = 0; i < 9; ++i)
			{
				const float X = 0.22f + i * 0.066f;
				P.MCapsule(FV(X + 0.03f, 0.26f), FV(X - 0.03f, 0.7f), i % 3 == 0 ? 0.019f : 0.011f, Maroon);
			}
		}
		// feathery brow, eye and pout
		for (int32 i = 0; i < 3; ++i)
		{
			P.Line({ FV(0.66f + i * 0.02f, 0.37f), FV(0.68f + i * 0.04f, 0.26f - i * 0.02f) }, 0.007f, Maroon);
			P.Circle(FV(0.68f + i * 0.04f, 0.26f - i * 0.02f), 0.012f, Hex(0xE8703C));
		}
		P.Ball(FV(0.7f, 0.43f), 0.05f, Hex(0xF5A020));
		P.MCircle(FV(0.712f, 0.43f), 0.024f, Hex(0x06090C));
		P.Ellipse(FV(0.82f, 0.5f), 0.032f, 0.026f, Hex(0xF0B090));
		P.MCapsule(FV(0.8f, 0.505f), FV(0.85f, 0.505f), 0.005f, Hex(0x50100C));
	}

	void PaintSawfish(FPainter& P)
	{
		Plinth(P, 0.42f, 0.87f, 0.36f);
		P.Ellipse(FV(0.42f, 0.8f), 0.3f, 0.07f, Hex(0xE8D8A0));
		// sea grass and an old rib
		for (int32 i = 0; i < 3; ++i)
		{
			P.Poly({ FV(0.13f + i * 0.04f, 0.8f), FV(0.1f + i * 0.05f, 0.56f + i * 0.04f), FV(0.17f + i * 0.04f, 0.8f) }, Hex(0x48B868));
		}

		const FLinearColor Top = Hex(0x7E7C58), Dark = Hex(0x5A583C), Belly = Hex(0xF0E8CC), Ivory = Hex(0xFFF8E0), Bone = Hex(0xB8AE82);

		// tall shark tail and dorsal fins
		P.Poly({ FV(0.13f, 0.42f), FV(0.0f, 0.2f), FV(0.05f, 0.42f), FV(0.03f, 0.56f) }, Dark);
		P.Poly({ FV(0.3f, 0.34f), FV(0.28f, 0.17f), FV(0.42f, 0.34f) }, Dark);
		P.Poly({ FV(0.16f, 0.39f), FV(0.13f, 0.28f), FV(0.24f, 0.37f) }, Dark);

		// the saw: a tapering snout lined with teeth, the outer ones bloodied
		{
			FPlace Place(P, FV(0.62f, 0.53f), 1.0f, -14.0f);
			for (int32 i = 0; i < 8; ++i)
			{
				const float X = 0.04f + i * 0.043f;
				for (const float Side : { -1.0f, 1.0f })
				{
					const float Edge = 0.034f - i * 0.002f;
					P.Poly({ FV(X - 0.012f, Side * Edge), FV(X + 0.01f, Side * (Edge + 0.05f)), FV(X + 0.014f, Side * Edge) }, Ivory);
					if (i > 4)
					{
						P.MPoly({ FV(X + 0.002f, Side * (Edge + 0.033f)), FV(X + 0.01f, Side * (Edge + 0.05f)), FV(X + 0.011f, Side * (Edge + 0.033f)) }, Blood);
					}
				}
			}
			P.Poly({ FV(-0.04f, -0.05f), FV(0.38f, -0.018f), FV(0.38f, 0.018f), FV(-0.04f, 0.05f) }, Bone);
			P.MCapsule(FV(0.0f, 0.0f), FV(0.36f, 0.0f), 0.005f, Hex(0x786E48));
		}
		// the long flat body, pale beneath
		const TArray<FV> Body = Smooth({ FV(0.06f, 0.44f), FV(0.2f, 0.38f), FV(0.42f, 0.36f), FV(0.62f, 0.46f), FV(0.66f, 0.55f), FV(0.44f, 0.64f), FV(0.2f, 0.56f) }, true);
		{
			FWith With(P);
			P.Look.Dome = 0.14f;
			P.Fill([&Body](FV Q) { return SdPoly(Q, Body); }, Top);
		}
		{
			FClip Clip(P, [&Body](FV Q) { return SdPoly(Q, Body); });
			P.MEllipse(FV(0.42f, 0.66f), 0.3f, 0.09f, Belly);
			for (const FV Spot : { FV(0.28f, 0.44f), FV(0.38f, 0.42f), FV(0.46f, 0.46f), FV(0.2f, 0.46f) })
			{
				P.MEllipse(Spot, 0.024f, 0.012f, Hex(0x56543A, 0.9f));
			}
			for (int32 i = 0; i < 4; ++i)
			{
				P.MCapsule(FV(0.44f + i * 0.02f, 0.53f), FV(0.43f + i * 0.02f, 0.58f), 0.004f, Hex(0x302C1C));
			}
		}
		// broad pectoral fin and a small eye
		P.Blob({ FV(0.38f, 0.56f), FV(0.3f, 0.72f), FV(0.42f, 0.74f), FV(0.5f, 0.6f) }, Dark);
		P.Ball(FV(0.56f, 0.46f), 0.024f, Hex(0xE8C040));
		P.MCircle(FV(0.565f, 0.46f), 0.012f, Hex(0x06090C));
		Drop(P, FV(0.95f, 0.54f), 0.14f, Blood);
	}

	void PaintHumpback(FPainter& P)
	{
		Plinth(P, 0.5f, 0.87f, 0.33f);
		P.Ellipse(FV(0.5f, 0.82f), 0.22f, 0.07f, Hex(0xF09C90));
		for (int32 i = 0; i < 4; ++i)
		{
			P.Ball(FV(0.34f + i * 0.1f, 0.78f), 0.022f, i % 2 ? Hex(0xFFD8B0) : Hex(0xF87A70), 0.3f);
		}

		const FLinearColor Slate = Hex(0x34466A), SlateLight = Hex(0x50648C), Pale = Hex(0xEFF4F8);

		// song rings in front of the mouth
		P.Arc(FV(0.68f, 0.26f), 0.3f, 0.018f, -55.0f, 15.0f, Hex(0xA8E8FF, 0.75f));
		P.Arc(FV(0.68f, 0.26f), 0.23f, 0.018f, -50.0f, 10.0f, Hex(0xA8E8FF, 0.9f));

		// fluke raised behind
		P.Blob({ FV(0.17f, 0.32f), FV(0.03f, 0.2f), FV(0.02f, 0.12f), FV(0.14f, 0.18f), FV(0.2f, 0.26f) }, Slate);
		P.Blob({ FV(0.17f, 0.32f), FV(0.22f, 0.16f), FV(0.31f, 0.09f), FV(0.31f, 0.2f), FV(0.24f, 0.32f) }, SlateLight);

		// plump arched body
		const TArray<FV> Whole = Smooth({ FV(0.16f, 0.3f), FV(0.26f, 0.32f), FV(0.46f, 0.24f), FV(0.68f, 0.22f), FV(0.84f, 0.3f), FV(0.88f, 0.42f), FV(0.76f, 0.56f), FV(0.52f, 0.62f), FV(0.32f, 0.54f), FV(0.22f, 0.42f) }, true);
		{
			FWith With(P);
			P.Look.Dome = 0.18f;
			P.Look.Spec = 0.35f;
			P.Fill([&Whole](FV Q) { return SdPoly(Q, Whole); }, Slate);
		}
		{
			FClip Clip(P, [&Whole](FV Q) { return SdPoly(Q, Whole); });
			// white grooved throat
			P.MEllipse(FV(0.67f, 0.58f), 0.27f, 0.17f, Pale);
			for (int32 i = 0; i < 6; ++i)
			{
				const float Y = 0.45f + i * 0.03f;
				P.MLine(Smooth({ FV(0.46f + i * 0.01f, Y + 0.03f), FV(0.64f, Y + 0.01f), FV(0.88f, Y - 0.05f) }), 0.0045f, Hex(0x7E8AA0));
			}
			// knobbed head
			for (int32 i = 0; i < 5; ++i)
			{
				P.MCircle(FV(0.6f + i * 0.055f, 0.27f + i * i * 0.004f), 0.012f, SlateLight);
			}
		}
		// mouth line, eye and barnacles
		P.MLine(Smooth({ FV(0.88f, 0.4f), FV(0.72f, 0.4f), FV(0.6f, 0.42f), FV(0.55f, 0.47f) }), 0.006f, Hex(0x0C1420));
		P.Ball(FV(0.5f, 0.41f), 0.02f, Pale, 0.2f);
		P.MCircle(FV(0.505f, 0.41f), 0.012f, Hex(0x06090C));
		for (const FV Bump : { FV(0.8f, 0.5f), FV(0.76f, 0.54f), FV(0.72f, 0.56f) })
		{
			P.Ball(Bump, 0.012f, Hex(0xE8E4D0), 0.1f);
		}
		// long flipper
		P.Blob({ FV(0.42f, 0.5f), FV(0.32f, 0.62f), FV(0.17f, 0.74f), FV(0.2f, 0.78f), FV(0.38f, 0.7f), FV(0.52f, 0.58f) }, Slate);
		P.MEllipse(FV(0.29f, 0.7f), 0.08f, 0.018f, Hex(0xE8EEF4, 0.8f));
		// cleaner fish
		P.Ellipse(FV(0.14f, 0.52f), 0.038f, 0.018f, Hex(0xFFD23C));
		P.Ellipse(FV(0.86f, 0.68f), 0.038f, 0.018f, Hex(0x2E8AF0));
		Bubble(P, FV(0.94f, 0.12f), 0.018f);
	}

	void PaintAcidVent(FPainter& P)
	{
		Plinth(P, 0.5f, 0.87f, 0.38f);

		const FLinearColor Char = Hex(0x302A2C), Rust = Hex(0x985030), Sulfur = Hex(0xF2D438), Hot = Hex(0xFF6A18);

		// rolling black smoke
		P.Glow([](FV Q) { return SdCircle(Q, FV(0.56f, 0.36f), 0.08f); }, Hex(0xFF7A20), 0.15f, 0.5f);
		for (const FVector Puff : { FVector(0.56f, 0.28f, 0.065f), FVector(0.48f, 0.18f, 0.085f), FVector(0.6f, 0.13f, 0.09f), FVector(0.72f, 0.1f, 0.06f), FVector(0.42f, 0.07f, 0.06f) })
		{
			P.Ball(FV(Puff.X, Puff.Y), Puff.Z, Hex(0x1C1820), 0.08f);
		}
		// side chimney
		P.Poly({ FV(0.71f, 0.85f), FV(0.76f, 0.56f), FV(0.84f, 0.56f), FV(0.88f, 0.85f) }, Char);
		P.Ellipse(FV(0.8f, 0.56f), 0.05f, 0.022f, Sulfur);
		// lumpy main chimney with rust and sulfur crust
		const TArray<FV> Stack = { FV(0.28f, 0.88f), FV(0.38f, 0.56f), FV(0.46f, 0.36f), FV(0.64f, 0.36f), FV(0.7f, 0.56f), FV(0.74f, 0.88f) };
		{
			FWith With(P);
			P.Look.Dome = 0.16f;
			P.Look.Grain = 0.2f;
			P.Fill([&Stack](FV Q) { return SdPoly(Q, Stack) - 0.012f; }, Char);
		}
		{
			FClip Clip(P, [&Stack](FV Q) { return SdPoly(Q, Stack) - 0.012f; });
			P.MEllipse(FV(0.5f, 0.5f), 0.2f, 0.05f, Rust);
			P.MEllipse(FV(0.56f, 0.7f), 0.2f, 0.06f, Rust);
			P.MEllipse(FV(0.58f, 0.44f), 0.09f, 0.022f, Sulfur);
			P.MEllipse(FV(0.42f, 0.6f), 0.07f, 0.02f, Sulfur);
			P.MEllipse(FV(0.62f, 0.8f), 0.09f, 0.022f, Sulfur);
			const TArray<FV> Crack = { FV(0.54f, 0.38f), FV(0.5f, 0.5f), FV(0.56f, 0.6f), FV(0.5f, 0.72f), FV(0.54f, 0.88f) };
			P.Glow([&Crack](FV Q) { return SdPolyline(Q, Crack, 0.008f); }, Hot, 0.05f, 0.8f);
			P.MLine(Crack, 0.01f, Hot);
			P.MLine(Crack, 0.004f, Hex(0xFFE0A0));
		}
		// glowing mouth
		P.Ellipse(FV(0.55f, 0.36f), 0.1f, 0.03f, Sulfur);
		P.MEllipse(FV(0.55f, 0.36f), 0.065f, 0.018f, Hot);
		// giant tube worms with red plumes
		for (int32 i = 0; i < 4; ++i)
		{
			const FV Base(0.14f + i * 0.075f, 0.88f), Tip(0.15f + i * 0.07f, 0.62f - (i % 2) * 0.08f);
			P.Capsule(Base, Tip, 0.018f, Hex(0xF4F0E0));
			P.Star(Tip + FV(0.0f, -0.03f), 0.055f, 0.022f, 7, 12.0f * i, Hex(0xE8282C));
		}
		// a shell plate eaten full of holes
		P.Ellipse(FV(0.86f, 0.36f), 0.07f, 0.06f, Hex(0xD87A3C));
		P.MCircle(FV(0.84f, 0.34f), 0.018f, Hex(0x08080C));
		P.MCircle(FV(0.89f, 0.38f), 0.013f, Hex(0x08080C));
		P.MCircle(FV(0.84f, 0.395f), 0.01f, Hex(0x9CE838));
	}

	void PaintTower(FPainter& P, ETDTowerType Type)
	{
		switch (Type)
		{
		case ETDTowerType::Wall:
			PaintWall(P);
			break;
		case ETDTowerType::Harpoon:
			PaintHarpoon(P);
			break;
		case ETDTowerType::Jelly:
			Plinth(P, 0.5f, 0.87f, 0.3f);
			P.MEllipse(FV(0.5f, 0.87f), 0.2f, 0.05f, Hex(0xFF9CE0, 0.7f), 0.03f);
			Jelly(P, FV(0.5f, 0.38f), 0.98f, Hex(0xF08CD8));
			Bubble(P, FV(0.14f, 0.26f), 0.03f);
			Bubble(P, FV(0.86f, 0.42f), 0.022f);
			break;
		case ETDTowerType::Puffer:
		{
			Plinth(P, 0.5f, 0.87f, 0.34f);
			Puffer(P, FV(0.5f, 0.4f), 0.98f, 0.1f);
			// riveted iron tub
			const TArray<FV> Tub = { FV(0.2f, 0.55f), FV(0.8f, 0.55f), FV(0.72f, 0.9f), FV(0.28f, 0.9f) };
			{
				FWith With(P);
				P.Look.Dome = 0.1f;
				P.Fill([&Tub](FV Q) { return SdPoly(Q, Tub) - 0.012f; }, Iron);
			}
			FClip Clip(P, [&Tub](FV Q) { return SdPoly(Q, Tub); });
			P.MCapsule(FV(0.2f, 0.71f), FV(0.8f, 0.71f), 0.022f, IronDark);
			P.MCapsule(FV(0.2f, 0.7f), FV(0.8f, 0.7f), 0.006f, Hex(0xA8B6BE, 0.7f));
			P.MCapsule(FV(0.2f, 0.59f), FV(0.8f, 0.59f), 0.02f, Hex(0x88969E));
			for (int32 Row = 0; Row < 3; ++Row)
			{
				const float Y = 0.6f + Row * 0.11f;
				for (int32 i = 0; i < 6; ++i)
				{
					Rivet(P, FV(0.27f + i * 0.092f, Y), 0.013f);
				}
			}
			break;
		}
		case ETDTowerType::Eel:
			PaintEelTower(P);
			break;
		case ETDTowerType::Mantis:
			PaintMantis(P);
			break;
		case ETDTowerType::Octopus:
			PaintOctopus(P);
			break;
		case ETDTowerType::Lionfish:
			PaintLionfish(P);
			break;
		case ETDTowerType::Sawfish:
			PaintSawfish(P);
			break;
		case ETDTowerType::Humpback:
			PaintHumpback(P);
			break;
		case ETDTowerType::AcidVent:
			PaintAcidVent(P);
			break;
		default:
			break;
		}
	}

	/** A crab claw whose pincers open upwards; Dir mirrors it */
	void Claw(FPainter& P, FV C, float S, float Dir, const FLinearColor& Color, const FLinearColor& Tip)
	{
		auto X = [C, S, Dir](float U, float V) { return C + FV(U * Dir, V) * S; };
		P.Blob({ X(-0.1f, 0.13f), X(-0.16f, -0.02f), X(-0.13f, -0.15f), X(-0.03f, -0.2f), X(-0.03f, -0.04f), X(0.04f, -0.04f), X(0.16f, -0.12f), X(0.15f, 0.04f), X(0.1f, 0.14f) }, Color);
		// dark inner notch between the pincers
		P.MPoly({ X(-0.02f, -0.03f), X(-0.015f, -0.15f), X(0.1f, -0.1f) }, Hex(0x5A1E0A));
		P.MCircle(X(-0.08f, -0.14f), 0.018f * S / 0.5f, Alpha(Tip, 0.9f));
		P.MCircle(X(0.1f, -0.06f), 0.015f * S / 0.5f, Alpha(Tip, 0.9f));
		P.Gloss(X(-0.08f, 0.0f), 0.03f * S, 0.05f * S, 0.5f);
	}

	void PaintEnemy(FPainter& P, ETDEnemyType Type)
	{
		switch (Type)
		{
		case ETDEnemyType::Crab:
		{
			const FLinearColor Shell = Hex(0xF26B2A);
			const FLinearColor Leg = Hex(0xC2501E);
			for (int32 i = 0; i < 3; ++i)
			{
				const float Y = 0.6f + i * 0.065f;
				for (int32 Side = -1; Side <= 1; Side += 2)
				{
					const float F = (float)Side;
					const TArray<FV> Seg = { FV(0.5f + F * 0.17f, Y), FV(0.5f + F * (0.34f - i * 0.02f), Y - 0.03f), FV(0.5f + F * (0.41f - i * 0.03f), Y + 0.13f) };
					P.Line(Seg, 0.021f, Leg);
					P.MCircle(Seg[1], 0.012f, Hex(0xF8A070));
				}
			}
			// arms and claws
			P.Line({ FV(0.34f, 0.56f), FV(0.2f, 0.5f), FV(0.2f, 0.4f) }, 0.032f, Leg);
			P.Line({ FV(0.66f, 0.56f), FV(0.8f, 0.5f), FV(0.8f, 0.4f) }, 0.032f, Leg);
			Claw(P, FV(0.18f, 0.28f), 0.95f, 1.0f, Shell, Hex(0xFFE0C8));
			Claw(P, FV(0.82f, 0.28f), 0.95f, -1.0f, Shell, Hex(0xFFE0C8));

			// eye stalks
			P.Capsule(FV(0.44f, 0.48f), FV(0.42f, 0.32f), 0.018f, Leg);
			P.Capsule(FV(0.56f, 0.48f), FV(0.58f, 0.32f), 0.018f, Leg);

			const TArray<FV> Carapace = Smooth({ FV(0.2f, 0.56f), FV(0.32f, 0.42f), FV(0.5f, 0.39f), FV(0.68f, 0.42f), FV(0.8f, 0.56f), FV(0.68f, 0.74f), FV(0.5f, 0.78f), FV(0.32f, 0.74f) }, true, 5);
			const auto Body = [&Carapace](FV Q) { return SdPoly(Q, Carapace); };
			{
				FWith With(P);
				P.Look.Dome = 0.2f;
				P.Look.Spec = 0.4f;
				P.Fill(Body, Shell);
			}
			{
				FClip Clip(P, Body);
				P.MEllipse(FV(0.5f, 0.75f), 0.25f, 0.075f, Hex(0xFFD2A0, 0.8f), 0.03f);
				const FV Dots[] = { FV(0.36f, 0.5f), FV(0.64f, 0.5f), FV(0.5f, 0.45f), FV(0.28f, 0.58f), FV(0.72f, 0.58f), FV(0.42f, 0.57f), FV(0.58f, 0.57f) };
				for (const FV& Dot : Dots)
				{
					P.MCircle(Dot, 0.016f, Hex(0xA83C10, 0.9f));
				}
				// smile
				P.MArc(FV(0.5f, 0.59f), 0.075f, 0.014f, 25.0f, 155.0f, Hex(0x5A1E0A));
			}
			P.Ball(FV(0.42f, 0.3f), 0.05f, White, 0.2f);
			P.Ball(FV(0.58f, 0.3f), 0.05f, White, 0.2f);
			P.MCircle(FV(0.425f, 0.31f), 0.024f, Ink);
			P.MCircle(FV(0.575f, 0.31f), 0.024f, Ink);
			P.MCircle(FV(0.415f, 0.3f), 0.008f, White);
			P.MCircle(FV(0.565f, 0.3f), 0.008f, White);
			break;
		}
		case ETDEnemyType::Piranha:
		{
			const FLinearColor Fin = Hex(0x7E2A2A);
			// tail, dorsal, ventral
			P.Blob({ FV(0.3f, 0.5f), FV(0.05f, 0.28f), FV(0.1f, 0.5f), FV(0.05f, 0.72f) }, Fin);
			for (int32 i = 0; i < 3; ++i)
			{
				P.MCapsule(FV(0.26f, 0.5f), FV(0.1f, 0.38f + i * 0.12f), 0.006f, Hex(0x3A1012, 0.8f));
			}
			P.Blob({ FV(0.34f, 0.3f), FV(0.42f, 0.14f), FV(0.56f, 0.18f), FV(0.66f, 0.3f) }, Fin);
			P.Blob({ FV(0.44f, 0.74f), FV(0.46f, 0.88f), FV(0.58f, 0.84f), FV(0.62f, 0.72f) }, Fin);

			const TArray<FV> Outline = Smooth({ FV(0.2f, 0.5f), FV(0.34f, 0.32f), FV(0.56f, 0.24f), FV(0.76f, 0.3f), FV(0.92f, 0.46f), FV(0.9f, 0.62f), FV(0.72f, 0.74f), FV(0.5f, 0.77f), FV(0.32f, 0.68f) }, true, 5);
			const auto Body = [&Outline](FV Q) { return SdPoly(Q, Outline); };
			{
				FWith With(P);
				P.Look.Dome = 0.2f;
				P.Look.Spec = 0.6f;
				P.Fill(Body, Hex(0xC2CDD4));
			}
			{
				FClip Clip(P, Body);
				// red belly, dark back, scale rows
				P.MEllipse(FV(0.56f, 0.76f), 0.36f, 0.17f, Hex(0xD8302C), 0.04f);
				P.MEllipse(FV(0.5f, 0.27f), 0.32f, 0.07f, Hex(0x4E5E6A, 0.75f), 0.05f);
				for (int32 Col = 0; Col < 6; ++Col)
				{
					for (int32 Row = 0; Row < 3; ++Row)
					{
						const FV C(0.28f + Col * 0.06f + (Row % 2) * 0.03f, 0.38f + Row * 0.065f);
						P.MArc(C, 0.03f, 0.007f, 270.0f, 450.0f, Hex(0x5E6C76, 0.75f));
					}
				}
				P.MArc(FV(0.54f, 0.5f), 0.18f, 0.012f, -60.0f, 60.0f, Hex(0x3A4852, 0.8f));
				P.MEllipse(FV(0.5f, 0.36f), 0.14f, 0.03f, Hex(0xFFFFFF, 0.55f), 0.02f);
			}
			// side fin
			P.Blob({ FV(0.56f, 0.53f), FV(0.46f, 0.56f), FV(0.44f, 0.65f), FV(0.55f, 0.62f) }, Hex(0xE85A48));

			// gaping mouth with a row of sharp teeth
			const TArray<FV> Mouth = { FV(0.94f, 0.5f), FV(0.72f, 0.57f), FV(0.8f, 0.66f), FV(0.93f, 0.66f) };
			P.MPoly(Mouth, Hex(0x3A0A12));
			for (int32 i = 0; i < 4; ++i)
			{
				const float X = 0.755f + i * 0.043f;
				const float Y = 0.565f - i * 0.016f;
				P.Poly({ FV(X - 0.018f, Y), FV(X + 0.022f, Y - 0.01f), FV(X + 0.005f, Y + 0.05f) }, White);
			}
			for (int32 i = 0; i < 3; ++i)
			{
				const float X = 0.8f + i * 0.045f;
				P.Poly({ FV(X - 0.018f, 0.665f), FV(X + 0.02f, 0.665f), FV(X + 0.002f, 0.615f) }, White);
			}

			// angry eye
			P.Ball(FV(0.73f, 0.41f), 0.06f, Hex(0xFFD860), 0.2f);
			P.MCircle(FV(0.745f, 0.415f), 0.03f, Ink);
			P.MCircle(FV(0.735f, 0.405f), 0.009f, White);
			P.MPoly({ FV(0.65f, 0.34f), FV(0.82f, 0.385f), FV(0.8f, 0.4f), FV(0.65f, 0.37f) }, Hex(0x2A343C));
			break;
		}
		case ETDEnemyType::Turtle:
		{
			const FLinearColor Skin = Hex(0x7FB862);
			const FLinearColor SkinDark = Hex(0x4E8440);
			// flippers with mottling
			for (int32 i = 0; i < 4; ++i)
			{
				const bool bFront = i < 2;
				const float F = (i % 2) ? 1.0f : -1.0f;
				const FV Root(0.45f + (bFront ? 0.14f : -0.14f), 0.5f + F * 0.2f);
				const FV TipPos = Root + FV(bFront ? 0.16f : -0.13f, F * (bFront ? 0.27f : 0.2f));
				const FV Mid = (Root + TipPos) * 0.5f + FV(bFront ? 0.05f : -0.03f, 0.0f);
				P.Ribbon(Smooth({ Root, Mid, TipPos }), 0.07f, 0.03f, Skin);
				P.MCircle(Mid, 0.02f, Alpha(SkinDark, 0.9f));
				P.MCircle(Mid + (TipPos - Mid) * 0.55f, 0.012f, Alpha(SkinDark, 0.9f));
			}
			P.Poly({ FV(0.18f, 0.46f), FV(0.05f, 0.5f), FV(0.18f, 0.54f) }, Skin);

			// head
			P.Ball(FV(0.84f, 0.5f), 0.1f, Skin, 0.3f);
			P.MCircle(FV(0.8f, 0.45f), 0.012f, Alpha(SkinDark, 0.9f));
			P.MCircle(FV(0.81f, 0.55f), 0.012f, Alpha(SkinDark, 0.9f));
			P.Ball(FV(0.885f, 0.44f), 0.026f, White, 0.1f);
			P.Ball(FV(0.885f, 0.56f), 0.026f, White, 0.1f);
			P.MCircle(FV(0.893f, 0.44f), 0.014f, Ink);
			P.MCircle(FV(0.893f, 0.56f), 0.014f, Ink);
			P.MCapsule(FV(0.93f, 0.47f), FV(0.93f, 0.53f), 0.006f, Hex(0x2A4A20));

			// domed shell, rim plates, then the big scutes
			const auto Shell = [](FV Q) { return SdEllipse(Q, FV(0.45f, 0.5f), 0.34f, 0.29f); };
			{
				FWith With(P);
				P.Look.Dome = 0.28f;
				P.Look.Spec = 0.4f;
				P.Fill(Shell, Hex(0x5E8E3C));
			}
			FClip Clip(P, Shell);
			const FLinearColor Scute = Hex(0x86B84E);
			const FLinearColor Seam = Hex(0x2E5020);
			auto Hexagon = [&P, &Scute, &Seam](FV C, float Rx, float Ry)
			{
				TArray<FV> Pts;
				for (int32 i = 0; i < 6; ++i)
				{
					const FV V = Spin(FV(1.0f, 0.0f), i * 60.0f);
					Pts.Add(C + FV(V.X * Rx, V.Y * Ry));
				}
				P.Mark([&Pts](FV Q) { return SdPoly(Q, Pts) - 0.008f; }, Seam);
				FWith With(P);
				P.Look.Ink = 0.0f;
				P.Look.Dome = 0.05f;
				P.Look.Spec = 0.25f;
				P.Poly(Pts, Scute);
				// growth rings
				TArray<FV> Inner;
				for (const FV& Pt : Pts)
				{
					Inner.Add(C + (Pt - C) * 0.55f);
				}
				P.Mark([&Inner](FV Q) { return FMath::Abs(SdPoly(Q, Inner)) - 0.004f; }, Alpha(Seam, 0.7f));
			};
			for (int32 i = 0; i < 3; ++i)
			{
				Hexagon(FV(0.27f + i * 0.18f, 0.5f), 0.08f, 0.078f);
			}
			for (int32 i = 0; i < 4; ++i)
			{
				Hexagon(FV(0.18f + i * 0.18f, 0.335f), 0.075f, 0.072f);
				Hexagon(FV(0.18f + i * 0.18f, 0.665f), 0.075f, 0.072f);
			}
			P.Mark([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.45f, 0.5f), 0.33f, 0.28f)) - 0.014f; }, Hex(0x3C6A2C));
			P.MEllipse(FV(0.36f, 0.32f), 0.14f, 0.05f, Hex(0xFFFFFF, 0.3f), 0.04f);
			break;
		}
		case ETDEnemyType::Manta:
		{
			const FLinearColor Back = Hex(0x2C3548);
			// tail
			P.Ribbon(Smooth({ FV(0.5f, 0.68f), FV(0.52f, 0.82f), FV(0.47f, 0.97f) }), 0.018f, 0.006f, Hex(0x1C2432));

			// pale underside peeks past the wing edge
			const TArray<FV> Wings = Smooth({ FV(0.5f, 0.2f), FV(0.66f, 0.26f), FV(0.84f, 0.4f), FV(0.98f, 0.58f), FV(0.76f, 0.58f), FV(0.6f, 0.64f), FV(0.5f, 0.76f), FV(0.4f, 0.64f), FV(0.24f, 0.58f), FV(0.02f, 0.58f), FV(0.16f, 0.4f), FV(0.34f, 0.26f) }, true, 4);
			{
				P.Fill([&Wings](FV Q) { return SdPoly(Q - FV(0.0f, 0.016f), Wings); }, Hex(0xDCE6EC));
			}
			const auto Body = [&Wings](FV Q) { return SdPoly(Q, Wings); };
			{
				FWith With(P);
				P.Look.Dome = 0.1f;
				P.Look.Spec = 0.5f;
				P.Fill(Body, Back);
			}

			// cephalic horns
			P.Ribbon(Smooth({ FV(0.42f, 0.25f), FV(0.38f, 0.16f), FV(0.41f, 0.07f) }), 0.03f, 0.014f, Hex(0x394560));
			P.Ribbon(Smooth({ FV(0.58f, 0.25f), FV(0.62f, 0.16f), FV(0.59f, 0.07f) }), 0.03f, 0.014f, Hex(0x394560));
			{
				FClip Clip(P, Body);
				// raised spine, shoulder markings and wing ribs
				P.MEllipse(FV(0.5f, 0.46f), 0.12f, 0.2f, Hex(0x45557A, 0.9f), 0.05f);
				for (int32 Side = -1; Side <= 1; Side += 2)
				{
					const float F = (float)Side;
					for (int32 i = 0; i < 4; ++i)
					{
						P.MCapsule(FV(0.5f + F * 0.13f, 0.34f + i * 0.055f), FV(0.5f + F * (0.34f + i * 0.02f), 0.42f + i * 0.055f), 0.0055f, Hex(0x5E7096, 0.7f));
					}
					P.MEllipse(FV(0.5f + F * 0.17f, 0.34f), 0.06f, 0.03f, Hex(0xD8E4F0, 0.95f), 0.01f);
					// gill slits
					for (int32 i = 0; i < 3; ++i)
					{
						P.MCapsule(FV(0.5f + F * 0.05f, 0.45f + i * 0.03f), FV(0.5f + F * 0.1f, 0.44f + i * 0.03f), 0.005f, Hex(0x0C121C, 0.9f));
					}
				}
				P.MCapsule(FV(0.5f, 0.34f), FV(0.5f, 0.68f), 0.008f, Hex(0x8496BC, 0.8f));
				P.MEllipse(FV(0.3f, 0.36f), 0.1f, 0.018f, Hex(0xFFFFFF, 0.2f), 0.02f);
				P.MEllipse(FV(0.7f, 0.36f), 0.1f, 0.018f, Hex(0xFFFFFF, 0.2f), 0.02f);
			}
			P.Ball(FV(0.42f, 0.29f), 0.03f, Hex(0xF0E6C8), 0.1f);
			P.Ball(FV(0.58f, 0.29f), 0.03f, Hex(0xF0E6C8), 0.1f);
			P.MCircle(FV(0.42f, 0.295f), 0.015f, Ink);
			P.MCircle(FV(0.58f, 0.295f), 0.015f, Ink);
			break;
		}
		case ETDEnemyType::Kraken:
		{
			const FLinearColor Arm = Hex(0x8A3FB0);
			const FLinearColor ArmDark = Hex(0x62278C);
			const FLinearColor Sucker = Hex(0xEBC0F5);

			// back arms first, curling outwards, then the front pair
			const TArray<TArray<FV>> Paths = {
				{ FV(0.34f, 0.5f), FV(0.15f, 0.54f), FV(0.05f, 0.42f), FV(0.1f, 0.32f), FV(0.18f, 0.35f) },
				{ FV(0.66f, 0.5f), FV(0.85f, 0.54f), FV(0.95f, 0.42f), FV(0.9f, 0.32f), FV(0.82f, 0.35f) },
				{ FV(0.38f, 0.58f), FV(0.2f, 0.72f), FV(0.06f, 0.76f), FV(0.04f, 0.88f), FV(0.13f, 0.88f) },
				{ FV(0.62f, 0.58f), FV(0.8f, 0.72f), FV(0.94f, 0.76f), FV(0.96f, 0.88f), FV(0.87f, 0.88f) },
				{ FV(0.42f, 0.62f), FV(0.32f, 0.78f), FV(0.22f, 0.86f), FV(0.24f, 0.96f), FV(0.32f, 0.93f) },
				{ FV(0.58f, 0.62f), FV(0.68f, 0.78f), FV(0.78f, 0.86f), FV(0.76f, 0.96f), FV(0.68f, 0.93f) },
				{ FV(0.47f, 0.64f), FV(0.44f, 0.78f), FV(0.47f, 0.92f), FV(0.42f, 0.98f) },
				{ FV(0.53f, 0.64f), FV(0.56f, 0.78f), FV(0.53f, 0.92f), FV(0.58f, 0.98f) } };
			for (int32 a = 0; a < Paths.Num(); ++a)
			{
				const TArray<FV> Path = Smooth(Paths[a], false, 6);
				P.Ribbon(Path, 0.066f, 0.016f, a < 2 ? ArmDark : Arm);
				for (int32 i = 1; i < 8; ++i)
				{
					FV Pos, Tan;
					const float T = 0.15f + i * 0.095f;
					Along(Path, T, Pos, Tan);
					const float Radius = FMath::Lerp(0.021f, 0.008f, T);
					const FV Side = FV(-Tan.Y, Tan.X) * ((a % 2) ? -1.0f : 1.0f);
					P.MCircle(Pos + Side * Radius * 0.65f, Radius, Sucker);
					P.MCircle(Pos + Side * Radius * 0.65f, Radius * 0.42f, ArmDark);
				}
			}

			// the mantle: tall dome with warts, vein stripes and spots
			const TArray<FV> Head = Smooth({ FV(0.5f, 0.03f), FV(0.7f, 0.12f), FV(0.77f, 0.32f), FV(0.74f, 0.52f), FV(0.62f, 0.65f), FV(0.5f, 0.68f), FV(0.38f, 0.65f), FV(0.26f, 0.52f), FV(0.23f, 0.32f), FV(0.3f, 0.12f) }, true, 5);
			const auto Mantle = [&Head](FV Q) { return SdPoly(Q, Head); };
			{
				FWith With(P);
				P.Look.Dome = 0.26f;
				P.Look.Spec = 0.5f;
				P.Fill(Mantle, Hex(0xA651CF));
			}
			{
				FClip Clip(P, Mantle);
				P.MEllipse(FV(0.5f, 0.62f), 0.2f, 0.08f, Hex(0xD89AEC, 0.55f), 0.05f);
				for (int32 i = 0; i < 3; ++i)
				{
					const float X = 0.32f + i * 0.18f;
					P.MLine(Smooth({ FV(0.5f + (X - 0.5f) * 0.3f, 0.06f), FV(X, 0.17f), FV(X + (X - 0.5f) * 0.1f, 0.3f) }), 0.007f, Hex(0x6A2A98, 0.75f));
				}
				const FV Warts[] = { FV(0.33f, 0.26f), FV(0.42f, 0.14f), FV(0.6f, 0.1f), FV(0.68f, 0.24f), FV(0.28f, 0.42f), FV(0.72f, 0.42f), FV(0.52f, 0.25f), FV(0.42f, 0.6f), FV(0.58f, 0.6f) };
				for (const FV& Wart : Warts)
				{
					P.MCircle(Wart, 0.017f, Hex(0x6A2A98, 0.9f));
					P.MCircle(Wart + FV(-0.004f, -0.005f), 0.0065f, Hex(0xE8B8F4, 0.9f));
				}
				P.Gloss(FV(0.39f, 0.13f), 0.085f, 0.035f, 0.55f);
			}

			// slit-pupilled yellow eyes under heavy brows, and a beak
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				const float F = (float)Side;
				const FV Eye(0.5f + F * 0.115f, 0.41f);
				P.Glow([Eye](FV Q) { return SdCircle(Q, Eye, 0.07f); }, Hex(0xC89A10), 0.05f, 0.6f);
				P.Ball(Eye, 0.078f, Hex(0xFFD54A), 0.4f);
				P.MEllipse(Eye, 0.018f, 0.062f, Ink);
				P.MCircle(Eye + FV(-0.02f, -0.025f), 0.012f, Hex(0xFFFFFF, 0.9f));
				P.MPoly({ Eye + FV(F * -0.1f, -0.045f), Eye + FV(F * 0.09f, -0.095f), Eye + FV(F * 0.095f, -0.065f), Eye + FV(F * -0.09f, -0.015f) }, Hex(0x4A1C6E));
			}
			P.Poly({ FV(0.46f, 0.53f), FV(0.54f, 0.53f), FV(0.5f, 0.6f) }, Hex(0xE8B040));
			break;
		}
		case ETDEnemyType::SeaStar:
		{
			const FV Mid(0.5f, 0.53f);
			const TArray<FV> Pts = StarPoints(Mid, 0.45f, 0.17f, 5, -90.0f);
			const auto Body = [&Pts](FV Q) { return SdPoly(Q, Pts) - 0.03f; };
			{
				FWith With(P);
				P.Look.Dome = 0.13f;
				P.Look.Spec = 0.3f;
				P.Look.Grain = 0.06f;
				P.Fill(Body, Hex(0xF58E48));
			}
			FClip Clip(P, Body);
			for (int32 i = 0; i < 5; ++i)
			{
				const FV Dir = Spin(FV(1.0f, 0.0f), -90.0f + i * 72.0f);
				const FV Perp(-Dir.Y, Dir.X);
				P.MCapsule(Mid + Dir * 0.08f, Mid + Dir * 0.42f, 0.012f, Hex(0xC85C22, 0.7f));
				// rows of bumps tapering to each tip
				for (int32 k = 0; k < 5; ++k)
				{
					const float R = 0.13f + k * 0.065f;
					const float W = 0.052f - k * 0.008f;
					const float Size = 0.021f - k * 0.002f;
					P.Ball(Mid + Dir * R, Size, Hex(0xFFD8A8), 0.3f);
					P.Ball(Mid + Dir * (R + 0.03f) + Perp * W, Size * 0.72f, Hex(0xFFC890), 0.2f);
					P.Ball(Mid + Dir * (R + 0.03f) - Perp * W, Size * 0.72f, Hex(0xFFC890), 0.2f);
				}
			}
			P.Ball(Mid, 0.05f, Hex(0xD9673A), 0.3f);
			for (int32 i = 0; i < 5; ++i)
			{
				P.MCircle(Mid + Spin(FV(0.075f, 0.0f), -54.0f + i * 72.0f), 0.012f, Hex(0xB24A1E));
			}
			break;
		}
		case ETDEnemyType::Cuttlefish:
		{
			// arms reaching forward to the right
			for (int32 i = 0; i < 5; ++i)
			{
				const float Y = 0.42f + i * 0.04f;
				P.Ribbon(Smooth({ FV(0.68f, 0.5f), FV(0.82f, Y + (Y - 0.5f) * 0.4f), FV(0.91f, Y + (Y - 0.5f) * 1.1f + 0.015f * ((i % 2) ? 1.0f : -1.0f)), FV(0.97f, Y + (Y - 0.5f) * 1.6f) }), 0.024f, 0.007f, i % 2 ? Hex(0xB5865A) : Hex(0xC89A6E));
			}

			// the rippling fin that skirts the mantle
			const auto Fin = [](FV Q)
			{
				const float Wave = FMath::Sin(FMath::Atan2((Q.Y - 0.5f) / 0.27f, (Q.X - 0.42f) / 0.38f) * 11.0f) * 0.01f;
				return SdEllipse(Q, FV(0.42f, 0.5f), 0.38f, 0.28f) + Wave;
			};
			{
				FWith With(P);
				P.Look.Dome = 0.05f;
				P.Fill(Fin, Hex(0xF2E0BE));
			}
			const auto Mantle = [](FV Q) { return SdEllipse(Q, FV(0.43f, 0.5f), 0.32f, 0.2f); };
			{
				FWith With(P);
				P.Look.Dome = 0.2f;
				P.Look.Spec = 0.35f;
				P.Fill(Mantle, Hex(0xC89562));
			}
			{
				FClip Clip(P, Mantle);
				// zebra stripes wrapping the mantle
				for (int32 i = 0; i < 7; ++i)
				{
					const float X = 0.15f + i * 0.075f;
					P.MLine(Smooth({ FV(X + 0.012f, 0.3f), FV(X - 0.012f, 0.4f), FV(X + 0.016f, 0.47f) }), 0.0145f, Hex(0x5E3A1C, 0.95f));
				}
				P.MEllipse(FV(0.42f, 0.62f), 0.3f, 0.07f, Hex(0xF2E0BE, 0.75f), 0.03f);
				P.Gloss(FV(0.33f, 0.37f), 0.09f, 0.022f, 0.5f);
			}
			// W-shaped pupil
			P.Ball(FV(0.68f, 0.44f), 0.058f, Hex(0xFFF0D0), 0.3f);
			{
				const TArray<FV> Pupil = { FV(0.648f, 0.43f), FV(0.664f, 0.46f), FV(0.68f, 0.43f), FV(0.696f, 0.46f), FV(0.712f, 0.43f) };
				P.MLine(Pupil, 0.0095f, Ink);
			}
			break;
		}
		case ETDEnemyType::ShockEel:
		{
			const FLinearColor Body = Hex(0x26B4AC);
			const FLinearColor Bright = Hex(0xFFE24A);
			const TArray<FV> Path = Smooth({ FV(0.06f, 0.86f), FV(0.2f, 0.7f), FV(0.42f, 0.78f), FV(0.6f, 0.62f), FV(0.5f, 0.44f), FV(0.3f, 0.42f), FV(0.3f, 0.27f), FV(0.5f, 0.22f), FV(0.66f, 0.26f) }, false, 7);
			const float R0 = 0.04f, R1 = 0.088f;
			P.Glow([&Path, R0, R1](FV Q) { return SdRibbon(Q, Path, R0, R1); }, Hex(0x20B8C0), 0.08f, 0.35f);
			{
				FWith With(P);
				P.Look.Dome = 10.0f;
				P.Look.Spec = 0.5f;
				P.Ribbon(Path, R0, R1, Body);
			}
			{
				FClip Clip(P, [&Path, R0, R1](FV Q) { return SdRibbon(Q, Path, R0, R1); });
				for (int32 i = 1; i < 12; ++i)
				{
					FV Pos, Tan;
					Along(Path, i / 12.0f, Pos, Tan);
					const FV Nrm(-Tan.Y, Tan.X);
					const FV A = Pos + Nrm * 0.1f, B = Pos - Nrm * 0.1f;
					P.Glow([A, B](FV Q) { return SdCapsule(Q, A, B, 0.015f); }, Hex(0xE8C820), 0.03f, 0.8f);
					P.MCapsule(A, B, 0.015f, Bright);
				}
			}
			// the head, then sparks
			EelHead(P, FV(0.71f, 0.27f), -6.0f, 1.2f, Body, Hex(0x9BE4E0), Bright);
			Bolt(P, FV(0.86f, 0.52f), 0.26f, Volt, 12.0f);
			Bolt(P, FV(0.16f, 0.5f), 0.22f, Volt, -15.0f);
			break;
		}
		default:
			break;
		}
	}

	// ---- skill motifs ----

	/** Four-pointed glint with a soft halo */
	void Glint(FPainter& P, FV C, float S, const FLinearColor& Color = White)
	{
		P.Glow([C, S](FV Q) { return SdCircle(Q, C, S * 0.1f); }, Alpha(Color, 1.0f), S * 0.45f, 0.55f);
		P.Star(C, S * 0.5f, S * 0.1f, 4, 0.0f, Color);
		P.MCircle(C, S * 0.07f, Hex(0xFFFFFF, 0.95f));
	}

	/** Glossy upgrade arrow */
	void UpBadge(FPainter& P, FV C, float S, const FLinearColor& Color = Gold)
	{
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Look.Spec = 0.7f;
			Arrow(P, C, S, Color);
		}
		P.MPoly({ C + FV(-0.01f, -0.4f) * S, C + FV(-0.27f, -0.1f) * S, C + FV(-0.09f, -0.1f) * S, C + FV(-0.09f, 0.38f) * S, C + FV(-0.03f, 0.38f) * S, C + FV(-0.03f, -0.1f) * S }, Hex(0xFFFFFF, 0.4f));
	}

	/** Glossy bonus cross */
	void PlusBadge(FPainter& P, FV C, float S, const FLinearColor& Color)
	{
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Look.Spec = 0.7f;
			Plus(P, C, S, Color);
		}
		P.MCapsule(C + FV(-0.32f, -0.05f) * S, C + FV(-0.05f, -0.05f) * S, 0.035f * S, Hex(0xFFFFFF, 0.45f));
		P.MCapsule(C + FV(-0.05f, -0.32f) * S, C + FV(-0.05f, -0.1f) * S, 0.035f * S, Hex(0xFFFFFF, 0.45f));
	}

	/** A ring broken into dashes */
	void DashedRing(FPainter& P, FV C, float R, float Thick, int32 Dashes, const FLinearColor& Color, float Phase = 0.0f)
	{
		const float Step = 360.0f / Dashes;
		for (int32 i = 0; i < Dashes; ++i)
		{
			P.Arc(C, R, Thick, Phase + i * Step, Phase + i * Step + Step * 0.55f, Color);
		}
	}

	/** Point on a quadratic curve */
	FV Bezier(FV A, FV B, FV C, float T)
	{
		return A * ((1.0f - T) * (1.0f - T)) + B * (2.0f * (1.0f - T) * T) + C * (T * T);
	}

	/** A small prey fish facing +X */
	void Fish(FPainter& P, FV C, float S, const FLinearColor& Color, float Rot = 0.0f)
	{
		FPlace Place(P, C, S, Rot);
		const FLinearColor Fin = Alpha(Tone(Color, 0.78f), 1.0f);
		P.Poly({ FV(-0.26f, 0.0f), FV(-0.52f, -0.22f), FV(-0.45f, 0.0f), FV(-0.52f, 0.22f) }, Fin);
		P.Poly({ FV(-0.12f, -0.14f), FV(0.02f, -0.3f), FV(0.12f, -0.16f) }, Fin);
		const auto Body = [](FV Q) { return SdEllipse(Q, FV(0.0f, 0.0f), 0.36f, 0.2f); };
		P.Fill(Body, Color);
		FClip Clip(P, Body);
		P.MEllipse(FV(-0.02f, 0.14f), 0.36f, 0.1f, Alpha(Tone(Color, 1.6f), 0.7f), 0.05f);
		for (int32 i = 0; i < 3; ++i)
		{
			P.MCapsule(FV(-0.2f + i * 0.09f, -0.16f), FV(-0.17f + i * 0.09f, 0.04f), 0.018f, Alpha(Tone(Color, 0.6f), 0.55f));
		}
		P.MArc(FV(0.08f, 0.0f), 0.15f, 0.02f, -70.0f, 70.0f, Alpha(Tone(Color, 0.45f), 0.85f));
		P.MCircle(FV(0.21f, -0.04f), 0.055f, White);
		P.MCircle(FV(0.225f, -0.04f), 0.032f, Ink);
		P.Gloss(FV(-0.04f, -0.11f), 0.14f, 0.035f, 0.5f);
	}

	/** A jagged electric arc between two points */
	void Zap(FPainter& P, FV A, FV B, float R, int32 Seed, const FLinearColor& Color)
	{
		const FV D = B - A;
		const FV N = FV(-D.Y, D.X).GetSafeNormal();
		const int32 Segs = 6;
		TArray<FV> Pts;
		for (int32 i = 0; i <= Segs; ++i)
		{
			float Off = 0.0f;
			if (i > 0 && i < Segs)
			{
				const uint32 H = (uint32)(Seed * 7919 + i * 104729) * 2654435761u;
				Off = ((i % 2) ? 1.0f : -1.0f) * (0.4f + 0.6f * (float)((H >> 8) & 0xFFFF) / 65535.0f);
			}
			Pts.Add(A + D * ((float)i / Segs) + N * (Off * D.Size() * 0.08f));
		}
		P.Glow([&Pts, R](FV Q) { return SdPolyline(Q, Pts, R); }, Alpha(Tone(Color, 0.8f), 1.0f), R * 4.0f, 0.7f);
		P.Line(Pts, R, Color);
		P.MLine(Pts, R * 0.35f, Hex(0xFFFFFF, 0.9f));
	}

	/** Striped venomous fin spine from Base to Tip */
	void VenomSpine(FPainter& P, FV Base, FV Tip, float W)
	{
		const FV D = Tip - Base;
		const FV N = FV(-D.Y, D.X).GetSafeNormal();
		const TArray<FV> Shape = { Base - N * W, Tip, Base + N * W };
		const auto Sd = [&Shape](FV Q) { return SdPoly(Q, Shape) - 0.006f; };
		P.Fill(Sd, Hex(0xF6EADC));
		FClip Clip(P, Sd);
		for (int32 i = 0; i < 5; ++i)
		{
			const FV C = Base + D * (0.06f + i * 0.18f);
			P.MCapsule(C - N * W, C + N * W + D * 0.05f, D.Size() * 0.04f, Hex(0xC8322A, 0.95f));
		}
	}

	/** A curling tentacle with suckers along one side (Side = 1 or -1) */
	void Tentacle(FPainter& P, const TArray<FV>& Ctrl, float R0, float R1, float Side, const FLinearColor& Color, const FLinearColor& Sucker)
	{
		const TArray<FV> Path = Smooth(Ctrl);
		P.Ribbon(Path, R0, R1, Color);
		P.Mark([&Path, R0, R1](FV Q) { return SdRibbon(Q, Path, R0 * 0.25f, R1 * 0.25f); }, Alpha(Tone(Color, 1.35f), 0.35f), 0.01f);
		for (int32 i = 1; i < 9; ++i)
		{
			FV Pos, Tan;
			Along(Path, i / 9.5f, Pos, Tan);
			const FV N = FV(-Tan.Y, Tan.X) * Side;
			const float R = FMath::Lerp(R0, R1, i / 9.5f);
			P.Ball(Pos + N * R * 0.55f, R * 0.4f, Sucker, 0.3f);
			P.MCircle(Pos + N * R * 0.55f, R * 0.17f, Alpha(Tone(Sucker, 0.55f), 0.9f));
		}
	}

	/** Faceted crystal growing from Base; Rot tilts it from upright */
	void Crystal(FPainter& P, FV Base, float Len, float W, float Rot, const FLinearColor& Color)
	{
		FPlace Place(P, Base, 1.0f, Rot);
		const TArray<FV> Pts = { FV(-W, 0.0f), FV(-W, -Len * 0.72f), FV(0.0f, -Len), FV(W, -Len * 0.72f), FV(W, 0.0f) };
		P.Glow([&Pts](FV Q) { return SdPoly(Q, Pts); }, Alpha(Tone(Color, 0.6f), 1.0f), 0.05f, 0.4f);
		{
			FWith With(P);
			P.Look.Dome = 0.015f;
			P.Look.Spec = 0.8f;
			P.Poly(Pts, Color);
		}
		FClip Clip(P, [&Pts](FV Q) { return SdPoly(Q, Pts); });
		P.MPoly({ FV(-W, 0.0f), FV(-W, -Len * 0.72f), FV(0.0f, -Len), FV(-W * 0.2f, 0.0f) }, Hex(0xFFFFFF, 0.3f));
		P.MPoly({ FV(W * 0.25f, 0.0f), FV(0.0f, -Len), FV(W, -Len * 0.72f), FV(W, 0.0f) }, Alpha(Tone(Color, 0.35f), 0.35f));
		P.MCapsule(FV(-W * 0.55f, -Len * 0.1f), FV(-W * 0.55f, -Len * 0.66f), W * 0.12f, Hex(0xFFFFFF, 0.6f));
	}

	/** Stalked compound eye of a mantis shrimp, with its striped midband */
	void MantisEye(FPainter& P, FV C, float S, float Rot)
	{
		FPlace Place(P, C, S, Rot);
		P.Ribbon({ FV(0.0f, 0.5f), FV(0.0f, 0.16f) }, 0.08f, 0.065f, Hex(0x3C9A80));
		P.MCapsule(FV(-0.02f, 0.46f), FV(-0.02f, 0.24f), 0.015f, Hex(0xBFF0E0, 0.5f));
		const auto Eye = [](FV Q) { return SdEllipse(Q, FV(0.0f, 0.0f), 0.3f, 0.22f); };
		{
			FWith With(P);
			P.Look.Dome = 0.12f;
			P.Look.Spec = 0.5f;
			P.Fill(Eye, Hex(0x2EA88A));
		}
		FClip Clip(P, Eye);
		// faceted surface
		for (int32 Row = 0; Row < 6; ++Row)
		{
			for (int32 Col = 0; Col < 8; ++Col)
			{
				const FV F(-0.3f + Col * 0.08f + (Row % 2) * 0.04f, -0.22f + Row * 0.09f);
				P.MCircle(F, 0.022f, Hex(0x9AF0D0, 0.28f));
			}
		}
		// a pupil in each half, split by the striped midband
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			P.MCircle(FV(Side * 0.15f, 0.01f), 0.085f, Hex(0x0A1A20, 0.95f));
			P.MCircle(FV(Side * 0.15f - 0.03f, -0.03f), 0.022f, Hex(0xFFFFFF, 0.9f));
		}
		P.MEllipse(FV(0.0f, 0.0f), 0.07f, 0.24f, Hex(0x1A2A3A));
		const FLinearColor Bands[] = { Hex(0xB060E0), Hex(0xFFD23C), Hex(0xFF7A3C) };
		for (int32 i = 0; i < 3; ++i)
		{
			const float X = -0.04f + i * 0.04f;
			P.MCapsule(FV(X, -0.24f), FV(X, 0.24f), 0.013f, Bands[i]);
		}
		P.Gloss(FV(-0.16f, -0.12f), 0.07f, 0.035f, 0.8f);
	}

	/** Sawfish rostrum from Base to Tip with teeth along both edges and sensing pores */
	void SawSnout(FPainter& P, FV Base, FV Tip, float W, int32 Teeth)
	{
		const FV D = Tip - Base;
		const FV Dir = D.GetSafeNormal();
		const FV N(-Dir.Y, Dir.X);
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			for (int32 i = 0; i < Teeth; ++i)
			{
				const float T = 0.12f + i * 0.84f / (Teeth - 1);
				const float Half = FMath::Lerp(W, W * 0.6f, T);
				const FV Root = Base + D * T + N * (Half * Side * 0.8f);
				P.Poly({ Root - Dir * 0.028f, Root + N * (Side * 0.09f) + Dir * 0.012f, Root + Dir * 0.028f }, Hex(0xFFF6DE));
			}
		}
		const TArray<FV> Blade = { Base - N * W, Tip - N * (W * 0.6f), Tip + Dir * 0.03f, Tip + N * (W * 0.6f), Base + N * W };
		const auto Sd = [&Blade](FV Q) { return SdPoly(Q, Blade) - 0.012f; };
		{
			FWith With(P);
			P.Look.Grain = 0.08f;
			P.Look.Dome = 0.05f;
			P.Fill(Sd, Hex(0xBFB38A));
		}
		FClip Clip(P, Sd);
		P.MCapsule(Base + N * (W * 0.1f), Tip - Dir * 0.03f, W * 0.13f, Hex(0x7A6E4E, 0.55f));
		P.MCapsule(Base - N * (W * 0.45f), Tip - N * (W * 0.3f), W * 0.12f, Hex(0xFFFFFF, 0.3f));
		for (int32 i = 0; i < 7; ++i)
		{
			const float T = 0.1f + i * 0.12f;
			const float Half = FMath::Lerp(W, W * 0.6f, T);
			P.MCircle(Base + D * T + N * (Half * 0.5f), 0.011f, Hex(0x4A4230, 0.75f));
			P.MCircle(Base + D * (T + 0.06f) - N * (Half * 0.45f), 0.009f, Hex(0x4A4230, 0.6f));
		}
	}

	/** Curved fang, root to tip */
	void Fang(FPainter& P, FV Root, FV Tip, float W, float Bend)
	{
		const FV D = Tip - Root;
		const FV N = FV(-D.Y, D.X).GetSafeNormal();
		const TArray<FV> Path = Smooth({ Root, Root + D * 0.5f + N * Bend, Tip });
		P.Ribbon(Path, W, 0.004f, Hex(0xFFF6DE));
		P.Mark([&Path, W](FV Q) { return SdRibbon(Q, Path, W * 0.3f, 0.002f); }, Hex(0xFFFFFF, 0.6f));
		P.MCapsule(Root - N * W * 0.8f, Root + N * W * 0.8f, W * 0.22f, Hex(0xC09A70, 0.8f));
	}

	/** Hourglass with sand running through it */
	void Hourglass(FPainter& P, FV C, float S, const FLinearColor& Sand)
	{
		FPlace Place(P, C, S);
		const auto Glass = [](FV Q)
		{
			return FMath::Min(FMath::Min(SdEllipse(Q, FV(0.0f, -0.19f), 0.25f, 0.21f), SdEllipse(Q, FV(0.0f, 0.19f), 0.25f, 0.21f)), SdBox(Q, FV(-0.05f, -0.06f), FV(0.05f, 0.06f), 0.02f));
		};
		P.Fill(Glass, Hex(0xCFF4FF, 0.42f));
		{
			FClip Clip(P, Glass);
			FWith With(P);
			P.Look.Dome = 0.08f;
			P.Fill([](FV Q) { return FMath::Max(SdEllipse(Q, FV(0.0f, -0.19f), 0.25f, 0.21f), -0.13f - Q.Y); }, Sand);
			P.Fill([](FV Q) { return FMath::Max(SdEllipse(Q, FV(0.0f, 0.19f), 0.25f, 0.21f), 0.24f - Q.Y); }, Sand);
			P.Poly({ FV(-0.14f, 0.27f), FV(0.0f, 0.16f), FV(0.14f, 0.27f) }, Sand);
			P.MCapsule(FV(0.0f, -0.1f), FV(0.0f, 0.2f), 0.014f, Sand);
			P.Gloss(FV(-0.13f, -0.24f), 0.035f, 0.08f, 0.6f);
			P.Gloss(FV(-0.13f, 0.14f), 0.03f, 0.06f, 0.45f);
		}
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			P.Capsule(FV(Side * 0.28f, -0.4f), FV(Side * 0.28f, 0.4f), 0.025f, WoodDark);
		}
		P.Box(FV(-0.36f, -0.5f), FV(0.36f, -0.4f), 0.03f, Wood);
		P.Box(FV(-0.36f, 0.4f), FV(0.36f, 0.5f), 0.03f, Wood);
		P.MCapsule(FV(-0.3f, -0.475f), FV(0.3f, -0.475f), 0.008f, Alpha(WoodLight, 0.85f));
		P.MCapsule(FV(-0.3f, 0.425f), FV(0.3f, 0.425f), 0.008f, Alpha(WoodLight, 0.85f));
	}

	/** Open clam holding a pearl */
	void Clam(FPainter& P, FV C, float S)
	{
		FPlace Place(P, C, S);
		const FLinearColor Shell = Hex(0xF2A08A);
		const auto Top = [](FV Q) { return FMath::Max(SdCircle(Q, FV(0.0f, 0.02f), 0.42f), Q.Y + 0.02f); };
		{
			FWith With(P);
			P.Look.Dome = 0.1f;
			P.Fill([&Top](FV Q) { return Top(Q) - 0.01f; }, Shell);
		}
		{
			FClip Clip(P, Top);
			for (int32 i = 0; i < 9; ++i)
			{
				const FV Dir = Spin(FV(1.0f, 0.0f), 180.0f + 10.0f + i * 20.0f);
				P.MCapsule(FV(0.0f, 0.02f) + Dir * 0.08f, FV(0.0f, 0.02f) + Dir * 0.45f, 0.012f, Hex(0xB0584A, 0.6f));
			}
			P.MArc(FV(0.0f, 0.02f), 0.33f, 0.02f, 200.0f, 340.0f, Hex(0xFFE0D0, 0.5f));
		}
		P.Ellipse(FV(0.0f, 0.03f), 0.38f, 0.07f, Hex(0x7A2E40));
		P.MEllipse(FV(0.0f, 0.03f), 0.3f, 0.04f, Hex(0xC05A70, 0.6f), 0.02f);
		P.Glow([](FV Q) { return SdCircle(Q, FV(0.0f, -0.05f), 0.13f); }, Hex(0xFFF0FA), 0.12f, 0.6f);
		PearlBall(P, FV(0.0f, -0.05f), 0.13f);
		const auto Bottom = [](FV Q) { return FMath::Max(SdCircle(Q, FV(0.0f, -0.08f), 0.44f), 0.03f - Q.Y); };
		{
			FWith With(P);
			P.Look.Dome = 0.1f;
			P.Fill([&Bottom](FV Q) { return Bottom(Q) - 0.01f; }, Tone(Shell, 1.1f));
		}
		FClip Clip(P, Bottom);
		for (int32 i = 0; i < 7; ++i)
		{
			const FV Dir = Spin(FV(1.0f, 0.0f), 32.0f + i * 19.0f);
			P.MCapsule(FV(0.0f, -0.4f) + Dir * 0.48f, FV(0.0f, -0.4f) + Dir * 0.78f, 0.012f, Hex(0xB0584A, 0.55f));
		}
		P.Gloss(FV(-0.18f, 0.1f), 0.1f, 0.025f, 0.55f);
	}

	/** Humpback tail rising out of the water */
	void Fluke(FPainter& P, FV C, float S, const FLinearColor& Color)
	{
		FPlace Place(P, C, S);
		P.Ribbon(Smooth({ FV(0.04f, 0.5f), FV(0.0f, 0.25f), FV(0.0f, 0.02f) }), 0.11f, 0.05f, Color);
		const TArray<FV> Pts = { FV(0.0f, -0.02f), FV(-0.18f, -0.14f), FV(-0.44f, -0.24f), FV(-0.5f, -0.16f), FV(-0.36f, -0.04f), FV(-0.14f, 0.07f), FV(0.0f, 0.05f), FV(0.14f, 0.07f), FV(0.36f, -0.04f), FV(0.5f, -0.16f), FV(0.44f, -0.24f), FV(0.18f, -0.14f) };
		const TArray<FV> Outline = Smooth(Pts, true, 5);
		const auto Sd = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		{
			FWith With(P);
			P.Look.Dome = 0.08f;
			P.Fill(Sd, Color);
		}
		FClip Clip(P, Sd);
		// pale underside with the unique markings of each whale
		P.MEllipse(FV(-0.24f, -0.03f), 0.2f, 0.07f, Hex(0xE6EEF4, 0.85f), 0.03f);
		P.MEllipse(FV(0.24f, -0.03f), 0.2f, 0.07f, Hex(0xE6EEF4, 0.85f), 0.03f);
		P.MCircle(FV(-0.3f, -0.04f), 0.025f, Alpha(Color, 0.8f));
		P.MCircle(FV(0.2f, -0.02f), 0.02f, Alpha(Color, 0.8f));
		P.MCircle(FV(0.32f, -0.06f), 0.015f, Alpha(Color, 0.8f));
		P.MCapsule(FV(0.0f, 0.0f), FV(0.0f, 0.06f), 0.012f, Alpha(Tone(Color, 0.5f), 0.9f));
		P.Gloss(FV(-0.3f, -0.17f), 0.08f, 0.02f, 0.45f);
	}

	/** Humpback head in profile facing +X, with throat grooves and knobbly tubercles */
	void WhaleHead(FPainter& P, FV C, float S, const FLinearColor& Color)
	{
		FPlace Place(P, C, S);
		const TArray<FV> Outline = Smooth({ FV(-0.55f, -0.08f), FV(-0.2f, -0.24f), FV(0.2f, -0.22f), FV(0.48f, -0.1f), FV(0.56f, 0.04f), FV(0.42f, 0.18f), FV(0.0f, 0.24f), FV(-0.55f, 0.14f) }, true, 5);
		const auto Sd = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		{
			FWith With(P);
			P.Look.Dome = 0.14f;
			P.Fill(Sd, Color);
		}
		{
			FClip Clip(P, Sd);
			P.MEllipse(FV(0.0f, 0.22f), 0.6f, 0.13f, Hex(0xDDE8F0, 0.9f), 0.03f);
			for (int32 i = 0; i < 5; ++i)
			{
				const float Y = 0.12f + i * 0.035f;
				P.MCapsule(FV(-0.45f, Y + 0.02f), FV(0.38f - i * 0.04f, Y - 0.01f), 0.007f, Alpha(Tone(Color, 0.6f), 0.7f));
			}
			P.MCapsule(FV(-0.1f, 0.06f), FV(0.52f, 0.04f), 0.012f, Alpha(Tone(Color, 0.35f), 0.9f));
			P.Gloss(FV(-0.1f, -0.15f), 0.2f, 0.04f, 0.45f);
		}
		for (int32 i = 0; i < 4; ++i)
		{
			P.Ball(FV(0.1f + i * 0.1f, -0.2f + i * 0.04f), 0.025f, Tone(Color, 1.2f), 0.3f);
		}
		P.MCircle(FV(-0.08f, 0.0f), 0.03f, Ink);
		P.MCircle(FV(-0.088f, -0.008f), 0.01f, Hex(0xFFFFFF, 0.9f));
		// long white flipper
		P.Blob({ FV(-0.3f, 0.14f), FV(-0.18f, 0.22f), FV(-0.36f, 0.42f), FV(-0.5f, 0.48f), FV(-0.44f, 0.32f) }, Hex(0xD8E4EC));
	}

	void PaintSkill(FPainter& P, ETDTowerType Tower, int32 Index)
	{
		const FV Mid(0.5f, 0.5f);
		switch (Tower)
		{
		case ETDTowerType::Harpoon:
			if (Index == 0)
			{
				// one harpoon running through two fleeing fish
				Harpoon(P, Mid, 1.0f, 0.0f);
				Fish(P, FV(0.27f, 0.5f), 0.48f, Hex(0xE8845A));
				Fish(P, FV(0.55f, 0.5f), 0.42f, Hex(0x5AA8D8));
				for (const FV Spark : { FV(0.4f, 0.36f), FV(0.66f, 0.38f) })
				{
					P.MCapsule(Spark, Spark + FV(0.04f, -0.05f), 0.008f, Hex(0xFFFFFF, 0.9f));
					P.MCapsule(Spark + FV(0.02f, 0.02f), Spark + FV(0.07f, 0.0f), 0.008f, Hex(0xFFFFFF, 0.9f));
				}
				Bubble(P, FV(0.12f, 0.3f), 0.035f);
				Bubble(P, FV(0.2f, 0.22f), 0.025f);
			}
			else if (Index == 1)
			{
				// a reload arrow circling a harpoon in flight
				const float R = 0.36f;
				P.Arc(Mid, R, 0.055f, 130.0f, 395.0f, Hex(0x7FD8F0, 0.95f));
				const FV End = Mid + Spin(FV(R, 0.0f), 395.0f);
				const FV Tan = Spin(FV(0.0f, 1.0f), 395.0f);
				const FV Out = Spin(FV(1.0f, 0.0f), 395.0f);
				P.Poly({ End + Tan * 0.1f, End + Out * 0.08f, End - Out * 0.08f }, Hex(0x7FD8F0));
				{
					FPlace Place(P, Mid, 1.0f, -45.0f);
					P.Capsule(FV(-0.46f, 0.13f), FV(-0.12f, 0.13f), 0.03f, Volt);
					P.Capsule(FV(-0.38f, 0.24f), FV(-0.04f, 0.24f), 0.03f, Volt);
					P.Capsule(FV(-0.46f, -0.13f), FV(-0.16f, -0.13f), 0.03f, Volt);
				}
				Harpoon(P, Mid, 1.1f, -45.0f);
				Bubble(P, FV(0.14f, 0.62f), 0.03f);
				Bubble(P, FV(0.24f, 0.82f), 0.022f);
			}
			else
			{
				// a harpoon head coated in dripping venom
				const FV C(0.42f, 0.58f);
				const float S = 0.95f;
				{
					FPlace Place(P, C, S, -45.0f);
					P.Glow([](FV Q) { return SdCircle(Q, FV(0.32f, 0.0f), 0.12f); }, Hex(0x5AE040), 0.18f, 0.7f);
				}
				Harpoon(P, C, S, -45.0f);
				{
					FPlace Place(P, C, S, -45.0f);
					P.MPoly({ FV(0.16f, -0.07f), FV(0.3f, -0.08f), FV(0.42f, -0.035f), FV(0.36f, 0.0f), FV(0.42f, 0.045f), FV(0.3f, 0.08f), FV(0.16f, 0.07f) }, Hex(0x74D84A, 0.6f));
					P.MPoly({ FV(0.24f, 0.02f), FV(0.06f, 0.17f), FV(0.1f, 0.2f), FV(0.33f, 0.05f) }, Hex(0x74D84A, 0.5f));
				}
				const FV DripTop = C + Spin(FV(0.28f, 0.1f), -45.0f) * S;
				P.Capsule(DripTop, DripTop + FV(0.0f, 0.1f), 0.018f, Poison);
				Drop(P, DripTop + FV(0.0f, 0.2f), 0.24f, Poison);
				Drop(P, FV(0.82f, 0.56f), 0.18f, Poison);
				for (const FVector B : { FVector(0.86f, 0.2f, 0.03f), FVector(0.92f, 0.32f, 0.02f), FVector(0.66f, 0.1f, 0.022f) })
				{
					P.MCircle(FV(B.X, B.Y), B.Z, Hex(0x74D84A, 0.35f));
					P.Mark([B](FV Q) { return FMath::Abs(SdCircle(Q, FV(B.X, B.Y), B.Z)) - 0.004f; }, Hex(0xC8FF9A, 0.9f));
				}
			}
			break;
		case ETDTowerType::Jelly:
			if (Index == 0)
			{
				// a jelly whose stinging tentacles leave frost behind
				Jelly(P, FV(0.42f, 0.44f), 0.8f, Hex(0xF08CD8, 0.95f));
				P.Glow([](FV Q) { return SdCircle(Q, FV(0.78f, 0.26f), 0.12f); }, Frost, 0.16f, 0.6f);
				Snowflake(P, FV(0.78f, 0.26f), 0.38f, Frost);
				for (const FV Tip : { FV(0.24f, 0.85f), FV(0.42f, 0.87f), FV(0.6f, 0.85f) })
				{
					Glint(P, Tip, 0.1f, Hex(0xBFEFFF));
				}
				Snowflake(P, FV(0.86f, 0.58f), 0.16f, Frost);
			}
			else if (Index == 1)
			{
				// toxin that hits slowed prey harder
				const FLinearColor Toxin = Hex(0xA45CE0);
				P.Glow([](FV Q) { return SdCircle(Q, FV(0.46f, 0.64f), 0.2f); }, Hex(0x8A3AD0), 0.14f, 0.5f);
				Drop(P, FV(0.46f, 0.56f), 0.75f, Toxin);
				const FV Core(0.46f, 0.665f);
				P.MArc(Core, 0.12f, 0.022f, 200.0f, 470.0f, Hex(0xE8C8FF, 0.6f));
				P.MArc(Core, 0.06f, 0.018f, 20.0f, 290.0f, Hex(0xE8C8FF, 0.5f));
				P.MCircle(Core, 0.02f, Hex(0xF4E4FF, 0.8f));
				Snowflake(P, FV(0.16f, 0.3f), 0.22f, Frost);
				PlusBadge(P, FV(0.8f, 0.26f), 0.32f, Hex(0xFF6A5A));
				Bubble(P, FV(0.18f, 0.68f), 0.03f);
				Bubble(P, FV(0.8f, 0.76f), 0.025f);
			}
			else
			{
				// pulse rings spreading out from a jelly
				const FLinearColor Ripple = Hex(0xF5B0E4);
				DashedRing(P, Mid, 0.44f, 0.03f, 16, Alpha(Ripple, 0.7f), 4.0f);
				P.Ring(Mid, 0.33f, 0.035f, Alpha(Ripple, 0.9f));
				for (int32 i = 0; i < 4; ++i)
				{
					const FV Dir = Spin(FV(1.0f, 0.0f), 45.0f + i * 90.0f);
					const FV Perp(-Dir.Y, Dir.X);
					const FV Base = Mid + Dir * 0.38f;
					P.Poly({ Base + Dir * 0.11f, Base + Perp * 0.06f, Base - Perp * 0.06f }, Hex(0xFFD8F2));
				}
				Jelly(P, FV(0.5f, 0.45f), 0.52f, Hex(0xF08CD8, 0.95f));
			}
			break;
		case ETDTowerType::Puffer:
			if (Index == 0)
			{
				// a puffed-up puffer with a wider blast ring
				DashedRing(P, FV(0.46f, 0.54f), 0.5f, 0.028f, 18, Hex(0xFFD27A, 0.75f), 6.0f);
				Puffer(P, FV(0.46f, 0.54f), 0.85f, 0.24f);
				UpBadge(P, FV(0.86f, 0.16f), 0.32f);
			}
			else if (Index == 1)
			{
				// spines flying out of the blast
				for (int32 i = 0; i < 8; ++i)
				{
					const FV Dir = Spin(FV(1.0f, 0.0f), i * 45.0f + 22.0f);
					const FV Base = Mid + Dir * 0.32f;
					P.MCapsule(Base - Dir * 0.14f, Base - Dir * 0.02f, 0.012f, Hex(0xFFFFFF, 0.5f));
				}
				Burst(P, Mid, 0.7f);
				for (int32 i = 0; i < 8; ++i)
				{
					const FV Dir = Spin(FV(1.0f, 0.0f), i * 45.0f + 22.0f);
					const FV Base = Mid + Dir * 0.32f;
					P.Ribbon({ Base, Base + Dir * 0.17f }, 0.032f, 0.004f, Hex(0xD8A838));
					P.MCapsule(Base + Dir * 0.02f, Base + Dir * 0.12f, 0.006f, Hex(0xFFF0B0, 0.7f));
				}
			}
			else
			{
				// a heavy shell with dizzy stars orbiting it
				const FV C(0.48f, 0.54f);
				const auto Orbit = [C](FV Q, float Lo, float Hi)
				{
					const float D = FMath::Abs(SdEllipse(Q, C, 0.42f, 0.12f)) - 0.016f;
					return FMath::Max(D, FMath::Max(Lo - Q.Y, Q.Y - Hi));
				};
				P.Mark([&Orbit, C](FV Q) { return Orbit(Q, -1.0f, C.Y); }, Hex(0xFFE9A0, 0.55f));
				P.Line({ FV(0.6f, 0.34f), FV(0.66f, 0.22f), FV(0.76f, 0.18f) }, 0.022f, Hex(0xC9A46A));
				P.Ball(C, 0.27f, Hex(0x3A4652), 0.5f);
				{
					FClip Clip(P, [C](FV Q) { return SdCircle(Q, C, 0.27f); });
					P.MCapsule(C + FV(-0.3f, 0.08f), C + FV(0.3f, 0.08f), 0.03f, Hex(0x262E36));
					for (int32 i = 0; i < 5; ++i)
					{
						Rivet(P, C + FV(-0.2f + i * 0.1f, 0.08f), 0.016f);
					}
				}
				P.Box(FV(0.52f, 0.28f), FV(0.66f, 0.34f), 0.015f, Iron);
				Glint(P, FV(0.78f, 0.17f), 0.16f, Hex(0xFFC060));
				P.Mark([&Orbit, C](FV Q) { return Orbit(Q, C.Y, 2.0f); }, Hex(0xFFE9A0, 0.85f));
				StarIcon(P, C + FV(-0.36f, 0.06f), 0.18f, Volt);
				StarIcon(P, C + FV(0.08f, 0.12f), 0.2f, Volt);
				StarIcon(P, C + FV(0.38f, 0.04f), 0.15f, Volt);
			}
			break;
		case ETDTowerType::Eel:
			if (Index == 0)
			{
				// lightning chaining from target to target
				const FV Nodes[] = { FV(0.14f, 0.72f), FV(0.38f, 0.3f), FV(0.62f, 0.7f), FV(0.86f, 0.28f) };
				for (int32 i = 0; i < 3; ++i)
				{
					Zap(P, Nodes[i], Nodes[i + 1], 0.022f, i + 3, Volt);
				}
				for (int32 i = 0; i < 4; ++i)
				{
					const float R = i == 0 ? 0.09f : 0.07f;
					P.Glow([&Nodes, i, R](FV Q) { return SdCircle(Q, Nodes[i], R); }, Hex(0xFFD040), 0.08f, 0.7f);
					P.Ball(Nodes[i], R, i == 0 ? Hex(0xFFF2A0) : Hex(0x2A4A66), 0.7f);
					if (i > 0)
					{
						P.MArc(Nodes[i], R * 0.6f, 0.012f, 200.0f, 300.0f, Hex(0xFFE24A, 0.9f));
					}
				}
			}
			else if (Index == 1)
			{
				// a shock that leaves its target seeing stars
				const FV Ring(0.5f, 0.16f);
				P.Mark([Ring](FV Q) { return FMath::Abs(SdEllipse(Q, Ring, 0.3f, 0.08f)) - 0.012f; }, Hex(0xBFEFFF, 0.6f));
				Bolt(P, FV(0.46f, 0.58f), 0.8f, Volt);
				Zap(P, FV(0.14f, 0.5f), FV(0.28f, 0.7f), 0.012f, 7, Hex(0xBFEFFF));
				Zap(P, FV(0.66f, 0.66f), FV(0.86f, 0.52f), 0.012f, 9, Hex(0xBFEFFF));
				StarIcon(P, Ring + FV(-0.26f, 0.02f), 0.16f, White);
				StarIcon(P, Ring + FV(0.04f, 0.07f), 0.2f, White);
				StarIcon(P, Ring + FV(0.28f, 0.0f), 0.14f, White);
			}
			else
			{
				// lightning cracking through armour
				Shield(P, FV(0.42f, 0.54f), 0.8f, Hex(0x7E8C96));
				const FV Hit(0.5f, 0.5f);
				const TArray<FV> Cracks[] = {
					{ Hit, FV(0.4f, 0.44f), FV(0.33f, 0.47f), FV(0.24f, 0.38f) },
					{ Hit, FV(0.46f, 0.6f), FV(0.38f, 0.64f), FV(0.36f, 0.74f) },
					{ Hit, FV(0.52f, 0.62f), FV(0.5f, 0.7f) } };
				for (const TArray<FV>& Crack : Cracks)
				{
					P.Glow([&Crack](FV Q) { return SdPolyline(Q, Crack, 0.006f); }, Hex(0xFFD040), 0.04f, 0.6f);
					P.MLine(Crack, 0.011f, Hex(0x1A2228));
					P.MLine(Crack, 0.004f, Hex(0xFFE88A));
				}
				Bolt(P, FV(0.64f, 0.4f), 0.7f, Volt, 12.0f);
				Glint(P, Hit, 0.16f, Hex(0xFFF4C0));
			}
			break;
		case ETDTowerType::Mantis:
			if (Index == 0)
			{
				// the smashing club landing with a crack
				const FV Impact(0.74f, 0.26f);
				P.Glow([Impact](FV Q) { return SdCircle(Q, Impact, 0.1f); }, Hex(0xFFB040), 0.16f, 0.7f);
				P.Star(Impact, 0.22f, 0.09f, 8, 10.0f, Hex(0xFFE26A));
				P.Star(Impact, 0.12f, 0.05f, 8, 32.0f, Hex(0xFFFBE0));
				for (int32 i = 0; i < 3; ++i)
				{
					P.MArc(FV(0.6f, 0.46f), 0.3f + i * 0.06f, 0.014f, 150.0f, 200.0f, Hex(0xFFFFFF, 0.65f - i * 0.15f));
				}
				const FLinearColor Limb = Hex(0xE85A8A);
				P.Capsule(FV(0.12f, 0.88f), FV(0.3f, 0.68f), 0.065f, Limb);
				P.Capsule(FV(0.3f, 0.68f), FV(0.46f, 0.52f), 0.075f, Tone(Limb, 1.08f));
				P.Ball(FV(0.3f, 0.68f), 0.06f, Hex(0x48C8A0), 0.4f);
				P.MCapsule(FV(0.15f, 0.83f), FV(0.27f, 0.69f), 0.012f, Hex(0xFFC0D8, 0.6f));
				P.MCapsule(FV(0.33f, 0.62f), FV(0.43f, 0.52f), 0.012f, Hex(0xFFC0D8, 0.6f));
				P.Ball(FV(0.56f, 0.42f), 0.22f, Hex(0xE8342E));
				{
					FClip Clip(P, [](FV Q) { return SdCircle(Q, FV(0.56f, 0.42f), 0.22f); });
					P.MCircle(FV(0.68f, 0.3f), 0.11f, Hex(0xFFD23C));
					P.MCircle(FV(0.7f, 0.28f), 0.05f, Hex(0xFFF2A0, 0.8f));
					for (const FV Spot : { FV(0.46f, 0.32f), FV(0.42f, 0.46f), FV(0.54f, 0.56f), FV(0.66f, 0.48f), FV(0.52f, 0.42f) })
					{
						P.MCircle(Spot, 0.022f, Hex(0x10080C));
					}
				}
			}
			else if (Index == 1)
			{
				// an empty moulted shell, worth a few pearls
				const TArray<FV> Spine = Smooth({ FV(0.16f, 0.3f), FV(0.32f, 0.36f), FV(0.46f, 0.5f), FV(0.52f, 0.68f) });
				const FLinearColor Husk = Hex(0x6AD8B8, 0.88f);
				FV Pos, Tan;
				Along(Spine, 1.0f, Pos, Tan);
				for (int32 i = -1; i <= 1; ++i)
				{
					FPlace Place(P, Pos + Tan * 0.06f, 1.0f, FMath::RadiansToDegrees(FMath::Atan2(Tan.Y, Tan.X)) + i * 28.0f);
					P.Blob({ FV(0.0f, -0.03f), FV(0.1f, -0.05f), FV(0.17f, 0.0f), FV(0.1f, 0.05f), FV(0.0f, 0.03f) }, Husk);
					P.MCapsule(FV(0.03f, 0.0f), FV(0.14f, 0.0f), 0.006f, Hex(0x1C6E70, 0.7f));
				}
				for (int32 i = 5; i >= 0; --i)
				{
					Along(Spine, i / 5.0f, Pos, Tan);
					const float R = 0.13f - i * 0.012f;
					FPlace Place(P, Pos, 1.0f, FMath::RadiansToDegrees(FMath::Atan2(Tan.Y, Tan.X)));
					const auto Plate = [R](FV Q) { return SdEllipse(Q, FV(0.0f, 0.0f), R * 0.62f, R); };
					P.Fill(Plate, Husk);
					FClip Clip(P, Plate);
					P.MArc(FV(-R * 0.5f, 0.0f), R * 0.9f, 0.012f, -70.0f, 70.0f, Hex(0x1C6E70, 0.7f));
					P.MEllipse(FV(-R * 0.15f, -R * 0.45f), R * 0.28f, R * 0.18f, Hex(0xFFFFFF, 0.45f), 0.02f);
				}
				P.Capsule(FV(0.12f, 0.27f), FV(0.04f, 0.12f), 0.01f, Hex(0x6AD8B8));
				P.Capsule(FV(0.16f, 0.24f), FV(0.18f, 0.08f), 0.01f, Hex(0x6AD8B8));
				PearlBall(P, FV(0.76f, 0.4f), 0.13f);
				PearlBall(P, FV(0.82f, 0.64f), 0.08f);
				Glint(P, FV(0.86f, 0.22f), 0.16f);
			}
			else
			{
				// stalked eyes that see colours no other creature can
				MantisEye(P, FV(0.28f, 0.5f), 0.56f, -18.0f);
				MantisEye(P, FV(0.62f, 0.48f), 0.56f, 16.0f);
				UpBadge(P, FV(0.88f, 0.22f), 0.3f);
			}
			break;
		case ETDTowerType::Octopus:
			if (Index == 0)
			{
				// a chipped stone with razor edges
				TArray<FV> Pts;
				for (int32 i = 0; i < 18; ++i)
				{
					const uint32 H = (uint32)(i * 104729 + 31) * 2654435761u;
					const float Noise = (float)((H >> 8) & 0xFFFF) / 65535.0f;
					const float R = (i % 2 == 0) ? 0.38f + 0.07f * Noise : 0.25f + 0.04f * Noise;
					Pts.Add(Mid + Spin(FV(R, 0.0f), i * 20.0f + 5.0f));
				}
				{
					FWith With(P);
					P.Look.Grain = 0.14f;
					P.Look.Dome = 0.1f;
					P.Poly(Pts, Stone);
				}
				FClip Clip(P, [&Pts](FV Q) { return SdPoly(Q, Pts); });
				for (int32 i = 0; i < 18; i += 2)
				{
					const FV Tip = Pts[i];
					const bool bLit = (Tip.X - Mid.X) + (Tip.Y - Mid.Y) < 0.0f;
					P.MPoly({ Mid + (Pts[(i + 17) % 18] - Mid) * 0.7f, Tip, Mid + (Pts[i + 1] - Mid) * 0.7f }, bLit ? Hex(0xFFFFFF, 0.28f) : Hex(0x20282E, 0.25f));
					P.MCapsule(Mid + (Tip - Mid) * 0.45f, Tip, 0.006f, Hex(0xFFFFFF, 0.35f));
				}
				P.Star(Mid + FV(-0.02f, 0.0f), 0.2f, 0.15f, 7, 0.0f, Hex(0xAEB6BC));
				P.MCapsule(FV(0.36f, 0.56f), FV(0.5f, 0.6f), 0.008f, Hex(0x40484E, 0.7f));
			}
			else if (Index == 1)
			{
				// a curling arm flinging a volley of stones, each trailing speed lines
				const FV From(0.34f, 0.4f);
				const FV To[] = { FV(0.74f, 0.16f), FV(0.84f, 0.45f), FV(0.72f, 0.75f) };
				const float Size[] = { 0.1f, 0.12f, 0.09f };
				P.MArc(FV(0.12f, 0.66f), 0.3f, 0.016f, -85.0f, -15.0f, Hex(0xFFFFFF, 0.45f));
				P.MArc(FV(0.12f, 0.66f), 0.37f, 0.011f, -75.0f, -25.0f, Hex(0xFFFFFF, 0.3f));
				for (int32 i = 0; i < 3; ++i)
				{
					const FV D = (To[i] - From).GetSafeNormal(), N(-D.Y, D.X);
					for (int32 j = -1; j <= 1; ++j)
					{
						const FV S = To[i] + N * (j * Size[i] * 0.5f);
						P.MCapsule(S - D * (Size[i] + 0.02f), S - D * (Size[i] + 0.15f - 0.05f * FMath::Abs(j)), 0.011f, Hex(0xE8F2F6, 0.8f));
					}
					P.Circle(FMath::Lerp(From, To[i], 0.22f), 0.018f, Hex(0xE8F2F6, 0.8f));
				}
				// a puff of silt where the arm snapped
				for (const FVector D : { FVector(0.4f, 0.3f, 0.035f), FVector(0.44f, 0.4f, 0.028f), FVector(0.38f, 0.48f, 0.025f) })
				{
					P.Ball(FV(D.X, D.Y), D.Z, Hex(0xD8D0C0), 0.1f);
				}
				Tentacle(P, { FV(0.08f, 0.96f), FV(0.22f, 0.78f), FV(0.12f, 0.58f), FV(0.2f, 0.4f), FV(0.34f, 0.36f) }, 0.1f, 0.026f, -1.0f, Hex(0xC8607A), Hex(0xF6CACA));
				for (int32 i = 0; i < 3; ++i)
				{
					Rock(P, To[i], Size[i], i + 2, Hex(0xA7AFB5));
					P.Gloss(To[i] + FV(-Size[i] * 0.3f, -Size[i] * 0.35f), Size[i] * 0.3f, Size[i] * 0.15f, 0.45f);
				}
			}
			else
			{
				// a long lob: a heavy cracked stone arcing down onto a bullseye, kicking up dust
				const FV A(0.1f, 0.82f), B(0.42f, -0.02f), C(0.84f, 0.66f);
				P.MEllipse(C + FV(0.02f, 0.1f), 0.17f, 0.05f, Hex(0x0A1418, 0.45f), 0.03f);
				{
					FWith With(P);
					P.Look.Dome = 0.03f;
					P.Ellipse(C + FV(0.0f, 0.06f), 0.16f, 0.066f, Hex(0xF4FAFC));
				}
				P.MEllipse(C + FV(0.0f, 0.06f), 0.12f, 0.048f, Hex(0xE04A4A));
				P.MEllipse(C + FV(0.0f, 0.06f), 0.075f, 0.03f, Hex(0xF4FAFC));
				P.MEllipse(C + FV(0.0f, 0.06f), 0.035f, 0.014f, Hex(0xE04A4A));
				for (const FVector D : { FVector(-0.2f, 0.07f, 0.03f), FVector(-0.15f, 0.1f, 0.024f), FVector(-0.24f, 0.11f, 0.018f), FVector(0.2f, 0.08f, 0.028f), FVector(0.15f, 0.11f, 0.022f), FVector(0.25f, 0.12f, 0.016f) })
				{
					P.MEllipse(C + FV(D.X, D.Y), D.Z * 1.3f, D.Z, Hex(0xD8D0C0, 0.75f), D.Z * 0.5f);
				}
				for (const float S : { -1.0f, 1.0f })
				{
					P.Circle(C + FV(S * 0.2f, -0.02f), 0.013f, Hex(0xB8B0A0));
					P.Circle(C + FV(S * 0.25f, 0.02f), 0.01f, Hex(0xB8B0A0));
				}
				for (int32 i = 0; i < 16; ++i)
				{
					const float T = i / 16.0f;
					if (T > 0.4f && T < 0.64f)
					{
						continue;
					}
					P.Capsule(Bezier(A, B, C, T), Bezier(A, B, C, T + 0.022f), 0.013f + 0.01f * T, Hex(0xE8F2F6, 0.9f));
				}
				const float T = 0.52f;
				const FV Pos = Bezier(A, B, C, T);
				const FV Tan = (Bezier(A, B, C, T + 0.01f) - Pos).GetSafeNormal();
				const FV Nrm(-Tan.Y, Tan.X);
				for (int32 i = -1; i <= 1; ++i)
				{
					P.MCapsule(Pos - Tan * (0.17f + 0.04f * (i != 0)) + Nrm * (i * 0.07f), Pos - Tan * (0.36f - 0.06f * (i != 0)) + Nrm * (i * 0.07f), 0.013f, Hex(0xFFFFFF, 0.65f));
				}
				Rock(P, Pos, 0.15f, 5, Stone);
				P.MLine({ Pos + FV(-0.1f, -0.03f), Pos + FV(-0.04f, -0.01f), Pos + FV(0.0f, 0.02f), Pos + FV(0.06f, 0.03f) }, 0.008f, Hex(0x3A4248, 0.85f));
				P.MLine({ Pos + FV(0.0f, 0.02f), Pos + FV(0.01f, 0.08f) }, 0.006f, Hex(0x3A4248, 0.8f));
				P.MLine({ Pos + FV(-0.04f, -0.01f), Pos + FV(-0.03f, -0.07f) }, 0.005f, Hex(0x3A4248, 0.75f));
				P.Gloss(Pos + FV(-0.05f, -0.07f), 0.04f, 0.02f, 0.5f);
				{
					FWith With(P);
					P.Look.Dome = 0.05f;
					P.Look.Spec = 0.7f;
					Arrow(P, FV(0.84f, 0.36f), 0.24f, Hex(0xE04A4A), 180.0f);
				}
			}
			break;
		case ETDTowerType::Lionfish:
			if (Index == 0)
			{
				// a fan of striped venom spines dripping poison
				const FV Base(0.22f, 0.84f);
				const FV Tips[] = { FV(0.26f, 0.14f), FV(0.52f, 0.16f), FV(0.74f, 0.32f) };
				P.Blob({ Base, Tips[0] + FV(0.0f, 0.14f), FV(0.4f, 0.3f), Tips[1] + FV(0.0f, 0.12f), FV(0.62f, 0.42f), Tips[2] + FV(-0.04f, 0.1f), FV(0.5f, 0.66f) }, Hex(0xE8805A, 0.6f));
				{
					FClip Clip(P, [](FV Q) { return SdCircle(Q, FV(0.22f, 0.84f), 0.8f); });
					for (int32 i = 0; i < 6; ++i)
					{
						P.MCapsule(Base, FMath::Lerp(Tips[i / 2], Tips[FMath::Min(i / 2 + 1, 2)], (i % 2) * 0.5f) * 0.85f + Base * 0.15f, 0.006f, Hex(0x9A2A1A, 0.5f));
					}
				}
				for (const FV& Tip : Tips)
				{
					P.Glow([Tip](FV Q) { return SdCircle(Q, Tip, 0.02f); }, Hex(0x74D84A), 0.06f, 0.8f);
					VenomSpine(P, Base, Tip, 0.042f);
				}
				P.Ball(Base, 0.06f, Hex(0xC8322A), 0.4f);
				P.Capsule(Tips[2], Tips[2] + FV(0.0f, 0.12f), 0.014f, Poison);
				Drop(P, Tips[2] + FV(0.02f, 0.28f), 0.3f, Poison);
				PlusBadge(P, FV(0.84f, 0.12f), 0.22f, Hex(0xFF6A5A));
			}
			else if (Index == 1)
			{
				// venom that keeps running like sand in a glass
				Hourglass(P, FV(0.42f, 0.5f), 0.85f, Poison);
				P.Glow([](FV Q) { return SdCircle(Q, FV(0.78f, 0.74f), 0.06f); }, Hex(0x74D84A), 0.1f, 0.6f);
				Drop(P, FV(0.78f, 0.7f), 0.36f, Poison);
				for (int32 i = 0; i < 3; ++i)
				{
					P.MArc(FV(0.78f, 0.36f), 0.06f + i * 0.04f, 0.012f, 200.0f + i * 20.0f, 300.0f + i * 20.0f, Hex(0x9AF07A, 0.75f - i * 0.18f));
				}
			}
			else
			{
				// an open clam with a treasure inside
				Clam(P, FV(0.5f, 0.54f), 1.0f);
				Glint(P, FV(0.8f, 0.2f), 0.2f);
				Glint(P, FV(0.2f, 0.26f), 0.13f);
				Bubble(P, FV(0.86f, 0.42f), 0.03f);
				Bubble(P, FV(0.12f, 0.46f), 0.022f);
			}
			break;
		case ETDTowerType::Sawfish:
			if (Index == 0)
			{
				// a toothed saw dripping blood
				SawSnout(P, FV(0.12f, 0.8f), FV(0.86f, 0.22f), 0.09f, 7);
				P.Capsule(FV(0.68f, 0.44f), FV(0.7f, 0.56f), 0.016f, Blood);
				Drop(P, FV(0.72f, 0.72f), 0.32f, Blood);
				P.Circle(FV(0.86f, 0.62f), 0.026f, Blood);
				P.Circle(FV(0.56f, 0.88f), 0.02f, Blood);
			}
			else if (Index == 1)
			{
				// teeth sprayed out in a fan
				SawSnout(P, FV(0.02f, 0.62f), FV(0.32f, 0.46f), 0.08f, 3);
				for (int32 i = 0; i < 4; ++i)
				{
					const FV Dir = Spin(FV(1.0f, 0.0f), -42.0f + i * 28.0f);
					const FV Root = FV(0.4f, 0.48f) + Dir * 0.22f;
					P.MCapsule(Root - Dir * 0.18f, Root - Dir * 0.02f, 0.012f, Hex(0xFFFFFF, 0.55f));
					Fang(P, Root, Root + Dir * 0.24f, 0.05f, 0.03f);
				}
				Glint(P, FV(0.4f, 0.48f), 0.18f, Hex(0xFFF6DE));
			}
			else
			{
				// a saw that senses the electric fields of prey
				const FV Tip(0.66f, 0.5f);
				for (int32 i = 0; i < 3; ++i)
				{
					P.Glow([Tip, i](FV Q) { return SdArc(Q, Tip, 0.1f + i * 0.09f, 0.01f, -80.0f, 40.0f); }, Hex(0x5AD8FF), 0.03f, 0.5f);
					P.Arc(Tip, 0.1f + i * 0.09f, 0.024f, -80.0f, 40.0f, Hex(0x9BEFFF, 0.95f - i * 0.2f));
				}
				SawSnout(P, FV(0.06f, 0.78f), Tip, 0.085f, 6);
				Bolt(P, FV(0.86f, 0.16f), 0.34f, Volt, 12.0f);
			}
			break;
		case ETDTowerType::Humpback:
			if (Index == 0)
			{
				// a whale tail raised in song
				const FV Water(0.4f, 0.84f);
				for (int32 i = 0; i < 3; ++i)
				{
					P.Mark([Water, i](FV Q) { return FMath::Abs(SdEllipse(Q, Water, 0.16f + i * 0.09f, 0.04f + i * 0.022f)) - 0.008f; }, Hex(0x9BE0F0, 0.85f - i * 0.25f));
				}
				Fluke(P, FV(0.4f, 0.56f), 0.68f, Hex(0x50648C));
				MusicNotes(P, FV(0.78f, 0.26f), 0.4f, Gold);
				P.Circle(FV(0.16f, 0.66f), 0.022f, Hex(0xBFEFFF, 0.9f));
				P.Circle(FV(0.66f, 0.7f), 0.018f, Hex(0xBFEFFF, 0.9f));
			}
			else if (Index == 1)
			{
				// song carrying through deep water
				for (int32 i = 0; i < 3; ++i)
				{
					P.Arc(FV(0.5f, 0.48f), 0.15f + i * 0.13f, 0.045f, -48.0f, 48.0f, Hex(0x9BE0F0, 0.95f - i * 0.22f));
				}
				WhaleHead(P, FV(0.22f, 0.48f), 0.52f, Hex(0x50648C));
				Bubble(P, FV(0.68f, 0.12f), 0.03f);
				Bubble(P, FV(0.78f, 0.86f), 0.024f);
			}
			else
			{
				// a songbook handing tunes on, with experience to spare
				Book(P, FV(0.44f, 0.68f), 0.72f, Hex(0x3C7FA0));
				P.MArc(FV(0.44f, 0.6f), 0.24f, 0.012f, 220.0f, 320.0f, Hex(0x9BE0F0, 0.6f));
				MusicNotes(P, FV(0.5f, 0.26f), 0.4f, Hex(0x5CD89A));
				StarIcon(P, FV(0.86f, 0.38f), 0.2f, Gold);
				StarIcon(P, FV(0.14f, 0.32f), 0.15f, Gold);
				Glint(P, FV(0.8f, 0.14f), 0.12f);
			}
			break;
		case ETDTowerType::AcidVent:
			if (Index == 0)
			{
				// acid eating through a shield
				const FV C(0.42f, 0.54f);
				const float S = 0.8f;
				Shield(P, C, S, Hex(0x7E8C96));
				{
					const TArray<FV> Outline = { C + FV(-0.39f, -0.45f) * S, C + FV(0.39f, -0.45f) * S, C + FV(0.39f, 0.03f) * S, C + FV(0.0f, 0.5f) * S, C + FV(-0.39f, 0.03f) * S };
					FClip Clip(P, [&Outline](FV Q) { return SdPoly(Q, Outline); });
					P.MPoly({ FV(0.4f, 0.16f), FV(0.8f, 0.16f), FV(0.8f, 0.3f), FV(0.6f, 0.34f), FV(0.5f, 0.28f) }, Hex(0xC4F030, 0.85f));
					P.MCapsule(FV(0.54f, 0.28f), FV(0.54f, 0.42f), 0.022f, Hex(0xC4F030, 0.85f));
					P.MCapsule(FV(0.66f, 0.3f), FV(0.66f, 0.5f), 0.026f, Hex(0xC4F030, 0.85f));
					for (const FVector Hole : { FVector(0.34f, 0.6f, 0.05f), FVector(0.46f, 0.74f, 0.035f), FVector(0.58f, 0.6f, 0.03f) })
					{
						P.MCircle(FV(Hole.X, Hole.Y), Hole.Z + 0.02f, Hex(0x8CC020, 0.9f));
						P.MCircle(FV(Hole.X, Hole.Y), Hole.Z, Hex(0x10141A));
					}
				}
				P.Glow([](FV Q) { return SdCircle(Q, FV(0.74f, 0.4f), 0.1f); }, Hex(0xA8E020), 0.12f, 0.6f);
				Drop(P, FV(0.74f, 0.34f), 0.5f, Hex(0xC4F030));
				for (const FVector B : { FVector(0.2f, 0.2f, 0.028f), FVector(0.3f, 0.1f, 0.02f), FVector(0.9f, 0.6f, 0.024f) })
				{
					P.MCircle(FV(B.X, B.Y), B.Z, Hex(0xC4F030, 0.3f));
					P.Mark([B](FV Q) { return FMath::Abs(SdCircle(Q, FV(B.X, B.Y), B.Z)) - 0.004f; }, Hex(0xE8FF9A, 0.9f));
				}
				Arrow(P, FV(0.84f, 0.8f), 0.24f, Hex(0xFF6A5A), 180.0f);
			}
			else if (Index == 1)
			{
				// a vent belching a thick cloud of silt
				const FLinearColor Silt = Hex(0x9A8560);
				{
					FWith With(P);
					P.Look.Grain = 0.14f;
					P.Look.Dome = 0.05f;
					P.Poly({ FV(0.34f, 0.97f), FV(0.66f, 0.97f), FV(0.58f, 0.74f), FV(0.42f, 0.74f) }, Hex(0x6A5446));
				}
				P.MCapsule(FV(0.4f, 0.86f), FV(0.6f, 0.86f), 0.01f, Hex(0x3A2E26, 0.7f));
				P.Ellipse(FV(0.5f, 0.74f), 0.09f, 0.03f, Hex(0x2A2018));
				const FVector Puffs[] = { FVector(0.5f, 0.62f, 0.1f), FVector(0.34f, 0.46f, 0.15f), FVector(0.66f, 0.44f, 0.16f), FVector(0.5f, 0.3f, 0.18f), FVector(0.26f, 0.26f, 0.12f), FVector(0.76f, 0.24f, 0.12f) };
				for (int32 i = 0; i < 6; ++i)
				{
					const FVector& Puff = Puffs[i];
					const FV C(Puff.X, Puff.Y);
					P.Ball(C, Puff.Z, Tone(Silt, 0.9f + 0.05f * (i % 3)), 0.12f);
					FClip Clip(P, [C, Puff](FV Q) { return SdCircle(Q, C, Puff.Z); });
					P.MArc(C + FV(0.02f, 0.02f), Puff.Z * 0.55f, Puff.Z * 0.12f, 160.0f, 300.0f, Hex(0xD8C8A0, 0.4f));
					P.MCircle(C + FV(Puff.Z * 0.3f, Puff.Z * 0.35f), Puff.Z * 0.12f, Hex(0x5A4A30, 0.35f));
				}
				for (int32 i = 0; i < 6; ++i)
				{
					P.Circle(FV(0.14f + i * 0.145f, 0.68f + (i % 2) * 0.08f), 0.016f + 0.006f * (i % 2), Hex(0xC8B488));
				}
			}
			else
			{
				// crystals sprouting from a vent chimney
				Rock(P, FV(0.44f, 0.76f), 0.3f, 4, Hex(0x985030));
				Crystal(P, FV(0.3f, 0.7f), 0.3f, 0.065f, -28.0f, Hex(0x6AE0C8));
				Crystal(P, FV(0.6f, 0.7f), 0.32f, 0.065f, 26.0f, Hex(0xF2D438));
				Crystal(P, FV(0.45f, 0.68f), 0.5f, 0.09f, -4.0f, Hex(0xF2D438));
				P.MEllipse(FV(0.44f, 0.71f), 0.16f, 0.03f, Hex(0x3A1E10, 0.5f), 0.02f);
				Glint(P, FV(0.42f, 0.18f), 0.16f);
				UpBadge(P, FV(0.84f, 0.24f), 0.3f);
			}
			break;
		default:
			break;
		}
	}

	void PaintItem(FPainter& P, ETDItemSlot Slot)
	{
		switch (Slot)
		{
		case ETDItemSlot::Shell:
		{
			// scallop: a fan of raised ribs with growth bands, on a hinge
			const FV Apex(0.5f, 0.84f);
			auto Rim = [&Apex](float Deg, float R) { const FV V = Spin(FV(1.0f, 0.0f), Deg); return Apex + V * R; };

			P.Poly({ FV(0.3f, 0.86f), FV(0.42f, 0.74f), FV(0.58f, 0.74f), FV(0.7f, 0.86f), FV(0.62f, 0.92f), FV(0.38f, 0.92f) }, Hex(0xD08E6A));
			P.MCapsule(FV(0.38f, 0.84f), FV(0.62f, 0.84f), 0.006f, Hex(0x7A4A30, 0.8f));

			TArray<FV> Fan = { Apex };
			const int32 Ribs = 7;
			const float A0 = -152.0f, Step = 124.0f / Ribs;
			for (int32 i = 0; i <= Ribs * 4; ++i)
			{
				const float T = (float)i / (Ribs * 4);
				// a scallop's edge: one bump per rib
				const float Bump = FMath::Abs(FMath::Sin(T * Ribs * PI)) * 0.045f;
				Fan.Add(Rim(A0 + T * Ribs * Step, 0.62f + Bump));
			}
			const auto Shell = [&Fan](FV Q) { return SdPoly(Q, Fan); };
			{
				FWith With(P);
				P.Look.Dome = 0.18f;
				P.Fill(Shell, Hex(0xF7CBA4));
			}
			FClip Clip(P, Shell);
			// colour bands across the ribs
			P.Mark([&Apex](FV Q) { return FMath::Abs((Q - Apex).Size() - 0.5f) - 0.045f; }, Hex(0xE8805E, 0.8f));
			P.Mark([&Apex](FV Q) { return FMath::Abs((Q - Apex).Size() - 0.31f) - 0.03f; }, Hex(0xE8805E, 0.7f));
			P.Mark([&Apex](FV Q) { return FMath::Abs((Q - Apex).Size() - 0.61f) - 0.014f; }, Hex(0xFFEAD4, 0.9f));
			// ribs: a dark groove and a lit crest per rib
			for (int32 i = 0; i <= Ribs; ++i)
			{
				P.MCapsule(Rim(A0 + i * Step, 0.08f), Rim(A0 + i * Step, 0.7f), 0.0095f, Hex(0x8E4A30, 0.95f));
			}
			for (int32 i = 0; i < Ribs; ++i)
			{
				P.MCapsule(Rim(A0 + (i + 0.4f) * Step, 0.14f), Rim(A0 + (i + 0.4f) * Step, 0.62f), 0.011f, Hex(0xFFF2E0, 0.75f));
			}
			break;
		}
		case ETDItemSlot::Pearl:
		{
			// a pearl resting in an open clam
			const auto Upper = [](FV Q) { return FMath::Max(SdEllipse(Q, FV(0.5f, 0.5f), 0.44f, 0.38f), Q.Y - 0.56f); };
			{
				FWith With(P);
				P.Look.Dome = 0.08f;
				P.Fill(Upper, Hex(0x8678A8));
			}
			{
				FClip Clip(P, Upper);
				P.MEllipse(FV(0.5f, 0.56f), 0.36f, 0.32f, Hex(0xE4D4F0));
				for (int32 i = 0; i < 7; ++i)
				{
					const FV Dir = Spin(FV(1.0f, 0.0f), -160.0f + i * 23.3f);
					P.MCapsule(FV(0.5f, 0.56f) + Dir * 0.1f, FV(0.5f, 0.56f) + Dir * 0.44f, 0.007f, Hex(0x8E7AA8, 0.65f));
				}
			}
			const auto Lower = [](FV Q) { return FMath::Max(SdEllipse(Q, FV(0.5f, 0.58f), 0.46f, 0.32f), 0.6f - Q.Y); };
			{
				FWith With(P);
				P.Look.Dome = 0.1f;
				P.Fill(Lower, Hex(0x7A6C9C));
			}
			{
				FClip Clip(P, Lower);
				for (int32 i = 0; i < 6; ++i)
				{
					const FV Dir = Spin(FV(1.0f, 0.0f), 28.0f + i * 25.0f);
					P.MCapsule(FV(0.5f, 0.56f) + Dir * 0.12f, FV(0.5f, 0.56f) + Dir * 0.4f, 0.0075f, Hex(0x3E345A, 0.7f));
				}
				P.MEllipse(FV(0.34f, 0.67f), 0.11f, 0.02f, Hex(0xFFFFFF, 0.3f), 0.02f);
			}
			PearlBall(P, FV(0.5f, 0.45f), 0.235f);
			break;
		}
		case ETDItemSlot::Charm:
		{
			// gold amulet on a chain, set with a faceted teal gem
			const FV Mid(0.5f, 0.63f);
			P.Arc(FV(0.5f, 0.28f), 0.2f, 0.02f, -190.0f, 10.0f, Hex(0xB8862E));
			for (int32 i = 0; i < 11; ++i)
			{
				const FV V = Spin(FV(0.2f, 0.0f), -190.0f + i * 20.0f);
				P.Ring(FV(0.5f, 0.28f) + V, 0.024f, 0.013f, Hex(0xFFE08A));
			}
			P.Ring(FV(0.5f, 0.355f), 0.032f, 0.016f, Gold);
			P.Box(FV(0.46f, 0.37f), FV(0.54f, 0.42f), 0.015f, Hex(0xC8962E));

			// the setting: a lobed gold frame
			auto Lobes = [Mid](FV Q)
			{
				const float A = FMath::Atan2(Q.Y - Mid.Y, Q.X - Mid.X);
				return (Q - Mid).Size() - 0.255f - FMath::Cos(A * 8.0f) * 0.016f;
			};
			{
				FWith With(P);
				P.Look.Dome = 0.08f;
				P.Look.Spec = 0.8f;
				P.Fill(Lobes, Gold);
			}
			for (int32 i = 0; i < 8; ++i)
			{
				P.Ball(Mid + Spin(FV(0.215f, 0.0f), i * 45.0f), 0.017f, Hex(0xFFF0B0), 0.8f);
			}
			P.Circle(Mid, 0.178f, Hex(0x8A6420));

			// faceted gem
			TArray<FV> Outer, Table;
			for (int32 i = 0; i < 8; ++i)
			{
				Outer.Add(Mid + Spin(FV(0.16f, 0.0f), i * 45.0f + 22.5f));
				Table.Add(Mid + FV(-0.008f, -0.012f) + Spin(FV(0.092f, 0.0f), i * 45.0f + 22.5f));
			}
			P.MPoly(Outer, Hex(0x18A8A0));
			for (int32 i = 0; i < 8; ++i)
			{
				const int32 N = (i + 1) % 8;
				const float Lit = 0.5f + 0.5f * FV::DotProduct(Spin(FV(1.0f, 0.0f), i * 45.0f + 45.0f), FV(-0.55f, -0.83f));
				P.MPoly({ Outer[i], Outer[N], Table[N], Table[i] }, FMath::Lerp(Hex(0x0A5A62), Hex(0x8CF4E4), Lit));
			}
			P.MPoly(Table, Hex(0x36D4C8));
			P.MPoly({ Table[3], Table[4], Table[5], Mid + FV(0.01f, 0.0f) }, Hex(0xC8FFF4, 0.75f));
			P.Mark([&Outer](FV Q) { return FMath::Abs(SdPoly(Q, Outer)) - 0.004f; }, Hex(0x053A40));
			P.Star(Mid + FV(-0.075f, -0.09f), 0.05f, 0.012f, 4, 0.0f, Hex(0xFFFFFF));

			// a dangling bead
			P.Capsule(Mid + FV(0.0f, 0.26f), Mid + FV(0.0f, 0.3f), 0.008f, Gold);
			P.Ball(Mid + FV(0.0f, 0.325f), 0.026f, Gold, 0.9f);
			break;
		}
		default:
			break;
		}
	}

	// ---- item types: one icon per entry of the item table (TD::ItemKind) ----

	/** A pearl in its own colours: base, a shadow sheen and a light sheen */
	void TintedPearl(FPainter& P, FV C, float R, const FLinearColor& Base, const FLinearColor& Shade, const FLinearColor& Sheen)
	{
		P.Ball(C, R, Base, 0.5f);
		FClip Clip(P, [C, R](FV Q) { return SdCircle(Q, C, R); });
		P.MEllipse(C + FV(R * 0.34f, R * 0.46f), R * 0.72f, R * 0.46f, Alpha(Shade, 0.55f), R * 0.4f);
		P.MEllipse(C + FV(R * 0.5f, -R * 0.2f), R * 0.32f, R * 0.46f, Alpha(Sheen, 0.4f), R * 0.3f);
		P.Gloss(C + FV(-R * 0.32f, -R * 0.38f), R * 0.3f, R * 0.2f, 0.95f);
		P.Gloss(C + FV(-R * 0.08f, -R * 0.6f), R * 0.08f, R * 0.05f, 0.7f);
	}

	/** A spiral shell (whelk, conch, trumpet): stacked whorls from the tip at A to the wide body at B */
	void SpiralShell(FPainter& P, FV A, FV B, float Width, const FLinearColor& Color, const FLinearColor& Band, int32 Whorls)
	{
		const FV D = (B - A).GetSafeNormal();
		const FV N(-D.Y, D.X);
		const float Len = (B - A).Size();
		TArray<FV> Outline;
		for (int32 i = 0; i <= 10; ++i)
		{
			const float T = i / 10.0f;
			Outline.Add(A + D * (Len * T) + N * (Width * FMath::Pow(T, 0.8f)));
		}
		Outline.Add(B + D * (Width * 0.55f));
		for (int32 i = 10; i >= 0; --i)
		{
			const float T = i / 10.0f;
			Outline.Add(A + D * (Len * T) - N * (Width * FMath::Pow(T, 0.8f)));
		}
		const TArray<FV> Body = Smooth(Outline, true, 3);
		const auto Shape = [&Body](FV Q) { return SdPoly(Q, Body); };
		{
			FWith With(P);
			P.Look.Dome = 0.12f;
			P.Fill(Shape, Color);
		}
		FClip Clip(P, Shape);
		for (int32 i = 1; i <= Whorls; ++i)
		{
			const float T = FMath::Pow((float)i / (Whorls + 1), 0.85f);
			const FV At = A + D * (Len * T);
			const float W = Width * FMath::Pow(T, 0.8f) * 1.1f;
			P.MLine(Smooth({ At - N * W, At + D * (W * 0.35f), At + N * W }), 0.012f, Alpha(Band, 0.9f));
			P.MLine(Smooth({ At - N * W + D * 0.02f, At + D * (W * 0.35f + 0.02f), At + N * W + D * 0.02f }), 0.008f, Hex(0xFFFFFF, 0.35f));
		}
	}

	/** Gold disc with a raised rim */
	void Coin(FPainter& P, FV C, float R)
	{
		{
			FWith With(P);
			P.Look.Spec = 0.8f;
			P.Look.Dome = 0.05f;
			P.Circle(C, R, Hex(0xE0A838));
		}
		P.Circle(C, R * 0.78f, Gold);
		P.Mark([C, R](FV Q) { return FMath::Abs(SdCircle(Q, C, R * 0.78f)) - R * 0.03f; }, Hex(0x8A5A18, 0.8f));
	}

	/** Gold-orange halo behind a unique legendary */
	void UniqueHalo(FPainter& P)
	{
		P.Glow([](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.36f); }, Hex(0xFFB040), 0.14f, 0.6f);
	}

	void PaintItemKind(FPainter& P, int32 Kind)
	{
		switch (Kind)
		{
		// ---------------- shells ----------------
		case 1:
		{
			// Sharpened Cowrie: glossy spotted cowrie with a honed silver edge
			FPlace Place(P, FV(0.5f, 0.5f), 1.0f, -30.0f);
			const auto Shell = [](FV Q) { return SdEllipse(Q, FV(0.0f, 0.0f), 0.42f, 0.27f); };
			{
				FWith With(P);
				P.Look.Dome = 0.2f;
				P.Look.Spec = 0.9f;
				P.Fill(Shell, Hex(0xD8A060));
			}
			{
				FClip Clip(P, Shell);
				P.MEllipse(FV(0.0f, 0.13f), 0.44f, 0.13f, Hex(0xF4E2C0), 0.03f);
				for (const FVector S : { FVector(-0.2f, -0.08f, 0.04f), FVector(-0.05f, -0.14f, 0.035f), FVector(0.12f, -0.09f, 0.045f), FVector(0.26f, -0.04f, 0.03f), FVector(-0.28f, 0.0f, 0.03f), FVector(0.02f, -0.02f, 0.03f) })
				{
					P.MCircle(FV(S.X, S.Y), S.Z, Hex(0x6E3A1C, 0.85f));
				}
				// toothed slit
				P.MCapsule(FV(-0.3f, 0.15f), FV(0.3f, 0.15f), 0.022f, Hex(0x3A1E10));
				for (int32 i = 0; i < 9; ++i)
				{
					const float X = -0.26f + i * 0.065f;
					P.MCapsule(FV(X, 0.12f), FV(X, 0.18f), 0.008f, Hex(0xF8EEDC));
				}
			}
			// the sharpened edge
			P.Mark([](FV Q) { return SdArc(Q, FV(0.0f, 0.0f), 0.4f, 0.03f, 190.0f, 330.0f); }, Hex(0xE8F4FA));
			P.Star(FV(0.2f, -0.24f), 0.09f, 0.02f, 4, 0.0f, Hex(0xFFFFFF));
			break;
		}
		case 2:
		{
			// Spiral Whelk: a tall coiled whelk with speed lines
			SpiralShell(P, FV(0.74f, 0.16f), FV(0.38f, 0.66f), 0.22f, Hex(0xE8C49A), Hex(0x9A5A36), 5);
			P.Ellipse(FV(0.33f, 0.72f), 0.12f, 0.08f, Hex(0xF4A890));
			P.MEllipse(FV(0.33f, 0.73f), 0.07f, 0.04f, Hex(0x9A4A3A));
			for (int32 i = 0; i < 3; ++i)
			{
				P.Capsule(FV(0.08f + i * 0.04f, 0.3f + i * 0.12f), FV(0.2f + i * 0.04f, 0.3f + i * 0.12f), 0.018f, Hex(0x9BE0F0, 0.9f));
			}
			break;
		}
		case 3:
		{
			// Barnacle Plate: a rusted, riveted iron plate crusted with barnacles, a mussel and a tuft of weed
			const auto Plate = [](FV Q) { return SdBox(Q, FV(0.12f, 0.18f), FV(0.88f, 0.82f), 0.08f); };
			{
				FWith With(P);
				P.Look.Grain = 0.1f;
				P.Fill(Plate, Iron);
			}
			{
				FClip Clip(P, Plate);
				// rust blooms, and streaks running down from the rivets
				P.MEllipse(FV(0.3f, 0.7f), 0.17f, 0.1f, Hex(0x9A5A2A, 0.5f), 0.06f);
				P.MEllipse(FV(0.76f, 0.4f), 0.12f, 0.08f, Hex(0xA8642E, 0.45f), 0.05f);
				for (const FV R : { FV(0.22f, 0.27f), FV(0.78f, 0.27f) })
				{
					P.MCapsule(R, R + FV(0.01f, 0.2f), 0.016f, Hex(0x8A4A22, 0.5f));
				}
				// a bevelled top edge, a welded seam and a few scratches
				P.MCapsule(FV(0.16f, 0.205f), FV(0.84f, 0.205f), 0.008f, Hex(0xFFFFFF, 0.3f));
				P.MCapsule(FV(0.12f, 0.5f), FV(0.88f, 0.5f), 0.007f, Hex(0x22282E, 0.7f));
				P.MCapsule(FV(0.12f, 0.512f), FV(0.88f, 0.512f), 0.004f, Hex(0xFFFFFF, 0.25f));
				P.MCapsule(FV(0.2f, 0.36f), FV(0.3f, 0.32f), 0.004f, Hex(0xFFFFFF, 0.3f));
				P.MCapsule(FV(0.66f, 0.76f), FV(0.8f, 0.72f), 0.004f, Hex(0xFFFFFF, 0.3f));
			}
			for (const FV R : { FV(0.2f, 0.26f), FV(0.8f, 0.26f), FV(0.2f, 0.74f), FV(0.8f, 0.74f), FV(0.16f, 0.5f), FV(0.84f, 0.5f) })
			{
				Rivet(P, R, 0.028f);
			}
			// a tuft of weed clinging to the lower corner
			for (int32 i = 0; i < 3; ++i)
			{
				const FV B(0.24f + i * 0.035f, 0.8f);
				P.Ribbon(Smooth({ B, B + FV(-0.05f + i * 0.03f, -0.1f), B + FV(-0.03f + i * 0.05f, -0.2f - i * 0.03f) }), 0.022f, 0.006f, i == 1 ? Hex(0x5AA040) : Hex(0x3E8A3A));
			}
			// a blue mussel wedged against the plate
			{
				FPlace Place(P, FV(0.72f, 0.7f), 1.0f, -25.0f);
				const TArray<FV> M = Smooth({ FV(-0.14f, 0.0f), FV(-0.04f, -0.06f), FV(0.11f, -0.05f), FV(0.15f, 0.0f), FV(0.11f, 0.05f), FV(-0.04f, 0.05f) }, true, 5);
				const auto Sd = [&M](FV Q) { return SdPoly(Q, M); };
				{
					FWith With(P);
					P.Look.Spec = 0.7f;
					P.Fill(Sd, Hex(0x2C3458));
				}
				FClip Clip(P, Sd);
				for (int32 i = 1; i <= 3; ++i)
				{
					P.MArc(FV(-0.16f, 0.0f), 0.08f * i, 0.008f, -60.0f, 60.0f, Hex(0x6A78A8, 0.6f));
				}
				P.MCapsule(FV(-0.13f, 0.0f), FV(0.14f, 0.0f), 0.004f, Hex(0x10142A, 0.8f));
				P.Gloss(FV(0.0f, -0.025f), 0.07f, 0.012f, 0.6f);
			}
			// barnacles: ridged wall plates, a dark opening with beak plates, and one feeding with its cirri
			const auto Barnacle = [&P](FV C, float S, bool bCirri)
			{
				P.MEllipse(C + FV(0.02f, S * 0.5f), S * 1.05f, S * 0.3f, Hex(0x101418, 0.45f), S * 0.2f);
				const TArray<FV> Cone = { C + FV(-S, S * 0.45f), C + FV(-S * 0.55f, -S * 0.5f), C + FV(S * 0.55f, -S * 0.5f), C + FV(S, S * 0.45f) };
				const auto Sd = [&Cone](FV Q) { return SdPoly(Q, Cone) - 0.006f; };
				{
					FWith With(P);
					P.Look.Dome = 0.03f;
					P.Look.Grain = 0.08f;
					P.Fill(Sd, Hex(0xE6D8BA));
				}
				{
					FClip Clip(P, Sd);
					for (int32 i = -2; i <= 2; ++i)
					{
						P.MCapsule(C + FV(i * S * 0.24f, -S * 0.5f), C + FV(i * S * 0.46f, S * 0.5f), S * 0.035f, Hex(0x9A8A68, 0.75f));
					}
					for (int32 i = 1; i <= 2; ++i)
					{
						P.MCapsule(C + FV(-S, -S * 0.5f + i * S * 0.33f), C + FV(S, -S * 0.5f + i * S * 0.33f), S * 0.03f, Hex(0xFFF8E8, 0.5f));
					}
					P.MEllipse(C + FV(-S * 0.4f, 0.0f), S * 0.18f, S * 0.45f, Hex(0xFFFFFF, 0.3f), S * 0.15f);
					P.MEllipse(C + FV(S * 0.65f, S * 0.2f), S * 0.3f, S * 0.55f, Hex(0x5A4A30, 0.35f), S * 0.2f);
				}
				P.Ellipse(C + FV(0.0f, -S * 0.5f), S * 0.52f, S * 0.2f, Hex(0xD8C8A4));
				P.MEllipse(C + FV(0.0f, -S * 0.5f), S * 0.36f, S * 0.12f, Hex(0x2A2018));
				P.MPoly({ C + FV(-S * 0.3f, -S * 0.5f), C + FV(0.0f, -S * 0.6f), C + FV(0.0f, -S * 0.42f) }, Hex(0xB8A888));
				P.MPoly({ C + FV(S * 0.3f, -S * 0.5f), C + FV(0.0f, -S * 0.6f), C + FV(0.0f, -S * 0.42f) }, Hex(0xD0C0A0));
				if (bCirri)
				{
					const FV Base = C + FV(0.0f, -S * 0.56f);
					for (int32 i = 0; i < 5; ++i)
					{
						const float A = -140.0f + i * 25.0f;
						const float Bend = (i < 2) ? -35.0f : (i > 2 ? 35.0f : 0.0f);
						P.Line(Smooth({ Base, Base + Spin(FV(S * 0.55f, 0.0f), A), Base + Spin(FV(S * 0.85f, 0.0f), A + Bend) }), S * 0.045f, Hex(0xF2C8A8));
					}
				}
			};
			Barnacle(FV(0.6f, 0.37f), 0.075f, false);
			Barnacle(FV(0.42f, 0.44f), 0.125f, true);
			Barnacle(FV(0.6f, 0.6f), 0.1f, false);
			Barnacle(FV(0.38f, 0.68f), 0.08f, false);
			Barnacle(FV(0.76f, 0.5f), 0.06f, false);
			break;
		}
		case 4:
		{
			// Scallop Fan: a pink scallop sending out ripples
			for (int32 i = 0; i < 3; ++i)
			{
				P.Arc(FV(0.5f, 0.74f), 0.42f + i * 0.12f, 0.035f, -150.0f, -30.0f, Hex(0x9BE0F0, 0.95f - i * 0.25f));
			}
			const FV Apex(0.5f, 0.82f);
			TArray<FV> Fan = { Apex };
			for (int32 i = 0; i <= 30; ++i)
			{
				const float T = i / 30.0f;
				Fan.Add(Apex + Spin(FV(1.0f, 0.0f), -150.0f + T * 120.0f) * (0.36f + FMath::Abs(FMath::Sin(T * 6.0f * PI)) * 0.025f));
			}
			const auto Shell = [&Fan](FV Q) { return SdPoly(Q, Fan); };
			{
				FWith With(P);
				P.Look.Dome = 0.15f;
				P.Fill(Shell, Hex(0xF08CB0));
			}
			FClip Clip(P, Shell);
			for (int32 i = 0; i <= 6; ++i)
			{
				const FV Dir = Spin(FV(1.0f, 0.0f), -150.0f + i * 20.0f);
				P.MCapsule(Apex + Dir * 0.05f, Apex + Dir * 0.4f, 0.008f, Hex(0x9A3A64, 0.9f));
			}
			P.Mark([&Apex](FV Q) { return FMath::Abs((Q - Apex).Size() - 0.22f) - 0.02f; }, Hex(0xFFD0E0, 0.7f));
			break;
		}
		case 5:
		{
			// Hermit's Hammer: a wooden handle with a spiral shell for a head
			P.Capsule(FV(0.28f, 0.86f), FV(0.56f, 0.38f), 0.045f, Wood);
			P.MCapsule(FV(0.3f, 0.8f), FV(0.52f, 0.42f), 0.012f, Alpha(WoodLight, 0.8f));
			P.Box(FV(0.26f, 0.78f), FV(0.36f, 0.86f), 0.02f, WoodDark);
			{
				FPlace Place(P, FV(0.58f, 0.32f), 1.0f, 30.0f);
				P.Ellipse(FV(0.0f, 0.0f), 0.3f, 0.19f, Hex(0xC07840));
				P.MArc(FV(0.06f, 0.0f), 0.13f, 0.03f, 0.0f, 300.0f, Hex(0x6A3A1C));
				P.MArc(FV(0.08f, 0.0f), 0.06f, 0.025f, 0.0f, 300.0f, Hex(0x6A3A1C));
				P.Ellipse(FV(-0.27f, 0.02f), 0.06f, 0.14f, Hex(0xF4C8A0));
				P.Gloss(FV(-0.05f, -0.1f), 0.1f, 0.04f, 0.6f);
			}
			// two crab claws peeking out
			P.Ball(FV(0.36f, 0.24f), 0.05f, Hex(0xE8503A));
			P.Ball(FV(0.33f, 0.36f), 0.04f, Hex(0xE8503A));
			break;
		}
		case 6:
		{
			// Razor Clam: a long clam gaping open, its honed edge glinting and dripping blood
			{
				FPlace Place(P, FV(0.46f, 0.44f), 1.0f, -40.0f);
				// the far valve, with pearly nacre showing in the gape
				const auto Back = [](FV Q) { return SdBox(Q, FV(-0.42f, -0.02f), FV(0.42f, 0.11f), 0.06f); };
				P.Fill(Back, Hex(0x8A6A48));
				{
					FClip Clip(P, Back);
					P.MEllipse(FV(0.0f, 0.06f), 0.4f, 0.03f, Hex(0xD8CCEC, 0.95f), 0.015f);
					P.MEllipse(FV(-0.12f, 0.06f), 0.16f, 0.012f, Hex(0xFFE0F0, 0.8f), 0.01f);
					P.MEllipse(FV(0.14f, 0.065f), 0.12f, 0.01f, Hex(0xB8F0E8, 0.7f), 0.01f);
					for (int32 i = 0; i < 6; ++i)
					{
						P.MCapsule(FV(-0.36f + i * 0.14f, 0.08f), FV(-0.33f + i * 0.14f, 0.11f), 0.006f, Hex(0x5A3E24, 0.6f));
					}
				}
				// the pink foot pushing out of one end, siphons out of the other
				P.Blob({ FV(0.36f, 0.02f), FV(0.5f, -0.01f), FV(0.6f, 0.05f), FV(0.57f, 0.12f), FV(0.46f, 0.13f), FV(0.36f, 0.1f) }, Hex(0xF09AA4));
				P.MEllipse(FV(0.5f, 0.04f), 0.06f, 0.015f, Hex(0xFFE0E4, 0.7f), 0.01f);
				P.Blob({ FV(-0.4f, -0.03f), FV(-0.5f, -0.02f), FV(-0.53f, 0.03f), FV(-0.5f, 0.08f), FV(-0.4f, 0.08f) }, Hex(0xE8C0A0));
				P.MCircle(FV(-0.5f, 0.005f), 0.012f, Hex(0x6A3A2A));
				P.MCircle(FV(-0.49f, 0.055f), 0.011f, Hex(0x6A3A2A));
				// the near valve: banded growth lines, olive skin along the hinge, a honed silver lip
				const auto Shell = [](FV Q) { return SdBox(Q, FV(-0.44f, -0.11f), FV(0.44f, 0.055f), 0.07f); };
				{
					FWith With(P);
					P.Look.Dome = 0.08f;
					P.Look.Grain = 0.05f;
					P.Fill(Shell, Hex(0xC8A070));
				}
				{
					FClip Clip(P, Shell);
					for (int32 i = 0; i < 7; ++i)
					{
						P.MArc(FV(-0.62f, -0.03f), 0.24f + i * 0.13f, 0.035f, -40.0f, 40.0f, i % 2 ? Hex(0x9A7448, 0.55f) : Hex(0xF0DCB8, 0.5f));
					}
					P.Mark([](FV Q) { return Q.Y + 0.08f; }, Hex(0x6A6A30, 0.8f), 0.01f);
					P.MCapsule(FV(-0.4f, -0.085f), FV(0.4f, -0.085f), 0.005f, Hex(0x3A3018));
					P.Gloss(FV(-0.08f, -0.04f), 0.26f, 0.016f, 0.5f);
					P.Mark([](FV Q) { return 0.038f - Q.Y; }, Hex(0xE4EEF6, 0.95f));
					P.MCapsule(FV(-0.4f, 0.04f), FV(0.4f, 0.04f), 0.004f, Hex(0x8A98A8, 0.8f));
				}
				Glint(P, FV(0.18f, 0.05f), 0.2f);
				// blood smeared on the honed lip
				P.Blob({ FV(0.24f, 0.05f), FV(0.34f, 0.045f), FV(0.38f, 0.07f), FV(0.3f, 0.085f) }, Blood);
			}
			Drop(P, FV(0.7f, 0.62f), 0.26f, Blood);
			Drop(P, FV(0.6f, 0.82f), 0.18f, Blood);
			Drop(P, FV(0.8f, 0.82f), 0.12f, Blood);
			break;
		}
		case 7:
		{
			// Ironclad Nautilus: a chambered nautilus shell plated in riveted iron
			const FV C(0.52f, 0.5f);
			{
				FWith With(P);
				P.Look.Spec = 0.6f;
				P.Look.Dome = 0.12f;
				P.Circle(C, 0.36f, SteelDark);
			}
			FClip Clip(P, [C](FV Q) { return SdCircle(Q, C, 0.36f); });
			TArray<FV> Spiral;
			for (int32 i = 0; i <= 60; ++i)
			{
				const float A = i * 0.18f;
				Spiral.Add(C + Spin(FV(0.03f * FMath::Exp(A * 0.2f), 0.0f), FMath::RadiansToDegrees(A)));
			}
			P.MLine(Spiral, 0.016f, IronDark);
			for (int32 i = 0; i < 9; ++i)
			{
				const FV Dir = Spin(FV(1.0f, 0.0f), i * 40.0f + 10.0f);
				P.MCapsule(C + Dir * (0.08f + i * 0.01f), C + Dir * 0.36f, 0.009f, Alpha(IronDark, 0.8f));
				Rivet(P, C + Dir * 0.3f, 0.018f);
			}
			P.MEllipse(C + FV(-0.12f, -0.16f), 0.14f, 0.07f, Hex(0xFFFFFF, 0.3f), 0.04f);
			break;
		}
		case 8:
		{
			// Titan Clam: a huge giant clam with a wavy blue mantle
			const auto Shell = [](FV Q) { return FMath::Max(SdEllipse(Q, FV(0.5f, 0.4f), 0.46f, 0.42f), 0.4f - Q.Y); };
			{
				FWith With(P);
				P.Look.Dome = 0.1f;
				P.Look.Grain = 0.06f;
				P.Fill(Shell, Hex(0xD8D0B8));
			}
			{
				FClip Clip(P, Shell);
				for (int32 i = 0; i < 6; ++i)
				{
					const float X = 0.12f + i * 0.152f;
					P.MCapsule(FV(X, 0.38f), FV(0.5f + (X - 0.5f) * 0.5f, 0.9f), 0.03f, Hex(0x8E8670, 0.7f));
				}
			}
			// zigzag lips with the mantle between
			TArray<FV> Lip;
			for (int32 i = 0; i <= 12; ++i)
			{
				Lip.Add(FV(0.08f + i * 0.07f, 0.4f + ((i % 2) ? -0.07f : 0.05f) + FMath::Abs(i - 6) * 0.012f));
			}
			P.Line(Lip, 0.045f, Hex(0x3A6AD8));
			P.MLine(Lip, 0.018f, Hex(0x8CE0FF));
			P.Line(Lip, 0.012f, Hex(0x1A2E6A));
			break;
		}
		case 9:
		{
			// Triton's Trumpet: a long trumpet shell with lightning leaping from its mouth
			SpiralShell(P, FV(0.86f, 0.82f), FV(0.38f, 0.38f), 0.2f, Hex(0xF0D8B0), Hex(0xA05A30), 6);
			P.Ellipse(FV(0.33f, 0.33f), 0.1f, 0.13f, Hex(0xFFB070));
			P.MEllipse(FV(0.33f, 0.33f), 0.05f, 0.08f, Hex(0x8A3A1A));
			Bolt(P, FV(0.16f, 0.2f), 0.3f, Volt, -30.0f);
			Bolt(P, FV(0.2f, 0.5f), 0.2f, Volt, 20.0f);
			break;
		}
		case 10:
		{
			// Murex of Endings: a dark spiny murex with a long canal
			const FV A(0.5f, 0.12f), B(0.5f, 0.92f);
			P.Capsule(FV(0.5f, 0.6f), B, 0.03f, Hex(0x5A2A50));
			for (int32 i = 0; i < 4; ++i)
			{
				const float Y = 0.22f + i * 0.11f;
				const float W = 0.1f + FMath::Sin((i + 0.5f) / 4.0f * PI) * 0.16f;
				P.Poly({ FV(0.5f - W * 0.6f, Y), FV(0.5f - W - 0.12f, Y - 0.06f), FV(0.5f - W * 0.6f, Y + 0.05f) }, Hex(0x3A1A3A));
				P.Poly({ FV(0.5f + W * 0.6f, Y), FV(0.5f + W + 0.12f, Y - 0.06f), FV(0.5f + W * 0.6f, Y + 0.05f) }, Hex(0x3A1A3A));
			}
			SpiralShell(P, A, FV(0.5f, 0.56f), 0.2f, Hex(0x7A3A70), Hex(0x2A0A28), 4);
			P.Ellipse(FV(0.5f, 0.6f), 0.08f, 0.06f, Hex(0xE870A0));
			P.Glow([](FV Q) { return SdCircle(Q, FV(0.5f, 0.6f), 0.05f); }, Hex(0xFF4080), 0.08f, 0.5f);
			break;
		}
		case 11:
		{
			// Heart of the Leviathan: an enormous deep-red heart, still glowing
			P.Glow([](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.3f); }, Hex(0xFF2050), 0.16f, 0.6f);
			Heart(P, FV(0.5f, 0.52f), 1.0f, Hex(0xB01838));
			P.MLine(Smooth({ FV(0.34f, 0.36f), FV(0.42f, 0.5f), FV(0.5f, 0.56f), FV(0.56f, 0.72f) }), 0.012f, Hex(0x5A0A20, 0.9f));
			P.MLine(Smooth({ FV(0.66f, 0.34f), FV(0.6f, 0.48f), FV(0.52f, 0.58f) }), 0.01f, Hex(0x5A0A20, 0.9f));
			// barnacles on the hide
			P.Ball(FV(0.64f, 0.58f), 0.035f, Hex(0xE8DCC0), 0.3f);
			P.Ball(FV(0.7f, 0.5f), 0.025f, Hex(0xE8DCC0), 0.3f);
			P.Capsule(FV(0.5f, 0.24f), FV(0.5f, 0.12f), 0.05f, Hex(0x7A1028));
			break;
		}
		case 12:
		{
			// Crown of Brine: a gold crown set with pearls and dripping sea water
			const TArray<FV> Band = { FV(0.14f, 0.72f), FV(0.14f, 0.34f), FV(0.3f, 0.52f), FV(0.4f, 0.24f), FV(0.5f, 0.46f), FV(0.6f, 0.24f), FV(0.7f, 0.52f), FV(0.86f, 0.34f), FV(0.86f, 0.72f) };
			{
				FWith With(P);
				P.Look.Spec = 0.8f;
				P.Poly(Band, Gold);
			}
			P.Box(FV(0.12f, 0.64f), FV(0.88f, 0.78f), 0.03f, Hex(0xC8902E));
			for (const FV Tip : { FV(0.14f, 0.32f), FV(0.4f, 0.22f), FV(0.6f, 0.22f), FV(0.86f, 0.32f) })
			{
				TintedPearl(P, Tip, 0.05f, Hex(0xF0EAF6), Hex(0x8E7CB8), Hex(0x9AF0E0));
			}
			P.Circle(FV(0.5f, 0.71f), 0.05f, Hex(0x18A8A0));
			P.Circle(FV(0.3f, 0.71f), 0.035f, Hex(0x2E86CC));
			P.Circle(FV(0.7f, 0.71f), 0.035f, Hex(0x2E86CC));
			Drop(P, FV(0.24f, 0.88f), 0.14f, Hex(0x5CC8F0));
			Drop(P, FV(0.78f, 0.9f), 0.11f, Hex(0x5CC8F0));
			break;
		}

		// ---------------- pearls ----------------
		case 13:
		{
			// Lantern Pearl: an anglerfish lure holding a glowing pearl
			P.Ribbon(Smooth({ FV(0.2f, 0.9f), FV(0.24f, 0.5f), FV(0.42f, 0.24f), FV(0.62f, 0.26f) }), 0.035f, 0.02f, Hex(0x4A5A7A));
			P.Glow([](FV Q) { return SdCircle(Q, FV(0.64f, 0.44f), 0.14f); }, Hex(0xFFD040), 0.22f, 0.9f);
			P.Capsule(FV(0.62f, 0.26f), FV(0.64f, 0.32f), 0.015f, Hex(0x4A5A7A));
			TintedPearl(P, FV(0.64f, 0.46f), 0.16f, Hex(0xFFF0A0), Hex(0xE0A020), Hex(0xFFFFE0));
			break;
		}
		case 14:
		{
			// Black Pearl: a dark iridescent pearl beading venom
			TintedPearl(P, FV(0.46f, 0.54f), 0.32f, Hex(0x2A2E3A), Hex(0x6A3A8A), Hex(0x3AC8A0));
			Drop(P, FV(0.78f, 0.24f), 0.26f, Poison);
			break;
		}
		case 15:
		{
			// Cold Seep Pearl: a pale icy pearl with frost crystals
			TintedPearl(P, FV(0.5f, 0.52f), 0.3f, Hex(0xD4F2FF), Hex(0x5AA8D8), Hex(0xFFFFFF));
			Snowflake(P, FV(0.76f, 0.24f), 0.3f, Frost);
			Snowflake(P, FV(0.22f, 0.8f), 0.18f, Frost);
			break;
		}
		case 16:
		{
			// Seafoam Pearl: a white pearl riding the foam on a breaking wave crest, with spray and bubbles
			const TArray<FV> Wave = Smooth({ FV(0.06f, 0.86f), FV(0.12f, 0.7f), FV(0.28f, 0.62f), FV(0.5f, 0.6f), FV(0.72f, 0.62f), FV(0.88f, 0.69f), FV(0.95f, 0.82f), FV(0.9f, 0.92f), FV(0.5f, 0.95f), FV(0.1f, 0.93f) }, true, 5);
			const auto Sea = [&Wave](FV Q) { return SdPoly(Q, Wave); };
			{
				FWith With(P);
				P.Look.Dome = 0.08f;
				P.Fill(Sea, Hex(0x2E8EC0));
			}
			{
				FClip Clip(P, Sea);
				P.MEllipse(FV(0.5f, 0.96f), 0.5f, 0.12f, Hex(0x1A5A8A, 0.6f), 0.08f);
				for (int32 i = 0; i < 3; ++i)
				{
					P.MArc(FV(0.5f + i * 0.05f, 1.25f), 0.4f + i * 0.07f, 0.014f, -128.0f, -62.0f, Hex(0x8FD8F0, 0.6f - i * 0.12f));
				}
				for (const FVector D : { FVector(0.22f, 0.76f, 0.014f), FVector(0.3f, 0.72f, 0.01f), FVector(0.68f, 0.74f, 0.012f), FVector(0.8f, 0.78f, 0.01f), FVector(0.46f, 0.72f, 0.009f) })
				{
					P.MCircle(FV(D.X, D.Y), D.Z, Hex(0xFFFFFF, 0.75f));
				}
			}
			// a cap of foam along the crest
			for (const FVector F : { FVector(0.92f, 0.76f, 0.045f), FVector(0.14f, 0.7f, 0.055f), FVector(0.85f, 0.67f, 0.06f), FVector(0.25f, 0.63f, 0.075f), FVector(0.74f, 0.61f, 0.075f), FVector(0.37f, 0.6f, 0.085f), FVector(0.62f, 0.59f, 0.085f), FVector(0.5f, 0.61f, 0.09f) })
			{
				P.Ball(FV(F.X, F.Y), F.Z, Hex(0xEEFBFF), 0.3f);
				P.MArc(FV(F.X, F.Y), F.Z * 0.7f, F.Z * 0.2f, 20.0f, 150.0f, Hex(0x9ACCE0, 0.6f));
			}
			TintedPearl(P, FV(0.5f, 0.38f), 0.2f, Hex(0xF8FCFF), Hex(0x9AC8E0), Hex(0xC8FFF4));
			// flung spray
			for (const FVector S : { FVector(0.12f, 0.54f, 0.024f), FVector(0.08f, 0.44f, 0.016f), FVector(0.88f, 0.52f, 0.022f), FVector(0.93f, 0.43f, 0.015f), FVector(0.2f, 0.48f, 0.014f) })
			{
				P.Ball(FV(S.X, S.Y), S.Z, Hex(0xD8F4FF), 0.8f);
			}
			Bubble(P, FV(0.2f, 0.24f), 0.06f);
			Bubble(P, FV(0.8f, 0.18f), 0.05f);
			Bubble(P, FV(0.78f, 0.38f), 0.032f);
			Bubble(P, FV(0.3f, 0.1f), 0.028f);
			break;
		}
		case 17:
		{
			// Baroque Pearl: a lumpy misshapen pearl with a gleam on its sweet spot
			const TArray<FV> Lumps = { FV(0.3f, 0.26f), FV(0.52f, 0.18f), FV(0.72f, 0.3f), FV(0.76f, 0.54f), FV(0.64f, 0.8f), FV(0.42f, 0.84f), FV(0.24f, 0.68f), FV(0.28f, 0.48f) };
			const TArray<FV> Outline = Smooth(Lumps, true, 5);
			const auto Shape = [&Outline](FV Q) { return SdPoly(Q, Outline); };
			{
				FWith With(P);
				P.Look.Dome = 0.25f;
				P.Look.Spec = 0.6f;
				P.Fill(Shape, Hex(0xF0E4CC));
			}
			{
				FClip Clip(P, Shape);
				P.MEllipse(FV(0.6f, 0.68f), 0.24f, 0.18f, Hex(0xB89A70, 0.5f), 0.08f);
				P.MEllipse(FV(0.36f, 0.6f), 0.1f, 0.16f, Hex(0xFFB8D8, 0.35f), 0.06f);
				P.Gloss(FV(0.4f, 0.32f), 0.08f, 0.05f, 0.9f);
			}
			P.Star(FV(0.7f, 0.42f), 0.14f, 0.03f, 4, 0.0f, Hex(0xFFFFFF));
			break;
		}
		case 18:
		{
			// Cone Snail Pearl: a tented cone snail firing a venom dart, a pearl in its mouth
			{
				FPlace Place(P, FV(0.42f, 0.56f), 1.0f, -35.0f);
				const TArray<FV> Cone = { FV(-0.38f, -0.18f), FV(-0.3f, -0.24f), FV(0.38f, -0.03f), FV(0.38f, 0.03f), FV(-0.3f, 0.24f), FV(-0.38f, 0.18f) };
				const auto Shape = [&Cone](FV Q) { return SdPoly(Q, Cone) - 0.02f; };
				{
					FWith With(P);
					P.Look.Dome = 0.1f;
					P.Fill(Shape, Hex(0xF0D8A0));
				}
				FClip Clip(P, Shape);
				for (int32 i = 0; i < 14; ++i)
				{
					const FV C(-0.3f + (i % 5) * 0.14f, -0.12f + (i / 5) * 0.11f);
					P.MPoly({ C + FV(-0.04f, 0.03f), C + FV(0.0f, -0.04f), C + FV(0.04f, 0.03f) }, Hex(0x7A4220, 0.9f));
				}
			}
			TintedPearl(P, FV(0.68f, 0.34f), 0.08f, Hex(0xF0EAF6), Hex(0x8E7CB8), Hex(0x9AF0E0));
			P.Capsule(FV(0.74f, 0.28f), FV(0.9f, 0.12f), 0.014f, Hex(0xE8E4C8));
			Drop(P, FV(0.9f, 0.24f), 0.14f, Poison);
			break;
		}
		case 19:
		{
			// Torpedo Ray Pearl: an electric ray carrying a pearl, crackling
			const auto Body = [](FV Q) { return SdEllipse(Q, FV(0.48f, 0.44f), 0.34f, 0.28f); };
			P.Ribbon(Smooth({ FV(0.56f, 0.66f), FV(0.66f, 0.8f), FV(0.82f, 0.9f) }), 0.035f, 0.01f, Hex(0x8A6A4A));
			{
				FWith With(P);
				P.Look.Dome = 0.15f;
				P.Fill(Body, Hex(0xA8845A));
			}
			{
				FClip Clip(P, Body);
				P.MEllipse(FV(0.48f, 0.5f), 0.2f, 0.14f, Hex(0xC8A878, 0.7f), 0.05f);
				for (const FV S : { FV(0.28f, 0.36f), FV(0.68f, 0.36f), FV(0.36f, 0.58f), FV(0.62f, 0.6f) })
				{
					P.MCircle(S, 0.025f, Hex(0x4A3020, 0.8f));
				}
			}
			P.MCircle(FV(0.4f, 0.32f), 0.02f, Ink);
			P.MCircle(FV(0.56f, 0.32f), 0.02f, Ink);
			TintedPearl(P, FV(0.48f, 0.46f), 0.08f, Hex(0xFFF2B0), Hex(0xD8A030), Hex(0xFFFFFF));
			Bolt(P, FV(0.14f, 0.2f), 0.24f, Volt, -20.0f);
			Bolt(P, FV(0.86f, 0.22f), 0.22f, Volt, 25.0f);
			break;
		}
		case 20:
		{
			// Moon Pearl: a pale pearl lit like the moon and pocked with craters, in a halo, among stars and a crescent
			const FV C(0.5f, 0.55f);
			const float R = 0.3f;
			P.Glow([C, R](FV Q) { return SdCircle(Q, C, R); }, Hex(0x9AB8FF), 0.12f, 0.6f);
			P.Mark([C, R](FV Q) { return FMath::Abs(SdCircle(Q, C, R + 0.055f)) - 0.006f; }, Hex(0xC8D8FF, 0.55f));
			TintedPearl(P, C, R, Hex(0xE8F0FF), Hex(0x7A8AC8), Hex(0xFFFFFF));
			{
				FClip Clip(P, [C, R](FV Q) { return SdCircle(Q, C, R); });
				for (const FVector K : { FVector(-0.1f, -0.02f, 0.06f), FVector(0.02f, -0.15f, 0.035f), FVector(-0.19f, 0.09f, 0.032f), FVector(0.05f, 0.05f, 0.03f), FVector(-0.06f, 0.17f, 0.025f), FVector(0.15f, -0.06f, 0.022f) })
				{
					const FV K0 = C + FV(K.X, K.Y);
					P.MCircle(K0, K.Z, Hex(0xA8B4D8, 0.55f));
					P.MArc(K0, K.Z * 0.8f, K.Z * 0.35f, 200.0f, 340.0f, Hex(0x5A68A0, 0.5f));
					P.MArc(K0, K.Z * 0.95f, K.Z * 0.25f, 20.0f, 160.0f, Hex(0xFFFFFF, 0.7f));
				}
				P.Mark([C](FV Q) { return -SdCircle(Q, C + FV(-0.14f, -0.08f), 0.3f); }, Hex(0x1A2A5A, 0.55f), 0.03f);
			}
			// a small golden crescent moon
			const auto Crescent = [](FV Q) { return FMath::Max(SdCircle(Q, FV(0.17f, 0.18f), 0.1f), -SdCircle(Q, FV(0.22f, 0.13f), 0.085f)); };
			P.Glow(Crescent, Hex(0xFFD860), 0.06f, 0.5f);
			{
				FWith With(P);
				P.Look.Spec = 0.6f;
				P.Fill(Crescent, Hex(0xFFE890));
			}
			Glint(P, FV(0.84f, 0.16f), 0.22f, Hex(0xFFF4C0));
			Glint(P, FV(0.9f, 0.46f), 0.13f, Hex(0xFFF4C0));
			Glint(P, FV(0.12f, 0.52f), 0.1f, Hex(0xFFF4C0));
			for (const FVector S : { FVector(0.62f, 0.1f, 0.012f), FVector(0.4f, 0.08f, 0.009f), FVector(0.88f, 0.84f, 0.011f), FVector(0.1f, 0.84f, 0.009f) })
			{
				P.MCircle(FV(S.X, S.Y), S.Z, Hex(0xFFF8D8, 0.9f));
			}
			break;
		}
		case 21:
		{
			// Siren's Tear: a tear-shaped aqua pearl and a drifting song
			{
				FPlace Place(P, FV(0.42f, 0.56f), 0.8f);
				const TArray<FV> Tip = { FV(0.0f, -0.5f), FV(0.27f, 0.02f), FV(-0.27f, 0.02f) };
				const auto Shape = [&Tip](FV Q) { return FMath::Min(SdCircle(Q, FV(0.0f, 0.14f), 0.3f), SdPoly(Q, Tip)); };
				{
					FWith With(P);
					P.Look.Dome = 10.0f;
					P.Look.Spec = 0.6f;
					P.Fill(Shape, Hex(0x7AE0E8));
				}
				FClip Clip(P, Shape);
				P.MEllipse(FV(0.1f, 0.24f), 0.22f, 0.12f, Hex(0x2A7AA8, 0.5f), 0.06f);
				P.MEllipse(FV(-0.12f, 0.2f), 0.08f, 0.14f, Hex(0xFFB8E8, 0.4f), 0.05f);
				P.Gloss(FV(-0.1f, 0.04f), 0.06f, 0.1f, 0.9f);
			}
			MusicNotes(P, FV(0.76f, 0.3f), 0.32f, Hex(0xE070C0));
			break;
		}
		case 22:
		{
			// Red Tide Pearl: a rust-red pearl in a bloom of red water
			P.Glow([](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.3f); }, Hex(0xC02020), 0.14f, 0.5f);
			TintedPearl(P, FV(0.48f, 0.46f), 0.28f, Hex(0xD84838), Hex(0x6A1010), Hex(0xFFB090));
			P.MLine(Smooth({ FV(0.1f, 0.84f), FV(0.3f, 0.78f), FV(0.5f, 0.86f), FV(0.7f, 0.78f), FV(0.9f, 0.84f) }), 0.03f, Hex(0xB02A2A));
			Drop(P, FV(0.8f, 0.24f), 0.2f, Blood);
			Drop(P, FV(0.18f, 0.66f), 0.16f, Poison);
			break;
		}
		case 23:
		{
			// Pearl of the Abyss: a midnight pearl with an eye of light deep inside
			P.Glow([](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.32f); }, Hex(0x7A40FF), 0.16f, 0.7f);
			TintedPearl(P, FV(0.5f, 0.5f), 0.34f, Hex(0x1A2050), Hex(0x05081A), Hex(0x7A60E0));
			P.Glow([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.52f), 0.12f, 0.05f); }, Hex(0xB080FF), 0.08f, 0.8f);
			P.MEllipse(FV(0.5f, 0.52f), 0.13f, 0.06f, Hex(0xE8D8FF));
			P.MEllipse(FV(0.5f, 0.52f), 0.025f, 0.055f, Hex(0x05081A));
			break;
		}
		case 24:
		{
			// Kraken's Eye: a great slit-pupilled eye ringed by tentacles in ink
			P.Mark([](FV Q) { return SdCircle(Q, FV(0.5f, 0.52f), 0.44f); }, Hex(0x101828, 0.75f), 0.08f);
			for (int32 i = 0; i < 4; ++i)
			{
				const float A = 45.0f + i * 90.0f;
				const FV D = Spin(FV(1.0f, 0.0f), A), N = Spin(FV(1.0f, 0.0f), A + 90.0f);
				P.Ribbon(Smooth({ FV(0.5f, 0.52f) + D * 0.2f, FV(0.5f, 0.52f) + D * 0.36f + N * 0.04f, FV(0.5f, 0.52f) + D * 0.44f - N * 0.06f, FV(0.5f, 0.52f) + D * 0.38f - N * 0.12f }), 0.06f, 0.015f, Hex(0xB04A6A));
			}
			P.Ellipse(FV(0.5f, 0.52f), 0.26f, 0.2f, Hex(0x7A2A4A));
			P.Ball(FV(0.5f, 0.52f), 0.17f, Hex(0xF0C020), 0.4f);
			P.MEllipse(FV(0.5f, 0.52f), 0.035f, 0.13f, Ink);
			P.Gloss(FV(0.44f, 0.45f), 0.04f, 0.03f, 0.9f);
			break;
		}

		// ---------------- charms ----------------
		case 25:
		{
			// Driftwood Totem: a weathered post carved with three faces, bound with rope, a winged bird on top, set in sand
			const FLinearColor Drift = Hex(0xB8A488);
			const FLinearColor Dark = Hex(0x3A2E20);
			const FLinearColor Paint = Hex(0xC8503A);
			// sand mound with a little shell
			{
				FWith With(P);
				P.Look.Grain = 0.12f;
				P.Blob({ FV(0.12f, 0.92f), FV(0.3f, 0.8f), FV(0.7f, 0.8f), FV(0.88f, 0.92f) }, Hex(0xE0C890));
			}
			P.MCircle(FV(0.24f, 0.88f), 0.008f, Hex(0xA88A58, 0.8f));
			P.MCircle(FV(0.7f, 0.9f), 0.007f, Hex(0xA88A58, 0.8f));
			P.Ellipse(FV(0.79f, 0.87f), 0.04f, 0.028f, Hex(0xF0A8A0));
			P.MArc(FV(0.79f, 0.9f), 0.025f, 0.006f, 200.0f, 340.0f, Hex(0xB86058, 0.8f));
			// feathered wings behind the top face
			const uint32 Feathers[] = { 0x78C8C8, 0x5AB0B8, 0x3E8E9E };
			for (const float S : { -1.0f, 1.0f })
			{
				for (int32 F = 2; F >= 0; --F)
				{
					const FV Root(0.5f + S * 0.14f, 0.24f + F * 0.04f);
					const FV Tip(0.5f + S * (0.45f - F * 0.06f), 0.11f + F * 0.075f);
					const FV Mid = (Root + Tip) * 0.5f;
					P.Blob({ Root + FV(0.0f, -0.03f), Mid + FV(0.0f, -0.035f), Tip, Mid + FV(0.0f, 0.03f), Root + FV(0.0f, 0.035f) }, Hex(Feathers[F]));
					P.MCapsule(Root, FMath::Lerp(Root, Tip, 0.85f), 0.005f, Hex(0xFFFFFF, 0.4f));
				}
			}
			// the post, in three carved blocks with wood grain
			for (int32 i = 0; i < 3; ++i)
			{
				const float Y = 0.2f + i * 0.205f;
				const auto Block = [Y](FV Q) { return SdBox(Q, FV(0.33f, Y), FV(0.67f, Y + 0.2f), 0.04f); };
				{
					FWith With(P);
					P.Look.Grain = 0.14f;
					P.Look.Dome = 0.05f;
					P.Fill(Block, i == 1 ? Hex(0xA89070) : Drift);
				}
				FClip Clip(P, Block);
				for (int32 G = 0; G < 4; ++G)
				{
					TArray<FV> Grain;
					for (int32 k = 0; k <= 6; ++k)
					{
						Grain.Add(FV(0.365f + G * 0.09f + FMath::Sin(k * 1.3f + G * 2.0f) * 0.008f, Y + k * 0.034f));
					}
					P.MLine(Grain, 0.004f, Hex(0x7A6448, 0.45f));
				}
				P.MCapsule(FV(0.35f, Y + 0.012f), FV(0.65f, Y + 0.012f), 0.008f, Hex(0xFFF0D8, 0.35f));
				P.MEllipse(FV(0.6f, Y + 0.17f), 0.1f, 0.05f, Hex(0x5A4A30, 0.3f), 0.04f);
			}
			// rope bindings between the blocks
			Rope(P, FV(0.31f, 0.405f), FV(0.69f, 0.405f), 0.02f);
			Rope(P, FV(0.31f, 0.61f), FV(0.69f, 0.61f), 0.02f);
			// top: a bird with ringed eyes and a hooked beak
			for (const float X : { 0.43f, 0.57f })
			{
				P.MCircle(FV(X, 0.27f), 0.036f, Hex(0xF0E8D0));
				P.Mark([X](FV Q) { return FMath::Abs(SdCircle(Q, FV(X, 0.27f), 0.036f)) - 0.007f; }, Dark);
				P.MCircle(FV(X, 0.272f), 0.016f, Dark);
				P.MArc(FV(X, 0.27f), 0.058f, 0.012f, 205.0f, 335.0f, Dark);
			}
			P.Poly({ FV(0.45f, 0.31f), FV(0.55f, 0.31f), FV(0.53f, 0.36f), FV(0.5f, 0.42f), FV(0.47f, 0.36f) }, Hex(0xD08A3A));
			P.MCapsule(FV(0.47f, 0.33f), FV(0.5f, 0.33f), 0.006f, Hex(0xFFFFFF, 0.45f));
			// middle: a scowling face with bared teeth
			P.MCapsule(FV(0.39f, 0.45f), FV(0.47f, 0.48f), 0.012f, Dark);
			P.MCapsule(FV(0.61f, 0.45f), FV(0.53f, 0.48f), 0.012f, Dark);
			for (const float X : { 0.43f, 0.57f })
			{
				P.MEllipse(FV(X, 0.505f), 0.035f, 0.018f, Hex(0xF0E8D0));
				P.MCircle(FV(X, 0.505f), 0.013f, Dark);
			}
			P.Mark([](FV Q) { return SdBox(Q, FV(0.41f, 0.545f), FV(0.59f, 0.59f), 0.01f); }, Dark);
			for (int32 t = 0; t < 4; ++t)
			{
				const float X = 0.43f + t * 0.047f;
				P.MPoly({ FV(X - 0.016f, 0.547f), FV(X + 0.016f, 0.547f), FV(X, 0.572f) }, Hex(0xF4ECD8));
			}
			// bottom: a wide-eyed face painted in faded red
			for (const float X : { 0.43f, 0.57f })
			{
				P.Mark([X](FV Q) { return FMath::Abs(SdCircle(Q, FV(X, 0.69f), 0.032f)) - 0.009f; }, Alpha(Paint, 0.9f));
				P.MCircle(FV(X, 0.69f), 0.014f, Dark);
				P.MCircle(FV(X + 0.06f * (X < 0.5f ? -1.0f : 1.0f), 0.745f), 0.018f, Alpha(Paint, 0.6f));
			}
			P.MEllipse(FV(0.5f, 0.765f), 0.035f, 0.025f, Dark);
			P.MEllipse(FV(0.5f, 0.775f), 0.018f, 0.01f, Alpha(Paint, 0.9f));
			break;
		}
		case 26:
		{
			// Sand Dollar: a pale disc with its five-petal flower
			const FV C(0.5f, 0.5f);
			{
				FWith With(P);
				P.Look.Grain = 0.1f;
				P.Look.GrainFreq = 40.0f;
				P.Look.Dome = 0.1f;
				P.Circle(C, 0.4f, Hex(0xEDE0C4));
			}
			for (int32 i = 0; i < 5; ++i)
			{
				const FV D = Spin(FV(0.0f, -1.0f), i * 72.0f);
				const FV N(-D.Y, D.X);
				P.Mark([C, D, N](FV Q) { return FMath::Abs(FMath::Max(SdCircle(Q, C + D * 0.15f + N * 0.08f, 0.13f), SdCircle(Q, C + D * 0.15f - N * 0.08f, 0.13f))) - 0.01f; }, Hex(0x9A8460, 0.9f));
			}
			P.MCircle(C, 0.025f, Hex(0x9A8460));
			Coin(P, FV(0.82f, 0.8f), 0.1f);
			break;
		}
		case 27:
		{
			// Lucky Starfish: a plump orange starfish dotted with bumps
			const TArray<FV> Pts = Smooth(StarPoints(FV(0.5f, 0.52f), 0.44f, 0.2f, 5, -90.0f), true, 4);
			const auto Shape = [&Pts](FV Q) { return SdPoly(Q, Pts) - 0.02f; };
			{
				FWith With(P);
				P.Look.Dome = 0.12f;
				P.Fill(Shape, Hex(0xF07A30));
			}
			FClip Clip(P, Shape);
			for (int32 i = 0; i < 5; ++i)
			{
				const FV D = Spin(FV(0.0f, -1.0f), i * 72.0f);
				for (int32 j = 1; j <= 3; ++j)
				{
					P.MCircle(FV(0.5f, 0.52f) + D * (j * 0.1f), 0.022f - j * 0.003f, Hex(0xFFD8A0));
				}
			}
			P.MCircle(FV(0.5f, 0.52f), 0.03f, Hex(0xFFD8A0));
			break;
		}
		case 28:
		{
			// Kelp Braid: two kelp strands plaited over and under into a loop, with blades, gas bladders and a cowrie bead
			const FV O(0.5f, 0.54f);
			const float Start = -90.0f, Turn = 285.0f;
			const auto At = [O, Start, Turn](float T, float Side)
			{
				return O + Spin(FV(1.0f, 0.0f), Start + T * Turn) * (0.3f + Side * FMath::Sin(T * 8.0f * PI) * 0.045f);
			};
			const auto Strand = [&P, &At](float Side, float T0, float T1, const FLinearColor& Color)
			{
				TArray<FV> Path;
				const int32 N = FMath::Max(6, FMath::RoundToInt((T1 - T0) * 96.0f));
				for (int32 i = 0; i <= N; ++i)
				{
					Path.Add(At(FMath::Lerp(T0, T1, (float)i / N), Side));
				}
				P.Ribbon(Path, 0.037f - 0.01f * T0, 0.037f - 0.01f * T1, Color);
				P.Mark([&Path](FV Q) { return SdPolyline(Q, Path, 0.005f); }, Alpha(Tone(Color, 1.6f), 0.6f));
			};
			const FLinearColor KelpA = Hex(0x3A8A3A), KelpB = Hex(0x74B84A);
			// blades sprouting from the tail end
			const FV End = At(1.0f, 0.0f);
			for (const FVector L : { FVector(225.0f, 0.2f, 0.045f), FVector(255.0f, 0.24f, 0.05f), FVector(285.0f, 0.17f, 0.04f) })
			{
				const FV D = Spin(FV(1.0f, 0.0f), L.X), N(-D.Y, D.X);
				const FV Tip = End + D * L.Y;
				P.Blob({ End, End + D * (L.Y * 0.45f) + N * L.Z, Tip, End + D * (L.Y * 0.45f) - N * L.Z * 0.8f }, Hex(0x5AA040));
				P.MCapsule(End + D * 0.02f, End + D * (L.Y * 0.85f), 0.005f, Hex(0xB8E080, 0.7f));
			}
			// over and under: the light strand over the dark at every other crossing
			Strand(1.0f, 0.0f, 1.0f, KelpA);
			Strand(-1.0f, 0.0f, 1.0f, KelpB);
			for (int32 k = 0; k <= 8; k += 2)
			{
				const FV X = At(k / 8.0f, 0.0f);
				FClip Clip(P, [X](FV Q) { return SdCircle(Q, X, 0.075f); });
				Strand(1.0f, FMath::Max(0.0f, k / 8.0f - 0.1f), FMath::Min(1.0f, k / 8.0f + 0.1f), KelpA);
			}
			// gas bladders at both ends
			P.Ball(At(0.0f, 0.0f) + FV(0.0f, -0.04f), 0.05f, Hex(0xC8B040), 0.6f);
			P.Ball(End + FV(0.02f, 0.03f), 0.042f, Hex(0xB8A438), 0.6f);
			// a cowrie bead threaded on the braid
			{
				const float T = 0.36f;
				FPlace Place(P, At(T, 0.0f), 1.0f, Start + T * Turn + 90.0f);
				const auto Cowrie = [](FV Q) { return SdEllipse(Q, FV(0.0f, 0.0f), 0.08f, 0.058f); };
				{
					FWith With(P);
					P.Look.Dome = 10.0f;
					P.Look.Spec = 0.7f;
					P.Fill(Cowrie, Hex(0xF2E2C8));
				}
				FClip Clip(P, Cowrie);
				P.MCircle(FV(-0.03f, -0.025f), 0.009f, Hex(0xA8703A, 0.8f));
				P.MCircle(FV(0.02f, -0.03f), 0.007f, Hex(0xA8703A, 0.8f));
				P.MCircle(FV(0.04f, 0.0f), 0.008f, Hex(0xA8703A, 0.8f));
				P.MCapsule(FV(-0.055f, 0.02f), FV(0.055f, 0.02f), 0.007f, Hex(0x4A3020));
				for (int32 i = 0; i < 5; ++i)
				{
					P.MCapsule(FV(-0.04f + i * 0.02f, 0.013f), FV(-0.04f + i * 0.02f, 0.027f), 0.0025f, Hex(0xFFFFFF, 0.7f));
				}
			}
			break;
		}
		case 29:
		{
			// Sunken Coin Purse: a barnacled leather pouch spilling gold
			const TArray<FV> Bag = Smooth({ FV(0.36f, 0.3f), FV(0.64f, 0.3f), FV(0.78f, 0.56f), FV(0.7f, 0.8f), FV(0.3f, 0.8f), FV(0.22f, 0.56f) }, true, 5);
			const auto Shape = [&Bag](FV Q) { return SdPoly(Q, Bag); };
			{
				FWith With(P);
				P.Look.Grain = 0.1f;
				P.Fill(Shape, Hex(0x8A5A34));
			}
			P.Ellipse(FV(0.5f, 0.24f), 0.16f, 0.07f, Hex(0x6A4224));
			P.Capsule(FV(0.36f, 0.32f), FV(0.64f, 0.32f), 0.02f, RopeColor);
			P.Ball(FV(0.38f, 0.64f), 0.03f, Hex(0xE8DCC0), 0.3f);
			Coin(P, FV(0.78f, 0.82f), 0.1f);
			Coin(P, FV(0.62f, 0.88f), 0.08f);
			Coin(P, FV(0.5f, 0.22f), 0.07f);
			break;
		}
		case 30:
		{
			// Scholar's Coral: pink coral branching up out of an open book
			const FLinearColor Coral = Hex(0xF06A80);
			P.Ribbon(Smooth({ FV(0.5f, 0.62f), FV(0.48f, 0.42f), FV(0.42f, 0.26f), FV(0.36f, 0.14f) }), 0.04f, 0.025f, Coral);
			P.Ribbon(Smooth({ FV(0.48f, 0.44f), FV(0.6f, 0.32f), FV(0.66f, 0.16f) }), 0.035f, 0.022f, Coral);
			P.Ribbon(Smooth({ FV(0.44f, 0.32f), FV(0.28f, 0.28f), FV(0.22f, 0.18f) }), 0.03f, 0.02f, Coral);
			P.Ribbon(Smooth({ FV(0.58f, 0.36f), FV(0.76f, 0.34f), FV(0.8f, 0.24f) }), 0.028f, 0.018f, Coral);
			for (const FV Tip : { FV(0.36f, 0.14f), FV(0.66f, 0.16f), FV(0.22f, 0.18f), FV(0.8f, 0.24f) })
			{
				P.Ball(Tip, 0.03f, Hex(0xFFB0C0), 0.4f);
			}
			Book(P, FV(0.5f, 0.74f), 0.6f, Hex(0x2E6AA0));
			break;
		}
		case 31:
		{
			// Clearwater Lens: a brass magnifying glass over clear blue water
			P.Capsule(FV(0.62f, 0.62f), FV(0.86f, 0.86f), 0.05f, WoodDark);
			P.Capsule(FV(0.6f, 0.6f), FV(0.66f, 0.66f), 0.055f, Brass);
			const FV C(0.42f, 0.42f);
			P.Ring(C, 0.27f, 0.07f, Brass);
			P.Mark([C](FV Q) { return SdCircle(Q, C, 0.235f); }, Hex(0x8CE0FF, 0.55f));
			FClip Clip(P, [C](FV Q) { return SdCircle(Q, C, 0.235f); });
			P.MLine(Smooth({ C + FV(-0.24f, 0.08f), C + FV(-0.08f, 0.02f), C + FV(0.08f, 0.1f), C + FV(0.24f, 0.04f) }), 0.018f, Hex(0x2E86CC, 0.8f));
			P.MEllipse(C + FV(-0.08f, -0.1f), 0.1f, 0.05f, Hex(0xFFFFFF, 0.7f), 0.03f);
			break;
		}
		case 32:
		{
			// Pirate's Doubloon: a gold doubloon stamped with skull and crossbones
			const FV C(0.5f, 0.5f);
			Coin(P, C, 0.4f);
			const FLinearColor Stamp = Hex(0x8A5A18);
			P.MCapsule(C + FV(-0.17f, -0.05f), C + FV(0.17f, 0.21f), 0.03f, Stamp);
			P.MCapsule(C + FV(0.17f, -0.05f), C + FV(-0.17f, 0.21f), 0.03f, Stamp);
			P.MCircle(C + FV(0.0f, -0.08f), 0.12f, Stamp);
			P.MCapsule(C + FV(-0.05f, 0.02f), C + FV(0.05f, 0.02f), 0.05f, Stamp);
			P.MCircle(C + FV(-0.045f, -0.09f), 0.032f, Gold);
			P.MCircle(C + FV(0.045f, -0.09f), 0.032f, Gold);
			P.Gloss(C + FV(-0.18f, -0.22f), 0.08f, 0.04f, 0.6f);
			break;
		}
		case 33:
		{
			// Whalesong Charm: a whale-tail amulet on a cord, humming
			P.Arc(FV(0.42f, 0.36f), 0.24f, 0.02f, 200.0f, 340.0f, RopeColor);
			const FLinearColor Bone = Hex(0x3E6A9A);
			P.Ribbon({ FV(0.42f, 0.4f), FV(0.42f, 0.66f) }, 0.035f, 0.07f, Bone);
			P.Blob({ FV(0.42f, 0.6f), FV(0.2f, 0.58f), FV(0.06f, 0.76f), FV(0.26f, 0.78f), FV(0.42f, 0.72f), FV(0.58f, 0.78f), FV(0.78f, 0.76f), FV(0.64f, 0.58f) }, Bone);
			P.Ring(FV(0.42f, 0.38f), 0.03f, 0.016f, Gold);
			P.MCapsule(FV(0.42f, 0.66f), FV(0.42f, 0.74f), 0.01f, Hex(0x1A3A5A));
			MusicNotes(P, FV(0.76f, 0.34f), 0.34f, Hex(0x5CD89A));
			break;
		}
		case 34:
		{
			// Flying Fish Fin: a flying fish leaping clear of the water on wide ribbed fins
			P.Arc(FV(0.5f, 1.02f), 0.36f, 0.03f, 220.0f, 320.0f, Hex(0x5CC8F0, 0.9f));
			FPlace Place(P, FV(0.5f, 0.5f), 1.0f, -20.0f);
			const FLinearColor Fin = Hex(0x8FD0F0, 0.95f);
			const TArray<FV> Wing = Smooth({ FV(0.02f, -0.04f), FV(-0.1f, -0.3f), FV(-0.3f, -0.44f), FV(-0.26f, -0.26f), FV(-0.16f, -0.04f) }, true, 4);
			P.Poly(Wing, Fin);
			for (int32 i = 0; i < 4; ++i)
			{
				P.MCapsule(FV(-0.04f - i * 0.03f, -0.04f), FV(-0.12f - i * 0.05f, -0.3f - i * 0.03f), 0.007f, Hex(0x2E6AB0, 0.8f));
			}
			// tail fork, body and head
			P.Poly({ FV(-0.3f, 0.0f), FV(-0.48f, -0.16f), FV(-0.4f, 0.0f), FV(-0.5f, 0.12f) }, Hex(0x2E6AB0));
			const auto Body = [](FV Q) { return SdEllipse(Q, FV(0.04f, 0.0f), 0.36f, 0.1f); };
			P.Fill(Body, Hex(0x3A80C8));
			{
				FClip Clip(P, Body);
				P.MEllipse(FV(0.04f, 0.07f), 0.36f, 0.06f, Hex(0xE0F4FF), 0.02f);
				P.MCapsule(FV(0.22f, -0.08f), FV(0.24f, 0.08f), 0.008f, Hex(0x1A3A6A, 0.8f));
			}
			P.MCircle(FV(0.3f, -0.02f), 0.028f, Hex(0xFFFFFF));
			P.MCircle(FV(0.305f, -0.02f), 0.016f, Ink);
			const TArray<FV> Front = Smooth({ FV(0.04f, 0.04f), FV(-0.06f, 0.24f), FV(-0.22f, 0.32f), FV(-0.12f, 0.12f) }, true, 4);
			P.Poly(Front, Alpha(Fin, 0.9f));
			break;
		}
		case 35:
		{
			// Tidecaller's Conch: a great pink conch, waves pouring from it
			SpiralShell(P, FV(0.82f, 0.74f), FV(0.42f, 0.42f), 0.26f, Hex(0xF0C8A8), Hex(0xB86A50), 5);
			P.Blob({ FV(0.3f, 0.24f), FV(0.46f, 0.3f), FV(0.42f, 0.5f), FV(0.24f, 0.46f) }, Hex(0xFF90A8));
			P.MEllipse(FV(0.35f, 0.37f), 0.05f, 0.08f, Hex(0xB0405A));
			for (int32 i = 0; i < 3; ++i)
			{
				P.Arc(FV(0.36f, 0.36f), 0.18f + i * 0.1f, 0.03f, 160.0f, 260.0f, Hex(0x5CC8F0, 0.95f - i * 0.25f));
			}
			break;
		}
		case 36:
		{
			// Siren's Locket: an open heart locket with a pearl inside, on a chain
			for (int32 i = 0; i < 7; ++i)
			{
				const FV V = Spin(FV(0.2f, 0.0f), -170.0f + i * 26.0f);
				P.Ring(FV(0.5f, 0.32f) + V, 0.022f, 0.012f, Hex(0xFFE08A));
			}
			Heart(P, FV(0.5f, 0.6f), 0.9f, Gold);
			{
				FPlace Place(P, FV(0.5f, 0.6f), 0.62f);
				const TArray<FV> Tip = { FV(-0.4f, -0.04f), FV(0.4f, -0.04f), FV(0.0f, 0.42f) };
				P.Mark([&Tip](FV Q) { return FMath::Min(FMath::Min(SdCircle(Q, FV(-0.2f, -0.12f), 0.22f), SdCircle(Q, FV(0.2f, -0.12f), 0.22f)), SdPoly(Q, Tip)); }, Hex(0x8A1838));
			}
			PearlBall(P, FV(0.5f, 0.58f), 0.1f);
			break;
		}
		// ---------------- uniques (one each per run): a gold-orange halo behind each ----------------
		case 37:
		{
			// Trident of the Deep King
			UniqueHalo(P);
			P.Capsule(FV(0.5f, 0.94f), FV(0.5f, 0.34f), 0.035f, Hex(0x8A5A30));
			P.Capsule(FV(0.26f, 0.34f), FV(0.74f, 0.34f), 0.035f, Gold);
			for (int32 i = -1; i <= 1; ++i)
			{
				const float X = 0.5f + i * 0.22f;
				P.Poly({ FV(X - 0.045f, 0.36f), FV(X, i == 0 ? 0.04f : 0.1f), FV(X + 0.045f, 0.36f) }, Gold);
				P.Poly({ FV(X - 0.06f, 0.26f), FV(X - 0.1f, 0.2f), FV(X - 0.02f, 0.24f) }, Brass);
			}
			Glint(P, FV(0.5f, 0.06f), 0.22f, Hex(0xFFF0B0));
			break;
		}
		case 38:
		{
			// Leviathan's Jaw: a great jawbone full of teeth
			UniqueHalo(P);
			P.Arc(FV(0.5f, 0.26f), 0.36f, 0.12f, 30.0f, 150.0f, Hex(0xE8DCC0));
			for (int32 i = 0; i < 7; ++i)
			{
				const float A = FMath::DegreesToRadians(40.0f + i * 16.6f);
				const FV Root = FV(0.5f, 0.26f) + FV(FMath::Cos(A), FMath::Sin(A)) * 0.3f;
				const FV Tip = FV(0.5f, 0.26f) + FV(FMath::Cos(A), FMath::Sin(A)) * 0.16f;
				const FV Side = FV(-FMath::Sin(A), FMath::Cos(A)) * 0.035f;
				P.Poly({ Root + Side, Tip, Root - Side }, White);
			}
			P.Star(FV(0.5f, 0.82f), 0.12f, 0.05f, 8, 0.0f, Blood);
			break;
		}
		case 39:
		{
			// Abyssal Anchor
			UniqueHalo(P);
			const FLinearColor Dark = Hex(0x4A4A6A);
			P.Ring(FV(0.5f, 0.14f), 0.07f, 0.035f, Dark);
			P.Capsule(FV(0.5f, 0.2f), FV(0.5f, 0.84f), 0.045f, Dark);
			P.Capsule(FV(0.32f, 0.3f), FV(0.68f, 0.3f), 0.035f, Dark);
			P.Arc(FV(0.5f, 0.56f), 0.3f, 0.07f, 20.0f, 160.0f, Dark);
			P.Poly({ FV(0.8f, 0.6f), FV(0.9f, 0.5f), FV(0.86f, 0.7f) }, Dark);
			P.Poly({ FV(0.2f, 0.6f), FV(0.1f, 0.5f), FV(0.14f, 0.7f) }, Dark);
			P.MArc(FV(0.5f, 0.56f), 0.3f, 0.02f, 40.0f, 140.0f, Hex(0xA890FF, 0.8f));
			break;
		}
		case 40:
		{
			// Tear of the Moon: a crescent and a falling pearl tear
			UniqueHalo(P);
			P.Fill([](FV Q) { return FMath::Max(SdCircle(Q, FV(0.42f, 0.42f), 0.32f), -SdCircle(Q, FV(0.56f, 0.34f), 0.28f)); }, Hex(0xE8EEFF));
			Drop(P, FV(0.72f, 0.72f), 0.34f, Hex(0x9ED8FF));
			Glint(P, FV(0.78f, 0.2f), 0.2f, Hex(0xFFFFFF));
			break;
		}
		case 41:
		{
			// Heart of Coral
			UniqueHalo(P);
			Heart(P, FV(0.5f, 0.5f), 0.95f, Hex(0xFF6A5A));
			P.Capsule(FV(0.36f, 0.42f), FV(0.3f, 0.26f), 0.03f, Hex(0xFFB0A0));
			P.Capsule(FV(0.33f, 0.33f), FV(0.24f, 0.3f), 0.022f, Hex(0xFFB0A0));
			P.Capsule(FV(0.62f, 0.42f), FV(0.7f, 0.24f), 0.03f, Hex(0xFFB0A0));
			break;
		}
		case 42:
		{
			// Black Tide Pearl: a black pearl wreathed in venom
			UniqueHalo(P);
			P.Glow([](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.3f); }, Poison, 0.1f, 0.6f);
			P.Ball(FV(0.5f, 0.5f), 0.32f, Hex(0x1E2228), 0.7f);
			{
				FClip Clip(P, [](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.32f); });
				P.MEllipse(FV(0.6f, 0.62f), 0.2f, 0.14f, Hex(0x74D84A, 0.45f), 0.08f);
				P.Gloss(FV(0.4f, 0.38f), 0.09f, 0.06f, 0.9f);
			}
			Drop(P, FV(0.78f, 0.84f), 0.2f, Poison);
			break;
		}
		case 43:
		{
			// Compass of Tides
			UniqueHalo(P);
			P.Circle(FV(0.5f, 0.5f), 0.4f, Brass);
			P.Circle(FV(0.5f, 0.5f), 0.32f, Hex(0xF0F6F8));
			for (int32 i = 0; i < 8; ++i)
			{
				const FV Dir = Spin(FV(1.0f, 0.0f), i * 45.0f);
				P.MCapsule(FV(0.5f, 0.5f) + Dir * 0.26f, FV(0.5f, 0.5f) + Dir * 0.3f, i % 2 ? 0.006f : 0.012f, Alpha(Ink, 0.8f));
			}
			P.Poly({ FV(0.5f, 0.2f), FV(0.56f, 0.5f), FV(0.44f, 0.5f) }, Blood);
			P.Poly({ FV(0.5f, 0.8f), FV(0.56f, 0.5f), FV(0.44f, 0.5f) }, Hex(0x2F8ED0));
			P.Ball(FV(0.5f, 0.5f), 0.04f, Gold);
			break;
		}
		case 44:
		{
			// Gilded Conch
			UniqueHalo(P);
			SpiralShell(P, FV(0.22f, 0.72f), FV(0.78f, 0.26f), 0.3f, Gold, Hex(0xC98A2A), 4);
			Coin(P, FV(0.24f, 0.24f), 0.1f);
			Coin(P, FV(0.78f, 0.8f), 0.08f);
			break;
		}
		case 45:
		{
			// Kraken's Heart: a dark heart in the kraken's grip
			UniqueHalo(P);
			Tentacle(P, { FV(0.08f, 0.9f), FV(0.2f, 0.6f), FV(0.14f, 0.36f), FV(0.3f, 0.2f) }, 0.07f, 0.02f, 1.0f, Hex(0x7A2A8A), Hex(0xE0A0E8));
			Heart(P, FV(0.54f, 0.52f), 0.85f, Hex(0xB0203A));
			Tentacle(P, { FV(0.94f, 0.86f), FV(0.82f, 0.64f), FV(0.9f, 0.4f), FV(0.74f, 0.26f) }, 0.07f, 0.02f, -1.0f, Hex(0x7A2A8A), Hex(0xE0A0E8));
			break;
		}
		default:
			break;
		}
	}

	void PaintMisc(FPainter& P, ETDMiscIcon Icon)
	{
		switch (Icon)
		{
		case ETDMiscIcon::Pearl:
			PearlBall(P, FV(0.5f, 0.5f), 0.42f);
			break;
		case ETDMiscIcon::Life:
			Heart(P, FV(0.5f, 0.52f), 1.0f, Hex(0xFF5F7E));
			break;
		case ETDMiscIcon::Wave:
		{
			// one curling wave over two swells
			const TArray<FV> Crest = Smooth({ FV(0.06f, 0.72f), FV(0.2f, 0.58f), FV(0.34f, 0.32f), FV(0.56f, 0.14f), FV(0.82f, 0.18f), FV(0.92f, 0.36f), FV(0.82f, 0.46f), FV(0.72f, 0.38f), FV(0.62f, 0.4f), FV(0.62f, 0.56f), FV(0.78f, 0.72f), FV(0.94f, 0.76f), FV(0.94f, 0.92f), FV(0.06f, 0.92f) }, true, 5);
			const auto Water = [&Crest](FV Q) { return SdPoly(Q, Crest); };
			{
				FWith With(P);
				P.Look.Dome = 0.14f;
				P.Fill(Water, Hex(0x2F8ED0));
			}
			FClip Clip(P, Water);
			P.MEllipse(FV(0.36f, 0.58f), 0.17f, 0.3f, Hex(0x74D4F2, 0.7f), 0.08f);
			P.MLine(Smooth({ FV(0.06f, 0.82f), FV(0.28f, 0.76f), FV(0.5f, 0.84f), FV(0.72f, 0.78f), FV(0.96f, 0.86f) }), 0.018f, Hex(0x124E8E, 0.85f));
			P.MLine(Smooth({ FV(0.2f, 0.62f), FV(0.32f, 0.38f), FV(0.5f, 0.24f), FV(0.72f, 0.2f), FV(0.88f, 0.28f) }), 0.03f, Hex(0xE4FAFF));
			P.MCircle(FV(0.85f, 0.36f), 0.03f, Hex(0xE4FAFF));
			P.MCircle(FV(0.76f, 0.33f), 0.022f, Hex(0xE4FAFF));
			P.MCircle(FV(0.68f, 0.34f), 0.018f, Hex(0xE4FAFF));
			break;
		}
		case ETDMiscIcon::SkillPoint:
		{
			FPlace Place(P, FV(0.5f, 0.5f), 0.95f);
			const TArray<FV> Pts = StarPoints(FV(0.0f, 0.02f), 0.5f, 0.23f, 5, -90.0f);
			{
				FWith With(P);
				P.Look.Dome = 0.3f;
				P.Look.Spec = 0.7f;
				P.Fill([&Pts](FV Q) { return SdPoly(Q, Pts) - 0.015f; }, Gold);
			}
			P.Star(FV(0.0f, 0.02f), 0.22f, 0.1f, 5, -90.0f, Hex(0xFFEFB0));
			break;
		}
		case ETDMiscIcon::Logo:
		{
			// tiny app icon sizes keep the big shapes (disc, sea, sand, coral, harpoon) and drop sub-pixel trimmings that only blur
			const bool bSmall = AppIconOutSize > 0 && AppIconOutSize <= 32;
			const bool bTiny = AppIconOutSize > 0 && AppIconOutSize <= 16;
			const auto Disc = [](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.47f); };
			P.Mark(Disc, Hex(0x0E5C80));
			{
				FClip Clip(P, Disc);
				P.MEllipse(FV(0.5f, 0.1f), 0.5f, 0.4f, Hex(0x4CC0E0, 0.6f), 0.2f);
				// a wave crest band near the surface, with a bright crest line
				P.Mark([](FV Q) { return Q.Y - (0.2f + FMath::Sin(Q.X * 18.0f) * 0.02f); }, Hex(0x2C9CC8, 0.55f));
				if (!bTiny)
				{
					P.Mark([](FV Q) { return FMath::Abs(Q.Y - 0.2f - FMath::Sin(Q.X * 18.0f) * 0.02f) - 0.009f; }, Hex(0xC8F6FF, 0.75f));
				}
				// a strand of kelp rising on the left
				P.MLine(Smooth({ FV(0.17f, 0.8f), FV(0.2f, 0.66f), FV(0.16f, 0.54f), FV(0.2f, 0.42f) }), bSmall ? 0.034f : 0.022f, Hex(0x2E8A5A, 0.9f));
				if (!bSmall)
				{
					P.Ball(FV(0.205f, 0.6f), 0.022f, Hex(0x5CC08A), 0.2f);
					P.Ball(FV(0.165f, 0.5f), 0.018f, Hex(0x5CC08A), 0.2f);
				}
				P.Mark([](FV Q) { return 0.76f + FMath::Sin(Q.X * 14.0f) * 0.012f - Q.Y; }, Hex(0xE8C98A));
				if (!bSmall)
				{
					P.Mark([](FV Q) { return FMath::Abs(Q.Y - 0.76f - FMath::Sin(Q.X * 14.0f) * 0.012f) - 0.008f; }, Hex(0xFFF0C8, 0.8f));
					// a small starfish on the sand to the right of the reef
					P.Star(FV(0.83f, 0.8f), 0.06f, 0.025f, 5, -80.0f, Hex(0xFF9A5A));
				}
				// little reef of brain coral along the sand
				for (int32 i = 0; i < 3; ++i)
				{
					const FV C(0.3f + i * 0.2f, 0.74f);
					P.Ball(C, 0.095f, i == 1 ? Hex(0xFF7C80) : Hex(0xE8606A), 0.3f);
					if (!bSmall)
					{
						P.MLine(Smooth({ C + FV(-0.06f, 0.0f), C + FV(-0.02f, -0.03f), C + FV(0.02f, 0.01f), C + FV(0.06f, -0.02f) }), 0.006f, Hex(0x8E2A44, 0.9f));
					}
				}
			}
			Harpoon(P, FV(0.54f, 0.38f), 0.9f, -45.0f);
			if (!bTiny)
			{
				Bubble(P, FV(0.24f, 0.34f), bSmall ? 0.06f : 0.05f);
			}
			if (!bSmall)
			{
				Bubble(P, FV(0.32f, 0.2f), 0.035f);
			}
			P.Mark([](FV Q) { return FMath::Abs(SdCircle(Q, FV(0.5f, 0.5f), 0.46f)) - 0.012f; }, Hex(0xBDEFFF, 0.8f));
			// rim highlight: a brighter arc over the top left, and a faint inner ring
			P.Mark([](FV Q)
			{
				const FV D = Q - FV(0.5f, 0.5f);
				const float Arc = FMath::Abs(SdCircle(Q, FV(0.5f, 0.5f), 0.46f)) - 0.016f;
				// only where the direction points up and left (fades out by the sides)
				const float Facing = (-D.X - D.Y) / FMath::Max(0.001f, D.Size());
				return FMath::Max(Arc, 0.55f - Facing);
			}, Hex(0xFFFFFF, 0.55f));
			if (bSmall)
			{
				break;
			}
			P.Mark([](FV Q) { return FMath::Abs(SdCircle(Q, FV(0.5f, 0.5f), 0.425f)) - 0.005f; }, Hex(0x9FE6FF, 0.3f));
			// a ring of tiny bubbles just inside the rim along the upper right
			for (int32 i = 0; i < 6; ++i)
			{
				const float A = FMath::DegreesToRadians(-70.0f + i * 16.0f);
				Bubble(P, FV(0.5f + FMath::Cos(A) * 0.39f, 0.5f + FMath::Sin(A) * 0.39f), i % 2 ? 0.016f : 0.022f);
			}
			break;
		}
		default:
			break;
		}
	}

	// ---- run boons ----

	/** Dark red-violet aura behind every cursed boon, so the family reads at a glance */
	void CurseAura(FPainter& P)
	{
		P.Glow([](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.36f); }, Hex(0x8A1E52), 0.14f, 0.75f);
		P.Mark([](FV Q) { return FMath::Abs(SdCircle(Q, FV(0.5f, 0.5f), 0.46f)) - 0.012f; }, Hex(0xB0306A, 0.7f));
	}

	void PaintBoon(FPainter& P, int32 Id)
	{
		const FLinearColor Coral = Hex(0xFF7A6A);
		const FLinearColor CoralDark = Hex(0xC24A44);
		const FLinearColor Sea = Hex(0x2F8ED0);
		const FLinearColor Foam = Hex(0xBFEFFF);
		if (Id >= (int32)ETDBoon::BloodTide)
		{
			CurseAura(P);
		}
		switch ((ETDBoon)Id)
		{
		case ETDBoon::SharpenedReef:
		{
			// a coral blade with a keen glinting edge
			const TArray<FV> Blade = { FV(0.22f, 0.72f), FV(0.72f, 0.16f), FV(0.86f, 0.12f), FV(0.83f, 0.26f), FV(0.3f, 0.8f) };
			P.Poly(Blade, Coral);
			{
				FClip Clip(P, [&Blade](FV Q) { return SdPoly(Q, Blade); });
				P.MLine({ FV(0.25f, 0.74f), FV(0.76f, 0.16f) }, 0.016f, Hex(0xFFE8E0, 0.9f));
				for (int32 i = 0; i < 4; ++i)
				{
					P.MCircle(FV(0.4f + i * 0.1f, 0.6f - i * 0.11f), 0.018f, Alpha(CoralDark, 0.7f));
				}
			}
			P.Capsule(FV(0.14f, 0.66f), FV(0.36f, 0.88f), 0.035f, CoralDark);
			P.Capsule(FV(0.1f, 0.92f), FV(0.24f, 0.78f), 0.05f, Wood);
			Glint(P, FV(0.8f, 0.18f), 0.3f);
			break;
		}
		case ETDBoon::TidalFury:
		{
			// a swirling wave with speed lines
			P.Arc(FV(0.56f, 0.5f), 0.28f, 0.12f, -70.0f, 230.0f, Sea);
			P.Arc(FV(0.56f, 0.5f), 0.12f, 0.08f, -70.0f, 200.0f, Hex(0x74D4F2));
			P.MArc(FV(0.56f, 0.5f), 0.28f, 0.03f, -60.0f, 120.0f, Alpha(Foam, 0.8f));
			for (int32 i = 0; i < 3; ++i)
			{
				const float Y = 0.32f + i * 0.16f;
				P.Capsule(FV(0.06f + i * 0.04f, Y), FV(0.24f, Y), 0.025f, Foam);
			}
			break;
		}
		case ETDBoon::LongSight:
			Eye(P, FV(0.46f, 0.5f), 0.8f);
			P.Arc(FV(0.46f, 0.5f), 0.42f, 0.035f, -40.0f, 40.0f, Frost);
			P.Arc(FV(0.46f, 0.5f), 0.52f, 0.035f, -30.0f, 30.0f, Alpha(Frost, 0.8f));
			break;
		case ETDBoon::ScholarsTide:
			Clam(P, FV(0.42f, 0.6f), 0.7f);
			Glint(P, FV(0.74f, 0.24f), 0.34f, Hex(0x60F0BA));
			PlusBadge(P, FV(0.86f, 0.5f), 0.22f, Hex(0x60F0BA));
			break;
		case ETDBoon::BountyHunters:
			PearlBall(P, FV(0.5f, 0.5f), 0.26f);
			P.Ring(FV(0.5f, 0.5f), 0.36f, 0.045f, Blood);
			P.Capsule(FV(0.5f, 0.04f), FV(0.5f, 0.2f), 0.03f, Blood);
			P.Capsule(FV(0.5f, 0.8f), FV(0.5f, 0.96f), 0.03f, Blood);
			P.Capsule(FV(0.04f, 0.5f), FV(0.2f, 0.5f), 0.03f, Blood);
			P.Capsule(FV(0.8f, 0.5f), FV(0.96f, 0.5f), 0.03f, Blood);
			break;
		case ETDBoon::DeepPockets:
		{
			// a pearl purse with a percent glint
			P.Blob({ FV(0.16f, 0.5f), FV(0.5f, 0.4f), FV(0.82f, 0.5f), FV(0.86f, 0.78f), FV(0.5f, 0.92f), FV(0.14f, 0.78f) }, Wood, 0.02f);
			P.Capsule(FV(0.2f, 0.46f), FV(0.8f, 0.46f), 0.035f, Hex(0x6A4224));
			PearlBall(P, FV(0.5f, 0.32f), 0.15f);
			P.Circle(FV(0.62f, 0.66f), 0.035f, Gold);
			P.Circle(FV(0.76f, 0.8f), 0.035f, Gold);
			P.Capsule(FV(0.6f, 0.82f), FV(0.78f, 0.62f), 0.018f, Gold);
			break;
		}
		case ETDBoon::CoralMasons:
		{
			// a coral brick and a trowel
			P.Box(FV(0.08f, 0.5f), FV(0.62f, 0.86f), 0.04f, Coral);
			P.MCapsule(FV(0.08f, 0.68f), FV(0.62f, 0.68f), 0.012f, Alpha(CoralDark, 0.8f));
			P.MCapsule(FV(0.34f, 0.5f), FV(0.34f, 0.68f), 0.012f, Alpha(CoralDark, 0.8f));
			P.MCapsule(FV(0.2f, 0.68f), FV(0.2f, 0.86f), 0.012f, Alpha(CoralDark, 0.8f));
			P.MCapsule(FV(0.48f, 0.68f), FV(0.48f, 0.86f), 0.012f, Alpha(CoralDark, 0.8f));
			P.Poly({ FV(0.5f, 0.42f), FV(0.86f, 0.14f), FV(0.94f, 0.26f), FV(0.62f, 0.5f) }, Steel);
			P.Capsule(FV(0.66f, 0.26f), FV(0.56f, 0.12f), 0.02f, SteelDark);
			P.Capsule(FV(0.56f, 0.12f), FV(0.44f, 0.06f), 0.04f, Wood);
			break;
		}
		case ETDBoon::CheapUpgrades:
		{
			UpBadge(P, FV(0.38f, 0.48f), 0.8f);
			// price tag
			FPlace Place(P, FV(0.72f, 0.68f), 0.36f, -30.0f);
			P.Poly({ FV(-0.5f, -0.3f), FV(0.3f, -0.3f), FV(0.55f, 0.0f), FV(0.3f, 0.3f), FV(-0.5f, 0.3f) }, Hex(0xF6EBD0));
			P.MCircle(FV(0.28f, 0.0f), 0.08f, Ink);
			P.MCapsule(FV(-0.34f, -0.06f), FV(0.06f, -0.06f), 0.045f, Blood);
			P.MCapsule(FV(-0.34f, 0.1f), FV(-0.04f, 0.1f), 0.035f, Alpha(Ink, 0.6f));
			break;
		}
		case ETDBoon::TreasureCurrents:
		{
			// a chest riding a current
			P.Arc(FV(0.5f, 0.9f), 0.44f, 0.05f, 200.0f, 340.0f, Alpha(Foam, 0.9f));
			P.Arc(FV(0.5f, 1.0f), 0.48f, 0.04f, 210.0f, 330.0f, Alpha(Sea, 0.9f));
			P.Box(FV(0.2f, 0.42f), FV(0.8f, 0.76f), 0.03f, Wood);
			P.Fill([](FV Q) { return FMath::Max(SdEllipse(Q, FV(0.5f, 0.44f), 0.31f, 0.2f), Q.Y - 0.44f); }, Hex(0x9C6A3A));
			P.Box(FV(0.2f, 0.42f), FV(0.8f, 0.48f), 0.01f, Gold);
			P.Box(FV(0.45f, 0.4f), FV(0.55f, 0.56f), 0.015f, Gold);
			Glint(P, FV(0.76f, 0.24f), 0.26f, Gold);
			break;
		}
		case ETDBoon::LuckyFind:
			Clover(P, FV(0.46f, 0.52f), 1.1f);
			Glint(P, FV(0.8f, 0.2f), 0.32f, Gold);
			break;
		case ETDBoon::ThickReef:
			Rock(P, FV(0.3f, 0.78f), 0.16f, 3, Stone);
			Rock(P, FV(0.66f, 0.8f), 0.14f, 7, Stone);
			P.Capsule(FV(0.42f, 0.8f), FV(0.36f, 0.6f), 0.035f, Coral);
			P.Capsule(FV(0.36f, 0.6f), FV(0.28f, 0.52f), 0.03f, Coral);
			P.Capsule(FV(0.38f, 0.66f), FV(0.46f, 0.56f), 0.028f, Coral);
			Heart(P, FV(0.58f, 0.34f), 0.6f, Hex(0xFF5F7E));
			break;
		case ETDBoon::GiantSlayer:
			Tentacle(P, { FV(0.1f, 0.94f), FV(0.3f, 0.7f), FV(0.62f, 0.66f), FV(0.82f, 0.4f), FV(0.7f, 0.2f) }, 0.12f, 0.03f, 1.0f, Hex(0xB04A8C), Hex(0xF2B8D8));
			P.Poly({ FV(0.62f, 0.5f), FV(0.9f, 0.08f), FV(0.96f, 0.12f), FV(0.68f, 0.56f) }, Steel);
			P.Capsule(FV(0.56f, 0.46f), FV(0.72f, 0.6f), 0.025f, Brass);
			P.Capsule(FV(0.6f, 0.58f), FV(0.52f, 0.68f), 0.035f, Wood);
			break;
		case ETDBoon::VenomBloom:
		{
			// a venom flower dripping
			for (int32 i = 0; i < 5; ++i)
			{
				const FV Dir = Spin(FV(0.0f, -1.0f), i * 72.0f);
				const FV C = FV(0.5f, 0.44f) + Dir * 0.17f;
				P.Fill([C, Dir](FV Q) { const FV L = Q - C; return SdEllipse(FV(L.X * -Dir.Y + L.Y * Dir.X, L.X * Dir.X + L.Y * Dir.Y), FV(0.0f, 0.0f), 0.1f, 0.17f); }, Hex(0x9A4AD0));
			}
			P.Ball(FV(0.5f, 0.44f), 0.09f, Poison);
			Drop(P, FV(0.36f, 0.84f), 0.26f, Poison);
			Drop(P, FV(0.64f, 0.88f), 0.2f, Poison);
			break;
		}
		case ETDBoon::StormCoil:
			P.Arc(FV(0.5f, 0.55f), 0.3f, 0.12f, 120.0f, 470.0f, Hex(0x4E8A3C));
			P.Arc(FV(0.5f, 0.55f), 0.13f, 0.09f, 140.0f, 420.0f, Hex(0x6AAA4C));
			P.MArc(FV(0.5f, 0.55f), 0.3f, 0.03f, 160.0f, 300.0f, Hex(0xD8F0A0, 0.6f));
			Bolt(P, FV(0.74f, 0.24f), 0.42f, Volt, 10.0f);
			break;
		// ---- cursed ----
		case ETDBoon::BloodTide:
		{
			const TArray<FV> Crest = Smooth({ FV(0.08f, 0.8f), FV(0.24f, 0.58f), FV(0.46f, 0.42f), FV(0.7f, 0.4f), FV(0.86f, 0.52f), FV(0.76f, 0.6f), FV(0.66f, 0.56f), FV(0.66f, 0.7f), FV(0.92f, 0.8f), FV(0.92f, 0.9f), FV(0.08f, 0.9f) }, true, 5);
			P.Poly(Crest, Hex(0x9A1A2A));
			P.MLine(Smooth({ FV(0.2f, 0.66f), FV(0.42f, 0.48f), FV(0.66f, 0.44f), FV(0.82f, 0.5f) }), 0.025f, Hex(0xFF8A8A, 0.85f));
			Drop(P, FV(0.42f, 0.2f), 0.34f, Blood);
			break;
		}
		case ETDBoon::KrakensPact:
		{
			// a scroll bound by a tentacle
			P.Box(FV(0.22f, 0.24f), FV(0.78f, 0.76f), 0.02f, Hex(0xF0DFB8));
			P.Capsule(FV(0.2f, 0.24f), FV(0.8f, 0.24f), 0.05f, Hex(0xD8C090));
			P.Capsule(FV(0.2f, 0.76f), FV(0.8f, 0.76f), 0.05f, Hex(0xD8C090));
			for (int32 i = 0; i < 3; ++i)
			{
				P.MCapsule(FV(0.32f, 0.38f + i * 0.1f), FV(0.68f, 0.38f + i * 0.1f), 0.012f, Alpha(Hex(0x8A2A3A), 0.8f));
			}
			Tentacle(P, { FV(0.04f, 0.62f), FV(0.3f, 0.5f), FV(0.62f, 0.62f), FV(0.86f, 0.46f), FV(0.9f, 0.3f) }, 0.08f, 0.025f, -1.0f, Hex(0x6A2A7A), Hex(0xD89AD8));
			break;
		}
		case ETDBoon::GildedShackles:
		{
			// a gold shackle on a short chain
			P.Arc(FV(0.36f, 0.4f), 0.2f, 0.07f, 180.0f, 360.0f, Gold);
			P.Capsule(FV(0.16f, 0.4f), FV(0.16f, 0.6f), 0.035f, Gold);
			P.Capsule(FV(0.56f, 0.4f), FV(0.56f, 0.6f), 0.035f, Gold);
			P.Box(FV(0.1f, 0.56f), FV(0.62f, 0.66f), 0.03f, Brass);
			for (int32 i = 0; i < 3; ++i)
			{
				const FV C = FV(0.62f + i * 0.1f, 0.68f + i * 0.08f);
				P.Fill([C](FV Q) { return FMath::Abs(SdEllipse(Q, C, 0.07f, 0.045f)) - 0.016f; }, Gold);
			}
			break;
		}
		case ETDBoon::FrenziedCurrents:
		{
			// a jagged red swirl
			TArray<FV> Spiral;
			for (int32 i = 0; i <= 18; ++i)
			{
				const float A = i * 40.0f;
				const float R = 0.4f - i * 0.019f + ((i % 2) ? 0.04f : -0.02f);
				Spiral.Add(FV(0.5f, 0.5f) + Spin(FV(R, 0.0f), A));
			}
			P.Line(Spiral, 0.04f, Hex(0xE0303A));
			P.MLine(Spiral, 0.012f, Hex(0xFFB0A0, 0.8f));
			break;
		}
		case ETDBoon::BrittleCoral:
		{
			// a coral branch cracked through, a broken tip falling away
			P.Capsule(FV(0.4f, 0.92f), FV(0.42f, 0.5f), 0.06f, Coral);
			P.Capsule(FV(0.42f, 0.6f), FV(0.22f, 0.36f), 0.045f, Coral);
			P.Capsule(FV(0.42f, 0.56f), FV(0.56f, 0.34f), 0.045f, Coral);
			P.Capsule(FV(0.22f, 0.36f), FV(0.18f, 0.16f), 0.04f, Coral);
			P.MLine({ FV(0.36f, 0.86f), FV(0.44f, 0.76f), FV(0.38f, 0.68f), FV(0.46f, 0.6f) }, 0.014f, Hex(0x3A0A14));
			P.MLine({ FV(0.24f, 0.4f), FV(0.18f, 0.32f), FV(0.24f, 0.26f) }, 0.012f, Hex(0x3A0A14));
			P.Capsule(FV(0.7f, 0.24f), FV(0.8f, 0.1f), 0.04f, CoralDark);
			P.Capsule(FV(0.74f, 0.4f), FV(0.82f, 0.34f), 0.03f, CoralDark);
			break;
		}
		case ETDBoon::HungryDeep:
		{
			// an open maw into the dark
			P.Circle(FV(0.5f, 0.5f), 0.4f, Hex(0x3A1048));
			FClip Clip(P, [](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.4f); });
			P.MCircle(FV(0.5f, 0.52f), 0.24f, Hex(0x08020C));
			for (int32 i = 0; i < 12; ++i)
			{
				const FV Dir = Spin(FV(1.0f, 0.0f), i * 30.0f);
				const FV Side(-Dir.Y, Dir.X);
				const FV Root = FV(0.5f, 0.5f) + Dir * 0.38f;
				P.Poly({ Root + Side * 0.05f, FV(0.5f, 0.5f) + Dir * 0.22f, Root - Side * 0.05f }, White);
			}
			break;
		}
		case ETDBoon::SirensDebt:
		{
			// a siren's silhouette and the coin she's owed
			const FLinearColor Body = Hex(0x5A3A8A);
			P.Circle(FV(0.38f, 0.22f), 0.11f, Body);
			P.Ribbon(Smooth({ FV(0.3f, 0.16f), FV(0.18f, 0.32f), FV(0.2f, 0.5f) }), 0.05f, 0.02f, Hex(0x8A4AC0));
			P.Ribbon(Smooth({ FV(0.38f, 0.32f), FV(0.42f, 0.52f), FV(0.36f, 0.72f), FV(0.46f, 0.86f) }), 0.1f, 0.04f, Body);
			P.Poly({ FV(0.46f, 0.84f), FV(0.62f, 0.78f), FV(0.58f, 0.96f) }, Body);
			Coin(P, FV(0.72f, 0.5f), 0.18f);
			break;
		}
		case ETDBoon::Eclipse:
			P.Glow([](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.3f); }, Gold, 0.16f, 0.9f);
			P.Star(FV(0.5f, 0.5f), 0.46f, 0.32f, 12, 0.0f, Hex(0xFFB04A));
			P.Circle(FV(0.5f, 0.5f), 0.3f, Gold);
			P.Circle(FV(0.56f, 0.46f), 0.29f, Hex(0x1A0A2A));
			P.MArc(FV(0.5f, 0.5f), 0.29f, 0.03f, 100.0f, 230.0f, Hex(0xFFF0B0, 0.9f));
			break;
		default:
			break;
		}
	}

	TArray<FColor> Downsample(const TArray<FColor>& Src, int32 SrcSize)
	{
		const int32 Size = SrcSize / 2;
		TArray<FColor> Out;
		Out.SetNumUninitialized(Size * Size);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				// average in premultiplied linear space so edges don't darken
				FLinearColor Sum(0, 0, 0, 0);
				for (int32 i = 0; i < 4; ++i)
				{
					const FLinearColor C = FLinearColor(Src[(Y * 2 + i / 2) * SrcSize + X * 2 + i % 2]);
					Sum += FLinearColor(C.R * C.A, C.G * C.A, C.B * C.A, C.A);
				}
				Out[Y * Size + X] = FPainter::Unpremultiply(Sum * 0.25f);
			}
		}
		return Out;
	}

	/** Light unsharp mask, because each averaged mip is softer than the level above it and the HUD draws icons from the small mips */
	TArray<FColor> SharpenMip(const TArray<FColor>& Src, int32 Size)
	{
		const float Amount = 0.35f;
		TArray<FColor> Out = Src;
		for (int32 Y = 1; Y + 1 < Size; ++Y)
		{
			for (int32 X = 1; X + 1 < Size; ++X)
			{
				const FColor& C = Src[Y * Size + X];
				if (C.A == 0)
				{
					continue;
				}
				const FColor Around[] = { Src[Y * Size + X - 1], Src[Y * Size + X + 1], Src[(Y - 1) * Size + X], Src[(Y + 1) * Size + X] };
				float R = 0.0f, G = 0.0f, B = 0.0f, A = 0.0f, Weight = 0.0f;
				for (const FColor& N : Around)
				{
					// transparent neighbours have no real colour, so they only count towards the alpha
					const float W = N.A / 255.0f;
					R += N.R * W;
					G += N.G * W;
					B += N.B * W;
					A += N.A;
					Weight += W;
				}
				auto Push = [Amount](float Value, float Mean) { return (uint8)FMath::Clamp(FMath::RoundToInt(Value + (Value - Mean) * Amount), 0, 255); };
				FColor& Dst = Out[Y * Size + X];
				if (Weight > 0.0f)
				{
					Dst.R = Push(C.R, R / Weight);
					Dst.G = Push(C.G, G / Weight);
					Dst.B = Push(C.B, B / Weight);
				}
				Dst.A = Push(C.A, A * 0.25f);
			}
		}
		return Out;
	}

	TMap<uint32, UTexture2D*>& Cache()
	{
		static TMap<uint32, UTexture2D*> Textures;
		return Textures;
	}

	uint32 CacheKey(ETDIcon Kind, int32 Id)
	{
		return ((uint32)Kind << 16) | (uint32)(Id & 0xFFFF);
	}

	/** Pixels painted ahead of time, waiting to become textures */
	TMap<uint32, TArray<FColor>>& Pending()
	{
		static TMap<uint32, TArray<FColor>> Painted;
		return Painted;
	}

	bool bPaintedAll = false;

	/** Drops every cached icon texture, so the next Get paints them again (after atd.DetailedIcons changes) */
	void FlushIcons()
	{
		for (TPair<uint32, UTexture2D*>& Entry : Cache())
		{
			if (Entry.Value)
			{
				Entry.Value->RemoveFromRoot();
			}
		}
		Cache().Reset();
		Pending().Reset();
		bPaintedAll = false;
	}

	TAutoConsoleVariable<int32> CVarDetailedIcons(
		TEXT("atd.DetailedIcons"),
		1,
		TEXT("1 paints every HUD icon with the detailed look (finer edges, cast shadows between parts, catch and bounce light, surface texture, a soft drop shadow); 0 the original look."),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*) { FlushIcons(); }));

	/** Paints every icon the HUD can show on all cores at once, so opening a panel never hitches later */
	void PaintAllOnce()
	{
		if (bPaintedAll)
		{
			return;
		}
		bPaintedAll = true;
		const double Start = FPlatformTime::Seconds();

		struct FJob
		{
			ETDIcon Kind;
			int32 Id;
			TArray<FColor> Pixels;
			bool bOk;
		};
		TArray<FJob> Jobs;
		const ETDIcon Kinds[] = { ETDIcon::Tower, ETDIcon::Enemy, ETDIcon::Skill, ETDIcon::Item, ETDIcon::Misc, ETDIcon::ItemKind, ETDIcon::Boon };
		for (ETDIcon Kind : Kinds)
		{
			for (int32 Id = 0; Id < TDIcons::Count(Kind); ++Id)
			{
				Jobs.Add(FJob{ Kind, Id, {}, false });
			}
		}
		ParallelFor(Jobs.Num(), [&Jobs](int32 i)
		{
			Jobs[i].bOk = TDIcons::Render(Jobs[i].Kind, Jobs[i].Id, TDIcons::TextureSize, Jobs[i].Pixels);
		});
		for (FJob& Job : Jobs)
		{
			if (Job.bOk)
			{
				Pending().Add(CacheKey(Job.Kind, Job.Id), MoveTemp(Job.Pixels));
			}
		}
		UE_LOG(LogAquaticTowerDefense, Log, TEXT("Painted %d icons (%s) in %.0f ms"), Jobs.Num(), TDIcons::UseDetailed() ? TEXT("detailed") : TEXT("original"), (FPlatformTime::Seconds() - Start) * 1000.0);
	}
}

int32 TDIcons::Count(ETDIcon Kind)
{
	switch (Kind)
	{
	case ETDIcon::Tower:	return TD::TowerTypeCount;
	case ETDIcon::Enemy:	return TD::EnemyTypeCount;
	case ETDIcon::Skill:	return TD::TowerTypeCount * TD::SkillsPerTower;
	case ETDIcon::Item:		return (int32)ETDItemSlot::Count;
	case ETDIcon::Misc:		return (int32)ETDMiscIcon::Count;
	case ETDIcon::ItemKind:	return TD::ItemKindCount() + 1;	// id = item type, 0 has no icon
	case ETDIcon::Boon:		return TD::BoonCount;
	default:				return 0;
	}
}

bool TDIcons::UseDetailed()
{
	return CVarDetailedIcons.GetValueOnAnyThread() != 0;
}

bool TDIcons::Render(ETDIcon Kind, int32 Id, int32 Size, TArray<FColor>& OutPixels, int32 Detail)
{
	if (Id < 0 || Id >= Count(Kind))
	{
		return false;
	}
	if (Kind == ETDIcon::ItemKind && !TD::ItemKind(Id))
	{
		return false;
	}
	if (Kind == ETDIcon::Skill && !TD::Skill((ETDTowerType)(Id / TD::SkillsPerTower), Id % TD::SkillsPerTower))
	{
		return false;
	}

	auto Paint = [Kind, Id](FPainter& P)
	{
		switch (Kind)
		{
		case ETDIcon::Tower:	PaintTower(P, (ETDTowerType)Id); break;
		case ETDIcon::Enemy:	PaintEnemy(P, (ETDEnemyType)Id); break;
		case ETDIcon::Skill:	PaintSkill(P, (ETDTowerType)(Id / TD::SkillsPerTower), Id % TD::SkillsPerTower); break;
		case ETDIcon::Item:		PaintItem(P, (ETDItemSlot)Id); break;
		case ETDIcon::Misc:		PaintMisc(P, (ETDMiscIcon)Id); break;
		case ETDIcon::ItemKind:	PaintItemKind(P, Id); break;
		case ETDIcon::Boon:		PaintBoon(P, Id); break;
		}
	};

	const bool bLogo = Kind == ETDIcon::Misc && Id == (int32)ETDMiscIcon::Logo;
	// small sizes are painted large and filtered down, so fine detail averages out instead of aliasing
	const bool bDetailed = Detail < 0 ? UseDetailed() : Detail > 0;
	FPainter P(FMath::Max(Size, TextureSize) * (bDetailed ? 2 : 1), bDetailed);
	P.bDropShadow = !bLogo;
	if (!bLogo)
	{
		// measure a quick draft, then scale and centre so every icon fills the tile the same way
		// the draft sees -0.5..1.5, so art painted past the tile's edge is measured (and pulled back in), not clipped
		FPainter Draft(96);
		Draft.bMeasuring = true;
		Draft.FrameS = 0.5f;
		Draft.FrameT = FV(0.25f, 0.25f);
		Paint(Draft);
		FV Min, Max;
		if (Draft.GetBounds(Min, Max))
		{
			Min = (Min - Draft.FrameT) / Draft.FrameS;
			Max = (Max - Draft.FrameT) / Draft.FrameS;
			const float Margin = 0.07f;
			const FV Extent = Max - Min;
			const float Scale = FMath::Clamp((1.0f - 2.0f * Margin) / FMath::Max(Extent.X, Extent.Y), 0.6f, 1.5f);
			P.FrameS = Scale;
			P.FrameT = FV(0.5f, 0.5f) - (Min + Max) * 0.5f * Scale;
		}
	}
	Paint(P);
	P.Finish(OutPixels, bLogo ? 0.015f : (bDetailed ? 0.021f : 0.028f), Size);
	return true;
}

UTexture2D* TDIcons::Get(ETDIcon Kind, int32 Id)
{
	const uint32 Key = CacheKey(Kind, Id);
	if (UTexture2D** Found = Cache().Find(Key))
	{
		return *Found;
	}

	PaintAllOnce();

	UTexture2D* Texture = nullptr;
	TArray<FColor> Pixels;
	const bool bPainted = Pending().RemoveAndCopyValue(Key, Pixels) || Render(Kind, Id, TextureSize, Pixels);
	if (bPainted)
	{
		const FName Name = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName(*FString::Printf(TEXT("TDIcon_%d_%d"), (int32)Kind, Id)));
		Texture = UTexture2D::CreateTransient(TextureSize, TextureSize, PF_B8G8R8A8, Name);
		if (Texture)
		{
			Texture->SRGB = true;
			Texture->Filter = TF_Trilinear;
			Texture->AddressX = TA_Clamp;
			Texture->AddressY = TA_Clamp;
			Texture->NeverStream = true;

			// full mip chain so small draws stay smooth
			FTexturePlatformData* Data = Texture->GetPlatformData();
			int32 MipSize = TextureSize;
			TArray<FColor> Mip = MoveTemp(Pixels);
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
				const int64 Bytes = (int64)Mip.Num() * sizeof(FColor);
				MipMap->BulkData.Lock(LOCK_READ_WRITE);
				void* Dest = MipMap->BulkData.Realloc(Bytes);
				FMemory::Memcpy(Dest, Mip.GetData(), Bytes);
				MipMap->BulkData.Unlock();

				if (MipSize > 4)
				{
					Mip = SharpenMip(Downsample(Mip, MipSize), MipSize / 2);
				}
				MipSize /= 2;
			}

			Texture->UpdateResource();
			Texture->AddToRoot();
		}
	}

	Cache().Add(Key, Texture);
	return Texture;
}

#if !UE_BUILD_SHIPPING

namespace
{
	/** Contact sheet of whole icon families, one family after another */
	bool WriteSheet(const FString& Path, TArrayView<const ETDIcon> Kinds, int32 Cell, int32 Columns)
	{
		int32 Rows = 0;
		for (ETDIcon Kind : Kinds)
		{
			Rows += FMath::DivideAndRoundUp(TDIcons::Count(Kind), Columns);
		}

		const int32 W = Columns * Cell;
		const int32 H = Rows * Cell;
		TArray<FColor> Sheet;
		Sheet.SetNumUninitialized(W * H);
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				// checker of two panel blues so transparency is visible
				Sheet[Y * W + X] = ((X / 8 + Y / 8) % 2) ? FColor(10, 32, 44) : FColor(16, 44, 58);
			}
		}

		int32 Row = 0;
		for (ETDIcon Kind : Kinds)
		{
			for (int32 Id = 0; Id < TDIcons::Count(Kind); ++Id)
			{
				TArray<FColor> Pixels;
				if (!TDIcons::Render(Kind, Id, Cell, Pixels))
				{
					continue;
				}
				const int32 OX = (Id % Columns) * Cell;
				const int32 OY = (Row + Id / Columns) * Cell;
				for (int32 Y = 0; Y < Cell; ++Y)
				{
					for (int32 X = 0; X < Cell; ++X)
					{
						const FColor Src = Pixels[Y * Cell + X];
						FColor& Dst = Sheet[(OY + Y) * W + OX + X];
						const float A = Src.A / 255.0f;
						Dst = FColor((uint8)(Src.R * A + Dst.R * (1 - A)), (uint8)(Src.G * A + Dst.G * (1 - A)), (uint8)(Src.B * A + Dst.B * (1 - A)), 255);
					}
				}
			}
			Row += FMath::DivideAndRoundUp(TDIcons::Count(Kind), Columns);
		}

		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(W, H, TArrayView64<const FColor>(Sheet.GetData(), Sheet.Num()), Png);
		return FFileHelper::SaveArrayToFile(Png, *Path);
	}
}

namespace
{
	/**
	 *  One size of the Windows app icon. Same painter and finish as the menu logo, but with the dark rim at least
	 *  1.25 output pixels wide (a canvas-relative rim fades out below 64 px), sub-pixel trimmings dropped at 32 px
	 *  and below, and a light sharpen after the box filter so small sizes stay as crisp as the big one.
	 */
	void RenderAppIcon(int32 OutSize, TArray<FColor>& Out)
	{
		const bool bDetailed = TDIcons::UseDetailed();
		AppIconOutSize = OutSize;
		FPainter P(FMath::Max(OutSize, TDIcons::TextureSize) * (bDetailed ? 2 : 1), bDetailed);
		P.bDropShadow = false;
		PaintMisc(P, ETDMiscIcon::Logo);
		AppIconOutSize = 0;
		P.Finish(Out, FMath::Max(0.015f, 1.25f / OutSize), OutSize);
		if (OutSize >= 128)
		{
			return;
		}

		// unsharp mask on fully opaque pixels (sRGB values are fine at this scale); the silhouette edge stays as filtered
		const float Amount = OutSize <= 16 ? 0.5f : 0.35f;
		const TArray<FColor> Src = Out;
		for (int32 Y = 1; Y + 1 < OutSize; ++Y)
		{
			for (int32 X = 1; X + 1 < OutSize; ++X)
			{
				const int32 I = Y * OutSize + X;
				const int32 N[] = { I - 1, I + 1, I - OutSize, I + OutSize };
				bool bOpaque = Src[I].A == 255;
				for (int32 J : N)
				{
					bOpaque &= Src[J].A == 255;
				}
				if (!bOpaque)
				{
					continue;
				}
				auto Sharpen = [&](uint8 FColor::* Ch)
				{
					const float Avg = (Src[N[0]].*Ch + Src[N[1]].*Ch + Src[N[2]].*Ch + Src[N[3]].*Ch) * 0.25f;
					const float V = Src[I].*Ch + (Src[I].*Ch - Avg) * Amount;
					Out[I].*Ch = (uint8)FMath::Clamp(FMath::RoundToInt(V), 0, 255);
				};
				Sharpen(&FColor::R);
				Sharpen(&FColor::G);
				Sharpen(&FColor::B);
			}
		}
	}

	/** Pairs of every icon in the families: original on the left, detailed on the right, on the HUD's panel colour */
	bool WriteCompareSheet(const FString& Path, TArrayView<const ETDIcon> Kinds, int32 Cell, int32 PairsPerRow)
	{
		struct FEntry { ETDIcon Kind; int32 Id; };
		TArray<FEntry> Entries;
		for (ETDIcon Kind : Kinds)
		{
			for (int32 Id = 0; Id < TDIcons::Count(Kind); ++Id)
			{
				const bool bMissing = (Kind == ETDIcon::ItemKind && !TD::ItemKind(Id))
					|| (Kind == ETDIcon::Skill && !TD::Skill((ETDTowerType)(Id / TD::SkillsPerTower), Id % TD::SkillsPerTower));
				if (!bMissing)
				{
					Entries.Add({ Kind, Id });
				}
			}
		}
		if (Entries.Num() == 0)
		{
			return false;
		}

		const int32 Gap = FMath::Max(4, Cell / 8);
		const int32 PairW = Cell * 2 + Gap;
		const int32 Rows = FMath::DivideAndRoundUp(Entries.Num(), PairsPerRow);
		const int32 W = PairsPerRow * PairW + (PairsPerRow + 1) * Gap;
		const int32 H = Rows * Cell + (Rows + 1) * Gap;
		TArray<FColor> Sheet;
		Sheet.Init(FColor(6, 20, 28), W * H);

		TArray<TArray<FColor>> Painted;
		Painted.SetNum(Entries.Num() * 2);
		ParallelFor(Painted.Num(), [&](int32 i)
		{
			TDIcons::Render(Entries[i / 2].Kind, Entries[i / 2].Id, Cell, Painted[i], i % 2);
		});

		for (int32 e = 0; e < Entries.Num(); ++e)
		{
			const int32 PX = Gap + (e % PairsPerRow) * (PairW + Gap);
			const int32 PY = Gap + (e / PairsPerRow) * (Cell + Gap);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const int32 OX = PX + Side * (Cell + Gap);
				const TArray<FColor>& Pixels = Painted[e * 2 + Side];
				for (int32 Y = 0; Y < Cell; ++Y)
				{
					for (int32 X = 0; X < Cell; ++X)
					{
						// the HUD's slot colour, slightly lighter for the detailed side
						FColor Back = Side ? FColor(22, 54, 70) : FColor(16, 44, 58);
						const FColor Src = Pixels[Y * Cell + X];
						const float A = Src.A / 255.0f;
						Sheet[(PY + Y) * W + OX + X] = FColor((uint8)(Src.R * A + Back.R * (1 - A)), (uint8)(Src.G * A + Back.G * (1 - A)), (uint8)(Src.B * A + Back.B * (1 - A)), 255);
					}
				}
			}
		}

		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(W, H, TArrayView64<const FColor>(Sheet.GetData(), Sheet.Num()), Png);
		return FFileHelper::SaveArrayToFile(Png, *Path);
	}
}

bool TDIcons::ExportCompare(const FString& Directory)
{
	IFileManager::Get().MakeDirectory(*Directory, true);
	const ETDIcon Towers[] = { ETDIcon::Tower };
	const ETDIcon Creatures[] = { ETDIcon::Enemy };
	const ETDIcon Skills[] = { ETDIcon::Skill };
	const ETDIcon Items[] = { ETDIcon::Item, ETDIcon::ItemKind, ETDIcon::Misc };
	const ETDIcon All[] = { ETDIcon::Tower, ETDIcon::Enemy, ETDIcon::Skill, ETDIcon::Item, ETDIcon::ItemKind, ETDIcon::Misc };
	bool bOk = WriteCompareSheet(Directory / TEXT("Compare_Towers.png"), Towers, 192, 3);
	bOk &= WriteCompareSheet(Directory / TEXT("Compare_Creatures.png"), Creatures, 192, 3);
	bOk &= WriteCompareSheet(Directory / TEXT("Compare_Skills.png"), Skills, 112, 5);
	bOk &= WriteCompareSheet(Directory / TEXT("Compare_Items.png"), Items, 128, 4);
	bOk &= WriteCompareSheet(Directory / TEXT("Compare_HUD42px.png"), All, 42, 8);
	return bOk;
}

bool TDIcons::Export(const FString& Directory)
{
	IFileManager::Get().MakeDirectory(*Directory, true);

	// contact sheets: everything at medium size, at the HUD's 42 px size, and each main family enlarged
	const ETDIcon Kinds[] = { ETDIcon::Tower, ETDIcon::Enemy, ETDIcon::Skill, ETDIcon::Item, ETDIcon::Misc, ETDIcon::ItemKind, ETDIcon::Boon };
	bool bOk = WriteSheet(Directory / TEXT("IconSheet.png"), Kinds, 96, 12);
	bOk &= WriteSheet(Directory / TEXT("IconSheet_42px.png"), Kinds, 42, 12);
	const ETDIcon Towers[] = { ETDIcon::Tower };
	bOk &= WriteSheet(Directory / TEXT("IconSheet_Towers.png"), Towers, 256, 4);
	const ETDIcon Creatures[] = { ETDIcon::Enemy, ETDIcon::Item };
	bOk &= WriteSheet(Directory / TEXT("IconSheet_Creatures.png"), Creatures, 256, 4);
	const ETDIcon Items[] = { ETDIcon::ItemKind };
	bOk &= WriteSheet(Directory / TEXT("IconSheet_Items.png"), Items, 128, 6);
	const ETDIcon Boons[] = { ETDIcon::Boon };
	bOk &= WriteSheet(Directory / TEXT("IconSheet_Boons.png"), Boons, 128, 7);

	// Windows icon: PNG-compressed entries are valid in .ico files (Vista and later)
	// every size Windows asks for at 100-200% display scaling, so it never has to resample one itself
	const int32 IconSizes[] = { 256, 64, 48, 40, 32, 24, 20, 16 };
	TArray<TArray64<uint8>> Images;
	for (int32 IconSize : IconSizes)
	{
		TArray<FColor> Pixels;
		RenderAppIcon(IconSize, Pixels);
		TArray64<uint8>& Image = Images.AddDefaulted_GetRef();
		FImageUtils::PNGCompressImageArray(IconSize, IconSize, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Image);
	}

	TArray<uint8> Ico;
	auto Put16 = [&Ico](uint16 V) { Ico.Add(V & 0xFF); Ico.Add(V >> 8); };
	auto Put32 = [&Ico](uint32 V) { for (int32 i = 0; i < 4; ++i) { Ico.Add((V >> (8 * i)) & 0xFF); } };
	Put16(0);
	Put16(1);
	Put16((uint16)Images.Num());
	uint32 Offset = 6 + 16 * Images.Num();
	for (int32 i = 0; i < Images.Num(); ++i)
	{
		Ico.Add(IconSizes[i] >= 256 ? 0 : (uint8)IconSizes[i]);
		Ico.Add(IconSizes[i] >= 256 ? 0 : (uint8)IconSizes[i]);
		Ico.Add(0);
		Ico.Add(0);
		Put16(1);
		Put16(32);
		Put32((uint32)Images[i].Num());
		Put32(Offset);
		Offset += (uint32)Images[i].Num();
	}
	for (const TArray64<uint8>& Image : Images)
	{
		Ico.Append(Image.GetData(), (int32)Image.Num());
	}
	bOk &= FFileHelper::SaveArrayToFile(Ico, *(Directory / TEXT("Application.ico")));
	return bOk;
}

#endif
