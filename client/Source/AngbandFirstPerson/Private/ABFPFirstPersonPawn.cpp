#include "ABFPFirstPersonPawn.h"

#include "ABFPDungeonActor.h"
#include "ABFPVisualCatalog.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AABFPFirstPersonPawn::AABFPFirstPersonPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;
    bUseControllerRotationYaw = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    Camera->SetupAttachment(SceneRoot);
    Camera->SetRelativeLocation(FVector(0.0f, 0.0f, CameraBaseHeight));
    Camera->bUsePawnControlRotation = true;

    Lantern = CreateDefaultSubobject<UPointLightComponent>(TEXT("Lantern"));
    Lantern->SetupAttachment(Camera);
    Lantern->SetRelativeLocation(FVector(28.0f, 18.0f, -12.0f));
    Lantern->SetIntensity(16000.0f);
    Lantern->SetAttenuationRadius(2400.0f);
    Lantern->SetSourceRadius(16.0f);
    Lantern->SetLightColor(FLinearColor(1.0f, 0.78f, 0.52f));
    Lantern->SetCastShadows(true);

    ViewModelMount = CreateDefaultSubobject<USceneComponent>(TEXT("ViewModelMount"));
    ViewModelMount->SetupAttachment(Camera);
    ViewModelMount->SetRelativeLocation(FVector(42.0f, 24.0f, -24.0f));
}

void AABFPFirstPersonPawn::BeginPlay()
{
    Super::BeginPlay();

    TargetControlYaw = -90.0f;
    CurrentControlYaw = -90.0f;
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->SetControlRotation(FRotator(0.0f, CurrentControlYaw, 0.0f));
    }

    if (!VisualCatalogAsset.IsNull())
    {
        LoadedVisualCatalog = VisualCatalogAsset.LoadSynchronous();
        if (LoadedVisualCatalog)
        {
            UE_LOG(LogTemp, Log, TEXT("ABFP: Loaded visual catalog %s"), *LoadedVisualCatalog->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ABFP: VisualCatalogAsset %s not found; using procedural fallback."),
                *VisualCatalogAsset.ToString());
        }
    }

    FActorSpawnParameters Params;
    Params.Owner = this;
    Dungeon = GetWorld()->SpawnActor<AABFPDungeonActor>(
        AABFPDungeonActor::StaticClass(), FVector::ZeroVector,
        FRotator::ZeroRotator, Params);

    if (Dungeon && LoadedVisualCatalog)
    {
        Dungeon->SetVisualCatalog(LoadedVisualCatalog);
    }

    bLiveMode = NetworkClient.Connect(HostAddress, HostPort);
    if (bLiveMode)
    {
        AddStatusMessage(FString::Printf(TEXT("Connected to Angband host %s:%d. Waiting for state..."),
            *HostAddress, HostPort));
    }
    else
    {
        BuildDemoSnapshot();
        const float GridCellSize = Dungeon ? Dungeon->CellSize : 400.0f;
        SetActorLocation(FVector(CurrentSnapshot.PlayerX * GridCellSize,
            CurrentSnapshot.PlayerY * GridCellSize, 0.0f));
        TargetPawnLocation = GetActorLocation();
        if (Dungeon) Dungeon->ApplySnapshot(CurrentSnapshot);
        AppendMessages(CurrentSnapshot.Messages);
    }
}

void AABFPFirstPersonPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    LanternElapsed += DeltaSeconds;
    PlayerDamageFlashSeconds = FMath::Max(0.0f, PlayerDamageFlashSeconds - DeltaSeconds);
    AttackSwingSeconds = FMath::Max(0.0f, AttackSwingSeconds - DeltaSeconds);

    /* Smoothly interpolate discrete 45-degree tank turns. */
    if (!FMath::IsNearlyEqual(CurrentControlYaw, TargetControlYaw, 0.1f))
    {
        const FRotator InterpRot = FMath::RInterpTo(
            FRotator(0.0f, CurrentControlYaw, 0.0f),
            FRotator(0.0f, TargetControlYaw, 0.0f),
            DeltaSeconds, 14.0f);
        CurrentControlYaw = InterpRot.Yaw;
        if (FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentControlYaw, TargetControlYaw)) < 0.25f)
        {
            CurrentControlYaw = TargetControlYaw;
        }
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            PC->SetControlRotation(FRotator(0.0f, CurrentControlYaw, 0.0f));
        }
    }

    if (bAwaitingServerTurn)
    {
        AwaitingTurnSeconds += DeltaSeconds;
        if (AwaitingTurnSeconds > 5.0f)
        {
            bAwaitingServerTurn = false;
            AwaitingTurnSeconds = 0.0f;
        }
    }

    if (!NetworkClient.IsConnected())
    {
        if (bLiveMode && bHaveAuthoritativeState)
        {
            bHaveAuthoritativeState = false;
            bAwaitingServerTurn = false;
            AwaitingTurnSeconds = 0.0f;
            AddStatusMessage(TEXT("Disconnected from Angband host. Reconnecting..."));
        }
        ReconnectRetrySeconds -= DeltaSeconds;
        if (ReconnectRetrySeconds <= 0.0f)
        {
            ReconnectRetrySeconds = 2.0f;
            if (NetworkClient.Connect(HostAddress, HostPort))
            {
                bLiveMode = true;
                bHaveAuthoritativeState = false;
                bAwaitingServerTurn = false;
                AwaitingTurnSeconds = 0.0f;
                bInterpolatingMove = false;
                AddStatusMessage(FString::Printf(TEXT("Connected to Angband host %s:%d"),
                    *HostAddress, HostPort));
            }
        }
    }
    else
    {
        bLiveMode = true;
        FABFPSnapshot AuthoritativeSnapshot;
        if (NetworkClient.PollSnapshot(AuthoritativeSnapshot))
        {
            for (const FABFPEvent& Event : AuthoritativeSnapshot.Events)
            {
                if (Event.Type == EABFPEventType::Damage && Event.TargetId == 1)
                {
                    PlayerDamageFlashSeconds = 0.35f;
                    OnPlayerDamaged(Event.Value);
                    if (LoadedVisualCatalog && LoadedVisualCatalog->PlayerHitSound)
                    {
                        UGameplayStatics::PlaySound2D(this, LoadedVisualCatalog->PlayerHitSound);
                    }
                }
                else if (Event.Type == EABFPEventType::Attack && Event.SubjectId == 1)
                {
                    PlayPlayerAttackSwing();
                }
            }

            const bool bFirstState = !bHaveAuthoritativeState;
            const bool bLevelChanged = bHaveAuthoritativeState &&
                AuthoritativeSnapshot.DungeonDepth != CurrentSnapshot.DungeonDepth;
            AppendMessages(AuthoritativeSnapshot.Messages);
            CurrentSnapshot = MoveTemp(AuthoritativeSnapshot);
            if (Dungeon) Dungeon->ApplySnapshot(CurrentSnapshot);

            const float GridCellSize = Dungeon ? Dungeon->CellSize : 400.0f;
            TargetPawnLocation = FVector(CurrentSnapshot.PlayerX * GridCellSize,
                CurrentSnapshot.PlayerY * GridCellSize, 0.0f);
            if (bFirstState || bLevelChanged)
            {
                SetActorLocation(TargetPawnLocation);
                bInterpolatingMove = false;
            }
            else if (FVector::DistSquared(GetActorLocation(), TargetPawnLocation) > 4.0f)
            {
                bInterpolatingMove = true;
                if (LoadedVisualCatalog && LoadedVisualCatalog->FootstepSound)
                {
                    UGameplayStatics::PlaySound2D(this, LoadedVisualCatalog->FootstepSound);
                }
            }

            if (!CurrentSnapshot.bInitialState)
            {
                bAwaitingServerTurn = false;
                AwaitingTurnSeconds = 0.0f;
            }
            bHaveAuthoritativeState = true;
        }
    }

    if (Lantern)
    {
        const float Flicker = 1.0f + FMath::Sin(LanternElapsed * 6.5f) * 0.035f +
            FMath::Sin(LanternElapsed * 14.2f) * 0.02f;
        Lantern->SetIntensity(16000.0f * Flicker);
        Lantern->SetLightColor(PlayerDamageFlashSeconds > 0.0f ?
            FLinearColor(1.0f, 0.08f, 0.04f) :
            FLinearColor(1.0f, 0.78f, 0.52f));
    }

    if (ViewModelMount)
    {
        const float SwingAlpha = FMath::Sin((AttackSwingSeconds / 0.24f) * PI);
        const float Sway = FMath::Sin(LanternElapsed * 2.0f) * 0.8f;
        ViewModelMount->SetRelativeLocation(FVector(
            42.0f + SwingAlpha * 16.0f,
            24.0f - SwingAlpha * 10.0f,
            -24.0f + Sway - SwingAlpha * 6.0f));
        ViewModelMount->SetRelativeRotation(FRotator(-SwingAlpha * 28.0f, -SwingAlpha * 18.0f, 0.0f));
    }

    if (bInterpolatingMove)
    {
        StepBobElapsed += DeltaSeconds;
        const FVector Current = FMath::VInterpTo(GetActorLocation(),
            TargetPawnLocation, DeltaSeconds, 12.0f);
        SetActorLocation(Current);
        if (Camera)
        {
            const float BobZ = FMath::Abs(FMath::Sin(StepBobElapsed * 12.0f)) * 5.0f;
            Camera->SetRelativeLocation(FVector(0.0f, 0.0f, CameraBaseHeight + BobZ));
        }
        if (FVector::DistSquared(Current, TargetPawnLocation) < 4.0f)
        {
            SetActorLocation(TargetPawnLocation);
            bInterpolatingMove = false;
            StepBobElapsed = 0.0f;
            if (Camera) Camera->SetRelativeLocation(FVector(0.0f, 0.0f, CameraBaseHeight));
        }
    }
    else if (Camera)
    {
        const FVector CamLoc = Camera->GetRelativeLocation();
        if (!FMath::IsNearlyEqual(CamLoc.Z, CameraBaseHeight, 0.1f))
        {
            Camera->SetRelativeLocation(FVector(0.0f, 0.0f,
                FMath::FInterpTo(CamLoc.Z, CameraBaseHeight, DeltaSeconds, 10.0f)));
        }
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
    PlayerInputComponent->BindAction("TurnLeft", IE_Pressed, this, &AABFPFirstPersonPawn::TurnLeft);
    PlayerInputComponent->BindAction("TurnRight", IE_Pressed, this, &AABFPFirstPersonPawn::TurnRight);
    PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &AABFPFirstPersonPawn::InteractForward);
    PlayerInputComponent->BindAction("WaitTurn", IE_Pressed, this, &AABFPFirstPersonPawn::WaitTurn);
    PlayerInputComponent->BindAction("GoUp", IE_Pressed, this, &AABFPFirstPersonPawn::GoUp);
    PlayerInputComponent->BindAction("GoDown", IE_Pressed, this, &AABFPFirstPersonPawn::GoDown);
    PlayerInputComponent->BindAction("AutoPickup", IE_Pressed, this, &AABFPFirstPersonPawn::AutoPickup);
    PlayerInputComponent->BindAction("ToggleHelp", IE_Pressed, this, &AABFPFirstPersonPawn::ToggleHelp);
}

