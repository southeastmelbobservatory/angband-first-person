#include "ABFPFirstPersonPawn.h"

#include "ABFPDungeonActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"

AABFPFirstPersonPawn::AABFPFirstPersonPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;
    bUseControllerRotationYaw = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    Camera->SetupAttachment(SceneRoot);
    Camera->SetRelativeLocation(FVector(0, 0, 170));
    Camera->bUsePawnControlRotation = true;

    Lantern = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lantern"));
    Lantern->SetupAttachment(Camera);
    Lantern->SetIntensity(2800.0f);
    Lantern->SetAttenuationRadius(1900.0f);
    Lantern->SetLightColor(FLinearColor(1.0f, 0.78f, 0.52f));
    Lantern->SetCastShadows(true);

    StatusReadout = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusReadout"));
    StatusReadout->SetupAttachment(Camera);
    StatusReadout->SetRelativeLocation(FVector(420.0f, 0.0f, -170.0f));
    StatusReadout->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
    StatusReadout->SetHorizontalAlignment(EHTA_Center);
    StatusReadout->SetWorldSize(28.0f);
    StatusReadout->SetTextRenderColor(FColor(232, 211, 160));
}

void AABFPFirstPersonPawn::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->SetControlRotation(FRotator(0.0f, -90.0f, 0.0f));
    }

    FActorSpawnParameters Params;
    Params.Owner = this;
    Dungeon = GetWorld()->SpawnActor<AABFPDungeonActor>(
        AABFPDungeonActor::StaticClass(), FVector::ZeroVector,
        FRotator::ZeroRotator, Params);

    bLiveMode = NetworkClient.Connect(TEXT("127.0.0.1"), 47652);
    if (bLiveMode)
    {
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 8.0f, FColor::Green,
                TEXT("Connected to local Angband host. Waiting for authoritative state..."));
    }
    else
    {
        BuildDemoSnapshot();
        const float GridCellSize = Dungeon ? Dungeon->CellSize : 400.0f;
        SetActorLocation(FVector(DemoSnapshot.PlayerX * GridCellSize,
            DemoSnapshot.PlayerY * GridCellSize, 0.0f));
        TargetPawnLocation = GetActorLocation();
        if (Dungeon) Dungeon->ApplySnapshot(DemoSnapshot);
        UpdateStatusReadout();
        if (GEngine)
            GEngine->AddOnScreenDebugMessage(-1, 12.0f, FColor::Yellow,
                TEXT("No Angband host at 127.0.0.1:47652 — running local renderer demo"));
    }
}

void AABFPFirstPersonPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (bLiveMode)
    {
        if (!NetworkClient.IsConnected())
        {
            ReconnectRetrySeconds -= DeltaSeconds;
            if (ReconnectRetrySeconds <= 0.0f)
            {
                ReconnectRetrySeconds = 2.0f;
                if (NetworkClient.Connect(TEXT("127.0.0.1"), 47652))
                {
                    bHaveAuthoritativeState = false;
                    bAwaitingServerTurn = false;
                    bInterpolatingMove = false;
                }
            }
        }

        FABFPSnapshot AuthoritativeSnapshot;
        if (NetworkClient.PollSnapshot(AuthoritativeSnapshot))
        {
            const bool bFirstState = !bHaveAuthoritativeState;
            const bool bLevelChanged = bHaveAuthoritativeState &&
                AuthoritativeSnapshot.DungeonDepth != DemoSnapshot.DungeonDepth;
            DemoSnapshot = MoveTemp(AuthoritativeSnapshot);
            if (Dungeon) Dungeon->ApplySnapshot(DemoSnapshot);

            const float GridCellSize = Dungeon ? Dungeon->CellSize : 400.0f;
            TargetPawnLocation = FVector(DemoSnapshot.PlayerX * GridCellSize,
                DemoSnapshot.PlayerY * GridCellSize, 0.0f);
            if (bFirstState || bLevelChanged)
            {
                SetActorLocation(TargetPawnLocation);
                bInterpolatingMove = false;
            }
            else if (FVector::DistSquared(GetActorLocation(), TargetPawnLocation) > 4.0f)
            {
                bInterpolatingMove = true;
            }

            if (!DemoSnapshot.bInitialState) bAwaitingServerTurn = false;
            bHaveAuthoritativeState = true;
            UpdateStatusReadout();
        }
    }

    if (!bInterpolatingMove) return;
    const FVector Current = FMath::VInterpTo(GetActorLocation(),
        TargetPawnLocation, DeltaSeconds, 12.0f);
    SetActorLocation(Current);
    if (FVector::DistSquared(Current, TargetPawnLocation) < 4.0f)
    {
        SetActorLocation(TargetPawnLocation);
        bInterpolatingMove = false;
    }
}

void AABFPFirstPersonPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    NetworkClient.Disconnect();
    Super::EndPlay(EndPlayReason);
}

void AABFPFirstPersonPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAction("MoveForward", IE_Pressed, this, &AABFPFirstPersonPawn::MoveForward);
    PlayerInputComponent->BindAction("MoveBackward", IE_Pressed, this, &AABFPFirstPersonPawn::MoveBackward);
    PlayerInputComponent->BindAction("MoveLeft", IE_Pressed, this, &AABFPFirstPersonPawn::MoveLeft);
    PlayerInputComponent->BindAction("MoveRight", IE_Pressed, this, &AABFPFirstPersonPawn::MoveRight);
    PlayerInputComponent->BindAction("WaitTurn", IE_Pressed, this, &AABFPFirstPersonPawn::WaitTurn);
    PlayerInputComponent->BindAction("GoUp", IE_Pressed, this, &AABFPFirstPersonPawn::GoUp);
    PlayerInputComponent->BindAction("GoDown", IE_Pressed, this, &AABFPFirstPersonPawn::GoDown);
    PlayerInputComponent->BindAction("AutoPickup", IE_Pressed, this, &AABFPFirstPersonPawn::AutoPickup);
    PlayerInputComponent->BindAxis("Turn", this, &AABFPFirstPersonPawn::Turn);
    PlayerInputComponent->BindAxis("LookUp", this, &AABFPFirstPersonPawn::LookUp);
}

void AABFPFirstPersonPawn::MoveForward() { MoveRelative(FVector2D(1, 0)); }
void AABFPFirstPersonPawn::MoveBackward() { MoveRelative(FVector2D(-1, 0)); }
void AABFPFirstPersonPawn::MoveLeft() { MoveRelative(FVector2D(0, -1)); }
void AABFPFirstPersonPawn::MoveRight() { MoveRelative(FVector2D(0, 1)); }

void AABFPFirstPersonPawn::SendSimpleCommand(const FString& Command)
{
    if (!bLiveMode || !bHaveAuthoritativeState || bAwaitingServerTurn) return;
    if (NetworkClient.SendCommand(Command))
        bAwaitingServerTurn = true;
}

void AABFPFirstPersonPawn::WaitTurn()
{
    if (bLiveMode)
    {
        SendSimpleCommand(TEXT("WAIT"));
        return;
    }

    ++DemoSnapshot.Turn;
    UpdateStatusReadout();
}

void AABFPFirstPersonPawn::GoUp()
{
    SendSimpleCommand(TEXT("GO_UP"));
}

void AABFPFirstPersonPawn::GoDown()
{
    SendSimpleCommand(TEXT("GO_DOWN"));
}

void AABFPFirstPersonPawn::AutoPickup()
{
    if (bLiveMode)
    {
        SendSimpleCommand(TEXT("AUTOPICKUP"));
        return;
    }

    const int32 Index = DemoSnapshot.Entities.IndexOfByPredicate(
        [this](const FABFPEntity& Entity)
        {
            return Entity.Type == EABFPEntityType::Object &&
                Entity.X == DemoSnapshot.PlayerX &&
                Entity.Y == DemoSnapshot.PlayerY;
        });
    if (Index != INDEX_NONE)
    {
        DemoSnapshot.Entities.RemoveAt(Index);
        if (Dungeon) Dungeon->ApplySnapshot(DemoSnapshot);
        UpdateStatusReadout();
    }
}

void AABFPFirstPersonPawn::Turn(float Value)
{
    AddControllerYawInput(Value);
}

void AABFPFirstPersonPawn::LookUp(float Value)
{
    AddControllerPitchInput(Value);
}

void AABFPFirstPersonPawn::MoveRelative(FVector2D LocalDirection)
{
    if (bInterpolatingMove) return;

    const float Yaw = GetControlRotation().Yaw;
    const FVector WorldDirection = FRotationMatrix(FRotator(0, Yaw, 0))
        .TransformVector(FVector(LocalDirection.X, LocalDirection.Y, 0));

    const int32 DX = FMath::Abs(WorldDirection.X) > 0.35f ?
        static_cast<int32>(FMath::Sign(WorldDirection.X)) : 0;
    const int32 DY = FMath::Abs(WorldDirection.Y) > 0.35f ?
        static_cast<int32>(FMath::Sign(WorldDirection.Y)) : 0;

    if (DX == 0 && DY == 0) return;

    if (bLiveMode)
    {
        if (!bHaveAuthoritativeState || bAwaitingServerTurn) return;
        FString Command;
        if (DX == 0 && DY < 0) Command = TEXT("MOVE_NORTH");
        else if (DX == 0 && DY > 0) Command = TEXT("MOVE_SOUTH");
        else if (DX < 0 && DY == 0) Command = TEXT("MOVE_WEST");
        else if (DX > 0 && DY == 0) Command = TEXT("MOVE_EAST");
        else if (DX < 0 && DY < 0) Command = TEXT("MOVE_NORTH_WEST");
        else if (DX > 0 && DY < 0) Command = TEXT("MOVE_NORTH_EAST");
        else if (DX < 0 && DY > 0) Command = TEXT("MOVE_SOUTH_WEST");
        else if (DX > 0 && DY > 0) Command = TEXT("MOVE_SOUTH_EAST");

        if (!Command.IsEmpty() && NetworkClient.SendCommand(Command))
            bAwaitingServerTurn = true;
        return;
    }

    TryDemoStep(DX, DY);
}

