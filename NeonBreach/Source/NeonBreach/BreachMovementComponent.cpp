#include "BreachMovementComponent.h"
#include "BreachGame.h"
#include "Net/UnrealNetwork.h"

UBreachMovementComponent::UBreachMovementComponent()
{
    SetIsReplicatedByDefault(true);
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
    const bool Pressed=bWantsToCrouch && !bPreviousCrouchRequest;
    bPreviousCrouchRequest=bWantsToCrouch;
    if(bSliding && (!Alive || !bWantsToCrouch || !IsCrouching() || !IsMovingOnGround())) bSliding=false;
    if(Pressed && Alive && SlideCooldown<=0 && IsMovingOnGround() && IsCrouching() && Velocity.Size2D()>=SlideEntrySpeed)
    {
        bSliding=true;
        SlideElapsed=0;
        SlideCooldown=SlideCooldownDuration;
        Velocity+=Velocity.GetSafeNormal2D()*SlideEntryBoost;
    }
}

void UBreachMovementComponent::CalcVelocity(float Dt,float Friction,bool bFluid,float Braking)
{
    if(bSliding && IsMovingOnGround() && HasValidData() && CharacterOwner->GetLocalRole()!=ROLE_SimulatedProxy)
    {
        // Preserve entry momentum, with no acceleration from held WASD.
        // PhysWalking still owns floor following, sweeps and wall collision.
        ApplyVelocityBraking(Dt,0.f,SlideDeceleration);
        return;
    }
    Super::CalcVelocity(Dt,Friction,bFluid,Braking);
}

void UBreachMovementComponent::UpdateCharacterStateAfterMovement(float Dt)
{
    Super::UpdateCharacterStateAfterMovement(Dt);
    if(!CharacterOwner || CharacterOwner->GetLocalRole()==ROLE_SimulatedProxy || !bSliding) return;
    SlideElapsed+=Dt;
    if(!IsMovingOnGround() || !IsCrouching() || Velocity.Size2D()<SlideExitSpeed || SlideElapsed>=SlideMaxDuration)
        bSliding=false;
}

// Preserve slide transitions and elapsed time when client movement is replayed.
// Do not combine across the crouch edge or through a timed slide.
class FSavedMove_Breach : public FSavedMove_Character
{
    using Super=FSavedMove_Character;
    bool bSavedSliding=false,bSavedPreviousCrouch=false;
    float SavedSlideElapsed=0,SavedSlideCooldown=0;
public:
    virtual void Clear() override
    {
        Super::Clear();
        bSavedSliding=false;bSavedPreviousCrouch=false;SavedSlideElapsed=0;SavedSlideCooldown=0;
    }
    virtual void SetMoveFor(ACharacter* C,float Dt,const FVector& NewAccel,FNetworkPredictionData_Client_Character& Data) override
    {
        Super::SetMoveFor(C,Dt,NewAccel,Data);
        const auto* Move=CastChecked<UBreachMovementComponent>(C->GetCharacterMovement());
        bSavedSliding=Move->bSliding;bSavedPreviousCrouch=Move->bPreviousCrouchRequest;SavedSlideElapsed=Move->SlideElapsed;
        SavedSlideCooldown=Move->SlideCooldown;
    }
    virtual bool CanCombineWith(const FSavedMovePtr& NewMove,ACharacter* C,float MaxDelta) const override
    {
        const auto* Other=static_cast<const FSavedMove_Breach*>(NewMove.Get());
        if(bSavedSliding || Other->bSavedSliding || SavedSlideCooldown>0 || Other->SavedSlideCooldown>0 || bSavedPreviousCrouch!=Other->bSavedPreviousCrouch) return false;
        return Super::CanCombineWith(NewMove,C,MaxDelta);
    }
    virtual void PrepMoveFor(ACharacter* C) override
    {
        Super::PrepMoveFor(C);
        auto* Move=CastChecked<UBreachMovementComponent>(C->GetCharacterMovement());
        Move->bSliding=bSavedSliding;Move->bPreviousCrouchRequest=bSavedPreviousCrouch;Move->SlideElapsed=SavedSlideElapsed;
        Move->SlideCooldown=SavedSlideCooldown;
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
