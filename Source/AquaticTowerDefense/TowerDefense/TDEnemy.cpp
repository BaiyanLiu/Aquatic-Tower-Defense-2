// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDEnemy.h"
#include "TDModels.h"
#include "TDAudio.h"
#include "TDGrid.h"
#include "TDGameMode.h"
#include "TDTower.h"
#include "TDFx.h"
#include "TDRun.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

namespace
{
	constexpr float FlyHeight = 140.0f;
}

ATDEnemy::ATDEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ATDEnemy::InitEnemy(ETDEnemyType InType, float HealthMultiplier, int32 InWave, ATDGrid* InGrid)
{
	Type = InType;
	Wave = InWave;
	Grid = InGrid;
	MaxHealth = GetDef().Health * HealthMultiplier;
	if (GetDef().bFlying)
	{
		// flyers skip the maze: they only get the mid part of the wave pressure (see TD::WavePressure)
		MaxHealth /= TD::WavePressureLatePart(Wave);
	}
	Health = MaxHealth;
	SplitsLeft = GetDef().Splits;
	SplitHealth = SplitsLeft > 0 ? MaxHealth / SplitsLeft : 0.0f;
	AnimTime = FMath::FRandRange(0.0f, 10.0f);
	Lateral = GetDef().bBoss ? 0.0f : FMath::FRandRange(-17.0f, 17.0f);

	CurrentCell = TD::SpawnCell();
	NextCell = CurrentCell;
	PathPos = TD::CellToWorld(CurrentCell);

	if (!IsFlying() && Grid)
	{
		FIntPoint First;
		if (Grid->GetNextStep(CurrentCell, MoveDir, First))
		{
			NextCell = First;
		}
	}

	VisualLocation = PathPos;
	VisualRotation = FRotator(0, 90, 0);	// spawn faces down the board (+col = +Y)
	SetActorLocationAndRotation(VisualLocation, VisualRotation);
	BuildVisuals();
}

void ATDEnemy::InitAsSplit(const ATDEnemy* Parent)
{
	Type = Parent->Type;
	Wave = Parent->Wave;
	Grid = Parent->Grid;

	// original rule: a copy has 1/(2*splits) of the parent's max health, is 1.5x faster and splits half as often
	TD::SplitCopyStats(Parent->MaxHealth, Parent->SplitsLeft, MaxHealth, SplitsLeft);
	Health = MaxHealth;
	SpeedMultiplier = Parent->SpeedMultiplier * 1.5f;
	SplitHealth = SplitsLeft > 0 ? MaxHealth / SplitsLeft : 0.0f;
	bSplitCopy = true;
	RewardScale = Parent->RewardScale;

	AnimTime = FMath::FRandRange(0.0f, 10.0f);
	Lateral = FMath::FRandRange(-17.0f, 17.0f);
	CurrentCell = Parent->CurrentCell;
	NextCell = Parent->NextCell;
	MoveDir = Parent->MoveDir;
	PathPos = Parent->PathPos;

	VisualLocation = Parent->GetActorLocation();
	VisualRotation = Parent->GetActorRotation();
	SetActorLocationAndRotation(VisualLocation, VisualRotation);
	SetActorScale3D(FVector(1.0f / 1.5f));
	BuildVisuals();
}

// ---------------------------------------------------------------------------------------------
// Visuals
// ---------------------------------------------------------------------------------------------

namespace
{
	constexpr uint64 ShadowBlobKey = 200;

	/** Ground shadow diameter per creature (unreal units) */
	float ShadowSize(ETDEnemyType Type)
	{
		switch (Type)
		{
		case ETDEnemyType::Piranha:		return 46.0f;
		case ETDEnemyType::Turtle:		return 84.0f;
		case ETDEnemyType::Manta:		return 80.0f;
		case ETDEnemyType::Kraken:		return 130.0f;
		case ETDEnemyType::ShockEel:	return 58.0f;
		default:						return 62.0f;
		}
	}
}

