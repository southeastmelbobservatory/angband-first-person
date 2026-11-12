#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABFPProtocol.h"
#include "ABFPDungeonActor.generated.h"

class UInstancedStaticMeshComponent;

UCLASS()
class ANG BANDFIRSTPERSON_API AABFPDungeonActor : public AActor
{
    GENERATED_BODY()

public:
    AABFPDungeonActor();

    UFUNCTION(BlueprintCallable, Category="Angband")
    void ApplySnapshot(const FABFPSnapshot& Snapshot);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon")
    float CellSize = 400.0f;

private:
    FVector GridToWorld(int32 X, int32 Y, int32 PlayerX, int32 PlayerY) const;

    UPROPERTY() TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Floors;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Walls;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Doors;
    UPROPERTY(Transient) TMap<uint64, TObjectPtr<AActor>> SpawnedEntities;
};
