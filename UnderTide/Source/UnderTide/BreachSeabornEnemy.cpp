#include "BreachSeabornEnemy.h"
#include "BreachSeabornProjectile.h"
#include "BreachGame.h"
#include "BreachNerveDamageComponent.h"
#include "BreachMovementComponent.h"
#include "BreachEnemyAwareness.h"
#include "BreachWeapons.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
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
    Awareness=CreateDefaultSubobject<UBreachEnemyAwareness>(TEXT("EnemyAwareness"));
    bReplicates=true;
    GetCapsuleComponent()->InitCapsuleSize(45,72);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetCharacterMovement()->bRunPhysicsWithNoController=true;
    GetCharacterMovement()->BrakingDecelerationWalking=2400;
    GetCharacterMovement()->DisableMovement();
    Visual=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("SeabornVisual"));
    Visual->SetupAttachment(GetCapsuleComponent());
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetBoundsScale(2);
    DamageHitbox=CreateDefaultSubobject<UBoxComponent>(TEXT("SeabornDamageHitbox"));
    DamageHitbox->SetupAttachment(Visual);
    DamageHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DamageHitbox->SetCollisionResponseToAllChannels(ECR_Ignore);
    DamageHitbox->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
}

void ABreachSeabornEnemy::BeginPlay()
{
    Super::BeginPlay();
    if(bMechanicsEnabled && HasAuthority()) ActivateSpecies(Species);
}

bool ABreachSeabornEnemy::ActivateSpecies(EBreachSeabornSpecies Kind)
{
    if(!HasAuthority() || uint8(Kind)>uint8(EBreachSeabornSpecies::FirstSeaPiercer)) return false;
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
    LoadPresentation();
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,Profile.bDormant?ECR_Ignore:ECR_Block);
    FVector Position=GetActorLocation();
    Position.Z=FloorZ+(Profile.bFlying?FlightAnchorHeight:GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
    SetActorLocation(Position);
    Awareness->Reset(UBreachMovementComponent::UnarmedSpeed*Profile.Speed/1.9f);
    SetAction(EBreachSeabornAction::Idle);
    DamageHitbox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
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
    // The runner FBX faces +X; the other Seaborn rigs face +Y.
    Visual->SetRelativeRotation(Species==EBreachSeabornSpecies::ShellSeaRunner?FRotator::ZeroRotator:FRotator(0,-90,0));
    const auto Bounds=Asset->GetBounds();
    const float HalfHeight=FMath::Max(40.f,float(Bounds.BoxExtent.Z)*Scale);
    const float Radius=FMath::Clamp(float(FMath::Min(Bounds.BoxExtent.X,Bounds.BoxExtent.Y))*Scale,25.f,FMath::Min(60.f,HalfHeight));
    GetCapsuleComponent()->SetCapsuleSize(Radius,HalfHeight);
    FlightAnchorHeight=Bounds.Origin.Z*Scale;
    Visual->SetRelativeLocation(FVector(0,0,Profile.bFlying?-FlightAnchorHeight:-HalfHeight-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale));
    Visual->SetRenderCustomDepth(true); Visual->SetCustomDepthStencilValue(1);
    DamageHitbox->SetBoxExtent(Bounds.BoxExtent);
    DamageHitbox->SetRelativeLocation(Bounds.Origin);
    DamageHitbox->SetCollisionEnabled(IsDefeated()?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryOnly);
    const auto Load=[&](const TCHAR* Name)
    {
        const FString Object=TEXT("A_")+Profile.Key+TEXT("_")+Name;
        auto* Clip=LoadObject<UAnimSequence>(nullptr,*(Folder+Object+TEXT(".")+Object));
#if WITH_EDITOR
        if(Clip) UE::Anim::IAnimSequenceCompilingManager::FinishCompilation(TArray<UAnimSequence*>{Clip});
#endif
        return Clip;
    };
    AwakeMoveClip=nullptr;
    if(Species==EBreachSeabornSpecies::ShellSeaRunner)
    {
        IdleClip=nullptr; MoveClip=Load(TEXT("Run")); AttackClip=Load(TEXT("Attack")); WakeClip=nullptr;
    }
    else if(Profile.bDormant)
    {
        IdleClip=Load(TEXT("Idle")); MoveClip=Load(TEXT("Move"));
        AttackClip=Load(TEXT("Skill_Attack")); WakeClip=Load(TEXT("Skill_Begin")); AwakeMoveClip=Load(TEXT("Skill_Move"));
    }
    else
    {
        IdleClip=Load(TEXT("Idle")); MoveClip=Load(TEXT("Move")); AttackClip=Load(TEXT("Attack")); WakeClip=nullptr;
    }
    DieClip=Load(TEXT("Die"));
    bRigReady=MoveClip && AttackClip && DieClip && (!Profile.bDormant || (WakeClip && AwakeMoveClip));
    Pose.Apply(Visual);
}

