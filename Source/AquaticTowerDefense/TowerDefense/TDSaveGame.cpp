// Aquatic Tower Defense - isometric underwater tower defense with mazing.

#include "TDSaveGame.h"

FTDItemSave FTDItemSave::FromItem(const FTDItem& Item)
{
	FTDItemSave Save;
	Save.Id = Item.Id;
	Save.Name = Item.Name;
	Save.Kind = Item.Kind;
	Save.Rarity = (uint8)Item.Rarity;
	Save.Slot = (uint8)Item.Slot;
	Save.Level = Item.Level;
	Save.RangeRevision = 1;
	Save.bLocked = Item.bLocked;
	for (const FTDAffix& Affix : Item.Affixes)
	{
		FTDAffixSave& A = Save.Affixes.AddDefaulted_GetRef();
		A.Type = (uint8)Affix.Type;
		A.Value = Affix.Value;
	}
	return Save;
}

FTDItem FTDItemSave::ToItem() const
{
	FTDItem Item;
	Item.Id = Id;
	Item.Name = Name;
	Item.Rarity = (ETDRarity)FMath::Clamp((int32)Rarity, 0, (int32)ETDRarity::Legendary);
	Item.Slot = (ETDItemSlot)FMath::Clamp((int32)Slot, 0, (int32)ETDItemSlot::Count - 1);
	Item.Level = Level;
	Item.bLocked = bLocked;

	// typed items are rebuilt from their type, so balance changes reach saved items too
	if (TD::ItemKind(Kind))
	{
		Item.Kind = Kind;
		TD::ApplyKind(Item);
		return Item;
	}

	for (const FTDAffixSave& A : Affixes)
	{
		if (A.Type < (uint8)ETDAffix::Count)
		{
			const bool bOldSpeed = A.Type == (uint8)ETDAffix::RetiredProjectileSpeed;
			const ETDAffix Type = bOldSpeed ? ETDAffix::RatePct : (ETDAffix)A.Type;
			float Value = bOldSpeed ? A.Value * TD::RetiredSpeedToRate : A.Value;
			if (RangeRevision < 1 && (Type == ETDAffix::Range || Type == ETDAffix::RangePct))
			{
				Value *= 0.5f;
			}

			// a converted affix joins any attack speed the item already has, so it shows as one line
			if (FTDAffix* Same = Item.Affixes.FindByPredicate([Type](const FTDAffix& Affix) { return Affix.Type == Type; }))
			{
				Same->Value += Value;
			}
			else
			{
				FTDAffix& Affix = Item.Affixes.AddDefaulted_GetRef();
				Affix.Type = Type;
				Affix.Value = Value;
			}
		}
	}
	return Item;
}