void ATDEnemy::BuildVisuals()
{
	USceneComponent* Root = GetRootComponent();

	{
		const float Diameter = ShadowSize(Type);
		const float Opacity = IsFlying() ? 0.4f : 0.6f;
		TArray<FTDMergedPart> Blobs;
		// the detailed look stacks more, finer discs, so the shadow fades smoothly from a dark core to nothing
		const bool bSoft = TD::UseDetailedModels();
		const int32 BlobLayers = bSoft ? 5 : 2;
		for (int32 i = 0; i < BlobLayers; ++i)
		{
			// a wide faint disc under smaller darker ones reads as a soft edge
			const float D = bSoft ? Diameter * (1.08f - 0.15f * i) : Diameter * (i == 0 ? 1.0f : 0.7f);
			FTDMergedPart& Blob = Blobs.AddDefaulted_GetRef();
			Blob.Shape = ETDShape::Cylinder;
			Blob.Mat = ETDPartMaterial::Glass;
			Blob.Location = FVector(0, 0, 1.0f + i * 0.3f);
			Blob.Size = FVector(D, D, 0.4f);
			Blob.Color = FLinearColor(0.0f, 0.02f, 0.03f, bSoft ? Opacity * (0.18f + 0.04f * i) : Opacity * (i == 0 ? 0.5f : 0.6f));
		}
		TD::AddMergedMesh(this, Root, TD::GetMergedMesh(TD::MakeMergedKey(ShadowBlobKey + (uint64)Type + (bSoft ? 64ull : 0ull)), Blobs), false);
	}

	ETDModel Model = ETDModel::Crab;
	switch (Type)
	{
	case ETDEnemyType::Piranha:		Model = ETDModel::Piranha; break;
	case ETDEnemyType::Turtle:		Model = ETDModel::Turtle; break;
	case ETDEnemyType::Manta:		Model = ETDModel::Manta; break;
	case ETDEnemyType::Kraken:		Model = ETDModel::Kraken; break;
	case ETDEnemyType::SeaStar:		Model = ETDModel::SeaStar; break;
	case ETDEnemyType::Cuttlefish:	Model = ETDModel::Cuttlefish; break;
	case ETDEnemyType::ShockEel:	Model = ETDModel::ShockEel; break;
	default:						break;
	}

	const FTDModel& ModelData = TD::GetModel(Model);
	for (int32 i = 0; i < ModelData.NumPivots; ++i)
	{
		const FTDModelPivot& P = ModelData.Pivots[i];
		PivotParents.Add(P.Parent);
		const FQuat Rotation(P.Q[0], P.Q[1], P.Q[2], P.Q[3]);
		PivotLocal.Add(P.Parent >= 0 ? FTransform(Rotation, FVector(P.L[0], P.L[1], P.L[2])) : FTransform::Identity);
		const FName Name(P.Name);
		if (Name == TEXT("Body"))
		{
			BodyPivot = i;
		}
		else if (Name == TEXT("Limb"))
		{
			LimbPivots.Add(i);
			LimbYaws.Add(Rotation.Rotator().Yaw);
		}
	}
	if (BodyPivot == 0)
	{
		// a model without a body pivot still bobs: give it an empty one for the animation to move
		PivotParents.Add(0);
		PivotLocal.Add(FTransform::Identity);
		BodyPivot = PivotParents.Num() - 1;
	}
	PivotInActor.SetNum(PivotLocal.Num());
	BodyBase = PivotLocal[BodyPivot].GetTranslation();

	switch (Type)
	{
	case ETDEnemyType::Crab:
		// crabs scuttle sideways, so the body is turned 90 degrees
		PivotLocal[BodyPivot].SetRotation(FRotator(0, 90, 0).Quaternion());
		break;
	case ETDEnemyType::Manta:
		BodyBase.Z = FlyHeight;
		PivotLocal[BodyPivot].SetTranslation(BodyBase);
		break;
	default:
		break;
	}

	// creatures bob and wobble every frame, so real shadows flicker as they move; the ground blob stays steady
	Crowd = ATDCrowd::Get(GetWorld());
	TD::ForEachModelMesh(Model, 3.0f, FVector::OneVector, [this, Root](int32 PivotIndex, bool bSolid, const FTDMergedMesh& Merged)
	{
		if (bSolid && Crowd)
		{
			FSolidPart& Part = SolidParts.AddDefaulted_GetRef();
			Part.Crowd = Crowd->AddPart(Merged);
			Part.Pivot = PivotIndex;
			Part.MeshIndex = SolidMeshes.Add(Merged);
		}
		else if (!bSolid)
		{
			ShellParts.Add(TD::AddMergedMesh(this, Root, Merged, false));
			ShellPivots.Add(PivotIndex);
		}
	});
	VisualScale = FVector(TD::SpawnPopScale(0.0f));
	UpdateVisuals();
}

void ATDEnemy::UpdateVisuals()
{
	// the animation and the movement only set relative transforms, so a whole creature updates once per frame;
	// a big hit tips the creature back and down and squashes it for a moment (the flinch), and it leans into turns
	USceneComponent* Root = GetRootComponent();
	FVector Location = VisualLocation;
	FRotator Rotation = VisualRotation;
	FVector Scale = VisualScale;
	if (!bDying)
	{
		Rotation.Roll += Bank;
		if (FlinchTime > 0.0f)
		{
			const float K = FMath::Sin(PI * (1.0f - FlinchTime / TD::FlinchSeconds));
			Rotation.Pitch += 12.0f * K;
			Location.Z -= 3.0f * K;
			Scale *= FVector(1.0f + 0.08f * K, 1.0f + 0.08f * K, 1.0f - 0.12f * K);
		}
	}

	// the model's scale sits on pivot 0 (the root stays at the actor's own scale), so every part and its offset follow it
	PivotInActor[0] = FTransform(FQuat::Identity, FVector::ZeroVector, Scale);
	for (int32 i = 1; i < PivotLocal.Num(); ++i)
	{
		PivotInActor[i] = PivotLocal[i] * PivotInActor[PivotParents[i]];
	}
	for (int32 i = 0; i < ShellParts.Num(); ++i)
	{
		const FTransform& InActor = PivotInActor[ShellPivots[i]];
		ShellParts[i]->SetRelativeLocation_Direct(InActor.GetTranslation());
		ShellParts[i]->SetRelativeRotation_Direct(InActor.Rotator());
		ShellParts[i]->SetRelativeScale3D_Direct(InActor.GetScale3D());
	}

	Root->SetRelativeLocation_Direct(Location);
	Root->SetRelativeRotation_Direct(Rotation);
	Root->UpdateComponentToWorld();

	if (Crowd)
	{
		const FTransform& ActorTransform = Root->GetComponentTransform();
		for (const FSolidPart& Part : SolidParts)
		{
			Crowd->SetPartTransform(Part.Crowd, PivotInActor[Part.Pivot] * ActorTransform);
		}
	}
}

void ATDEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Crowd)
	{
		for (FSolidPart& Part : SolidParts)
		{
			Crowd->RemovePart(Part.Crowd);
		}
	}
	SolidParts.Reset();
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------------------------

float ATDEnemy::GetCurrentSpeed() const
{
	if (StunTime > 0.0f)
	{
		return 0.0f;
	}
	float Slow = FMath::Min(SlowTime > 0.0f ? SlowFactor : 1.0f, AuraSlow);
	float Enrage = 1.0f;
	if (IsTitan())
	{
		// the Abyssal Kraken shrugs off half of every slow and speeds up once badly hurt
		Slow = 1.0f - (1.0f - Slow) * 0.5f;
		Enrage = (MaxHealth > 0.0f && Health < MaxHealth * 0.5f) ? 1.5f : 1.0f;
	}
	return GetDef().Speed * SpeedMultiplier * Slow * Enrage;
}

void ATDEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bDying)
	{
		TickDying(DeltaTime);
		return;
	}
	if (!IsAlive())
	{
		return;
	}
	FlinchTime = FMath::Max(0.0f, FlinchTime - DeltaTime);

	AnimTime += DeltaTime;

	if (SlowTime > 0.0f)
	{
		SlowTime -= DeltaTime;
		if (SlowTime <= 0.0f)
		{
			SlowFactor = 1.0f;
		}
	}

	TickEffects(DeltaTime);
	if (!IsAlive())
	{
		return;
	}

	if (StunTime > 0.0f)
	{
		StunTime -= DeltaTime;
	}
	else if (IsFlying())
	{
		TickFlying(DeltaTime);
	}
	else
	{
		TickGround(DeltaTime);
	}

	if (!IsAlive())
	{
		return;
	}

	// creature-specific locomotion, on a clock that follows the creature's real pace
	const float Pace = StunTime > 0.0f ? 0.12f : FMath::Clamp(GetCurrentSpeed() / FMath::Max(0.01f, GetDef().Speed), 0.25f, 2.5f);
	GaitRate = FMath::FInterpTo(GaitRate, Pace, DeltaTime, 6.0f);
	GaitTime += DeltaTime * GaitRate;
	StunWobble = FMath::FInterpTo(StunWobble, StunTime > 0.0f ? 1.0f : 0.0f, DeltaTime, 10.0f);
	Age += DeltaTime;
	VisualScale = FVector(TD::SpawnPopScale(Age));
	const float G = GaitTime;

	FVector BodyOffset = FVector::ZeroVector;
	FRotator BodyRot = FRotator::ZeroRotator;
	FVector BodyScale = FVector::OneVector;
	switch (Type)
	{
	case ETDEnemyType::Crab:
	{
		// crabs scuttle sideways (the body is turned 90 degrees), rocking over alternating legs; each leg lifts as it swings forward
		const float Step = G * 14.0f;
		BodyOffset.Z = FMath::Abs(FMath::Sin(Step)) * 2.5f;
		BodyRot = FRotator(0, 90, FMath::Sin(Step) * 4.0f);
		for (int32 i = 0; i < LimbPivots.Num(); ++i)
		{
			const float Side = PivotLocal[LimbPivots[i]].GetTranslation().Y >= 0.0f ? 1.0f : -1.0f;
			const float Phase = Step + (((i % 4) + (Side > 0.0f ? 1 : 0)) % 2) * PI;
			const float Lift = FMath::Max(0.0f, FMath::Cos(Phase)) * 14.0f;
			PivotLocal[LimbPivots[i]].SetRotation(FRotator(FMath::Sin(Phase) * 20.0f, 0, -Side * Lift).Quaternion());
		}
		break;
	}
	case ETDEnemyType::Piranha:
	{
		// the tail beats and the body swings against it, so the fish swims in an S
		const float Beat = G * 16.0f;
		BodyOffset.Z = FMath::Sin(G * 4.0f) * 3.0f;
		BodyRot.Yaw = FMath::Sin(Beat + PI) * 4.0f;
		if (LimbPivots.Num() > 0)
		{
			PivotLocal[LimbPivots[0]].SetRotation(FRotator(0, FMath::Sin(Beat - 0.6f) * 28.0f, 0).Quaternion());
		}
		break;
	}
	case ETDEnemyType::Turtle:
	{
		// a crawl: diagonal flippers stroke together, lifting as they reach forward, and the shell rocks and bobs with each stroke
		const float Stroke = G * 3.0f;
		BodyOffset.Z = FMath::Abs(FMath::Sin(Stroke)) * 1.5f;
		BodyRot.Roll = FMath::Sin(Stroke) * 2.5f;
		for (int32 i = 0; i < LimbPivots.Num(); ++i)
		{
			const FVector Rest = PivotLocal[LimbPivots[i]].GetTranslation();
			const float Side = Rest.Y >= 0.0f ? 1.0f : -1.0f;
			const bool bDiagonal = (Rest.X >= 0.0f) == (Side > 0.0f);
			const float Phase = Stroke + (bDiagonal ? 0.0f : PI);
			const float Lift = FMath::Max(0.0f, FMath::Cos(Phase)) * 10.0f;
			PivotLocal[LimbPivots[i]].SetRotation(FRotator(0, Side * FMath::Sin(Phase) * 25.0f, -Side * Lift).Quaternion());
		}
		break;
	}
	case ETDEnemyType::Manta:
	{
		// wings flap with the tips lagging, the body lifts on each downstroke and pitches with it
		const float Flap = G * 4.0f;
		BodyOffset.Z = FMath::Sin(AnimTime * 1.1f) * 7.0f - FMath::Cos(Flap) * 3.0f;
		BodyRot.Pitch = FMath::Sin(Flap + 0.5f) * 3.0f;
		for (int32 i = 0; i < LimbPivots.Num(); ++i)
		{
			const float Side = (i == 0) ? 1.0f : -1.0f;
			PivotLocal[LimbPivots[i]].SetRotation(FRotator(FMath::Sin(Flap - 0.8f) * 4.0f, 0, Side * FMath::Sin(Flap) * 24.0f).Quaternion());
		}
		break;
	}
	case ETDEnemyType::Kraken:
	{
		// the mantle pulses like a jet (squeezes and rises, then relaxes), and a wave runs round the ring of arms
		const float Pulse = FMath::Sin(G * 1.5f);
		BodyOffset.Z = Pulse * 5.0f;
		BodyScale = FVector(1.0f - 0.04f * Pulse, 1.0f - 0.04f * Pulse, 1.0f + 0.05f * Pulse);
		for (int32 i = 0; i < LimbPivots.Num(); ++i)
		{
			PivotLocal[LimbPivots[i]].SetRotation(FRotator(FMath::Sin(G * 2.5f - i * 0.8f) * 14.0f + Pulse * 4.0f, LimbYaws[i] + FMath::Sin(G * 1.7f + i) * 6.0f, 0).Quaternion());
		}
		break;
	}
	case ETDEnemyType::SeaStar:
		// rolls along, wobbling on its points
		BodyRot = FRotator(FMath::Sin(G * 3.0f) * 4.0f, G * 70.0f, FMath::Cos(G * 3.0f) * 4.0f);
		BodyOffset.Z = FMath::Abs(FMath::Sin(G * 4.5f)) * 1.5f;
		break;
	case ETDEnemyType::Cuttlefish:
	{
		// the body ripples as it hovers, and the arms trail and curl in a wave
		BodyOffset.Z = FMath::Sin(G * 3.0f) * 3.0f;
		BodyRot.Pitch = FMath::Sin(G * 3.0f + 0.7f) * 2.0f;
		for (int32 i = 0; i < LimbPivots.Num(); ++i)
		{
			PivotLocal[LimbPivots[i]].SetRotation(FRotator(FMath::Sin(G * 4.0f + i * 0.7f) * 8.0f, FMath::Sin(G * 6.0f + i) * 15.0f, 0).Quaternion());
		}
		break;
	}
	case ETDEnemyType::ShockEel:
		// a wave runs down the body, growing towards the tail
		BodyOffset.Z = FMath::Sin(G * 2.0f) * 1.5f;
		for (int32 i = 0; i < LimbPivots.Num(); ++i)
		{
			PivotLocal[LimbPivots[i]].SetTranslation(FVector(16 - i * 8.5f, FMath::Sin(G * 7.0f - i * 0.6f) * (2.0f + i * 0.9f), FMath::Sin(G * 3.0f - i * 0.4f) * 1.5f));
		}
		break;
	default:
		break;
	}
	if (StunWobble > 0.01f)
	{
		// stunned: a dizzy wobble
		BodyRot.Yaw += FMath::Sin(AnimTime * 11.0f) * 7.0f * StunWobble;
		BodyRot.Roll += FMath::Sin(AnimTime * 7.0f) * 5.0f * StunWobble;
	}
	PivotLocal[BodyPivot] = FTransform(BodyRot.Quaternion(), BodyBase + BodyOffset, BodyScale);

	UpdateVisuals();
}

