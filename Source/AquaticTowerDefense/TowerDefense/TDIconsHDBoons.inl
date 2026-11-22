// Hand-drawn HD boon icons (atd.DetailedIcons 2) for the boons whose detailed look was built from flat symbols.
// Included from TDIcons.cpp inside its anonymous namespace, just before PaintBoon; the curse aura is still
// painted by PaintBoon before these run.

	/** A branching coral from Root: each branch a tapering, slightly kinked limb melted into its parent, with polyps */
	void HDCoralBranches(FPainter& P, FV Root, float Len, float W, float Angle, int32 Depth, const FLinearColor& Color, int32 Seed, TArray<TArray<FV>>* OutTips = nullptr)
	{
		TArray<TArray<FV>> Limbs;
		TArray<float> Widths;
		struct FBranch { FV From; float Len; float W; float Angle; int32 Depth; };
		TArray<FBranch> Todo = { { Root, Len, W, Angle, Depth } };
		int32 Count = 0;
		while (Todo.Num() > 0)
		{
			const FBranch B = Todo.Last();
			Todo.RemoveAt(Todo.Num() - 1);
			const FV Dir = Spin(FV(0.0f, -1.0f), B.Angle);
			const FV Mid = B.From + Dir * B.Len * 0.5f + FV(-Dir.Y, Dir.X) * B.Len * (Rand01(Seed, Count) - 0.5f) * 0.2f;
			const FV End = B.From + Dir * B.Len;
			Limbs.Add(Smooth({ B.From, Mid, End }));
			Widths.Add(B.W);
			++Count;
			if (B.Depth > 0)
			{
				const float Spread = 28.0f + Rand01(Seed, Count + 50) * 14.0f;
				Todo.Add({ End, B.Len * 0.72f, B.W * 0.72f, B.Angle - Spread, B.Depth - 1 });
				Todo.Add({ End, B.Len * 0.68f, B.W * 0.72f, B.Angle + Spread * 0.9f, B.Depth - 1 });
			}
		}
		const auto Coral = [&Limbs, &Widths](FV Q)
		{
			float D = 1e6f;
			for (int32 i = 0; i < Limbs.Num(); ++i)
			{
				D = SMin(D, SdRibbon(Q, Limbs[i], Widths[i], Widths[i] * 0.72f), Widths[i] * 0.6f);
			}
			return D;
		};
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.5f;
			P.Fill(Coral, Color);
		}
		FClip Clip(P, Coral);
		// tiny polyp cups dotted over every limb, darker undersides and a lit top edge
		for (int32 i = 0; i < Limbs.Num(); ++i)
		{
			for (int32 k = 0; k < 6; ++k)
			{
				FV Pos, Tan;
				Along(Limbs[i], (k + 0.5f) / 6.0f, Pos, Tan);
				const FV At = Pos + FV(-Tan.Y, Tan.X) * Widths[i] * (Rand01(Seed + 7, i * 6 + k) - 0.5f) * 1.2f;
				P.MCircle(At, Widths[i] * 0.22f, Alpha(Tone(Color, 0.55f), 0.7f));
				P.MCircle(At - FV(Widths[i] * 0.06f, Widths[i] * 0.06f), Widths[i] * 0.09f, Alpha(Tone(Color, 1.5f), 0.8f));
			}
		}
		if (OutTips)
		{
			*OutTips = Limbs;
		}
	}

	/** A whirlpool at C: spiral arms of water that thin to foam towards the eye */
	void HDWhirlpool(FPainter& P, FV C, float R, const FLinearColor& Deep, const FLinearColor& Light, int32 Arms, float Turns)
	{
		P.Fill([C, R](FV Q) { return SdCircle(Q, C, R) + Noise2(Q * 16.0f) * 0.01f; }, Deep);
		FClip Clip(P, [C, R](FV Q) { return SdCircle(Q, C, R); });
		for (int32 a = 0; a < Arms; ++a)
		{
			TArray<FV> Arm;
			for (int32 k = 0; k <= 30; ++k)
			{
				const float T = k / 30.0f;
				const float Ang = a * 2.0f * PI / Arms + T * Turns * 2.0f * PI;
				Arm.Add(C + FV(FMath::Cos(Ang), FMath::Sin(Ang)) * R * (1.05f - T * 0.92f));
			}
			{
				FWith With(P);
				P.Look.Dome = 0.3f;
				P.Look.Spec = 0.9f;
				P.Look.Ink = 0.005f;
				P.Fill([Arm, R](FV Q) { return SdRibbon(Q, Arm, R * 0.16f, R * 0.02f); }, Light);
			}
			P.MLine(Arm, R * 0.025f, Hex(0xFFFFFF, 0.6f));
		}
		// the dark eye of the vortex
		P.Mark([C, R](FV Q) { return SdCircle(Q, C, R * 0.16f); }, Hex(0x041020, 0.9f), R * 0.1f);
		Speckle(P, C - FV(R, R), C + FV(R, R), 40, R * 0.01f, R * 0.025f, Hex(0xFFFFFF, 0.7f), 1501);
	}

	void HDBoonSharpenedReef(FPainter& P)
	{
		// a blade knapped from coral rock: wavy, chipped edges honed bright, bound to a driftwood grip
		const FV Root(0.3f, 0.7f), Tip(0.86f, 0.12f);
		const FV D = (Tip - Root).GetSafeNormal(), N(-D.Y, D.X);
		TArray<FV> Edge;
		for (int32 k = 0; k <= 12; ++k)
		{
			const float T = k / 12.0f;
			Edge.Add(FMath::Lerp(Root, Tip, T) + N * (0.075f * (1.0f - T * T) + 0.006f * FMath::Sin(k * 2.6f)));
		}
		for (int32 k = 12; k >= 0; --k)
		{
			const float T = k / 12.0f;
			Edge.Add(FMath::Lerp(Root, Tip, T) - N * (0.07f * (1.0f - T * T) + 0.006f * FMath::Sin(k * 3.1f)));
		}
		const auto Blade = [&Edge](FV Q) { return SdPoly(Q, Edge); };
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.8f;
			P.Fill(Blade, Hex(0xFF8A78));
		}
		{
			FClip Clip(P, Blade);
			// coral texture: tiny corallite pits, flake scars along the edges, a bright honed rim
			Pits(P, Root - FV(0.1f, 0.1f), Tip + FV(0.1f, 0.1f), 40, 0.006f, Hex(0xA83A34, 0.55f), 1511);
			for (int32 k = 0; k < 10; ++k)
			{
				const FV At = FMath::Lerp(Root, Tip, 0.05f + k * 0.09f) + N * ((k % 2) ? 0.06f : -0.06f) * (1.0f - k * 0.08f);
				P.Mark([At](FV Q) { return FMath::Abs(SdCircle(Q, At, 0.03f)) - 0.002f; }, Hex(0xFFE0D8, 0.45f));
			}
			P.Mark([&Edge](FV Q) { return FMath::Abs(SdPoly(Q, Edge) + 0.006f) - 0.004f; }, Hex(0xFFF4F0, 0.85f));
			P.MLine({ Root, Tip }, 0.004f, Hex(0xA83A34, 0.5f));
		}
		// the grip and its cord lashing
		P.Fill([Root, D](FV Q) { return SdRibbon(Q, { Root - D * 0.24f, Root + D * 0.02f }, 0.04f, 0.036f) + Noise2(Q * 60.0f) * 0.003f; }, Wood);
		for (int32 k = 0; k < 4; ++k)
		{
			const FV C = Root - D * (0.04f + k * 0.045f);
			Rope(P, C - N * 0.045f, C + N * 0.045f, 0.013f);
		}
		P.Fill([Root, N](FV Q) { return SdCapsule(Q, Root - N * 0.1f, Root + N * 0.1f, 0.022f); }, Hex(0xC24A44));
		HDSpark(P, Tip + FV(-0.03f, 0.04f), 0.16f);
	}

	void HDBoonTidalFury(FPainter& P)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			const float Y = 0.32f + i * 0.16f;
			P.Mark([Y, i](FV Q) { return SdRibbon(Q, { FV(0.04f + i * 0.04f, Y), FV(0.26f, Y) }, 0.004f, 0.022f); }, Hex(0xBFEFFF, 0.9f));
		}
		HDWhirlpool(P, FV(0.58f, 0.5f), 0.34f, Hex(0x1E6AB0), Hex(0x5AC0EE), 3, 0.85f);
	}

	void HDBoonCoralMasons(FPainter& P)
	{
		// a little wall of cut coral blocks, mortar oozing, and a trowel laying the next course
		const FLinearColor Block = Hex(0xF08070);
		for (int32 Row = 0; Row < 3; ++Row)
		{
			for (int32 i = 0; i < 3 - (Row == 0 ? 1 : 0); ++i)
			{
				const float X0 = 0.12f + i * 0.17f + (Row % 2) * 0.085f, Y0 = 0.82f - Row * 0.13f;
				const FV Mn(X0, Y0 - 0.12f), Mx(X0 + 0.16f, Y0);
				const int32 Id = Row * 3 + i;
				const auto Brick = [Mn, Mx, Id](FV Q) { return SdBox(Q, Mn, Mx, 0.018f) + Noise2(Q * 40.0f + FV(Id * 3.0f, 0.0f)) * 0.006f; };
				{
					FWith With(P);
					P.Look.Dome = 0.08f;
					P.Look.Grain = 0.1f;
					P.Fill(Brick, Tone(Block, 0.92f + 0.05f * (Id % 3)));
				}
				FClip Clip(P, Brick);
				Pits(P, Mn, Mx, 10, 0.006f, Hex(0x9A3A30, 0.6f), 1521 + Id);
				Speckle(P, Mn, Mx, 10, 0.002f, 0.004f, Hex(0xFFE0D8, 0.6f), 1531 + Id);
				P.MLine({ Mn + FV(0.01f, 0.008f), FV(Mx.X - 0.01f, Mn.Y + 0.008f) }, 0.004f, Hex(0xFFFFFF, 0.35f));
			}
		}
		// mortar squeezing out between the top course
		P.Fill([](FV Q) { return SdRibbon(Q, Smooth({ FV(0.2f, 0.565f), FV(0.32f, 0.56f), FV(0.46f, 0.57f), FV(0.56f, 0.56f) }), 0.012f, 0.01f) + Noise2(Q * 50.0f) * 0.004f; }, Hex(0xE8E0CC));
		// the trowel: a worn steel blade on a wooden handle
		{
			FNest Place(P, FV(0.66f, 0.4f), 1.0f, -38.0f);
			const TArray<FV> Blade = Smooth({ FV(0.0f, 0.0f), FV(0.24f, -0.05f), FV(0.3f, 0.0f), FV(0.24f, 0.05f) }, true, 4);
			{
				FWith With(P);
				P.Look.Spec = 0.95f;
				P.Look.Dome = 0.04f;
				P.Fill([&Blade](FV Q) { return SdPoly(Q, Blade); }, Steel);
			}
			{
				FClip Clip(P, [&Blade](FV Q) { return SdPoly(Q, Blade); });
				P.MPoly({ FV(0.0f, 0.0f), FV(0.3f, 0.0f), FV(0.24f, 0.05f) }, Hex(0x5E6C76, 0.35f));
				P.MCircle(FV(0.22f, 0.02f), 0.025f, Hex(0xE8E0CC, 0.85f));
			}
			P.Fill([](FV Q) { return SdRibbon(Q, Smooth({ FV(0.02f, 0.0f), FV(-0.04f, -0.06f), FV(-0.08f, -0.1f) }), 0.008f, 0.008f); }, SteelDark);
			P.Fill([](FV Q) { return SdRibbon(Q, { FV(-0.08f, -0.1f), FV(-0.2f, -0.18f) }, 0.026f, 0.022f); }, Wood);
			P.MLine({ FV(-0.09f, -0.115f), FV(-0.19f, -0.19f) }, 0.006f, Hex(0xFFE8C0, 0.4f));
		}
	}

	/** A rounded heart outline from the classic parametric curve, Width wide, centred on C */
	TArray<FV> HDHeartPts(FV C, float Width)
	{
		TArray<FV> Pts;
		for (int32 k = 0; k < 72; ++k)
		{
			const float T = k * 2.0f * PI / 72.0f;
			const float Sn = FMath::Sin(T);
			const float X = 16.0f * Sn * Sn * Sn;
			const float Y = -(13.0f * FMath::Cos(T) - 5.0f * FMath::Cos(2.0f * T) - 2.0f * FMath::Cos(3.0f * T) - FMath::Cos(4.0f * T));
			Pts.Add(C + FV(X, Y + 2.5f) * (Width / 32.0f));
		}
		return Pts;
	}

	/** A glossy living heart, its skin faintly veined like coral flesh */
	void HDHeart(FPainter& P, FV C, float Width, const FLinearColor& Color)
	{
		const TArray<FV> Pts = HDHeartPts(C, Width);
		const auto Body = [Pts](FV Q) { return SdPoly(Q, Pts); };
		{
			FWith With(P);
			P.Look.Dome = Width * 0.5f;
			P.Look.Spec = 0.85f;
			P.Fill(Body, Color);
		}
		FClip Clip(P, Body);
		// the cleft between the lobes and soft veins
		P.Mark([C, Width](FV Q) { return SdCapsule(Q, C + FV(0.0f, -Width * 0.26f), C + FV(Width * 0.03f, Width * 0.05f), Width * 0.012f); }, Alpha(Tone(Color, 0.5f), 0.45f), Width * 0.03f);
		for (int32 k = 0; k < 4; ++k)
		{
			const FV A = C + FV(-0.25f + k * 0.17f, -0.12f + (k % 2) * 0.06f) * Width;
			P.MLine(Smooth({ A, A + FV(0.06f, 0.12f) * Width, A + FV(0.02f + (k - 1.5f) * -0.03f, 0.26f) * Width }), Width * 0.006f, Alpha(Tone(Color, 0.55f), 0.3f));
		}
		// the gloss: a broad soft sheen on the left lobe and a crisp catchlight
		P.MEllipse(C + FV(-0.2f, -0.14f) * Width, Width * 0.11f, Width * 0.07f, Hex(0xFFFFFF, 0.45f), Width * 0.06f);
		P.MEllipse(C + FV(-0.22f, -0.17f) * Width, Width * 0.035f, Width * 0.022f, Hex(0xFFFFFF, 0.9f));
		P.MEllipse(C + FV(0.16f, 0.12f) * Width, Width * 0.2f, Width * 0.12f, Hex(0x400818, 0.25f), Width * 0.12f);
	}

	void HDBoonThickReef(FPainter& P)
	{
		// a thick, healthy reef: a dense thicket of branching coral grown up from a rock, sheltering a glossy heart
		HDCoralBranches(P, FV(0.26f, 0.84f), 0.15f, 0.034f, -24.0f, 2, Hex(0xE86A8A), 1544);
		HDCoralBranches(P, FV(0.74f, 0.84f), 0.15f, 0.034f, 24.0f, 2, Hex(0xE86A8A), 1545);
		HDCoralBranches(P, FV(0.34f, 0.86f), 0.2f, 0.042f, -14.0f, 2, Hex(0xF08A5A), 1541);
		HDCoralBranches(P, FV(0.66f, 0.86f), 0.2f, 0.042f, 14.0f, 2, Hex(0xF08A5A), 1542);
		HDHeart(P, FV(0.5f, 0.44f), 0.4f, Hex(0xE8405A));
		HDMound(P, FV(0.5f, 0.88f), 0.36f, 0.075f, Hex(0x8A9098), 1543);
		// a few small coral heads and polyps clinging to the rock in front
		for (int32 i = 0; i < 4; ++i)
		{
			const FV At(0.26f + i * 0.16f, 0.84f + (i % 2) * 0.03f);
			P.Ball(At, 0.026f + 0.006f * (i % 2), i % 2 ? Hex(0xF0A050) : Hex(0xC85AA0), 0.5f);
			for (int32 k = 0; k < 5; ++k)
			{
				P.MCircle(At + Spin(FV(0.016f, 0.0f), k * 72.0f + i * 20.0f), 0.004f, Hex(0xFFE0D0, 0.7f));
			}
		}
	}

	void HDBoonVenomBloom(FPainter& P)
	{
		// a venomous sea anemone in bloom: tapering tentacle-petals around a glowing mouth, dripping venom
		const FV C(0.5f, 0.42f);
		P.Fill([C](FV Q) { return SdRibbon(Q, { C + FV(0.0f, 0.1f), C + FV(0.0f, 0.36f) }, 0.07f, 0.1f); }, Hex(0x7A3AA8));
		{
			FClip Clip(P, [C](FV Q) { return SdRibbon(Q, { C + FV(0.0f, 0.1f), C + FV(0.0f, 0.36f) }, 0.07f, 0.1f); });
			for (int32 k = 0; k < 6; ++k)
			{
				P.MLine({ C + FV(-0.06f + k * 0.024f, 0.12f), C + FV(-0.09f + k * 0.036f, 0.36f) }, 0.003f, Hex(0x3A1A5A, 0.6f));
			}
		}
		for (int32 Layer = 0; Layer < 2; ++Layer)
		{
			const int32 Count = Layer ? 9 : 11;
			for (int32 k = 0; k < Count; ++k)
			{
				const float A = (k + Layer * 0.5f) * 360.0f / Count;
				const FV Dir = Spin(FV(1.0f, 0.0f), A) * FV(1.0f, 0.75f);
				const float L = Layer ? 0.2f : 0.32f;
				const TArray<FV> Petal = Smooth({ C, C + Dir * L * 0.6f + Spin(Dir, 90.0f) * 0.02f, C + Dir * L });
				P.Fill([Petal, Layer](FV Q) { return SdRibbon(Q, Petal, Layer ? 0.04f : 0.05f, 0.008f); }, Layer ? Hex(0xC07AF0) : Hex(0x9A4AD8));
				FV Pos, Tan;
				Along(Petal, 0.95f, Pos, Tan);
				P.Ball(Pos, 0.012f, Hex(0x9AF070), 0.9f);
			}
		}
		P.Glow([C](FV Q) { return SdCircle(Q, C, 0.05f); }, Hex(0x9AF070), 0.08f, 0.7f);
		P.Ball(C, 0.06f, Hex(0x5AC838), 0.8f);
		P.MEllipse(C + FV(0.0f, 0.005f), 0.03f, 0.012f, Hex(0x1A3A0A));
		Drop(P, FV(0.3f, 0.86f), 0.16f, Poison);
		Drop(P, FV(0.7f, 0.84f), 0.13f, Poison);
	}

	void HDBoonStormCoil(FPainter& P)
	{
		// the shock eel wound into a tight coil, tail at the centre, head reared at the outside, its electric organs lit
		const FLinearColor Body = Hex(0x2AA8A4), Dark = Hex(0x14605E), Belly = Hex(0xA8E8D8), Band = Hex(0xFFE24A);
		TArray<FV> Coil;
		for (int32 k = 0; k <= 60; ++k)
		{
			const float T = k / 60.0f;
			const float A = 2.2f + T * 2.0f * PI * 1.45f;
			Coil.Add(FV(0.47f, 0.55f) + FV(FMath::Cos(A), FMath::Sin(A)) * (0.05f + T * 0.25f));
		}
		const float R0 = 0.018f, R1 = 0.06f;
		FV HeadPos, HeadTan;
		Along(Coil, 1.0f, HeadPos, HeadTan);
		const float HS = 1.15f;
		const FV HeadC = HeadPos + HeadTan * 0.06f;
		const float HeadDeg = FMath::RadiansToDegrees(FMath::Atan2(HeadTan.Y, HeadTan.X));
		const auto HeadLocal = [](FV Q) { return SMin(SdEllipse(Q, FV(-0.02f, 0.0f), 0.1f, 0.07f), SdEllipse(Q, FV(0.07f, 0.01f), 0.07f, 0.05f), 0.04f); };
		// one continuous animal: the head is blended into the neck, so it shares the body's outline and shading
		const auto Eel = [&Coil, R0, R1, HeadLocal, HeadC, HeadTan, HS](FV Q)
		{
			const FV D = Q - HeadC;
			const FV L(FV::DotProduct(D, HeadTan) / HS, (D.X * -HeadTan.Y + D.Y * HeadTan.X) / HS);
			return SMin(SdRibbon(Q, Coil, R0, R1), HeadLocal(L) * HS, 0.03f);
		};
		P.Glow(Eel, Hex(0x20B8C0), 0.07f, 0.35f);
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.6f;
			P.Fill(Eel, Body);
		}
		{
			FClip Clip(P, Eel);
			TArray<FV> BellyLine, BackLine;
			for (int32 i = 0; i <= 50; ++i)
			{
				FV Pos, Tan;
				const float T = i / 50.0f;
				Along(Coil, T, Pos, Tan);
				const FV N(-Tan.Y, Tan.X);
				const float R = FMath::Lerp(R0, R1, T);
				BellyLine.Add(Pos + N * R * 0.75f);
				BackLine.Add(Pos - N * R * 0.7f);
			}
			P.MLine(BellyLine, 0.014f, Alpha(Belly, 0.7f));
			P.MLine(BackLine, 0.012f, Alpha(Dark, 0.7f));
			// glowing electric organ bands, each a row of cells lit from within, stopping short of the head
			for (int32 i = 1; i < 19; ++i)
			{
				FV Pos, Tan;
				const float T = i / 20.0f;
				Along(Coil, T, Pos, Tan);
				const FV N(-Tan.Y, Tan.X);
				const float R = FMath::Lerp(R0, R1, T);
				P.Glow([Pos, N, R](FV Q) { return SdCapsule(Q, Pos - N * R, Pos + N * R, 0.007f); }, Hex(0xE8C820), 0.025f, 0.7f);
				P.MCapsule(Pos - N * R, Pos + N * R, 0.007f + R * 0.03f, Band);
				const int32 Cells = R > 0.04f ? 3 : 2;
				for (int32 k = -Cells; k <= Cells; ++k)
				{
					P.MCircle(Pos + N * (R * k / (Cells + 0.5f)), 0.0035f, Hex(0xFFFFE8));
				}
			}
			Speckle(P, FV(0.1f, 0.18f), FV(0.9f, 0.9f), 160, 0.0015f, 0.004f, Alpha(Dark, 0.6f), 1551);
		}
		// the head's features, in the head's own frame, with the same skin as the body
		{
			FNest Place(P, HeadC, HS, HeadDeg);
			FClip Clip(P, HeadLocal);
			P.MEllipse(FV(0.02f, 0.05f), 0.11f, 0.03f, Alpha(Belly, 0.8f), 0.02f);
			P.MEllipse(FV(0.0f, -0.05f), 0.1f, 0.03f, Alpha(Dark, 0.5f), 0.03f);
			const TArray<FV> Mouth = Smooth({ FV(0.13f, 0.022f), FV(0.07f, 0.03f), FV(0.0f, 0.05f) });
			P.MLine(Mouth, 0.006f, Hex(0x0A2A2A));
			for (int32 k = 0; k < 6; ++k)
			{
				FV Pos, Tan;
				Along(Mouth, (k + 0.5f) / 6.0f, Pos, Tan);
				P.MPoly({ Pos + FV(-0.006f, -0.002f), Pos + FV(0.006f, -0.002f), Pos + FV(0.0f, 0.014f) }, Hex(0xF4F8F0));
			}
			Speckle(P, FV(-0.1f, -0.07f), FV(0.13f, 0.06f), 40, 0.0015f, 0.004f, Alpha(Dark, 0.6f), 722);
			for (int32 k = 0; k < 5; ++k)
			{
				P.MCircle(FV(-0.06f + k * 0.03f, -0.018f + FMath::Abs(k - 2) * 0.003f), 0.004f, Hex(0x0A3A3A, 0.8f));
			}
			HDEye(P, FV(0.05f, -0.03f), 0.02f, Band, FV(0.3f, 0.1f), 0.7f);
			HDAngryLid(P, FV(0.05f, -0.03f), 0.02f, 1.0f, Tone(Body, 0.7f), 0.8f, 0.0f);
		}
		HDLightning(P, FV(0.74f, 0.74f), FV(0.92f, 0.86f), 1552, 0.006f);
		HDLightning(P, FV(0.2f, 0.36f), FV(0.06f, 0.2f), 1553, 0.005f);
		HDLightning(P, FV(0.28f, 0.82f), FV(0.12f, 0.94f), 1554, 0.004f);
	}

	void HDBoonBloodTide(FPainter& P)
	{
		// a breaking wave of blood-red water, its crest curling and spraying
		const TArray<FV> Crest = Smooth({ FV(0.12f, 0.76f), FV(0.24f, 0.6f), FV(0.36f, 0.4f), FV(0.54f, 0.26f), FV(0.76f, 0.28f), FV(0.84f, 0.42f), FV(0.76f, 0.5f), FV(0.66f, 0.42f), FV(0.58f, 0.46f), FV(0.6f, 0.6f), FV(0.74f, 0.72f), FV(0.86f, 0.76f), FV(0.86f, 0.84f), FV(0.12f, 0.84f) }, true, 5);
		const auto Water = [&Crest](FV Q) { return SdPoly(Q, Crest); };
		{
			FWith With(P);
			P.Look.Dome = 0.18f;
			P.Look.Spec = 0.8f;
			P.Fill(Water, Hex(0xB01E2A));
		}
		{
			FClip Clip(P, Water);
			P.MEllipse(FV(0.42f, 0.62f), 0.16f, 0.24f, Hex(0xF05A5A, 0.55f), 0.08f);
			P.MLine(Smooth({ FV(0.24f, 0.62f), FV(0.36f, 0.42f), FV(0.54f, 0.3f), FV(0.72f, 0.3f), FV(0.8f, 0.38f) }), 0.022f, Hex(0xFFC8C8, 0.85f));
			for (int32 k = 0; k < 4; ++k)
			{
				P.MLine(Smooth({ FV(0.14f + k * 0.02f, 0.74f + k * 0.02f), FV(0.34f, 0.68f + k * 0.03f), FV(0.52f, 0.72f + k * 0.025f), FV(0.62f, 0.66f + k * 0.035f) }), 0.003f, Hex(0x5A0A10, 0.5f));
			}
		}
		for (int32 k = 0; k < 6; ++k)
		{
			const FV At(0.62f + Rand01(1561, k) * 0.24f, 0.18f + Rand01(1562, k) * 0.12f);
			P.Ball(At, 0.008f + Rand01(1563, k) * 0.01f, Hex(0xD8303A), 0.9f);
		}
		Drop(P, FV(0.5f, 0.12f), 0.2f, Blood);
	}

	void HDBoonFrenziedCurrents(FPainter& P)
	{
		// a churning, frenzied whirlpool of red water
		HDWhirlpool(P, FV(0.5f, 0.5f), 0.34f, Hex(0x7A1A2A), Hex(0xE85A5A), 4, 1.1f);
		for (int32 k = 0; k < 4; ++k)
		{
			const float A = k * 90.0f + 20.0f;
			const FV D = Spin(FV(1.0f, 0.0f), A);
			P.Mark([D](FV Q) { return SdArc(Q, FV(0.5f, 0.5f), 0.4f, 0.012f, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)), FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)) + 40.0f); }, Hex(0xFFB0B0, 0.7f));
		}
	}

	void HDBoonBrittleCoral(FPainter& P)
	{
		// a coral branch cracking apart, a broken tip falling away
		TArray<TArray<FV>> Limbs;
		HDCoralBranches(P, FV(0.42f, 0.9f), 0.3f, 0.055f, 6.0f, 2, Hex(0xE88A80), 1571, &Limbs);
		const TArray<TArray<FV>> Cracks = {
			{ FV(0.4f, 0.8f), FV(0.44f, 0.74f), FV(0.41f, 0.68f), FV(0.45f, 0.62f) },
			{ FV(0.5f, 0.5f), FV(0.46f, 0.46f), FV(0.5f, 0.42f) },
			{ FV(0.32f, 0.4f), FV(0.36f, 0.36f), FV(0.33f, 0.32f) } };
		for (const TArray<FV>& Cr : Cracks)
		{
			P.MLine(Cr, 0.007f, Hex(0x2A0A10, 0.9f));
			P.MLine(Cr, 0.002f, Hex(0xFFE0D8, 0.6f));
		}
		// a snapped-off piece tumbling away, with fragments
		{
			FNest Place(P, FV(0.78f, 0.36f), 1.0f, 40.0f);
			P.Fill([](FV Q) { return SdRibbon(Q, { FV(0.0f, 0.08f), FV(0.0f, -0.08f) }, 0.03f, 0.022f) + Noise2(Q * 80.0f) * 0.004f; }, Hex(0xE88A80));
			P.MLine({ FV(-0.03f, 0.08f), FV(-0.01f, 0.1f), FV(0.01f, 0.08f), FV(0.03f, 0.1f) }, 0.004f, Hex(0x5A1A20, 0.8f));
		}
		for (int32 k = 0; k < 5; ++k)
		{
			const FV At(0.62f + Rand01(1572, k) * 0.24f, 0.5f + Rand01(1573, k) * 0.3f);
			P.Fill([At, k](FV Q) { return SdPoly(Q, { At + Spin(FV(0.016f, 0.0f), k * 70.0f), At + Spin(FV(0.016f, 0.0f), k * 70.0f + 130.0f), At + Spin(FV(0.012f, 0.0f), k * 70.0f + 240.0f) }); }, Hex(0xD87A70));
		}
	}

	/** One gold manacle: a hinged band seen nearly face on, its lock lug at the bottom */
	void HDManacle(FPainter& P, FV C, float Rx, float Ry, float T, bool bHingeRight)
	{
		const auto Band = [C, Rx, Ry, T](FV Q) { return FMath::Abs(SdEllipse(Q, C, Rx, Ry)) - T * 0.5f; };
		{
			FWith With(P);
			P.Look.Dome = T * 0.5f;
			P.Look.Spec = 0.9f;
			P.Fill(Band, Hex(0xE8B040));
		}
		{
			FClip Clip(P, Band);
			// a raised rim each side of an engraved channel, and the shine running round the upper left
			P.Mark([C, Rx, Ry](FV Q) { return FMath::Abs(SdEllipse(Q, C, Rx, Ry)) - 0.004f; }, Hex(0x8A5A18, 0.55f));
			P.Mark([C, Rx, Ry, T](FV Q) { return FMath::Max(FMath::Abs(SdEllipse(Q, C, Rx - T * 0.22f, Ry - T * 0.22f)) - 0.005f, (Q.X - C.X) * 0.7f + (Q.Y - C.Y)); }, Hex(0xFFF4C0, 0.7f), 0.004f);
			P.Mark([C, Rx, Ry, T](FV Q) { return FMath::Max(FMath::Abs(SdEllipse(Q, C, Rx + T * 0.24f, Ry + T * 0.24f)) - 0.004f, -((Q.X - C.X) * 0.7f + (Q.Y - C.Y))); }, Hex(0x6A3A08, 0.5f), 0.004f);
		}
		// the hinge: a rolled knuckle across the band
		const FV H = C + FV(bHingeRight ? Rx : -Rx, -Ry * 0.2f);
		{
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Spec = 0.8f;
			P.Fill([H, T](FV Q) { return SdCapsule(Q, H + FV(0.0f, -T * 0.75f), H + FV(0.0f, T * 0.75f), T * 0.4f); }, Hex(0xD49A30));
		}
		for (const float Y : { -0.25f, 0.25f })
		{
			P.MCapsule(H + FV(-T * 0.38f, Y * T), H + FV(T * 0.38f, Y * T), 0.003f, Hex(0x6A3A08, 0.7f));
		}
		// the lock lug with its keyhole, where the chain is made fast
		const FV L = C + FV(0.0f, Ry);
		{
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Spec = 0.8f;
			P.Fill([L, T](FV Q) { return SdBox(Q, L - FV(T * 0.8f, T * 0.6f), L + FV(T * 0.8f, T * 0.6f), T * 0.25f); }, Hex(0xE0A838));
		}
		P.MCircle(L + FV(0.0f, -T * 0.12f), T * 0.17f, Hex(0x3A2008));
		P.MPoly({ L + FV(-T * 0.1f, -T * 0.05f), L + FV(T * 0.1f, -T * 0.05f), L + FV(T * 0.16f, T * 0.32f), L + FV(-T * 0.16f, T * 0.32f) }, Hex(0x3A2008));
		P.MCircle(L + FV(-T * 0.5f, -T * 0.35f), T * 0.08f, Hex(0xFFF4C0, 0.8f));
	}

	/** A gold chain along Path: face-on oval links threaded through edge-on ones */
	void HDChain(FPainter& P, const TArray<FV>& Path, int32 Links, float Len, float T)
	{
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (int32 i = Pass; i < Links; i += 2)
			{
				FV Pos, Tan;
				Along(Path, (i + 0.5f) / Links, Pos, Tan);
				const FV Nrm(-Tan.Y, Tan.X);
				const auto Local = [Pos, Tan, Nrm](FV Q) { const FV D = Q - Pos; return FV(FV::DotProduct(D, Tan), FV::DotProduct(D, Nrm)); };
				FWith With(P);
				P.Look.Spec = 0.9f;
				if (Pass == 0)
				{
					// face on: an open oval ring
					const auto Ring = [Local, Len, T](FV Q) { return FMath::Abs(SdEllipse(Local(Q), FV(0.0f, 0.0f), Len * 0.62f, Len * 0.36f)) - T * 0.5f; };
					P.Look.Dome = T * 0.5f;
					P.Fill(Ring, Hex(0xE8B040));
					FClip Clip(P, Ring);
					P.Mark([Local, Len, T](FV Q) { const FV L = Local(Q); return FMath::Max(FMath::Abs(SdEllipse(L, FV(0.0f, 0.0f), Len * 0.62f, Len * 0.36f)) - T * 0.12f, L.Y * 0.3f + L.X * 0.3f); }, Hex(0xFFF4C0, 0.6f), 0.003f);
				}
				else
				{
					// edge on: a flat bar with the slot of the ring showing as a dark line
					const auto Bar = [Local, Len, T](FV Q) { const FV L = Local(Q); return SdCapsule(L, FV(-Len * 0.62f, 0.0f), FV(Len * 0.62f, 0.0f), T * 0.75f); };
					P.Look.Dome = T * 0.6f;
					P.Fill(Bar, Hex(0xD8A038));
					P.Mark([Local, Len, T](FV Q) { const FV L = Local(Q); return SdCapsule(L, FV(-Len * 0.4f, 0.0f), FV(Len * 0.4f, 0.0f), T * 0.13f); }, Hex(0x5A3008, 0.85f));
				}
			}
		}
	}

	void HDBoonGildedShackles(FPainter& P)
	{
		// a pair of gilded manacles hanging open on a sagging gold chain
		HDChain(P, Smooth({ FV(0.3f, 0.6f), FV(0.38f, 0.76f), FV(0.5f, 0.8f), FV(0.62f, 0.76f), FV(0.7f, 0.6f) }), 9, 0.055f, 0.016f);
		HDManacle(P, FV(0.3f, 0.4f), 0.13f, 0.15f, 0.05f, false);
		HDManacle(P, FV(0.7f, 0.4f), 0.13f, 0.15f, 0.05f, true);
		Glint(P, FV(0.36f, 0.24f), 0.05f);
	}

	void HDBoonSirensDebt(FPainter& P)
	{
		// a siren's scaled tail sweeping in from beyond the frame and curling round a gold coin she means to collect
		const FV Coin0(0.53f, 0.5f);
		Coin(P, Coin0, 0.17f);
		const TArray<FV> Tail = Smooth({ FV(0.06f, 0.2f), FV(0.2f, 0.36f), FV(0.26f, 0.58f), FV(0.42f, 0.7f), FV(0.6f, 0.66f), FV(0.7f, 0.54f), FV(0.74f, 0.36f) });
		const auto Body = [&Tail](FV Q) { return FMath::Max(SdRibbon(Q, Tail, 0.085f, 0.028f), SdCircle(Q, FV(0.5f, 0.5f), 0.45f)); };
		{
			FWith With(P);
			P.Look.Dome = 0.1f;
			P.Look.Spec = 0.9f;
			P.Fill(Body, Hex(0x6A3AB8));
		}
		{
			FClip Clip(P, Body);
			Scales(P, FV(0.0f, 0.1f), FV(0.9f, 0.85f), 0.022f, 0.0f, Hex(0x2A1060, 0.5f), Hex(0xC8A8FF, 0.45f));
			TArray<FV> Shine, Belly;
			for (int32 k = 0; k <= 20; ++k)
			{
				FV Pos, Tan;
				Along(Tail, k / 20.0f, Pos, Tan);
				const float W = FMath::Lerp(0.085f, 0.028f, k / 20.0f);
				Shine.Add(Pos - FV(-Tan.Y, Tan.X) * W * 0.5f);
				Belly.Add(Pos + FV(-Tan.Y, Tan.X) * W * 0.62f);
			}
			// a pale belly stripe on the inside of the curl and the long highlight on its back
			P.MLine(Belly, 0.012f, Hex(0xE8C8F8, 0.35f));
			P.MLine(Shine, 0.007f, Hex(0xE8D8FF, 0.5f));
		}
		// the tail fin flaring up off the coin's edge: two sheer, ribbed lobes
		const FV Root = Tail.Last();
		HDFin(P, Root, { Root + FV(-0.08f, -0.1f), Root + FV(-0.1f, -0.2f), Root + FV(-0.02f, -0.14f), Root + FV(0.01f, -0.08f) }, Hex(0x9A6AE8, 0.9f), Hex(0x4A2A8A, 0.7f));
		HDFin(P, Root, { Root + FV(0.01f, -0.08f), Root + FV(0.08f, -0.18f), Root + FV(0.14f, -0.14f), Root + FV(0.1f, -0.04f) }, Hex(0x9A6AE8, 0.9f), Hex(0x4A2A8A, 0.7f));
	}

	void HDBoonGiantSlayer(FPainter& P)
	{
		// a kraken's tentacle hefting a keen steel sword: straight double-edged blade, a fuller, a needle point
		Tentacle(P, { FV(0.1f, 0.94f), FV(0.3f, 0.7f), FV(0.5f, 0.7f), FV(0.6f, 0.6f) }, 0.12f, 0.05f, 1.0f, Hex(0xB04A8C), Hex(0xF2B8D8));
		const FV Guard(0.6f, 0.52f), Tip(0.9f, 0.08f);
		const FV D = (Tip - Guard).GetSafeNormal();
		const FV N(-D.Y, D.X);
		const float Len = (Tip - Guard).Size();
		const float W = 0.032f;
		const auto At = [Guard, D, N](float U, float V) { return Guard + D * U + N * V; };
		// the blade: parallel edges, then a long taper to a sharp point
		const TArray<FV> Blade = { At(0.0f, -W), At(Len * 0.72f, -W * 0.92f), At(Len, 0.0f), At(Len * 0.72f, W * 0.92f), At(0.0f, W) };
		{
			FWith With(P);
			P.Look.Dome = 0.012f;
			P.Look.Spec = 0.9f;
			P.Look.Ink = 0.006f;
			P.Poly(Blade, Hex(0xC8D4DC));
		}
		{
			FClip Clip(P, [&Blade](FV Q) { return SdPoly(Q, Blade); });
			// bright honed edge on the lit side, darker bevel on the other, a central fuller groove
			P.MPoly({ At(0.0f, -W), At(Len * 0.72f, -W * 0.92f), At(Len, 0.0f), At(Len * 0.72f, -W * 0.45f), At(0.0f, -W * 0.5f) }, Hex(0xF4FAFF, 0.75f));
			P.MPoly({ At(0.0f, W), At(Len * 0.72f, W * 0.92f), At(Len, 0.0f), At(Len * 0.72f, W * 0.45f), At(0.0f, W * 0.5f) }, Hex(0x5A6A78, 0.55f));
			P.MCapsule(At(0.03f, 0.0f), At(Len * 0.62f, 0.0f), 0.006f, Hex(0x3A4A58, 0.7f));
			P.MCapsule(At(0.03f, -0.005f), At(Len * 0.62f, -0.005f), 0.002f, Hex(0xFFFFFF, 0.6f));
		}
		Glint(P, At(Len * 0.97f, 0.0f), 0.06f);
		// crossguard with flared quillons, a wrapped grip and a round pommel
		{
			FWith With(P);
			P.Look.Spec = 0.8f;
			P.Fill([At](FV Q) { return SMin(SdCapsule(Q, At(-0.004f, -0.075f), At(-0.004f, 0.075f), 0.013f), SMin(SdCircle(Q, At(0.0f, -0.08f), 0.017f), SdCircle(Q, At(0.0f, 0.08f), 0.017f), 0.01f), 0.01f); }, Hex(0xD8A838));
		}
		P.Fill([At](FV Q) { return SdCapsule(Q, At(-0.02f, 0.0f), At(-0.11f, 0.0f), 0.017f); }, Hex(0x6A3A1A));
		for (int32 k = 0; k < 5; ++k)
		{
			const float U = -0.028f - k * 0.017f;
			P.MCapsule(At(U + 0.005f, -0.017f), At(U - 0.005f, 0.017f), 0.003f, Hex(0x2A1408, 0.7f));
		}
		P.Ball(At(-0.13f, 0.0f), 0.024f, Hex(0xD8A838), 0.8f);
		// the tentacle tip coiling once round the grip
		P.Fill([At](FV Q) { return SdRibbon(Q, Smooth({ At(-0.12f, 0.06f), At(-0.07f, 0.03f), At(-0.06f, -0.025f), At(-0.035f, -0.04f) }), 0.02f, 0.012f); }, Hex(0xB04A8C));
		P.MCircle(At(-0.075f, 0.03f), 0.006f, Hex(0xF2B8D8));
	}

	/** A soft cord along Path, twisted, in Color */
	void HDCord(FPainter& P, const TArray<FV>& Path, float R, const FLinearColor& Color)
	{
		{
			FWith With(P);
			P.Look.Dome = R;
			P.Look.Spec = 0.4f;
			P.Fill([&Path, R](FV Q) { return SdRibbon(Q, Path, R, R); }, Color);
		}
		FClip Clip(P, [&Path, R](FV Q) { return SdRibbon(Q, Path, R, R); });
		const int32 Count = 18;
		for (int32 k = 0; k < Count; ++k)
		{
			FV Pos, Tan;
			Along(Path, (k + 0.5f) / Count, Pos, Tan);
			const FV N(-Tan.Y, Tan.X);
			P.MCapsule(Pos - N * R - Tan * R * 0.5f, Pos + N * R + Tan * R * 0.5f, R * 0.18f, Alpha(Tone(Color, 0.5f), 0.6f));
		}
	}

	void HDBoonDeepPockets(FPainter& P)
	{
		// a fat leather coin purse cinched with a drawstring, a pearl peeking out of its mouth and coins spilling beside it
		const FLinearColor Leather = Hex(0x9A6038), LeatherDark = Hex(0x4A2810);
		const auto Bag = [](FV Q)
		{
			const float Belly = SdEllipse(Q, FV(0.48f, 0.68f), 0.29f, 0.23f);
			const float Neck = SdPoly(Q, { FV(0.4f, 0.36f), FV(0.56f, 0.36f), FV(0.64f, 0.5f), FV(0.32f, 0.5f) });
			return SMin(Belly, Neck, 0.08f) + Noise2(Q * 18.0f) * 0.004f;
		};
		const auto Frill = [](FV Q)
		{
			// the gathered mouth above the cord: a flared, scalloped ruff
			const FV L = Q - FV(0.48f, 0.3f);
			const float A = FMath::Atan2(L.Y, L.X);
			return SdEllipse(Q, FV(0.48f, 0.29f), 0.15f + FMath::Sin(A * 7.0f) * 0.008f, 0.07f);
		};
		// the pearl sits in the open mouth, behind the near rim
		{
			FWith With(P);
			P.Look.Dome = 0.15f;
			P.Look.Grain = 0.2f;
			P.Fill(Frill, Tone(Leather, 1.05f));
		}
		P.MEllipse(FV(0.48f, 0.28f), 0.11f, 0.04f, LeatherDark);
		PearlBall(P, FV(0.5f, 0.2f), 0.09f);
		P.Fill([](FV Q) { return FMath::Max(SdEllipse(Q, FV(0.48f, 0.3f), 0.15f, 0.06f), 0.28f - Q.Y); }, Tone(Leather, 1.05f));
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.35f;
			P.Look.Grain = 0.25f;
			P.Fill(Bag, Leather);
		}
		{
			FClip Clip(P, Bag);
			// gathered folds fanning down from the cinched neck, each with a lit side
			for (int32 k = 0; k < 7; ++k)
			{
				const float X = 0.38f + k * 0.03f;
				const float Out = (k - 3) * 0.07f;
				const TArray<FV> Fold = Smooth({ FV(X, 0.4f), FV(X + Out * 0.4f, 0.5f), FV(X + Out, 0.62f) });
				P.Mark([Fold](FV Q) { return SdRibbon(Q, Fold, 0.008f, 0.002f); }, Alpha(LeatherDark, 0.45f), 0.008f);
				P.MLine({ Fold[0] + FV(-0.008f, 0.0f), Fold.Last() + FV(-0.012f, 0.0f) }, 0.003f, Hex(0xFFE0C0, 0.18f));
			}
			// a stitched seam round the base, scuffs and a soft sheen on the belly
			for (int32 k = 0; k < 16; ++k)
			{
				const float A = FMath::DegreesToRadians(20.0f + k * 9.0f);
				const FV At = FV(0.48f, 0.66f) + FV(FMath::Cos(A) * 0.25f, FMath::Sin(A) * 0.19f);
				P.MCapsule(At, At + FV(-FMath::Sin(A), FMath::Cos(A) * 0.75f) * 0.012f, 0.003f, Hex(0xF0D8A8, 0.75f));
			}
			Speckle(P, FV(0.2f, 0.4f), FV(0.78f, 0.9f), 80, 0.002f, 0.005f, Alpha(LeatherDark, 0.4f), 1571);
			P.MEllipse(FV(0.38f, 0.62f), 0.1f, 0.07f, Hex(0xFFE8C8, 0.25f), 0.06f);
			P.MEllipse(FV(0.56f, 0.86f), 0.24f, 0.06f, Alpha(LeatherDark, 0.5f), 0.06f);
		}
		// the drawstring: a cord round the neck, tied off with two dangling, knotted ends
		HDCord(P, Smooth({ FV(0.34f, 0.37f), FV(0.48f, 0.4f), FV(0.62f, 0.37f) }), 0.014f, Hex(0xD8B070));
		HDCord(P, Smooth({ FV(0.6f, 0.38f), FV(0.68f, 0.46f), FV(0.7f, 0.56f) }), 0.011f, Hex(0xD8B070));
		HDCord(P, Smooth({ FV(0.6f, 0.38f), FV(0.74f, 0.42f), FV(0.8f, 0.5f) }), 0.011f, Hex(0xD8B070));
		P.Ball(FV(0.6f, 0.38f), 0.02f, Hex(0xC89A58), 0.4f);
		P.Ball(FV(0.7f, 0.57f), 0.016f, Hex(0xC89A58), 0.4f);
		P.Ball(FV(0.8f, 0.51f), 0.016f, Hex(0xC89A58), 0.4f);
		// coins spilled at its foot
		Coin(P, FV(0.8f, 0.8f), 0.085f);
		Coin(P, FV(0.68f, 0.88f), 0.07f);
	}

	// ------------------------------------------------------------------ the last nine boons, drawn as scenes

	void HDBoonLongSight(FPainter& P)
	{
		// a brass spyglass drawn out to full length, its big lens catching the light; leather on the barrel, barnacles on the brass
		const FLinearColor BrassC = Hex(0xD49A3A), BrassDark = Hex(0x7A4E14), BrassLit = Hex(0xFFE8A8);
		FNest Place(P, FV(0.49f, 0.52f), 1.0f, -39.0f);
		// one tube section from U0 to U1 of half width R: a cylinder lit along its top, shaded along its bottom
		const auto Tube = [&P, BrassC, BrassDark, BrassLit](float U0, float U1, float R, const FLinearColor& Color)
		{
			const auto Shape = [U0, U1, R](FV Q) { return SdBox(Q, FV(U0, -R), FV(U1, R), R * 0.12f); };
			{
				FWith With(P);
				P.Look.Dome = R * 0.25f;
				P.Look.Spec = 0.85f;
				P.Fill(Shape, Color);
			}
			FClip Clip(P, Shape);
			P.Mark([U0, U1, R](FV Q) { return SdBox(Q, FV(U0, R * 0.45f), FV(U1, R * 1.2f), 0.0f); }, Alpha(BrassDark, 0.45f), R * 0.35f);
			P.Mark([U0, U1, R](FV Q) { return SdBox(Q, FV(U0, -R * 0.62f), FV(U1, -R * 0.42f), 0.0f); }, Alpha(BrassLit, 0.75f), R * 0.12f);
			P.Mark([U0, U1, R](FV Q) { return SdBox(Q, FV(U0, -R * 0.56f), FV(U1, -R * 0.5f), 0.0f); }, Hex(0xFFFFFF, 0.7f));
			// fine engraved lines round the tube near each end
			for (const float U : { U0 + 0.012f, U1 - 0.012f })
			{
				P.Mark([U, R](FV Q) { return FMath::Max(FMath::Abs(Q.X - U) - 0.0025f, FMath::Abs(Q.Y) - R); }, Alpha(BrassDark, 0.7f));
			}
		};
		// a raised ring where two sections meet
		const auto Collar = [&P, BrassDark](float U, float R, float W)
		{
			const auto Shape = [U, R, W](FV Q) { return SdBox(Q, FV(U - W, -R), FV(U + W, R), W * 0.5f); };
			FWith With(P);
			P.Look.Dome = W;
			P.Look.Spec = 0.9f;
			P.Fill(Shape, Hex(0xE8B850));
			P.Mark([U, R, W](FV Q) { return SdBox(Q, FV(U - W, R * 0.5f), FV(U + W, R), 0.0f); }, Alpha(BrassDark, 0.4f), R * 0.3f);
		};
		Tube(-0.47f, -0.38f, 0.042f, Hex(0x4A3A2A));
		Tube(-0.39f, -0.18f, 0.06f, BrassC);
		Tube(-0.19f, 0.04f, 0.08f, Tone(BrassC, 1.05f));
		Tube(0.03f, 0.44f, 0.11f, BrassC);
		// the barrel's leather wrap, stitched along its edges, with two barnacles grown onto it
		{
			const auto Wrap = [](FV Q) { return SdBox(Q, FV(0.1f, -0.113f), FV(0.34f, 0.113f), 0.01f); };
			{
				FWith With(P);
				P.Look.Dome = 0.04f;
				P.Look.Grain = 0.3f;
				P.Look.Spec = 0.25f;
				P.Fill(Wrap, Hex(0x6A3A1E));
			}
			FClip Clip(P, Wrap);
			P.Mark([](FV Q) { return SdBox(Q, FV(0.1f, 0.05f), FV(0.34f, 0.14f), 0.0f); }, Hex(0x2A1408, 0.45f), 0.04f);
			P.Mark([](FV Q) { return SdBox(Q, FV(0.1f, -0.075f), FV(0.34f, -0.06f), 0.0f); }, Hex(0xE8C8A0, 0.35f), 0.01f);
			for (int32 k = 0; k < 11; ++k)
			{
				const float U = 0.115f + k * 0.0215f;
				for (const float V : { -0.095f, 0.095f })
				{
					P.MCapsule(FV(U, V), FV(U + 0.01f, V), 0.003f, Hex(0xF0D8A8, 0.85f));
				}
			}
			Speckle(P, FV(0.1f, -0.11f), FV(0.34f, 0.11f), 50, 0.002f, 0.005f, Hex(0x2A1408, 0.45f), 1601);
			HDBarnacle(P, FV(0.27f, 0.07f), 0.026f);
			HDBarnacle(P, FV(0.31f, 0.045f), 0.017f);
		}
		Collar(-0.38f, 0.05f, 0.012f);
		Collar(-0.18f, 0.068f, 0.012f);
		Collar(0.035f, 0.09f, 0.014f);
		Collar(0.43f, 0.122f, 0.018f);
		// the objective lens seen nearly edge on: blue glass reflecting the sea, ringed by the brass lip
		const auto Lens = [](FV Q) { return SdEllipse(Q, FV(0.452f, 0.0f), 0.026f, 0.104f); };
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Spec = 1.0f;
			P.Fill(Lens, Hex(0x2A7AB8));
		}
		{
			FClip Clip(P, Lens);
			P.MEllipse(FV(0.455f, -0.05f), 0.02f, 0.04f, Hex(0xBFF0FF, 0.75f), 0.015f);
			P.MEllipse(FV(0.45f, 0.06f), 0.02f, 0.03f, Hex(0x0A2A50, 0.6f), 0.02f);
		}
		// light gathering into the lens
		P.Glow([](FV Q) { return SdEllipse(Q, FV(0.47f, 0.0f), 0.02f, 0.08f); }, Hex(0xBFF0FF), 0.08f, 0.5f);
		HDSpark(P, FV(0.465f, -0.06f), 0.11f);
		// the eyepiece's rim
		P.Fill([](FV Q) { return SdBox(Q, FV(-0.49f, -0.047f), FV(-0.465f, 0.047f), 0.008f); }, Hex(0x2A1E14));
	}

	void HDBoonScholarsTide(FPainter& P)
	{
		// a chambered nautilus shell cut open: each chamber, from the tiny first to the newest, glows a little brighter,
		// the shell's whole life of growth laid out in one spiral
		const FV O(0.5f, 0.53f);
		const float RMax = 0.4f;
		const float K = FMath::Loge(3.0f) / (2.0f * PI);	// the shell triples its size every turn
		const float A0 = FMath::DegreesToRadians(200.0f);	// where the aperture opens
		const auto R = [RMax, K](float T) { return RMax * FMath::Exp(-K * T); };
		const auto At = [O, R, A0](float T, float Scale = 1.0f) { const float A = A0 - T; return O + FV(FMath::Cos(A), FMath::Sin(A)) * R(T) * Scale; };
		TArray<FV> Outline;
		for (int32 i = 0; i <= 72; ++i)
		{
			Outline.Add(At(i * 2.0f * PI / 72.0f));
		}
		const auto Shell = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		P.Glow(Shell, Hex(0x60F0BA), 0.09f, 0.35f);
		{
			FWith With(P);
			P.Look.Dome = 0.08f;
			P.Look.Spec = 0.7f;
			P.Fill(Shell, Hex(0xF2E6D0));
		}
		FClip Clip(P, Shell);
		// the chambers between one whorl and the next, walled off by septa that curve back toward the mouth
		const float TEnd = 4.0f * PI;
		const float TStart = 1.1f;	// the living chamber, open at the aperture
		const int32 Chambers = 18;
		const float Step = (TEnd - TStart) / Chambers;
		const float Inner = FMath::Exp(-K * 2.0f * PI);
		const auto Bow = [&At, Inner, Step](float T, float S) { return At(T - 1.1f * Step * S * (1.0f - S), FMath::Lerp(1.0f, Inner, S)); };
		const auto RoomPts = [&At, &Bow](float T0, float T1)
		{
			TArray<FV> Room;
			for (int32 s = 0; s <= 6; ++s)
			{
				Room.Add(At(FMath::Lerp(T0, T1, s / 6.0f)));
			}
			for (int32 s = 1; s <= 8; ++s)
			{
				Room.Add(Bow(T1, s / 8.0f));
			}
			for (int32 s = 5; s >= 0; --s)
			{
				Room.Add(At(FMath::Lerp(T0, T1, s / 6.0f) + 2.0f * PI));
			}
			for (int32 s = 7; s >= 1; --s)
			{
				Room.Add(Bow(T0, s / 8.0f));
			}
			return Room;
		};
		for (int32 c = 0; c < Chambers; ++c)
		{
			const float T0 = TStart + Step * c;
			const TArray<FV> Room = RoomPts(T0, T0 + Step);
			// a hollow of pearly shadow, lit from inside: the oldest chambers (deepest in the spiral) glow faintest
			const float Age = 1.0f - (float)c / (Chambers - 1);
			const FLinearColor Light = FMath::Lerp(Hex(0x2A8A70), Hex(0xA8FFE0), Age);
			const float Thick = R(T0 + Step * 0.5f) * (1.0f - Inner);
			FV Mid(0.0f, 0.0f);
			for (const FV& V : Room)
			{
				Mid += V / (float)Room.Num();
			}
			P.Mark([Room](FV Q) { return SdPoly(Q, Room); }, Hex(0x123C46));
			P.Mark([Room](FV Q) { return SdPoly(Q, Room) + 0.006f; }, Hex(0x0A2430, 0.6f), 0.02f);
			P.Glow([Mid, Thick](FV Q) { return SdCircle(Q, Mid, Thick * 0.12f); }, Light, Thick * 0.45f, 0.9f);
			// the septum's pearly wall, with a soft shadow cast into the chamber beside it
			P.Mark([Room](FV Q) { return FMath::Abs(SdPoly(Q, Room) + 0.006f) - 0.004f; }, Hex(0x06141A, 0.35f), 0.006f);
			P.Mark([Room](FV Q) { return FMath::Abs(SdPoly(Q, Room)) - 0.0042f; }, Hex(0xEEE4D4));
			P.Mark([Room](FV Q) { return FMath::Abs(SdPoly(Q, Room) - 0.0015f) - 0.0012f; }, Hex(0xFFFFFF, 0.7f));
		}
		// the living chamber at the mouth: a deep, shadowed hollow lined with nacre
		{
			const TArray<FV> Mouth = RoomPts(0.0f, TStart);
			P.Mark([Mouth](FV Q) { return SdPoly(Q, Mouth); }, Hex(0x2A2232));
			// mother-of-pearl catching the light round the opening, fading into the shadowed back of the chamber
			const FV Lip = At(0.15f, 0.62f);
			P.Mark([Lip](FV Q) { return SdCircle(Q, Lip, 0.05f); }, Hex(0xE8C8D8, 0.75f), 0.12f);
			P.Mark([Lip](FV Q) { return SdCircle(Q, Lip + FV(0.03f, 0.04f), 0.02f); }, Hex(0xB8F0E8, 0.45f), 0.06f);
			P.Mark([Mouth](FV Q) { return FMath::Abs(SdPoly(Q, Mouth)) - 0.0042f; }, Hex(0xEEE4D4));
		}
		// the siphuncle: a thin pearly tube threading every chamber
		TArray<FV> Siphon;
		for (int32 i = 0; i <= 60; ++i)
		{
			const float T = TStart + (TEnd - TStart) * i / 60.0f;
			Siphon.Add(At(T, 0.66f));
		}
		P.MLine(Siphon, 0.0045f, Hex(0xFFF0E0, 0.8f));
		// the shell wall seen in section round the outside
		P.Mark([&Outline](FV Q) { return FMath::Abs(SdPoly(Q, Outline) + 0.009f) - 0.009f; }, Hex(0xF4ECDC));
		P.Mark([&Outline](FV Q) { return FMath::Abs(SdPoly(Q, Outline) + 0.02f) - 0.002f; }, Hex(0x8A7A6A, 0.5f));
		// motes of light drifting up out of the newest chambers
		for (int32 k = 0; k < 7; ++k)
		{
			const FV M = O + FV(-0.18f + Rand01(1611, k) * 0.3f, -0.08f - Rand01(1612, k) * 0.2f);
			P.Glow([M](FV Q) { return SdCircle(Q, M, 0.004f); }, Hex(0x9AFFD8), 0.025f, 0.6f);
			P.MCircle(M, 0.005f + 0.004f * Rand01(1613, k), Hex(0xE8FFF4, 0.9f));
		}
	}

	void HDBoonBountyHunters(FPainter& P)
	{
		// a hunter's net hauled up heavy with pearls, its neck lashed with rope to a cork float
		const auto Bag = [](FV Q)
		{
			const float Belly = SdEllipse(Q, FV(0.5f, 0.62f), 0.33f, 0.27f);
			const float Neck = SdPoly(Q, { FV(0.43f, 0.24f), FV(0.57f, 0.24f), FV(0.7f, 0.44f), FV(0.3f, 0.44f) });
			return SMin(Belly, Neck, 0.1f);
		};
		P.Mark([&Bag](FV Q) { return Bag(Q - FV(0.012f, 0.025f)); }, Hex(0x06121A, 0.3f), 0.03f);
		// the pearls packed inside, darker toward the back
		P.Fill(Bag, Hex(0x1A3040));
		{
			FClip Clip(P, Bag);
			for (int32 i = 0; i < 22; ++i)
			{
				const float Row = (float)(i / 5);
				const FV At(0.22f + (i % 5) * 0.14f + (i / 5 % 2) * 0.07f + (Rand01(1621, i) - 0.5f) * 0.03f, 0.84f - Row * 0.12f + (Rand01(1622, i) - 0.5f) * 0.03f);
				const float Rr = 0.055f + Rand01(1623, i) * 0.02f;
				const FLinearColor Body = i % 7 == 3 ? Hex(0xF4D0E0) : (i % 5 == 1 ? Hex(0xF0E8D8) : Hex(0xF4F0EC));
				HDPearl(P, At, Rr, Body, Hex(0xA8A0C8));
			}
			P.MEllipse(FV(0.5f, 0.66f), 0.36f, 0.3f, Hex(0x06121A, 0.25f), 0.12f);
		}
		// the net: tarred cord knotted into diamonds that stretch round the load
		const auto Mesh = [](FV Q, float Off)
		{
			const FV C(0.5f, 0.62f);
			const FV L = Q - C;
			const float Bulge = 1.0f - 0.35f * FMath::Clamp(1.0f - L.SizeSquared() / 0.12f, 0.0f, 1.0f);
			const float U = (L.X + L.Y * 0.9f) * Bulge + Off, V = (L.X - L.Y * 0.9f) * Bulge + Off;
			const float Step = 0.085f;
			const auto Line = [Step](float X) { return FMath::Abs(X / Step - FMath::RoundToFloat(X / Step)) * Step; };
			return FMath::Min(Line(U), Line(V));
		};
		{
			FClip Clip(P, Bag);
			P.Mark([&Mesh](FV Q) { return Mesh(Q - FV(0.004f, 0.006f), 0.0f) - 0.0085f; }, Hex(0x0A1418, 0.6f));
			P.Mark([&Mesh](FV Q) { return Mesh(Q, 0.0f) - 0.0065f; }, Hex(0x8A6A3A));
			P.Mark([&Mesh](FV Q) { return Mesh(Q + FV(0.002f, 0.003f), 0.0f) - 0.002f; }, Hex(0xE8C890, 0.8f));
		}
		P.Mark([&Bag](FV Q) { return FMath::Abs(Bag(Q)) - 0.008f; }, Hex(0x6A4A22));
		// the drawn-up neck, lashed with several turns of rope
		for (int32 k = 0; k < 3; ++k)
		{
			const float Y = 0.26f + k * 0.035f;
			HDCord(P, Smooth({ FV(0.41f - k * 0.01f, Y), FV(0.5f, Y + 0.015f), FV(0.59f + k * 0.01f, Y) }), 0.014f, RopeColor);
		}
		// the rope running up to a cork float, and a pearl that slipped through the mesh
		HDCord(P, Smooth({ FV(0.58f, 0.27f), FV(0.66f, 0.18f), FV(0.76f, 0.16f) }), 0.011f, RopeColor);
		{
			const auto Cork = [](FV Q) { return SdEllipse(Q, FV(0.82f, 0.15f), 0.07f, 0.055f) + Noise2(Q * 30.0f) * 0.004f; };
			{
				FWith With(P);
				P.Look.Dome = 0.05f;
				P.Look.Grain = 0.45f;
				P.Fill(Cork, Hex(0xC8783A));
			}
			FClip Clip(P, Cork);
			P.Mark([](FV Q) { return FMath::Abs(Q.X - 0.82f) - 0.016f; }, Hex(0xE84A3A, 0.85f));
			Pits(P, FV(0.75f, 0.1f), FV(0.89f, 0.2f), 14, 0.004f, Hex(0x5A2A10, 0.6f), 1624);
		}
		HDPearl(P, FV(0.2f, 0.9f), 0.045f, Hex(0xF4F0EC), Hex(0xA8A0C8));
		Glint(P, FV(0.27f, 0.5f), 0.08f);
	}

	void HDBoonCheapUpgrades(FPainter& P)
	{
		// a crab's claw has snipped a bite out of a gold coin: the price, cut down
		const FV C(0.42f, 0.58f);
		const float R = 0.3f;
		const FV BiteC = C + Spin(FV(R * 1.02f, 0.0f), -38.0f);
		const auto Bite = [BiteC, R](FV Q)
		{
			// a bite with a ragged, scalloped edge where each cut landed
			const FV L = Q - BiteC;
			const float A = FMath::Atan2(L.Y, L.X);
			return SdCircle(Q, BiteC, R * 0.42f) - FMath::Abs(FMath::Sin(A * 4.0f)) * R * 0.05f;
		};
		Coin(P, C, R);
		P.Erase(Bite);
		// the coin's thickness showing along the cut, and bright fresh metal at the edge
		{
			FClip Clip(P, [C, R](FV Q) { return SdCircle(Q, C, R); });
			P.Mark([&Bite](FV Q) { return FMath::Abs(Bite(Q) - 0.012f) - 0.012f; }, Hex(0x9A6418));
			P.Mark([&Bite](FV Q) { return FMath::Abs(Bite(Q) - 0.003f) - 0.004f; }, Hex(0xFFF0B0, 0.95f));
		}
		// gold crumbs falling from the cut
		for (int32 k = 0; k < 5; ++k)
		{
			const FV At(0.6f + Rand01(1631, k) * 0.12f, 0.5f + Rand01(1632, k) * 0.2f);
			P.Ball(At, 0.008f + Rand01(1633, k) * 0.007f, Gold, 0.9f);
		}
		// the claw, reaching in from the upper right, holding the snipped piece between its two pincer fingers
		FNest Place(P, FV(0.66f, 0.3f), 1.0f, -32.0f);
		{
			const TArray<FV> Shard = { FV(-0.045f, -0.03f), FV(0.04f, -0.038f), FV(0.055f, 0.008f), FV(0.012f, 0.045f), FV(-0.04f, 0.03f) };
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Spec = 0.9f;
			P.Fill([&Shard](FV Q) { return SdPoly(Q, Shard) - 0.004f; }, Gold);
			P.MLine({ FV(-0.035f, -0.02f), FV(0.035f, -0.026f) }, 0.004f, Hex(0xFFF4C0, 0.8f));
			P.MLine({ FV(0.05f, 0.012f), FV(0.01f, 0.042f) }, 0.006f, Hex(0x9A6418, 0.8f));
		}
		{
			const FLinearColor Shell = Hex(0xE0503A), ShellDark = Hex(0x8A2418), Tip = Hex(0x3A1810);
			// two curved fingers closing on the piece, the upper one hinged, both tapering to dark points
			const TArray<FV> Upper = Smooth({ FV(0.16f, -0.05f), FV(0.07f, -0.075f), FV(-0.01f, -0.06f), FV(-0.06f, -0.025f) });
			const TArray<FV> Lower = Smooth({ FV(0.16f, 0.05f), FV(0.07f, 0.075f), FV(-0.01f, 0.06f), FV(-0.06f, 0.028f) });
			const auto Fingers = [&Upper, &Lower](FV Q) { return FMath::Min(SdRibbon(Q, Upper, 0.036f, 0.007f), SdRibbon(Q, Lower, 0.04f, 0.008f)); };
			// a heavy palm and the wrist joint running off the top right of the icon
			const auto Palm = [](FV Q) { return SMin(SdEllipse(Q, FV(0.24f, 0.0f), 0.12f, 0.095f), SdCapsule(Q, FV(0.3f, -0.01f), FV(0.46f, -0.04f), 0.06f), 0.04f); };
			const auto Pincer = [&Fingers, &Palm](FV Q) { return SMin(Palm(Q), Fingers(Q), 0.04f); };
			P.Mark([&Pincer](FV Q) { return Pincer(Q - FV(0.01f, 0.02f)); }, Hex(0x06121A, 0.3f), 0.03f);
			{
				FWith With(P);
				P.Look.Dome = 0.08f;
				P.Look.Spec = 0.75f;
				P.Fill(Pincer, Shell);
			}
			FClip Clip(P, Pincer);
			// the tips darken over their last third
			P.Mark([&Fingers](FV Q) { return Fingers(Q) + FMath::Clamp((Q.X + 0.005f) * 3.0f, 0.0f, 1.0f) * 0.06f; }, Tip, 0.02f);
			Granules(P, Pincer, FV(0.04f, -0.12f), FV(0.5f, 0.12f), 70, 0.004f, 0.01f, Alpha(ShellDark, 0.6f), Hex(0xFF9A80), 1634);
			// the seam where the wrist meets the palm, the palm's paler underside and a long sheen across the top
			P.MLine(Smooth({ FV(0.33f, -0.07f), FV(0.35f, -0.01f), FV(0.33f, 0.05f) }), 0.005f, Alpha(ShellDark, 0.8f));
			P.MEllipse(FV(0.22f, 0.06f), 0.1f, 0.03f, Hex(0xF8C0A0, 0.4f), 0.03f);
			P.MLine(Smooth({ FV(0.3f, -0.075f), FV(0.18f, -0.08f), FV(0.06f, -0.09f) }), 0.006f, Hex(0xFFD8C8, 0.6f));
			// small teeth along both inner edges, biting into the gold
			for (int32 k = 0; k < 3; ++k)
			{
				const float X = 0.01f + k * 0.04f;
				P.MPoly({ FV(X - 0.008f, -0.045f), FV(X + 0.008f, -0.045f), FV(X, -0.03f) }, Hex(0xF8E8D8, 0.9f));
				P.MPoly({ FV(X - 0.008f, 0.048f), FV(X + 0.008f, 0.048f), FV(X, 0.033f) }, Hex(0xF8E8D8, 0.9f));
			}
		}
	}

	void HDBoonTreasureCurrents(FPainter& P)
	{
		// an open treasure chest on the sand, the current swirling round it and carrying out coins and pearls
		const FLinearColor Plank = Hex(0x9A5E2E), PlankDark = Hex(0x4A2810), IronC = Hex(0x4A5560);
		HDMound(P, FV(0.5f, 0.88f), 0.42f, 0.07f, Hex(0xD8C090), 1641, false);
		// the open lid, swung back on its hinges: we see its curved underside, the planks bending round the barrel
		// top, a lighter rim where the wood's edge shows, and the iron straps following the curve
		const auto LidAt = [](float U, float V)
		{
			// U across (0 left, 1 right), V up from the hinge (0) to the crown (1); the sides flare a little and the top arches
			const float X = 0.5f + (U - 0.5f) * (0.6f + 0.04f * V);
			const float Y = 0.5f - V * (0.2f + 0.13f * FMath::Sin(U * PI));
			return FV(X, Y);
		};
		TArray<FV> LidPts;
		for (int32 i = 0; i <= 4; ++i) { LidPts.Add(LidAt(0.0f, i / 4.0f)); }
		for (int32 i = 1; i <= 16; ++i) { LidPts.Add(LidAt(i / 16.0f, 1.0f)); }
		for (int32 i = 3; i >= 0; --i) { LidPts.Add(LidAt(1.0f, i / 4.0f)); }
		const auto Lid = [&LidPts](FV Q) { return SdPoly(Q, LidPts); };
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Grain = 0.25f;
			P.Look.Spec = 0.25f;
			P.Fill(Lid, Tone(Plank, 0.72f));
		}
		{
			FClip Clip(P, Lid);
			// plank seams bending with the arch, and fine grain between them
			for (int32 k = 1; k < 5; ++k)
			{
				TArray<FV> Seam;
				for (int32 i = 0; i <= 12; ++i) { Seam.Add(LidAt(i / 12.0f, k / 5.0f)); }
				P.MLine(Seam, 0.004f, Alpha(PlankDark, 0.85f));
				TArray<FV> Lit;
				for (int32 i = 0; i <= 12; ++i) { Lit.Add(LidAt(i / 12.0f, k / 5.0f) + FV(0.0f, 0.007f)); }
				P.MLine(Lit, 0.0016f, Hex(0xFFD8A8, 0.25f));
			}
			for (int32 k = 0; k < 8; ++k)
			{
				const float V = 0.08f + Rand01(1648, k) * 0.84f;
				TArray<FV> Grain;
				for (int32 i = 0; i <= 10; ++i) { Grain.Add(LidAt(0.05f + i * 0.09f, V + FMath::Sin(i * 1.3f + k) * 0.015f)); }
				P.MLine(Grain, 0.0015f, Alpha(PlankDark, 0.45f));
			}
			// the iron straps, curving up the inside, riveted
			for (const float U : { 0.14f, 0.86f })
			{
				TArray<FV> Strap;
				for (int32 i = 0; i <= 8; ++i) { Strap.Add(LidAt(U, i / 8.0f)); }
				P.Fill([Strap](FV Q) { return SdRibbon(Q, Strap, 0.026f, 0.026f); }, Tone(IronC, 0.85f));
				for (int32 i = 1; i < 4; ++i) { Rivet(P, LidAt(U, i / 4.0f), 0.009f); }
			}
			// shadow deepening toward the hinge, and gold light thrown up onto it from the treasure
			P.Mark([](FV Q) { return SdBox(Q, FV(0.0f, 0.44f), FV(1.0f, 0.6f), 0.0f); }, Hex(0x1A0A04, 0.45f), 0.08f);
			P.Glow([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.46f), 0.22f, 0.04f); }, Hex(0xFFD860), 0.12f, 0.55f);
			// the lid's lighter wooden rim, where the edge of the boards shows
			P.Mark([&Lid](FV Q) { return FMath::Abs(Lid(Q) + 0.011f) - 0.011f; }, Tone(Plank, 1.1f));
			P.Mark([&Lid](FV Q) { return FMath::Abs(Lid(Q) + 0.022f) - 0.0025f; }, Alpha(PlankDark, 0.8f));
		}
		// iron hinge plates where the lid meets the chest
		for (const float U : { 0.14f, 0.86f })
		{
			const FV H = LidAt(U, 0.0f);
			FWith With(P);
			P.Look.Spec = 0.6f;
			P.Fill([H](FV Q) { return SdBox(Q, H - FV(0.03f, 0.03f), H + FV(0.03f, 0.012f), 0.006f); }, IronC);
		}
		// the heap of gold glowing in the chest's mouth
		const auto Heap = [](FV Q) { return SMin(SMin(SdCircle(Q, FV(0.4f, 0.52f), 0.12f), SdCircle(Q, FV(0.56f, 0.48f), 0.14f), 0.06f), SdCircle(Q, FV(0.68f, 0.53f), 0.1f), 0.06f); };
		P.Glow(Heap, Hex(0xFFD860), 0.1f, 0.6f);
		P.Fill([&Heap](FV Q) { return FMath::Max(Heap(Q), 0.0f); }, Hex(0xE8B040));
		{
			FClip Clip(P, Heap);
			for (int32 k = 0; k < 16; ++k)
			{
				const FV At(0.3f + Rand01(1642, k) * 0.46f, 0.38f + Rand01(1643, k) * 0.16f);
				P.MEllipse(At, 0.03f, 0.012f, Hex(0xFFF0A0, 0.9f));
				P.MEllipse(At + FV(0.0f, 0.008f), 0.03f, 0.006f, Hex(0x9A6418, 0.6f));
			}
		}
		HDPearl(P, FV(0.47f, 0.4f), 0.04f, Hex(0xF4F0EC), Hex(0xA8A0C8));
		HDPearl(P, FV(0.63f, 0.37f), 0.035f, Hex(0xF4D0E0), Hex(0xA8A0C8));
		// the chest body: three planks bound with riveted iron, a lock plate in front
		const auto Body = [](FV Q) { return SdBox(Q, FV(0.2f, 0.5f), FV(0.8f, 0.84f), 0.02f); };
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Grain = 0.35f;
			P.Look.Spec = 0.3f;
			P.Fill(Body, Plank);
		}
		{
			FClip Clip(P, Body);
			for (int32 k = 1; k < 3; ++k)
			{
				const float Y = 0.5f + k * 0.113f;
				P.MLine({ FV(0.2f, Y), FV(0.8f, Y) }, 0.005f, Alpha(PlankDark, 0.9f));
				P.MLine({ FV(0.2f, Y + 0.008f), FV(0.8f, Y + 0.008f) }, 0.002f, Hex(0xFFD8A8, 0.3f));
			}
			for (int32 k = 0; k < 10; ++k)
			{
				const float Y = 0.52f + Rand01(1644, k) * 0.3f;
				P.MLine(Smooth({ FV(0.2f, Y), FV(0.45f, Y + 0.006f), FV(0.62f, Y - 0.004f), FV(0.8f, Y + 0.004f) }), 0.0018f, Alpha(PlankDark, 0.5f));
			}
			for (const float X : { 0.27f, 0.73f })
			{
				P.Fill([X](FV Q) { return SdBox(Q, FV(X - 0.03f, 0.48f), FV(X + 0.03f, 0.86f), 0.004f); }, IronC);
				for (int32 k = 0; k < 3; ++k)
				{
					Rivet(P, FV(X, 0.56f + k * 0.11f), 0.011f);
				}
			}
			P.MEllipse(FV(0.5f, 0.86f), 0.36f, 0.06f, Hex(0x1A0A04, 0.5f), 0.05f);
		}
		{
			const auto Plate = [](FV Q) { return SdBox(Q, FV(0.45f, 0.5f), FV(0.55f, 0.62f), 0.012f); };
			FWith With(P);
			P.Look.Spec = 0.9f;
			P.Fill(Plate, Brass);
			P.MCircle(FV(0.5f, 0.55f), 0.012f, Hex(0x2A1408));
			P.MPoly({ FV(0.494f, 0.555f), FV(0.506f, 0.555f), FV(0.51f, 0.59f), FV(0.49f, 0.59f) }, Hex(0x2A1408));
		}
		// the current: broken streaks of moving water arcing over the chest, carrying coins and pearls out of it
		for (int32 Lane = 0; Lane < 3; ++Lane)
		{
			const float Off = Lane * 0.05f;
			const TArray<FV> Stream = Smooth({ FV(0.1f - Off * 0.5f, 0.62f), FV(0.12f - Off, 0.36f), FV(0.3f - Off * 0.5f, 0.16f - Off), FV(0.56f, 0.08f - Off), FV(0.82f + Off * 0.5f, 0.14f - Off * 0.5f), FV(0.92f + Off, 0.3f) });
			for (int32 Dash = 0; Dash < 5; ++Dash)
			{
				const float T0 = (Dash + Rand01(1646, Lane * 5 + Dash) * 0.3f) / 5.0f;
				const float T1 = FMath::Min(1.0f, T0 + 0.1f + Rand01(1647, Lane * 5 + Dash) * 0.06f);
				TArray<FV> Seg;
				for (int32 k = 0; k <= 8; ++k)
				{
					FV Pos, Tan;
					Along(Stream, FMath::Lerp(T0, T1, k / 8.0f), Pos, Tan);
					Seg.Add(Pos);
				}
				P.Mark([Seg](FV Q) { return SdRibbon(Q, Seg, 0.0015f, 0.007f); }, Hex(0xBFF0FF, 0.75f - Lane * 0.15f));
			}
		}
		const TArray<FV> Carry = Smooth({ FV(0.16f, 0.5f), FV(0.2f, 0.28f), FV(0.36f, 0.14f), FV(0.58f, 0.1f), FV(0.8f, 0.16f), FV(0.9f, 0.34f) });
		for (int32 k = 0; k < 4; ++k)
		{
			FV Pos, Tan;
			Along(Carry, 0.18f + k * 0.21f, Pos, Tan);
			if (k % 2 == 0)
			{
				FNest Place(P, Pos, 1.0f, FMath::RadiansToDegrees(FMath::Atan2(Tan.Y, Tan.X)) + 60.0f);
				FWith With(P);
				P.Look.Spec = 0.9f;
				P.Fill([](FV Q) { return SdEllipse(Q, FV(0.0f, 0.0f), 0.04f, 0.016f); }, Gold);
				P.MEllipse(FV(0.0f, -0.003f), 0.028f, 0.008f, Hex(0xFFF4C0, 0.7f));
			}
			else
			{
				HDPearl(P, Pos, 0.028f, Hex(0xF4F0EC), Hex(0xA8A0C8));
			}
		}
		BubbleTrail(P, FV(0.9f, 0.7f), 4, 0.016f, 1645);
	}

	void HDBoonLuckyFind(FPainter& P)
	{
		// a rare golden cowrie turned up in the sand, gleaming
		HDMound(P, FV(0.5f, 0.82f), 0.44f, 0.1f, Hex(0xD8C090), 1651, true);
		// the cowrie: a glossy dome, spotted, with its toothed lip just showing underneath
		FNest Place(P, FV(0.5f, 0.6f), 1.0f, -14.0f);
		const auto Dome = [](FV Q) { return SdEllipse(Q, FV(0.0f, 0.0f), 0.3f, 0.19f); };
		P.Mark([&Dome](FV Q) { return Dome(Q - FV(0.01f, 0.04f)); }, Hex(0x3A2808, 0.45f), 0.04f);
		// the base and lip, cream with dark teeth across the slit
		const auto Base = [](FV Q) { return SdEllipse(Q, FV(0.0f, 0.07f), 0.29f, 0.14f); };
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Fill(Base, Hex(0xF2DCA8));
		}
		{
			FClip Clip(P, Base);
			P.MLine(Smooth({ FV(-0.26f, 0.13f), FV(0.0f, 0.165f), FV(0.26f, 0.13f) }), 0.01f, Hex(0x3A2410));
			for (int32 k = 0; k < 15; ++k)
			{
				const float X = -0.22f + k * 0.031f;
				const float Y = 0.15f + 0.015f * (1.0f - FMath::Square(X / 0.26f));
				P.MCapsule(FV(X, Y - 0.018f), FV(X, Y + 0.012f), 0.0045f, Hex(0x6A4420, 0.85f));
			}
		}
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 1.0f;
			P.Fill([&Dome](FV Q) { return FMath::Max(Dome(Q), Q.Y - 0.12f); }, Hex(0xF0B830));
		}
		{
			FClip Clip(P, [&Dome](FV Q) { return FMath::Max(Dome(Q), Q.Y - 0.12f); });
			// a darker band of colour across the back, pale spots, and a violet sheen at the rim
			P.MEllipse(FV(0.0f, -0.02f), 0.3f, 0.06f, Hex(0xC07818, 0.45f), 0.05f);
			for (int32 k = 0; k < 26; ++k)
			{
				const FV At((Rand01(1652, k) - 0.5f) * 0.52f, (Rand01(1653, k) - 0.5f) * 0.26f);
				const float Rr = 0.008f + Rand01(1654, k) * 0.014f;
				P.MCircle(At, Rr, Hex(0xFFF0C0, 0.75f));
				P.Mark([At, Rr](FV Q) { return FMath::Abs(SdCircle(Q, At, Rr)) - 0.002f; }, Hex(0xA06010, 0.5f));
			}
			P.Mark([&Dome](FV Q) { return FMath::Abs(Dome(Q)) - 0.02f; }, Hex(0xC890FF, 0.3f), 0.02f);
			// the mirror-gloss of a cowrie: a crisp window reflection and a soft long highlight
			P.MEllipse(FV(-0.08f, -0.11f), 0.14f, 0.035f, Hex(0xFFFFFF, 0.55f), 0.02f);
			P.MEllipse(FV(-0.12f, -0.115f), 0.035f, 0.016f, Hex(0xFFFFFF, 0.95f));
		}
		HDSpark(P, FV(0.17f, -0.14f), 0.14f, Hex(0xFFF4C0));
		HDSpark(P, FV(-0.26f, 0.0f), 0.07f, Hex(0xFFF4C0));
	}

	void HDBoonKrakensPact(FPainter& P)
	{
		// a contract on old parchment, sealed in red wax with the kraken's mark, a tentacle coiled round it
		const FLinearColor Parch = Hex(0xEAD8AE), ParchDark = Hex(0x8A6A3A);
		{
			FNest Place(P, FV(0.48f, 0.5f), 1.0f, -12.0f);
			const auto Sheet = [](FV Q) { return SdBox(Q, FV(-0.22f, -0.27f), FV(0.22f, 0.27f), 0.01f) + Noise2(Q * 14.0f) * 0.006f; };
			{
				FWith With(P);
				P.Look.Dome = 0.03f;
				P.Look.Grain = 0.25f;
				P.Fill(Sheet, Parch);
			}
			{
				FClip Clip(P, Sheet);
				// age stains, lines of script and a signature in red
				P.MEllipse(FV(0.12f, 0.16f), 0.1f, 0.07f, Hex(0xB89050, 0.3f), 0.06f);
				P.MEllipse(FV(-0.14f, -0.18f), 0.08f, 0.05f, Hex(0xB89050, 0.25f), 0.05f);
				for (int32 Line = 0; Line < 7; ++Line)
				{
					const float Y = -0.19f + Line * 0.052f;
					float X = -0.17f;
					int32 W = 0;
					while (X < 0.15f - (Line == 6 ? 0.12f : 0.0f))
					{
						const float Len = 0.025f + Rand01(1661, Line * 20 + W) * 0.05f;
						TArray<FV> Word;
						for (int32 s = 0; s <= 6; ++s)
						{
							Word.Add(FV(X + Len * s / 6.0f, Y + FMath::Sin(s * 2.4f + W) * 0.005f));
						}
						P.MLine(Word, 0.003f, Hex(0x3A2A1A, 0.75f));
						X += Len + 0.015f;
						++W;
					}
				}
				P.MLine(Smooth({ FV(-0.16f, 0.2f), FV(-0.12f, 0.17f), FV(-0.09f, 0.22f), FV(-0.05f, 0.18f), FV(0.0f, 0.21f), FV(0.04f, 0.19f) }), 0.004f, Hex(0x9A1A1A, 0.85f));
				P.Mark([&Sheet](FV Q) { return FMath::Abs(Sheet(Q)) - 0.012f; }, Hex(0xA8804A, 0.35f), 0.015f);
			}
			// the rolled ends, top and bottom
			for (const float Y : { -0.27f, 0.27f })
			{
				const auto Roll = [Y](FV Q) { return SdCapsule(Q, FV(-0.235f, Y), FV(0.235f, Y), 0.034f); };
				{
					FWith With(P);
					P.Look.Dome = 0.04f;
					P.Look.Spec = 0.4f;
					P.Fill(Roll, Tone(Parch, 0.95f));
				}
				FClip Clip(P, Roll);
				P.MLine({ FV(-0.235f, Y + 0.018f), FV(0.235f, Y + 0.018f) }, 0.008f, Alpha(ParchDark, 0.45f));
				P.MLine({ FV(-0.235f, Y - 0.014f), FV(0.235f, Y - 0.014f) }, 0.004f, Hex(0xFFF8E8, 0.6f));
				for (const float X : { -0.235f, 0.235f })
				{
					P.Mark([X, Y](FV Q) { return FMath::Abs(SdCircle(Q, FV(X, Y), 0.022f)) - 0.003f; }, Alpha(ParchDark, 0.7f));
				}
			}
		}
		// the tentacle sweeping up across the contract
		HDArm(P, { FV(0.02f, 0.86f), FV(0.24f, 0.74f), FV(0.5f, 0.66f), FV(0.72f, 0.5f), FV(0.76f, 0.34f), FV(0.66f, 0.24f) }, 0.075f, 0.022f, 1.0f, Hex(0x8A3AA0), Hex(0xE8B0E8));
		// the wax seal, dripped over the tentacle and stamped with a kraken's eye
		const FV S(0.36f, 0.72f);
		const auto Wax = [S](FV Q) { const FV L = Q - S; return SdCircle(Q, S, 0.085f) + FMath::Sin(FMath::Atan2(L.Y, L.X) * 7.0f) * 0.008f; };
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Spec = 0.85f;
			P.Fill(Wax, Hex(0xB81E28));
		}
		{
			FClip Clip(P, Wax);
			P.Mark([S](FV Q) { return FMath::Abs(SdCircle(Q, S, 0.06f)) - 0.004f; }, Hex(0x6A0A10, 0.8f));
			P.MEllipse(S, 0.035f, 0.02f, Hex(0x6A0A10, 0.85f));
			P.MEllipse(S, 0.012f, 0.018f, Hex(0xE84A50, 0.9f));
			for (int32 k = 0; k < 6; ++k)
			{
				const FV D = Spin(FV(1.0f, 0.0f), 30.0f + k * 24.0f);
				P.MLine(Smooth({ S + D * 0.04f, S + D * 0.055f + Spin(D, 90.0f) * 0.008f }), 0.004f, Hex(0x6A0A10, 0.75f));
			}
			P.MEllipse(S + FV(-0.03f, -0.035f), 0.025f, 0.012f, Hex(0xFFFFFF, 0.45f), 0.01f);
		}
		P.Fill([S](FV Q) { return SdRibbon(Q, Smooth({ S + FV(0.04f, 0.07f), S + FV(0.05f, 0.11f), S + FV(0.045f, 0.135f) }), 0.014f, 0.01f); }, Hex(0xA81A24));
	}

	void HDBoonHungryDeep(FPainter& P)
	{
		// a creature of the deep, all mouth: a gaping maw of needle teeth under two pale eyes, lit by its own lure
		const FV C(0.5f, 0.55f);
		const auto Head = [C](FV Q) { return SdEllipse(Q, C, 0.39f, 0.34f) + Noise2(Q * 9.0f) * 0.012f; };
		{
			FWith With(P);
			P.Look.Dome = 0.25f;
			P.Look.Spec = 0.45f;
			P.Look.Grain = 0.2f;
			P.Fill(Head, Hex(0x2E2A4A));
		}
		const auto Mouth = [C](FV Q) { return SdEllipse(Q, C + FV(0.0f, 0.07f), 0.27f, 0.19f); };
		{
			FClip Clip(P, Head);
			// mottled skin and a row of glowing photophores along the flank
			Speckle(P, C - FV(0.4f, 0.35f), C + FV(0.4f, 0.35f), 120, 0.004f, 0.012f, Hex(0x14102A, 0.5f), 1671);
			for (int32 k = 0; k < 9; ++k)
			{
				const float A = FMath::DegreesToRadians(150.0f + k * 30.0f);
				const FV At = C + FV(FMath::Cos(A) * 0.33f, FMath::Sin(A) * 0.28f + 0.03f);
				if (At.Y < C.Y - 0.16f)
				{
					continue;
				}
				P.Glow([At](FV Q) { return SdCircle(Q, At, 0.006f); }, Hex(0x60F0FF), 0.025f, 0.8f);
				P.MCircle(At, 0.009f, Hex(0xC8FCFF));
			}
			// fleshy lips round the gape
			P.Mark([&Mouth](FV Q) { return Mouth(Q) - 0.03f; }, Hex(0x5A3A62));
			P.Mark([&Mouth](FV Q) { return FMath::Abs(Mouth(Q) - 0.03f) - 0.004f; }, Hex(0x14102A, 0.8f));
			// the throat: gums, then darkness
			P.Mark(Mouth, Hex(0x8A2440));
			P.Mark([&Mouth](FV Q) { return Mouth(Q) + 0.03f; }, Hex(0x1A0410), 0.06f);
			P.Mark([C](FV Q) { return SdEllipse(Q, C + FV(0.0f, 0.1f), 0.12f, 0.08f); }, Hex(0x000000, 0.9f), 0.06f);
		}
		// needle teeth of every length, top row hanging down, bottom row jutting up, all leaning inward
		{
			FClip Clip(P, [&Mouth](FV Q) { return Mouth(Q) - 0.012f; });
			for (int32 Row = 0; Row < 2; ++Row)
			{
				const int32 Count = Row == 0 ? 9 : 8;
				for (int32 k = 0; k < Count; ++k)
				{
					const float T = (k + 0.5f) / Count;
					const float X = C.X - 0.24f + T * 0.48f;
					const float Edge = 0.19f * FMath::Sqrt(FMath::Max(0.0f, 1.0f - FMath::Square((X - C.X) / 0.27f)));
					const float Long = 0.07f + 0.08f * Rand01(1672 + Row, k) + 0.05f * (1.0f - FMath::Abs(T - 0.5f) * 2.0f);
					const float Lean = (X - C.X) * -0.35f;
					if (Row == 0)
					{
						const FV Root(X, C.Y + 0.07f - Edge + 0.01f);
						HDFang(P, Root, Root + FV(Lean, Long), 0.013f, 0.008f);
					}
					else
					{
						const FV Root(X + 0.012f, C.Y + 0.07f + Edge - 0.01f);
						HDFang(P, Root, Root + FV(Lean, -Long * 0.85f), 0.012f, -0.008f);
					}
				}
			}
		}
		// small, pale, filmy eyes set high on the head
		for (const float Side : { -1.0f, 1.0f })
		{
			const FV E = C + FV(Side * 0.17f, -0.19f);
			P.Glow([E](FV Q) { return SdCircle(Q, E, 0.02f); }, Hex(0x9AF0FF), 0.03f, 0.4f);
			HDEye(P, E, 0.036f, Hex(0x60D0E0), FV(-Side * 0.15f, 0.2f), 0.5f);
			P.MEllipse(E + FV(0.0f, -0.02f), 0.045f, 0.018f, Hex(0x2E2A4A, 0.85f));
		}
		// the lure: a stalk arching up from the brow to a glowing bulb
		const TArray<FV> Stalk = Smooth({ C + FV(0.0f, -0.3f), C + FV(0.06f, -0.44f), C + FV(0.18f, -0.5f), C + FV(0.26f, -0.44f) });
		P.Fill([&Stalk](FV Q) { return SdRibbon(Q, Stalk, 0.014f, 0.008f); }, Hex(0x3A3458));
		const FV Bulb = Stalk.Last() + FV(0.01f, 0.03f);
		P.Glow([Bulb](FV Q) { return SdCircle(Q, Bulb, 0.03f); }, Hex(0x60F0FF), 0.1f, 0.9f);
		P.Ball(Bulb, 0.035f, Hex(0x9AF8FF), 0.9f);
		P.MCircle(Bulb, 0.016f, Hex(0xFFFFFF, 0.85f));
	}

	void HDBoonEclipse(FPainter& P)
	{
		// the sun swallowed by a dark moon, its corona flaring round the rim, seen over the sea's surface
		const FV C(0.5f, 0.42f);
		const float R = 0.2f;
		// corona: a soft glow and long uneven streamers
		P.Glow([C, R](FV Q) { return SdCircle(Q, C, R); }, Hex(0xFFB030), 0.16f, 0.9f);
		for (int32 k = 0; k < 22; ++k)
		{
			const float A = k * 360.0f / 22.0f + (Rand01(1681, k) - 0.5f) * 10.0f;
			const float Len = R * (0.35f + Rand01(1682, k) * 0.55f);
			const FV D = Spin(FV(1.0f, 0.0f), A);
			const TArray<FV> Ray = Smooth({ C + D * R * 0.95f, C + Spin(D, 3.0f) * (R + Len * 0.6f), C + Spin(D, 6.0f) * (R + Len) });
			P.Mark([Ray, R](FV Q) { return SdRibbon(Q, Ray, R * 0.07f, 0.0f); }, Hex(0xFFD870, 0.7f), 0.012f);
		}
		P.Mark([C, R](FV Q) { return FMath::Abs(SdCircle(Q, C, R * 1.02f)) - R * 0.04f; }, Hex(0xFFF0C0), 0.01f);
		// the moon: near black, faintly lit by the earth, a few soft craters
		{
			FWith With(P);
			P.Look.Dome = 0.3f;
			P.Look.Spec = 0.15f;
			P.Fill([C, R](FV Q) { return SdCircle(Q, C, R); }, Hex(0x1A1E2E));
		}
		{
			FClip Clip(P, [C, R](FV Q) { return SdCircle(Q, C, R); });
			for (int32 k = 0; k < 6; ++k)
			{
				const FV At = C + FV((Rand01(1683, k) - 0.5f) * R * 1.4f, (Rand01(1684, k) - 0.5f) * R * 1.4f);
				const float Rr = R * (0.08f + Rand01(1685, k) * 0.12f);
				P.MCircle(At, Rr, Hex(0x0A0C16, 0.5f));
				P.MArc(At, Rr, Rr * 0.25f, 200.0f, 300.0f, Hex(0x4A5070, 0.4f));
			}
			P.MEllipse(C + FV(-R * 0.35f, -R * 0.35f), R * 0.45f, R * 0.3f, Hex(0x5A6A8A, 0.18f), R * 0.3f);
		}
		// the diamond ring: one bead of sunlight breaking past the rim
		const FV Bead = C + Spin(FV(R * 1.0f, 0.0f), -130.0f);
		HDSpark(P, Bead, 0.17f, Hex(0xFFF4D0));
		// the sea below: a band of water whose ripples break the eclipse's reflection into strokes
		const auto Sea = [](FV Q) { return FMath::Max(SdCircle(Q, FV(0.5f, 0.5f), 0.44f), 0.72f - Q.Y + Noise2(Q * 14.0f) * 0.004f); };
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Look.Spec = 0.6f;
			P.Fill(Sea, Hex(0x1A3A6A));
		}
		{
			FClip Clip(P, Sea);
			for (int32 Row = 0; Row < 6; ++Row)
			{
				const float Y = 0.745f + Row * 0.03f;
				const float Half = 0.05f + Row * 0.018f;
				for (int32 k = -1; k <= 1; ++k)
				{
					const float X = 0.5f + k * (Half * 0.9f) + (Rand01(1686, Row * 3 + k + 1) - 0.5f) * 0.04f;
					const float W = Half * (k == 0 ? 0.55f : 0.3f);
					P.Mark([X, Y, W](FV Q) { return SdCapsule(Q, FV(X - W, Y), FV(X + W, Y), 0.005f); }, Hex(0xFFD870, 0.75f - Row * 0.08f));
				}
			}
			for (int32 Row = 0; Row < 4; ++Row)
			{
				const float Y = 0.76f + Row * 0.04f;
				P.MLine(Smooth({ FV(0.06f, Y), FV(0.25f, Y - 0.008f), FV(0.42f, Y + 0.006f), FV(0.6f, Y - 0.006f), FV(0.78f, Y + 0.008f), FV(0.94f, Y) }), 0.0025f, Hex(0x8FD8FF, 0.35f));
			}
			P.MLine(Smooth({ FV(0.06f, 0.73f), FV(0.3f, 0.722f), FV(0.5f, 0.732f), FV(0.7f, 0.722f), FV(0.94f, 0.73f) }), 0.005f, Hex(0xC8F0FF, 0.6f));
		}
	}

	bool PaintBoonHD(FPainter& P, int32 Id)
	{
		switch ((ETDBoon)Id)
		{
		case ETDBoon::SharpenedReef:	HDBoonSharpenedReef(P); return true;
		case ETDBoon::TidalFury:		HDBoonTidalFury(P); return true;
		case ETDBoon::CoralMasons:		HDBoonCoralMasons(P); return true;
		case ETDBoon::ThickReef:		HDBoonThickReef(P); return true;
		case ETDBoon::VenomBloom:		HDBoonVenomBloom(P); return true;
		case ETDBoon::StormCoil:		HDBoonStormCoil(P); return true;
		case ETDBoon::BloodTide:		HDBoonBloodTide(P); return true;
		case ETDBoon::FrenziedCurrents:	HDBoonFrenziedCurrents(P); return true;
		case ETDBoon::BrittleCoral:		HDBoonBrittleCoral(P); return true;
		case ETDBoon::GildedShackles:	HDBoonGildedShackles(P); return true;
		case ETDBoon::GiantSlayer:		HDBoonGiantSlayer(P); return true;
		case ETDBoon::DeepPockets:		HDBoonDeepPockets(P); return true;
		case ETDBoon::SirensDebt:		HDBoonSirensDebt(P); return true;
		case ETDBoon::LongSight:	HDBoonLongSight(P); return true;
		case ETDBoon::ScholarsTide:	HDBoonScholarsTide(P); return true;
		case ETDBoon::BountyHunters:	HDBoonBountyHunters(P); return true;
		case ETDBoon::CheapUpgrades:	HDBoonCheapUpgrades(P); return true;
		case ETDBoon::TreasureCurrents:	HDBoonTreasureCurrents(P); return true;
		case ETDBoon::LuckyFind:	HDBoonLuckyFind(P); return true;
		case ETDBoon::KrakensPact:	HDBoonKrakensPact(P); return true;
		case ETDBoon::HungryDeep:	HDBoonHungryDeep(P); return true;
		case ETDBoon::Eclipse:	HDBoonEclipse(P); return true;
		default:						return false;
		}
	}
