#include "BreachGame.h"
#include "BreachVisuals.h"
#include "BreachMovementComponent.h"
#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"

ABreachEnemy::ABreachEnemy()
{
    PrimaryActorTick.bCanEverTick=true;
    GetCapsuleComponent()->InitCapsuleSize(33,89);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    GetCharacterMovement()->MaxWalkSpeed=190;
    GetCharacterMovement()->bOrientRotationToMovement=false;
    bUseControllerRotationYaw=false;
    AutoPossessAI=EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass=AAIController::StaticClass();
    Visual=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("CharacterVisual"));
    Visual->SetupAttachment(GetCapsuleComponent());
    Visual->SetRelativeLocation(FVector(0,0,-89));
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetCastShadow(true);
    Visual->bCastCinematicShadow=true;
    RunnerVisual=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("ShellSeaRunnerVisual"));
    RunnerVisual->SetupAttachment(GetCapsuleComponent());
    RunnerVisual->SetVisibility(false);
    RunnerVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RunnerVisual->SetCastShadow(true);
    RunnerHitbox=CreateDefaultSubobject<UBoxComponent>(TEXT("ShellSeaRunnerHitbox"));
    RunnerHitbox->SetupAttachment(GetCapsuleComponent());
    RunnerHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RunnerHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    RunnerHitbox->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
}
void ABreachEnemy::BeginPlay()
{
    Super::BeginPlay();
    Breach::EnableToonStencil(Visual);
    Breach::EnableToonStencil(RunnerVisual);
    if(bShellSeaRunner) ConfigureShellSeaRunner(1);
    else Configure(ModelIndex,1);
    if(bDisplayOnly)
    {
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}
void ABreachEnemy::ConfigureShellSeaRunner(int32 Wave)
{
    bShellSeaRunner=true;
    Health=MaxHealth=85+Wave*9;
    GetCapsuleComponent()->SetCapsuleSize(43.f,70.f);
    GetCharacterMovement()->MaxWalkSpeed=UBreachMovementComponent::UnarmedSpeed;
    GetCharacterMovement()->MaxAcceleration=2200.f;
    GetCharacterMovement()->BrakingDecelerationWalking=6000.f;
    Visual->SetVisibility(false);
    RunnerVisual->SetVisibility(true);
    // The skeletal FBX faces +X; retain the static study's scale and UVs.
    RunnerVisual->SetRelativeScale3D(FVector(.65f));
    RunnerVisual->SetRelativeLocation(FVector(0,0,-71.5f));
    RunnerVisual->SetRelativeRotation(FRotator::ZeroRotator);
    USkeletalMesh* RunnerMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Enemies/Seaborn/ShellSeaRunner/Rig/SK_ShellSeaRunner.SK_ShellSeaRunner"));
    RunnerRun=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Enemies/Seaborn/ShellSeaRunner/Rig/A_ShellSeaRunner_Run.A_ShellSeaRunner_Run"));
    RunnerAttack=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Enemies/Seaborn/ShellSeaRunner/Rig/A_ShellSeaRunner_Attack.A_ShellSeaRunner_Attack"));
    RunnerDie=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Enemies/Seaborn/ShellSeaRunner/Rig/A_ShellSeaRunner_Die.A_ShellSeaRunner_Die"));
    if(!RunnerMesh || !RunnerRun || RunnerRun->GetSkeleton()!=RunnerMesh->GetSkeleton() || !RunnerPose.InitSkeleton(RunnerMesh))
    {
        UE_LOG(LogTemp,Warning,TEXT("ShellSeaRunner rig or run animation is missing; using the ordinary enemy visual"));
        bShellSeaRunner=false;
        RunnerVisual->SetVisibility(false);
        RunnerHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Visual->SetVisibility(true);
        GetCapsuleComponent()->SetCapsuleSize(33.f,89.f);
        Configure(0,Wave);
        return;
    }
    RunnerVisual->SetSkinnedAssetAndUpdate(RunnerMesh);
    for(TObjectPtr<UAnimSequence>* Clip:{&RunnerAttack,&RunnerDie})
        if(*Clip && (*Clip)->GetSkeleton()!=RunnerMesh->GetSkeleton()) *Clip=nullptr;
    RunnerPose.Apply(RunnerVisual);
    const FBoxSphereBounds Bounds=RunnerMesh->GetBounds();
    RunnerHitbox->SetRelativeLocation(Bounds.Origin*.65f+RunnerVisual->GetRelativeLocation());
    RunnerHitbox->SetBoxExtent(Bounds.BoxExtent*.65f);
    RunnerHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    RunnerTime=RunnerBlend=0.f;
    RunnerAttackTime=-1.f;
    RunnerActionStart.Reset();
    AttackCooldown=.5f;
    DeathAnimation=nullptr;
    DeathBoneIndices.Reset();
}
bool ABreachEnemy::SampleRunnerPose(float Time,float Blend)
{
    if(!RunnerPose.Sample(RunnerRun,Time,true)) return false;
    const float Alpha=FMath::Clamp(Blend,0.f,1.f);
    for(int32 I=0;I<RunnerPose.Local.Num();++I)
    {
        FTransform Animated=RunnerPose.Local[I];
        RunnerPose.Local[I].Blend(RunnerPose.Reference[I],Animated,Alpha);
    }
    RunnerPose.Rebuild();
    RunnerPose.Apply(RunnerVisual);
    return true;
}
bool ABreachEnemy::SampleRunnerAction(EBreachRunnerAction Action,float Time)
{
    UAnimSequence* Clip=Action==EBreachRunnerAction::Attack?RunnerAttack.Get():(Action==EBreachRunnerAction::Die?RunnerDie.Get():RunnerRun.Get());
    if(!RunnerPose.Sample(Clip,Time,Action==EBreachRunnerAction::Run)) return false;
    RunnerPose.Apply(RunnerVisual);
    return true;
}
bool ABreachEnemy::StartRunnerAttack()
{
    if(!bShellSeaRunner || bDefeated || !RunnerAttack || IsRunnerAttacking()) return false;
    RunnerActionStart=RunnerPose.Local;
    RunnerAttackTime=0.f;
    ConsumeMovementInputVector();
    GetCharacterMovement()->StopMovementImmediately();
    return true;
}
void ABreachEnemy::Configure(int32 Index,int32 Wave)
{
    ModelIndex=FMath::Clamp(Index,0,3);
    Health=MaxHealth=85+Wave*9;
    GetCharacterMovement()->MaxWalkSpeed=FMath::Min(310.f,165.f+Wave*13);
    AttackCooldown=2.f+FMath::FRand();
    Phase=FMath::FRand()*2*PI;
    if(auto* CharacterAsset=Breach::CharacterMesh(ModelIndex))
    {
        Visual->SetSkinnedAssetAndUpdate(CharacterAsset);
        // Reset any live component overrides when an enemy is reconfigured.
        for(int32 Slot : {10,11,12,13,14}) Visual->SetMaterial(Slot,nullptr);
        if(ModelIndex==0)
        {
            // The imported Eula mesh keeps its original SkeletalMaterial array
            // on reload, so bind the verified cape material on the live mesh.
            if(auto* Cape=Breach::Material(TEXT("M_Eula_CapeCorrect")))
                for(int32 Slot : {10,11,12,13,14}) Visual->SetMaterial(Slot,Cape);
        }
        if(ModelIndex==0)
            for(int32 Slot=0;Slot<Visual->GetNumMaterials();++Slot)
                UE_LOG(LogTemp,Display,TEXT("EULA_MATERIAL_SLOT %d %s"),Slot,*Visual->GetMaterial(Slot)->GetName());
        const FBoxSphereBounds B=CharacterAsset->GetBounds();
        const float Scale=178.f/FMath::Max(1.f,float(B.BoxExtent.Z*2));
        Visual->SetRelativeScale3D(FVector(Scale));
        Visual->SetRelativeLocation(FVector(0,0,-89-(B.Origin.Z-B.BoxExtent.Z)*Scale));
        // PMX -> glTF front maps to +Y; align it with Unreal's +X forward.
        Visual->SetRelativeRotation(FRotator(0,-90.f,0));
        Pose.Init(CharacterAsset,ModelIndex);
        Cloth.Init(CharacterAsset,ModelIndex);
        DeathAnimation=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Animations/Death/A_%s_Death01.A_%s_Death01"),Breach::Keys[ModelIndex],Breach::Keys[ModelIndex]));
        DeathBoneIndices.Reset();
        if(DeathAnimation)
            for(int32 I=0;I<Pose.Reference.Num();++I)
                DeathBoneIndices.Add(DeathAnimation->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(CharacterAsset->GetRefSkeleton().GetBoneName(I)));
    }
}
void ABreachEnemy::UpdatePose(float Dt)
{
    float Speed=GetVelocity().Size2D();
    if(bShellSeaRunner)
    {
        if(IsRunnerAttacking())
        {
            RunnerAttackTime+=FMath::Max(0.f,Dt);
            SampleRunnerAction(EBreachRunnerAction::Attack,RunnerAttackTime);
            const float Alpha=FMath::Clamp(RunnerAttackTime/.1f,0.f,1.f);
            for(int32 I=0;I<RunnerPose.Local.Num() && RunnerActionStart.IsValidIndex(I);++I)
            {
                const FTransform Target=RunnerPose.Local[I];
                RunnerPose.Local[I].Blend(RunnerActionStart[I],Target,Alpha);
            }
            RunnerPose.Rebuild(); RunnerPose.Apply(RunnerVisual);
            if(RunnerAttackTime>=RunnerAttack->GetPlayLength())
            {
                RunnerAttackTime=-1.f;
                RunnerBlend=0.f;
                AttackCooldown=.5f;
            }
            return;
        }
        // The reference's 0.533 s in-place gallop is authored for 700 cm/s.
        // CharacterMovement supplies all translation, so no root motion is added.
        RunnerTime+=FMath::Max(Dt,0.f)*Speed/700.f;
        RunnerBlend=FMath::FInterpTo(RunnerBlend,FMath::Clamp(Speed/180.f,0.f,1.f),Dt,10.f);
        SampleRunnerPose(RunnerTime,RunnerBlend);
        return;
    }
    Phase+=Dt*(Speed>10?7.f:2.f);
    Pose.Walk(Phase,Speed);
    FBreachPose Display=Pose;Cloth.Update(Display,Visual->GetComponentTransform(),Dt,GetWorld(),this);Display.Apply(Visual);
}
void ABreachEnemy::Tick(float Dt)
{
    Super::Tick(Dt);
    if(bDefeated)
    {
        UpdateDeathPose(Dt);
        return;
    }
    UpdatePose(Dt);
    if(bDisplayOnly) return;
    auto* GM=GetWorld()->GetAuthGameMode<ABreachGameMode>();
    auto* Player=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!GM || GM->bGameOver || GM->bGallery || !Player) return;
    if(bShellSeaRunner && IsRunnerAttacking()) return;
    FVector To=Player->GetActorLocation()-GetActorLocation(); To.Z=0;
    const float Distance=To.Size();
    const FVector Direction=To.GetSafeNormal();
    SetActorRotation(FMath::RInterpTo(GetActorRotation(),Direction.Rotation(),Dt,5));
    FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemySight),false,this);
    FHitResult Sight;
    const FVector Eye=GetActorLocation()+FVector(0,0,50);
    const bool SightBlocked=GetWorld()->LineTraceSingleByChannel(Sight,Eye,Player->GetActorLocation()+FVector(0,0,45),ECC_Visibility,Params);
    // Player capsules may ignore Visibility; an unobstructed ray still sees them.
    const bool HasSight=bShellSeaRunner?(!SightBlocked || Sight.GetActor()==Player):(SightBlocked && Sight.GetActor()==Player);
    if(bShellSeaRunner ? Distance>165.f : (Distance>430 || !HasSight))
    {
        FVector Move=Direction;
        FHitResult Obstacle;
        Params.AddIgnoredActor(Player);
        if(GetWorld()->LineTraceSingleByChannel(Obstacle,GetActorLocation(),GetActorLocation()+Move*150,ECC_WorldStatic,Params))
        {
            const FVector Right=FVector::CrossProduct(FVector::UpVector,Move);
            FVector LeftGoal=GetActorLocation()+(Move*.25f+Right)*190;
            FHitResult SideHit;
            const bool BlockRight=GetWorld()->LineTraceSingleByChannel(SideHit,GetActorLocation(),LeftGoal,ECC_WorldStatic,Params);
            Move=(Move*.2f+Right*(BlockRight?-1.f:1.f)).GetSafeNormal();
        }
        AddMovementInput(Move);
    }
    else if(!bShellSeaRunner && Distance>200)
    {
        AddMovementInput(FVector::CrossProduct(FVector::UpVector,Direction),FMath::Sin(Phase*.4f)*.45f);
    }
    if(bShellSeaRunner)
    {
        AttackCooldown-=Dt;
        if(HasSight && Distance<=175.f && GetVelocity().Size2D()<40.f && AttackCooldown<=0.f)
            StartRunnerAttack();
        return; // Bite animation only; contact and attack damage remain undefined.
    }
    AttackCooldown-=Dt;
    if(HasSight && Distance<1700 && AttackCooldown<=0)
    {
        AttackCooldown=FMath::Max(.9f,2.3f-GM->Wave*.08f)+FMath::FRandRange(0.f,.6f);
        Breach::Beam(GetWorld(),Eye+Direction*35,Player->Camera->GetComponentLocation(),FLinearColor(1,.22f,.035f),2.5f,.14f);
        const float HitChance=Player->GetVelocity().Size2D()>400?.22f:.62f;
        if(FMath::FRand()<HitChance) UGameplayStatics::ApplyDamage(Player,7.f+GM->Wave,GetController(),this,UDamageType::StaticClass());
    }
}
float ABreachEnemy::TakeDamage(float Damage,const FDamageEvent& Event,AController* DamageInstigator,AActor* Causer)
{
    if(bDisplayOnly || bDefeated) return 0;
    Health=FMath::Max(0.f,Health-Damage);
    if(Health<=0)
    {
        bDefeated=true;
        if(bShellSeaRunner)
        {
            RunnerActionStart=RunnerPose.Local;
            RunnerAttackTime=-1.f;
        }
        if(!bShellSeaRunner)
        {
            DeathStartPose=Pose.CS;
            DeathScale=Visual->GetRelativeScale3D();
        }
        if(Event.IsOfType(FPointDamageEvent::ClassID))
            DeathDirection=FVector::DotProduct(static_cast<const FPointDamageEvent&>(Event).ShotDirection,GetActorForwardVector())>0?-1.f:1.f;
        FHitResult Floor;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(DeathFloor),false,this);
        DeathFloorZ=GetWorld()->LineTraceSingleByChannel(Floor,GetActorLocation(),GetActorLocation()-FVector(0,0,500),ECC_WorldStatic,Params)?Floor.ImpactPoint.Z:GetActorLocation().Z-89;
        GetCharacterMovement()->StopMovementImmediately();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if(bShellSeaRunner) RunnerHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetCharacterMovement()->DisableMovement();
        if(auto* GM=GetWorld()->GetAuthGameMode<ABreachGameMode>()) GM->EnemyDefeated(this,Event.IsOfType(FPointDamageEvent::ClassID) && Damage>50);
    }
    return Damage;
}

