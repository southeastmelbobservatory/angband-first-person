#include "ABFPCreatureActor.h"
#include "ABFPVisualCatalog.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AABFPCreatureActor::AABFPCreatureActor()
{
    PrimaryActorTick.bCanEverTick = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    SkeletalMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
    SkeletalMeshComp->SetupAttachment(SceneRoot);
    SkeletalMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SkeletalMeshComp->SetCastShadow(true);
    SkeletalMeshComp->SetVisibility(false);

    CustomStaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CustomStaticMesh"));
    CustomStaticMeshComp->SetupAttachment(SceneRoot);
    CustomStaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CustomStaticMeshComp->SetCastShadow(true);
    CustomStaticMeshComp->SetVisibility(false);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
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
    UStaticMesh* Cylinder = CylinderMesh.Succeeded() ? CylinderMesh.Object.Get() : Sphere;
    UStaticMesh* Cone = ConeMesh.Succeeded() ? ConeMesh.Object.Get() : Sphere;

    Body = MakePart(TEXT("Body"), Sphere, FVector(0, 0, 105), FVector(0.48f, 0.38f, 0.66f));
    Head = MakePart(TEXT("Head"), Sphere, FVector(0, 0, 205), FVector(0.30f, 0.29f, 0.32f));
    LeftArm = MakePart(TEXT("LeftArm"), Cylinder, FVector(0, -42, 112), FVector(0.12f, 0.12f, 0.48f));
    RightArm = MakePart(TEXT("RightArm"), Cylinder, FVector(0, 42, 112), FVector(0.12f, 0.12f, 0.48f));
    LeftLeg = MakePart(TEXT("LeftLeg"), Cylinder, FVector(0, -18, 34), FVector(0.14f, 0.14f, 0.38f));
    RightLeg = MakePart(TEXT("RightLeg"), Cylinder, FVector(0, 18, 34), FVector(0.14f, 0.14f, 0.38f));
    LeftEye = MakePart(TEXT("LeftEye"), Sphere, FVector(24, -12, 211), FVector(0.055f, 0.055f, 0.055f));
    RightEye = MakePart(TEXT("RightEye"), Sphere, FVector(24, 12, 211), FVector(0.055f, 0.055f, 0.055f));
    Tail = MakePart(TEXT("Tail"), Cone, FVector(-72, 0, 68), FVector(0.16f, 0.16f, 0.72f));
    Tail->SetRelativeRotation(FRotator(0, 0, -90));
    LeftWing = MakePart(TEXT("LeftWing"), Cone, FVector(-8, -68, 155), FVector(0.58f, 0.12f, 0.34f));
    RightWing = MakePart(TEXT("RightWing"), Cone, FVector(-8, 68, 155), FVector(0.58f, 0.12f, 0.34f));
    LeftWing->SetRelativeRotation(FRotator(0, 0, -35));
    RightWing->SetRelativeRotation(FRotator(0, 0, 35));
    LeftHorn = MakePart(TEXT("LeftHorn"), Cone, FVector(26, -18, 245), FVector(0.09f, 0.09f, 0.28f));
    RightHorn = MakePart(TEXT("RightHorn"), Cone, FVector(26, 18, 245), FVector(0.09f, 0.09f, 0.28f));
    Tail->SetVisibility(false);
    LeftWing->SetVisibility(false);
    RightWing->SetVisibility(false);
    LeftHorn->SetVisibility(false);
    RightHorn->SetVisibility(false);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (BasicMaterial.Succeeded())
    {
        CreatureMaterial = UMaterialInstanceDynamic::Create(BasicMaterial.Object, this);
        UMaterialInstanceDynamic* EyeMaterial = UMaterialInstanceDynamic::Create(BasicMaterial.Object, this);
        CreatureMaterial->SetVectorParameterValue(TEXT("Color"), BaseColor);
        EyeMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.55f, 0.08f));
        Body->SetMaterial(0, CreatureMaterial);
        Head->SetMaterial(0, CreatureMaterial);
        LeftArm->SetMaterial(0, CreatureMaterial);
        RightArm->SetMaterial(0, CreatureMaterial);
        LeftLeg->SetMaterial(0, CreatureMaterial);
        RightLeg->SetMaterial(0, CreatureMaterial);
        Tail->SetMaterial(0, CreatureMaterial);
        LeftWing->SetMaterial(0, CreatureMaterial);
        RightWing->SetMaterial(0, CreatureMaterial);
        LeftHorn->SetMaterial(0, CreatureMaterial);
        RightHorn->SetMaterial(0, CreatureMaterial);
        LeftEye->SetMaterial(0, EyeMaterial);
        RightEye->SetMaterial(0, EyeMaterial);
    }
}

