// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDModels.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "HAL/IConsoleManager.h"
#include "Async/ParallelFor.h"
#include "Misc/ScopeLock.h"
#include "TDModelDetail.h"

namespace
{
	/** A basic shape of the same size and pivot as the engine's /Engine/BasicShapes meshes (100 units across) */
	struct FUnitShape
	{
		TArray<FVector3f> Positions;
		TArray<FVector3f> Normals;
		TArray<int32> Triangles;

		int32 AddVertex(const FVector3f& P, const FVector3f& N)
		{
			Positions.Add(P);
			Normals.Add(N);
			return Positions.Num() - 1;
		}

		/** Adds a triangle wound so that it faces along its vertex normals */
		void AddTriangle(int32 A, int32 B, int32 C)
		{
			const FVector3f Face = FVector3f::CrossProduct(Positions[C] - Positions[A], Positions[B] - Positions[A]);
			const bool bFacing = FVector3f::DotProduct(Face, Normals[A] + Normals[B] + Normals[C]) >= 0.0f;
			Triangles.Append({ A, bFacing ? B : C, bFacing ? C : B });
		}

		void AddQuad(int32 A, int32 B, int32 C, int32 D)
		{
			AddTriangle(A, B, C);
			AddTriangle(A, C, D);
		}

		/** A flat disc at height Z, facing up or down */
		void AddCap(int32 Segments, float Z, float NormalZ)
		{
			const FVector3f N(0.0f, 0.0f, NormalZ);
			const int32 Center = AddVertex(FVector3f(0.0f, 0.0f, Z), N);
			const int32 First = Positions.Num();
			for (int32 i = 0; i < Segments; ++i)
			{
				const float A = 2.0f * PI * i / Segments;
				AddVertex(FVector3f(FMath::Cos(A) * 50.0f, FMath::Sin(A) * 50.0f, Z), N);
			}
			for (int32 i = 0; i < Segments; ++i)
			{
				AddTriangle(Center, First + i, First + (i + 1) % Segments);
			}
		}
	};

