// Aquatic Tower Defense - HD icon artwork (atd.DetailedIcons 2), included by TDIcons.cpp inside its anonymous namespace.
// Each subject is drawn as a few organic forms melted together (smooth unions), so limbs grow out of bodies and
// silhouettes read as one creature rather than separate shapes; detail is then painted inside those forms.

	/** Smooth union of two distances: K (icon units) is how far the blend reaches */
	float SMin(float A, float B, float K)
	{
		const float H = FMath::Max(K - FMath::Abs(A - B), 0.0f) / K;
		return FMath::Min(A, B) - H * H * K * 0.25f;
	}

	/** Smooth subtraction: carves B out of A with a rounded edge */
	float SSub(float A, float B, float K)
	{
		return -SMin(-A, B, K);
	}

	/** Mirrors a point across the vertical centre line of the icon (for symmetric creatures) */
	FV MirrorX(FV Q, float Cx = 0.5f)
	{
		return FV(Cx - FMath::Abs(Q.X - Cx), Q.Y);
	}

	/** Small shiny granules with lit tops scattered inside a shape */
	void Granules(FPainter& P, TFunctionRef<float(FV)> Inside, FV Min, FV Max, int32 Count, float R0, float R1, const FLinearColor& Dark, const FLinearColor& Lit, int32 Seed)
	{
		for (int32 i = 0; i < Count; ++i)
		{
			const FV At(FMath::Lerp(Min.X, Max.X, Rand01(Seed, i * 3)), FMath::Lerp(Min.Y, Max.Y, Rand01(Seed, i * 3 + 1)));
			if (Inside(At) > -R1)
			{
				continue;
			}
			const float R = FMath::Lerp(R0, R1, Rand01(Seed, i * 3 + 2));
			P.MCircle(At + FV(R * 0.25f, R * 0.3f), R, Dark);
			P.MCircle(At, R * 0.8f, Alpha(Lit, Lit.A * 0.6f));
			P.MCircle(At - FV(R * 0.3f, R * 0.35f), R * 0.32f, Hex(0xFFFFFF, 0.55f));
		}
	}

	/** A glossy cartoon eye: white ball, coloured iris, pupil, two catch lights. Look is the pupil offset (units of R). */
	void HDEye(FPainter& P, FV C, float R, const FLinearColor& Iris, FV Look = FV(0.0f, 0.0f), float IrisScale = 0.62f)
	{
		P.Ball(C, R, Hex(0xF6FAFC), 0.3f);
		FClip Clip(P, [C, R](FV Q) { return SdCircle(Q, C, R); });
		const FV I = C + Look * R;
		P.MCircle(I, R * IrisScale, Iris);
		P.Mark([I, R, IrisScale](FV Q) { return FMath::Abs(SdCircle(Q, I, R * IrisScale)) - R * 0.05f; }, Alpha(Tone(Iris, 0.45f), 0.9f));
		for (int32 k = 0; k < 14; ++k)
		{
			const FV Dir = Spin(FV(1.0f, 0.0f), k * 360.0f / 14.0f);
			P.MCapsule(I + Dir * R * IrisScale * 0.5f, I + Dir * R * IrisScale * 0.9f, R * 0.025f, Alpha(Tone(Iris, 1.5f), 0.45f));
		}
		P.MCircle(I, R * IrisScale * 0.52f, Hex(0x081014));
		P.MEllipse(C + FV(0.0f, R * 0.55f), R * 0.9f, R * 0.4f, Hex(0x5A7080, 0.18f), R * 0.25f);
		P.MCircle(I + FV(-R * 0.22f, -R * 0.24f), R * 0.17f, Hex(0xFFFFFF, 0.95f));
		P.MCircle(I + FV(R * 0.2f, R * 0.2f), R * 0.07f, Hex(0xFFFFFF, 0.7f));
	}

	/**
	 *  An angry glare over an eye: a heavy lid slanting down towards the inner side (Inner = +1 or -1 in X),
	 *  with a dark lash edge, and a raised brow ridge above it angled the same way.
	 */
	void HDAngryLid(FPainter& P, FV C, float R, float Inner, const FLinearColor& Skin, float Slant = 0.75f, float Drop = 0.05f)
	{
		const auto Lid = [C, R, Inner, Slant, Drop](FV Q)
		{
			const FV L = Q - C;
			// the lid edge: a line that sinks towards the inner corner
			const float Edge = (L.Y - (-R * Drop + Inner * L.X * Slant)) / FMath::Sqrt(1.0f + Slant * Slant);
			return FMath::Max(SdCircle(Q, C, R * 1.02f), Edge);
		};
		{
			FWith With(P);
			P.Look.Dome = 0.15f;
			P.Look.Spec = 0.4f;
			P.Fill(Lid, Skin);
		}
		{
			FClip Clip(P, [C, R](FV Q) { return SdCircle(Q, C, R * 1.02f); });
			P.Mark([C, R, Inner, Slant, Drop](FV Q)
			{
				const FV L = Q - C;
				return FMath::Abs((L.Y - (-R * Drop + Inner * L.X * Slant)) / FMath::Sqrt(1.0f + Slant * Slant)) - R * 0.07f;
			}, Hex(0x10080A, 0.95f));
		}
		// the brow: a thick wedge over the eye, low and heavy on the inner side
		const FV In = C + FV(Inner * R * 1.05f, -R * 0.55f), Out = C + FV(-Inner * R * 1.1f, -R * 1.45f);
		const TArray<FV> Brow = Smooth({ Out, (In + Out) * 0.5f + FV(0.0f, -R * 0.12f), In });
		P.Fill([Brow, R](FV Q) { return SdRibbon(Q, Brow, R * 0.18f, R * 0.34f); }, Tone(Skin, 0.6f));
	}

	// ------------------------------------------------------------------ creatures

	void HDCrab(FPainter& P)
	{
		const FLinearColor Shell = Hex(0xEE5E24), ShellDeep = Hex(0xA8381A), ShellPale = Hex(0xFFB27A), Under = Hex(0xF6C89A);

		// ---- walking legs: three segments each, jointed, ending in a sharp dark dactyl
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const float F = (float)Side;
			for (int32 i = 0; i < 4; ++i)
			{
				const float Y = 0.53f + i * 0.05f;
				const float Out = 0.18f - i * 0.012f;
				// front legs reach forwards, back legs trail behind
				const float Sweep = (i - 1.5f) * 0.045f;
				const FV Root(0.5f + F * 0.2f, Y);
				const FV Knee(0.5f + F * (0.2f + Out), Y - 0.075f + Sweep * 0.6f);
				const FV Ankle(0.5f + F * (0.24f + Out + 0.03f - FMath::Abs(Sweep) * 0.3f), Y + 0.04f + Sweep * 1.6f);
				const FV Tip(0.5f + F * (0.22f + Out + 0.02f - FMath::Abs(Sweep) * 0.4f), Y + 0.11f + Sweep * 2.2f);
				const auto Leg = [=](FV Q)
				{
					const float Upper = SdCapsule(Q, Root, Knee, 0.026f - i * 0.002f);
					const float Lower = SdRibbon(Q, { Knee, FMath::Lerp(Knee, Ankle, 0.5f) + FV(F * 0.006f, 0.0f), Ankle }, 0.02f, 0.016f);
					const float Claw = SdRibbon(Q, { Ankle, FMath::Lerp(Ankle, Tip, 0.5f) + FV(F * 0.008f, 0.0f), Tip }, 0.014f, 0.003f);
					return SMin(SMin(Upper, Lower, 0.012f), Claw, 0.01f);
				};
				{
					FWith With(P);
					P.Look.Dome = 0.05f;
					P.Look.Spec = 0.45f;
					P.Fill(Leg, i % 2 ? Tone(Shell, 0.92f) : Shell);
				}
				FClip Clip(P, Leg);
				// pale underside, dark joint bands, bristles and a dark claw tip
				P.MCapsule(Root + FV(0.0f, 0.012f), Knee + FV(0.0f, 0.012f), 0.012f, Alpha(Under, 0.55f));
				P.MCircle(Knee, 0.016f, Alpha(ShellDeep, 0.55f));
				P.MCircle(Ankle, 0.013f, Alpha(ShellDeep, 0.55f));
				P.MCapsule(Ankle + (Tip - Ankle) * 0.45f, Tip + (Tip - Ankle) * 0.2f, 0.016f, Hex(0x3A140A, 0.9f));
				for (int32 k = 1; k < 5; ++k)
				{
					const FV At = FMath::Lerp(Root, Knee, k / 5.0f);
					P.MCapsule(At + FV(0.0f, -0.018f), At + FV(F * 0.006f, -0.03f), 0.0022f, Hex(0x5A200C, 0.9f));
				}
			}
		}

		// ---- chelipeds: arm segments flowing into a swollen claw with a real gap between the fingers
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const float F = (float)Side;
			auto X = [F](float U, float V) { return FV(0.5f + F * U, V); };
			const FV Shoulder = X(0.19f, 0.5f), Elbow = X(0.3f, 0.43f), Wrist = X(0.3f, 0.34f);
			const FV Palm = X(0.29f, 0.24f);
			const auto Arm = [=](FV Q)
			{
				const float Merus = SdRibbon(Q, { Shoulder, X(0.27f, 0.47f), Elbow }, 0.036f, 0.032f);
				const float Carpus = SdRibbon(Q, { Elbow, X(0.31f, 0.37f), Wrist }, 0.034f, 0.03f);
				return SMin(Merus, Carpus, 0.02f);
			};
			const auto Hand = [=](FV Q)
			{
				// the palm, tilted inwards, and two curved fingers that meet at the tips
				const FV L = Spin(Q - Palm, F * -18.0f);
				const float PalmD = SdEllipse(L, FV(0.0f, 0.0f), 0.1f, 0.105f);
				const float Fixed = SdRibbon(L, { FV(F * 0.05f, -0.06f), FV(F * 0.05f, -0.13f), FV(F * 0.012f, -0.2f) }, 0.042f, 0.009f);
				const float Dactyl = SdRibbon(L, { FV(F * -0.055f, -0.055f), FV(F * -0.075f, -0.13f), FV(F * -0.03f, -0.195f) }, 0.036f, 0.008f);
				// the gap between the fingers is cut out, so it shows whatever lies behind
				const float Gap = SdEllipse(L, FV(F * -0.008f, -0.13f), 0.026f, 0.06f);
				return SSub(SMin(PalmD, FMath::Min(Fixed, Dactyl), 0.03f), Gap, 0.01f);
			};
			{
				FWith With(P);
				P.Look.Dome = 0.08f;
				P.Look.Spec = 0.5f;
				P.Fill(Arm, Tone(Shell, 0.95f));
			}
			{
				FClip Clip(P, Arm);
				P.MCapsule(Shoulder + FV(0.0f, 0.016f), Elbow + FV(F * 0.012f, 0.012f), 0.012f, Alpha(Under, 0.5f));
				P.MCircle(Elbow, 0.022f, Alpha(ShellDeep, 0.45f));
				for (int32 k = 0; k < 4; ++k)
				{
					const FV At = FMath::Lerp(Elbow, Wrist, 0.2f + k * 0.2f) + FV(-F * 0.028f, 0.0f);
					P.MCircle(At, 0.0055f, Hex(0xFFD8B8, 0.95f));
				}
			}
			{
				FWith With(P);
				P.Look.Dome = 0.1f;
				P.Look.Spec = 0.7f;
				P.Fill(Hand, Shell);
			}
			FClip Clip(P, Hand);
			const FV Up = Spin(FV(0.0f, -1.0f), F * 18.0f);
			const FV Across = Spin(FV(F, 0.0f), F * 18.0f);
			// a pale inner face, a dark rich back, rows of tubercles, teeth along the biting edges and dark finger tips
			P.MEllipse(Palm - Across * 0.045f + Up * 0.01f, 0.05f, 0.09f, Alpha(ShellPale, 0.6f), 0.035f);
			P.MEllipse(Palm + Across * 0.065f + Up * 0.02f, 0.04f, 0.09f, Alpha(ShellDeep, 0.5f), 0.035f);
			for (int32 k = 0; k < 10; ++k)
			{
				const FV At = Palm + Across * (0.045f - (k % 3) * 0.03f) + Up * (-0.07f + k * 0.013f);
				P.MCircle(At, 0.006f, Hex(0x9A3012, 0.8f));
				P.MCircle(At - FV(0.002f, 0.002f), 0.003f, Hex(0xFFD0A8, 0.9f));
			}
			for (int32 k = 0; k < 5; ++k)
			{
				P.MCircle(Palm + Up * (0.09f + k * 0.02f) + Across * 0.024f, 0.007f, Hex(0xFFF0E0, 0.95f));
				P.MCircle(Palm + Up * (0.085f + k * 0.02f) - Across * 0.04f, 0.006f, Hex(0xFFF0E0, 0.95f));
			}
			P.MCircle(Palm + Up * 0.2f + Across * 0.012f, 0.035f, Hex(0x2E0E06, 0.92f));
			P.MCircle(Palm + Up * 0.195f - Across * 0.03f, 0.032f, Hex(0x2E0E06, 0.92f));
		}

		// ---- the carapace: broad, with toothed front edges and a gently domed back
		const auto Body = [](FV Q)
		{
			const FV M = MirrorX(Q);
			float D = SdEllipse(Q, FV(0.5f, 0.57f), 0.25f, 0.16f);
			D = SMin(D, SdEllipse(Q, FV(0.5f, 0.5f), 0.2f, 0.1f), 0.06f);
			// four teeth along each front-lateral edge
			for (int32 i = 0; i < 4; ++i)
			{
				const float A = FMath::DegreesToRadians(200.0f + i * 14.0f);
				const FV At(0.5f + FMath::Cos(A) * 0.25f, 0.55f + FMath::Sin(A) * 0.15f);
				D = SMin(D, SdCircle(M, At, 0.018f), 0.02f);
			}
			// eye sockets notch the front
			D = SSub(D, SdCircle(M, FV(0.43f, 0.405f), 0.03f), 0.015f);
			return D;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.55f;
			P.Look.Grain = 0.05f;
			P.Fill(Body, Shell);
		}
		{
			FClip Clip(P, Body);
			// rim of pale underside showing at the back, rich darker crown
			P.MEllipse(FV(0.5f, 0.73f), 0.24f, 0.06f, Alpha(Under, 0.8f), 0.03f);
			P.MEllipse(FV(0.5f, 0.5f), 0.17f, 0.07f, Alpha(ShellDeep, 0.35f), 0.05f);
			// the H-shaped grooves that divide the shell into regions
			// (arched, so the front of the shell reads as a frown rather than a smile)
			P.MLine(Smooth({ FV(0.4f, 0.56f), FV(0.45f, 0.505f), FV(0.5f, 0.49f), FV(0.55f, 0.505f), FV(0.6f, 0.56f) }), 0.004f, Alpha(ShellDeep, 0.7f));
			P.MLine(Smooth({ FV(0.401f, 0.566f), FV(0.451f, 0.511f), FV(0.5f, 0.497f), FV(0.549f, 0.511f), FV(0.599f, 0.566f) }), 0.002f, Hex(0xFFD2A8, 0.4f));
			for (const float F : { -1.0f, 1.0f })
			{
				P.MLine(Smooth({ FV(0.5f + F * 0.06f, 0.6f), FV(0.5f + F * 0.11f, 0.64f), FV(0.5f + F * 0.13f, 0.7f) }), 0.0035f, Alpha(ShellDeep, 0.55f));
				P.MLine(Smooth({ FV(0.5f + F * 0.16f, 0.5f), FV(0.5f + F * 0.18f, 0.56f), FV(0.5f + F * 0.2f, 0.6f) }), 0.003f, Alpha(ShellDeep, 0.45f));
			}
			Granules(P, Body, FV(0.26f, 0.41f), FV(0.74f, 0.72f), 70, 0.0035f, 0.007f, Alpha(ShellDeep, 0.5f), ShellPale, 401);
			Speckle(P, FV(0.25f, 0.4f), FV(0.75f, 0.73f), 120, 0.0015f, 0.0035f, Alpha(ShellDeep, 0.5f), 402);
			P.MEllipse(FV(0.42f, 0.47f), 0.1f, 0.03f, Hex(0xFFFFFF, 0.22f), 0.03f);
		}

		// ---- stalked eyes in their sockets, and small mouthparts between them
		for (const float F : { -1.0f, 1.0f })
		{
			const FV Socket(0.5f + F * 0.07f, 0.41f), Eye(0.5f + F * 0.08f, 0.355f);
			P.Capsule(Socket, Eye, 0.016f, Tone(Shell, 0.9f));
			HDEye(P, Eye, 0.036f, Hex(0x2A2016), FV(F * -0.18f, 0.22f), 0.5f);
			HDAngryLid(P, Eye, 0.036f, -F, Tone(Shell, 0.85f));
		}
		P.Ellipse(FV(0.5f, 0.425f), 0.035f, 0.016f, Hex(0xC24A22));
		P.MCapsule(FV(0.5f, 0.414f), FV(0.5f, 0.436f), 0.002f, Hex(0x5A1A08, 0.9f));
		P.MCapsule(FV(0.48f, 0.42f), FV(0.52f, 0.42f), 0.002f, Hex(0xFFC8A0, 0.6f));
	}

	/** A fin membrane between rays fanning from Root: Tips are the ray ends (in order around the fin edge) */
	void HDFin(FPainter& P, FV Root, const TArray<FV>& Tips, const FLinearColor& Membrane, const FLinearColor& Ray, float Width = 0.02f)
	{
		TArray<FV> Outline;
		Outline.Add(Root);
		for (const FV& T : Tips)
		{
			Outline.Add(T);
		}
		const TArray<FV> Shape = Smooth(Outline, true, 4);
		const auto Fin = [&Shape, Width](FV Q) { return SdPoly(Q, Shape) - Width * 0.3f; };
		{
			FWith With(P);
			P.Look.Dome = 0.04f;
			P.Look.Spec = 0.25f;
			P.Fill(Fin, Membrane);
		}
		FClip Clip(P, Fin);
		for (int32 i = 0; i < Tips.Num(); ++i)
		{
			P.MLine(Smooth({ Root, FMath::Lerp(Root, Tips[i], 0.55f) + (Tips[i] - Root).GetSafeNormal() * 0.0f, Tips[i] }), 0.0028f, Ray);
			if (i + 1 < Tips.Num())
			{
				const FV Mid = (Tips[i] + Tips[i + 1]) * 0.5f;
				P.MLine({ FMath::Lerp(Root, Mid, 0.35f), Mid }, 0.0015f, Alpha(Ray, Ray.A * 0.5f));
			}
		}
		P.Mark([&Shape](FV Q) { return FMath::Abs(SdPoly(Q, Shape) + 0.01f) - 0.006f; }, Alpha(Tone(Membrane, 1.5f), 0.35f));
	}

	void HDPiranha(FPainter& P)
	{
		const FLinearColor Silver = Hex(0xB8C4CC), Back = Hex(0x56626E), Red = Hex(0xE0402E), RedDeep = Hex(0x8E1C1C), FinC = Hex(0x8E3A34), RayC = Hex(0x3A1210, 0.8f);

		// fins behind the body: forked tail, dorsal, adipose, anal and pelvic
		HDFin(P, FV(0.26f, 0.5f), { FV(0.1f, 0.26f), FV(0.06f, 0.3f), FV(0.1f, 0.42f), FV(0.12f, 0.5f), FV(0.1f, 0.58f), FV(0.06f, 0.7f), FV(0.1f, 0.74f) }, FinC, RayC);
		HDFin(P, FV(0.47f, 0.27f), { FV(0.36f, 0.27f), FV(0.38f, 0.18f), FV(0.44f, 0.13f), FV(0.5f, 0.14f), FV(0.56f, 0.24f) }, Back, Hex(0x1E262E, 0.8f));
		HDFin(P, FV(0.4f, 0.69f), { FV(0.32f, 0.66f), FV(0.3f, 0.78f), FV(0.38f, 0.83f), FV(0.48f, 0.78f), FV(0.5f, 0.71f) }, Hex(0xC8443A), RayC);
		HDFin(P, FV(0.6f, 0.73f), { FV(0.54f, 0.74f), FV(0.55f, 0.84f), FV(0.62f, 0.85f), FV(0.66f, 0.75f) }, Hex(0xC8443A), RayC);

		// deep body with a blunt head and a heavy jutting lower jaw; the gape is carved out of it
		const TArray<FV> Outline = Smooth({ FV(0.25f, 0.5f), FV(0.32f, 0.37f), FV(0.45f, 0.26f), FV(0.6f, 0.23f), FV(0.74f, 0.27f), FV(0.85f, 0.36f), FV(0.91f, 0.46f), FV(0.95f, 0.56f), FV(0.93f, 0.63f), FV(0.82f, 0.7f), FV(0.64f, 0.75f), FV(0.46f, 0.72f), FV(0.32f, 0.62f) }, true, 6);
		// the open mouth: a wedge between the upper and lower jaw lines
		const FV UpA(0.925f, 0.5f), UpB(0.76f, 0.55f), LoA(0.96f, 0.6f), LoB(0.76f, 0.575f);
		const TArray<FV> Gape = { UpA, UpB, LoB, LoA };
		// the carved gape runs out past the snout so no sliver of body is left across the front of the mouth
		const TArray<FV> GapeCut = { UpA + (UpA - UpB) * 0.6f, UpA, UpB, LoB, LoA, LoA + (LoA - LoB) * 0.6f };
		const auto Body = [&Outline, &GapeCut](FV Q) { return SSub(SdPoly(Q, Outline), SdPoly(Q, GapeCut), 0.006f); };
		{
			FWith With(P);
			P.Look.Dome = 0.22f;
			P.Look.Spec = 0.7f;
			P.Fill(Body, Silver);
		}
		{
			FClip Clip(P, Body);
			// dark olive back fading into silver flanks, and the fiery throat and belly
			P.MEllipse(FV(0.58f, 0.22f), 0.4f, 0.13f, Back, 0.08f);
			P.MEllipse(FV(0.66f, 0.75f), 0.34f, 0.12f, Alpha(Red, 0.95f), 0.06f);
			P.MEllipse(FV(0.86f, 0.64f), 0.1f, 0.06f, Alpha(Red, 0.9f), 0.04f);
			P.MEllipse(FV(0.6f, 0.79f), 0.28f, 0.06f, Alpha(RedDeep, 0.5f), 0.04f);
			// fine scales over the flank, glitter, and a scatter of dark spots
			Scales(P, FV(0.28f, 0.32f), FV(0.74f, 0.74f), 0.026f, 0.0f, Hex(0x5E6A74, 0.35f), Hex(0xFFFFFF, 0.3f));
			Speckle(P, FV(0.3f, 0.3f), FV(0.76f, 0.7f), 60, 0.0015f, 0.004f, Hex(0xFFFFFF, 0.85f), 411);
			Speckle(P, FV(0.36f, 0.3f), FV(0.7f, 0.56f), 28, 0.004f, 0.008f, Hex(0x2A343C, 0.55f), 412);
			// lateral line
			P.MLine(Smooth({ FV(0.28f, 0.49f), FV(0.45f, 0.43f), FV(0.62f, 0.41f), FV(0.72f, 0.42f) }), 0.0025f, Hex(0x3E4A54, 0.6f));
			for (int32 i = 0; i < 18; ++i)
			{
				P.MCircle(FV(0.3f + i * 0.023f, 0.485f - FMath::Sin(i * 0.17f) * 0.07f), 0.0022f, Hex(0xE8F0F4, 0.8f));
			}
			// gill cover: a curved plate edge behind the eye, lit on its rim
			P.MArc(FV(0.6f, 0.47f), 0.15f, 0.007f, -55.0f, 62.0f, Hex(0x3A4650, 0.7f));
			P.MArc(FV(0.595f, 0.47f), 0.15f, 0.003f, -50.0f, 58.0f, Hex(0xFFFFFF, 0.5f));
			// lips along the jaw lines and a dark mouth corner
			P.MLine({ UpA + FV(0.005f, -0.012f), UpB + FV(0.0f, -0.012f) }, 0.007f, Hex(0x7A848C, 0.6f));
			P.MCircle(UpB + FV(-0.005f, 0.008f), 0.012f, Hex(0x2A0A0E, 0.8f));
			P.MEllipse(FV(0.5f, 0.32f), 0.16f, 0.03f, Hex(0xFFFFFF, 0.28f), 0.03f);
		}

		// the mouth interior and razor teeth set exactly along the jaw lines, interlocking
		{
			// the mouth interior stays inside the head's silhouette, so the open front reads as the lips' edge
			FClip Mouth(P, [&Outline](FV Q) { return SdPoly(Q, Outline) + 0.004f; });
			P.MPoly(Gape, Hex(0x3A0A12));
		}
		P.MPoly({ UpB, (UpB + LoB) * 0.5f + FV(0.03f, 0.0f), LoB }, Hex(0x1A0408));
		const int32 Count = 6;
		for (int32 i = 0; i < Count; ++i)
		{
			const float T0 = (i + 0.1f) / Count, T1 = (i + 0.9f) / Count;
			const FV A = FMath::Lerp(UpA, UpB, T0), B = FMath::Lerp(UpA, UpB, T1);
			const FV Down = FV(-(UpB - UpA).Y, (UpB - UpA).X).GetSafeNormal() * -1.0f;
			const FV TipU = (A + B) * 0.5f + FV(0.0f, 0.03f - i * 0.002f);
			P.Poly({ A + FV(0.0f, -0.004f), B + FV(0.0f, -0.004f), TipU }, Hex(0xF4F2EA));
			const float S0 = (i + 0.6f) / Count, S1 = (i + 1.4f) / Count;
			if (S1 <= 1.02f)
			{
				const FV C = FMath::Lerp(LoA, LoB, S0), D = FMath::Lerp(LoA, LoB, FMath::Min(S1, 1.0f));
				P.Poly({ C + FV(0.0f, 0.004f), D + FV(0.0f, 0.004f), (C + D) * 0.5f + FV(0.0f, -0.03f + i * 0.002f) }, Hex(0xF4F2EA));
			}
		}

		// pectoral fin on the flank
		HDFin(P, FV(0.6f, 0.58f), { FV(0.54f, 0.58f), FV(0.48f, 0.62f), FV(0.47f, 0.67f), FV(0.52f, 0.67f), FV(0.6f, 0.62f) }, Hex(0xE8665A), RayC);

		// the eye high on the head, and a heavy brow ridge that stops at the eye's edge
		const FV EyeC(0.775f, 0.38f);
		const float EyeR = 0.042f;
		P.Circle(EyeC, EyeR + 0.008f, Hex(0x3A444C));
		{
			FClip Clip(P, [EyeC, EyeR](FV Q) { return SdCircle(Q, EyeC, EyeR); });
			P.Ball(EyeC, EyeR, Hex(0xE8601A), 0.4f);
			P.Mark([EyeC, EyeR](FV Q) { return FMath::Abs(SdCircle(Q, EyeC, EyeR * 0.92f)) - EyeR * 0.08f; }, Hex(0xB83A10, 0.8f));
			P.MCircle(EyeC + FV(0.006f, 0.002f), EyeR * 0.5f, Hex(0x06090C));
			P.MCircle(EyeC + FV(-EyeR * 0.3f, -EyeR * 0.15f), EyeR * 0.16f, Hex(0xFFFFFF, 0.95f));
		}
		// a heavy lid glaring down towards the snout
		HDAngryLid(P, EyeC, EyeR, 1.0f, Hex(0x56626E), 0.8f, 0.0f);
		// nostril
		P.MCircle(FV(0.885f, 0.44f), 0.006f, Hex(0x1A2228));
	}

	/** Nearest and second-nearest distance from Q to a set of seed points (for natural plates, scales and cells) */
	void Voronoi(FV Q, const TArray<FV>& Seeds, float& OutF1, float& OutF2, int32& OutNearest)
	{
		OutF1 = OutF2 = 1e6f;
		OutNearest = 0;
		for (int32 i = 0; i < Seeds.Num(); ++i)
		{
			const float D = (Q - Seeds[i]).Size();
			if (D < OutF1)
			{
				OutF2 = OutF1;
				OutF1 = D;
				OutNearest = i;
			}
			else if (D < OutF2)
			{
				OutF2 = D;
			}
		}
	}

	/**
	 *  Paints natural plates (turtle scutes, head scales, pebbly skin) from seed points inside the current clip:
	 *  each plate gets a lit dome, concentric growth rings and optional radiating streaks, separated by dark seams.
	 */
	void Plates(FPainter& P, const TArray<FV>& Seeds, float Seam, const FLinearColor& SeamColor, const FLinearColor& Light, int32 Rings, float RingStep, const FLinearColor& Streak = FLinearColor(0, 0, 0, 0))
	{
		// soft lit centre on each plate
		P.Mark([&Seeds](FV Q) { float F1, F2; int32 N; Voronoi(Q, Seeds, F1, F2, N); return -(F2 - F1) * 0.5f + 0.012f; }, Light, 0.025f);
		if (Streak.A > 0.0f)
		{
			P.Mark([&Seeds](FV Q)
			{
				float F1, F2; int32 N; Voronoi(Q, Seeds, F1, F2, N);
				const FV D = Q - Seeds[N];
				const float A = FMath::Atan2(D.Y, D.X) * 7.0f + Rand01(N, 3) * 6.0f;
				return (0.4f - FMath::Abs(FMath::Sin(A + Noise2(Q * 30.0f) * 1.2f))) * 0.01f;
			}, Streak, 0.004f);
		}
		for (int32 k = 1; k <= Rings; ++k)
		{
			P.Mark([&Seeds, k, RingStep](FV Q)
			{
				float F1, F2; int32 N; Voronoi(Q, Seeds, F1, F2, N);
				const float Edge = (F2 - F1) * 0.5f;
				return FMath::Abs(Edge - k * RingStep) - RingStep * 0.12f;
			}, Alpha(SeamColor, SeamColor.A * 0.4f));
		}
		// seams, with a thin light lip on the upper side
		P.Mark([&Seeds, Seam](FV Q) { float F1, F2; int32 N; Voronoi(Q - FV(0.0f, Seam * 0.9f), Seeds, F1, F2, N); return (F2 - F1) * 0.5f - Seam * 0.5f; }, Hex(0xFFFFFF, 0.25f));
		P.Mark([&Seeds, Seam](FV Q) { float F1, F2; int32 N; Voronoi(Q, Seeds, F1, F2, N); return (F2 - F1) * 0.5f - Seam; }, SeamColor);
	}

	void HDTurtle(FPainter& P)
	{
		const FLinearColor Skin = Hex(0x8CA456), SkinDark = Hex(0x4E6630), SkinPale = Hex(0xE6E2A0);
		const FLinearColor ShellC = Hex(0x7A6A34), Amber = Hex(0xD8A040), SeamC = Hex(0x2E2410, 0.9f);

		// a scaled limb: fill, then irregular plate scales inside it, and a pale trailing edge
		auto Limb = [&](const TArray<FV>& Path, float R0, float R1, int32 Seed, float Scale)
		{
			const auto Shape = [&Path, R0, R1](FV Q) { return SdRibbon(Q, Path, R0, R1); };
			{
				FWith With(P);
				P.Look.Dome = 0.05f;
				P.Look.Spec = 0.35f;
				P.Fill(Shape, Skin);
			}
			FClip Clip(P, Shape);
			TArray<FV> Seeds;
			for (int32 i = 0; i < 40; ++i)
			{
				FV Pos, Tan;
				Along(Path, Rand01(Seed, i), Pos, Tan);
				Seeds.Add(Pos + FV(-Tan.Y, Tan.X) * ((Rand01(Seed + 1, i) - 0.5f) * 2.0f * R0));
			}
			Plates(P, Seeds, 0.0022f * Scale, Alpha(SkinDark, 0.85f), Hex(0xE8F0B0, 0.35f), 0, 0.01f);
			// the pale leading edge of the flipper
			TArray<FV> Edge;
			for (int32 i = 0; i <= 12; ++i)
			{
				FV Pos, Tan;
				Along(Path, i / 12.0f, Pos, Tan);
				Edge.Add(Pos - FV(-Tan.Y, Tan.X) * FMath::Lerp(R0, R1, i / 12.0f) * 0.85f);
			}
			P.MLine(Edge, 0.006f, Alpha(SkinPale, 0.6f));
		};

		// rear flippers and tail, then the big front flippers swept back in a stroke
		Limb(Smooth({ FV(0.26f, 0.34f), FV(0.18f, 0.27f), FV(0.11f, 0.26f) }), 0.045f, 0.018f, 501, 1.0f);
		Limb(Smooth({ FV(0.26f, 0.66f), FV(0.18f, 0.73f), FV(0.11f, 0.74f) }), 0.045f, 0.018f, 502, 1.0f);
		Limb(Smooth({ FV(0.2f, 0.5f), FV(0.13f, 0.5f), FV(0.08f, 0.52f) }), 0.03f, 0.006f, 503, 1.0f);
		Limb(Smooth({ FV(0.6f, 0.33f), FV(0.67f, 0.21f), FV(0.64f, 0.11f), FV(0.52f, 0.05f) }), 0.075f, 0.02f, 504, 1.0f);
		Limb(Smooth({ FV(0.6f, 0.67f), FV(0.67f, 0.79f), FV(0.64f, 0.89f), FV(0.52f, 0.95f) }), 0.075f, 0.02f, 505, 1.0f);

		// neck and head with large head scales, a hooked beak and side eyes
		const auto Head = [](FV Q) { return SMin(SdCapsule(Q, FV(0.72f, 0.5f), FV(0.82f, 0.5f), 0.055f), SdEllipse(Q, FV(0.86f, 0.5f), 0.08f, 0.066f), 0.04f); };
		{
			FWith With(P);
			P.Look.Dome = 0.12f;
			P.Look.Spec = 0.45f;
			P.Fill(Head, Skin);
		}
		{
			FClip Clip(P, Head);
			const TArray<FV> HeadSeeds = { FV(0.9f, 0.5f), FV(0.86f, 0.47f), FV(0.86f, 0.53f), FV(0.83f, 0.5f), FV(0.8f, 0.46f), FV(0.8f, 0.54f), FV(0.77f, 0.5f), FV(0.89f, 0.45f), FV(0.89f, 0.55f), FV(0.84f, 0.43f), FV(0.84f, 0.57f), FV(0.74f, 0.47f), FV(0.74f, 0.53f) };
			Plates(P, HeadSeeds, 0.003f, Alpha(SkinDark, 0.9f), Hex(0xF0F8C0, 0.35f), 0, 0.01f);
			P.MEllipse(FV(0.86f, 0.5f), 0.05f, 0.02f, Hex(0xFFFFE0, 0.18f), 0.02f);
		}
		P.Fill([](FV Q) { return SdPoly(Q, { FV(0.925f, 0.47f), FV(0.965f, 0.5f), FV(0.925f, 0.53f) }) - 0.006f; }, Hex(0x5A5030));
		for (const float F : { -1.0f, 1.0f })
		{
			const FV Eye(0.875f, 0.5f + F * 0.052f);
			P.Ball(Eye, 0.016f, Hex(0x2A1A10), 0.8f);
			P.MCircle(Eye + FV(-0.004f, -0.004f), 0.004f, White);
		}

		// the carapace: an egg-shaped dome with a serrated margin of small scutes
		const auto Shell = [](FV Q)
		{
			const float Wave = FMath::Sin(FMath::Atan2(Q.Y - 0.5f, Q.X - 0.45f) * 13.0f) * 0.004f;
			return SdEllipse(Q, FV(0.45f, 0.5f), 0.3f, 0.25f) + Wave;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.24f;
			P.Look.Spec = 0.6f;
			P.Fill(Shell, ShellC);
		}
		{
			FClip Clip(P, Shell);
			TArray<FV> Seeds = { FV(0.25f, 0.5f), FV(0.36f, 0.5f), FV(0.47f, 0.5f), FV(0.58f, 0.5f), FV(0.68f, 0.5f) };
			for (int32 i = 0; i < 4; ++i)
			{
				const float X = 0.3f + i * 0.115f;
				const float Y = 0.14f - FMath::Abs(i - 1.5f) * 0.02f;
				Seeds.Add(FV(X, 0.5f - Y));
				Seeds.Add(FV(X, 0.5f + Y));
			}
			for (int32 i = 0; i < 26; ++i)
			{
				const float A = FMath::DegreesToRadians(i * 360.0f / 26.0f);
				Seeds.Add(FV(0.45f + FMath::Cos(A) * 0.29f, 0.5f + FMath::Sin(A) * 0.24f));
			}
			P.MEllipse(FV(0.45f, 0.5f), 0.22f, 0.17f, Alpha(Hex(0x9A8440), 0.6f), 0.08f);
			Plates(P, Seeds, 0.0035f, SeamC, Hex(0xF0D890, 0.3f), 4, 0.011f, Alpha(Amber, 0.35f));
			Speckle(P, FV(0.15f, 0.25f), FV(0.75f, 0.75f), 80, 0.002f, 0.005f, Hex(0x2E2410, 0.45f), 506);
			// algae fuzz and a barnacle hitchhiker on the back edge
			P.MEllipse(FV(0.24f, 0.62f), 0.06f, 0.035f, Hex(0x5E9A48, 0.55f), 0.03f);
			P.Ball(FV(0.28f, 0.66f), 0.016f, Hex(0xE8E2D0), 0.2f);
			P.MCircle(FV(0.28f, 0.66f), 0.006f, Hex(0x3A3028));
			P.MEllipse(FV(0.4f, 0.36f), 0.14f, 0.04f, Hex(0xFFFFFF, 0.2f), 0.04f);
		}
	}

	/** A forked lightning arc from A to B with a side branch, glowing */
	void HDLightning(FPainter& P, FV A, FV B, int32 Seed, float W = 0.005f, const FLinearColor& Glow = Hex(0xE8C820))
	{
		TArray<FV> Main;
		const FV D = B - A, N(-D.Y, D.X);
		for (int32 i = 0; i <= 6; ++i)
		{
			const float T = i / 6.0f;
			Main.Add(A + D * T + N * ((i == 0 || i == 6) ? 0.0f : (Rand01(Seed, i) - 0.5f) * 0.35f));
		}
		const FV Fork = Main[3], ForkEnd = Fork + Spin(D * 0.4f, Rand01(Seed, 9) > 0.5f ? 35.0f : -35.0f);
		const TArray<FV> Branch = { Fork, (Fork + ForkEnd) * 0.5f + N * 0.08f, ForkEnd };
		for (const TArray<FV>* L : { (const TArray<FV>*)&Main, &Branch })
		{
			const float Wd = L == &Main ? W : W * 0.6f;
			P.Glow([L, Wd](FV Q) { return SdPolyline(Q, *L, Wd); }, Glow, 0.04f, 0.7f);
			P.MLine(*L, Wd, Hex(0xFFF6C0));
			P.MLine(*L, Wd * 0.4f, Hex(0xFFFFFF));
		}
	}

	void HDManta(FPainter& P)
	{
		const FLinearColor Back = Hex(0x262E3E), BackLight = Hex(0x3E4A62), Belly = Hex(0xE4ECF2);
		// whip tail
		const TArray<FV> Tail = Smooth({ FV(0.5f, 0.66f), FV(0.52f, 0.8f), FV(0.49f, 0.9f), FV(0.46f, 0.98f) });
		P.Ribbon(Tail, 0.016f, 0.004f, Hex(0x1C2230));
		// broad wings sweeping back to pointed tips, the pale underside catching light along the trailing edge
		const TArray<FV> Wings = Smooth({ FV(0.5f, 0.2f), FV(0.62f, 0.24f), FV(0.78f, 0.33f), FV(0.92f, 0.44f), FV(0.99f, 0.53f), FV(0.88f, 0.55f), FV(0.72f, 0.56f), FV(0.6f, 0.62f), FV(0.5f, 0.7f), FV(0.4f, 0.62f), FV(0.28f, 0.56f), FV(0.12f, 0.55f), FV(0.01f, 0.53f), FV(0.08f, 0.44f), FV(0.22f, 0.33f), FV(0.38f, 0.24f) }, true, 5);
		P.Fill([&Wings](FV Q) { return SdPoly(Q - FV(0.0f, 0.014f), Wings); }, Belly);
		const auto Body = [&Wings](FV Q) { return SdPoly(Q, Wings); };
		{
			FWith With(P);
			P.Look.Dome = 0.12f;
			P.Look.Spec = 0.55f;
			P.Fill(Body, Back);
		}
		{
			FClip Clip(P, Body);
			// raised body ridge, white shoulder chevrons, wing muscles and a starry skin speckle
			P.MEllipse(FV(0.5f, 0.45f), 0.13f, 0.22f, Alpha(BackLight, 0.85f), 0.07f);
			const TArray<FV> Chevron = Smooth({ FV(0.57f, 0.29f), FV(0.68f, 0.3f), FV(0.8f, 0.37f), FV(0.66f, 0.38f), FV(0.58f, 0.36f) }, true, 4);
			for (const float F : { -1.0f, 1.0f })
			{
				P.Mark([F, &Chevron](FV Q) { return SdPoly(MirrorX(Q) * FV(-1.0f, 1.0f) + FV(1.0f, 0.0f), Chevron); }, Hex(0xE8F0F6, 0.9f), 0.008f);
				for (int32 i = 0; i < 5; ++i)
				{
					P.MLine(Smooth({ FV(0.5f + F * 0.12f, 0.36f + i * 0.04f), FV(0.5f + F * (0.25f + i * 0.02f), 0.4f + i * 0.035f), FV(0.5f + F * (0.38f + i * 0.02f), 0.47f + i * 0.02f) }), 0.003f, Hex(0x56688A, 0.45f));
				}
			}
			Speckle(P, FV(0.02f, 0.2f), FV(0.98f, 0.7f), 150, 0.0015f, 0.004f, Hex(0xB8C8E0, 0.5f), 601);
			Speckle(P, FV(0.15f, 0.3f), FV(0.85f, 0.6f), 40, 0.004f, 0.009f, Hex(0x0E121C, 0.6f), 602);
			P.MLine(Smooth({ FV(0.5f, 0.3f), FV(0.5f, 0.5f), FV(0.5f, 0.68f) }), 0.004f, Hex(0x7A8CB0, 0.6f));
			P.MLine(Smooth({ FV(0.3f, 0.32f), FV(0.18f, 0.39f), FV(0.06f, 0.48f) }), 0.006f, Hex(0xFFFFFF, 0.16f));
			P.MLine(Smooth({ FV(0.7f, 0.32f), FV(0.82f, 0.39f), FV(0.94f, 0.48f) }), 0.006f, Hex(0xFFFFFF, 0.16f));
		}
		// cephalic fins curled forward like horns, with the wide mouth between them
		for (const float F : { -1.0f, 1.0f })
		{
			const TArray<FV> Horn = Smooth({ FV(0.5f + F * 0.07f, 0.25f), FV(0.5f + F * 0.1f, 0.17f), FV(0.5f + F * 0.085f, 0.09f), FV(0.5f + F * 0.06f, 0.06f) });
			{
				FWith With(P);
				P.Look.Dome = 10.0f;
				P.Look.Spec = 0.4f;
				P.Ribbon(Horn, 0.028f, 0.012f, Hex(0x323C52));
			}
			FClip Clip(P, [&Horn](FV Q) { return SdRibbon(Q, Horn, 0.028f, 0.012f); });
			for (int32 k = 1; k < 6; ++k)
			{
				FV Pos, Tan;
				Along(Horn, k / 6.0f, Pos, Tan);
				P.MCapsule(Pos - FV(-Tan.Y, Tan.X) * 0.025f, Pos + FV(-Tan.Y, Tan.X) * 0.025f, 0.002f, Hex(0x141A26, 0.8f));
			}
		}
		P.Fill([](FV Q) { return SdEllipse(Q, FV(0.5f, 0.215f), 0.06f, 0.016f); }, Hex(0xD8E2EA));
		P.MEllipse(FV(0.5f, 0.218f), 0.045f, 0.006f, Hex(0x141A26));
		for (const float F : { -1.0f, 1.0f })
		{
			const FV Eye(0.5f + F * 0.085f, 0.27f);
			P.Ball(Eye, 0.02f, Hex(0x3A2A1A), 0.8f);
			P.MCircle(Eye + FV(-0.005f, -0.006f), 0.005f, White);
		}
	}

	void HDKraken(FPainter& P)
	{
		const FLinearColor Skin = Hex(0x9A48C4), SkinDeep = Hex(0x5A1E88), SkinPale = Hex(0xE2A8F2), Sucker = Hex(0xF2D0F8);
		// eight arms flowing out of the head as one melted form, curling at the tips
		TArray<TArray<FV>> Arms;
		Arms.Add(Smooth({ FV(0.4f, 0.55f), FV(0.22f, 0.56f), FV(0.08f, 0.47f), FV(0.07f, 0.36f), FV(0.15f, 0.31f), FV(0.2f, 0.37f) }, false, 6));
		Arms.Add(Smooth({ FV(0.42f, 0.6f), FV(0.24f, 0.7f), FV(0.1f, 0.76f), FV(0.06f, 0.86f), FV(0.14f, 0.9f), FV(0.18f, 0.84f) }, false, 6));
		Arms.Add(Smooth({ FV(0.45f, 0.63f), FV(0.36f, 0.78f), FV(0.28f, 0.88f), FV(0.3f, 0.97f), FV(0.37f, 0.95f) }, false, 6));
		Arms.Add(Smooth({ FV(0.48f, 0.65f), FV(0.46f, 0.8f), FV(0.44f, 0.92f), FV(0.4f, 0.98f) }, false, 6));
		for (int32 i = 0; i < 4; ++i)
		{
			TArray<FV> M;
			for (const FV& Pt : Arms[i])
			{
				M.Add(FV(1.0f - Pt.X, Pt.Y));
			}
			Arms.Add(M);
		}
		const float R0 = 0.07f, R1 = 0.014f;
		const auto Mantle = [](FV Q)
		{
			float D = SdEllipse(Q, FV(0.5f, 0.3f), 0.24f, 0.27f);
			D = SMin(D, SdEllipse(Q, FV(0.5f, 0.52f), 0.19f, 0.13f), 0.08f);
			return D;
		};
		const auto Whole = [&Arms, R0, R1, &Mantle](FV Q)
		{
			float D = Mantle(Q);
			for (const TArray<FV>& A : Arms)
			{
				D = SMin(D, SdRibbon(Q, A, R0, R1), 0.05f);
			}
			return D;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.16f;
			P.Look.Spec = 0.55f;
			P.Fill(Whole, Skin);
		}
		{
			FClip Clip(P, Whole);
			// darker arms underneath, suckers along the inside of each curl, skin texture everywhere
			P.MEllipse(FV(0.5f, 0.8f), 0.5f, 0.25f, Alpha(SkinDeep, 0.35f), 0.15f);
			for (int32 a = 0; a < Arms.Num(); ++a)
			{
				const float Side = a < 4 ? 1.0f : -1.0f;
				for (int32 k = 2; k < 16; ++k)
				{
					FV Pos, Tan;
					const float T = k / 16.0f;
					Along(Arms[a], T, Pos, Tan);
					const float R = FMath::Lerp(R0, R1, T);
					const FV N = FV(-Tan.Y, Tan.X) * Side;
					const FV C = Pos + N * R * 0.55f;
					const float Sr = R * 0.32f;
					P.MCircle(C + FV(Sr * 0.2f, Sr * 0.25f), Sr * 1.1f, Alpha(SkinDeep, 0.6f));
					P.MCircle(C, Sr, Sucker);
					P.MCircle(C + FV(Sr * 0.1f, Sr * 0.15f), Sr * 0.45f, Hex(0xA05AB8));
					P.MCircle(C - FV(Sr * 0.35f, Sr * 0.35f), Sr * 0.25f, Hex(0xFFFFFF, 0.7f));
				}
				// a lit ridge down the top of each arm
				TArray<FV> Ridge;
				for (int32 k = 0; k <= 14; ++k)
				{
					FV Pos, Tan;
					Along(Arms[a], k / 14.0f, Pos, Tan);
					Ridge.Add(Pos - FV(-Tan.Y, Tan.X) * Side * FMath::Lerp(R0, R1, k / 14.0f) * 0.35f);
				}
				P.MLine(Ridge, 0.004f, Alpha(SkinPale, 0.35f));
			}
			// mottled mantle with raised papillae and branching veins
			Speckle(P, FV(0.26f, 0.03f), FV(0.74f, 0.62f), 140, 0.003f, 0.009f, Alpha(SkinDeep, 0.45f), 611);
			Granules(P, Mantle, FV(0.27f, 0.05f), FV(0.73f, 0.5f), 45, 0.005f, 0.011f, Alpha(SkinDeep, 0.6f), SkinPale, 612);
			for (const float F : { -1.0f, 1.0f })
			{
				P.MLine(Smooth({ FV(0.5f + F * 0.04f, 0.07f), FV(0.5f + F * 0.1f, 0.14f), FV(0.5f + F * 0.12f, 0.24f) }), 0.003f, Alpha(SkinDeep, 0.55f));
				P.MLine(Smooth({ FV(0.5f + F * 0.1f, 0.14f), FV(0.5f + F * 0.17f, 0.18f) }), 0.0025f, Alpha(SkinDeep, 0.5f));
			}
			P.MEllipse(FV(0.42f, 0.13f), 0.1f, 0.05f, Hex(0xFFFFFF, 0.3f), 0.05f);
		}
		// heavy-lidded eyes with horizontal slit pupils, set in raised sockets
		for (const float F : { -1.0f, 1.0f })
		{
			const FV Eye(0.5f + F * 0.115f, 0.42f);
			P.Fill([Eye](FV Q) { return SdEllipse(Q, Eye, 0.085f, 0.076f); }, Tone(Skin, 0.85f));
			{
				FClip Clip(P, [Eye](FV Q) { return SdEllipse(Q, Eye, 0.07f, 0.062f); });
				P.Ball(Eye, 0.07f, Hex(0xFFD040), 0.4f);
				for (int32 k = 0; k < 18; ++k)
				{
					const FV Dir = Spin(FV(1.0f, 0.0f), k * 20.0f);
					P.MCapsule(Eye + Dir * 0.02f, Eye + Dir * 0.055f, 0.002f, Hex(0xC07A10, 0.5f));
				}
				P.MCapsule(Eye - FV(0.036f, -0.006f), Eye + FV(0.036f, 0.006f), 0.013f, Hex(0x08060C));
				// the upper lid scowls down at the inner corner
				P.Fill([Eye, F](FV Q) { const FV L = Q - Eye; return L.Y - (-0.045f - F * L.X * 0.6f); }, Tone(Skin, 0.75f));
				P.MCircle(Eye + FV(-0.02f, 0.012f), 0.008f, Hex(0xFFFFFF, 0.9f));
			}
		}
		// siphon on the side, and the hard beak peeking from the arm web
		P.Fill([](FV Q) { return SdPoly(Q, { FV(0.483f, 0.575f), FV(0.517f, 0.575f), FV(0.5f, 0.605f) }) - 0.004f; }, Hex(0x3A2A40));
		P.MLine({ FV(0.49f, 0.575f), FV(0.5f, 0.6f) }, 0.002f, Hex(0x8A7A90, 0.8f));
	}

	void HDSeaStar(FPainter& P)
	{
		const FLinearColor Orange = Hex(0xF07A3A), Deep = Hex(0xA8401E), Pale = Hex(0xFFD2A0), Spine = Hex(0xFFF0D8);
		const FV Mid(0.5f, 0.52f);
		// five arms, each gently curved and a little different, melted into the central disc
		TArray<TArray<FV>> Arms;
		for (int32 i = 0; i < 5; ++i)
		{
			const float A = -90.0f + i * 72.0f + (Rand01(701, i) - 0.5f) * 10.0f;
			const float Bend = (Rand01(702, i) - 0.5f) * 30.0f;
			const float Len = 0.4f + Rand01(703, i) * 0.05f;
			Arms.Add(Smooth({ Mid, Mid + Spin(FV(Len * 0.5f, 0.0f), A + Bend * 0.3f), Mid + Spin(FV(Len, 0.0f), A + Bend) }, false, 6));
		}
		const auto Body = [&Arms, Mid](FV Q)
		{
			float D = SdCircle(Q, Mid, 0.1f);
			for (const TArray<FV>& A : Arms)
			{
				D = SMin(D, SdRibbon(Q, A, 0.1f, 0.022f), 0.06f);
			}
			return D;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.1f;
			P.Look.Spec = 0.35f;
			P.Look.Grain = 0.05f;
			P.Fill(Body, Orange);
		}
		FClip Clip(P, Body);
		// a darker reticulated net between the plates, and rows of white-tipped spines down every arm
		TArray<FV> Seeds;
		for (int32 a = 0; a < 5; ++a)
		{
			for (int32 k = 0; k < 22; ++k)
			{
				FV Pos, Tan;
				const float T = 0.12f + k * 0.04f;
				Along(Arms[a], T, Pos, Tan);
				const float W = FMath::Lerp(0.07f, 0.012f, T);
				Seeds.Add(Pos + FV(-Tan.Y, Tan.X) * W * ((k % 3) - 1.0f) * 0.75f);
			}
		}
		for (int32 k = 0; k < 10; ++k)
		{
			Seeds.Add(Mid + Spin(FV(0.06f, 0.0f), k * 36.0f));
		}
		P.MEllipse(Mid, 0.12f, 0.12f, Alpha(Deep, 0.25f), 0.08f);
		Plates(P, Seeds, 0.006f, Alpha(Deep, 0.75f), Alpha(Pale, 0.3f), 0, 0.01f);
		for (const FV& S : Seeds)
		{
			if (Body(S) > -0.012f)
			{
				continue;
			}
			P.MCircle(S + FV(0.003f, 0.004f), 0.009f, Alpha(Deep, 0.55f));
			P.MCircle(S, 0.0075f, Spine);
			P.MCircle(S - FV(0.002f, 0.0025f), 0.003f, Hex(0xFFFFFF));
		}
		// the edge of every arm is fringed with small spines, and the tips curl up showing the tube feet groove
		for (int32 a = 0; a < 5; ++a)
		{
			for (int32 k = 0; k < 18; ++k)
			{
				FV Pos, Tan;
				const float T = 0.15f + k * 0.047f;
				Along(Arms[a], T, Pos, Tan);
				const float W = FMath::Lerp(0.1f, 0.022f, T) * 0.92f;
				for (const float S : { -1.0f, 1.0f })
				{
					P.MCircle(Pos + FV(-Tan.Y, Tan.X) * W * S, 0.0045f, Alpha(Pale, 0.9f));
				}
			}
			FV Pos, Tan;
			Along(Arms[a], 0.97f, Pos, Tan);
			P.MCircle(Pos, 0.012f, Hex(0xFFB080, 0.9f));
		}
		Speckle(P, FV(0.05f, 0.08f), FV(0.95f, 0.98f), 160, 0.0015f, 0.0035f, Alpha(Deep, 0.5f), 704);
		// the central disc: a ring of plates and the yellow sieve plate (madreporite)
		P.Mark([Mid](FV Q) { return FMath::Abs(SdCircle(Q, Mid, 0.055f)) - 0.004f; }, Alpha(Deep, 0.5f));
		P.Ball(Mid + FV(0.04f, 0.045f), 0.017f, Hex(0xFFC830), 0.3f);
		for (int32 k = 0; k < 5; ++k)
		{
			P.MLine({ Mid + FV(0.028f, 0.04f + k * 0.005f), Mid + FV(0.052f, 0.038f + k * 0.006f) }, 0.0012f, Hex(0x8A5A10, 0.9f));
		}
		P.MEllipse(Mid + FV(-0.08f, -0.12f), 0.1f, 0.04f, Hex(0xFFFFFF, 0.18f), 0.05f);
	}

	void HDCuttlefish(FPainter& P)
	{
		const FLinearColor Mantle = Hex(0xC8945E), Stripe = Hex(0x4E2E16), Pale = Hex(0xF6E4C4), FinC = Hex(0xF0DCC0);
		// arms bunched forward: eight short tapering arms melted into the head, two curling slightly apart
		TArray<TArray<FV>> Arms;
		for (int32 i = 0; i < 6; ++i)
		{
			const float Y = 0.44f + i * 0.026f;
			const float Spread = (Y - 0.505f) * 1.4f;
			Arms.Add(Smooth({ FV(0.7f, 0.5f + Spread * 0.2f), FV(0.82f, Y + Spread * 0.4f), FV(0.9f, Y + Spread * 0.9f + (i % 2 ? 0.012f : -0.012f)), FV(0.96f, Y + Spread * 1.3f) }, false, 5));
		}
		const auto Head = [&Arms](FV Q)
		{
			float D = SdEllipse(Q, FV(0.71f, 0.5f), 0.08f, 0.075f);
			for (const TArray<FV>& A : Arms)
			{
				D = SMin(D, SdRibbon(Q, A, 0.024f, 0.006f), 0.025f);
			}
			return D;
		};
		// the rippling fin that skirts the whole mantle, see-through and lined
		const auto Fin = [](FV Q)
		{
			const float A = FMath::Atan2((Q.Y - 0.5f) / 0.25f, (Q.X - 0.42f) / 0.36f);
			return SdEllipse(Q, FV(0.42f, 0.5f), 0.36f, 0.25f) + FMath::Sin(A * 14.0f) * 0.008f;
		};
		{
			FWith With(P);
			P.Look.Dome = 0.03f;
			P.Look.Spec = 0.3f;
			P.Fill(Fin, Alpha(FinC, 0.92f));
		}
		{
			FClip Clip(P, Fin);
			for (int32 k = 0; k < 60; ++k)
			{
				const FV Dir = Spin(FV(1.0f, 0.0f), k * 6.0f);
				P.MLine({ FV(0.42f, 0.5f) + FV(Dir.X * 0.28f, Dir.Y * 0.18f), FV(0.42f, 0.5f) + FV(Dir.X * 0.37f, Dir.Y * 0.26f) }, 0.0016f, Hex(0xB89A70, 0.55f));
			}
			P.Mark([](FV Q) { return FMath::Abs(SdEllipse(Q, FV(0.42f, 0.5f), 0.345f, 0.235f)) - 0.004f; }, Hex(0x7AD0E8, 0.55f));
		}
		{
			FWith With(P);
			P.Look.Dome = 0.1f;
			P.Look.Spec = 0.4f;
			P.Fill(Head, Tone(Mantle, 1.05f));
		}
		{
			FClip Clip(P, Head);
			for (const TArray<FV>& A : Arms)
			{
				for (int32 k = 2; k < 9; ++k)
				{
					FV Pos, Tan;
					Along(A, k / 9.0f, Pos, Tan);
					P.MCircle(Pos + FV(-Tan.Y, Tan.X) * 0.006f, FMath::Lerp(0.006f, 0.0025f, k / 9.0f), Hex(0xFFEAD8, 0.85f));
				}
			}
			Speckle(P, FV(0.63f, 0.42f), FV(0.97f, 0.6f), 60, 0.0015f, 0.004f, Alpha(Stripe, 0.6f), 711);
		}
		// the mantle with its zebra display bands, a pale fringe and shifting chromatophores
		const auto Body = [](FV Q) { return SdEllipse(Q, FV(0.42f, 0.5f), 0.3f, 0.185f); };
		{
			FWith With(P);
			P.Look.Dome = 0.2f;
			P.Look.Spec = 0.45f;
			P.Fill(Body, Mantle);
		}
		{
			FClip Clip(P, Body);
			P.MEllipse(FV(0.42f, 0.66f), 0.3f, 0.08f, Alpha(Pale, 0.85f), 0.04f);
			for (int32 i = 0; i < 9; ++i)
			{
				const float X = 0.16f + i * 0.055f;
				const float W = 0.009f + (i % 2) * 0.004f;
				P.MLine(Smooth({ FV(X + 0.02f, 0.32f), FV(X - 0.004f, 0.38f), FV(X + 0.012f, 0.45f), FV(X - 0.006f, 0.53f) }), W, Alpha(Stripe, 0.9f));
			}
			Speckle(P, FV(0.12f, 0.32f), FV(0.72f, 0.66f), 160, 0.0015f, 0.005f, Alpha(Stripe, 0.5f), 712);
			Speckle(P, FV(0.12f, 0.5f), FV(0.72f, 0.66f), 40, 0.003f, 0.007f, Hex(0xFFFFFF, 0.45f), 713);
			P.MEllipse(FV(0.42f, 0.55f), 0.24f, 0.05f, Hex(0x8AD8E8, 0.16f), 0.04f);
			P.MEllipse(FV(0.36f, 0.37f), 0.16f, 0.03f, Hex(0xFFFFFF, 0.3f), 0.03f);
		}
		// the eye: a W-shaped pupil under a hooded lid
		const FV Eye(0.66f, 0.44f);
		P.Fill([Eye](FV Q) { return SdEllipse(Q, Eye, 0.05f, 0.045f); }, Tone(Mantle, 0.9f));
		{
			FClip Clip(P, [Eye](FV Q) { return SdEllipse(Q, Eye, 0.04f, 0.036f); });
			P.Ball(Eye, 0.04f, Hex(0xE8D8A8), 0.4f);
			Speckle(P, Eye - FV(0.04f, 0.04f), Eye + FV(0.04f, 0.04f), 16, 0.002f, 0.004f, Hex(0xA87A40, 0.7f), 714);
			P.MLine({ Eye + FV(-0.028f, -0.008f), Eye + FV(-0.014f, 0.012f), Eye + FV(0.0f, -0.004f), Eye + FV(0.014f, 0.012f), Eye + FV(0.028f, -0.008f) }, 0.0075f, Hex(0x08060A));
			P.MEllipse(Eye + FV(0.0f, -0.036f), 0.05f, 0.016f, Tone(Mantle, 0.8f));
			P.MCircle(Eye + FV(-0.014f, -0.012f), 0.006f, Hex(0xFFFFFF, 0.9f));
		}
	}

	void HDShockEel(FPainter& P)
	{
		const FLinearColor Body = Hex(0x2AA8A4), Dark = Hex(0x14605E), Belly = Hex(0xA8E8D8), Band = Hex(0xFFE24A);
		const TArray<FV> Path = Smooth({ FV(0.06f, 0.86f), FV(0.2f, 0.72f), FV(0.4f, 0.78f), FV(0.58f, 0.64f), FV(0.52f, 0.46f), FV(0.32f, 0.44f), FV(0.3f, 0.3f), FV(0.46f, 0.22f), FV(0.64f, 0.24f) }, false, 8);
		const float R0 = 0.035f, R1 = 0.075f;
		// the long rippling fin along the belly
		TArray<FV> FinEdge;
		for (int32 i = 0; i <= 60; ++i)
		{
			FV Pos, Tan;
			const float T = i / 60.0f;
			Along(Path, T, Pos, Tan);
			const FV N(-Tan.Y, Tan.X);
			FinEdge.Add(Pos + N * (FMath::Lerp(R0, R1, T) + 0.025f + FMath::Sin(i * 1.3f) * 0.006f) * (T < 0.85f ? 1.0f : 0.0f));
		}
		P.Glow([&Path, R0, R1](FV Q) { return SdRibbon(Q, Path, R0, R1); }, Hex(0x20B8C0), 0.07f, 0.35f);
		const auto Eel = [&Path, R0, R1](FV Q) { return SdRibbon(Q, Path, R0, R1); };
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
				Along(Path, T, Pos, Tan);
				const FV N(-Tan.Y, Tan.X);
				const float R = FMath::Lerp(R0, R1, T);
				BellyLine.Add(Pos + N * R * 0.75f);
				BackLine.Add(Pos - N * R * 0.7f);
			}
			P.MLine(BellyLine, 0.016f, Alpha(Belly, 0.7f));
			P.MLine(BackLine, 0.014f, Alpha(Dark, 0.7f));
			// glowing electric organ bands: each band is a row of cells lit from within
			for (int32 i = 1; i < 14; ++i)
			{
				FV Pos, Tan;
				const float T = i / 14.0f;
				Along(Path, T, Pos, Tan);
				const FV N(-Tan.Y, Tan.X);
				const float R = FMath::Lerp(R0, R1, T);
				P.Glow([Pos, N, R](FV Q) { return SdCapsule(Q, Pos - N * R, Pos + N * R, 0.008f); }, Hex(0xE8C820), 0.03f, 0.7f);
				P.MCapsule(Pos - N * R, Pos + N * R, 0.009f, Band);
				for (int32 k = -3; k <= 3; ++k)
				{
					P.MCircle(Pos + N * (R * k / 3.5f), 0.004f, Hex(0xFFFFE8));
				}
			}
			Speckle(P, FV(0.04f, 0.18f), FV(0.66f, 0.9f), 140, 0.0015f, 0.004f, Alpha(Dark, 0.6f), 721);
			for (int32 i = 0; i < 40; ++i)
			{
				FV Pos, Tan;
				Along(Path, i / 40.0f, Pos, Tan);
				P.MCircle(Pos + FV(-Tan.Y, Tan.X) * 0.004f, 0.0022f, Hex(0xD8FFF8, 0.8f));
			}
		}
		// the broad flat head with a wide grin of small teeth and beady eyes
		FV HeadPos, HeadTan;
		Along(Path, 1.0f, HeadPos, HeadTan);
		{
			FNest Place(P, HeadPos + HeadTan * 0.07f, 1.45f, FMath::RadiansToDegrees(FMath::Atan2(HeadTan.Y, HeadTan.X)));
			const auto Head = [](FV Q) { return SMin(SdEllipse(Q, FV(-0.02f, 0.0f), 0.1f, 0.075f), SdEllipse(Q, FV(0.07f, 0.01f), 0.07f, 0.05f), 0.04f); };
			{
				FWith With(P);
				P.Look.Dome = 0.12f;
				P.Look.Spec = 0.6f;
				P.Fill(Head, Body);
			}
			FClip Clip(P, Head);
			P.MEllipse(FV(0.02f, 0.05f), 0.11f, 0.03f, Alpha(Belly, 0.8f), 0.02f);
			P.MEllipse(FV(0.0f, -0.05f), 0.1f, 0.03f, Alpha(Dark, 0.6f), 0.03f);
			// the mouth line with tiny teeth
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
		// sparks crackling off the body
		HDLightning(P, FV(0.62f, 0.62f), FV(0.86f, 0.52f), 731);
		HDLightning(P, FV(0.2f, 0.66f), FV(0.08f, 0.5f), 732);
		HDLightning(P, FV(0.36f, 0.24f), FV(0.3f, 0.06f), 733, 0.004f);
	}

	bool PaintEnemyHD(FPainter& P, ETDEnemyType Type)
	{
		switch (Type)
		{
		case ETDEnemyType::Crab:	HDCrab(P); return true;
		case ETDEnemyType::Piranha:	HDPiranha(P); return true;
		case ETDEnemyType::Turtle:	HDTurtle(P); return true;
		case ETDEnemyType::Manta:	HDManta(P); return true;
		case ETDEnemyType::Kraken:	HDKraken(P); return true;
		case ETDEnemyType::SeaStar:	HDSeaStar(P); return true;
		case ETDEnemyType::Cuttlefish:	HDCuttlefish(P); return true;
		case ETDEnemyType::ShockEel:	HDShockEel(P); return true;
		default:					return false;
		}
	}
