#include "BreachGame.h"
#include "BreachMovementComponent.h"
#include "BreachVisuals.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

ABreachCharacter::ABreachCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UBreachMovementComponent>(ACharacter::CharacterMovementComponentName))
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 92.f);
    GetCharacterMovement()->MaxWalkSpeed = UBreachMovementComponent::RifleSpeed;
    GetCharacterMovement()->JumpZVelocity = 540.f;
    GetCharacterMovement()->AirControl = .45f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2200.f;
    GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch=true;
    GetCharacterMovement()->SetCrouchedHalfHeight(54.f);
    GetCharacterMovement()->MaxWalkSpeedCrouched=200.f;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(8, 0, 67));
    Camera->bUsePawnControlRotation = true;
    Camera->FieldOfView = BaseFieldOfView;
    Camera->SetEnableFirstPersonFieldOfView(true);
    Camera->SetEnableFirstPersonScale(true);
    Camera->SetFirstPersonFieldOfView(BaseFieldOfView);
    Camera->SetFirstPersonScale(.3f);
    WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
    WeaponRoot->SetupAttachment(Camera);
    WeaponRoot->SetRelativeLocation(FVector(34, 10, -5));
    WeaponRoot->SetRelativeScale3D(FVector(.8f));
    WorldWeaponRoot=CreateDefaultSubobject<USceneComponent>(TEXT("WorldWeaponRoot"));
    WorldWeaponRoot->SetupAttachment(Camera);
    WorldWeaponRoot->SetRelativeTransform(WeaponRoot->GetRelativeTransform());
    const auto AddPart = [this](const TCHAR* Name, const FVector& Position, const FVector& Size, const TCHAR* Mat,FRotator Rotation=FRotator::ZeroRotator)
    {
        auto* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Part->SetupAttachment(WeaponRoot);
        Part->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Part->SetRelativeLocation(Position);
        Part->SetRelativeScale3D(Size / 100.f);
        Part->SetRelativeRotation(Rotation);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->CastShadow = false;
        Part->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
        Part->SetOnlyOwnerSee(true);
        Part->SetMaterial(0, Breach::Material(Mat));
        auto* WorldPart=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("World_%s"),Name));
        WorldPart->SetupAttachment(WorldWeaponRoot);
        WorldPart->SetStaticMesh(Part->GetStaticMesh());
        WorldPart->SetRelativeTransform(Part->GetRelativeTransform());
        WorldPart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WorldPart->SetOwnerNoSee(true);
        WorldPart->SetCastHiddenShadow(true);
        WorldPart->SetCastShadow(true);
        WorldPart->bCastCinematicShadow=true;
        WorldPart->SetMaterial(0,Breach::Material(Mat));
        return Part;
    };
    AddPart(TEXT("Receiver"), FVector(0,0,0), FVector(36,7,8), TEXT("M_Gun"));
    AddPart(TEXT("UpperRail"), FVector(4,0,5), FVector(38,5,2), TEXT("M_Metal"));
    AddPart(TEXT("Barrel"), FVector(27,0,1), FVector(22,3,3), TEXT("M_Metal"));
    AddPart(TEXT("MuzzleBrake"), FVector(39,0,1), FVector(6,5,5), TEXT("M_Gun"));
    AddPart(TEXT("EnergyStripe"), FVector(3,-3.6f,1), FVector(28,.5f,1.3f), TEXT("M_Cyan"));
    AddPart(TEXT("RightStripe"), FVector(3,3.6f,1), FVector(28,.5f,1.3f), TEXT("M_Cyan"));
    AddPart(TEXT("Magazine"), FVector(-1,0,-9), FVector(8,5,12), TEXT("M_Metal"),FRotator(12,0,0));
    AddPart(TEXT("Grip"), FVector(-12,0,-8), FVector(5,5,12), TEXT("M_Gun"),FRotator(-16,0,0));
    AddPart(TEXT("Stock"), FVector(-25,0,-1), FVector(17,6,7), TEXT("M_Gun"));
    AddPart(TEXT("SightLeft"), FVector(5,-2.8f,9), FVector(3,1,7), TEXT("M_Metal"));
    AddPart(TEXT("SightRight"), FVector(5,2.8f,9), FVector(3,1,7), TEXT("M_Metal"));
    AddPart(TEXT("SightTop"), FVector(5,0,12), FVector(3,6,1), TEXT("M_Metal"));
    AddPart(TEXT("SightDot"), FVector(5,0,8), FVector(1,.6f,.6f), TEXT("M_Orange"));
    MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleFlash"));
    MuzzleLight->SetupAttachment(WeaponRoot);
    MuzzleLight->SetRelativeLocation(FVector(44,0,1));
    MuzzleLight->SetLightColor(FLinearColor(.12f,.85f,1));
    MuzzleLight->SetIntensity(0);
    MuzzleLight->SetAttenuationRadius(220);
    MuzzleLight->SetCastShadows(false);
    Sword=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("AcheronSword"));
    Sword->SetupAttachment(GetCapsuleComponent());
    Sword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sword->SetOnlyOwnerSee(true);
    Sword->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    Sword->SetCastShadow(false);
    Sword->SetBoundsScale(2.f);
    Sword->SetVisibility(false);
    WorldSword=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("AcheronWorldSword"));
    WorldSword->SetupAttachment(GetCapsuleComponent());
    WorldSword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WorldSword->SetOwnerNoSee(true);
    WorldSword->SetCastHiddenShadow(true);
    WorldSword->SetCastShadow(true);
    WorldSword->bCastCinematicShadow=true;
    WorldSword->SetBoundsScale(2.f);
    WorldSword->SetVisibility(false);
    Scabbard=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("AcheronScabbard"));
    Scabbard->SetupAttachment(GetCapsuleComponent());
    Scabbard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Scabbard->SetOnlyOwnerSee(true);
    Scabbard->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    Scabbard->SetCastShadow(false);
    Scabbard->SetBoundsScale(2.f);
    Scabbard->SetVisibility(false);
    WorldScabbard=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("AcheronWorldScabbard"));
    WorldScabbard->SetupAttachment(GetCapsuleComponent());
    WorldScabbard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WorldScabbard->SetOwnerNoSee(true);
    WorldScabbard->SetCastHiddenShadow(true);
    WorldScabbard->SetCastShadow(true);
    WorldScabbard->bCastCinematicShadow=true;
    WorldScabbard->SetBoundsScale(2.f);
    WorldScabbard->SetVisibility(false);
    Body = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("OperatorBody"));
    Body->SetupAttachment(GetCapsuleComponent());
    Body->SetRelativeLocation(FVector(-10,0,-92));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetOnlyOwnerSee(true);
    Body->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    Body->CastShadow=false;
    Body->SetBoundsScale(2.f);
    WorldBody=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("OperatorWorldBody"));
    WorldBody->SetupAttachment(GetCapsuleComponent());
    WorldBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WorldBody->SetOwnerNoSee(true);
    WorldBody->SetCastHiddenShadow(true);
    WorldBody->SetCastShadow(true);
    WorldBody->bCastCinematicShadow=true;
    WorldBody->SetBoundsScale(2.f);
}

