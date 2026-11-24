#include "ABFPFirstPersonPawn.h"

#include "ABFPDungeonActor.h"
#include "ABFPVisualCatalog.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
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
    Lantern->SetRelativeLocation(FVector(34.0f, -20.0f, -10.0f));
    Lantern->SetUseInverseSquaredFalloff(false);
    Lantern->SetLightFalloffExponent(1.65f);
    Lantern->SetIntensity(7.5f);
    Lantern->SetAttenuationRadius(2600.0f);
    Lantern->SetSourceRadius(16.0f);
    Lantern->SetLightColor(FLinearColor(1.0f, 0.80f, 0.56f));
    Lantern->SetCastShadows(true);

    ViewModelMount = CreateDefaultSubobject<USceneComponent>(TEXT("ViewModelMount"));
    ViewModelMount->SetupAttachment(Camera);
    ViewModelMount->SetRelativeLocation(FVector(44.0f, 22.0f, -22.0f));

    HeldWeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldWeaponMesh"));
    HeldWeaponMesh->SetupAttachment(ViewModelMount);
    HeldWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeldWeaponMesh->SetCastShadow(false);
    HeldWeaponMesh->SetVisibility(false);

    LanternMount = CreateDefaultSubobject<USceneComponent>(TEXT("LanternMount"));
    LanternMount->SetupAttachment(Camera);
    LanternMount->SetRelativeLocation(FVector(44.0f, -22.0f, -24.0f));

    HeldLanternMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldLanternMesh"));
    HeldLanternMesh->SetupAttachment(LanternMount);
    HeldLanternMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeldLanternMesh->SetCastShadow(false);
    HeldLanternMesh->SetVisibility(false);
}

