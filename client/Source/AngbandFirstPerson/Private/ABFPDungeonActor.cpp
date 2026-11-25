#include "ABFPDungeonActor.h"

#include "ABFPCreatureActor.h"
#include "ABFPItemActor.h"
#include "ABFPVisualCatalog.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    uint32 DeterministicCellHash(int32 X, int32 Y, int32 Salt)
    {
        uint32 H = static_cast<uint32>(X) * 73856093u ^
            static_cast<uint32>(Y) * 19349663u ^
            static_cast<uint32>(Salt) * 83492791u;
        H ^= (H >> 13);
        H *= 1274126177u;
        return H ^ (H >> 16);
    }

    bool IsWallLike(const FABFPCell* Cell)
    {
        if (!Cell) return false;
        return Cell->Terrain == EABFPTerrain::Wall ||
            Cell->Terrain == EABFPTerrain::Vein ||
            Cell->Terrain == EABFPTerrain::Shop;
    }

    FTransform ComposeVariantTransform(const FVector& TileCenter, const FRotator& TileRot,
        const FABFPMeshVariant& Variant)
    {
        const FRotator CombinedRot = (TileRot.Quaternion() * Variant.RotationOffset.Quaternion()).Rotator();
        const FVector CombinedLoc = TileCenter + TileRot.RotateVector(Variant.LocationOffset);
        return FTransform(CombinedRot, CombinedLoc, Variant.Scale);
    }

    FTransform ComposePoolTransform(const FVector& TileCenter, const FRotator& TileRot,
        const FABFPMeshPoolEntry& Entry)
    {
        const FRotator CombinedRot = (TileRot.Quaternion() * Entry.RotationOffset.Quaternion()).Rotator();
        const FVector CombinedLoc = TileCenter + TileRot.RotateVector(Entry.LocationOffset);
        return FTransform(CombinedRot, CombinedLoc, Entry.Scale);
    }
}

AABFPDungeonActor::AABFPDungeonActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    auto MakeLayer = [this](const TCHAR* Name, bool bCastShadow) -> UInstancedStaticMeshComponent*
    {
        UInstancedStaticMeshComponent* Comp = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
        Comp->SetupAttachment(SceneRoot);
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetGenerateOverlapEvents(false);
        Comp->SetCanEverAffectNavigation(false);
        Comp->SetCastShadow(bCastShadow);
        return Comp;
    };

    Floors = MakeLayer(TEXT("FloorInstances"), false);
    RememberedFloors = MakeLayer(TEXT("RememberedFloorInstances"), false);
    Ceilings = MakeLayer(TEXT("CeilingInstances"), false);
    RememberedCeilings = MakeLayer(TEXT("RememberedCeilingInstances"), false);
    Walls = MakeLayer(TEXT("WallInstances"), true);
    RememberedWalls = MakeLayer(TEXT("RememberedWallInstances"), false);
    Veins = MakeLayer(TEXT("VeinInstances"), true);
    RememberedVeins = MakeLayer(TEXT("RememberedVeinInstances"), false);
    DoorFrames = MakeLayer(TEXT("DoorFrameInstances"), true);
    DoorsClosed = MakeLayer(TEXT("DoorClosedInstances"), true);
    DoorsOpen = MakeLayer(TEXT("DoorOpenInstances"), true);
    DoorsBroken = MakeLayer(TEXT("DoorBrokenInstances"), true);
    StairsUp = MakeLayer(TEXT("StairsUpInstances"), true);
    StairsDown = MakeLayer(TEXT("StairsDownInstances"), true);
    Rubble = MakeLayer(TEXT("RubbleInstances"), true);
    Shops = MakeLayer(TEXT("ShopInstances"), true);
    Traps = MakeLayer(TEXT("TrapInstances"), false);
    WallTorches = MakeLayer(TEXT("WallTorchInstances"), false);
    TorchFlames = MakeLayer(TEXT("TorchFlameInstances"), false);
    DarknessShroud = MakeLayer(TEXT("DarknessShroudInstances"), false);
    DarknessBridgeFloors = MakeLayer(TEXT("DarknessBridgeFloorInstances"), false);
    DarknessBridgeCeilings = MakeLayer(TEXT("DarknessBridgeCeilingInstances"), false);

    DungeonFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("DungeonFog"));
    DungeonFog->SetupAttachment(SceneRoot);
    DungeonFog->FogDensity = 0.0035f;
    DungeonFog->FogHeightFalloff = 0.18f;
    DungeonFog->StartDistance = 450.0f;
    DungeonFog->bEnableVolumetricFog = false;

    AmbientSky = CreateDefaultSubobject<USkyLightComponent>(TEXT("AmbientSky"));
    AmbientSky->SetupAttachment(SceneRoot);
    AmbientSky->SetIntensity(0.95f);
    AmbientSky->bLowerHemisphereIsBlack = false;
    AmbientSky->SetLowerHemisphereColor(FLinearColor(0.22f, 0.20f, 0.18f));

    TownSky = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("TownSky"));
    TownSky->SetupAttachment(SceneRoot);
    TownSky->SetVisibility(false);

    TownSunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("TownSunLight"));
    TownSunLight->SetupAttachment(SceneRoot);
    TownSunLight->SetRelativeRotation(FRotator(-28.0f, -45.0f, 0.0f));
    TownSunLight->SetIntensity(2.4f);
    TownSunLight->SetLightColor(FLinearColor(1.0f, 0.84f, 0.66f));
    TownSunLight->SetAtmosphereSunLight(true);
    TownSunLight->SetCastShadows(true);
    TownSunLight->SetVisibility(false);

    AtmospherePostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("AtmospherePostProcess"));
    AtmospherePostProcess->SetupAttachment(SceneRoot);
    AtmospherePostProcess->bUnbound = true;
    AtmospherePostProcess->Settings.bOverride_BloomIntensity = true;
    AtmospherePostProcess->Settings.BloomIntensity = 0.65f;
    AtmospherePostProcess->Settings.bOverride_BloomThreshold = true;
    AtmospherePostProcess->Settings.BloomThreshold = 0.85f;
    AtmospherePostProcess->Settings.bOverride_VignetteIntensity = true;
    AtmospherePostProcess->Settings.VignetteIntensity = 0.36f;
    AtmospherePostProcess->Settings.bOverride_ColorContrast = true;
    AtmospherePostProcess->Settings.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.0f);
    AtmospherePostProcess->Settings.bOverride_ColorSaturation = true;
    AtmospherePostProcess->Settings.ColorSaturation = FVector4(1.04f, 1.04f, 1.04f, 1.0f);
    AtmospherePostProcess->Settings.bOverride_AutoExposureMinBrightness = true;
    AtmospherePostProcess->Settings.AutoExposureMinBrightness = 0.45f;
    AtmospherePostProcess->Settings.bOverride_AutoExposureMaxBrightness = true;
    AtmospherePostProcess->Settings.AutoExposureMaxBrightness = 1.6f;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        FallbackSphereMesh = SphereMesh.Object;
        TorchFlames->SetStaticMesh(FallbackSphereMesh);
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        FallbackCubeMesh = CubeMesh.Object;
        Floors->SetStaticMesh(FallbackCubeMesh);
        RememberedFloors->SetStaticMesh(FallbackCubeMesh);
        Ceilings->SetStaticMesh(FallbackCubeMesh);
        RememberedCeilings->SetStaticMesh(FallbackCubeMesh);
        Walls->SetStaticMesh(FallbackCubeMesh);
        RememberedWalls->SetStaticMesh(FallbackCubeMesh);
        Veins->SetStaticMesh(FallbackCubeMesh);
        RememberedVeins->SetStaticMesh(FallbackCubeMesh);
        DoorFrames->SetStaticMesh(FallbackCubeMesh);
        DoorsClosed->SetStaticMesh(FallbackCubeMesh);
        DoorsOpen->SetStaticMesh(FallbackCubeMesh);
        DoorsBroken->SetStaticMesh(FallbackCubeMesh);
        StairsUp->SetStaticMesh(FallbackCubeMesh);
        StairsDown->SetStaticMesh(FallbackCubeMesh);
        Rubble->SetStaticMesh(FallbackCubeMesh);
        Shops->SetStaticMesh(FallbackCubeMesh);
        Traps->SetStaticMesh(FallbackCubeMesh);
        WallTorches->SetStaticMesh(FallbackCubeMesh);
        DarknessShroud->SetStaticMesh(FallbackCubeMesh);
        DarknessBridgeFloors->SetStaticMesh(FallbackCubeMesh);
        DarknessBridgeCeilings->SetStaticMesh(FallbackCubeMesh);
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BasicMaterial.Succeeded())
    {
        UMaterialInterface* BaseMat = BasicMaterial.Object;
        BaseShapeMat = BaseMat;
        auto MakeColorMat = [this, BaseMat](const FLinearColor& Color) -> UMaterialInstanceDynamic*
        {
            UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseMat, this);
            Mat->SetVectorParameterValue(TEXT("Color"), Color);
            return Mat;
        };

        FallbackFloorMat = MakeColorMat(FLinearColor(0.14f, 0.15f, 0.17f));
        FallbackMemoryFloorMat = MakeColorMat(FLinearColor(0.045f, 0.05f, 0.065f));
        FallbackCeilingMat = MakeColorMat(FLinearColor(0.10f, 0.11f, 0.13f));
        FallbackWallMat = MakeColorMat(FLinearColor(0.28f, 0.29f, 0.32f));
        FallbackMemoryWallMat = MakeColorMat(FLinearColor(0.08f, 0.09f, 0.11f));
        FallbackVeinMat = MakeColorMat(FLinearColor(0.46f, 0.36f, 0.16f));
        FallbackDoorFrameMat = MakeColorMat(FLinearColor(0.14f, 0.13f, 0.12f));
        FallbackDoorMat = MakeColorMat(FLinearColor(0.32f, 0.16f, 0.055f));
        FallbackUpStairMat = MakeColorMat(FLinearColor(0.48f, 0.43f, 0.32f));
        FallbackDownStairMat = MakeColorMat(FLinearColor(0.24f, 0.29f, 0.38f));
        FallbackRubbleMat = MakeColorMat(FLinearColor(0.26f, 0.22f, 0.17f));
        FallbackShopMat = MakeColorMat(FLinearColor(0.62f, 0.36f, 0.08f));
        FallbackTrapMat = MakeColorMat(FLinearColor(0.62f, 0.12f, 0.08f));
        FallbackTorchMat = MakeColorMat(FLinearColor(0.85f, 0.48f, 0.12f));
        FallbackFlameMat = MakeColorMat(FLinearColor(1.0f, 0.72f, 0.18f));
        DarknessShroudMat = MakeColorMat(FLinearColor(0.002f, 0.0025f, 0.004f));

        Floors->SetMaterial(0, FallbackFloorMat);
        RememberedFloors->SetMaterial(0, FallbackMemoryFloorMat);
        Ceilings->SetMaterial(0, FallbackCeilingMat);
        RememberedCeilings->SetMaterial(0, FallbackMemoryFloorMat);
        Walls->SetMaterial(0, FallbackWallMat);
        RememberedWalls->SetMaterial(0, FallbackMemoryWallMat);
        Veins->SetMaterial(0, FallbackVeinMat);
        RememberedVeins->SetMaterial(0, FallbackMemoryWallMat);
        DoorFrames->SetMaterial(0, FallbackDoorFrameMat);
        DoorsClosed->SetMaterial(0, FallbackDoorMat);
        DoorsOpen->SetMaterial(0, FallbackDoorMat);
        DoorsBroken->SetMaterial(0, FallbackDoorMat);
        StairsUp->SetMaterial(0, FallbackUpStairMat);
        StairsDown->SetMaterial(0, FallbackDownStairMat);
        Rubble->SetMaterial(0, FallbackRubbleMat);
        Shops->SetMaterial(0, FallbackShopMat);
        Traps->SetMaterial(0, FallbackTrapMat);
        WallTorches->SetMaterial(0, FallbackTorchMat);
        TorchFlames->SetMaterial(0, FallbackFlameMat);
        DarknessShroud->SetMaterial(0, DarknessShroudMat);
        DarknessBridgeFloors->SetMaterial(0, FallbackMemoryFloorMat);
        DarknessBridgeCeilings->SetMaterial(0, DarknessShroudMat);
    }
}

