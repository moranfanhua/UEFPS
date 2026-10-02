#include "BreachEnemyAwareness.h"
#include "BreachGame.h"
#include "BreachSeabornEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"

UBreachEnemyAwareness::UBreachEnemyAwareness()
{
    PrimaryComponentTick.bCanEverTick=false;
}

bool UBreachEnemyAwareness::IsActive() const
{
    if(!GetOwner() || !GetOwner()->HasAuthority() || GetOwner()->IsActorBeingDestroyed() || Intent==EBreachEnemyIntent::Dead) return false;
    if(const auto* Enemy=Cast<ABreachEnemy>(GetOwner())) return !Enemy->bDisplayOnly && !Enemy->bDefeated;
    if(const auto* Enemy=Cast<ABreachSeabornEnemy>(GetOwner())) return Enemy->bMechanicsEnabled && !Enemy->IsDefeated();
    return false;
}

void UBreachEnemyAwareness::Reset(float Speed)
{
    VisibleTarget.Reset(); Intent=EBreachEnemyIntent::Wander;
    Destination=GetOwner()->GetActorLocation();
    BroadcastCooldown=WanderTime=AvoidSide=0; bWanderPoint=false;
    Stop(); SetCombatSpeed(Speed);
}

void UBreachEnemyAwareness::SetCombatSpeed(float Speed)
{
    CombatSpeed=FMath::Max(0.f,Speed);
    if(auto* Character=Cast<ACharacter>(GetOwner()))
    {
        const float Limit=CombatSpeed*(Intent==EBreachEnemyIntent::Wander?FMath::Clamp(WanderSpeedFraction,0.f,1.f):1.f);
        Character->GetCharacterMovement()->MaxWalkSpeed=Limit;
        Character->GetCharacterMovement()->MaxFlySpeed=Limit;
    }
}

bool UBreachEnemyAwareness::CanSee(const ABreachCharacter* Player) const
{
    if(!IsValid(Player) || Player->Health<=0 || Player->IsActorBeingDestroyed()) return false;
    const FVector Start=GetOwner()->GetActorLocation();
    const FVector Offset=Player->GetActorLocation()-Start;
    if(Offset.SizeSquared()>FMath::Square(FMath::Max(0.f,SightRadius))) return false;
    const FVector Flat=Offset.GetSafeNormal2D();
    if(!Flat.IsNearlyZero() && FVector::DotProduct(GetOwner()->GetActorForwardVector().GetSafeNormal2D(),Flat)<FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(SightHalfAngle,0.f,180.f)))) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyAwarenessSight),false,GetOwner());
    // Crowds must not mask walls, or constantly interrupt one another's sight.
    for(TActorIterator<ACharacter> It(GetWorld());It;++It)
        if(It->FindComponentByClass<UBreachEnemyAwareness>()) Params.AddIgnoredActor(*It);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Start,Player->GetActorLocation(),ECC_Visibility,Params) || Hit.GetActor()==Player;
}

void UBreachEnemyAwareness::ReceiveSignal(FVector Position)
{
    if(!IsActive() || Position.ContainsNaN() || UGameplayStatics::IsGamePaused(this)) return;
    if(CanSee(VisibleTarget.Get())) return;
    VisibleTarget.Reset(); Destination=Position; Intent=EBreachEnemyIntent::Investigate;
    SetCombatSpeed(CombatSpeed);
}

void UBreachEnemyAwareness::NotifyDamage(AController* DamageInstigator,AActor* DamageCauser)
{
    if(!IsActive() || UGameplayStatics::IsGamePaused(this)) return;
    AActor* Source=IsValid(DamageInstigator)?DamageInstigator->GetPawn():nullptr;
    if(!IsValid(Source) && IsValid(DamageCauser)) Source=DamageCauser->GetInstigator();
    if(!IsValid(Source)) Source=DamageCauser;
    if(!IsValid(Source) || Source==GetOwner() || Source->IsActorBeingDestroyed()) return;
    // Snapshot the attacker, rather than following its actor or a projectile's impact position.
    // ReceiveSignal keeps direct sight ahead of unseen sources and does not broadcast.
    ReceiveSignal(Source->GetActorLocation());
}

void UBreachEnemyAwareness::Broadcast(FVector Position)
{
    const float RadiusSquared=FMath::Square(FMath::Max(0.f,SignalRadius));
    for(TActorIterator<ACharacter> It(GetWorld());It;++It)
    {
        if(*It==GetOwner() || FVector::DistSquared(GetOwner()->GetActorLocation(),It->GetActorLocation())>RadiusSquared) continue;
        if(auto* Receiver=It->FindComponentByClass<UBreachEnemyAwareness>()) Receiver->ReceiveSignal(Position);
    }
}

void UBreachEnemyAwareness::PickWanderPoint()
{
    // Recenter each new wander step on the owner's current position.
    const float Angle=FMath::FRandRange(-PI,PI);
    const float Maximum=FMath::Max(0.f,WanderRadius);
    const float Minimum=FMath::Clamp(WanderMinRadius,0.f,Maximum);
    const float Radius=FMath::Sqrt(FMath::Lerp(Minimum*Minimum,Maximum*Maximum,FMath::FRand()));
    Destination=GetOwner()->GetActorLocation()+FVector(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0);
    const float WanderSpeed=CombatSpeed*FMath::Clamp(WanderSpeedFraction,0.f,1.f);
    // Allow the full trip to distant points, with eight extra seconds for local steering.
    bWanderPoint=true; WanderTime=8.f+(WanderSpeed>UE_KINDA_SMALL_NUMBER?Radius/WanderSpeed:0.f); AvoidSide=0;
}

