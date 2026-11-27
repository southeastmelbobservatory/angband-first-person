#pragma once

#include "CoreMinimal.h"
#include "ABFPProtocol.generated.h"

namespace ABFPItemFlags
{
    constexpr int32 Usable    = 0x001;
    constexpr int32 Wearable  = 0x002;
    constexpr int32 Potion    = 0x004;
    constexpr int32 Scroll    = 0x008;
    constexpr int32 Food      = 0x010;
    constexpr int32 Fuel      = 0x020;
    constexpr int32 Sellable  = 0x040;
    constexpr int32 Ammo      = 0x080;
    constexpr int32 Quiver    = 0x100;
    constexpr int32 Throwable = 0x200;
}

namespace ABFPSpellFlags
{
    constexpr int32 Learned  = 0x01;
    constexpr int32 CanCast  = 0x02;
    constexpr int32 CanStudy = 0x04;
    constexpr int32 NeedsAim = 0x08;
}

UENUM(BlueprintType)
enum class EABFPMenuMode : uint8
{
    None = 0,
    Inventory = 1,
    Equipment = 2,
    ShopBuy = 3,
    ShopSell = 4,
    Spellbook = 5,
    CharacterCreation = 6,
    SaveLoad = 7
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
struct FABFPSpellEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Index = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BookIndex = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Level = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Mana = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FailPct = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Flags = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Info;
};

USTRUCT(BlueprintType)
struct FABFPBirthOption
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Index = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> StatAdj;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HitDie = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ExpFactor = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasMagic = false;
};

USTRUCT(BlueprintType)
struct FABFPBirthState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bActive = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRolled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RaceIndex = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ClassIndex = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PointsLeft = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HitPoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Gold = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> StatsBase;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> StatsTotal;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> PointsSpent;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> PointsInc;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPBirthOption> Races;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPBirthOption> Classes;
};

USTRUCT(BlueprintType)
struct FABFPSaveEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Description;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsCurrent = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsDead = false;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerNewSpells = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCanChooseSpells = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasLauncher = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PlayerName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PlayerRaceName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PlayerClassName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPCell> Cells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPEntity> Entities;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPEvent> Events;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPMessage> Messages;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPInventoryItem> Inventory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FABFPShopState Shop;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPSpellEntry> Spells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FABFPBirthState Birth;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPSaveEntry> Saves;
};