void ABreachCharacter::BeginPlay()
{
    Super::BeginPlay();
    SelectOperator(0);
    FireSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Fire.Fire"));
    ReloadSound = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/Reload.Reload"));
    if (auto* PC = Cast<APlayerController>(Controller))
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
}

void ABreachCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis("MoveForward", this, &ABreachCharacter::MoveForward);
    Input->BindAxis("MoveRight", this, &ABreachCharacter::MoveRight);
    Input->BindAxis("Turn", this, &ABreachCharacter::Turn);
    Input->BindAxis("LookUp", this, &ABreachCharacter::LookUp);
    Input->BindAction("Fire", IE_Pressed, this, &ABreachCharacter::StartFire);
    Input->BindAction("Fire", IE_Released, this, &ABreachCharacter::StopFire);
    Input->BindAction("Aim", IE_Pressed, this, &ABreachCharacter::AimOn);
    Input->BindAction("Aim", IE_Released, this, &ABreachCharacter::AimOff);
    Input->BindAction("Reload", IE_Pressed, this, &ABreachCharacter::Reload);
    Input->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
    Input->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);
    Input->BindAction("Unarmed", IE_Pressed, this, &ABreachCharacter::HolsterRifle);
    Input->BindAction("DrawRifle", IE_Pressed, this, &ABreachCharacter::DrawRifle);
    Input->BindAction("Crouch", IE_Pressed, this, &ABreachCharacter::CrouchOn);
    Input->BindAction("Crouch", IE_Released, this, &ABreachCharacter::CrouchOff);
    Input->BindAction("Pause", IE_Pressed, this, &ABreachCharacter::TogglePause).bExecuteWhenPaused = true;
    Input->BindAction("Selection", IE_Pressed, this, &ABreachCharacter::ToggleSelection).bExecuteWhenPaused = true;
    Input->BindAction("Restart", IE_Pressed, this, &ABreachCharacter::RestartRun).bExecuteWhenPaused = true;
}
void ABreachCharacter::MoveForward(float V) { if(Health>0) AddMovementInput(FRotationMatrix(FRotator(0,GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::X), V); }
void ABreachCharacter::MoveRight(float V) { if(Health>0) AddMovementInput(FRotationMatrix(FRotator(0,GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y), V); }
void ABreachCharacter::Turn(float V) { if(Health>0) AddControllerYawInput(V * (bAiming ? .45f : .75f)); }
void ABreachCharacter::LookUp(float V) { if(Health>0) AddControllerPitchInput(V * (bAiming ? .45f : .75f)); }
void ABreachCharacter::StartFire() { if(UsesSword() || !bUnarmed) { bTrigger=true; Fire(); } }
void ABreachCharacter::StopFire() { bTrigger=false; }
void ABreachCharacter::SetAim(bool Value)
{
    bAiming=Value && !bUnarmed;
    CastChecked<UBreachMovementComponent>(GetCharacterMovement())->SetLocomotionIntent(bUnarmed,bAiming);
}
bool ABreachCharacter::CanJumpInternal_Implementation() const
{
    return Health>0 && (IsSliding()?JumpIsAllowedInternal():Super::CanJumpInternal_Implementation());
}

void ABreachCharacter::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bTrigger) Fire();
    UpdateSwordAttack(Dt);
    bSprint=bUnarmed && !bIsCrouched;
    Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, bAiming ? AimFieldOfView : BaseFieldOfView, Dt, 12));
    Camera->SetFirstPersonFieldOfView(Camera->FieldOfView);
    Bob += Dt * (bSprint ? 13.f : 9.f);
    const float Movement = FMath::Clamp(GetVelocity().Size2D()/510.f,0.f,1.f);
    const FVector Hip(34,10,-5);
    const FVector Aim(30,0,-6.4f);
    FVector Target = bAiming ? Aim : Hip;
    Target.Z += FMath::Sin(Bob)*Movement*(bAiming?.05f:.2f);
    Target.X -= Recoil*2.7f;
    if (bReloading)
    {
        ReloadProgress = FMath::Clamp((GetWorld()->GetTimeSeconds()-ReloadStarted)/1.55f,0.f,1.f);
        Target.Z -= FMath::Sin(ReloadProgress*PI)*14.f;
    }
    WeaponRoot->SetRelativeLocation(FMath::VInterpTo(WeaponRoot->GetRelativeLocation(),Target,Dt,18));
    WeaponRoot->SetRelativeRotation(FRotator(Recoil*2.f,0,bReloading?FMath::Sin(ReloadProgress*PI)*-25.f:0));
    WorldWeaponRoot->SetRelativeTransform(WeaponRoot->GetRelativeTransform());
    UpdateOperatorPose(Dt);
    Recoil = FMath::FInterpTo(Recoil,0,Dt,15);
    MuzzleLight->SetIntensity(!UsesSword() && Recoil>.55f ? 5000.f : 0.f);
    HitMarker=FMath::Max(0.f,HitMarker-Dt);
    DamageFlash=FMath::Max(0.f,DamageFlash-Dt);
}

