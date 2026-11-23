#pragma once

#include "CoreMinimal.h"
#include "ABFPProtocol.generated.h"

namespace ABFPItemFlags
{
    constexpr int32 Usable   = 0x01;
    constexpr int32 Wearable = 0x02;
    constexpr int32 Potion   = 0x04;
    constexpr int32 Scroll   = 0x08;
    constexpr int32 Food     = 0x10;
    constexpr int32 Fuel     = 0x20;
    constexpr int32 Sellable = 0x40;
}

UENUM(BlueprintType)
enum class EABFPMenuMode : uint8
{
    None = 0,
    Inventory = 1,
    Equipment = 2,
    ShopBuy = 3,
    ShopSell = 4
};

UENUM(BlueprintType)
enum class EABFPTerrain : uint8
{
    Unknown = 0,
    Floor = 1,
    Wall = 2,
    Door = 3,
    StairsUp = 4,
    StairsDown = 5,
    Rubble = 6,
    Shop = 7,
    DoorOpen = 8,
    DoorBroken = 9,
    Trap = 10,
    Vein = 11
};

UENUM(BlueprintType)
enum class EABFPEntityType : uint8
{
    None = 0,
    Monster = 1,
    Object = 2
};

USTRUCT(BlueprintType)
struct FABFPCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 X = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Y = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EABFPTerrain Terrain = EABFPTerrain::Unknown;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bVisible = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLit = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOccupiedByMonster = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOccupiedByObject = false;
};

USTRUCT(BlueprintType)
struct FABFPEntity
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 Id = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 X = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Y = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EABFPEntityType Type = EABFPEntityType::Monster;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 KindId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CategoryId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 VariantId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HitPoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxHitPoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
};

UENUM(BlueprintType)
enum class EABFPEventType : uint8
{
    None = 0,
    EntityMoved = 1,
    Attack = 2,
    Damage = 3,
    Spell = 4,
    Pickup = 5,
    Death = 6,
    TerrainChanged = 7
};

USTRUCT(BlueprintType)
struct FABFPEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EABFPEventType Type = EABFPEventType::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 Turn = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 SubjectId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 TargetId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FromX = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FromY = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ToX = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ToY = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Value = 0;
};

USTRUCT(BlueprintType)
struct FABFPMessage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Type = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Text;
};

USTRUCT(BlueprintType)
struct FABFPInventoryItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Index = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEquipped = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Flags = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CategoryId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 VariantId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SellPrice = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SlotName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
};

USTRUCT(BlueprintType)
struct FABFPShopItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Index = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Price = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CategoryId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 VariantId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
};

USTRUCT(BlueprintType)
struct FABFPShopState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bActive = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsHome = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Owner;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPShopItem> Items;
};

USTRUCT(BlueprintType)
struct FABFPSnapshot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 Turn = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInitialState = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DungeonDepth = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerX = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerY = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerHitPoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerMaxHitPoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerLevel = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerGold = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerMana = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerMaxMana = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPlayerDead = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerExp = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerMaxExp = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerArmorClass = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerSpeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerFoodPct = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPCell> Cells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPEntity> Entities;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPEvent> Events;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPMessage> Messages;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPInventoryItem> Inventory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FABFPShopState Shop;
};
