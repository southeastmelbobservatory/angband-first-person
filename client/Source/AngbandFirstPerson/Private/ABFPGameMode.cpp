#include "ABFPGameMode.h"
#include "ABFPFirstPersonPawn.h"
#include "ABFPHUD.h"

AABFPGameMode::AABFPGameMode()
{
    DefaultPawnClass = AABFPFirstPersonPawn::StaticClass();
    HUDClass = AABFPHUD::StaticClass();
}