void ABreachCharacter::Fire()
{
    if(Health<=0 || bReloading || UGameplayStatics::IsGamePaused(this)) return;
    const float Now=GetWorld()->GetTimeSeconds();
    if(Now<NextShot) return;
    if(UsesSword())
    {
        NextShot=Now+SwordAttackInterval;
        SwordAttackTime=0.f; bSwordDamageApplied=false;
        SwordAttackOrigin=Camera->GetComponentLocation();
        SwordAttackDirection=Camera->GetForwardVector();
        ++ShotsFired;
        return;
    }
    if(bUnarmed) return;
    if(Ammo<=0) { Reload(); return; }
    NextShot=Now+FireInterval;
    --Ammo; ++ShotsFired; Recoil=1;
    const FVector Start=Camera->GetComponentLocation();
    FVector Direction=Camera->GetForwardVector();
    const float Spread=bAiming?.001f:.004f;
    Direction=FMath::VRandCone(Direction,Spread);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreachShot),true,this);
    const FVector End=Start+Direction*15000;
    const bool bHit=GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Params);
    const FVector Impact=bHit?Hit.ImpactPoint:End;
    Breach::Beam(GetWorld(),WeaponRoot->GetComponentTransform().TransformPosition(FVector(42,0,1)),Impact,FLinearColor(.1f,.85f,1),1.5f,.055f);
    if(FireSound) UGameplayStatics::PlaySound2D(this,FireSound,.4f);
    if(auto* Enemy=Cast<ABreachEnemy>(Hit.GetActor()); Enemy && !Enemy->bDisplayOnly && !Enemy->bDefeated)
    {
        ++ShotsHit;
        const bool Head=Hit.ImpactPoint.Z > Enemy->GetActorLocation().Z+47.f;
        bLastHeadshot=Head; HitMarker=.18f;
        UGameplayStatics::ApplyPointDamage(Enemy,ShotDamage*(Head?2.f:1.f),Direction,Hit,Controller,this,UDamageType::StaticClass());
    }
    else if(bHit)
    {
        for(int32 i=0;i<4;++i) Breach::Beam(GetWorld(),Impact,Impact+Hit.ImpactNormal*12+FMath::VRand()*14,FLinearColor(1,.35f,.08f),1,.1f);
    }
    AddControllerPitchInput(-.10f);
}

