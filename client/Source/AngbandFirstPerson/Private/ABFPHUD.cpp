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
    DrawTargetAndPrompt(Pawn, Snapshot);
    DrawControlsHelp(Pawn);
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

    const float PanelW = 290.0f;
    const float PanelH = Snapshot.PlayerMaxMana > 0 ? 124.0f : 102.0f;
    const float X = 20.0f;
    const float Y = Canvas->ClipY - PanelH - 20.0f;

    DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.78f), X, Y, PanelW, PanelH);
    DrawRect(FLinearColor(0.45f, 0.36f, 0.20f, 0.65f), X, Y, PanelW, 2.0f);

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
    DrawText(DepthLabel, FLinearColor(0.90f, 0.85f, 0.72f), X + 150.0f, Y + 8.0f, SmallFont, 1.0f);

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

    float StatsY = BarY + 26.0f;
    if (Snapshot.PlayerMaxMana > 0)
    {
        const float MpRatio = FMath::Clamp(
            static_cast<float>(Snapshot.PlayerMana) / static_cast<float>(Snapshot.PlayerMaxMana), 0.0f, 1.0f);
        DrawRect(FLinearColor(0.10f, 0.12f, 0.18f, 0.92f), BarX, StatsY, BarW, 14.0f);
        DrawRect(FLinearColor(0.20f, 0.45f, 0.92f), BarX, StatsY, BarW * MpRatio, 14.0f);
        DrawText(FString::Printf(TEXT("MP  %d / %d"), Snapshot.PlayerMana, Snapshot.PlayerMaxMana),
            FLinearColor(0.88f, 0.94f, 1.0f), BarX + 8.0f, StatsY, SmallFont, 0.85f);
        StatsY += 22.0f;
    }

    const FString StatsLine = FString::Printf(TEXT("Lv %d   Gold %d   Turn %lld"),
        Snapshot.PlayerLevel, Snapshot.PlayerGold, Snapshot.Turn);
    DrawText(StatsLine, FLinearColor(0.92f, 0.84f, 0.62f), BarX, StatsY + 2.0f, MedFont, 0.88f);
}

void AABFPHUD::DrawMessageLog(const AABFPFirstPersonPawn* Pawn)
{
    const TArray<FABFPMessage>& History = Pawn->GetMessageHistory();
    if (History.Num() == 0) return;

    UFont* SmallFont = GEngine->GetSmallFont();
    const int32 VisibleLines = FMath::Min(History.Num(), 7);
    const float LineHeight = 18.0f;
    const float PanelW = FMath::Min(540.0f, Canvas->ClipX * 0.44f);
    const float PanelH = VisibleLines * LineHeight + 18.0f;
    const float X = Canvas->ClipX - PanelW - 20.0f;
    const float Y = Canvas->ClipY - PanelH - 20.0f;

    DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.72f), X, Y, PanelW, PanelH);
    DrawRect(FLinearColor(0.45f, 0.36f, 0.20f, 0.55f), X, Y, PanelW, 2.0f);

    const int32 StartIdx = History.Num() - VisibleLines;
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
        else if (Lower.Contains(TEXT("pick")) || Lower.Contains(TEXT("gold")) || Lower.Contains(TEXT("found")))
        {
            TextColor = FLinearColor(0.95f, 0.82f, 0.32f, AgeAlpha);
        }
        DrawText(Msg.Text, TextColor, X + 10.0f, Y + 9.0f + Row * LineHeight, SmallFont, 0.92f);
    }
}

