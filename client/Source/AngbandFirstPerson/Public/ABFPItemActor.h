#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABFPItemActor.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class UMaterialInstanceDynamic;

UCLASS()
class ANGBANDFIRSTPERSON_API AABFPItemActor : public AActor
{
    GENERATED_BODY()

public:
    AABFPItemActor();
    void SetItemAppearance(const FString& Name, int32 CategoryId, int32 VariantId);

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY() TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Core;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ShardA;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> ShardB;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> CoreMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> ShardMaterial;
    FString ItemName;
    int32 ItemCategoryId = INDEX_NONE;
    int32 ItemVariantId = INDEX_NONE;
    FLinearColor BaseColor = FLinearColor(0.08f, 0.52f, 0.72f);
    float Elapsed = 0.0f;
};