void ABreachCharacter::ConfigureSwordLoadout()
{
    bFirstPersonSwordGripReady=false;
    if(!UsesSword())
    {
        SwordAttackTime=-1.f; bSwordDamageApplied=false;
        Sword->SetVisibility(false,true); WorldSword->SetVisibility(false,true);
        Scabbard->SetVisibility(false,true); WorldScabbard->SetVisibility(false,true);
        return;
    }
    if(!bSwordRigReady)
    {
        auto* Asset=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/AcheronSword/SK_AcheronSword.SK_AcheronSword"));
        bSwordRigReady=Asset && SwordPose.InitSkeleton(Asset);
        if(bSwordRigReady)
        {
            for(auto* Prop:{Sword.Get(),WorldSword.Get(),Scabbard.Get(),WorldScabbard.Get()})
            {
                Prop->SetSkinnedAssetAndUpdate(Asset);
                SwordPose.Apply(Prop);
                Prop->RefreshBoneTransforms();
            }
            Sword->SetMaterial(0,Breach::Material(TEXT("M_ShadowOverlay")));
            WorldSword->SetMaterial(0,Breach::Material(TEXT("M_ShadowOverlay")));
            Scabbard->SetMaterial(1,Breach::Material(TEXT("M_ShadowOverlay")));
            WorldScabbard->SetMaterial(1,Breach::Material(TEXT("M_ShadowOverlay")));
            bSwordRigReady=Sword->GetBoneIndex(TEXT("bone_002"))!=INDEX_NONE && Scabbard->GetBoneIndex(TEXT("bone_003"))!=INDEX_NONE;
        }
    }
    if(!SwordAttackAnimation)
        SwordAttackAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Animations/Entrance/Acheron/A_Acheron_Sword_Attack.A_Acheron_Sword_Attack"));
}

void ABreachCharacter::UpdateSwordAttack(float Dt)
{
    if(!UsesSword() || SwordAttackTime<0.f) return;
    SwordAttackTime+=FMath::Max(0.f,Dt);
    if(!bSwordDamageApplied && SwordAttackTime>=SwordAttackInterval*.3f)
    {
        bSwordDamageApplied=true;
        PerformSwordHit();
    }
    if(SwordAttackTime>=SwordAttackInterval) SwordAttackTime=-1.f;
}

void ABreachCharacter::PerformSwordHit()
{
    const FVector Forward=SwordAttackDirection;
    const FVector Start=SwordAttackOrigin+Forward*30.f;
    const FVector End=Start+Forward*SwordRange;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreachSword),true,this);
    FHitResult Hit;
    const bool bHit=GetWorld()->SweepSingleByChannel(Hit,Start,End,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(SwordRadius),Params);
    if(auto* Enemy=bHit?Cast<ABreachEnemy>(Hit.GetActor()):nullptr; Enemy && !Enemy->bDisplayOnly && !Enemy->bDefeated)
    {
        ++ShotsHit; bLastHeadshot=false; HitMarker=.22f;
        UGameplayStatics::ApplyDamage(Enemy,SwordDamage,Controller,this,UDamageType::StaticClass());
    }
}

void ABreachCharacter::ApplySwordAttackPose()
{
    if(!UsesSword() || SwordAttackTime<0.f || !SwordAttackAnimation) return;
    const float Progress=FMath::Clamp(SwordAttackTime/FMath::Max(.01f,SwordAttackInterval),0.f,1.f);
    const float SourceEnd=FMath::Min(.9f,SwordAttackAnimation->GetPlayLength());
    const float SourceTime=Progress<.28f?FMath::Lerp(0.f,.32f,Progress/.28f):
        (Progress<.55f?FMath::Lerp(.32f,.6f,(Progress-.28f)/.27f):FMath::Lerp(.6f,SourceEnd,(Progress-.55f)/.45f));
    FBreachPose AttackPose=BodyPose;
    if(!AttackPose.Sample(SwordAttackAnimation,SourceTime,false)) return;
    const float BlendIn=FMath::SmoothStep(0.f,.1f,Progress);
    const float BlendOut=1.f-FMath::SmoothStep(.82f,1.f,Progress);
    const float Blend=FMath::Min(BlendIn,BlendOut);
    const int32 Spine=BodyPose.Bone(EBreachBone::Spine);
    for(int32 I=0;I<BodyPose.Local.Num();++I)
        if(BodyPose.IsUnder(I,Spine) && AttackPose.Local.IsValidIndex(I))
        {
            FTransform Mixed;
            Mixed.Blend(BodyPose.Local[I],AttackPose.Local[I],Blend);
            BodyPose.Local[I]=Mixed;
        }
    BodyPose.Rebuild();
}

