#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABFPItemActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class ANG BANDFIRSTPERSON_API AABFPItemActor : public AActor
{
    GENERATED_BODY()

public:
    AABFPItemActor();

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY() TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Core;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ShardA;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ShardB;
    float Elapsed = 0.0f;
};
