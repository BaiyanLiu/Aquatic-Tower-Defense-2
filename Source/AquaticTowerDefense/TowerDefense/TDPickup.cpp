// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDPickup.h"
#include "TDModels.h"
#include "TDItems.h"
#include "TDGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"

namespace
{
	constexpr float FloatTime = 1.0f;	// seconds before auto-collect, so the player sees the drop
	constexpr float PickupLife = 12.0f;
}

ATDPickup::ATDPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ATDPickup* ATDPickup::Spawn(UWorld* World, const FVector& Location, const FTDItem& InItem)
{
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Ground(Location.X, Location.Y, 0.0f);
	ATDPickup* Pickup = World->SpawnActor<ATDPickup>(ATDPickup::StaticClass(), Ground, FRotator::ZeroRotator, Params);
	if (!Pickup)
	{
		return nullptr;
	}

	Pickup->Item = InItem;
	Pickup->Base = Ground;
	Pickup->SetLifeSpan(PickupLife);

	const FLinearColor Color = TD::RarityColor(InItem.Rarity);
	Pickup->Orb = TD::AddPivot(Pickup, Pickup->GetRootComponent(), FVector(0, 0, 30));
	// a shell, pearl or charm floating in a bubble tinted by rarity, over a glowing ring
	const ETDModel Model = InItem.Slot == ETDItemSlot::Pearl ? ETDModel::DropPearl : (InItem.Slot == ETDItemSlot::Charm ? ETDModel::DropCharm : ETDModel::DropShell);
	FTDModelOptions Options;
	Options.bShadows = false;
	Options.Recolor = [Color](const FLinearColor& PartColor, uint8 Flags)
	{
		return (Flags & TDPartFlag::Tint) ? FMath::Lerp(PartColor, Color, 0.6f) : PartColor;
	};
	TD::BuildModel(Pickup, Pickup->Orb, Model, Options);
	UStaticMeshComponent* Glow = TD::AddShape(Pickup, Pickup->GetRootComponent(), ETDShape::Cylinder, FVector(0, 0, 1.5f), FVector(52, 52, 1), Color,
		FRotator::ZeroRotator, TD::SharedFxMaterial(Color * 0.8f, 0.35f));
	Glow->SetCastShadow(false);

	Pickup->Light = NewObject<UPointLightComponent>(Pickup);
	Pickup->Light->SetupAttachment(Pickup->Orb);
	Pickup->Light->SetIntensity(3000.0f + 2000.0f * (int32)InItem.Rarity);
	Pickup->Light->SetAttenuationRadius(200.0f);
	Pickup->Light->SetLightColor(Color);
	Pickup->Light->SetCastShadows(false);
	Pickup->Light->RegisterComponent();
	Pickup->AddInstanceComponent(Pickup->Light);

	return Pickup;
}

void ATDPickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Age += DeltaTime;

	// rise and bob, spin the rim
	const float Rise = FMath::Min(Age / FloatTime, 1.0f) * 30.0f;
	Orb->SetRelativeLocation(FVector(0, 0, 30 + Rise + FMath::Sin(Age * 4.0f) * 6.0f));
	Orb->SetRelativeRotation(FRotator(0, Age * 120.0f, 0));

	if (Age >= FloatTime)
	{
		TryCollect(false);
	}
}

bool ATDPickup::TryCollect(bool bFromClick)
{
	ATDGameMode* GameMode = GetWorld()->GetAuthGameMode<ATDGameMode>();
	if (!GameMode || IsActorBeingDestroyed())
	{
		return false;
	}

	if (!GameMode->AddToStorage(Item))
	{
		if (bFromClick)
		{
			GameMode->Toast(TEXT("Storage full"), FLinearColor(1.0f, 0.4f, 0.35f), ETDToast::Refusal);
		}
		return false;
	}

	GameMode->Toast(FString::Printf(TEXT("Found %s: %s"), TD::RarityName(Item.Rarity), *Item.Name), TD::RarityColor(Item.Rarity), Item.Rarity >= ETDRarity::Epic ? ETDToast::Important : ETDToast::Info);
	Destroy();
	return true;
}