void ABreachCharacter::UpdateSwordVisual(const FBreachPose& Pose,UPoseableMeshComponent* CharacterMesh,UPoseableMeshComponent* SwordMesh,float Dt)
{
    if(!CharacterMesh || !SwordMesh || !bSwordRigReady) return;
    const int32 Hand=Pose.Bone(EBreachBone::RHand);
    const int32 Middle=Pose.Fingers[1][6],Index=Pose.Fingers[1][3],Pinky=Pose.Fingers[1][12];
    const int32 Handle=SwordMesh->GetBoneIndex(TEXT("bone_002"));
    if(!Pose.CS.IsValidIndex(Hand) || !Pose.CS.IsValidIndex(Middle) || !Pose.CS.IsValidIndex(Index) || !Pose.CS.IsValidIndex(Pinky) || !SwordPose.ReferenceCS.IsValidIndex(Handle)) return;
    const FVector HandPosition=Pose.CS[Hand].GetLocation();
    FVector Along=(Pose.CS[Middle].GetLocation()-HandPosition).GetSafeNormal();
    FVector Across=(Pose.CS[Index].GetLocation()-Pose.CS[Pinky].GetLocation()).GetSafeNormal();
    Along=(Along-Across*FVector::DotProduct(Along,Across)).GetSafeNormal();
    const FVector Normal=FVector::CrossProduct(Along,Across).GetSafeNormal();
    const FTransform ToWorld=CharacterMesh->GetComponentTransform();
    const FVector Axis=ToWorld.TransformVectorNoScale(Across).GetSafeNormal();
    FQuat Rotation=FRotationMatrix::MakeFromYZ(Axis,ToWorld.TransformVectorNoScale(Along)).ToQuat();
    FVector Grip=ToWorld.TransformPosition(HandPosition+Along*3.f+Normal*1.5f);
    if(SwordMesh==Sword && Camera)
    {
        const FTransform View=Camera->GetComponentTransform();
        const FTransform RelativeGrip=FTransform(Rotation,Grip).GetRelativeTransform(View);
        if(!bFirstPersonSwordGripReady)
        {
            FirstPersonSwordAnchor=RelativeGrip;
            bFirstPersonSwordGripReady=true;
        }
        else
        {
            const float AnchorAlpha=1.f-FMath::Exp(-FMath::Max(.1f,FirstPersonSwordAnchorSpeed)*FMath::Max(0.f,Dt));
            FirstPersonSwordAnchor.SetLocation(FMath::Lerp(FirstPersonSwordAnchor.GetLocation(),RelativeGrip.GetLocation(),AnchorAlpha));
            FirstPersonSwordAnchor.SetRotation(FQuat::Slerp(FirstPersonSwordAnchor.GetRotation(),RelativeGrip.GetRotation(),AnchorAlpha).GetNormalized());
        }
        FTransform ReducedGrip;
        ReducedGrip.Blend(FirstPersonSwordAnchor,RelativeGrip,FMath::Clamp(FirstPersonSwordMotionScale,0.f,1.f));
        FirstPersonSwordGrip=ReducedGrip*View;
        Rotation=FirstPersonSwordGrip.GetRotation();
        Grip=FirstPersonSwordGrip.GetLocation();
    }
    const FVector Anchor=SwordPose.ReferenceCS[Handle].GetLocation();
    const float VisualScale=FMath::Max(.1f,SwordVisualScale);
    SwordMesh->SetWorldTransform(FTransform(Rotation,Grip-Rotation.RotateVector(Anchor*VisualScale),FVector(VisualScale)));
}

void ABreachCharacter::UpdateScabbardVisual(const FBreachPose& Pose,UPoseableMeshComponent* CharacterMesh,UPoseableMeshComponent* ScabbardMesh)
{
    if(!CharacterMesh || !ScabbardMesh || !bSwordRigReady) return;
    const int32 Hand=Pose.Bone(EBreachBone::RHand);
    const int32 Middle=Pose.Fingers[1][6],Index=Pose.Fingers[1][3],Pinky=Pose.Fingers[1][12];
    const int32 Handle=ScabbardMesh->GetBoneIndex(TEXT("bone_003"));
    if(!Pose.CS.IsValidIndex(Hand) || !Pose.CS.IsValidIndex(Middle) || !Pose.CS.IsValidIndex(Index) || !Pose.CS.IsValidIndex(Pinky) || !SwordPose.ReferenceCS.IsValidIndex(Handle)) return;
    const FVector HandPosition=Pose.CS[Hand].GetLocation();
    FVector Along=(Pose.CS[Middle].GetLocation()-HandPosition).GetSafeNormal();
    FVector Across=(Pose.CS[Index].GetLocation()-Pose.CS[Pinky].GetLocation()).GetSafeNormal();
    Along=(Along-Across*FVector::DotProduct(Along,Across)).GetSafeNormal();
    const FVector Normal=FVector::CrossProduct(Along,Across).GetSafeNormal();
    const FTransform ToWorld=CharacterMesh->GetComponentTransform();
    const FVector Axis=ToWorld.TransformVectorNoScale(Across).GetSafeNormal();
    FQuat Rotation=FRotationMatrix::MakeFromYZ(Axis,ToWorld.TransformVectorNoScale(Along)).ToQuat();
    FVector Grip=ToWorld.TransformPosition(HandPosition+Along*3.f+Normal*1.5f);
    if(ScabbardMesh==Scabbard && bFirstPersonSwordGripReady)
    {
        Rotation=FirstPersonSwordGrip.GetRotation();
        Grip=FirstPersonSwordGrip.GetLocation();
    }
    const FVector Anchor=SwordPose.ReferenceCS[Handle].GetLocation();
    const float VisualScale=FMath::Max(.1f,SwordVisualScale);
    ScabbardMesh->SetWorldTransform(FTransform(Rotation,Grip-Rotation.RotateVector(Anchor*VisualScale),FVector(VisualScale)));
}

