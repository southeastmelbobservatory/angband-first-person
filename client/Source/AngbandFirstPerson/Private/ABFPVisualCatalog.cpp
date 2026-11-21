#include "ABFPVisualCatalog.h"

const FABFPCreatureVisualEntry* UABFPVisualCatalog::FindCreatureVisual(
    const FABFPEntity& Entity) const
{
    for (const FABFPCreatureVisualEntry& Entry : Creatures)
    {
        if (Entry.KindId >= 0 && Entry.KindId == Entity.KindId && Entry.HasVisualOverride())
        {
            return &Entry;
        }
    }

    const FString LowerName = Entity.Name.ToLower();
    for (const FABFPCreatureVisualEntry& Entry : Creatures)
    {
        if (Entry.KindId < 0 && !Entry.NameContains.IsEmpty() && Entry.HasVisualOverride() &&
            LowerName.Contains(Entry.NameContains.ToLower()))
        {
            return &Entry;
        }
    }

    return DefaultCreature.HasVisualOverride() ? &DefaultCreature : nullptr;
}

const FABFPItemVisualEntry* UABFPVisualCatalog::FindItemVisual(
    const FABFPEntity& Entity) const
{
    for (const FABFPItemVisualEntry& Entry : Items)
    {
        if (Entry.KindId >= 0 && Entry.KindId == Entity.KindId && Entry.HasVisualOverride())
        {
            return &Entry;
        }
    }

    for (const FABFPItemVisualEntry& Entry : Items)
    {
        if (Entry.KindId < 0 && Entry.CategoryId >= 0 && Entry.CategoryId == Entity.CategoryId &&
            Entry.VariantId >= 0 && Entry.VariantId == Entity.VariantId && Entry.HasVisualOverride())
        {
            return &Entry;
        }
    }

    for (const FABFPItemVisualEntry& Entry : Items)
    {
        if (Entry.KindId < 0 && Entry.CategoryId >= 0 && Entry.CategoryId == Entity.CategoryId &&
            Entry.VariantId < 0 && Entry.HasVisualOverride())
        {
            return &Entry;
        }
    }

    const FString LowerName = Entity.Name.ToLower();
    for (const FABFPItemVisualEntry& Entry : Items)
    {
        if (Entry.KindId < 0 && Entry.CategoryId < 0 && !Entry.NameContains.IsEmpty() &&
            Entry.HasVisualOverride() && LowerName.Contains(Entry.NameContains.ToLower()))
        {
            return &Entry;
        }
    }

    return DefaultItem.HasVisualOverride() ? &DefaultItem : nullptr;
}

const FABFPTerrainVisualSet& UABFPVisualCatalog::GetTerrainSetForDepth(int32 DungeonDepth) const
{
    if (DungeonDepth == 0)
    {
        const bool bHasTownOverrides =
            TownTerrain.FloorVariants.Num() > 0 ||
            TownTerrain.WallVariants.Num() > 0 ||
            TownTerrain.ShopFacade.IsValid();
        if (bHasTownOverrides)
        {
            return TownTerrain;
        }
    }
    return DungeonTerrain;
}
