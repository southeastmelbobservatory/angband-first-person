#include "ABFPCreatureActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

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

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BasicMaterial.Succeeded())
    {
        UMaterialInstanceDynamic* HideMaterial = UMaterialInstanceDynamic::Create(BasicMaterial.Object, this);
        UMaterialInstanceDynamic* EyeMaterial = UMaterialInstanceDynamic::Create(BasicMaterial.Object, this);
        HideMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.19f, 0.11f));
        EyeMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.55f, 0.08f));
        Body->SetMaterial(0, HideMaterial);
        Head->SetMaterial(0, HideMaterial);
        LeftArm->SetMaterial(0, HideMaterial);
        RightArm->SetMaterial(0, HideMaterial);
        LeftLeg->SetMaterial(0, HideMaterial);
        RightLeg->SetMaterial(0, HideMaterial);
        LeftEye->SetMaterial(0, EyeMaterial);
        RightEye->SetMaterial(0, EyeMaterial);
    }
}

void AABFPCreatureActor::PlayHitReaction(int32 Damage)
{
    (void)Damage;
    DamageFlashSeconds = 0.22f;
}

void AABFPCreatureActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Elapsed += DeltaSeconds;
    DamageFlashSeconds = FMath::Max(0.0f, DamageFlashSeconds - DeltaSeconds);

    const float Breath = FMath::Sin(Elapsed * 2.1f) * 3.0f;
    const float ArmSwing = FMath::Sin(Elapsed * 1.3f) * 7.0f;

    if (Body) Body->SetRelativeLocation(FVector(0, 0, 105 + Breath));
    if (Head) Head->SetRelativeRotation(FRotator(0, FMath::Sin(Elapsed * 0.7f) * 5.0f, 0));
    if (LeftArm) LeftArm->SetRelativeRotation(FRotator(ArmSwing, 0, -8));
    if (RightArm) RightArm->SetRelativeRotation(FRotator(-ArmSwing, 0, 8));

    if (DamageFlashSeconds > 0.0f)
    {
        const float Punch = 1.0f + FMath::Sin(Elapsed * 45.0f) * 0.09f;
        if (Body) Body->SetRelativeScale3D(FVector(0.48f * Punch, 0.38f * Punch, 0.66f * Punch));
        if (Head) Head->SetRelativeScale3D(FVector(0.30f * Punch, 0.29f * Punch, 0.32f * Punch));
    }
    else
    {
        if (Body) Body->SetRelativeScale3D(FVector(0.48f, 0.38f, 0.66f));
        if (Head) Head->SetRelativeScale3D(FVector(0.30f, 0.29f, 0.32f));
    }
}