void ABreachCharacter::Reload()
{
    if(Health<=0 || bUnarmed || bReloading || Ammo>=MagazineSize || Reserve<=0) return;
    bReloading=true; ReloadStarted=GetWorld()->GetTimeSeconds(); ReloadProgress=0;
    if(ReloadSound) UGameplayStatics::PlaySound2D(this,ReloadSound,.5f);
    GetWorldTimerManager().SetTimer(ReloadTimer,this,&ABreachCharacter::FinishReload,1.55f,false);
}
void ABreachCharacter::FinishReload()
{
    if(Health<=0 || bUnarmed) { bReloading=false; return; }
    const int32 Count=FMath::Min(MagazineSize-Ammo,Reserve);
    Ammo+=Count; Reserve-=Count; bReloading=false;
}
float ABreachCharacter::TakeDamage(float Damage,const FDamageEvent& Event,AController* DamageInstigator,AActor* Causer)
{
    if(Health<=0) return 0;
    Health=FMath::Max(0.f,Health-Damage); DamageFlash=.45f;
    if(Health<=0)
    {
        StopFire(); GetWorldTimerManager().ClearTimer(ReloadTimer); bReloading=false;
        if(auto* GM=GetWorld()->GetAuthGameMode<ABreachGameMode>()) GM->EndRun();
    }
    return Damage;
}
void ABreachCharacter::SelectOperator(int32 Index)
{
    const int32 NextOperator=FMath::Clamp(Index,0,3);
    const bool WasSword=UsesSword();
    if(!WasSword && NextOperator==1) bLoadoutBeforeSword=bUnarmed;
    if(WasSword && NextOperator!=1) bUnarmed=bLoadoutBeforeSword;
    OperatorIndex=NextOperator;
    if(auto* CharacterAsset=Breach::CharacterMesh(OperatorIndex))
    {
        Body->SetSkinnedAssetAndUpdate(CharacterAsset);
        // Component material overrides survive SetSkinnedAssetAndUpdate.
        // Clear the Eula cape overrides before switching to another operator.
        for(int32 Slot : {10,11,12,13,14})
        {
            Body->SetMaterial(Slot,nullptr);
            WorldBody->SetMaterial(Slot,nullptr);
        }
        // The source PMX assigns Eula's large cape panel to the clothing atlas.
        // That atlas contains a warm yellow region, while the actual cape art is
        // the dedicated blue snowflake texture.  Keep the original slot and
        // remap only this cape material at runtime so both body representations
        // show the supplied blue cape.
        const FBoxSphereBounds Bounds=CharacterAsset->GetBounds();
        const float Scale=178.f/FMath::Max(1.f,float(Bounds.BoxExtent.Z*2));
        Body->SetRelativeScale3D(FVector(Scale));
        Body->SetRelativeRotation(FRotator(0,-90.f,0));
        Body->SetRelativeLocation(FVector(0,0,-92-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale));
        WorldBody->SetSkinnedAssetAndUpdate(CharacterAsset);
        WorldBody->SetRelativeTransform(Body->GetRelativeTransform());
        if(OperatorIndex==0)
        {
            if(auto* Cape=Breach::Material(TEXT("M_Eula_CapeCorrect")))
                for(int32 Slot : {10,11,12,13,14})
                {
                    Body->SetMaterial(Slot,Cape);
                    WorldBody->SetMaterial(Slot,Cape);
                }
        }
        bBodyRigReady=BodyPose.Init(CharacterAsset,OperatorIndex);
        BodyCloth.Init(CharacterAsset,OperatorIndex);
        LoadLocomotionAnimations();
    }
    ConfigureSwordLoadout();
    SetUnarmed(bUnarmed);
    UpdateOperatorPose(0);
    if(auto* GM=GetWorld()->GetAuthGameMode<ABreachGameMode>())
    {
        GM->Notice=FString::Printf(TEXT("OPERATOR LINK / %s"),Breach::Names[OperatorIndex]); GM->NoticeTime=2.5f;
    }
}

