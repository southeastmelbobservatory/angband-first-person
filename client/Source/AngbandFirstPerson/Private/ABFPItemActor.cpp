#include "ABFPItemActor.h"
#include "ABFPVisualCatalog.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AABFPItemActor::AABFPItemActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    CustomMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CustomItemMesh"));
    CustomMeshComp->SetupAttachment(SceneRoot);
    CustomMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CustomMeshComp->SetGenerateOverlapEvents(false);
    CustomMeshComp->SetCanEverAffectNavigation(false);
    CustomMeshComp->SetCastShadow(false);
    CustomMeshComp->SetVisibility(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(
        TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    CachedSphereMesh = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
    CachedConeMesh = ConeMesh.Succeeded() ? ConeMesh.Object.Get() : CachedSphereMesh.Get();
    CachedBasicMat = BasicMaterial.Succeeded() ? BasicMaterial.Object : nullptr;
}

void AABFPItemActor::EnsureAccentLight()
{
    if (AccentLight || !GetWorld()) return;
    AccentLight = NewObject<UPointLightComponent>(this);
    AccentLight->SetupAttachment(SceneRoot);
    AccentLight->SetMobility(EComponentMobility::Movable);
    AccentLight->SetRelativeLocation(FVector(0.0f, 0.0f, 32.0f));
    AccentLight->SetIntensity(350.0f);
    AccentLight->SetAttenuationRadius(260.0f);
    AccentLight->SetCastShadows(false);
    AccentLight->SetVisibility(false);
    AccentLight->RegisterComponent();
}

void AABFPItemActor::EnsureProceduralParts()
{
    if (Core || !GetWorld()) return;

    auto MakePart = [this](UStaticMesh* Mesh, const FVector& Location,
        const FVector& Scale) -> UStaticMeshComponent*
    {
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
        Part->SetupAttachment(SceneRoot);
        if (Mesh) Part->SetStaticMesh(Mesh);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeScale3D(Scale);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);
        Part->SetCastShadow(false);
        Part->RegisterComponent();
        return Part;
    };

    Core = MakePart(CachedSphereMesh, FVector(0, 0, 35), FVector(0.22f, 0.22f, 0.22f));
    ShardA = MakePart(CachedConeMesh, FVector(0, -16, 43), FVector(0.10f, 0.10f, 0.20f));
    ShardB = MakePart(CachedConeMesh, FVector(0, 16, 43), FVector(0.10f, 0.10f, 0.20f));

    if (CachedBasicMat)
    {
        CoreMaterial = UMaterialInstanceDynamic::Create(CachedBasicMat, this);
        ShardMaterial = UMaterialInstanceDynamic::Create(CachedBasicMat, this);
        if (CoreMaterial)
        {
            CoreMaterial->SetVectorParameterValue(TEXT("Color"), BaseColor);
            Core->SetMaterial(0, CoreMaterial);
        }
        if (ShardMaterial)
        {
            ShardMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.28f, 0.82f, 0.94f));
            ShardA->SetMaterial(0, ShardMaterial);
            ShardB->SetMaterial(0, ShardMaterial);
        }
    }
}

void AABFPItemActor::ApplyVisualEntry(const FABFPItemVisualEntry* Entry)
{
    if (!Entry || (!Entry->StaticMesh && !Entry->ActorClassOverride))
    {
        bUsingCustomVisual = false;
        bFloatAndSpin = true;
        SetActorTickEnabled(true);
        BaseHeightZ = 38.0f;
        if (CustomMeshComp) CustomMeshComp->SetVisibility(false);
        if (AccentLight) AccentLight->SetVisibility(false);
        EnsureProceduralParts();
        if (Core) Core->SetVisibility(true);
        return;
    }

    bUsingCustomVisual = true;
    bFloatAndSpin = Entry->bFloatAndSpin;
    SetActorTickEnabled(bFloatAndSpin);
    BaseHeightZ = Entry->LocationOffset.Z;

    if (Core) Core->SetVisibility(false);
    if (ShardA) ShardA->SetVisibility(false);
    if (ShardB) ShardB->SetVisibility(false);

    if (Entry->StaticMesh && CustomMeshComp)
    {
        CustomMeshComp->SetStaticMesh(Entry->StaticMesh);
        CustomMeshComp->SetRelativeLocation(FVector(Entry->LocationOffset.X, Entry->LocationOffset.Y, 0.0f));
        CustomMeshComp->SetRelativeRotation(Entry->RotationOffset);
        CustomMeshComp->SetRelativeScale3D(Entry->Scale);
        if (Entry->MaterialOverride)
        {
            CustomMeshComp->SetMaterial(0, Entry->MaterialOverride);
        }
        CustomMeshComp->SetVisibility(true);
    }

    if (Entry->bEmitSubtleLight)
    {
        EnsureAccentLight();
        if (AccentLight)
        {
            AccentLight->SetLightColor(Entry->AccentLightColor);
            AccentLight->SetVisibility(true);
        }
    }
    else if (AccentLight)
    {
        AccentLight->SetVisibility(false);
    }

    const FVector Loc = GetActorLocation();
    SetActorLocation(FVector(Loc.X, Loc.Y, BaseHeightZ));
}

void AABFPItemActor::UpdateEntityState(const FABFPEntity& Entity)
{
    SetItemAppearance(Entity.Name, Entity.CategoryId, Entity.VariantId);
    OnItemConfigured(Entity);
}

void AABFPItemActor::SetItemAppearance(const FString& Name,
    int32 CategoryId, int32 VariantId)
{
    if (ItemName == Name && ItemCategoryId == CategoryId &&
        ItemVariantId == VariantId) return;

    ItemName = Name;
    ItemCategoryId = CategoryId;
    ItemVariantId = VariantId;

    if (bUsingCustomVisual) return;
    EnsureProceduralParts();

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
    if (!bFloatAndSpin) return;

    Elapsed += DeltaSeconds;
    SetActorRotation(FRotator(0, Elapsed * 32.0f, 0));
    SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y,
        BaseHeightZ + FMath::Sin(Elapsed * 2.0f) * 4.0f));
}
