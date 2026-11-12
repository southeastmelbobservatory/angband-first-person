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
    void PlayDeathReaction();
    bool IsDying() const { return bDying; }
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
    FVector BaseBodyScale = FVector(0.48f, 0.38f, 0.66f);
    FVector BaseHeadScale = FVector(0.30f, 0.29f, 0.32f);
    FVector BaseBodyLocation = FVector(0, 0, 105);
    FVector BaseHeadLocation = FVector(0, 0, 205);
    bool bDying = false;
    float DeathSeconds = 0.0f;
    float Elapsed = 0.0f;
    float DamageFlashSeconds = 0.0f;
    FVector TargetLocation = FVector::ZeroVector;
    FRotator TargetRotation = FRotator::ZeroRotator;
    bool bInterpolating = false;
};
