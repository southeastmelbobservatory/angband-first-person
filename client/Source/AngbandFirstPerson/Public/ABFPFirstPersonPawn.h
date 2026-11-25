#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ABFPProtocol.h"
#include "ABFPNetworkClient.h"
#include "ABFPFirstPersonPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class AABFPDungeonActor;
class UABFPVisualCatalog;

UCLASS(Config=Game, Blueprintable)
class ANGBANDFIRSTPERSON_API AABFPFirstPersonPawn : public APawn
{
    GENERATED_BODY()

public:
    AABFPFirstPersonPawn();

    UFUNCTION(BlueprintPure, Category="Angband|State")
    const FABFPSnapshot& GetCurrentSnapshot() const { return CurrentSnapshot; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    const TArray<FABFPMessage>& GetMessageHistory() const { return MessageHistory; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    bool IsLiveMode() const { return bLiveMode; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    bool HasAuthoritativeState() const { return bHaveAuthoritativeState; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    bool IsAwaitingServerTurn() const { return bAwaitingServerTurn; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    float GetDamageFlashAlpha() const { return FMath::Clamp(PlayerDamageFlashSeconds / 0.35f, 0.0f, 1.0f); }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    bool IsHelpVisible() const { return bShowHelpOverlay; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    bool IsMinimapExpanded() const { return bExpandedMinimap; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    EABFPMenuMode GetActiveMenu() const { return ActiveMenu; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    int32 GetSelectedMenuIndex() const { return SelectedMenuIndex; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    int32 GetBirthFocusColumn() const { return BirthFocusColumn; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    int32 GetBirthSelectedStat() const { return BirthSelectedStat; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    const FString& GetBirthDraftName() const { return BirthDraftName; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    int32 GetLastCastSpellIndex() const { return LastCastSpellIndex; }

    UFUNCTION(BlueprintPure, Category="Angband|State")
    int64 GetLockedTargetId() const { return LockedTargetId; }

    const FABFPEntity* GetActiveRangedTarget(bool* OutIsManualLock = nullptr) const;
    const FABFPSpellEntry* GetQuickCastSpell() const;

    UFUNCTION(BlueprintPure, Category="Angband|State")
    FIntPoint GetFacingGridDelta() const;

    UFUNCTION(BlueprintPure, Category="Angband|State")
    int32 GetFacingKeypadDirection() const;

    UFUNCTION(BlueprintPure, Category="Angband|State")
    float GetFacingYawDegrees() const { return TargetControlYaw; }

    UFUNCTION(BlueprintPure, Category="Angband|Visuals")
    UABFPVisualCatalog* GetVisualCatalog() const { return LoadedVisualCatalog; }

    UFUNCTION(BlueprintCallable, Category="Angband|Presentation")
    void PlayPlayerAttackSwing();

    TArray<int32> GetMenuInventoryIndices(bool bEquippedOnly) const;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UFUNCTION(BlueprintImplementableEvent, Category="Angband|Presentation")
    void OnPlayerAttackSwing();

    UFUNCTION(BlueprintImplementableEvent, Category="Angband|Presentation")
    void OnPlayerDamaged(int32 Damage);

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Angband|Connection")
    FString HostAddress = TEXT("127.0.0.1");

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Angband|Connection")
    int32 HostPort = 47652;

    /**
     * Optional DataAsset mapping Angband terrain, monsters, and items to
     * high-fidelity Static/Skeletal Meshes, Materials, and Audio.
     * Can be set in a Pawn Blueprint or in Config/DefaultGame.ini.
     */
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Angband|Visuals")
    TSoftObjectPtr<UABFPVisualCatalog> VisualCatalogAsset;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UPointLightComponent> Lantern;

    /** Mount point for the right-hand first-person weapon mesh. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USceneComponent> ViewModelMount;

    /** Mount point for the left-hand first-person lantern mesh. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USceneComponent> LanternMount;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UStaticMeshComponent> HeldWeaponMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UStaticMeshComponent> HeldLanternMesh;

    /** Glowing flame overlay attached to HeldLanternMesh when carrying a lit wooden torch. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UStaticMeshComponent> HeldTorchFlameMesh;

private:
    void MoveForward();
    void MoveBackward();
    void TurnLeft();
    void TurnRight();
    void InteractForward();
    void WaitTurn();
    void GoUp();
    void GoDown();
    void AutoPickup();
    void ToggleHelp();
    void ToggleMinimap();
    void ToggleInventory();
    void ToggleEquipment();
    void ToggleShop();
    void ToggleSpells();
    void QuickFire();
    void QuickCast();
    void NewCharacter();
    void QuickQuaff();
    void QuickEat();
    void RestTurn();
    void MenuConfirm();
    void MenuUseSelected();
    void MenuWieldSelected();
    void MenuTakeOffSelected();
    void MenuDropSelected();
    void MenuRefillSelected();
    void MenuThrowSelected();
    void MenuTab();
    void MenuClose();

    void CycleRangedTarget();
    FString BuildAimSuffix() const;
    bool IsTypingCharacterName() const;
    void TickCharacterNameInput();
    void OpenDemoCharacterCreation();
    void RecalcDemoBirthStats();
    void MoveMenuSelection(int32 Delta);
    int32 GetActiveMenuItemCount() const;
    void ClampSelectedMenuIndex();
    void ConfigureFirstPersonViewModels();
    void ApplyViewModelVariant(UStaticMeshComponent* MeshComp, const struct FABFPMeshVariant& Variant);
    void UpdateEquippedViewModels(bool bForceRefresh = false);

    void SendSimpleCommand(const FString& Command);
    void MoveRelative(FVector2D LocalDirection);
    void BuildDemoSnapshot();
    void TryDemoStep(int32 DX, int32 DY);
    void TryDemoInteract(int32 DX, int32 DY);
    void AddStatusMessage(const FString& Text);
    void AppendMessages(const TArray<FABFPMessage>& NewMessages);
    static FString DirectionCommandName(const TCHAR* Prefix, int32 DX, int32 DY);

    UPROPERTY(Transient) TObjectPtr<UABFPVisualCatalog> LoadedVisualCatalog;
    UPROPERTY() TObjectPtr<AABFPDungeonActor> Dungeon;

    FABFPSnapshot CurrentSnapshot;
    TArray<FABFPMessage> MessageHistory;
    FVector TargetPawnLocation = FVector::ZeroVector;
    float TargetControlYaw = -90.0f;
    float CurrentControlYaw = -90.0f;
    float CameraBaseHeight = 170.0f;
    float StepBobElapsed = 0.0f;
    float LanternElapsed = 0.0f;
    float AttackSwingSeconds = 0.0f;
    bool bInterpolatingMove = false;
    bool bLiveMode = false;
    bool bAwaitingServerTurn = false;
    float AwaitingTurnSeconds = 0.0f;
    bool bHaveAuthoritativeState = false;
    bool bShowHelpOverlay = true;
    bool bExpandedMinimap = false;
    EABFPMenuMode ActiveMenu = EABFPMenuMode::None;
    int32 SelectedMenuIndex = 0;
    float ReconnectRetrySeconds = 0.0f;
    float PlayerDamageFlashSeconds = 0.0f;
    float NewCharConfirmSeconds = 0.0f;
    int64 LockedTargetId = 0;
    int32 LastCastSpellIndex = INDEX_NONE;
    int32 BirthFocusColumn = 0;
    int32 BirthSelectedStat = 0;
    FString BirthDraftName;
    bool bWasInBirthMode = false;
    FString LastEquippedWeaponKey;
    FString LastEquippedLightKey;
    FLinearColor ActiveLightColor = FLinearColor(1.0f, 0.80f, 0.56f);
    float ActiveLightIntensity = 7.5f;
    float ActiveLightRadius = 2600.0f;
    float ActiveLightFlickerAmp = 0.035f;
    bool bHoldingLitTorch = false;
    FABFPNetworkClient NetworkClient;
};