FIntPoint AABFPFirstPersonPawn::GetFacingGridDelta() const
{
    const FVector WorldDirection = FRotationMatrix(FRotator(0.0f, TargetControlYaw, 0.0f))
        .TransformVector(FVector(1.0f, 0.0f, 0.0f));
    const int32 DX = FMath::Abs(WorldDirection.X) > 0.35f ?
        static_cast<int32>(FMath::Sign(WorldDirection.X)) : 0;
    const int32 DY = FMath::Abs(WorldDirection.Y) > 0.35f ?
        static_cast<int32>(FMath::Sign(WorldDirection.Y)) : 0;
    return FIntPoint(DX, DY);
}

FString AABFPFirstPersonPawn::DirectionCommandName(const TCHAR* Prefix, int32 DX, int32 DY)
{
    if (DX == 0 && DY < 0) return FString::Printf(TEXT("%s_NORTH"), Prefix);
    if (DX == 0 && DY > 0) return FString::Printf(TEXT("%s_SOUTH"), Prefix);
    if (DX < 0 && DY == 0) return FString::Printf(TEXT("%s_WEST"), Prefix);
    if (DX > 0 && DY == 0) return FString::Printf(TEXT("%s_EAST"), Prefix);
    if (DX < 0 && DY < 0) return FString::Printf(TEXT("%s_NORTH_WEST"), Prefix);
    if (DX > 0 && DY < 0) return FString::Printf(TEXT("%s_NORTH_EAST"), Prefix);
    if (DX < 0 && DY > 0) return FString::Printf(TEXT("%s_SOUTH_WEST"), Prefix);
    if (DX > 0 && DY > 0) return FString::Printf(TEXT("%s_SOUTH_EAST"), Prefix);
    return FString();
}

