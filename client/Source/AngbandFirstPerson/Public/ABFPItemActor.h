#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABFPProtocol.h"
#include "ABFPItemActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UPointLightComponent;
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
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Core;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ShardA;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ShardB;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShardMaterial;

    FString ItemName;
    int32 ItemCategoryId = INDEX_NONE;
    int32 ItemVariantId = INDEX_NONE;
    FLinearColor BaseColor = FLinearColor(0.08f, 0.52f, 0.72f);
    bool bUsingCustomVisual = false;
    bool bFloatAndSpin = true;
    float BaseHeightZ = 38.0f;
    float Elapsed = 0.0f;
};