	FUnitShape MakeUnitShape(ETDShape Shape, int32 Segments)
	{
		FUnitShape Out;
		switch (Shape)
		{
		case ETDShape::Sphere:
		{
			// as many rings as half the sides, so the faces come out about square (more rings add triangles, not a rounder outline)
			const int32 Rings = FMath::Max(4, (Segments + 1) / 2);
			for (int32 R = 0; R <= Rings; ++R)
			{
				const float Polar = PI * R / Rings;
				for (int32 i = 0; i <= Segments; ++i)
				{
					const float A = 2.0f * PI * i / Segments;
					const FVector3f N(FMath::Sin(Polar) * FMath::Cos(A), FMath::Sin(Polar) * FMath::Sin(A), FMath::Cos(Polar));
					Out.AddVertex(N * 50.0f, N);
				}
			}
			for (int32 R = 0; R < Rings; ++R)
			{
				for (int32 i = 0; i < Segments; ++i)
				{
					const int32 A = R * (Segments + 1) + i;
					const int32 B = A + Segments + 1;
					if (R > 0)
					{
						Out.AddTriangle(A, A + 1, B);
					}
					if (R < Rings - 1)
					{
						Out.AddTriangle(A + 1, B + 1, B);
					}
				}
			}
			break;
		}
		case ETDShape::Cylinder:
		{
			for (int32 i = 0; i <= Segments; ++i)
			{
				const float A = 2.0f * PI * i / Segments;
				const FVector3f N(FMath::Cos(A), FMath::Sin(A), 0.0f);
				Out.AddVertex(N * 50.0f + FVector3f(0.0f, 0.0f, 50.0f), N);
				Out.AddVertex(N * 50.0f - FVector3f(0.0f, 0.0f, 50.0f), N);
			}
			for (int32 i = 0; i < Segments; ++i)
			{
				Out.AddQuad(i * 2, i * 2 + 1, i * 2 + 3, i * 2 + 2);
			}
			Out.AddCap(Segments, 50.0f, 1.0f);
			Out.AddCap(Segments, -50.0f, -1.0f);
			break;
		}
		case ETDShape::Cone:
		{
			// the point at the top; side normals lean up by the slope of a cone as wide as it is tall
			for (int32 i = 0; i <= Segments; ++i)
			{
				const float A = 2.0f * PI * i / Segments;
				const float Mid = 2.0f * PI * (i + 0.5f) / Segments;
				const FVector3f Rim(FMath::Cos(A), FMath::Sin(A), 0.0f);
				const FVector3f TipDir(FMath::Cos(Mid), FMath::Sin(Mid), 0.0f);
				Out.AddVertex(Rim * 50.0f - FVector3f(0.0f, 0.0f, 50.0f), (Rim * 2.0f + FVector3f(0.0f, 0.0f, 1.0f)).GetSafeNormal());
				Out.AddVertex(FVector3f(0.0f, 0.0f, 50.0f), (TipDir * 2.0f + FVector3f(0.0f, 0.0f, 1.0f)).GetSafeNormal());
			}
			for (int32 i = 0; i < Segments; ++i)
			{
				Out.AddTriangle(i * 2, i * 2 + 2, i * 2 + 1);
			}
			Out.AddCap(Segments, -50.0f, -1.0f);
			break;
		}
		case ETDShape::Plane:
		{
			const FVector3f N(0.0f, 0.0f, 1.0f);
			const int32 A = Out.AddVertex(FVector3f(-50.0f, -50.0f, 0.0f), N);
			const int32 B = Out.AddVertex(FVector3f(50.0f, -50.0f, 0.0f), N);
			const int32 C = Out.AddVertex(FVector3f(50.0f, 50.0f, 0.0f), N);
			const int32 D = Out.AddVertex(FVector3f(-50.0f, 50.0f, 0.0f), N);
			Out.AddQuad(A, B, C, D);
			break;
		}
		default:
		{
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				for (float Sign : { 1.0f, -1.0f })
				{
					FVector3f N(0.0f), U(0.0f), V(0.0f);
					N[Axis] = Sign;
					U[(Axis + 1) % 3] = 50.0f;
					V[(Axis + 2) % 3] = 50.0f;
					const FVector3f C = N * 50.0f;
					const int32 A0 = Out.AddVertex(C - U - V, N);
					const int32 A1 = Out.AddVertex(C + U - V, N);
					const int32 A2 = Out.AddVertex(C + U + V, N);
					const int32 A3 = Out.AddVertex(C - U + V, N);
					Out.AddQuad(A0, A1, A2, A3);
				}
			}
			break;
		}
		}
		return Out;
	}

	/** Only as many sides as a shape of this size shows on screen: the engine's shapes spend the same triangles on a barnacle as on a boulder */
	int32 SegmentsForSize(float Size)
	{
		if (Size < 8.0f)
		{
			return 8;
		}
		if (Size < 24.0f)
		{
			return 12;
		}
		return Size < 60.0f ? 16 : 24;
	}

	/** Shared unit shapes; safe to call from the threads that prepare merged meshes (each shape is allocated once and never moves) */
	const FUnitShape& GetUnitShape(ETDShape Shape, int32 Segments)
	{
		static FCriticalSection Lock;
		static TMap<uint32, TUniquePtr<FUnitShape>> Shapes;
		const uint32 Key = ((uint32)Shape << 8) | (uint32)Segments;
		FScopeLock Scope(&Lock);
		if (const TUniquePtr<FUnitShape>* Found = Shapes.Find(Key))
		{
			return **Found;
		}
		return *Shapes.Add(Key, MakeUnique<FUnitShape>(MakeUnitShape(Shape, Segments)));
	}

	/** Merged meshes get coarser levels of detail for zoomed-out views. Each level keeps every part and only cuts the sides of round
	 *  shapes, so far that its outline is never further off (in pixels) than the full mesh is at the closest zoom. */
	constexpr int32 MaxMergedLods = 3;
	constexpr float LodSegmentScale[MaxMergedLods] = { 1.0f, 0.8f, 0.63f };
	/** Camera ortho width beyond which each level shows on a 2.4:1 screen (the default view is 1300 wide, the closest 800).
	 *  The engine measures screen size against the view height, so other screen shapes switch at the same pixel density per screen height. */
	constexpr float LodSwitchOrthoWidth[MaxMergedLods] = { 0.0f, 1250.0f, 2000.0f };
	constexpr float LodSwitchAspect = 2.4f;

	/** Sides of a round shape at a level of detail: the coarsest shapes (8 sides) stay as they are, the others drop to an even count */
	int32 ScaleSegments(int32 Segments, float Scale)
	{
		return Segments <= 8 || Scale >= 1.0f ? Segments : FMath::Max(8, 2 * FMath::RoundToInt(Segments * Scale * 0.5f));
	}

	/** Sides of a round shape in the detailed models, from the width of its round section */
	int32 DetailedSegments(float Across)
	{
		if (Across < 4.0f)
		{
			return 12;
		}
		if (Across < 10.0f)
		{
			return 18;
		}
		if (Across < 24.0f)
		{
			return 28;
		}
		return Across < 60.0f ? 40 : 56;
	}

	/** A box of half size H with edges rounded to radius R, made at its final size so every edge gets the same rounding.
	 *  Fine boxes round each edge in four steps, the others in two; normals are smooth across the rounding. */
	FUnitShape MakeRoundedBox(const FVector3f& H, float R, bool bFine)
	{
		FUnitShape Out;
		const FVector3f Inner = H - FVector3f(R);
		const float Step = R * 0.41421356f;	// tan(22.5 degrees): the middle row of a fine edge sits at 22.5 degrees
		auto Coords = [&](int32 Axis)
		{
			TArray<float, TInlineAllocator<6>> C;
			if (bFine)
			{
				C = { -H[Axis], -Inner[Axis] - Step, -Inner[Axis], Inner[Axis], Inner[Axis] + Step, H[Axis] };
			}
			else
			{
				C = { -H[Axis], -Inner[Axis], Inner[Axis], H[Axis] };
			}
			return C;
		};
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const int32 U = (Axis + 1) % 3;
			const int32 V = (Axis + 2) % 3;
			const auto CU = Coords(U);
			const auto CV = Coords(V);
			for (float Sign : { 1.0f, -1.0f })
			{
				const int32 First = Out.Positions.Num();
				for (int32 i = 0; i < CU.Num(); ++i)
				{
					for (int32 j = 0; j < CV.Num(); ++j)
					{
						FVector3f P;
						P[Axis] = Sign * H[Axis];
						P[U] = CU[i];
						P[V] = CV[j];
						const FVector3f Core(FMath::Clamp(P.X, -Inner.X, Inner.X), FMath::Clamp(P.Y, -Inner.Y, Inner.Y), FMath::Clamp(P.Z, -Inner.Z, Inner.Z));
						const FVector3f N = (P - Core).GetSafeNormal();
						Out.AddVertex(Core + N * R, N);
					}
				}
				const int32 Row = CV.Num();
				for (int32 i = 0; i + 1 < CU.Num(); ++i)
				{
					for (int32 j = 0; j + 1 < CV.Num(); ++j)
					{
						const int32 A = First + i * Row + j;
						Out.AddQuad(A, A + Row, A + Row + 1, A + 1);
					}
				}
			}
		}
		return Out;
	}

	/** A cylinder (round or oval) of radii RX, RY and half height HZ whose rims are rounded to radius R, at its final size */
	FUnitShape MakeRoundedCylinder(float RX, float RY, float HZ, float R, int32 Segments)
	{
		FUnitShape Out;
		// the profile from the bottom centre's rim up to the top's: (angle of the normal above the horizontal, height of the rounding centre)
		struct FRing { float Angle; float Z; };
		const float Low = -HZ + R;
		const float High = HZ - R;
		const FRing Rings[] = {
			{ -90.0f, Low }, { -60.0f, Low }, { -30.0f, Low }, { 0.0f, Low },
			{ 0.0f, High }, { 30.0f, High }, { 60.0f, High }, { 90.0f, High } };
		const int32 NumRings = UE_ARRAY_COUNT(Rings);
		for (const FRing& Ring : Rings)
		{
			const float Up = FMath::DegreesToRadians(Ring.Angle);
			const float Out_ = R * FMath::Cos(Up) - R;	// how far inside the outer wall this ring sits
			for (int32 i = 0; i <= Segments; ++i)
			{
				const float A = 2.0f * PI * i / Segments;
				const float C = FMath::Cos(A);
				const float S = FMath::Sin(A);
				const FVector3f Across = FVector3f(C / FMath::Max(RX, 1e-3f), S / FMath::Max(RY, 1e-3f), 0.0f).GetSafeNormal();
				const FVector3f P(C * (RX + Out_), S * (RY + Out_), Ring.Z + R * FMath::Sin(Up));
				const FVector3f N = Across * FMath::Cos(Up) + FVector3f(0.0f, 0.0f, FMath::Sin(Up));
				Out.AddVertex(P, N.GetSafeNormal());
			}
		}
		const int32 Row = Segments + 1;
		for (int32 r = 0; r + 1 < NumRings; ++r)
		{
			for (int32 i = 0; i < Segments; ++i)
			{
				const int32 A = r * Row + i;
				Out.AddQuad(A, A + 1, A + Row + 1, A + Row);
			}
		}
		// flat caps fanned from the rounded rims
		for (const bool bTop : { false, true })
		{
			const FVector3f N(0.0f, 0.0f, bTop ? 1.0f : -1.0f);
			const int32 Center = Out.AddVertex(FVector3f(0.0f, 0.0f, bTop ? HZ : -HZ), N);
			const int32 RimRow = bTop ? NumRings - 1 : 0;
			for (int32 i = 0; i < Segments; ++i)
			{
				Out.AddTriangle(Center, RimRow * Row + i, RimRow * Row + i + 1);
			}
		}
		return Out;
	}

	/** The detailed version of a part, at its final size (before rotation), or false when the plain unit shape is used scaled */
	bool MakeDetailedShape(const FTDMergedPart& Part, FUnitShape& Out, int32& OutSegments, float SegmentScale)
	{
		const FVector3f Size(Part.Size);
		switch (Part.Shape)
		{
		case ETDShape::Cube:
		{
			const float MinSide = Size.GetMin();
			const float R = FMath::Min(MinSide * 0.3f, 6.0f);
			if (R < 0.25f)
			{
				return false;
			}
			Out = MakeRoundedBox(Size * 0.5f, R, R >= 1.5f);
			return true;
		}
		case ETDShape::Cylinder:
		{
			const float Across = FMath::Min(Size.X, Size.Y);
			const float R = FMath::Min3(Across * 0.22f, Size.Z * 0.45f, 5.0f);
			OutSegments = ScaleSegments(DetailedSegments(FMath::Max(Size.X, Size.Y)), SegmentScale);
			if (R < 0.25f)
			{
				return false;
			}
			Out = MakeRoundedCylinder(Size.X * 0.5f, Size.Y * 0.5f, Size.Z * 0.5f, R, OutSegments);
			return true;
		}
		case ETDShape::Cone:
			OutSegments = ScaleSegments(DetailedSegments(FMath::Max(Size.X, Size.Y)), SegmentScale);
			return false;
		case ETDShape::Sphere:
			OutSegments = ScaleSegments(DetailedSegments(Size.GetMax()), SegmentScale);
			return false;
		default:
			return false;
		}
	}

	UMaterialInterface* SlotMaterial(const FLinearColor& Color, ETDPartMaterial Mat, float Opacity)
	{
		return Mat == ETDPartMaterial::Lit ? TD::SharedColorMaterial(FLinearColor(Color.R, Color.G, Color.B, 1.0f))
			: TD::SharedFxMaterial(FLinearColor(Color.R, Color.G, Color.B, 1.0f), Mat == ETDPartMaterial::Glass ? Opacity : 1.0f);
	}

	/** Adds one shape to a mesh description: Shape scaled by Scale, then placed by the part's rotation and location */
	void AppendShape(FMeshDescription& Description, FStaticMeshAttributes& Attributes, FPolygonGroupID Group, const FUnitShape& Shape, const FVector3f& Scale,
		const FTDMergedPart& Part, TArray<FVertexInstanceID>& Instances)
	{
		TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attributes.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attributes.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> BinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attributes.GetVertexInstanceUVs();
		const FQuat4f Rotation(Part.Rotation);
		const FVector3f Location(Part.Location);
		Instances.Reset(Shape.Positions.Num());
		for (int32 i = 0; i < Shape.Positions.Num(); ++i)
		{
			const FVertexID Vertex = Description.CreateVertex();
			Positions[Vertex] = Location + Rotation.RotateVector(Shape.Positions[i] * Scale);
			const FVertexInstanceID Instance = Description.CreateVertexInstance(Vertex);
			// normals on a stretched shape follow the inverse of the stretch
			const FVector3f Stretched(Shape.Normals[i].X / FMath::Max(Scale.X, 1e-4f), Shape.Normals[i].Y / FMath::Max(Scale.Y, 1e-4f), Shape.Normals[i].Z / FMath::Max(Scale.Z, 1e-4f));
			const FVector3f Normal = Rotation.RotateVector(Stretched.GetSafeNormal());
			const FVector3f Across = FMath::Abs(Normal.Z) < 0.9f ? FVector3f(0.0f, 0.0f, 1.0f) : FVector3f(1.0f, 0.0f, 0.0f);
			Normals[Instance] = Normal;
			Tangents[Instance] = FVector3f::CrossProduct(Across, Normal).GetSafeNormal();
			BinormalSigns[Instance] = 1.0f;
			UVs.Set(Instance, 0, FVector2f::ZeroVector);
			Instances.Add(Instance);
		}
		for (int32 i = 0; i + 2 < Shape.Triangles.Num(); i += 3)
		{
			const FVertexInstanceID Corners[] = { Instances[Shape.Triangles[i]], Instances[Shape.Triangles[i + 1]], Instances[Shape.Triangles[i + 2]] };
			Description.CreateTriangle(Group, Corners);
		}
	}

	UStaticMesh* NewRuntimeMesh()
	{
		UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage());
		Mesh->bSupportRayTracing = false;
		return Mesh;
	}

	void FinishRuntimeMesh(UStaticMesh* Mesh, const TArray<const FMeshDescription*>& Descriptions)
	{
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bCommitMeshDescription = false;
		Params.bFastBuild = true;
		Mesh->BuildFromMeshDescriptions(Descriptions, Params);
		Mesh->AddToRoot();

		// the levels switch by zoom (see LodSwitchOrthoWidth): with an ortho view the engine compares the bounds radius times
		// aspect / ortho width against half of each level's screen size
		if (FStaticMeshRenderData* RenderData = Mesh->GetRenderData())
		{
			const float Radius = RenderData->Bounds.SphereRadius;
			for (int32 Lod = 1; Lod < Descriptions.Num() && Lod < MaxMergedLods; ++Lod)
			{
				const float Wanted = 2.0f * Radius * LodSwitchAspect / LodSwitchOrthoWidth[Lod];
				RenderData->ScreenSize[Lod].Default = FMath::Min(Wanted, RenderData->ScreenSize[Lod - 1].Default * 0.9f);
			}
		}
	}

	/** The shadow copy of a merged mesh: one section, the plainer sides of SegmentsForSize and the coarse rounding on detailed boxes and
	 *  cylinders. Every shape stays inside the full one's surface, so the full mesh never falls into its copy's shadow. */
	UStaticMesh* BuildShadowMesh(const TArray<FTDMergedPart>& Parts)
	{
		FMeshDescription Description;
		FStaticMeshAttributes Attributes(Description);
		Attributes.Register();
		UStaticMesh* Mesh = NewRuntimeMesh();
		const FPolygonGroupID Group = Description.CreatePolygonGroup();
		Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Shadow");
		Mesh->GetStaticMaterials().Add(FStaticMaterial(TD::SharedColorMaterial(FLinearColor(0.5f, 0.5f, 0.5f)), TEXT("Shadow")));

		TArray<FVertexInstanceID> Instances;
		for (const FTDMergedPart& Part : Parts)
		{
			const float Largest = Part.Size.GetMax();
			const int32 Segments = Part.Segments > 0 ? FMath::Min(Part.Segments, SegmentsForSize(Largest)) : SegmentsForSize(Largest);
			FUnitShape Own;
			bool bOwnShape = false;
			if (Part.bDetailed && Part.Shape == ETDShape::Cube)
			{
				const float R = FMath::Min((float)Part.Size.GetMin() * 0.3f, 6.0f);
				if (R >= 0.25f)
				{
					Own = MakeRoundedBox(FVector3f(Part.Size) * 0.5f, R, false);
					bOwnShape = true;
				}
			}
			else if (Part.bDetailed && Part.Shape == ETDShape::Cylinder)
			{
				const FVector3f Size(Part.Size);
				const float R = FMath::Min3(FMath::Min(Size.X, Size.Y) * 0.22f, Size.Z * 0.45f, 5.0f);
				if (R >= 0.25f)
				{
					Own = MakeRoundedCylinder(Size.X * 0.5f, Size.Y * 0.5f, Size.Z * 0.5f, R, Segments);
					bOwnShape = true;
				}
			}
			const FUnitShape& Shape = bOwnShape ? Own : GetUnitShape(Part.Shape, Segments);
			AppendShape(Description, Attributes, Group, Shape, bOwnShape ? FVector3f(1.0f) : FVector3f(Part.Size / 100.0f), Part, Instances);
		}
		FinishRuntimeMesh(Mesh, { &Description });
		return Mesh;
	}

	/** A merged part's shape at a level of detail (scaled by OutScale), or the plain unit shape scaled to the part's size */
	const FUnitShape& PartShape(const FTDMergedPart& Part, int32 Lod, FUnitShape& Own, FVector3f& OutScale)
	{
		// detailed parts with rounded edges are made at their final size; the rest scale a shared unit shape
		const float SegmentScale = LodSegmentScale[Lod];
		int32 Segments = Part.Segments > 0 ? Part.Segments : SegmentsForSize(Part.Size.GetMax());
		if (Part.bDetailed && MakeDetailedShape(Part, Own, Segments, SegmentScale))
		{
			OutScale = FVector3f(1.0f);
			return Own;
		}
		if (!Part.bDetailed)	// detailed round shapes got their level's sides from MakeDetailedShape
		{
			Segments = ScaleSegments(Segments, SegmentScale);
		}
		OutScale = FVector3f(Part.Size / 100.0f);
		return GetUnitShape(Part.Shape, Segments);
	}

	/** Whether a coarser level would change any part (only round shapes with more than 8 sides do) */
	bool NeedsLods(const TArray<FTDMergedPart>& Parts)
	{
		for (const FTDMergedPart& Part : Parts)
		{
			if (Part.Shape == ETDShape::Sphere || Part.Shape == ETDShape::Cylinder || Part.Shape == ETDShape::Cone)
			{
				const int32 Segments = Part.Segments > 0 ? Part.Segments : (Part.bDetailed ? 9 : SegmentsForSize(Part.Size.GetMax()));
				if (Segments > 8)
				{
					return true;
				}
			}
		}
		return false;
	}

	/** The geometry of a merged mesh, made without touching any UObject so that many can be prepared at once on worker threads */
	struct FPreparedMesh
	{
		int32 NumLods = 0;
		FMeshDescription Descriptions[MaxMergedLods];
		TArray<FTDMergedMesh::FSlot> Slots;
		TArray<FTDMergedPart> ShadowParts;
	};

	void PrepareMergedMesh(const TArray<FTDMergedPart>& Parts, FPreparedMesh& Out)
	{
		if (Parts.Num() == 0)
		{
			return;
		}
		static const IConsoleVariable* LodsVar = IConsoleManager::Get().FindConsoleVariable(TEXT("atd.ModelLods"));
		Out.NumLods = (!LodsVar || LodsVar->GetInt() != 0) && NeedsLods(Parts) ? MaxMergedLods : 1;

		// every part's shape on every level first, so each description can reserve its memory once
		struct FPlaced { const FUnitShape* Shape; FVector3f Scale; int32 Own; };
		TArray<FUnitShape> Owned;
		Owned.Reserve(Parts.Num() * Out.NumLods);
		TArray<FPlaced> Placed[MaxMergedLods];
		for (int32 Lod = 0; Lod < Out.NumLods; ++Lod)
		{
			int32 Vertices = 0, Triangles = 0;
			Placed[Lod].Reserve(Parts.Num());
			for (const FTDMergedPart& Part : Parts)
			{
				FUnitShape Own;
				FVector3f Scale;
				const FUnitShape& Shape = PartShape(Part, Lod, Own, Scale);
				if (&Shape == &Own)
				{
					Placed[Lod].Add({ nullptr, Scale, Owned.Num() });
					Owned.Add(MoveTemp(Own));
				}
				else
				{
					Placed[Lod].Add({ &Shape, Scale, INDEX_NONE });
				}
				Vertices += Shape.Positions.Num();
				Triangles += Shape.Triangles.Num() / 3;
			}
			FMeshDescription& Description = Out.Descriptions[Lod];
			FStaticMeshAttributes(Description).Register();
			Description.ReserveNewVertices(Vertices);
			Description.ReserveNewVertexInstances(Vertices);
			Description.ReserveNewTriangles(Triangles);
			Description.ReserveNewEdges(Triangles * 3 / 2 + Parts.Num() * 4);
		}

		// one section per distinct look, in the same order on every level
		TMap<uint64, int32> SlotOfLook;
		TArray<FPolygonGroupID> Groups[MaxMergedLods];
		TArray<FVertexInstanceID> Instances;
		for (int32 PartIndex = 0; PartIndex < Parts.Num(); ++PartIndex)
		{
			const FTDMergedPart& Part = Parts[PartIndex];
			auto Q = [](float V) { return (uint64)FMath::Clamp(FMath::RoundToInt(V * 128.0f), 0, 0xFFF); };
			const uint64 Look = Q(Part.Color.R) | (Q(Part.Color.G) << 12) | (Q(Part.Color.B) << 24) | (Q(Part.Color.A) << 36) | ((uint64)Part.Mat << 48) | ((uint64)Part.Flags << 52);
			int32* Slot = SlotOfLook.Find(Look);
			if (!Slot)
			{
				const FName SlotName(*FString::Printf(TEXT("Slot%d"), Out.Slots.Num()));
				for (int32 Lod = 0; Lod < Out.NumLods; ++Lod)
				{
					const FPolygonGroupID NewGroup = Out.Descriptions[Lod].CreatePolygonGroup();
					FStaticMeshAttributes(Out.Descriptions[Lod]).GetPolygonGroupMaterialSlotNames()[NewGroup] = SlotName;
					Groups[Lod].Add(NewGroup);
				}
				Out.Slots.Add({ Part.Color, Part.Mat, Part.Flags });
				Slot = &SlotOfLook.Add(Look, Out.Slots.Num() - 1);
			}

			for (int32 Lod = 0; Lod < Out.NumLods; ++Lod)
			{
				const FPlaced& Where = Placed[Lod][PartIndex];
				const FUnitShape& Shape = Where.Shape ? *Where.Shape : Owned[Where.Own];
				FStaticMeshAttributes Attributes(Out.Descriptions[Lod]);
				AppendShape(Out.Descriptions[Lod], Attributes, Groups[Lod][*Slot], Shape, Where.Scale, Part, Instances);
			}

			// the shadow copy keeps the lit parts that are big enough to show in a shadow
			if (Part.Mat == ETDPartMaterial::Lit && Part.Size.GetMax() >= TD::ShadowMinPartSize)
			{
				Out.ShadowParts.Add(Part);
			}
		}
	}

	/** Makes the static mesh of a prepared merged mesh (game thread: it creates objects and materials) */
	FTDMergedMesh FinishMergedMesh(FPreparedMesh& Prepared)
	{
		FTDMergedMesh Out;
		if (Prepared.NumLods == 0)
		{
			return Out;
		}
		UStaticMesh* Mesh = NewRuntimeMesh();
		for (int32 i = 0; i < Prepared.Slots.Num(); ++i)
		{
			const FTDMergedMesh::FSlot& Slot = Prepared.Slots[i];
			Mesh->GetStaticMaterials().Add(FStaticMaterial(SlotMaterial(Slot.Color, Slot.Mat, Slot.Color.A), FName(*FString::Printf(TEXT("Slot%d"), i))));
		}
		TArray<const FMeshDescription*> Levels;
		for (int32 Lod = 0; Lod < Prepared.NumLods; ++Lod)
		{
			Levels.Add(&Prepared.Descriptions[Lod]);
		}
		FinishRuntimeMesh(Mesh, Levels);
		Out.Mesh = Mesh;
		Out.Slots = MoveTemp(Prepared.Slots);
		Out.ShadowParts = MoveTemp(Prepared.ShadowParts);
		return Out;
	}

	FTDMergedMesh BuildMergedMesh(const TArray<FTDMergedPart>& Parts)
	{
		FPreparedMesh Prepared;
		PrepareMergedMesh(Parts, Prepared);
		return FinishMergedMesh(Prepared);
	}

	TMap<uint64, FTDMergedMesh>& MergedMeshes()
	{
		static TMap<uint64, FTDMergedMesh> Meshes;
		return Meshes;
	}

	/** The merged mesh of one model pivot: its solid parts, or its see-through parts, which sort apart from the solid ones */
	uint64 ModelPivotKey(ETDModel Id, int32 PivotIndex, bool bSolid, float MinSize, const FVector& RootSpread, bool bDetailed)
	{
		const uint64 Options = ((uint64)FMath::RoundToInt(MinSize * 10.0f) << 24) | (((uint64)GetTypeHash(RootSpread) << 32) & ~(3ull << 62));
		return (uint64)Id | ((uint64)PivotIndex << 8) | ((uint64)(bSolid ? 1 : 0) << 16) | Options | ((uint64)(bDetailed ? 1 : 0) << 62);
	}

	/** The parts of one model pivot: its solid parts, or its see-through parts, which sort apart from the solid ones */
	void ModelPivotParts(ETDModel Id, int32 PivotIndex, bool bSolid, float MinSize, const FVector& RootSpread, bool bDetailed, TArray<FTDMergedPart>& Parts)
	{
		{
			const FTDModel& Model = TD::GetModel(Id);
			for (int32 i = 0; i < Model.NumParts; ++i)
			{
				const FTDModelPart& P = Model.Parts[i];
				const FVector Size(P.S[0], P.S[1], P.S[2]);
				// the detail pass looks at every part of the pivot, so both kinds are gathered when it runs
				if (P.Pivot != PivotIndex || Size.GetMax() < MinSize || (!bDetailed && (((ETDPartMaterial)P.Mat == ETDPartMaterial::Lit) != bSolid)))
				{
					continue;
				}
				const FVector Location(P.L[0], P.L[1], P.L[2]);
				FTDMergedPart& Part = Parts.AddDefaulted_GetRef();
				Part.Shape = (ETDShape)P.Shape;
				Part.Mat = (ETDPartMaterial)P.Mat;
				Part.Flags = P.Flags;
				Part.Location = P.Pivot == 0 ? Location * RootSpread : Location;
				Part.Size = Size;
				Part.Rotation = FQuat(P.Q[0], P.Q[1], P.Q[2], P.Q[3]);
				Part.Color = FLinearColor(P.C[0], P.C[1], P.C[2], P.C[3]);
			}
			if (bDetailed)
			{
				TDModelDetail::AddDetail(Id, PivotIndex, MinSize, Parts);
				Parts.RemoveAll([bSolid](const FTDMergedPart& Part) { return (Part.Mat == ETDPartMaterial::Lit) != bSolid; });
				for (FTDMergedPart& Part : Parts)
				{
					Part.bDetailed = true;
				}
			}
		}
	}

	/** The merged mesh of one model pivot (see ModelPivotParts) */
	const FTDMergedMesh& ModelPivotMesh(ETDModel Id, int32 PivotIndex, bool bSolid, float MinSize, const FVector& RootSpread, bool bDetailed)
	{
		const uint64 Key = ModelPivotKey(Id, PivotIndex, bSolid, MinSize, RootSpread, bDetailed);
		TArray<FTDMergedPart> Parts;
		if (!MergedMeshes().Contains(Key))
		{
			ModelPivotParts(Id, PivotIndex, bSolid, MinSize, RootSpread, bDetailed, Parts);
		}
		return TD::GetMergedMesh(Key, Parts);
	}

	const FName& ShadowCopyTag()
	{
		static const FName Tag(TEXT("ATDShadowCopy"));
		return Tag;
	}

	TAutoConsoleVariable<int32> CVarModelLods(
		TEXT("atd.ModelLods"),
		1,
		TEXT("1 gives merged meshes coarser levels of detail for zoomed-out views (round shapes with fewer sides); 0 only the full mesh. Takes effect for meshes merged afterwards."));

	TAutoConsoleVariable<int32> CVarShadowCopies(
		TEXT("atd.ShadowCopies"),
		1,
		TEXT("1 casts model shadows from plainer one-section copies (fewer triangles and draws in the shadow pass); 0 from the full meshes. Takes effect for models built afterwards."));

	TAutoConsoleVariable<int32> CVarDetailedModels(
		TEXT("atd.DetailedModels"),
		1,
		TEXT("1 builds towers, creatures and props with the detailed shapes (rounded edges, smoother curves, extra detail parts); 0 the original ones. Takes effect for models built afterwards."));
}

