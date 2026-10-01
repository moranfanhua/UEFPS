#include "BreachSeabornEnemy.h"
#include "BreachGame.h"
#include "BreachNerveDamageComponent.h"
#include "BreachMovementComponent.h"
#include "BreachVisuals.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#if WITH_EDITOR
#include "Animation/IAnimationSequenceCompiler.h"
#endif

ABreachSeabornEnemy::ABreachSeabornEnemy()
{
    PrimaryActorTick.bCanEverTick=true;
    bReplicates=true;
    GetCapsuleComponent()->InitCapsuleSize(45,72);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCharacterMovement()->bRunPhysicsWithNoController=true;
    GetCharacterMovement()->BrakingDecelerationWalking=2400;
    Visual=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("SeabornVisual"));
    Visual->SetupAttachment(GetCapsuleComponent());
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetBoundsScale(2);
}

void ABreachSeabornEnemy::BeginPlay()
{
    Super::BeginPlay();
    if(bMechanicsEnabled && HasAuthority()) ActivateSpecies(Species);
}

bool ABreachSeabornEnemy::ActivateSpecies(EBreachSeabornSpecies Kind)
{
    if(!HasAuthority()) return false;
    const float FloorZ=GetActorLocation().Z-(Profile.bFlying && bRigReady?FlightAnchorHeight:GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
    Species=Kind;
    SetProfile();
    MaxHealth=Profile.Health*FBreachSeabornProfile::CombatScale;
    Health=MaxHealth;
    AttackCooldown=ActionTime=MoveTime=Incapacitated=Disarmed=DormantTime=0;
    bAwake=false; bHitApplied=false; AttackTarget.Reset();
    bMechanicsEnabled=true;
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetCharacterMovement()->SetMovementMode(Profile.bFlying?MOVE_Flying:MOVE_Walking);
    GetCharacterMovement()->MaxWalkSpeed=UBreachMovementComponent::UnarmedSpeed*Profile.Speed/1.9f;
    GetCharacterMovement()->MaxFlySpeed=GetCharacterMovement()->MaxWalkSpeed;
    LoadPresentation();
    FVector Position=GetActorLocation();
    Position.Z=FloorZ+(Profile.bFlying?FlightAnchorHeight:GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
    SetActorLocation(Position);
    SetAction(EBreachSeabornAction::Idle);
    ForceNetUpdate();
    return bRigReady;
}

void ABreachSeabornEnemy::LoadPresentation()
{
    const FString Folder=TEXT("/Game/Enemies/Seaborn/")+Profile.Key+TEXT("/Rig/");
    auto* Asset=LoadObject<USkeletalMesh>(nullptr,*(Folder+TEXT("SK_")+Profile.Key+TEXT(".SK_")+Profile.Key));
    bRigReady=Asset && Pose.InitSkeleton(Asset);
    if(!bRigReady) { Visual->SetVisibility(false); return; }
    Visual->SetVisibility(true);
    Visual->SetSkinnedAssetAndUpdate(Asset);
    const float Scale=Species==EBreachSeabornSpecies::ShellSeaRunner?.65f:1.f;
    Visual->SetRelativeScale3D(FVector(Scale));
    Visual->SetRelativeRotation(FRotator(0,-90,0));
    const auto Bounds=Asset->GetBounds();
    const float HalfHeight=FMath::Max(40.f,float(Bounds.BoxExtent.Z)*Scale);
    const float Radius=FMath::Clamp(float(FMath::Min(Bounds.BoxExtent.X,Bounds.BoxExtent.Y))*Scale,25.f,HalfHeight);
    GetCapsuleComponent()->SetCapsuleSize(Radius,HalfHeight);
    FlightAnchorHeight=Bounds.Origin.Z*Scale;
    Visual->SetRelativeLocation(FVector(0,0,Profile.bFlying?-FlightAnchorHeight:-HalfHeight-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale));
    Visual->SetRenderCustomDepth(true); Visual->SetCustomDepthStencilValue(1);
    const auto Load=[&](const TCHAR* Name)
    {
        const FString Object=TEXT("A_")+Profile.Key+TEXT("_")+Name;
        auto* Clip=LoadObject<UAnimSequence>(nullptr,*(Folder+Object+TEXT(".")+Object));
#if WITH_EDITOR
        if(Clip) UE::Anim::IAnimSequenceCompilingManager::FinishCompilation(TArray<UAnimSequence*>{Clip});
#endif
        return Clip;
    };
    if(Species==EBreachSeabornSpecies::ShellSeaRunner)
    {
        IdleClip=nullptr; MoveClip=Load(TEXT("Run")); AttackClip=Load(TEXT("Attack")); WakeClip=nullptr;
    }
    else
    {
        IdleClip=Load(TEXT("Idle")); MoveClip=Load(TEXT("Move")); AttackClip=Load(TEXT("Attack")); WakeClip=nullptr;
    }
    DieClip=Load(TEXT("Die"));
    bRigReady=MoveClip && AttackClip && DieClip;
    Pose.Apply(Visual);
}

void ABreachSeabornEnemy::OnRepSpecies()
{
    if(!bMechanicsEnabled) { Visual->SetVisibility(false); GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision); return; }
    SetProfile();
    LoadPresentation();
    OnRepAction();
}

void ABreachSeabornEnemy::OnRepAction()
{
    GetCapsuleComponent()->SetCollisionEnabled(bMechanicsEnabled && !IsDefeated()?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    if(IsDefeated()) { GetCharacterMovement()->StopMovementImmediately(); GetCharacterMovement()->DisableMovement(); }
    else if(bMechanicsEnabled) GetCharacterMovement()->SetMovementMode(Profile.bFlying?MOVE_Flying:MOVE_Walking);
}

void ABreachSeabornEnemy::SetProfile()
{
    Profile=FBreachSeabornProfile(); Profile.Key=TEXT("ShellSeaRunner");
    if(Species==EBreachSeabornSpecies::DeepSeaSlider)
    {
        Profile.Key=TEXT("DeepSeaSlider"); Profile.Health=2800; Profile.Defense=130;
        Profile.ArtsResistance=10; Profile.Speed=1.1f; Profile.Interval=2; Profile.NerveFraction=.15f;
    }
    else if(Species==EBreachSeabornSpecies::SpinalSeaSpitter)
    {
        Profile.Key=TEXT("SpinalSeaSpitter"); Profile.Health=4400; Profile.Defense=160;
        Profile.Speed=.75f; Profile.Interval=3; Profile.Range=2.5f*FBreachSeabornProfile::TileSize; Profile.bRanged=true;
    }
    else if(Species==EBreachSeabornSpecies::SeaDrifter)
    {
        Profile.Key=TEXT("SeaDrifter"); Profile.Attack=220; Profile.Defense=200;
        Profile.Speed=.75f; Profile.Interval=3; Profile.Range=2.5f*FBreachSeabornProfile::TileSize;
        Profile.NerveFraction=.2f; Profile.bRanged=true; Profile.bFlying=true;
    }
}

void ABreachSeabornEnemy::SetAction(EBreachSeabornAction Next)
{
    if(Action==Next && Next!=EBreachSeabornAction::Attack) return;
    Action=Next; ActionTime=0;
    ActionStarted=GetWorld()->GetTimeSeconds();
    ForceNetUpdate();
}

bool ABreachSeabornEnemy::CanHit(const ABreachCharacter* Player) const
{
    if(!IsValid(Player) || Player->Health<=0 || FVector::Dist(GetActorLocation(),Player->GetActorLocation())>Profile.Range) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SeabornAttack),false,this);
    const FVector Start=GetActorLocation();
    return !GetWorld()->LineTraceSingleByChannel(Hit,Start,Player->GetActorLocation(),ECC_Visibility,Params) || Hit.GetActor()==Player;
}

