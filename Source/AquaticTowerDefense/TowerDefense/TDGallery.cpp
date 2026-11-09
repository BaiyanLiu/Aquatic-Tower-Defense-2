// Aquatic Tower Defense - isometric underwater tower defense with mazing.
// Dev model gallery (ATDGallery): each model as built now next to its detailed version, for previews.

#include "TDDevTools.h"
#include "TDPlayerController.h"
#include "TDModels.h"
#include "AquaticTowerDefense.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "GameFramework/HUD.h"

void ATDPlayerController::ATDGallery(int32 PairsPerPage, int32 First, int32 Last)
{
#if !UE_BUILD_SHIPPING
	DevTask = MakeShared<FTDGallery>(this, PairsPerPage, First, Last);
#endif
}

#if !UE_BUILD_SHIPPING

namespace
{
	/** Far from the board, so nothing of the game shows */
	const FVector GalleryOrigin(60000.0f, 0.0f, 0.0f);
	constexpr float ViewPitch = -35.0f;
	constexpr float ViewYaw = 45.0f;

	/** Smallest part each kind of model builds with in the game */
	float GameMinSize(ETDModel Id)
	{
		if (Id >= ETDModel::Crab && Id <= ETDModel::ShockEel)
		{
			return 3.0f;
		}
		if ((Id >= ETDModel::Harpoon && Id <= ETDModel::AcidVent) || Id >= ETDModel::HarpoonSwordfish
			|| (Id >= ETDModel::Boulders && Id <= ETDModel::KelpTall) || Id == ETDModel::Spines)
		{
			return 2.5f;
		}
		return 0.0f;
	}

	/** Rough reach of a model: (horizontal radius, top height) from its parts and pivot offsets */
	FVector2D ModelExtent(ETDModel Id)
	{
		const FTDModel& Model = TD::GetModel(Id);
		TArray<FVector> PivotAt;
		PivotAt.SetNum(Model.NumPivots);
		for (int32 i = 0; i < Model.NumPivots; ++i)
		{
			const FTDModelPivot& P = Model.Pivots[i];
			PivotAt[i] = (P.Parent >= 0 ? PivotAt[P.Parent] : FVector::ZeroVector) + FVector(P.L[0], P.L[1], P.L[2]);
		}
		FVector2D Out(10.0f, 10.0f);
		for (int32 i = 0; i < Model.NumParts; ++i)
		{
			const FTDModelPart& P = Model.Parts[i];
			const FVector At = PivotAt[P.Pivot] + FVector(P.L[0], P.L[1], P.L[2]);
			const float Half = FVector(P.S[0], P.S[1], P.S[2]).GetMax() * 0.5f;
			Out.X = FMath::Max(Out.X, FVector2D(At.X, At.Y).Size() + Half);
			Out.Y = FMath::Max(Out.Y, At.Z + Half);
		}
		return Out;
	}

	void AddLabel(AActor* Owner, const FVector& At, const FString& Text, float Size, const FColor& Color)
	{
		UTextRenderComponent* Label = NewObject<UTextRenderComponent>(Owner);
		Label->SetupAttachment(Owner->GetRootComponent());
		Label->SetText(FText::FromString(Text));
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
		Label->SetWorldSize(Size);
		Label->SetTextRenderColor(Color);
		Label->SetCastShadow(false);
		Label->RegisterComponent();
		Label->SetWorldLocation(At + FVector(0.0f, 0.0f, Size * 0.8f));
		Label->SetWorldRotation((-FRotator(ViewPitch, ViewYaw, 0.0f).Vector()).Rotation());
		Owner->AddInstanceComponent(Label);
	}

	AActor* SpawnHolder(UWorld* World, const FVector& At, const FRotator& Rotation)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), Params);
		USceneComponent* Root = NewObject<USceneComponent>(Holder);
		Holder->SetRootComponent(Root);
		Root->RegisterComponent();
		Holder->AddInstanceComponent(Root);
		Root->SetWorldLocationAndRotation(At, Rotation);
		return Holder;
	}
}

FTDGallery::FTDGallery(ATDPlayerController* InPC, int32 InPairsPerPage, int32 InFirst, int32 InLast)
	: PC(InPC)
	, PairsPerPage(FMath::Clamp(InPairsPerPage, 1, 16))
	, First(InFirst < 0 ? 0 : FMath::Min(InFirst, (int32)ETDModel::Count - 1))
	, Last(InLast < 0 ? (int32)ETDModel::Count - 1 : FMath::Clamp(InLast, 0, (int32)ETDModel::Count - 1))
{
	Next = First;
	Timer = 1.0f;
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("GALLERY models %d to %d, %d per page"), First, Last, PairsPerPage);
}

void FTDGallery::ClearPage()
{
	for (const TWeakObjectPtr<AActor>& Actor : Spawned)
	{
		if (Actor.IsValid())
		{
			Actor->Destroy();
		}
	}
	Spawned.Reset();
}