void ABreachEnemy::UpdateDeathPose(float Dt)
{
    DespawnTime+=Dt;
    if(DespawnTime>9.f) { Destroy(); return; }
    if(bShellSeaRunner)
    {
        if(RunnerDie && SampleRunnerAction(EBreachRunnerAction::Die,DespawnTime))
        {
            const float Alpha=FMath::Clamp(DespawnTime/.12f,0.f,1.f);
            for(int32 I=0;I<RunnerPose.Local.Num() && RunnerActionStart.IsValidIndex(I);++I)
            {
                const FTransform Target=RunnerPose.Local[I];
                RunnerPose.Local[I].Blend(RunnerActionStart[I],Target,Alpha);
            }
            RunnerPose.Rebuild(); RunnerPose.Apply(RunnerVisual);
            RunnerVisual->SetRelativeLocation(FVector(0,0,-71.5f+DeathFloorZ-(GetActorLocation().Z-70.f)));
            return;
        }
        const float Fall=FMath::Clamp(DespawnTime/.55f,0.f,1.f);
        RunnerVisual->SetRelativeLocation(FVector(0,0,FMath::Lerp(RunnerVisual->GetRelativeLocation().Z,-92.f,Fall)));
        RunnerVisual->SetRelativeRotation(FRotator(Fall*22.f,0.f,Fall*28.f*DeathDirection));
        return;
    }
    if(Pose.ReferenceCS.IsEmpty()) return;
    if(DeathAnimation)
    {
        const float Duration=DeathAnimation->GetPlayLength();
        if(DespawnTime-Dt>=Duration) return;
        Pose.Reset();
        for(int32 I=0;I<Pose.Local.Num();++I)
            if(DeathBoneIndices.IsValidIndex(I) && DeathBoneIndices[I]>=0)
                DeathAnimation->GetBoneTransform(Pose.Local[I],FSkeletonPoseBoneIndex(DeathBoneIndices[I]),FAnimExtractContext(double(FMath::Min(DespawnTime,Duration)),false),false);
        Pose.Rebuild();
        const float Blend=FMath::Clamp(DespawnTime/.15f,0.f,1.f);
        const float GroundOffset=(DeathFloorZ-(GetActorLocation().Z-89.f))/DeathScale.X;
        for(int32 I=0;I<Pose.CS.Num();++I)
        {
            FTransform Target=Pose.CS[I]; Target.AddToTranslation(FVector(0,0,GroundOffset));
            if(DeathStartPose.IsValidIndex(I)) Pose.CS[I].Blend(DeathStartPose[I],Target,Blend);
            else Pose.CS[I]=Target;
        }
        Cloth.Update(Pose,Visual->GetComponentTransform(),Dt,GetWorld(),this);Pose.Apply(Visual);
        return;
    }
    if(DespawnTime-Dt>=1.4f) return;
    struct FKey { float Time,Tilt,Drop,Thigh,Knee; };
    static const FKey Keys[]={{0,0,0,0,0},{.18f,8,.08f,12,-20},{.5f,32,.44f,45,-68},{.95f,77,.85f,28,-42},{1.4f,90,1,7,-16}};
    const float T=FMath::Min(DespawnTime,1.4f);
    int32 K=1; while(K<4 && T>Keys[K].Time) ++K;
    float A=FMath::Clamp((T-Keys[K-1].Time)/(Keys[K].Time-Keys[K-1].Time),0.f,1.f);
    A=A*A*(3-2*A);
    const auto Value=[&](float FKey::* Field) { return FMath::Lerp(Keys[K-1].*Field,Keys[K].*Field,A); };
    const float Drop=Value(&FKey::Drop),Scale=DeathScale.X;
    Pose.Reset();
    Pose.Rotate(EBreachBone::Spine,FVector::ForwardVector,-FMath::Sin(Drop*PI)*12*DeathDirection);
    Pose.Rotate(EBreachBone::Neck,FVector::ForwardVector,6*Drop*DeathDirection);
    for(int32 S=0;S<2;++S)
    {
        Pose.Rotate(S?EBreachBone::RThigh:EBreachBone::LThigh,FVector::ForwardVector,Value(&FKey::Thigh));
        Pose.Rotate(S?EBreachBone::RKnee:EBreachBone::LKnee,FVector::ForwardVector,Value(&FKey::Knee));
        const int32 Arm=Pose.Bone(S?EBreachBone::RArm:EBreachBone::LArm),Elbow=Pose.Bone(S?EBreachBone::RElbow:EBreachBone::LElbow),Hand=Pose.Bone(S?EBreachBone::RHand:EBreachBone::LHand);
        const float Side=FMath::Sign(Pose.ReferenceCS[Elbow].GetLocation().X-Pose.ReferenceCS[Arm].GetLocation().X);
        const float Spread=.28f+FMath::Sin(Drop*PI)*.6f;
        Pose.Aim(Arm,Elbow,FVector(Side*Spread,.08f*DeathDirection,-FMath::Sqrt(1-Spread*Spread)));
        Pose.Aim(Elbow,Hand,FVector(Side*(S?.35f:.48f),.08f*DeathDirection,-.85f));
    }
    // Blend out of the exact pose at the hit before the knees buckle.
    const float Blend=FMath::Clamp(T/.22f,0.f,1.f);
    for(int32 i=0;i<Pose.CS.Num() && DeathStartPose.IsValidIndex(i);++i)
    {
        FTransform Blended; Blended.Blend(DeathStartPose[i],Pose.CS[i],Blend); Pose.CS[i]=Blended;
    }
    const FVector Pivot=Pose.ReferenceCS[Pose.Bone(EBreachBone::Pelvis)].GetLocation();
    const FVector Ground=Visual->GetComponentTransform().InverseTransformPosition(FVector(GetActorLocation().X,GetActorLocation().Y,DeathFloorZ));
    const FVector Center(Pivot.X,Pivot.Y-38.f*Drop*DeathDirection/Scale,FMath::Lerp(Pivot.Z,Ground.Z+14.f/Scale,Drop));
    const FQuat Fall(FVector::ForwardVector,FMath::DegreesToRadians(Value(&FKey::Tilt)*DeathDirection));
    for(FTransform& Transform:Pose.CS)
    {
        Transform.SetLocation(Center+Fall.RotateVector(Transform.GetLocation()-Pivot));
        Transform.SetRotation((Fall*Transform.GetRotation()).GetNormalized());
    }
    float Lift=0;
    for(EBreachBone Contact:{EBreachBone::Pelvis,EBreachBone::Chest,EBreachBone::Head,EBreachBone::LHand,EBreachBone::RHand,EBreachBone::LFoot,EBreachBone::RFoot})
    {
        const float Radius=Contact==EBreachBone::Head?9.f:(Contact==EBreachBone::Pelvis || Contact==EBreachBone::Chest?12.f:3.f);
        Lift=FMath::Max(Lift,float(Ground.Z+Radius/Scale-Pose.CS[Pose.Bone(Contact)].GetLocation().Z));
    }
    for(FTransform& Transform:Pose.CS) Transform.AddToTranslation(FVector(0,0,Lift));
    Cloth.Update(Pose,Visual->GetComponentTransform(),Dt,GetWorld(),this);Pose.Apply(Visual);
}