void ATDEnemy::TickGround(float DeltaTime)
{
	if (!Grid)
	{
		return;
	}

	float Move = GetCurrentSpeed() * TD::TileSize * DeltaTime;
	for (int32 Guard = 0; Move > 0.0f && Guard < 8; ++Guard)
	{
		const FVector Target = TD::CellToWorld(NextCell);
		const float Remaining = FVector::Dist2D(PathPos, Target);
		if (Move < Remaining)
		{
			PathPos += (Target - PathPos).GetSafeNormal2D() * Move;
			break;
		}

		// arrived at the next tile: pick a new one from the current field
		PathPos = Target;
		Move -= Remaining;
		if (NextCell != CurrentCell)
		{
			MoveDir = NextCell - CurrentCell;
		}
		CurrentCell = NextCell;

		if (CurrentCell == TD::GoalCell())
		{
			ReachGoal();
			return;
		}

		FIntPoint Step;
		if (!Grid->GetNextStep(CurrentCell, MoveDir, Step))
		{
			break;	// no route (placement rules should prevent this); wait in place
		}
		NextCell = Step;
	}

	// visual position: path position plus a sideways offset, smoothed through turns
	const FIntPoint Heading = (NextCell != CurrentCell) ? (NextCell - CurrentCell) : MoveDir;
	const FVector Forward = TD::GridDirToWorld(Heading);
	const FVector Side(-Forward.Y, Forward.X, 0.0f);
	const FVector Desired = PathPos + Side * Lateral;
	VisualLocation = FMath::VInterpTo(VisualLocation, Desired, DeltaTime, 10.0f);
	const float OldYaw = VisualRotation.Yaw;
	VisualRotation = FMath::RInterpTo(VisualRotation, Forward.Rotation(), DeltaTime, 8.0f);

	// lean into the turn: swimmers bank harder than crawlers
	const bool bSwimmer = Type == ETDEnemyType::Piranha || Type == ETDEnemyType::Cuttlefish || Type == ETDEnemyType::ShockEel;
	const float TurnRate = DeltaTime > 0.0f ? FMath::FindDeltaAngleDegrees(OldYaw, VisualRotation.Yaw) / DeltaTime : 0.0f;
	const float MaxBank = bSwimmer ? 20.0f : 8.0f;
	Bank = FMath::FInterpTo(Bank, FMath::Clamp(TurnRate * (bSwimmer ? 0.09f : 0.04f), -MaxBank, MaxBank), DeltaTime, 8.0f);
}

void ATDEnemy::TickFlying(float DeltaTime)
{
	const FVector Goal = TD::CellToWorld(TD::GoalCell());
	const float Move = GetCurrentSpeed() * TD::TileSize * DeltaTime;
	const FVector ToGoal = Goal - PathPos;
	const float Remaining = ToGoal.Size2D();
	if (Remaining <= Move)
	{
		ReachGoal();
		return;
	}

	const FVector Dir = ToGoal.GetSafeNormal2D();
	PathPos += Dir * Move;
	CurrentCell = TD::WorldToCell(PathPos);
	NextCell = CurrentCell;

	const FVector Side(-Dir.Y, Dir.X, 0.0f);
	VisualLocation = PathPos + Side * Lateral;
	VisualRotation = Dir.Rotation();
}

void ATDEnemy::ReachGoal()
{
	bFinished = true;
	if (ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>())
	{
		GameMode->OnEnemyLeaked(this);
	}
	else
	{
		Destroy();
	}
}

// ---------------------------------------------------------------------------------------------
// Info
// ---------------------------------------------------------------------------------------------

FString ATDEnemy::GetDisplayName() const
{
	FString Name = bSplitCopy ? FString::Printf(TEXT("Small %s"), GetDef().Name) : FString(GetDef().Name);
	if (Mods & TDMod::Shielded)	Name = TEXT("Shielded ") + Name;
	if (Mods & TDMod::Fast)		Name = TEXT("Fast ") + Name;
	if (Mods & TDMod::Elite)	Name = TEXT("Elite ") + Name;
	if (Mods & TDMod::Titan)	Name = TEXT("Abyssal ") + Name;
	return Name;
}