void AABFPFirstPersonPawn::PlayPlayerAttackSwing()
{
    AttackSwingSeconds = 0.24f;
    if (LoadedVisualCatalog && LoadedVisualCatalog->AttackSwingSound)
    {
        UGameplayStatics::PlaySound2D(this, LoadedVisualCatalog->AttackSwingSound);
    }
    OnPlayerAttackSwing();
}

void AABFPFirstPersonPawn::AddStatusMessage(const FString& Text)
{
    if (Text.IsEmpty()) return;
    FABFPMessage Msg;
    Msg.Type = 0;
    Msg.Text = Text;
    AppendMessages({ Msg });
}

void AABFPFirstPersonPawn::AppendMessages(const TArray<FABFPMessage>& NewMessages)
{
    for (const FABFPMessage& Msg : NewMessages)
    {
        if (Msg.Text.IsEmpty()) continue;
        MessageHistory.Add(Msg);
    }
    constexpr int32 MaxHistory = 24;
    if (MessageHistory.Num() > MaxHistory)
    {
        MessageHistory.RemoveAt(0, MessageHistory.Num() - MaxHistory);
    }
}

void AABFPFirstPersonPawn::MoveForward()
{
    MoveRelative(FVector2D(1.0f, 0.0f));
}

void AABFPFirstPersonPawn::MoveBackward()
{
    MoveRelative(FVector2D(-1.0f, 0.0f));
}

void AABFPFirstPersonPawn::TurnLeft()
{
    TargetControlYaw = FRotator::NormalizeAxis(TargetControlYaw - 45.0f);
}

void AABFPFirstPersonPawn::TurnRight()
{
    TargetControlYaw = FRotator::NormalizeAxis(TargetControlYaw + 45.0f);
}

void AABFPFirstPersonPawn::ToggleHelp()
{
    bShowHelpOverlay = !bShowHelpOverlay;
}

void AABFPFirstPersonPawn::SendSimpleCommand(const FString& Command)
{
    if (!bLiveMode || !bHaveAuthoritativeState || bAwaitingServerTurn || Command.IsEmpty()) return;
    if (NetworkClient.SendCommand(Command))
    {
        bAwaitingServerTurn = true;
        AwaitingTurnSeconds = 0.0f;
    }
}

void AABFPFirstPersonPawn::InteractForward()
{
    const FIntPoint Delta = GetFacingGridDelta();
    if (Delta.X == 0 && Delta.Y == 0) return;

    if (bLiveMode)
    {
        SendSimpleCommand(DirectionCommandName(TEXT("ALTER"), Delta.X, Delta.Y));
        return;
    }

    TryDemoInteract(Delta.X, Delta.Y);
}

void AABFPFirstPersonPawn::WaitTurn()
{
    if (bLiveMode)
    {
        SendSimpleCommand(TEXT("WAIT"));
        return;
    }

    ++CurrentSnapshot.Turn;
}

void AABFPFirstPersonPawn::GoUp()
{
    if (bLiveMode)
    {
        SendSimpleCommand(TEXT("GO_UP"));
        return;
    }

    if (CurrentSnapshot.DungeonDepth > 0)
    {
        --CurrentSnapshot.DungeonDepth;
        ++CurrentSnapshot.Turn;
        if (Dungeon) Dungeon->ApplySnapshot(CurrentSnapshot);
    }
}

void AABFPFirstPersonPawn::GoDown()
{
    if (bLiveMode)
    {
        SendSimpleCommand(TEXT("GO_DOWN"));
        return;
    }

    ++CurrentSnapshot.DungeonDepth;
    ++CurrentSnapshot.Turn;
    if (Dungeon) Dungeon->ApplySnapshot(CurrentSnapshot);
}

