// Hand-drawn HD item icons (atd.DetailedIcons 2) for the items whose detailed look was built from stuck-together
// shapes. Included from TDIcons.cpp inside its anonymous namespace, just before PaintItemKind; every other item
// keeps its detailed painter, which already picks up the HD motifs (shells, drops, pearls).

	void HDItemTitanClam(FPainter& P)
	{
		// a giant clam: two heavy, deeply fluted valves meeting in a wavy gape, the iridescent mantle bulging out
		const FLinearColor Shell = Hex(0xE4DCC4), ShellDeep = Hex(0x8E8670);
		const auto Gape = [](float X) { return 0.42f + FMath::Sin((X - 0.08f) * PI * 5.5f) * 0.05f; };
		const auto Valve = [&Gape](FV Q) { return FMath::Max(SdEllipse(Q, FV(0.5f, 0.46f), 0.46f, 0.42f), Gape(Q.X) - Q.Y - 0.015f) + FMath::Abs(FMath::Sin(Q.X * 34.0f)) * 0.004f; };
		{
			FWith With(P);
			P.Look.Dome = 0.15f;
			P.Look.Spec = 0.4f;
			P.Look.Grain = 0.08f;
			P.Fill(Valve, Shell);
		}
		{
			FClip Clip(P, Valve);
			// broad radiating flutes, each lit on one side and shaded on the other
			for (int32 i = 0; i < 6; ++i)
			{
				const float X = 0.13f + i * 0.148f;
				const FV Top(X, Gape(X)), Hinge(0.5f + (X - 0.5f) * 0.4f, 0.9f);
				P.Mark([Top, Hinge](FV Q) { return SdRibbon(Q, { Top, Hinge }, 0.05f, 0.012f); }, Hex(0xFFFFFF, 0.25f), 0.02f);
				P.Mark([Top, Hinge](FV Q) { return SdRibbon(Q, { Top + FV(0.07f, 0.0f), Hinge + FV(0.01f, 0.0f) }, 0.012f, 0.004f); }, Alpha(ShellDeep, 0.65f), 0.01f);
			}
			// growth scales stepping across the flutes
			for (int32 k = 1; k < 6; ++k)
			{
				P.Mark([k](FV Q) { return FMath::Abs((Q - FV(0.5f, 0.92f)).Size() - k * 0.085f + Noise2(Q * 30.0f) * 0.006f) - 0.004f; }, Alpha(ShellDeep, 0.45f));
			}
			HDBarnacle(P, FV(0.24f, 0.72f), 0.026f);
			P.Mark([](FV Q) { return SdCircle(Q, FV(0.72f, 0.78f), 0.05f) + Noise2(Q * 50.0f) * 0.02f; }, Hex(0x5A9A4A, 0.55f), 0.01f);
		}
		// the mantle: wavy, glowing blue-green with a pattern of iridescent spots
		const auto Mantle = [&Gape](FV Q) { return FMath::Abs(Q.Y - Gape(Q.X) + 0.01f) - 0.045f + FMath::Max(0.0f, FMath::Abs(Q.X - 0.5f) - 0.4f) * 2.0f; };
		{
			FWith With(P);
			P.Look.Dome = 0.4f;
			P.Look.Spec = 0.9f;
			P.Fill(Mantle, Hex(0x2A7AD8));
		}
		{
			FClip Clip(P, Mantle);
			P.Mark([&Gape](FV Q) { return FMath::Abs(Q.Y - Gape(Q.X) + 0.005f) - 0.008f; }, Hex(0x0A1E4A, 0.8f));
			for (int32 k = 0; k < 40; ++k)
			{
				const float X = 0.1f + Rand01(1411, k) * 0.8f;
				const FV At(X, Gape(X) - 0.01f + (Rand01(1412, k) - 0.5f) * 0.07f);
				P.MCircle(At, 0.006f + Rand01(1413, k) * 0.006f, k % 3 ? Hex(0x8CE8FF, 0.85f) : Hex(0x7AF0B0, 0.85f));
			}
			P.Mark([&Gape](FV Q) { return FMath::Abs(Q.Y - Gape(Q.X) + 0.04f) - 0.006f; }, Hex(0xFFFFFF, 0.3f));
		}
		// the upper valve's rim, overhanging the mantle
		P.Mark([&Gape](FV Q) { return FMath::Max(FMath::Abs(Q.Y - (Gape(Q.X) - 0.048f)) - 0.006f, SdEllipse(Q, FV(0.5f, 0.46f), 0.46f, 0.42f)); }, Hex(0xFFF8E8, 0.8f));
	}

	/**
	 * A spired sea shell drawn whorl by whorl: each whorl a rounded swelling blended into the next, so the outline is
	 * naturally lumpy on both sides; sutures, spiral cords and growth lines follow the whorls, and the body whorl opens
	 * into a glossy aperture running down into the siphonal canal. Returns the body whorl's centre and the aperture's centre.
	 */
	struct FWhorlShell
	{
		FV Apex, Base, D, N;
		float Len = 0.0f, Width = 0.0f;
		TArray<FV> Centres;
		TArray<float> Radii;
		FV ApertureC;
		FV BodyC;

		FV At(float T, float V) const { return Apex + D * (Len * T) + N * V; }
		/** Signed distance to whorl I: an ellipse across the axis */
		float Whorl(FV Q, int32 I) const
		{
			const FV L = Q - Centres[I];
			const FV Loc(FV::DotProduct(L, N), FV::DotProduct(L, D));
			return SdEllipse(Loc, FV(0.0f, 0.0f), Radii[I], Radii[I] * 0.62f);
		}
		float Shape(FV Q) const
		{
			// the spire, the whorls and the canal
			float Dist = SdPoly(Q, { Apex, At(0.62f, Width * 0.7f), At(0.62f, -Width * 0.7f) });
			for (int32 I = 0; I < Centres.Num(); ++I)
			{
				Dist = SMin(Dist, Whorl(Q, I), 0.018f);
			}
			const FV L = Q - Apex;
			const float T = FV::DotProduct(L, D) / Len;
			const float V = FV::DotProduct(L, N);
			const float Canal = FMath::Max(FMath::Abs(V + Width * 0.08f) - Width * FMath::Lerp(0.42f, 0.1f, FMath::Clamp((T - 0.8f) / 0.2f, 0.0f, 1.0f)), FMath::Max(0.8f - T, T - 1.0f) * Len);
			return SMin(Dist, Canal, 0.03f);
		}
	};

	FWhorlShell HDWhorlGeom(FV Apex, FV Base, float Width, int32 Whorls)
	{
		FWhorlShell S;
		S.Apex = Apex;
		S.Base = Base;
		S.D = (Base - Apex).GetSafeNormal();
		S.N = FV(-S.D.Y, S.D.X);
		S.Len = (Base - Apex).Size();
		S.Width = Width;
		for (int32 I = 0; I < Whorls; ++I)
		{
			const float U = (I + 1.0f) / Whorls;
			const float T = 0.06f + 0.6f * FMath::Pow(U, 1.35f);
			const float R = Width * (I == Whorls - 1 ? 1.0f : FMath::Pow(U, 1.1f) * 0.82f);
			S.Centres.Add(S.At(T, 0.0f));
			S.Radii.Add(R);
		}
		S.BodyC = S.Centres.Last();
		S.ApertureC = S.At(0.78f, S.Width * 0.08f);
		return S;
	}

	FWhorlShell HDWhorlShell(FPainter& P, FV Apex, FV Base, float Width, int32 Whorls, const FLinearColor& Color, const FLinearColor& Band, const FLinearColor& Lip, const FLinearColor& Throat, int32 Seed, bool bAperture = true)
	{
		FWhorlShell S = HDWhorlGeom(Apex, Base, Width, Whorls);
		const auto Shape = [&S](FV Q) { return S.Shape(Q); };
		P.Mark([&S](FV Q) { return S.Shape(Q - FV(0.012f, 0.02f)); }, Hex(0x06121A, 0.3f), 0.03f);
		{
			FWith With(P);
			P.Look.Dome = Width * 0.6f;
			P.Look.Spec = 0.8f;
			P.Look.Grain = 0.08f;
			P.Fill(Shape, Color);
		}
		{
			FClip Clip(P, Shape);
			// each whorl rounded: shade gathering under the suture above it, light across its shoulder
			for (int32 I = 1; I < Whorls; ++I)
			{
				const FV C = S.Centres[I];
				const FV Dd = S.D;
				const float R = S.Radii[I];
				P.Mark([&S, I, C, Dd](FV Q) { return FMath::Max(FMath::Abs(S.Whorl(Q, I)) - 0.0035f, FV::DotProduct(Q - C, Dd)); }, Alpha(Tone(Band, 0.45f), 0.9f));
				P.Mark([&S, I, C, Dd, R](FV Q) { return FMath::Max(S.Whorl(Q, I) + R * 0.08f, FMath::Max(-S.Whorl(Q, I) - R * 0.25f, FV::DotProduct(Q - C, Dd) + R * 0.1f)); }, Alpha(Tone(Band, 0.5f), 0.3f), 0.02f);
				P.Mark([&S, I, C, Dd, R](FV Q) { return FMath::Max(FMath::Abs(S.Whorl(Q, I) + R * 0.2f) - 0.003f, FV::DotProduct(Q - C, Dd) + R * 0.2f); }, Hex(0xFFFFFF, 0.35f), 0.003f);
			}
			// spiral cords of colour round every whorl, and a scatter of streaks along the growth lines
			for (int32 I = 0; I < Whorls; ++I)
			{
				const FV C = S.Centres[I];
				const FV Dd = S.D;
				const float R = S.Radii[I];
				for (const float K : { 0.15f, 0.42f })
				{
					P.Mark([C, Dd, R, K, &S](FV Q) { const FV L = Q - C; const FV Loc(FV::DotProduct(L, S.N), FV::DotProduct(L, Dd)); return FMath::Abs(SdEllipse(Loc - FV(0.0f, -R * K), FV(0.0f, 0.0f), R * 1.02f, R * 0.5f)) - R * 0.035f; }, Alpha(Band, 0.45f), 0.003f);
				}
			}
			for (int32 K = 0; K < 30; ++K)
			{
				const float T = 0.12f + Rand01(Seed, K) * 0.7f;
				const FV At = S.At(T, (Rand01(Seed + 1, K) - 0.5f) * Width * 1.4f * T);
				P.MLine({ At, At + S.D * 0.012f + S.N * 0.005f, At + S.D * 0.022f }, 0.0025f, Alpha(Band, 0.6f));
			}
			Speckle(P, S.Apex - FV(Width, Width), S.Base + FV(Width, Width), 60, 0.0015f, 0.004f, Alpha(Tone(Band, 0.6f), 0.5f), Seed + 2);
			// a long soft reflection down the lit side
			P.Mark([&S](FV Q) { return SdCapsule(Q, S.At(0.15f, -S.Width * 0.12f), S.At(0.7f, -S.Width * 0.6f), S.Width * 0.06f); }, Hex(0xFFFFFF, 0.22f), S.Width * 0.08f);
		}
		// the aperture: an oval opening down the body whorl, its outer lip flared and glossy, the throat deep coloured
		S.ApertureC = S.At(0.78f, S.Width * 0.08f);
		if (bAperture)
		{
			const float Rot = FMath::RadiansToDegrees(FMath::Atan2(S.D.Y, S.D.X));
			FNest Place(P, S.ApertureC, 1.0f, Rot);
			const float A0 = S.Len * 0.15f, B0 = Width * 0.4f;
			const auto Opening = [A0, B0](FV Q) { return SdEllipse(Q, FV(0.0f, 0.0f), A0, B0 * (1.0f - FMath::Max(0.0f, Q.X / A0) * 0.35f)); };
			P.Mark([&Opening](FV Q) { return Opening(Q - FV(0.0f, 0.012f)) - 0.006f; }, Hex(0x2A140A, 0.4f), 0.02f);
			{
				FWith With(P);
				P.Look.Dome = B0 * 0.5f;
				P.Look.Spec = 0.95f;
				P.Look.Ink = 0.007f;
				P.Fill(Opening, Lip);
			}
			FClip Clip(P, Opening);
			P.Mark([&Opening, B0](FV Q) { return Opening(Q * 1.28f + FV(0.0f, -B0 * 0.1f)); }, Throat, 0.012f);
			P.Mark([&Opening, B0](FV Q) { return Opening(Q * 1.9f + FV(-0.02f, -B0 * 0.25f)); }, Tone(Throat, 0.45f), 0.03f);
			// the columella's folds along the inner edge and a wet gleam on the lip
			P.MLine(Smooth({ FV(-A0 * 0.7f, -B0 * 0.45f), FV(0.0f, -B0 * 0.62f), FV(A0 * 0.7f, -B0 * 0.5f) }), 0.006f, Alpha(Tone(Lip, 1.15f), 0.8f));
			P.Mark([&Opening](FV Q) { return FMath::Max(FMath::Abs(Opening(Q) + 0.008f) - 0.003f, Q.Y + Q.X * 0.3f); }, Hex(0xFFFFFF, 0.75f));
		}
		return S;
	}

	void HDItemSharpenedCowrie(FPainter& P)
	{
		// a glossy tiger cowrie turned to show its toothed slit, its rim honed to a silver edge
		FNest Place(P, FV(0.5f, 0.52f), 1.0f, -24.0f);
		const auto Shell = [](FV Q)
		{
			// an egg: fuller at the back, a little pinched at the front
			const float Rx = 0.37f, Ry = 0.24f - Q.X * 0.035f;
			return SdEllipse(Q, FV(0.0f, 0.0f), Rx, Ry);
		};
		// the honed rim first, a sliver of steel showing round the upper edge
		{
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Spec = 1.0f;
			P.Fill([&Shell](FV Q) { return FMath::Max(Shell(Q) - 0.022f, Q.Y + 0.04f - Q.X * 0.1f); }, Hex(0xC8D6E0));
		}
		P.Mark([&Shell](FV Q) { return FMath::Max(FMath::Abs(Shell(Q) - 0.019f) - 0.0025f, Q.Y + 0.06f); }, Hex(0xFFFFFF, 0.9f));
		{
			FWith With(P);
			P.Look.Dome = 0.24f;
			P.Look.Spec = 1.0f;
			P.Fill(Shell, Hex(0xF2E2C4));
		}
		{
			FClip Clip(P, Shell);
			// the dorsal mantle colouring: warm tan above a wavy mantle line, cream below
			P.Mark([](FV Q) { return Q.Y - (0.05f + FMath::Sin(Q.X * 26.0f) * 0.008f + Q.X * Q.X * 0.4f); }, Hex(0xC8884A), 0.03f);
			P.Mark([](FV Q) { return FMath::Abs(Q.Y - (0.05f + FMath::Sin(Q.X * 26.0f) * 0.008f + Q.X * Q.X * 0.4f)) - 0.004f; }, Hex(0xF8EAD0, 0.6f));
			// tiger spots: irregular, some ringed with a pale halo, larger towards the crown
			for (int32 k = 0; k < 26; ++k)
			{
				const FV At(-0.33f + Rand01(1601, k) * 0.66f, -0.22f + Rand01(1602, k) * 0.26f);
				const float R = 0.012f + Rand01(1603, k) * 0.026f * (1.0f - FMath::Abs(At.X) * 1.6f);
				if (R < 0.008f)
				{
					continue;
				}
				P.Mark([At, R](FV Q) { return SdEllipse(Q, At, R * 1.15f, R) + Noise2(Q * 40.0f + FV(At.X * 9.0f, 0.0f)) * 0.0015f; }, Hex(0x4A2410, 0.85f), R * 0.35f);
			}
			Speckle(P, FV(-0.36f, -0.24f), FV(0.36f, 0.05f), 90, 0.002f, 0.005f, Hex(0x6A3A18, 0.6f), 1604);
			// the underside: the slit runs along a shallow curve, both lips ridged with teeth
			const auto SlitY = [](float X) { return 0.13f - X * X * 0.35f; };
			P.Mark([SlitY](FV Q) { return FMath::Max(FMath::Abs(Q.Y - SlitY(Q.X)) - 0.016f * (1.0f - FMath::Square(Q.X / 0.3f)), FMath::Abs(Q.X) - 0.3f); }, Hex(0x2A140A));
			for (int32 i = 0; i < 12; ++i)
			{
				const float X = -0.25f + i * 0.046f;
				const float Y = SlitY(X);
				const float H = 0.03f * (1.0f - FMath::Square(X / 0.32f));
				for (const float Side : { -1.0f, 1.0f })
				{
					const FV R0(X, Y + Side * 0.008f), R1(X + 0.004f, Y + Side * (0.008f + H));
					P.Mark([R0, R1](FV Q) { return SdCapsule(Q, R0 + FV(0.005f, 0.0f), R1 + FV(0.005f, 0.0f), 0.009f); }, Hex(0xA87A50, 0.35f), 0.006f);
					P.Mark([R0, R1](FV Q) { return SdCapsule(Q, R0, R1, 0.008f); }, Hex(0xFFF8EC), 0.004f);
				}
			}
			// glaze: a long soft sheen over the crown, a crisp window highlight, and a shadow under the belly
			P.MEllipse(FV(-0.06f, -0.13f), 0.24f, 0.05f, Hex(0xFFFFFF, 0.35f), 0.04f);
			P.MEllipse(FV(-0.14f, -0.15f), 0.07f, 0.018f, Hex(0xFFFFFF, 0.9f));
			P.MEllipse(FV(0.04f, 0.24f), 0.38f, 0.06f, Hex(0x5A3A20, 0.3f), 0.05f);
		}
		Glint(P, FV(0.16f, -0.25f), 0.08f);
	}

	void HDItemSpiralWhelk(FPainter& P)
	{
		// a tall whelk caught in its own swirling current, a vortex of water winding round it
		const FV A(0.7f, 0.12f), B(0.36f, 0.74f);
		const FV D = (B - A).GetSafeNormal();
		const FV N(-D.Y, D.X);
		const float Len = (B - A).Size();
		const FV M = A + D * Len * 0.55f;
		// the current: a ribbon spiralling round the shell's axis, the far side behind the shell, the near side in front
		TArray<FV> Swirl;
		TArray<float> Depth;
		for (int32 k = 0; k <= 80; ++k)
		{
			const float T = k / 80.0f;
			const float Ang = T * 2.0f * PI * 1.75f + 0.4f;
			const float Rad = 0.08f + T * 0.1f;
			Swirl.Add(M + D * (0.2f - T * 0.42f + FMath::Sin(Ang) * 0.04f) + N * FMath::Cos(Ang) * Rad);
			Depth.Add(FMath::Sin(Ang));
		}
		const auto DrawSwirl = [&P, &Swirl, &Depth](bool bFront)
		{
			int32 Start = -1;
			for (int32 k = 0; k <= Swirl.Num(); ++k)
			{
				const bool bIn = k < Swirl.Num() && ((Depth[k] > 0.0f) == bFront);
				if (bIn && Start < 0)
				{
					Start = k;
				}
				if (!bIn && Start >= 0)
				{
					TArray<FV> Run;
					for (int32 j = FMath::Max(0, Start - 1); j < FMath::Min(Swirl.Num(), k + 1); ++j)
					{
						Run.Add(Swirl[j]);
					}
					const float W0 = 0.006f + Start / 80.0f * 0.02f, W1 = 0.006f + k / 80.0f * 0.02f;
					P.Glow([Run, W0, W1](FV Q) { return SdRibbon(Q, Run, W0, W1); }, Hex(0x7AD8F8), 0.03f, bFront ? 0.4f : 0.2f);
					P.Mark([Run, W0, W1](FV Q) { return SdRibbon(Q, Run, W0, W1); }, Hex(bFront ? 0x8AE0FA : 0x4A98C0, bFront ? 0.8f : 0.55f), 0.004f);
					P.Mark([Run, W0, W1](FV Q) { return SdRibbon(Q, Run, W0 * 0.3f, W1 * 0.3f); }, Hex(0xFFFFFF, bFront ? 0.8f : 0.3f), 0.003f);
					Start = -1;
				}
			}
		};
		DrawSwirl(false);
		HDWhorlShell(P, A, B, 0.19f, 6, Hex(0xEACCA0), Hex(0x9A5A36), Hex(0xFAD8C0), Hex(0xD8705A), 1614);
		DrawSwirl(true);
		// spray flung off the current
		for (int32 k = 0; k < 7; ++k)
		{
			const FV At = M + N * (0.25f + Rand01(1611, k) * 0.06f) * (k % 2 ? 1.0f : -1.0f) + D * (Rand01(1612, k) - 0.5f) * 0.4f;
			Bubble(P, At, 0.008f + Rand01(1613, k) * 0.01f);
		}
	}

	void HDItemScallopFan(FPainter& P)
	{
		// a pink scallop swimming by clapping its valves: ribbed fan, ears at the hinge, a row of blue eyes on the mantle, a jet of bubbles behind
		const FV Hinge(0.5f, 0.8f);
		const float R = 0.5f;
		const auto Fan = [Hinge, R](FV Q)
		{
			const FV L = Q - Hinge;
			const float Ang = FMath::Atan2(L.X, -L.Y);
			const float Scallop = FMath::Abs(FMath::Sin(Ang * 9.0f)) * 0.018f;
			return FMath::Max(L.Size() - R + Scallop, (FMath::Abs(Ang) - 0.95f) * L.Size());
		};
		const auto Ears = [Hinge](FV Q) { return SdPoly(Q, { Hinge + FV(-0.17f, -0.06f), Hinge + FV(0.17f, -0.06f), Hinge + FV(0.13f, 0.03f), Hinge + FV(-0.13f, 0.03f) }) - 0.01f; };
		const auto Valve = [&Fan, &Ears](FV Q) { return SMin(Fan(Q), Ears(Q), 0.03f); };
		// the lower valve peeking out under the upper, and the mantle edge between them with its eyes
		P.Fill([Hinge, R](FV Q) { const FV L = Q - (Hinge + FV(0.0f, 0.02f)); return FMath::Max(L.Size() - R * 0.98f, (FMath::Abs(FMath::Atan2(L.X, -L.Y)) - 0.93f) * L.Size()); }, Hex(0xC86A7A));
		P.Fill([Hinge, R](FV Q) { const FV L = Q - (Hinge + FV(0.0f, 0.01f)); return FMath::Max(FMath::Abs(L.Size() - R * 0.95f) - 0.022f, (FMath::Abs(FMath::Atan2(L.X, -L.Y)) - 0.9f) * L.Size()); }, Hex(0xE8A090));
		for (int32 k = 0; k < 13; ++k)
		{
			const float Ang = -0.82f + k * (1.64f / 12.0f);
			const FV At = Hinge + FV(0.0f, 0.01f) + FV(FMath::Sin(Ang), -FMath::Cos(Ang)) * R * 0.95f;
			P.Ball(At, 0.011f, Hex(0x2A7AE8), 0.9f);
			P.MCircle(At + FV(0.002f, 0.002f), 0.004f, Hex(0x0A1A40));
		}
		// the upper valve, tipped up as it claps
		{
			FNest Place(P, Hinge, 1.0f, 0.0f);
			const FV Up(0.0f, 0.0f);
			FNest Lift(P, Up, 0.86f, 0.0f);
			{
				FWith With(P);
				P.Look.Dome = 0.08f;
				P.Look.Spec = 0.7f;
				P.Fill([&Valve, Hinge](FV Q) { return Valve(Q + Hinge); }, Hex(0xF48AA0));
			}
			FClip Clip(P, [&Valve, Hinge](FV Q) { return Valve(Q + Hinge); });
			// radiating ribs: each a rounded ridge with a lit flank and a shaded flank
			for (int32 k = -8; k <= 8; ++k)
			{
				const float Ang = k * (0.95f / 8.5f);
				const FV Dir(FMath::Sin(Ang), -FMath::Cos(Ang));
				P.Mark([Dir](FV Q) { return SdCapsule(Q, Dir * 0.06f, Dir * 0.5f, 0.004f) - FV::DotProduct(Q, Dir) * 0.04f; }, Hex(0x9A3A5A, 0.5f), 0.008f);
				P.Mark([Dir](FV Q) { const FV Sh = FV(-Dir.Y, Dir.X) * 0.012f; return SdCapsule(Q, Dir * 0.06f + Sh, Dir * 0.5f + Sh, 0.002f); }, Hex(0xFFE0EA, 0.55f), 0.004f);
			}
			// growth lines, concentric, and a band of deeper colour
			for (int32 k = 1; k <= 7; ++k)
			{
				const float Rr = 0.06f + k * 0.06f;
				P.Mark([Rr](FV Q) { return FMath::Abs(Q.Size() - Rr) - 0.002f; }, Hex(0xB04A6A, 0.35f));
			}
			P.Mark([](FV Q) { return FMath::Abs(Q.Size() - 0.3f) - 0.04f; }, Hex(0xE85A7A, 0.35f), 0.02f);
			Speckle(P, FV(-0.5f, -0.5f), FV(0.5f, 0.04f), 90, 0.002f, 0.004f, Hex(0xFFFFFF, 0.4f), 1621);
			// the ears: finely ribbed, with the hinge line
			P.MLine({ FV(-0.17f, 0.0f), FV(0.17f, 0.0f) }, 0.003f, Hex(0x8A2A4A, 0.6f));
			for (int32 k = 0; k < 6; ++k)
			{
				P.MLine({ FV(-0.16f + k * 0.018f, -0.055f), FV(-0.16f + k * 0.018f, 0.0f) }, 0.002f, Hex(0x8A2A4A, 0.4f));
				P.MLine({ FV(0.07f + k * 0.018f, -0.055f), FV(0.07f + k * 0.018f, 0.0f) }, 0.002f, Hex(0x8A2A4A, 0.4f));
			}
			P.MEllipse(FV(-0.14f, -0.32f), 0.1f, 0.05f, Hex(0xFFFFFF, 0.35f), 0.04f);
		}
		// the jet: water and bubbles driven out past the hinge as the valves snap shut
		for (const float Side : { -1.0f, 1.0f })
		{
			const TArray<FV> Jet = Smooth({ Hinge + FV(Side * 0.12f, 0.02f), Hinge + FV(Side * 0.2f, 0.07f), Hinge + FV(Side * 0.3f, 0.1f) });
			P.Mark([Jet](FV Q) { return SdRibbon(Q, Jet, 0.012f, 0.002f); }, Hex(0xBFEFFF, 0.6f), 0.006f);
			for (int32 k = 0; k < 3; ++k)
			{
				Bubble(P, Hinge + FV(Side * (0.24f + k * 0.07f), 0.06f + k * 0.03f + Rand01(1622, k) * 0.02f), 0.014f - k * 0.003f);
			}
		}
	}

	void HDItemRazorClamBody(FPainter& P);
	void HDItemRazorClam(FPainter& P)
	{
		// a long razor clam gaping open, its foot pushing out one end and siphons out the other, its lip honed and bloodied
		HDItemRazorClamBody(P);
		const FV Edge = FV(0.5f, 0.5f) + Spin(FV(0.16f, 0.108f - 0.16f * 0.16f * 0.18f), -38.0f);
		HDDrip(P, Edge + FV(-0.005f, -0.012f), 0.11f, 0.032f, Hex(0xC01820), 0.004f, 0.012f);
	}

	void HDItemRazorClamBody(FPainter& P)
	{
		FNest Place(P, FV(0.5f, 0.5f), 1.0f, -38.0f);
		const auto Bend = [](FV Q) { return FV(Q.X, Q.Y + Q.X * Q.X * 0.18f); };
		const auto Valve = [Bend](FV Q, float Y0, float H) { const FV L = Bend(Q); return SdBox(L, FV(-0.38f, Y0 - H), FV(0.38f, Y0 + H), H * 0.95f); };
		// the far valve behind, showing pearly nacre inside the gape
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Fill([&Valve](FV Q) { return Valve(Q, -0.045f, 0.07f); }, Hex(0x8A7A5A));
		}
		P.Mark([&Valve](FV Q) { return Valve(Q, -0.03f, 0.05f); }, Hex(0xE8E2DA));
		P.Mark([&Valve](FV Q) { return Valve(Q, -0.02f, 0.035f); }, Hex(0xC8D0E0, 0.5f), 0.02f);
		// the pink foot bulging out of the right end, the paired siphons out of the left
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Look.Spec = 0.5f;
			P.Fill([Bend](FV Q) { const FV L = Bend(Q); return SMin(SdEllipse(L, FV(0.4f, 0.0f), 0.07f, 0.05f), SdCircle(L, FV(0.45f, -0.01f), 0.045f), 0.02f); }, Hex(0xF4A8B0));
			P.Fill([Bend](FV Q) { const FV L = Bend(Q); return SMin(SdCapsule(L, FV(-0.36f, -0.03f), FV(-0.44f, -0.05f), 0.026f), SdCapsule(L, FV(-0.36f, 0.02f), FV(-0.43f, 0.03f), 0.024f), 0.012f); }, Hex(0xD8A890));
		}
		for (const FV Tip : { FV(-0.44f, -0.05f), FV(-0.43f, 0.03f) })
		{
			P.Mark([Bend, Tip](FV Q) { return SdCircle(Bend(Q), Tip + FV(-0.012f, 0.0f), 0.013f); }, Hex(0x5A3A2A));
			for (int32 k = 0; k < 7; ++k)
			{
				const FV Dir = Spin(FV(-1.0f, 0.0f), -70.0f + k * 23.0f);
				P.Mark([Bend, Tip, Dir](FV Q) { return SdCapsule(Bend(Q), Tip + Dir * 0.022f, Tip + Dir * 0.034f, 0.003f); }, Hex(0xC89A70));
			}
		}
		P.Mark([Bend](FV Q) { return FMath::Abs(SdEllipse(Bend(Q), FV(0.43f, 0.0f), 0.04f, 0.025f)) - 0.003f; }, Hex(0xC86A7A, 0.6f));
		// the near valve: olive-brown skin over banded shell, growth lines sweeping from the beak
		const auto Near = [&Valve](FV Q) { return Valve(Q, 0.04f, 0.075f); };
		{
			FWith With(P);
			P.Look.Dome = 0.08f;
			P.Look.Spec = 0.85f;
			P.Fill(Near, Hex(0xC8A878));
		}
		{
			FClip Clip(P, Near);
			for (int32 k = 0; k < 12; ++k)
			{
				const float X0 = -0.36f + k * 0.065f;
				P.Mark([Bend, X0](FV Q) { const FV L = Bend(Q); return FMath::Abs(L.X - X0 - (L.Y - 0.04f) * (L.Y - 0.04f) * 3.0f) - 0.0025f; }, Hex(0x6A4A2A, 0.45f));
			}
			for (int32 k = 0; k < 4; ++k)
			{
				const float X0 = -0.3f + k * 0.17f;
				P.Mark([Bend, X0](FV Q) { const FV L = Bend(Q); return FMath::Abs(L.X - X0 - (L.Y - 0.04f) * (L.Y - 0.04f) * 3.0f) - 0.022f; }, Hex(0xA87A50, 0.35f), 0.01f);
			}
			// glossy olive periostracum along the hinge side
			P.Mark([Bend](FV Q) { const FV L = Bend(Q); return L.Y - 0.005f + FMath::Sin(L.X * 30.0f) * 0.004f; }, Hex(0x5A5A26, 0.85f), 0.015f);
			P.Mark([Bend](FV Q) { const FV L = Bend(Q); return FMath::Abs(L.Y + 0.02f) - 0.004f; }, Hex(0xE8E8A0, 0.4f), 0.004f);
			Speckle(P, FV(-0.4f, -0.05f), FV(0.4f, 0.14f), 60, 0.0015f, 0.0035f, Hex(0x3A2A10, 0.5f), 1631);
			// the honed lip: a band of bright steel along the gaping edge
			P.Mark([Bend](FV Q) { const FV L = Bend(Q); return FMath::Abs(L.Y - 0.1f) - 0.012f; }, Hex(0xD8E4EC));
			P.Mark([Bend](FV Q) { const FV L = Bend(Q); return FMath::Abs(L.Y - 0.094f) - 0.003f; }, Hex(0xFFFFFF, 0.9f));
			P.Mark([Bend](FV Q) { const FV L = Bend(Q); return FMath::Abs(L.Y - 0.108f) - 0.003f; }, Hex(0x5A6A78, 0.7f));
			// blood smeared along the edge
			P.Mark([Bend](FV Q) { const FV L = Bend(Q); return FMath::Max(FMath::Abs(L.Y - 0.104f) - 0.012f, FMath::Abs(L.X - 0.12f) - 0.12f + Noise2(Q * 60.0f) * 0.04f); }, Hex(0xB0101A, 0.85f));
		}
		// a drip running off the edge, in screen space so it falls straight down
		Glint(P, FV(-0.18f, 0.105f), 0.07f);
	}

	/** A heart outline from the classic parametric curve, Width wide, centred on C, rotated by RotDeg */
	TArray<FV> HDHeartOutline(FV C, float Width, float RotDeg = 0.0f)
	{
		TArray<FV> Pts;
		for (int32 K = 0; K < 96; ++K)
		{
			const float T = K * 2.0f * PI / 96.0f;
			const float Sn = FMath::Sin(T);
			const float X = 16.0f * Sn * Sn * Sn;
			const float Y = -(13.0f * FMath::Cos(T) - 5.0f * FMath::Cos(2.0f * T) - 2.0f * FMath::Cos(3.0f * T) - FMath::Cos(4.0f * T));
			Pts.Add(C + Spin(FV(X, Y + 2.5f) * (Width / 32.0f), RotDeg));
		}
		return Pts;
	}

	void HDItemIroncladNautilus(FPainter& P)
	{
		// a chambered nautilus in side view, its flame-striped shell bound in riveted iron, the hood and tentacles in the mouth
		const FV C(0.46f, 0.5f);
		const float R = 0.4f;
		const float B = FMath::Loge(3.0f) / (2.0f * PI);
		const float End = FMath::DegreesToRadians(-35.0f);
		const auto Rad = [R, B, End](float Th) { return R * FMath::Exp(B * (Th - End)); };
		TArray<FV> Outline, Suture;
		for (int32 K = 0; K <= 120; ++K)
		{
			const float Th = End - 2.0f * PI + K * (2.0f * PI / 120.0f);
			Outline.Add(C + FV(FMath::Cos(Th), FMath::Sin(Th)) * Rad(Th));
			Suture.Add(C + FV(FMath::Cos(Th), FMath::Sin(Th)) * Rad(Th) / 3.0f);
		}
		const auto Shell = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		// polar helpers: which turn of the shell a point is on and how far across that whorl it lies
		const auto Across = [C, Rad, End](FV Q, float& OutTh, float& OutU)
		{
			const FV L = Q - C;
			float Th = FMath::Atan2(L.Y, L.X);
			while (Th > End) Th -= 2.0f * PI;
			while (Th < End - 2.0f * PI) Th += 2.0f * PI;
			const float Outer = Rad(Th);
			OutTh = Th;
			OutU = (L.Size() - Outer / 3.0f) / (Outer * (2.0f / 3.0f));
		};
		P.Mark([&Shell](FV Q) { return Shell(Q - FV(0.015f, 0.025f)); }, Hex(0x06121A, 0.3f), 0.04f);
		{
			FWith With(P);
			P.Look.Dome = 0.18f;
			P.Look.Spec = 0.8f;
			P.Fill(Shell, Hex(0xF2E6D2));
		}
		{
			FClip Clip(P, Shell);
			// flame stripes sweeping out from the coil, fading towards the newest part of the shell
			P.Mark([&Across, End](FV Q)
			{
				float Th, U;
				Across(Q, Th, U);
				const float Age = (End - Th) / (2.0f * PI);
				const float S = FMath::Sin(Th * 9.0f + FMath::Loge(FMath::Max(U, 0.02f) + 0.3f) * 5.0f + Noise2(Q * 20.0f) * 0.6f);
				return FMath::Max(FMath::Max((0.35f - S) * 0.02f, (0.32f - Age) * 0.2f), (U - 0.95f) * 0.2f);
			}, Hex(0xB0582A, 0.85f), 0.006f);
			// the nacre glow towards the coil and the shading across each whorl
			P.Mark([&Across](FV Q) { float Th, U; Across(Q, Th, U); return (U - 0.25f) * 0.05f; }, Hex(0x7A5A4A, 0.35f), 0.01f);
			P.MLine(Suture, 0.006f, Hex(0x4A2A18, 0.9f));
			P.MCircle(C, 0.03f, Hex(0x3A2418));
			P.Mark([C](FV Q) { return FMath::Abs(SdCircle(Q, C, 0.03f)) - 0.004f; }, Hex(0xB89A80, 0.6f));
			// growth lines following the curve of the aperture
			for (int32 K = 0; K < 26; ++K)
			{
				const float Th = End - 0.1f - K * 0.2f;
				const FV Dir(FMath::Cos(Th), FMath::Sin(Th));
				P.MLine({ C + Dir * Rad(Th) / 3.0f, C + Spin(Dir, -6.0f) * Rad(Th) * 0.66f, C + Spin(Dir, -2.0f) * Rad(Th) }, 0.0018f, Hex(0x8A6A50, 0.3f));
			}
			// the iron: a riveted rim band round the outer edge and three curved straps across the whorl
			const FLinearColor BandIron = Hex(0x8A949C), BandIronDark = Hex(0x3A4248);
			P.Mark([&Shell](FV Q) { return FMath::Abs(Shell(Q) + 0.03f) - 0.03f; }, BandIronDark);
			{
				FWith With(P);
				P.Look.Dome = 0.025f;
				P.Look.Spec = 0.9f;
				P.Fill([&Shell](FV Q) { return FMath::Abs(Shell(Q) + 0.03f) - 0.025f; }, BandIron);
			}
			for (const float Deg : { 120.0f, 230.0f, 330.0f })
			{
				const float Th = End - FMath::DegreesToRadians(Deg);
				const FV Dir(FMath::Cos(Th), FMath::Sin(Th));
				const TArray<FV> Strap = Smooth({ C + Dir * Rad(Th) / 3.0f, C + Spin(Dir, -7.0f) * Rad(Th) * 0.66f, C + Spin(Dir, -3.0f) * Rad(Th) });
				P.Mark([Strap](FV Q) { return SdPolyline(Q, Strap, 0.028f); }, BandIronDark);
				{
					FWith With(P);
					P.Look.Dome = 0.02f;
					P.Look.Spec = 0.9f;
					P.Fill([Strap](FV Q) { return SdPolyline(Q, Strap, 0.023f); }, BandIron);
				}
				for (int32 K = 0; K < 4; ++K)
				{
					FV Pos, Tan;
					Along(Strap, 0.12f + K * 0.25f, Pos, Tan);
					P.Ball(Pos, 0.009f, Hex(0xB8C2CA), 0.9f);
				}
				// rust weeping from the strap
				P.Mark([Strap](FV Q) { return SdPolyline(Q, Strap, 0.04f) + Noise2(Q * 40.0f) * 0.02f; }, Hex(0xA0542A, 0.3f), 0.01f);
			}
			for (int32 K = 0; K < 22; ++K)
			{
				const float Th = End - 0.25f - K * (2.0f * PI - 0.5f) / 21.0f;
				P.Ball(C + FV(FMath::Cos(Th), FMath::Sin(Th)) * (Rad(Th) - 0.03f), 0.008f, Hex(0xB8C2CA), 0.9f);
			}
			P.MEllipse(C + FV(-0.12f, -0.16f), 0.12f, 0.05f, Hex(0xFFFFFF, 0.25f), 0.05f);
		}
		// the mouth: the leathery hood over the opening, tentacles curling out beneath it
		{
			const FV Out = C + FV(FMath::Cos(End), FMath::Sin(End)) * R;
			const FV In = C + FV(FMath::Cos(End), FMath::Sin(End)) * (R / 3.0f);
			const FV Mid = (Out + In) * 0.5f;
			const FV Along2 = (Out - In).GetSafeNormal();
			const FV Side(-Along2.Y, Along2.X);
			for (int32 K = 0; K < 5; ++K)
			{
				const FV Root = FMath::Lerp(In, Out, 0.3f + K * 0.1f);
				const FV Tip = Root + Side * (0.08f + K * 0.012f) + Along2 * (0.03f - K * 0.02f);
				P.Fill([Root, Tip, Side](FV Q) { return SdRibbon(Q, Smooth({ Root, Root + Side * 0.05f, Tip + Side * 0.01f - (Tip - Root).GetSafeNormal() * 0.01f, Tip }), 0.011f, 0.004f); }, Hex(0xE8C8B0));
			}
			P.Fill([Mid, Along2, Side, Out, In](FV Q)
			{
				const FV L = Q - Mid;
				const FV Loc(FV::DotProduct(L, Along2), FV::DotProduct(L, Side));
				return SdEllipse(Loc, FV(0.0f, 0.01f), (Out - In).Size() * 0.48f, 0.045f);
			}, Hex(0x7A4A2E));
			P.Mark([Mid, Along2, Side, Out, In](FV Q)
			{
				const FV L = Q - Mid;
				const FV Loc(FV::DotProduct(L, Along2), FV::DotProduct(L, Side));
				return FMath::Max(FMath::Abs(FMath::Sin(Loc.X * 90.0f)) * 0.01f + FMath::Abs(Loc.Y - 0.01f) - 0.03f, SdEllipse(Loc, FV(0.0f, 0.01f), (Out - In).Size() * 0.46f, 0.04f));
			}, Hex(0x4A2414, 0.35f));
			const FV Eye = FMath::Lerp(In, Out, 0.25f) + Side * 0.03f;
			P.Ball(Eye, 0.016f, Hex(0x2A2018), 0.9f);
		}
	}

	void HDItemTritonsTrumpet(FPainter& P)
	{
		// a long triton's trumpet, its toothed orange mouth blasting out a crackling surge of sound and lightning
		const FV Apex(0.86f, 0.88f), Base(0.4f, 0.36f);
		// the blast first, so the shell's mouth sits over its root
		const FV Mouth = Base + (Apex - Base).GetSafeNormal() * 0.14f;
		const FV Out = (Base - Apex).GetSafeNormal();
		const FV Side(-Out.Y, Out.X);
		P.Glow([Mouth, Out](FV Q) { return SdCircle(Q, Mouth + Out * 0.14f, 0.08f); }, Hex(0xFFE070), 0.12f, 0.55f);
		for (int32 K = 0; K < 3; ++K)
		{
			const float R = 0.1f + K * 0.075f;
			P.Mark([Mouth, Out, Side, R](FV Q) { const FV L = Q - Mouth; const float U = FV::DotProduct(L, Out); const float V = FV::DotProduct(L, Side); return FMath::Max(FMath::Abs(L.Size() - R) - 0.007f + FMath::Abs(V) / R * 0.006f, FMath::Abs(V) - U * 0.75f); }, Hex(0xFFF0B0, 0.85f - K * 0.22f), 0.004f);
		}
		HDLightning(P, Mouth + Out * 0.06f, Mouth + Out * 0.36f + Side * 0.1f, 1641, 0.006f);
		HDLightning(P, Mouth + Out * 0.06f, Mouth + Out * 0.3f - Side * 0.14f, 1642, 0.005f);
		const FWhorlShell S = HDWhorlShell(P, Apex, Base, 0.17f, 8, Hex(0xF0DCC0), Hex(0x8A4A28), Hex(0xF89048), Hex(0xC0401A), 1643);
		// the triton's pattern: dark crescents scalloped along each whorl, and pale teeth across the outer lip
		{
			FClip Clip(P, [&S](FV Q) { return S.Shape(Q); });
			for (int32 I = 0; I < S.Centres.Num(); ++I)
			{
				for (int32 K = -2; K <= 2; ++K)
				{
					const FV At = S.Centres[I] + S.N * (S.Radii[I] * K * 0.38f) + S.D * (S.Radii[I] * 0.1f);
					const float Rr = S.Radii[I] * 0.22f;
					P.Mark([At, Rr, &S](FV Q) { return FMath::Max(SdCircle(Q, At, Rr), -SdCircle(Q, At - S.D * Rr * 0.5f, Rr * 0.9f)); }, Hex(0x5A2A14, 0.7f), 0.003f);
				}
			}
		}
	}

	void HDItemMurexOfEndings(FPainter& P)
	{
		// a dark venus-comb murex: frilled spines curling off three varices, a long spined canal, a pink mouth
		const FV Apex(0.5f, 0.1f), Base(0.5f, 0.6f);
		const FLinearColor Shell = Hex(0x6A3060), Dark = Hex(0x240A22);
		// the long siphonal canal running on below the body whorl
		const TArray<FV> Canal = Smooth({ FV(0.5f, 0.56f), FV(0.49f, 0.74f), FV(0.5f, 0.94f) });
		// spines first, so their roots tuck under the shell
		const auto Spine = [&P, Shell, Dark](FV Root, FV Dir, float Len, float W)
		{
			const FV Curl(-Dir.Y, Dir.X);
			const TArray<FV> Path = Smooth({ Root, Root + Dir * Len * 0.55f + Curl * Len * 0.06f, Root + Dir * Len - Curl * Len * 0.08f });
			{
				FWith With(P);
				P.Look.Dome = W;
				P.Look.Spec = 0.7f;
				P.Fill([Path, W](FV Q) { return SdRibbon(Q, Path, W, 0.002f); }, Tone(Shell, 0.85f));
			}
			P.Mark([Path, W](FV Q) { return SdRibbon(Q, Path, W * 0.25f, 0.001f); }, Alpha(Dark, 0.7f));
			P.Mark([Path, W, Curl](FV Q) { TArray<FV> Lit; for (const FV& V : Path) Lit.Add(V + Curl * W * 0.45f); return SdRibbon(Q, Lit, W * 0.18f, 0.001f); }, Hex(0xE8C0E8, 0.35f));
		};
		const FWhorlShell G = HDWhorlGeom(Apex, Base, 0.17f, 5);
		for (int32 I = 1; I < G.Centres.Num(); ++I)
		{
			for (const float Sgn : { -1.0f, 1.0f })
			{
				const FV Root = G.Centres[I] + G.N * (Sgn * G.Radii[I] * 0.75f);
				const FV Dir = Spin(G.N * Sgn, Sgn * -18.0f * 0.0f) - G.D * 0.35f;
				Spine(Root, Dir.GetSafeNormal(), 0.05f + G.Radii[I] * 0.55f, 0.012f + G.Radii[I] * 0.1f);
				if (I == G.Centres.Num() - 1)
				{
					Spine(Root + G.D * 0.05f, (G.N * Sgn + G.D * 0.1f).GetSafeNormal(), 0.12f, 0.018f);
					Spine(Root + G.D * 0.1f - G.N * Sgn * 0.02f, (G.N * Sgn + G.D * 0.5f).GetSafeNormal(), 0.1f, 0.015f);
				}
			}
		}
		for (int32 K = 0; K < 5; ++K)
		{
			FV Pos, Tan;
			Along(Canal, 0.2f + K * 0.17f, Pos, Tan);
			for (const float Sgn : { -1.0f, 1.0f })
			{
				Spine(Pos + FV(Sgn * 0.012f, 0.0f), Spin(FV(Sgn, 0.0f), -Sgn * 35.0f), 0.08f - K * 0.01f, 0.01f);
			}
		}
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Spec = 0.7f;
			P.Fill([Canal](FV Q) { return SdRibbon(Q, Canal, 0.035f, 0.01f); }, Shell);
		}
		P.Mark([Canal](FV Q) { return SdRibbon(Q, Canal, 0.008f, 0.002f); }, Hex(0xF0A0C8, 0.7f));
		HDWhorlShell(P, Apex, Base, 0.17f, 5, Shell, Dark, Hex(0xF4B0D0), Hex(0xC03A80), 1651);
		// a faint ominous sheen
		P.Glow([Canal](FV Q) { return SdRibbon(Q, Canal, 0.01f, 0.002f); }, Hex(0xC060C0), 0.04f, 0.3f);
	}

	void HDItemHeartOfTheLeviathan(FPainter& P)
	{
		// an enormous heart still beating: thick vessels rising from its crown, blue veins and red arteries over its muscle, crusted with barnacles
		const TArray<FV> Outline = HDHeartOutline(FV(0.5f, 0.56f), 0.66f, 8.0f);
		const auto Aorta = [](FV Q) { return SMin(SdRibbon(Q, Smooth({ FV(0.47f, 0.36f), FV(0.46f, 0.2f), FV(0.37f, 0.13f), FV(0.27f, 0.18f), FV(0.24f, 0.26f) }), 0.058f, 0.045f), FMath::Min(SdCapsule(Q, FV(0.32f, 0.14f), FV(0.3f, 0.06f), 0.014f), FMath::Min(SdCapsule(Q, FV(0.38f, 0.12f), FV(0.39f, 0.04f), 0.015f), SdCapsule(Q, FV(0.44f, 0.15f), FV(0.47f, 0.07f), 0.013f))), 0.015f); };
		const auto Pulm = [](FV Q) { return SdRibbon(Q, Smooth({ FV(0.58f, 0.38f), FV(0.62f, 0.26f), FV(0.7f, 0.21f) }), 0.05f, 0.042f); };
		const auto Heart = [&Outline, &Aorta, &Pulm](FV Q) { return SMin(SdPoly(Q, Outline), SMin(Aorta(Q), Pulm(Q), 0.02f), 0.05f); };
		P.Glow(Heart, Hex(0xE02040), 0.1f, 0.6f);
		{
			FWith With(P);
			P.Look.Dome = 0.3f;
			P.Look.Spec = 0.8f;
			P.Look.Grain = 0.15f;
			P.Look.GrainFreq = 40.0f;
			P.Fill(Heart, Hex(0xA8182C));
		}
		{
			FClip Clip(P, Heart);
			// the vessels' pale walls and their cut ends
			P.Mark([&Aorta](FV Q) { return Aorta(Q) + 0.01f; }, Hex(0xE07080, 0.5f), 0.02f);
			P.Mark([&Pulm](FV Q) { return Pulm(Q) + 0.01f; }, Hex(0x6A5AA8, 0.5f), 0.02f);
			// muscle fibres sweeping round the ventricles
			for (int32 K = 0; K < 9; ++K)
			{
				P.MLine(Smooth({ FV(0.2f + K * 0.02f, 0.4f + K * 0.035f), FV(0.42f, 0.5f + K * 0.04f), FV(0.62f + K * 0.01f, 0.44f + K * 0.04f), FV(0.8f, 0.36f + K * 0.035f) }), 0.003f, Hex(0x5A0814, 0.3f));
			}
			// fat along the groove, then the coronary vessels: a red artery and a blue vein branching down the face
			P.Mark([](FV Q) { return SdCapsule(Q, FV(0.28f, 0.4f), FV(0.72f, 0.36f), 0.03f) + Noise2(Q * 30.0f) * 0.02f; }, Hex(0xF0D890, 0.5f), 0.02f);
			const auto Vessel = [&P](const TArray<FV>& Path, float W, const FLinearColor& Col)
			{
				{
					FWith With(P);
					P.Look.Dome = W;
					P.Look.Spec = 0.7f;
					P.Look.Ink = 0.004f;
					P.Fill([Path, W](FV Q) { return SdRibbon(Q, Path, W, W * 0.35f); }, Col);
				}
			};
			Vessel(Smooth({ FV(0.46f, 0.38f), FV(0.5f, 0.52f), FV(0.47f, 0.68f), FV(0.5f, 0.8f) }), 0.014f, Hex(0xE03040));
			Vessel(Smooth({ FV(0.49f, 0.5f), FV(0.6f, 0.58f), FV(0.66f, 0.68f) }), 0.009f, Hex(0xE03040));
			Vessel(Smooth({ FV(0.48f, 0.62f), FV(0.38f, 0.68f), FV(0.34f, 0.6f) }), 0.007f, Hex(0xE03040));
			Vessel(Smooth({ FV(0.62f, 0.4f), FV(0.66f, 0.5f), FV(0.72f, 0.56f) }), 0.011f, Hex(0x4A5AB8));
			Vessel(Smooth({ FV(0.66f, 0.48f), FV(0.58f, 0.52f) }), 0.006f, Hex(0x4A5AB8));
			P.MEllipse(FV(0.34f, 0.42f), 0.08f, 0.05f, Hex(0xFFFFFF, 0.3f), 0.05f);
			P.MEllipse(FV(0.31f, 0.4f), 0.025f, 0.014f, Hex(0xFFFFFF, 0.8f));
		}
		// the open ends of the vessels
		for (const FVector E : { FVector(0.24f, 0.27f, 0.045f), FVector(0.71f, 0.205f, 0.042f), FVector(0.3f, 0.06f, 0.014f), FVector(0.39f, 0.04f, 0.015f), FVector(0.47f, 0.07f, 0.013f) })
		{
			const FV At(E.X, E.Y);
			P.Fill([At, E](FV Q) { return SdEllipse(Q, At, E.Z * 0.95f, E.Z * 0.55f); }, Hex(0xE88A90));
			P.Mark([At, E](FV Q) { return SdEllipse(Q, At + FV(0.0f, 0.004f), E.Z * 0.6f, E.Z * 0.3f); }, Hex(0x3A0410));
		}
		// barnacles crusted on, as on an old whale
		HDBarnacle(P, FV(0.7f, 0.62f), 0.035f);
		HDBarnacle(P, FV(0.76f, 0.54f), 0.025f);
		HDBarnacle(P, FV(0.64f, 0.7f), 0.022f);
	}

	void HDItemCrownOfBrine(FPainter& P)
	{
		// a sea-king's gold crown in three-quarter view: shell-fluted points tipped with pearls, gems in bezels, kelp draped over it, brine dripping
		const float Cx = 0.5f, Yc = 0.66f, Rx = 0.36f, Ry = 0.1f, H = 0.15f;
		const FLinearColor Gold0 = Hex(0xE8B040), GoldDark = Hex(0x8A5A12);
		const auto Ell = [Cx, Rx](float X) { const float L = (X - Cx) / Rx; return FMath::Sqrt(FMath::Max(0.0f, 1.0f - L * L)); };
		const auto Band = [Cx, Yc, Rx, Ry, H, Ell](FV Q, float Sgn)
		{
			// Sgn 1: the near half of the band; -1: the far half seen from inside
			const float S = Ell(Q.X) * Sgn;
			return FMath::Max(FMath::Abs(Q.X - Cx) - Rx, FMath::Max((Yc - H + Ry * S) - Q.Y, Q.Y - (Yc + Ry * S)));
		};
		const auto Point = [Cx, Yc, Ry, H, Ell](FV Q, float X, float Sgn, float Height, float W)
		{
			const float BaseY = Yc - H + Ry * Ell(X) * Sgn;
			const FV L = Q - FV(X, BaseY);
			const float T = FMath::Clamp(-L.Y / Height, 0.0f, 1.0f);
			const float Half = W * FMath::Pow(1.0f - T, 0.8f) * (0.75f + 0.45f * FMath::Sin(FMath::Min(T * 1.6f, 1.0f) * PI)) + 0.006f;
			return FMath::Max(FMath::Abs(L.X) - Half, FMath::Max(L.Y - 0.01f, -L.Y - Height));
		};
		// the far side: inner band and points, in shadow, and the velvet cap
		P.Fill([&Band](FV Q) { return Band(Q, -1.0f); }, Tone(Gold0, 0.55f));
		for (const float X : { 0.32f, 0.5f, 0.68f })
		{
			P.Fill([&Point, X](FV Q) { return Point(Q, X, -1.0f, 0.16f, 0.05f); }, Tone(Gold0, 0.6f));
			P.Ball(FV(X, Yc - H - Ry * Ell(X) - 0.16f), 0.022f, Hex(0xC8C0D0), 0.6f);
		}
		P.Fill([Cx, Yc, Rx, Ry, H](FV Q) { return SdEllipse(Q, FV(Cx, Yc - H - 0.01f), Rx * 0.92f, Ry * 0.85f); }, Hex(0x6A1028));
		P.Mark([Cx, Yc, Rx, Ry, H](FV Q) { return SdEllipse(Q, FV(Cx - 0.05f, Yc - H - 0.03f), Rx * 0.5f, Ry * 0.4f); }, Hex(0xC04060, 0.4f), 0.03f);
		// the near band, then its points
		const auto Near = [&Band, &Point](FV Q)
		{
			float D = Band(Q, 1.0f);
			for (const float X : { 0.2f, 0.35f, 0.5f, 0.65f, 0.8f })
			{
				const float Hgt = X == 0.5f ? 0.26f : (X == 0.35f || X == 0.65f ? 0.2f : 0.15f);
				D = SMin(D, Point(Q, X, 1.0f, Hgt, 0.06f), 0.012f);
			}
			return D;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Spec = 0.95f;
			P.Fill(Near, Gold0);
		}
		{
			FClip Clip(P, Near);
			// flutes up each point like a scallop's ribs
			for (const float X : { 0.2f, 0.35f, 0.5f, 0.65f, 0.8f })
			{
				const float BaseY = Yc - H + Ry * Ell(X);
				for (const float Off : { -0.022f, 0.0f, 0.022f })
				{
					P.MLine({ FV(X + Off, BaseY), FV(X + Off * 0.2f, BaseY - 0.16f) }, 0.003f, Alpha(GoldDark, 0.55f));
					P.MLine({ FV(X + Off - 0.006f, BaseY), FV(X + Off * 0.2f - 0.004f, BaseY - 0.16f) }, 0.002f, Hex(0xFFF4C0, 0.6f));
				}
			}
			// rims along the band's edges and a wave engraved between them
			P.Mark([&Band](FV Q) { return FMath::Abs(Band(Q, 1.0f)) - 0.006f; }, Alpha(GoldDark, 0.6f));
			P.Mark([Cx, Yc, H, Ry, Ell](FV Q) { const float Mid = Yc - H * 0.5f + Ry * Ell(Q.X); return FMath::Abs(Q.Y - Mid - FMath::Sin(Q.X * 60.0f) * 0.012f) - 0.003f; }, Alpha(GoldDark, 0.7f));
			P.Mark([Cx, Yc, H, Ry, Ell](FV Q) { const float Mid = Yc - H * 0.5f + Ry * Ell(Q.X); return FMath::Abs(Q.Y - Mid - FMath::Sin(Q.X * 60.0f) * 0.012f + 0.005f) - 0.0015f; }, Hex(0xFFF4C0, 0.6f));
			P.MEllipse(FV(0.36f, Yc - 0.06f), 0.1f, 0.03f, Hex(0xFFFFFF, 0.35f), 0.03f);
		}
		// pearls on the points, gems in their bezels on the band
		for (const float X : { 0.2f, 0.35f, 0.5f, 0.65f, 0.8f })
		{
			const float Hgt = X == 0.5f ? 0.26f : (X == 0.35f || X == 0.65f ? 0.2f : 0.15f);
			PearlBall(P, FV(X, Yc - H + Ry * Ell(X) - Hgt - 0.01f), 0.028f);
		}
		for (const FVector G : { FVector(0.5f, 0.032f, 0.0f), FVector(0.31f, 0.024f, 1.0f), FVector(0.69f, 0.024f, 1.0f) })
		{
			const FV At(G.X, Yc - H * 0.5f + Ry * Ell(G.X) + 0.005f);
			P.Ball(At, G.Y + 0.008f, Tone(Gold0, 0.8f), 0.8f);
			P.Ball(At, G.Y, G.Z > 0.5f ? Hex(0x2A5AD8) : Hex(0x18A890), 1.0f);
			P.Mark([At, G](FV Q) { return SdPoly(Q, { At + FV(-G.Y * 0.5f, -G.Y * 0.2f), At + FV(0.0f, -G.Y * 0.7f), At + FV(G.Y * 0.5f, -G.Y * 0.2f), At + FV(0.0f, G.Y * 0.5f) }); }, Hex(0xFFFFFF, 0.25f));
		}
		// a crust of white sea salt dried along the lower rim
		P.Mark([Yc, Ry, Ell](FV Q) { const float Rim = Yc + Ry * Ell(Q.X); return FMath::Max(FMath::Abs(Q.Y - Rim + 0.012f) - 0.014f + Noise2(Q * 45.0f) * 0.02f, FMath::Abs(Q.X - 0.5f) - 0.33f); }, Hex(0xF4FAFF, 0.85f));
		Speckle(P, FV(0.16f, Yc + 0.02f), FV(0.84f, Yc + 0.1f), 50, 0.002f, 0.005f, Hex(0xFFFFFF, 0.9f), 1661);
		// brine running off the bottom rim and dripping
		for (const float X : { 0.27f, 0.44f, 0.6f })
		{
			const FV Edge(X, Yc + Ry * Ell(X));
			HDDrip(P, Edge + FV(0.0f, -0.012f), 0.06f + (X == 0.44f ? 0.03f : 0.0f), 0.024f, Hex(0x7ACCF0), 0.0f, 0.009f);
		}
	}

	/** A pearl of any body colour, optionally lumpy (baroque): soft orient colours, nacre rings, a window highlight */
	void HDPearl(FPainter& P, FV C, float R, const FLinearColor& Body, const FLinearColor& Shade, float Lumpy = 0.0f, int32 Seed = 0)
	{
		const auto Shape = [C, R, Lumpy, Seed](FV Q)
		{
			const FV L = Q - C;
			const float A = FMath::Atan2(L.Y, L.X);
			const float Bump = Lumpy * R * (FMath::Sin(A * 3.0f + Seed) * 0.5f + FMath::Sin(A * 5.0f + Seed * 2.0f) * 0.3f + FMath::Sin(A * 2.0f) * 0.4f);
			return L.Size() - R - Bump;
		};
		{
			FWith With(P);
			P.Look.Dome = R;
			P.Look.Spec = 0.4f;
			P.Fill(Shape, Body);
		}
		FClip Clip(P, Shape);
		P.MEllipse(C + FV(R * 0.34f, R * 0.46f), R * 0.75f, R * 0.5f, Alpha(Shade, 0.55f), R * 0.45f);
		P.MEllipse(C + FV(-R * 0.5f, R * 0.25f), R * 0.36f, R * 0.5f, Hex(0xFFB8D8, 0.3f), R * 0.3f);
		P.MEllipse(C + FV(R * 0.5f, -R * 0.2f), R * 0.32f, R * 0.46f, Hex(0x9AF0E0, 0.28f), R * 0.3f);
		for (int32 K = 1; K < 7; ++K)
		{
			P.Mark([C, R, K](FV Q) { return FMath::Abs(SdEllipse(Q, C + FV(R * 0.18f, R * 0.22f), R * (0.16f * K), R * (0.14f * K))) - R * 0.006f; }, Hex(0xFFFFFF, 0.05f));
		}
		if (Lumpy > 0.0f)
		{
			// the lumps each catch their own little light
			for (int32 K = 0; K < 4; ++K)
			{
				const FV At = C + Spin(FV(R * 0.55f, 0.0f), 200.0f + K * 50.0f + Seed * 10.0f);
				P.MEllipse(At, R * 0.12f, R * 0.08f, Hex(0xFFFFFF, 0.35f), R * 0.08f);
			}
		}
		P.MEllipse(C + FV(-R * 0.32f, -R * 0.4f), R * 0.3f, R * 0.18f, Hex(0xFFFFFF, 0.55f), R * 0.12f);
		P.MEllipse(C + FV(-R * 0.36f, -R * 0.44f), R * 0.12f, R * 0.07f, Hex(0xFFFFFF, 0.95f));
		P.Mark([C, R](FV Q) { return SdArc(Q, C, R * 0.82f, R * 0.08f, 20.0f, 95.0f); }, Hex(0xFFFFFF, 0.45f), R * 0.06f);
	}

	void HDItemLanternPearl(FPainter& P)
	{
		// a deep-sea anglerfish in profile, jaws agape, its lure arched out in front holding a pearl that glows like a lantern
		const FLinearColor Skin = Hex(0x3E3846), SkinLit = Hex(0x8A7A80), Fin = Hex(0x5A4E5E);
		const FV Esca(0.84f, 0.2f);
		P.Glow([Esca](FV Q) { return SdCircle(Q, Esca, 0.06f); }, Hex(0xFFE890), 0.16f, 0.7f);
		// tail and fins behind the body
		HDFin(P, FV(0.2f, 0.63f), { FV(0.06f, 0.48f), FV(0.03f, 0.58f), FV(0.03f, 0.68f), FV(0.06f, 0.8f) }, Fin, Hex(0x1A141E, 0.7f), 0.012f);
		HDFin(P, FV(0.34f, 0.44f), { FV(0.26f, 0.38f), FV(0.3f, 0.34f), FV(0.38f, 0.37f) }, Fin, Hex(0x1A141E, 0.7f), 0.01f);
		HDFin(P, FV(0.34f, 0.74f), { FV(0.26f, 0.8f), FV(0.32f, 0.84f), FV(0.4f, 0.8f) }, Fin, Hex(0x1A141E, 0.7f), 0.01f);
		// the mouth's inside, seen through the gape
		const TArray<FV> Gape = Smooth({ FV(0.86f, 0.5f), FV(0.74f, 0.54f), FV(0.64f, 0.6f), FV(0.74f, 0.63f), FV(0.9f, 0.62f) }, true, 4);
		P.Fill([&Gape](FV Q) { return SdPoly(Q, Gape) - 0.006f; }, Hex(0x1A0812));
		P.Mark([](FV Q) { return SdEllipse(Q, FV(0.74f, 0.58f), 0.06f, 0.025f); }, Hex(0x6A1A30, 0.6f), 0.02f);
		// the body: a huge round head and gut tapering to a slim tail, the lower jaw jutting past the upper
		const TArray<FV> Outline = Smooth({ FV(0.18f, 0.6f), FV(0.28f, 0.48f), FV(0.42f, 0.38f), FV(0.56f, 0.34f), FV(0.68f, 0.38f), FV(0.78f, 0.44f), FV(0.86f, 0.5f),
			FV(0.74f, 0.54f), FV(0.64f, 0.6f), FV(0.74f, 0.63f), FV(0.9f, 0.62f), FV(0.86f, 0.7f), FV(0.76f, 0.78f), FV(0.58f, 0.82f), FV(0.42f, 0.78f), FV(0.28f, 0.7f), FV(0.18f, 0.66f) }, true, 5);
		const auto Body = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		P.Mark([&Body](FV Q) { return Body(Q - FV(0.012f, 0.02f)); }, Hex(0x06121A, 0.35f), 0.03f);
		{
			FWith With(P);
			P.Look.Dome = 0.16f;
			P.Look.Spec = 0.45f;
			P.Look.Grain = 0.1f;
			P.Fill(Body, Skin);
		}
		{
			FClip Clip(P, Body);
			// lure-light falling on the brow and snout, a paler belly, pitted skin and a lateral line of sensory pores
			P.MEllipse(FV(0.74f, 0.4f), 0.16f, 0.08f, Alpha(Hex(0xE8C880), 0.4f), 0.08f);
			P.MEllipse(FV(0.56f, 0.78f), 0.2f, 0.05f, Alpha(SkinLit, 0.35f), 0.05f);
			Pits(P, FV(0.18f, 0.34f), FV(0.9f, 0.82f), 40, 0.005f, Hex(0x14101A, 0.7f), 1671);
			Speckle(P, FV(0.18f, 0.34f), FV(0.9f, 0.82f), 60, 0.002f, 0.004f, Alpha(SkinLit, 0.6f), 1672);
			const TArray<FV> Lateral = Smooth({ FV(0.22f, 0.62f), FV(0.4f, 0.56f), FV(0.56f, 0.5f), FV(0.66f, 0.48f) });
			for (int32 K = 0; K < 10; ++K)
			{
				FV Pos, Tan;
				Along(Lateral, K / 9.0f, Pos, Tan);
				P.MCircle(Pos, 0.005f, Hex(0xD8D0B8, 0.6f));
			}
			// the gill slit and the lips of the gape
			P.MLine(Smooth({ FV(0.5f, 0.56f), FV(0.48f, 0.64f), FV(0.5f, 0.72f) }), 0.004f, Hex(0x14101A, 0.8f));
			P.Mark([&Gape](FV Q) { return FMath::Abs(SdPoly(Q, Gape)) - 0.006f; }, Alpha(Hex(0x8A5A68), 0.8f));
		}
		// needle teeth raking inward from both jaws
		for (int32 K = 0; K < 6; ++K)
		{
			const float T = K / 5.0f;
			const FV Up = FMath::Lerp(FV(0.84f, 0.51f), FV(0.68f, 0.575f), T);
			const FV Dn = FMath::Lerp(FV(0.88f, 0.615f), FV(0.7f, 0.62f), T);
			const float L = 0.06f - T * 0.035f;
			P.Fill([Up, L](FV Q) { return SdRibbon(Q, Smooth({ Up, Up + FV(-0.01f, L * 0.6f), Up + FV(-0.022f, L) }), 0.006f, 0.0008f); }, Hex(0xEDE8DC, 0.95f));
			P.Fill([Dn, L](FV Q) { return SdRibbon(Q, Smooth({ Dn, Dn + FV(-0.01f, -L * 0.6f), Dn + FV(-0.022f, -L) }), 0.006f, 0.0008f); }, Hex(0xEDE8DC, 0.95f));
		}
		// a small, pale, glassy eye
		P.Ball(FV(0.68f, 0.45f), 0.022f, Hex(0xC8C8B0), 0.8f);
		P.MCircle(FV(0.686f, 0.45f), 0.01f, Hex(0x0A0A10));
		P.MCircle(FV(0.68f, 0.443f), 0.004f, Hex(0xFFFFFF, 0.9f));
		// the lure: a spine growing from the brow, arched forward over the jaws, a fleshy bulb cupping the glowing pearl
		const TArray<FV> Rod = Smooth({ FV(0.58f, 0.36f), FV(0.6f, 0.2f), FV(0.7f, 0.1f), FV(0.8f, 0.1f), FV(0.84f, 0.15f) });
		{
			FWith With(P);
			P.Look.Dome = 0.01f;
			P.Fill([Rod](FV Q) { return SdRibbon(Q, Rod, 0.013f, 0.006f); }, Skin);
		}
		P.Mark([Rod](FV Q) { TArray<FV> R2; for (const FV& V : Rod) R2.Add(V + FV(0.003f, 0.003f)); return SdRibbon(Q, R2, 0.004f, 0.002f); }, Hex(0xE8D090, 0.55f));
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Fill([Esca](FV Q) { return SMin(SdCircle(Q, Esca, 0.065f), SdCapsule(Q, Esca, Esca + FV(0.0f, -0.06f), 0.01f), 0.03f); }, Hex(0xE8C8A0, 0.5f));
		}
		HDPearl(P, Esca, 0.05f, Hex(0xFFF4C8), Hex(0xE8B040));
		P.Glow([Esca](FV Q) { return SdCircle(Q, Esca, 0.045f); }, Hex(0xFFFFE0), 0.04f, 0.5f);
	}

	void HDItemBlackPearl(FPainter& P)
	{
		// a black Tahitian pearl in an open black-lipped oyster, beaded with green venom that runs down over the shell
		const FV Hinge(0.5f, 0.86f);
		const auto Valve = [](FV Q, FV C, float Rx, float Ry, float Rot) { const FV L = Spin(Q - C, -Rot); return SdEllipse(L, FV(0.0f, 0.0f), Rx, Ry) + Noise2(Q * 18.0f) * 0.012f; };
		// the lower valve: rough grey-brown outside, black-lipped mother-of-pearl within
		const auto Lower = [&Valve](FV Q) { return Valve(Q, FV(0.5f, 0.66f), 0.4f, 0.2f, 0.0f); };
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Grain = 0.3f;
			P.Fill(Lower, Hex(0x5A5048));
		}
		{
			FClip Clip(P, Lower);
			P.Mark([&Valve](FV Q) { return Valve(Q, FV(0.5f, 0.64f), 0.36f, 0.16f, 0.0f); }, Hex(0x1A1A22));
			P.Mark([&Valve](FV Q) { return Valve(Q, FV(0.5f, 0.63f), 0.31f, 0.12f, 0.0f); }, Hex(0x8A90A8), 0.04f);
			P.MEllipse(FV(0.42f, 0.6f), 0.14f, 0.04f, Hex(0xC8A0E0, 0.35f), 0.04f);
			P.MEllipse(FV(0.6f, 0.66f), 0.12f, 0.04f, Hex(0x80E0C8, 0.3f), 0.04f);
			for (int32 K = 0; K < 3; ++K)
			{
				P.Mark([&Lower, K](FV Q) { return FMath::Max(FMath::Abs(Lower(Q) + 0.008f + K * 0.009f + Noise2(Q * 40.0f) * 0.004f) - 0.002f, 0.64f - Q.Y); }, Hex(0xA89A88, 0.5f));
			}
		}
		// the upper valve, gaping up behind
		const auto Upper = [&Valve](FV Q) { return Valve(Q, FV(0.5f, 0.42f), 0.38f, 0.14f, 0.0f); };
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Grain = 0.3f;
			P.Fill([&Upper](FV Q) { return FMath::Max(Upper(Q), Q.Y - 0.5f); }, Hex(0x4A423C));
		}
		P.Mark([&Valve](FV Q) { return FMath::Max(Valve(Q, FV(0.5f, 0.44f), 0.33f, 0.1f, 0.0f), Q.Y - 0.5f); }, Hex(0x14141C));
		P.Mark([&Valve](FV Q) { return FMath::Max(Valve(Q, FV(0.5f, 0.45f), 0.28f, 0.07f, 0.0f), Q.Y - 0.5f); }, Hex(0x6A7090), 0.03f);
		// the pearl: near-black with a peacock-green and aubergine orient
		HDPearl(P, FV(0.5f, 0.56f), 0.14f, Hex(0x2A2C38), Hex(0x0A0A12));
		{
			FClip Clip(P, [](FV Q) { return SdCircle(Q, FV(0.5f, 0.56f), 0.14f); });
			P.MEllipse(FV(0.58f, 0.6f), 0.08f, 0.06f, Hex(0x30C090, 0.35f), 0.05f);
			P.MEllipse(FV(0.42f, 0.62f), 0.07f, 0.06f, Hex(0xA040A0, 0.3f), 0.05f);
		}
		// venom: beads clinging to the pearl and a rivulet running off it down the shell lip
		// venom filming the underside of the pearl and dribbling off it in one run
		{
			FClip Clip(P, [](FV Q) { return SdCircle(Q, FV(0.5f, 0.56f), 0.14f); });
			P.Mark([](FV Q) { return FMath::Max(SdCircle(Q, FV(0.5f, 0.56f), 0.14f) + 0.0f, 0.04f - FV::DotProduct(Q - FV(0.5f, 0.56f), FV(0.5f, 0.86f))); }, Hex(0x6AD038, 0.55f), 0.03f);
			P.MArc(FV(0.5f, 0.56f), 0.13f, 0.005f, 30.0f, 70.0f, Hex(0xC8FF90, 0.6f));
		}
		HDDrip(P, FV(0.6f, 0.655f), 0.2f, 0.032f, Hex(0x6AD038), 0.0f, 0.014f);
	}

	void HDItemColdSeepPearl(FPainter& P)
	{
		// a pale, frost-cold pearl caught among white methane-ice crystals on a seep in the sea floor, bubbles streaming up
		HDMound(P, FV(0.5f, 0.84f), 0.4f, 0.1f, Hex(0x5A6068), 1681);
		// bacterial mats, white and orange, round the seep
		P.Mark([](FV Q) { return SdEllipse(Q, FV(0.48f, 0.8f), 0.26f, 0.05f) + Noise2(Q * 30.0f) * 0.02f; }, Hex(0xF0F0E8, 0.7f), 0.01f);
		P.Mark([](FV Q) { return SdEllipse(Q, FV(0.62f, 0.83f), 0.1f, 0.03f) + Noise2(Q * 40.0f) * 0.015f; }, Hex(0xE89A40, 0.6f), 0.01f);
		// ice crystals, blue-white and faceted, clustered round the pearl
		for (const FVector Cr : { FVector(0.3f, 0.82f, -28.0f), FVector(0.38f, 0.8f, -12.0f), FVector(0.66f, 0.8f, 20.0f), FVector(0.72f, 0.84f, 38.0f), FVector(0.56f, 0.8f, 6.0f) })
		{
			HDCrystal(P, FV(Cr.X, Cr.Y), 0.18f + FMath::Abs(Cr.Z) * 0.002f, 0.05f, Cr.Z, Hex(0xD8F4FF));
		}
		HDPearl(P, FV(0.48f, 0.6f), 0.15f, Hex(0xEAF4FA), Hex(0x8AB8D8));
		// a dusting of frost on its shoulder
		{
			FClip Clip(P, [](FV Q) { return SdCircle(Q, FV(0.48f, 0.6f), 0.15f); });
			Speckle(P, FV(0.34f, 0.45f), FV(0.62f, 0.56f), 50, 0.002f, 0.005f, Hex(0xFFFFFF, 0.9f), 1682);
		}
		// bubbles streaming up from the seep
		for (int32 K = 0; K < 6; ++K)
		{
			Bubble(P, FV(0.76f + FMath::Sin(K * 1.7f) * 0.03f, 0.66f - K * 0.09f), 0.012f + K * 0.003f);
		}
		for (int32 K = 0; K < 4; ++K)
		{
			Bubble(P, FV(0.22f + FMath::Sin(K * 2.1f) * 0.02f, 0.62f - K * 0.1f), 0.01f + K * 0.002f);
		}
	}

	void HDItemBaroquePearl(FPainter& P)
	{
		// a big lumpy baroque pearl hung as a pendant: a gold cap with filigree leaves, a bail and a fine chain
		const FV C(0.5f, 0.6f);
		// the chain, looping up out of the frame
		for (const float Sgn : { -1.0f, 1.0f })
		{
			for (int32 K = 0; K < 9; ++K)
			{
				const float T = K / 8.0f;
				const FV At = FV(0.5f + Sgn * (0.02f + T * 0.24f), 0.25f - FMath::Sin(T * PI * 0.5f) * 0.2f);
				P.Mark([At, K](FV Q) { return FMath::Abs(SdEllipse(Q, At, K % 2 ? 0.006f : 0.014f, K % 2 ? 0.014f : 0.008f)) - 0.003f; }, Hex(0xE8B040));
			}
		}
		HDPearl(P, C, 0.22f, Hex(0xF2ECE4), Hex(0xB8A8C8), 0.12f, 3);
		// the cap: gold leaves clasping the top of the pearl
		for (int32 K = -2; K <= 2; ++K)
		{
			const FV Root(0.5f, 0.36f);
			const FV Tip = Root + Spin(FV(0.0f, 0.1f), K * 28.0f);
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Spec = 0.9f;
			P.Fill([Root, Tip](FV Q) { return SdRibbon(Q, Smooth({ Root, (Root + Tip) * 0.5f + FV(0.0f, 0.005f), Tip }), 0.022f, 0.003f); }, Hex(0xE8B040));
			P.Mark([Root, Tip](FV Q) { return SdCapsule(Q, Root, Tip, 0.002f); }, Hex(0x8A5A12, 0.7f));
		}
		P.Ball(FV(0.5f, 0.35f), 0.03f, Hex(0xE8B040), 0.9f);
		// the bail
		{
			FWith With(P);
			P.Look.Spec = 0.9f;
			P.Fill([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.5f, 0.27f), 0.025f, 0.04f)) - 0.008f; }, Hex(0xE8B040));
		}
		Glint(P, FV(0.38f, 0.5f), 0.07f);
	}

	/** A breaking wave curling to the left: its body, a darker trough under the curl, and lacy foam along the crest */
	void HDWave(FPainter& P, const FLinearColor& Water, const FLinearColor& Deep, int32 Seed)
	{
		const TArray<FV> Body = Smooth({ FV(0.08f, 0.9f), FV(0.1f, 0.74f), FV(0.2f, 0.6f), FV(0.36f, 0.46f), FV(0.56f, 0.38f), FV(0.74f, 0.42f), FV(0.82f, 0.52f), FV(0.76f, 0.6f), FV(0.68f, 0.56f), FV(0.62f, 0.6f), FV(0.66f, 0.72f), FV(0.8f, 0.8f), FV(0.92f, 0.82f), FV(0.92f, 0.9f) }, true, 5);
		const auto Wave = [&Body](FV Q) { return SdPoly(Q, Body); };
		{
			FWith With(P);
			P.Look.Dome = 0.12f;
			P.Look.Spec = 0.8f;
			P.Fill(Wave, Water);
		}
		FClip Clip(P, Wave);
		// light through the thin face of the wave, the dark barrel under the curl, streaks running down the face
		P.MEllipse(FV(0.46f, 0.6f), 0.18f, 0.12f, Alpha(Tone(Water, 1.5f), 0.5f), 0.08f);
		P.MEllipse(FV(0.68f, 0.64f), 0.08f, 0.06f, Alpha(Deep, 0.7f), 0.05f);
		// short, broken streaks of foam sliding down the face, curving with the wave
		for (int32 K = 0; K < 9; ++K)
		{
			const float X = 0.22f + K * 0.055f + (Rand01(Seed + 5, K) - 0.5f) * 0.03f;
			const float Y0 = 0.5f + FMath::Abs(X - 0.56f) * 0.4f + Rand01(Seed + 6, K) * 0.04f;
			const float Len = 0.08f + Rand01(Seed + 7, K) * 0.1f;
			const TArray<FV> Streak = Smooth({ FV(X, Y0), FV(X - 0.012f, Y0 + Len * 0.5f), FV(X - 0.035f, Y0 + Len) });
			P.Mark([Streak](FV Q) { return SdRibbon(Q, Streak, 0.004f, 0.001f); }, Hex(0xFFFFFF, 0.16f), 0.004f);
		}
		P.MEllipse(FV(0.5f, 0.9f), 0.5f, 0.08f, Alpha(Deep, 0.6f), 0.06f);
		// foam lace along the crest and the lip of the curl
		const TArray<FV> Crest = Smooth({ FV(0.14f, 0.7f), FV(0.24f, 0.56f), FV(0.38f, 0.46f), FV(0.56f, 0.4f), FV(0.72f, 0.43f), FV(0.8f, 0.52f), FV(0.74f, 0.58f) });
		P.Mark([&Crest](FV Q) { return SdPolyline(Q, Crest, 0.02f) + Noise2(Q * 40.0f) * 0.02f; }, Hex(0xF4FCFF, 0.95f), 0.006f);
		for (int32 K = 0; K < 40; ++K)
		{
			FV Pos, Tan;
			Along(Crest, Rand01(Seed, K), Pos, Tan);
			const FV At = Pos + FV(-Tan.Y, Tan.X) * (0.02f + Rand01(Seed + 1, K) * 0.06f);
			P.Mark([At](FV Q) { return FMath::Abs(SdCircle(Q, At, 0.008f)) - 0.002f; }, Hex(0xFFFFFF, 0.6f));
		}
	}

	void HDItemSeafoamPearl(FPainter& P)
	{
		// a white pearl riding the foam at the very top of a breaking wave, spray flung off around it
		HDWave(P, Hex(0x2A9AC8), Hex(0x0A3A6A), 1691);
		// a cushion of foam heaped under the pearl
		for (int32 K = 0; K < 9; ++K)
		{
			const FV At = FV(0.4f + K * 0.035f, 0.42f + FMath::Abs(K - 4) * 0.006f - (K % 2) * 0.012f);
			P.Ball(At, 0.032f - FMath::Abs(K - 4) * 0.002f, Hex(0xF4FCFF), 0.3f);
		}
		HDPearl(P, FV(0.54f, 0.28f), 0.12f, Hex(0xF4F2EE), Hex(0xA8B8D0));
		for (int32 K = 0; K < 10; ++K)
		{
			const FV At = FV(0.3f + Rand01(1692, K) * 0.5f, 0.1f + Rand01(1693, K) * 0.22f);
			if ((At - FV(0.54f, 0.28f)).Size() < 0.15f)
			{
				continue;
			}
			Bubble(P, At, 0.008f + Rand01(1694, K) * 0.01f);
		}
	}

	void HDItemMoonPearl(FPainter& P)
	{
		// a great pale pearl hanging over the night sea like the full moon, laying a glittering path across the swell
		P.Glow([](FV Q) { return SdCircle(Q, FV(0.5f, 0.38f), 0.2f); }, Hex(0xC8D8FF), 0.12f, 0.5f);
		// the sea: a band of dark swell with moonlit glints running to the horizon
		const auto Sea = [](FV Q) { return FMath::Max(SdEllipse(Q, FV(0.5f, 0.74f), 0.46f, 0.16f), 0.66f - Q.Y + FMath::Sin(Q.X * 30.0f) * 0.006f); };
		{
			FWith With(P);
			P.Look.Dome = 0.06f;
			P.Look.Spec = 0.5f;
			P.Fill(Sea, Hex(0x1A3A60));
		}
		{
			FClip Clip(P, Sea);
			for (int32 K = 0; K < 8; ++K)
			{
				const float Y = 0.69f + K * 0.03f;
				P.MLine(Smooth({ FV(0.06f, Y), FV(0.3f, Y + 0.008f), FV(0.5f, Y - 0.004f), FV(0.7f, Y + 0.006f), FV(0.94f, Y) }), 0.002f, Hex(0x3A6A9A, 0.6f));
			}
			for (int32 K = 0; K < 26; ++K)
			{
				const float Y = 0.68f + Rand01(1701, K) * 0.22f;
				const float X = 0.5f + (Rand01(1702, K) - 0.5f) * (0.06f + (Y - 0.66f) * 0.8f);
				P.MCapsule(FV(X - 0.02f, Y), FV(X + 0.02f, Y), 0.003f, Hex(0xE8F0FF, 0.8f));
			}
		}
		// the pearl moon, its nacre blemishes like the maria and craters
		HDPearl(P, FV(0.5f, 0.38f), 0.2f, Hex(0xE8ECF4), Hex(0x8A98C0));
		{
			FClip Clip(P, [](FV Q) { return SdCircle(Q, FV(0.5f, 0.38f), 0.2f); });
			for (const FVector M : { FVector(0.44f, 0.34f, 0.06f), FVector(0.56f, 0.42f, 0.045f), FVector(0.58f, 0.3f, 0.03f) })
			{
				P.Mark([M](FV Q) { return SdCircle(Q, FV(M.X, M.Y), M.Z) + Noise2(Q * 30.0f) * 0.015f; }, Hex(0x9AA4C0, 0.25f), 0.02f);
			}
			for (const FVector Cr : { FVector(0.4f, 0.44f, 0.016f), FVector(0.6f, 0.46f, 0.012f), FVector(0.5f, 0.26f, 0.01f) })
			{
				P.Mark([Cr](FV Q) { return FMath::Abs(SdCircle(Q, FV(Cr.X, Cr.Y), Cr.Z)) - 0.002f; }, Hex(0x8A94B0, 0.35f));
			}
		}
		// a few stars
		for (int32 K = 0; K < 6; ++K)
		{
			const FV At(0.1f + Rand01(1703, K) * 0.8f, 0.08f + Rand01(1704, K) * 0.4f);
			if ((At - FV(0.5f, 0.38f)).Size() < 0.27f)
			{
				continue;
			}
			HDSpark(P, At, 0.025f + Rand01(1705, K) * 0.02f, Hex(0xE8F0FF));
		}
	}

	void HDItemSirensTear(FPainter& P)
	{
		// a siren's tear turned to aquamarine, set in a little gold scallop and hung on a fine chain
		const FV C(0.5f, 0.6f);
		const TArray<FV> Gem = Smooth({ FV(0.5f, 0.32f), FV(0.56f, 0.44f), FV(0.64f, 0.56f), FV(0.66f, 0.68f), FV(0.6f, 0.8f), FV(0.5f, 0.84f), FV(0.4f, 0.8f), FV(0.34f, 0.68f), FV(0.36f, 0.56f), FV(0.44f, 0.44f) }, true, 6);
		const auto Shape = [&Gem](FV Q) { return SdPoly(Q, Gem); };
		P.Glow(Shape, Hex(0x60D8F0), 0.06f, 0.4f);
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Spec = 1.0f;
			P.Fill(Shape, Hex(0x48C0D8));
		}
		{
			FClip Clip(P, Shape);
			// facets: a fan of planes radiating from the table, each lit or shadowed
			const FV Table(0.5f, 0.66f);
			for (int32 K = 0; K < 10; ++K)
			{
				const FV A = Gem[K * Gem.Num() / 10], B = Gem[((K + 1) * Gem.Num() / 10) % Gem.Num()];
				P.MPoly({ Table, A, B }, K % 3 == 0 ? Hex(0xFFFFFF, 0.25f) : (K % 3 == 1 ? Hex(0x0A5A7A, 0.3f) : Hex(0x7AE8FF, 0.2f)));
				P.MLine({ Table, A }, 0.0025f, Hex(0xE0FFFF, 0.5f));
			}
			P.MEllipse(Table, 0.07f, 0.05f, Hex(0xB0F4FF, 0.45f));
			P.Mark([Table](FV Q) { return FMath::Abs(SdEllipse(Q, Table, 0.07f, 0.05f)) - 0.002f; }, Hex(0xFFFFFF, 0.7f));
			P.MEllipse(FV(0.42f, 0.58f), 0.025f, 0.06f, Hex(0xFFFFFF, 0.6f), 0.01f);
		}
		// the scallop-shell cap gripping the top of the tear, a bail and the chain
		{
			const FV Hinge(0.5f, 0.37f);
			const auto Cap = [Hinge](FV Q) { const FV L = Q - Hinge; const float A = FMath::Atan2(L.X, L.Y); return FMath::Max(L.Size() - 0.08f + FMath::Abs(FMath::Sin(A * 7.0f)) * 0.006f, (FMath::Abs(A) - 1.0f) * L.Size()); };
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Spec = 0.95f;
			P.Fill([Cap, Hinge](FV Q) { return Cap(Q + FV(0.0f, 0.04f)); }, Hex(0xE8B040));
			for (int32 K = -3; K <= 3; ++K)
			{
				const FV Dir = Spin(FV(0.0f, 1.0f), K * 15.0f);
				P.MCapsule(Hinge - FV(0.0f, 0.04f) + Dir * 0.015f, Hinge - FV(0.0f, 0.04f) + Dir * 0.075f, 0.002f, Hex(0x8A5A12, 0.7f));
			}
		}
		P.Fill([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.5f, 0.27f), 0.022f, 0.035f)) - 0.007f; }, Hex(0xE8B040));
		for (const float Sgn : { -1.0f, 1.0f })
		{
			for (int32 K = 0; K < 8; ++K)
			{
				const float T = K / 7.0f;
				const FV At = FV(0.5f + Sgn * (0.02f + T * 0.26f), 0.22f - FMath::Sin(T * PI * 0.5f) * 0.16f);
				P.Mark([At, K](FV Q) { return FMath::Abs(SdEllipse(Q, At, K % 2 ? 0.006f : 0.013f, K % 2 ? 0.013f : 0.007f)) - 0.003f; }, Hex(0xE8B040));
			}
		}
		Glint(P, FV(0.6f, 0.5f), 0.07f);
	}

	void HDItemRedTidePearl(FPainter& P)
	{
		// a blood-red pearl bobbing in a red-tide bloom: rust-red water streaked with algae, rings spreading round it
		const auto Pool = [](FV Q) { return SdEllipse(Q, FV(0.5f, 0.7f), 0.44f, 0.2f) + Noise2(Q * 8.0f) * 0.02f; };
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Spec = 0.6f;
			P.Fill(Pool, Hex(0x8A1A1A));
		}
		{
			FClip Clip(P, Pool);
			// swirls of bloom, darker and brighter
			for (int32 K = 0; K < 6; ++K)
			{
				const float Y = 0.56f + K * 0.05f;
				P.Mark([Y, K](FV Q) { return FMath::Abs(Q.Y - Y - FMath::Sin(Q.X * 14.0f + K) * 0.02f) - 0.008f + Noise2(Q * 30.0f) * 0.01f; }, K % 2 ? Hex(0xC83A20, 0.5f) : Hex(0x4A0A0A, 0.4f), 0.008f);
			}
			Speckle(P, FV(0.06f, 0.5f), FV(0.94f, 0.9f), 120, 0.0015f, 0.004f, Hex(0xF07A40, 0.7f), 1711);
			// rings spreading out round the pearl, in perspective
			for (int32 K = 1; K <= 3; ++K)
			{
				P.Mark([K](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.5f, 0.68f), 0.1f + K * 0.08f, (0.1f + K * 0.08f) * 0.35f)) - 0.004f; }, Hex(0xFFB090, 0.55f - K * 0.12f), 0.004f);
			}
		}
		// the pearl, half sunk, its reflection broken in the water below it
		P.Mark([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.74f), 0.13f, 0.04f) + FMath::Abs(FMath::Sin(Q.Y * 200.0f)) * 0.01f; }, Hex(0xE84040, 0.5f));
		{
			FClip Clip(P, [](FV Q) { return Q.Y - 0.69f - FMath::Sin(Q.X * 40.0f) * 0.004f; });
			HDPearl(P, FV(0.5f, 0.54f), 0.17f, Hex(0xD02828), Hex(0x5A0A10));
		}
		P.Mark([](FV Q) { return FMath::Max(FMath::Abs(Q.Y - 0.69f - FMath::Sin(Q.X * 40.0f) * 0.004f) - 0.004f, FMath::Abs(Q.X - 0.5f) - 0.16f); }, Hex(0xFFC0A0, 0.7f));
		// a single drop running off its crown
		P.Ball(FV(0.58f, 0.42f), 0.016f, Hex(0xFF6060), 1.0f);
	}

	void HDItemPearlOfTheAbyss(FPainter& P)
	{
		// a pearl as black as the deep, a slow violet whirlpool turning inside it, set in a cup of cracked black rock
		const FV C(0.5f, 0.46f);
		const float R = 0.26f;
		P.Glow([C, R](FV Q) { return SdCircle(Q, C, R); }, Hex(0x7A3AD8), 0.1f, 0.6f);
		// the obsidian cup
		const auto Cup = [](FV Q) { return FMath::Max(SdEllipse(Q, FV(0.5f, 0.68f), 0.3f, 0.16f) + Noise2(Q * 12.0f) * 0.03f, 0.6f - Q.Y); };
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Look.Spec = 0.9f;
			P.Fill(Cup, Hex(0x14121C));
		}
		{
			FClip Clip(P, Cup);
			Plates(P, GridSeeds(FV(0.2f, 0.6f), FV(0.8f, 0.84f), 5, 2, 1721), 0.003f, Hex(0x000000, 0.8f), Hex(0x8A7AC0, 0.1f), 0, 0.0f);
			P.MEllipse(FV(0.5f, 0.8f), 0.3f, 0.08f, Hex(0x000000, 0.5f), 0.06f);
		}
		HDPearl(P, C, R, Hex(0x221C30), Hex(0x050308));
		{
			FClip Clip(P, [C, R](FV Q) { return SdCircle(Q, C, R * 0.98f); });
			// the whirlpool: violet arms spiralling into a black eye
			for (int32 Arm = 0; Arm < 3; ++Arm)
			{
				TArray<FV> Path;
				for (int32 K = 0; K <= 30; ++K)
				{
					const float T = K / 30.0f;
					const float A = Arm * 2.0f * PI / 3.0f + T * 3.6f;
					Path.Add(C + FV(FMath::Cos(A), FMath::Sin(A) * 0.8f) * R * (0.9f - T * 0.8f));
				}
				P.Mark([Path](FV Q) { return SdRibbon(Q, Path, 0.004f, 0.022f); }, Hex(0x9A5AF0, 0.55f), 0.012f);
				P.Mark([Path](FV Q) { return SdRibbon(Q, Path, 0.001f, 0.006f); }, Hex(0xE0C8FF, 0.6f), 0.004f);
			}
			P.Mark([C](FV Q) { return SdEllipse(Q, C, 0.035f, 0.028f); }, Hex(0x000000), 0.02f);
			Speckle(P, C - FV(R, R), C + FV(R, R), 40, 0.0015f, 0.004f, Hex(0xD0B8FF, 0.8f), 1722);
			P.MEllipse(C + FV(-R * 0.35f, -R * 0.45f), R * 0.25f, R * 0.12f, Hex(0xFFFFFF, 0.5f), R * 0.1f);
		}
	}

	void HDItemKrakensEye(FPainter& P)
	{
		// one huge kraken eye, close up: a golden iris split by a bar pupil, held in folds of mottled, warty lid
		const FLinearColor Skin = Hex(0xA03A5A), Dark = Hex(0x4A1028);
		const FV E(0.5f, 0.5f);
		// the folds of skin round the socket: an almond of lid, wrinkled in rings
		const auto Lids = [E](FV Q)
		{
			const FV L = Q - E;
			const float Almond = SdEllipse(L, FV(0.0f, 0.0f), 0.44f, 0.3f + 0.04f * (1.0f - FMath::Abs(L.X) / 0.44f));
			return Almond + Noise2(Q * 9.0f) * 0.02f;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.14f;
			P.Look.Spec = 0.6f;
			P.Fill(Lids, Skin);
		}
		{
			FClip Clip(P, Lids);
			for (int32 K = 1; K < 6; ++K)
			{
				P.Mark([E, K](FV Q) { const FV L = Q - E; return FMath::Abs(SdEllipse(L, FV(0.0f, 0.0f), 0.28f + K * 0.035f, 0.2f + K * 0.026f) + Noise2(Q * 22.0f + FV(K * 3.0f, 0.0f)) * 0.012f) - 0.003f; }, Alpha(Dark, 0.5f));
				P.Mark([E, K](FV Q) { const FV L = Q - E + FV(0.0f, 0.006f); return FMath::Abs(SdEllipse(L, FV(0.0f, 0.0f), 0.28f + K * 0.035f, 0.2f + K * 0.026f) + Noise2(Q * 22.0f + FV(K * 3.0f, 0.0f)) * 0.012f) - 0.002f; }, Hex(0xFFD0DC, 0.25f));
			}
			for (int32 K = 0; K < 40; ++K)
			{
				const FV At = E + FV((Rand01(1731, K) - 0.5f) * 0.86f, (Rand01(1732, K) - 0.5f) * 0.64f);
				const float Rr = 0.006f + Rand01(1733, K) * 0.016f;
				P.Mark([At, Rr](FV Q) { return SdCircle(Q, At, Rr) + Noise2(Q * 80.0f) * 0.004f; }, Alpha(Dark, 0.4f), Rr * 0.5f);
			}
			Pits(P, E - FV(0.44f, 0.32f), E + FV(0.44f, 0.32f), 40, 0.004f, Alpha(Dark, 0.6f), 1734);
			// shadow cast into the socket
			P.Mark([E](FV Q) { return SdEllipse(Q, E + FV(0.0f, 0.01f), 0.31f, 0.23f); }, Alpha(Dark, 0.7f), 0.04f);
			P.MEllipse(E + FV(-0.18f, -0.22f), 0.14f, 0.05f, Hex(0xFFFFFF, 0.25f), 0.05f);
		}
		const auto Eye = [E](FV Q) { return SdEllipse(Q, E, 0.27f, 0.2f); };
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.95f;
			P.Fill(Eye, Hex(0xE8B030));
		}
		{
			FClip Clip(P, Eye);
			// the iris: radial fibres, crypts, flecks and a dark limbal ring
			for (int32 K = 0; K < 90; ++K)
			{
				const FV Dir = Spin(FV(1.0f, 0.0f), K * 4.0f + Rand01(1735, K) * 2.0f);
				P.MCapsule(E + Dir * FV(0.07f, 0.05f), E + Dir * FV(0.27f, 0.2f), 0.0022f, K % 3 ? Hex(0x8A4A10, 0.45f) : Hex(0xFFE890, 0.45f));
			}
			for (int32 K = 0; K < 14; ++K)
			{
				const FV At = E + Spin(FV(0.1f + Rand01(1736, K) * 0.12f, 0.0f), K * 26.0f) * FV(1.0f, 0.75f);
				P.MEllipse(At, 0.012f, 0.007f, Hex(0x6A3008, 0.5f), 0.004f);
			}
			P.Mark([E](FV Q) { const FV L = Q - E; return FMath::Abs(SdEllipse(L, FV(0.0f, 0.0f), 0.1f, 0.075f)) - 0.012f; }, Hex(0xFFF0A0, 0.4f), 0.01f);
			P.Mark([E](FV Q) { return FMath::Abs(SdEllipse(Q, E, 0.27f, 0.2f)) - 0.022f; }, Hex(0x4A1A04, 0.7f), 0.02f);
			// the bar pupil, a wide dumbbell slot
			P.Mark([E](FV Q) { const FV L = Q - E; return SMin(SdCapsule(L, FV(-0.11f, 0.0f), FV(0.11f, 0.0f), 0.022f), FMath::Min(SdEllipse(L, FV(-0.12f, 0.0f), 0.04f, 0.05f), SdEllipse(L, FV(0.12f, 0.0f), 0.04f, 0.05f)), 0.03f) + Noise2(Q * 40.0f) * 0.003f; }, Hex(0x0A0408));
			// the heavy upper lid drooping over the top of the eye
			P.Fill([E](FV Q) { return FMath::Max(SdEllipse(Q, E + FV(0.0f, -0.2f), 0.4f, 0.16f), Q.Y - (E.Y - 0.08f + FMath::Sin((Q.X - E.X) * 4.0f) * 0.01f)); }, Skin);
			P.Mark([E](FV Q) { return FMath::Abs(Q.Y - (E.Y - 0.08f + FMath::Sin((Q.X - E.X) * 4.0f) * 0.01f)) - 0.008f; }, Alpha(Dark, 0.8f));
			P.Mark([E](FV Q) { return SdEllipse(Q, E + FV(0.0f, -0.06f), 0.3f, 0.03f); }, Alpha(Dark, 0.4f), 0.03f);
			// wet catchlights and a sheen along the lower rim
			P.MEllipse(E + FV(-0.11f, -0.035f), 0.04f, 0.022f, Hex(0xFFFFFF, 0.92f));
			P.MCircle(E + FV(-0.05f, -0.045f), 0.01f, Hex(0xFFFFFF, 0.8f));
			P.MArc(E, 0.24f, 0.006f, 20.0f, 160.0f, Hex(0xFFFFFF, 0.35f));
		}
	}

	/** Something carved into wood: a dark cut with a lit lower edge, as relief work catches light from the upper left */
	void HDCarve(FPainter& P, TFunctionRef<float(FV)> Cut, float Depth = 0.006f)
	{
		P.Mark([&Cut, Depth](FV Q) { return Cut(Q - FV(-Depth * 0.4f, -Depth)); }, Hex(0xFFF0D8, 0.45f));
		P.Mark(Cut, Hex(0x2A1A0E, 0.85f));
		P.Mark([&Cut, Depth](FV Q) { return FMath::Max(Cut(Q), -Cut(Q - FV(Depth * 0.5f, Depth))); }, Hex(0x0A0604, 0.5f));
	}

	/** A small branching coral growing from Root */
	void HDSmallCoral(FPainter& P, FV Root, float Len, float W, float Angle, int32 Depth, const FLinearColor& Color, int32 Seed)
	{
		TArray<TArray<FV>> Limbs;
		TArray<float> Widths;
		struct FB { FV From; float Len; float W; float Angle; int32 Depth; };
		TArray<FB> Todo = { { Root, Len, W, Angle, Depth } };
		int32 Count = 0;
		while (Todo.Num() > 0)
		{
			const FB B = Todo.Last();
			Todo.RemoveAt(Todo.Num() - 1);
			const FV Dir = Spin(FV(0.0f, -1.0f), B.Angle);
			const FV End = B.From + Dir * B.Len;
			Limbs.Add(Smooth({ B.From, B.From + Dir * B.Len * 0.5f + FV(-Dir.Y, Dir.X) * B.Len * (Rand01(Seed, Count) - 0.5f) * 0.25f, End }));
			Widths.Add(B.W);
			++Count;
			if (B.Depth > 0)
			{
				const float Spread = 24.0f + Rand01(Seed, Count + 40) * 16.0f;
				Todo.Add({ End, B.Len * 0.72f, B.W * 0.74f, B.Angle - Spread, B.Depth - 1 });
				Todo.Add({ End, B.Len * 0.66f, B.W * 0.74f, B.Angle + Spread, B.Depth - 1 });
			}
		}
		const auto Coral = [&Limbs, &Widths](FV Q)
		{
			float D = 1e6f;
			for (int32 I = 0; I < Limbs.Num(); ++I)
			{
				D = SMin(D, SdRibbon(Q, Limbs[I], Widths[I], Widths[I] * 0.75f), Widths[I] * 0.6f);
			}
			return D;
		};
		{
			FWith With(P);
			P.Look.Dome = W;
			P.Look.Spec = 0.4f;
			P.Fill(Coral, Color);
		}
		FClip Clip(P, Coral);
		for (int32 I = 0; I < Limbs.Num(); ++I)
		{
			for (int32 K = 0; K < 4; ++K)
			{
				FV Pos, Tan;
				Along(Limbs[I], (K + 0.5f) / 4.0f, Pos, Tan);
				P.MCircle(Pos + FV(-Tan.Y, Tan.X) * Widths[I] * (K % 2 ? 0.4f : -0.4f), Widths[I] * 0.22f, Alpha(Tone(Color, 1.35f), 0.8f));
			}
		}
	}

	void HDItemDriftwoodTotem(FPainter& P)
	{
		// a totem carved from a sea-bleached driftwood post: three faces cut in relief, a carved bird with spread wings on top, lashed with rope, set in a heap of sand
		const FLinearColor DriftWood = Hex(0xB0A08A), DriftWoodDark = Hex(0x5A4A38);
		HDMound(P, FV(0.5f, 0.88f), 0.36f, 0.07f, Hex(0xD8C090), 1741, false);
		// the bird's wings: carved planks, fanned out behind its head
		for (const float Sgn : { -1.0f, 1.0f })
		{
			const TArray<FV> Wing = Smooth({ FV(0.5f + Sgn * 0.08f, 0.3f), FV(0.5f + Sgn * 0.22f, 0.18f), FV(0.5f + Sgn * 0.4f, 0.12f), FV(0.5f + Sgn * 0.36f, 0.22f), FV(0.5f + Sgn * 0.24f, 0.32f), FV(0.5f + Sgn * 0.1f, 0.36f) }, true, 5);
			const auto Shape = [Wing](FV Q) { return SdPoly(Q, Wing) + Noise2(Q * FV(30.0f, 8.0f)) * 0.006f; };
			{
				FWith With(P);
				P.Look.Dome = 0.03f;
				P.Look.Grain = 0.2f;
				P.Fill(Shape, Tone(DriftWood, 0.92f));
			}
			FClip Clip(P, Shape);
			for (int32 K = 0; K < 4; ++K)
			{
				const FV A = FV(0.5f + Sgn * (0.12f + K * 0.06f), 0.33f - K * 0.03f), B = FV(0.5f + Sgn * (0.2f + K * 0.06f), 0.18f - K * 0.012f);
				HDCarve(P, [A, B](FV Q) { return SdCapsule(Q, A, B, 0.004f); }, 0.004f);
			}
		}
		// the post: knotty, a little crooked, weathered silver-brown
		const auto Post = [](FV Q) { const float Lean = (Q.Y - 0.5f) * 0.04f; return SdBox(Q + FV(Lean, 0.0f), FV(0.37f, 0.2f), FV(0.63f, 0.88f), 0.05f) + Noise2(Q * FV(6.0f, 2.0f)) * 0.018f; };
		{
			FWith With(P);
			P.Look.Dome = 0.1f;
			P.Look.Spec = 0.2f;
			P.Look.Grain = 0.15f;
			P.Fill(Post, DriftWood);
		}
		{
			FClip Clip(P, Post);
			Strands(P, FV(0.5f, 0.18f), FV(0.5f, 0.9f), 0.12f, 14, 0.0018f, Alpha(DriftWoodDark, 0.45f), 1742, 0.012f);
			// weather cracks and a knot
			P.MLine(Smooth({ FV(0.42f, 0.7f), FV(0.43f, 0.78f), FV(0.42f, 0.86f) }), 0.003f, Alpha(DriftWoodDark, 0.8f));
			P.Mark([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.59f, 0.82f), 0.02f, 0.028f)) - 0.004f; }, Alpha(DriftWoodDark, 0.7f));
			// the top face: a bird with a heavy hooked beak and ringed eyes
			for (const float X : { 0.44f, 0.56f })
			{
				HDCarve(P, [X](FV Q) { return FMath::Abs(SdCircle(Q, FV(X, 0.27f), 0.03f)) - 0.006f; });
				HDCarve(P, [X](FV Q) { return SdCircle(Q, FV(X, 0.27f), 0.011f); });
			}
			// the middle face: a scowl, heavy brows, a toothed mouth
			HDCarve(P, [](FV Q) { return FMath::Min(SdCapsule(Q, FV(0.41f, 0.48f), FV(0.47f, 0.5f), 0.007f), SdCapsule(Q, FV(0.53f, 0.5f), FV(0.59f, 0.48f), 0.007f)); });
			HDCarve(P, [](FV Q) { return FMath::Min(SdEllipse(Q, FV(0.44f, 0.525f), 0.022f, 0.011f), SdEllipse(Q, FV(0.56f, 0.525f), 0.022f, 0.011f)); });
			HDCarve(P, [](FV Q) { return FMath::Max(SdBox(Q, FV(0.42f, 0.57f), FV(0.58f, 0.6f), 0.008f), -FMath::Min(FMath::Min(SdPoly(Q, { FV(0.44f, 0.565f), FV(0.46f, 0.565f), FV(0.45f, 0.585f) }), SdPoly(Q, { FV(0.49f, 0.565f), FV(0.51f, 0.565f), FV(0.5f, 0.585f) })), SdPoly(Q, { FV(0.54f, 0.565f), FV(0.56f, 0.565f), FV(0.55f, 0.585f) }))); });
			// the bottom face: round surprised eyes and an open mouth
			HDCarve(P, [](FV Q) { return FMath::Min(FMath::Abs(SdCircle(Q, FV(0.44f, 0.72f), 0.022f)) - 0.005f, FMath::Abs(SdCircle(Q, FV(0.56f, 0.72f), 0.022f)) - 0.005f); });
			HDCarve(P, [](FV Q) { return SdEllipse(Q, FV(0.5f, 0.79f), 0.025f, 0.02f); });
			// sea-bleached highlights on the high spots
			P.MEllipse(FV(0.44f, 0.4f), 0.04f, 0.15f, Hex(0xFFFFFF, 0.12f), 0.04f);
		}
		// the beak, carved separately and pegged on
		{
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Grain = 0.2f;
			P.Fill([](FV Q) { return SdPoly(Q, Smooth({ FV(0.47f, 0.3f), FV(0.53f, 0.3f), FV(0.52f, 0.36f), FV(0.5f, 0.4f), FV(0.49f, 0.36f) }, true, 4)); }, Tone(DriftWood, 1.05f));
		}
		HDCarve(P, [](FV Q) { return SdCapsule(Q, FV(0.48f, 0.335f), FV(0.52f, 0.335f), 0.002f); }, 0.003f);
		// rope lashings between the blocks
		for (const float Y : { 0.42f, 0.64f })
		{
			for (int32 K = 0; K < 2; ++K)
			{
				Rope(P, FV(0.355f, Y + K * 0.026f), FV(0.645f, Y + K * 0.026f + 0.006f), 0.013f);
			}
		}
		// a small cowrie half buried in the sand
		P.Fill([](FV Q) { return SdEllipse(Q, FV(0.74f, 0.86f), 0.035f, 0.022f); }, Hex(0xF0D8B8));
		P.MCapsule(FV(0.715f, 0.865f), FV(0.765f, 0.865f), 0.003f, Hex(0x5A3A20));
	}

	void HDItemSandDollar(FPainter& P)
	{
		// a sun-bleached sand dollar lying on rippled sand: five petal rows of pores, slotted lunules, a gold coin half buried beside it
		const auto Sand = [](FV Q) { return SdEllipse(Q, FV(0.5f, 0.66f), 0.46f, 0.26f) + Noise2(Q * 8.0f) * 0.02f; };
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Look.Grain = 0.25f;
			P.Fill(Sand, Hex(0xD8BE88));
		}
		{
			FClip Clip(P, Sand);
			for (int32 K = 0; K < 9; ++K)
			{
				const float Y = 0.44f + K * 0.05f;
				P.Mark([Y](FV Q) { return FMath::Abs(Q.Y - Y - FMath::Sin(Q.X * 18.0f + Y * 20.0f) * 0.012f) - 0.006f; }, Hex(0xA88A58, 0.35f), 0.008f);
			}
			Speckle(P, FV(0.04f, 0.4f), FV(0.96f, 0.92f), 120, 0.0015f, 0.004f, Hex(0x8A6A3A, 0.6f), 1751);
		}
		// the coin, half sunk, sand drifted over its lower edge
		{
			FClip Clip(P, [](FV Q) { return Q.Y - 0.8f - FMath::Sin(Q.X * 40.0f) * 0.006f; });
			Coin(P, FV(0.78f, 0.78f), 0.09f);
		}
		P.Mark([](FV Q) { return SdEllipse(Q, FV(0.78f, 0.81f), 0.11f, 0.02f) + Noise2(Q * 50.0f) * 0.01f; }, Hex(0xD8BE88), 0.006f);
		// the dollar, seen at a slant
		const FV C(0.46f, 0.52f);
		FNest Place(P, C, 1.0f, -8.0f);
		const auto Test = [](FV Q) { return SdEllipse(Q, FV(0.0f, 0.0f), 0.32f, 0.26f); };
		P.Mark([&Test](FV Q) { return Test(Q - FV(0.01f, 0.03f)); }, Hex(0x5A4020, 0.35f), 0.03f);
		{
			FWith With(P);
			P.Look.Dome = 0.06f;
			P.Look.Spec = 0.25f;
			P.Look.Grain = 0.2f;
			P.Look.GrainFreq = 60.0f;
			P.Fill(Test, Hex(0xF0E6D0));
		}
		FClip Clip(P, Test);
		// the petals: each an outline of paired pore rows, open at the tip
		for (int32 K = 0; K < 5; ++K)
		{
			const float A = -90.0f + K * 72.0f;
			for (const float Side : { -1.0f, 1.0f })
			{
				for (int32 J = 0; J < 9; ++J)
				{
					const float T = 0.12f + J * 0.1f;
					const float Wd = FMath::Sin(T * PI) * 0.045f;
					const FV Pt = Spin(FV(T * 0.17f, Side * Wd), A) * FV(1.0f, 0.82f);
					const FV Pt2 = Spin(FV(T * 0.17f, Side * (Wd + 0.008f)), A) * FV(1.0f, 0.82f);
					P.MCircle(Pt, 0.0035f, Hex(0x8A7458, 0.85f));
					P.MCircle(Pt2, 0.0035f, Hex(0x8A7458, 0.85f));
				}
			}
			// a lunule: a slot right through the test between this petal and the next
			const FV Slot = Spin(FV(0.2f, 0.0f), A + 36.0f) * FV(1.0f, 0.82f);
			const FV Dir = Slot.GetSafeNormal();
			P.Mark([Slot, Dir](FV Q) { return SdCapsule(Q, Slot - Dir * 0.025f, Slot + Dir * 0.025f, 0.008f); }, Hex(0xB89A68));
			P.Mark([Slot, Dir](FV Q) { return FMath::Abs(SdCapsule(Q, Slot - Dir * 0.025f, Slot + Dir * 0.025f, 0.008f)) - 0.002f; }, Hex(0x6A5030, 0.6f));
		}
		// the central star of pores, fine radiating grooves and a sheen
		for (int32 K = 0; K < 5; ++K)
		{
			P.MCircle(Spin(FV(0.012f, 0.0f), -90.0f + K * 72.0f), 0.004f, Hex(0x6A5438));
		}
		for (int32 K = 0; K < 30; ++K)
		{
			const FV Dir = Spin(FV(1.0f, 0.0f), K * 12.0f) * FV(1.0f, 0.82f);
			P.MLine({ Dir * 0.22f, Dir * 0.3f }, 0.0015f, Hex(0xC8B898, 0.5f));
		}
		P.MEllipse(FV(-0.1f, -0.1f), 0.14f, 0.07f, Hex(0xFFFFFF, 0.3f), 0.07f);
	}

	void HDItemLuckyStarfish(FPainter& P)
	{
		// an orange sea star mid-crawl, arms of different lengths curling, rows of knobbly spines and a pale madreporite
		const FV C(0.5f, 0.52f);
		TArray<TArray<FV>> Arms;
		for (int32 K = 0; K < 5; ++K)
		{
			const float A = -90.0f + K * 72.0f + (Rand01(1761, K) - 0.5f) * 10.0f;
			const float Len = 0.36f + Rand01(1762, K) * 0.06f;
			const float Curl = (Rand01(1763, K) - 0.5f) * 30.0f;
			const FV D0 = Spin(FV(1.0f, 0.0f), A);
			const FV D1 = Spin(FV(1.0f, 0.0f), A + Curl);
			Arms.Add(Smooth({ C, C + D0 * Len * 0.5f, C + D0 * Len * 0.75f + D1 * Len * 0.05f, C + D1 * Len }));
		}
		const auto Star = [&Arms, C](FV Q)
		{
			float D = SdCircle(Q, C, 0.1f);
			for (const TArray<FV>& Arm : Arms)
			{
				D = SMin(D, SdRibbon(Q, Arm, 0.085f, 0.022f), 0.05f);
			}
			return D;
		};
		P.Mark([&Star](FV Q) { return Star(Q - FV(0.012f, 0.025f)); }, Hex(0x06121A, 0.35f), 0.03f);
		{
			FWith With(P);
			P.Look.Dome = 0.07f;
			P.Look.Spec = 0.5f;
			P.Fill(Star, Hex(0xF07028));
		}
		FClip Clip(P, Star);
		// a darker reticulate network and rows of pale knobs down each arm
		P.Mark([](FV Q) { return (FMath::Abs(FMath::Sin(Q.X * 70.0f) * FMath::Sin(Q.Y * 70.0f)) - 0.12f) * 0.02f; }, Hex(0xB83A10, 0.35f), 0.003f);
		for (const TArray<FV>& Arm : Arms)
		{
			for (int32 J = 0; J < 9; ++J)
			{
				FV Pos, Tan;
				const float T = 0.12f + J * 0.1f;
				Along(Arm, T, Pos, Tan);
				const FV N(-Tan.Y, Tan.X);
				const float W = FMath::Lerp(0.085f, 0.022f, T);
				for (const float Off : { -0.5f, 0.0f, 0.5f })
				{
					const float R = (Off == 0.0f ? 0.014f : 0.009f) * (1.0f - T * 0.6f);
					P.Ball(Pos + N * W * Off, R, Off == 0.0f ? Hex(0xFFD8A0) : Hex(0xFFB070), 0.6f);
				}
			}
		}
		P.Ball(C + FV(0.03f, -0.02f), 0.018f, Hex(0xF8E0B0), 0.5f);
		P.Mark([C](FV Q) { return (FMath::Abs(FMath::Sin((Q.X - C.X) * 300.0f)) - 0.3f) * 0.01f + SdCircle(Q, C + FV(0.03f, -0.02f), 0.018f) * 0.0f + FMath::Max(0.0f, SdCircle(Q, C + FV(0.03f, -0.02f), 0.015f)); }, Hex(0xC07A40, 0.5f));
		P.MEllipse(C + FV(-0.08f, -0.08f), 0.1f, 0.05f, Hex(0xFFFFFF, 0.25f), 0.05f);
	}

	void HDItemKelpBraid(FPainter& P)
	{
		// a bracelet of three kelp strands braided over and under, tied off with a knot of leafy blades, a cowrie charm hanging from it
		const FV C(0.5f, 0.48f);
		const float R = 0.28f;
		const FLinearColor Greens[3] = { Hex(0x5A8A2A), Hex(0x7A9A30), Hex(0x4A7A3A) };
		// each strand is split into runs where it passes over or under; all the under-runs are drawn first
		const auto Point = [C, R](int32 S, float T, float& OutDepth)
		{
			const float A = -1.2f + T * (2.0f * PI - 0.5f);
			const float Ph = T * 2.0f * PI * 6.0f + S * 2.0f * PI / 3.0f;
			OutDepth = FMath::Cos(Ph);
			return C + FV(FMath::Cos(A), FMath::Sin(A)) * (R + FMath::Sin(Ph) * 0.034f);
		};
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (int32 S = 0; S < 3; ++S)
			{
				TArray<FV> Run;
				const int32 Steps = 240;
				for (int32 K = 0; K <= Steps + 1; ++K)
				{
					float Depth = 0.0f;
					const FV Pt = K <= Steps ? Point(S, (float)K / Steps, Depth) : FV(0.0f, 0.0f);
					const bool bIn = K <= Steps && ((Depth > 0.0f) == (Pass == 1));
					if (bIn)
					{
						Run.Add(Pt);
					}
					else if (Run.Num() > 1)
					{
						const TArray<FV> Pts = Run;
						const float Shade = Pass == 1 ? 1.0f : 0.7f;
						P.Mark([Pts](FV Q) { return SdPolyline(Q, Pts, 0.027f); }, Hex(0x1A2A0A, 0.9f));
						{
							FWith With(P);
							P.Look.Dome = 0.02f;
							P.Look.Ink = 0.0f;
							P.Fill([Pts](FV Q) { return SdPolyline(Q, Pts, 0.022f); }, Tone(Greens[S], Shade));
						}
						P.Mark([Pts](FV Q) { return SdPolyline(Q, Pts, 0.003f); }, Hex(0xC8E080, 0.4f));
						Run.Reset();
					}
					else
					{
						Run.Reset();
					}
				}
			}
		}
		// the knot and the blades of kelp left long at the tie
		const FV Knot = C + FV(FMath::Cos(-1.45f), FMath::Sin(-1.45f)) * R;
		for (int32 K = 0; K < 4; ++K)
		{
			const FV Dir = Spin(FV(0.0f, -1.0f), -60.0f + K * 38.0f);
			const TArray<FV> Blade = Smooth({ Knot, Knot + Dir * 0.08f + Spin(Dir, 90.0f) * 0.02f, Knot + Dir * 0.16f });
			FWith With(P);
			P.Look.Dome = 0.01f;
			P.Look.Ink = 0.006f;
			P.Fill([Blade](FV Q) { return SdRibbon(Q, Blade, 0.012f, 0.004f) + FMath::Abs(FMath::Sin(Q.X * 120.0f + Q.Y * 80.0f)) * 0.003f; }, Greens[K % 3]);
			P.MLine(Blade, 0.002f, Hex(0x2A3A10, 0.7f));
		}
		P.Ball(Knot, 0.03f, Hex(0x5A7A28), 0.4f);
		P.MLine(Smooth({ Knot + FV(-0.02f, -0.01f), Knot + FV(0.0f, 0.01f), Knot + FV(0.02f, -0.005f) }), 0.003f, Hex(0x2A3A10, 0.7f));
		// the cowrie charm on a short cord at the bottom
		const FV Low = C + FV(0.0f, R + 0.02f);
		P.MLine({ Low, Low + FV(0.0f, 0.07f) }, 0.004f, Hex(0xC8A870));
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Spec = 0.9f;
			P.Fill([Low](FV Q) { return SdEllipse(Q, Low + FV(0.0f, 0.12f), 0.04f, 0.055f); }, Hex(0xF0DCC0));
		}
		P.Mark([Low](FV Q) { return SdCapsule(Q, Low + FV(0.0f, 0.08f), Low + FV(0.0f, 0.16f), 0.004f); }, Hex(0x4A2A14));
		for (int32 K = 0; K < 5; ++K)
		{
			const float Y = Low.Y + 0.09f + K * 0.016f;
			P.MCapsule(FV(Low.X - 0.01f, Y), FV(Low.X + 0.01f, Y), 0.0025f, Hex(0xFFF8EC));
		}
		P.MEllipse(Low + FV(-0.015f, 0.1f), 0.008f, 0.016f, Hex(0xFFFFFF, 0.8f));
	}

	void HDItemSunkenCoinPurse(FPainter& P)
	{
		// an old clasp purse lost on the sea floor: barnacles and weed on its leather, its brass frame sprung open, coins spilling out over the sand
		HDMound(P, FV(0.5f, 0.84f), 0.42f, 0.09f, Hex(0xC8AE7A), 1771, false);
		const FLinearColor Leather = Hex(0x6A4A3A), Cord = Hex(0xB89A6A);
		// a drawstring pouch: a round belly, cinched at the neck, the leather bunched in a ruffle above the tie
		const FV Neck(0.42f, 0.4f);
		const auto Belly = [](FV Q) { return SdEllipse(Q, FV(0.42f, 0.62f), 0.25f, 0.2f) + Noise2(Q * 14.0f) * 0.008f; };
		const auto Ruffle = [Neck](FV Q)
		{
			const float H = FMath::Clamp((Neck.Y - Q.Y) / 0.14f, 0.0f, 1.0f);
			const float Half = 0.07f + H * 0.07f + FMath::Sin(Q.X * 70.0f) * 0.006f * H;
			const float Top = Neck.Y - 0.13f - FMath::Abs(FMath::Sin(Q.X * 34.0f)) * 0.025f;
			return FMath::Max(FMath::Abs(Q.X - Neck.X) - Half, FMath::Max(Top - Q.Y, Q.Y - Neck.Y - 0.06f));
		};
		const auto Bag = [&Belly, &Ruffle](FV Q) { return SMin(Belly(Q), Ruffle(Q), 0.05f); };
		{
			FWith With(P);
			P.Look.Dome = 0.14f;
			P.Look.Grain = 0.3f;
			P.Fill(Bag, Leather);
		}
		{
			FClip Clip(P, Bag);
			// folds gathered into the neck, both below and above the tie
			for (int32 K = 0; K < 7; ++K)
			{
				const float X = -0.18f + K * 0.06f;
				P.MLine(Smooth({ Neck + FV(X * 0.15f, 0.03f), Neck + FV(X * 0.7f, 0.14f), Neck + FV(X * 1.05f, 0.3f) }), 0.004f, Hex(0x2A1A10, 0.45f));
				P.MLine(Smooth({ Neck + FV(X * 0.15f + 0.008f, 0.03f), Neck + FV(X * 0.7f + 0.01f, 0.14f) }), 0.003f, Hex(0xFFE0C0, 0.12f));
			}
			for (int32 K = 0; K < 6; ++K)
			{
				const float X = -0.1f + K * 0.04f;
				P.MLine({ Neck + FV(X * 0.4f, -0.01f), Neck + FV(X, -0.12f) }, 0.004f, Hex(0x2A1A10, 0.5f));
			}
			Speckle(P, FV(0.16f, 0.26f), FV(0.68f, 0.82f), 80, 0.002f, 0.006f, Hex(0x9AA070, 0.5f), 1772);
			P.MEllipse(FV(0.32f, 0.54f), 0.08f, 0.06f, Hex(0xFFE0C0, 0.18f), 0.06f);
		}
		// the drawstring wound twice round the neck, knotted at the front, its two ends trailing over the sand
		for (int32 K = 0; K < 2; ++K)
		{
			const float Y = Neck.Y - 0.008f + K * 0.018f;
			P.Fill([Neck, Y](FV Q) { return SdRibbon(Q, Smooth({ FV(Neck.X - 0.085f, Y - 0.005f), FV(Neck.X, Y + 0.012f), FV(Neck.X + 0.085f, Y - 0.005f) }), 0.009f, 0.009f); }, Cord);
			for (int32 J = 0; J < 9; ++J)
			{
				const float X = Neck.X - 0.08f + J * 0.02f;
				P.MLine({ FV(X - 0.004f, Y - 0.006f), FV(X + 0.004f, Y + 0.008f) }, 0.002f, Hex(0x6A4A20, 0.7f));
			}
		}
		const FV Knot = Neck + FV(0.03f, 0.016f);
		const TArray<FV> EndA = Smooth({ Knot, Knot + FV(0.04f, 0.06f), Knot + FV(0.02f, 0.14f), Knot + FV(0.07f, 0.2f) });
		const TArray<FV> EndB = Smooth({ Knot, Knot + FV(0.08f, 0.03f), Knot + FV(0.14f, 0.08f), Knot + FV(0.16f, 0.14f) });
		for (const TArray<FV>* E : { &EndA, &EndB })
		{
			P.Fill([E](FV Q) { return SdPolyline(Q, *E, 0.007f); }, Cord);
			for (int32 J = 1; J < 10; ++J)
			{
				FV Pos, Tan;
				Along(*E, J / 10.0f, Pos, Tan);
				P.MLine({ Pos - FV(-Tan.Y, Tan.X) * 0.006f - Tan * 0.003f, Pos + FV(-Tan.Y, Tan.X) * 0.006f + Tan * 0.003f }, 0.0018f, Hex(0x6A4A20, 0.7f));
			}
			P.Ball(E->Last(), 0.012f, Cord, 0.4f);
		}
		P.Ball(Knot, 0.017f, Cord, 0.5f);
		P.MLine({ Knot + FV(-0.01f, -0.006f), Knot + FV(0.01f, 0.008f) }, 0.002f, Hex(0x6A4A20, 0.7f));
		// barnacles and weed
		HDBarnacle(P, FV(0.3f, 0.66f), 0.035f);
		HDBarnacle(P, FV(0.4f, 0.74f), 0.025f);
		HDBarnacle(P, FV(0.22f, 0.56f), 0.02f);
		HDWeed(P, FV(0.18f, 0.78f), 0.2f, -0.05f, 0.014f, Hex(0x4A8A3A));
		HDWeed(P, FV(0.22f, 0.8f), 0.14f, 0.04f, 0.012f, Hex(0x6A9A3A));
		// the spill of coins out of the mouth onto the sand
		Coin(P, FV(0.72f, 0.48f), 0.07f);
		Coin(P, FV(0.78f, 0.64f), 0.08f);
		Coin(P, FV(0.66f, 0.74f), 0.075f);
		Coin(P, FV(0.86f, 0.78f), 0.06f);
	}

	void HDItemScholarsCoral(FPainter& P)
	{
		// an old book lying open on the sea floor, its swollen pages written in faded ink, a pink coral grown up out of its gutter
		const FLinearColor Cover = Hex(0x2A4A7A), Page = Hex(0xF0E2C0);
		// the cover under the pages, in perspective
		P.Fill([](FV Q) { return SdPoly(Q, { FV(0.06f, 0.6f), FV(0.5f, 0.68f), FV(0.94f, 0.6f), FV(0.92f, 0.86f), FV(0.5f, 0.94f), FV(0.08f, 0.86f) }) - 0.01f; }, Cover);
		// the page blocks: each bowed up from the gutter, with the stacked page edges showing
		for (const float Sgn : { -1.0f, 1.0f })
		{
			const auto Leaf = [Sgn](FV Q)
			{
				const float X = (Q.X - 0.5f) * Sgn;
				if (X < -0.01f) return 1.0f;
				const float Top = 0.5f - FMath::Sin(FMath::Clamp(X / 0.42f, 0.0f, 1.0f) * PI) * 0.05f + X * 0.15f;
				const float Bot = 0.84f + X * 0.0f - FMath::Sin(FMath::Clamp(X / 0.42f, 0.0f, 1.0f) * PI) * 0.03f;
				return FMath::Max(FMath::Max(Top - Q.Y, Q.Y - Bot), FMath::Max(X - 0.42f, -X));
			};
			P.Fill([&Leaf](FV Q) { return Leaf(Q - FV(0.0f, -0.02f)); }, Tone(Page, 0.8f));
			for (int32 K = 0; K < 4; ++K)
			{
				P.Mark([&Leaf, K](FV Q) { return FMath::Abs(Leaf(Q - FV(0.0f, -0.005f * K)) ) - 0.0015f; }, Hex(0x8A7450, 0.4f));
			}
			{
				FWith With(P);
				P.Look.Dome = 0.04f;
				P.Look.Grain = 0.12f;
				P.Fill(Leaf, Page);
			}
			FClip Clip(P, Leaf);
			// lines of faded writing following the bow of the page
			for (int32 L = 0; L < 8; ++L)
			{
				const float Y0 = 0.56f + L * 0.033f;
				for (int32 W = 0; W < 5; ++W)
				{
					const float X0 = 0.05f + W * 0.072f + Rand01(1781, L * 7 + W) * 0.02f;
					const float X1 = X0 + 0.04f + Rand01(1782, L * 7 + W) * 0.02f;
					const FV A(0.5f + Sgn * X0, Y0 - FMath::Sin(X0 / 0.42f * PI) * 0.04f + X0 * 0.1f), B(0.5f + Sgn * X1, Y0 - FMath::Sin(X1 / 0.42f * PI) * 0.04f + X1 * 0.1f);
					P.MCapsule(A, B, 0.003f, Hex(0x5A4A3A, 0.45f));
				}
			}
			// water stains and foxing
			P.Mark([Sgn](FV Q) { return SdCircle(Q, FV(0.5f + Sgn * 0.3f, 0.72f), 0.06f) + Noise2(Q * 20.0f) * 0.02f; }, Hex(0xB89A60, 0.3f), 0.02f);
			P.MEllipse(FV(0.5f + Sgn * 0.04f, 0.68f), 0.04f, 0.18f, Hex(0x6A5030, 0.25f), 0.04f);
		}
		// the coral growing from the gutter, branching up and out
		HDSmallCoral(P, FV(0.5f, 0.66f), 0.2f, 0.03f, -6.0f, 3, Hex(0xF07A8A), 1783);
		HDSmallCoral(P, FV(0.46f, 0.68f), 0.12f, 0.02f, -40.0f, 2, Hex(0xE8A040), 1784);
		HDBarnacle(P, FV(0.86f, 0.86f), 0.025f);
	}

	/** A gold chain hanging in a V from the top of the frame down to Bail */
	void HDChainV(FPainter& P, FV Bail, float Spread, float Rise)
	{
		for (const float Sgn : { -1.0f, 1.0f })
		{
			for (int32 K = 0; K < 9; ++K)
			{
				const float T = K / 8.0f;
				const FV At = FV(Bail.X + Sgn * (0.02f + T * Spread), Bail.Y - 0.03f - FMath::Sin(T * PI * 0.5f) * Rise);
				P.Mark([At, K](FV Q) { return FMath::Abs(SdEllipse(Q, At, K % 2 ? 0.006f : 0.013f, K % 2 ? 0.013f : 0.007f)) - 0.003f; }, Hex(0xE8B040));
				P.Mark([At, K](FV Q) { return FMath::Abs(SdEllipse(Q, At - FV(0.002f, 0.002f), K % 2 ? 0.006f : 0.013f, K % 2 ? 0.013f : 0.007f)) - 0.001f; }, Hex(0xFFF0B0, 0.7f));
			}
		}
	}

	void HDItemClearwaterLens(FPainter& P)
	{
		// a brass magnifying glass held over the sea floor: murky sand around it, and under the glass everything bright, clear and enlarged
		const FV L(0.42f, 0.42f);
		const float R = 0.25f;
		// the sea floor
		const auto Floor = [](FV Q) { return SdEllipse(Q, FV(0.48f, 0.56f), 0.44f, 0.32f) + Noise2(Q * 6.0f) * 0.03f; };
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Grain = 0.25f;
			P.Fill(Floor, Hex(0x6A7A70));
		}
		{
			FClip Clip(P, Floor);
			Speckle(P, FV(0.04f, 0.24f), FV(0.92f, 0.9f), 120, 0.002f, 0.005f, Hex(0x3A4A44, 0.6f), 1791);
			P.MEllipse(FV(0.72f, 0.74f), 0.04f, 0.025f, Hex(0x9A8A70, 0.6f), 0.01f);
		}
		// under the glass: clear water, caustics, a little shell and grains of sand made large
		{
			FClip Clip(P, [L, R](FV Q) { return SdCircle(Q, L, R); });
			P.Mark([L, R](FV Q) { return SdCircle(Q, L, R); }, Hex(0xE0CC98));
			Speckle(P, L - FV(R, R), L + FV(R, R), 40, 0.006f, 0.014f, Hex(0xB89A60, 0.7f), 1792);
			Speckle(P, L - FV(R, R), L + FV(R, R), 30, 0.004f, 0.01f, Hex(0xFFF4D8, 0.8f), 1793);
			for (int32 K = 0; K < 12; ++K)
			{
				const FV A = L + FV(Rand01(1794, K) - 0.5f, Rand01(1795, K) - 0.5f) * R * 1.8f;
				P.MLine(Smooth({ A, A + FV(0.05f, 0.02f), A + FV(0.08f, -0.02f), A + FV(0.12f, 0.01f) }), 0.004f, Hex(0xFFFFFF, 0.5f));
			}
			// a small spotted cowrie, seen big
			{
				FWith With(P);
				P.Look.Dome = 0.05f;
				P.Look.Spec = 0.9f;
				P.Fill([L](FV Q) { return SdEllipse(Q, L + FV(0.05f, 0.06f), 0.1f, 0.065f); }, Hex(0xE8A060));
			}
			for (int32 K = 0; K < 8; ++K)
			{
				P.MCircle(L + FV(0.05f, 0.04f) + FV((Rand01(1796, K) - 0.5f) * 0.16f, (Rand01(1797, K) - 0.5f) * 0.06f), 0.008f + Rand01(1798, K) * 0.008f, Hex(0x6A3A18, 0.8f));
			}
			P.MEllipse(L + FV(0.02f, 0.03f), 0.05f, 0.015f, Hex(0xFFFFFF, 0.6f), 0.01f);
			P.Mark([L, R](FV Q) { return SdCircle(Q, L, R); }, Hex(0x60C8F0, 0.18f));
			// the glass's own reflections
			P.Mark([L, R](FV Q) { return SdArc(Q, L, R * 0.8f, R * 0.12f, 200.0f, 250.0f); }, Hex(0xFFFFFF, 0.6f), 0.01f);
			P.Mark([L, R](FV Q) { return SdArc(Q, L, R * 0.8f, R * 0.05f, 30.0f, 60.0f); }, Hex(0xFFFFFF, 0.4f), 0.006f);
		}
		// the handle: turned wood with a brass ferrule
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Spec = 0.6f;
			P.Look.Grain = 0.15f;
			P.Fill([](FV Q) { return SdRibbon(Q, { FV(0.64f, 0.64f), FV(0.9f, 0.9f) }, 0.032f, 0.04f); }, Hex(0x7A4A28));
		}
		P.MLine({ FV(0.66f, 0.64f), FV(0.9f, 0.88f) }, 0.004f, Hex(0xFFD8B0, 0.35f));
		{
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Spec = 0.95f;
			P.Fill([](FV Q) { return SdCapsule(Q, FV(0.6f, 0.6f), FV(0.67f, 0.67f), 0.04f); }, Hex(0xD8A840));
			// the brass rim
			P.Fill([L, R](FV Q) { return FMath::Abs(SdCircle(Q, L, R + 0.012f)) - 0.022f; }, Hex(0xD8A840));
		}
		P.Mark([L, R](FV Q) { return FMath::Abs(SdCircle(Q, L, R + 0.012f)) - 0.003f; }, Hex(0x7A5A12, 0.6f));
		P.Mark([L, R](FV Q) { return SdArc(Q, L, R + 0.02f, 0.006f, 190.0f, 260.0f); }, Hex(0xFFF4C0, 0.8f));
	}

	void HDItemPiratesDoubloon(FPainter& P)
	{
		// a thick, worn gold doubloon lying tilted, struck with a skull and crossed bones, its milled edge showing
		const FV C(0.5f, 0.46f);
		const float Rx = 0.36f, Ry = 0.3f;
		const FLinearColor Gold0 = Hex(0xE8B040), Dark = Hex(0x7A4A08);
		// the edge: a thick band below the face, milled
		P.Fill([C, Rx, Ry](FV Q) { return FMath::Min(SdEllipse(Q, C + FV(0.0f, 0.05f), Rx, Ry), SdBox(Q, C + FV(-Rx, 0.0f), C + FV(Rx, 0.05f), 0.0f)); }, Hex(0xB07A20));
		{
			FClip Clip(P, [C, Rx, Ry](FV Q) { return FMath::Max(SdEllipse(Q, C + FV(0.0f, 0.05f), Rx, Ry), -SdEllipse(Q, C, Rx, Ry)); });
			for (int32 K = 0; K < 60; ++K)
			{
				const float X = C.X - Rx + K * (2.0f * Rx / 60.0f);
				P.MLine({ FV(X, C.Y), FV(X, C.Y + Ry + 0.06f) }, 0.002f, Hex(0x5A3A08, 0.5f));
			}
		}
		const auto Face = [C, Rx, Ry](FV Q) { return SdEllipse(Q, C, Rx, Ry) + Noise2(Q * 10.0f) * 0.006f; };
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Spec = 0.95f;
			P.Fill(Face, Gold0);
		}
		FClip Clip(P, Face);
		// the raised rim and a ring of beads
		P.Mark([C, Rx, Ry](FV Q) { return FMath::Abs(SdEllipse(Q, C, Rx * 0.86f, Ry * 0.86f)) - 0.004f; }, Alpha(Dark, 0.6f));
		P.Mark([C, Rx, Ry](FV Q) { return FMath::Abs(SdEllipse(Q, C + FV(-0.003f, -0.004f), Rx * 0.86f, Ry * 0.86f)) - 0.002f; }, Hex(0xFFF4C0, 0.7f));
		for (int32 K = 0; K < 36; ++K)
		{
			const FV At = C + Spin(FV(1.0f, 0.0f), K * 10.0f) * FV(Rx * 0.93f, Ry * 0.93f);
			P.Ball(At, 0.007f, Tone(Gold0, 1.05f), 0.6f);
		}
		// the struck design in relief: shadow down-right, light up-left
		const auto Skull = [C](FV Q)
		{
			const FV L = Q - C;
			const float Head = SMin(SdEllipse(L, FV(0.0f, -0.04f), 0.09f, 0.085f), SdBox(L, FV(-0.05f, 0.0f), FV(0.05f, 0.08f), 0.02f), 0.02f);
			const float Eyes = FMath::Min(SdEllipse(L, FV(-0.035f, -0.03f), 0.025f, 0.03f), SdEllipse(L, FV(0.035f, -0.03f), 0.025f, 0.03f));
			const float Nose = SdPoly(L, { FV(0.0f, 0.0f), FV(0.012f, 0.025f), FV(-0.012f, 0.025f) });
			const float Teeth = FMath::Max(SdBox(L, FV(-0.04f, 0.05f), FV(0.04f, 0.075f), 0.0f), -FMath::Abs(FMath::Sin(L.X * 120.0f)) * 0.01f + 0.004f);
			return FMath::Max(Head, -FMath::Min(FMath::Min(Eyes, Nose), Teeth * 0.0f + 1.0f) );
		};
		const auto Bones = [C](FV Q)
		{
			const FV L = Q - C;
			float D = 1e6f;
			for (const float Sgn : { -1.0f, 1.0f })
			{
				const FV A(-0.17f * Sgn, -0.12f), B(0.17f * Sgn, 0.16f);
				D = FMath::Min(D, SdCapsule(L, A, B, 0.016f));
				for (const FV E : { A, B })
				{
					const FV Dir = (B - A).GetSafeNormal() * (E.Y < 0.0f ? -1.0f : 1.0f);
					const FV N(-Dir.Y, Dir.X);
					D = FMath::Min(D, FMath::Min(SdCircle(L, E + N * 0.016f, 0.02f), SdCircle(L, E - N * 0.016f, 0.02f)));
				}
			}
			return D;
		};
		P.Mark([&Bones](FV Q) { return Bones(Q - FV(0.006f, 0.008f)); }, Alpha(Dark, 0.7f));
		P.Mark([&Bones](FV Q) { return Bones(Q + FV(0.004f, 0.005f)); }, Hex(0xFFF4C0, 0.8f));
		P.Mark(Bones, Tone(Gold0, 0.95f));
		P.Mark([&Skull](FV Q) { return Skull(Q - FV(0.006f, 0.008f)); }, Alpha(Dark, 0.7f));
		P.Mark([&Skull](FV Q) { return Skull(Q + FV(0.004f, 0.005f)); }, Hex(0xFFF4C0, 0.8f));
		P.Mark(Skull, Tone(Gold0, 1.0f));
		// the eye sockets and teeth, sunk into the skull
		for (const float X : { -0.035f, 0.035f })
		{
			P.Mark([C, X](FV Q) { return SdEllipse(Q, C + FV(X, -0.03f), 0.022f, 0.026f); }, Alpha(Dark, 0.75f));
		}
		for (int32 K = 0; K < 4; ++K)
		{
			P.MLine({ C + FV(-0.03f + K * 0.02f, 0.05f), C + FV(-0.03f + K * 0.02f, 0.075f) }, 0.002f, Alpha(Dark, 0.7f));
		}
		// wear: scratches across the face and a dark tarnish in the low places
		for (int32 K = 0; K < 10; ++K)
		{
			const FV A = C + FV((Rand01(1801, K) - 0.5f) * Rx * 1.4f, (Rand01(1802, K) - 0.5f) * Ry * 1.4f);
			P.MCapsule(A, A + Spin(FV(0.05f, 0.0f), Rand01(1803, K) * 180.0f), 0.0012f, Hex(0xFFFFFF, 0.4f));
		}
		P.MEllipse(C + FV(0.12f, 0.12f), 0.2f, 0.12f, Hex(0x8A5A10, 0.25f), 0.1f);
		P.MEllipse(C + FV(-0.14f, -0.14f), 0.12f, 0.06f, Hex(0xFFFFFF, 0.35f), 0.06f);
	}

	void HDItemFlyingFishFin(FPainter& P)
	{
		// a flying fish bursting out of a wave, its great pectoral fins spread like glassy wings, water streaming off its tail
		// the splash it leaves
		P.Fill([](FV Q) { return SdEllipse(Q, FV(0.24f, 0.86f), 0.2f, 0.05f) + Noise2(Q * 20.0f) * 0.02f; }, Hex(0x3A9ACA));
		P.Mark([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.24f, 0.85f), 0.17f, 0.035f)) - 0.006f + Noise2(Q * 40.0f) * 0.008f; }, Hex(0xF4FCFF, 0.9f));
		for (int32 K = 0; K < 9; ++K)
		{
			const FV At(0.12f + Rand01(1811, K) * 0.26f, 0.62f + Rand01(1812, K) * 0.2f);
			P.Ball(At, 0.008f + Rand01(1813, K) * 0.01f, Hex(0xA8E8FF), 0.9f);
		}
		const FV Head(0.78f, 0.24f), Tail(0.3f, 0.7f);
		const FV D = (Head - Tail).GetSafeNormal();
		const FV N(-D.Y, D.X);
		const auto At = [Tail, D, N, Head](float T, float V) { return Tail + D * ((Head - Tail).Size() * T) + N * V; };
		// the far wing behind the body
		HDFin(P, At(0.64f, -0.02f), { At(0.66f, -0.14f), At(0.56f, -0.3f), At(0.4f, -0.4f), At(0.3f, -0.36f), At(0.36f, -0.22f), At(0.48f, -0.1f) }, Hex(0x9AD8F8, 0.55f), Hex(0x2A6AA0, 0.6f), 0.012f);
		// the tail fork, the lower lobe longer as flying fish have
		HDFin(P, At(0.08f, 0.0f), { At(-0.12f, -0.1f), At(-0.08f, -0.02f), At(-0.04f, 0.02f), At(-0.16f, 0.12f) }, Hex(0x5AA8E0, 0.85f), Hex(0x1A4A80, 0.7f), 0.012f);
		// the body: a sleek torpedo with a blunt rounded head and a slim tail stock, deep blue back, silver flank and belly
		const TArray<FV> Outline = Smooth({ At(0.04f, 0.0f), At(0.2f, -0.03f), At(0.5f, -0.055f), At(0.78f, -0.06f), At(0.92f, -0.048f), At(1.0f, -0.02f), At(1.02f, 0.006f),
			At(0.98f, 0.03f), At(0.88f, 0.05f), At(0.7f, 0.055f), At(0.45f, 0.048f), At(0.2f, 0.028f) }, true, 6);
		const auto Body = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Look.Spec = 0.9f;
			P.Fill(Body, Hex(0xC8D8E8));
		}
		{
			FClip Clip(P, Body);
			// the dark back, its edge wavering along the flank
			P.Mark([At, N](FV Q) { return FV::DotProduct(Q - At(0.5f, 0.0f), N) + 0.004f + FMath::Sin(Q.X * 90.0f) * 0.003f; }, Hex(0x1A4A8A), 0.01f);
			Scales(P, FV(0.2f, 0.1f), FV(0.9f, 0.8f), 0.016f, 0.0f, Hex(0x0A2A5A, 0.3f), Hex(0xFFFFFF, 0.25f));
			// gill cover, a lit stripe down the flank
			P.Mark([At](FV Q) { return FMath::Abs(SdCircle(Q, At(0.94f, 0.0f), 0.07f)) - 0.0025f; }, Hex(0x0A2A5A, 0.55f));
			P.MLine(Smooth({ At(0.15f, 0.012f), At(0.5f, 0.016f), At(0.82f, 0.012f) }), 0.004f, Hex(0xFFFFFF, 0.45f));
			P.MEllipse(At(0.97f, 0.03f), 0.03f, 0.016f, Hex(0xFFFFFF, 0.35f), 0.01f);
		}
		// a big round eye near the snout, and a small upturned mouth
		HDEye(P, At(0.955f, -0.012f), 0.019f, Hex(0x2A2A30), FV(0.3f, 0.0f), 0.75f);
		P.MLine(Smooth({ At(1.015f, 0.012f), At(0.995f, 0.022f), At(0.98f, 0.02f) }), 0.003f, Hex(0x0A1A3A));
		// the near wing: huge, sheer, ribbed, spread forward
		HDFin(P, At(0.64f, 0.02f), { At(0.66f, 0.16f), At(0.58f, 0.34f), At(0.42f, 0.44f), At(0.3f, 0.42f), At(0.34f, 0.28f), At(0.46f, 0.12f) }, Hex(0xA8E0FF, 0.65f), Hex(0x2A6AA0, 0.7f), 0.012f);
		// water streaming off the tail
		P.Mark([At](FV Q) { return SdRibbon(Q, Smooth({ At(0.0f, 0.04f), At(-0.1f, 0.08f), At(-0.18f, 0.16f) }), 0.006f, 0.002f); }, Hex(0xE0F8FF, 0.8f));
	}

	void HDItemTidecallersConch(FPainter& P)
	{
		// a queen conch with knobbed shoulders and a flaring pink lip, the sea itself pouring out of its mouth in a curling stream
		const FV Apex(0.82f, 0.2f), Base(0.32f, 0.66f);
		const FWhorlShell G = HDWhorlGeom(Apex, Base, 0.22f, 5);
		// the flared lip: a broad wing of shell growing out from the body whorl, glossy pink inside
		const FV LipC = G.At(0.6f, -G.Width * 0.85f);
		{
			FWith With(P);
			P.Look.Dome = 0.05f;
			P.Look.Spec = 0.9f;
			P.Fill([LipC, &G](FV Q) { return SMin(SdEllipse(Q, LipC, 0.16f, 0.2f) + Noise2(Q * 12.0f) * 0.01f, G.Whorl(Q, G.Centres.Num() - 1), 0.04f); }, Hex(0xF0D0B0));
		}
		P.Mark([LipC](FV Q) { return SdEllipse(Q, LipC + FV(0.01f, 0.01f), 0.12f, 0.16f); }, Hex(0xF89A90, 0.8f), 0.04f);
		P.Mark([LipC](FV Q) { return SdEllipse(Q, LipC + FV(0.03f, 0.03f), 0.06f, 0.1f); }, Hex(0xE85A6A, 0.6f), 0.04f);
		// knobbed spines along the shoulders, drawn first so the shell grows over their roots
		for (int32 I = 1; I < G.Centres.Num(); ++I)
		{
			for (const float Sgn : { -1.0f, 1.0f })
			{
				const FV Root = G.Centres[I] + G.N * (Sgn * G.Radii[I] * 0.85f) - G.D * G.Radii[I] * 0.2f;
				const FV Tip = Root + (G.N * Sgn - G.D * 0.6f).GetSafeNormal() * G.Radii[I] * 0.45f;
				FWith With(P);
				P.Look.Dome = 0.02f;
				P.Fill([Root, Tip, &G, I](FV Q) { return SdRibbon(Q, { Root, Tip }, G.Radii[I] * 0.22f, 0.006f); }, Hex(0xE8CCA8));
			}
		}
		const FWhorlShell S = HDWhorlShell(P, Apex, Base, 0.22f, 5, Hex(0xF0D8B8), Hex(0xA06A40), Hex(0xFAC8B0), Hex(0xE86A6A), 1821);
		// the sea pouring out of the mouth: a broad, clear gush that splashes down below
		const TArray<FV> Stream = Smooth({ S.ApertureC, S.ApertureC + FV(-0.06f, 0.05f), FV(0.18f, 0.8f), FV(0.12f, 0.92f) });
		const auto Gush = [Stream](FV Q) { return SdRibbon(Q, Stream, 0.03f, 0.05f) + Noise2(Q * 30.0f) * 0.006f; };
		P.Mark(Gush, Hex(0x3AA8E0, 0.6f));
		P.Mark([&Gush](FV Q) { return Gush(Q) + 0.012f; }, Hex(0x8AD8F8, 0.5f), 0.01f);
		for (int32 K = 0; K < 5; ++K)
		{
			const float Off = -0.02f + K * 0.01f;
			P.Mark([Stream, Off](FV Q) { TArray<FV> Line; for (int32 J = 0; J <= 12; ++J) { FV Pos, Tan; Along(Stream, J / 12.0f, Pos, Tan); Line.Add(Pos + FV(-Tan.Y, Tan.X) * Off * (1.0f + J / 12.0f)); } return SdPolyline(Q, Line, 0.002f); }, Hex(0xFFFFFF, 0.5f));
		}
		P.Mark([](FV Q) { return SdEllipse(Q, FV(0.13f, 0.92f), 0.12f, 0.03f) + Noise2(Q * 40.0f) * 0.015f; }, Hex(0xF4FCFF, 0.9f));
		for (int32 K = 0; K < 7; ++K)
		{
			const FV At(0.04f + Rand01(1822, K) * 0.24f, 0.78f + Rand01(1823, K) * 0.1f);
			P.Ball(At, 0.007f + Rand01(1824, K) * 0.008f, Hex(0xA8E8FF), 0.9f);
		}
	}

	void HDItemSirensLocket(FPainter& P)
	{
		// a gold heart locket hanging open on its hinge: a pearl nested in the velvet of one half, a wave engraved inside the other
		HDChainV(P, FV(0.62f, 0.4f), 0.24f, 0.26f);
		P.Fill([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.625f, 0.41f), 0.02f, 0.03f)) - 0.007f; }, Hex(0xE8B040));
		const FLinearColor Gold0 = Hex(0xE8B040), Dark = Hex(0x7A4A08);
		// the open lid, swung out to the left
		const TArray<FV> Lid = HDHeartOutline(FV(0.3f, 0.58f), 0.36f, -20.0f);
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Spec = 0.95f;
			P.Fill([&Lid](FV Q) { return SdPoly(Q, Lid); }, Gold0);
		}
		{
			FClip Clip(P, [&Lid](FV Q) { return SdPoly(Q, Lid) + 0.018f; });
			P.Mark([&Lid](FV Q) { return SdPoly(Q, Lid) + 0.018f; }, Tone(Gold0, 0.85f));
			// an engraved wave and a little scallop
			for (int32 K = 0; K < 3; ++K)
			{
				P.Mark([K](FV Q) { return FMath::Abs(Q.Y - 0.56f - K * 0.035f - FMath::Sin(Q.X * 50.0f + K) * 0.012f) - 0.0025f; }, Alpha(Dark, 0.6f));
			}
		}
		P.Mark([&Lid](FV Q) { return FMath::Abs(SdPoly(Q, Lid) + 0.018f) - 0.003f; }, Alpha(Dark, 0.5f));
		// the body of the locket, with its velvet bed and the pearl
		const TArray<FV> Body = HDHeartOutline(FV(0.64f, 0.56f), 0.4f, 8.0f);
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Spec = 0.95f;
			P.Fill([&Body](FV Q) { return SdPoly(Q, Body); }, Gold0);
		}
		P.Fill([&Body](FV Q) { return SdPoly(Q, Body) + 0.024f; }, Hex(0x5A1838));
		P.Mark([&Body](FV Q) { return SdPoly(Q, Body) + 0.05f; }, Hex(0x8A2A5A, 0.5f), 0.03f);
		HDPearl(P, FV(0.635f, 0.52f), 0.075f, Hex(0xF4F0EC), Hex(0xB0A0C8));
		// the hinge between the halves and a sparkle on the rim
		for (int32 K = 0; K < 3; ++K)
		{
			P.Ball(FV(0.45f, 0.54f + K * 0.035f), 0.012f, Tone(Gold0, 0.9f), 0.8f);
		}
		Glint(P, FV(0.76f, 0.42f), 0.07f);
	}

	/** The gold-orange halo behind a unique legendary, kept inside the tile */
	void HDHalo(FPainter& P)
	{
		P.Glow([](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.3f); }, Hex(0xFFB040), 0.08f, 0.6f);
	}

	void HDItemTridentOfTheDeepKing(FPainter& P)
	{
		// the sea-king's trident: three barbed gold tines rising from a crown of shell-work, an aquamarine at the heart, a shaft bound in sharkskin
		HDHalo(P);
		const FLinearColor Gold0 = Hex(0xE8B040), Dark = Hex(0x7A4A08);
		FNest Place(P, FV(0.5f, 0.5f), 1.0f, 18.0f);
		// the shaft: dark sea-wood with a wrapped grip and a gold butt-cap
		{
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Grain = 0.2f;
			P.Fill([](FV Q) { return SdCapsule(Q, FV(0.0f, -0.12f), FV(0.0f, 0.46f), 0.022f); }, Hex(0x4A3020));
		}
		for (int32 K = 0; K < 10; ++K)
		{
			const float Y = 0.12f + K * 0.022f;
			P.MCapsule(FV(-0.022f, Y), FV(0.022f, Y + 0.012f), 0.007f, K % 2 ? Hex(0x6A7A80) : Hex(0x8A9AA0));
		}
		P.Ball(FV(0.0f, 0.46f), 0.03f, Gold0, 0.9f);
		// the tines: one tall centre, two curving outer, each with a backward barb, fused into a scalloped collar
		const auto Tine = [](FV Q, float X, float Top, float Bend)
		{
			const TArray<FV> Path = Smooth({ FV(X * 0.4f, -0.1f), FV(X * 0.9f + Bend * 0.3f, -0.2f), FV(X + Bend, Top) });
			const float Body = SdRibbon(Q, Path, 0.024f, 0.004f);
			const FV Barb = FV(X + Bend * 0.8f, Top + 0.08f);
			const float BarbD = SdPoly(Q, { Barb + FV(-0.012f * (X >= 0.0f ? 1.0f : -1.0f), -0.01f), Barb + FV(0.04f * (X > 0.0f ? 1.0f : (X < 0.0f ? -1.0f : 1.0f)), 0.03f), Barb + FV(0.004f, 0.03f) });
			return SMin(Body, BarbD, 0.008f);
		};
		const auto Head = [&Tine](FV Q)
		{
			float D = SdEllipse(Q, FV(0.0f, -0.1f), 0.1f, 0.045f);
			D = SMin(D, Tine(Q, 0.0f, -0.46f, 0.0f), 0.02f);
			D = SMin(D, Tine(Q, -0.14f, -0.36f, -0.02f), 0.02f);
			D = SMin(D, Tine(Q, 0.14f, -0.36f, 0.02f), 0.02f);
			return D;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Spec = 1.0f;
			P.Fill(Head, Gold0);
		}
		{
			FClip Clip(P, Head);
			// a bright edge down each tine and fluting like a shell on the collar
			for (const float X : { -0.14f, 0.0f, 0.14f })
			{
				P.MLine(Smooth({ FV(X * 0.4f - 0.006f, -0.12f), FV(X * 0.9f - 0.008f, -0.22f), FV(X * 1.0f - 0.006f, X == 0.0f ? -0.4f : -0.32f) }), 0.003f, Hex(0xFFF8D0, 0.85f));
			}
			for (int32 K = -4; K <= 4; ++K)
			{
				P.MLine({ FV(K * 0.02f, -0.06f), FV(K * 0.016f, -0.13f) }, 0.002f, Alpha(Dark, 0.6f));
			}
		}
		// the aquamarine set in the collar, and pearls at its ends
		P.Ball(FV(0.0f, -0.1f), 0.036f, Tone(Gold0, 0.8f), 0.8f);
		P.Ball(FV(0.0f, -0.1f), 0.028f, Hex(0x40C0D8), 1.0f);
		P.MPoly({ FV(-0.012f, -0.11f), FV(0.0f, -0.124f), FV(0.012f, -0.11f), FV(0.0f, -0.09f) }, Hex(0xFFFFFF, 0.35f));
		for (const float X : { -0.1f, 0.1f })
		{
			P.Ball(FV(X, -0.1f), 0.016f, Hex(0xF0ECF4), 0.6f);
		}
		Glint(P, FV(0.0f, -0.44f), 0.12f, Hex(0xFFF0B0));
	}

	void HDItemLeviathansJaw(FPainter& P)
	{
		// the lower jawbone of a leviathan: a long curved bone, porous and cracked, set with ranks of serrated fangs, barnacles on the old bone
		HDHalo(P);
		const FLinearColor Bone = Hex(0xE8DCC0), BoneDark = Hex(0x8A7A5A);
		const TArray<FV> Ridge = Smooth({ FV(0.08f, 0.5f), FV(0.3f, 0.62f), FV(0.56f, 0.64f), FV(0.8f, 0.56f), FV(0.92f, 0.42f) });
		// the teeth first so the gum line of bone overlaps their roots
		for (int32 K = 0; K < 11; ++K)
		{
			const float T = 0.06f + K * 0.088f;
			FV Pos, Tan;
			Along(Ridge, T, Pos, Tan);
			const FV Up(Tan.Y, -Tan.X);
			const float H = (K % 3 == 1 ? 0.15f : 0.09f) * (0.6f + FMath::Sin(T * PI) * 0.5f);
			const float W = H * 0.32f;
			const FV Tip = Pos + Up * H + Tan * H * 0.25f;
			const bool bBroken = K == 7;
			const TArray<FV> Tooth = { Pos - Tan * W, bBroken ? Pos + Up * H * 0.5f - Tan * W * 0.4f : Tip, bBroken ? Pos + Up * H * 0.4f + Tan * W * 0.3f : Tip, Pos + Tan * W };
			{
				FWith With(P);
				P.Look.Dome = W;
				P.Look.Spec = 0.6f;
				P.Fill([Tooth](FV Q) { return SdPoly(Q, Tooth) - 0.003f; }, Hex(0xF4ECD8));
			}
			// serrations down the trailing edge and a yellowed base
			for (int32 J = 1; J < 5; ++J)
			{
				const FV At = FMath::Lerp(Tooth[3], Tooth[2], J / 5.0f);
				P.MCapsule(At, At + Tan * 0.006f, 0.002f, Hex(0x8A7A5A, 0.7f));
			}
			P.MPoly({ Pos - Tan * W, Pos + Up * H * 0.3f - Tan * W * 0.7f, Pos + Up * H * 0.3f + Tan * W * 0.7f, Pos + Tan * W }, Hex(0xC8A870, 0.5f));
		}
		// the jawbone
		const TArray<FV> Lower = Smooth({ FV(0.06f, 0.52f), FV(0.16f, 0.74f), FV(0.42f, 0.84f), FV(0.7f, 0.78f), FV(0.9f, 0.6f), FV(0.94f, 0.44f) });
		const auto Jaw = [&Ridge, &Lower](FV Q)
		{
			TArray<FV> Outline = Ridge;
			for (int32 K = Lower.Num() - 1; K >= 0; --K)
			{
				Outline.Add(Lower[K]);
			}
			return SdPoly(Q, Outline) + Noise2(Q * 14.0f) * 0.008f;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.08f;
			P.Look.Spec = 0.3f;
			P.Look.Grain = 0.3f;
			P.Fill(Jaw, Bone);
		}
		{
			FClip Clip(P, Jaw);
			Pits(P, FV(0.08f, 0.5f), FV(0.92f, 0.86f), 70, 0.004f, Alpha(BoneDark, 0.7f), 1831);
			P.MLine(Smooth({ FV(0.34f, 0.66f), FV(0.38f, 0.72f), FV(0.36f, 0.78f), FV(0.4f, 0.84f) }), 0.003f, Hex(0x4A3A20, 0.8f));
			P.MLine(Smooth({ FV(0.7f, 0.64f), FV(0.72f, 0.7f) }), 0.003f, Hex(0x4A3A20, 0.8f));
			// foramina: a row of nerve holes along the bone
			for (int32 K = 0; K < 5; ++K)
			{
				P.MEllipse(FV(0.24f + K * 0.13f, 0.74f - FMath::Sin(K * 0.8f) * 0.02f), 0.012f, 0.008f, Hex(0x3A2A18, 0.8f));
			}
			P.MEllipse(FV(0.5f, 0.68f), 0.34f, 0.04f, Hex(0xFFFFFF, 0.2f), 0.03f);
			P.MEllipse(FV(0.5f, 0.84f), 0.4f, 0.06f, Hex(0x5A4A30, 0.35f), 0.04f);
		}
		HDBarnacle(P, FV(0.2f, 0.72f), 0.03f);
		HDBarnacle(P, FV(0.26f, 0.79f), 0.022f);
		HDBarnacle(P, FV(0.8f, 0.68f), 0.025f);
	}

	void HDItemAbyssalAnchor(FPainter& P)
	{
		// a great iron anchor dredged from the abyss: rust-scabbed, crusted with barnacles and pale deep-sea growths, its chain trailing up into the dark
		HDHalo(P);
		const FLinearColor AnchorIron = Hex(0x5A5E66), Rust = Hex(0x9A4A20);
		// the chain, from the ring up out of the frame
		for (int32 K = 0; K < 5; ++K)
		{
			const FV At(0.5f + FMath::Sin(K * 0.6f) * 0.03f, 0.13f - K * 0.06f);
			if (K % 2 == 0)
			{
				P.Fill([At](FV Q) { return FMath::Abs(SdEllipse(Q, At, 0.024f, 0.04f)) - 0.008f; }, AnchorIron);
			}
			else
			{
				P.Fill([At](FV Q) { return SdCapsule(Q, At - FV(0.0f, 0.035f), At + FV(0.0f, 0.035f), 0.009f); }, Tone(AnchorIron, 0.8f));
			}
		}
		const auto Anchor = [](FV Q)
		{
			float D = FMath::Abs(SdCircle(Q, FV(0.5f, 0.17f), 0.05f)) - 0.016f;
			D = FMath::Min(D, SdBox(Q, FV(0.47f, 0.21f), FV(0.53f, 0.78f), 0.01f));
			D = FMath::Min(D, SdBox(Q, FV(0.32f, 0.3f), FV(0.68f, 0.35f), 0.02f));
			D = FMath::Min(D, SdCircle(Q, FV(0.3f, 0.325f), 0.03f));
			D = FMath::Min(D, SdCircle(Q, FV(0.7f, 0.325f), 0.03f));
			// the arms sweeping up from the crown, ending in broad flukes
			const float Arms = FMath::Max(FMath::Abs(SdCircle(Q, FV(0.5f, 0.5f), 0.3f)) - 0.028f, Q.Y - 0.5f - 0.3f * 0.0f > -0.02f ? -1.0f : 1.0f);
			const float ArmsD = FMath::Max(FMath::Abs(SdCircle(Q, FV(0.5f, 0.52f), 0.28f)) - 0.028f, 0.6f - Q.Y);
			D = SMin(D, ArmsD, 0.03f);
			for (const float Sgn : { -1.0f, 1.0f })
			{
				const FV Tip(0.5f + Sgn * 0.28f, 0.56f);
				D = SMin(D, SdPoly(Q, { Tip + FV(-0.04f * Sgn, 0.06f), Tip + FV(0.05f * Sgn, 0.02f), Tip + FV(0.01f * Sgn, -0.12f) }), 0.01f);
			}
			(void)Arms;
			return D + Noise2(Q * 30.0f) * 0.004f;
		};
		P.Glow(Anchor, Hex(0x6A3AC8), 0.05f, 0.4f);
		{
			FWith With(P);
			P.Look.Dome = 0.025f;
			P.Look.Spec = 0.4f;
			P.Look.Grain = 0.35f;
			P.Fill(Anchor, AnchorIron);
		}
		{
			FClip Clip(P, Anchor);
			// rust scabs and streaks, and pitted iron
			for (int32 K = 0; K < 14; ++K)
			{
				const FV At(0.2f + Rand01(1841, K) * 0.6f, 0.15f + Rand01(1842, K) * 0.7f);
				P.Mark([At](FV Q) { return SdCircle(Q, At, 0.03f) + Noise2(Q * 50.0f) * 0.02f; }, Alpha(Rust, 0.7f), 0.01f);
			}
			Pits(P, FV(0.2f, 0.1f), FV(0.8f, 0.86f), 60, 0.004f, Hex(0x1A1A20, 0.7f), 1843);
			P.MLine({ FV(0.485f, 0.24f), FV(0.485f, 0.74f) }, 0.004f, Hex(0xC8D0D8, 0.35f));
		}
		// deep-sea growth: barnacles, pale tube worms and a sea anemone on the crown
		HDBarnacle(P, FV(0.42f, 0.78f), 0.03f);
		HDBarnacle(P, FV(0.56f, 0.8f), 0.024f);
		HDBarnacle(P, FV(0.34f, 0.32f), 0.02f);
		for (int32 K = 0; K < 4; ++K)
		{
			const FV Root(0.62f + K * 0.025f, 0.74f + K * 0.012f);
			P.Fill([Root, K](FV Q) { return SdRibbon(Q, Smooth({ Root, Root + FV(0.01f, -0.04f), Root + FV(0.025f + K * 0.005f, -0.07f) }), 0.007f, 0.006f); }, Hex(0xF0E8D8));
			P.MCircle(Root + FV(0.025f + K * 0.005f, -0.07f), 0.006f, Hex(0xD84040));
		}
	}

	void HDItemTearOfTheMoon(FPainter& P)
	{
		// a pale crescent moon weeping: a single luminous pearl tear gathers at its lower horn and falls
		HDHalo(P);
		const auto Moon = [](FV Q) { return FMath::Max(SdCircle(Q, FV(0.44f, 0.42f), 0.3f), -SdCircle(Q, FV(0.58f, 0.34f), 0.27f)); };
		P.Glow(Moon, Hex(0xC8D8FF), 0.06f, 0.5f);
		{
			FWith With(P);
			P.Look.Dome = 0.1f;
			P.Look.Spec = 0.4f;
			P.Fill(Moon, Hex(0xE8ECF4));
		}
		{
			FClip Clip(P, Moon);
			for (const FVector M : { FVector(0.24f, 0.4f, 0.05f), FVector(0.3f, 0.56f, 0.04f), FVector(0.22f, 0.28f, 0.025f) })
			{
				P.Mark([M](FV Q) { return SdCircle(Q, FV(M.X, M.Y), M.Z) + Noise2(Q * 30.0f) * 0.015f; }, Hex(0x9AA4C0, 0.35f), 0.02f);
			}
			for (const FVector Cr : { FVector(0.2f, 0.48f, 0.014f), FVector(0.34f, 0.66f, 0.012f), FVector(0.28f, 0.2f, 0.01f) })
			{
				P.Mark([Cr](FV Q) { return FMath::Abs(SdCircle(Q, FV(Cr.X, Cr.Y), Cr.Z)) - 0.002f; }, Hex(0x7A84A0, 0.5f));
			}
			P.MEllipse(FV(0.5f, 0.66f), 0.2f, 0.06f, Hex(0x6A7AA0, 0.3f), 0.05f);
		}
		// the tear: welling at the horn and running down into one luminous pearly bead
		const FV Horn(0.6f, 0.69f);
		P.Glow([Horn](FV Q) { return SdCircle(Q, Horn + FV(0.03f, 0.2f), 0.05f); }, Hex(0xD0E0FF), 0.06f, 0.7f);
		HDDrip(P, Horn, 0.2f, 0.06f, Hex(0xEEF2FA), 0.03f, 0.016f);
		{
			const FV B = Horn + FV(0.03f, 0.2f);
			FClip Clip(P, [B](FV Q) { return SdCircle(Q, B, 0.06f); });
			P.MEllipse(B + FV(0.02f, 0.02f), 0.04f, 0.03f, Hex(0x9AA8D8, 0.45f), 0.03f);
			P.MEllipse(B + FV(-0.025f, 0.02f), 0.015f, 0.025f, Hex(0xFFB8D8, 0.35f), 0.015f);
		}
		HDSpark(P, FV(0.82f, 0.22f), 0.05f, Hex(0xE8F0FF));
		HDSpark(P, FV(0.7f, 0.1f), 0.03f, Hex(0xE8F0FF));
	}

	void HDItemHeartOfCoral(FPainter& P)
	{
		// a heart grown entirely of living coral: porous, lumpy, studded with polyps, branches sprouting from its crown
		HDHalo(P);
		const TArray<FV> Outline = HDHeartOutline(FV(0.5f, 0.56f), 0.7f);
		const auto Heart = [&Outline](FV Q) { return SdPoly(Q, Outline) + Noise2(Q * 18.0f) * 0.012f + Noise2(Q * 50.0f) * 0.004f; };
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.3f;
			P.Look.Grain = 0.3f;
			P.Look.GrainFreq = 60.0f;
			P.Fill(Heart, Hex(0xF0705A));
		}
		{
			FClip Clip(P, Heart);
			// corallites: little star-cupped pores packed over the surface, each with a polyp half out
			for (int32 K = 0; K < 70; ++K)
			{
				const FV At(0.16f + Rand01(1853, K) * 0.68f, 0.3f + Rand01(1854, K) * 0.56f);
				P.Mark([At](FV Q) { return SdCircle(Q, At, 0.014f); }, Hex(0xA03A30, 0.6f), 0.004f);
				P.MCircle(At - FV(0.002f, 0.003f), 0.008f, Hex(0xFFC8A8, 0.75f));
				if (K % 3 == 0)
				{
					for (int32 J = 0; J < 6; ++J)
					{
						P.MCapsule(At, At + Spin(FV(0.012f, 0.0f), J * 60.0f + K * 7.0f), 0.0015f, Hex(0xFFF0E0, 0.8f));
					}
				}
			}
			// the cleft and a soft fold of shadow down the middle
			P.Mark([](FV Q) { return SdCapsule(Q, FV(0.5f, 0.36f), FV(0.51f, 0.5f), 0.005f); }, Hex(0x7A2A20, 0.5f), 0.03f);
			P.MEllipse(FV(0.36f, 0.42f), 0.1f, 0.05f, Hex(0xFFFFFF, 0.25f), 0.05f);
			P.MEllipse(FV(0.6f, 0.7f), 0.2f, 0.12f, Hex(0x6A1A10, 0.3f), 0.1f);
		}
		HDSmallCoral(P, FV(0.38f, 0.4f), 0.15f, 0.026f, -20.0f, 2, Hex(0xF07A70), 1851);
		HDSmallCoral(P, FV(0.64f, 0.4f), 0.13f, 0.024f, 18.0f, 2, Hex(0xF09060), 1852);
	}

	void HDItemBlackTidePearl(FPainter& P)
	{
		// a black pearl riding the crest of a black, oily tide, poison-green light glimmering in the water
		HDHalo(P);
		HDWave(P, Hex(0x1A2A2A), Hex(0x000808), 1861);
		{
			FClip Clip(P, [](FV Q) { return SdEllipse(Q, FV(0.48f, 0.66f), 0.42f, 0.24f); });
			// a sickly green glow welling up through the dark water, with an oily sheen on the face
			P.MEllipse(FV(0.44f, 0.64f), 0.18f, 0.1f, Hex(0x40C060, 0.3f), 0.1f);
			P.MEllipse(FV(0.36f, 0.74f), 0.1f, 0.05f, Hex(0x60E070, 0.25f), 0.06f);
			P.Mark([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.42f, 0.6f), 0.2f, 0.08f) + Noise2(Q * 12.0f) * 0.03f) - 0.006f; }, Hex(0xA080E0, 0.15f), 0.01f);
		}
		HDPearl(P, FV(0.52f, 0.3f), 0.14f, Hex(0x262A30), Hex(0x050708));
		{
			FClip Clip(P, [](FV Q) { return SdCircle(Q, FV(0.52f, 0.3f), 0.14f); });
			P.MEllipse(FV(0.58f, 0.36f), 0.07f, 0.04f, Hex(0x40B080, 0.1f), 0.05f);
			P.MEllipse(FV(0.44f, 0.36f), 0.06f, 0.04f, Hex(0x8040A0, 0.3f), 0.04f);
		}
		P.Glow([](FV Q) { return SdCircle(Q, FV(0.52f, 0.3f), 0.14f); }, Hex(0x40E070), 0.04f, 0.12f);
	}

	void HDItemCompassOfTides(FPainter& P)
	{
		// a brass pocket compass open on its hinge: an engraved rose under the glass, a blued needle, a wave chased into the lid
		HDHalo(P);
		const FLinearColor CompassBrass = Hex(0xD8A840), Dark = Hex(0x6A4A10);
		// the lid, swung up behind
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Spec = 0.9f;
			P.Fill([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.24f), 0.3f, 0.14f); }, Tone(CompassBrass, 0.85f));
		}
		P.Mark([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.24f), 0.26f, 0.11f); }, Tone(CompassBrass, 0.7f));
		for (int32 K = 0; K < 3; ++K)
		{
			P.Mark([K](FV Q) { return FMath::Max(FMath::Abs(Q.Y - 0.2f - K * 0.035f - FMath::Sin(Q.X * 40.0f + K) * 0.01f) - 0.003f, SdEllipse(Q, FV(0.5f, 0.24f), 0.24f, 0.1f)); }, Alpha(Dark, 0.6f));
		}
		P.MCapsule(FV(0.44f, 0.37f), FV(0.56f, 0.37f), 0.016f, CompassBrass);
		// the case
		const FV C(0.5f, 0.6f);
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Spec = 0.95f;
			P.Fill([C](FV Q) { return SdCircle(Q, C, 0.27f); }, CompassBrass);
		}
		for (int32 K = 0; K < 40; ++K)
		{
			const FV Dir = Spin(FV(1.0f, 0.0f), K * 9.0f);
			P.MLine({ C + Dir * 0.255f, C + Dir * 0.27f }, 0.002f, Alpha(Dark, 0.6f));
		}
		// the dial, the rose, the needle, the glass
		P.Fill([C](FV Q) { return SdCircle(Q, C, 0.21f); }, Hex(0xF4ECD8));
		{
			FClip Clip(P, [C](FV Q) { return SdCircle(Q, C, 0.21f); });
			for (int32 K = 0; K < 72; ++K)
			{
				const FV Dir = Spin(FV(1.0f, 0.0f), K * 5.0f);
				P.MLine({ C + Dir * (K % 2 ? 0.19f : 0.175f), C + Dir * 0.205f }, 0.0015f, Hex(0x3A2A18, 0.8f));
			}
			for (int32 K = 0; K < 8; ++K)
			{
				const float Len = K % 2 ? 0.1f : 0.16f;
				const FV Dir = Spin(FV(0.0f, -1.0f), K * 45.0f);
				const FV Side(-Dir.Y, Dir.X);
				P.MPoly({ C, C + Side * 0.025f + Dir * 0.03f, C + Dir * Len }, K % 2 ? Hex(0x8A7A60, 0.7f) : Hex(0x2A4A7A, 0.8f));
				P.MPoly({ C, C - Side * 0.025f + Dir * 0.03f, C + Dir * Len }, K % 2 ? Hex(0xB8A888, 0.7f) : Hex(0x4A7AB0, 0.8f));
			}
			// the needle, swung a little off north as if pulled by the tide
			const FV N = Spin(FV(0.0f, -1.0f), 24.0f);
			const FV Sd(-N.Y, N.X);
			P.Fill([C, N, Sd](FV Q) { return SdPoly(Q, { C + N * 0.18f, C + Sd * 0.018f, C - N * 0.02f, C - Sd * 0.018f }); }, Hex(0xC82A2A));
			P.Fill([C, N, Sd](FV Q) { return SdPoly(Q, { C - N * 0.15f, C + Sd * 0.016f, C + N * 0.02f, C - Sd * 0.016f }); }, Hex(0x2A3A5A));
			P.Ball(C, 0.016f, CompassBrass, 0.9f);
			P.Mark([C](FV Q) { return SdArc(Q, C, 0.16f, 0.03f, 200.0f, 260.0f); }, Hex(0xFFFFFF, 0.5f), 0.01f);
			P.Mark([C](FV Q) { return SdCircle(Q, C, 0.21f); }, Hex(0x8AC8E0, 0.1f));
		}
		P.Mark([C](FV Q) { return FMath::Abs(SdCircle(Q, C, 0.21f)) - 0.006f; }, Tone(CompassBrass, 0.75f));
		P.Ball(FV(0.5f, 0.32f), 0.025f, CompassBrass, 0.9f);
	}

	void HDItemGildedConch(FPainter& P)
	{
		// a conch cast in gold: every whorl gleaming, set with pearls and garnets along its shoulder, a ruby-red throat
		HDHalo(P);
		const FWhorlShell S = HDWhorlShell(P, FV(0.2f, 0.74f), FV(0.76f, 0.3f), 0.24f, 6, Hex(0xE8B040), Hex(0x8A5A10), Hex(0xFFE08A), Hex(0xB0202A), 1871);
		for (int32 I = 1; I < S.Centres.Num(); ++I)
		{
			const FV At = S.Centres[I] - S.D * S.Radii[I] * 0.45f;
			if (I % 2)
			{
				P.Ball(At, S.Radii[I] * 0.14f, Hex(0xF0ECF4), 0.7f);
			}
			else
			{
				P.Ball(At, S.Radii[I] * 0.16f, Tone(Hex(0xE8B040), 0.8f), 0.8f);
				P.Ball(At, S.Radii[I] * 0.11f, Hex(0xB0203A), 1.0f);
			}
		}
		Glint(P, S.At(0.4f, -S.Width * 0.4f), 0.1f, Hex(0xFFF4C0));
		Glint(P, S.At(0.1f, 0.0f), 0.06f, Hex(0xFFF4C0));
	}

	void HDItemKrakensHeart(FPainter& P)
	{
		// a dark, swollen heart caught in the kraken's grip: one arm coiled round it from behind, crossing over its front
		HDHalo(P);
		const FLinearColor Arm = Hex(0x6A2A7A), Sucker = Hex(0xD89AD8);
		Tentacle(P, { FV(0.92f, 0.94f), FV(0.82f, 0.62f), FV(0.86f, 0.36f), FV(0.7f, 0.2f), FV(0.54f, 0.26f) }, 0.08f, 0.03f, -1.0f, Arm, Sucker);
		const TArray<FV> Outline = HDHeartOutline(FV(0.48f, 0.54f), 0.62f, -6.0f);
		const auto Heart = [&Outline](FV Q) { return SdPoly(Q, Outline); };
		P.Glow(Heart, Hex(0xB02050), 0.06f, 0.5f);
		{
			FWith With(P);
			P.Look.Dome = 0.3f;
			P.Look.Spec = 0.8f;
			P.Fill(Heart, Hex(0x7A1830));
		}
		{
			FClip Clip(P, Heart);
			for (int32 K = 0; K < 3; ++K)
			{
				P.MLine(Smooth({ FV(0.4f + K * 0.08f, 0.36f), FV(0.42f + K * 0.06f, 0.5f), FV(0.38f + K * 0.08f, 0.66f) }), 0.006f, Hex(0x3A0818, 0.5f));
			}
			P.MEllipse(FV(0.34f, 0.4f), 0.07f, 0.04f, Hex(0xFFFFFF, 0.35f), 0.04f);
		}
		// the arm crossing in front, squeezing
		Tentacle(P, { FV(0.06f, 0.9f), FV(0.2f, 0.7f), FV(0.42f, 0.62f), FV(0.66f, 0.52f), FV(0.8f, 0.4f) }, 0.075f, 0.03f, 1.0f, Arm, Sucker);
	}

	void HDItemHermitsHammerV3(FPainter& P)
	{
		// a war hammer made from a heavy whelk lashed to a driftwood haft, its hermit crab tenant peering out of the mouth, claws up
		// the haft
		{
			FWith With(P);
			P.Look.Dome = 0.02f;
			P.Look.Grain = 0.25f;
			P.Fill([](FV Q) { return SdRibbon(Q, Smooth({ FV(0.18f, 0.92f), FV(0.36f, 0.64f), FV(0.56f, 0.36f) }), 0.03f, 0.026f) + Noise2(Q * 20.0f) * 0.004f; }, Hex(0x9A8468));
		}
		Strands(P, FV(0.18f, 0.92f), FV(0.56f, 0.36f), 0.03f, 4, 0.0018f, Hex(0x4A3A28, 0.6f), 1881, 0.006f);
		// the shell head, lying across the end of the haft
		const FWhorlShell S = HDWhorlShell(P, FV(0.86f, 0.12f), FV(0.3f, 0.42f), 0.17f, 6, Hex(0xE8CCA0), Hex(0x8A5030), Hex(0xF8D8C0), Hex(0x3A1A10), 1882);
		// rope lashings binding head to haft
		for (int32 K = 0; K < 3; ++K)
		{
			const FV A = FV(0.5f, 0.36f) + FV(0.03f, -0.018f) * (float)K;
			Rope(P, A + FV(-0.05f, -0.06f), A + FV(0.06f, 0.05f), 0.011f);
		}
		// the hermit crab in the shell mouth: claws up, eyes on stalks
		const FV M = S.ApertureC;
		for (const float Sgn : { -1.0f, 1.0f })
		{
			const FV Base = M + FV(Sgn * 0.03f, -0.01f);
			P.Fill([Base, Sgn](FV Q) { return SdCapsule(Q, Base, Base + FV(Sgn * 0.01f, -0.06f), 0.006f); }, Hex(0xE07040));
			P.Ball(Base + FV(Sgn * 0.01f, -0.065f), 0.012f, Hex(0x1A1A20), 0.9f);
		}
		// the big claw, the smaller claw and a couple of striped walking legs
		const auto Claw = [&P](FV Root, FV Dir, float S2)
		{
			const FV N(-Dir.Y, Dir.X);
			const auto Shape = [Root, Dir, N, S2](FV Q)
			{
				const float Arm = SdCapsule(Q, Root, Root + Dir * 0.05f * S2, 0.016f * S2);
				const float Palm = SdEllipse(FV(FV::DotProduct(Q - Root - Dir * 0.08f * S2, Dir), FV::DotProduct(Q - Root - Dir * 0.08f * S2, N)), FV(0.0f, 0.0f), 0.05f * S2, 0.032f * S2);
				const float Gap = SdPoly(Q, { Root + Dir * 0.1f * S2 + N * 0.004f, Root + Dir * 0.15f * S2 + N * 0.012f, Root + Dir * 0.15f * S2 - N * 0.012f });
				return FMath::Max(SMin(Arm, Palm, 0.01f), -Gap);
			};
			{
				FWith With(P);
				P.Look.Dome = 0.02f;
				P.Look.Spec = 0.6f;
				P.Fill(Shape, Hex(0xE8703A));
			}
			FClip Clip(P, Shape);
			Speckle(P, Root - FV(0.15f, 0.15f), Root + FV(0.15f, 0.15f), 30, 0.003f, 0.006f, Hex(0xFFD0A0, 0.8f), 1883);
		};
		Claw(M + FV(0.02f, 0.02f), Spin(FV(0.0f, 1.0f), -30.0f), 1.2f);
		Claw(M + FV(-0.03f, 0.02f), Spin(FV(0.0f, 1.0f), 25.0f), 0.85f);
		for (int32 K = 0; K < 2; ++K)
		{
			const FV A = M + FV(0.04f + K * 0.02f, 0.0f);
			P.Fill([A, K](FV Q) { return SdRibbon(Q, Smooth({ A, A + FV(0.05f, 0.02f), A + FV(0.07f, 0.07f + K * 0.02f) }), 0.008f, 0.004f); }, Hex(0xD8603A));
		}
	}

	void HDItemConeSnailPearlV3(FPainter& P)
	{
		// a textile cone snail, its tented gold-and-brown shell lying on the sand, reaching its pink proboscis to a pearl, a bead of venom at the tip
		HDMound(P, FV(0.5f, 0.8f), 0.42f, 0.08f, Hex(0xD8BE88), 1891, false);
		const FV C0(0.38f, 0.66f);
		const FV Pearl(0.84f, 0.72f);
		// the pearl rests on the sand ahead of the snail, a soft shadow beneath it
		P.MEllipse(Pearl + FV(0.01f, 0.065f), 0.08f, 0.018f, Hex(0x5A4020, 0.45f), 0.02f);
		HDPearl(P, Pearl, 0.075f, Hex(0xF4F0EC), Hex(0xB0A0C8));
		{
		FNest Place(P, C0, 1.0f, -6.0f);
		// the cone: a long tapering body from a broad shoulder, a low spire behind
		const auto Cone = [](FV Q)
		{
			const float Body = SdRibbon(Q, { FV(-0.18f, 0.0f), FV(0.3f, 0.0f) }, 0.14f, 0.014f);
			const float Spire = SdPoly(Q, { FV(-0.2f, -0.11f), FV(-0.32f, 0.0f), FV(-0.2f, 0.11f) });
			return SMin(Body, Spire, 0.03f);
		};
		P.Mark([&Cone](FV Q) { return Cone(Q - FV(0.0f, 0.03f)); }, Hex(0x3A2A10, 0.35f), 0.03f);
		{
			FWith With(P);
			P.Look.Dome = 0.12f;
			P.Look.Spec = 0.9f;
			P.Fill(Cone, Hex(0x7A4A20));
		}
		{
			FClip Clip(P, Cone);
			// the "textile" pattern: a dense net of small white tents over brown, with bands of gold
			P.Mark([](FV Q) { return FMath::Abs(Q.X - 0.06f) - 0.035f; }, Hex(0xE8A040, 0.55f), 0.02f);
			P.Mark([](FV Q) { return FMath::Abs(Q.X + 0.12f) - 0.03f; }, Hex(0xE8A040, 0.55f), 0.02f);
			for (int32 K = 0; K < 140; ++K)
			{
				const FV At(-0.22f + Rand01(1892, K) * 0.5f, (Rand01(1893, K) - 0.5f) * 0.28f);
				const float S = 0.005f + Rand01(1894, K) * 0.007f;
				P.MPoly({ At + FV(-S, S * 0.6f), At + FV(0.0f, -S), At + FV(S, S * 0.6f) }, Hex(0xFFF4E0, 0.95f));
			}
			for (int32 K = 0; K < 3; ++K)
			{
				P.Mark([K](FV Q) { return FMath::Max(FMath::Abs(Q.X + 0.29f - K * 0.03f + FMath::Abs(Q.Y) * 0.8f) - 0.002f, Q.X + 0.19f); }, Hex(0x3A1A08, 0.7f));
			}
			P.MEllipse(FV(-0.02f, -0.07f), 0.22f, 0.02f, Hex(0xFFFFFF, 0.5f), 0.02f);
		}
		}
		// the proboscis snaking out of the narrow end and down to touch the pearl, a bead of venom on its tip
		const FV Nose = C0 + Spin(FV(0.3f, 0.0f), -6.0f);
		const FV Touch = Pearl + Spin(FV(-0.075f, 0.0f), 30.0f);
		const TArray<FV> Prob = Smooth({ Nose, Nose + FV(0.04f, 0.0f), Touch + FV(-0.05f, -0.045f), Touch + FV(-0.008f, -0.004f) });
		{
			FWith With(P);
			P.Look.Dome = 0.01f;
			P.Look.Spec = 0.6f;
			P.Fill([Prob](FV Q) { return SdRibbon(Q, Prob, 0.016f, 0.008f); }, Hex(0xF08AA0));
		}
		P.MLine(Prob, 0.002f, Hex(0xFFD0DA, 0.7f));
		P.Ball(Touch + FV(-0.004f, -0.002f), 0.012f, Hex(0x7AE040), 1.0f);
	}

	void HDItemTorpedoRayPearlV3(FPainter& P)
	{
		// a torpedo ray seen from above, its round disc mottled brown, the paired electric organs glowing through, a pearl carried on its back, sparks crackling at its rim
		FNest Place(P, FV(0.5f, 0.5f), 1.0f, -20.0f);
		const FV C(0.0f, -0.08f);
		// the long whip of a tail
		{
			FWith With(P);
			P.Look.Dome = 0.01f;
			P.Fill([C](FV Q) { return SdRibbon(Q, Smooth({ C + FV(0.0f, 0.14f), C + FV(0.02f, 0.34f), C + FV(-0.03f, 0.5f), C + FV(0.02f, 0.62f) }), 0.03f, 0.003f); }, Hex(0x8A6A4A));
		}
		for (const float Sgn : { -1.0f, 1.0f })
		{
			P.Fill([C, Sgn](FV Q) { return SdEllipse(Q, C + FV(Sgn * 0.07f, 0.17f), 0.06f, 0.035f); }, Hex(0x8A6A4A));
		}
		// the disc
		const TArray<FV> DiscPts = Smooth({ C + FV(0.0f, -0.2f), C + FV(0.2f, -0.15f), C + FV(0.44f, 0.02f), C + FV(0.22f, 0.1f), C + FV(0.0f, 0.18f), C + FV(-0.22f, 0.1f), C + FV(-0.44f, 0.02f), C + FV(-0.2f, -0.15f) }, true, 3);
		const auto Disc = [&DiscPts](FV Q) { return SdPoly(Q, DiscPts); };
		{
			FWith With(P);
			P.Look.Dome = 0.025f;
			P.Look.Spec = 0.5f;
			P.Fill(Disc, Hex(0x9A7A58));
		}
		{
			FClip Clip(P, Disc);
			// the raised body down the middle, the thin wings either side, rippled at the edge
			P.MEllipse(C + FV(0.0f, 0.0f), 0.13f, 0.2f, Hex(0xC8A880, 0.5f), 0.08f);
			P.MEllipse(C + FV(0.03f, 0.04f), 0.12f, 0.2f, Hex(0x3A2410, 0.2f), 0.08f);
			P.Mark([&Disc](FV Q) { return FMath::Abs(Disc(Q) + 0.015f) - 0.006f + FMath::Sin(Q.X * 80.0f) * 0.003f; }, Hex(0x5A3A20, 0.35f), 0.006f);
			for (int32 K = 0; K < 40; ++K)
			{
				const FV At = C + FV((Rand01(1901, K) - 0.5f) * 0.66f, (Rand01(1902, K) - 0.5f) * 0.54f);
				P.Mark([At](FV Q) { return SdCircle(Q, At, 0.018f) + Noise2(Q * 60.0f) * 0.008f; }, Hex(0x4A3020, 0.45f), 0.008f);
			}
			// the electric organs: two kidney shapes either side of the head, lit from within
			for (const float Sgn : { -1.0f, 1.0f })
			{
				const FV O = C + FV(Sgn * 0.17f, -0.03f);
								P.Mark([O](FV Q) { return SdEllipse(Q, O, 0.07f, 0.12f); }, Hex(0xBFF0FF, 0.1f), 0.04f);
			}
			// small eyes and spiracles near the front
			for (const float Sgn : { -1.0f, 1.0f })
			{
				P.Ball(C + FV(Sgn * 0.04f, -0.1f), 0.009f, Hex(0x2A2018), 0.8f);
				P.MEllipse(C + FV(Sgn * 0.045f, -0.07f), 0.008f, 0.005f, Hex(0x2A1A10, 0.8f));
			}
			P.MEllipse(C + FV(-0.1f, -0.12f), 0.12f, 0.05f, Hex(0xFFFFFF, 0.2f), 0.05f);
		}
		// the pearl riding on its back
		HDPearl(P, C + FV(0.0f, 0.05f), 0.065f, Hex(0xF4F0EC), Hex(0xA0B0D0));
		// sparks crackling off the rim
		HDLightning(P, C + FV(-0.3f, -0.1f), C + FV(-0.44f, -0.26f), 1905, 0.004f, Hex(0x60D8FF));
		HDLightning(P, C + FV(0.3f, -0.1f), C + FV(0.44f, -0.26f), 1906, 0.004f, Hex(0x60D8FF));
		HDLightning(P, C + FV(0.32f, 0.1f), C + FV(0.46f, 0.18f), 1907, 0.003f, Hex(0x60D8FF));
	}

	void HDItemWhalesongCharmV3(FPainter& P)
	{
		// a whale's-tail charm carved from old ivory, scrimshawed with a whale and waves, hung on a waxed cord with a single blue bead
		// the cord
		P.MLine(Smooth({ FV(0.16f, 0.06f), FV(0.32f, 0.16f), FV(0.5f, 0.22f) }), 0.006f, Hex(0x6A4A2A));
		P.MLine(Smooth({ FV(0.84f, 0.06f), FV(0.68f, 0.16f), FV(0.5f, 0.22f) }), 0.006f, Hex(0x6A4A2A));
		P.Ball(FV(0.5f, 0.24f), 0.03f, Hex(0x2A6AC8), 1.0f);
		// the tail: a fluke on a short stock
		const auto Fluke = [](FV Q)
		{
			const FV L = Q - FV(0.5f, 0.56f);
			const float Stock = SdRibbon(Q, { FV(0.5f, 0.32f), FV(0.5f, 0.58f) }, 0.045f, 0.07f);
			TArray<FV> Wing;
			const auto Mid = [](float X) { return 0.6f + X * X * 0.3f - FMath::Abs(X) * 0.08f; };
			const auto Half = [](float X) { return 0.008f + 0.075f * (1.0f - FMath::Pow(FMath::Abs(X) / 0.38f, 2.0f)); };
			for (int32 K = 0; K <= 24; ++K)
			{
				const float X = (K / 24.0f - 0.5f) * 0.76f;
				Wing.Add(FV(0.5f + X, Mid(X) - Half(X)));
			}
			for (int32 K = 24; K >= 0; --K)
			{
				const float X = (K / 24.0f - 0.5f) * 0.76f;
				const float Notch = FMath::Exp(-X * X * 600.0f) * 0.04f;
				Wing.Add(FV(0.5f + X, Mid(X) + Half(X) - Notch));
			}
			(void)L;
			return SMin(Stock, SdPoly(Q, Wing), 0.04f);
		};
		P.Fill([](FV Q) { return FMath::Abs(SdCircle(Q, FV(0.5f, 0.29f), 0.025f)) - 0.008f; }, Hex(0xE8DCC0));
		{
			FWith With(P);
			P.Look.Dome = 0.06f;
			P.Look.Spec = 0.6f;
			P.Look.Grain = 0.15f;
			P.Fill(Fluke, Hex(0xF0E6CE));
		}
		{
			FClip Clip(P, Fluke);
			// age: a yellowed patina and fine cracks along the grain
			P.MEllipse(FV(0.5f, 0.72f), 0.4f, 0.1f, Hex(0xC8A870, 0.35f), 0.08f);
			for (int32 K = 0; K < 6; ++K)
			{
				const float X = 0.2f + K * 0.12f;
				P.MLine(Smooth({ FV(X, 0.58f), FV(X + 0.02f, 0.64f), FV(X + 0.01f, 0.7f) }), 0.0012f, Hex(0x8A7040, 0.5f));
			}
			// the scrimshaw: a sperm whale and waves cut in fine black lines
			P.MLine(Smooth({ FV(0.34f, 0.63f), FV(0.42f, 0.6f), FV(0.56f, 0.6f), FV(0.62f, 0.63f), FV(0.56f, 0.66f), FV(0.42f, 0.66f), FV(0.34f, 0.63f) }), 0.003f, Hex(0x1A1410, 0.85f));
			P.MLine(Smooth({ FV(0.62f, 0.63f), FV(0.66f, 0.6f), FV(0.68f, 0.62f), FV(0.66f, 0.65f), FV(0.62f, 0.63f) }), 0.0025f, Hex(0x1A1410, 0.85f));
			P.MCircle(FV(0.4f, 0.625f), 0.004f, Hex(0x1A1410));
			for (int32 K = 0; K < 3; ++K)
			{
				P.Mark([K](FV Q) { return FMath::Max(FMath::Abs(Q.Y - 0.7f - K * 0.025f - FMath::Sin(Q.X * 50.0f) * 0.008f) - 0.0018f, FMath::Abs(Q.X - 0.5f) - 0.26f + K * 0.05f); }, Hex(0x1A1410, 0.75f));
			}
			for (int32 K = 0; K < 4; ++K)
			{
				P.MLine({ FV(0.48f, 0.35f + K * 0.05f), FV(0.52f, 0.37f + K * 0.05f) }, 0.0015f, Hex(0x1A1410, 0.6f));
			}
		}
		P.MEllipse(FV(0.38f, 0.56f), 0.08f, 0.02f, Hex(0xFFFFFF, 0.4f), 0.02f);
	}

	/** A scallop valve seen from above: radiating ribs, growth bands and ears at the hinge */
	void HDScallopValve(FPainter& P, FV Hinge, float R, const FLinearColor& Color, const FLinearColor& Band, int32 Seed)
	{
		const auto Fan = [Hinge, R](FV Q)
		{
			const FV L = Q - Hinge;
			const float Ang = FMath::Atan2(L.X, -L.Y);
			const float Scallop = FMath::Abs(FMath::Sin(Ang * 9.0f)) * R * 0.04f;
			return FMath::Max(L.Size() - R + Scallop, (FMath::Abs(Ang) - 0.95f) * L.Size());
		};
		const auto Ears = [Hinge, R](FV Q) { return SdPoly(Q, { Hinge + FV(-0.2f, -0.16f) * R, Hinge + FV(0.2f, -0.16f) * R, Hinge + FV(0.34f, 0.02f) * R, Hinge + FV(-0.34f, 0.02f) * R }) - R * 0.02f; };
		const auto Valve = [&Fan, &Ears, R](FV Q) { return SMin(Fan(Q), Ears(Q), R * 0.06f); };
		P.Mark([&Valve](FV Q) { return Valve(Q - FV(0.01f, 0.02f)); }, Hex(0x06121A, 0.3f), 0.03f);
		{
			FWith With(P);
			P.Look.Dome = R * 0.15f;
			P.Look.Spec = 0.7f;
			P.Fill(Valve, Color);
		}
		FClip Clip(P, Valve);
		for (int32 K = -8; K <= 8; ++K)
		{
			const float Ang = K * (0.95f / 8.5f);
			const FV Dir(FMath::Sin(Ang), -FMath::Cos(Ang));
			const FV Sh = FV(-Dir.Y, Dir.X) * R * 0.025f;
			P.Mark([Hinge, Dir, R](FV Q) { return SdCapsule(Q, Hinge + Dir * R * 0.12f, Hinge + Dir * R, R * 0.008f) - FV::DotProduct(Q - Hinge, Dir) * 0.04f; }, Alpha(Tone(Color, 0.55f), 0.55f), R * 0.016f);
			P.Mark([Hinge, Dir, Sh, R](FV Q) { return SdCapsule(Q, Hinge + Dir * R * 0.12f + Sh, Hinge + Dir * R + Sh, R * 0.004f); }, Hex(0xFFFFFF, 0.4f), R * 0.008f);
		}
		for (int32 K = 1; K <= 7; ++K)
		{
			const float Rr = R * (0.12f + K * 0.12f);
			P.Mark([Hinge, Rr, R](FV Q) { return FMath::Abs((Q - Hinge).Size() - Rr) - R * 0.004f; }, Alpha(Tone(Color, 0.6f), 0.35f));
		}
		P.Mark([Hinge, R](FV Q) { return FMath::Abs((Q - Hinge).Size() - R * 0.62f) - R * 0.09f; }, Alpha(Band, 0.4f), R * 0.05f);
		P.Mark([Hinge, R](FV Q) { return FMath::Abs((Q - Hinge).Size() - R * 0.36f) - R * 0.05f; }, Alpha(Band, 0.3f), R * 0.04f);
		Speckle(P, Hinge - FV(R, R), Hinge + FV(R, 0.0f), 60, R * 0.004f, R * 0.01f, Hex(0xFFFFFF, 0.4f), Seed);
		P.MLine({ Hinge + FV(-0.34f, 0.0f) * R, Hinge + FV(0.34f, 0.0f) * R }, R * 0.006f, Alpha(Tone(Color, 0.4f), 0.6f));
		P.MEllipse(Hinge + FV(-0.3f, -0.62f) * R, R * 0.2f, R * 0.1f, Hex(0xFFFFFF, 0.35f), R * 0.1f);
	}

	/** A sea star of any colour, mid-crawl, with rows of knobs */
	void HDSeaStar(FPainter& P, FV C, float S, const FLinearColor& Color, const FLinearColor& Knob, int32 Seed)
	{
		TArray<TArray<FV>> Arms;
		for (int32 K = 0; K < 5; ++K)
		{
			const float A = -90.0f + K * 72.0f + (Rand01(Seed, K) - 0.5f) * 8.0f;
			const float Len = S * (0.86f + Rand01(Seed + 1, K) * 0.14f);
			const float Curl = (Rand01(Seed + 2, K) - 0.5f) * 24.0f;
			const FV D0 = Spin(FV(1.0f, 0.0f), A);
			const FV D1 = Spin(FV(1.0f, 0.0f), A + Curl);
			Arms.Add(Smooth({ C, C + D0 * Len * 0.5f, C + D0 * Len * 0.75f + D1 * Len * 0.05f, C + D1 * Len }));
		}
		const auto Star = [&Arms, C, S](FV Q)
		{
			float D = SdCircle(Q, C, S * 0.25f);
			for (const TArray<FV>& Arm : Arms)
			{
				D = SMin(D, SdRibbon(Q, Arm, S * 0.22f, S * 0.05f), S * 0.12f);
			}
			return D;
		};
		P.Mark([&Star](FV Q) { return Star(Q - FV(0.012f, 0.025f)); }, Hex(0x06121A, 0.35f), 0.03f);
		{
			FWith With(P);
			P.Look.Dome = S * 0.18f;
			P.Look.Spec = 0.7f;
			P.Fill(Star, Color);
		}
		FClip Clip(P, Star);
		for (const TArray<FV>& Arm : Arms)
		{
			for (int32 J = 0; J < 8; ++J)
			{
				FV Pos, Tan;
				const float T = 0.14f + J * 0.11f;
				Along(Arm, T, Pos, Tan);
				const FV N(-Tan.Y, Tan.X);
				const float W = FMath::Lerp(S * 0.22f, S * 0.05f, T);
				for (const float Off : { -0.5f, 0.0f, 0.5f })
				{
					P.Ball(Pos + N * W * Off, S * (Off == 0.0f ? 0.035f : 0.024f) * (1.0f - T * 0.6f), Knob, 0.7f);
				}
			}
		}
		P.MEllipse(C + FV(-0.2f, -0.2f) * S, S * 0.25f, S * 0.12f, Hex(0xFFFFFF, 0.3f), S * 0.12f);
	}

	bool PaintItemSlotHD(FPainter& P, int32 Slot)
	{
		switch ((ETDItemSlot)Slot)
		{
		case ETDItemSlot::Shell:
			// a peach-and-coral scallop shell
			HDScallopValve(P, FV(0.5f, 0.82f), 0.6f, Hex(0xF4B08A), Hex(0xE8604A), 1911);
			return true;
		case ETDItemSlot::Pearl:
		{
			// a pearl resting in an open oyster, its mother-of-pearl lining shimmering
			const auto Valve = [](FV Q, FV C, float Rx, float Ry) { return SdEllipse(Q, C, Rx, Ry) + Noise2(Q * 18.0f) * 0.012f; };
			const auto Lower = [&Valve](FV Q) { return Valve(Q, FV(0.5f, 0.64f), 0.42f, 0.22f); };
			{
				FWith With(P);
				P.Look.Dome = 0.04f;
				P.Look.Grain = 0.3f;
				P.Fill(Lower, Hex(0x8A7A8A));
			}
			{
				FClip Clip(P, Lower);
				P.Mark([&Valve](FV Q) { return Valve(Q, FV(0.5f, 0.62f), 0.37f, 0.17f); }, Hex(0xD8D0E8));
				P.MEllipse(FV(0.4f, 0.6f), 0.16f, 0.05f, Hex(0xFFB8D8, 0.4f), 0.05f);
				P.MEllipse(FV(0.62f, 0.66f), 0.14f, 0.05f, Hex(0x9AF0E0, 0.35f), 0.05f);
				P.MEllipse(FV(0.5f, 0.7f), 0.3f, 0.06f, Hex(0x6A5A8A, 0.35f), 0.05f);
			}
			const auto Upper = [&Valve](FV Q) { return Valve(Q, FV(0.5f, 0.4f), 0.4f, 0.15f); };
			{
				FWith With(P);
				P.Look.Dome = 0.03f;
				P.Look.Grain = 0.3f;
				P.Fill([&Upper](FV Q) { return FMath::Max(Upper(Q), Q.Y - 0.5f); }, Hex(0x7A6A7A));
			}
			P.Mark([&Valve](FV Q) { return FMath::Max(Valve(Q, FV(0.5f, 0.42f), 0.35f, 0.11f), Q.Y - 0.5f); }, Hex(0xC8C0D8));
			HDPearl(P, FV(0.5f, 0.54f), 0.16f, Hex(0xF4F0EC), Hex(0xB0A0C8));
			return true;
		}
		case ETDItemSlot::Charm:
		{
			// a sea-glass charm: a rounded teal stone wrapped in gold wire, hung from a ring
			const FV C(0.5f, 0.56f);
			const auto SeaStone = [C](FV Q) { return SdEllipse(Q, C, 0.22f, 0.27f) + Noise2(Q * 6.0f) * 0.02f; };
			P.Glow(SeaStone, Hex(0x40D0C0), 0.05f, 0.35f);
			{
				FWith With(P);
				P.Look.Dome = 0.12f;
				P.Look.Spec = 0.9f;
				P.Fill(SeaStone, Hex(0x3AB8A8));
			}
			{
				FClip Clip(P, SeaStone);
				// frosted, sea-worn surface and light caught inside
				Speckle(P, C - FV(0.25f, 0.3f), C + FV(0.25f, 0.3f), 140, 0.002f, 0.006f, Hex(0xE0FFF8, 0.3f), 1912);
				P.MEllipse(C + FV(0.06f, 0.08f), 0.12f, 0.14f, Hex(0x9AF0E0, 0.35f), 0.08f);
				P.MEllipse(C + FV(-0.08f, -0.12f), 0.06f, 0.03f, Hex(0xFFFFFF, 0.6f), 0.03f);
			}
			// gold wire wrapped round it in loose spirals
			for (int32 K = 0; K < 3; ++K)
			{
				const float Y = C.Y - 0.14f + K * 0.12f;
				const TArray<FV> Wire = Smooth({ FV(C.X - 0.21f, Y + 0.04f), FV(C.X - 0.1f, Y - 0.02f + K * 0.01f), FV(C.X + 0.1f, Y + 0.03f), FV(C.X + 0.21f, Y - 0.03f) });
				P.Mark([Wire](FV Q) { return SdPolyline(Q, Wire, 0.009f); }, Hex(0x7A5A12));
				P.Mark([Wire](FV Q) { return SdPolyline(Q, Wire, 0.0065f); }, Hex(0xE8B040));
				P.Mark([Wire](FV Q) { TArray<FV> W2; for (const FV& V : Wire) W2.Add(V - FV(0.0f, 0.002f)); return SdPolyline(Q, W2, 0.002f); }, Hex(0xFFF0B0, 0.8f));
			}
			{
				FWith With(P);
				P.Look.Spec = 0.9f;
				P.Fill([C](FV Q) { return FMath::Abs(SdCircle(Q, FV(C.X, 0.22f), 0.045f)) - 0.011f; }, Hex(0xE8B040));
			}
			P.MLine(Smooth({ FV(C.X - 0.02f, 0.26f), FV(C.X, 0.3f), FV(C.X + 0.02f, 0.26f) }), 0.007f, Hex(0xE8B040));
			return true;
		}
		default:
			return false;
		}
	}

	bool PaintMiscHD(FPainter& P, int32 Icon)
	{
		switch ((ETDMiscIcon)Icon)
		{
		case ETDMiscIcon::Pearl:
			HDPearl(P, FV(0.5f, 0.5f), 0.42f, Hex(0xF4F0EC), Hex(0xA8A0C8));
			return true;
		case ETDMiscIcon::Wave:
			HDWave(P, Hex(0x2A9AC8), Hex(0x0A3A6A), 1921);
			return true;
		case ETDMiscIcon::SkillPoint:
			// a golden sea star, glowing
			P.Glow([](FV Q) { return SdCircle(Q, FV(0.5f, 0.5f), 0.2f); }, Hex(0xFFE070), 0.16f, 0.5f);
			HDSeaStar(P, FV(0.5f, 0.52f), 0.42f, Hex(0xF0B030), Hex(0xFFF0B0), 1922);
			return true;
		default:
			return false;
		}
	}

	bool PaintItemKindHD(FPainter& P, int32 Kind)
	{
		switch (Kind)
		{
		case 5:	HDItemHermitsHammerV3(P); return true;
		case 8:		HDItemTitanClam(P); return true;
		case 18:	HDItemConeSnailPearlV3(P); return true;
		case 19:	HDItemTorpedoRayPearlV3(P); return true;
		case 33:	HDItemWhalesongCharmV3(P); return true;
		case 1:	HDItemSharpenedCowrie(P); return true;
		case 2:	HDItemSpiralWhelk(P); return true;
		case 4:	HDItemScallopFan(P); return true;
		case 6:	HDItemRazorClam(P); return true;
		case 7:	HDItemIroncladNautilus(P); return true;
		case 9:	HDItemTritonsTrumpet(P); return true;
		case 10:	HDItemMurexOfEndings(P); return true;
		case 11:	HDItemHeartOfTheLeviathan(P); return true;
		case 12:	HDItemCrownOfBrine(P); return true;
		case 13:	HDItemLanternPearl(P); return true;
		case 14:	HDItemBlackPearl(P); return true;
		case 15:	HDItemColdSeepPearl(P); return true;
		case 17:	HDItemBaroquePearl(P); return true;
		case 16:	HDItemSeafoamPearl(P); return true;
		case 20:	HDItemMoonPearl(P); return true;
		case 21:	HDItemSirensTear(P); return true;
		case 22:	HDItemRedTidePearl(P); return true;
		case 23:	HDItemPearlOfTheAbyss(P); return true;
		case 24:	HDItemKrakensEye(P); return true;
		case 25:	HDItemDriftwoodTotem(P); return true;
		case 26:	HDItemSandDollar(P); return true;
		case 27:	HDItemLuckyStarfish(P); return true;
		case 28:	HDItemKelpBraid(P); return true;
		case 29:	HDItemSunkenCoinPurse(P); return true;
		case 30:	HDItemScholarsCoral(P); return true;
		case 31:	HDItemClearwaterLens(P); return true;
		case 32:	HDItemPiratesDoubloon(P); return true;
		case 34:	HDItemFlyingFishFin(P); return true;
		case 35:	HDItemTidecallersConch(P); return true;
		case 36:	HDItemSirensLocket(P); return true;
		case 37:	HDItemTridentOfTheDeepKing(P); return true;
		case 38:	HDItemLeviathansJaw(P); return true;
		case 39:	HDItemAbyssalAnchor(P); return true;
		case 40:	HDItemTearOfTheMoon(P); return true;
		case 41:	HDItemHeartOfCoral(P); return true;
		case 42:	HDItemBlackTidePearl(P); return true;
		case 43:	HDItemCompassOfTides(P); return true;
		case 44:	HDItemGildedConch(P); return true;
		case 45:	HDItemKrakensHeart(P); return true;
		default:	return false;
		}
	}