void ABreachSeabornEnemy::OnRepSpecies()
{
    if(!bMechanicsEnabled)
    {
        Visual->SetVisibility(false); GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        DamageHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision); GetCharacterMovement()->DisableMovement(); return;
    }
    SetProfile();
    LoadPresentation();
    OnRepAction();
}

void ABreachSeabornEnemy::OnRepAction()
{
    GetCapsuleComponent()->SetCollisionEnabled(bMechanicsEnabled && !IsDefeated()?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    DamageHitbox->SetCollisionEnabled(bMechanicsEnabled && !IsDefeated()?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,Profile.bDormant && !bAwake?ECR_Ignore:ECR_Block);
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
        Profile.Speed=.75f; Profile.Interval=3; Profile.Range=2500; Profile.bRanged=true;
    }
    else if(Species==EBreachSeabornSpecies::SeaDrifter)
    {
        Profile.Key=TEXT("SeaDrifter"); Profile.Attack=220; Profile.Defense=200;
        Profile.Speed=.75f; Profile.Interval=3; Profile.Range=2500;
        Profile.NerveFraction=.2f; Profile.bRanged=true; Profile.bFlying=true;
    }
    else if(Species==EBreachSeabornSpecies::BowlSeaReaper)
    {
        Profile.Key=TEXT("BowlSeaReaper"); Profile.Health=20000; Profile.Attack=400; Profile.Defense=600;
        Profile.ArtsResistance=75; Profile.Speed=.3f; Profile.Interval=3; Profile.NerveFraction=.1f; Profile.bDormant=true;
    }
    else if(Species==EBreachSeabornSpecies::FirstSeaPiercer)
    {
        Profile.Key=TEXT("FirstSeaPiercer"); Profile.Health=9000; Profile.Attack=550; Profile.Defense=240;
        Profile.Speed=.75f; Profile.Interval=3.5f; Profile.Range=2000;
        Profile.bRanged=true; Profile.bLowestHealthTarget=true;
    }
    // Ranged actors need to see targets throughout their extended attack area.
    if(Profile.bRanged) Awareness->SightRadius=FMath::Max(Awareness->SightRadius,Profile.Range);
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
    if(!IsValid(Player) || Player->Health<=0) return false;
    FVector Separation=Player->GetActorLocation()-GetActorLocation();
    if(!Profile.bFlying) Separation.Z=FMath::Max(0.f,FMath::Abs(Separation.Z)-GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    if(Separation.Size()>Profile.Range) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SeabornAttack),false,this);
    const FVector Start=GetActorLocation();
    return !GetWorld()->LineTraceSingleByChannel(Hit,Start,Player->GetActorLocation(),ECC_Visibility,Params) || Hit.GetActor()==Player;
}

ABreachCharacter* ABreachSeabornEnemy::SelectTarget() const
{
    ABreachCharacter* Best=nullptr;
    ABreachCharacter* InRange=nullptr;
    float BestDistance=TNumericLimits<float>::Max();
    for(TActorIterator<ABreachCharacter> It(GetWorld());It;++It)
    {
        auto* Player=*It;
        if(!Awareness->CanSee(Player)) continue;
        const float Distance=FVector::DistSquared(GetActorLocation(),Player->GetActorLocation());
        if(Distance<BestDistance) { Best=Player; BestDistance=Distance; }
        if(Profile.bLowestHealthTarget && CanHit(Player))
        {
            const float Ratio=Player->GetHealthFraction();
            const float Current=InRange?InRange->GetHealthFraction():2.f;
            if(Ratio<Current || (Ratio==Current && Player->GetTargetSpawnOrder()<InRange->GetTargetSpawnOrder())) InRange=Player;
        }
    }
    return InRange?InRange:Best;
}

