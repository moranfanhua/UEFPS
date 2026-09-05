#include "BreachGame.h"
#include "BreachVisuals.h"
#include "CharacterBones.h"
#include "AIController.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
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
}
void ABreachEnemy::BeginPlay()
{
    Super::BeginPlay();
    Configure(ModelIndex,1);
    if(bDisplayOnly)
    {
        GetCharacterMovement()->DisableMovement();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
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
        const FBoxSphereBounds B=CharacterAsset->GetBounds();
        const float Scale=178.f/FMath::Max(1.f,float(B.BoxExtent.Z*2));
        Visual->SetRelativeScale3D(FVector(Scale));
        Visual->SetRelativeLocation(FVector(0,0,-89-(B.Origin.Z-B.BoxExtent.Z)*Scale));
        // PMX -> glTF front maps to +Y; align it with Unreal's +X forward.
        Visual->SetRelativeRotation(FRotator(0,-90.f,0));
        Pose.Init(CharacterAsset,ModelIndex);
    }
}
void ABreachEnemy::UpdatePose(float Dt)
{
    float Speed=GetVelocity().Size2D();
    Phase+=Dt*(Speed>10?7.f:2.f);
    Pose.Walk(Phase,Speed);
    Pose.Apply(Visual);
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
    FVector To=Player->GetActorLocation()-GetActorLocation(); To.Z=0;
    const float Distance=To.Size();
    const FVector Direction=To.GetSafeNormal();
    SetActorRotation(FMath::RInterpTo(GetActorRotation(),Direction.Rotation(),Dt,5));
    FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemySight),false,this);
    FHitResult Sight;
    const FVector Eye=GetActorLocation()+FVector(0,0,50);
    const bool HasSight=GetWorld()->LineTraceSingleByChannel(Sight,Eye,Player->GetActorLocation()+FVector(0,0,45),ECC_Visibility,Params) && Sight.GetActor()==Player;
    if(Distance>430 || !HasSight)
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
    else if(Distance>200)
    {
        AddMovementInput(FVector::CrossProduct(FVector::UpVector,Direction),FMath::Sin(Phase*.4f)*.45f);
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
        DeathStartPose=Pose.CS;
        DeathScale=Visual->GetRelativeScale3D();
        if(Event.IsOfType(FPointDamageEvent::ClassID))
            DeathDirection=FVector::DotProduct(static_cast<const FPointDamageEvent&>(Event).ShotDirection,GetActorForwardVector())>0?-1.f:1.f;
        FHitResult Floor;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(DeathFloor),false,this);
        DeathFloorZ=GetWorld()->LineTraceSingleByChannel(Floor,GetActorLocation(),GetActorLocation()-FVector(0,0,500),ECC_WorldStatic,Params)?Floor.ImpactPoint.Z:GetActorLocation().Z-89;
        GetCharacterMovement()->StopMovementImmediately();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetCharacterMovement()->DisableMovement();
        if(auto* GM=GetWorld()->GetAuthGameMode<ABreachGameMode>()) GM->EnemyDefeated(this,Damage>50);
    }
    return Damage;
}

void ABreachEnemy::UpdateDeathPose(float Dt)
{
    DespawnTime+=Dt;
    if(DespawnTime>9.f) { Destroy(); return; }
    if(DespawnTime-Dt>=1.4f) return;
    if(Pose.ReferenceCS.IsEmpty()) return;
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
        Pose.Rotate(S?EBreachBone::RThigh:EBreachBone::LThigh,FVector::ForwardVector,Value(&FKey::Thigh)*DeathDirection);
        Pose.Rotate(S?EBreachBone::RKnee:EBreachBone::LKnee,FVector::ForwardVector,Value(&FKey::Knee)*DeathDirection);
        const int32 Arm=Pose.Bone(S?EBreachBone::RArm:EBreachBone::LArm),Elbow=Pose.Bone(S?EBreachBone::RElbow:EBreachBone::LElbow),Hand=Pose.Bone(S?EBreachBone::RHand:EBreachBone::LHand);
        const float Side=FMath::Sign(Pose.ReferenceCS[Elbow].GetLocation().X-Pose.ReferenceCS[Arm].GetLocation().X);
        Pose.Aim(Arm,Elbow,FVector(Side*FMath::Lerp(.3f,.95f,Drop),.12f*DeathDirection,FMath::Lerp(-.9f,-.12f,Drop)));
        Pose.Aim(Elbow,Hand,FVector(Side*.65f,.16f*DeathDirection,-.25f));
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
    Pose.Apply(Visual);
}

