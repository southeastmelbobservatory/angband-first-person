#include "ABFPHUD.h"

#include "ABFPFirstPersonPawn.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

void AABFPHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !GEngine) return;

    const AABFPFirstPersonPawn* Pawn = Cast<AABFPFirstPersonPawn>(GetOwningPawn());
    if (!Pawn) return;

    const FABFPSnapshot& Snapshot = Pawn->GetCurrentSnapshot();
    DrawStatusOverlay(Pawn, Snapshot);
    DrawVitalsPanel(Pawn, Snapshot);
    DrawMessageLog(Pawn);
    DrawMinimap(Pawn, Snapshot);
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
        DrawRect(FLinearColor(0.05f, 0.01f, 0.01f, 0.65f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
        UFont* LargeFont = GEngine->GetLargeFont();
        const FString Banner = TEXT("YOU HAVE DIED");
        float TextW = 0.0f, TextH = 0.0f;
        GetTextSize(Banner, TextW, TextH, LargeFont, 1.8f);
        DrawText(Banner, FLinearColor(0.95f, 0.18f, 0.14f, 1.0f),
            (Canvas->ClipX - TextW) * 0.5f, Canvas->ClipY * 0.42f, LargeFont, 1.8f);
    }
}

void AABFPHUD::DrawVitalsPanel(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot)
{
    UFont* SmallFont = GEngine->GetSmallFont();
    UFont* MedFont = GEngine->GetMediumFont();

    const float PanelW = 316.0f;
    const float PanelH = Snapshot.PlayerMaxMana > 0 ? 148.0f : 126.0f;
    const float X = 20.0f;
    const float Y = Canvas->ClipY - PanelH - 20.0f;

    DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.82f), X, Y, PanelW, PanelH);
    DrawRect(FLinearColor(0.55f, 0.43f, 0.22f, 0.75f), X, Y, PanelW, 2.0f);

    const FString ModeText = Pawn->IsLiveMode()
        ? (Pawn->HasAuthoritativeState() ? TEXT("ANGBAND LIVE") : TEXT("CONNECTING..."))
        : TEXT("RENDERER DEMO");
    const FLinearColor ModeColor = Pawn->IsLiveMode()
        ? (Pawn->HasAuthoritativeState() ? FLinearColor(0.32f, 0.90f, 0.48f) : FLinearColor(0.95f, 0.78f, 0.25f))
        : FLinearColor(0.92f, 0.74f, 0.34f);

    const FString DepthLabel = Snapshot.DungeonDepth == 0
        ? TEXT("Town")
        : FString::Printf(TEXT("Depth %d (%dft)"), Snapshot.DungeonDepth, Snapshot.DungeonDepth * 50);

    DrawText(ModeText, ModeColor, X + 12.0f, Y + 8.0f, SmallFont, 1.0f);
    DrawText(DepthLabel, FLinearColor(0.92f, 0.87f, 0.74f), X + 166.0f, Y + 8.0f, SmallFont, 1.0f);

    /* Health bar */
    const float BarX = X + 12.0f;
    const float BarY = Y + 32.0f;
    const float BarW = PanelW - 24.0f;
    const float BarH = 18.0f;
    const float HpRatio = Snapshot.PlayerMaxHitPoints > 0
        ? FMath::Clamp(static_cast<float>(Snapshot.PlayerHitPoints) / static_cast<float>(Snapshot.PlayerMaxHitPoints), 0.0f, 1.0f)
        : 0.0f;
    const FLinearColor HpColor = HpRatio > 0.55f ? FLinearColor(0.18f, 0.68f, 0.28f)
        : (HpRatio > 0.25f ? FLinearColor(0.86f, 0.56f, 0.12f) : FLinearColor(0.85f, 0.14f, 0.12f));

    DrawRect(FLinearColor(0.12f, 0.12f, 0.14f, 0.92f), BarX, BarY, BarW, BarH);
    DrawRect(HpColor, BarX, BarY, BarW * HpRatio, BarH);
    const FString HpText = Snapshot.PlayerMaxHitPoints > 0
        ? FString::Printf(TEXT("HP  %d / %d"), Snapshot.PlayerHitPoints, Snapshot.PlayerMaxHitPoints)
        : TEXT("HP  --");
    DrawText(HpText, FLinearColor::White, BarX + 8.0f, BarY + 2.0f, SmallFont, 0.95f);

    float StatsY = BarY + 25.0f;
    if (Snapshot.PlayerMaxMana > 0)
    {
        const float MpRatio = FMath::Clamp(
            static_cast<float>(Snapshot.PlayerMana) / static_cast<float>(Snapshot.PlayerMaxMana), 0.0f, 1.0f);
        DrawRect(FLinearColor(0.10f, 0.12f, 0.18f, 0.92f), BarX, StatsY, BarW, 14.0f);
        DrawRect(FLinearColor(0.20f, 0.45f, 0.92f), BarX, StatsY, BarW * MpRatio, 14.0f);
        DrawText(FString::Printf(TEXT("MP  %d / %d"), Snapshot.PlayerMana, Snapshot.PlayerMaxMana),
            FLinearColor(0.88f, 0.94f, 1.0f), BarX + 8.0f, StatsY, SmallFont, 0.85f);
        StatsY += 21.0f;
    }

    const FString StatsLine = FString::Printf(TEXT("Lv %d   AC %d   Gold %d"),
        Snapshot.PlayerLevel, Snapshot.PlayerArmorClass, Snapshot.PlayerGold);
    DrawText(StatsLine, FLinearColor(0.95f, 0.86f, 0.60f), BarX, StatsY + 2.0f, MedFont, 0.86f);

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

    for (const FABFPEntity& Entity : Snapshot.Entities)
    {
        const int32 RelX = Entity.X - Snapshot.PlayerX + Radius;
        const int32 RelY = Entity.Y - Snapshot.PlayerY + Radius;
        if (RelX < 0 || RelX >= GridSide || RelY < 0 || RelY >= GridSide) continue;
        const FLinearColor DotColor = (Entity.Type == EABFPEntityType::Monster)
            ? FLinearColor(0.95f, 0.20f, 0.16f)
            : FLinearColor(0.98f, 0.84f, 0.22f);
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

    /* Look up to 4 tiles ahead along the player's facing ray for a visible monster. */
    const FABFPEntity* FacedMonster = nullptr;
    for (int32 Step = 1; Step <= 4 && !FacedMonster; ++Step)
    {
        const int32 CheckX = Snapshot.PlayerX + Facing.X * Step;
        const int32 CheckY = Snapshot.PlayerY + Facing.Y * Step;
        for (const FABFPEntity& Entity : Snapshot.Entities)
        {
            if (Entity.Type == EABFPEntityType::Monster && Entity.X == CheckX && Entity.Y == CheckY)
            {
                FacedMonster = &Entity;
                break;
            }
        }
        const FABFPCell* Cell = Snapshot.Cells.FindByPredicate(
            [CheckX, CheckY](const FABFPCell& C) { return C.X == CheckX && C.Y == CheckY; });
        if (Cell && (Cell->Terrain == EABFPTerrain::Wall || Cell->Terrain == EABFPTerrain::Vein || Cell->Terrain == EABFPTerrain::Door))
        {
            break;
        }
    }

    if (FacedMonster)
    {
        const float PlateW = 280.0f;
        const float PlateH = 46.0f;
        const float PlateX = (Canvas->ClipX - PlateW) * 0.5f;
        const float PlateY = 24.0f;
        DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.84f), PlateX, PlateY, PlateW, PlateH);
        DrawRect(FLinearColor(0.75f, 0.22f, 0.16f, 0.75f), PlateX, PlateY, PlateW, 2.0f);
        const FString Label = FacedMonster->Name.IsEmpty() ? TEXT("Unknown Creature") : FacedMonster->Name;
        DrawText(Label, FLinearColor(0.96f, 0.86f, 0.68f), PlateX + 10.0f, PlateY + 7.0f, MedFont, 0.90f);

        const float Ratio = FacedMonster->MaxHitPoints > 0
            ? FMath::Clamp(static_cast<float>(FacedMonster->HitPoints) / static_cast<float>(FacedMonster->MaxHitPoints), 0.0f, 1.0f)
            : 1.0f;
        DrawRect(FLinearColor(0.14f, 0.10f, 0.10f, 0.92f), PlateX + 10.0f, PlateY + 30.0f, PlateW - 20.0f, 9.0f);
        DrawRect(FLinearColor(0.86f, 0.18f, 0.14f), PlateX + 10.0f, PlateY + 30.0f, (PlateW - 20.0f) * Ratio, 9.0f);
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