void AABFPDungeonActor::SetVisualCatalog(UABFPVisualCatalog* InCatalog)
{
    VisualCatalog = InCatalog;
    ConfiguredVisualDepthCategory = INDEX_NONE;
    bHasBuiltTerrainOnce = false;
    if (VisualCatalog)
    {
        CellSize = VisualCatalog->CellSize;
        WallHeight = VisualCatalog->WallHeight;
        EnsureRoomLightPool(VisualCatalog->MaxDynamicRoomLights);
    }
    else
    {
        EnsureRoomLightPool(10);
    }
}

void AABFPDungeonActor::EnsureRoomLightPool(int32 DesiredCount)
{
    if (!GetWorld()) return;
    DesiredCount = FMath::Clamp(DesiredCount, 0, 6);

    while (RoomLights.Num() < DesiredCount)
    {
        const int32 Index = RoomLights.Num();
        UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
        Light->SetupAttachment(SceneRoot);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetCastShadows(false);
        Light->SetUseInverseSquaredFalloff(false);
        Light->SetLightFalloffExponent(1.8f);
        const float SafeTorchIntensity = VisualCatalog ? FMath::Clamp(VisualCatalog->RoomTorchIntensity, 0.5f, 25.0f) : 5.5f;
        Light->SetIntensity(SafeTorchIntensity);
        Light->SetAttenuationRadius(VisualCatalog ? VisualCatalog->RoomTorchRadius : 1850.0f);
        Light->SetLightColor(VisualCatalog ? VisualCatalog->RoomTorchColor : FLinearColor(1.0f, 0.65f, 0.32f));
        Light->SetVisibility(false);
        Light->RegisterComponent();
        RoomLights.Add(Light);
        RoomLightPhases.Add(static_cast<float>(Index) * 1.37f);
    }

    for (int32 Idx = DesiredCount; Idx < RoomLights.Num(); ++Idx)
    {
        if (RoomLights[Idx]) RoomLights[Idx]->SetVisibility(false);
    }
}

void AABFPDungeonActor::ConfigureMeshPool(
    TArray<FABFPMeshPoolEntry>& Pool,
    const TArray<FABFPMeshVariant>& Variants,
    UMaterialInterface* OverrideMat,
    bool bCastShadow,
    const FString& Prefix,
    const FLinearColor* DynamicTint)
{
    for (FABFPMeshPoolEntry& Existing : Pool)
    {
        if (Existing.Component)
        {
            Existing.Component->ClearInstances();
            Existing.Component->DestroyComponent();
        }
    }
    Pool.Empty();

    if (!GetWorld()) return;

    for (int32 Index = 0; Index < Variants.Num(); ++Index)
    {
        const FABFPMeshVariant& Variant = Variants[Index];
        if (!Variant.IsValid()) continue;

        UInstancedStaticMeshComponent* Comp = NewObject<UInstancedStaticMeshComponent>(
            this, *FString::Printf(TEXT("%s_%d"), *Prefix, Index));
        Comp->SetupAttachment(SceneRoot);
        Comp->SetStaticMesh(Variant.Mesh);
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetGenerateOverlapEvents(false);
        Comp->SetCanEverAffectNavigation(false);
        Comp->SetCastShadow(bCastShadow);
        UMaterialInterface* ChosenMat = nullptr;
        if (DynamicTint && Variant.MaterialOverride)
        {
            UMaterialInstanceDynamic* DimMat = UMaterialInstanceDynamic::Create(Variant.MaterialOverride.Get(), this);
            if (DimMat)
            {
                DimMat->SetVectorParameterValue(TEXT("Tint"), *DynamicTint);
                DynamicPoolMaterials.Add(DimMat);
                ChosenMat = DimMat;
            }
        }
        if (!ChosenMat)
        {
            ChosenMat = OverrideMat ? OverrideMat : Variant.MaterialOverride.Get();
        }
        if (ChosenMat)
        {
            const int32 NumMats = FMath::Max(1, Comp->GetNumMaterials());
            for (int32 MatIdx = 0; MatIdx < NumMats; ++MatIdx)
            {
                Comp->SetMaterial(MatIdx, ChosenMat);
            }
        }
        Comp->RegisterComponent();

        FABFPMeshPoolEntry Entry;
        Entry.Component = Comp;
        Entry.LocationOffset = Variant.LocationOffset;
        Entry.RotationOffset = Variant.RotationOffset;
        Entry.Scale = Variant.Scale;
        Pool.Add(MoveTemp(Entry));
    }
}

void AABFPDungeonActor::ConfigureSingleLayer(
    UInstancedStaticMeshComponent* Component,
    const FABFPMeshVariant& Variant,
    UStaticMesh* FallbackMesh,
    UMaterialInterface* FallbackMat,
    UMaterialInterface* OverrideMat)
{
    if (!Component) return;
    if (Variant.IsValid())
    {
        Component->SetStaticMesh(Variant.Mesh);
        if (OverrideMat)
        {
            Component->SetMaterial(0, OverrideMat);
        }
        else if (Variant.MaterialOverride)
        {
            Component->SetMaterial(0, Variant.MaterialOverride);
        }
        else
        {
            Component->EmptyOverrideMaterials();
        }
    }
    else
    {
        if (FallbackMesh) Component->SetStaticMesh(FallbackMesh);
        if (FallbackMat) Component->SetMaterial(0, FallbackMat);
    }
}