void ATDEnemy::ApplyModifiers(uint8 InMods)
{
	Mods = InMods;
	USceneComponent* Root = GetRootComponent();

	auto Glow = [this, Root](ETDShape Shape, const FVector& Loc, const FVector& Size, const FLinearColor& Color, float Opacity)
	{
		UStaticMeshComponent* Comp = TD::AddShape(this, Root, Shape, Loc, Size, Color, FRotator::ZeroRotator, TD::SharedFxMaterial(Color, Opacity));
		Comp->SetCastShadow(false);
		return Comp;
	};

	if (Mods & TDMod::Elite)
	{
		// mini-boss: much tougher, bigger, gold-ringed and crowned
		MaxHealth *= 6.0f;
		Health = MaxHealth;
		SplitHealth = SplitsLeft > 0 ? MaxHealth / SplitsLeft : 0.0f;
		SetActorScale3D(GetActorScale3D() * 1.5f);
		Glow(ETDShape::Cylinder, FVector(0, 0, 2), FVector(90, 90, 0.5f), FLinearColor(1.0f, 0.75f, 0.2f), 0.45f);
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector Spot = FRotator(0, i * 120.0f, 0).RotateVector(FVector(9, 0, 0));
			UStaticMeshComponent* Spike = TD::AddShape(this, Root, ETDShape::Cone, Spot + FVector(0, 0, GetBarHeight() / GetActorScale3D().Z - 4.0f), FVector(8, 8, 14), FLinearColor(1.0f, 0.78f, 0.2f));
			Spike->SetCastShadow(false);
		}
	}
	if (Mods & TDMod::Titan)
	{
		// final boss: 3.2x the health behind a shield of 5% of that, heavier armor, resists slows and bleed, immune to stuns,
		// and enrages below half health. Leaking it costs every remaining life.
		// the wave pressure factor keeps regular waves up with the towers; the titan is tuned on its own
		MaxHealth *= TitanHealthFactor / TD::WavePressure(Wave);
		Health = MaxHealth;
		MaxShield = MaxHealth * TitanShieldFraction;
		Shield = MaxShield;
		SpeedMultiplier *= 0.8f;
		SetActorScale3D(GetActorScale3D() * 2.0f);
		Glow(ETDShape::Cylinder, FVector(0, 0, 2), FVector(110, 110, 0.5f), FLinearColor(0.9f, 0.1f, 0.25f), 0.5f);
		const float D = (GetBarHeight() / GetActorScale3D().Z + 10.0f) * 1.2f;
		ShieldBubble = Glow(ETDShape::Sphere, FVector(0, 0, D * 0.4f), FVector(D, D, D * 0.9f), FLinearColor(0.75f, 0.2f, 0.9f), 0.3f);
		for (int32 i = 0; i < 5; ++i)
		{
			const FVector Spot = FRotator(0, i * 72.0f, 0).RotateVector(FVector(11, 0, 0));
			UStaticMeshComponent* Spike = TD::AddShape(this, Root, ETDShape::Cone, Spot + FVector(0, 0, GetBarHeight() / GetActorScale3D().Z - 4.0f), FVector(8, 8, 18), FLinearColor(0.85f, 0.1f, 0.2f));
			Spike->SetCastShadow(false);
		}
	}
	if (Mods & TDMod::Fast)
	{
		SpeedMultiplier *= 1.45f;
		Glow(ETDShape::Cylinder, FVector(0, 0, 2.5f), FVector(64, 64, 0.5f), FLinearColor(1.0f, 0.95f, 0.3f), 0.4f);
	}
	if (Mods & TDMod::Shielded)
	{
		MaxShield = MaxHealth * ShieldedShieldFraction;
		Shield = MaxShield;
		const float D = (GetBarHeight() / GetActorScale3D().Z + 10.0f) * 1.2f;
		ShieldBubble = Glow(ETDShape::Sphere, FVector(0, 0, D * 0.4f), FVector(D, D, D * 0.9f), FLinearColor(0.4f, 0.85f, 1.0f), 0.28f);
	}
	UpdateVisuals();
}

int32 ATDEnemy::GetReward() const
{
	return GetDef().Reward * (IsTitan() ? 25 : IsElite() ? 5 : 1);
}

float ATDEnemy::GetDropChance() const
{
	// elites have an even chance of an item
	return (GetDef().DropChance + (IsElite() ? 0.5f : 0.0f) + (IsTitan() ? 4.0f : 0.0f)) * TD::DropRateScale;
}

float ATDEnemy::GetBaseArmor() const
{
	const float Armor = GetDef().Armor + GetDef().ArmorGain * FMath::Max(0, Wave - 1) + TD::EndlessArmorBonus(Wave) + (IsTitan() ? TitanArmorBonus : 0.0f);
	// Armored Tide (mutator)
	return Armor > 0.0f ? Armor * TD::Run().ArmorScale() : Armor;
}

float ATDEnemy::GetGoalDistance() const
{
	if (IsFlying() || !Grid)
	{
		return FVector::Dist2D(PathPos, TD::CellToWorld(TD::GoalCell())) / TD::TileSize;
	}

	const int32 Steps = Grid->GetDistance(NextCell);
	const float ToNext = FVector::Dist2D(PathPos, TD::CellToWorld(NextCell)) / TD::TileSize;
	return (Steps >= 0 ? Steps : 999) + ToNext;
}

FVector ATDEnemy::GetAimLocation() const
{
	switch (Type)
	{
	case ETDEnemyType::Kraken:	return GetActorLocation() + FVector(0, 0, 70);
	case ETDEnemyType::Turtle:	return GetActorLocation() + FVector(0, 0, 22);
	default:					return PivotInActor.IsValidIndex(BodyPivot) ? GetRootComponent()->GetComponentTransform().TransformPosition(PivotInActor[BodyPivot].GetTranslation()) : GetActorLocation();
	}
}

float ATDEnemy::GetBarHeight() const
{
	const float Scale = GetActorScale3D().Z;
	switch (Type)
	{
	case ETDEnemyType::Crab:	return 55.0f * Scale;
	case ETDEnemyType::Piranha:	return 60.0f * Scale;
	case ETDEnemyType::Turtle:	return 70.0f * Scale;
	case ETDEnemyType::Manta:	return (FlyHeight + 40.0f) * Scale;
	case ETDEnemyType::Kraken:	return 175.0f * Scale;
	default:					return 60.0f * Scale;
	}
}

void ATDEnemy::GetStatuses(TArray<ETDStatus>& OutStatuses) const
{
	OutStatuses.Reset();
	if (SlowTime > 0.0f || AuraSlow < 1.0f)
	{
		OutStatuses.Add(ETDStatus::Slow);
	}
	if (Dots.Has(ETDDot::Poison))
	{
		OutStatuses.Add(ETDStatus::Poison);
	}
	if (Dots.Has(ETDDot::Bleed))
	{
		OutStatuses.Add(ETDStatus::Bleed);
	}
	if (StunTime > 0.0f)
	{
		OutStatuses.Add(ETDStatus::Stun);
	}
	if (ArmorAura.Up > 0.0f)
	{
		OutStatuses.Add(ETDStatus::ArmorUp);
	}
	if (ArmorAura.Down < 0.0f)
	{
		OutStatuses.Add(ETDStatus::ArmorDown);
	}
}

