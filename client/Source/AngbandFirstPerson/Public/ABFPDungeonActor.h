#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ABFPProtocol.h"
#include "ABFPDungeonActor.generated.h"

class UInstancedStaticMeshComponent;
class USceneComponent;
class UExponentialHeightFogComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class UPostProcessComponent;
class UMaterialInterface;
class UStaticMesh;
class UABFPVisualCatalog;
struct FABFPMeshVariant;

USTRUCT()
struct FABFPMeshPoolEntry
{
    GENERATED_BODY()

    UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Component = nullptr;
    UPROPERTY(Transient) FVector LocationOffset = FVector::ZeroVector;
    UPROPERTY(Transient) FRotator RotationOffset = FRotator::ZeroRotator;
    UPROPERTY(Transient) FVector Scale = FVector::OneVector;
};

UCLASS(Blueprintable)
class ANGBANDFIRSTPERSON_API AABFPDungeonActor : public AActor
{
    GENERATED_BODY()

public:
    AABFPDungeonActor();

    UFUNCTION(BlueprintCallable, Category="Angband")
    void SetVisualCatalog(UABFPVisualCatalog* InCatalog);

    UFUNCTION(BlueprintCallable, Category="Angband")
    void ApplySnapshot(const FABFPSnapshot& Snapshot);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon")
    float CellSize = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dungeon")
    float WallHeight = 320.0f;

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    FVector GridToWorld(int32 X, int32 Y) const;
    void ConfigureMeshPool(
        TArray<FABFPMeshPoolEntry>& Pool,
        const TArray<FABFPMeshVariant>& Variants,
        UMaterialInterface* OverrideMat,
        bool bCastShadow,
        const FString& Prefix,
        const FLinearColor* DynamicTint = nullptr);
    void ConfigureSingleLayer(
        UInstancedStaticMeshComponent* Component,
        const FABFPMeshVariant& Variant,
        UStaticMesh* FallbackMesh,
        UMaterialInterface* FallbackMat,
        UMaterialInterface* OverrideMat);
    void ApplyTerrainVisualsForDepth(int32 DungeonDepth);
    void UpdateDynamicRoomLights(const FABFPSnapshot& Snapshot,
        const TMap<FIntPoint, const FABFPCell*>& CellMap);
    void EnsureRoomLightPool(int32 DesiredCount);

    UPROPERTY() TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Floors;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RememberedFloors;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Ceilings;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RememberedCeilings;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Walls;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RememberedWalls;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Veins;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> RememberedVeins;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> DoorFrames;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> DoorsClosed;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> DoorsOpen;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> DoorsBroken;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> StairsUp;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> StairsDown;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Rubble;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Shops;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Traps;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> WallTorches;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> TorchFlames;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> DarknessShroud;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> DarknessBridgeFloors;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> DarknessBridgeCeilings;

    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInterface>> DynamicPoolMaterials;

    UPROPERTY(Transient) TArray<FABFPMeshPoolEntry> CustomFloorPools;
    UPROPERTY(Transient) TArray<FABFPMeshPoolEntry> CustomRememberedFloorPools;
    UPROPERTY(Transient) TArray<FABFPMeshPoolEntry> CustomCeilingPools;
    UPROPERTY(Transient) TArray<FABFPMeshPoolEntry> CustomWallPools;
    UPROPERTY(Transient) TArray<FABFPMeshPoolEntry> CustomRememberedWallPools;
    UPROPERTY(Transient) TArray<FABFPMeshPoolEntry> CustomVeinPools;
    UPROPERTY(Transient) TArray<FABFPMeshPoolEntry> CustomRememberedVeinPools;
    UPROPERTY(Transient) TArray<FABFPMeshPoolEntry> CustomPillarPools;
    UPROPERTY(Transient) TArray<FABFPMeshPoolEntry> CustomWallDecorPools;

    UPROPERTY() TObjectPtr<UExponentialHeightFogComponent> DungeonFog;
    UPROPERTY() TObjectPtr<USkyLightComponent> AmbientSky;
    UPROPERTY() TObjectPtr<USkyAtmosphereComponent> TownSky;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> TownSunLight;
    UPROPERTY() TObjectPtr<UPostProcessComponent> AtmospherePostProcess;
    UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> RoomLights;
    UPROPERTY(Transient) TArray<float> RoomLightPhases;

    UPROPERTY(Transient) TObjectPtr<UStaticMesh> FallbackCubeMesh;
    UPROPERTY(Transient) TObjectPtr<UStaticMesh> FallbackSphereMesh;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackFlameMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackFloorMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackMemoryFloorMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackCeilingMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackWallMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackMemoryWallMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackVeinMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackDoorFrameMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackDoorMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackUpStairMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackDownStairMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackRubbleMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackShopMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackTrapMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> FallbackTorchMat;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> DarknessShroudMat;

    UPROPERTY(Transient) TObjectPtr<UABFPVisualCatalog> VisualCatalog;
    UPROPERTY(Transient) TMap<int64, TObjectPtr<AActor>> SpawnedEntities;

    int32 LastDepth = INDEX_NONE;
    int32 ConfiguredVisualDepthCategory = INDEX_NONE;
    uint64 LastTerrainSignature = 0;
    bool bHasBuiltTerrainOnce = false;
    float FlickerElapsed = 0.0f;
};