void UBreachEnemyAwareness::Advance(float Dt)
{
    if(!IsActive() || !FMath::IsFinite(Dt) || Dt<=0 || UGameplayStatics::IsGamePaused(this)) return;
    ABreachCharacter* Player=nullptr;
    if(const auto* Enemy=Cast<ABreachSeabornEnemy>(GetOwner())) Player=Enemy->SelectTarget();
    else
    {
        float Nearest=TNumericLimits<float>::Max();
        for(TActorIterator<ABreachCharacter> It(GetWorld());It;++It)
        {
            const float Distance=FVector::DistSquared(GetOwner()->GetActorLocation(),It->GetActorLocation());
            if(Distance<Nearest && CanSee(*It)) { Player=*It; Nearest=Distance; }
        }
    }
    BroadcastCooldown=FMath::Max(0.f,BroadcastCooldown-Dt);
    if(Player)
    {
        VisibleTarget=Player; Intent=EBreachEnemyIntent::Pursue; Destination=Player->GetActorLocation();
        if(BroadcastCooldown<=0) { Broadcast(Destination); BroadcastCooldown=FMath::Max(.05f,SignalInterval); }
    }
    else
    {
        VisibleTarget.Reset();
        if(Intent==EBreachEnemyIntent::Pursue) Intent=EBreachEnemyIntent::Investigate;
        const float Distance=FVector::Dist2D(GetOwner()->GetActorLocation(),Destination);
        if(Intent==EBreachEnemyIntent::Investigate && Distance<=FMath::Max(1.f,ArrivalRadius)) { Intent=EBreachEnemyIntent::Wait; Stop(); }
        if(Intent==EBreachEnemyIntent::Wander)
        {
            WanderTime-=Dt;
            if(bWanderPoint && (Distance<=FMath::Max(1.f,ArrivalRadius) || WanderTime<=0))
            {
                bWanderPoint=false; WanderTime=FMath::FRandRange(2.f,4.f); Stop();
            }
            if(!bWanderPoint && WanderTime<=0) PickWanderPoint();
        }
    }
    SetCombatSpeed(CombatSpeed);
}

void UBreachEnemyAwareness::Stop()
{
    if(auto* Character=Cast<ACharacter>(GetOwner()))
    {
        Character->ConsumeMovementInputVector();
        Character->GetCharacterMovement()->StopMovementImmediately();
    }
}

bool UBreachEnemyAwareness::MoveTowardDestination(float Dt,float StopDistance,bool bFlying,float FlightHeight)
{
    auto* Character=Cast<ACharacter>(GetOwner());
    if(!Character || !IsActive() || Intent==EBreachEnemyIntent::Wait || (Intent==EBreachEnemyIntent::Wander && !bWanderPoint)) { Stop(); return false; }
    FVector Direction=Destination-Character->GetActorLocation(); Direction.Z=0;
    const float Radius=Intent==EBreachEnemyIntent::Pursue?StopDistance:FMath::Max(1.f,ArrivalRadius);
    if(Direction.Size()<=Radius) { Stop(); return false; }
    Direction.Normalize();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemySteering),false,Character);
    const FCollisionObjectQueryParams StaticObjects(ECC_WorldStatic);
    const FVector Start=Character->GetActorLocation();
    const float ProbeDistance=FMath::Max(150.f,Character->GetCharacterMovement()->GetMaxSpeed()*.35f);
    const auto Blocked=[&](FVector Heading)
    {
        FHitResult Hit;
        return GetWorld()->SweepSingleByObjectType(Hit,Start,Start+Heading*ProbeDistance,FQuat::Identity,StaticObjects,
            FCollisionShape::MakeSphere(Character->GetCapsuleComponent()->GetScaledCapsuleRadius()),Params);
    };
    if(Blocked(Direction))
    {
        bool Found=false;
        for(float Angle:{60.f,90.f,120.f})
        {
            for(float Side:{AvoidSide<0?-1.f:1.f,AvoidSide<0?1.f:-1.f})
            {
                const FVector Candidate=FRotator(0,Angle*Side,0).RotateVector(Direction);
                if(!Blocked(Candidate)) { Direction=Candidate; AvoidSide=Side; Found=true; break; }
            }
            if(Found) break;
        }
        if(!Found) { Stop(); return false; }
    }
    else AvoidSide=0;
    Character->SetActorRotation(FMath::RInterpTo(Character->GetActorRotation(),Direction.Rotation(),Dt,5));
    if(bFlying)
    {
        FHitResult Ground;
        if(GetWorld()->LineTraceSingleByObjectType(Ground,Start+FVector(0,0,300),Start-FVector(0,0,2000),StaticObjects,Params))
            Direction.Z=FMath::Clamp(float((Ground.ImpactPoint.Z+FlightHeight-Start.Z)/150.f),-1.f,1.f);
    }
    Character->AddMovementInput(Direction.GetSafeNormal(),1,true);
    return true;
}

void UBreachEnemyAwareness::Die()
{
    Intent=EBreachEnemyIntent::Dead; VisibleTarget.Reset(); Stop();
}