void AABFPHUD::DrawMinimap(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot)
{
    if (Snapshot.Cells.Num() == 0) return;

    constexpr int32 Radius = 10;
    constexpr int32 GridSide = Radius * 2 + 1;
    const float TilePx = 8.0f;
    const float MapSize = GridSide * TilePx;
    const float Pad = 10.0f;
    const float OriginX = Canvas->ClipX - MapSize - Pad * 2.0f - 20.0f;
    const float OriginY = 20.0f;

    DrawRect(FLinearColor(0.03f, 0.035f, 0.05f, 0.80f), OriginX, OriginY, MapSize + Pad * 2.0f, MapSize + Pad * 2.0f);
    DrawRect(FLinearColor(0.45f, 0.36f, 0.20f, 0.65f), OriginX, OriginY, MapSize + Pad * 2.0f, 2.0f);

    const float BaseX = OriginX + Pad;
    const float BaseY = OriginY + Pad;

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
            Color = FLinearColor(0.90f, 0.58f, 0.18f);
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
        const float PlateW = 260.0f;
        const float PlateH = 44.0f;
        const float PlateX = (Canvas->ClipX - PlateW) * 0.5f;
        const float PlateY = 24.0f;
        DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.80f), PlateX, PlateY, PlateW, PlateH);
        const FString Label = FacedMonster->Name.IsEmpty() ? TEXT("Unknown Creature") : FacedMonster->Name;
        DrawText(Label, FLinearColor(0.96f, 0.86f, 0.68f), PlateX + 10.0f, PlateY + 6.0f, MedFont, 0.90f);

        const float Ratio = FacedMonster->MaxHitPoints > 0
            ? FMath::Clamp(static_cast<float>(FacedMonster->HitPoints) / static_cast<float>(FacedMonster->MaxHitPoints), 0.0f, 1.0f)
            : 1.0f;
        DrawRect(FLinearColor(0.14f, 0.10f, 0.10f, 0.92f), PlateX + 10.0f, PlateY + 28.0f, PlateW - 20.0f, 9.0f);
        DrawRect(FLinearColor(0.86f, 0.18f, 0.14f), PlateX + 10.0f, PlateY + 28.0f, (PlateW - 20.0f) * Ratio, 9.0f);
    }

    /* Context action hint (underfoot stairs/items or immediately ahead door/trap/rubble). */
    FString Prompt;
    for (const FABFPEntity& Entity : Snapshot.Entities)
    {
        if (Entity.Type == EABFPEntityType::Object &&
            Entity.X == Snapshot.PlayerX && Entity.Y == Snapshot.PlayerY)
        {
            Prompt = FString::Printf(TEXT("[E] Pick up %s"),
                Entity.Name.IsEmpty() ? TEXT("item") : *Entity.Name);
            break;
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
        DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.78f), BoxX, BoxY, BoxW, BoxH);
        DrawText(Prompt, FLinearColor(0.98f, 0.88f, 0.56f), BoxX + 12.0f, BoxY + 6.0f, SmallFont, 1.05f);
    }
}

void AABFPHUD::DrawControlsHelp(const AABFPFirstPersonPawn* Pawn)
{
    UFont* SmallFont = GEngine->GetSmallFont();
    const float X = 20.0f;
    const float Y = 20.0f;

    if (!Pawn->IsHelpVisible())
    {
        DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.65f), X, Y, 118.0f, 24.0f);
        DrawText(TEXT("[H] Controls"), FLinearColor(0.80f, 0.78f, 0.72f), X + 10.0f, Y + 5.0f, SmallFont, 0.90f);
        return;
    }

    const FString Lines[] = {
        TEXT("CONTROLS  ([H] hide)"),
        TEXT("Up / Down      Step forward / back"),
        TEXT("Left / Right   Turn 45 deg"),
        TEXT("F              Interact / Door / Disarm"),
        TEXT("E              Pick up item"),
        TEXT("Space          Wait 1 turn"),
        TEXT("PgUp / PgDn    Ascend / Descend stairs")
    };

    const float LineH = 16.0f;
    const float PanelW = 264.0f;
    const float PanelH = UE_ARRAY_COUNT(Lines) * LineH + 14.0f;
    DrawRect(FLinearColor(0.04f, 0.045f, 0.06f, 0.74f), X, Y, PanelW, PanelH);
    DrawRect(FLinearColor(0.45f, 0.36f, 0.20f, 0.60f), X, Y, PanelW, 2.0f);

    for (int32 Idx = 0; Idx < UE_ARRAY_COUNT(Lines); ++Idx)
    {
        const FLinearColor Color = (Idx == 0)
            ? FLinearColor(0.95f, 0.82f, 0.45f)
            : FLinearColor(0.84f, 0.84f, 0.80f);
        DrawText(Lines[Idx], Color, X + 10.0f, Y + 7.0f + Idx * LineH, SmallFont, 0.88f);
    }
}
