#include "BreachMovementComponent.h"
#include "BreachGame.h"
#include "Net/UnrealNetwork.h"

UBreachMovementComponent::UBreachMovementComponent()
{
    SetIsReplicatedByDefault(true);
}

void UBreachMovementComponent::SetLocomotionIntent(bool Unarmed,bool Aiming)
{
    bWantsUnarmed=Unarmed; bWantsAim=Aiming && !Unarmed;
}

float UBreachMovementComponent::GetTargetMoveSpeed() const
{
    if(IsCrouching()) return MaxWalkSpeedCrouched;
    if(const auto* Player=Cast<ABreachCharacter>(CharacterOwner); Player && Player->UsesSword()) return SwordSpeed;
    return bWantsAim?AimSpeed:(bWantsUnarmed?UnarmedSpeed:RifleSpeed);
}

float UBreachMovementComponent::GetMaxSpeed() const
{
    if(IsMovingOnGround()) return bSliding?FMath::Max(SmoothedMoveSpeed,float(Velocity.Size2D())):SmoothedMoveSpeed;
    // Drawing a weapon or holding crouch in the air must not erase the
    // horizontal momentum of a slide jump. Acceleration still uses UE air control.
    if(IsFalling()) return FMath::Max(SmoothedMoveSpeed,float(Velocity.Size2D()));
    return Super::GetMaxSpeed();
}

bool UBreachMovementComponent::CanAttemptJump() const
{
    return (bSliding && IsMovingOnGround() && IsJumpAllowed()) || Super::CanAttemptJump();
}

bool UBreachMovementComponent::DoJump(bool bReplayingMoves,float DeltaTime)
{
    if(bSliding && IsMovingOnGround())
    {
        // Let UE test the standing capsule first, so a slide jump cannot push
        // the head through a low ceiling. Keep crouch intent for landing slides.
        UnCrouch(false);
        if(IsCrouching()) return false;
    }
    return Super::DoJump(bReplayingMoves,DeltaTime);
}

void UBreachMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode,uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PreviousMovementMode,PreviousCustomMode);
    if(!CharacterOwner || CharacterOwner->GetLocalRole()==ROLE_SimulatedProxy) return;
    if(!IsMovingOnGround()) EndSlide();
    if(PreviousMovementMode==MOVE_Falling && IsMovingOnGround())
    {
        SmoothedMoveSpeed=FMath::Max(SmoothedMoveSpeed,float(Velocity.Size2D()));
        if(bWantsToCrouch) { Crouch(false); TryStartSlide(); }
    }
}

void UBreachMovementComponent::TryStartSlide()
{
    const auto* Player=Cast<ABreachCharacter>(CharacterOwner);
    if(bSliding || (Player && Player->Health<=0) || SlideCooldown>0 || !bWantsToCrouch || !IsMovingOnGround() || !IsCrouching() || Velocity.Size2D()<SlideEntrySpeed) return;
    bSliding=true; SlideElapsed=0;
    SlideCooldown=FMath::Max(0.f,SlideCooldownDuration);
    SlideBoostRemaining=0;
    if(SlideBoostCooldown<=0)
    {
        SlideBoostRemaining=FMath::Clamp(SlideMaxSpeed-float(Velocity.Size2D()),0.f,FMath::Max(0.f,SlideEntryBoost));
        SlideBoostCooldown=FMath::Max(0.f,SlideBoostRecoveryTime);
    }
}

void UBreachMovementComponent::EndSlide()
{
    if(!bSliding) return;
    bSliding=false; SlideBoostRemaining=0;
    // Resume from actual momentum, then ease toward the new stance's speed.
    SmoothedMoveSpeed=float(Velocity.Size2D());
}

void UBreachMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);
    SetLocomotionIntent((Flags&FSavedMove_Character::FLAG_Custom_0)!=0,(Flags&FSavedMove_Character::FLAG_Custom_1)!=0);
}

void UBreachMovementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UBreachMovementComponent,bSliding,COND_SimulatedOnly);
}