float ATDEnemy::GetStatusTime(ETDStatus Status) const
{
	switch (Status)
	{
	case ETDStatus::Slow:	return FMath::Max(0.0f, SlowTime);
	case ETDStatus::Stun:	return FMath::Max(0.0f, StunTime);
	case ETDStatus::Poison:	return Dots.LongestRemaining(ETDDot::Poison);
	case ETDStatus::Bleed:	return Dots.LongestRemaining(ETDDot::Bleed);
	default:				return 0.0f;
	}
}

// ---------------------------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------------------------

void ATDEnemy::DeliverHit(ATDEnemy* Target, const FTDHit& Hit)
{
	if (!Target || !Target->IsAlive())
	{
		return;
	}

	UWorld* World = Target->GetWorld();
	const FVector At = Target->GetActorLocation();
	const bool bAir = Target->IsFlying();
	Target->ApplyHit(Hit);

	// Tear of the Moon: a critical hit arcs to the nearest other creature for half the damage
	const ATDGameMode* ArcMode = (Hit.bArcOnCrit && Hit.bCrit && !Hit.bSecondary && World) ? World->GetAuthGameMode<ATDGameMode>() : nullptr;
	if (ArcMode)
	{
		ATDEnemy* Nearest = nullptr;
		float Best = TD::TearArcRange * TD::TileSize;
		for (ATDEnemy* Other : ArcMode->GetEnemies())
		{
			if (Other && Other != Target && Other->IsAlive())
			{
				const float Distance = FVector::Dist2D(Other->GetActorLocation(), At);
				if (Distance <= Best)
				{
					Best = Distance;
					Nearest = Other;
				}
			}
		}
		if (Nearest)
		{
			FTDHit Arc = Hit;
			Arc.Damage *= TD::TearArcShare;
			Arc.bSecondary = true;
			Arc.bArcOnCrit = false;
			ATDFx::SpawnBolt(World, { At + FVector(0, 0, 40), Nearest->GetAimLocation() }, FLinearColor(0.75f, 0.88f, 1.0f));
			Nearest->ApplyHit(Arc);
		}
	}

	if (Hit.bSecondary || (Hit.SplashRadius <= 0.0f && Hit.ChainExtra <= 0))
	{
		return;
	}

	const ATDGameMode* GameMode = World ? World->GetAuthGameMode<ATDGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}

	// gather first: secondary hits may kill and remove enemies from the list
	TArray<ATDEnemy*> SplashTargets;
	TArray<TPair<float, ATDEnemy*>> ChainCandidates;
	for (ATDEnemy* Other : GameMode->GetEnemies())
	{
		if (!Other || Other == Target || !Other->IsAlive())
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Other->GetActorLocation(), At);
		if (Hit.SplashRadius > 0.0f && Other->IsFlying() == bAir && Distance <= Hit.SplashRadius * TD::TileSize)
		{
			SplashTargets.Add(Other);
		}
		else if (Hit.ChainExtra > 0 && Distance <= 1.6f * TD::TileSize)
		{
			ChainCandidates.Add(TPair<float, ATDEnemy*>(Distance, Other));
		}
	}

	FTDHit Secondary = Hit;
	Secondary.bSecondary = true;
	Secondary.XpScale = Hit.XpScale * TD::XpScaleSecondary;
	Secondary.SplashRadius = 0.0f;
	Secondary.ChainExtra = 0;

	if (SplashTargets.Num() > 0)
	{
		FTDHit Splash = Secondary;
		Splash.Damage *= Hit.SplashFraction;
		for (ATDEnemy* Other : SplashTargets)
		{
			Other->ApplyHit(Splash);
		}
	}

	if (ChainCandidates.Num() > 0)
	{
		ChainCandidates.Sort([](const TPair<float, ATDEnemy*>& A, const TPair<float, ATDEnemy*>& B) { return A.Key < B.Key; });
		FTDHit Chain = Secondary;
		Chain.Damage *= Hit.ChainFraction;
		for (int32 i = 0; i < FMath::Min(Hit.ChainExtra, ChainCandidates.Num()); ++i)
		{
			ChainCandidates[i].Value->ApplyHit(Chain);
		}
	}
}

void ATDEnemy::StartDying()
{
	if (bDying)
	{
		return;
	}
	bDying = true;
	bFinished = true;
	DyingTime = 0.0f;
	DeathLocation = VisualLocation;
	DeathRotation = VisualRotation + FRotator(0, 0, Bank);
	DeathBody = PivotLocal[BodyPivot];
	FlinchTime = 0.0f;
	if (ShieldBubble)
	{
		ShieldBubble->DestroyComponent();
		ShieldBubble = nullptr;
	}
	// a little puff of sand where a crawler hits the seabed
	if (!IsFlying())
	{
		ATDFx::SpawnSplash(GetWorld(), FVector(VisualLocation.X, VisualLocation.Y, 6.0f), FLinearColor(0.78f, 0.68f, 0.5f), 0.6f);
	}
	SetActorTickEnabled(true);
}