void AABFPFirstPersonPawn::ConfigureFirstPersonViewModels()
{
    if (!LoadedVisualCatalog) return;

    if (HeldLanternMesh && LoadedVisualCatalog->FirstPersonLantern.IsValid())
    {
        const FABFPMeshVariant& Variant = LoadedVisualCatalog->FirstPersonLantern;
        HeldLanternMesh->SetStaticMesh(Variant.Mesh);
        if (Variant.MaterialOverride)
        {
            HeldLanternMesh->SetMaterial(0, Variant.MaterialOverride);
        }
        HeldLanternMesh->SetRelativeLocation(Variant.LocationOffset);
        HeldLanternMesh->SetRelativeRotation(Variant.RotationOffset);
        HeldLanternMesh->SetRelativeScale3D(Variant.Scale);
        HeldLanternMesh->SetVisibility(true);
    }

    if (HeldWeaponMesh && LoadedVisualCatalog->FirstPersonWeapon.IsValid())
    {
        const FABFPMeshVariant& Variant = LoadedVisualCatalog->FirstPersonWeapon;
        HeldWeaponMesh->SetStaticMesh(Variant.Mesh);
        if (Variant.MaterialOverride)
        {
            HeldWeaponMesh->SetMaterial(0, Variant.MaterialOverride);
        }
        HeldWeaponMesh->SetRelativeLocation(Variant.LocationOffset);
        HeldWeaponMesh->SetRelativeRotation(Variant.RotationOffset);
        HeldWeaponMesh->SetRelativeScale3D(Variant.Scale);
        HeldWeaponMesh->SetVisibility(true);
    }
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

    ConfigureFirstPersonViewModels();

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

            if (!CurrentSnapshot.Shop.bActive &&
                (ActiveMenu == EABFPMenuMode::ShopBuy || ActiveMenu == EABFPMenuMode::ShopSell))
            {
                ActiveMenu = EABFPMenuMode::None;
                SelectedMenuIndex = 0;
            }
            ClampSelectedMenuIndex();

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
        Lantern->SetIntensity(7.5f * Flicker);
        Lantern->SetLightColor(PlayerDamageFlashSeconds > 0.0f ?
            FLinearColor(1.0f, 0.08f, 0.04f) :
            FLinearColor(1.0f, 0.80f, 0.56f));
    }

    if (ViewModelMount)
    {
        const float SwingAlpha = FMath::Sin((AttackSwingSeconds / 0.24f) * PI);
        const float Sway = FMath::Sin(LanternElapsed * 2.0f) * 0.9f;
        const float WalkBob = bInterpolatingMove ? FMath::Sin(StepBobElapsed * 12.0f) * 1.8f : 0.0f;
        ViewModelMount->SetRelativeLocation(FVector(
            44.0f + SwingAlpha * 18.0f,
            22.0f - SwingAlpha * 12.0f,
            -22.0f + Sway + WalkBob - SwingAlpha * 8.0f));
        ViewModelMount->SetRelativeRotation(FRotator(-SwingAlpha * 32.0f, -SwingAlpha * 20.0f, SwingAlpha * 8.0f));
    }

    if (LanternMount)
    {
        const float SwayZ = FMath::Sin(LanternElapsed * 1.7f + 1.1f) * 0.9f;
        const float SwayRoll = FMath::Sin(LanternElapsed * 1.4f) * 2.2f;
        const float WalkBob = bInterpolatingMove ? FMath::Sin(StepBobElapsed * 12.0f + PI) * 1.6f : 0.0f;
        LanternMount->SetRelativeLocation(FVector(44.0f, -22.0f, -24.0f + SwayZ + WalkBob));
        LanternMount->SetRelativeRotation(FRotator(0.0f, 0.0f, SwayRoll));
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
    PlayerInputComponent->BindAction("ToggleMinimap", IE_Pressed, this, &AABFPFirstPersonPawn::ToggleMinimap);
    PlayerInputComponent->BindAction("ToggleInventory", IE_Pressed, this, &AABFPFirstPersonPawn::ToggleInventory);
    PlayerInputComponent->BindAction("ToggleEquipment", IE_Pressed, this, &AABFPFirstPersonPawn::ToggleEquipment);
    PlayerInputComponent->BindAction("ToggleShop", IE_Pressed, this, &AABFPFirstPersonPawn::ToggleShop);
    PlayerInputComponent->BindAction("QuickQuaff", IE_Pressed, this, &AABFPFirstPersonPawn::QuickQuaff);
    PlayerInputComponent->BindAction("QuickEat", IE_Pressed, this, &AABFPFirstPersonPawn::QuickEat);
    PlayerInputComponent->BindAction("RestTurn", IE_Pressed, this, &AABFPFirstPersonPawn::RestTurn);
    PlayerInputComponent->BindAction("MenuConfirm", IE_Pressed, this, &AABFPFirstPersonPawn::MenuConfirm);
    PlayerInputComponent->BindAction("MenuUse", IE_Pressed, this, &AABFPFirstPersonPawn::MenuUseSelected);
    PlayerInputComponent->BindAction("MenuWield", IE_Pressed, this, &AABFPFirstPersonPawn::MenuWieldSelected);
    PlayerInputComponent->BindAction("MenuTakeOff", IE_Pressed, this, &AABFPFirstPersonPawn::MenuTakeOffSelected);
    PlayerInputComponent->BindAction("MenuDrop", IE_Pressed, this, &AABFPFirstPersonPawn::MenuDropSelected);
    PlayerInputComponent->BindAction("MenuRefill", IE_Pressed, this, &AABFPFirstPersonPawn::MenuRefillSelected);
    PlayerInputComponent->BindAction("MenuTab", IE_Pressed, this, &AABFPFirstPersonPawn::MenuTab);
    PlayerInputComponent->BindAction("MenuClose", IE_Pressed, this, &AABFPFirstPersonPawn::MenuClose);
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

int32 AABFPFirstPersonPawn::GetFacingKeypadDirection() const
{
    const FIntPoint D = GetFacingGridDelta();
    if (D.X == 0 && D.Y < 0) return 8;
    if (D.X == 0 && D.Y > 0) return 2;
    if (D.X < 0 && D.Y == 0) return 4;
    if (D.X > 0 && D.Y == 0) return 6;
    if (D.X < 0 && D.Y < 0) return 7;
    if (D.X > 0 && D.Y < 0) return 9;
    if (D.X < 0 && D.Y > 0) return 1;
    if (D.X > 0 && D.Y > 0) return 3;
    return 5;
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

TArray<int32> AABFPFirstPersonPawn::GetMenuInventoryIndices(bool bEquippedOnly) const
{
    TArray<int32> Result;
    for (int32 i = 0; i < CurrentSnapshot.Inventory.Num(); ++i)
    {
        if (CurrentSnapshot.Inventory[i].bEquipped == bEquippedOnly)
        {
            Result.Add(i);
        }
    }
    return Result;
}

int32 AABFPFirstPersonPawn::GetActiveMenuItemCount() const
{
    switch (ActiveMenu)
    {
    case EABFPMenuMode::Inventory:
    case EABFPMenuMode::ShopSell:
        return GetMenuInventoryIndices(false).Num();
    case EABFPMenuMode::Equipment:
        return GetMenuInventoryIndices(true).Num();
    case EABFPMenuMode::ShopBuy:
        return CurrentSnapshot.Shop.Items.Num();
    default:
        return 0;
    }
}

void AABFPFirstPersonPawn::ClampSelectedMenuIndex()
{
    const int32 Count = GetActiveMenuItemCount();
    if (Count <= 0)
    {
        SelectedMenuIndex = 0;
    }
    else
    {
        SelectedMenuIndex = FMath::Clamp(SelectedMenuIndex, 0, Count - 1);
    }
}

void AABFPFirstPersonPawn::MoveMenuSelection(int32 Delta)
{
    const int32 Count = GetActiveMenuItemCount();
    if (Count <= 0)
    {
        SelectedMenuIndex = 0;
        return;
    }
    SelectedMenuIndex = (SelectedMenuIndex + Delta + Count) % Count;
}

void AABFPFirstPersonPawn::MoveForward()
{
    if (ActiveMenu != EABFPMenuMode::None)
    {
        MoveMenuSelection(-1);
        return;
    }
    MoveRelative(FVector2D(1.0f, 0.0f));
}

void AABFPFirstPersonPawn::MoveBackward()
{
    if (ActiveMenu != EABFPMenuMode::None)
    {
        MoveMenuSelection(1);
        return;
    }
    MoveRelative(FVector2D(-1.0f, 0.0f));
}

void AABFPFirstPersonPawn::TurnLeft()
{
    if (ActiveMenu != EABFPMenuMode::None)
    {
        MenuTab();
        return;
    }
    TargetControlYaw = FRotator::NormalizeAxis(TargetControlYaw - 45.0f);
}

void AABFPFirstPersonPawn::TurnRight()
{
    if (ActiveMenu != EABFPMenuMode::None)
    {
        MenuTab();
        return;
    }
    TargetControlYaw = FRotator::NormalizeAxis(TargetControlYaw + 45.0f);
}

void AABFPFirstPersonPawn::ToggleHelp()
{
    bShowHelpOverlay = !bShowHelpOverlay;
}

void AABFPFirstPersonPawn::ToggleMinimap()
{
    bExpandedMinimap = !bExpandedMinimap;
}

void AABFPFirstPersonPawn::ToggleInventory()
{
    ActiveMenu = (ActiveMenu == EABFPMenuMode::Inventory) ? EABFPMenuMode::None : EABFPMenuMode::Inventory;
    SelectedMenuIndex = 0;
    ClampSelectedMenuIndex();
}

void AABFPFirstPersonPawn::ToggleEquipment()
{
    ActiveMenu = (ActiveMenu == EABFPMenuMode::Equipment) ? EABFPMenuMode::None : EABFPMenuMode::Equipment;
    SelectedMenuIndex = 0;
    ClampSelectedMenuIndex();
}

void AABFPFirstPersonPawn::ToggleShop()
{
    if (ActiveMenu == EABFPMenuMode::ShopBuy || ActiveMenu == EABFPMenuMode::ShopSell)
    {
        ActiveMenu = EABFPMenuMode::None;
        SelectedMenuIndex = 0;
        return;
    }
    if (!CurrentSnapshot.Shop.bActive)
    {
        AddStatusMessage(TEXT("Stand in a town shop entrance to trade."));
        return;
    }
    ActiveMenu = EABFPMenuMode::ShopBuy;
    SelectedMenuIndex = 0;
    ClampSelectedMenuIndex();
}

void AABFPFirstPersonPawn::MenuClose()
{
    if (ActiveMenu != EABFPMenuMode::None)
    {
        ActiveMenu = EABFPMenuMode::None;
        SelectedMenuIndex = 0;
    }
}

void AABFPFirstPersonPawn::MenuTab()
{
    if (ActiveMenu == EABFPMenuMode::Inventory)
    {
        ActiveMenu = EABFPMenuMode::Equipment;
    }
    else if (ActiveMenu == EABFPMenuMode::Equipment)
    {
        ActiveMenu = EABFPMenuMode::Inventory;
    }
    else if (ActiveMenu == EABFPMenuMode::ShopBuy)
    {
        ActiveMenu = EABFPMenuMode::ShopSell;
    }
    else if (ActiveMenu == EABFPMenuMode::ShopSell)
    {
        ActiveMenu = EABFPMenuMode::ShopBuy;
    }
    SelectedMenuIndex = 0;
    ClampSelectedMenuIndex();
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

void AABFPFirstPersonPawn::MenuConfirm()
{
    if (ActiveMenu == EABFPMenuMode::None) return;

    if (ActiveMenu == EABFPMenuMode::Inventory)
    {
        const TArray<int32> Pack = GetMenuInventoryIndices(false);
        if (!Pack.IsValidIndex(SelectedMenuIndex)) return;
        const FABFPInventoryItem& Item = CurrentSnapshot.Inventory[Pack[SelectedMenuIndex]];
        if ((Item.Flags & (ABFPItemFlags::Potion | ABFPItemFlags::Scroll |
            ABFPItemFlags::Food | ABFPItemFlags::Usable | ABFPItemFlags::Fuel)) != 0)
        {
            MenuUseSelected();
        }
        else if ((Item.Flags & ABFPItemFlags::Wearable) != 0)
        {
            MenuWieldSelected();
        }
        else
        {
            AddStatusMessage(FString::Printf(TEXT("Cannot directly use %s."), *Item.Name));
        }
        return;
    }

    if (ActiveMenu == EABFPMenuMode::Equipment)
    {
        MenuTakeOffSelected();
        return;
    }

    if (ActiveMenu == EABFPMenuMode::ShopBuy)
    {
        if (!CurrentSnapshot.Shop.bActive || !CurrentSnapshot.Shop.Items.IsValidIndex(SelectedMenuIndex)) return;
        const FABFPShopItem& ShopItem = CurrentSnapshot.Shop.Items[SelectedMenuIndex];
        if (!CurrentSnapshot.Shop.bIsHome && ShopItem.Price > CurrentSnapshot.PlayerGold)
        {
            AddStatusMessage(TEXT("You do not have enough gold."));
            return;
        }
        if (bLiveMode)
        {
            SendSimpleCommand(FString::Printf(TEXT("BUY %d 1"), ShopItem.Index));
        }
        else
        {
            CurrentSnapshot.PlayerGold -= ShopItem.Price;
            AddStatusMessage(FString::Printf(TEXT("You buy %s for %d gold."), *ShopItem.Name, ShopItem.Price));
        }
        return;
    }

    if (ActiveMenu == EABFPMenuMode::ShopSell)
    {
        if (!CurrentSnapshot.Shop.bActive) return;
        const TArray<int32> Pack = GetMenuInventoryIndices(false);
        if (!Pack.IsValidIndex(SelectedMenuIndex)) return;
        const FABFPInventoryItem& Item = CurrentSnapshot.Inventory[Pack[SelectedMenuIndex]];
        if (!CurrentSnapshot.Shop.bIsHome && (Item.Flags & ABFPItemFlags::Sellable) == 0)
        {
            AddStatusMessage(TEXT("This shop does not buy that item."));
            return;
        }
        if (bLiveMode)
        {
            SendSimpleCommand(FString::Printf(TEXT("SELL %d 1"), Item.Index));
        }
        else
        {
            CurrentSnapshot.PlayerGold += Item.SellPrice;
            AddStatusMessage(FString::Printf(TEXT("You sell %s for %d gold."), *Item.Name, Item.SellPrice));
        }
    }
}

void AABFPFirstPersonPawn::MenuUseSelected()
{
    if (ActiveMenu != EABFPMenuMode::Inventory) return;
    const TArray<int32> Pack = GetMenuInventoryIndices(false);
    if (!Pack.IsValidIndex(SelectedMenuIndex)) return;
    const FABFPInventoryItem& Item = CurrentSnapshot.Inventory[Pack[SelectedMenuIndex]];

    if (bLiveMode)
    {
        if ((Item.Flags & ABFPItemFlags::Potion) != 0)
        {
            SendSimpleCommand(FString::Printf(TEXT("QUAFF %d"), Item.Index));
        }
        else if ((Item.Flags & ABFPItemFlags::Scroll) != 0)
        {
            SendSimpleCommand(FString::Printf(TEXT("READ %d"), Item.Index));
        }
        else if ((Item.Flags & ABFPItemFlags::Food) != 0)
        {
            SendSimpleCommand(FString::Printf(TEXT("EAT %d"), Item.Index));
        }
        else if ((Item.Flags & ABFPItemFlags::Fuel) != 0)
        {
            SendSimpleCommand(FString::Printf(TEXT("REFILL %d"), Item.Index));
        }
        else if ((Item.Flags & ABFPItemFlags::Usable) != 0)
        {
            SendSimpleCommand(FString::Printf(TEXT("USE %d %d"), Item.Index, GetFacingKeypadDirection()));
        }
        else
        {
            AddStatusMessage(FString::Printf(TEXT("You cannot use %s."), *Item.Name));
        }
        return;
    }

    AddStatusMessage(FString::Printf(TEXT("You use %s."), *Item.Name));
    if ((Item.Flags & ABFPItemFlags::Potion) != 0)
    {
        CurrentSnapshot.PlayerHitPoints = CurrentSnapshot.PlayerMaxHitPoints;
    }
    ++CurrentSnapshot.Turn;
}

void AABFPFirstPersonPawn::MenuWieldSelected()
{
    if (ActiveMenu != EABFPMenuMode::Inventory) return;
    const TArray<int32> Pack = GetMenuInventoryIndices(false);
    if (!Pack.IsValidIndex(SelectedMenuIndex)) return;
    const FABFPInventoryItem& Item = CurrentSnapshot.Inventory[Pack[SelectedMenuIndex]];
    if ((Item.Flags & ABFPItemFlags::Wearable) == 0)
    {
        AddStatusMessage(FString::Printf(TEXT("You cannot wield or wear %s."), *Item.Name));
        return;
    }
    if (bLiveMode)
    {
        SendSimpleCommand(FString::Printf(TEXT("WIELD %d"), Item.Index));
    }
    else
    {
        AddStatusMessage(FString::Printf(TEXT("You equip %s."), *Item.Name));
    }
}

void AABFPFirstPersonPawn::MenuTakeOffSelected()
{
    if (ActiveMenu != EABFPMenuMode::Equipment) return;
    const TArray<int32> Gear = GetMenuInventoryIndices(true);
    if (!Gear.IsValidIndex(SelectedMenuIndex)) return;
    const FABFPInventoryItem& Item = CurrentSnapshot.Inventory[Gear[SelectedMenuIndex]];
    if (bLiveMode)
    {
        SendSimpleCommand(FString::Printf(TEXT("TAKEOFF %d"), Item.Index));
    }
    else
    {
        AddStatusMessage(FString::Printf(TEXT("You take off %s."), *Item.Name));
    }
}

void AABFPFirstPersonPawn::MenuDropSelected()
{
    if (ActiveMenu != EABFPMenuMode::Inventory && ActiveMenu != EABFPMenuMode::Equipment) return;
    const TArray<int32> List = GetMenuInventoryIndices(ActiveMenu == EABFPMenuMode::Equipment);
    if (!List.IsValidIndex(SelectedMenuIndex)) return;
    const FABFPInventoryItem& Item = CurrentSnapshot.Inventory[List[SelectedMenuIndex]];
    if (bLiveMode)
    {
        SendSimpleCommand(FString::Printf(TEXT("DROP %d 1"), Item.Index));
    }
    else
    {
        AddStatusMessage(FString::Printf(TEXT("You drop %s."), *Item.Name));
    }
}

void AABFPFirstPersonPawn::MenuRefillSelected()
{
    if (ActiveMenu != EABFPMenuMode::Inventory) return;
    const TArray<int32> Pack = GetMenuInventoryIndices(false);
    if (!Pack.IsValidIndex(SelectedMenuIndex)) return;
    const FABFPInventoryItem& Item = CurrentSnapshot.Inventory[Pack[SelectedMenuIndex]];
    if ((Item.Flags & ABFPItemFlags::Fuel) == 0)
    {
        AddStatusMessage(TEXT("Select a flask of oil or torch to refill your light."));
        return;
    }
    if (bLiveMode)
    {
        SendSimpleCommand(FString::Printf(TEXT("REFILL %d"), Item.Index));
    }
    else
    {
        AddStatusMessage(TEXT("You refill your lantern."));
    }
}

void AABFPFirstPersonPawn::QuickQuaff()
{
    if (ActiveMenu != EABFPMenuMode::None) return;

    int32 BestIdx = INDEX_NONE;
    FString BestName;
    for (const FABFPInventoryItem& Item : CurrentSnapshot.Inventory)
    {
        if (Item.bEquipped || (Item.Flags & ABFPItemFlags::Potion) == 0) continue;
        const FString Lower = Item.Name.ToLower();
        if (BestIdx == INDEX_NONE || Lower.Contains(TEXT("cure")) || Lower.Contains(TEXT("heal")))
        {
            BestIdx = Item.Index;
            BestName = Item.Name;
            if (Lower.Contains(TEXT("cure")) || Lower.Contains(TEXT("heal"))) break;
        }
    }

    if (BestIdx == INDEX_NONE)
    {
        AddStatusMessage(TEXT("No potions in your pack."));
        return;
    }
    if (bLiveMode)
    {
        SendSimpleCommand(FString::Printf(TEXT("QUAFF %d"), BestIdx));
    }
    else
    {
        CurrentSnapshot.PlayerHitPoints = CurrentSnapshot.PlayerMaxHitPoints;
        AddStatusMessage(FString::Printf(TEXT("You quaff %s."), *BestName));
    }
}

void AABFPFirstPersonPawn::QuickEat()
{
    if (ActiveMenu != EABFPMenuMode::None) return;

    for (const FABFPInventoryItem& Item : CurrentSnapshot.Inventory)
    {
        if (Item.bEquipped || (Item.Flags & ABFPItemFlags::Food) == 0) continue;
        if (bLiveMode)
        {
            SendSimpleCommand(FString::Printf(TEXT("EAT %d"), Item.Index));
        }
        else
        {
            CurrentSnapshot.PlayerFoodPct = 100;
            AddStatusMessage(FString::Printf(TEXT("You eat %s."), *Item.Name));
        }
        return;
    }
    AddStatusMessage(TEXT("No food in your pack."));
}

void AABFPFirstPersonPawn::RestTurn()
{
    if (ActiveMenu != EABFPMenuMode::None) return;

    if (bLiveMode)
    {
        SendSimpleCommand(TEXT("REST"));
        return;
    }
    CurrentSnapshot.PlayerHitPoints = CurrentSnapshot.PlayerMaxHitPoints;
    CurrentSnapshot.PlayerMana = CurrentSnapshot.PlayerMaxMana;
    CurrentSnapshot.Turn += 10;
    AddStatusMessage(TEXT("You rest and recover your strength."));
}

void AABFPFirstPersonPawn::InteractForward()
{
    if (ActiveMenu != EABFPMenuMode::None)
    {
        MenuConfirm();
        return;
    }

    if (CurrentSnapshot.Shop.bActive)
    {
        ToggleShop();
        return;
    }

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
    if (ActiveMenu != EABFPMenuMode::None) return;

    if (bLiveMode)
    {
        SendSimpleCommand(TEXT("WAIT"));
        return;
    }

    ++CurrentSnapshot.Turn;
}

void AABFPFirstPersonPawn::GoUp()
{
    if (ActiveMenu != EABFPMenuMode::None) return;

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
    if (ActiveMenu != EABFPMenuMode::None) return;

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
    if (ActiveMenu != EABFPMenuMode::None) return;

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
    if (bInterpolatingMove || ActiveMenu != EABFPMenuMode::None) return;

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
    CurrentSnapshot.PlayerHitPoints = 34;
    CurrentSnapshot.PlayerMaxHitPoints = 42;
    CurrentSnapshot.PlayerMana = 14;
    CurrentSnapshot.PlayerMaxMana = 14;
    CurrentSnapshot.PlayerLevel = 1;
    CurrentSnapshot.PlayerGold = 185;
    CurrentSnapshot.PlayerExp = 45;
    CurrentSnapshot.PlayerMaxExp = 100;
    CurrentSnapshot.PlayerArmorClass = 14;
    CurrentSnapshot.PlayerSpeed = 0;
    CurrentSnapshot.PlayerFoodPct = 92;

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

    auto AddDemoItem = [this](int32 Idx, const TCHAR* Label, bool bEquip, int32 Flags,
        int32 Qty, int32 SellPrice, const TCHAR* Slot, const TCHAR* Name)
    {
        FABFPInventoryItem Item;
        Item.Index = Idx;
        Item.Label = Label;
        Item.bEquipped = bEquip;
        Item.Flags = Flags;
        Item.Quantity = Qty;
        Item.SellPrice = SellPrice;
        Item.SlotName = Slot;
        Item.Name = Name;
        CurrentSnapshot.Inventory.Add(MoveTemp(Item));
    };

    AddDemoItem(0, TEXT("a"), true, ABFPItemFlags::Wearable, 1, 0, TEXT("wielding"), TEXT("a Long Sword (2d5) (+0,+0)"));
    AddDemoItem(1, TEXT("b"), true, ABFPItemFlags::Wearable, 1, 0, TEXT("wearing"), TEXT("Soft Leather Armour [8,+0]"));
    AddDemoItem(2, TEXT("c"), true, ABFPItemFlags::Wearable, 1, 0, TEXT("light"), TEXT("a Brass Lantern (with 6200 turns)"));
    AddDemoItem(10, TEXT("a"), false, ABFPItemFlags::Usable | ABFPItemFlags::Potion | ABFPItemFlags::Sellable, 2, 15, TEXT(""), TEXT("2 Potions of Cure Light Wounds"));
    AddDemoItem(11, TEXT("b"), false, ABFPItemFlags::Usable | ABFPItemFlags::Scroll | ABFPItemFlags::Sellable, 3, 20, TEXT(""), TEXT("3 Scrolls of Phase Door"));
    AddDemoItem(12, TEXT("c"), false, ABFPItemFlags::Usable | ABFPItemFlags::Food | ABFPItemFlags::Sellable, 4, 3, TEXT(""), TEXT("4 Rations of Food"));
    AddDemoItem(13, TEXT("d"), false, ABFPItemFlags::Fuel | ABFPItemFlags::Sellable, 1, 10, TEXT(""), TEXT("a Flask of Oil"));

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
    Hint.Text = TEXT("Press [I] Pack, [C] Gear, [Q] Quaff, [R] Rest, [M] Map, or [F] Interact.");
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