void UBreachMovementComponent::UpdateCharacterStateBeforeMovement(float Dt)
{
    Super::UpdateCharacterStateBeforeMovement(Dt);
    if(!CharacterOwner || CharacterOwner->GetLocalRole()==ROLE_SimulatedProxy) return;
    const auto* Player=Cast<ABreachCharacter>(CharacterOwner);
    const bool Alive=!Player || Player->Health>0;
    SlideCooldown=FMath::Max(0.f,SlideCooldown-Dt);
    SlideBoostCooldown=FMath::Max(0.f,SlideBoostCooldown-Dt);
    const bool Pressed=bWantsToCrouch && !bPreviousCrouchRequest;
    bPreviousCrouchRequest=bWantsToCrouch;
    if(bSliding && (!Alive || !bWantsToCrouch || !IsCrouching() || !IsMovingOnGround())) EndSlide();
    if(Pressed && Alive) TryStartSlide();
    const float Target=GetTargetMoveSpeed();
    if(bSliding) SmoothedMoveSpeed=float(Velocity.Size2D());
    else SmoothedMoveSpeed=FMath::FInterpConstantTo(SmoothedMoveSpeed,Target,Dt,FMath::Max(1.f,Target>SmoothedMoveSpeed?SpeedRiseRate:SpeedFallRate));
}

void UBreachMovementComponent::CalcVelocity(float Dt,float Friction,bool bFluid,float Braking)
{
    if(bSliding && IsMovingOnGround() && HasValidData() && CharacterOwner->GetLocalRole()!=ROLE_SimulatedProxy)
    {
        const float Speed=float(Velocity.Size2D());
        FVector Direction=Velocity.GetSafeNormal2D();
        if(!Acceleration.IsNearlyZero() && Speed>UE_KINDA_SMALL_NUMBER)
        {
            // Steer the momentum gradually; holding W never adds free speed.
            const float Turn=FMath::Clamp(FMath::FindDeltaAngleDegrees(Direction.Rotation().Yaw,Acceleration.Rotation().Yaw),-SlideTurnRate*Dt,SlideTurnRate*Dt);
            Direction=Direction.RotateAngleAxis(Turn,FVector::UpVector);
        }
        const float Boost=FMath::Min(SlideBoostRemaining,FMath::Max(0.f,SlideEntryBoost)*Dt/FMath::Max(.01f,SlideBoostDuration));
        SlideBoostRemaining-=Boost;
        const FVector FloorNormal=CurrentFloor.IsWalkableFloor()?CurrentFloor.HitResult.ImpactNormal:FVector::UpVector;
        const FVector SlopeGravity=FVector::VectorPlaneProject(FVector(0,0,GetGravityZ()),FloorNormal)*SlideGravityScale;
        // The gravity component along travel speeds up downhill slides and
        // slows uphill ones. UE PhysWalking retains floor sweeps / wall collision.
        const float SlopeAcceleration=FVector::DotProduct(SlopeGravity,Direction);
        const float NextSpeed=FMath::Max(0.f,Speed+Boost+(SlopeAcceleration-FMath::Max(0.f,SlideDeceleration))*Dt);
        Velocity=Direction*FMath::Min(NextSpeed,FMath::Max(Speed,SlideMaxSpeed));
        return;
    }
    Super::CalcVelocity(Dt,Friction,bFluid,Braking);
}

void UBreachMovementComponent::UpdateCharacterStateAfterMovement(float Dt)
{
    Super::UpdateCharacterStateAfterMovement(Dt);
    if(!CharacterOwner || CharacterOwner->GetLocalRole()==ROLE_SimulatedProxy || !bSliding) return;
    SlideElapsed+=Dt;
    if(!IsMovingOnGround() || !IsCrouching() || Velocity.Size2D()<SlideExitSpeed) EndSlide();
}

