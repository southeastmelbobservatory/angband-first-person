#include "ABFPItemActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

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

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BasicMaterial.Succeeded())
    {
        CoreMaterial = UMaterialInstanceDynamic::Create(BasicMaterial.Object, this);
        ShardMaterial = UMaterialInstanceDynamic::Create(BasicMaterial.Object, this);
        CoreMaterial->SetVectorParameterValue(TEXT("Color"), BaseColor);
        ShardMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.28f, 0.82f, 0.94f));
        Core->SetMaterial(0, CoreMaterial);
        ShardA->SetMaterial(0, ShardMaterial);
        ShardB->SetMaterial(0, ShardMaterial);
    }
}

void AABFPItemActor::SetItemAppearance(const FString& Name,
    int32 CategoryId, int32 VariantId)
{
    if (ItemName == Name && ItemCategoryId == CategoryId &&
        ItemVariantId == VariantId) return;

    ItemName = Name;
    ItemCategoryId = CategoryId;
    ItemVariantId = VariantId;
    const FString Lower = Name.ToLower();

    const bool bPotion = Lower.Contains(TEXT("potion"));
    const bool bScroll = Lower.Contains(TEXT("scroll"));
    const bool bFood = Lower.Contains(TEXT("food")) ||
        Lower.Contains(TEXT("ration")) || Lower.Contains(TEXT("mushroom"));
    const bool bGold = Lower.Contains(TEXT("gold")) ||
        Lower.Contains(TEXT("treasure"));
    const bool bWeapon = Lower.Contains(TEXT("sword")) ||
        Lower.Contains(TEXT("dagger")) || Lower.Contains(TEXT("weapon")) ||
        Lower.Contains(TEXT("arrow")) || Lower.Contains(TEXT("bolt"));
    const bool bArmor = Lower.Contains(TEXT("armour")) ||
        Lower.Contains(TEXT("armor")) || Lower.Contains(TEXT("shield")) ||
        Lower.Contains(TEXT("helm"));
    const bool bRing = Lower.Contains(TEXT("ring"));

    BaseColor = bPotion ?
        (VariantId % 3 == 0 ? FLinearColor(0.12f, 0.30f, 0.88f) :
         VariantId % 3 == 1 ? FLinearColor(0.12f, 0.72f, 0.42f) :
                             FLinearColor(0.58f, 0.16f, 0.82f)) :
        bScroll ? FLinearColor(0.72f, 0.57f, 0.32f) :
        bFood ? FLinearColor(0.68f, 0.32f, 0.08f) :
        bGold ? FLinearColor(0.95f, 0.66f, 0.08f) :
        bWeapon || bArmor ? FLinearColor(0.48f, 0.58f, 0.66f) :
        bRing ? FLinearColor(0.90f, 0.68f, 0.18f) :
                FLinearColor(0.08f, 0.52f, 0.72f);

    if (CoreMaterial)
        CoreMaterial->SetVectorParameterValue(TEXT("Color"), BaseColor);
    if (ShardMaterial)
        ShardMaterial->SetVectorParameterValue(TEXT("Color"),
            bWeapon || bArmor ? FLinearColor(0.78f, 0.84f, 0.92f) :
            bGold || bRing ? FLinearColor(1.0f, 0.82f, 0.28f) :
            bScroll ? FLinearColor(0.92f, 0.80f, 0.56f) :
            FLinearColor(0.28f, 0.82f, 0.94f));

    if (Core) {
        Core->SetRelativeScale3D(
            bScroll ? FVector(0.13f, 0.13f, 0.42f) :
            bWeapon ? FVector(0.10f, 0.10f, 0.55f) :
            bRing ? FVector(0.13f, 0.13f, 0.13f) :
            bFood ? FVector(0.26f, 0.24f, 0.20f) :
                    FVector(0.22f, 0.22f, 0.22f));
    }
    if (ShardA && ShardB) {
        const bool bShowShards = !bScroll && !bFood;
        ShardA->SetVisibility(bShowShards);
        ShardB->SetVisibility(bShowShards);
        ShardA->SetRelativeScale3D(bWeapon ?
            FVector(0.07f, 0.07f, 0.62f) : FVector(0.10f, 0.10f, 0.20f));
        ShardB->SetRelativeScale3D(bWeapon ?
            FVector(0.16f, 0.16f, 0.10f) : FVector(0.10f, 0.10f, 0.20f));
        ShardA->SetRelativeLocation(bWeapon ?
            FVector(0, 0, 72) : FVector(0, -16, 43));
        ShardB->SetRelativeLocation(bWeapon ?
            FVector(0, 0, 22) : FVector(0, 16, 43));
    }
}

void AABFPItemActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Elapsed += DeltaSeconds;
    SetActorRotation(FRotator(0, Elapsed * 32.0f, 0));
    SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y,
        38.0f + FMath::Sin(Elapsed * 2.0f) * 4.0f));
}