bool TD::UseDetailedModels()
{
	return CVarDetailedModels.GetValueOnGameThread() != 0;
}

const TCHAR* TD::ModelName(ETDModel Id)
{
	static const TCHAR* const Names[] = {
		TEXT("Harpoon"), TEXT("Jelly"), TEXT("Puffer"), TEXT("Eel"), TEXT("Mantis Shrimp"), TEXT("Octopus"), TEXT("Lionfish"), TEXT("Sawfish"),
		TEXT("Humpback"), TEXT("Acid Vent"), TEXT("Crab"), TEXT("Piranha"), TEXT("Turtle"), TEXT("Manta"), TEXT("Kraken"), TEXT("Sea Star"),
		TEXT("Cuttlefish"), TEXT("Shock Eel"), TEXT("Boulders"), TEXT("Trench"), TEXT("Goal"), TEXT("Kelp Short"), TEXT("Kelp Mid"), TEXT("Kelp Tall"),
		TEXT("Shot Harpoon"), TEXT("Shot Rock"), TEXT("Shot Lionfish"), TEXT("Shot Sawfish"), TEXT("Shot Bomb"), TEXT("Spines"),
		TEXT("Drop Shell"), TEXT("Drop Pearl"), TEXT("Drop Charm"),
		TEXT("Harpoon: Swordfish"), TEXT("Harpoon: Narwhal"), TEXT("Jelly: Box"), TEXT("Jelly: Moon"), TEXT("Puffer: Cluster"), TEXT("Puffer: Lobber"),
		TEXT("Eel: Storm"), TEXT("Eel: Overload"), TEXT("Mantis: Smasher"), TEXT("Mantis: Spearer"), TEXT("Octopus: Juggler"), TEXT("Octopus: Blue-ringed"),
		TEXT("Lionfish: Cloud"), TEXT("Lionfish: Paralytic"), TEXT("Sawfish: Ripper"), TEXT("Sawfish: Sweeper"), TEXT("Humpback: Sonar"), TEXT("Humpback: Chorus"),
		TEXT("Acid Vent: Cold"), TEXT("Acid Vent: Black") };
	static_assert(UE_ARRAY_COUNT(Names) == (int32)ETDModel::Count, "one name per model");
	return Names[FMath::Clamp((int32)Id, 0, (int32)ETDModel::Count - 1)];
}

