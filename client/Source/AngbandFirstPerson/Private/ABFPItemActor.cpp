#include "ABFPItemActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AABFPItemActor::AABFPItemActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(
        TEXT("/Engine/BasicShapes/Cone.Cone"));

    auto MakePart = [this](const TCHAR* Name, UStaticMesh* Mesh,
        const FVector& Location, const FVector& Scale) -> UStaticMeshComponent*
    {
        UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(SceneRoot);
        if (Mesh) Part->SetStaticMesh(Mesh);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCastShadow(true);
        return Part;
    };

    UStaticMesh* Sphere = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
    UStaticMesh* Cone = ConeMesh.Succeeded() ? ConeMesh.Object : Sphere;
    Core = MakePart(TEXT("Core"), Sphere, FVector(0, 0, 35), FVector(0.22f, 0.22f, 0.22f));
    ShardA = MakePart(TEXT("ShardA"), Cone, FVector(0, -16, 43), FVector(0.10f, 0.10f, 0.20f));
    ShardB = MakePart(TEXT("ShardB"), Cone, FVector(0, 16, 43), FVector(0.10f, 0.10f, 0.20f));
}

void AABFPItemActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Elapsed += DeltaSeconds;
    SetActorRotation(FRotator(0, Elapsed * 32.0f, 0));
    SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y,
        38.0f + FMath::Sin(Elapsed * 2.0f) * 4.0f));
}
