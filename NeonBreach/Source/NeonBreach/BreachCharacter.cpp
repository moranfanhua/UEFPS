#include "BreachGame.h"
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

ABreachCharacter::ABreachCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 92.f);
    GetCharacterMovement()->MaxWalkSpeed = 510.f;
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
    Camera->FieldOfView = 96;
    Camera->SetEnableFirstPersonFieldOfView(true);
    Camera->SetEnableFirstPersonScale(true);
    Camera->SetFirstPersonFieldOfView(96.f);
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
void ABreachCharacter::StartFire() { if(!bUnarmed) { bTrigger=true; Fire(); } }
void ABreachCharacter::StopFire() { bTrigger=false; }
void ABreachCharacter::SetAim(bool Value) { bAiming=Value && !bUnarmed; }

void ABreachCharacter::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bTrigger) Fire();
    bSprint=bUnarmed && !bIsCrouched;
    GetCharacterMovement()->MaxWalkSpeed = bAiming ? 300.f : (bSprint ? 790.f : 510.f);
    Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, bAiming ? 66.f : 96.f, Dt, 12));
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
    MuzzleLight->SetIntensity(Recoil>.55f ? 5000.f : 0.f);
    HitMarker=FMath::Max(0.f,HitMarker-Dt);
    DamageFlash=FMath::Max(0.f,DamageFlash-Dt);
}

void ABreachCharacter::Fire()
{
    if(Health<=0 || bUnarmed || bReloading || UGameplayStatics::IsGamePaused(this)) return;
    const float Now=GetWorld()->GetTimeSeconds();
    if(Now<NextShot) return;
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
    OperatorIndex=FMath::Clamp(Index,0,3);
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
        LoadLocomotionAnimations();
        UpdateOperatorPose(0);
    }
    if(auto* GM=GetWorld()->GetAuthGameMode<ABreachGameMode>())
    {
        GM->Notice=FString::Printf(TEXT("OPERATOR LINK / %s"),Breach::Names[OperatorIndex]); GM->NoticeTime=2.5f;
    }
}

static FVector OperatorGrip(const ABreachCharacter* Player,int32 Side)
{
    FVector Grip=Side?FVector(-18,7,-10):FVector(0,-9,-7);
    if(!Side && Player->bReloading)
        Grip=FMath::Lerp(Grip,FVector(-1,-10,-24),FMath::Sin(Player->ReloadProgress*PI));
    return Player->WeaponRoot->GetComponentTransform().TransformPosition(Grip);
}
void ABreachCharacter::UpdateOperatorPose(float Dt)
{
    if(!bBodyRigReady) return;
    UpdateLocomotion(Dt);
    const auto Bounds=Body->GetSkinnedAsset()->GetBounds();
    const float BaseZ=-GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Body->GetRelativeScale3D().Z;
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
    const float HalfHeight=GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
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
    };
    const bool SeparateWorldArms=!bUnarmed && !OwnerAdjustment.IsNearlyZero(.01f);
    if(SeparateWorldArms)
    {
        FBreachPose WorldPose=BodyPose;
        PoseArms(WorldPose,WorldBody->GetComponentTransform().Inverse());
        WorldPose.Apply(WorldBody);
    }
    PoseArms(BodyPose,ToBody);
    // Both representations use the complete source mesh and locomotion pose.
    // Only the owning camera hides the head; world views and shadows keep it.
    if(!SeparateWorldArms) BodyPose.Apply(WorldBody);
    BodyPose.Apply(Body,true);
    WorldBody->RefreshBoneTransforms();
    Body->RefreshBoneTransforms();
}
float ABreachCharacter::GripError() const
{
    if(!bBodyRigReady) return BIG_NUMBER;
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
    StopFire(); bAiming=false; bSprint=false;
    UGameplayStatics::SetGamePaused(this,Paused);
}
void ABreachCharacter::ToggleSelection()
{
    if(Health<=0) return;
    StopFire();bAiming=false;
    if(auto* PC=Cast<APlayerController>(Controller))
        if(auto* HUD=Cast<ABreachHUD>(PC->GetHUD())) HUD->ToggleSelection();
}