void AABFPDungeonActor::ApplyTerrainVisualsForDepth(int32 DungeonDepth)
{
    const int32 DepthCategory = (DungeonDepth == 0) ? 0 : ((DungeonDepth < 5) ? 1 : 2);
    if (ConfiguredVisualDepthCategory == DepthCategory) return;
    ConfiguredVisualDepthCategory = DepthCategory;

    if (TownSky)
    {
        TownSky->SetVisibility(DepthCategory == 0);
    }
    if (TownSunLight)
    {
        TownSunLight->SetVisibility(DepthCategory == 0);
    }
    if (AmbientSky)
    {
        if (DepthCategory == 0)
        {
            AmbientSky->SetIntensity(1.35f);
            AmbientSky->SetLightColor(FLinearColor(0.88f, 0.88f, 0.96f));
            AmbientSky->SetLowerHemisphereColor(FLinearColor(0.22f, 0.20f, 0.18f));
        }
        else if (DepthCategory == 1)
        {
            AmbientSky->SetIntensity(0.24f);
            AmbientSky->SetLightColor(FLinearColor(0.55f, 0.62f, 0.78f));
            AmbientSky->SetLowerHemisphereColor(FLinearColor(0.012f, 0.014f, 0.018f));
        }
        else
        {
            AmbientSky->SetIntensity(0.18f);
            AmbientSky->SetLightColor(FLinearColor(0.75f, 0.45f, 0.36f));
            AmbientSky->SetLowerHemisphereColor(FLinearColor(0.015f, 0.008f, 0.008f));
        }
        AmbientSky->RecaptureSky();
    }
    if (DungeonFog)
    {
        if (DepthCategory == 0)
        {
            DungeonFog->SetFogDensity(0.0014f);
            DungeonFog->StartDistance = 450.0f;
            DungeonFog->SetFogInscatteringColor(FLinearColor(0.28f, 0.26f, 0.24f));
        }
        else if (DepthCategory == 1)
        {
            DungeonFog->SetFogDensity(0.0046f);
            DungeonFog->StartDistance = 280.0f;
            DungeonFog->SetFogInscatteringColor(FLinearColor(0.002f, 0.0025f, 0.004f));
        }
        else
        {
            DungeonFog->SetFogDensity(0.0052f);
            DungeonFog->StartDistance = 260.0f;
            DungeonFog->SetFogInscatteringColor(FLinearColor(0.004f, 0.002f, 0.0025f));
        }
    }
    if (AtmospherePostProcess)
    {
        AtmospherePostProcess->Settings.AutoExposureMinBrightness = (DepthCategory == 0) ? 0.45f : 0.80f;
    }

    if (!VisualCatalog) return;

    DynamicPoolMaterials.Empty();
    const FABFPTerrainVisualSet& TerrainSet = VisualCatalog->GetTerrainSetForDepth(DungeonDepth);
    UMaterialInterface* RememberedMat = TerrainSet.RememberedMaterialOverride ?
        TerrainSet.RememberedMaterialOverride.Get() : FallbackMemoryWallMat.Get();
    const FLinearColor RememberedTint = (DepthCategory == 0)
        ? FLinearColor(0.42f, 0.44f, 0.50f, 1.0f)
        : FLinearColor(0.26f, 0.28f, 0.34f, 1.0f);

    ConfigureMeshPool(CustomFloorPools, TerrainSet.FloorVariants, nullptr, false, TEXT("CustomFloor"));
    ConfigureMeshPool(CustomRememberedFloorPools, TerrainSet.FloorVariants, RememberedMat, false, TEXT("CustomMemFloor"), &RememberedTint);
    ConfigureMeshPool(CustomCeilingPools, TerrainSet.CeilingVariants, nullptr, false, TEXT("CustomCeiling"));
    ConfigureMeshPool(CustomWallPools, TerrainSet.WallVariants, nullptr, true, TEXT("CustomWall"));
    ConfigureMeshPool(CustomRememberedWallPools, TerrainSet.WallVariants, RememberedMat, false, TEXT("CustomMemWall"), &RememberedTint);
    ConfigureMeshPool(CustomVeinPools, TerrainSet.VeinWallVariants, nullptr, true, TEXT("CustomVein"));
    ConfigureMeshPool(CustomRememberedVeinPools, TerrainSet.VeinWallVariants, RememberedMat, false, TEXT("CustomMemVein"), &RememberedTint);
    ConfigureMeshPool(CustomPillarPools, TerrainSet.WallPillarVariants, nullptr, true, TEXT("CustomPillar"));
    ConfigureMeshPool(CustomWallDecorPools, TerrainSet.WallDecorVariants, nullptr, false, TEXT("CustomWallDecor"));

    ConfigureSingleLayer(DoorFrames, TerrainSet.DoorFrame, FallbackCubeMesh,
        TerrainSet.DoorFrame.MaterialOverride ? TerrainSet.DoorFrame.MaterialOverride.Get() : FallbackDoorFrameMat.Get(),
        nullptr);
    ConfigureSingleLayer(DoorsClosed, TerrainSet.DoorClosedPanel, FallbackCubeMesh,
        TerrainSet.DoorClosedPanel.MaterialOverride ? TerrainSet.DoorClosedPanel.MaterialOverride.Get() : FallbackDoorMat.Get(),
        nullptr);
    ConfigureSingleLayer(DoorsOpen, TerrainSet.DoorOpenPanel, FallbackCubeMesh,
        TerrainSet.DoorOpenPanel.MaterialOverride ? TerrainSet.DoorOpenPanel.MaterialOverride.Get() : FallbackDoorMat.Get(),
        nullptr);
    ConfigureSingleLayer(DoorsBroken, TerrainSet.DoorBrokenPanel, FallbackCubeMesh,
        TerrainSet.DoorBrokenPanel.MaterialOverride ? TerrainSet.DoorBrokenPanel.MaterialOverride.Get() : FallbackDoorMat.Get(),
        nullptr);
    ConfigureSingleLayer(StairsUp, TerrainSet.StairsUp, FallbackCubeMesh, FallbackUpStairMat, nullptr);
    ConfigureSingleLayer(StairsDown, TerrainSet.StairsDown, FallbackCubeMesh, FallbackDownStairMat, nullptr);
    ConfigureSingleLayer(Rubble, TerrainSet.Rubble, FallbackCubeMesh, FallbackRubbleMat, nullptr);
    ConfigureSingleLayer(Shops, TerrainSet.ShopFacade, FallbackCubeMesh,
        TerrainSet.ShopFacade.MaterialOverride ? TerrainSet.ShopFacade.MaterialOverride.Get() : FallbackDoorMat.Get(),
        nullptr);
    ConfigureSingleLayer(Traps, TerrainSet.TrapMarker, FallbackCubeMesh, FallbackTrapMat, nullptr);
    ConfigureSingleLayer(WallTorches, TerrainSet.WallTorchProp, FallbackCubeMesh, FallbackTorchMat, nullptr);

    if (TorchFlames)
    {
        if (FallbackSphereMesh) TorchFlames->SetStaticMesh(FallbackSphereMesh);
        UMaterialInterface* FlameMat = VisualCatalog->TorchFlameMaterial ?
            VisualCatalog->TorchFlameMaterial.Get() : FallbackFlameMat.Get();
        if (FlameMat) TorchFlames->SetMaterial(0, FlameMat);
    }
    if (DarknessShroud && DarknessBridgeCeilings && DarknessBridgeFloors)
    {
        UMaterialInterface* VoidMat = TerrainSet.RememberedMaterialOverride ?
            TerrainSet.RememberedMaterialOverride.Get() : DarknessShroudMat.Get();
        if (VoidMat)
        {
            DarknessShroud->SetMaterial(0, VoidMat);
            DarknessBridgeCeilings->SetMaterial(0, VoidMat);
        }
        UMaterialInterface* BridgeFloorMat = (CustomRememberedFloorPools.Num() > 0 && CustomRememberedFloorPools[0].Component)
            ? CustomRememberedFloorPools[0].Component->GetMaterial(0)
            : FallbackMemoryFloorMat.Get();
        if (BridgeFloorMat)
        {
            DarknessBridgeFloors->SetMaterial(0, BridgeFloorMat);
        }
    }
}

FVector AABFPDungeonActor::GridToWorld(int32 X, int32 Y) const
{
    return FVector(X * CellSize, Y * CellSize, 0.0f);
}