ABreachCharacter* ABreachSeabornEnemy::SelectTarget() const
{
    ABreachCharacter* Best=nullptr;
    float BestDistance=TNumericLimits<float>::Max();
    for(TActorIterator<ABreachCharacter> It(GetWorld());It;++It)
    {
        auto* Player=*It;
        if(Player->Health<=0 || Player->IsActorBeingDestroyed()) continue;
        const float Distance=FVector::DistSquared(GetActorLocation(),Player->GetActorLocation());
        if(Distance<BestDistance) { Best=Player; BestDistance=Distance; }
    }
    return Best;
}

void ABreachSeabornEnemy::ApplyIncapacitation(float Seconds)
{
    if(!HasAuthority() || IsDefeated() || !FMath::IsFinite(Seconds) || Seconds<=0) return;
    Incapacitated=FMath::Max(Incapacitated,Seconds);
    GetCharacterMovement()->StopMovementImmediately();
    if(Action==EBreachSeabornAction::Attack) { AttackTarget.Reset(); SetAction(EBreachSeabornAction::Idle); }
}

void ABreachSeabornEnemy::ApplyDisarm(float Seconds)
{
    if(!HasAuthority() || IsDefeated() || !FMath::IsFinite(Seconds) || Seconds<=0) return;
    Disarmed=FMath::Max(Disarmed,Seconds);
    if(Action==EBreachSeabornAction::Attack) { AttackTarget.Reset(); SetAction(EBreachSeabornAction::Idle); }
}

