#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABFPCreatureActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UMaterialInstanceDynamic;

UCLASS()
class ANGBANDFIRSTPERSON_API AABFPCreatureActor : public AActor
{
    GENERATED_BODY()

public:
    AABFPCreatureActor();

    void PlayHitReaction(int32 Damage);
    void SetTargetLocation(const FVector& Location, const FRotator& Rotation);
    void SetCreatureAppearance(const FString& Name, int32 KindId);

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
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Tail;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftWing;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightWing;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> LeftHorn;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RightHorn;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CreatureMaterial;
    FString CreatureName;
    int32 CreatureKindId = INDEX_NONE;
    FLinearColor BaseColor = FLinearColor(0.12f, 0.19f, 0.11f);
    float Elapsed = 0.0f;
    float DamageFlashSeconds = 0.0f;
    FVector TargetLocation = FVector::ZeroVector;
    FRotator TargetRotation = FRotator::ZeroRotator;
    bool bInterpolating = false;
};
