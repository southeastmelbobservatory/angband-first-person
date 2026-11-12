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

UCLASS()
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
    void MoveForward();
    void MoveBackward();
    void MoveLeft();
    void MoveRight();
    void WaitTurn();
    void Turn(float Value);
    void LookUp(float Value);
    void MoveRelative(FVector2D LocalDirection);
    void BuildDemoSnapshot();
    void TryDemoStep(int32 DX, int32 DY);
    void UpdateStatusReadout();

    UPROPERTY() TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Lantern;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> StatusReadout;
    UPROPERTY() TObjectPtr<AABFPDungeonActor> Dungeon;
    FABFPSnapshot DemoSnapshot;
    FVector TargetPawnLocation = FVector::ZeroVector;
    bool bInterpolatingMove = false;
    bool bLiveMode = false;
    bool bAwaitingServerTurn = false;
    bool bHaveAuthoritativeState = false;
    float ReconnectRetrySeconds = 0.0f;
    FABFPNetworkClient NetworkClient;
};
