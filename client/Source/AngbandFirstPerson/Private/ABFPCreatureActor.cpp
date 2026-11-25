#include "ABFPCreatureActor.h"
#include "ABFPVisualCatalog.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/PlayerCameraManager.h"
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
    PrimaryActorTick.bStartWithTickEnabled = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    SkeletalMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
    SkeletalMeshComp->SetupAttachment(SceneRoot);
    SkeletalMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SkeletalMeshComp->SetGenerateOverlapEvents(false);
    SkeletalMeshComp->SetCanEverAffectNavigation(false);
    SkeletalMeshComp->SetCastShadow(false);
    SkeletalMeshComp->bCastInsetShadow = false;
    SkeletalMeshComp->bComponentUseFixedSkelBounds = true;
    SkeletalMeshComp->bEnableUpdateRateOptimizations = true;
    SkeletalMeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    SkeletalMeshComp->SetVisibility(false);

    CustomStaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CustomStaticMesh"));
    CustomStaticMeshComp->SetupAttachment(SceneRoot);
    CustomStaticMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CustomStaticMeshComp->SetGenerateOverlapEvents(false);
    CustomStaticMeshComp->SetCanEverAffectNavigation(false);
    CustomStaticMeshComp->SetCastShadow(false);
    CustomStaticMeshComp->SetVisibility(false);

    HealthBarRoot = CreateDefaultSubobject<USceneComponent>(TEXT("HealthBarRoot"));
    HealthBarRoot->SetupAttachment(SceneRoot);
    HealthBarRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 255.0f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(
        TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

    CachedSphereMesh = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
    CachedCylinderMesh = CylinderMesh.Succeeded() ? CylinderMesh.Object.Get() : CachedSphereMesh.Get();
    CachedConeMesh = ConeMesh.Succeeded() ? ConeMesh.Object.Get() : CachedSphereMesh.Get();
    CachedCubeMesh = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
    CachedBasicMat = BasicMaterial.Succeeded() ? BasicMaterial.Object : nullptr;
}

void AABFPCreatureActor::EnsureHealthBar()
{
    if (HealthBarBg || !GetWorld() || !CachedCubeMesh) return;

    auto MakeBar = [this](const FVector& Loc, const FVector& Scale) -> UStaticMeshComponent*
    {
        UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
        Comp->SetupAttachment(HealthBarRoot);
        Comp->SetStaticMesh(CachedCubeMesh);
        Comp->SetRelativeLocation(Loc);
        Comp->SetRelativeScale3D(Scale);
        Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Comp->SetGenerateOverlapEvents(false);
        Comp->SetCanEverAffectNavigation(false);
        Comp->SetCastShadow(false);
        Comp->RegisterComponent();
        return Comp;
    };

    HealthBarBg = MakeBar(FVector::ZeroVector, FVector(0.02f, 0.86f, 0.09f));
    HealthBarFill = MakeBar(FVector(0.4f, 0.0f, 0.0f), FVector(0.024f, 0.82f, 0.065f));
    if (CachedBasicMat)
    {
        UMaterialInstanceDynamic* HealthBgMat = UMaterialInstanceDynamic::Create(CachedBasicMat, this);
        HealthBarFillMat = UMaterialInstanceDynamic::Create(CachedBasicMat, this);
        if (HealthBgMat)
        {
            HealthBgMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.06f, 0.05f, 0.05f));
            HealthBarBg->SetMaterial(0, HealthBgMat);
        }
        if (HealthBarFillMat)
        {
            HealthBarFillMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.88f, 0.16f, 0.12f));
            HealthBarFill->SetMaterial(0, HealthBarFillMat);
        }
    }
}