const FTDMergedMesh& TD::GetMergedMesh(uint64 Key, const TArray<FTDMergedPart>& Parts)
{
	if (const FTDMergedMesh* Found = MergedMeshes().Find(Key))
	{
		return *Found;
	}
	return MergedMeshes().Add(Key, BuildMergedMesh(Parts));
}

const FTDMergedMesh& TD::GetDetailedMergedMesh(uint64 Id, const TArray<FTDMergedPart>& Parts)
{
	if (!UseDetailedModels())
	{
		return GetMergedMesh(MakeMergedKey(Id), Parts);
	}
	const uint64 Key = MakeMergedKey(Id | (1ull << 40));
	if (const FTDMergedMesh* Found = MergedMeshes().Find(Key))
	{
		return *Found;
	}
	TArray<FTDMergedPart> Detailed = Parts;
	TDModelDetail::AddDetail((int32)(Id & 0x7FFFF), 0.0f, Detailed);
	return GetMergedMesh(Key, Detailed);
}

UStaticMesh* TD::UnitShapeMesh(ETDShape Shape, float Size)
{
	FTDMergedPart Part;
	Part.Shape = Shape;
	Part.Segments = SegmentsForSize(Size);
	const FTDMergedMesh& Merged = GetMergedMesh(MakeMergedKey(1000 + (uint64)Shape * 100 + Part.Segments), { Part });
	return Merged.Mesh;
}

