#include "ABFPDungeonActor.h"

#include "ABFPCreatureActor.h"
#include "ABFPItemActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/ConstructorHelpers.h"

AABFPDungeonActor::AABFPDungeonActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    Floors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FloorInstances"));
    Floors->SetupAttachment(SceneRoot);
    Walls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WallInstances"));
    Walls->SetupAttachment(SceneRoot);
    Doors = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("DoorInstances"));
    Doors->SetupAttachment(SceneRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeMesh.Succeeded())
    {
        Floors->SetStaticMesh(CubeMesh.Object);
        Walls->SetStaticMesh(CubeMesh.Object);
        Doors->SetStaticMesh(CubeMesh.Object);
    }

    Floors->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Walls->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Doors->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Floors->SetCastShadow(true);
    Walls->SetCastShadow(true);
    Doors->SetCastShadow(true);
}

FVector AABFPDungeonActor::GridToWorld(int32 X, int32 Y,
    int32 PlayerX, int32 PlayerY) const
{
    return FVector((X - PlayerX) * CellSize, (Y - PlayerY) * CellSize, 0.0f);
}

void AABFPDungeonActor::ApplySnapshot(const FABFPSnapshot& Snapshot)
{
    Floors->ClearInstances();
    Walls->ClearInstances();
    Doors->ClearInstances();

    for (const FABFPCell& Cell : Snapshot.Cells)
    {
        if (Cell.Terrain == EABFPTerrain::Unknown) continue;

        const FVector Tile = GridToWorld(Cell.X, Cell.Y,
            Snapshot.PlayerX, Snapshot.PlayerY);

        if (Cell.Terrain == EABFPTerrain::Wall)
        {
            Walls->AddInstance(FTransform(FRotator::ZeroRotator,
                Tile + FVector(0, 0, 150.0f),
                FVector(CellSize / 100.0f, CellSize / 100.0f, 3.0f)));
            continue;
        }

        Floors->AddInstance(FTransform(FRotator::ZeroRotator,
            Tile - FVector(0, 0, 6.0f),
            FVector(CellSize / 100.0f, CellSize / 100.0f, 0.12f)));

        if (Cell.Terrain == EABFPTerrain::Door)
        {
            Doors->AddInstance(FTransform(FRotator::ZeroRotator,
                Tile + FVector(0, 0, 115.0f),
                FVector(CellSize / 100.0f, 0.25f, 2.3f)));
        }
    }

    TSet<int64> CurrentIds;
    for (const FABFPEntity& Entity : Snapshot.Entities)
    {
        CurrentIds.Add(Entity.Id);
        const FVector Location = GridToWorld(Entity.X, Entity.Y,
            Snapshot.PlayerX, Snapshot.PlayerY);

        TObjectPtr<AActor>* Existing = SpawnedEntities.Find(Entity.Id);
        if (Existing && IsValid(Existing->Get()))
        {
            Existing->Get()->SetActorLocation(Location);
            continue;
        }

        FActorSpawnParameters Params;
        Params.Owner = this;
        AActor* Spawned = nullptr;

        if (Entity.Type == EABFPEntityType::Monster)
        {
            Spawned = GetWorld()->SpawnActor<AABFPCreatureActor>(
                AABFPCreatureActor::StaticClass(), Location,
                FRotator::ZeroRotator, Params);
        }
        else
        {
            Spawned = GetWorld()->SpawnActor<AABFPItemActor>(
                AABFPItemActor::StaticClass(), Location,
                FRotator::ZeroRotator, Params);
        }

        if (Spawned) SpawnedEntities.Add(Entity.Id, Spawned);
    }

    for (auto It = SpawnedEntities.CreateIterator(); It; ++It)
    {
        if (!CurrentIds.Contains(It.Key()))
        {
            if (IsValid(It.Value())) It->Value()->Destroy();
            It.RemoveCurrent();
        }
    }
}
