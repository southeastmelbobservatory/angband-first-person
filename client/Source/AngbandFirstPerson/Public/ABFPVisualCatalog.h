#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ABFPProtocol.h"
#include "ABFPCreatureActor.h"
#include "ABFPItemActor.h"
#include "ABFPVisualCatalog.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UMaterialInterface;
class UAnimInstance;
class UAnimSequenceBase;
class UAnimMontage;
class USoundBase;

USTRUCT(BlueprintType)
struct FABFPMeshVariant
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
    TObjectPtr<UStaticMesh> Mesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
    TObjectPtr<UMaterialInterface> MaterialOverride = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
    FVector LocationOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
    FRotator RotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
    FVector Scale = FVector::OneVector;

    bool IsValid() const { return Mesh != nullptr; }
};

USTRUCT(BlueprintType)
struct FABFPTerrainVisualSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture")
    TArray<FABFPMeshVariant> FloorVariants;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture")
    TArray<FABFPMeshVariant> CeilingVariants;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture")
    TArray<FABFPMeshVariant> WallVariants;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture")
    TArray<FABFPMeshVariant> VeinWallVariants;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture")
    TArray<FABFPMeshVariant> WallPillarVariants;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doors")
    FABFPMeshVariant DoorFrame;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doors")
    FABFPMeshVariant DoorClosedPanel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doors")
    FABFPMeshVariant DoorOpenPanel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Doors")
    FABFPMeshVariant DoorBrokenPanel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Features")
    FABFPMeshVariant StairsUp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Features")
    FABFPMeshVariant StairsDown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Features")
    FABFPMeshVariant Rubble;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Features")
    FABFPMeshVariant ShopFacade;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Features")
    FABFPMeshVariant TrapMarker;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting")
    FABFPMeshVariant WallTorchProp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Materials")
    TObjectPtr<UMaterialInterface> RememberedMaterialOverride = nullptr;
};

USTRUCT(BlueprintType)
struct FABFPCreatureVisualEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match")
    FName EntryName = NAME_None;

    /** Exact Angband monster race index (ridx). Leave at -1 to match by NameContains. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match")
    int32 KindId = -1;

    /** Case-insensitive substring match against Angband monster race name (e.g. "goblin", "mouse", "skeleton"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match")
    FString NameContains;

    /** Optional Blueprint subclass of AABFPCreatureActor for full custom rigs/VFX. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
    TSubclassOf<AABFPCreatureActor> ActorClassOverride;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
    TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
    TSubclassOf<UAnimInstance> AnimClass;

    /** Single-node looping animation used when SkeletalMesh is set and AnimClass is null. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
    TObjectPtr<UAnimSequenceBase> IdleAnim = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
    TObjectPtr<UAnimMontage> HitMontage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation")
    TObjectPtr<UAnimMontage> DeathMontage = nullptr;

    /** Optional static mesh used when SkeletalMesh is unset. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
    TObjectPtr<UStaticMesh> StaticMeshFallback = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
    TObjectPtr<UMaterialInterface> MaterialOverride = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
    FVector LocationOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
    FRotator RotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
    FVector Scale = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> HitSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> DeathSound = nullptr;

    bool HasVisualOverride() const
    {
        return ActorClassOverride != nullptr || SkeletalMesh != nullptr || StaticMeshFallback != nullptr;
    }
};

USTRUCT(BlueprintType)
struct FABFPItemVisualEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match")
    FName EntryName = NAME_None;

    /** Exact Angband object kind index (kidx). Leave at -1 to match by Category/Name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match")
    int32 KindId = -1;

    /** Exact Angband tval category. Leave at -1 to ignore. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match")
    int32 CategoryId = -1;

    /** Exact Angband sval variant (requires CategoryId >= 0). Leave at -1 to match any variant in the category. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match")
    int32 VariantId = -1;

    /** Case-insensitive substring match against known item name (e.g. "potion", "scroll", "ration", "sword"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match")
    FString NameContains;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
    TSubclassOf<AABFPItemActor> ActorClassOverride;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
    TObjectPtr<UStaticMesh> StaticMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visual")
    TObjectPtr<UMaterialInterface> MaterialOverride = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
    FVector LocationOffset = FVector(0.0f, 0.0f, 8.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
    FRotator RotationOffset = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Transform")
    FVector Scale = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    bool bFloatAndSpin = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    bool bEmitSubtleLight = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
    FLinearColor AccentLightColor = FLinearColor(0.35f, 0.78f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> PickupSound = nullptr;

    bool HasVisualOverride() const
    {
        return ActorClassOverride != nullptr || StaticMesh != nullptr;
    }
};

/**
 * Data-driven catalog mapping authoritative Angband terrain, monsters, and items
 * to high-fidelity Unreal Static Meshes, Skeletal Meshes, Materials, Blueprints, and Audio.
 */
UCLASS(BlueprintType)
class ANGBANDFIRSTPERSON_API UABFPVisualCatalog : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dimensions")
    float CellSize = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dimensions")
    float WallHeight = 320.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Architecture")
    bool bBuildCeilings = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting")
    bool bSpawnLitRoomTorches = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting", meta=(ClampMin="0", ClampMax="32"))
    int32 MaxDynamicRoomLights = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting")
    float RoomTorchIntensity = 5.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting")
    float RoomTorchRadius = 1850.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lighting")
    FLinearColor RoomTorchColor = FLinearColor(1.0f, 0.62f, 0.28f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain")
    FABFPTerrainVisualSet DungeonTerrain;

    /** Optional visual override used for Depth 0 (Town). Unset slots fall back to DungeonTerrain. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain")
    FABFPTerrainVisualSet TownTerrain;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Creatures")
    TArray<FABFPCreatureVisualEntry> Creatures;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Creatures")
    FABFPCreatureVisualEntry DefaultCreature;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items")
    TArray<FABFPItemVisualEntry> Items;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Items")
    FABFPItemVisualEntry DefaultItem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> FootstepSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> AttackSwingSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> PlayerHitSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> DoorOpenSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> StairsSound = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
    TObjectPtr<USoundBase> PickupDefaultSound = nullptr;

    const FABFPCreatureVisualEntry* FindCreatureVisual(const FABFPEntity& Entity) const;
    const FABFPItemVisualEntry* FindItemVisual(const FABFPEntity& Entity) const;
    const FABFPTerrainVisualSet& GetTerrainSetForDepth(int32 DungeonDepth) const;
};