void TD::PrewarmModels()
{
	const bool bDetailed = UseDetailedModels();
	struct FRequest
	{
		uint64 Key;
		TArray<FTDMergedPart> Parts;
	};
	TArray<FRequest> Requests;
	const auto Gather = [bDetailed, &Requests](ETDModel First, ETDModel Last, float MinSize)
	{
		for (int32 Id = (int32)First; Id <= (int32)Last; ++Id)
		{
			for (int32 Pivot = 0; Pivot < GetModel((ETDModel)Id).NumPivots; ++Pivot)
			{
				for (const bool bSolid : { false, true })
				{
					const uint64 Key = ModelPivotKey((ETDModel)Id, Pivot, bSolid, MinSize, FVector::OneVector, bDetailed);
					if (!MergedMeshes().Contains(Key) && !Requests.ContainsByPredicate([Key](const FRequest& R) { return R.Key == Key; }))
					{
						FRequest& Request = Requests.AddDefaulted_GetRef();
						Request.Key = Key;
						ModelPivotParts((ETDModel)Id, Pivot, bSolid, MinSize, FVector::OneVector, bDetailed, Request.Parts);
					}
				}
			}
		}
	};
	// the same minimum part sizes the towers and creatures build with
	Gather(ETDModel::Harpoon, ETDModel::AcidVent, 2.5f);
	Gather(ETDModel::HarpoonSwordfish, ETDModel::AcidVentBlack, 2.5f);
	Gather(ETDModel::Crab, ETDModel::ShockEel, 3.0f);
	Gather(ETDModel::ShotHarpoon, ETDModel::ShotBomb, 0.0f);
	Gather(ETDModel::DropShell, ETDModel::DropCharm, 0.0f);

	// the geometry on all cores, a batch at a time (prepared meshes are large), then the meshes themselves on this thread
	constexpr int32 Batch = 48;
	for (int32 First = 0; First < Requests.Num(); First += Batch)
	{
		const int32 Count = FMath::Min(Batch, Requests.Num() - First);
		TArray<FPreparedMesh> Prepared;
		Prepared.SetNum(Count);
		ParallelFor(Count, [&](int32 i) { PrepareMergedMesh(Requests[First + i].Parts, Prepared[i]); });
		for (int32 i = 0; i < Count; ++i)
		{
			MergedMeshes().Add(Requests[First + i].Key, FinishMergedMesh(Prepared[i]));
		}
	}
}