void AABFPFirstPersonPawn::AutoPickup()
{
    if (bLiveMode)
    {
        SendSimpleCommand(TEXT("AUTOPICKUP"));
        return;
    }

    const int32 Index = CurrentSnapshot.Entities.IndexOfByPredicate(
        [this](const FABFPEntity& Entity)
        {
            return Entity.Type == EABFPEntityType::Object &&
                Entity.X == CurrentSnapshot.PlayerX &&
                Entity.Y == CurrentSnapshot.PlayerY;
        });
    if (Index != INDEX_NONE)
    {
        AddStatusMessage(FString::Printf(TEXT("You pick up %s."), *CurrentSnapshot.Entities[Index].Name));
        CurrentSnapshot.Entities.RemoveAt(Index);
        if (Dungeon) Dungeon->ApplySnapshot(CurrentSnapshot);
    }
}

void AABFPFirstPersonPawn::MoveRelative(FVector2D LocalDirection)
{
    if (bInterpolatingMove) return;

    const FVector WorldDirection = FRotationMatrix(FRotator(0.0f, TargetControlYaw, 0.0f))
        .TransformVector(FVector(LocalDirection.X, LocalDirection.Y, 0.0f));

    const int32 DX = FMath::Abs(WorldDirection.X) > 0.35f ?
        static_cast<int32>(FMath::Sign(WorldDirection.X)) : 0;
    const int32 DY = FMath::Abs(WorldDirection.Y) > 0.35f ?
        static_cast<int32>(FMath::Sign(WorldDirection.Y)) : 0;

    if (DX == 0 && DY == 0) return;

    if (bLiveMode)
    {
        SendSimpleCommand(DirectionCommandName(TEXT("MOVE"), DX, DY));
        return;
    }

    TryDemoStep(DX, DY);
}

void AABFPFirstPersonPawn::BuildDemoSnapshot()
{
    CurrentSnapshot = FABFPSnapshot();
    CurrentSnapshot.Turn = 1;
    CurrentSnapshot.DungeonDepth = 1;
    CurrentSnapshot.PlayerX = 7;
    CurrentSnapshot.PlayerY = 7;
    CurrentSnapshot.PlayerHitPoints = 42;
    CurrentSnapshot.PlayerMaxHitPoints = 42;
    CurrentSnapshot.PlayerMana = 14;
    CurrentSnapshot.PlayerMaxMana = 14;
    CurrentSnapshot.PlayerLevel = 1;
    CurrentSnapshot.PlayerGold = 120;

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
            Cell.bLit = (X >= 2 && X <= 9 && Y >= 2 && Y <= 10);

            if ((X == 0 && (Y == 5 || Y == 6)) || (Y == 0 && (X == 6 || X == 8)))
                Cell.Terrain = EABFPTerrain::Vein;

            if (X == 10 && Y > 1 && Y < 13)
                Cell.Terrain = EABFPTerrain::Wall;
            if (X == 10 && Y == 7)
                Cell.Terrain = EABFPTerrain::Door;
            if (X == 10 && Y == 4)
                Cell.Terrain = EABFPTerrain::DoorOpen;
            if (X == 4 && Y == 4)
                Cell.Terrain = EABFPTerrain::StairsDown;
            if (X == 3 && Y == 9)
                Cell.Terrain = EABFPTerrain::Rubble;
            if (X == 6 && Y == 5)
                Cell.Terrain = EABFPTerrain::Trap;

            CurrentSnapshot.Cells.Add(Cell);
        }
    }

    FABFPEntity Goblin;
    Goblin.Id = 0x1000000000000001ull;
    Goblin.X = 7;
    Goblin.Y = 3;
    Goblin.Type = EABFPEntityType::Monster;
    Goblin.KindId = 1;
    Goblin.Name = TEXT("goblin");
    Goblin.HitPoints = 8;
    Goblin.MaxHitPoints = 12;
    CurrentSnapshot.Entities.Add(Goblin);

    FABFPEntity Skeleton;
    Skeleton.Id = 0x1000000000000002ull;
    Skeleton.X = 12;
    Skeleton.Y = 4;
    Skeleton.Type = EABFPEntityType::Monster;
    Skeleton.KindId = 2;
    Skeleton.Name = TEXT("skeleton");
    Skeleton.HitPoints = 16;
    Skeleton.MaxHitPoints = 16;
    CurrentSnapshot.Entities.Add(Skeleton);

    FABFPEntity Wraith;
    Wraith.Id = 0x1000000000000003ull;
    Wraith.X = 8;
    Wraith.Y = 10;
    Wraith.Type = EABFPEntityType::Monster;
    Wraith.KindId = 3;
    Wraith.Name = TEXT("wraith");
    Wraith.HitPoints = 22;
    Wraith.MaxHitPoints = 22;
    CurrentSnapshot.Entities.Add(Wraith);

    FABFPEntity Potion;
    Potion.Id = 0x2000000000000001ull;
    Potion.X = 8;
    Potion.Y = 6;
    Potion.Type = EABFPEntityType::Object;
    Potion.KindId = 1;
    Potion.Name = TEXT("Potion of Healing");
    Potion.Quantity = 1;
    CurrentSnapshot.Entities.Add(Potion);

    FABFPEntity Sword;
    Sword.Id = 0x2000000000000002ull;
    Sword.X = 5;
    Sword.Y = 7;
    Sword.Type = EABFPEntityType::Object;
    Sword.KindId = 2;
    Sword.Name = TEXT("Short Sword");
    Sword.Quantity = 1;
    CurrentSnapshot.Entities.Add(Sword);

    FABFPMessage Welcome;
    Welcome.Text = FString::Printf(TEXT("No Angband host at %s:%d - running offline renderer showcase."),
        *HostAddress, HostPort);
    CurrentSnapshot.Messages.Add(MoveTemp(Welcome));

    FABFPMessage Hint;
    Hint.Text = TEXT("Press [F] facing a door to open/close it, or connect the native Angband host.");
    CurrentSnapshot.Messages.Add(MoveTemp(Hint));
}