void AABFPDungeonActor::UpdateDynamicRoomLights(const FABFPSnapshot& Snapshot,
    const TMap<FIntPoint, const FABFPCell*>& CellMap)
{
    const bool bEnableTorches = !VisualCatalog || VisualCatalog->bSpawnLitRoomTorches;
    const int32 DesiredLights = bEnableTorches ?
        (VisualCatalog ? VisualCatalog->MaxDynamicRoomLights : 10) : 0;
    EnsureRoomLightPool(DesiredLights);

    for (TObjectPtr<UPointLightComponent>& Light : RoomLights)
    {
        if (Light) Light->SetVisibility(false);
    }

    if (!bEnableTorches || DesiredLights <= 0 || Snapshot.DungeonDepth == 0)
    {
        return;
    }

    const FABFPTerrainVisualSet* TerrainSet = VisualCatalog ?
        &VisualCatalog->GetTerrainSetForDepth(Snapshot.DungeonDepth) : nullptr;
    const bool bHasCustomTorchMesh = TerrainSet && TerrainSet->WallTorchProp.IsValid();

    TArray<FIntPoint> PlacedCoords;
    int32 LightIndex = 0;

    static const FIntPoint CardinalDirs[4] = {
        FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0)
    };
    static const float WallFacingYaw[4] = { 90.0f, 180.0f, -90.0f, 0.0f };

    for (const FABFPCell& Cell : Snapshot.Cells)
    {
        if (LightIndex >= RoomLights.Num()) break;
        if (!Cell.bVisible || !Cell.bLit) continue;
        if (Cell.Terrain != EABFPTerrain::Floor && Cell.Terrain != EABFPTerrain::DoorOpen) continue;

        bool bTooClose = false;
        for (const FIntPoint& Used : PlacedCoords)
        {
            if (FMath::Abs(Used.X - Cell.X) + FMath::Abs(Used.Y - Cell.Y) < 4)
            {
                bTooClose = true;
                break;
            }
        }
        if (bTooClose) continue;

        /* Prefer mounting a torch sconce on an adjacent wall facing this lit floor tile. */
        int32 AdjacentWallDir = INDEX_NONE;
        for (int32 DirIdx = 0; DirIdx < 4; ++DirIdx)
        {
            const FABFPCell* const* Neighbor = CellMap.Find(FIntPoint(Cell.X + CardinalDirs[DirIdx].X,
                Cell.Y + CardinalDirs[DirIdx].Y));
            if (Neighbor && IsWallLike(*Neighbor))
            {
                AdjacentWallDir = DirIdx;
                break;
            }
        }
        if (AdjacentWallDir == INDEX_NONE) continue;

        const FVector Tile = GridToWorld(Cell.X, Cell.Y);
        const FIntPoint WallDir = CardinalDirs[AdjacentWallDir];
        const FVector SconceOffset(
            WallDir.X * (CellSize * 0.44f),
            WallDir.Y * (CellSize * 0.44f),
            WallHeight * 0.66f);
        const FRotator SconceRot(0.0f, WallFacingYaw[AdjacentWallDir], 0.0f);

        if (UPointLightComponent* Light = RoomLights[LightIndex])
        {
            Light->SetWorldLocation(Tile + SconceOffset - FVector(WallDir.X * 26.0f, WallDir.Y * 26.0f, -8.0f));
            Light->SetLightColor(VisualCatalog ? VisualCatalog->RoomTorchColor : FLinearColor(1.0f, 0.62f, 0.28f));
            Light->SetAttenuationRadius(VisualCatalog ? VisualCatalog->RoomTorchRadius : 1600.0f);
            Light->SetVisibility(true);
        }

        if (bHasCustomTorchMesh)
        {
            WallTorches->AddInstance(ComposeVariantTransform(
                Tile + SconceOffset, SconceRot, TerrainSet->WallTorchProp));
        }
        else
        {
            WallTorches->AddInstance(FTransform(SconceRot, Tile + SconceOffset, FVector(0.14f, 0.14f, 0.42f)));
        }

        if (TorchFlames)
        {
            const FVector FlamePos = Tile + SconceOffset -
                FVector(WallDir.X * 16.0f, WallDir.Y * 16.0f, -26.0f);
            TorchFlames->AddInstance(FTransform(FRotator::ZeroRotator, FlamePos, FVector(0.14f, 0.14f, 0.22f)));
        }

        PlacedCoords.Add(FIntPoint(Cell.X, Cell.Y));
        ++LightIndex;
    }
}

void AABFPDungeonActor::SpawnProjectileEffect(const FABFPEvent& Event)
{
    if (!GetWorld() || !FallbackSphereMesh || !BaseShapeMat) return;
    if (ActiveProjectiles.Num() >= 12) return;

    const bool bIsExplosion = (Event.Value >= 1000);
    const bool bIsPhysicalMissile = (Event.Value == 0);
    const int32 ProjIdx = bIsExplosion ? (Event.Value - 1000) : (bIsPhysicalMissile ? -1 : (Event.Value - 1));

    /* Map Angband list-elements.h indices to vivid elemental colors:
       0=ACID, 1=ELEC, 2=FIRE, 3=COLD, 4=POIS, 5=LIGHT, 6=DARK, 7=SOUND, 8=SHARD,
       9=NEXUS, 10=NETHER, 11=CHAOS, 12=DISEN, 13=WATER, 14=ICE, 19=PLASMA, 20=METEOR,
       21=MISSILE, 22=MANA, 23=HOLY_ORB, 24=ARROW */
    FLinearColor Color(0.55f, 0.72f, 1.0f);
    if (bIsPhysicalMissile || ProjIdx == 24) Color = FLinearColor(1.0f, 0.84f, 0.45f);
    else if (ProjIdx == 0 || ProjIdx == 4) Color = FLinearColor(0.22f, 0.95f, 0.30f);
    else if (ProjIdx == 1) Color = FLinearColor(0.95f, 0.96f, 0.35f);
    else if (ProjIdx == 2 || ProjIdx == 19 || ProjIdx == 20) Color = FLinearColor(1.0f, 0.36f, 0.08f);
    else if (ProjIdx == 3 || ProjIdx == 13 || ProjIdx == 14) Color = FLinearColor(0.32f, 0.86f, 1.0f);
    else if (ProjIdx == 5 || ProjIdx == 23) Color = FLinearColor(1.0f, 0.92f, 0.50f);
    else if (ProjIdx == 6 || ProjIdx == 10) Color = FLinearColor(0.52f, 0.16f, 0.72f);
    else if (ProjIdx == 21 || ProjIdx == 22) Color = FLinearColor(0.42f, 0.58f, 1.0f);

    const FVector StartPos = GridToWorld(Event.FromX, Event.FromY) + FVector(0.0f, 0.0f, 145.0f);
    const FVector EndPos = GridToWorld(Event.ToX, Event.ToY) + FVector(0.0f, 0.0f, 128.0f);

    UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(this);
    MeshComp->SetupAttachment(SceneRoot);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComp->SetCastShadow(false);
    MeshComp->SetStaticMesh(bIsPhysicalMissile && FallbackCubeMesh ? FallbackCubeMesh.Get() : FallbackSphereMesh.Get());

    UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(BaseShapeMat.Get(), this);
    if (Mat)
    {
        Mat->SetVectorParameterValue(TEXT("Color"), Color);
        MeshComp->SetMaterial(0, Mat);
    }

    const FVector Delta = EndPos - StartPos;
    const FRotator Orient = Delta.IsNearlyZero() ? FRotator::ZeroRotator : Delta.Rotation();
    const FVector BaseScale = bIsExplosion
        ? FVector(0.55f, 0.55f, 0.55f)
        : (bIsPhysicalMissile ? FVector(0.75f, 0.07f, 0.07f) : FVector(0.32f, 0.24f, 0.24f));

    MeshComp->SetWorldLocation(StartPos);
    MeshComp->SetWorldRotation(Orient);
    MeshComp->SetWorldScale3D(BaseScale);
    MeshComp->RegisterComponent();

    UPointLightComponent* LightComp = NewObject<UPointLightComponent>(this);
    LightComp->SetupAttachment(SceneRoot);
    LightComp->SetMobility(EComponentMobility::Movable);
    LightComp->SetCastShadows(false);
    LightComp->SetUseInverseSquaredFalloff(false);
    LightComp->SetLightFalloffExponent(1.6f);
    LightComp->SetLightColor(Color);
    LightComp->SetIntensity(bIsExplosion ? 14.0f : (bIsPhysicalMissile ? 3.5f : 8.5f));
    LightComp->SetAttenuationRadius(bIsExplosion ? 1200.0f : 650.0f);
    LightComp->SetWorldLocation(StartPos);
    LightComp->RegisterComponent();

    FABFPActiveProjectile Proj;
    Proj.Mesh = MeshComp;
    Proj.Light = LightComp;
    Proj.Material = Mat;
    Proj.StartPos = StartPos;
    Proj.EndPos = EndPos;
    Proj.BaseScale = BaseScale;
    Proj.Elapsed = 0.0f;
    Proj.Duration = bIsExplosion ? 0.34f : FMath::Clamp(Delta.Size() / 4200.0f, 0.14f, 0.32f);
    Proj.bIsExplosion = bIsExplosion;
    ActiveProjectiles.Add(MoveTemp(Proj));
}

void AABFPDungeonActor::TickActiveProjectiles(float DeltaSeconds)
{
    for (int32 Idx = ActiveProjectiles.Num() - 1; Idx >= 0; --Idx)
    {
        FABFPActiveProjectile& Proj = ActiveProjectiles[Idx];
        Proj.Elapsed += DeltaSeconds;
        const float Alpha = FMath::Clamp(Proj.Elapsed / FMath::Max(0.01f, Proj.Duration), 0.0f, 1.0f);

        if (Alpha >= 1.0f)
        {
            if (Proj.Mesh) Proj.Mesh->DestroyComponent();
            if (Proj.Light) Proj.Light->DestroyComponent();
            ActiveProjectiles.RemoveAtSwap(Idx);
            continue;
        }

        if (Proj.bIsExplosion)
        {
            const float ScaleMult = FMath::Lerp(0.5f, 3.4f, FMath::Sin(Alpha * PI * 0.65f));
            if (Proj.Mesh) Proj.Mesh->SetWorldScale3D(Proj.BaseScale * ScaleMult);
            if (Proj.Light) Proj.Light->SetIntensity(14.0f * (1.0f - Alpha));
        }
        else
        {
            const FVector Pos = FMath::Lerp(Proj.StartPos, Proj.EndPos, Alpha);
            if (Proj.Mesh) Proj.Mesh->SetWorldLocation(Pos);
            if (Proj.Light)
            {
                Proj.Light->SetWorldLocation(Pos);
                Proj.Light->SetIntensity(8.0f * (1.0f - Alpha * 0.5f));
            }
        }
    }
}

void AABFPDungeonActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FlickerElapsed += DeltaSeconds;
    TickActiveProjectiles(DeltaSeconds);

    const float BaseIntensity = VisualCatalog ? FMath::Clamp(VisualCatalog->RoomTorchIntensity, 0.5f, 25.0f) : 5.5f;
    for (int32 Idx = 0; Idx < RoomLights.Num(); ++Idx)
    {
        UPointLightComponent* Light = RoomLights[Idx];
        if (!Light || !Light->IsVisible()) continue;
        const float Phase = RoomLightPhases.IsValidIndex(Idx) ? RoomLightPhases[Idx] : 0.0f;
        const float Flicker = 1.0f +
            FMath::Sin(FlickerElapsed * 7.3f + Phase) * 0.06f +
            FMath::Sin(FlickerElapsed * 13.7f + Phase * 2.1f) * 0.035f;
        Light->SetIntensity(BaseIntensity * Flicker);
    }
}

