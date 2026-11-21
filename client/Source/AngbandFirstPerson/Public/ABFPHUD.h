#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ABFPProtocol.h"
#include "ABFPHUD.generated.h"

class AABFPFirstPersonPawn;

/**
 * Lightweight 2D HUD presenting authoritative Angband vitals, 21x21 minimap,
 * look-ahead target/interaction indicator, and scrolling Angband message log.
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
    void DrawStatusOverlay(const AABFPFirstPersonPawn* Pawn, const FABFPSnapshot& Snapshot);
};