void AABFPCreatureActor::SetProceduralPartsVisible(bool bVisible)
{
    if (Body) Body->SetVisibility(bVisible);
    if (Head) Head->SetVisibility(bVisible);
    if (LeftArm) LeftArm->SetVisibility(bVisible);
    if (RightArm) RightArm->SetVisibility(bVisible);
    if (LeftLeg) LeftLeg->SetVisibility(bVisible);
    if (RightLeg) RightLeg->SetVisibility(bVisible);
    if (LeftEye) LeftEye->SetVisibility(bVisible);
    if (RightEye) RightEye->SetVisibility(bVisible);
    if (!bVisible)
    {
        if (Tail) Tail->SetVisibility(false);
        if (LeftWing) LeftWing->SetVisibility(false);
        if (RightWing) RightWing->SetVisibility(false);
        if (LeftHorn) LeftHorn->SetVisibility(false);
        if (RightHorn) RightHorn->SetVisibility(false);
    }
}

void AABFPCreatureActor::ApplyVisualEntry(const FABFPCreatureVisualEntry* Entry)
{
    ActiveHitMontage = nullptr;
    ActiveDeathMontage = nullptr;
    ActiveHitSound = nullptr;
    ActiveDeathSound = nullptr;

    if (!Entry)
    {
        bUsingCustomVisual = false;
        bUsingSkeletalVisual = false;
        if (SkeletalMeshComp) SkeletalMeshComp->SetVisibility(false);
        if (CustomStaticMeshComp) CustomStaticMeshComp->SetVisibility(false);
        SetProceduralPartsVisible(true);
        return;
    }

    ActiveHitMontage = Entry->HitMontage;
    ActiveDeathMontage = Entry->DeathMontage;
    ActiveHitSound = Entry->HitSound;
    ActiveDeathSound = Entry->DeathSound;

    if (Entry->SkeletalMesh && SkeletalMeshComp)
    {
        bUsingCustomVisual = true;
        bUsingSkeletalVisual = true;
        SetProceduralPartsVisible(false);
        if (CustomStaticMeshComp) CustomStaticMeshComp->SetVisibility(false);

        SkeletalMeshComp->SetSkeletalMeshAsset(Entry->SkeletalMesh);
        SkeletalMeshComp->SetRelativeLocation(Entry->LocationOffset);
        SkeletalMeshComp->SetRelativeRotation(Entry->RotationOffset);
        SkeletalMeshComp->SetRelativeScale3D(Entry->Scale);
        if (Entry->MaterialOverride)
        {
            SkeletalMeshComp->SetMaterial(0, Entry->MaterialOverride);
        }
        if (Entry->AnimClass)
        {
            SkeletalMeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
            SkeletalMeshComp->SetAnimInstanceClass(Entry->AnimClass.Get());
        }
        else if (Entry->IdleAnim)
        {
            SkeletalMeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            SkeletalMeshComp->PlayAnimation(Entry->IdleAnim, true);
        }
        SkeletalMeshComp->SetVisibility(true);
        return;
    }

    if (Entry->StaticMeshFallback && CustomStaticMeshComp)
    {
        bUsingCustomVisual = true;
        bUsingSkeletalVisual = false;
        SetProceduralPartsVisible(false);
        if (SkeletalMeshComp) SkeletalMeshComp->SetVisibility(false);

        CustomStaticMeshComp->SetStaticMesh(Entry->StaticMeshFallback);
        CustomStaticMeshComp->SetRelativeLocation(Entry->LocationOffset);
        CustomStaticMeshComp->SetRelativeRotation(Entry->RotationOffset);
        CustomStaticMeshComp->SetRelativeScale3D(Entry->Scale);
        if (Entry->MaterialOverride)
        {
            CustomStaticMeshComp->SetMaterial(0, Entry->MaterialOverride);
        }
        CustomStaticMeshComp->SetVisibility(true);
        return;
    }

    if (Entry->ActorClassOverride)
    {
        /* A custom Blueprint actor subclass owns its own mesh hierarchy. */
        bUsingCustomVisual = true;
        bUsingSkeletalVisual = false;
        SetProceduralPartsVisible(false);
        return;
    }

    bUsingCustomVisual = false;
    bUsingSkeletalVisual = false;
    if (SkeletalMeshComp) SkeletalMeshComp->SetVisibility(false);
    if (CustomStaticMeshComp) CustomStaticMeshComp->SetVisibility(false);
    SetProceduralPartsVisible(true);
}

void AABFPCreatureActor::UpdateEntityState(const FABFPEntity& Entity)
{
    CurrentHitPoints = Entity.HitPoints;
    MaxHitPoints = Entity.MaxHitPoints;
    SetCreatureAppearance(Entity.Name, Entity.KindId);
    OnCreatureConfigured(Entity);
}

