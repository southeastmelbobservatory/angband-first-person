#pragma once

#include "CoreMinimal.h"
#include "ABFPProtocol.generated.h"

UENUM(BlueprintType)
enum class EABFPTerrain : uint8
{
    Unknown,
    Floor,
    Wall,
    Door,
    StairsUp,
    StairsDown,
    Rubble,
    Shop
};

UENUM(BlueprintType)
enum class EABFPEntityType : uint8
{
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
};

USTRUCT(BlueprintType)
struct FABFPEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Type = 0;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPCell> Cells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPEntity> Entities;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FABFPEvent> Events;
};