void AABFPFirstPersonPawn::TryDemoInteract(int32 DX, int32 DY)
{
    const int32 TargetX = CurrentSnapshot.PlayerX + DX;
    const int32 TargetY = CurrentSnapshot.PlayerY + DY;
    FABFPCell* Target = CurrentSnapshot.Cells.FindByPredicate(
        [TargetX, TargetY](const FABFPCell& Cell)
        {
            return Cell.X == TargetX && Cell.Y == TargetY;
        });
    if (!Target) return;

    if (Target->Terrain == EABFPTerrain::Door)
    {
        Target->Terrain = EABFPTerrain::DoorOpen;
        ++CurrentSnapshot.Turn;
        AddStatusMessage(TEXT("You open the door."));
        if (Dungeon) Dungeon->ApplySnapshot(CurrentSnapshot);
    }
    else if (Target->Terrain == EABFPTerrain::DoorOpen)
    {
        Target->Terrain = EABFPTerrain::Door;
        ++CurrentSnapshot.Turn;
        AddStatusMessage(TEXT("You close the door."));
        if (Dungeon) Dungeon->ApplySnapshot(CurrentSnapshot);
    }
    else if (Target->Terrain == EABFPTerrain::Trap)
    {
        Target->Terrain = EABFPTerrain::Floor;
        ++CurrentSnapshot.Turn;
        AddStatusMessage(TEXT("You disarm the trap."));
        if (Dungeon) Dungeon->ApplySnapshot(CurrentSnapshot);
    }
}

void AABFPFirstPersonPawn::TryDemoStep(int32 DX, int32 DY)
{
    const int32 TargetX = CurrentSnapshot.PlayerX + DX;
    const int32 TargetY = CurrentSnapshot.PlayerY + DY;

    FABFPCell* Target = CurrentSnapshot.Cells.FindByPredicate(
        [TargetX, TargetY](const FABFPCell& Cell)
        {
            return Cell.X == TargetX && Cell.Y == TargetY;
        });

    if (!Target || Target->Terrain == EABFPTerrain::Wall ||
        Target->Terrain == EABFPTerrain::Vein ||
        Target->Terrain == EABFPTerrain::Rubble ||
        Target->Terrain == EABFPTerrain::Unknown) return;

    if (Target->Terrain == EABFPTerrain::Door)
    {
        TryDemoInteract(DX, DY);
        return;
    }

    CurrentSnapshot.PlayerX = TargetX;
    CurrentSnapshot.PlayerY = TargetY;
    ++CurrentSnapshot.Turn;

    if (Dungeon) Dungeon->ApplySnapshot(CurrentSnapshot);
    const float GridCellSize = Dungeon ? Dungeon->CellSize : 400.0f;
    TargetPawnLocation = FVector(TargetX * GridCellSize, TargetY * GridCellSize, 0.0f);
    bInterpolatingMove = true;
}