void AABFPCreatureActor::SetCreatureAppearance(const FString& Name, int32 KindId)
{
    if (CreatureName == Name && CreatureKindId == KindId) return;
    CreatureName = Name;
    CreatureKindId = KindId;

    if (bUsingCustomVisual) return;

    const FString Lower = Name.ToLower();
    const bool bDragon = Lower.Contains(TEXT("dragon")) || Lower.Contains(TEXT("wyrm"));
    const bool bSerpent = Lower.Contains(TEXT("snake")) ||
        Lower.Contains(TEXT("worm")) || Lower.Contains(TEXT("naga"));
    const bool bUndead = Lower.Contains(TEXT("skeleton")) ||
        Lower.Contains(TEXT("zombie")) || Lower.Contains(TEXT("wraith")) ||
        Lower.Contains(TEXT("ghost"));
    const bool bBrute = Lower.Contains(TEXT("troll")) ||
        Lower.Contains(TEXT("ogre")) || Lower.Contains(TEXT("giant"));
    const bool bDemon = Lower.Contains(TEXT("demon")) ||
        Lower.Contains(TEXT("devil"));

    BaseColor = bDragon ? FLinearColor(0.42f, 0.075f, 0.045f) :
        bSerpent ? FLinearColor(0.12f, 0.38f, 0.16f) :
        bUndead ? FLinearColor(0.48f, 0.58f, 0.66f) :
        bDemon ? FLinearColor(0.38f, 0.055f, 0.045f) :
        bBrute ? FLinearColor(0.30f, 0.34f, 0.12f) :
        Lower.Contains(TEXT("kobold")) || Lower.Contains(TEXT("goblin")) ?
            FLinearColor(0.20f, 0.34f, 0.12f) :
            FLinearColor(0.34f, 0.25f, 0.16f);

    const float KindTint = static_cast<float>(FMath::Abs(KindId) % 5) * 0.018f;
    BaseColor.R = FMath::Clamp(BaseColor.R + KindTint, 0.0f, 1.0f);
    BaseColor.G = FMath::Clamp(BaseColor.G + KindTint * 0.5f, 0.0f, 1.0f);

    if (CreatureMaterial)
        CreatureMaterial->SetVectorParameterValue(TEXT("Color"), BaseColor);

    const FVector BodyScale = bSerpent ? FVector(0.82f, 0.25f, 0.24f) :
        bDragon ? FVector(0.78f, 0.68f, 0.66f) :
        bBrute ? FVector(0.72f, 0.68f, 0.88f) :
        FVector(0.48f, 0.38f, 0.66f);
    BaseBodyScale = BodyScale;
    BaseBodyLocation = bSerpent ? FVector(0, 0, 52) : FVector(0, 0, 105);
    BaseHeadScale = bDragon || bBrute ? FVector(0.40f, 0.38f, 0.40f) :
        FVector(0.30f, 0.29f, 0.32f);
    BaseHeadLocation = bSerpent ? FVector(65, 0, 66) : FVector(0, 0, 205);
    if (Body) {
        Body->SetRelativeScale3D(BaseBodyScale);
        Body->SetRelativeLocation(BaseBodyLocation);
    }
    if (Head) {
        Head->SetRelativeScale3D(BaseHeadScale);
        Head->SetRelativeLocation(BaseHeadLocation);
    }

    const bool bArmedShape = !bSerpent;
    if (LeftArm) LeftArm->SetVisibility(bArmedShape);
    if (RightArm) RightArm->SetVisibility(bArmedShape);
    if (LeftLeg) LeftLeg->SetVisibility(bArmedShape);
    if (RightLeg) RightLeg->SetVisibility(bArmedShape);
    if (Tail) Tail->SetVisibility(bSerpent || bDragon);
    if (LeftWing) LeftWing->SetVisibility(bDragon);
    if (RightWing) RightWing->SetVisibility(bDragon);
    if (LeftHorn) LeftHorn->SetVisibility(bDragon || bDemon);
    if (RightHorn) RightHorn->SetVisibility(bDragon || bDemon);
}

void AABFPCreatureActor::PlayHitReaction(int32 Damage)
{
    if (bDying) return;
    DamageFlashSeconds = 0.22f;

    if (bUsingSkeletalVisual && SkeletalMeshComp && ActiveHitMontage)
    {
        if (UAnimInstance* AnimInst = SkeletalMeshComp->GetAnimInstance())
        {
            AnimInst->Montage_Play(ActiveHitMontage);
        }
    }
    if (ActiveHitSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ActiveHitSound, GetActorLocation());
    }
    OnCreatureDamaged(Damage, CurrentHitPoints, MaxHitPoints);
}