void ATDEnemy::TickDying(float DeltaTime)
{
	DyingTime += DeltaTime;
	const float Alpha = DyingTime / TD::DeathSeconds;
	const TD::FTDDeathPose Pose = TD::DeathPose(Alpha, IsFlying());
	// the whole creature spirals, hops and sinks; the kraken is tall, so it sinks further to be gone by the end
	const float SinkScale = (Type == ETDEnemyType::Kraken ? 2.6f : 1.0f) * GetActorScale3D().Z;
	VisualLocation = DeathLocation + FVector(0, 0, Pose.Hop - Pose.Sink * SinkScale);
	VisualRotation = DeathRotation + FRotator(0, Pose.Spin, 0);
	// the body rolls over about its own middle (not the ground under it), so it flips in place instead of swinging into the floor;
	// a swimmer (the manta) also falls from the height it swims at
	const float CenterAbovePivot = Type == ETDEnemyType::Turtle ? 16.0f : (Type == ETDEnemyType::Kraken ? 74.0f : 0.0f);
	const FVector Center = DeathBody.GetTranslation() + FVector(0, 0, CenterAbovePivot);
	const FQuat Roll(FVector::XAxisVector, FMath::DegreesToRadians(Pose.Roll));
	const FVector Fall(0, 0, IsFlying() ? -Pose.Drop * BodyBase.Z : 0.0f);
	PivotLocal[BodyPivot] = FTransform(Roll * DeathBody.GetRotation(), Center + Roll.RotateVector(DeathBody.GetTranslation() - Center) + Fall, DeathBody.GetScale3D());
	const float Bulge = 1.0f + (1.0f - Pose.Squash) * 0.6f;
	VisualScale = FVector(Pose.Scale * Bulge, Pose.Scale * Bulge, Pose.Scale * Pose.Squash);

	// limbs: a fading twitch, while they curl up (crawlers) or go slack (swimmers)
	const float Twitch = FMath::Exp(-DyingTime * 5.0f);
	const float L = Pose.Limp;
	for (int32 i = 0; i < LimbPivots.Num(); ++i)
	{
		FTransform& Limb = PivotLocal[LimbPivots[i]];
		const float Jerk = FMath::Sin(DyingTime * 34.0f + i * 1.9f) * Twitch;
		const float Side = Limb.GetTranslation().Y >= 0.0f ? 1.0f : -1.0f;
		switch (Type)
		{
		case ETDEnemyType::Crab:
		case ETDEnemyType::Turtle:
			// legs fold in over the belly, kicking feebly
			Limb.SetRotation(FRotator(Jerk * 16.0f, 0, Side * L * 40.0f).Quaternion());
			break;
		case ETDEnemyType::Piranha:
			// a last flutter of the tail
			Limb.SetRotation(FRotator(0, FMath::Sin(DyingTime * 30.0f) * 26.0f * Twitch, 0).Quaternion());
			break;
		case ETDEnemyType::Manta:
			// wings droop and flutter
			Limb.SetRotation(FRotator(0, 0, (i == 0 ? 1.0f : -1.0f) * (-20.0f * L + Jerk * 14.0f)).Quaternion());
			break;
		case ETDEnemyType::Kraken:
			// arms curl in under the mantle
			Limb.SetRotation(FRotator(-30.0f * L + Jerk * 8.0f, LimbYaws[i], 0).Quaternion());
			break;
		case ETDEnemyType::Cuttlefish:
			// arms splay and go still
			Limb.SetRotation(FRotator(-12.0f * L, (i - LimbPivots.Num() * 0.5f) * 4.0f * L + Jerk * 10.0f, 0).Quaternion());
			break;
		case ETDEnemyType::ShockEel:
			// a spasm runs down the body and dies away, leaving it slack
			Limb.SetTranslation(FVector(16 - i * 8.5f, FMath::Sin(DyingTime * 22.0f - i * 0.9f) * (2.0f + i * 1.2f) * Twitch, 0.0f));
			break;
		default:
			break;
		}
	}
	UpdateVisuals();
	if (Alpha >= 1.0f)
	{
		Destroy();
	}
}

void ATDEnemy::ApplyHit(const FTDHit& Hit)
{
	if (!IsAlive())
	{
		return;
	}
	// hits never recolor the creature (no hit or crit flash, by request)
	TDAudio::Play(this, ETDSound::Hit, 0.5f);

	float Damage = TD::ComputeHitDamage(Hit.Damage, Hit.DamageType, GetDef().ArmorType, GetArmor(), Hit.ArmorPierce, Hit.bIgnoreArmor, Hit.ArmoredBonus);
	Damage *= TD::ConditionalDamageMultiplier(Hit, GetDef().bBoss || IsElite(), IsFlying(), MaxHealth > 0.0f ? Health / MaxHealth : 1.0f);

	// Ripper sawfish: checked before this hit's own bleed lands, so the first tooth sets it up
	if (Hit.BleedingBonus > 0.0f && Dots.Has(ETDDot::Bleed))
	{
		Damage *= 1.0f + Hit.BleedingBonus;
	}

	ATDTower* Source = Hit.Source.Get();
	if (Hit.PoisonPerTick > 0.0f && Hit.PoisonDuration > 0.0f)
	{
		ApplyPoison(Hit.PoisonPerTick, Hit.PoisonDuration, Source);
	}
	if (Hit.BleedPercent > 0.0f && Hit.BleedDuration > 0.0f)
	{
		ApplyBleed(Hit.BleedPercent, Hit.BleedDuration, Source);
	}
	if (Hit.SlowDuration > 0.0f && Hit.SlowFactor < 1.0f)
	{
		ApplySlow(Hit.SlowFactor, Hit.SlowDuration);
	}
	if (Hit.StunChance > 0.0f && FMath::FRand() < Hit.StunChance)
	{
		ApplyStun(Hit.StunDuration);
	}
	// Abyssal Anchor: holds a crawler in place, each creature at most once every few seconds
	if (Hit.bAnchor && !IsFlying())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (TD::AnchorReady(Now, AnchorReadyAt))
		{
			ApplyStun(TD::AnchorHold);
			AnchorReadyAt = Now + TD::AnchorCooldown;
		}
	}

	DealDamage(Damage, Source, Hit.XpScale);
}

void ATDEnemy::DealDamage(float Amount, ATDTower* Source, float XpScale)
{
	if (!IsAlive() || Amount <= 0.0f)
	{
		return;
	}

	// a shield soaks damage first and pops when empty
	if (Shield > 0.0f)
	{
		const float Absorbed = FMath::Min(Shield, Amount);
		Shield -= Absorbed;
		Amount -= Absorbed;
		if (Source)
		{
			Source->OnDamageDealt(Absorbed, XpScale);
		}
		if (Shield <= 0.0f && ShieldBubble)
		{
			ShieldBubble->DestroyComponent();
			ShieldBubble = nullptr;
			ATDFx::SpawnSplash(GetWorld(), GetAimLocation(), FLinearColor(0.5f, 0.9f, 1.0f), 0.8f);
			TDAudio::Play(this, ETDSound::ShieldPop);
		}
		if (Amount <= 0.0f)
		{
			return;
		}
	}

	const float OldHealth = Health;
	const float Dealt = FMath::Min(Amount, Health);
	Health -= Amount;
	// a big hit (a good share of max health) makes it flinch
	if (Health > 0.0f && MaxHealth > 0.0f && Dealt >= MaxHealth * TD::FlinchHealthShare)
	{
		FlinchTime = TD::FlinchSeconds;
	}
	if (Source)
	{
		Source->OnDamageDealt(Dealt, XpScale);
	}

	ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();

	// sea star: spawn a smaller copy every time health crosses a split threshold (original rule)
	if (GameMode && TD::CrossesSplitThreshold(OldHealth, Health, SplitHealth, SplitsLeft))
	{
		GameMode->SpawnSplit(this);
		TDAudio::Play(this, ETDSound::Split, 0.7f);
	}

	if (Health <= 0.0f)
	{
		Health = 0.0f;
		bFinished = true;
		SpreadBlackTide();
		if (GameMode)
		{
			GameMode->OnEnemyKilled(this, Source);
		}
		else
		{
			Destroy();
		}
	}
}

