#include "ABFPCreatureActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AABFPCreatureActor::AABFPCreatureActor()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

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
    UStaticMesh* Cylinder = CylinderMesh.Succeeded() ? CylinderMesh.Object : Sphere;

    Body = MakePart(TEXT("Body"), Sphere, FVector(0, 0, 105), FVector(0.48f, 0.38f, 0.66f));
    Head = MakePart(TEXT("Head"), Sphere, FVector(0, 0, 205), FVector(0.30f, 0.29f, 0.32f));
    LeftArm = MakePart(TEXT("LeftArm"), Cylinder, FVector(0, -42, 112), FVector(0.12f, 0.12f, 0.48f));
    RightArm = MakePart(TEXT("RightArm"), Cylinder, FVector(0, 42, 112), FVector(0.12f, 0.12f, 0.48f));
    LeftLeg = MakePart(TEXT("LeftLeg"), Cylinder, FVector(0, -18, 34), FVector(0.14f, 0.14f, 0.38f));
    RightLeg = MakePart(TEXT("RightLeg"), Cylinder, FVector(0, 18, 34), FVector(0.14f, 0.14f, 0.38f));
    LeftEye = MakePart(TEXT("LeftEye"), Sphere, FVector(24, -12, 211), FVector(0.055f, 0.055f, 0.055f));
    RightEye = MakePart(TEXT("RightEye"), Sphere, FVector(24, 12, 211), FVector(0.055f, 0.055f, 0.055f));
}

void AABFPCreatureActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Elapsed += DeltaSeconds;

    const float Breath = FMath::Sin(Elapsed * 2.1f) * 3.0f;
    const float ArmSwing = FMath::Sin(Elapsed * 1.3f) * 7.0f;

    if (Body) Body->SetRelativeLocation(FVector(0, 0, 105 + Breath));
    if (Head) Head->SetRelativeRotation(FRotator(0, FMath::Sin(Elapsed * 0.7f) * 5.0f, 0));
    if (LeftArm) LeftArm->SetRelativeRotation(FRotator(ArmSwing, 0, -8));
    if (RightArm) RightArm->SetRelativeRotation(FRotator(-ArmSwing, 0, 8));
}
