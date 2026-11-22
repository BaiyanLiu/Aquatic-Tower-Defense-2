// Hand-drawn HD tower skill icons (atd.DetailedIcons 2). Included from TDIcons.cpp inside its anonymous namespace,
// just before PaintSkill, so the shared motifs above (badges, notes, drops) and the HD creatures are all available.

	// ------------------------------------------------------------------ shared skill motifs

	/** A soft four-point twinkle with a glow */
	void HDSpark(FPainter& P, FV C, float S, const FLinearColor& Color = Hex(0xFFFFFF))
	{
		P.Glow([C, S](FV Q) { return SdCircle(Q, C, S * 0.1f); }, Color, S * 0.6f, 0.6f);
		for (int32 k = 0; k < 2; ++k)
		{
			const FV D = Spin(FV(S, 0.0f), k * 90.0f);
			// each ray tapers from the centre to a needle-sharp point
			P.Mark([C, D, S](FV Q) { return FMath::Min(SdRibbon(Q, { C, C + D }, S * 0.12f, 0.0f), SdRibbon(Q, { C, C - D }, S * 0.12f, 0.0f)); }, Color);
		}
		for (int32 k = 0; k < 2; ++k)
		{
			const FV D = Spin(FV(S * 0.45f, 0.0f), 45.0f + k * 90.0f);
			P.Mark([C, D, S](FV Q) { return FMath::Min(SdRibbon(Q, { C, C + D }, S * 0.06f, 0.0f), SdRibbon(Q, { C, C - D }, S * 0.06f, 0.0f)); }, Alpha(Color, 0.8f));
		}
		P.MCircle(C, S * 0.12f, Hex(0xFFFFFF));
	}

	/** A small fish swimming to the right (Flip mirrors it), with rayed fins, scales and a glossy eye */
	void HDFish(FPainter& P, FV C, float S, const FLinearColor& Body, const FLinearColor& Belly, float Flip = 1.0f)
	{
		FNest Place(P, C, S);
		const auto X = [Flip](float U, float V) { return FV(U * Flip, V); };
		const FLinearColor Ray = Alpha(Tone(Body, 0.5f), 0.7f);
		HDFin(P, X(-0.2f, 0.0f), { X(-0.36f, -0.14f), X(-0.32f, -0.06f), X(-0.3f, 0.0f), X(-0.32f, 0.06f), X(-0.36f, 0.14f) }, Tone(Body, 0.95f), Ray, 0.03f);
		HDFin(P, X(0.0f, -0.1f), { X(-0.1f, -0.12f), X(-0.06f, -0.2f), X(0.04f, -0.2f), X(0.08f, -0.12f) }, Tone(Body, 0.95f), Ray, 0.03f);
		const TArray<FV> Outline = Smooth({ X(-0.24f, 0.0f), X(-0.12f, -0.1f), X(0.06f, -0.13f), X(0.2f, -0.08f), X(0.27f, 0.0f), X(0.2f, 0.07f), X(0.06f, 0.11f), X(-0.12f, 0.08f) }, true, 5);
		const auto Shape = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		{
			FWith With(P);
			P.Look.Dome = 0.4f;
			P.Look.Spec = 0.7f;
			P.Fill(Shape, Body);
		}
		{
			FClip Clip(P, Shape);
			P.MEllipse(X(0.02f, 0.08f), 0.24f, 0.06f, Alpha(Belly, 0.85f), 0.04f);
			Scales(P, X(-0.24f, -0.13f) - FV(Flip < 0.0f ? 0.0f : 0.0f, 0.0f), X(0.18f, 0.11f), 0.035f, 0.0f, Alpha(Tone(Body, 0.5f), 0.3f), Hex(0xFFFFFF, 0.25f));
			P.MArc(X(0.12f, 0.0f), 0.1f, 0.008f, Flip > 0.0f ? 140.0f : -40.0f, Flip > 0.0f ? 220.0f : 40.0f, Alpha(Tone(Body, 0.5f), 0.7f));
			P.MEllipse(X(0.0f, -0.07f), 0.16f, 0.025f, Hex(0xFFFFFF, 0.35f), 0.02f);
		}
		HDFin(P, X(0.06f, 0.03f), { X(-0.02f, 0.06f), X(-0.04f, 0.1f), X(0.02f, 0.1f) }, Tone(Body, 1.15f), Ray, 0.02f);
		HDEye(P, X(0.17f, -0.025f), 0.035f, Hex(0x1A2A30), X(0.15f, 0.0f), 0.6f);
		P.MLine({ X(0.27f, 0.015f), X(0.23f, 0.025f) }, 0.006f, Hex(0x2A1010, 0.8f));
	}

	/** A thrown harpoon from Tail to Tip: grained shaft, lashed collar and a forged barbed head */
	void HDHarpoon(FPainter& P, FV Tail, FV Tip, float W)
	{
		const FV D = (Tip - Tail).GetSafeNormal(), N(-D.Y, D.X);
		const float Len = (Tip - Tail).Size();
		const FV Neck = Tip - D * W * 6.5f;
		// rope ring at the tail
		P.Fill([Tail, D, W](FV Q) { return FMath::Abs(SdCircle(Q, Tail - D * W * 1.4f, W * 1.4f)) - W * 0.45f; }, RopeColor);
		// shaft
		const auto Shaft = [Tail, Neck, W](FV Q) { return SdCapsule(Q, Tail, Neck, W); };
		P.Fill(Shaft, Wood);
		{
			FClip Clip(P, Shaft);
			for (int32 k = 0; k < 4; ++k)
			{
				P.MLine({ Tail + N * W * (-0.6f + k * 0.4f), Neck + N * W * (-0.5f + k * 0.35f) }, W * 0.08f, Alpha(WoodDark, 0.6f));
			}
			P.MLine({ Tail - N * W * 0.5f, Neck - N * W * 0.5f }, W * 0.2f, Hex(0xFFE8C0, 0.4f));
		}
		// the head: a leaf-shaped blade with two swept barbs, all one forged piece
		const auto At = [Neck, D, N, W](float U, float V) { return Neck + D * U * W + N * V * W; };
		const TArray<FV> Blade = Smooth({ At(0.0f, -1.6f), At(2.5f, -1.8f), At(6.5f, 0.0f), At(2.5f, 1.8f), At(0.0f, 1.6f) }, true, 4);
		const TArray<FV> BarbA = Smooth({ At(1.0f, -1.2f), At(-1.2f, -2.8f), At(-2.2f, -3.4f), At(-0.4f, -1.6f) }, true, 3);
		const TArray<FV> BarbB = Smooth({ At(1.0f, 1.2f), At(-1.2f, 2.8f), At(-2.2f, 3.4f), At(-0.4f, 1.6f) }, true, 3);
		const auto Head = [&Blade, &BarbA, &BarbB](FV Q) { return SMin(SMin(SdPoly(Q, Blade), SdPoly(Q, BarbA), 0.006f), SdPoly(Q, BarbB), 0.006f); };
		{
			FWith With(P);
			P.Look.Spec = 0.95f;
			P.Look.Dome = 0.02f;
			P.Fill(Head, Steel);
		}
		{
			FClip Clip(P, Head);
			P.MPoly({ At(0.0f, 0.0f), At(6.5f, 0.0f), At(2.5f, 1.8f), At(0.0f, 1.6f), At(-2.2f, 3.4f) }, Hex(0x4E5C66, 0.45f));
			P.MLine({ At(0.5f, -1.5f), At(6.0f, -0.1f) }, W * 0.22f, Hex(0xFFFFFF, 0.9f));
			P.MLine({ At(0.0f, 0.0f), At(6.2f, 0.0f) }, W * 0.12f, Hex(0x2E3A42, 0.8f));
		}
		// lashing that binds head to shaft
		const auto Lash = [Neck, D, W](FV Q) { return SdCapsule(Q, Neck - D * W * 1.6f, Neck + D * W * 0.4f, W * 1.25f); };
		P.Fill(Lash, RopeColor);
		{
			FClip Clip(P, Lash);
			for (int32 k = 0; k < 4; ++k)
			{
				const FV C = Neck - D * W * (1.4f - k * 0.55f);
				P.MLine({ C - N * W * 1.3f - D * W * 0.2f, C + N * W * 1.3f + D * W * 0.2f }, W * 0.12f, Hex(0x7E5C30, 0.8f));
			}
		}
		(void)Len;
	}

	/** A natural pebble or stone */
	void HDStone(FPainter& P, FV C, float R, int32 Seed, const FLinearColor& Color)
	{
		const auto Shape = [C, R, Seed](FV Q) { return SdEllipse(Q, C, R, R * 0.82f) + Noise2((Q - C) / R * 1.6f + FV(Seed * 2.3f, 0.0f)) * R * 0.14f; };
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.3f;
			P.Look.Grain = 0.12f;
			P.Fill(Shape, Color);
		}
		FClip Clip(P, Shape);
		Speckle(P, C - FV(R, R), C + FV(R, R), 24, R * 0.02f, R * 0.06f, Alpha(Tone(Color, 0.5f), 0.6f), Seed + 40);
		Speckle(P, C - FV(R, R), C + FV(R, R), 14, R * 0.02f, R * 0.05f, Hex(0xFFFFFF, 0.4f), Seed + 41);
		P.MEllipse(C + FV(-R * 0.3f, -R * 0.35f), R * 0.35f, R * 0.18f, Hex(0xFFFFFF, 0.35f), R * 0.15f);
	}

	/** A muscular arm along Path with suckers on the Side (+1 or -1) of the curl */
	void HDArm(FPainter& P, const TArray<FV>& Ctrl, float R0, float R1, float Side, const FLinearColor& Skin, const FLinearColor& Sucker)
	{
		const TArray<FV> Path = Smooth(Ctrl, false, 8);
		const auto Arm = [&Path, R0, R1](FV Q) { return SdRibbon(Q, Path, R0, R1); };
		{
			FWith With(P);
			P.Look.Dome = 0.6f;
			P.Look.Spec = 0.55f;
			P.Fill(Arm, Skin);
		}
		FClip Clip(P, Arm);
		for (int32 k = 1; k < 18; ++k)
		{
			FV Pos, Tan;
			const float T = k / 18.0f;
			Along(Path, T, Pos, Tan);
			const float R = FMath::Lerp(R0, R1, T);
			const FV C = Pos + FV(-Tan.Y, Tan.X) * Side * R * 0.55f;
			const float Sr = R * 0.32f;
			P.MCircle(C + FV(Sr * 0.2f, Sr * 0.25f), Sr * 1.1f, Alpha(Tone(Skin, 0.6f), 0.6f));
			P.MCircle(C, Sr, Sucker);
			P.MCircle(C + FV(Sr * 0.1f, Sr * 0.15f), Sr * 0.45f, Tone(Sucker, 0.75f));
		}
		TArray<FV> Ridge;
		for (int32 k = 0; k <= 16; ++k)
		{
			FV Pos, Tan;
			Along(Path, k / 16.0f, Pos, Tan);
			Ridge.Add(Pos - FV(-Tan.Y, Tan.X) * Side * FMath::Lerp(R0, R1, k / 16.0f) * 0.4f);
		}
		P.MLine(Ridge, R0 * 0.08f, Hex(0xFFFFFF, 0.3f));
		Speckle(P, Path[0] - FV(0.3f, 0.3f), Path[0] + FV(0.3f, 0.3f), 40, 0.002f, 0.005f, Alpha(Tone(Skin, 0.6f), 0.5f), 77);
	}

	/** A sawfish rostrum from Root to Tip with Teeth pairs */
	void HDSaw(FPainter& P, FV Root, FV Tip, float W, int32 Teeth)
	{
		const FV D = (Tip - Root).GetSafeNormal(), N(-D.Y, D.X);
		const FLinearColor Skin = Hex(0x9A9670), Dark = Hex(0x4E4E36);
		for (int32 k = 0; k < Teeth; ++k)
		{
			const float T = 0.14f + k * (0.82f / Teeth);
			const FV At = FMath::Lerp(Root, Tip, T);
			const float Wd = FMath::Lerp(W, W * 0.55f, T);
			for (const float S : { -1.0f, 1.0f })
			{
				const FV Base = At + N * S * Wd * 0.85f;
				const TArray<FV> Tooth = Smooth({ Base - D * Wd * 0.25f, Base + N * S * Wd * 0.55f - D * Wd * 0.05f, Base + N * S * Wd * 0.9f - D * Wd * 0.2f });
				{
					FWith With(P);
					P.Look.Dome = 10.0f;
					P.Look.Spec = 0.8f;
					P.Look.Ink = FMath::Min(P.Look.Ink, Wd * 0.12f);
					P.Fill([Tooth, Wd](FV Q) { return SdRibbon(Q, Tooth, Wd * 0.2f, Wd * 0.03f); }, Hex(0xF6F0E0));
				}
			}
		}
		const auto Blade = [Root, Tip, W](FV Q) { return SdRibbon(Q, { Root, Tip }, W, W * 0.55f); };
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Look.Spec = 0.4f;
			P.Fill(Blade, Skin);
		}
		FClip Clip(P, Blade);
		P.MLine({ Root, Tip }, W * 0.08f, Alpha(Dark, 0.6f));
		for (int32 k = 0; k < 30; ++k)
		{
			const FV At = FMath::Lerp(Root, Tip, Rand01(1201, k)) + N * (Rand01(1202, k) - 0.5f) * W * 1.2f;
			P.MCircle(At, W * 0.05f, Alpha(Dark, 0.6f));
		}
		P.MLine({ Root + N * W * 0.5f, Tip + N * W * 0.3f }, W * 0.12f, Hex(0xFFFFFF, 0.3f));
		Speckle(P, FV(FMath::Min(Root.X, Tip.X), FMath::Min(Root.Y, Tip.Y)) - FV(W, W), FV(FMath::Max(Root.X, Tip.X), FMath::Max(Root.Y, Tip.Y)) + FV(W, W), 60, 0.0015f, 0.004f, Alpha(Dark, 0.5f), 1203);
	}

	/** A curved tooth from Root to Tip, ivory with a darker root */
	void HDFang(FPainter& P, FV Root, FV Tip, float W, float Bend)
	{
		const FV D = Tip - Root, N = FV(-D.Y, D.X).GetSafeNormal();
		const TArray<FV> Path = Smooth({ Root, FMath::Lerp(Root, Tip, 0.5f) + N * Bend, Tip });
		const auto Fang = [Path, W](FV Q) { return SdRibbon(Q, Path, W, W * 0.05f); };
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.9f;
			P.Look.Ink = FMath::Min(P.Look.Ink, W * 0.2f);
			P.Fill(Fang, Hex(0xF8F2E2));
		}
		FClip Clip(P, Fang);
		P.MCircle(Root, W * 1.1f, Hex(0xC8A878, 0.6f));
		P.MLine(Path, W * 0.15f, Hex(0xFFFFFF, 0.6f));
		// serrations along the cutting edge
		for (int32 k = 2; k < 9; ++k)
		{
			FV Pos, Tan;
			Along(Path, k / 10.0f, Pos, Tan);
			P.MCircle(Pos + FV(-Tan.Y, Tan.X) * FMath::Lerp(W, W * 0.05f, k / 10.0f) * 0.9f, W * 0.12f, Hex(0xA89878, 0.6f));
		}
	}

	/** A metal heater shield, scratched and riveted, centred on C */
	TArray<FV> HDShieldOutline(FV C, float S)
	{
		return Smooth({ C + FV(-0.38f, -0.44f) * S, C + FV(0.0f, -0.4f) * S, C + FV(0.38f, -0.44f) * S, C + FV(0.38f, 0.0f) * S, C + FV(0.24f, 0.3f) * S, C + FV(0.0f, 0.5f) * S, C + FV(-0.24f, 0.3f) * S, C + FV(-0.38f, 0.0f) * S }, true, 5);
	}

	void HDShield(FPainter& P, FV C, float S, const FLinearColor& Color)
	{
		const TArray<FV> Outline = HDShieldOutline(C, S);
		const auto Shape = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		{
			FWith With(P);
			P.Look.Dome = 0.12f;
			P.Look.Spec = 0.8f;
			P.Fill(Shape, Color);
		}
		FClip Clip(P, Shape);
		// a raised rim, brushed metal, scratches, dents and rivets
		P.Mark([&Outline, S](FV Q) { return FMath::Abs(SdPoly(Q, Outline) + 0.05f * S) - 0.008f * S; }, Hex(0x2A343A, 0.5f));
		P.Mark([&Outline, S](FV Q) { return FMath::Abs(SdPoly(Q, Outline) + 0.04f * S) - 0.005f * S; }, Hex(0xFFFFFF, 0.3f));
		Strands(P, C + FV(0.0f, -0.42f) * S, C + FV(0.0f, 0.48f) * S, 0.7f * S, 28, 0.0012f, Hex(0xFFFFFF, 0.12f), 1211, 0.002f);
		for (int32 k = 0; k < 8; ++k)
		{
			const FV A = C + FV(Rand01(1212, k) - 0.5f, Rand01(1213, k) - 0.5f) * 0.6f * S;
			P.MLine({ A, A + Spin(FV(0.06f * S, 0.0f), Rand01(1214, k) * 180.0f) }, 0.0018f, Hex(0x2A343A, 0.5f));
		}
		for (const FV R : { FV(-0.3f, -0.36f), FV(0.3f, -0.36f), FV(-0.3f, 0.0f), FV(0.3f, 0.0f), FV(0.0f, 0.4f) })
		{
			Rivet(P, C + R * S, 0.018f * S);
		}
		P.MEllipse(C + FV(-0.12f, -0.2f) * S, 0.14f * S, 0.2f * S, Hex(0xFFFFFF, 0.18f), 0.08f * S);
	}

	/** A faceted crystal growing from Base, Len long, rotated by Rot degrees */
	void HDCrystal(FPainter& P, FV Base, float Len, float W, float Rot, const FLinearColor& Color)
	{
		FNest Place(P, Base, 1.0f, Rot);
		const TArray<FV> Body = { FV(-W, 0.0f), FV(-W, -Len * 0.72f), FV(-W * 0.2f, -Len), FV(W * 0.3f, -Len * 0.95f), FV(W, -Len * 0.7f), FV(W, 0.0f) };
		const auto Shape = [&Body](FV Q) { return SdPoly(Q, Body) - 0.004f; };
		P.Glow(Shape, Color, 0.04f, 0.4f);
		{
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Spec = 0.9f;
			P.Fill(Shape, Alpha(Color, 0.92f));
		}
		FClip Clip(P, Shape);
		// facets: a lit left face, a shaded right face, a bright ridge and inner refraction streaks
		P.MPoly({ FV(-W, 0.0f), FV(-W, -Len * 0.72f), FV(-W * 0.2f, -Len), FV(-W * 0.15f, 0.0f) }, Hex(0xFFFFFF, 0.28f));
		P.MPoly({ FV(W * 0.35f, 0.0f), FV(W * 0.3f, -Len * 0.95f), FV(W, -Len * 0.7f), FV(W, 0.0f) }, Alpha(Tone(Color, 0.4f), 0.35f));
		P.MLine({ FV(-W * 0.15f, 0.0f), FV(-W * 0.2f, -Len) }, W * 0.06f, Hex(0xFFFFFF, 0.8f));
		P.MLine({ FV(-W * 0.7f, -Len * 0.2f), FV(-W * 0.5f, -Len * 0.55f) }, W * 0.05f, Hex(0xFFFFFF, 0.5f));
		P.MLine({ FV(W * 0.5f, -Len * 0.1f), FV(W * 0.6f, -Len * 0.4f) }, W * 0.04f, Hex(0xFFFFFF, 0.35f));
		Speckle(P, FV(-W, -Len), FV(W, 0.0f), 10, W * 0.03f, W * 0.07f, Hex(0xFFFFFF, 0.6f), (int32)(Rot * 7.0f + 1300));
	}

	/** A stalked compound eye (mantis shrimp) at C */
	void HDMantisEye(FPainter& P, FV C, float S, float Rot)
	{
		FNest Place(P, C, S, Rot);
		const FLinearColor Stalk = Hex(0x26B89C);
		P.Fill([](FV Q) { return SdRibbon(Q, Smooth({ FV(0.0f, 0.42f), FV(0.03f, 0.2f), FV(0.0f, 0.0f) }), 0.05f, 0.04f); }, Stalk);
		{
			FClip Clip(P, [](FV Q) { return SdRibbon(Q, Smooth({ FV(0.0f, 0.42f), FV(0.03f, 0.2f), FV(0.0f, 0.0f) }), 0.05f, 0.04f); });
			for (int32 k = 1; k < 5; ++k)
			{
				P.MArc(FV(0.0f, 0.42f - k * 0.08f - 0.1f), 0.11f, 0.008f, 50.0f, 130.0f, Hex(0x0E5A4C, 0.7f));
			}
			P.MLine({ FV(-0.035f, 0.4f), FV(-0.02f, 0.05f) }, 0.012f, Hex(0xFFFFFF, 0.3f));
		}
		const auto Eye = [](FV Q) { return SdEllipse(Q, FV(0.0f, -0.1f), 0.15f, 0.2f); };
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.95f;
			P.Fill(Eye, Hex(0x5AD0E0));
		}
		FClip Clip(P, Eye);
		// two hemispheres split by the six-row midband, every facet a tiny lens
		P.MEllipse(FV(0.0f, -0.2f), 0.15f, 0.1f, Hex(0x8AE8F0, 0.5f), 0.04f);
		P.MEllipse(FV(0.0f, 0.02f), 0.15f, 0.08f, Hex(0x2A8AB0, 0.45f), 0.04f);
		P.Mark([](FV Q) { const FV L = Q * 90.0f; return (FMath::Abs(FMath::Sin(L.X + FMath::Sin(L.Y * 0.5f))) + FMath::Abs(FMath::Sin(L.Y)) - 0.35f) * 0.005f; }, Hex(0x1A5A7A, 0.3f));
		for (int32 k = 0; k < 6; ++k)
		{
			const float Y = -0.13f + k * 0.012f;
			const FLinearColor Band[] = { Hex(0xE84A6A), Hex(0xF0B030), Hex(0x6AD84A), Hex(0x3A8AE8), Hex(0xA05AE8), Hex(0xE84AA8) };
			P.MCapsule(FV(-0.16f, Y), FV(0.16f, Y), 0.0055f, Alpha(Band[k], 0.85f));
		}
		// three pseudopupils, one in each part of the eye
		for (const float Y : { -0.22f, -0.1f, 0.02f })
		{
			P.MEllipse(FV(0.03f, Y), 0.028f, 0.022f, Hex(0x06101A, 0.92f), 0.006f);
		}
		P.MCircle(FV(-0.06f, -0.2f), 0.028f, Hex(0xFFFFFF, 0.9f));
		P.MCircle(FV(0.08f, -0.0f), 0.012f, Hex(0xFFFFFF, 0.5f));
	}

	// ------------------------------------------------------------------ the skills

	// ------------------------------------------------------------------ round 3 skills: harpoon and jellyfish

	/** A wound where a shaft enters a body: a dark puncture with a torn pale rim */
	void HDPuncture(FPainter& P, FV C, float R, FV Dir)
	{
		const FV N(-Dir.Y, Dir.X);
		P.Mark([C, R, N](FV Q) { const FV L = Q - C; return SdEllipse(FV(L.X * N.X + L.Y * N.Y, L.X * -N.Y + L.Y * N.X), FV(0.0f, 0.0f), R, R * 0.55f); }, Hex(0x3A0E10, 0.75f), R * 0.3f);
		P.MArc(C, R * 1.05f, R * 0.12f, 200.0f, 320.0f, Hex(0xFFE8E0, 0.5f));
	}

	/** A small jellyfish with a translucent bell centred at C (S is the bell's half width), trailing tentacles */
	void HDLittleJelly(FPainter& P, FV C, float S, const FLinearColor& Color, int32 Seed, float Tilt = 0.0f, float TentLen = 1.6f)
	{
		FNest Place(P, C, S, Tilt);
		const FLinearColor Pale = Tone(Color, 1.6f), Deep = Tone(Color, 0.6f);
		// tentacles: thin, wavy, fading towards the tips
		for (int32 i = 0; i < 7; ++i)
		{
			const float X = -0.8f + i * (1.6f / 6.0f);
			TArray<FV> T;
			for (int32 k = 0; k <= 6; ++k)
			{
				const float Y = 0.18f + k * (TentLen / 6.0f);
				T.Add(FV(X * (1.0f - k * 0.05f) + FMath::Sin(i * 1.7f + k * 1.1f + Seed) * 0.12f * (k / 6.0f + 0.2f), Y));
			}
			const TArray<FV> Sm = Smooth(T);
			P.Glow([Sm](FV Q) { return SdPolyline(Q, Sm, 0.01f); }, Color, 0.05f, 0.1f);
			P.Mark([Sm](FV Q) { return SdRibbon(Q, Sm, 0.035f, 0.008f); }, Alpha(Pale, 0.7f));
			P.Mark([Sm](FV Q) { return SdRibbon(Q, Sm, 0.012f, 0.003f); }, Alpha(Tone(Color, 0.8f), 0.5f));
		}
		// frilly oral arms under the bell
		for (const float F : { -1.0f, 1.0f })
		{
			const TArray<FV> Arm = Smooth({ FV(F * 0.15f, 0.15f), FV(F * 0.25f, 0.5f), FV(F * 0.12f, 0.85f), FV(F * 0.2f, 1.15f) });
			P.Fill([Arm](FV Q) { return SdRibbon(Q, Arm, 0.12f, 0.04f) - FMath::Sin(Q.Y * 40.0f) * 0.02f; }, Alpha(Color, 0.7f));
		}
		// the bell: a dome with a scalloped rim
		const auto Bell = [](FV Q)
		{
			const float Dome = SdEllipse(Q, FV(0.0f, 0.12f), 1.0f, 0.82f);
			const float Rim = Q.Y - 0.12f - FMath::Abs(FMath::Sin(Q.X * 9.0f)) * 0.06f;
			return FMath::Max(Dome, Rim);
		};
		P.Glow(Bell, Color, 0.18f, 0.16f);
		{
			FWith With(P);
			P.Look.Dome = 0.5f;
			P.Look.Spec = 0.8f;
			P.Fill(Bell, Alpha(Color, 0.88f));
		}
		FClip Clip(P, Bell);
		// radial canals, the four-leaf gonads and a bright crown
		for (int32 k = 0; k < 9; ++k)
		{
			const float A = -160.0f + k * 17.5f;
			P.MLine({ FV(0.0f, 0.1f) + Spin(FV(0.15f, 0.0f), A), FV(0.0f, 0.12f) + Spin(FV(1.0f, 0.0f), A) }, 0.012f, Alpha(Deep, 0.4f));
		}
		for (int32 k = 0; k < 4; ++k)
		{
			const FV L = FV(0.0f, -0.12f) + Spin(FV(0.22f, 0.0f), 45.0f + k * 90.0f) * FV(1.0f, 0.6f);
			P.MEllipse(L, 0.16f, 0.1f, Alpha(Pale, 0.55f), 0.05f);
		}
		P.Mark([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.0f, 0.12f), 0.9f, 0.72f)) - 0.015f; }, Alpha(Pale, 0.5f));
		P.MEllipse(FV(-0.35f, -0.4f), 0.32f, 0.16f, Hex(0xFFFFFF, 0.5f), 0.15f);
		Speckle(P, FV(-1.0f, -0.7f), FV(1.0f, 0.12f), 18, 0.012f, 0.025f, Hex(0xFFFFFF, 0.5f), Seed);
	}

	void HDSkPiercingBolt(FPainter& P)
	{
		// one harpoon driven clean through two fish, its wake still churning behind it
		const FV Tail(0.12f, 0.8f), Tip(0.9f, 0.18f);
		const FV D = (Tip - Tail).GetSafeNormal(), N(-D.Y, D.X);
		// the wake: streaming water and bubbles along the line of flight
		for (int32 k = 0; k < 9; ++k)
		{
			const FV At = Tail + D * (0.02f + k * 0.035f) + N * (Rand01(3001, k) - 0.5f) * 0.07f;
			Bubble(P, At, 0.006f + Rand01(3004, k) * 0.01f);
		}
		HDHarpoon(P, Tail, Tip, 0.017f);
		// the far fish (orange) and the near fish (blue-grey), both skewered
		const FV F1 = Tail + D * 0.63f, F0 = Tail + D * 0.33f;
		HDFish(P, F1 + FV(0.0f, 0.005f), 0.7f, Hex(0xE8844A), Hex(0xFFE0B8), -1.0f);
		HDPuncture(P, F1 + FV(-0.035f, 0.03f), 0.018f, D);
		HDFish(P, F0 + FV(0.0f, 0.005f), 0.74f, Hex(0x5A88A8), Hex(0xD8ECF4), -1.0f);
		HDPuncture(P, F0 + FV(-0.037f, 0.032f), 0.019f, D);
		// scales knocked loose, drifting
		for (int32 k = 0; k < 6; ++k)
		{
			const FV At = F0 + FV((Rand01(3002, k) - 0.3f) * 0.3f, (Rand01(3003, k) - 0.5f) * 0.3f);
			P.MEllipse(At, 0.009f, 0.006f, Hex(0xD8ECF4, 0.7f), 0.002f);
		}
	}

	void HDSkPoisonedTip(FPainter& P)
	{
		// a harpoon lifted dripping from a clay jar of green venom
		const FLinearColor Clay = Hex(0xB8683A), ClayDark = Hex(0x5A2A14), Venom = Hex(0x74D84A);
		const FV Jar(0.36f, 0.7f);
		// the jar: round belly, a short neck, a rolled lip
		const auto Pot = [Jar](FV Q)
		{
			const float Belly = SdEllipse(Q, Jar, 0.24f, 0.2f);
			const float Neck = SdBox(Q, Jar + FV(-0.1f, -0.27f), Jar + FV(0.1f, -0.1f), 0.02f);
			return SMin(Belly, Neck, 0.06f);
		};
		P.Mark([&Pot](FV Q) { return Pot(Q - FV(0.012f, 0.02f)); }, Hex(0x06121A, 0.35f), 0.03f);
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.35f;
			P.Look.Grain = 0.3f;
			P.Fill(Pot, Clay);
		}
		{
			FClip Clip(P, Pot);
			// a painted band of waves, a crack, a venom stain running down
			P.Mark([Jar](FV Q) { return FMath::Abs(Q.Y - Jar.Y + 0.02f + FMath::Sin(Q.X * 60.0f) * 0.012f) - 0.006f; }, Hex(0x2A1A10, 0.7f));
			P.Mark([Jar](FV Q) { return FMath::Abs(Q.Y - Jar.Y - 0.04f) - 0.004f; }, Hex(0x2A1A10, 0.6f));
			P.MLine({ Jar + FV(0.12f, 0.06f), Jar + FV(0.15f, 0.1f), Jar + FV(0.13f, 0.14f), Jar + FV(0.16f, 0.18f) }, 0.003f, Alpha(ClayDark, 0.9f));
			P.Mark([Jar](FV Q) { return SdRibbon(Q, Smooth({ Jar + FV(-0.06f, -0.26f), Jar + FV(-0.07f, -0.15f), Jar + FV(-0.05f, -0.04f) }), 0.018f, 0.006f); }, Alpha(Venom, 0.7f));
			P.MEllipse(Jar + FV(-0.1f, -0.06f), 0.05f, 0.1f, Hex(0xFFE0C0, 0.25f), 0.05f);
		}
		// the rolled lip and the venom brimming inside it
		P.Fill([Jar](FV Q) { return FMath::Abs(SdEllipse(Q, Jar + FV(0.0f, -0.27f), 0.12f, 0.035f)) - 0.012f; }, Clay);
		P.Fill([Jar](FV Q) { return SdEllipse(Q, Jar + FV(0.0f, -0.27f), 0.105f, 0.025f); }, Tone(Venom, 0.7f));
		P.MEllipse(Jar + FV(-0.03f, -0.276f), 0.05f, 0.008f, Hex(0xE8FFC8, 0.6f), 0.004f);
		// the harpoon, its head slick with venom
		const FV Tail(0.94f, 0.1f), Tip(0.42f, 0.36f);
		const FV D = (Tip - Tail).GetSafeNormal(), N(-D.Y, D.X);
		HDHarpoon(P, Tail, Tip, 0.015f);
		const FV Neck = Tip - D * 0.015f * 6.5f;
		const auto Coat = [Neck, Tip, D, N](FV Q)
		{
			const FV L = Q - Neck;
			const float U = L.X * D.X + L.Y * D.Y, V = L.X * N.X + L.Y * N.Y;
			const float Half = FMath::Max(0.0f, 0.032f * FMath::Sin(FMath::Clamp(U / 0.1f, 0.0f, 1.0f) * PI * 0.9f + 0.3f));
			return FMath::Max(FMath::Abs(V) - Half - Noise2(Q * 60.0f) * 0.004f, FMath::Max(-U - 0.01f, U - 0.1f));
		};
		P.Mark(Coat, Alpha(Venom, 0.55f));
		P.Mark([Coat](FV Q) { return FMath::Abs(Coat(Q)) - 0.002f; }, Alpha(Tone(Venom, 0.6f), 0.6f));
		// venom running off the point and swelling into a drop about to fall into the jar
		HDDrip(P, Tip + FV(-0.004f, -0.006f), 0.1f, 0.026f, Venom, 0.0f, 0.009f);
		// venom bubbles rising off the brim
		for (int32 k = 0; k < 5; ++k)
		{
			const FV B = Jar + FV(-0.08f + Rand01(3022, k) * 0.12f, -0.31f - Rand01(3023, k) * 0.1f);
			P.Ball(B, 0.007f + Rand01(3024, k) * 0.008f, Alpha(Hex(0xA8F07A), 0.85f), 0.9f);
		}
		Speckle(P, Tip - FV(0.04f, 0.04f), Tip + FV(0.06f, 0.02f), 6, 0.004f, 0.007f, Hex(0xE8FFC8, 0.9f), 3021);
	}

	void HDSkDeepSting(FPainter& P)
	{
		// a jellyfish's stinging tentacles wound round a fish, which hangs numb and slowed in their grip
		const FLinearColor Pink = Hex(0xF08CD8), Pale = Hex(0xFFD8F4);
		const TArray<TArray<FV>> Tents = {
			Smooth({ FV(0.3f, 0.24f), FV(0.27f, 0.42f), FV(0.33f, 0.58f), FV(0.28f, 0.8f), FV(0.32f, 0.94f) }),
			Smooth({ FV(0.44f, 0.26f), FV(0.42f, 0.4f), FV(0.47f, 0.52f), FV(0.42f, 0.74f), FV(0.46f, 0.92f) }),
			Smooth({ FV(0.58f, 0.26f), FV(0.62f, 0.42f), FV(0.56f, 0.6f), FV(0.62f, 0.78f), FV(0.58f, 0.95f) }),
			Smooth({ FV(0.72f, 0.24f), FV(0.74f, 0.44f), FV(0.7f, 0.62f), FV(0.76f, 0.84f) }) };
		// the tentacles behind the fish
		for (const TArray<FV>& T : Tents)
		{
			P.Glow([&T](FV Q) { return SdPolyline(Q, T, 0.003f); }, Pink, 0.02f, 0.25f);
			P.Mark([&T](FV Q) { return SdRibbon(Q, T, 0.009f, 0.003f) - FMath::Sin(Q.Y * 140.0f) * 0.0015f; }, Alpha(Pale, 0.7f));
		}
		// the fish, tipped over and drifting, fins limp, frosted with a numb pallor
		HDFish(P, FV(0.5f, 0.62f), 0.86f, Hex(0x7AA0B4), Hex(0xE4F0F4), -1.0f);
		P.Mark([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.62f), 0.17f, 0.08f); }, Hex(0xC8E8FF, 0.18f), 0.04f);
		// its eye drooping half shut
		P.Mark([](FV Q) { return FMath::Max(SdCircle(Q, FV(0.354f, 0.598f), 0.032f), Q.Y - 0.596f); }, Hex(0x5A7A8C));
		P.MLine({ FV(0.324f, 0.596f), FV(0.384f, 0.596f) }, 0.0025f, Hex(0x2A3A44));
		// tentacles coiled over the body: each loop wraps across the front
		for (int32 k = 0; k < 3; ++k)
		{
			const float X = 0.42f + k * 0.1f;
			const TArray<FV> Loop = Smooth({ FV(X - 0.05f, 0.52f), FV(X + 0.01f, 0.6f), FV(X - 0.01f, 0.67f), FV(X + 0.04f, 0.73f) });
			P.Mark([Loop](FV Q) { return SdPolyline(Q, Loop, 0.009f); }, Alpha(Tone(Pink, 0.6f), 0.5f));
			P.Mark([Loop](FV Q) { return SdRibbon(Q, Loop, 0.007f, 0.006f); }, Alpha(Pale, 0.95f));
			// stinging cells bristling along the coil
			for (int32 j = 0; j < 6; ++j)
			{
				FV Pos, Tan;
				Along(Loop, (j + 0.5f) / 6.0f, Pos, Tan);
				P.MCircle(Pos + FV(-Tan.Y, Tan.X) * 0.006f, 0.0035f, Hex(0xFF4AA8));
			}
		}
		// welts where it has been stung
		for (const FV W : { FV(0.46f, 0.66f), FV(0.56f, 0.58f), FV(0.62f, 0.66f) })
		{
			P.MEllipse(W, 0.018f, 0.01f, Hex(0xE84A6A, 0.35f), 0.008f);
		}
		// the jelly's bell rim above, glowing
		HDLittleJelly(P, FV(0.5f, 0.15f), 0.27f, Pink, 3031, 0.0f, 0.2f);
		// tiny threads fired from the stinging cells
		for (int32 k = 0; k < 7; ++k)
		{
			const FV A = FV(0.38f + Rand01(3032, k) * 0.28f, 0.5f + Rand01(3033, k) * 0.2f);
			P.MLine(Smooth({ A, A + FV(0.01f, -0.012f), A + FV(0.022f, -0.006f) }), 0.0015f, Hex(0xFFFFFF, 0.7f));
		}
	}

	void HDSkWideBloom(FPainter& P)
	{
		// a bloom of jellyfish spreading out through the water around a large mother jelly
		const FLinearColor Pink = Hex(0xF08CD8);
		const FLinearColor Cols[] = { Hex(0xF08CD8), Hex(0xC890F0), Hex(0xFFA8C8), Hex(0xB0A0F8), Hex(0xF0A0E8), Hex(0xE890D0) };
		// the spreading ring, the farthest ones smallest and dimmest
		for (int32 k = 0; k < 7; ++k)
		{
			const float A = -100.0f + k * 51.0f + Rand01(3051, k) * 14.0f;
			const float R = 0.32f + Rand01(3052, k) * 0.06f;
			const FV At = FV(0.5f, 0.5f) + Spin(FV(R, 0.0f), A) * FV(1.0f, 0.82f);
			HDLittleJelly(P, At, 0.045f + Rand01(3053, k) * 0.018f, Cols[k % 6], 3054 + k, (Rand01(3055, k) - 0.5f) * 40.0f, 1.3f);
		}
		// the big one at the centre
		HDLittleJelly(P, FV(0.5f, 0.42f), 0.15f, Pink, 3056, 0.0f, 1.7f);
		Bubble(P, FV(0.22f, 0.2f), 0.02f);
		Bubble(P, FV(0.82f, 0.74f), 0.016f);
		Bubble(P, FV(0.86f, 0.66f), 0.01f);
	}

	// ------------------------------------------------------------------ round 3 skills: puffer and eel

	/** A single long puffer spine from Base to Tip: cream, banded brown, needle-sharp */
	void HDPufferSpine(FPainter& P, FV Base, FV Tip, float W)
	{
		const auto Spine = [Base, Tip, W](FV Q) { return SdRibbon(Q, { Base, Tip }, W, W * 0.04f); };
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.8f;
			P.Look.Ink = FMath::Min(P.Look.Ink, W * 0.3f);
			P.Fill(Spine, Hex(0xF0E2B8));
		}
		FClip Clip(P, Spine);
		const FV D = (Tip - Base);
		P.MCapsule(Base, Base + D * 0.3f, W * 1.2f, Hex(0xB8861A, 0.55f));
		P.MCapsule(Base + D * 0.5f, Base + D * 0.58f, W * 1.2f, Hex(0x8A5A10, 0.45f));
		const FV N = FV(-D.Y, D.X).GetSafeNormal();
		P.MLine({ Base - N * W * 0.4f, Tip - N * W * 0.02f }, W * 0.15f, Hex(0xFFFFFF, 0.6f));
	}

	/** A small fish knocked senseless: tilted, a spiral where its eye should be */
	void HDDizzyEye(FPainter& P, FV C, float R)
	{
		P.Ball(C, R, Hex(0xF8F4EC), 0.4f);
		TArray<FV> Spiral;
		for (int32 k = 0; k <= 40; ++k)
		{
			const float T = k / 40.0f;
			Spiral.Add(C + Spin(FV(R * 0.85f * T, 0.0f), T * 720.0f));
		}
		P.MLine(Spiral, R * 0.1f, Hex(0x1A2A30));
	}

	void HDSkBigSpines(FPainter& P)
	{
		// the puffer blown up as far as it will go, every spine standing out twice as long
		{
			FNest Place(P, FV(-0.1f, 0.06f), 1.2f);
			HDPufferBody(P, 2.1f);
		}
		Bubble(P, FV(0.13f, 0.2f), 0.024f);
		Bubble(P, FV(0.88f, 0.78f), 0.02f);
		Bubble(P, FV(0.9f, 0.68f), 0.012f);
	}

	void HDSkConcussion(FPainter& P)
	{
		// a fish knocked senseless by a puffer spine: tipped over, eye spinning, a lump rising on its head
		{
			FNest Tilt(P, FV(0.5f, 0.56f), 1.0f, 24.0f);
			HDFish(P, FV(0.0f, 0.0f), 0.95f, Hex(0x5AA8A0), Hex(0xE4F4E8), -1.0f);
			// the lump on its head, and a crack in the scales
			P.Ball(FV(-0.12f, -0.1f), 0.035f, Hex(0x7AC0B8), 0.7f);
			P.MEllipse(FV(-0.125f, -0.112f), 0.016f, 0.009f, Hex(0xFFFFFF, 0.6f), 0.004f);
			P.MLine({ FV(-0.06f, -0.09f), FV(-0.05f, -0.06f), FV(-0.065f, -0.04f), FV(-0.055f, -0.015f) }, 0.003f, Hex(0x1A3A38, 0.8f));
			// mouth agape
			P.MEllipse(FV(-0.25f, 0.02f), 0.014f, 0.01f, Hex(0x2A1010));
		}
		// the eye spinning (placed in world space)
		const FV Eye = FV(0.5f, 0.56f) + Spin(FV(-0.17f * 0.95f, -0.025f * 0.95f), 24.0f);
		HDDizzyEye(P, Eye, 0.034f);
		// the spine that did it, bounced away
		HDPufferSpine(P, FV(0.2f, 0.3f), FV(0.06f, 0.14f), 0.016f);
		// bubbles knocked loose spiralling round its head
		for (int32 k = 0; k < 7; ++k)
		{
			const float A = FMath::DegreesToRadians(k * 51.0f);
			const FV At = FV(0.4f, 0.32f) + FV(FMath::Cos(A) * 0.17f, FMath::Sin(A) * 0.05f);
			Bubble(P, At, 0.012f + 0.008f * (0.5f + 0.5f * FMath::Sin(A)));
		}
		// a puff of stirred water at the point of impact
		P.MEllipse(FV(0.32f, 0.42f), 0.05f, 0.03f, Hex(0xBFEFFF, 0.25f), 0.03f);
	}

	void HDSkOvercharge(FPainter& P)
	{
		// a single arc leaping from fish to fish to fish, each one lit up and jolted
		const FV A(0.18f, 0.24f), B(0.52f, 0.6f), C(0.82f, 0.3f), D(0.76f, 0.82f);
		HDLightning(P, A, B, 3201, 0.007f);
		HDLightning(P, B, C, 3202, 0.007f);
		HDLightning(P, B, D, 3203, 0.005f);
		const FV Fish[] = { A, B, C, D };
		const FLinearColor Body[] = { Hex(0xE8844A), Hex(0x5A88A8), Hex(0x9A7AC0), Hex(0x6AAA70) };
		for (int32 k = 0; k < 4; ++k)
		{
			const float S = k == 1 ? 0.72f : 0.58f;
			P.Glow([F = Fish[k], S](FV Q) { return SdEllipse(Q, F, S * 0.24f, S * 0.12f); }, Hex(0xFFE870), 0.05f, 0.7f);
			HDFish(P, Fish[k], S, Body[k], Hex(0xFFF8D8), k % 2 ? 1.0f : -1.0f);
			// crackle round the outline
			P.Mark([F = Fish[k], S, k](FV Q) { const float D0 = SdEllipse(Q, F, S * 0.25f, S * 0.13f); return FMath::Abs(D0) - 0.002f + FMath::Max(0.0f, FMath::Sin(FMath::Atan2(Q.Y - F.Y, Q.X - F.X) * 9.0f + k) * 0.01f); }, Hex(0xFFFFFF, 0.8f));
		}
	}

	void HDSkParalyzingShock(FPainter& P)
	{
		// a fish locked rigid mid-swim, the shock lighting it up so its skeleton shows through
		const FV C(0.5f, 0.52f);
		const float S = 1.15f;
		P.Glow([C, S](FV Q) { return SdEllipse(Q, C, S * 0.25f, S * 0.12f); }, Hex(0xFFE870), 0.1f, 0.7f);
		HDFish(P, C, S, Hex(0x8AA0B0), Hex(0xFFF8D8), 1.0f);
		// the x-ray glow over the body, and the bones beneath
		const auto Body = [C, S](FV Q) { return SdEllipse(Q, C + FV(0.01f, 0.0f) * S, 0.25f * S, 0.115f * S); };
		{
			FClip Clip(P, Body);
			P.Mark(Body, Hex(0xFFE870, 0.45f));
			P.MLine(Smooth({ C + FV(-0.24f, 0.0f) * S, C + FV(0.0f, -0.01f) * S, C + FV(0.14f, -0.005f) * S }), 0.006f * S, Hex(0xFFFFF0));
			for (int32 k = 0; k < 9; ++k)
			{
				const float X = -0.17f + k * 0.035f;
				const float H = 0.085f * FMath::Sin((k + 1) / 10.0f * PI);
				P.MLine(Smooth({ C + FV(X, -0.008f) * S, C + FV(X - 0.015f, -H * 0.6f) * S, C + FV(X - 0.03f, -H) * S }), 0.0035f * S, Hex(0xFFFFF0, 0.95f));
				P.MLine(Smooth({ C + FV(X, -0.004f) * S, C + FV(X - 0.015f, H * 0.6f) * S, C + FV(X - 0.03f, H) * S }), 0.0035f * S, Hex(0xFFFFF0, 0.95f));
			}
			// the skull
			P.Mark([C, S](FV Q) { return FMath::Abs(SdEllipse(Q, C + FV(0.18f, -0.005f) * S, 0.06f * S, 0.06f * S)) - 0.004f * S; }, Hex(0xFFFFF0, 0.95f));
		}
		HDEye(P, C + FV(0.17f, -0.025f) * S, 0.04f * S, Hex(0x1A2A30), FV(0.0f, 0.0f), 0.3f);
		// arcs crawling over the body and leaping off the fins
		HDLightning(P, C + FV(-0.4f, -0.26f), C + FV(-0.12f, -0.06f), 3211, 0.005f);
		HDLightning(P, C + FV(0.36f, 0.3f), C + FV(0.12f, 0.08f), 3212, 0.005f);
		HDLightning(P, C + FV(0.08f, -0.34f), C + FV(0.02f, -0.12f), 3213, 0.004f);
		HDLightning(P, C + FV(-0.34f, 0.28f), C + FV(-0.18f, 0.08f), 3214, 0.004f);
		for (int32 k = 0; k < 10; ++k)
		{
			const FV At = C + FV((Rand01(3215, k) - 0.5f) * 0.8f, (Rand01(3216, k) - 0.5f) * 0.6f);
			P.MCircle(At, 0.004f + Rand01(3217, k) * 0.004f, Hex(0xFFF6C0, 0.9f));
		}
	}

	// ------------------------------------------------------------------ round 3 skills: mantis shrimp and octopus

	/** A small round sea snail shell at C, R across, spiral sutures and a lip on the left */
	void HDSnailShell(FPainter& P, FV C, float R, const FLinearColor& Color, int32 Seed)
	{
		const auto Shell = [C, R](FV Q) { return SdEllipse(Q, C, R, R * 0.9f); };
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.6f;
			P.Fill(Shell, Color);
		}
		FClip Clip(P, Shell);
		TArray<FV> Spiral;
		for (int32 k = 0; k <= 80; ++k)
		{
			const float T = k / 80.0f;
			Spiral.Add(C + FV(R * 0.2f, -R * 0.15f) + Spin(FV(R * 0.95f * T, 0.0f), T * 900.0f) * FV(1.0f, 0.9f));
		}
		P.MLine(Spiral, R * 0.04f, Alpha(Tone(Color, 0.45f), 0.8f));
		for (int32 k = 0; k < 18; ++k)
		{
			const float A = k * 20.0f;
			P.MLine({ C + Spin(FV(R * 0.5f, 0.0f), A), C + Spin(FV(R, 0.0f), A + 10.0f) }, R * 0.012f, Alpha(Tone(Color, 0.6f), 0.4f));
		}
		Speckle(P, C - FV(R, R), C + FV(R, R), 20, R * 0.02f, R * 0.05f, Alpha(Tone(Color, 0.5f), 0.6f), Seed);
		P.MEllipse(C + FV(-R * 0.3f, -R * 0.4f), R * 0.35f, R * 0.18f, Hex(0xFFFFFF, 0.4f), R * 0.15f);
	}

	/** A jagged knapped flint: dark, rippled with conchoidal fractures, bright along its sharp edges */
	void HDFlint(FPainter& P, const TArray<FV>& Outline, FV Strike, const FLinearColor& Color)
	{
		const auto Shape = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		P.Mark([&Shape](FV Q) { return Shape(Q - FV(0.01f, 0.018f)); }, Hex(0x06121A, 0.35f), 0.025f);
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Spec = 0.9f;
			P.Fill(Shape, Color);
		}
		FClip Clip(P, Shape);
		// shell-like ripples spreading from each strike point, facets catching the light
		for (int32 k = 1; k < 9; ++k)
		{
			P.Mark([Strike, k](FV Q) { return FMath::Abs((Q - Strike).Size() - k * 0.03f + Noise2(Q * 20.0f) * 0.006f) - 0.0025f; }, Alpha(Tone(Color, 1.8f), 0.35f));
		}
		for (int32 i = 0; i < Outline.Num(); i += 3)
		{
			const FV A = Outline[i], B = Outline[(i + 3) % Outline.Num()];
			const FV M = (A + B) * 0.5f;
			P.MPoly({ A, B, FMath::Lerp(M, Strike, 0.35f) }, i % 2 ? Hex(0xFFFFFF, 0.12f) : Hex(0x000000, 0.15f));
		}
		P.Mark([&Outline](FV Q) { return FMath::Abs(SdPoly(Q, Outline) + 0.004f) - 0.0025f; }, Hex(0xFFFFFF, 0.55f));
		Speckle(P, Strike - FV(0.3f, 0.3f), Strike + FV(0.3f, 0.3f), 30, 0.002f, 0.004f, Hex(0xFFFFFF, 0.3f), 3301);
	}

	void HDSkKeenEyes(FPainter& P)
	{
		// the mantis shrimp's face: two great stalked eyes swivelled forward, antennae and their orange scales spread
		const FLinearColor Teal = Hex(0x26B89C), TealDeep = Hex(0x0E5A4C), Orange = Hex(0xF4962A);
		// antennae and their paddle scales
		for (const float F : { -1.0f, 1.0f })
		{
			const TArray<FV> Ant = Smooth({ FV(0.5f + F * 0.06f, 0.66f), FV(0.5f + F * 0.24f, 0.5f), FV(0.5f + F * 0.42f, 0.36f), FV(0.5f + F * 0.48f, 0.12f) });
			P.Mark([Ant](FV Q) { return SdRibbon(Q, Ant, 0.007f, 0.002f); }, Hex(0xE8463A));
			for (int32 k = 1; k < 10; ++k)
			{
				FV Pos, Tan;
				Along(Ant, k / 10.0f, Pos, Tan);
				P.MCircle(Pos, 0.004f, Hex(0x8A1A10, 0.7f));
			}
		}
		// the head: a smooth teal carapace with a pointed rostral plate between the eyes
		const auto Head = [](FV Q) { return SMin(SdEllipse(Q, FV(0.5f, 0.82f), 0.22f, 0.13f), SdPoly(Q, { FV(0.44f, 0.72f), FV(0.5f, 0.56f), FV(0.56f, 0.72f) }), 0.03f); };
		{
			FWith With(P);
			P.Look.Dome = 0.1f;
			P.Look.Spec = 0.7f;
			P.Fill(Head, Teal);
		}
		{
			FClip Clip(P, Head);
			Speckle(P, FV(0.28f, 0.6f), FV(0.72f, 0.96f), 70, 0.003f, 0.008f, Alpha(Hex(0x7ADA5A), 0.6f), 3501);
			P.MLine({ FV(0.5f, 0.6f), FV(0.5f, 0.92f) }, 0.004f, Alpha(TealDeep, 0.6f));
			P.MEllipse(FV(0.44f, 0.76f), 0.08f, 0.03f, Hex(0xFFFFFF, 0.3f), 0.03f);
		}
		// the eyes on their stalks, angled in towards what they have spotted
		HDMantisEye(P, FV(0.37f, 0.48f), 0.75f, -14.0f);
		HDMantisEye(P, FV(0.63f, 0.48f), 0.75f, 14.0f);
		Glint(P, FV(0.32f, 0.24f), 0.1f);
	}

	void HDSkStrongThrow(FPainter& P)
	{
		// a stone hurled in a long, high arc, its wake trailing all the way back to the arm that flung it
		const FLinearColor Skin = Hex(0xEE6A44), Sucker = Hex(0xFFE0CC);
		TArray<FV> Arc;
		for (int32 k = 0; k <= 24; ++k)
		{
			const float T = k / 24.0f;
			Arc.Add(FV(0.2f + T * 0.62f, 0.62f - FMath::Sin(T * PI * 0.8f) * 0.5f + T * 0.08f));
		}
		// the wake: a widening, fading ribbon of churned water
		P.Mark([&Arc](FV Q) { return SdRibbon(Q, Arc, 0.004f, 0.03f); }, Hex(0xBFEFFF, 0.18f), 0.02f);
		P.Mark([&Arc](FV Q) { return SdRibbon(Q, Arc, 0.001f, 0.01f); }, Hex(0xE8FCFF, 0.4f));
		for (int32 k = 0; k < 14; ++k)
		{
			FV Pos, Tan;
			Along(Arc, 0.12f + k * 0.055f, Pos, Tan);
			Bubble(P, Pos + FV(-Tan.Y, Tan.X) * (Rand01(3801, k) - 0.5f) * 0.05f, 0.005f + k * 0.0012f);
		}
		// sand far below
		// the throwing arm, just released, its tip still flicked forward
		HDArm(P, { FV(0.04f, 0.98f), FV(0.1f, 0.84f), FV(0.18f, 0.72f), FV(0.22f, 0.62f), FV(0.27f, 0.56f) }, 0.07f, 0.014f, 1.0f, Skin, Sucker);
		// the stone in flight, spinning, at the end of the arc
		HDStone(P, Arc.Last() + FV(0.02f, 0.0f), 0.08f, 3803, Hex(0x8A8A80));
		P.MArc(Arc.Last() + FV(0.02f, 0.0f), 0.1f, 0.004f, 200.0f, 260.0f, Hex(0xFFFFFF, 0.5f));
	}

	// ------------------------------------------------------------------ round 3 skills: lionfish and sawfish

	/** A soft, curling wisp of colour hanging in the water (blood, venom, silt) along Path */
	void HDWisp(FPainter& P, const TArray<FV>& Ctrl, float W, const FLinearColor& Color, int32 Seed)
	{
		const TArray<FV> Path = Smooth(Ctrl, false, 8);
		P.Mark([&Path, W, Seed](FV Q) { return SdRibbon(Q, Path, W * 0.4f, W) + Noise2(Q * 18.0f + FV(Seed * 1.3f, 0.0f)) * W * 0.8f; }, Alpha(Color, Color.A * 0.22f), W * 1.2f);
		P.Mark([&Path, W, Seed](FV Q) { return SdRibbon(Q, Path, W * 0.2f, W * 0.45f) + Noise2(Q * 30.0f + FV(0.0f, Seed * 0.7f)) * W * 0.5f; }, Alpha(Color, Color.A * 0.3f), W * 0.8f);
	}

	/** A branching tree of coral from Root, Height tall */
	void HDBranchCoral(FPainter& P, FV Root, float Height, float W, const FLinearColor& Color, int32 Seed)
	{
		// every twig tapers smoothly from the width of its parent's tip to a slimmer tip of its own
		TArray<TArray<FV>> Twigs;
		TArray<FV> Widths;
		struct FT { FV A; float Ang; float Len; float W; int32 Depth; };
		TArray<FT> Todo = { { Root, -90.0f, Height * 0.42f, W, 0 } };
		int32 N = 0;
		while (Todo.Num() > 0)
		{
			const FT T = Todo.Last();
			Todo.RemoveAt(Todo.Num() - 1);
			const FV B = T.A + Spin(FV(T.Len, 0.0f), T.Ang);
			const FV M = (T.A + B) * 0.5f + Spin(FV(0.0f, T.Len * 0.12f * (Rand01(Seed, N) - 0.5f)), T.Ang);
			Twigs.Add(Smooth({ T.A, M, B }));
			const float WEnd = T.Depth < 3 ? T.W * 0.8f : T.W * 0.55f;
			Widths.Add(FV(T.W, WEnd));
			++N;
			if (T.Depth < 3)
			{
				for (const float S : { -1.0f, 1.0f })
				{
					Todo.Add({ B, T.Ang + S * (22.0f + Rand01(Seed + 1, N) * 18.0f), T.Len * 0.74f, WEnd, T.Depth + 1 });
					++N;
				}
			}
		}
		const auto Coral = [&Twigs, &Widths, W](FV Q)
		{
			float D = 1e9f;
			for (int32 i = 0; i < Twigs.Num(); ++i)
			{
				D = SMin(D, SdRibbon(Q, Twigs[i], Widths[i].X, Widths[i].Y), W * 0.3f);
			}
			return D;
		};
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.4f;
			P.Look.Grain = 0.3f;
			P.Fill(Coral, Color);
		}
		FClip Clip(P, Coral);
		Pits(P, Root - FV(Height * 0.6f, Height), Root + FV(Height * 0.6f, 0.0f), 60, W * 0.12f, Alpha(Tone(Color, 0.5f), 0.7f), Seed + 3);
	}

	void HDSkLingeringVenom(FPainter& P)
	{
		// an hourglass of turned driftwood whose venom sand trickles on and on, its green haze seeping out and hanging in the water
		const FV C(0.5f, 0.48f);
		const float S = 1.05f;
		// the lingering haze behind and around the base
		HDWisp(P, { FV(0.22f, 0.86f), FV(0.12f, 0.74f), FV(0.16f, 0.6f), FV(0.08f, 0.5f) }, 0.05f, Hex(0x74D84A, 0.8f), 1);
		HDWisp(P, { FV(0.78f, 0.86f), FV(0.9f, 0.72f), FV(0.84f, 0.58f), FV(0.92f, 0.44f), FV(0.86f, 0.32f) }, 0.055f, Hex(0x74D84A, 0.8f), 2);
		HDWisp(P, { FV(0.3f, 0.92f), FV(0.5f, 0.96f), FV(0.7f, 0.92f) }, 0.05f, Hex(0x9AF07A, 0.8f), 3);
		const auto Glass = [C, S](FV Q)
		{
			const FV L = (Q - C) / S;
			const float Waist = 0.035f + 0.17f * FMath::Pow(FMath::Clamp(FMath::Abs(L.Y) / 0.3f, 0.0f, 1.0f), 0.7f) * FMath::Sqrt(FMath::Max(0.0f, 1.0f - FMath::Pow(FMath::Abs(L.Y) / 0.34f, 6.0f)));
			return (FMath::Max(FMath::Abs(L.X) - Waist, FMath::Abs(L.Y) - 0.33f)) * S;
		};
		{
			FClip Clip(P, Glass);
			P.Mark([C, S](FV Q) { const FV L = (Q - C) / S; return (0.15f + L.X * L.X * 1.6f - L.Y) * S; }, Hex(0x5AB830));
			P.Mark([C, S](FV Q) { const FV L = (Q - C) / S; return (0.17f + L.X * L.X * 1.6f - L.Y) * S; }, Hex(0x74D84A));
			P.Mark([C, S](FV Q) { const FV L = (Q - C) / S; return FMath::Max(FMath::Max((-0.2f - L.X * L.X * 1.4f) - L.Y, L.Y - 0.01f), FMath::Abs(L.X) - (0.007f + FMath::Max(0.0f, -L.Y) * 1.3f)) * S; }, Hex(0x74D84A));
			P.Mark([C, S](FV Q) { const FV L = (Q - C) / S; return FMath::Max(FMath::Abs(L.Y - (-0.2f - L.X * L.X * 1.4f)) - 0.006f, FMath::Abs(L.X) - 0.2f) * S; }, Hex(0xC8FFA0, 0.6f));
			P.Mark([C, S](FV Q) { const FV L = (Q - C) / S; return FMath::Max(FMath::Abs(L.X) - 0.007f, FMath::Abs(L.Y - 0.065f) - 0.1f) * S; }, Hex(0x74D84A));
			Speckle(P, C + FV(-0.2f, 0.12f) * S, C + FV(0.2f, 0.33f) * S, 40, 0.002f, 0.005f, Hex(0x2A7A10, 0.7f), 1351);
			Speckle(P, C + FV(-0.2f, -0.3f) * S, C + FV(0.2f, -0.12f) * S, 30, 0.002f, 0.005f, Hex(0x2A7A10, 0.7f), 1352);
			P.Glow([C, S](FV Q) { return SdEllipse(Q, C + FV(0.0f, 0.24f) * S, 0.12f * S, 0.06f * S); }, Hex(0x9AF07A), 0.05f, 0.5f);
		}
		P.Mark(Glass, Hex(0xBFEFFF, 0.14f));
		P.Mark([&Glass](FV Q) { return FMath::Abs(Glass(Q)) - 0.004f; }, Hex(0xE0F8FF, 0.75f));
		P.Mark([C, S](FV Q) { const FV L = (Q - C) / S; return FMath::Max(FMath::Abs(L.X + 0.12f) - 0.012f, FMath::Abs(L.Y + 0.2f) - 0.08f) * S; }, Hex(0xFFFFFF, 0.6f), 0.004f);
		P.Mark([C, S](FV Q) { const FV L = (Q - C) / S; return FMath::Max(FMath::Abs(L.X + 0.13f) - 0.01f, FMath::Abs(L.Y - 0.2f) - 0.06f) * S; }, Hex(0xFFFFFF, 0.45f), 0.004f);
		for (const float Y : { -0.38f, 0.38f })
		{
			const FV A = C + FV(-0.27f, Y) * S, B = C + FV(0.27f, Y) * S;
			P.Fill([A, B, S](FV Q) { return SdCapsule(Q, A, B, 0.035f * S) + Noise2(Q * 50.0f) * 0.003f; }, Hex(0x8A6A48));
			FClip Clip(P, [A, B, S](FV Q) { return SdCapsule(Q, A, B, 0.035f * S); });
			Strands(P, (A + B) * 0.5f - FV(0.25f * S, 0.0f), (A + B) * 0.5f + FV(0.25f * S, 0.0f), 0.05f * S, 6, 0.0015f, Hex(0x4A3018, 0.7f), (int32)(1353 + Y * 10));
			P.MLine({ A + FV(0.0f, -0.015f * S), B + FV(0.0f, -0.015f * S) }, 0.004f, Hex(0xFFE8C8, 0.35f));
		}
		for (const float X : { -0.24f, 0.24f })
		{
			const TArray<FV> Post = { C + FV(X, -0.36f) * S, C + FV(X, 0.36f) * S };
			P.Fill([Post, S](FV Q) { const float T = FMath::Clamp((Q.Y - Post[0].Y) / (Post[1].Y - Post[0].Y), 0.0f, 1.0f); return SdSegment(Q, Post[0], Post[1]) - (0.016f + 0.006f * FMath::Abs(FMath::Sin(T * 3.14159f * 4.0f))) * S; }, Hex(0x7A5A3A));
			P.MLine({ Post[0] + FV(-0.006f, 0.0f), Post[1] + FV(-0.006f, 0.0f) }, 0.003f, Hex(0xFFE8C8, 0.35f));
		}
		// haze curling up in front of the frame
		HDWisp(P, { FV(0.62f, 0.9f), FV(0.72f, 0.82f), FV(0.7f, 0.74f) }, 0.035f, Hex(0x9AF07A, 0.7f), 4);
	}

	void HDSkReefForager(FPainter& P)
	{
		// a nook in the reef turned over by a forager: coral, a waving anemone, and treasure turned up from the sand
		HDMound(P, FV(0.5f, 0.82f), 0.44f, 0.12f, Hex(0xD8C090), 3901, false);
		HDBranchCoral(P, FV(0.22f, 0.76f), 0.62f, 0.03f, Hex(0xE8705A), 3902);
		HDBranchCoral(P, FV(0.82f, 0.74f), 0.42f, 0.024f, Hex(0xC870C8), 3903);
		// an anemone waving its tentacles
		for (int32 k = 0; k < 11; ++k)
		{
			const float A = -160.0f + k * 14.0f;
			const FV Root = FV(0.64f, 0.7f);
			const TArray<FV> Tent = Smooth({ Root, Root + Spin(FV(0.06f, 0.0f), A) + FV(0.0f, -0.02f), Root + Spin(FV(0.12f, 0.0f), A + FMath::Sin(k * 1.3f) * 15.0f) + FV(0.0f, -0.03f) });
			P.Fill([Tent](FV Q) { return SdRibbon(Q, Tent, 0.012f, 0.006f); }, Hex(0x7ADAB0));
			P.Ball(Tent.Last(), 0.009f, Hex(0xE8F8A0), 0.6f);
		}
		P.Fill([](FV Q) { return SdEllipse(Q, FV(0.64f, 0.74f), 0.06f, 0.04f); }, Hex(0x3A9A7A));
		// the find: an open clam with a pearl, a gold coin and a spotted cowrie dug out of the sand
		const auto Lower = [](FV Q) { return FMath::Max(SdEllipse(Q, FV(0.44f, 0.78f), 0.15f, 0.08f), 0.76f - Q.Y); };
		const auto Upper = [](FV Q) { return SdEllipse(Q, FV(0.42f, 0.66f), 0.14f, 0.1f); };
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Grain = 0.2f;
			P.Fill(Upper, Hex(0xC89878));
		}
		{
			FClip Clip(P, Upper);
			for (int32 k = -4; k <= 4; ++k)
			{
				P.MLine({ FV(0.42f, 0.76f), FV(0.42f + k * 0.04f, 0.56f) }, 0.004f, Hex(0x7A4A30, 0.5f));
			}
			P.MEllipse(FV(0.42f, 0.7f), 0.11f, 0.06f, Hex(0xF0D8E8, 0.6f), 0.03f);
		}
		P.Fill([](FV Q) { return SdEllipse(Q, FV(0.44f, 0.76f), 0.14f, 0.035f); }, Hex(0xE8D0E0));
		Glint(P, FV(0.44f, 0.68f), 0.14f, Hex(0xFFF8E0));
		P.Ball(FV(0.44f, 0.72f), 0.05f, Hex(0xF4F0EC), 0.95f);
		P.MCircle(FV(0.455f, 0.735f), 0.02f, Hex(0xC8B8E0, 0.5f));
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Grain = 0.2f;
			P.Fill(Lower, Hex(0xB88868));
		}
		{
			FClip Clip(P, Lower);
			for (int32 k = -4; k <= 4; ++k)
			{
				P.MLine({ FV(0.44f, 0.86f), FV(0.44f + k * 0.04f, 0.76f) }, 0.004f, Hex(0x7A4A30, 0.5f));
			}
		}
		{
			FWith With(P);
			P.Look.Spec = 0.95f;
			P.Fill([](FV Q) { return SdEllipse(Q, FV(0.66f, 0.86f), 0.05f, 0.03f); }, Hex(0xE8B040));
		}
		P.Mark([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.66f, 0.86f), 0.038f, 0.022f)) - 0.003f; }, Hex(0x8A5A10, 0.7f));
		P.MEllipse(FV(0.65f, 0.852f), 0.018f, 0.006f, Hex(0xFFF0B0, 0.8f));
		// the cowrie, glossy and spotted
		P.Fill([](FV Q) { return SdEllipse(Q, FV(0.24f, 0.86f), 0.06f, 0.035f); }, Hex(0xC88A4A));
		Speckle(P, FV(0.19f, 0.83f), FV(0.29f, 0.89f), 14, 0.003f, 0.006f, Hex(0xFFF0D8, 0.9f), 3904);
		P.MLine({ FV(0.19f, 0.875f), FV(0.29f, 0.875f) }, 0.003f, Hex(0x3A1A08));
		P.MEllipse(FV(0.22f, 0.845f), 0.02f, 0.008f, Hex(0xFFFFFF, 0.7f));
		// stirred-up sand drifting
		Speckle(P, FV(0.3f, 0.5f), FV(0.7f, 0.66f), 30, 0.002f, 0.005f, Hex(0xF0E2C0, 0.6f), 3905);
		Bubble(P, FV(0.5f, 0.42f), 0.02f);
		Bubble(P, FV(0.54f, 0.34f), 0.013f);
	}

	void HDSkSerratedTeeth(FPainter& P)
	{
		// a sawfish's rostrum up close, its rows of serrated teeth wet with blood that clouds the water
		HDWisp(P, { FV(0.5f, 0.56f), FV(0.62f, 0.68f), FV(0.82f, 0.66f), FV(0.92f, 0.78f) }, 0.05f, Hex(0xC81820, 0.8f), 5);
		HDWisp(P, { FV(0.34f, 0.66f), FV(0.4f, 0.8f), FV(0.3f, 0.92f) }, 0.04f, Hex(0xC81820, 0.7f), 6);
		HDSaw(P, FV(0.08f, 0.86f), FV(0.9f, 0.18f), 0.09f, 8);
		// blood on two teeth, one running and dripping
		const FV D = (FV(0.9f, 0.18f) - FV(0.08f, 0.86f)).GetSafeNormal(), N(-D.Y, D.X);
		for (const float T : { 0.42f, 0.6f })
		{
			const FV At = FMath::Lerp(FV(0.08f, 0.86f), FV(0.9f, 0.18f), T);
			const float Wd = FMath::Lerp(0.09f, 0.09f * 0.55f, T);
			const FV Tooth = At + N * Wd * 1.6f;
			P.Mark([Tooth](FV Q) { return SdEllipse(Q, Tooth, 0.016f, 0.012f) + Noise2(Q * 80.0f) * 0.004f; }, Hex(0xB01018, 0.85f));
		}
		const FV At = FMath::Lerp(FV(0.08f, 0.86f), FV(0.9f, 0.18f), 0.42f);
		HDDrip(P, At + N * 0.09f * 0.8f * 1.7f, 0.13f, 0.03f, Hex(0xC01820), 0.0f, 0.011f);
		Speckle(P, FV(0.4f, 0.5f), FV(0.95f, 0.85f), 16, 0.003f, 0.006f, Hex(0xC81820, 0.6f), 3906);
	}

	void HDSkToothSpray(FPainter& P)
	{
		// the sawfish slashes and its teeth fly loose, spinning off through the water at everything nearby
		HDSaw(P, FV(0.02f, 0.94f), FV(0.56f, 0.4f), 0.08f, 6);
		const FV D = (FV(0.56f, 0.4f) - FV(0.02f, 0.94f)).GetSafeNormal(), N(-D.Y, D.X);
		// empty sockets where teeth tore loose
		for (const float T : { 0.62f, 0.76f, 0.9f })
		{
			const FV At = FMath::Lerp(FV(0.02f, 0.94f), FV(0.56f, 0.4f), T) + N * FMath::Lerp(0.08f, 0.044f, T) * 0.85f;
			P.MEllipse(At, 0.012f, 0.008f, Hex(0x3A2A1A, 0.85f), 0.003f);
		}
		// the flying teeth, each trailing a wake of bubbles
		struct FFly { FV At; float Ang; float S; };
		const FFly Fly[] = { { FV(0.64f, 0.16f), -60.0f, 1.0f }, { FV(0.84f, 0.3f), -25.0f, 0.9f }, { FV(0.86f, 0.58f), 10.0f, 0.85f }, { FV(0.42f, 0.14f), -95.0f, 0.75f } };
		for (const FFly& F : Fly)
		{
			const FV Dir = Spin(FV(1.0f, 0.0f), F.Ang);
			for (int32 k = 0; k < 5; ++k)
			{
				Bubble(P, F.At - Dir * (0.05f + k * 0.035f) + FV(-Dir.Y, Dir.X) * (Rand01(4001, k) - 0.5f) * 0.03f, (0.012f - k * 0.0018f) * F.S);
			}
			P.Mark([F, Dir](FV Q) { return SdRibbon(Q, { F.At - Dir * 0.2f, F.At - Dir * 0.03f }, 0.001f, 0.012f * F.S); }, Hex(0xBFEFFF, 0.3f), 0.006f);
			HDFang(P, F.At - Dir * 0.04f * F.S, F.At + Dir * 0.06f * F.S, 0.024f * F.S, 0.015f * F.S);
		}
		// a fish caught by one of them, flinching
		HDFish(P, FV(0.86f, 0.78f), 0.4f, Hex(0x6A9AB8), Hex(0xE4F0F4), -1.0f);
	}

	void HDSkElectricSnout(FPainter& P)
	{
		// the sawfish seen from above, its long snout dotted with electric-sensing pores, feeling out a fish hidden in the sand far ahead
		const FLinearColor Skin = Hex(0x9A9670), Dark = Hex(0x4E4E36);
		HDMound(P, FV(0.78f, 0.27f), 0.16f, 0.06f, Hex(0xD8C090), 4101, false);
		// the hidden fish, only its outline and eyes showing through the sand, lit up by the sense
		P.Glow([](FV Q) { return SdEllipse(Q, FV(0.78f, 0.24f), 0.08f, 0.04f); }, Hex(0x6AE0FF), 0.05f, 0.6f);
		P.Mark([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.78f, 0.24f), 0.08f, 0.035f)) - 0.004f; }, Hex(0x8AF0FF, 0.9f));
		P.MCircle(FV(0.73f, 0.235f), 0.006f, Hex(0x1A2A30));
		// the field felt through the water: rings spreading out from the snout tip
		for (int32 k = 0; k < 4; ++k)
		{
			const float R = 0.08f + k * 0.07f;
			P.Mark([R](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.5f, 0.2f), R * 1.3f, R) + Noise2(Q * 20.0f) * 0.006f) - 0.003f; }, Hex(0x8AF0FF, 0.55f - k * 0.1f));
		}
		// the head and pectoral fins
		const auto Head = [](FV Q)
		{
			const float Body = SdEllipse(Q, FV(0.5f, 0.9f), 0.14f, 0.16f);
			const float Fins = SdPoly(Q, { FV(0.36f, 0.82f), FV(0.18f, 0.98f), FV(0.5f, 0.94f), FV(0.82f, 0.98f), FV(0.64f, 0.82f) });
			return SMin(Body, Fins, 0.04f);
		};
		{
			FWith With(P);
			P.Look.Dome = 0.1f;
			P.Look.Spec = 0.5f;
			P.Fill(Head, Skin);
		}
		{
			FClip Clip(P, Head);
			Speckle(P, FV(0.2f, 0.76f), FV(0.8f, 1.0f), 60, 0.002f, 0.005f, Alpha(Dark, 0.5f), 4102);
			P.MEllipse(FV(0.46f, 0.84f), 0.06f, 0.03f, Hex(0xFFFFFF, 0.3f), 0.03f);
		}
		for (const float F : { -1.0f, 1.0f })
		{
			HDEye(P, FV(0.5f + F * 0.07f, 0.83f), 0.018f, Hex(0x5A6A2A), FV(0.0f, -0.3f), 0.6f);
		}
		// the snout itself, pores glowing along its length
		HDSaw(P, FV(0.5f, 0.8f), FV(0.5f, 0.24f), 0.05f, 8);
		for (int32 k = 0; k < 16; ++k)
		{
			const float T = 0.08f + k * 0.055f;
			const FV At = FMath::Lerp(FV(0.5f, 0.8f), FV(0.5f, 0.24f), T);
			const float Wd = FMath::Lerp(0.05f, 0.0275f, T);
			for (const float S : { -0.45f, 0.45f })
			{
				const FV Pore = At + FV(S * Wd, (k % 2) * 0.01f);
				P.Glow([Pore](FV Q) { return SdCircle(Q, Pore, 0.003f); }, Hex(0x6AE0FF), 0.012f, 0.8f);
				P.MCircle(Pore, 0.004f, Hex(0xD8FAFF));
			}
		}
	}

	// ------------------------------------------------------------------ round 3 skills: humpback and acid vent

	/** A billowing cloud of silt: overlapping soft puffs, lit on top and shadowed below */
	void HDSiltCloud(FPainter& P, const TArray<FVector>& Puffs, const FLinearColor& Color, int32 Seed)
	{
		const auto Cloud = [&Puffs, Seed](FV Q)
		{
			float D = 1e9f;
			for (const FVector& B : Puffs)
			{
				D = SMin(D, SdCircle(Q, FV(B.X, B.Y), B.Z), 0.03f);
			}
			return D + Noise2(Q * 22.0f + FV(Seed * 0.3f, 0.0f)) * 0.012f;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.08f;
			P.Look.Spec = 0.05f;
			P.Look.Ink = 0.003f;
			P.Look.Grain = 0.35f;
			P.Look.GrainFreq = 40.0f;
			P.Glow(Cloud, Color, 0.05f, 0.4f);
			P.Fill(Cloud, Alpha(Color, 0.85f));
		}
		FClip Clip(P, Cloud);
		for (const FVector& B : Puffs)
		{
			P.MEllipse(FV(B.X, B.Y + B.Z * 0.45f), B.Z * 0.9f, B.Z * 0.45f, Alpha(Tone(Color, 0.55f), 0.45f), B.Z * 0.35f);
			P.MEllipse(FV(B.X - B.Z * 0.25f, B.Y - B.Z * 0.4f), B.Z * 0.5f, B.Z * 0.25f, Hex(0xFFFFFF, 0.22f), B.Z * 0.25f);
		}
		Speckle(P, FV(0.0f, 0.0f), FV(1.0f, 1.0f), 160, 0.0015f, 0.004f, Alpha(Tone(Color, 0.5f), 0.6f), Seed + 1);
	}

	/** A rough rock chimney of a hydrothermal vent from Base up to Top */
	void HDVentChimney(FPainter& P, FV Base, FV Top, float R0, float R1, int32 Seed)
	{
		const FLinearColor RockC = Hex(0x4E4A48);
		const auto Rock = [Base, Top, R0, R1, Seed](FV Q) { return SdRibbon(Q, { Base, Top }, R0, R1) + Noise2(Q * 16.0f + FV(Seed, 0.0f)) * 0.016f; };
		{
			FWith With(P);
			P.Look.Dome = 0.08f;
			P.Look.Grain = 0.3f;
			P.Fill(Rock, RockC);
		}
		FClip Clip(P, Rock);
		P.Mark([](FV Q) { return (FMath::Abs(FMath::Sin(Q.Y * 70.0f + Noise2(Q * 12.0f) * 2.0f)) - 0.85f) * 0.008f; }, Hex(0x2A2624, 0.7f));
		P.Mark([](FV Q) { return (FMath::Abs(FMath::Sin(Q.Y * 70.0f + 0.4f + Noise2(Q * 12.0f) * 2.0f)) - 0.9f) * 0.008f; }, Hex(0xA89A88, 0.4f));
		P.MEllipse(Top + FV(0.0f, 0.02f), R1 * 1.1f, R1 * 0.5f, Hex(0xFFA030, 0.55f), R1 * 0.4f);
		Speckle(P, Top - FV(R0, 0.0f), Base + FV(R0, 0.0f), 50, 0.002f, 0.006f, Hex(0xE8C060, 0.5f), Seed + 2);
	}

	void HDSkSiltPlume(FPainter& P)
	{
		// the vent belches a great billowing plume of silt, and a fish caught in it gropes along half-blind
		HDMound(P, FV(0.5f, 0.92f), 0.36f, 0.07f, Hex(0x6A6058), 4401, true);
		HDVentChimney(P, FV(0.44f, 0.92f), FV(0.42f, 0.64f), 0.09f, 0.045f, 4402);
		const TArray<FVector> Puffs = { FVector(0.42f, 0.58f, 0.06f), FVector(0.38f, 0.48f, 0.08f), FVector(0.48f, 0.42f, 0.09f), FVector(0.3f, 0.36f, 0.1f),
			FVector(0.52f, 0.28f, 0.12f), FVector(0.7f, 0.34f, 0.1f), FVector(0.36f, 0.2f, 0.11f), FVector(0.66f, 0.16f, 0.12f), FVector(0.2f, 0.24f, 0.08f), FVector(0.82f, 0.24f, 0.08f) };
		HDSiltCloud(P, Puffs, Hex(0xA89478), 4403);
		// the fish half swallowed by the cloud, its eye squinting
		HDFish(P, FV(0.78f, 0.5f), 0.42f, Hex(0x6A9AB8), Hex(0xE4F0F4), -1.0f);
		P.Mark([](FV Q) { return SdCircle(Q, FV(0.85f, 0.42f), 0.1f) + Noise2(Q * 20.0f) * 0.02f; }, Hex(0xA89478, 0.85f), 0.03f);
		P.MLine({ FV(0.69f, 0.485f), FV(0.72f, 0.49f) }, 0.003f, Hex(0x1A2A30));
		// grit raining out of the plume
		Speckle(P, FV(0.1f, 0.5f), FV(0.9f, 0.86f), 40, 0.002f, 0.005f, Hex(0xC8B898, 0.7f), 4404);
	}

	void HDSkMineralGrowth(FPainter& P)
	{
		// a vent chimney crusted with fresh mineral crystals, tube worms crowding round it with their red plumes out
		HDMound(P, FV(0.5f, 0.9f), 0.42f, 0.08f, Hex(0x5A5450), 4501, true);
		HDVentChimney(P, FV(0.5f, 0.9f), FV(0.48f, 0.34f), 0.14f, 0.06f, 4502);
		P.Mark([](FV Q) { return SdRibbon(Q, Smooth({ FV(0.48f, 0.32f), FV(0.46f, 0.2f), FV(0.5f, 0.06f) }), 0.03f, 0.07f) + Noise2(Q * 14.0f) * 0.02f; }, Hex(0x8A7A6A, 0.4f), 0.03f);
		// tube worms: white tubes, blood-red feathery plumes
		for (const FVector W : { FVector(0.22f, 0.9f, 0.3f), FVector(0.28f, 0.9f, 0.22f), FVector(0.76f, 0.9f, 0.26f), FVector(0.82f, 0.9f, 0.18f), FVector(0.7f, 0.9f, 0.16f) })
		{
			const FV Base(W.X, W.Y), Top = Base + FV((W.X - 0.5f) * 0.1f, -W.Z);
			P.Fill([Base, Top](FV Q) { return SdRibbon(Q, Smooth({ Base, FMath::Lerp(Base, Top, 0.5f) + FV(0.01f, 0.0f), Top }), 0.018f, 0.014f); }, Hex(0xE8E0D0));
			for (int32 j = 1; j < 6; ++j)
			{
				const FV At = FMath::Lerp(Base, Top, j / 6.0f);
				P.MLine({ At - FV(0.016f, 0.0f), At + FV(0.016f, 0.004f) }, 0.002f, Hex(0x8A7A6A, 0.6f));
			}
			for (int32 j = 0; j < 7; ++j)
			{
				const FV E = Top + Spin(FV(0.0f, -0.05f), -45.0f + j * 15.0f);
				P.Fill([Top, E](FV Q) { return SdRibbon(Q, Smooth({ Top, (Top + E) * 0.5f + FV(0.004f, 0.0f), E }), 0.007f, 0.004f) - FMath::Abs(FMath::Sin(Q.Y * 300.0f)) * 0.002f; }, Hex(0xD8202A));
			}
		}
		// crystal clusters breaking out of the chimney walls
		HDCrystal(P, FV(0.4f, 0.62f), 0.18f, 0.035f, -40.0f, Hex(0xF8E070));
		HDCrystal(P, FV(0.42f, 0.66f), 0.12f, 0.028f, -70.0f, Hex(0x9AF0E0));
		HDCrystal(P, FV(0.58f, 0.52f), 0.2f, 0.038f, 35.0f, Hex(0xB0E0FF));
		HDCrystal(P, FV(0.56f, 0.56f), 0.13f, 0.03f, 65.0f, Hex(0xF8E070));
		HDCrystal(P, FV(0.5f, 0.36f), 0.16f, 0.032f, -8.0f, Hex(0xE8B0F8));
		HDCrystal(P, FV(0.46f, 0.8f), 0.1f, 0.025f, -55.0f, Hex(0x9AF0E0));
		Glint(P, FV(0.5f, 0.2f), 0.12f, Hex(0xFFF8D0));
	}

	void HDMantisShrimp(FPainter& P)
	{
		// the mantis shrimp crouches on the plinth: abdomen low at the back, thorax and head reared up at the front
		const FLinearColor Teal = Hex(0x26B89C), TealDeep = Hex(0x0E5A4C), Green = Hex(0x7ADA5A), Red = Hex(0xE8463A), Orange = Hex(0xF4962A), Blue = Hex(0x2A5AD8);
		FNest Place(P, FV(0.0f, 0.06f), 1.0f);
		const TArray<FV> Spine = Smooth({ FV(0.2f, 0.75f), FV(0.31f, 0.73f), FV(0.43f, 0.7f), FV(0.55f, 0.64f), FV(0.63f, 0.54f), FV(0.66f, 0.42f) });
		const float R0 = 0.045f, R1 = 0.07f;
		const auto Normal = [&Spine](float T, FV& Pos, FV& Tan) { Along(Spine, T, Pos, Tan); return FV(Tan.Y, -Tan.X); };
		// walking legs, jointed, planted on the plinth
		for (int32 i = 0; i < 3; ++i)
		{
			FV Pos, Tan;
			Along(Spine, 0.55f + i * 0.09f, Pos, Tan);
			const FV Knee = Pos + FV(0.03f - i * 0.01f, 0.07f), Foot = Pos + FV(-0.01f - i * 0.02f, 0.13f);
			P.Fill([Pos, Knee, Foot](FV Q) { return SMin(SdCapsule(Q, Pos, Knee, 0.009f), SdCapsule(Q, Knee, Foot, 0.006f), 0.006f); }, Red);
		}
		// the tail fan: a spiny telson flanked by uropods with blue-ringed eyespots
		{
			const FV Root = Spine[0] + FV(0.01f, 0.0f);
			for (int32 k = 0; k < 5; ++k)
			{
				const float T = -0.7f + k * 0.35f;
				const FV D(-FMath::Cos(T), FMath::Sin(T));
				const FV Tip = Root + D * (k == 2 ? 0.09f : 0.1f);
				P.Fill([Root, Tip](FV Q) { return SdRibbon(Q, Smooth({ Root, (Root + Tip) * 0.5f, Tip }), 0.012f, 0.022f) + FMath::Abs(FMath::Sin(Q.X * 400.0f)) * 0.0015f; }, k == 2 ? Teal : Orange);
				if (k != 2)
				{
					P.MCircle(Tip - D * 0.016f, 0.01f, Blue);
					P.MCircle(Tip - D * 0.016f, 0.0045f, Hex(0xFFE08A));
				}
				else
				{
					P.MLine({ Root, Tip }, 0.003f, Alpha(TealDeep, 0.7f));
				}
			}
		}
		// swimmerets fringing the belly
		for (int32 k = 1; k < 6; ++k)
		{
			FV Pos, Tan;
			const FV N = Normal(k / 10.0f, Pos, Tan);
			for (int32 j = 0; j < 3; ++j)
			{
				P.Fill([Pos, N, Tan, j](FV Q) { return SdRibbon(Q, { Pos - N * 0.035f + Tan * (j * 0.01f - 0.01f), Pos - N * 0.075f + Tan * (j * 0.01f - 0.02f) }, 0.004f, 0.002f); }, Hex(0xF07A4A));
			}
		}
		// the armoured body, segment by segment
		const auto Body = [&Spine, R0, R1](FV Q) { return SdRibbon(Q, Spine, R0, R1); };
		{
			FWith With(P);
			P.Look.Dome = 0.3f;
			P.Look.Spec = 0.85f;
			P.Fill(Body, Teal);
		}
		{
			FClip Clip(P, Body);
			for (int32 k = 1; k < 11; ++k)
			{
				FV Pos, Tan;
				const FV N = Normal(k / 11.0f, Pos, Tan);
				// each plate: a dark seam, a lit leading lip, a lime band and a pair of yellow spots
				P.MCapsule(Pos - N * 0.08f, Pos + N * 0.08f, 0.005f, Alpha(TealDeep, 0.9f));
				P.MCapsule(Pos - N * 0.08f + Tan * 0.009f, Pos + N * 0.08f + Tan * 0.009f, 0.003f, Hex(0xC8FFF0, 0.55f));
				P.MCapsule(Pos - N * 0.08f + Tan * 0.022f, Pos + N * 0.08f + Tan * 0.022f, 0.007f, Alpha(Green, 0.25f));
				P.MCircle(Pos + N * 0.03f + Tan * 0.02f, 0.006f, Hex(0xFFE04A, 0.85f));
			}
			// a pale underside and a lit dorsal ridge
			TArray<FV> Under, Ridge;
			for (int32 k = 0; k <= 24; ++k)
			{
				FV Pos, Tan;
				const FV N = Normal(k / 24.0f, Pos, Tan);
				const float R = FMath::Lerp(R0, R1, k / 24.0f);
				Under.Add(Pos - N * R * 0.8f);
				Ridge.Add(Pos + N * R * 0.55f);
			}
			P.MLine(Under, 0.012f, Hex(0xE8F8C8, 0.35f));
			P.MLine(Ridge, 0.006f, Hex(0xFFFFFF, 0.4f));
			Speckle(P, FV(0.15f, 0.35f), FV(0.72f, 0.8f), 70, 0.0015f, 0.004f, Alpha(TealDeep, 0.6f), 951);
		}
		const FV Head = Spine.Last();
		// the head shield
		P.Fill([Head](FV Q) { return SdEllipse(Q, Head + FV(0.01f, -0.005f), 0.05f, 0.04f); }, Tone(Teal, 1.1f));
		P.MEllipse(Head + FV(-0.005f, -0.02f), 0.03f, 0.012f, Hex(0xFFFFFF, 0.35f), 0.008f);
		// antennae sweeping back, and orange antennal scales
		for (int32 k = 0; k < 2; ++k)
		{
			const FV Root = Head + FV(0.03f, -0.03f);
			P.MLine(Smooth({ Root, Root + FV(0.06f, -0.08f - k * 0.03f), Root + FV(0.16f - k * 0.04f, -0.12f - k * 0.05f) }), 0.003f, Hex(0xF0A030));
		}
		P.Fill([Head](FV Q) { return SdEllipse(Q, Head + FV(0.07f, -0.03f), 0.035f, 0.014f) ; }, Orange);
		P.MLine({ Head + FV(0.04f, -0.03f), Head + FV(0.1f, -0.03f) }, 0.002f, Hex(0x9A4A10, 0.7f));
		// the raptorial club folded under the head, cocked to strike
		{
			const FV Sh = Head + FV(0.02f, 0.03f), Elbow = Head + FV(0.1f, 0.11f), Club = Head + FV(0.15f, 0.02f);
			P.Fill([Sh, Elbow](FV Q) { return SdRibbon(Q, { Sh, Elbow }, 0.022f, 0.016f); }, Red);
			const auto Fist = [Elbow, Club](FV Q) { return SMin(SdCapsule(Q, Elbow, Club, 0.017f), SdEllipse(Q, Club, 0.036f, 0.03f), 0.02f); };
			{
				FWith With(P);
				P.Look.Spec = 0.9f;
				P.Look.Dome = 0.4f;
				P.Fill(Fist, Orange);
			}
			FClip Clip(P, Fist);
			P.MEllipse(Club + FV(-0.008f, -0.01f), 0.02f, 0.012f, Hex(0xFFF0B0, 0.6f), 0.006f);
			P.MArc(Club, 0.03f, 0.005f, -70.0f, 70.0f, Hex(0x8A2A10, 0.8f));
			Speckle(P, Club - FV(0.04f, 0.04f), Club + FV(0.04f, 0.04f), 20, 0.0015f, 0.003f, Hex(0xC04A10, 0.7f), 952);
		}
		// stalked trinocular eyes on top of the head
		for (int32 k = 0; k < 2; ++k)
		{
			const FV Base = Head + FV(-0.005f + k * 0.03f, -0.03f), E = Head + FV(-0.025f + k * 0.055f, -0.095f + k * 0.005f);
			P.Fill([Base, E](FV Q) { return SdRibbon(Q, { Base, E }, 0.012f, 0.009f); }, Teal);
			const auto Eye = [E](FV Q) { return SdEllipse(Q, E, 0.026f, 0.02f); };
			{
				FWith With(P);
				P.Look.Spec = 0.95f;
				P.Look.Dome = 10.0f;
				P.Fill(Eye, Hex(0x6AD8E0));
			}
			FClip Clip(P, Eye);
			P.Mark([E](FV Q) { const FV L = (Q - E) * 260.0f; return (FMath::Abs(FMath::Sin(L.X)) + FMath::Abs(FMath::Sin(L.Y)) - 0.4f) * 0.002f; }, Hex(0x1A6A8A, 0.3f));
			// the mid-band of the compound eye and its pseudopupil
			P.MCapsule(E - FV(0.026f, -0.002f), E + FV(0.026f, 0.002f), 0.0045f, Hex(0x1A3A6A));
			P.MCircle(E + FV(0.004f, 0.0f), 0.006f, Hex(0x06101A));
			P.MCircle(E + FV(-0.008f, -0.008f), 0.0045f, Hex(0xFFFFFF, 0.9f));
		}
	}

	void HDLionfishBody(FPainter& P, bool bVenom)
	{
		const FLinearColor Cream = Hex(0xF8ECD8), Maroon = Hex(0x9A2A2A), Fin = Hex(0xF4E0D0);
		FNest Place(P, FV(-0.15f, -0.1f), 1.3f);
		const FV C(0.5f, 0.48f);
		// venomous dorsal spines: tall, separate, banded, with fine membrane between
		for (int32 k = 0; k < 11; ++k)
		{
			const FV Root = C + FV(-0.12f + k * 0.024f, -0.09f);
			const float H = 0.16f + FMath::Sin(k * 0.35f) * 0.06f;
			const FV Tip = Root + FV(-0.03f + k * 0.002f, -H);
			if (k > 0)
			{
				const FV PrevRoot = C + FV(-0.12f + (k - 1) * 0.024f, -0.09f);
				P.Mark([Root, PrevRoot, Tip](FV Q) { return SdPoly(Q, { PrevRoot, Root, FMath::Lerp(Root, Tip, 0.6f), FMath::Lerp(PrevRoot, Tip + FV(-0.024f, 0.0f), 0.6f) }); }, Hex(0xF4C8C0, 0.4f));
			}
			P.Fill([Root, Tip](FV Q) { return SdRibbon(Q, { Root, Tip }, 0.006f, 0.0015f); }, Cream);
			for (int32 j = 1; j < 5; ++j)
			{
				P.MCircle(FMath::Lerp(Root, Tip, j / 5.0f), 0.005f, Alpha(Maroon, 0.9f));
			}
			if (bVenom)
			{
				// venom welling from every needle tip, running down the longest into drops
				P.Glow([Tip](FV Q) { return SdCircle(Q, Tip, 0.004f); }, Hex(0x74D84A), 0.03f, 0.8f);
				if (k == 2 || k == 5 || k == 8)
				{
					HDDrip(P, Tip + FV(0.0f, 0.002f), 0.05f + (k == 5 ? 0.02f : 0.0f), 0.011f, Hex(0x74D84A), 0.0f, 0.004f);
				}
				else
				{
					P.Ball(Tip + FV(0.0f, 0.003f), 0.007f, Hex(0x74D84A), 1.0f);
				}
			}
		}
		// the body: a deep, tapering fish with wavy vertical bands
		const TArray<FV> Outline = Smooth({ FV(0.24f, 0.48f), FV(0.32f, 0.38f), FV(0.46f, 0.34f), FV(0.6f, 0.35f), FV(0.7f, 0.41f), FV(0.74f, 0.49f), FV(0.7f, 0.56f), FV(0.58f, 0.6f), FV(0.42f, 0.59f), FV(0.3f, 0.54f) }, true, 5);
		HDFin(P, FV(0.27f, 0.48f), { FV(0.18f, 0.4f), FV(0.14f, 0.46f), FV(0.14f, 0.52f), FV(0.18f, 0.57f) }, Fin, Alpha(Maroon, 0.7f));
		const auto Body = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.55f;
			P.Fill(Body, Cream);
		}
		{
			FClip Clip(P, Body);
			P.Mark([](FV Q) { const float V = Q.X * 48.0f + FMath::Sin(Q.Y * 26.0f) * 0.9f + Noise2(Q * 18.0f) * 0.5f; return (FMath::Sin(V) - 0.1f) * -0.006f; }, Alpha(Maroon, 0.92f), 0.0015f);
			P.Mark([](FV Q) { const float V = Q.X * 48.0f + FMath::Sin(Q.Y * 26.0f) * 0.9f + Noise2(Q * 18.0f) * 0.5f; return (FMath::Abs(FMath::Sin(V + 1.57f)) - 0.12f) * 0.006f; }, Hex(0x5A1414, 0.5f));
			Scales(P, FV(0.24f, 0.33f), FV(0.74f, 0.6f), 0.02f, 0.0f, Hex(0x5A1A1A, 0.2f), Hex(0xFFFFFF, 0.2f));
			P.MEllipse(FV(0.5f, 0.38f), 0.16f, 0.025f, Hex(0xFFFFFF, 0.35f), 0.02f);
			P.MEllipse(FV(0.5f, 0.58f), 0.2f, 0.03f, Hex(0x3A0A0A, 0.3f), 0.02f);
		}
		// the great feathery pectoral fan on the near side: banded rays with a sheer membrane between them
		{
			const FV Root(0.55f, 0.52f);
			TArray<FV> Tips;
			for (int32 k = 0; k < 9; ++k)
			{
				const float T = -0.25f + k * 0.17f;
				Tips.Add(Root + FV(-FMath::Cos(T), FMath::Sin(T)) * (0.26f - FMath::Abs(k - 4) * 0.012f));
			}
			TArray<FV> Web = { Root };
			for (const FV& T : Tips)
			{
				Web.Add(FMath::Lerp(Root, T, 0.8f));
			}
			P.Mark([&Web](FV Q) { return SdPoly(Q, Web); }, Hex(0xF8D8D0, 0.35f));
			for (int32 k = 0; k < Tips.Num(); ++k)
			{
				const TArray<FV> Ray = Smooth({ Root, FMath::Lerp(Root, Tips[k], 0.5f) + FV(0.0f, 0.01f), Tips[k] });
				P.Fill([Ray](FV Q) { return SdRibbon(Q, Ray, 0.009f, 0.003f); }, Fin);
				for (int32 j = 1; j < 7; ++j)
				{
					FV Pos, Tan;
					Along(Ray, j / 7.0f, Pos, Tan);
					P.MCapsule(Pos - Tan * 0.006f, Pos + Tan * 0.006f, 0.006f - j * 0.0006f, Alpha(Maroon, 0.9f));
				}
			}
		}
		// the head: frilly tentacle brows, a big eye in a dark band, a downturned mouth
		const FV Eye(0.66f, 0.43f);
		P.MCapsule(Eye + FV(-0.01f, -0.06f), Eye + FV(0.0f, 0.08f), 0.02f, Alpha(Maroon, 0.8f));
		HDEye(P, Eye, 0.03f, Hex(0xC0402A), FV(0.2f, 0.05f), 0.6f);
		for (int32 k = 0; k < 3; ++k)
		{
			const FV Root = Eye + FV(-0.01f + k * 0.012f, -0.03f);
			P.Fill([Root, k](FV Q) { return SdRibbon(Q, Smooth({ Root, Root + FV(0.01f, -0.025f), Root + FV(-0.005f + k * 0.01f, -0.05f) }), 0.004f, 0.002f) - FMath::Abs(FMath::Sin(Q.Y * 300.0f)) * 0.002f; }, Cream);
		}
		P.MLine(Smooth({ FV(0.735f, 0.5f), FV(0.715f, 0.51f), FV(0.69f, 0.505f) }), 0.003f, Hex(0x3A0A0A));
		// pelvic fins under the belly and a ragged anal fin
		HDFin(P, FV(0.56f, 0.58f), { FV(0.52f, 0.66f), FV(0.56f, 0.7f), FV(0.62f, 0.66f) }, Fin, Alpha(Maroon, 0.7f));
		HDFin(P, FV(0.36f, 0.56f), { FV(0.3f, 0.6f), FV(0.32f, 0.66f), FV(0.4f, 0.63f) }, Fin, Alpha(Maroon, 0.7f));
	}

	void HDHumpbackBody(FPainter& P)
	{
		const FLinearColor Back = Hex(0x2E3A58), Belly = Hex(0xDCE6F0), Pleat = Hex(0x7A88A8);
		// the whale swims over the plinth: big blunt head on the left, body arching up to a raised fluke
		const TArray<FV> Spine = Smooth({ FV(0.16f, 0.56f), FV(0.32f, 0.6f), FV(0.5f, 0.58f), FV(0.66f, 0.5f), FV(0.78f, 0.38f), FV(0.84f, 0.27f) });
		const float RHead = 0.12f, RTail = 0.026f;
		const auto Side = [&Spine](float T, FV& Pos, FV& Tan) { Along(Spine, T, Pos, Tan); return FV(-Tan.Y, Tan.X); };
		// far pectoral fin, peeking behind the body
		P.Fill([](FV Q) { return SdRibbon(Q, Smooth({ FV(0.36f, 0.6f), FV(0.42f, 0.54f), FV(0.5f, 0.5f) }), 0.022f, 0.01f); }, Tone(Back, 0.8f));
		// the fluke, set across the tail stock
		{
			FV Pos, Tan;
			Along(Spine, 1.0f, Pos, Tan);
			const float Ang = FMath::RadiansToDegrees(FMath::Atan2(Tan.Y, Tan.X)) + 90.0f;
			const auto Fluke = [Pos, Ang](FV Q)
			{
				const FV L = Spin(Q - Pos, -Ang);
				const float Lobes = SMin(SdEllipse(Spin(L - FV(-0.065f, -0.03f), -16.0f), FV(0.0f, 0.0f), 0.075f, 0.026f), SdEllipse(Spin(L - FV(0.065f, -0.03f), 16.0f), FV(0.0f, 0.0f), 0.075f, 0.026f), 0.02f);
				const float Stock = SdCapsule(L, FV(0.0f, 0.0f), FV(0.0f, -0.03f), 0.02f);
				const float Notch = SdCircle(L, FV(0.0f, -0.06f), 0.016f);
				return SSub(SMin(Lobes, Stock, 0.015f) + FMath::Abs(FMath::Sin(L.X * 90.0f)) * 0.002f, Notch, 0.008f);
			};
			{
				FWith With(P);
				P.Look.Dome = 0.08f;
				P.Look.Spec = 0.3f;
				P.Fill(Fluke, Back);
			}
			FClip Clip(P, Fluke);
			P.Mark([Pos, Ang](FV Q) { const FV L = Spin(Q - Pos, -Ang); return L.Y + 0.035f; }, Alpha(Belly, 0.7f), 0.01f);
			Speckle(P, Pos - FV(0.12f, 0.12f), Pos + FV(0.12f, 0.12f), 30, 0.002f, 0.005f, Hex(0x1A2238, 0.6f), 974);
		}
		// the body
		const auto Body = [&Spine, RHead, RTail](FV Q) { return SdRibbon(Q, Spine, RHead, RTail); };
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.35f;
			P.Fill(Body, Back);
		}
		{
			FClip Clip(P, Body);
			// white throat and belly with the long ventral pleats
			TArray<FV> BellyLine;
			for (int32 k = 0; k <= 24; ++k)
			{
				FV Pos, Tan;
				const FV N = Side(k / 24.0f, Pos, Tan);
				BellyLine.Add(Pos + N * FMath::Lerp(RHead, RTail, k / 24.0f) * 0.95f);
			}
			P.Mark([&BellyLine](FV Q) { return SdRibbon(Q, BellyLine, 0.065f, 0.006f); }, Belly, 0.01f);
			for (int32 j = 0; j < 6; ++j)
			{
				TArray<FV> Line;
				for (int32 k = 0; k <= 12; ++k)
				{
					FV Pos, Tan;
					const FV N = Side(k / 24.0f, Pos, Tan);
					Line.Add(Pos + N * FMath::Lerp(RHead, RTail, k / 24.0f) * (0.5f + j * 0.09f));
				}
				P.MLine(Smooth(Line), 0.0022f, Alpha(Pleat, 0.75f));
			}
			// mottling, scars and a lit back
			Speckle(P, FV(0.05f, 0.25f), FV(0.95f, 0.7f), 90, 0.002f, 0.006f, Hex(0xA8B8D0, 0.3f), 971);
			for (int32 k = 0; k < 4; ++k)
			{
				const FV At(0.42f + k * 0.07f, 0.5f - k * 0.03f);
				P.MLine({ At, At + FV(0.03f, -0.006f) }, 0.0016f, Hex(0xC8D4E4, 0.5f));
			}
			TArray<FV> Top;
			for (int32 k = 0; k <= 20; ++k)
			{
				FV Pos, Tan;
				const FV N = Side(k / 20.0f, Pos, Tan);
				Top.Add(Pos - N * FMath::Lerp(RHead, RTail, k / 20.0f) * 0.6f);
			}
			P.MLine(Top, 0.008f, Hex(0xFFFFFF, 0.22f));
		}
		// knobbly tubercles on the snout, barnacles, a small hump and dorsal fin
		for (int32 k = 0; k < 7; ++k)
		{
			const FV At(0.08f + k * 0.03f, 0.5f - FMath::Sin(k * 0.45f) * 0.01f + Rand01(972, k) * 0.012f);
			P.Fill([At](FV Q) { return SdCircle(Q, At, 0.0075f); }, Tone(Back, 1.25f));
		}
		HDBarnacle(P, FV(0.12f, 0.6f), 0.014f);
		HDBarnacle(P, FV(0.15f, 0.62f), 0.011f);
		P.Fill([](FV Q) { return SdPoly(Q, Smooth({ FV(0.6f, 0.47f), FV(0.64f, 0.42f), FV(0.66f, 0.46f) }, true, 3)) - 0.005f; }, Back);
		// the long mouth line and the small eye at its end
		P.MLine(Smooth({ FV(0.05f, 0.56f), FV(0.14f, 0.6f), FV(0.24f, 0.62f), FV(0.3f, 0.61f) }), 0.003f, Hex(0x10162A));
		HDEye(P, FV(0.32f, 0.58f), 0.012f, Hex(0x3A2A1A), FV(-0.2f, 0.0f), 0.65f);
		// near pectoral fin: very long, scalloped, white with dark edges
		const TArray<FV> Pec = Smooth({ FV(0.34f, 0.64f), FV(0.42f, 0.7f), FV(0.52f, 0.75f), FV(0.62f, 0.76f) });
		const auto PecFin = [&Pec](FV Q) { return SdRibbon(Q, Pec, 0.03f, 0.01f) + FMath::Abs(FMath::Sin(Q.X * 130.0f)) * 0.004f; };
		{
			FWith With(P);
			P.Look.Dome = 0.1f;
			P.Fill(PecFin, Belly);
		}
		{
			FClip Clip(P, PecFin);
			P.Mark([&Pec](FV Q) { return SdRibbon(Q, Pec, 0.022f, 0.006f) - 0.0f; }, Alpha(Back, 0.0f));
			P.MLine({ FV(0.34f, 0.62f), FV(0.62f, 0.745f) }, 0.006f, Alpha(Back, 0.45f));
		}
	}
	void HDHarpoonLauncher(FPainter& P)
	{

		// a weathered round piling, tapered and split, with real wood grain flowing around a knot
		const auto Post = [](FV Q)
		{
			const float W = 0.1f + (Q.Y - 0.45f) * 0.03f;
			return SdBox(Q, FV(0.47f - W, 0.46f), FV(0.47f + W, 0.87f), 0.03f) + Noise2(Q * 40.0f) * 0.003f;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.12f;
			P.Look.Spec = 0.2f;
			P.Fill(Post, Wood);
		}
		{
			FClip Clip(P, Post);
			const FV Knot(0.52f, 0.76f);
			// grain lines that part around the knot
			for (int32 i = 0; i < 14; ++i)
			{
				const float X0 = 0.38f + i * 0.0135f;
				TArray<FV> Line;
				for (int32 k = 0; k <= 16; ++k)
				{
					const float Y = 0.45f + k * 0.027f;
					FV Pt(X0 + Noise2(FV(i * 1.7f, Y * 9.0f)) * 0.004f, Y);
					const FV D = Pt - Knot;
					const float Push = 0.03f * FMath::Exp(-D.SizeSquared() / 0.0012f);
					Pt.X += (D.X >= 0.0f ? 1.0f : -1.0f) * Push;
					Line.Add(Pt);
				}
				P.MLine(Line, 0.0016f + (i % 3 == 0 ? 0.001f : 0.0f), Alpha(WoodDark, 0.55f));
			}
			P.MEllipse(Knot, 0.012f, 0.02f, Hex(0x3E2410));
			P.Mark([Knot](FV Q) { return FMath::Abs(SdEllipse(Q, Knot, 0.02f, 0.03f)) - 0.0018f; }, Alpha(WoodDark, 0.8f));
			// a long split, weathered silver top, and green slime at the waterline
			Crack(P, { FV(0.42f, 0.47f), FV(0.425f, 0.52f), FV(0.418f, 0.58f) }, 0.003f, Hex(0x2A1808, 0.9f));
			P.MEllipse(FV(0.47f, 0.47f), 0.12f, 0.03f, Hex(0xC8C0B0, 0.35f), 0.02f);
			P.MEllipse(FV(0.47f, 0.88f), 0.14f, 0.05f, Hex(0x4E7A2A, 0.75f), 0.03f);
			Speckle(P, FV(0.37f, 0.8f), FV(0.57f, 0.87f), 20, 0.002f, 0.005f, Hex(0x2E4A1A, 0.8f), 911);
			P.MCapsule(FV(0.4f, 0.48f), FV(0.39f, 0.86f), 0.012f, Hex(0xFFE8C0, 0.18f));
		}
		// rope lashed round the post, sagging slightly at the front
		for (int32 i = 0; i < 4; ++i)
		{
			const float Y = 0.565f + i * 0.05f;
			Rope(P, FV(0.355f, Y + 0.012f), FV(0.585f, Y - 0.01f), 0.024f);
		}
		// the loose end of the line hanging down in a curl
		P.Line(Smooth({ FV(0.58f, 0.7f), FV(0.64f, 0.74f), FV(0.66f, 0.8f), FV(0.63f, 0.84f), FV(0.6f, 0.82f) }), 0.009f, RopeColor);

		// the launcher: a curved wooden stock and iron barrel on a swivel, aimed up and to the right
		{
			FNest Place(P, FV(0.47f, 0.43f), 1.0f, -32.0f);
			// swivel yoke
			P.Fill([](FV Q) { return SdCapsule(Q, FV(0.0f, 0.02f), FV(0.0f, 0.07f), 0.03f); }, Iron);
			// stock with a curved grip
			const TArray<FV> Stock = Smooth({ FV(-0.3f, 0.06f), FV(-0.22f, 0.02f), FV(-0.1f, 0.0f), FV(0.1f, -0.005f) });
			P.Fill([&Stock](FV Q) { return SdRibbon(Q, Stock, 0.045f, 0.03f); }, Hex(0x8A5A2E));
			{
				FClip Clip(P, [&Stock](FV Q) { return SdRibbon(Q, Stock, 0.045f, 0.03f); });
				Strands(P, FV(-0.3f, 0.04f), FV(0.1f, 0.0f), 0.08f, 10, 0.0014f, Alpha(WoodDark, 0.6f), 912, 0.006f);
				P.MLine(Smooth({ FV(-0.28f, 0.035f), FV(-0.15f, 0.0f), FV(0.08f, -0.02f) }), 0.006f, Hex(0xFFE0B0, 0.35f));
			}
			// iron barrel with bands
			P.Fill([](FV Q) { return SdCapsule(Q, FV(-0.08f, -0.03f), FV(0.18f, -0.03f), 0.026f); }, Iron);
			{
				FClip Clip(P, [](FV Q) { return SdCapsule(Q, FV(-0.08f, -0.03f), FV(0.18f, -0.03f), 0.026f); });
				P.MCapsule(FV(-0.08f, -0.042f), FV(0.18f, -0.042f), 0.005f, Hex(0xC8D4DA, 0.6f));
				Speckle(P, FV(-0.1f, -0.06f), FV(0.2f, 0.0f), 30, 0.002f, 0.006f, Hex(0x9A5A2E, 0.5f), 913);
			}
			for (const float X : { -0.05f, 0.06f, 0.16f })
			{
				P.Fill([X](FV Q) { return SdBox(Q, FV(X - 0.012f, -0.062f), FV(X + 0.012f, 0.002f), 0.006f); }, Brass);
				Rivet(P, FV(X, -0.03f), 0.007f);
			}
			// the harpoon shaft and a forged, barbed head with a bright honed edge
			P.Fill([](FV Q) { return SdCapsule(Q, FV(0.12f, -0.03f), FV(0.36f, -0.03f), 0.011f); }, WoodLight);
			const TArray<FV> Head = Smooth({ FV(0.34f, -0.06f), FV(0.42f, -0.055f), FV(0.52f, -0.03f), FV(0.42f, -0.005f), FV(0.34f, 0.0f) }, true, 4);
			const TArray<FV> BarbA = Smooth({ FV(0.37f, -0.05f), FV(0.33f, -0.09f), FV(0.3f, -0.11f), FV(0.35f, -0.075f), FV(0.39f, -0.055f) }, true, 3);
			const TArray<FV> BarbB = Smooth({ FV(0.37f, -0.01f), FV(0.33f, 0.03f), FV(0.3f, 0.05f), FV(0.35f, 0.015f), FV(0.39f, -0.005f) }, true, 3);
			const auto Steelhead = [&Head, &BarbA, &BarbB](FV Q) { return SMin(SMin(SdPoly(Q, Head), SdPoly(Q, BarbA), 0.008f), SdPoly(Q, BarbB), 0.008f); };
			{
				FWith With(P);
				P.Look.Spec = 0.9f;
				P.Look.Dome = 0.03f;
				P.Fill(Steelhead, Steel);
			}
			{
				FClip Clip(P, Steelhead);
				P.MPoly({ FV(0.34f, -0.03f), FV(0.52f, -0.03f), FV(0.42f, -0.005f), FV(0.34f, 0.0f) }, Hex(0x5E6C76, 0.5f));
				P.MLine({ FV(0.36f, -0.054f), FV(0.5f, -0.032f) }, 0.003f, Hex(0xFFFFFF, 0.9f));
				P.MLine({ FV(0.34f, -0.03f), FV(0.5f, -0.03f) }, 0.0018f, Hex(0x3A464E, 0.8f));
			}
			// a socket collar binding the head to the shaft
			P.Fill([](FV Q) { return SdBox(Q, FV(0.32f, -0.046f), FV(0.35f, -0.014f), 0.006f); }, Brass);
			// line tied to the shaft, running back to the coil
			P.MLine(Smooth({ FV(0.2f, -0.02f), FV(0.12f, 0.05f), FV(0.05f, 0.1f) }), 0.004f, Hex(0xD9BE8A, 0.9f));
		}
		Bubble(P, FV(0.83f, 0.5f), 0.022f);
		Bubble(P, FV(0.86f, 0.42f), 0.014f);
	}

	void HDSkHeavyClubs2(FPainter& P)
	{
		// the mantis shrimp lashes out its club and shatters a snail shell, the water flashing where it strikes
		{
			FNest Place(P, FV(0.0f, 0.0f), 1.0f);
			HDMantisShrimp(P);
		}
		const FV Shell(0.91f, 0.5f), Hit(0.85f, 0.49f);
		HDSnailShell(P, Shell + FV(0.025f, 0.0f), 0.09f, Hex(0xC8A070), 3311);
		// cracks running out from the point of impact and chips flying off
		for (int32 k = 0; k < 6; ++k)
		{
			const float A = -60.0f + k * 24.0f;
			P.MLine({ Hit + FV(0.01f, 0.0f), Hit + FV(0.01f, 0.0f) + Spin(FV(0.025f, 0.0f), A + 10.0f), Hit + FV(0.01f, 0.0f) + Spin(FV(0.04f + Rand01(3312, k) * 0.03f, 0.0f), A) }, 0.0025f, Hex(0x2A1A10, 0.9f));
		}
		for (int32 k = 0; k < 6; ++k)
		{
			const FV At = Hit + FV(0.02f + Rand01(3313, k) * 0.12f, -0.03f - Rand01(3314, k) * 0.1f);
			const float S = 0.008f + Rand01(3315, k) * 0.008f;
			P.Fill([At, S, k](FV Q) { return SdPoly(Q, { At + Spin(FV(S, 0.0f), k * 40.0f), At + Spin(FV(S, 0.0f), k * 40.0f + 130.0f), At + Spin(FV(S * 0.7f, 0.0f), k * 40.0f + 230.0f) }); }, Hex(0xD8B080));
		}
		// the cavitation flash: a collapsing bubble of light between club and shell
		P.Glow([Hit](FV Q) { return SdCircle(Q, Hit, 0.01f); }, Hex(0xBFF4FF), 0.06f, 0.9f);
		P.Mark([Hit](FV Q) { return FMath::Abs(SdCircle(Q, Hit, 0.03f) + Noise2(Q * 60.0f) * 0.004f) - 0.0025f; }, Hex(0xE8FCFF, 0.85f));
		P.MCircle(Hit, 0.012f, Hex(0xFFFFFF, 0.95f));
		for (int32 k = 0; k < 7; ++k)
		{
			Bubble(P, Hit + Spin(FV(0.05f + Rand01(3320, k) * 0.04f, 0.0f), -150.0f + k * 40.0f), 0.004f + Rand01(3321, k) * 0.005f);
		}
	}

	void HDSkMoultedShell2(FPainter& P)
	{
		// the mantis shrimp, fresh and bright, beside the glassy empty skin it has just crawled out of, and the pearls it will fetch
		const FLinearColor Ghost = Hex(0xCFEFE6), Edge = Hex(0x4A9A88);
		const TArray<FV> Spine = Smooth({ FV(0.2f, 0.75f), FV(0.31f, 0.73f), FV(0.43f, 0.7f), FV(0.55f, 0.64f), FV(0.63f, 0.54f), FV(0.66f, 0.42f) });
		{
			// the moult: the same shape as the shrimp, hollow and see-through, split open along the back
			FNest Place(P, FV(-0.04f, 0.0f), 1.0f);
			const auto Skin = [&Spine](FV Q) { return SdRibbon(Q, Spine, 0.045f, 0.07f); };
			// empty legs and tail fan
			for (int32 i = 0; i < 3; ++i)
			{
				FV Pos, Tan;
				Along(Spine, 0.55f + i * 0.09f, Pos, Tan);
				const TArray<FV> Leg = { Pos, Pos + FV(0.03f - i * 0.01f, 0.07f), Pos + FV(-0.01f - i * 0.02f, 0.13f) };
				P.Mark([Leg](FV Q) { return SdPolyline(Q, Leg, 0.008f); }, Alpha(Ghost, 0.75f));
				P.Mark([Leg](FV Q) { return FMath::Abs(SdPolyline(Q, Leg, 0.008f)) - 0.0015f; }, Alpha(Edge, 0.8f));
			}
			for (int32 k = 0; k < 5; ++k)
			{
				const float T = -0.7f + k * 0.35f;
				const FV Root = Spine[0] + FV(0.01f, 0.0f), Tip = Root + FV(-FMath::Cos(T), FMath::Sin(T)) * 0.1f;
				P.Mark([Root, Tip](FV Q) { return SdRibbon(Q, { Root, Tip }, 0.012f, 0.022f); }, Alpha(Ghost, 0.7f));
				P.Mark([Root, Tip](FV Q) { return FMath::Abs(SdRibbon(Q, { Root, Tip }, 0.012f, 0.022f)) - 0.0015f; }, Alpha(Edge, 0.8f));
			}
			{
				FWith With(P);
				P.Look.Dome = 0.3f;
				P.Look.Spec = 0.95f;
				P.Look.Ink = 0.004f;
				P.Fill(Skin, Alpha(Ghost, 0.55f));
			}
			{
				FClip Clip(P, Skin);
				for (int32 k = 1; k < 11; ++k)
				{
					FV Pos, Tan;
					Along(Spine, k / 11.0f, Pos, Tan);
					const FV N(Tan.Y, -Tan.X);
					P.MCapsule(Pos - N * 0.08f, Pos + N * 0.08f, 0.004f, Alpha(Edge, 0.7f));
					P.MCapsule(Pos - N * 0.08f + Tan * 0.008f, Pos + N * 0.08f + Tan * 0.008f, 0.003f, Hex(0xFFFFFF, 0.6f));
				}
				// the slit along the back where the shrimp pulled itself free
				TArray<FV> Split;
				for (int32 k = 4; k <= 20; ++k)
				{
					FV Pos, Tan;
					Along(Spine, k / 20.0f, Pos, Tan);
					Split.Add(Pos + FV(Tan.Y, -Tan.X) * FMath::Lerp(0.045f, 0.07f, k / 20.0f) * 0.7f + FV(0.0f, (k % 2) * 0.004f));
				}
				P.MLine(Split, 0.005f, Hex(0x1A3A34, 0.55f));
			}
			P.Mark([&Skin](FV Q) { return FMath::Abs(Skin(Q)) - 0.002f; }, Alpha(Edge, 0.85f));
			// empty eye-stalk casings and a hollow club
			const FV Head = Spine.Last();
			for (int32 k = 0; k < 2; ++k)
			{
				const FV Base = Head + FV(-0.005f + k * 0.03f, -0.03f), E = Head + FV(-0.025f + k * 0.055f, -0.095f + k * 0.005f);
				P.Mark([Base, E](FV Q) { return SMin(SdRibbon(Q, { Base, E }, 0.012f, 0.009f), SdEllipse(Q, E, 0.026f, 0.02f), 0.01f); }, Alpha(Ghost, 0.75f));
				P.Mark([Base, E](FV Q) { return FMath::Abs(SMin(SdRibbon(Q, { Base, E }, 0.012f, 0.009f), SdEllipse(Q, E, 0.026f, 0.02f), 0.01f)) - 0.0015f; }, Alpha(Edge, 0.85f));
			}
			const FV Elbow = Head + FV(0.1f, 0.11f), Club = Head + FV(0.15f, 0.02f);
			P.Mark([Head, Elbow, Club](FV Q) { return SMin(SdRibbon(Q, { Head + FV(0.02f, 0.03f), Elbow }, 0.022f, 0.016f), SMin(SdCapsule(Q, Elbow, Club, 0.017f), SdEllipse(Q, Club, 0.036f, 0.03f), 0.02f), 0.01f); }, Alpha(Ghost, 0.7f));
			P.Mark([Head, Elbow, Club](FV Q) { return FMath::Abs(SMin(SdRibbon(Q, { Head + FV(0.02f, 0.03f), Elbow }, 0.022f, 0.016f), SMin(SdCapsule(Q, Elbow, Club, 0.017f), SdEllipse(Q, Club, 0.036f, 0.03f), 0.01f), 0.02f)) - 0.0015f; }, Alpha(Edge, 0.85f));
		}
		// the pearls it is worth, in a little heap on the sand
		HDMound(P, FV(0.5f, 0.92f), 0.42f, 0.06f, Hex(0xD8C090), 3401, false);
		for (const FVector Pr : { FVector(0.38f, 0.9f, 0.032f), FVector(0.46f, 0.91f, 0.036f), FVector(0.42f, 0.86f, 0.03f), FVector(0.53f, 0.9f, 0.028f) })
		{
			P.Ball(FV(Pr.X, Pr.Y), Pr.Z, Hex(0xF4F0EC), 0.95f);
			P.MCircle(FV(Pr.X, Pr.Y) + FV(Pr.Z * 0.3f, Pr.Z * 0.35f), Pr.Z * 0.4f, Hex(0xC8B8E0, 0.5f));
		}
		Glint(P, FV(0.41f, 0.84f), 0.08f);
	}

	void HDSkShrapnel2(FPainter& P)
	{
		// the puffer blasts out a volley of spines that stab into the seabed, leaving a patch of needles for anything that crosses it
		HDMound(P, FV(0.56f, 0.84f), 0.4f, 0.1f, Hex(0xD8C090), 3101, false);
		{
			FClip Clip(P, [](FV Q) { return SdEllipse(Q, FV(0.56f, 0.84f), 0.4f, 0.1f); });
			Pits(P, FV(0.16f, 0.74f), FV(0.96f, 0.94f), 22, 0.005f, Hex(0x8A7040, 0.5f), 3102);
		}
		// spines already planted in the sand: points buried, butts sticking back up toward the puffer they came from
		for (int32 k = 0; k < 8; ++k)
		{
			const FV Base(0.3f + k * 0.075f + (Rand01(3107, k) - 0.5f) * 0.03f, 0.82f + Rand01(3108, k) * 0.06f);
			const FV Dir = Spin(FV(1.0f, 0.0f), 52.0f + (Rand01(3110, k) - 0.5f) * 18.0f);
			const float Len = 0.15f + Rand01(3109, k) * 0.06f;
			{
				FClip Clip(P, [Base](FV Q) { return Q.Y - Base.Y - 0.002f; });
				HDPufferSpine(P, Base - Dir * Len, Base + Dir * 0.03f, 0.017f);
			}
			P.MEllipse(Base + FV(0.0f, 0.004f), 0.024f, 0.007f, Hex(0x8A7040, 0.6f), 0.004f);
			P.MEllipse(Base + FV(0.004f, 0.0f), 0.016f, 0.004f, Hex(0xE8D8A8, 0.8f), 0.003f);
		}
		// spines still in flight from the puffer, trailing bubbles
		for (int32 k = 0; k < 4; ++k)
		{
			const FV A = FV(0.4f + k * 0.08f, 0.5f + k * 0.02f);
			const FV Dir = Spin(FV(1.0f, 0.0f), 58.0f - k * 6.0f);
			for (int32 j = 0; j < 4; ++j)
			{
				Bubble(P, A - Dir * (0.03f + j * 0.03f), 0.007f - j * 0.001f);
			}
			HDPufferSpine(P, A, A + Dir * 0.15f, 0.016f);
		}
		// the puffer itself, puffed up and bristling
		{
			FNest Place(P, FV(-0.02f, -0.06f), 0.92f);
			HDPufferBody(P, 1.3f);
		}
	}

	void HDSkPotentVenom2(FPainter& P)
	{
		// the lionfish with its crest of venom spines raised, venom welling from every tip and running down into drops
		HDLionfishBody(P, true);
	}

	void HDSkMelody2(FPainter& P)
	{
		// the humpback singing, its song rippling out ahead of it through the water
		HDHumpbackBody(P);
		for (int32 k = 0; k < 4; ++k)
		{
			const FV From(0.08f, 0.52f);
			const float R = 0.06f + k * 0.05f;
			P.Glow([From, R](FV Q) { return SdArc(Q, From, R, 0.002f, 150.0f, 230.0f); }, Hex(0x8AE0FF), 0.02f, 0.4f - k * 0.07f);
			P.Mark([From, R](FV Q) { return SdArc(Q, From, R + FMath::Sin(FMath::Atan2(Q.Y - From.Y, Q.X - From.X) * 16.0f) * 0.005f, 0.004f, 150.0f, 230.0f); }, Hex(0xBFF4FF, 0.85f - k * 0.17f));
		}
		BubbleTrail(P, FV(0.3f, 0.44f), 4, 0.012f, 973);
	}

	void HDSkDeepChannel2(FPainter& P)
	{
		// far down in the deep sound channel a whale calls, and its song carries out and out through the dark water
		P.Mark([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.52f), 0.5f, 0.22f); }, Hex(0x0A2A4A, 0.6f), 0.15f);
		for (int32 k = 0; k < 5; ++k)
		{
			const float Rx = 0.2f + k * 0.075f, Ry = 0.08f + k * 0.028f;
			P.Glow([Rx, Ry](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.5f, 0.52f), Rx, Ry)) - 0.002f; }, Hex(0x6AD0F0), 0.025f, 0.4f - k * 0.06f);
			P.Mark([Rx, Ry, k](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.5f, 0.52f), Rx, Ry) + Noise2(Q * 18.0f + FV(k, 0.0f)) * 0.006f) - 0.004f + k * 0.0004f; }, Hex(0xBFF4FF, 0.75f - k * 0.13f));
		}
		{
			FNest Place(P, FV(0.29f, 0.28f), 0.44f);
			HDHumpbackBody(P);
		}
		Speckle(P, FV(0.05f, 0.25f), FV(0.95f, 0.85f), 30, 0.002f, 0.004f, Hex(0xE0F4FF, 0.55f), 4212);
	}

	void HDSkPassedOnSongs2(FPainter& P)
	{
		// a mother humpback with her calf tucked in beneath her, her song flowing back to the little one
		{
			FNest Place(P, FV(0.06f, -0.06f), 0.88f);
			HDHumpbackBody(P);
		}
		for (int32 k = 0; k < 3; ++k)
		{
			const TArray<FV> Path = Smooth({ FV(0.14f, 0.47f), FV(0.18f + k * 0.02f, 0.56f), FV(0.24f + k * 0.02f, 0.62f), FV(0.32f + k * 0.01f, 0.66f) });
			P.Glow([Path](FV Q) { return SdPolyline(Q, Path, 0.002f); }, Hex(0xFFE08A), 0.025f, 0.5f);
			P.Mark([Path](FV Q) { return SdRibbon(Q, Path, 0.005f, 0.0015f); }, Hex(0xFFF4C8, 0.9f));
		}
		{
			FNest Place(P, FV(0.3f, 0.43f), 0.46f);
			HDHumpbackBody(P);
		}
	}

	void HDSkRapidReload2(FPainter& P)
	{
		// the harpoon launcher firing shot after shot: one harpoon already streaking away, the next loaded, more racked and ready
		// the last shot, flying off with its line paying out behind it
		const FV Tail(0.66f, 0.18f), Tip(0.96f, -0.06f);
		for (int32 k = 0; k < 6; ++k)
		{
			Bubble(P, Tail + FV(-0.03f - k * 0.035f, 0.03f + k * 0.03f) + FV((Rand01(3060, k) - 0.5f) * 0.03f, 0.0f), 0.006f + k * 0.0015f);
		}
		P.MLine(Smooth({ FV(0.58f, 0.36f), FV(0.6f, 0.28f), Tail }), 0.004f, RopeColor);
		HDHarpoon(P, Tail, Tip, 0.012f);
		// spare harpoons racked against the post, lashed together
		for (int32 k = 0; k < 3; ++k)
		{
			const FV Foot(0.2f + k * 0.035f, 0.88f);
			HDHarpoon(P, Foot, Foot + FV(-0.06f + k * 0.03f, -0.48f), 0.011f);
		}
		Rope(P, FV(0.16f, 0.62f), FV(0.3f, 0.62f), 0.012f);
		HDHarpoonLauncher(P);
	}

	void HDSkStrongerAcid2(FPainter& P)
	{
		// a pool of seething vent acid, a shed crab claw half sunk in it and dissolving, fumes and bubbles boiling off
		const FLinearColor Acid = Hex(0xB8F040), AcidDeep = Hex(0x5A9A10);
		// the rocky basin and the acid in it
		HDMound(P, FV(0.5f, 0.74f), 0.46f, 0.2f, Hex(0x4E4A48), 4310, true);
		const auto Pool = [](FV Q) { return SdEllipse(Q, FV(0.5f, 0.7f), 0.38f, 0.13f) + Noise2(Q * 14.0f) * 0.012f; };
		P.Glow(Pool, Acid, 0.08f, 0.6f);
		{
			FWith With(P);
			P.Look.Dome = 0.06f;
			P.Look.Spec = 0.9f;
			P.Fill(Pool, AcidDeep);
		}
		{
			FClip Clip(P, Pool);
			P.MEllipse(FV(0.46f, 0.66f), 0.3f, 0.08f, Alpha(Acid, 0.8f), 0.05f);
			for (int32 k = 0; k < 4; ++k)
			{
				P.Mark([k](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.62f, 0.7f), 0.05f + k * 0.04f, 0.015f + k * 0.012f)) - 0.003f; }, Hex(0xE8FFB0, 0.6f - k * 0.12f));
			}
			Speckle(P, FV(0.14f, 0.6f), FV(0.86f, 0.8f), 40, 0.003f, 0.008f, Hex(0xE8FFB0, 0.7f), 4311);
		}
		// the crab claw sinking into it, pitted and eaten away below the surface line
		{
		FNest Big(P, FV(-0.406f, -0.462f), 1.7f);
		const auto Claw = [](FV Q)
		{
			const float Palm = SdEllipse(Q, FV(0.6f, 0.56f), 0.1f, 0.07f);
			const float Fixed = SdRibbon(Q, Smooth({ FV(0.55f, 0.52f), FV(0.5f, 0.4f), FV(0.52f, 0.3f), FV(0.57f, 0.26f) }), 0.035f, 0.007f);
			const float Moving = SdRibbon(Q, Smooth({ FV(0.67f, 0.5f), FV(0.7f, 0.4f), FV(0.66f, 0.31f), FV(0.6f, 0.28f) }), 0.03f, 0.007f);
			return FMath::Max(SMin(SMin(Palm, Fixed, 0.02f), Moving, 0.02f), Q.Y - 0.66f);
		};
		{
			FWith With(P);
			P.Look.Dome = 0.08f;
			P.Look.Spec = 0.7f;
			P.Fill(Claw, Hex(0xEE5E24));
		}
		{
			FClip Clip(P, Claw);
			// acid creeping up the shell, and the pits it has burned
			P.Mark([](FV Q) { return 0.58f - Q.Y + Noise2(Q * 30.0f) * 0.03f; }, Alpha(Acid, 0.7f), 0.01f);
			for (int32 k = 0; k < 10; ++k)
			{
				const FV At(0.48f + Rand01(4301, k) * 0.22f, 0.4f + Rand01(4302, k) * 0.24f);
				const float R = 0.007f + Rand01(4303, k) * 0.012f;
				P.Mark([At, R](FV Q) { return SdCircle(Q, At, R) + Noise2(Q * 90.0f) * 0.003f; }, Hex(0x2A1408, 0.9f));
				P.Mark([At, R](FV Q) { return FMath::Abs(SdCircle(Q, At, R + 0.003f)) - 0.0025f; }, Alpha(Acid, 0.85f));
			}
			P.MEllipse(FV(0.54f, 0.44f), 0.03f, 0.06f, Hex(0xFFFFFF, 0.3f), 0.02f);
		}
		// acid running off the claw tips back into the pool
		HDDrip(P, FV(0.505f, 0.36f), 0.06f, 0.014f, Acid, 0.0f, 0.005f);
		}
		// boiling bubbles and choking fumes
		for (int32 k = 0; k < 14; ++k)
		{
			const FV At(0.2f + Rand01(4304, k) * 0.6f, 0.62f - Rand01(4305, k) * 0.36f);
			P.Ball(At, 0.006f + Rand01(4306, k) * 0.01f, Alpha(Hex(0xD8FF90), 0.85f), 0.9f);
		}
		HDWisp(P, { FV(0.3f, 0.64f), FV(0.24f, 0.48f), FV(0.3f, 0.32f), FV(0.24f, 0.14f) }, 0.05f, Hex(0xC8F070, 0.8f), 7);
		HDWisp(P, { FV(0.76f, 0.64f), FV(0.82f, 0.48f), FV(0.76f, 0.3f), FV(0.82f, 0.16f) }, 0.045f, Hex(0xC8F070, 0.7f), 8);
	}

	void HDSkRoughEdges2(FPainter& P)
	{
		// a sling stone knapped to a jagged, razor-sharp edge, spinning through the water, flakes still breaking from it
		const TArray<FV> Flint = { FV(0.4f, 0.18f), FV(0.5f, 0.2f), FV(0.58f, 0.16f), FV(0.66f, 0.26f), FV(0.72f, 0.27f), FV(0.78f, 0.38f), FV(0.86f, 0.5f), FV(0.8f, 0.56f), FV(0.76f, 0.68f), FV(0.66f, 0.7f),
			FV(0.6f, 0.78f), FV(0.5f, 0.74f), FV(0.4f, 0.8f), FV(0.34f, 0.7f), FV(0.24f, 0.62f), FV(0.27f, 0.52f), FV(0.22f, 0.42f), FV(0.3f, 0.34f), FV(0.3f, 0.26f) };
		// the swirl of water it spins through
		for (int32 k = 0; k < 3; ++k)
		{
			P.MArc(FV(0.53f, 0.49f), 0.4f + k * 0.04f, 0.004f, 120.0f + k * 20.0f, 200.0f + k * 15.0f, Hex(0xBFEFFF, 0.45f - k * 0.12f));
		}
		HDFlint(P, Flint, FV(0.56f, 0.46f), Hex(0x3A4A5A));
		// flakes chipping off its edges
		for (int32 k = 0; k < 8; ++k)
		{
			const FV At = FV(0.53f, 0.49f) + Spin(FV(0.4f + Rand01(3601, k) * 0.1f, 0.0f), -60.0f + k * 35.0f + Rand01(3602, k) * 15.0f);
			const float S = 0.01f + Rand01(3603, k) * 0.016f;
			P.Fill([At, S, k](FV Q) { return SdPoly(Q, { At + Spin(FV(S, 0.0f), k * 50.0f), At + Spin(FV(S, 0.0f), k * 50.0f + 120.0f), At + Spin(FV(S * 0.6f, 0.0f), k * 50.0f + 230.0f) }); }, Hex(0x4A5A6A));
		}
		// light catching the sharpest edges
		Glint(P, FV(0.86f, 0.56f), 0.1f);
		Glint(P, FV(0.44f, 0.16f), 0.07f);
	}

	void HDSkNeurotoxin2(FPainter& P)
	{
		// a violet tentacle has stung a fish, and the nerve venom is spreading through it, lighting up every nerve on its way
		const FLinearColor Violet = Hex(0xA05AE8), Pale = Hex(0xE8D0FF);
		const FV C(0.52f, 0.6f);
		const float S = 1.3f;
		HDFish(P, C, S, Hex(0x7A90A8), Hex(0xE4ECF4), -1.0f);
		// the nerves: branching violet light spreading from the sting along the body
		const FV Sting(0.64f, 0.55f);
		const auto Body = [C, S](FV Q) { return SdEllipse(Q, C, 0.24f * S, 0.11f * S); };
		{
			FClip Clip(P, Body);
			P.MEllipse(Sting, 0.08f, 0.05f, Alpha(Violet, 0.4f), 0.05f);
			for (int32 k = 0; k < 9; ++k)
			{
				const float A = 120.0f + k * 22.0f + Rand01(3051, k) * 10.0f;
				TArray<FV> Nerve = { Sting };
				FV At = Sting;
				for (int32 j = 0; j < 5; ++j)
				{
					At += Spin(FV(0.045f, 0.0f), A + (Rand01(3052, k * 7 + j) - 0.5f) * 50.0f);
					Nerve.Add(At);
				}
				const TArray<FV> Sm = Smooth(Nerve);
				P.Glow([Sm](FV Q) { return SdPolyline(Q, Sm, 0.003f); }, Hex(0xB070FF), 0.02f, 0.9f);
				P.Mark([Sm](FV Q) { return SdRibbon(Q, Sm, 0.006f, 0.0015f); }, Hex(0xF4E0FF));
				FV Pos, Tan;
				Along(Sm, 0.5f, Pos, Tan);
				P.MLine({ Pos, Pos + Spin(Tan * 0.04f, 35.0f) }, 0.0015f, Hex(0xF0D8FF, 0.8f));
			}
			P.Ball(Sting, 0.012f, Hex(0xFFD8FA), 0.9f);
		}
		// the fish's eye glazed and its fins locked rigid
		P.MCircle(C + FV(-0.17f * S, -0.025f * S), 0.024f, Hex(0xC8B0E8, 0.6f));
		// the tentacle that stung it, hanging down from above, still touching
		const TArray<FV> Path = Smooth({ FV(0.78f, 0.02f), FV(0.72f, 0.18f), FV(0.78f, 0.32f), FV(0.68f, 0.46f), FV(0.63f, 0.55f) }, false, 8);
		const auto Tent = [&Path](FV Q) { return SdRibbon(Q, Path, 0.04f, 0.012f) - FMath::Abs(FMath::Sin(Q.Y * 80.0f)) * 0.0025f; };
		P.Glow(Tent, Violet, 0.04f, 0.35f);
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Spec = 0.8f;
			P.Fill(Tent, Alpha(Violet, 0.85f));
		}
		{
			FClip Clip(P, Tent);
			for (int32 k = 1; k < 14; ++k)
			{
				FV Pos, Tan;
				Along(Path, k / 14.0f, Pos, Tan);
				const FV N(-Tan.Y, Tan.X);
				const float W = FMath::Lerp(0.04f, 0.012f, k / 14.0f);
				for (const float F : { -0.5f, 0.0f, 0.5f })
				{
					P.MCircle(Pos + N * W * F, W * 0.16f, Hex(0xFFFFFF, 0.75f));
				}
			}
			P.Mark([&Path](FV Q) { TArray<FV> L; for (const FV& V : Path) L.Add(V + FV(-0.01f, -0.004f)); return SdPolyline(Q, L, 0.004f); }, Hex(0xFFFFFF, 0.35f));
		}
		// stinging threads fired into the fish
		for (int32 k = 0; k < 4; ++k)
		{
			const FV A = Sting + FV(0.01f + k * 0.008f, -0.02f - k * 0.01f);
			P.MLine(Smooth({ A, A + FV(-0.02f, 0.01f), A + FV(-0.035f, 0.025f) }), 0.0015f, Alpha(Pale, 0.8f));
		}
	}

	void HDSkEightArms2(FPainter& P)
	{
		// the octopus with all eight arms flung wide, every one of them curled round its own stone
		const FLinearColor Skin = Hex(0xEE6A44), Deep = Hex(0xA8341E), Pale = Hex(0xFFC0A0), Sucker = Hex(0xFFE0CC);
		const float Angles[] = { 160.0f, 190.0f, 218.0f, 246.0f, 20.0f, -10.0f, -38.0f, -66.0f };
		const FV Hub(0.5f, 0.55f);
		TArray<TArray<FV>> Arms;
		TArray<FV> Stones;
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = FMath::DegreesToRadians(Angles[i]);
			const FV D(FMath::Cos(A), -FMath::Sin(A)), N(-D.Y, D.X);
			const float W = (i % 2 ? 1.0f : -1.0f) * (i < 4 ? 1.0f : -1.0f);
			const FV Tip = Hub + D * 0.36f;
			Arms.Add(Smooth({ Hub + D * 0.04f, Hub + D * 0.14f + N * 0.03f * W, Hub + D * 0.25f - N * 0.03f * W, Tip, Tip + D * 0.02f + N * 0.04f * (i < 4 ? -1.0f : 1.0f) }, false, 6));
			Stones.Add(Tip + D * 0.045f + N * 0.01f * (i < 4 ? -1.0f : 1.0f));
		}
		// the stones first, so the arm tips wrap over them
		for (int32 i = 0; i < 8; ++i)
		{
			HDStone(P, Stones[i], 0.042f, 3701 + i, i % 2 ? Hex(0x8A8A80) : Hex(0x7A8A90));
		}
		const float R0 = 0.05f, R1 = 0.012f;
		const auto Mantle = [](FV Q) { return SMin(SdEllipse(Q, FV(0.5f, 0.36f), 0.14f, 0.16f), SdEllipse(Q, FV(0.5f, 0.53f), 0.11f, 0.08f), 0.06f); };
		const auto Whole = [&Arms, R0, R1, &Mantle](FV Q)
		{
			float D = Mantle(Q);
			for (const TArray<FV>& A : Arms)
			{
				D = SMin(D, SdRibbon(Q, A, R0, R1), 0.04f);
			}
			return D;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.14f;
			P.Look.Spec = 0.55f;
			P.Fill(Whole, Skin);
		}
		{
			FClip Clip(P, Whole);
			for (int32 a = 0; a < Arms.Num(); ++a)
			{
				const float Side = a < 4 ? -1.0f : 1.0f;
				for (int32 k = 3; k < 14; ++k)
				{
					FV Pos, Tan;
					const float T = k / 14.0f;
					Along(Arms[a], T, Pos, Tan);
					const float R = FMath::Lerp(R0, R1, T);
					const FV C = Pos + FV(-Tan.Y, Tan.X) * Side * R * 0.55f;
					const float Sr = R * 0.32f;
					P.MCircle(C + FV(Sr * 0.2f, Sr * 0.25f), Sr * 1.1f, Alpha(Deep, 0.6f));
					P.MCircle(C, Sr, Sucker);
					P.MCircle(C + FV(Sr * 0.1f, Sr * 0.15f), Sr * 0.45f, Hex(0xD87A6A));
				}
			}
			Speckle(P, FV(0.34f, 0.18f), FV(0.66f, 0.6f), 90, 0.003f, 0.008f, Alpha(Deep, 0.4f), 951);
			Granules(P, Mantle, FV(0.36f, 0.2f), FV(0.64f, 0.5f), 30, 0.005f, 0.009f, Alpha(Deep, 0.55f), Pale, 952);
			P.MEllipse(FV(0.45f, 0.27f), 0.06f, 0.035f, Hex(0xFFFFFF, 0.3f), 0.03f);
		}
		for (const float F : { -1.0f, 1.0f })
		{
			const FV E(0.5f + F * 0.062f, 0.45f);
			P.Fill([E](FV Q) { return SdEllipse(Q, E, 0.043f, 0.038f); }, Tone(Skin, 0.88f));
			FClip Clip(P, [E](FV Q) { return SdEllipse(Q, E, 0.034f, 0.03f); });
			P.Ball(E, 0.034f, Hex(0xFFD848), 0.4f);
			P.MCapsule(E - FV(0.017f, 0.0f), E + FV(0.017f, 0.0f), 0.008f, Hex(0x0A0806));
			P.Fill([E](FV Q) { return (Q.Y - E.Y) + 0.017f; }, Tone(Skin, 0.8f));
			P.MCircle(E + FV(-0.01f, 0.01f), 0.005f, Hex(0xFFFFFF, 0.9f));
		}
	}

	void HDSkConductor2(FPainter& P)
	{
		// a bolt striking a riveted iron armour plate: the metal carries the current all over, every rivet and edge blazing
		HDShield(P, FV(0.5f, 0.54f), 0.9f, Hex(0x8A9AA4));
		const TArray<FV> Outline = HDShieldOutline(FV(0.5f, 0.54f), 0.9f);
		{
			FClip Clip(P, [&Outline](FV Q) { return SdPoly(Q, Outline); });
			// current crawling across the plate in fine forking arcs
			for (int32 k = 0; k < 5; ++k)
			{
				const FV A = FV(0.56f, 0.36f), B = FV(0.5f, 0.54f) + Spin(FV(0.32f, 0.0f), -10.0f + k * 50.0f);
				HDLightning(P, A, B, 3230 + k, 0.003f);
			}
			P.MEllipse(FV(0.56f, 0.36f), 0.1f, 0.08f, Hex(0xFFF0A0, 0.45f), 0.07f);
		}
		// the rim glowing as the charge runs round it
		P.Glow([&Outline](FV Q) { return FMath::Abs(SdPoly(Q, Outline)) - 0.004f; }, Hex(0xFFE870), 0.04f, 0.7f);
		P.Mark([&Outline](FV Q) { return FMath::Abs(SdPoly(Q, Outline)) - 0.003f; }, Hex(0xFFFBE0));
		for (const FV R : { FV(-0.3f, -0.36f), FV(0.3f, -0.36f), FV(-0.3f, 0.0f), FV(0.3f, 0.0f), FV(0.0f, 0.4f) })
		{
			const FV At = FV(0.5f, 0.54f) + R * 0.9f;
			P.Glow([At](FV Q) { return SdCircle(Q, At, 0.008f); }, Hex(0xFFE870), 0.03f, 0.9f);
			P.MCircle(At, 0.009f, Hex(0xFFFBE0));
		}
		// the strike from above, and sparks spitting off the edges
		HDLightning(P, FV(0.78f, -0.06f), FV(0.56f, 0.36f), 3221, 0.008f);
		HDSpark(P, FV(0.56f, 0.36f), 0.12f, Hex(0xFFF0A0));
		for (const FV S : { FV(0.14f, 0.36f), FV(0.86f, 0.5f), FV(0.3f, 0.88f) })
		{
			HDSpark(P, S, 0.05f, Hex(0xFFF0A0));
		}
	}

	bool PaintSkillHD(FPainter& P, ETDTowerType Tower, int32 Index)
	{
		using FSkill = void (*)(FPainter&);
		static const FSkill Skills[][3] = {
			{ HDSkPiercingBolt, HDSkRapidReload2, HDSkPoisonedTip },
			{ HDSkDeepSting, HDSkNeurotoxin2, HDSkWideBloom },
			{ HDSkBigSpines, HDSkShrapnel2, HDSkConcussion },
			{ HDSkOvercharge, HDSkParalyzingShock, HDSkConductor2 },
			{ HDSkHeavyClubs2, HDSkMoultedShell2, HDSkKeenEyes },
			{ HDSkRoughEdges2, HDSkEightArms2, HDSkStrongThrow },
			{ HDSkPotentVenom2, HDSkLingeringVenom, HDSkReefForager },
			{ HDSkSerratedTeeth, HDSkToothSpray, HDSkElectricSnout },
			{ HDSkMelody2, HDSkDeepChannel2, HDSkPassedOnSongs2 },
			{ HDSkStrongerAcid2, HDSkSiltPlume, HDSkMineralGrowth } };
		int32 Row = -1;
		switch (Tower)
		{
		case ETDTowerType::Harpoon:		Row = 0; break;
		case ETDTowerType::Jelly:		Row = 1; break;
		case ETDTowerType::Puffer:		Row = 2; break;
		case ETDTowerType::Eel:			Row = 3; break;
		case ETDTowerType::Mantis:		Row = 4; break;
		case ETDTowerType::Octopus:		Row = 5; break;
		case ETDTowerType::Lionfish:	Row = 6; break;
		case ETDTowerType::Sawfish:		Row = 7; break;
		case ETDTowerType::Humpback:	Row = 8; break;
		case ETDTowerType::AcidVent:	Row = 9; break;
		default:						break;
		}
		if (Row < 0 || Index < 0 || Index > 2)
		{
			return false;
		}
		Skills[Row][Index](P);
		return true;
	}

	/**
	 *  A gastropod shell from its apex A to the end of its body whorl B: a spire of stacked, shouldered whorls
	 *  separated by spiralling sutures, a swollen body whorl, and an aperture with a flared lip on the +N side.
	 */
	void HDSpiralShell(FPainter& P, FV A, FV B, float Width, const FLinearColor& Color, const FLinearColor& Band, int32 Whorls)
	{
		const FV D = (B - A).GetSafeNormal();
		const FV N(-D.Y, D.X);
		const float Len = (B - A).Size();
		// whorl boundaries along the axis (closer together towards the apex)
		TArray<float> Sut;
		for (int32 i = 1; i <= Whorls; ++i)
		{
			Sut.Add(FMath::Pow((float)i / (Whorls + 1), 0.8f));
		}
		const auto Half = [&Sut, Width](float T)
		{
			float W = Width * FMath::Pow(FMath::Max(T, 0.0f), 0.85f);
			// each whorl bulges between its sutures
			float Prev = 0.0f;
			for (const float S : Sut)
			{
				if (T < S)
				{
					break;
				}
				Prev = S;
			}
			float Next = 1.0f;
			for (const float S : Sut)
			{
				if (S > T)
				{
					Next = S;
					break;
				}
			}
			const float Ph = (T - Prev) / FMath::Max(Next - Prev, 0.001f);
			W *= 1.0f + 0.18f * FMath::Sin(Ph * PI) * (T < Sut.Last() ? 1.0f : 0.0f);
			// the body whorl swells, then tucks in to the siphonal canal
			if (T >= Sut.Last())
			{
				const float U = (T - Sut.Last()) / (1.0f - Sut.Last());
				W *= 1.0f + 0.12f * FMath::Sin(FMath::Min(U * 1.4f, 1.0f) * PI * 0.5f);
			}
			return W;
		};
		TArray<FV> Outline;
		const int32 Steps = 48;
		for (int32 i = 0; i <= Steps; ++i)
		{
			const float T = (float)i / Steps;
			Outline.Add(A + D * (Len * T) + N * Half(T));
		}
		Outline.Add(B + D * (Width * 0.25f));
		for (int32 i = Steps; i >= 0; --i)
		{
			const float T = (float)i / Steps;
			Outline.Add(A + D * (Len * T) - N * Half(T));
		}
		const auto Shape = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		{
			FWith With(P);
			P.Look.Dome = 0.3f;
			P.Look.Spec = 0.75f;
			P.Fill(Shape, Color);
		}
		{
			FClip Clip(P, Shape);
			const auto Local = [A, D, N, Len](FV Q, float& T, float& V) { const FV L = Q - A; T = (L.X * D.X + L.Y * D.Y) / Len; V = L.X * N.X + L.Y * N.Y; };
			// each whorl rounded: shade near its sutures, light across its middle
			for (int32 i = 0; i < Sut.Num(); ++i)
			{
				const float S = Sut[i];
				const float Slant = 0.06f;
				P.Mark([&Local, S, Slant, Width](FV Q) { float T, V; Local(Q, T, V); return (FMath::Abs(T - S - V / Width * Slant) - 0.035f) * 0.2f; }, Alpha(Tone(Band, 0.6f), 0.35f), 0.006f);
				P.Mark([&Local, S, Slant, Width](FV Q) { float T, V; Local(Q, T, V); return (FMath::Abs(T - S - V / Width * Slant) - 0.006f) * 0.2f; }, Alpha(Tone(Band, 0.5f), 0.95f));
				P.Mark([&Local, S, Slant, Width](FV Q) { float T, V; Local(Q, T, V); return (FMath::Abs(T - S - 0.014f - V / Width * Slant) - 0.004f) * 0.2f; }, Hex(0xFFFFFF, 0.4f));
			}
			// colour bands spiralling round each whorl, and fine growth lines
			P.Mark([&Local, Width](FV Q) { float T, V; Local(Q, T, V); return (FMath::Abs(FMath::Sin(T * 24.0f + V / Width * 0.8f)) - 0.8f) * 0.01f; }, Alpha(Band, 0.22f), 0.004f);
			for (int32 i = 0; i < 26; ++i)
			{
				const float T = 0.15f + Rand01(301, i) * 0.8f;
				const FV At = A + D * (Len * T) + N * (Half(T) * (Rand01(302, i) - 0.5f) * 1.5f);
				P.MLine({ At, At + D * 0.01f + N * 0.007f, At + D * 0.02f, At + D * 0.03f + N * 0.007f }, 0.0028f, Alpha(Band, 0.7f));
			}
			// a soft long highlight down the shell
			P.MLine({ A + D * (Len * 0.15f) - N * (Width * 0.1f), A + D * (Len * 0.9f) - N * (Width * 0.55f) }, Width * 0.06f, Hex(0xFFFFFF, 0.14f));
		}
	}
