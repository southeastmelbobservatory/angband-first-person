#include "ABFPHUD.h"

#include "ABFPFirstPersonPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"

namespace
{
    FString FormatAngbandStat(int32 Value)
    {
        if (Value <= 18)
        {
            return FString::Printf(TEXT("%d"), Value);
        }
        const int32 Bonus = Value - 18;
        if (Bonus >= 200)
        {
            return TEXT("18/***");
        }
        return FString::Printf(TEXT("18/%02d"), Bonus);
    }

    FString FormatSigned(int32 Value)
    {
        return Value > 0 ? FString::Printf(TEXT("+%d"), Value) : FString::Printf(TEXT("%d"), Value);
    }

    FString FormatStatModSummary(const TArray<int32>& Adj)
    {
        static const TCHAR* Names[5] = { TEXT("STR"), TEXT("INT"), TEXT("WIS"), TEXT("DEX"), TEXT("CON") };
        FString Out;
        for (int32 s = 0; s < 5 && s < Adj.Num(); ++s)
        {
            if (!Out.IsEmpty()) Out += TEXT("  ");
            Out += FString::Printf(TEXT("%s %s"), Names[s], *FormatSigned(Adj[s]));
        }
        return Out;
    }
}

void AABFPHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !GEngine) return;

    const AABFPFirstPersonPawn* Pawn = Cast<AABFPFirstPersonPawn>(GetOwningPawn());
    if (!Pawn) return;

    const FABFPSnapshot& Snapshot = Pawn->GetCurrentSnapshot();
    if (Pawn->GetActiveMenu() == EABFPMenuMode::CharacterCreation ||
        (Snapshot.Birth.bActive && Pawn->GetActiveMenu() != EABFPMenuMode::SaveLoad))
    {
        DrawCharacterCreationMenu(Pawn, Snapshot);
        return;
    }

    if (!Snapshot.Birth.bActive)
    {
        DrawStatusOverlay(Pawn, Snapshot);
        DrawVitalsPanel(Pawn, Snapshot);
        DrawMinimap(Pawn, Snapshot);
    }
    DrawMessageLog(Pawn);
    if (Pawn->GetActiveMenu() == EABFPMenuMode::None)
    {
        DrawTargetAndPrompt(Pawn, Snapshot);
        DrawControlsHelp(Pawn);
    }
    else
    {
        DrawActiveMenu(Pawn, Snapshot);
    }
}

void AABFPHUD::DrawStatusOverlay(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot)
{
    const float FlashAlpha = Pawn->GetDamageFlashAlpha();
    if (FlashAlpha > 0.01f)
    {
        const FLinearColor FlashColor(0.85f, 0.06f, 0.04f, FlashAlpha * 0.28f);
        DrawRect(FlashColor, 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
    }

    if (Snapshot.bPlayerDead)
    {
        DrawRect(FLinearColor(0.05f, 0.01f, 0.01f, 0.68f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
        UFont* LargeFont = GEngine->GetLargeFont();
        UFont* MedFont = GEngine->GetMediumFont();
        const FString Banner = TEXT("YOU HAVE DIED");
        float TextW = 0.0f, TextH = 0.0f;
        GetTextSize(Banner, TextW, TextH, LargeFont, 1.8f);
        const float BannerY = Canvas->ClipY * 0.40f;
        DrawText(Banner, FLinearColor(0.95f, 0.18f, 0.14f, 1.0f),
            (Canvas->ClipX - TextW) * 0.5f, BannerY, LargeFont, 1.8f);

        const FString Sub = Snapshot.Saves.Num() > 0
            ? TEXT("Press [N] / [Enter] for New Hero   |   [F9 / O] Load Saved Character")
            : TEXT("Press [N] or [Enter] to Create a New Character");
        float SubW = 0.0f, SubH = 0.0f;
        GetTextSize(Sub, SubW, SubH, MedFont, 1.0f);
        DrawText(Sub, FLinearColor(0.96f, 0.85f, 0.48f, 1.0f),
            (Canvas->ClipX - SubW) * 0.5f, BannerY + TextH + 20.0f, MedFont, 1.0f);
    }
}

void AABFPHUD::DrawVitalsPanel(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot)
{
    UFont* SmallFont = GEngine->GetSmallFont();
    UFont* MedFont = GEngine->GetMediumFont();

    const bool bHasIdentity = !Snapshot.PlayerName.IsEmpty() || !Snapshot.PlayerClassName.IsEmpty();
    const float PanelW = 336.0f;
    float PanelH = 128.0f;
    if (Snapshot.PlayerMaxMana > 0) PanelH += 22.0f;
    if (bHasIdentity) PanelH += 17.0f;

    const float X = 20.0f;
    const float Y = Canvas->ClipY - PanelH - 20.0f;

    if (Pawn->IsRunModeEnabled() || Pawn->IsRunning())
    {
        const float PillH = 22.0f;
        const float PillY = Y - PillH - 6.0f;
        const bool bRunningNow = Pawn->IsRunning();
        const FLinearColor PillBg = bRunningNow
            ? FLinearColor(0.06f, 0.16f, 0.10f, 0.90f)
            : FLinearColor(0.12f, 0.09f, 0.04f, 0.88f);
        const FLinearColor PillAccent = bRunningNow
            ? FLinearColor(0.32f, 0.94f, 0.52f, 0.95f)
            : FLinearColor(0.95f, 0.74f, 0.24f, 0.92f);
        const FString PillText = bRunningNow
            ? TEXT(">> RUNNING  ([Any Key] stop | [Shift/.] off)")
            : TEXT("RUN MODE: ON  ([Up/Dn] run | [Shift/.] off)");

        DrawRect(PillBg, X, PillY, PanelW, PillH);
        DrawRect(PillAccent, X, PillY, PanelW, 2.0f);
        DrawText(PillText, PillAccent, X + 10.0f, PillY + 4.0f, SmallFont, 0.86f);
    }

    DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.84f), X, Y, PanelW, PanelH);
    DrawRect(FLinearColor(0.55f, 0.43f, 0.22f, 0.78f), X, Y, PanelW, 2.0f);

    const FString ModeText = Pawn->IsLiveMode()
        ? (Pawn->HasAuthoritativeState() ? TEXT("ANGBAND LIVE") : TEXT("CONNECTING..."))
        : TEXT("RENDERER DEMO");
    const FLinearColor ModeColor = Pawn->IsLiveMode()
        ? (Pawn->HasAuthoritativeState() ? FLinearColor(0.32f, 0.90f, 0.48f) : FLinearColor(0.95f, 0.78f, 0.25f))
        : FLinearColor(0.92f, 0.74f, 0.34f);

    const FString DepthLabel = Snapshot.DungeonDepth == 0
        ? TEXT("Town")
        : FString::Printf(TEXT("Depth %d (%dft)"), Snapshot.DungeonDepth, Snapshot.DungeonDepth * 50);

    DrawText(ModeText, ModeColor, X + 12.0f, Y + 8.0f, SmallFont, 0.96f);
    DrawText(DepthLabel, FLinearColor(0.92f, 0.87f, 0.74f), X + 180.0f, Y + 8.0f, SmallFont, 0.96f);

    float CurY = Y + 27.0f;
    if (bHasIdentity)
    {
        const FString NamePart = Snapshot.PlayerName.IsEmpty() ? TEXT("Hero") : Snapshot.PlayerName;
        const FString RolePart = (!Snapshot.PlayerRaceName.IsEmpty() && !Snapshot.PlayerClassName.IsEmpty())
            ? FString::Printf(TEXT("%s %s"), *Snapshot.PlayerRaceName, *Snapshot.PlayerClassName)
            : (Snapshot.PlayerClassName.IsEmpty() ? Snapshot.PlayerRaceName : Snapshot.PlayerClassName);
        const FString IdLine = RolePart.IsEmpty()
            ? NamePart
            : FString::Printf(TEXT("%s  -  %s"), *NamePart, *RolePart);
        DrawText(IdLine, FLinearColor(0.86f, 0.82f, 0.72f), X + 12.0f, CurY, SmallFont, 0.86f);
        CurY += 17.0f;
    }

    /* Health bar */
    const float BarX = X + 12.0f;
    const float BarW = PanelW - 24.0f;
    const float BarH = 18.0f;
    const float HpRatio = Snapshot.PlayerMaxHitPoints > 0
        ? FMath::Clamp(static_cast<float>(Snapshot.PlayerHitPoints) / static_cast<float>(Snapshot.PlayerMaxHitPoints), 0.0f, 1.0f)
        : 0.0f;
    const FLinearColor HpColor = HpRatio > 0.55f ? FLinearColor(0.18f, 0.68f, 0.28f)
        : (HpRatio > 0.25f ? FLinearColor(0.86f, 0.56f, 0.12f) : FLinearColor(0.85f, 0.14f, 0.12f));

    DrawRect(FLinearColor(0.12f, 0.12f, 0.14f, 0.92f), BarX, CurY, BarW, BarH);
    DrawRect(HpColor, BarX, CurY, BarW * HpRatio, BarH);
    const FString HpText = Snapshot.PlayerMaxHitPoints > 0
        ? FString::Printf(TEXT("HP  %d / %d"), Snapshot.PlayerHitPoints, Snapshot.PlayerMaxHitPoints)
        : TEXT("HP  --");
    DrawText(HpText, FLinearColor::White, BarX + 8.0f, CurY + 2.0f, SmallFont, 0.95f);

    float StatsY = CurY + 24.0f;
    if (Snapshot.PlayerMaxMana > 0)
    {
        const float MpRatio = FMath::Clamp(
            static_cast<float>(Snapshot.PlayerMana) / static_cast<float>(Snapshot.PlayerMaxMana), 0.0f, 1.0f);
        DrawRect(FLinearColor(0.10f, 0.12f, 0.18f, 0.92f), BarX, StatsY, BarW, 14.0f);
        DrawRect(FLinearColor(0.20f, 0.45f, 0.92f), BarX, StatsY, BarW * MpRatio, 14.0f);
        DrawText(FString::Printf(TEXT("MP  %d / %d"), Snapshot.PlayerMana, Snapshot.PlayerMaxMana),
            FLinearColor(0.88f, 0.94f, 1.0f), BarX + 8.0f, StatsY, SmallFont, 0.85f);
        if (Snapshot.PlayerNewSpells > 0)
        {
            DrawText(FString::Printf(TEXT("+%d Study [S]"), Snapshot.PlayerNewSpells),
                FLinearColor(1.0f, 0.88f, 0.34f), BarX + BarW - 92.0f, StatsY, SmallFont, 0.84f);
        }
        StatsY += 21.0f;
    }

    const FString StatsLine = FString::Printf(TEXT("Lv %d   AC %d   Gold %d"),
        Snapshot.PlayerLevel, Snapshot.PlayerArmorClass, Snapshot.PlayerGold);
    DrawText(StatsLine, FLinearColor(0.95f, 0.86f, 0.60f), BarX, StatsY + 2.0f, MedFont, 0.86f);
    if (Snapshot.PlayerMaxMana == 0 && Snapshot.PlayerNewSpells > 0)
    {
        DrawText(FString::Printf(TEXT("+%d Spells [S]"), Snapshot.PlayerNewSpells),
            FLinearColor(1.0f, 0.88f, 0.34f), BarX + BarW - 98.0f, StatsY + 4.0f, SmallFont, 0.84f);
    }

    const FString SpeedStr = Snapshot.PlayerSpeed == 0
        ? TEXT("Norm")
        : (Snapshot.PlayerSpeed > 0
            ? FString::Printf(TEXT("+%d"), Snapshot.PlayerSpeed)
            : FString::Printf(TEXT("%d"), Snapshot.PlayerSpeed));

    const FString FoodLabel = Snapshot.PlayerFoodPct >= 80 ? TEXT("Fed")
        : (Snapshot.PlayerFoodPct >= 30 ? TEXT("OK")
            : (Snapshot.PlayerFoodPct >= 15 ? TEXT("Hungry") : TEXT("Weak!")));
    const FLinearColor FoodColor = Snapshot.PlayerFoodPct >= 30
        ? FLinearColor(0.76f, 0.80f, 0.78f)
        : (Snapshot.PlayerFoodPct >= 15 ? FLinearColor(0.95f, 0.72f, 0.22f) : FLinearColor(0.95f, 0.25f, 0.20f));

    const FString SubLine1 = FString::Printf(TEXT("EXP %d   Spd %s   Turn %lld"),
        Snapshot.PlayerExp, *SpeedStr, Snapshot.Turn);
    DrawText(SubLine1, FLinearColor(0.76f, 0.80f, 0.84f), BarX, StatsY + 24.0f, SmallFont, 0.86f);
    DrawText(FString::Printf(TEXT("Food: %s (%d%%)"), *FoodLabel, Snapshot.PlayerFoodPct),
        FoodColor, BarX, StatsY + 41.0f, SmallFont, 0.86f);

    if (Snapshot.bHasLauncher)
    {
        int32 AmmoTotal = 0;
        for (const FABFPInventoryItem& Item : Snapshot.Inventory)
        {
            if (!Item.bEquipped && (Item.Flags & ABFPItemFlags::Ammo) != 0)
            {
                AmmoTotal += FMath::Max(1, Item.Quantity);
            }
        }
        const FString AmmoText = AmmoTotal > 0
            ? FString::Printf(TEXT("Ammo: %d [Z]"), AmmoTotal)
            : TEXT("No Ammo [Z]");
        const FLinearColor AmmoColor = AmmoTotal > 0
            ? FLinearColor(0.62f, 0.84f, 0.96f)
            : FLinearColor(0.92f, 0.62f, 0.28f);
        DrawText(AmmoText, AmmoColor, BarX + BarW - 102.0f, StatsY + 41.0f, SmallFont, 0.85f);
    }
}

void AABFPHUD::DrawMessageLog(const AABFPFirstPersonPawn* Pawn)
{
    const TArray<FABFPMessage>& History = Pawn->GetMessageHistory();
    if (History.Num() == 0) return;

    UFont* SmallFont = GEngine->GetSmallFont();
    const int32 VisibleLines = FMath::Min(History.Num(), 7);
    const float LineHeight = 18.0f;
    const float PanelW = FMath::Min(620.0f, Canvas->ClipX * 0.48f);
    const float PanelH = VisibleLines * LineHeight + 18.0f;
    const float X = Canvas->ClipX - PanelW - 20.0f;
    const float Y = Canvas->ClipY - PanelH - 20.0f;

    DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.76f), X, Y, PanelW, PanelH);
    DrawRect(FLinearColor(0.55f, 0.43f, 0.22f, 0.60f), X, Y, PanelW, 2.0f);

    const int32 StartIdx = History.Num() - VisibleLines;
    const int32 MaxChars = FMath::Max(24, static_cast<int32>((PanelW - 20.0f) / 7.5f));
    for (int32 Row = 0; Row < VisibleLines; ++Row)
    {
        const FABFPMessage& Msg = History[StartIdx + Row];
        const float AgeAlpha = FMath::Lerp(0.52f, 1.0f, static_cast<float>(Row + 1) / static_cast<float>(VisibleLines));
        const FString Lower = Msg.Text.ToLower();
        FLinearColor TextColor(0.88f, 0.86f, 0.80f, AgeAlpha);
        if (Lower.Contains(TEXT("hit")) || Lower.Contains(TEXT("damage")) || Lower.Contains(TEXT("die")) || Lower.Contains(TEXT("killed")))
        {
            TextColor = FLinearColor(1.0f, 0.52f, 0.36f, AgeAlpha);
        }
        else if (Lower.Contains(TEXT("pick")) || Lower.Contains(TEXT("gold")) || Lower.Contains(TEXT("bought")) || Lower.Contains(TEXT("sold")))
        {
            TextColor = FLinearColor(0.95f, 0.82f, 0.32f, AgeAlpha);
        }
        else if (Lower.Contains(TEXT("cast")) || Lower.Contains(TEXT("spell")) || Lower.Contains(TEXT("learned")) || Lower.Contains(TEXT("bolt")))
        {
            TextColor = FLinearColor(0.56f, 0.82f, 1.0f, AgeAlpha);
        }
        FString DisplayText = Msg.Text;
        if (DisplayText.Len() > MaxChars)
        {
            DisplayText = DisplayText.Left(MaxChars - 3) + TEXT("...");
        }
        DrawText(DisplayText, TextColor, X + 10.0f, Y + 9.0f + Row * LineHeight, SmallFont, 0.92f);
    }
}

