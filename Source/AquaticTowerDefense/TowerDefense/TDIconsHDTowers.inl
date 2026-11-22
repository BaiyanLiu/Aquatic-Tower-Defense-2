// Hand-drawn HD tower portraits (atd.DetailedIcons 2). Included from TDIcons.cpp inside its anonymous namespace,
// after TDIconsHD.inl, whose organic helpers (SMin, HDEye, HDFin, Plates...) these build on.

	// ------------------------------------------------------------------ shared tower pieces

	/** Seed points jittered on a grid over a box, for Voronoi plates and stones */
	TArray<FV> GridSeeds(FV Min, FV Max, int32 Cols, int32 Rows, int32 Seed, float Jitter = 0.7f)
	{
		TArray<FV> Out;
		const FV Step((Max.X - Min.X) / Cols, (Max.Y - Min.Y) / Rows);
		for (int32 y = 0; y < Rows; ++y)
		{
			for (int32 x = 0; x < Cols; ++x)
			{
				const int32 I = y * Cols + x;
				const float Shift = (y & 1) ? 0.5f : 0.0f;
				Out.Add(Min + FV((x + Shift + (Rand01(Seed, I) - 0.5f) * Jitter) * Step.X, (y + 0.5f + (Rand01(Seed, I + 500) - 0.5f) * Jitter) * Step.Y));
			}
		}
		return Out;
	}

	/**
	 *  A run of liquid hanging from Top and swelling into a drop at its end, all one continuous shape:
	 *  the thread of liquid thickens into the bead, so the drop never looks stuck on.
	 */
	void HDDrip(FPainter& P, FV Top, float Len, float R, const FLinearColor& Color, float Lean = 0.0f, float Thread = -1.0f)
	{
		const float W = Thread > 0.0f ? Thread : R * 0.42f;
		const FV Bulb = Top + FV(Lean, Len);
		const TArray<FV> Run = Smooth({ Top, Top + FV(Lean * 0.2f, Len * 0.45f), Bulb + FV(0.0f, -R * 0.6f) });
		const auto Shape = [Run, Bulb, R, W](FV Q)
		{
			const float Strand = SdRibbon(Q, Run, W, W * 0.75f);
			// the bead: round below, drawn up to a point where the thread feeds it
			const FV L = Q - Bulb;
			const float Bead = SdCircle(Q, Bulb, R) + FMath::Max(0.0f, -L.Y) * 0.25f;
			return SMin(Strand, Bead, R * 0.9f);
		};
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.85f;
			P.Look.Ink = FMath::Min(P.Look.Ink, R * 0.12f);
			P.Fill(Shape, Color);
		}
		FClip Clip(P, Shape);
		// light pooled at the bottom of the bead, a long highlight down the thread and a bright gleam
		P.MEllipse(Bulb + FV(R * 0.1f, R * 0.5f), R * 0.65f, R * 0.3f, Alpha(Tone(Color, 1.7f), 0.5f), R * 0.2f);
		P.Mark([Run, W](FV Q) { TArray<FV> L; for (const FV& V : Run) L.Add(V - FV(W * 0.35f, 0.0f)); return SdPolyline(Q, L, W * 0.16f); }, Hex(0xFFFFFF, 0.55f));
		P.MEllipse(Bulb + FV(-R * 0.38f, -R * 0.2f), R * 0.16f, R * 0.3f, Hex(0xFFFFFF, 0.85f), R * 0.06f);
		P.MCircle(Bulb + FV(R * 0.35f, R * 0.1f), R * 0.08f, Hex(0xFFFFFF, 0.5f));
	}

	/** A natural seabed mound: a lumpy rock or sand pile the tower sits on, with pebbles and weed at its foot */
	void HDMound(FPainter& P, FV C, float Rx, float Ry, const FLinearColor& Color, int32 Seed, bool bStones = true)
	{
		const auto Mound = [C, Rx, Ry, Seed](FV Q)
		{
			const float D = SdEllipse(Q, C, Rx, Ry);
			return D + Noise2(Q * 14.0f + FV(Seed * 3.1f, 0.0f)) * 0.012f + Noise2(Q * 34.0f) * 0.004f;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.08f;
			P.Look.Spec = 0.15f;
			P.Look.Grain = 0.12f;
			P.Fill(Mound, Color);
		}
		FClip Clip(P, Mound);
		if (bStones)
		{
			const TArray<FV> Seeds = GridSeeds(C - FV(Rx, Ry), C + FV(Rx, Ry), 5, 2, Seed);
			Plates(P, Seeds, 0.003f, Alpha(Tone(Color, 0.45f), 0.5f), Hex(0xFFFFFF, 0.12f), 0, 0.0f);
		}
		Speckle(P, C - FV(Rx, Ry), C + FV(Rx, Ry), 90, 0.0015f, 0.004f, Alpha(Tone(Color, 0.5f), 0.6f), Seed + 1);
		Speckle(P, C - FV(Rx, Ry), C + FV(Rx, Ry), 50, 0.0015f, 0.0035f, Hex(0xFFFFFF, 0.35f), Seed + 2);
		// cool shade under the belly of the mound and a soft lit crown
		P.MEllipse(C + FV(0.0f, Ry * 0.8f), Rx * 1.1f, Ry * 0.6f, Hex(0x0A1820, 0.35f), Ry * 0.5f);
		P.MEllipse(C + FV(-Rx * 0.2f, -Ry * 0.6f), Rx * 0.6f, Ry * 0.3f, Hex(0xFFFFFF, 0.15f), Ry * 0.3f);
	}

	/** A swaying frond of weed from Root, bending by Bend */
	void HDWeed(FPainter& P, FV Root, float Height, float Bend, float W, const FLinearColor& Color)
	{
		const TArray<FV> Path = Smooth({ Root, Root + FV(Bend * 0.3f, -Height * 0.35f), Root + FV(-Bend * 0.2f, -Height * 0.7f), Root + FV(Bend, -Height) });
		{
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Ink = 0.006f;
			P.Fill([Path, W](FV Q) { return SdRibbon(Q, Path, W, W * 0.3f); }, Color);
		}
		P.MLine(Path, W * 0.15f, Alpha(Tone(Color, 1.5f), 0.6f));
	}

	/** A barnacle cone with its plated wall and a dark slit at the top */
	void HDBarnacle(FPainter& P, FV C, float R)
	{
		P.Fill([C, R](FV Q) { return SdEllipse(Q, C, R, R * 0.8f) + Noise2(Q * 120.0f) * R * 0.08f; }, Hex(0xD8D2C0));
		for (int32 k = 0; k < 6; ++k)
		{
			const FV D = Spin(FV(1.0f, 0.0f), k * 60.0f + 15.0f);
			P.MCapsule(C + D * R * 0.35f, C + FV(D.X, D.Y * 0.8f) * R * 0.95f, R * 0.06f, Hex(0x8A8270, 0.7f));
		}
		P.MEllipse(C, R * 0.4f, R * 0.25f, Hex(0x5A5446));
		P.MCapsule(C - FV(R * 0.25f, 0.0f), C + FV(R * 0.25f, 0.0f), R * 0.06f, Hex(0x1A1610));
	}

	// ------------------------------------------------------------------ towers

	void HDTowerWall(FPainter& P)
	{
		HDMound(P, FV(0.5f, 0.78f), 0.42f, 0.15f, Hex(0x7E7A72), 901);

		// brain coral: a lumpy dome covered in a meandering maze of ridges and valleys
		const FV Bc(0.42f, 0.47f);
		const auto Brain = [Bc](FV Q)
		{
			return SdEllipse(Q, Bc, 0.29f, 0.25f) + Noise2(Q * 7.0f) * 0.014f;
		};
		const FLinearColor Coral = Hex(0xE8707E), CoralDeep = Hex(0x9A2E4A), CoralPale = Hex(0xFFC2C6);
		{
			FWith With(P);
			P.Look.Dome = 0.3f;
			P.Look.Spec = 0.35f;
			P.Fill(Brain, Coral);
		}
		{
			FClip Clip(P, Brain);
			// the maze: contours of a warped field, so the grooves wander like a real brain coral
			const auto Maze = [Bc](FV Q)
			{
				// a narrow-band random field (plane waves of one wavelength in many directions):
				// its zero lines form an even-width labyrinth, like the meanders of a real brain coral
				float F = 0.0f;
				for (int32 k = 0; k < 9; ++k)
				{
					const float A = k * 0.349f + Rand01(905, k) * 0.3f;
					F += FMath::Cos((Q.X * FMath::Cos(A) + Q.Y * FMath::Sin(A)) * 88.0f + Rand01(906, k) * 6.28f);
				}
				return FMath::Min(1.0f, FMath::Abs(F) * 0.42f);
			};
			// dark valleys, then lit ridge crests just above them
			P.Mark([&Maze](FV Q) { return (Maze(Q) - 0.22f) * 0.012f; }, Alpha(CoralDeep, 0.9f), 0.002f);
			P.Mark([&Maze](FV Q) { return (Maze(Q - FV(0.0f, 0.005f)) - 0.12f) * 0.012f; }, Alpha(Hex(0x4A0A1E), 0.5f), 0.002f);
			P.Mark([&Maze](FV Q) { return (0.8f - Maze(Q + FV(0.0f, 0.004f))) * 0.012f; }, Alpha(CoralPale, 0.45f), 0.003f);
			// polyp dots along the ridges
			Speckle(P, Bc - FV(0.29f, 0.25f), Bc + FV(0.29f, 0.25f), 160, 0.0015f, 0.003f, Alpha(CoralPale, 0.7f), 902);
			// shade on the lower half and a soft sheen on top
			P.MEllipse(Bc + FV(0.04f, 0.2f), 0.32f, 0.12f, Hex(0x2A0A18, 0.35f), 0.08f);
			P.MEllipse(Bc + FV(-0.08f, -0.13f), 0.13f, 0.06f, Hex(0xFFFFFF, 0.25f), 0.05f);
		}

		// organ-pipe coral: a cluster of tubes that grow together at the base, each with an open lit mouth
		const FLinearColor Pipe = Hex(0xF09A3E), PipeDeep = Hex(0x9A4A16);
		struct FTube { FV Base; FV Top; float R; };
		const FTube Tubes[] = { { FV(0.72f, 0.72f), FV(0.7f, 0.33f), 0.03f }, { FV(0.77f, 0.72f), FV(0.79f, 0.24f), 0.032f }, { FV(0.83f, 0.74f), FV(0.86f, 0.38f), 0.028f }, { FV(0.67f, 0.74f), FV(0.64f, 0.5f), 0.025f } };
		const auto Cluster = [&Tubes](FV Q)
		{
			float D = 1e6f;
			for (const FTube& T : Tubes)
			{
				D = SMin(D, SdCapsule(Q, T.Base, T.Top, T.R + (Q.Y - T.Top.Y) * 0.03f), 0.02f);
			}
			return D;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.4f;
			P.Look.Spec = 0.45f;
			P.Fill(Cluster, Pipe);
		}
		{
			FClip Clip(P, Cluster);
			for (const FTube& T : Tubes)
			{
				// growth rings around each pipe
				for (int32 k = 1; k < 9; ++k)
				{
					const FV At = FMath::Lerp(T.Top, T.Base, k / 9.0f);
					P.MArc(At - FV(0.0f, T.R * 0.4f), T.R * 1.1f, 0.0025f, 20.0f, 160.0f, Alpha(PipeDeep, 0.5f));
				}
				P.MCapsule(T.Top + FV(-T.R * 0.45f, 0.01f), T.Base + FV(-T.R * 0.45f, -0.04f), T.R * 0.18f, Hex(0xFFE0B0, 0.45f));
			}
			P.MEllipse(FV(0.76f, 0.72f), 0.12f, 0.05f, Hex(0x3A1406, 0.4f), 0.04f);
		}
		for (const FTube& T : Tubes)
		{
			P.Ellipse(T.Top, T.R * 1.05f, T.R * 0.45f, Hex(0xFFC27A));
			P.MEllipse(T.Top + FV(0.0f, T.R * 0.06f), T.R * 0.72f, T.R * 0.28f, Hex(0x3A1406));
			// a tiny feathery polyp peeking out of one pipe
		}
		for (int32 k = 0; k < 5; ++k)
		{
			const FV Root = Tubes[1].Top + FV(0.0f, -0.004f);
			const FV Dir = Spin(FV(0.0f, -1.0f), -40.0f + k * 20.0f);
			P.MLine(Smooth({ Root, Root + Dir * 0.025f + FV(0.0f, -0.005f), Root + Dir * 0.045f }), 0.0025f, Hex(0xFFF2D8, 0.9f));
			P.MCircle(Root + Dir * 0.045f, 0.004f, Hex(0xFFF8E8));
		}

		// weed and small life at the foot
		HDWeed(P, FV(0.14f, 0.8f), 0.2f, -0.04f, 0.018f, Hex(0x3E9A44));
		HDWeed(P, FV(0.18f, 0.82f), 0.14f, 0.05f, 0.014f, Hex(0x5AB84E));
		HDWeed(P, FV(0.9f, 0.8f), 0.12f, 0.03f, 0.012f, Hex(0x3E9A44));
		HDBarnacle(P, FV(0.32f, 0.82f), 0.022f);
		HDBarnacle(P, FV(0.58f, 0.86f), 0.018f);
		HDBarnacle(P, FV(0.62f, 0.8f), 0.014f);
	}

	void HDTowerHarpoon(FPainter& P)
	{
		Plinth(P, 0.47f, 0.86f, 0.36f);

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

	/** The jellyfish alone: bell centred at (0.5, 0.3), tentacles reaching down to about 0.83 */
	void HDJellyBody(FPainter& P)
	{
		const FLinearColor Pink = Hex(0xF08CD8), Deep = Hex(0xB84AA8), Pale = Hex(0xFFD8F4);
		const FV Bell(0.5f, 0.3f);
		// trailing tentacles, thin and wavy, drifting behind the frilled oral arms
		for (int32 i = 0; i < 9; ++i)
		{
			const float X = 0.29f + i * 0.0525f;
			const float Ph = i * 1.3f;
			TArray<FV> T;
			for (int32 k = 0; k <= 8; ++k)
			{
				const float Y = 0.39f + k * 0.055f;
				T.Add(FV(X + FMath::Sin(Ph + k * 0.9f) * 0.018f * (k / 8.0f + 0.3f), Y));
			}
			const TArray<FV> S = Smooth(T);
			P.Glow([S](FV Q) { return SdPolyline(Q, S, 0.002f); }, Pink, 0.02f, 0.4f);
			P.Mark([S](FV Q) { return SdRibbon(Q, S, 0.005f, 0.002f); }, Alpha(Pale, 0.85f));
		}
		// four frilly oral arms: ruffled ribbons
		for (int32 i = 0; i < 4; ++i)
		{
			const float X = 0.4f + i * 0.067f;
			const float F = (i < 2) ? -1.0f : 1.0f;
			const TArray<FV> Arm = Smooth({ FV(X, 0.38f), FV(X + F * 0.02f, 0.5f), FV(X - F * 0.015f, 0.62f), FV(X + F * 0.01f, 0.74f) });
			const auto Frill = [Arm](FV Q)
			{
				const float D = SdRibbon(Q, Arm, 0.028f, 0.01f);
				return D - FMath::Sin(Q.Y * 160.0f + Q.X * 40.0f) * 0.005f;
			};
			{
				FWith With(P);
				P.Look.Dome = 0.02f;
				P.Look.Ink = 0.005f;
				P.Fill(Frill, Alpha(Pink, 0.75f));
			}
			FClip Clip(P, Frill);
			P.Mark([Arm](FV Q) { return SdPolyline(Q, Arm, 0.004f); }, Alpha(Deep, 0.6f));
			P.Mark([](FV Q) { return (FMath::Abs(FMath::Sin(Q.Y * 160.0f + Q.X * 40.0f)) - 0.85f) * 0.01f; }, Alpha(Pale, 0.7f));
		}
		// the bell: a translucent dome with a scalloped rim
		const auto Dome = [Bell](FV Q)
		{
			const FV L = Q - Bell;
			float D = SdEllipse(Q, Bell, 0.27f, 0.22f);
			D = FMath::Max(D, L.Y - 0.08f - FMath::Abs(FMath::Sin(L.X * 68.0f)) * 0.012f);
			return D;
		};
		P.Glow(Dome, Pink, 0.07f, 0.6f);
		{
			FWith With(P);
			P.Look.Dome = 0.25f;
			P.Look.Spec = 0.8f;
			P.Fill(Dome, Alpha(Pink, 0.88f));
		}
		{
			FClip Clip(P, Dome);
			// radial canals running down from the crown, and the four glowing horseshoe gonads seen through the bell
			for (int32 k = 0; k < 16; ++k)
			{
				const float A = -170.0f + k * (160.0f / 15.0f);
				const FV D = Spin(FV(1.0f, 0.0f), A);
				P.MLine(Smooth({ Bell + FV(D.X * 0.03f, -0.02f), Bell + FV(D.X * 0.14f, D.Y * 0.1f), Bell + FV(D.X * 0.24f, 0.08f) }), 0.0025f, Alpha(Deep, 0.4f));
			}
			for (int32 k = 0; k < 4; ++k)
			{
				const FV G = Bell + FV(-0.11f + k * 0.073f, -0.01f + (k == 1 || k == 2 ? -0.03f : 0.0f));
				P.Mark([G](FV Q) { return SdEllipse(Q, G, 0.03f, 0.022f) + Noise2(Q * 80.0f) * 0.004f; }, Hex(0xFF6AD0, 0.35f), 0.012f);
				P.Mark([G](FV Q) { return SdEllipse(Q, G + FV(0.0f, -0.004f), 0.014f, 0.008f); }, Hex(0xFFF0FF, 0.4f), 0.006f);
			}
			// a thick inner jelly layer and the rim's muscle band
			P.Mark([Bell](FV Q) { return FMath::Abs(SdEllipse(Q, Bell, 0.2f, 0.17f)) - 0.004f; }, Alpha(Pale, 0.35f));
			P.MEllipse(Bell + FV(0.0f, 0.085f), 0.24f, 0.02f, Alpha(Deep, 0.45f), 0.01f);
			Speckle(P, Bell - FV(0.22f, 0.2f), Bell + FV(0.22f, 0.08f), 60, 0.0015f, 0.004f, Hex(0xFFFFFF, 0.55f), 921);
			// window highlight on the crown
			P.MEllipse(Bell + FV(-0.08f, -0.12f), 0.08f, 0.035f, Hex(0xFFFFFF, 0.55f), 0.025f);
			P.MEllipse(Bell + FV(0.11f, -0.06f), 0.025f, 0.015f, Hex(0xFFFFFF, 0.3f), 0.01f);
		}
		// marginal tentacle stubs and sense organs along the rim
		for (int32 k = 0; k < 12; ++k)
		{
			const float X = Bell.X - 0.21f + k * 0.0382f;
			P.MCircle(FV(X, Bell.Y + 0.085f + FMath::Abs(FMath::Sin((X - Bell.X) * 68.0f)) * 0.01f), 0.0045f, Hex(0xFFF0FF, 0.9f));
		}
	}

	void HDTowerJelly(FPainter& P)
	{
		Plinth(P, 0.5f, 0.87f, 0.3f);
		P.Glow([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.87f), 0.2f, 0.05f); }, Hex(0xFF9CE0), 0.06f, 0.5f);
		HDJellyBody(P);
		Bubble(P, FV(0.14f, 0.26f), 0.03f);
		Bubble(P, FV(0.18f, 0.18f), 0.016f);
		Bubble(P, FV(0.86f, 0.42f), 0.022f);
	}

	/** The puffer alone, centred at (0.5, 0.38) with radius about 0.27 including its spines */
	void HDPufferBody(FPainter& P, float SpineScale = 1.0f)
	{
		// the puffer, fully inflated: a round body bristling with spines that rise out of the skin
		const FV C(0.5f, 0.38f);
		const float R = 0.215f;
		const FLinearColor Skin = Hex(0xF4C83A), Deep = Hex(0xB8861A), Belly = Hex(0xFFF0C0), SpineC = Hex(0xE8D8A8);
		// fins first: the tail and two side fins
		HDFin(P, C + FV(0.0f, -R * 0.9f), { C + FV(-0.05f, -R - 0.06f), C + FV(0.0f, -R - 0.08f), C + FV(0.05f, -R - 0.06f) }, Hex(0xE8A830), Hex(0x8A5A10, 0.7f));
		for (const float F : { -1.0f, 1.0f })
		{
			HDFin(P, C + FV(F * R * 0.85f, 0.02f), { C + FV(F * (R + 0.06f), -0.05f), C + FV(F * (R + 0.09f), 0.0f), C + FV(F * (R + 0.08f), 0.05f), C + FV(F * (R + 0.04f), 0.07f) }, Hex(0xF0B848), Hex(0x8A5A10, 0.7f));
		}
		const int32 Spines = 26;
		const auto Body = [C, R, Spines, SpineScale](FV Q)
		{
			float D = SdCircle(Q, C, R);
			// each spine is a cone that melts into the skin
			for (int32 k = 0; k < Spines; ++k)
			{
				const FV Dir = Spin(FV(1.0f, 0.0f), k * 360.0f / Spines + 5.0f);
				const float L = (0.045f + 0.01f * FMath::Sin(k * 2.3f)) * SpineScale;
				D = SMin(D, SdRibbon(Q, { C + Dir * (R - 0.02f), C + Dir * (R + L) }, 0.012f, 0.0015f), 0.012f);
			}
			return D;
		};
		{
			FWith With(P);
			P.Look.Dome = 10.0f;
			P.Look.Spec = 0.55f;
			P.Fill(Body, Skin);
		}
		{
			FClip Clip(P, Body);
			// pale belly, dark saddle spots, skin pores, and pale spine tips
			P.MEllipse(C + FV(0.0f, R * 0.55f), R * 0.85f, R * 0.5f, Alpha(Belly, 0.85f), 0.05f);
			for (int32 k = 0; k < 9; ++k)
			{
				const FV At = C + FV((Rand01(931, k) - 0.5f) * R * 1.4f, -R * 0.15f - Rand01(932, k) * R * 0.65f);
				P.Mark([At](FV Q) { return SdCircle(Q, At, 0.014f) + Noise2(Q * 160.0f) * 0.004f; }, Alpha(Hex(0x6A4A10), 0.55f));
			}
			for (int32 k = 0; k < Spines; ++k)
			{
				const FV Dir = Spin(FV(1.0f, 0.0f), k * 360.0f / Spines + 5.0f);
				const float L = (0.045f + 0.01f * FMath::Sin(k * 2.3f)) * SpineScale;
				P.MLine({ C + Dir * (R + L * 0.2f), C + Dir * (R + L) }, 0.003f, Alpha(SpineC, 0.9f));
			}
			Speckle(P, C - FV(R, R), C + FV(R, R), 90, 0.0015f, 0.003f, Alpha(Deep, 0.5f), 933);
		}
		// big round eyes set high, a pouting mouth
		for (const float F : { -1.0f, 1.0f })
		{
			const FV E = C + FV(F * 0.085f, -0.04f);
			P.Fill([E](FV Q) { return SdCircle(Q, E, 0.058f); }, Tone(Skin, 0.9f));
			HDEye(P, E, 0.048f, Hex(0x2A7AB8), FV(-F * 0.15f, 0.1f), 0.6f);
		}
		P.Fill([C](FV Q) { return SdEllipse(Q, C + FV(0.0f, 0.075f), 0.035f, 0.026f); }, Hex(0xE88A4A));
		P.MEllipse(C + FV(0.0f, 0.078f), 0.018f, 0.011f, Hex(0x5A1A0A));
		P.MEllipse(C + FV(-0.01f, 0.065f), 0.012f, 0.005f, Hex(0xFFFFFF, 0.5f), 0.003f);

	}

	void HDTowerPuffer(FPainter& P)
	{
		Plinth(P, 0.5f, 0.87f, 0.34f);
		HDPufferBody(P);
		// the riveted iron tub it sits in: hammered plates, banding, rust and algae
		const TArray<FV> Tub = Smooth({ FV(0.2f, 0.56f), FV(0.5f, 0.575f), FV(0.8f, 0.56f), FV(0.76f, 0.75f), FV(0.72f, 0.9f), FV(0.5f, 0.91f), FV(0.28f, 0.9f), FV(0.24f, 0.75f) }, true, 4);
		{
			FWith With(P);
			P.Look.Dome = 0.14f;
			P.Look.Spec = 0.6f;
			P.Fill([&Tub](FV Q) { return SdPoly(Q, Tub) - 0.01f; }, Iron);
		}
		{
			FClip Clip(P, [&Tub](FV Q) { return SdPoly(Q, Tub) - 0.01f; });
			// hammer dimples
			Strands(P, FV(0.5f, 0.56f), FV(0.5f, 0.92f), 0.3f, 22, 0.001f, Hex(0xC8D4DA, 0.14f), 934, 0.004f);
			Pits(P, FV(0.26f, 0.62f), FV(0.74f, 0.88f), 10, 0.008f, Hex(0x2A343A, 0.5f), 935);
			P.MCapsule(FV(0.5f, 0.6f), FV(0.5f, 0.9f), 0.003f, Hex(0x1E282E, 0.8f));
			P.MCapsule(FV(0.505f, 0.6f), FV(0.505f, 0.9f), 0.0015f, Hex(0xC8D4DA, 0.5f));
			// a thick rolled rim and a middle band
			P.Fill([](FV Q) { return SdCapsule(Q, FV(0.2f, 0.585f), FV(0.8f, 0.585f), 0.024f); }, Hex(0x6E7C86));
			P.MCapsule(FV(0.2f, 0.575f), FV(0.8f, 0.575f), 0.005f, Hex(0xD8E4EA, 0.6f));
			P.MCapsule(FV(0.2f, 0.73f), FV(0.8f, 0.73f), 0.02f, IronDark);
			P.MCapsule(FV(0.2f, 0.72f), FV(0.8f, 0.72f), 0.004f, Hex(0xA8B6BE, 0.6f));
			for (int32 Row = 0; Row < 2; ++Row)
			{
				for (int32 i = 0; i < 7; ++i)
				{
					Rivet(P, FV(0.26f + i * 0.08f, 0.63f + Row * 0.2f), 0.011f);
				}
			}
			for (const FV At : { FV(0.3f, 0.82f), FV(0.66f, 0.67f), FV(0.6f, 0.86f) })
			{
				P.Mark([At](FV Q) { return SdEllipse(Q, At, 0.04f, 0.022f) + Noise2(Q * 90.0f) * 0.01f; }, Hex(0x8A4A22, 0.6f), 0.004f);
				P.MCapsule(At + FV(0.0f, 0.01f), At + FV(0.004f, 0.06f), 0.004f, Hex(0x8A4A22, 0.45f));
			}
			P.Mark([](FV Q) { return 0.865f - Q.Y + Noise2(Q * 40.0f) * 0.02f; }, Hex(0x3E7A3A, 0.75f), 0.01f);
			P.MCapsule(FV(0.24f, 0.62f), FV(0.27f, 0.88f), 0.012f, Hex(0xFFFFFF, 0.14f));
		}
	}

	/** A moray's head and body: Path runs from the tail (inside the rock) to the head */
	void HDMorayBody(FPainter& P, const TArray<FV>& Path, float R0, float R1, const FLinearColor& Skin, const FLinearColor& Spot, int32 Seed)
	{
		const auto Body = [&Path, R0, R1](FV Q) { return SdRibbon(Q, Path, R0, R1); };
		{
			FWith With(P);
			P.Look.Dome = 0.3f;
			P.Look.Spec = 0.7f;
			P.Fill(Body, Skin);
		}
		FClip Clip(P, Body);
		// leopard rosettes over the skin, a dorsal fin ridge, a pale belly edge
		for (int32 k = 0; k < 60; ++k)
		{
			FV Pos, Tan;
			Along(Path, Rand01(Seed, k), Pos, Tan);
			const FV N(-Tan.Y, Tan.X);
			const float R = FMath::Lerp(R0, R1, Rand01(Seed, k));
			const FV At = Pos + N * R * (Rand01(Seed, k + 100) * 1.6f - 0.8f);
			const float S = 0.006f + Rand01(Seed, k + 200) * 0.006f;
			P.Mark([At, S](FV Q) { return FMath::Abs(SdCircle(Q, At, S)) - S * 0.35f + Noise2(Q * 200.0f) * 0.002f; }, Spot);
		}
		TArray<FV> Belly, Ridge;
		for (int32 k = 0; k <= 30; ++k)
		{
			FV Pos, Tan;
			Along(Path, k / 30.0f, Pos, Tan);
			const FV N(-Tan.Y, Tan.X);
			const float R = FMath::Lerp(R0, R1, k / 30.0f);
			Belly.Add(Pos + N * R * 0.75f);
			Ridge.Add(Pos - N * R * 0.55f);
		}
		P.MLine(Belly, 0.008f, Hex(0xF4F0C0, 0.4f));
		P.MLine(Ridge, 0.005f, Hex(0xFFFFFF, 0.3f));
	}

	void HDTowerEel(FPainter& P)
	{
		// a hollow reef rock with the moray coiled out of its mouth
		const auto Rock = [](FV Q)
		{
			return SdEllipse(Q, FV(0.38f, 0.7f), 0.3f, 0.2f) + Noise2(Q * 9.0f) * 0.025f + Noise2(Q * 30.0f) * 0.006f;
		};
		const FLinearColor RockC = Hex(0x8A8E8C);
		{
			FWith With(P);
			P.Look.Dome = 0.15f;
			P.Look.Grain = 0.15f;
			P.Fill(Rock, RockC);
		}
		{
			FClip Clip(P, Rock);
			Plates(P, GridSeeds(FV(0.08f, 0.5f), FV(0.68f, 0.9f), 4, 2, 941), 0.0025f, Hex(0x3A403E, 0.45f), Hex(0xFFFFFF, 0.1f), 0, 0.0f);
			Pits(P, FV(0.12f, 0.6f), FV(0.62f, 0.86f), 14, 0.008f, Hex(0x1E2422, 0.8f), 942);
			// encrusting coralline algae and sponge patches
			for (int32 k = 0; k < 6; ++k)
			{
				const FV At(0.12f + Rand01(943, k) * 0.5f, 0.56f + Rand01(944, k) * 0.28f);
				P.Mark([At](FV Q) { return SdCircle(Q, At, 0.03f) + Noise2(Q * 70.0f) * 0.014f; }, k % 2 ? Hex(0xB86A8A, 0.35f) : Hex(0x6A8A4A, 0.35f), 0.008f);
			}
			P.MEllipse(FV(0.38f, 0.86f), 0.32f, 0.08f, Hex(0x0A1416, 0.4f), 0.06f);
		}
		// the dark hole the eel lives in
		P.Mark([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.66f), 0.085f, 0.06f) + Noise2(Q * 40.0f) * 0.008f; }, Hex(0x0A0E0E));
		HDWeed(P, FV(0.14f, 0.78f), 0.16f, -0.04f, 0.014f, Hex(0x3E9A44));
		HDBarnacle(P, FV(0.24f, 0.62f), 0.02f);

		// the eel's body curls up out of the hole
		const TArray<FV> Path = Smooth({ FV(0.5f, 0.68f), FV(0.62f, 0.62f), FV(0.72f, 0.5f), FV(0.66f, 0.38f), FV(0.52f, 0.34f), FV(0.46f, 0.24f), FV(0.54f, 0.15f), FV(0.66f, 0.13f) });
		const FLinearColor Skin = Hex(0x8A9A3A), Spot = Hex(0x2E3412, 0.85f);
		HDMorayBody(P, Path, 0.075f, 0.06f, Skin, Spot, 945);

		// the head: blunt snout, gaping jaws full of needle teeth, a beady eye and a gill pore
		FV HeadPos, HeadTan;
		Along(Path, 1.0f, HeadPos, HeadTan);
		{
			FNest Place(P, HeadPos, 1.45f, FMath::RadiansToDegrees(FMath::Atan2(HeadTan.Y, HeadTan.X)) - 5.0f);
			const auto Upper = [](FV Q) { return SMin(SdEllipse(Q, FV(0.02f, -0.012f), 0.11f, 0.045f), SdCircle(Q, FV(-0.04f, 0.0f), 0.05f), 0.03f); };
			const auto Lower = [](FV Q) { return SdEllipse(Q, FV(0.02f, 0.035f), 0.095f, 0.025f); };
			P.Fill([](FV Q) { return SdEllipse(Q, FV(0.03f, 0.012f), 0.085f, 0.028f); }, Hex(0x5A0E14));
			P.Fill(Lower, Skin);
			{
				FClip Clip(P, Lower);
				P.MEllipse(FV(0.02f, 0.045f), 0.09f, 0.015f, Hex(0xF4F0C0, 0.35f), 0.01f);
				Speckle(P, FV(-0.07f, 0.01f), FV(0.11f, 0.06f), 14, 0.003f, 0.006f, Spot, 946);
			}
			{
				FWith With(P);
				P.Look.Dome = 0.2f;
				P.Look.Spec = 0.7f;
				P.Fill(Upper, Skin);
			}
			{
				FClip Clip(P, Upper);
				Speckle(P, FV(-0.09f, -0.06f), FV(0.13f, 0.03f), 26, 0.003f, 0.008f, Spot, 947);
				P.MEllipse(FV(-0.01f, -0.04f), 0.08f, 0.015f, Hex(0xFFFFFF, 0.3f), 0.01f);
			}
			// needle teeth along both jaws
			for (int32 k = 0; k < 6; ++k)
			{
				const float X = -0.03f + k * 0.022f;
				P.MPoly({ FV(X - 0.005f, 0.018f), FV(X + 0.005f, 0.018f), FV(X + 0.002f, 0.034f) }, Hex(0xF8F4E8));
				P.MPoly({ FV(X + 0.006f, 0.04f), FV(X + 0.015f, 0.04f), FV(X + 0.01f, 0.027f) }, Hex(0xF8F4E8));
			}
			HDEye(P, FV(0.03f, -0.025f), 0.016f, Hex(0xE8C030), FV(0.3f, 0.0f), 0.65f);
			P.MCircle(FV(0.1f, -0.02f), 0.004f, Hex(0x1A1E08));
			P.MEllipse(FV(-0.06f, 0.01f), 0.01f, 0.006f, Hex(0x1A1E08, 0.9f));
		}
		BubbleTrail(P, FV(0.82f, 0.12f), 3, 0.014f, 948);
	}

	void HDTowerMantis(FPainter& P)
	{
		Plinth(P, 0.5f, 0.87f, 0.3f);
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
		// a bright crack of impact in front of the club
		{
			const FV S = Head + FV(0.21f, 0.0f);
			P.Glow([S](FV Q) { return SdCircle(Q, S, 0.006f); }, Hex(0xFFF4C0), 0.045f, 0.9f);
			for (int32 k = 0; k < 4; ++k)
			{
				const FV D = Spin(FV(0.026f - (k % 2) * 0.01f, 0.0f), k * 45.0f);
				P.MCapsule(S - D, S + D, 0.0022f, Hex(0xFFFFFF));
			}
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

	void HDTowerOctopus(FPainter& P)
	{
		Plinth(P, 0.5f, 0.88f, 0.3f);
		const FLinearColor Skin = Hex(0xEE6A44), Deep = Hex(0xA8341E), Pale = Hex(0xFFC0A0), Sucker = Hex(0xFFE0CC);
		// arms spread over the plinth; the two upper ones raise sling stones
		TArray<TArray<FV>> Arms;
		Arms.Add(Smooth({ FV(0.42f, 0.5f), FV(0.3f, 0.44f), FV(0.2f, 0.32f), FV(0.17f, 0.2f) }, false, 6));
		Arms.Add(Smooth({ FV(0.42f, 0.56f), FV(0.28f, 0.6f), FV(0.16f, 0.66f), FV(0.1f, 0.76f), FV(0.15f, 0.8f) }, false, 6));
		Arms.Add(Smooth({ FV(0.44f, 0.6f), FV(0.36f, 0.72f), FV(0.3f, 0.84f), FV(0.34f, 0.88f) }, false, 6));
		Arms.Add(Smooth({ FV(0.48f, 0.62f), FV(0.46f, 0.76f), FV(0.42f, 0.88f), FV(0.46f, 0.9f) }, false, 6));
		for (int32 i = 0; i < 4; ++i)
		{
			TArray<FV> M;
			for (const FV& Pt : Arms[i])
			{
				M.Add(FV(1.0f - Pt.X, Pt.Y));
			}
			Arms.Add(M);
		}
		const float R0 = 0.05f, R1 = 0.012f;
		const auto Mantle = [](FV Q) { return SMin(SdEllipse(Q, FV(0.5f, 0.33f), 0.15f, 0.17f), SdEllipse(Q, FV(0.5f, 0.52f), 0.12f, 0.09f), 0.06f); };
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
			P.MEllipse(FV(0.5f, 0.82f), 0.4f, 0.14f, Alpha(Deep, 0.3f), 0.1f);
			for (int32 a = 0; a < Arms.Num(); ++a)
			{
				const float Side = a < 4 ? 1.0f : -1.0f;
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
			Speckle(P, FV(0.34f, 0.15f), FV(0.66f, 0.6f), 90, 0.003f, 0.008f, Alpha(Deep, 0.4f), 951);
			Granules(P, Mantle, FV(0.36f, 0.17f), FV(0.64f, 0.5f), 30, 0.005f, 0.009f, Alpha(Deep, 0.55f), Pale, 952);
			P.MEllipse(FV(0.45f, 0.24f), 0.06f, 0.035f, Hex(0xFFFFFF, 0.3f), 0.03f);
		}
		// friendly eyes with horizontal pupils, and a little siphon
		for (const float F : { -1.0f, 1.0f })
		{
			const FV E(0.5f + F * 0.065f, 0.43f);
			P.Fill([E](FV Q) { return SdEllipse(Q, E, 0.045f, 0.04f); }, Tone(Skin, 0.88f));
			FClip Clip(P, [E](FV Q) { return SdEllipse(Q, E, 0.036f, 0.032f); });
			P.Ball(E, 0.036f, Hex(0xFFD848), 0.4f);
			P.MCapsule(E - FV(0.018f, 0.0f), E + FV(0.018f, 0.0f), 0.008f, Hex(0x0A0806));
			P.Fill([E](FV Q) { return (Q.Y - E.Y) + 0.018f; }, Tone(Skin, 0.8f));
			P.MCircle(E + FV(-0.01f, 0.01f), 0.005f, Hex(0xFFFFFF, 0.9f));
		}
		P.Fill([](FV Q) { return SdCapsule(Q, FV(0.5f, 0.5f), FV(0.5f, 0.53f), 0.012f); }, Tone(Skin, 0.85f));
		// the raised arms each grip a smooth grey stone
		for (const float F : { -1.0f, 1.0f })
		{
			const FV S(0.5f + F * 0.34f, 0.15f);
			P.Fill([S](FV Q) { return SdEllipse(Q, S, 0.05f, 0.042f) + Noise2(Q * 30.0f) * 0.005f; }, Stone);
			{
				FClip Clip(P, [S](FV Q) { return SdEllipse(Q, S, 0.05f, 0.042f); });
				Speckle(P, S - FV(0.05f, 0.04f), S + FV(0.05f, 0.04f), 16, 0.002f, 0.004f, Hex(0x4A5258, 0.6f), (int32)(953 + F));
				P.MEllipse(S + FV(-0.015f, -0.015f), 0.02f, 0.012f, Hex(0xFFFFFF, 0.35f), 0.008f);
			}
			// the arm tip curled round the stone
			P.Fill([S, F](FV Q) { return SdRibbon(Q, Smooth({ S + FV(F * -0.02f, 0.06f), S + FV(F * 0.035f, 0.03f), S + FV(F * 0.03f, -0.02f), S + FV(F * -0.005f, -0.035f) }), 0.014f, 0.006f); }, Skin);
		}
		// a little clam at its feet
		P.Fill([](FV Q) { return SdEllipse(Q, FV(0.74f, 0.84f), 0.05f, 0.03f); }, Hex(0xE8D8C0));
		FClip ClamClip(P, [](FV Q) { return SdEllipse(Q, FV(0.74f, 0.84f), 0.05f, 0.03f); });
		P.Mark([](FV Q) { const FV L = Q - FV(0.74f, 0.88f); return (FMath::Abs(FMath::Sin(FMath::Atan2(L.Y, L.X) * 9.0f)) - 0.2f) * 0.004f; }, Hex(0x9A8A70, 0.6f));
	}

	void HDTowerLionfish(FPainter& P)
	{
		Plinth(P, 0.5f, 0.88f, 0.28f);
		P.Fill([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.86f), 0.12f, 0.03f); }, Hex(0xA05AC8));
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

	void HDTowerSawfish(FPainter& P)
	{
		// a sandy mound with seagrass
		HDMound(P, FV(0.42f, 0.66f), 0.32f, 0.09f, Hex(0xE8D8A0), 961, false);
		for (int32 k = 0; k < 5; ++k)
		{
			HDWeed(P, FV(0.16f + k * 0.03f, 0.66f), 0.12f + k * 0.015f, (k % 2 ? 0.03f : -0.02f), 0.008f, Hex(0x4AAA54));
		}
		const FLinearColor Skin = Hex(0x8A8A62), Belly = Hex(0xF0E8D0), Dark = Hex(0x4E4E36);
		// tail with a big upper lobe, dorsal and pectoral fins: shark-like
		HDFin(P, FV(0.14f, 0.44f), { FV(0.06f, 0.3f), FV(0.04f, 0.34f), FV(0.09f, 0.46f), FV(0.07f, 0.54f), FV(0.11f, 0.52f) }, Skin, Alpha(Dark, 0.6f));
		HDFin(P, FV(0.4f, 0.37f), { FV(0.34f, 0.37f), FV(0.38f, 0.26f), FV(0.42f, 0.25f), FV(0.48f, 0.37f) }, Skin, Alpha(Dark, 0.6f));
		HDFin(P, FV(0.24f, 0.4f), { FV(0.2f, 0.4f), FV(0.21f, 0.34f), FV(0.24f, 0.33f), FV(0.28f, 0.4f) }, Skin, Alpha(Dark, 0.6f));
		const TArray<FV> Outline = Smooth({ FV(0.12f, 0.44f), FV(0.24f, 0.4f), FV(0.42f, 0.36f), FV(0.56f, 0.37f), FV(0.66f, 0.41f), FV(0.68f, 0.45f), FV(0.62f, 0.49f), FV(0.46f, 0.52f), FV(0.28f, 0.5f), FV(0.16f, 0.47f) }, true, 5);
		const auto Body = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		// the rostrum: a long flat blade with paired teeth along both edges
		const FV RootS(0.64f, 0.44f), TipS(0.93f, 0.36f);
		const FV Dir = (TipS - RootS).GetSafeNormal(), N(-Dir.Y, Dir.X);
		const auto Saw = [RootS, TipS](FV Q) { return SdRibbon(Q, { RootS, TipS }, 0.028f, 0.016f); };
		for (int32 k = 0; k < 9; ++k)
		{
			const FV At = FMath::Lerp(RootS, TipS, 0.12f + k * 0.1f);
			const float W = FMath::Lerp(0.028f, 0.016f, 0.12f + k * 0.1f);
			for (const float S : { -1.0f, 1.0f })
			{
				const FV Base = At + N * S * W * 0.8f;
				P.Fill([Base, N, Dir, S](FV Q) { return SdRibbon(Q, { Base, Base + N * S * 0.022f - Dir * 0.004f }, 0.006f, 0.0015f); }, Hex(0xF4F0E0));
			}
		}
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Fill(Saw, Tone(Skin, 1.05f));
		}
		{
			FClip Clip(P, Saw);
			P.MLine({ RootS, TipS }, 0.003f, Alpha(Dark, 0.6f));
			Pits(P, FMath::Lerp(RootS, TipS, 0.1f) - FV(0.0f, 0.02f), TipS + FV(0.0f, 0.02f), 14, 0.003f, Alpha(Dark, 0.7f), 962);
			P.MLine({ RootS - N * 0.015f, TipS - N * 0.008f }, 0.004f, Hex(0xFFFFFF, 0.3f));
		}
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.5f;
			P.Fill(Body, Skin);
		}
		{
			FClip Clip(P, Body);
			P.MEllipse(FV(0.42f, 0.53f), 0.3f, 0.045f, Alpha(Belly, 0.9f), 0.025f);
			Speckle(P, FV(0.14f, 0.36f), FV(0.68f, 0.48f), 80, 0.003f, 0.008f, Alpha(Dark, 0.4f), 963);
			Speckle(P, FV(0.14f, 0.36f), FV(0.68f, 0.5f), 120, 0.001f, 0.002f, Hex(0xFFFFFF, 0.4f), 964);
			// gill slits and a lit back
			for (int32 k = 0; k < 4; ++k)
			{
				P.MLine(Smooth({ FV(0.54f - k * 0.016f, 0.43f), FV(0.535f - k * 0.016f, 0.46f), FV(0.54f - k * 0.016f, 0.49f) }), 0.0025f, Alpha(Dark, 0.7f));
			}
			P.MEllipse(FV(0.42f, 0.39f), 0.2f, 0.02f, Hex(0xFFFFFF, 0.35f), 0.015f);
		}
		HDFin(P, FV(0.46f, 0.5f), { FV(0.42f, 0.5f), FV(0.38f, 0.6f), FV(0.42f, 0.62f), FV(0.52f, 0.52f) }, Tone(Skin, 0.9f), Alpha(Dark, 0.6f));
		HDEye(P, FV(0.61f, 0.415f), 0.017f, Hex(0x5A6A2A), FV(0.2f, 0.0f), 0.6f);
		// a drop of blood on the last tooth: it bites
		HDDrip(P, TipS + FV(0.0f, -0.004f), 0.03f, 0.012f, Blood, 0.0f, 0.004f);
	}

	void HDTowerHumpback(FPainter& P)
	{
		Plinth(P, 0.5f, 0.86f, 0.28f);
		P.Fill([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.84f), 0.12f, 0.03f) + Noise2(Q * 60.0f) * 0.004f; }, Hex(0xE88A80));
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
		// song rings out ahead of the whale
		for (int32 k = 0; k < 3; ++k)
		{
			P.Arc(FV(0.1f, 0.52f), 0.05f + k * 0.035f, 0.009f, 200.0f, 250.0f, Hex(0xBFE8FF, 0.9f - k * 0.2f));
		}
		BubbleTrail(P, FV(0.3f, 0.44f), 3, 0.012f, 973);
	}

	void HDTowerAcidVent(FPainter& P)
	{
		Plinth(P, 0.5f, 0.88f, 0.32f);
		// a cluster of mineral chimneys, built up in rough, layered crusts, venting glowing acid
		struct FChimney { FV Base; FV Top; float R0; float R1; };
		const FChimney Chimneys[] = { { FV(0.52f, 0.88f), FV(0.5f, 0.3f), 0.12f, 0.05f }, { FV(0.34f, 0.88f), FV(0.32f, 0.56f), 0.07f, 0.035f }, { FV(0.7f, 0.88f), FV(0.72f, 0.5f), 0.08f, 0.04f } };
		const auto Rock = [&Chimneys](FV Q)
		{
			float D = 1e6f;
			for (const FChimney& C : Chimneys)
			{
				D = SMin(D, SdRibbon(Q, { C.Base, C.Top }, C.R0, C.R1), 0.04f);
			}
			// knobbly mineral lumps
			return D + Noise2(Q * 22.0f) * 0.012f + Noise2(Q * 60.0f) * 0.004f;
		};
		const FLinearColor RockC = Hex(0x4E4A48), Glow = Hex(0xFFA030), Acid = Hex(0xB8F040);
		{
			FWith With(P);
			P.Look.Dome = 0.18f;
			P.Look.Grain = 0.18f;
			P.Look.Spec = 0.25f;
			P.Fill(Rock, RockC);
		}
		{
			FClip Clip(P, Rock);
			// layered crust bands
			P.Mark([](FV Q) { return (FMath::Abs(FMath::Sin(Q.Y * 70.0f + Noise2(Q * 12.0f) * 2.0f)) - 0.85f) * 0.008f; }, Hex(0x2A2624, 0.7f));
			P.Mark([](FV Q) { return (FMath::Abs(FMath::Sin(Q.Y * 70.0f + 0.4f + Noise2(Q * 12.0f) * 2.0f)) - 0.9f) * 0.008f; }, Hex(0xA89A88, 0.4f));
			// cracks glowing with heat from inside
			const TArray<TArray<FV>> Cracks = {
				{ FV(0.5f, 0.36f), FV(0.48f, 0.44f), FV(0.52f, 0.52f), FV(0.49f, 0.6f), FV(0.53f, 0.7f), FV(0.5f, 0.82f) },
				{ FV(0.52f, 0.52f), FV(0.58f, 0.58f), FV(0.6f, 0.66f) },
				{ FV(0.33f, 0.6f), FV(0.35f, 0.7f), FV(0.32f, 0.8f) },
				{ FV(0.72f, 0.56f), FV(0.7f, 0.66f), FV(0.73f, 0.78f) } };
			for (const TArray<FV>& Cr : Cracks)
			{
				P.Glow([&Cr](FV Q) { return SdPolyline(Q, Cr, 0.002f); }, Glow, 0.025f, 0.8f);
				P.MLine(Cr, 0.006f, Hex(0xE85A10));
				P.MLine(Cr, 0.0025f, Hex(0xFFE890));
			}
			// sulphur crust and bacterial mats
			Speckle(P, FV(0.25f, 0.3f), FV(0.8f, 0.88f), 60, 0.003f, 0.008f, Hex(0xE8D040, 0.55f), 981);
			Speckle(P, FV(0.25f, 0.6f), FV(0.8f, 0.88f), 40, 0.004f, 0.01f, Hex(0xF4F0E0, 0.5f), 982);
			P.MEllipse(FV(0.44f, 0.5f), 0.04f, 0.2f, Hex(0xFFFFFF, 0.1f), 0.03f);
		}
		// glowing vent mouths
		for (const FChimney& C : Chimneys)
		{
			P.Glow([C](FV Q) { return SdEllipse(Q, C.Top, C.R1 * 0.8f, C.R1 * 0.35f); }, Acid, 0.05f, 0.8f);
			P.Ellipse(C.Top, C.R1 * 1.05f, C.R1 * 0.45f, Hex(0x5A524C));
			P.MEllipse(C.Top + FV(0.0f, 0.003f), C.R1 * 0.75f, C.R1 * 0.28f, Hex(0xE8FF80));
			P.MEllipse(C.Top + FV(0.0f, 0.004f), C.R1 * 0.4f, C.R1 * 0.15f, Hex(0xFFFFFF));
		}
		// a billowing plume of acid smoke from the tallest chimney: soft overlapping puffs
		const FV Top = Chimneys[0].Top;
		struct FPuff { FV C; float R; };
		const FPuff Puffs[] = { { Top + FV(0.0f, -0.05f), 0.035f }, { Top + FV(-0.03f, -0.11f), 0.045f }, { Top + FV(0.03f, -0.13f), 0.045f }, { Top + FV(-0.06f, -0.19f), 0.05f }, { Top + FV(0.04f, -0.21f), 0.05f }, { Top + FV(-0.01f, -0.25f), 0.05f } };
		const auto Plume = [&Puffs](FV Q)
		{
			float D = 1e6f;
			for (const FPuff& F : Puffs)
			{
				D = SMin(D, SdCircle(Q, F.C, F.R), 0.02f);
			}
			return D + Noise2(Q * 30.0f) * 0.006f;
		};
		P.Glow(Plume, Acid, 0.05f, 0.45f);
		P.Mark(Plume, Hex(0x8AC83A, 0.45f), 0.025f);
		{
			FClip Clip(P, Plume);
			for (const FPuff& F : Puffs)
			{
				// each billow lit on its upper left and darker underneath
				P.MCircle(F.C + FV(0.0f, F.R * 0.35f), F.R * 0.7f, Hex(0x3A6A1A, 0.25f));
				P.Mark([F](FV Q) { return SdCircle(Q, F.C + FV(-F.R * 0.3f, -F.R * 0.35f), F.R * 0.45f); }, Hex(0xE8FFB0, 0.35f), F.R * 0.3f);
			}
			P.MEllipse(Top + FV(0.0f, -0.04f), 0.025f, 0.06f, Hex(0xF4FFD0, 0.7f), 0.025f);
		}
		// droplets of acid flung out of the plume
		for (int32 k = 0; k < 5; ++k)
		{
			const FV At = Top + FV((Rand01(986, k) - 0.5f) * 0.22f, -0.05f - Rand01(987, k) * 0.2f);
			P.Glow([At](FV Q) { return SdCircle(Q, At, 0.004f); }, Acid, 0.015f, 0.6f);
			P.Ball(At, 0.007f, Hex(0xC8F050), 0.8f);
		}
		// tube worms with red plumes at the foot of the vent
		for (int32 k = 0; k < 6; ++k)
		{
			const float X = 0.2f + k * 0.022f + (k > 2 ? 0.48f : 0.0f);
			const float H = 0.1f + Rand01(983, k) * 0.08f;
			const FV Base(X, 0.86f), Tip(X + (Rand01(984, k) - 0.5f) * 0.03f, 0.86f - H);
			P.Fill([Base, Tip](FV Q) { return SdRibbon(Q, Smooth({ Base, (Base + Tip) * 0.5f + FV(0.006f, 0.0f), Tip }), 0.008f, 0.006f); }, Hex(0xF0EAD8));
			{
				FClip Clip(P, [Base, Tip](FV Q) { return SdCapsule(Q, Base, Tip, 0.008f); });
				for (int32 j = 1; j < 8; ++j)
				{
					P.MCapsule(FMath::Lerp(Base, Tip, j / 8.0f) - FV(0.008f, 0.0f), FMath::Lerp(Base, Tip, j / 8.0f) + FV(0.008f, 0.0f), 0.0012f, Hex(0x9A9078, 0.6f));
				}
			}
			for (int32 j = 0; j < 5; ++j)
			{
				const FV D = Spin(FV(0.0f, -1.0f), -50.0f + j * 25.0f);
				P.Fill([Tip, D](FV Q) { return SdRibbon(Q, { Tip, Tip + D * 0.022f }, 0.005f, 0.002f); }, Hex(0xE8303A));
			}
		}
		BubbleTrail(P, FV(0.78f, 0.36f), 3, 0.014f, 985);
	}

	bool PaintTowerHD(FPainter& P, ETDTowerType Type)
	{
		switch (Type)
		{
		case ETDTowerType::Wall:		HDTowerWall(P); return true;
		case ETDTowerType::Harpoon:		HDTowerHarpoon(P); return true;
		case ETDTowerType::Jelly:		HDTowerJelly(P); return true;
		case ETDTowerType::Puffer:		HDTowerPuffer(P); return true;
		case ETDTowerType::Eel:			HDTowerEel(P); return true;
		case ETDTowerType::Mantis:		HDTowerMantis(P); return true;
		case ETDTowerType::Octopus:		HDTowerOctopus(P); return true;
		case ETDTowerType::Lionfish:	HDTowerLionfish(P); return true;
		case ETDTowerType::Sawfish:		HDTowerSawfish(P); return true;
		case ETDTowerType::Humpback:	HDTowerHumpback(P); return true;
		case ETDTowerType::AcidVent:	HDTowerAcidVent(P); return true;
		default:						return false;
		}
	}