void ATDEnemy::SpreadBlackTide()
{
	const ATDGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ATDGameMode>() : nullptr;
	if (!GameMode)
	{
		return;
	}
	for (const FTDDotSet::FEntry& Entry : Dots.Entries)
	{
		const TWeakObjectPtr<ATDTower>* SourcePtr = DotSources.Find(Entry.SourceId);
		ATDTower* Source = SourcePtr ? SourcePtr->Get() : nullptr;
		if (!Source || !Source->HasUnique(ETDUnique::BlackTide) || Entry.Remaining <= 0.0f)
		{
			continue;
		}
		ATDEnemy* Nearest = nullptr;
		float Best = TD::BlackTideRange * TD::TileSize;
		for (ATDEnemy* Other : GameMode->GetEnemies())
		{
			if (Other && Other != this && Other->IsAlive())
			{
				const float Distance = FVector::Dist2D(Other->GetActorLocation(), GetActorLocation());
				if (Distance <= Best)
				{
					Best = Distance;
					Nearest = Other;
				}
			}
		}
		if (Nearest)
		{
			Nearest->AddDot(Entry.Kind, Entry.Amount, Entry.Remaining, Source);
		}
	}
}

void ATDEnemy::ApplyPoison(float DamagePerTick, float Duration, ATDTower* Source)
{
	AddDot(ETDDot::Poison, DamagePerTick, Duration, Source);
}

void ATDEnemy::ApplyBleed(float Percent, float Duration, ATDTower* Source)
{
	AddDot(ETDDot::Bleed, Percent, Duration, Source);
}

void ATDEnemy::AddDot(ETDDot Kind, float Amount, float Duration, ATDTower* Source)
{
	if (!IsAlive())
	{
		return;
	}
	if (Kind == ETDDot::Bleed)
	{
		// a shield can't bleed: it has to be broken by hits first. Bosses and elites bleed less, the Abyssal Kraken least.
		if (Shield > 0.0f)
		{
			return;
		}
		Amount *= IsTitan() ? TitanBleedScale : (GetDef().bBoss || IsElite()) ? BossBleedScale : 1.0f;
	}
	const uint64 SourceId = Source ? (uint64)Source->GetUniqueID() : 0;
	DotSources.Add(SourceId, Source);
	Dots.Apply(Kind, Amount, Duration, SourceId);
}

void ATDEnemy::TickEffects(float DeltaTime)
{
	if (Dots.Entries.Num() == 0)
	{
		return;
	}
	Dots.Tick(DeltaTime, [this]() { return IsAlive() ? Health : 0.0f; }, [this](uint64 SourceId, float Damage)
	{
		const TWeakObjectPtr<ATDTower>* Source = DotSources.Find(SourceId);
		DealDamage(Damage, Source ? Source->Get() : nullptr, TD::XpScaleDot);
	});
}

void ATDEnemy::ApplyStun(float Duration)
{
	// bosses shrug off half of it, the Abyssal Kraken all of it
	if (IsTitan())
	{
		return;
	}
	StunTime = FMath::Max(StunTime, GetDef().bBoss ? Duration * 0.5f : Duration);
}

void ATDEnemy::ApplySlow(float Factor, float Duration)
{
	if (GetDef().bBoss)
	{
		Factor = 1.0f - (1.0f - Factor) * 0.5f;
	}

	// keep the strongest active slow, refresh the duration
	if (SlowTime <= 0.0f || Factor <= SlowFactor)
	{
		SlowFactor = Factor;
	}
	SlowTime = FMath::Max(SlowTime, Duration);
}

// ---------------------------------------------------------------------------------------------
// Status display
// ---------------------------------------------------------------------------------------------

FLinearColor TD::StatusColor(ETDStatus Status)
{
	switch (Status)
	{
	case ETDStatus::Slow:		return FLinearColor(0.45f, 0.85f, 1.0f);
	case ETDStatus::Poison:		return FLinearColor(0.35f, 0.95f, 0.3f);
	case ETDStatus::Bleed:		return FLinearColor(1.0f, 0.25f, 0.25f);
	case ETDStatus::Stun:		return FLinearColor(1.0f, 0.95f, 0.3f);
	case ETDStatus::ArmorUp:	return FLinearColor(0.35f, 0.5f, 1.0f);
	case ETDStatus::ArmorDown:	return FLinearColor(1.0f, 0.6f, 0.2f);
	default:					return FLinearColor::White;
	}
}

const TCHAR* TD::StatusName(ETDStatus Status)
{
	switch (Status)
	{
	case ETDStatus::Slow:		return TEXT("Slowed");
	case ETDStatus::Poison:		return TEXT("Poisoned");
	case ETDStatus::Bleed:		return TEXT("Bleeding");
	case ETDStatus::Stun:		return TEXT("Stunned");
	case ETDStatus::ArmorUp:	return TEXT("Armor raised");
	case ETDStatus::ArmorDown:	return TEXT("Armor lowered");
	default:					return TEXT("");
	}
}

const TCHAR* TD::StatusDescription(ETDStatus Status)
{
	switch (Status)
	{
	case ETDStatus::Slow:		return TEXT("Moves slower (jellyfish, auras, items).");
	case ETDStatus::Poison:		return TEXT("Takes flat damage every half second. One poison per tower.");
	case ETDStatus::Bleed:		return TEXT("Loses a share of its current health every half second. Shields block it; bosses resist it.");
	case ETDStatus::Stun:		return TEXT("Cannot move.");
	case ETDStatus::ArmorUp:	return TEXT("A nearby Cuttlefish raises its armor.");
	case ETDStatus::ArmorDown:	return TEXT("A nearby Acid Vent lowers its armor.");
	default:					return TEXT("");
	}
}