void AABFPHUD::DrawMinimap(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot)
{
    if (Snapshot.Cells.Num() == 0) return;

    UFont* SmallFont = GEngine->GetSmallFont();
    const bool bExpanded = Pawn->IsMinimapExpanded();
    const int32 Radius = bExpanded ? 18 : 10;
    const int32 GridSide = Radius * 2 + 1;
    const float TilePx = bExpanded ? 9.0f : 8.0f;
    const float MapSize = GridSide * TilePx;
    const float Pad = 10.0f;
    const float HeaderH = 18.0f;
    const float OriginX = Canvas->ClipX - MapSize - Pad * 2.0f - 20.0f;
    const float OriginY = 20.0f;

    DrawRect(FLinearColor(0.03f, 0.035f, 0.05f, 0.84f), OriginX, OriginY, MapSize + Pad * 2.0f, MapSize + Pad * 2.0f + HeaderH);
    DrawRect(FLinearColor(0.55f, 0.43f, 0.22f, 0.70f), OriginX, OriginY, MapSize + Pad * 2.0f, 2.0f);
    DrawText(bExpanded ? TEXT("MAP  ([M] shrink)") : TEXT("MAP  ([M] expand)"),
        FLinearColor(0.90f, 0.82f, 0.56f), OriginX + Pad, OriginY + 5.0f, SmallFont, 0.80f);

    const float BaseX = OriginX + Pad;
    const float BaseY = OriginY + Pad + HeaderH;

    for (const FABFPCell& Cell : Snapshot.Cells)
    {
        if (Cell.Terrain == EABFPTerrain::Unknown) continue;
        const int32 RelX = Cell.X - Snapshot.PlayerX + Radius;
        const int32 RelY = Cell.Y - Snapshot.PlayerY + Radius;
        if (RelX < 0 || RelX >= GridSide || RelY < 0 || RelY >= GridSide) continue;

        FLinearColor Color(0.18f, 0.20f, 0.24f, 0.85f);
        switch (Cell.Terrain)
        {
        case EABFPTerrain::Wall:
            Color = Cell.bVisible ? FLinearColor(0.42f, 0.44f, 0.48f) : FLinearColor(0.20f, 0.22f, 0.26f);
            break;
        case EABFPTerrain::Vein:
            Color = Cell.bVisible ? FLinearColor(0.75f, 0.58f, 0.24f) : FLinearColor(0.32f, 0.26f, 0.14f);
            break;
        case EABFPTerrain::Floor:
            Color = Cell.bVisible ? (Cell.bLit ? FLinearColor(0.18f, 0.19f, 0.20f) : FLinearColor(0.11f, 0.12f, 0.14f))
                : FLinearColor(0.06f, 0.07f, 0.09f);
            break;
        case EABFPTerrain::Door:
            Color = FLinearColor(0.72f, 0.42f, 0.14f);
            break;
        case EABFPTerrain::DoorOpen:
        case EABFPTerrain::DoorBroken:
            Color = FLinearColor(0.46f, 0.32f, 0.16f);
            break;
        case EABFPTerrain::StairsUp:
            Color = FLinearColor(0.85f, 0.85f, 0.95f);
            break;
        case EABFPTerrain::StairsDown:
            Color = FLinearColor(0.30f, 0.68f, 0.95f);
            break;
        case EABFPTerrain::Rubble:
            Color = FLinearColor(0.45f, 0.38f, 0.30f);
            break;
        case EABFPTerrain::Shop:
            Color = FLinearColor(0.95f, 0.64f, 0.18f);
            break;
        case EABFPTerrain::Trap:
            Color = FLinearColor(0.92f, 0.22f, 0.18f);
            break;
        default:
            break;
        }
        DrawRect(Color, BaseX + RelX * TilePx, BaseY + RelY * TilePx, TilePx - 1.0f, TilePx - 1.0f);
    }

    const int64 LockedId = Pawn->GetLockedTargetId();
    for (const FABFPEntity& Entity : Snapshot.Entities)
    {
        const int32 RelX = Entity.X - Snapshot.PlayerX + Radius;
        const int32 RelY = Entity.Y - Snapshot.PlayerY + Radius;
        if (RelX < 0 || RelX >= GridSide || RelY < 0 || RelY >= GridSide) continue;
        FLinearColor DotColor = (Entity.Type == EABFPEntityType::Monster)
            ? FLinearColor(0.95f, 0.20f, 0.16f)
            : FLinearColor(0.98f, 0.84f, 0.22f);
        if (Entity.Type == EABFPEntityType::Monster && Entity.Id == LockedId)
        {
            DotColor = FLinearColor(1.0f, 0.92f, 0.22f);
        }
        DrawRect(DotColor, BaseX + RelX * TilePx + 1.5f, BaseY + RelY * TilePx + 1.5f, TilePx - 3.0f, TilePx - 3.0f);
    }

    /* Player dot + facing vector arrow */
    const float PlayerCenterX = BaseX + Radius * TilePx + TilePx * 0.5f;
    const float PlayerCenterY = BaseY + Radius * TilePx + TilePx * 0.5f;
    const FIntPoint Facing = Pawn->GetFacingGridDelta();
    DrawLine(PlayerCenterX, PlayerCenterY,
        PlayerCenterX + Facing.X * TilePx * 1.6f,
        PlayerCenterY + Facing.Y * TilePx * 1.6f,
        FLinearColor(0.32f, 0.96f, 0.58f), 2.0f);
    DrawRect(FLinearColor(0.32f, 0.96f, 0.58f),
        BaseX + Radius * TilePx + 1.0f, BaseY + Radius * TilePx + 1.0f, TilePx - 2.0f, TilePx - 2.0f);
}