void AABFPCreatureActor::EnsureProceduralParts()
{
    if (Body || !GetWorld()) return;

    auto MakePart = [this](UStaticMesh* Mesh, const FVector& Location,
        const FVector& Scale, const FRotator& Rot = FRotator::ZeroRotator, bool bStartVis = true) -> UStaticMeshComponent*
    {
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
        Part->SetupAttachment(SceneRoot);
        if (Mesh) Part->SetStaticMesh(Mesh);
        Part->SetRelativeLocation(Location);
        Part->SetRelativeRotation(Rot);
        Part->SetRelativeScale3D(Scale);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetGenerateOverlapEvents(false);
        Part->SetCanEverAffectNavigation(false);
        Part->SetCastShadow(false);
        Part->SetVisibility(bStartVis);
        Part->RegisterComponent();
        return Part;
    };

    Body = MakePart(CachedSphereMesh, FVector(0, 0, 105), FVector(0.48f, 0.38f, 0.66f));
    Head = MakePart(CachedSphereMesh, FVector(0, 0, 205), FVector(0.30f, 0.29f, 0.32f));
    LeftArm = MakePart(CachedCylinderMesh, FVector(0, -42, 112), FVector(0.12f, 0.12f, 0.48f));
    RightArm = MakePart(CachedCylinderMesh, FVector(0, 42, 112), FVector(0.12f, 0.12f, 0.48f));
    LeftLeg = MakePart(CachedCylinderMesh, FVector(0, -18, 34), FVector(0.14f, 0.14f, 0.38f));
    RightLeg = MakePart(CachedCylinderMesh, FVector(0, 18, 34), FVector(0.14f, 0.14f, 0.38f));
    LeftEye = MakePart(CachedSphereMesh, FVector(24, -12, 211), FVector(0.055f, 0.055f, 0.055f));
    RightEye = MakePart(CachedSphereMesh, FVector(24, 12, 211), FVector(0.055f, 0.055f, 0.055f));
    Tail = MakePart(CachedConeMesh, FVector(-72, 0, 68), FVector(0.16f, 0.16f, 0.72f), FRotator(0, 0, -90), false);
    LeftWing = MakePart(CachedConeMesh, FVector(-8, -68, 155), FVector(0.58f, 0.12f, 0.34f), FRotator(0, 0, -35), false);
    RightWing = MakePart(CachedConeMesh, FVector(-8, 68, 155), FVector(0.58f, 0.12f, 0.34f), FRotator(0, 0, 35), false);
    LeftHorn = MakePart(CachedConeMesh, FVector(26, -18, 245), FVector(0.09f, 0.09f, 0.28f), FRotator::ZeroRotator, false);
    RightHorn = MakePart(CachedConeMesh, FVector(26, 18, 245), FVector(0.09f, 0.09f, 0.28f), FRotator::ZeroRotator, false);

    if (CachedBasicMat)
    {
        CreatureMaterial = UMaterialInstanceDynamic::Create(CachedBasicMat, this);
        UMaterialInstanceDynamic* EyeMaterial = UMaterialInstanceDynamic::Create(CachedBasicMat, this);
        if (CreatureMaterial)
        {
            CreatureMaterial->SetVectorParameterValue(TEXT("Color"), BaseColor);
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
        }
        if (EyeMaterial)
        {
            EyeMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.55f, 0.08f));
            LeftEye->SetMaterial(0, EyeMaterial);
            RightEye->SetMaterial(0, EyeMaterial);
        }
    }
}