void AABFPDungeonActor::ApplySnapshot(const FABFPSnapshot& Snapshot)
{
    if (Snapshot.Birth.bActive && Snapshot.Cells.IsEmpty())
    {
        for (auto& Pair : SpawnedEntities)
        {
            if (IsValid(Pair.Value.Get())) Pair.Value->Destroy();
        }
        SpawnedEntities.Empty();
        LastDepth = INDEX_NONE;
        bHasBuiltTerrainOnce = false;
        return;
    }

    if (LastDepth != Snapshot.DungeonDepth)
    {
        const bool bChangedLevels = (LastDepth != INDEX_NONE);
        for (auto& Pair : SpawnedEntities)
        {
            if (IsValid(Pair.Value.Get())) Pair.Value->Destroy();
        }
        SpawnedEntities.Empty();
        LastDepth = Snapshot.DungeonDepth;
        bHasBuiltTerrainOnce = false;
        if (bChangedLevels && VisualCatalog && VisualCatalog->StairsSound)
        {
            UGameplayStatics::PlaySound2D(this, VisualCatalog->StairsSound);
        }
    }

    ApplyTerrainVisualsForDepth(Snapshot.DungeonDepth);

    /* Hash terrain layout (depth + all cell coordinates/types/visibility/lighting).
       When only monsters/items move (e.g. a pack of creatures moving while the player fights or waits),
       the terrain signature is identical — skip tearing down and rebuilding 1,500+ ISM instances! */
    uint64 TerrainSignature = static_cast<uint64>(Snapshot.DungeonDepth) * 0x9E3779B97F4A7C15ULL ^
        static_cast<uint64>(Snapshot.Cells.Num());
    for (const FABFPCell& Cell : Snapshot.Cells)
    {
        const uint64 Packed =
            (static_cast<uint64>(static_cast<uint32>(Cell.X)) << 32) ^
            (static_cast<uint64>(static_cast<uint32>(Cell.Y)) << 16) ^
            (static_cast<uint64>(Cell.Terrain) << 4) ^
            (Cell.bVisible ? 2ULL : 0ULL) ^
            (Cell.bLit ? 1ULL : 0ULL);
        TerrainSignature ^= Packed + 0x9E3779B97F4A7C15ULL + (TerrainSignature << 6) + (TerrainSignature >> 2);
    }

    const bool bTerrainChanged = !bHasBuiltTerrainOnce || (TerrainSignature != LastTerrainSignature);
    if (bTerrainChanged)
    {
        LastTerrainSignature = TerrainSignature;
        bHasBuiltTerrainOnce = true;

    Floors->ClearInstances();
    RememberedFloors->ClearInstances();
    Ceilings->ClearInstances();
    RememberedCeilings->ClearInstances();
    Walls->ClearInstances();
    RememberedWalls->ClearInstances();
    Veins->ClearInstances();
    RememberedVeins->ClearInstances();
    DoorFrames->ClearInstances();
    DoorsClosed->ClearInstances();
    DoorsOpen->ClearInstances();
    DoorsBroken->ClearInstances();
    StairsUp->ClearInstances();
    StairsDown->ClearInstances();
    Rubble->ClearInstances();
    Shops->ClearInstances();
    Traps->ClearInstances();
    WallTorches->ClearInstances();
    if (TorchFlames) TorchFlames->ClearInstances();
    if (DarknessShroud) DarknessShroud->ClearInstances();
    if (DarknessBridgeFloors) DarknessBridgeFloors->ClearInstances();
    if (DarknessBridgeCeilings) DarknessBridgeCeilings->ClearInstances();

    auto ClearPool = [](TArray<FABFPMeshPoolEntry>& Pool)
    {
        for (FABFPMeshPoolEntry& Entry : Pool)
        {
            if (Entry.Component) Entry.Component->ClearInstances();
        }
    };
    ClearPool(CustomFloorPools);
    ClearPool(CustomRememberedFloorPools);
    ClearPool(CustomCeilingPools);
    ClearPool(CustomWallPools);
    ClearPool(CustomRememberedWallPools);
    ClearPool(CustomVeinPools);
    ClearPool(CustomRememberedVeinPools);
    ClearPool(CustomPillarPools);
    ClearPool(CustomWallDecorPools);

    TMap<FIntPoint, const FABFPCell*> CellMap;
    CellMap.Reserve(Snapshot.Cells.Num());
    for (const FABFPCell& Cell : Snapshot.Cells)
    {
        CellMap.Add(FIntPoint(Cell.X, Cell.Y), &Cell);
    }

    auto IsWallAt = [&CellMap](int32 X, int32 Y) -> bool
    {
        const FABFPCell* const* Found = CellMap.Find(FIntPoint(X, Y));
        return Found && IsWallLike(*Found);
    };

    /* Compute portal yaw so jambs span between opposite walls (works for both 2-wall corridors and 3-wall shop alcoves). */
    auto ComputePortalYaw = [&IsWallAt](int32 X, int32 Y) -> float
    {
        const bool bN = IsWallAt(X, Y - 1);
        const bool bS = IsWallAt(X, Y + 1);
        const bool bW = IsWallAt(X - 1, Y);
        const bool bE = IsWallAt(X + 1, Y);
        const bool bPairNS = bN && bS;
        const bool bPairEW = bW && bE;
        if (bPairNS && !bPairEW) return 90.0f;
        if (bPairEW && !bPairNS) return 0.0f;
        if ((bN || bS) && !(bW || bE)) return 90.0f;
        return 0.0f;
    };

    /* Compute stair entrance yaw so steps face an open walkable neighbor (preferring a back wall behind them). */
    auto ComputeStairEntranceYaw = [&CellMap, &IsWallAt](int32 X, int32 Y) -> float
    {
        static const FIntPoint Dirs[4] = {
            FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1)
        };
        static const float Yaws[4] = { 0.0f, 90.0f, 180.0f, -90.0f };

        auto IsOpenFloorAt = [&CellMap](int32 NX, int32 NY) -> bool
        {
            const FABFPCell* const* Found = CellMap.Find(FIntPoint(NX, NY));
            if (!Found || !*Found) return false;
            const EABFPTerrain T = (*Found)->Terrain;
            return T == EABFPTerrain::Floor || T == EABFPTerrain::DoorOpen ||
                T == EABFPTerrain::Door || T == EABFPTerrain::DoorBroken ||
                T == EABFPTerrain::Shop || T == EABFPTerrain::Trap;
        };

        /* First pass: open neighbor with a wall directly opposite (ideal for 3-wall alcoves & room edges). */
        for (int32 i = 0; i < 4; ++i)
        {
            if (IsOpenFloorAt(X + Dirs[i].X, Y + Dirs[i].Y) &&
                IsWallAt(X - Dirs[i].X, Y - Dirs[i].Y))
            {
                return Yaws[i];
            }
        }
        /* Second pass: any open neighbor. */
        for (int32 i = 0; i < 4; ++i)
        {
            if (IsOpenFloorAt(X + Dirs[i].X, Y + Dirs[i].Y))
            {
                return Yaws[i];
            }
        }
        return 90.0f;
    };

    const FABFPTerrainVisualSet* TerrainSet = VisualCatalog ?
        &VisualCatalog->GetTerrainSetForDepth(Snapshot.DungeonDepth) : nullptr;
    const bool bShouldBuildCeilings = (Snapshot.DungeonDepth > 0) &&
        (!VisualCatalog || VisualCatalog->bBuildCeilings);
    const float CellScaleXY = CellSize / 100.0f;
    const float WallScaleZ = WallHeight / 100.0f;

    auto IsOpenPassableAt = [&CellMap](int32 NX, int32 NY) -> bool
    {
        const FABFPCell* const* Found = CellMap.Find(FIntPoint(NX, NY));
        if (!Found || !*Found) return false;
        const EABFPTerrain T = (*Found)->Terrain;
        return T != EABFPTerrain::Unknown && T != EABFPTerrain::Wall && T != EABFPTerrain::Vein;
    };

    auto EmitFloorAndCeiling = [&](const FABFPCell& TargetCell, const FVector& TargetTile, uint32 TargetHash, bool bSkipFloor)
    {
        if (!bSkipFloor)
        {
            const TArray<FABFPMeshPoolEntry>& FloorPool =
                TargetCell.bVisible ? CustomFloorPools : CustomRememberedFloorPools;
            if (FloorPool.Num() > 0)
            {
                const FABFPMeshPoolEntry& Pick = FloorPool[static_cast<int32>(TargetHash % static_cast<uint32>(FloorPool.Num()))];
                if (Pick.Component)
                {
                    const FRotator RandYaw(0.0f, static_cast<float>((TargetHash >> 8) & 3u) * 90.0f, 0.0f);
                    Pick.Component->AddInstance(ComposePoolTransform(TargetTile, RandYaw, Pick));
                }
            }
            else
            {
                UInstancedStaticMeshComponent* FloorLayer = TargetCell.bVisible ? Floors : RememberedFloors;
                FloorLayer->AddInstance(FTransform(FRotator::ZeroRotator,
                    TargetTile - FVector(0.0f, 0.0f, 6.0f),
                    FVector(CellScaleXY, CellScaleXY, 0.12f)));
            }
        }

        if (bShouldBuildCeilings)
        {
            if (CustomCeilingPools.Num() > 0)
            {
                const FABFPMeshPoolEntry& Pick = CustomCeilingPools[static_cast<int32>((TargetHash >> 4) % static_cast<uint32>(CustomCeilingPools.Num()))];
                if (Pick.Component)
                {
                    Pick.Component->AddInstance(ComposePoolTransform(
                        TargetTile + FVector(0.0f, 0.0f, WallHeight), FRotator::ZeroRotator, Pick));
                }
            }
            else
            {
                UInstancedStaticMeshComponent* CeilingLayer = TargetCell.bVisible ? Ceilings : RememberedCeilings;
                CeilingLayer->AddInstance(FTransform(FRotator::ZeroRotator,
                    TargetTile + FVector(0.0f, 0.0f, WallHeight + 6.0f),
                    FVector(CellScaleXY, CellScaleXY, 0.12f)));
            }
        }
    };

    for (const FABFPCell& Cell : Snapshot.Cells)
    {
        if (Cell.Terrain == EABFPTerrain::Unknown) continue;

        const FVector Tile = GridToWorld(Cell.X, Cell.Y);
        const uint32 CellHash = DeterministicCellHash(Cell.X, Cell.Y, Snapshot.DungeonDepth);

        if (Cell.Terrain == EABFPTerrain::Wall || Cell.Terrain == EABFPTerrain::Vein)
        {
            const bool bIsVein = (Cell.Terrain == EABFPTerrain::Vein);
            const TArray<FABFPMeshPoolEntry>& Pool =
                (bIsVein && Cell.bVisible && CustomVeinPools.Num() > 0) ? CustomVeinPools :
                (bIsVein && !Cell.bVisible && CustomRememberedVeinPools.Num() > 0) ? CustomRememberedVeinPools :
                (Cell.bVisible ? CustomWallPools : CustomRememberedWallPools);

            /* Detect checkerboard diagonal passages where two touching corner walls would seal a valid Angband
               diagonal entry shut in 3D (e.g. room corner entry). Recess the pinching corner(s) cleanly and
               back them with floor/ceiling slabs so the diagonal passage is clearly visible and walkable. */
            const bool bOpenW = IsOpenPassableAt(Cell.X - 1, Cell.Y);
            const bool bOpenE = IsOpenPassableAt(Cell.X + 1, Cell.Y);
            const bool bOpenN = IsOpenPassableAt(Cell.X, Cell.Y - 1);
            const bool bOpenS = IsOpenPassableAt(Cell.X, Cell.Y + 1);
            const bool bPinchNW = bOpenW && bOpenN && IsWallAt(Cell.X - 1, Cell.Y - 1);
            const bool bPinchNE = bOpenE && bOpenN && IsWallAt(Cell.X + 1, Cell.Y - 1);
            const bool bPinchSW = bOpenW && bOpenS && IsWallAt(Cell.X - 1, Cell.Y + 1);
            const bool bPinchSE = bOpenE && bOpenS && IsWallAt(Cell.X + 1, Cell.Y + 1);
            const bool bAnyPinch = bPinchNW || bPinchNE || bPinchSW || bPinchSE;

            UInstancedStaticMeshComponent* WallTargetComp = nullptr;
            const FABFPMeshPoolEntry* WallPick = nullptr;
            if (Pool.Num() > 0)
            {
                WallPick = &Pool[static_cast<int32>(CellHash % static_cast<uint32>(Pool.Num()))];
                WallTargetComp = WallPick->Component;
            }
            else
            {
                WallTargetComp = bIsVein
                    ? (Cell.bVisible ? Veins : RememberedVeins)
                    : (Cell.bVisible ? Walls : RememberedWalls);
            }

            if (WallTargetComp)
            {
                if (!bAnyPinch)
                {
                    if (WallPick)
                    {
                        WallTargetComp->AddInstance(ComposePoolTransform(Tile, FRotator::ZeroRotator, *WallPick));
                    }
                    else
                    {
                        WallTargetComp->AddInstance(FTransform(FRotator::ZeroRotator,
                            Tile + FVector(0.0f, 0.0f, WallHeight * 0.5f),
                            FVector(CellScaleXY, CellScaleXY, WallScaleZ)));
                    }
                }
                else
                {
                    /* Back the recessed corner with watertight floor & ceiling slabs. */
                    EmitFloorAndCeiling(Cell, Tile, CellHash, false);

                    const float Half = CellSize * 0.5f;
                    const float Notch = CellSize * 0.2875f; /* 115 cm cut per side -> ~163 cm diagonal passage */
                    const float X0N = bPinchNW ? (-Half + Notch) : -Half;
                    const float X1N = bPinchNE ? (Half - Notch) : Half;
                    const float X0S = bPinchSW ? (-Half + Notch) : -Half;
                    const float X1S = bPinchSE ? (Half - Notch) : Half;

                    struct FBand { float Y0, Y1, X0, X1; };
                    FBand Bands[3] = {
                        { -Half, -Half + Notch, X0N, X1N },
                        { -Half + Notch, Half - Notch, -Half, Half },
                        { Half - Notch, Half, X0S, X1S }
                    };

                    /* Merge adjacent bands with identical X span (single-corner pinch emits just 2 boxes). */
                    TArray<FBand, TInlineAllocator<3>> Merged;
                    for (int32 b = 0; b < 3; ++b)
                    {
                        if (Merged.Num() > 0 &&
                            FMath::IsNearlyEqual(Merged.Last().X0, Bands[b].X0, 0.1f) &&
                            FMath::IsNearlyEqual(Merged.Last().X1, Bands[b].X1, 0.1f))
                        {
                            Merged.Last().Y1 = Bands[b].Y1;
                        }
                        else
                        {
                            Merged.Add(Bands[b]);
                        }
                    }

                    for (const FBand& B : Merged)
                    {
                        const float CX = 0.5f * (B.X0 + B.X1);
                        const float CY = 0.5f * (B.Y0 + B.Y1);
                        const float SX = (B.X1 - B.X0) / 100.0f;
                        const float SY = (B.Y1 - B.Y0) / 100.0f;
                        WallTargetComp->AddInstance(FTransform(FRotator::ZeroRotator,
                            Tile + FVector(CX, CY, WallHeight * 0.5f),
                            FVector(SX, SY, WallScaleZ)));
                    }
                }
            }
            continue;
        }

        /* Floor & ceiling layers (skip solid floor slab on sunken StairsDown when custom stairwell mesh provides its own collar). */
        const bool bSkipFloorForSunkenStairs = (Cell.Terrain == EABFPTerrain::StairsDown) &&
            TerrainSet && TerrainSet->StairsDown.IsValid();
        EmitFloorAndCeiling(Cell, Tile, CellHash, bSkipFloorForSunkenStairs);

        /* Neighbor-aware door orientation for closed, open, and broken doors. */
        if (Cell.Terrain == EABFPTerrain::Door ||
            Cell.Terrain == EABFPTerrain::DoorOpen ||
            Cell.Terrain == EABFPTerrain::DoorBroken)
        {
            const float DoorYaw = ComputePortalYaw(Cell.X, Cell.Y);
            const FRotator DoorRot(0.0f, DoorYaw, 0.0f);

            if (TerrainSet && TerrainSet->DoorFrame.IsValid())
            {
                DoorFrames->AddInstance(ComposeVariantTransform(Tile, DoorRot, TerrainSet->DoorFrame));
            }
            else
            {
                /* Left jamb, right jamb, and lintel framing the doorway. */
                DoorFrames->AddInstance(FTransform(DoorRot,
                    Tile + DoorRot.RotateVector(FVector(-CellSize * 0.44f, 0.0f, WallHeight * 0.44f)),
                    FVector(0.24f, 0.42f, (WallHeight * 0.88f) / 100.0f)));
                DoorFrames->AddInstance(FTransform(DoorRot,
                    Tile + DoorRot.RotateVector(FVector(CellSize * 0.44f, 0.0f, WallHeight * 0.44f)),
                    FVector(0.24f, 0.42f, (WallHeight * 0.88f) / 100.0f)));
                DoorFrames->AddInstance(FTransform(DoorRot,
                    Tile + FVector(0.0f, 0.0f, WallHeight * 0.92f),
                    FVector(CellScaleXY, 0.44f, 0.48f)));
            }

            if (Cell.Terrain == EABFPTerrain::Door)
            {
                if (TerrainSet && TerrainSet->DoorClosedPanel.IsValid())
                {
                    DoorsClosed->AddInstance(ComposeVariantTransform(Tile, DoorRot, TerrainSet->DoorClosedPanel));
                }
                else
                {
                    DoorsClosed->AddInstance(FTransform(DoorRot,
                        Tile + FVector(0.0f, 0.0f, WallHeight * 0.42f),
                        FVector(CellScaleXY * 0.84f, 0.22f, (WallHeight * 0.84f) / 100.0f)));
                }
            }
            else if (Cell.Terrain == EABFPTerrain::DoorOpen)
            {
                if (TerrainSet && TerrainSet->DoorOpenPanel.IsValid())
                {
                    DoorsOpen->AddInstance(ComposeVariantTransform(Tile, DoorRot, TerrainSet->DoorOpenPanel));
                }
                else
                {
                    /* Swung 90 degrees open against the right jamb so the corridor is passable visually. */
                    const FRotator SwungRot(0.0f, DoorYaw + 90.0f, 0.0f);
                    const FVector SwungOffset = DoorRot.RotateVector(
                        FVector(CellSize * 0.36f, CellSize * 0.38f, WallHeight * 0.42f));
                    DoorsOpen->AddInstance(FTransform(SwungRot, Tile + SwungOffset,
                        FVector(CellScaleXY * 0.80f, 0.20f, (WallHeight * 0.84f) / 100.0f)));
                }
            }
            else if (Cell.Terrain == EABFPTerrain::DoorBroken)
            {
                if (TerrainSet && TerrainSet->DoorBrokenPanel.IsValid())
                {
                    DoorsBroken->AddInstance(ComposeVariantTransform(Tile, DoorRot, TerrainSet->DoorBrokenPanel));
                }
                else
                {
                    const FRotator SplinterRot(12.0f, DoorYaw + 24.0f, 78.0f);
                    DoorsBroken->AddInstance(FTransform(SplinterRot,
                        Tile + DoorRot.RotateVector(FVector(CellSize * 0.22f, CellSize * 0.14f, 18.0f)),
                        FVector(CellScaleXY * 0.48f, 0.18f, 0.65f)));
                }
            }
        }
        else if (Cell.Terrain == EABFPTerrain::StairsUp ||
            Cell.Terrain == EABFPTerrain::StairsDown)
        {
            const bool bUp = (Cell.Terrain == EABFPTerrain::StairsUp);
            const FABFPMeshVariant* CustomStair = TerrainSet ?
                (bUp ? &TerrainSet->StairsUp : &TerrainSet->StairsDown) : nullptr;
            UInstancedStaticMeshComponent* StairLayer = bUp ? StairsUp : StairsDown;
            const FRotator StairRot(0.0f, ComputeStairEntranceYaw(Cell.X, Cell.Y), 0.0f);
            if (CustomStair && CustomStair->IsValid())
            {
                StairLayer->AddInstance(ComposeVariantTransform(Tile, StairRot, *CustomStair));
            }
            else
            {
                for (int32 Step = 0; Step < 5; ++Step)
                {
                    const int32 RampStep = bUp ? Step : 4 - Step;
                    const float OffsetX = (static_cast<float>(Step) - 2.0f) * (CellSize * 0.18f);
                    const float Height = 12.0f + RampStep * 26.0f;
                    StairLayer->AddInstance(FTransform(StairRot,
                        Tile + StairRot.RotateVector(FVector(OffsetX, 0.0f, Height)),
                        FVector(CellSize / 520.0f, CellSize / 120.0f, 0.26f)));
                }
            }
        }
        else if (Cell.Terrain == EABFPTerrain::Rubble)
        {
            if (TerrainSet && TerrainSet->Rubble.IsValid())
            {
                const FRotator RandYaw(0.0f, static_cast<float>(CellHash % 360u), 0.0f);
                Rubble->AddInstance(ComposeVariantTransform(Tile, RandYaw, TerrainSet->Rubble));
            }
            else
            {
                for (int32 Rock = 0; Rock < 4; ++Rock)
                {
                    const float OffsetX = (Rock - 1.5f) * CellSize * 0.16f;
                    const float OffsetY = ((Rock % 2) ? 1.0f : -1.0f) * CellSize * 0.14f;
                    const float Height = 18.0f + Rock * 8.0f;
                    Rubble->AddInstance(FTransform(FRotator(Rock * 9.0f, Rock * 47.0f, Rock * 5.0f),
                        Tile + FVector(OffsetX, OffsetY, Height),
                        FVector(0.32f + Rock * 0.04f, 0.28f, 0.26f + Rock * 0.05f)));
                }
            }
        }
        else if (Cell.Terrain == EABFPTerrain::Shop)
        {
            const float ShopYaw = ComputePortalYaw(Cell.X, Cell.Y);
            const FRotator ShopRot(0.0f, ShopYaw, 0.0f);

            if (TerrainSet && TerrainSet->ShopFacade.IsValid())
            {
                Shops->AddInstance(ComposeVariantTransform(Tile, ShopRot, TerrainSet->ShopFacade));
            }
            else
            {
                /* Full-height warm 2K dark-oak timber shop portal (left jamb, right jamb, heavy header beam & signboard). */
                Shops->AddInstance(FTransform(ShopRot,
                    Tile + ShopRot.RotateVector(FVector(-CellSize * 0.43f, 0.0f, WallHeight * 0.50f)),
                    FVector(0.54f, 0.72f, WallScaleZ)));
                Shops->AddInstance(FTransform(ShopRot,
                    Tile + ShopRot.RotateVector(FVector(CellSize * 0.43f, 0.0f, WallHeight * 0.50f)),
                    FVector(0.54f, 0.72f, WallScaleZ)));
                Shops->AddInstance(FTransform(ShopRot,
                    Tile + FVector(0.0f, 0.0f, WallHeight * 0.90f),
                    FVector(CellScaleXY, 0.84f, 0.64f)));
            }
        }
        else if (Cell.Terrain == EABFPTerrain::Trap)
        {
            if (TerrainSet && TerrainSet->TrapMarker.IsValid())
            {
                Traps->AddInstance(ComposeVariantTransform(Tile, FRotator::ZeroRotator, TerrainSet->TrapMarker));
            }
            else
            {
                Traps->AddInstance(FTransform(FRotator(0.0f, 45.0f, 0.0f),
                    Tile + FVector(0.0f, 0.0f, 4.0f),
                    FVector(CellScaleXY * 0.42f, CellScaleXY * 0.42f, 0.06f)));
            }
        }

        /* Optional corner pillars from VisualCatalog where two perpendicular walls meet. */
        if (CustomPillarPools.Num() > 0)
        {
            const bool bN = IsWallAt(Cell.X, Cell.Y - 1);
            const bool bS = IsWallAt(Cell.X, Cell.Y + 1);
            const bool bW = IsWallAt(Cell.X - 1, Cell.Y);
            const bool bE = IsWallAt(Cell.X + 1, Cell.Y);
            auto PlaceCornerPillar = [&](float CornerX, float CornerY)
            {
                const FABFPMeshPoolEntry& Pick = CustomPillarPools[static_cast<int32>((CellHash >> 10) % static_cast<uint32>(CustomPillarPools.Num()))];
                if (Pick.Component)
                {
                    Pick.Component->AddInstance(ComposePoolTransform(
                        Tile + FVector(CornerX, CornerY, 0.0f), FRotator::ZeroRotator, Pick));
                }
            };
            const float Half = CellSize * 0.48f;
            if (bN && bW) PlaceCornerPillar(-Half, -Half);
            if (bN && bE) PlaceCornerPillar(Half, -Half);
            if (bS && bW) PlaceCornerPillar(-Half, Half);
            if (bS && bE) PlaceCornerPillar(Half, Half);
        }

        /* Non-blocking decorative medieval wall props (banners, barrels, candles) along open room perimeters only
           (never in 1-tile-wide corridors or 90-degree corners where props would block walkways or clip into walls). */
        const uint32 DecorMod = (Snapshot.DungeonDepth == 0) ? 11u : 7u;
        if (Cell.bVisible && Cell.Terrain == EABFPTerrain::Floor && CustomWallDecorPools.Num() > 0 &&
            (CellHash % DecorMod) == 0u)
        {
            static const FIntPoint DecorDirs[4] = {
                FIntPoint(0, -1), FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0)
            };
            static const float DecorYaw[4] = { 90.0f, 180.0f, -90.0f, 0.0f };
            auto IsSolidStoneWallAt = [&CellMap](int32 NX, int32 NY) -> bool
            {
                const FABFPCell* const* Found = CellMap.Find(FIntPoint(NX, NY));
                return Found && *Found && ((*Found)->Terrain == EABFPTerrain::Wall || (*Found)->Terrain == EABFPTerrain::Vein);
            };
            auto IsPlainFloorAt = [&CellMap](int32 NX, int32 NY) -> bool
            {
                const FABFPCell* const* Found = CellMap.Find(FIntPoint(NX, NY));
                return Found && *Found && (*Found)->Terrain == EABFPTerrain::Floor;
            };
            for (int32 DirIdx = 0; DirIdx < 4; ++DirIdx)
            {
                const int32 CheckIdx = static_cast<int32>(((CellHash >> 6) + static_cast<uint32>(DirIdx)) & 3u);
                const FIntPoint Dir = DecorDirs[CheckIdx];
                const FIntPoint Perp(-Dir.Y, Dir.X);
                if (IsSolidStoneWallAt(Cell.X + Dir.X, Cell.Y + Dir.Y) &&
                    IsPlainFloorAt(Cell.X - Dir.X, Cell.Y - Dir.Y) &&
                    IsPlainFloorAt(Cell.X + Perp.X, Cell.Y + Perp.Y) &&
                    IsPlainFloorAt(Cell.X - Perp.X, Cell.Y - Perp.Y))
                {
                    const FABFPMeshPoolEntry& Pick = CustomWallDecorPools[
                        static_cast<int32>((CellHash >> 12) % static_cast<uint32>(CustomWallDecorPools.Num()))];
                    if (Pick.Component)
                    {
                        const FVector EdgePos = Tile + FVector(Dir.X * CellSize * 0.35f, Dir.Y * CellSize * 0.35f, 0.0f);
                        const FRotator EdgeRot(0.0f, DecorYaw[CheckIdx], 0.0f);
                        Pick.Component->AddInstance(ComposePoolTransform(EdgePos, EdgeRot, Pick));
                    }
                    break;
                }
            }
        }
    }

    /* Subterranean fog-of-war darkness shroud (Depth > 0):
       Seal unexplored Unknown tiles bordering known walkable tiles with pitch-black dungeon darkness
       so unlit corridors never leak into an empty skybox horizon; if an Unknown tile sits directly
       between two known walkable tiles (Angband LOS crossing an unlit corridor into a lit room),
       place a dark shadowed floor+ceiling bridge instead of an opaque wall so sightlines stay clear. */
    if (Snapshot.DungeonDepth > 0 && DarknessShroud && DarknessBridgeFloors && DarknessBridgeCeilings)
    {
        auto HasKnownWalkableAlong = [&CellMap](int32 StartX, int32 StartY, int32 StepX, int32 StepY) -> bool
        {
            for (int32 Dist = 1; Dist <= 6; ++Dist)
            {
                const int32 CX = StartX + StepX * Dist;
                const int32 CY = StartY + StepY * Dist;
                const FABFPCell* const* Found = CellMap.Find(FIntPoint(CX, CY));
                if (Found && *Found)
                {
                    const EABFPTerrain T = (*Found)->Terrain;
                    if (T == EABFPTerrain::Wall || T == EABFPTerrain::Vein) return false;
                    if (T != EABFPTerrain::Unknown) return true;
                }
            }
            return false;
        };

        TSet<FIntPoint> VisitedUnknown;
        for (const FABFPCell& Cell : Snapshot.Cells)
        {
            if (Cell.Terrain == EABFPTerrain::Unknown ||
                Cell.Terrain == EABFPTerrain::Wall ||
                Cell.Terrain == EABFPTerrain::Vein)
            {
                continue;
            }
            for (int32 DY = -1; DY <= 1; ++DY)
            {
                for (int32 DX = -1; DX <= 1; ++DX)
                {
                    if (DX == 0 && DY == 0) continue;
                    const FIntPoint NeighborPt(Cell.X + DX, Cell.Y + DY);
                    if (VisitedUnknown.Contains(NeighborPt)) continue;
                    const FABFPCell* const* Found = CellMap.Find(NeighborPt);
                    if (Found && *Found && (*Found)->Terrain != EABFPTerrain::Unknown) continue;
                    VisitedUnknown.Add(NeighborPt);

                    const FVector NeighborTile = GridToWorld(NeighborPt.X, NeighborPt.Y);
                    const bool bCorridorBridgeNS = HasKnownWalkableAlong(NeighborPt.X, NeighborPt.Y, 0, -1) &&
                        HasKnownWalkableAlong(NeighborPt.X, NeighborPt.Y, 0, 1);
                    const bool bCorridorBridgeEW = HasKnownWalkableAlong(NeighborPt.X, NeighborPt.Y, -1, 0) &&
                        HasKnownWalkableAlong(NeighborPt.X, NeighborPt.Y, 1, 0);

                    if (bCorridorBridgeNS || bCorridorBridgeEW)
                    {
                        DarknessBridgeFloors->AddInstance(FTransform(FRotator::ZeroRotator,
                            NeighborTile - FVector(0.0f, 0.0f, 6.0f),
                            FVector(CellScaleXY, CellScaleXY, 0.12f)));
                        DarknessBridgeCeilings->AddInstance(FTransform(FRotator::ZeroRotator,
                            NeighborTile + FVector(0.0f, 0.0f, WallHeight + 6.0f),
                            FVector(CellScaleXY, CellScaleXY, 0.12f)));
                    }
                    else
                    {
                        DarknessShroud->AddInstance(FTransform(FRotator::ZeroRotator,
                            NeighborTile + FVector(0.0f, 0.0f, WallHeight * 0.5f),
                            FVector(CellScaleXY, CellScaleXY, (WallHeight + 24.0f) / 100.0f)));
                    }
                }
            }
        }
    }

    UpdateDynamicRoomLights(Snapshot, CellMap);
    } /* end if (bTerrainChanged) */

    TSet<int64> CurrentIds;
    for (const FABFPEntity& Entity : Snapshot.Entities)
    {
        CurrentIds.Add(Entity.Id);
        const FVector Location = GridToWorld(Entity.X, Entity.Y);
        const FVector ToPlayer((Snapshot.PlayerX - Entity.X) * CellSize,
            (Snapshot.PlayerY - Entity.Y) * CellSize, 0.0f);
        const FRotator Facing = ToPlayer.IsNearlyZero() ?
            FRotator::ZeroRotator : ToPlayer.Rotation();

        TObjectPtr<AActor>* Existing = SpawnedEntities.Find(Entity.Id);
        if (Existing && IsValid(Existing->Get()))
        {
            if (Entity.Type == EABFPEntityType::Monster)
            {
                if (AABFPCreatureActor* Creature = Cast<AABFPCreatureActor>(Existing->Get()))
                {
                    Creature->UpdateEntityState(Entity);
                    Creature->SetTargetLocation(Location, Facing);
                }
                else
                {
                    Existing->Get()->SetActorLocation(Location);
                }
            }
            else
            {
                if (AABFPItemActor* Item = Cast<AABFPItemActor>(Existing->Get()))
                {
                    Item->UpdateEntityState(Entity);
                }
                const FVector CurrentLoc = Existing->Get()->GetActorLocation();
                Existing->Get()->SetActorLocation(FVector(Location.X, Location.Y, CurrentLoc.Z));
            }
            continue;
        }

        if (!GetWorld()) continue;

        FActorSpawnParameters Params;
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AActor* Spawned = nullptr;

        if (Entity.Type == EABFPEntityType::Monster)
        {
            const FABFPCreatureVisualEntry* VisualEntry = VisualCatalog ?
                VisualCatalog->FindCreatureVisual(Entity) : nullptr;
            UClass* SpawnClass = (VisualEntry && VisualEntry->ActorClassOverride) ?
                VisualEntry->ActorClassOverride.Get() : AABFPCreatureActor::StaticClass();
            Spawned = GetWorld()->SpawnActor<AActor>(SpawnClass, Location, Facing, Params);
            if (AABFPCreatureActor* Creature = Cast<AABFPCreatureActor>(Spawned))
            {
                Creature->ApplyVisualEntry(VisualEntry);
                Creature->SetCastShadowsEnabled(Snapshot.DungeonDepth == 0);
                Creature->UpdateEntityState(Entity);
            }
        }
        else
        {
            const FABFPItemVisualEntry* VisualEntry = VisualCatalog ?
                VisualCatalog->FindItemVisual(Entity) : nullptr;
            UClass* SpawnClass = (VisualEntry && VisualEntry->ActorClassOverride) ?
                VisualEntry->ActorClassOverride.Get() : AABFPItemActor::StaticClass();
            Spawned = GetWorld()->SpawnActor<AActor>(SpawnClass, Location, FRotator::ZeroRotator, Params);
            if (AABFPItemActor* Item = Cast<AABFPItemActor>(Spawned))
            {
                Item->ApplyVisualEntry(VisualEntry);
                Item->UpdateEntityState(Entity);
            }
        }

        if (Spawned) SpawnedEntities.Add(Entity.Id, Spawned);
    }

    /*
     * Apply combat/world reactions before pruning entities absent from this
     * snapshot. A lethal hit removes the monster from the authoritative
     * entity list in the same packet as its damage/death events.
     */
    for (const FABFPEvent& Event : Snapshot.Events)
    {
        if (Event.Type == EABFPEventType::Damage)
        {
            if (TObjectPtr<AActor>* Target = SpawnedEntities.Find(Event.TargetId))
            {
                if (AABFPCreatureActor* Creature = Cast<AABFPCreatureActor>(Target->Get()))
                    Creature->PlayHitReaction(Event.Value);
            }
        }
        else if (Event.Type == EABFPEventType::Death)
        {
            if (TObjectPtr<AActor>* Target = SpawnedEntities.Find(Event.SubjectId))
            {
                if (AABFPCreatureActor* Creature = Cast<AABFPCreatureActor>(Target->Get()))
                    Creature->PlayDeathReaction();
                SpawnedEntities.Remove(Event.SubjectId);
            }
        }
        else if (Event.Type == EABFPEventType::TerrainChanged)
        {
            if (VisualCatalog && VisualCatalog->DoorOpenSound)
            {
                UGameplayStatics::PlaySoundAtLocation(this, VisualCatalog->DoorOpenSound,
                    GridToWorld(Event.ToX, Event.ToY));
            }
        }
        else if (Event.Type == EABFPEventType::Pickup)
        {
            if (VisualCatalog && VisualCatalog->PickupDefaultSound)
            {
                UGameplayStatics::PlaySoundAtLocation(this, VisualCatalog->PickupDefaultSound,
                    GridToWorld(Event.ToX, Event.ToY));
            }
        }
        else if (Event.Type == EABFPEventType::Spell)
        {
            SpawnProjectileEffect(Event);
        }
    }

    for (auto It = SpawnedEntities.CreateIterator(); It; ++It)
    {
        if (!CurrentIds.Contains(It.Key()))
        {
            if (IsValid(It.Value())) It.Value()->Destroy();
            It.RemoveCurrent();
        }
    }
}