void AABFPHUD::DrawTargetAndPrompt(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot)
{
    UFont* SmallFont = GEngine->GetSmallFont();
    UFont* MedFont = GEngine->GetMediumFont();
    const FIntPoint Facing = Pawn->GetFacingGridDelta();

    bool bManualLock = false;
    const FABFPEntity* TargetMonster = Pawn->GetActiveRangedTarget(&bManualLock);

    if (TargetMonster)
    {
        const int32 Dist = FMath::Max(
            FMath::Abs(TargetMonster->X - Snapshot.PlayerX),
            FMath::Abs(TargetMonster->Y - Snapshot.PlayerY));

        const float PlateW = 348.0f;
        const float PlateH = 64.0f;
        const float PlateX = (Canvas->ClipX - PlateW) * 0.5f;
        const float PlateY = 20.0f;
        DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.86f), PlateX, PlateY, PlateW, PlateH);
        const FLinearColor Accent = bManualLock
            ? FLinearColor(0.98f, 0.78f, 0.22f, 0.92f)
            : FLinearColor(0.75f, 0.22f, 0.16f, 0.80f);
        DrawRect(Accent, PlateX, PlateY, PlateW, 2.5f);

        const FString NameStr = TargetMonster->Name.IsEmpty() ? TEXT("Unknown Creature") : TargetMonster->Name;
        const FString Header = FString::Printf(TEXT("%s  (%d %s)%s"),
            *NameStr, Dist, Dist == 1 ? TEXT("tile") : TEXT("tiles"),
            bManualLock ? TEXT("  [LOCKED]") : TEXT(""));
        DrawText(Header, FLinearColor(0.96f, 0.88f, 0.70f), PlateX + 10.0f, PlateY + 6.0f, MedFont, 0.84f);

        const float Ratio = TargetMonster->MaxHitPoints > 0
            ? FMath::Clamp(static_cast<float>(TargetMonster->HitPoints) / static_cast<float>(TargetMonster->MaxHitPoints), 0.0f, 1.0f)
            : 1.0f;
        DrawRect(FLinearColor(0.14f, 0.10f, 0.10f, 0.92f), PlateX + 10.0f, PlateY + 29.0f, PlateW - 20.0f, 10.0f);
        DrawRect(FLinearColor(0.86f, 0.18f, 0.14f), PlateX + 10.0f, PlateY + 29.0f, (PlateW - 20.0f) * Ratio, 10.0f);

        FString ActionHint;
        if (Snapshot.bHasLauncher)
        {
            ActionHint += TEXT("[Z] Fire   ");
        }
        if (const FABFPSpellEntry* QuickSp = Pawn->GetQuickCastSpell())
        {
            FString SpLabel = QuickSp->Name;
            if (SpLabel.Len() > 16) SpLabel = SpLabel.Left(15) + TEXT(".");
            ActionHint += FString::Printf(TEXT("[V] %s   "), *SpLabel);
        }
        ActionHint += TEXT("[Tab] Cycle Target");
        DrawText(ActionHint, FLinearColor(0.78f, 0.84f, 0.90f), PlateX + 10.0f, PlateY + 44.0f, SmallFont, 0.82f);
    }

    /* Context action hint (active shop, underfoot items/stairs, or door/trap/rubble ahead). */
    FString Prompt;
    if (Snapshot.Shop.bActive)
    {
        const FString ShopName = Snapshot.Shop.Name.IsEmpty() ? TEXT("Shop") : Snapshot.Shop.Name;
        Prompt = FString::Printf(TEXT("[B / F] Enter %s"), *ShopName);
    }

    if (Prompt.IsEmpty())
    {
        for (const FABFPEntity& Entity : Snapshot.Entities)
        {
            if (Entity.Type == EABFPEntityType::Object &&
                Entity.X == Snapshot.PlayerX && Entity.Y == Snapshot.PlayerY)
            {
                Prompt = FString::Printf(TEXT("[E / G] Pick up %s"),
                    Entity.Name.IsEmpty() ? TEXT("item") : *Entity.Name);
                break;
            }
        }
    }

    if (Prompt.IsEmpty())
    {
        const FABFPCell* Underfoot = Snapshot.Cells.FindByPredicate(
            [&Snapshot](const FABFPCell& C) { return C.X == Snapshot.PlayerX && C.Y == Snapshot.PlayerY; });
        if (Underfoot && Underfoot->Terrain == EABFPTerrain::StairsDown)
            Prompt = TEXT("[Page Down] Descend Stairs");
        else if (Underfoot && Underfoot->Terrain == EABFPTerrain::StairsUp)
            Prompt = TEXT("[Page Up] Ascend Stairs");
    }

    if (Prompt.IsEmpty())
    {
        const int32 AheadX = Snapshot.PlayerX + Facing.X;
        const int32 AheadY = Snapshot.PlayerY + Facing.Y;
        const FABFPCell* Ahead = Snapshot.Cells.FindByPredicate(
            [AheadX, AheadY](const FABFPCell& C) { return C.X == AheadX && C.Y == AheadY; });
        if (Ahead)
        {
            if (Ahead->Terrain == EABFPTerrain::Door) Prompt = TEXT("[F] Open Door");
            else if (Ahead->Terrain == EABFPTerrain::DoorOpen) Prompt = TEXT("[F] Close Door");
            else if (Ahead->Terrain == EABFPTerrain::Trap) Prompt = TEXT("[F] Disarm Trap");
            else if (Ahead->Terrain == EABFPTerrain::Rubble) Prompt = TEXT("[F] Clear Rubble");
        }
    }

    if (!Prompt.IsEmpty())
    {
        float TextW = 0.0f, TextH = 0.0f;
        GetTextSize(Prompt, TextW, TextH, SmallFont, 1.05f);
        const float BoxW = TextW + 24.0f;
        const float BoxH = TextH + 12.0f;
        const float BoxX = (Canvas->ClipX - BoxW) * 0.5f;
        const float BoxY = Canvas->ClipY * 0.62f;
        DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.82f), BoxX, BoxY, BoxW, BoxH);
        DrawRect(FLinearColor(0.65f, 0.50f, 0.22f, 0.70f), BoxX, BoxY, BoxW, 2.0f);
        DrawText(Prompt, FLinearColor(0.98f, 0.88f, 0.56f), BoxX + 12.0f, BoxY + 6.0f, SmallFont, 1.05f);
    }
}

