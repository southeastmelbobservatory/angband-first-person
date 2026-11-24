#include "ABFPVisualCatalog.h"

namespace
{
    bool MatchesWordKeyword(const FString& LowerName, const FString& LowerKeyword)
    {
        if (LowerKeyword.IsEmpty()) return false;
        const int32 NameLen = LowerName.Len();
        const int32 KeyLen = LowerKeyword.Len();
        int32 SearchStart = 0;

        while (SearchStart <= NameLen - KeyLen)
        {
            const int32 Pos = LowerName.Find(LowerKeyword, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchStart);
            if (Pos == INDEX_NONE) return false;

            const bool bLeftBoundary = (Pos == 0) || !FChar::IsAlpha(LowerName[Pos - 1]);
            const int32 After = Pos + KeyLen;
            bool bRightBoundary = (After >= NameLen) || !FChar::IsAlpha(LowerName[After]);
            if (!bRightBoundary && LowerName[After] == TEXT('s'))
            {
                bRightBoundary = (After + 1 >= NameLen) || !FChar::IsAlpha(LowerName[After + 1]);
            }

            if (bLeftBoundary && bRightBoundary)
            {
                return true;
            }
            SearchStart = Pos + 1;
        }
        return false;
    }
}

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
            MatchesWordKeyword(LowerName, Entry.NameContains.ToLower()))
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