// Preserve slide transitions and elapsed time when client movement is replayed.
// Do not combine through stance transitions, sliding or boost recovery.
class FSavedMove_Breach : public FSavedMove_Character
{
    using Super=FSavedMove_Character;
    bool bSavedSliding=false,bSavedPreviousCrouch=false,bSavedUnarmed=false,bSavedAim=false;
    float SavedSlideElapsed=0,SavedSlideCooldown=0,SavedBoostCooldown=0,SavedBoostRemaining=0,SavedMoveSpeed=0;
public:
    virtual void Clear() override
    {
        Super::Clear();
        bSavedSliding=false;bSavedPreviousCrouch=false;SavedSlideElapsed=0;SavedSlideCooldown=0;
        bSavedUnarmed=false;bSavedAim=false;SavedBoostCooldown=0;SavedBoostRemaining=0;SavedMoveSpeed=0;
    }
    virtual uint8 GetCompressedFlags() const override
    {
        return Super::GetCompressedFlags()|(bSavedUnarmed?FLAG_Custom_0:0)|(bSavedAim?FLAG_Custom_1:0);
    }
    virtual void SetMoveFor(ACharacter* C,float Dt,const FVector& NewAccel,FNetworkPredictionData_Client_Character& Data) override
    {
        Super::SetMoveFor(C,Dt,NewAccel,Data);
        const auto* Move=CastChecked<UBreachMovementComponent>(C->GetCharacterMovement());
        bSavedSliding=Move->bSliding;bSavedPreviousCrouch=Move->bPreviousCrouchRequest;SavedSlideElapsed=Move->SlideElapsed;
        SavedSlideCooldown=Move->SlideCooldown;
        SavedBoostCooldown=Move->SlideBoostCooldown;SavedBoostRemaining=Move->SlideBoostRemaining;
        SavedMoveSpeed=Move->SmoothedMoveSpeed;bSavedUnarmed=Move->bWantsUnarmed;bSavedAim=Move->bWantsAim;
    }
    virtual bool CanCombineWith(const FSavedMovePtr& NewMove,ACharacter* C,float MaxDelta) const override
    {
        const auto* Other=static_cast<const FSavedMove_Breach*>(NewMove.Get());
        const auto* Move=CastChecked<UBreachMovementComponent>(C->GetCharacterMovement());
        if(bSavedSliding || Other->bSavedSliding || SavedSlideCooldown>0 || Other->SavedSlideCooldown>0 || SavedBoostCooldown>0 || Other->SavedBoostCooldown>0 ||
            bSavedPreviousCrouch!=Other->bSavedPreviousCrouch || bSavedUnarmed!=Other->bSavedUnarmed || bSavedAim!=Other->bSavedAim ||
            !FMath::IsNearlyEqual(SavedMoveSpeed,Other->SavedMoveSpeed,.01f) || !FMath::IsNearlyEqual(SavedMoveSpeed,Move->GetTargetMoveSpeed(),.01f)) return false;
        return Super::CanCombineWith(NewMove,C,MaxDelta);
    }
    virtual void PrepMoveFor(ACharacter* C) override
    {
        Super::PrepMoveFor(C);
        auto* Move=CastChecked<UBreachMovementComponent>(C->GetCharacterMovement());
        Move->bSliding=bSavedSliding;Move->bPreviousCrouchRequest=bSavedPreviousCrouch;Move->SlideElapsed=SavedSlideElapsed;
        Move->SlideCooldown=SavedSlideCooldown;
        Move->SlideBoostCooldown=SavedBoostCooldown;Move->SlideBoostRemaining=SavedBoostRemaining;Move->SmoothedMoveSpeed=SavedMoveSpeed;
        Move->SetLocomotionIntent(bSavedUnarmed,bSavedAim);
    }
};

class FNetworkPredictionData_Client_Breach : public FNetworkPredictionData_Client_Character
{
public:
    explicit FNetworkPredictionData_Client_Breach(const UCharacterMovementComponent& Move) : FNetworkPredictionData_Client_Character(Move) {}
    virtual FSavedMovePtr AllocateNewMove() override { return FSavedMovePtr(new FSavedMove_Breach()); }
};

FNetworkPredictionData_Client* UBreachMovementComponent::GetPredictionData_Client() const
{
    if(!ClientPredictionData)
        const_cast<UBreachMovementComponent*>(this)->ClientPredictionData=new FNetworkPredictionData_Client_Breach(*this);
    return ClientPredictionData;
}