void AABFPHUD::DrawSpellbookMenu(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot)
{
    UFont* SmallFont = GEngine->GetSmallFont();
    UFont* MedFont = GEngine->GetMediumFont();

    DrawRect(FLinearColor(0.01f, 0.014f, 0.028f, 0.52f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);

    const float PanelW = FMath::Min(760.0f, Canvas->ClipX - 72.0f);
    const float PanelH = FMath::Min(476.0f, Canvas->ClipY - 196.0f);
    const float X = (Canvas->ClipX - PanelW) * 0.5f;
    const float Y = FMath::Max(24.0f, (Canvas->ClipY - PanelH) * 0.42f);

    DrawRect(FLinearColor(0.042f, 0.05f, 0.08f, 0.95f), X, Y, PanelW, PanelH);
    DrawRect(FLinearColor(0.36f, 0.62f, 0.96f, 0.92f), X, Y, PanelW, 3.0f);
    DrawRect(FLinearColor(0.24f, 0.36f, 0.58f, 0.72f), X, Y + PanelH - 2.0f, PanelW, 2.0f);

    const FABFPSpellEntry* QuickSp = Pawn->GetQuickCastSpell();
    const FString QuickName = QuickSp ? QuickSp->Name : TEXT("None");

    DrawText(TEXT("SPELLBOOK & ARCANE STUDY"), FLinearColor(0.70f, 0.88f, 1.0f), X + 16.0f, Y + 12.0f, MedFont, 1.0f);
    const FString Subtitle = FString::Printf(
        TEXT("MP: %d / %d   |   Open Study Slots: %d   |   Quick-Cast [V]: %s"),
        Snapshot.PlayerMana, Snapshot.PlayerMaxMana, Snapshot.PlayerNewSpells, *QuickName);
    DrawText(Subtitle, FLinearColor(0.80f, 0.85f, 0.92f), X + 16.0f, Y + 36.0f, SmallFont, 0.90f);
    DrawRect(FLinearColor(0.24f, 0.34f, 0.52f, 0.65f), X + 12.0f, Y + 56.0f, PanelW - 24.0f, 1.0f);

    const float ColNameX = X + 18.0f;
    const float ColLvX = X + PanelW - 360.0f;
    const float ColMpX = X + PanelW - 318.0f;
    const float ColFailX = X + PanelW - 270.0f;
    const float ColInfoX = X + PanelW - 214.0f;
    const float ColTagX = X + PanelW - 86.0f;

    const float HeaderY = Y + 61.0f;
    const FLinearColor HeaderCol(0.62f, 0.72f, 0.86f);
    DrawText(TEXT("Spell"), HeaderCol, ColNameX, HeaderY, SmallFont, 0.84f);
    DrawText(TEXT("Lv"), HeaderCol, ColLvX, HeaderY, SmallFont, 0.84f);
    DrawText(TEXT("MP"), HeaderCol, ColMpX, HeaderY, SmallFont, 0.84f);
    DrawText(TEXT("Fail"), HeaderCol, ColFailX, HeaderY, SmallFont, 0.84f);
    DrawText(TEXT("Effect"), HeaderCol, ColInfoX, HeaderY, SmallFont, 0.84f);
    DrawText(TEXT("Status"), HeaderCol, ColTagX, HeaderY, SmallFont, 0.84f);

    const float ListTop = Y + 80.0f;
    const float RowH = 22.0f;
    const float FooterH = 34.0f;
    const int32 MaxVisible = FMath::Max(4, static_cast<int32>((PanelH - 80.0f - FooterH) / RowH));
    const int32 Total = Snapshot.Spells.Num();
    const int32 Selected = Pawn->GetSelectedMenuIndex();

    if (Total == 0)
    {
        DrawText(TEXT("No spells available in your pack."),
            FLinearColor(0.84f, 0.84f, 0.88f), X + 22.0f, ListTop + 14.0f, SmallFont, 0.96f);
        DrawText(TEXT("Carry a spellbook for your class (Mage, Priest, Ranger, Rogue, Paladin, Druid, Necromancer, Blackguard)."),
            FLinearColor(0.62f, 0.66f, 0.74f), X + 22.0f, ListTop + 36.0f, SmallFont, 0.86f);
    }
    else
    {
        const int32 ScrollStart = FMath::Clamp(Selected - MaxVisible / 2, 0, FMath::Max(0, Total - MaxVisible));
        const int32 EndIdx = FMath::Min(Total, ScrollStart + MaxVisible);
        for (int32 Row = ScrollStart; Row < EndIdx; ++Row)
        {
            const FABFPSpellEntry& Sp = Snapshot.Spells[Row];
            const float RowY = ListTop + (Row - ScrollStart) * RowH;
            const bool bSel = (Row == Selected);
            if (bSel)
            {
                DrawRect(FLinearColor(0.14f, 0.24f, 0.42f, 0.92f), X + 12.0f, RowY - 1.0f, PanelW - 24.0f, RowH);
            }

            const bool bLearned = (Sp.Flags & ABFPSpellFlags::Learned) != 0;
            const bool bCanCast = (Sp.Flags & ABFPSpellFlags::CanCast) != 0;
            const bool bCanStudy = (Sp.Flags & ABFPSpellFlags::CanStudy) != 0;
            const bool bIsQuick = (QuickSp && QuickSp->Index == Sp.Index);

            FLinearColor NameColor = FLinearColor(0.52f, 0.56f, 0.64f);
            FString Tag;
            FLinearColor TagColor(0.54f, 0.56f, 0.62f);

            if (bLearned && bCanCast)
            {
                NameColor = bSel ? FLinearColor(1.0f, 0.97f, 0.82f) : FLinearColor(0.90f, 0.94f, 0.98f);
                Tag = TEXT("[CAST]");
                TagColor = FLinearColor(0.36f, 0.92f, 0.56f);
            }
            else if (bLearned)
            {
                NameColor = bSel ? FLinearColor(0.94f, 0.90f, 0.78f) : FLinearColor(0.70f, 0.76f, 0.86f);
                Tag = (Sp.Mana > Snapshot.PlayerMana) ? TEXT("[NO MP]") : TEXT("[KNOWN]");
                TagColor = (Sp.Mana > Snapshot.PlayerMana)
                    ? FLinearColor(0.45f, 0.62f, 0.92f)
                    : FLinearColor(0.88f, 0.72f, 0.34f);
            }
            else if (bCanStudy)
            {
                NameColor = bSel ? FLinearColor(1.0f, 0.94f, 0.68f) : FLinearColor(0.96f, 0.86f, 0.48f);
                Tag = TEXT("[STUDY]");
                TagColor = FLinearColor(1.0f, 0.84f, 0.26f);
            }
            else
            {
                if (bSel) NameColor = FLinearColor(0.78f, 0.80f, 0.84f);
                Tag = (Sp.Level > Snapshot.PlayerLevel)
                    ? FString::Printf(TEXT("[LV %d]"), Sp.Level)
                    : TEXT("[LOCKED]");
            }

            const TCHAR Letter = static_cast<TCHAR>('a' + (Row % 26));
            const FString NameText = FString::Printf(TEXT("%s%c) %s"),
                bIsQuick ? TEXT("*") : TEXT(" "), Letter, *Sp.Name);
            DrawText(NameText, NameColor, ColNameX, RowY + 3.0f, SmallFont, 0.90f);
            DrawText(FString::Printf(TEXT("%2d"), Sp.Level), NameColor, ColLvX, RowY + 3.0f, SmallFont, 0.88f);
            DrawText(FString::Printf(TEXT("%2d"), Sp.Mana),
                Sp.Mana <= Snapshot.PlayerMana ? FLinearColor(0.54f, 0.78f, 1.0f) : FLinearColor(0.88f, 0.38f, 0.34f),
                ColMpX, RowY + 3.0f, SmallFont, 0.88f);
            DrawText(FString::Printf(TEXT("%d%%"), Sp.FailPct), NameColor, ColFailX, RowY + 3.0f, SmallFont, 0.88f);
            if (!Sp.Info.IsEmpty())
            {
                FString ShortInfo = Sp.Info;
                if (ShortInfo.Len() > 17) ShortInfo = ShortInfo.Left(16) + TEXT(".");
                DrawText(ShortInfo, FLinearColor(0.74f, 0.78f, 0.84f), ColInfoX, RowY + 3.0f, SmallFont, 0.84f);
            }
            DrawText(Tag, TagColor, ColTagX, RowY + 3.0f, SmallFont, 0.86f);
        }
    }

    DrawRect(FLinearColor(0.24f, 0.34f, 0.52f, 0.65f), X + 12.0f, Y + PanelH - FooterH, PanelW - 24.0f, 1.0f);
    DrawText(TEXT("[Up/Down] Select   [Enter/U] Cast or Study   [V] Quick-Cast   [S/Bksp] Close"),
        FLinearColor(0.82f, 0.88f, 0.98f), X + 16.0f, Y + PanelH - FooterH + 9.0f, SmallFont, 0.85f);
}

void AABFPHUD::DrawCharacterCreationMenu(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot)
{
    UFont* SmallFont = GEngine->GetSmallFont();
    UFont* MedFont = GEngine->GetMediumFont();

    const FABFPBirthState& B = Snapshot.Birth;
    const int32 FocusCol = Pawn->GetBirthFocusColumn();
    const int32 SelectedStat = Pawn->GetBirthSelectedStat();

    /* Full-screen medieval slate backdrop */
    DrawRect(FLinearColor(0.022f, 0.025f, 0.036f, 0.94f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);

    const float PanelW = FMath::Min(1060.0f, Canvas->ClipX - 40.0f);
    const float PanelH = FMath::Min(610.0f, Canvas->ClipY - 40.0f);
    const float X = (Canvas->ClipX - PanelW) * 0.5f;
    const float Y = (Canvas->ClipY - PanelH) * 0.5f;

    DrawRect(FLinearColor(0.045f, 0.050f, 0.072f, 0.98f), X, Y, PanelW, PanelH);
    DrawRect(FLinearColor(0.76f, 0.60f, 0.26f, 0.95f), X, Y, PanelW, 3.0f);
    DrawRect(FLinearColor(0.38f, 0.30f, 0.14f, 0.80f), X, Y + PanelH - 2.0f, PanelW, 2.0f);

    const FABFPBirthOption* CurRace = B.Races.IsValidIndex(B.RaceIndex) ? &B.Races[B.RaceIndex] : nullptr;
    const FABFPBirthOption* CurClass = B.Classes.IsValidIndex(B.ClassIndex) ? &B.Classes[B.ClassIndex] : nullptr;
    const FString RaceName = CurRace ? CurRace->Name : TEXT("Human");
    const FString ClassName = CurClass ? CurClass->Name : TEXT("Warrior");
    const FString DraftName = Pawn->GetBirthDraftName().IsEmpty() ? TEXT("Hero") : Pawn->GetBirthDraftName();

    /* Top title & live summary */
    DrawText(TEXT("ANGBAND  -  CHARACTER CREATION"), FLinearColor(0.98f, 0.85f, 0.44f), X + 18.0f, Y + 12.0f, MedFont, 1.05f);
    const FString Summary = FString::Printf(TEXT("%s  |  %s %s  |  HP %d  |  Gold %d gp"),
        *DraftName, *RaceName, *ClassName, B.HitPoints, B.Gold);
    DrawText(Summary, FLinearColor(0.84f, 0.88f, 0.92f), X + 18.0f, Y + 38.0f, SmallFont, 0.94f);
    DrawRect(FLinearColor(0.32f, 0.27f, 0.16f, 0.70f), X + 14.0f, Y + 58.0f, PanelW - 28.0f, 1.0f);

    const float Gap = 14.0f;
    const float InnerW = PanelW - 36.0f;
    const float Col0W = InnerW * 0.27f;
    const float Col1W = InnerW * 0.29f;
    const float Col2W = InnerW - Col0W - Col1W - Gap * 2.0f;
    const float Col0X = X + 18.0f;
    const float Col1X = Col0X + Col0W + Gap;
    const float Col2X = Col1X + Col1W + Gap;

    const float BodyTop = Y + 68.0f;
    const float BottomBarH = 74.0f;
    const float FooterH = 32.0f;
    const float ColH = PanelH - 68.0f - BottomBarH - FooterH - 18.0f;

    auto DrawSectionBox = [&](float BX, float BY, float BW, float BH, const FString& Heading, bool bFocused)
    {
        DrawRect(bFocused ? FLinearColor(0.08f, 0.085f, 0.12f, 0.95f) : FLinearColor(0.035f, 0.04f, 0.058f, 0.92f),
            BX, BY, BW, BH);
        DrawRect(bFocused ? FLinearColor(0.98f, 0.80f, 0.30f, 0.98f) : FLinearColor(0.34f, 0.30f, 0.22f, 0.70f),
            BX, BY, BW, bFocused ? 3.0f : 1.5f);
        DrawText(Heading, bFocused ? FLinearColor(1.0f, 0.88f, 0.44f) : FLinearColor(0.76f, 0.74f, 0.66f),
            BX + 10.0f, BY + 8.0f, SmallFont, 0.92f);
    };

    /* Column 0: Races */
    DrawSectionBox(Col0X, BodyTop, Col0W, ColH, TEXT("[1] RACE"), FocusCol == 0);
    const float RowH = 22.0f;
    const float ListStartY = BodyTop + 32.0f;
    for (int32 i = 0; i < B.Races.Num(); ++i)
    {
        const float RY = ListStartY + i * RowH;
        if (RY + RowH > BodyTop + ColH - 46.0f) break;
        const bool bSel = (i == B.RaceIndex);
        if (bSel)
        {
            DrawRect(FocusCol == 0 ? FLinearColor(0.30f, 0.24f, 0.11f, 0.92f) : FLinearColor(0.16f, 0.16f, 0.20f, 0.88f),
                Col0X + 6.0f, RY - 1.0f, Col0W - 12.0f, RowH);
        }
        const FLinearColor Color = bSel ? FLinearColor(1.0f, 0.95f, 0.74f) : FLinearColor(0.82f, 0.84f, 0.86f);
        DrawText(FString::Printf(TEXT("%s %s"), bSel ? TEXT(">") : TEXT(" "), *B.Races[i].Name),
            Color, Col0X + 10.0f, RY + 3.0f, SmallFont, 0.90f);
    }
    if (CurRace)
    {
        const float InfoY = BodyTop + ColH - 40.0f;
        DrawRect(FLinearColor(0.24f, 0.22f, 0.18f, 0.55f), Col0X + 8.0f, InfoY - 4.0f, Col0W - 16.0f, 1.0f);
        DrawText(FString::Printf(TEXT("HD %s   EXP %d%%"), *FormatSigned(CurRace->HitDie), CurRace->ExpFactor),
            FLinearColor(0.86f, 0.82f, 0.66f), Col0X + 10.0f, InfoY + 1.0f, SmallFont, 0.82f);
        DrawText(FormatStatModSummary(CurRace->StatAdj),
            FLinearColor(0.66f, 0.74f, 0.82f), Col0X + 10.0f, InfoY + 17.0f, SmallFont, 0.74f);
    }

    /* Column 1: Classes */
    DrawSectionBox(Col1X, BodyTop, Col1W, ColH, TEXT("[2] CLASS"), FocusCol == 1);
    for (int32 i = 0; i < B.Classes.Num(); ++i)
    {
        const float RY = ListStartY + i * RowH;
        if (RY + RowH > BodyTop + ColH - 46.0f) break;
        const bool bSel = (i == B.ClassIndex);
        if (bSel)
        {
            DrawRect(FocusCol == 1 ? FLinearColor(0.30f, 0.24f, 0.11f, 0.92f) : FLinearColor(0.16f, 0.16f, 0.20f, 0.88f),
                Col1X + 6.0f, RY - 1.0f, Col1W - 12.0f, RowH);
        }
        const FLinearColor Color = bSel ? FLinearColor(1.0f, 0.95f, 0.74f) : FLinearColor(0.82f, 0.84f, 0.86f);
        DrawText(FString::Printf(TEXT("%s %s"), bSel ? TEXT(">") : TEXT(" "), *B.Classes[i].Name),
            Color, Col1X + 10.0f, RY + 3.0f, SmallFont, 0.90f);
        if (B.Classes[i].bHasMagic)
        {
            DrawText(TEXT("[Magic]"), FLinearColor(0.46f, 0.78f, 1.0f),
                Col1X + Col1W - 64.0f, RY + 3.0f, SmallFont, 0.80f);
        }
    }
    if (CurClass)
    {
        const float InfoY = BodyTop + ColH - 40.0f;
        DrawRect(FLinearColor(0.24f, 0.22f, 0.18f, 0.55f), Col1X + 8.0f, InfoY - 4.0f, Col1W - 16.0f, 1.0f);
        DrawText(FString::Printf(TEXT("HD %s   EXP %d%%   %s"),
                *FormatSigned(CurClass->HitDie), CurClass->ExpFactor,
                CurClass->bHasMagic ? TEXT("Spellcaster") : TEXT("Martial")),
            FLinearColor(0.86f, 0.82f, 0.66f), Col1X + 10.0f, InfoY + 1.0f, SmallFont, 0.82f);
        DrawText(FormatStatModSummary(CurClass->StatAdj),
            FLinearColor(0.66f, 0.74f, 0.82f), Col1X + 10.0f, InfoY + 17.0f, SmallFont, 0.74f);
    }

    /* Column 2: Stats & Point-Buy / Random Roll */
    DrawSectionBox(Col2X, BodyTop, Col2W, ColH, TEXT("[3] ABILITY SCORES"), FocusCol == 2);
    const FString PointsHeader = B.bRolled
        ? TEXT("Mode: Random Roll   ([R] Re-Roll   [X] Switch to Point-Buy)")
        : FString::Printf(TEXT("Points Left: %d   ([Left/Right] -/+   [R] Roll   [X] Reset)"), B.PointsLeft);
    DrawText(PointsHeader, B.bRolled ? FLinearColor(0.62f, 0.86f, 1.0f) : FLinearColor(1.0f, 0.86f, 0.36f),
        Col2X + 12.0f, BodyTop + 32.0f, SmallFont, 0.84f);

    const float StatHeaderY = BodyTop + 58.0f;
    const float SColName = Col2X + 14.0f;
    const float SColBase = Col2X + 80.0f;
    const float SColMod = Col2X + 150.0f;
    const float SColTot = Col2X + 210.0f;
    const float SColSpent = Col2X + 286.0f;
    const float SColNext = Col2X + 344.0f;
    const FLinearColor StatHdrCol(0.64f, 0.70f, 0.78f);

    DrawText(TEXT("Stat"), StatHdrCol, SColName, StatHeaderY, SmallFont, 0.84f);
    DrawText(TEXT("Base"), StatHdrCol, SColBase, StatHeaderY, SmallFont, 0.84f);
    DrawText(TEXT("Mod"), StatHdrCol, SColMod, StatHeaderY, SmallFont, 0.84f);
    DrawText(TEXT("Total"), StatHdrCol, SColTot, StatHeaderY, SmallFont, 0.84f);
    DrawText(TEXT("Spent"), StatHdrCol, SColSpent, StatHeaderY, SmallFont, 0.84f);
    DrawText(TEXT("Next"), StatHdrCol, SColNext, StatHeaderY, SmallFont, 0.84f);

    static const TCHAR* StatNames[5] = { TEXT("STR"), TEXT("INT"), TEXT("WIS"), TEXT("DEX"), TEXT("CON") };
    const float StatRowH = 30.0f;
    const float StatStartY = BodyTop + 80.0f;
    for (int32 s = 0; s < 5; ++s)
    {
        const float RY = StatStartY + s * StatRowH;
        const bool bSel = (FocusCol == 2 && s == SelectedStat);
        if (bSel)
        {
            DrawRect(FLinearColor(0.30f, 0.24f, 0.11f, 0.92f), Col2X + 8.0f, RY - 2.0f, Col2W - 16.0f, StatRowH - 2.0f);
        }

        const int32 Base = B.StatsBase.IsValidIndex(s) ? B.StatsBase[s] : 10;
        const int32 Tot = B.StatsTotal.IsValidIndex(s) ? B.StatsTotal[s] : Base;
        const int32 Spent = B.PointsSpent.IsValidIndex(s) ? B.PointsSpent[s] : 0;
        const int32 Inc = B.PointsInc.IsValidIndex(s) ? B.PointsInc[s] : 0;
        const int32 Mod = (CurRace && CurRace->StatAdj.IsValidIndex(s) ? CurRace->StatAdj[s] : 0) +
            (CurClass && CurClass->StatAdj.IsValidIndex(s) ? CurClass->StatAdj[s] : 0);

        const FLinearColor RowColor = bSel ? FLinearColor(1.0f, 0.96f, 0.78f) : FLinearColor(0.86f, 0.88f, 0.90f);
        DrawText(StatNames[s], RowColor, SColName, RY + 5.0f, SmallFont, 0.94f);
        const FString BaseStr = (bSel && !B.bRolled)
            ? FString::Printf(TEXT("< %2d >"), Base)
            : FString::Printf(TEXT("  %2d"), Base);
        DrawText(BaseStr, RowColor, SColBase - 8.0f, RY + 5.0f, SmallFont, 0.92f);

        const FLinearColor ModColor = Mod > 0
            ? FLinearColor(0.38f, 0.90f, 0.52f)
            : (Mod < 0 ? FLinearColor(0.94f, 0.42f, 0.36f) : FLinearColor(0.62f, 0.66f, 0.72f));
        DrawText(FormatSigned(Mod), ModColor, SColMod, RY + 5.0f, SmallFont, 0.90f);
        DrawText(FormatAngbandStat(Tot), FLinearColor(0.98f, 0.88f, 0.52f), SColTot, RY + 5.0f, MedFont, 0.86f);
        DrawText(FString::Printf(TEXT("%d"), Spent), FLinearColor(0.74f, 0.78f, 0.84f), SColSpent + 8.0f, RY + 5.0f, SmallFont, 0.88f);

        FString NextStr = TEXT("--");
        FLinearColor NextCol(0.54f, 0.56f, 0.62f);
        if (!B.bRolled)
        {
            if (Inc > 0)
            {
                NextStr = FString::Printf(TEXT("+%d pt"), Inc);
                NextCol = (Inc <= B.PointsLeft) ? FLinearColor(0.42f, 0.90f, 0.56f) : FLinearColor(0.88f, 0.36f, 0.32f);
            }
            else if (Base >= 18)
            {
                NextStr = TEXT("MAX");
                NextCol = FLinearColor(0.95f, 0.80f, 0.32f);
            }
        }
        DrawText(NextStr, NextCol, SColNext, RY + 5.0f, SmallFont, 0.86f);
    }

    const float SummaryY = StatStartY + 5 * StatRowH + 16.0f;
    DrawRect(FLinearColor(0.24f, 0.22f, 0.18f, 0.55f), Col2X + 10.0f, SummaryY - 6.0f, Col2W - 20.0f, 1.0f);
    DrawText(FString::Printf(TEXT("Starting Hit Points:  %d       Starting Gold:  %d gp"), B.HitPoints, B.Gold),
        FLinearColor(0.94f, 0.88f, 0.66f), Col2X + 14.0f, SummaryY + 4.0f, SmallFont, 0.92f);
    DrawText(TEXT("Unspent points convert directly into extra starting gold for Town gear & spellbooks."),
        FLinearColor(0.64f, 0.70f, 0.78f), Col2X + 14.0f, SummaryY + 26.0f, SmallFont, 0.80f);

    /* Bottom interactive row: [4] Name + [5] Begin Adventure */
    const float BottomY = BodyTop + ColH + 12.0f;
    const float NameBoxW = InnerW * 0.54f;
    const float AcceptBoxW = InnerW - NameBoxW - Gap;
    const float AcceptBoxX = Col0X + NameBoxW + Gap;

    DrawSectionBox(Col0X, BottomY, NameBoxW, BottomBarH,
        FocusCol == 3 ? TEXT("[4] CHARACTER NAME  (TYPING - Enter/Tab when done)") : TEXT("[4] CHARACTER NAME"),
        FocusCol == 3);
    const bool bBlink = GetWorld() && FMath::Fmod(GetWorld()->GetTimeSeconds(), 1.0f) < 0.55f;
    const FString NameField = (FocusCol == 3 && bBlink) ? (Pawn->GetBirthDraftName() + TEXT("_")) : Pawn->GetBirthDraftName();
    DrawRect(FLinearColor(0.02f, 0.024f, 0.036f, 0.95f), Col0X + 12.0f, BottomY + 32.0f, NameBoxW - 24.0f, 30.0f);
    DrawText(NameField.IsEmpty() ? TEXT("(type name)") : NameField,
        FocusCol == 3 ? FLinearColor(1.0f, 0.96f, 0.76f) : FLinearColor(0.88f, 0.90f, 0.92f),
        Col0X + 20.0f, BottomY + 39.0f, MedFont, 0.94f);

    const bool bAcceptFocused = (FocusCol == 4);
    DrawRect(bAcceptFocused ? FLinearColor(0.14f, 0.38f, 0.20f, 0.96f) : FLinearColor(0.08f, 0.16f, 0.11f, 0.92f),
        AcceptBoxX, BottomY, AcceptBoxW, BottomBarH);
    DrawRect(bAcceptFocused ? FLinearColor(0.45f, 0.98f, 0.58f, 1.0f) : FLinearColor(0.28f, 0.56f, 0.34f, 0.80f),
        AcceptBoxX, BottomY, AcceptBoxW, bAcceptFocused ? 3.0f : 1.5f);

    const FString BtnTitle = bAcceptFocused ? TEXT(">>  [5] BEGIN ADVENTURE  <<") : TEXT("[5] BEGIN ADVENTURE");
    float BtnW = 0.0f, BtnH = 0.0f;
    GetTextSize(BtnTitle, BtnW, BtnH, MedFont, 0.96f);
    DrawText(BtnTitle, bAcceptFocused ? FLinearColor(0.95f, 1.0f, 0.86f) : FLinearColor(0.76f, 0.90f, 0.78f),
        AcceptBoxX + (AcceptBoxW - BtnW) * 0.5f, BottomY + 16.0f, MedFont, 0.96f);

    const FString BtnSub = TEXT("Press [Enter] (or [F]) to enter Town");
    float SubW = 0.0f, SubH = 0.0f;
    GetTextSize(BtnSub, SubW, SubH, SmallFont, 0.82f);
    DrawText(BtnSub, FLinearColor(0.74f, 0.88f, 0.78f),
        AcceptBoxX + (AcceptBoxW - SubW) * 0.5f, BottomY + 46.0f, SmallFont, 0.82f);

    /* Footer controls */
    DrawRect(FLinearColor(0.32f, 0.27f, 0.16f, 0.70f), X + 14.0f, Y + PanelH - FooterH, PanelW - 28.0f, 1.0f);
    const FString BirthFooter = Snapshot.Saves.Num() > 0
        ? TEXT("[Tab] Next  [Up/Dn] Pick  [L/R] Stat  [R] Roll  [X] Reset  [Enter] Begin  [F9] Load Save")
        : TEXT("[Tab] Next Section   [Up/Down] Choose   [Left/Right] Stat -/+ or Section   [R] Random Roll   [X] Reset   [Enter] Advance / Begin");
    DrawText(BirthFooter,
        FLinearColor(0.88f, 0.80f, 0.56f), X + 18.0f, Y + PanelH - FooterH + 8.0f, SmallFont, 0.84f);
}

void AABFPHUD::DrawSaveLoadMenu(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot)
{
    UFont* SmallFont = GEngine->GetSmallFont();
    UFont* MedFont = GEngine->GetMediumFont();

    DrawRect(FLinearColor(0.01f, 0.012f, 0.02f, 0.58f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);

    const float PanelW = FMath::Min(740.0f, Canvas->ClipX - 72.0f);
    const float PanelH = FMath::Min(460.0f, Canvas->ClipY - 200.0f);
    const float X = (Canvas->ClipX - PanelW) * 0.5f;
    const float Y = FMath::Max(24.0f, (Canvas->ClipY - PanelH) * 0.42f);

    DrawRect(FLinearColor(0.045f, 0.05f, 0.07f, 0.95f), X, Y, PanelW, PanelH);
    DrawRect(FLinearColor(0.74f, 0.58f, 0.24f, 0.92f), X, Y, PanelW, 3.0f);
    DrawRect(FLinearColor(0.34f, 0.28f, 0.14f, 0.72f), X, Y + PanelH - 2.0f, PanelW, 2.0f);

    DrawText(TEXT("SAVE & LOAD CHARACTERS"), FLinearColor(0.98f, 0.86f, 0.46f), X + 16.0f, Y + 12.0f, MedFont, 1.0f);

    FString Subtitle;
    if (!Snapshot.Birth.bActive && !Snapshot.bPlayerDead && !Snapshot.PlayerName.IsEmpty())
    {
        const FString DepthStr = Snapshot.DungeonDepth == 0
            ? TEXT("Town")
            : FString::Printf(TEXT("Depth %d (%dft)"), Snapshot.DungeonDepth, Snapshot.DungeonDepth * 50);
        Subtitle = FString::Printf(TEXT("Current: %s (Lv %d %s %s, %s)   |   [F5] Quick Save Now"),
            *Snapshot.PlayerName, Snapshot.PlayerLevel, *Snapshot.PlayerRaceName,
            *Snapshot.PlayerClassName, *DepthStr);
    }
    else
    {
        Subtitle = FString::Printf(TEXT("%d saved %s on disk   |   Select a living hero and press [Enter] to load"),
            Snapshot.Saves.Num(), Snapshot.Saves.Num() == 1 ? TEXT("character") : TEXT("characters"));
    }
    DrawText(Subtitle, FLinearColor(0.78f, 0.82f, 0.86f), X + 16.0f, Y + 36.0f, SmallFont, 0.88f);
    DrawRect(FLinearColor(0.30f, 0.26f, 0.16f, 0.65f), X + 12.0f, Y + 56.0f, PanelW - 24.0f, 1.0f);

    const float ListTop = Y + 64.0f;
    const float RowH = 26.0f;
    const float FooterH = 34.0f;
    const int32 MaxVisible = FMath::Max(4, static_cast<int32>((PanelH - 64.0f - FooterH) / RowH));
    const int32 Total = Snapshot.Saves.Num();
    const int32 Selected = Pawn->GetSelectedMenuIndex();

    if (Total == 0)
    {
        DrawText(TEXT("(No saved characters found yet)"),
            FLinearColor(0.72f, 0.72f, 0.76f), X + 22.0f, ListTop + 14.0f, SmallFont, 0.96f);
        DrawText(TEXT("Press [F5] during gameplay to save your hero, or [N] to create a new character."),
            FLinearColor(0.58f, 0.62f, 0.68f), X + 22.0f, ListTop + 38.0f, SmallFont, 0.86f);
    }
    else
    {
        const int32 ScrollStart = FMath::Clamp(Selected - MaxVisible / 2, 0, FMath::Max(0, Total - MaxVisible));
        const int32 EndIdx = FMath::Min(Total, ScrollStart + MaxVisible);
        for (int32 Row = ScrollStart; Row < EndIdx; ++Row)
        {
            const FABFPSaveEntry& Entry = Snapshot.Saves[Row];
            const float RowY = ListTop + (Row - ScrollStart) * RowH;
            const bool bSel = (Row == Selected);
            if (bSel)
            {
                DrawRect(FLinearColor(0.26f, 0.22f, 0.11f, 0.92f), X + 12.0f, RowY - 1.0f, PanelW - 24.0f, RowH);
            }

            const FLinearColor NameColor = Entry.bIsDead
                ? (bSel ? FLinearColor(0.86f, 0.56f, 0.52f) : FLinearColor(0.56f, 0.44f, 0.44f))
                : (bSel ? FLinearColor(1.0f, 0.95f, 0.76f) : FLinearColor(0.88f, 0.88f, 0.85f));
            const FString DescText = Entry.Description.IsEmpty() ? Entry.Name : Entry.Description;
            const FString LineText = DescText.StartsWith(Entry.Name, ESearchCase::IgnoreCase)
                ? DescText
                : FString::Printf(TEXT("%s - %s"), *Entry.Name, *DescText);
            DrawText(LineText, NameColor, X + 18.0f, RowY + 5.0f, SmallFont, 0.92f);

            const FString Tag = Entry.bIsCurrent ? TEXT("[ACTIVE]")
                : (Entry.bIsDead ? TEXT("[FALLEN]") : TEXT("[ALIVE]"));
            const FLinearColor TagColor = Entry.bIsCurrent ? FLinearColor(0.32f, 0.92f, 0.48f)
                : (Entry.bIsDead ? FLinearColor(0.88f, 0.30f, 0.26f) : FLinearColor(0.95f, 0.80f, 0.34f));
            DrawText(Tag, TagColor, X + PanelW - 84.0f, RowY + 5.0f, SmallFont, 0.86f);
        }
    }

    DrawRect(FLinearColor(0.30f, 0.26f, 0.16f, 0.65f), X + 12.0f, Y + PanelH - FooterH, PanelW - 24.0f, 1.0f);
    DrawText(TEXT("[Up/Dn] Select   [Enter] Load Hero   [F5] Quick Save   [D x2] Delete   [N] New Hero   [F9/O/Esc] Close"),
        FLinearColor(0.88f, 0.80f, 0.56f), X + 16.0f, Y + PanelH - FooterH + 9.0f, SmallFont, 0.84f);
}

void AABFPHUD::DrawActiveMenu(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot)
{
    const EABFPMenuMode Mode = Pawn->GetActiveMenu();
    if (Mode == EABFPMenuMode::None) return;
    if (Mode == EABFPMenuMode::SaveLoad)
    {
        DrawSaveLoadMenu(Pawn, Snapshot);
        return;
    }
    if (Mode == EABFPMenuMode::Spellbook)
    {
        DrawSpellbookMenu(Pawn, Snapshot);
        return;
    }
    if (Mode == EABFPMenuMode::CharacterCreation)
    {
        DrawCharacterCreationMenu(Pawn, Snapshot);
        return;
    }

    UFont* SmallFont = GEngine->GetSmallFont();
    UFont* MedFont = GEngine->GetMediumFont();

    /* Subtle dark modal backdrop */
    DrawRect(FLinearColor(0.01f, 0.012f, 0.02f, 0.48f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);

    const float PanelW = FMath::Min(700.0f, Canvas->ClipX - 80.0f);
    const float PanelH = FMath::Min(460.0f, Canvas->ClipY - 210.0f);
    const float X = (Canvas->ClipX - PanelW) * 0.5f;
    const float Y = FMath::Max(24.0f, (Canvas->ClipY - PanelH) * 0.42f);

    DrawRect(FLinearColor(0.045f, 0.05f, 0.07f, 0.94f), X, Y, PanelW, PanelH);
    DrawRect(FLinearColor(0.68f, 0.54f, 0.25f, 0.90f), X, Y, PanelW, 3.0f);
    DrawRect(FLinearColor(0.32f, 0.26f, 0.14f, 0.70f), X, Y + PanelH - 2.0f, PanelW, 2.0f);

    FString Title;
    FString Subtitle;
    FString Footer;

    if (Mode == EABFPMenuMode::Inventory)
    {
        Title = TEXT("BACKPACK & QUIVER");
        Subtitle = FString::Printf(TEXT("Gold: %d gp   |   [Tab] Equipment   |   [S] Spellbook"), Snapshot.PlayerGold);
        Footer = TEXT("[Up/Dn] Sel  [Enter/U] Use/Fire  [W] Wield  [K] Throw  [L] Fuel  [D] Drop  [I/Bksp] Close");
    }
    else if (Mode == EABFPMenuMode::Equipment)
    {
        Title = TEXT("WORN EQUIPMENT & CHARACTER");
        Subtitle = FString::Printf(TEXT("Lv %d   AC %d   HP %d/%d   EXP %d   |   [Tab] Switch to Backpack"),
            Snapshot.PlayerLevel, Snapshot.PlayerArmorClass,
            Snapshot.PlayerHitPoints, Snapshot.PlayerMaxHitPoints, Snapshot.PlayerExp);
        Footer = TEXT("[Up/Down] Select   [Enter/T] Take Off   [D] Drop   [Tab] Backpack   [C/Bksp] Close");
    }
    else if (Mode == EABFPMenuMode::ShopBuy)
    {
        const FString ShopName = Snapshot.Shop.Name.IsEmpty() ? TEXT("Town Shop") : Snapshot.Shop.Name;
        Title = Snapshot.Shop.bIsHome
            ? FString::Printf(TEXT("%s  —  RETRIEVE ITEMS"), *ShopName)
            : FString::Printf(TEXT("%s  —  BUY GOODS"), *ShopName);
        Subtitle = Snapshot.Shop.Owner.IsEmpty()
            ? FString::Printf(TEXT("Your Gold: %d gp   |   [Tab] Switch to %s"),
                Snapshot.PlayerGold, Snapshot.Shop.bIsHome ? TEXT("Stash") : TEXT("Sell"))
            : FString::Printf(TEXT("Proprietor: %s   |   Your Gold: %d gp   |   [Tab] %s"),
                *Snapshot.Shop.Owner, Snapshot.PlayerGold, Snapshot.Shop.bIsHome ? TEXT("Stash") : TEXT("Sell"));
        Footer = Snapshot.Shop.bIsHome
            ? TEXT("[Up/Down] Select   [Enter] Retrieve 1   [Tab] Stash from Pack   [B/Bksp] Leave")
            : TEXT("[Up/Down] Select   [Enter] Buy 1   [Tab] Sell from Pack   [B/Bksp] Leave");
    }
    else if (Mode == EABFPMenuMode::ShopSell)
    {
        const FString ShopName = Snapshot.Shop.Name.IsEmpty() ? TEXT("Town Shop") : Snapshot.Shop.Name;
        Title = Snapshot.Shop.bIsHome
            ? FString::Printf(TEXT("%s  —  STASH FROM PACK"), *ShopName)
            : FString::Printf(TEXT("%s  —  SELL FROM PACK"), *ShopName);
        Subtitle = FString::Printf(TEXT("Your Gold: %d gp   |   [Tab] Switch to %s"),
            Snapshot.PlayerGold, Snapshot.Shop.bIsHome ? TEXT("Retrieve") : TEXT("Buy"));
        Footer = Snapshot.Shop.bIsHome
            ? TEXT("[Up/Down] Select   [Enter] Stash 1   [Tab] Retrieve from Home   [B/Bksp] Leave")
            : TEXT("[Up/Down] Select   [Enter] Sell 1   [Tab] Buy Goods   [B/Bksp] Leave");
    }

    DrawText(Title, FLinearColor(0.98f, 0.85f, 0.46f), X + 16.0f, Y + 12.0f, MedFont, 1.0f);
    DrawText(Subtitle, FLinearColor(0.76f, 0.80f, 0.84f), X + 16.0f, Y + 36.0f, SmallFont, 0.90f);
    DrawRect(FLinearColor(0.28f, 0.25f, 0.18f, 0.60f), X + 12.0f, Y + 56.0f, PanelW - 24.0f, 1.0f);

    const float ListTop = Y + 64.0f;
    const float RowH = 22.0f;
    const float FooterH = 34.0f;
    const int32 MaxVisible = FMath::Max(4, static_cast<int32>((PanelH - 64.0f - FooterH) / RowH));
    const int32 Selected = Pawn->GetSelectedMenuIndex();

    if (Mode == EABFPMenuMode::ShopBuy)
    {
        const int32 Total = Snapshot.Shop.Items.Num();
        if (Total == 0)
        {
            DrawText(Snapshot.Shop.bIsHome ? TEXT("(Your home is empty)") : TEXT("(No goods currently in stock)"),
                FLinearColor(0.65f, 0.66f, 0.70f), X + 20.0f, ListTop + 12.0f, SmallFont, 0.95f);
        }
        else
        {
            const int32 ScrollStart = FMath::Clamp(Selected - MaxVisible / 2, 0, FMath::Max(0, Total - MaxVisible));
            const int32 EndIdx = FMath::Min(Total, ScrollStart + MaxVisible);
            for (int32 Row = ScrollStart; Row < EndIdx; ++Row)
            {
                const FABFPShopItem& Item = Snapshot.Shop.Items[Row];
                const float RowY = ListTop + (Row - ScrollStart) * RowH;
                const bool bSel = (Row == Selected);
                if (bSel)
                {
                    DrawRect(FLinearColor(0.26f, 0.22f, 0.11f, 0.90f), X + 12.0f, RowY - 1.0f, PanelW - 24.0f, RowH);
                }
                const TCHAR Letter = static_cast<TCHAR>('a' + (Row % 26));
                const bool bAffordable = Snapshot.Shop.bIsHome || (Item.Price <= Snapshot.PlayerGold);
                const FLinearColor NameColor = bSel
                    ? FLinearColor(1.0f, 0.95f, 0.78f)
                    : (bAffordable ? FLinearColor(0.88f, 0.88f, 0.86f) : FLinearColor(0.54f, 0.50f, 0.50f));
                DrawText(FString::Printf(TEXT("%c) %s"), Letter, *Item.Name),
                    NameColor, X + 18.0f, RowY + 3.0f, SmallFont, 0.92f);

                if (!Snapshot.Shop.bIsHome)
                {
                    const FString PriceStr = FString::Printf(TEXT("%d gp"), Item.Price);
                    const FLinearColor PriceColor = bAffordable
                        ? FLinearColor(0.95f, 0.82f, 0.30f)
                        : FLinearColor(0.88f, 0.28f, 0.24f);
                    DrawText(PriceStr, PriceColor, X + PanelW - 96.0f, RowY + 3.0f, SmallFont, 0.92f);
                }
            }
        }
    }
    else
    {
        const bool bEquip = (Mode == EABFPMenuMode::Equipment);
        const TArray<int32> Indices = Pawn->GetMenuInventoryIndices(bEquip);
        const int32 Total = Indices.Num();
        if (Total == 0)
        {
            DrawText(bEquip ? TEXT("(No items equipped)") : TEXT("(Your pack is empty)"),
                FLinearColor(0.65f, 0.66f, 0.70f), X + 20.0f, ListTop + 12.0f, SmallFont, 0.95f);
        }
        else
        {
            const int32 ScrollStart = FMath::Clamp(Selected - MaxVisible / 2, 0, FMath::Max(0, Total - MaxVisible));
            const int32 EndIdx = FMath::Min(Total, ScrollStart + MaxVisible);
            for (int32 Row = ScrollStart; Row < EndIdx; ++Row)
            {
                const FABFPInventoryItem& Item = Snapshot.Inventory[Indices[Row]];
                const float RowY = ListTop + (Row - ScrollStart) * RowH;
                const bool bSel = (Row == Selected);
                if (bSel)
                {
                    DrawRect(FLinearColor(0.26f, 0.22f, 0.11f, 0.90f), X + 12.0f, RowY - 1.0f, PanelW - 24.0f, RowH);
                }

                const FString Label = Item.Label.IsEmpty() ? TEXT("-") : Item.Label;
                FString RowText;
                if (bEquip && !Item.SlotName.IsEmpty())
                {
                    RowText = FString::Printf(TEXT("%s) [%-10s]  %s"), *Label, *Item.SlotName, *Item.Name);
                }
                else
                {
                    RowText = FString::Printf(TEXT("%s) %s"), *Label, *Item.Name);
                }

                bool bDimmed = false;
                if (Mode == EABFPMenuMode::ShopSell && !Snapshot.Shop.bIsHome &&
                    (Item.Flags & ABFPItemFlags::Sellable) == 0)
                {
                    bDimmed = true;
                }

                const FLinearColor NameColor = bSel
                    ? FLinearColor(1.0f, 0.95f, 0.78f)
                    : (bDimmed ? FLinearColor(0.48f, 0.48f, 0.52f) : FLinearColor(0.88f, 0.88f, 0.86f));
                DrawText(RowText, NameColor, X + 18.0f, RowY + 3.0f, SmallFont, 0.92f);

                if (Mode == EABFPMenuMode::ShopSell && !Snapshot.Shop.bIsHome)
                {
                    if ((Item.Flags & ABFPItemFlags::Sellable) != 0)
                    {
                        DrawText(FString::Printf(TEXT("+%d gp"), Item.SellPrice),
                            FLinearColor(0.42f, 0.92f, 0.48f), X + PanelW - 96.0f, RowY + 3.0f, SmallFont, 0.92f);
                    }
                    else
                    {
                        DrawText(TEXT("Unsellable"),
                            FLinearColor(0.50f, 0.46f, 0.46f), X + PanelW - 102.0f, RowY + 3.0f, SmallFont, 0.86f);
                    }
                }
                else if (Mode == EABFPMenuMode::Inventory)
                {
                    FString Tag;
                    FLinearColor TagColor(0.62f, 0.76f, 0.92f);
                    if ((Item.Flags & ABFPItemFlags::Quiver) != 0)
                    {
                        Tag = Snapshot.bHasLauncher ? TEXT("[Quiver/Fire]") : TEXT("[Quiver]");
                        TagColor = FLinearColor(0.96f, 0.80f, 0.36f);
                    }
                    else if ((Item.Flags & ABFPItemFlags::Ammo) != 0)
                    {
                        Tag = Snapshot.bHasLauncher ? TEXT("[Fire]") : TEXT("[Ammo]");
                        TagColor = FLinearColor(0.62f, 0.88f, 0.98f);
                    }
                    else if ((Item.Flags & ABFPItemFlags::Potion) != 0) Tag = TEXT("[Quaff]");
                    else if ((Item.Flags & ABFPItemFlags::Scroll) != 0) Tag = TEXT("[Read]");
                    else if ((Item.Flags & ABFPItemFlags::Food) != 0) Tag = TEXT("[Eat]");
                    else if ((Item.Flags & ABFPItemFlags::Fuel) != 0) Tag = TEXT("[Fuel]");
                    else if ((Item.Flags & ABFPItemFlags::Usable) != 0) Tag = TEXT("[Use]");
                    else if ((Item.Flags & ABFPItemFlags::Wearable) != 0) Tag = TEXT("[Wield]");
                    if (!Tag.IsEmpty())
                    {
                        DrawText(Tag, TagColor, X + PanelW - 104.0f, RowY + 3.0f, SmallFont, 0.84f);
                    }
                }
            }
        }
    }

    DrawRect(FLinearColor(0.28f, 0.25f, 0.18f, 0.60f), X + 12.0f, Y + PanelH - FooterH, PanelW - 24.0f, 1.0f);
    DrawText(Footer, FLinearColor(0.86f, 0.78f, 0.56f), X + 16.0f, Y + PanelH - FooterH + 9.0f, SmallFont, 0.84f);
}

void AABFPHUD::DrawControlsHelp(const AABFPFirstPersonPawn* Pawn)
{
    UFont* SmallFont = GEngine->GetSmallFont();
    const float X = 20.0f;
    const float Y = 20.0f;

    if (!Pawn->IsHelpVisible())
    {
        DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.68f), X, Y, 118.0f, 24.0f);
        DrawText(TEXT("[H] Controls"), FLinearColor(0.80f, 0.78f, 0.72f), X + 10.0f, Y + 5.0f, SmallFont, 0.90f);
        return;
    }

    const FString RunLine = Pawn->IsRunModeEnabled()
        ? TEXT("Shift / .      Run mode [ON]")
        : TEXT("Shift / .      Toggle Run mode");
    const FString Lines[] = {
        TEXT("CONTROLS  ([H] hide)"),
        TEXT("Up / Down      Step / Run fwd & back"),
        RunLine,
        TEXT("Left / Right   Turn 45 deg"),
        TEXT("F              Interact / Door / Shop"),
        TEXT("E / G          Pick up item"),
        TEXT("Z              Fire bow / sling"),
        TEXT("S / V          Spells / Quick cast"),
        TEXT("Tab            Cycle ranged target"),
        TEXT("I / C          Inventory / Equipment"),
        TEXT("B              Open Town Shop"),
        TEXT("Q / X / R      Quaff / Eat / Rest"),
        TEXT("M / N          Minimap / New hero"),
        TEXT("F5 / F9 (O)    Quick Save / Load Menu"),
        TEXT("Space          Wait 1 turn"),
        TEXT("PgUp / PgDn    Ascend / Descend stairs")
    };

    const float LineH = 16.0f;
    const float PanelW = 286.0f;
    const float PanelH = UE_ARRAY_COUNT(Lines) * LineH + 14.0f;
    DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.76f), X, Y, PanelW, PanelH);
    DrawRect(FLinearColor(0.55f, 0.43f, 0.22f, 0.65f), X, Y, PanelW, 2.0f);

    for (int32 Idx = 0; Idx < UE_ARRAY_COUNT(Lines); ++Idx)
    {
        const FLinearColor Color = (Idx == 0)
            ? FLinearColor(0.95f, 0.82f, 0.45f)
            : FLinearColor(0.84f, 0.84f, 0.80f);
        DrawText(Lines[Idx], Color, X + 10.0f, Y + 7.0f + Idx * LineH, SmallFont, 0.88f);
    }
}
