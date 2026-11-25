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
            else if (!bRightBoundary && LowerName[After] == TEXT('e') &&
                     After + 1 < NameLen && LowerName[After + 1] == TEXT('s'))
            {
                bRightBoundary = (After + 2 >= NameLen) || !FChar::IsAlpha(LowerName[After + 2]);
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
    /* 1. Exact Angband object kind index (kidx) */
    for (const FABFPItemVisualEntry& Entry : Items)
    {
        if (Entry.KindId >= 0 && Entry.KindId == Entity.KindId && Entry.HasVisualOverride())
        {
            return &Entry;
        }
    }

    /* 2. Exact Angband category (tval) + variant (sval) */
    if (Entity.CategoryId > 0)
    {
        for (const FABFPItemVisualEntry& Entry : Items)
        {
            if (Entry.KindId < 0 && Entry.CategoryId >= 0 && Entry.CategoryId == Entity.CategoryId &&
                Entry.VariantId >= 0 && Entry.VariantId == Entity.VariantId && Entry.HasVisualOverride())
            {
                return &Entry;
            }
        }
    }

    const FString LowerName = Entity.Name.ToLower();

    /* 3. Category-scoped keyword overrides (e.g. Torch/Phial in TV_LIGHT, Dagger in TV_SWORD,
          Axe in TV_POLEARM, Quarterstaff in TV_HAFTED) before blanket tval fallback. */
    if (Entity.CategoryId > 0)
    {
        for (const FABFPItemVisualEntry& Entry : Items)
        {
            if (Entry.KindId < 0 && Entry.CategoryId >= 0 && Entry.CategoryId == Entity.CategoryId &&
                Entry.VariantId < 0 && !Entry.NameContains.IsEmpty() && Entry.HasVisualOverride() &&
                MatchesWordKeyword(LowerName, Entry.NameContains.ToLower()))
            {
                return &Entry;
            }
        }

        /* 4. Blanket Angband tval category match — covers all 35 tvals without cross-category collisions. */
        for (const FABFPItemVisualEntry& Entry : Items)
        {
            if (Entry.KindId < 0 && Entry.CategoryId >= 0 && Entry.CategoryId == Entity.CategoryId &&
                Entry.VariantId < 0 && Entry.NameContains.IsEmpty() && Entry.HasVisualOverride())
            {
                return &Entry;
            }
        }
    }

    /* 5. Unscoped word-boundary keyword match (Demo mode or items with CategoryId == 0). */
    for (const FABFPItemVisualEntry& Entry : Items)
    {
        if (Entry.KindId < 0 && Entry.CategoryId < 0 && !Entry.NameContains.IsEmpty() &&
            Entry.HasVisualOverride() && MatchesWordKeyword(LowerName, Entry.NameContains.ToLower()))
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