void FTDGallery::BuildPage()
{
	ATDPlayerController* P = PC.Get();
	UWorld* World = P->GetWorld();
	const int32 Count = FMath::Min(PairsPerPage, Last - Next + 1);
	const int32 Cols = Count <= 2 ? Count : FMath::DivideAndRoundUp(Count, 2);
	const int32 Rows = FMath::DivideAndRoundUp(Count, Cols);

	float Reach = 20.0f, Height = 20.0f;
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector2D Extent = ModelExtent((ETDModel)(Next + i));
		Reach = FMath::Max(Reach, Extent.X);
		Height = FMath::Max(Height, Extent.Y);
	}

	const FRotator View(ViewPitch, ViewYaw, 0.0f);
	const FVector Right = FRotationMatrix(FRotator(0.0f, ViewYaw, 0.0f)).GetUnitAxis(EAxis::Y);
	const FVector Ahead = FRotator(0.0f, ViewYaw, 0.0f).Vector();
	const float SinPitch = FMath::Sin(FMath::DegreesToRadians(-ViewPitch));
	const float CosPitch = FMath::Cos(FMath::DegreesToRadians(-ViewPitch));
	const float Sep = Reach * 2.3f;
	const float CellW = Sep * 2.25f;
	const float CellScreenH = (Reach * 2.0f * SinPitch + Height * CosPitch) * 1.45f;
	const float CellD = CellScreenH / SinPitch;

	AActor* Stage = SpawnHolder(World, GalleryOrigin, FRotator::ZeroRotator);
	Spawned.Add(Stage);
	TD::AddShape(Stage, Stage->GetRootComponent(), ETDShape::Plane, FVector(0.0f, 0.0f, -0.5f), FVector(20000.0f, 20000.0f, 1.0f), FLinearColor(0.21f, 0.27f, 0.27f));

	const float LabelSize = FMath::Clamp(Reach * 0.32f, 9.0f, 40.0f);
	FString PageNames;
	for (int32 i = 0; i < Count; ++i)
	{
		const ETDModel Id = (ETDModel)(Next + i);
		const int32 Col = i % Cols;
		const int32 Row = i / Cols;
		const FVector Center = GalleryOrigin + Right * ((Col - (Cols - 1) * 0.5f) * CellW) + Ahead * (((Rows - 1) * 0.5f - Row) * CellD);
		for (int32 Detail = 0; Detail < 2; ++Detail)
		{
			const FVector At = Center + Right * ((Detail == 0 ? -0.5f : 0.5f) * Sep);
			// -ATDGalleryYaw=<deg> turns every model, e.g. 135 for a side view
			float ModelYaw = 200.0f;
			FParse::Value(FCommandLine::Get(), TEXT("ATDGalleryYaw="), ModelYaw);
			AActor* Holder = SpawnHolder(World, At, FRotator(0.0f, ModelYaw, 0.0f));
			FTDModelOptions Options;
			Options.MinSize = GameMinSize(Id);
			Options.Detail = (int8)Detail;
			TD::BuildModel(Holder, Holder->GetRootComponent(), Id, Options);
			Spawned.Add(Holder);
			AddLabel(Stage, At - Ahead * Reach * 1.25f, Detail == 0 ? TEXT("current") : TEXT("detailed"), LabelSize * 0.7f,
				Detail == 0 ? FColor(200, 200, 200) : FColor(255, 220, 120));
		}
		AddLabel(Stage, Center - Ahead * (Reach * 1.25f + LabelSize * 2.2f), TD::ModelName(Id), LabelSize, FColor::White);
		PageNames += FString::Printf(TEXT("%s%d %s"), i ? TEXT(", ") : TEXT(""), (int32)Id, TD::ModelName(Id));
	}

	float Aspect = 16.0f / 9.0f;
	int32 SizeX = 0, SizeY = 0;
	P->GetViewportSize(SizeX, SizeY);
	if (SizeX > 0 && SizeY > 0)
	{
		Aspect = (float)SizeX / SizeY;
	}
	const float OrthoWidth = FMath::Max(Cols * CellW, Rows * CellScreenH * Aspect) * 1.04f;
	const FVector LookAt = GalleryOrigin + FVector(0.0f, 0.0f, Height * 0.35f) - Ahead * Reach * 0.4f;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Cam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform(View, LookAt - View.Vector() * 2600.0f), Params);
	Cam->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
	Cam->GetCameraComponent()->SetOrthoWidth(OrthoWidth);
	Cam->GetCameraComponent()->bConstrainAspectRatio = false;
	Cam->GetCameraComponent()->SetUseCameraHeightAsViewTarget(false);
	Cam->GetCameraComponent()->bOverrideAspectRatioAxisConstraint = true;
	Cam->GetCameraComponent()->AspectRatioAxisConstraint = EAspectRatioAxisConstraint::AspectRatio_MaintainXFOV;
	Spawned.Add(Cam);
	P->SetViewTarget(Cam);
	UE_LOG(LogAquaticTowerDefense, Display, TEXT("GALLERY page %d (reach %.0f, height %.0f, width %.0f, aspect %.2f): %s"), Page, Reach, Height, OrthoWidth, Aspect, *PageNames);
	Next += Count;
}

bool FTDGallery::Tick(float RealDeltaTime)
{
	ATDPlayerController* P = PC.Get();
	if (!P)
	{
		return false;
	}
	Timer -= RealDeltaTime;
	if (Timer > 0.0f)
	{
		return true;
	}
	if (Phase == 0)
	{
		ClearPage();
		if (Next > Last)
		{
			P->SetViewTarget(P->GetPawn());
			if (AHUD* HUD = P->GetHUD())
			{
				HUD->bShowHUD = true;
			}
			UE_LOG(LogAquaticTowerDefense, Display, TEXT("GALLERY DONE: %d pages in %s"), Page, *(FPaths::ProjectSavedDir() / TEXT("Gallery")));
			if (FParse::Param(FCommandLine::Get(), TEXT("ATDGalleryQuit")))
			{
				P->ConsoleCommand(TEXT("Quit"));
			}
			return false;
		}
		if (AHUD* HUD = P->GetHUD())
		{
			HUD->bShowHUD = false;
		}
		BuildPage();
		Phase = 1;
		Timer = 2.5f;	// lighting settles
		return true;
	}
	FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Gallery/Page%02d.png"), Page), false, false);
	Phase = 0;
	Timer = 0.8f;
	++Page;
	return true;
}

#endif
