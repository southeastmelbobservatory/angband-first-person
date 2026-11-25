#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABFPProtocol.h"
#include "ABFPItemActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UPointLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FABFPItemVisualEntry;

UCLASS(Blueprintable)
class ANGBANDFIRSTPERSON_API AABFPItemActor : public AActor
{
    GENERATED_BODY()

public:
    AABFPItemActor();

    void SetItemAppearance(const FString& Name, int32 CategoryId, int32 VariantId);
    void ApplyVisualEntry(const FABFPItemVisualEntry* Entry);
    void UpdateEntityState(const FABFPEntity& Entity);

    UFUNCTION(BlueprintPure, Category="Angband|Item")
    FString GetItemName() const { return ItemName; }

protected:
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintImplementableEvent, Category="Angband|Item")
    void OnItemConfigured(const FABFPEntity& EntityData);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UStaticMeshComponent> CustomMeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<UPointLightComponent> AccentLight;

private:
    void EnsureProceduralParts();
    void EnsureAccentLight();

    UPROPERTY(Transient) TObjectPtr<UStaticMesh> CachedSphereMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> CachedConeMesh;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CachedBasicMat;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Core;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ShardA;
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ShardB;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ShardMaterial;

    FString ItemName;
    int32 ItemCategoryId = INDEX_NONE;
    int32 ItemVariantId = INDEX_NONE;
    FLinearColor BaseColor = FLinearColor(0.08f, 0.52f, 0.72f);
    bool bUsingCustomVisual = false;
    bool bFloatAndSpin = true;
    float BaseHeightZ = 38.0f;
    float Elapsed = 0.0f;
};