void ABreachSeabornEnemy::AdvanceMechanics(float Dt)
{
    if(!HasAuthority() || !bMechanicsEnabled || IsDefeated() || !FMath::IsFinite(Dt) || Dt<=0) return;
    AttackCooldown=FMath::Max(0.f,AttackCooldown-Dt);
    Disarmed=FMath::Max(0.f,Disarmed-Dt);
    if(Incapacitated>0) { Incapacitated=FMath::Max(0.f,Incapacitated-Dt); return; }
    if(Action==EBreachSeabornAction::Attack)
    {
        ActionTime+=Dt;
        const float Duration=GetAttackDuration();
        if(!bHitApplied && ActionTime>=Duration*.42f) { bHitApplied=true; ResolveAttack(); }
        if(ActionTime>=Duration) { AttackTarget.Reset(); SetAction(EBreachSeabornAction::Idle); }
        return;
    }
    auto* Player=SelectTarget();
    if(!Player) { SetAction(EBreachSeabornAction::Idle); return; }
    FVector Direction=Player->GetActorLocation()-GetActorLocation(); Direction.Z=0;
    if(!Direction.IsNearlyZero()) SetActorRotation(FRotator(0,Direction.Rotation().Yaw,0));
    if(CanHit(Player))
    {
        GetCharacterMovement()->StopMovementImmediately();
        SetAction(EBreachSeabornAction::Idle);
        if(AttackCooldown<=0 && Disarmed<=0)
        {
            AttackTarget=Player; bHitApplied=false; AttackCooldown=Profile.Interval;
            SetAction(EBreachSeabornAction::Attack);
        }
    }
    else
    {
        SetAction(EBreachSeabornAction::Move);
        if(Profile.bFlying)
        {
            FHitResult Ground;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(SeabornFlight),false,this);
            Params.AddIgnoredActor(Player);
            if(GetWorld()->LineTraceSingleByChannel(Ground,GetActorLocation()+FVector(0,0,300),GetActorLocation()-FVector(0,0,2000),ECC_WorldStatic,Params))
                Direction.Z=Ground.ImpactPoint.Z+FlightAnchorHeight-GetActorLocation().Z;
        }
        AddMovementInput(Direction.GetSafeNormal(),1,true);
    }
}

float ABreachSeabornEnemy::GetAttackDuration() const { return AttackClip?AttackClip->GetPlayLength():.8f; }