void AABFPCreatureActor::SetProceduralPartsVisible(bool bVisible)
{
    if (bVisible && !Body)
    {
        EnsureProceduralParts();
    }
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

void AABFPCreatureActor::SetCastShadowsEnabled(bool bEnableShadows)
{
    if (SkeletalMeshComp) SkeletalMeshComp->SetCastShadow(bEnableShadows);
    if (CustomStaticMeshComp) CustomStaticMeshComp->SetCastShadow(bEnableShadows);
}

void AABFPCreatureActor::RefreshTickState()
{
    const bool bHealthBarVisible = HealthBarBg && HealthBarBg->IsVisible();
    const bool bNeedTick = !bUsingCustomVisual || bInterpolating || bDying ||
        DamageFlashSeconds > 0.0f || bHealthBarVisible;
    SetActorTickEnabled(bNeedTick);
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
        RefreshTickState();
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
            const int32 NumMats = FMath::Max(1, SkeletalMeshComp->GetNumMaterials());
            for (int32 MatIdx = 0; MatIdx < NumMats; ++MatIdx)
            {
                SkeletalMeshComp->SetMaterial(MatIdx, Entry->MaterialOverride);
            }
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
        RefreshTickState();
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
            const int32 NumMats = FMath::Max(1, CustomStaticMeshComp->GetNumMaterials());
            for (int32 MatIdx = 0; MatIdx < NumMats; ++MatIdx)
            {
                CustomStaticMeshComp->SetMaterial(MatIdx, Entry->MaterialOverride);
            }
        }
        CustomStaticMeshComp->SetVisibility(true);
        RefreshTickState();
        return;
    }

    if (Entry->ActorClassOverride)
    {
        bUsingCustomVisual = true;
        bUsingSkeletalVisual = false;
        SetProceduralPartsVisible(false);
        RefreshTickState();
        return;
    }

    bUsingCustomVisual = false;
    bUsingSkeletalVisual = false;
    if (SkeletalMeshComp) SkeletalMeshComp->SetVisibility(false);
    if (CustomStaticMeshComp) CustomStaticMeshComp->SetVisibility(false);
    SetProceduralPartsVisible(true);
    RefreshTickState();
}

void AABFPCreatureActor::UpdateHealthBar()
{
    const bool bShowBar = !bDying && MaxHitPoints > 0 && CurrentHitPoints > 0 && CurrentHitPoints < MaxHitPoints;
    if (bShowBar && !HealthBarBg)
    {
        EnsureHealthBar();
    }
    if (HealthBarBg) HealthBarBg->SetVisibility(bShowBar);
    if (HealthBarFill) HealthBarFill->SetVisibility(bShowBar);
    RefreshTickState();
    if (!bShowBar || !HealthBarFill) return;

    const float Ratio = FMath::Clamp(
        static_cast<float>(CurrentHitPoints) / static_cast<float>(MaxHitPoints), 0.04f, 1.0f);
    HealthBarFill->SetRelativeScale3D(FVector(0.024f, 0.82f * Ratio, 0.065f));
    HealthBarFill->SetRelativeLocation(FVector(0.4f, -(1.0f - Ratio) * 41.0f, 0.0f));
    if (HealthBarFillMat)
    {
        const FLinearColor BarColor = Ratio > 0.55f ? FLinearColor(0.24f, 0.78f, 0.24f)
            : (Ratio > 0.28f ? FLinearColor(0.92f, 0.62f, 0.12f) : FLinearColor(0.92f, 0.14f, 0.10f));
        HealthBarFillMat->SetVectorParameterValue(TEXT("Color"), BarColor);
    }
}

void AABFPCreatureActor::UpdateEntityState(const FABFPEntity& Entity)
{
    CurrentHitPoints = Entity.HitPoints;
    MaxHitPoints = Entity.MaxHitPoints;
    SetCreatureAppearance(Entity.Name, Entity.KindId);
    UpdateHealthBar();
    OnCreatureConfigured(Entity);
}

void AABFPCreatureActor::SetCreatureAppearance(const FString& Name, int32 KindId)
{
    if (CreatureName == Name && CreatureKindId == KindId) return;
    CreatureName = Name;
    CreatureKindId = KindId;

    if (bUsingCustomVisual) return;
    EnsureProceduralParts();

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
    RefreshTickState();

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
    UpdateHealthBar();
    RefreshTickState();
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
    RefreshTickState();
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
            RefreshTickState();
        }
    }

    if (HealthBarRoot && HealthBarBg && HealthBarBg->IsVisible())
    {
        if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0))
        {
            const FVector ToCam = CamMgr->GetCameraLocation() - HealthBarRoot->GetComponentLocation();
            if (!ToCam.IsNearlyZero())
            {
                const FRotator BillboardRot(0.0f, ToCam.Rotation().Yaw, 0.0f);
                HealthBarRoot->SetWorldRotation(BillboardRot);
            }
        }
    }

    if (bUsingCustomVisual)
    {
        if (DamageFlashSeconds <= 0.0f && !bInterpolating && !(HealthBarBg && HealthBarBg->IsVisible()))
        {
            SetActorTickEnabled(false);
        }
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