UStaticMeshComponent* TD::AddMergedMesh(AActor* Owner, USceneComponent* Parent, const FTDMergedMesh& Merged, bool bShadows,
	const TFunction<FLinearColor(const FLinearColor& Color, uint8 Flags)>& Recolor)
{
	UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(Owner);
	Comp->SetStaticMesh(Merged.Mesh);
	Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Comp->SetGenerateOverlapEvents(false);
	Comp->SetCanEverAffectNavigation(false);
	Comp->SetupAttachment(Parent ? Parent : Owner->GetRootComponent());
	// the shadow comes from the plainer copy (built the first time one is needed), so the full mesh skips the shadow passes
	const bool bUseCopy = bShadows && CVarShadowCopies.GetValueOnGameThread() != 0;
	if (bUseCopy && !Merged.ShadowMesh && Merged.ShadowParts.Num() > 0)
	{
		Merged.ShadowMesh = BuildShadowMesh(Merged.ShadowParts);
		Merged.ShadowParts.Empty();
	}
	UStaticMesh* ShadowMesh = bUseCopy ? Merged.ShadowMesh.Get() : nullptr;
	Comp->SetCastShadow(bShadows && !ShadowMesh);
	if (Recolor)
	{
		for (int32 i = 0; i < Merged.Slots.Num(); ++i)
		{
			const FTDMergedMesh::FSlot& Slot = Merged.Slots[i];
			Comp->SetMaterial(i, SlotMaterial(Recolor(Slot.Color, Slot.Flags), Slot.Mat, Slot.Color.A));
		}
	}
	Comp->RegisterComponent();
	Owner->AddInstanceComponent(Comp);

	if (ShadowMesh)
	{
		UStaticMeshComponent* Shadow = NewObject<UStaticMeshComponent>(Owner);
		Shadow->SetStaticMesh(ShadowMesh);
		Shadow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shadow->SetGenerateOverlapEvents(false);
		Shadow->SetCanEverAffectNavigation(false);
		Shadow->SetupAttachment(Comp);
		Shadow->SetHiddenInGame(true);
		Shadow->bCastHiddenShadow = true;
		Shadow->SetCastShadow(true);
		Shadow->bAffectDistanceFieldLighting = false;
		Shadow->bVisibleInRayTracing = false;
		Shadow->bVisibleInReflectionCaptures = false;
		Shadow->ComponentTags.Add(ShadowCopyTag());
		Shadow->RegisterComponent();
		Owner->AddInstanceComponent(Shadow);
	}
	return Comp;
}