void ABreachSeabornEnemy::ResolveAttack()
{
    auto* Player=AttackTarget.Get();
    if(!CanHit(Player) || IsDefeated() || Incapacitated>0 || Disarmed>0) return;
    if(Profile.bRanged)
        Breach::Beam(GetWorld(),GetActorLocation(),Player->GetActorLocation(),FLinearColor(.3f,.7f,.65f),3.f,.12f);
    UGameplayStatics::ApplyDamage(Player,Profile.Attack*FBreachSeabornProfile::CombatScale,GetController(),this,UDamageType::StaticClass());
    Player->NerveDamage->ApplyNerveDamage(Profile.Attack*Profile.NerveFraction,this);
}

float ABreachSeabornEnemy::TakeDamage(float Damage,const FDamageEvent& Event,AController* DamageInstigator,AActor* Causer)
{
    if(!HasAuthority() || !bMechanicsEnabled || IsDefeated() || !FMath::IsFinite(Damage) || Damage<=0) return 0;
    const auto* Type=Event.DamageTypeClass?Event.DamageTypeClass->GetDefaultObject<UDamageType>():nullptr;
    float Applied=Damage;
    if(Type && Type->IsA<UBreachArtsDamage>()) Applied*=1.f-Profile.ArtsResistance/100.f;
    else if(!Type || !Type->IsA<UBreachTrueDamage>()) Applied=FMath::Max(Damage*.05f,Damage-Profile.Defense*FBreachSeabornProfile::CombatScale);
    Applied=FMath::Min(Applied,Health);
    Health-=Applied;
    if(Health<=0) Die();
    ForceNetUpdate();
    return Applied;
}

void ABreachSeabornEnemy::Die()
{
    Health=0; AttackTarget.Reset(); bHitApplied=true;
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetAction(EBreachSeabornAction::Dead);
}

void ABreachSeabornEnemy::UpdatePresentation(float Dt)
{
    if(!bRigReady) return;
    UAnimSequence* Clip=IdleClip;
    float Time=MoveTime; bool Loop=true;
    if(Action==EBreachSeabornAction::Attack) { Clip=AttackClip; Time=ActionTime; Loop=false; }
    else if(Action==EBreachSeabornAction::Dead) { Clip=DieClip; Time=ActionTime; Loop=false; }
    else if(Action==EBreachSeabornAction::Wake) { Clip=WakeClip; Time=ActionTime; Loop=false; }
    else if(Action==EBreachSeabornAction::Move) Clip=MoveClip;
    if(Clip) Pose.Sample(Clip,Time,Loop);
    else Pose.Local=Pose.Reference;
    Pose.Rebuild(); Pose.Apply(Visual);
}

void ABreachSeabornEnemy::Tick(float Dt)
{
    Super::Tick(Dt);
    if(!bMechanicsEnabled) return;
    if(HasAuthority()) AdvanceMechanics(Dt);
    else if(Incapacitated<=0)
    {
        const auto* State=GetWorld()->GetGameState();
        ActionTime=FMath::Max(0.f,(State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds())-ActionStarted);
    }
    if(IsDefeated()) ActionTime+=HasAuthority()?Dt:0;
    if(Incapacitated<=0) MoveTime+=Dt*(Action==EBreachSeabornAction::Move?FMath::Max(.1f,GetVelocity().Size2D()/350.f):1.f);
    UpdatePresentation(Dt);
}

void ABreachSeabornEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABreachSeabornEnemy,Species);
    DOREPLIFETIME(ABreachSeabornEnemy,bMechanicsEnabled);
    DOREPLIFETIME(ABreachSeabornEnemy,Health);
    DOREPLIFETIME(ABreachSeabornEnemy,MaxHealth);
    DOREPLIFETIME(ABreachSeabornEnemy,Action);
    DOREPLIFETIME(ABreachSeabornEnemy,bAwake);
    DOREPLIFETIME(ABreachSeabornEnemy,ActionStarted);
    DOREPLIFETIME(ABreachSeabornEnemy,Incapacitated);
}