void AABFPCreatureActor::PlayDeathReaction()
{
    if (bDying) return;
    bDying = true;
    DeathSeconds = 0.0f;
    DeathDuration = 0.72f;
    if (bInterpolating)
    {
        bInterpolating = false;
        OnCreatureMovementChanged(false);
    }
    SetActorEnableCollision(false);

    if (bUsingSkeletalVisual && SkeletalMeshComp && ActiveDeathMontage)
    {
        if (UAnimInstance* AnimInst = SkeletalMeshComp->GetAnimInstance())
        {
            const float PlayedDuration = AnimInst->Montage_Play(ActiveDeathMontage);
            if (PlayedDuration > 0.1f)
            {
                DeathDuration = PlayedDuration;
            }
        }
    }
    if (ActiveDeathSound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ActiveDeathSound, GetActorLocation());
    }
    OnCreatureDied();
}

void AABFPCreatureActor::SetTargetLocation(const FVector& Location,
    const FRotator& Rotation)
{
    TargetLocation = Location;
    TargetRotation = Rotation;
    const bool bWasMoving = bInterpolating;
    bInterpolating = FVector::DistSquared(GetActorLocation(), TargetLocation) > 4.0f;
    if (!bInterpolating) SetActorRotation(TargetRotation);
    if (bWasMoving != bInterpolating)
    {
        OnCreatureMovementChanged(bInterpolating);
    }
}

void AABFPCreatureActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bDying)
    {
        DeathSeconds += DeltaSeconds;
        if (!bUsingSkeletalVisual || !ActiveDeathMontage)
        {
            const float Alpha = FMath::Clamp(DeathSeconds / DeathDuration, 0.0f, 1.0f);
            const float RemainingScale = 1.0f - Alpha * 0.78f;
            SetActorScale3D(FVector(RemainingScale, RemainingScale, RemainingScale));
            AddActorLocalRotation(FRotator(0.0f, 0.0f, 190.0f * DeltaSeconds));
            if (!bUsingCustomVisual && CreatureMaterial)
            {
                CreatureMaterial->SetVectorParameterValue(TEXT("Color"),
                    FLinearColor::LerpUsingHSV(BaseColor, FLinearColor(0.015f, 0.012f, 0.01f), Alpha));
            }
        }
        if (DeathSeconds >= DeathDuration) Destroy();
        return;
    }

    Elapsed += DeltaSeconds;
    DamageFlashSeconds = FMath::Max(0.0f, DamageFlashSeconds - DeltaSeconds);

    if (bInterpolating)
    {
        const FVector CurrentLocation = FMath::VInterpTo(
            GetActorLocation(), TargetLocation, DeltaSeconds, 8.0f);
        const FRotator CurrentRotation = FMath::RInterpTo(
            GetActorRotation(), TargetRotation, DeltaSeconds, 8.0f);
        SetActorLocation(CurrentLocation);
        SetActorRotation(CurrentRotation);
        if (FVector::DistSquared(CurrentLocation, TargetLocation) < 4.0f)
        {
            SetActorLocation(TargetLocation);
            SetActorRotation(TargetRotation);
            bInterpolating = false;
            OnCreatureMovementChanged(false);
        }
    }

    if (bUsingCustomVisual)
    {
        return;
    }

    const float Breath = FMath::Sin(Elapsed * 2.1f) * 3.0f;
    const float ArmSwing = FMath::Sin(Elapsed * 1.3f) * 7.0f;

    if (Body) Body->SetRelativeLocation(BaseBodyLocation + FVector(0, 0, Breath));
    if (Head) Head->SetRelativeRotation(FRotator(0, FMath::Sin(Elapsed * 0.7f) * 5.0f, 0));
    if (LeftArm) LeftArm->SetRelativeRotation(FRotator(ArmSwing, 0, -8));
    if (RightArm) RightArm->SetRelativeRotation(FRotator(-ArmSwing, 0, 8));

    if (CreatureMaterial)
        CreatureMaterial->SetVectorParameterValue(TEXT("Color"),
            DamageFlashSeconds > 0.0f ? FLinearColor(1.0f, 0.06f, 0.025f) : BaseColor);

    if (DamageFlashSeconds > 0.0f)
    {
        const float Punch = 1.0f + FMath::Sin(Elapsed * 45.0f) * 0.09f;
        if (Body) Body->SetRelativeScale3D(BaseBodyScale * Punch);
        if (Head) Head->SetRelativeScale3D(BaseHeadScale * Punch);
    }
    else
    {
        if (Body) Body->SetRelativeScale3D(BaseBodyScale);
        if (Head) Head->SetRelativeScale3D(BaseHeadScale);
    }
}