void TD::DestroyMergedMesh(UStaticMeshComponent* Comp)
{
	if (!Comp)
	{
		return;
	}
	TArray<USceneComponent*> Children;
	Comp->GetChildrenComponents(false, Children);
	for (USceneComponent* Child : Children)
	{
		if (Child && Child->ComponentHasTag(ShadowCopyTag()))
		{
			Child->DestroyComponent();
		}
	}
	Comp->DestroyComponent();
}

void TD::SetShadowCopiesEnabled(AActor* Actor, bool bEnabled)
{
	if (!Actor)
	{
		return;
	}
	TInlineComponentArray<UStaticMeshComponent*> Comps(Actor);
	for (UStaticMeshComponent* Comp : Comps)
	{
		if (Comp->ComponentHasTag(ShadowCopyTag()))
		{
			Comp->SetCastShadow(bEnabled);
		}
	}
}

USceneComponent* FTDModelInstance::Find(FName Name) const
{
	const int32 Index = PivotNames.IndexOfByKey(Name);
	return Index != INDEX_NONE ? Pivots[Index] : nullptr;
}

TArray<USceneComponent*> FTDModelInstance::FindAll(FName Name) const
{
	TArray<USceneComponent*> Found;
	for (int32 i = 0; i < PivotNames.Num(); ++i)
	{
		if (PivotNames[i] == Name)
		{
			Found.Add(Pivots[i]);
		}
	}
	return Found;
}