static FVector OperatorGrip(const ABreachCharacter* Player,int32 Side)
{
    // Ascalon's shorter forearm supports the rear of the handguard.
    FVector Grip=Side?FVector(-18,7,-10):FVector(Player->OperatorIndex==3?-6.f:0.f,-9,-7);
    if(!Side && Player->bReloading)
        Grip=FMath::Lerp(Grip,FVector(-1,-10,-24),FMath::Sin(Player->ReloadProgress*PI));
    return Player->WeaponRoot->GetComponentTransform().TransformPosition(Grip);
}
void ABreachCharacter::UpdateOperatorPose(float Dt)
{
    if(!bBodyRigReady) return;
    UpdateLocomotion(Dt);
    ApplySwordAttackPose();
    const auto Bounds=Body->GetSkinnedAsset()->GetBounds();
    const float Ground=Bounds.Origin.Z-Bounds.BoxExtent.Z;
    const float HalfHeight=GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    FQuat TargetTilt=FQuat::Identity;
    float TargetFloorOffset=0.f;
    const auto* Move=GetCharacterMovement();
    if(IsSliding() && Move->CurrentFloor.IsWalkableFloor())
    {
        const FVector Normal=Move->CurrentFloor.HitResult.ImpactNormal;
        TargetTilt=FQuat::FindBetweenNormals(FVector::UpVector,Body->GetComponentQuat().UnrotateVector(Normal));
        const FVector Base=GetActorLocation()-FVector(0,0,HalfHeight);
        TargetFloorOffset=FVector::DotProduct(Move->CurrentFloor.HitResult.ImpactPoint-Base,Normal)/FMath::Max(.2,Normal.Z);
    }
    const float GroundBlend=1.f-FMath::Exp(-18.f*FMath::Max(0.f,Dt));
    SlideFloorTilt=FQuat::Slerp(SlideFloorTilt,TargetTilt,GroundBlend).GetNormalized();
    SlideFloorOffset=FMath::Lerp(SlideFloorOffset,TargetFloorOffset,GroundBlend);
    // Tilt the complete pose about its contact plane, including independent
    // cloth roots. The owner's eye compensation below keeps this out of the camera.
    const FVector Pivot(0,0,Ground);
    TArray<TPair<int32,FTransform>,TInlineAllocator<4>> UntiltedRoots;
    for(int32 I=0;I<BodyPose.Parents.Num();++I) if(BodyPose.Parents[I]<0)
    {
        UntiltedRoots.Emplace(I,BodyPose.Local[I]);
        BodyPose.Local[I].SetLocation(Pivot+SlideFloorTilt.RotateVector(BodyPose.Local[I].GetLocation()-Pivot));
        BodyPose.Local[I].SetRotation(SlideFloorTilt*BodyPose.Local[I].GetRotation());
    }
    BodyPose.Rebuild();
    const float BaseZ=-HalfHeight-Ground*Body->GetRelativeScale3D().Z+SlideFloorOffset;
    Body->SetRelativeLocation(FVector(0,0,BaseZ));
    WorldBody->SetRelativeLocation(Body->GetRelativeLocation());
    // Locomotion owns the unarmed torso; the head always looks where the
    // player aims. Rifle aiming also stabilizes the spine and chest.
    for(EBreachBone Joint:{EBreachBone::Spine,EBreachBone::Chest,EBreachBone::Neck,EBreachBone::Head})
        if(!bUnarmed || Joint==EBreachBone::Neck || Joint==EBreachBone::Head)
        {
            const int32 I=BodyPose.Bone(Joint);
            BodyPose.Rotate(I,BodyPose.ReferenceCS[I].GetRotation()*BodyPose.CS[I].GetRotation().Inverse());
        }
    const float Pitch=FMath::Clamp(FRotator::NormalizeAxis(GetControlRotation().Pitch),-80.f,80.f);
    BodyPose.Rotate(EBreachBone::Spine,FVector::ForwardVector,Pitch*.12f);
    BodyPose.Rotate(EBreachBone::Neck,FVector::ForwardVector,Pitch*.88f);
    // The camera follows player movement, not the neck's animation bob.
    // Retain the animated eye as an anchor for the complete owner mesh so
    // stabilizing the view cannot expose the hidden head's collar.
    FTransform StandingBody=Body->GetRelativeTransform();
    StandingBody.SetTranslation(FVector(0,0,-92-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Body->GetRelativeScale3D().Z));
    const int32 Neck=BodyPose.Bone(EBreachBone::Neck);
    const FVector ReferenceNeck=StandingBody.TransformPosition(BodyPose.ReferenceCS[Neck].GetLocation());
    const FVector EyeOffset=FRotator(Pitch,0,0).RotateVector(FVector(8,0,67)-ReferenceNeck);
    const FVector AnimatedNeck=Body->GetRelativeTransform().TransformPosition(BodyPose.CS[Neck].GetLocation());
    const FVector AnimatedEye=AnimatedNeck+EyeOffset;
    CrouchEyeDrop=FMath::Lerp(CrouchEyeDrop,bIsCrouched?50.f:0.f,1.f-FMath::Exp(-12.f*FMath::Max(Dt,0.f)));
    // Cancel the capsule's instant resize and smooth only the crouch height.
    // Jumping remains driven by the actor's physical world-space movement.
    const FVector Eye=ReferenceNeck+EyeOffset+FVector(0,0,92.f-HalfHeight-CrouchEyeDrop);
    const FTransform ActorTransform=GetActorTransform();
    const float EyeLimit=HalfHeight-6.f;
    const FVector Start=ActorTransform.TransformPosition(FVector(0,0,FMath::Clamp(Eye.Z,-EyeLimit,EyeLimit)));
    const FVector End=ActorTransform.TransformPosition(Eye);
    FHitResult CameraHit;
    FCollisionQueryParams CameraQuery(SCENE_QUERY_STAT(BreachEye),false,this);
    const bool Obstructed=GetWorld()->SweepSingleByChannel(CameraHit,Start,End,FQuat::Identity,ECC_Camera,FCollisionShape::MakeSphere(6.f),CameraQuery);
    const FVector SafeEye=Obstructed?ActorTransform.InverseTransformPosition(CameraHit.Location):Eye;
    Camera->SetRelativeLocation(SafeEye);
    // Absorb animation bob and camera collision in the owner mesh placement.
    // The world body keeps its original animation, foot placement and shadow.
    const FVector OwnerAdjustment=SafeEye-AnimatedEye;
    Body->SetRelativeLocation(Body->GetRelativeLocation()+OwnerAdjustment);
    const FTransform ToBody=Body->GetComponentTransform().Inverse();
    const FTransform View=Camera->GetComponentTransform();
    const auto PoseArms=[&](FBreachPose& Pose,const FTransform& ToMesh)
    {
        for(int32 Side=0;Side<2 && !bUnarmed;++Side)
        {
            const FVector Hint=View.TransformPosition(FVector(2,Side?39.f:-39.f,-40));
            Pose.SolveArm(Side,ToMesh.TransformPosition(OperatorGrip(this,Side)),ToMesh.TransformPosition(Hint));
            const FVector Direction=View.TransformVectorNoScale(Side?FVector(1,-.2f,0):FVector(.1f,1,0));
            const FVector Palm=View.TransformVectorNoScale(Side?FVector(0,-1,0):FVector(0,0,1));
            Pose.PoseHand(Side,ToMesh.TransformVectorNoScale(Direction),ToMesh.TransformVectorNoScale(Palm),.85f);
        }
        if(UsesSword())
        {
            const int32 Hand=Pose.Bone(EBreachBone::RHand);
            const int32 Middle=Pose.Fingers[1][6],Index=Pose.Fingers[1][3],Pinky=Pose.Fingers[1][12];
            if(Pose.CS.IsValidIndex(Hand) && Pose.CS.IsValidIndex(Middle) && Pose.CS.IsValidIndex(Index) && Pose.CS.IsValidIndex(Pinky))
            {
                const FVector Along=(Pose.CS[Middle].GetLocation()-Pose.CS[Hand].GetLocation()).GetSafeNormal();
                const FVector Across=(Pose.CS[Index].GetLocation()-Pose.CS[Pinky].GetLocation()).GetSafeNormal();
                Pose.PoseHand(1,Along,FVector::CrossProduct(Along,Across).GetSafeNormal(),.95f);
            }
        }
    };
    FBreachPose WorldPose=BodyPose;
    PoseArms(WorldPose,WorldBody->GetComponentTransform().Inverse());
    BodyCloth.Update(WorldPose,WorldBody->GetComponentTransform(),Dt,GetWorld(),this);
    WorldPose.Apply(WorldBody);
    PoseArms(BodyPose,ToBody);
    // Both representations use the complete source mesh and locomotion pose.
    // Only the owning camera hides the head; world views and shadows keep it.
    FBreachPose OwnerPose=BodyPose;BodyCloth.CopyTo(OwnerPose);
    OwnerPose.Apply(Body,true);
    WorldBody->RefreshBoneTransforms();
    Body->RefreshBoneTransforms();
    if(UsesSword() && bSwordRigReady)
    {
        UpdateSwordVisual(WorldPose,WorldBody,WorldSword,Dt);
        UpdateSwordVisual(OwnerPose,Body,Sword,Dt);
        UpdateScabbardVisual(WorldPose,WorldBody,WorldScabbard);
        UpdateScabbardVisual(OwnerPose,Body,Scabbard);
    }
    // Keep floor alignment out of the next animation transition's cached roots;
    // otherwise leaving a slide would apply the same tilt twice while blending.
    for(const auto& Root:UntiltedRoots) BodyPose.Local[Root.Key]=Root.Value;
    BodyPose.Rebuild();
}
float ABreachCharacter::GripError() const
{
    if(!bBodyRigReady) return BIG_NUMBER;
    if(UsesSword())
    {
        if(!bSwordRigReady || !Sword) return BIG_NUMBER;
        const int32 Hand=BodyPose.Bone(EBreachBone::RHand);
        return FVector::Distance(Sword->GetBoneLocationByName(TEXT("bone_002"),EBoneSpaces::WorldSpace),Body->GetBoneLocationByName(Body->GetBoneName(Hand),EBoneSpaces::WorldSpace));
    }
    float Error=0;
    for(int32 S=0;S<2;++S)
    {
        const int32 Hand=BodyPose.Bone(S?EBreachBone::RHand:EBreachBone::LHand);
        Error=FMath::Max(Error,float(FVector::Distance(OperatorGrip(this,S),Body->GetBoneLocationByName(Body->GetBoneName(Hand),EBoneSpaces::WorldSpace))));
    }
    return Error;
}
void ABreachCharacter::RestartRun()
{
    if(auto* PC=Cast<APlayerController>(Controller))
        if(auto* HUD=Cast<ABreachHUD>(PC->GetHUD()); HUD && HUD->IsSelectionOpen()) { HUD->ToggleSelection(); return; }
    UGameplayStatics::SetGamePaused(this,false);
    UGameplayStatics::OpenLevel(this,FName(TEXT("/Game/Maps/Arena")));
}
void ABreachCharacter::TogglePause()
{
    if(auto* PC=Cast<APlayerController>(Controller))
        if(auto* HUD=Cast<ABreachHUD>(PC->GetHUD()); HUD && HUD->IsSelectionOpen()) { HUD->ToggleSelection(); return; }
    const bool Paused=!UGameplayStatics::IsGamePaused(this);
    StopFire(); SetAim(false); bSprint=false;
    UGameplayStatics::SetGamePaused(this,Paused);
}
void ABreachCharacter::ToggleSelection()
{
    if(Health<=0) return;
    StopFire();SetAim(false);
    if(auto* PC=Cast<APlayerController>(Controller))
        if(auto* HUD=Cast<ABreachHUD>(PC->GetHUD())) HUD->ToggleSelection();
}

