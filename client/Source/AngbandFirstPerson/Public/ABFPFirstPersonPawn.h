#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ABFPProtocol.h"
#include "ABFPNetworkClient.h"
#include "ABFPFirstPersonPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class UPointLightComponent;
class UTextRenderComponent;
class AABFPDungeonActor;

UCLASS(Config=Game)
class ANGBANDFIRSTPERSON_API AABFPFirstPersonPawn : public APawn
{
    GENERATED_BODY()

public:
    AABFPFirstPersonPawn();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    void MoveForwardAxis(float Value);
    void MoveRightAxis(float Value);
    void WaitTurn();
    void GoUp();
    void GoDown();
    void AutoPickup();
    void SendSimpleCommand(const FString& Command);
    void Turn(float Value);
    void LookUp(float Value);
    void MoveRelative(FVector2D LocalDirection);
    void BuildDemoSnapshot();
    void TryDemoStep(int32 DX, int32 DY);
    void UpdateStatusReadout();

    UPROPERTY(Config, EditAnywhere, Category="Angband|Connection") FString HostAddress = TEXT("127.0.0.1");
    UPROPERTY(Config, EditAnywhere, Category="Angband|Connection") int32 HostPort = 47652;

    UPROPERTY() TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Lantern;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> StatusReadout;
    UPROPERTY() TObjectPtr<AABFPDungeonActor> Dungeon;
    FABFPSnapshot DemoSnapshot;
    FVector2D PendingMoveInput = FVector2D::ZeroVector;
    FVector TargetPawnLocation = FVector::ZeroVector;
    bool bInterpolatingMove = false;
    bool bLiveMode = false;
    bool bAwaitingServerTurn = false;
    bool bHaveAuthoritativeState = false;
    float ReconnectRetrySeconds = 0.0f;
    float PlayerDamageFlashSeconds = 0.0f;
    FABFPNetworkClient NetworkClient;
};