UMaterialInterface* TD::SharedFxMaterial(const FLinearColor& Color, float Opacity)
{
	static TMap<uint64, TStrongObjectPtr<UMaterialInstanceDynamic>> Cache;

	auto Q = [](float V) { return (uint64)FMath::Clamp(FMath::RoundToInt(V * 128.0f), 0, 0xFFFF); };
	const uint64 Key = Q(Color.R) | (Q(Color.G) << 16) | (Q(Color.B) << 32) | (Q(Opacity) << 48);
	if (const TStrongObjectPtr<UMaterialInstanceDynamic>* Found = Cache.Find(Key))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}

	// the range-ring material is unlit and translucent with Color and Opacity parameters
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/AquaticTowerDefense/M_ATD_Range.M_ATD_Range"));
	if (!Base)
	{
		return SharedColorMaterial(Color);
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	MID->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	Cache.Add(Key, TStrongObjectPtr<UMaterialInstanceDynamic>(MID));
	return MID;
}

FTDModelInstance TD::BuildModel(AActor* Owner, USceneComponent* Parent, ETDModel Id, const FTDModelOptions& Options)
{
	const FTDModel& Model = GetModel(Id);
	FTDModelInstance Out;
	Out.Pivots.Reserve(Model.NumPivots);
	Out.PivotNames.Reserve(Model.NumPivots);

	for (int32 i = 0; i < Model.NumPivots; ++i)
	{
		const FTDModelPivot& P = Model.Pivots[i];
		USceneComponent* Pivot = Parent;
		if (P.Parent >= 0)
		{
			const FVector Location(P.L[0], P.L[1], P.L[2]);
			Pivot = AddPivot(Owner, Out.Pivots[P.Parent], P.Parent == 0 ? Location * Options.RootSpread : Location);
			Pivot->SetRelativeRotation(FQuat(P.Q[0], P.Q[1], P.Q[2], P.Q[3]));
		}
		Out.Pivots.Add(Pivot);
		Out.PivotNames.Add(FName(P.Name));
	}

	ForEachModelMesh(Id, Options.MinSize, Options.RootSpread, [&](int32 PivotIndex, bool bSolid, const FTDMergedMesh& Merged)
	{
		UStaticMeshComponent* Comp = AddMergedMesh(Owner, Out.Pivots[PivotIndex], Merged, Options.bShadows && bSolid, Options.Recolor);
		if (Options.OnPart)
		{
			Options.OnPart(Comp);
		}
	}, Options.Detail);
	return Out;
}

void TD::ForEachModelMesh(ETDModel Id, float MinSize, const FVector& RootSpread, TFunctionRef<void(int32 PivotIndex, bool bSolid, const FTDMergedMesh& Merged)> Visit, int32 Detail)
{
	const bool bDetailed = Detail < 0 ? UseDetailedModels() : Detail > 0;
	const int32 NumPivots = GetModel(Id).NumPivots;
	for (int32 PivotIndex = 0; PivotIndex < NumPivots; ++PivotIndex)
	{
		for (const bool bSolid : { false, true })
		{
			const FTDMergedMesh& Merged = ModelPivotMesh(Id, PivotIndex, bSolid, MinSize, RootSpread, bDetailed);
			if (Merged.Mesh)
			{
				Visit(PivotIndex, bSolid, Merged);
			}
		}
	}
}
