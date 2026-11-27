#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ABFPProtocol.h"
#include "ABFPHUD.generated.h"

class AABFPFirstPersonPawn;

/**
 * 2D HUD presenting authoritative Angband vitals (HP, MP, AC, Speed, Food, EXP),
 * compact/expanded tactical minimap, look-ahead target & shop prompts,
 * keyboard-driven Inventory / Equipment / Shop overlays, and message log.
 */
UCLASS(Blueprintable)
class ANGBANDFIRSTPERSON_API AABFPHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    void DrawVitalsPanel(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot);
    void DrawMessageLog(const AABFPFirstPersonPawn* Pawn);
    void DrawMinimap(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot);
    void DrawTargetAndPrompt(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot);
    void DrawControlsHelp(const AABFPFirstPersonPawn* Pawn);
    void DrawActiveMenu(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot);
    void DrawSpellbookMenu(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot);
    void DrawCharacterCreationMenu(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot);
    void DrawSaveLoadMenu(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot);
    void DrawStatusOverlay(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot);
};