void ABreachSeabornEnemy::ApplyIncapacitation(float Seconds)
{
    if(!HasAuthority() || IsDefeated() || !FMath::IsFinite(Seconds) || Seconds<=0) return;
    Incapacitated=FMath::Max(Incapacitated,Seconds);
    Awareness->Stop();
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
    if(UGameplayStatics::IsGamePaused(this)) return;
    if(bWaveEnemy)
        if(const auto* Mode=GetWorld()->GetAuthGameMode<ABreachGameMode>(); Mode && (Mode->bGameOver || Mode->bGallery)) { Awareness->Stop(); return; }
    Awareness->Advance(Dt);
    AttackCooldown=FMath::Max(0.f,AttackCooldown-Dt);
    Disarmed=FMath::Max(0.f,Disarmed-Dt);
    if(bAwake && Profile.bDormant)
    {
        const float Drain=MaxHealth*.04f;
        const float AliveTime=FMath::Min(Dt,Health/Drain);
        ApplyNerveAura(AliveTime);
        Health=FMath::Max(0.f,Health-Drain*AliveTime);
        if(Health<=0) { Die(); return; }
    }
    if(Incapacitated>0)
    {
        ActionStarted+=FMath::Min(Dt,Incapacitated);
        Incapacitated=FMath::Max(0.f,Incapacitated-Dt); return;
    }
    if(Profile.bDormant && !bAwake)
    {
        TryWake();
        if(Action==EBreachSeabornAction::Wake)
        {
            ActionTime+=Dt;
            if(ActionTime>=GetWakeDuration())
            {
                bAwake=true;
                Awareness->SetCombatSpeed(UBreachMovementComponent::UnarmedSpeed*Profile.Speed/1.9f*6.f);
                GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
                SetAction(EBreachSeabornAction::Idle);
            }
            return;
        }
        DormantTime+=Dt;
        if(DormantTime<30)
        {
            Awareness->Stop(); SetAction(EBreachSeabornAction::Idle); return;
        }
        SetAction(Awareness->MoveTowardDestination(Dt)?EBreachSeabornAction::Move:EBreachSeabornAction::Idle);
        return;
    }
    if(Action==EBreachSeabornAction::Attack)
    {
        ActionTime+=Dt;
        const float Duration=GetAttackDuration();
        if(!bHitApplied && ActionTime>=Duration*.42f) { bHitApplied=true; ResolveAttack(); }
        if(ActionTime>=Duration) { AttackTarget.Reset(); SetAction(EBreachSeabornAction::Idle); }
        return;
    }
    auto* Player=Awareness->GetVisibleTarget();
    if(Player && CanHit(Player))
    {
        Awareness->Stop();
        FVector Direction=Player->GetActorLocation()-GetActorLocation(); Direction.Z=0;
        if(!Direction.IsNearlyZero()) SetActorRotation(Direction.Rotation());
        SetAction(EBreachSeabornAction::Idle);
        if(AttackCooldown<=0 && Disarmed<=0)
        {
            AttackTarget=Player; bHitApplied=false; AttackCooldown=Profile.Interval;
            SetAction(EBreachSeabornAction::Attack);
        }
    }
    else
    {
        SetAction(Awareness->MoveTowardDestination(Dt,0,Profile.bFlying,FlightAnchorHeight)?EBreachSeabornAction::Move:EBreachSeabornAction::Idle);
    }
}

float ABreachSeabornEnemy::GetAttackDuration() const { return AttackClip?AttackClip->GetPlayLength():.8f; }
float ABreachSeabornEnemy::GetWakeDuration() const { return WakeClip?WakeClip->GetPlayLength():1.f; }

void ABreachSeabornEnemy::TryWake()
{
    if(!Profile.bDormant || bAwake || IsDefeated() || Action==EBreachSeabornAction::Wake || Incapacitated>0 || Health>=MaxHealth*.9999f) return;
    Awareness->Stop();
    SetAction(EBreachSeabornAction::Wake);
}

void ABreachSeabornEnemy::ApplyNerveAura(float Dt)
{
    const float Radius=2.5f*FBreachSeabornProfile::TileSize;
    for(TActorIterator<ABreachCharacter> It(GetWorld());It;++It)
        if(It->Health>0 && FVector::DistSquared(GetActorLocation(),It->GetActorLocation())<=FMath::Square(Radius))
            It->NerveDamage->ApplyNerveDamage(Profile.Attack*.2f*Dt,this);
}

