#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "ABFPProtocol.h"
#include "ABFPFirstPersonPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class UPointLightComponent;
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
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    void MoveForward();
    void MoveBackward();
    void MoveLeft();
    void MoveRight();
    void Turn(float Value);
    void LookUp(float Value);
    void MoveRelative(FVector2D LocalDirection);
    void BuildDemoSnapshot();
    void TryDemoStep(int32 DX, int32 DY);

    UPROPERTY() TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Lantern;
    UPROPERTY() TObjectPtr<AABFPDungeonActor> Dungeon;
    FABFPSnapshot DemoSnapshot;
    FVector TargetPawnLocation = FVector::ZeroVector;
    bool bInterpolatingMove = false;
};
