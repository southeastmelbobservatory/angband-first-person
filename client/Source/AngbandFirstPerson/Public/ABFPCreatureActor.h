#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABFPCreatureActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class ANG BANDFIRSTPERSON_API AABFPCreatureActor : public AActor
{
    GENERATED_BODY()

public:
    AABFPCreatureActor();

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY() TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftArm;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightArm;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftLeg;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightLeg;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftEye;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightEye;
    float Elapsed = 0.0f;
};