void ABreachSeabornEnemy::ResolveAttack()
{
    auto* Player=AttackTarget.Get();
    if(!CanHit(Player) || !Awareness->CanSee(Player) || IsDefeated() || Incapacitated>0 || Disarmed>0) return;
    if(Profile.bRanged)
    {
        const FVector Direction=(Player->GetActorLocation()-GetActorLocation()).GetSafeNormal();
        const FTransform SpawnTransform(Direction.Rotation(),GetActorLocation());
        auto* Projectile=GetWorld()->SpawnActorDeferred<ABreachSeabornProjectile>(ABreachSeabornProjectile::StaticClass(),SpawnTransform,this,this,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if(Projectile)
        {
            Projectile->Launch(Direction,Profile.Attack*FBreachSeabornProfile::CombatScale,Profile.Attack*Profile.NerveFraction,
                Profile.ProjectileSpeed,(Profile.Range+100.f)/Profile.ProjectileSpeed,bWaveEnemy);
            UGameplayStatics::FinishSpawningActor(Projectile,SpawnTransform);
        }
        return;
    }
    UGameplayStatics::ApplyDamage(Player,Profile.Attack*FBreachSeabornProfile::CombatScale,GetController(),this,UDamageType::StaticClass());
    Player->NerveDamage->ApplyNerveDamage(Profile.Attack*Profile.NerveFraction,this);
}

float ABreachSeabornEnemy::TakeDamage(float Damage,const FDamageEvent& Event,AController* DamageInstigator,AActor* Causer)
{
    if(!HasAuthority() || !bMechanicsEnabled || IsDefeated() || !FMath::IsFinite(Damage) || Damage<=0) return 0;
    const auto* Type=Event.DamageTypeClass?Event.DamageTypeClass->GetDefaultObject<UDamageType>():nullptr;
    float Applied=Damage;
    if(Type && Type->IsA<UBreachArtsDamage>()) Applied*=1.f-Profile.ArtsResistance/100.f;
    else if(!Type || (!Type->IsA<UBreachTrueDamage>() && !Type->IsA<UBreachArmorIgnoringDamage>()))
    {
        const float DefenseMultiplier=Type && Type->IsA<UBreachArmorPiercingDamage>()?1.f-Breach::AA12ArmorPenetration:1.f;
        const float Defense=Profile.Defense*FBreachSeabornProfile::CombatScale*DefenseMultiplier;
        Applied=FMath::Max(Damage*.1f,Damage-Defense);
    }
    Applied=FMath::Min(Applied,Health);
    Health-=Applied;
    if(Health<=0) Die(Event.IsOfType(FPointDamageEvent::ClassID) && Damage>50);
    else
    {
        if(Applied>0) Awareness->NotifyDamage(DamageInstigator,Causer);
        TryWake();
    }
    ForceNetUpdate();
    return Applied;
}

void ABreachSeabornEnemy::Die(bool bHeadshot)
{
    Health=0; AttackTarget.Reset(); bHitApplied=true;
    Awareness->Die();
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DamageHitbox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetAction(EBreachSeabornAction::Dead);
    if(bWaveEnemy)
    {
        if(auto* Mode=GetWorld()->GetAuthGameMode<ABreachGameMode>()) Mode->EnemyDefeated(this,bHeadshot);
        SetLifeSpan(9.f);
    }
}

void ABreachSeabornEnemy::UpdatePresentation(float Dt)
{
    if(!bRigReady) return;
    UAnimSequence* Clip=IdleClip;
    float Time=MoveTime; bool Loop=true;
    if(Action==EBreachSeabornAction::Attack) { Clip=AttackClip; Time=ActionTime; Loop=false; }
    else if(Action==EBreachSeabornAction::Dead) { Clip=DieClip; Time=ActionTime; Loop=false; }
    else if(Action==EBreachSeabornAction::Wake) { Clip=WakeClip; Time=ActionTime; Loop=false; }
    else if(Action==EBreachSeabornAction::Move) Clip=bAwake && AwakeMoveClip?AwakeMoveClip:MoveClip;
    else if(bAwake && WakeClip) { Clip=WakeClip; Time=WakeClip->GetPlayLength(); Loop=false; }
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
    DOREPLIFETIME(ABreachSeabornEnemy,bWaveEnemy);
    DOREPLIFETIME(ABreachSeabornEnemy,Health);
    DOREPLIFETIME(ABreachSeabornEnemy,MaxHealth);
    DOREPLIFETIME(ABreachSeabornEnemy,Action);
    DOREPLIFETIME(ABreachSeabornEnemy,bAwake);
    DOREPLIFETIME(ABreachSeabornEnemy,ActionStarted);
    DOREPLIFETIME(ABreachSeabornEnemy,Incapacitated);
}