void AABFPHUD::DrawActiveMenu(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot)
{
    const EABFPMenuMode Mode = Pawn->GetActiveMenu();
    if (Mode == EABFPMenuMode::None) return;

    UFont* SmallFont = GEngine->GetSmallFont();
    UFont* MedFont = GEngine->GetMediumFont();

    /* Subtle dark modal backdrop */
    DrawRect(FLinearColor(0.01f, 0.012f, 0.02f, 0.48f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);

    const float PanelW = FMath::Min(680.0f, Canvas->ClipX - 80.0f);
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
        Title = TEXT("BACKPACK INVENTORY");
        Subtitle = FString::Printf(TEXT("Gold: %d gp   |   [Tab] Switch to Equipment"), Snapshot.PlayerGold);
        Footer = TEXT("[Up/Down] Select   [Enter/U] Use   [W] Wield   [L] Refill   [D] Drop   [I/Bksp] Close");
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
                    if ((Item.Flags & ABFPItemFlags::Potion) != 0) Tag = TEXT("[Quaff]");
                    else if ((Item.Flags & ABFPItemFlags::Scroll) != 0) Tag = TEXT("[Read]");
                    else if ((Item.Flags & ABFPItemFlags::Food) != 0) Tag = TEXT("[Eat]");
                    else if ((Item.Flags & ABFPItemFlags::Fuel) != 0) Tag = TEXT("[Fuel]");
                    else if ((Item.Flags & ABFPItemFlags::Usable) != 0) Tag = TEXT("[Use]");
                    else if ((Item.Flags & ABFPItemFlags::Wearable) != 0) Tag = TEXT("[Wield]");
                    if (!Tag.IsEmpty())
                    {
                        DrawText(Tag, FLinearColor(0.62f, 0.76f, 0.92f), X + PanelW - 88.0f, RowY + 3.0f, SmallFont, 0.86f);
                    }
                }
            }
        }
    }

    DrawRect(FLinearColor(0.28f, 0.25f, 0.18f, 0.60f), X + 12.0f, Y + PanelH - FooterH, PanelW - 24.0f, 1.0f);
    DrawText(Footer, FLinearColor(0.86f, 0.78f, 0.56f), X + 16.0f, Y + PanelH - FooterH + 9.0f, SmallFont, 0.85f);
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

    const FString Lines[] = {
        TEXT("CONTROLS  ([H] hide)"),
        TEXT("Up / Down      Step forward / back"),
        TEXT("Left / Right   Turn 45 deg"),
        TEXT("F              Interact / Door / Shop"),
        TEXT("E / G          Pick up item"),
        TEXT("I / C          Inventory / Equipment"),
        TEXT("B              Open Town Shop"),
        TEXT("Q / X / R      Quaff / Eat / Rest"),
        TEXT("M              Expand Minimap"),
        TEXT("Space          Wait 1 turn"),
        TEXT("PgUp / PgDn    Ascend / Descend stairs")
    };

    const float LineH = 16.0f;
    const float PanelW = 278.0f;
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
