#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABFPProtocol.h"
#include "ABFPDungeonActor.generated.h"

class UInstancedStaticMeshComponent;
class USceneComponent;
class UExponentialHeightFogComponent;
class USkyLightComponent;

UCLASS()
class ANGBANDFIRSTPERSON_API AABFPDungeonActor : public AActor
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
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RememberedFloors;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Walls;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RememberedWalls;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Doors;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> StairsUp;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> StairsDown;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Rubble;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Shops;
    UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> DungeonFog;
    UPROPERTY() TObjectPtr<USkyLightComponent> AmbientSky;
    UPROPERTY(Transient) TMap<int64, TObjectPtr<AActor>> SpawnedEntities;
    int32 LastDepth = INDEX_NONE;
};
