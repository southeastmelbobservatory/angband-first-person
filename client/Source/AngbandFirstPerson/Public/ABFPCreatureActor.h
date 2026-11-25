#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABFPProtocol.h"
#include "ABFPCreatureActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UAnimMontage;
class USoundBase;
struct FABFPCreatureVisualEntry;

UCLASS(Blueprintable)
class ANGBANDFIRSTPERSON_API AABFPCreatureActor : public AActor
{
    GENERATED_BODY()

public:
    AABFPCreatureActor();

    UFUNCTION(BlueprintCallable, Category="Angband|Creature")
    void PlayHitReaction(int32 Damage);

    UFUNCTION(BlueprintCallable, Category="Angband|Creature")
    void PlayDeathReaction();

    UFUNCTION(BlueprintPure, Category="Angband|Creature")
    bool IsDying() const { return bDying; }

    UFUNCTION(BlueprintPure, Category="Angband|Creature")
    bool IsMoving() const { return bInterpolating; }

    UFUNCTION(BlueprintPure, Category="Angband|Creature")
    FString GetCreatureName() const { return CreatureName; }

    UFUNCTION(BlueprintPure, Category="Angband|Creature")
    int32 GetCurrentHitPoints() const { return CurrentHitPoints; }

    UFUNCTION(BlueprintPure, Category="Angband|Creature")
    int32 GetMaxHitPoints() const { return MaxHitPoints; }

    void SetTargetLocation(const FVector& Location, const FRotator& Rotation);
    void SetCreatureAppearance(const FString& Name, int32 KindId);
    void ApplyVisualEntry(const FABFPCreatureVisualEntry* Entry);
    void SetCastShadowsEnabled(bool bEnableShadows);
    void UpdateEntityState(const FABFPEntity& Entity);

protected:
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintImplementableEvent, Category="Angband|Creature")
    void OnCreatureConfigured(const FABFPEntity& EntityData);

    UFUNCTION(BlueprintImplementableEvent, Category="Angband|Creature")
    void OnCreatureMovementChanged(bool bIsMoving);

    UFUNCTION(BlueprintImplementableEvent, Category="Angband|Creature")
    void OnCreatureDamaged(int32 Damage, int32 RemainingHP, int32 MaxHP);

    UFUNCTION(BlueprintImplementableEvent, Category="Angband|Creature")
    void OnCreatureDied();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USkeletalMeshComponent> SkeletalMeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UStaticMeshComponent> CustomStaticMeshComp;

private:
    void EnsureProceduralParts();
    void EnsureHealthBar();
    void SetProceduralPartsVisible(bool bVisible);
    void RefreshTickState();
    void UpdateHealthBar();

    UPROPERTY(Transient) TObjectPtr<UStaticMesh> CachedSphereMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> CachedCylinderMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> CachedConeMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> CachedCubeMesh;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CachedBasicMat;

    UPROPERTY() TObjectPtr<USceneComponent> HealthBarRoot;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HealthBarBg;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> HealthBarFill;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> HealthBarFillMat;

    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LeftArm;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> RightArm;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LeftLeg;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> RightLeg;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LeftEye;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> RightEye;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Tail;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LeftWing;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> RightWing;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> LeftHorn;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> RightHorn;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CreatureMaterial;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveHitMontage;
    UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveDeathMontage;
    UPROPERTY(Transient) TObjectPtr<USoundBase> ActiveHitSound;
    UPROPERTY(Transient) TObjectPtr<USoundBase> ActiveDeathSound;

    FString CreatureName;
    int32 CreatureKindId = INDEX_NONE;
    int32 CurrentHitPoints = 0;
    int32 MaxHitPoints = 0;
    bool bUsingCustomVisual = false;
    bool bUsingSkeletalVisual = false;
    FLinearColor BaseColor = FLinearColor(0.12f, 0.19f, 0.11f);
    FVector BaseBodyScale = FVector(0.48f, 0.38f, 0.66f);
    FVector BaseHeadScale = FVector(0.30f, 0.29f, 0.32f);
    FVector BaseBodyLocation = FVector(0, 0, 105);
    FVector BaseHeadLocation = FVector(0, 0, 205);
    bool bDying = false;
    float DeathSeconds = 0.0f;
    float DeathDuration = 0.72f;
    float Elapsed = 0.0f;
    float DamageFlashSeconds = 0.0f;
    FVector TargetLocation = FVector::ZeroVector;
    FRotator TargetRotation = FRotator::ZeroRotator;
    bool bInterpolating = false;
};
