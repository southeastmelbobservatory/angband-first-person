#include "ABFPGameMode.h"
#include "ABFPFirstPersonPawn.h"

AABFPGameMode::AABFPGameMode()
{
    DefaultPawnClass = AABFPFirstPersonPawn::StaticClass();
}