void AABFPFirstPersonPawn::BuildDemoSnapshot()
{
    DemoSnapshot = FABFPSnapshot();
    DemoSnapshot.Turn = 1;
    DemoSnapshot.DungeonDepth = 1;
    DemoSnapshot.PlayerX = 7;
    DemoSnapshot.PlayerY = 7;
    DemoSnapshot.PlayerHitPoints = 42;
    DemoSnapshot.PlayerMaxHitPoints = 42;
    DemoSnapshot.PlayerLevel = 1;
    DemoSnapshot.PlayerGold = 120;

    for (int32 Y = 0; Y < 15; ++Y)
    {
        for (int32 X = 0; X < 15; ++X)
        {
            FABFPCell Cell;
            Cell.X = X;
            Cell.Y = Y;
            Cell.Terrain = (X == 0 || Y == 0 || X == 14 || Y == 14) ?
                EABFPTerrain::Wall : EABFPTerrain::Floor;
            Cell.bVisible = true;
            Cell.bLit = (X > 4 && X < 10 && Y > 4 && Y < 10);

            if (X == 10 && Y > 2 && Y < 12)
                Cell.Terrain = EABFPTerrain::Wall;
            if (X == 10 && Y == 7)
                Cell.Terrain = EABFPTerrain::Door;
            if (X == 4 && Y == 4)
                Cell.Terrain = EABFPTerrain::StairsDown;

            DemoSnapshot.Cells.Add(Cell);
        }
    }

    FABFPEntity Goblin;
    Goblin.Id = 0x1000000000000001ull;
    Goblin.X = 7;
    Goblin.Y = 3;
    Goblin.Type = EABFPEntityType::Monster;
    Goblin.KindId = 1;
    Goblin.HitPoints = 8;
    Goblin.MaxHitPoints = 12;
    DemoSnapshot.Entities.Add(Goblin);

    FABFPEntity Wraith;
    Wraith.Id = 0x1000000000000002ull;
    Wraith.X = 8;
    Wraith.Y = 9;
    Wraith.Type = EABFPEntityType::Monster;
    Wraith.KindId = 2;
    Wraith.HitPoints = 22;
    Wraith.MaxHitPoints = 22;
    DemoSnapshot.Entities.Add(Wraith);

    FABFPEntity Potion;
    Potion.Id = 0x2000000000000001ull;
    Potion.X = 8;
    Potion.Y = 6;
    Potion.Type = EABFPEntityType::Object;
    Potion.KindId = 1;
    Potion.Quantity = 1;
    DemoSnapshot.Entities.Add(Potion);
}

void AABFPFirstPersonPawn::UpdateStatusReadout()
{
    if (!StatusReadout) return;

    const FString Mode = bLiveMode ? TEXT("ANGBAND LIVE") : TEXT("RENDERER DEMO");
    const FString Health = DemoSnapshot.PlayerMaxHitPoints > 0 ?
        FString::Printf(TEXT("HP %d/%d"), DemoSnapshot.PlayerHitPoints,
            DemoSnapshot.PlayerMaxHitPoints) : TEXT("HP --");
    StatusReadout->SetText(FText::FromString(FString::Printf(
        TEXT("%s\nDepth %d  Turn %lld\n%s  Level %d  Gold %d"),
        *Mode, DemoSnapshot.DungeonDepth, DemoSnapshot.Turn, *Health,
        DemoSnapshot.PlayerLevel, DemoSnapshot.PlayerGold)));
}

void AABFPFirstPersonPawn::TryDemoStep(int32 DX, int32 DY)
{
    const int32 TargetX = DemoSnapshot.PlayerX + DX;
    const int32 TargetY = DemoSnapshot.PlayerY + DY;

    const FABFPCell* Target = DemoSnapshot.Cells.FindByPredicate(
        [TargetX, TargetY](const FABFPCell& Cell)
        {
            return Cell.X == TargetX && Cell.Y == TargetY;
        });

    if (!Target || Target->Terrain == EABFPTerrain::Wall ||
        Target->Terrain == EABFPTerrain::Unknown) return;

    DemoSnapshot.PlayerX = TargetX;
    DemoSnapshot.PlayerY = TargetY;
    ++DemoSnapshot.Turn;

    if (Dungeon) Dungeon->ApplySnapshot(DemoSnapshot);
    UpdateStatusReadout();
    const float GridCellSize = Dungeon ? Dungeon->CellSize : 400.0f;
    TargetPawnLocation = FVector(TargetX * GridCellSize, TargetY * GridCellSize, 0.0f);
    bInterpolatingMove = true;

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(1, 1.5f, FColor::Green,
            FString::Printf(TEXT("Demo turn %lld | depth %d"),
                DemoSnapshot.Turn, DemoSnapshot.DungeonDepth));
    }
}
