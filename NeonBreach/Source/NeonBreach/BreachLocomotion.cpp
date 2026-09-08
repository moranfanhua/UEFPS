#include "BreachGame.h"
#include "BreachMovementComponent.h"
#include "BreachVisuals.h"
#include "Animation/AnimSequence.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

void ABreachCharacter::HolsterRifle() { if(!bUnarmed) SetUnarmed(true); }
void ABreachCharacter::DrawRifle() { if(bUnarmed) SetUnarmed(false); }
void ABreachCharacter::SetUnarmed(bool Enabled)
{
    if(Health<=0) return;
    bUnarmed=Enabled;
    bSprint=bUnarmed && !bIsCrouched;
    bTrigger=false; bAiming=false; bReloading=false;
    ReloadProgress=0; Recoil=0;
    GetWorldTimerManager().ClearTimer(ReloadTimer);
    MuzzleLight->SetIntensity(0);
    WeaponRoot->SetVisibility(!bUnarmed,true);
    WorldWeaponRoot->SetVisibility(!bUnarmed,true);
    TArray<USceneComponent*> Parts;
    WorldWeaponRoot->GetChildrenComponents(true,Parts);
    for(auto* Part:Parts)
        if(auto* WeaponPart=Cast<UStaticMeshComponent>(Part)) WeaponPart->SetCastShadow(!bUnarmed);
    // Blend out of the current held pose when holstering or drawing the rifle.
    LocomotionBlendFrom=BodyPose.Local; LocomotionBlendTime=0;
}
void ABreachCharacter::CrouchOn() { if(Health>0) Crouch(); }
void ABreachCharacter::CrouchOff() { UnCrouch(); }
bool ABreachCharacter::IsSliding() const { return CastChecked<UBreachMovementComponent>(GetCharacterMovement())->IsSliding(); }

void ABreachCharacter::LoadLocomotionAnimations()
{
    static const TCHAR* Clips[]={TEXT("Idle_Loop"),TEXT("Jog_Fwd_Loop"),TEXT("Sprint_Loop"),TEXT("Female_Jump_Start"),TEXT("Female_Jump_Air"),TEXT("Female_Jump_Land"),TEXT("Crouch_Idle_Loop"),TEXT("Crouch_Fwd_Loop"),TEXT("Crouch_Idle_Loop")};
    LocomotionAnimations.Reset();
    for(const TCHAR* Clip:Clips)
        LocomotionAnimations.Add(LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Animations/Locomotion/%s/A_%s_%s.A_%s_%s"),Breach::Keys[OperatorIndex],Breach::Keys[OperatorIndex],Clip,Breach::Keys[OperatorIndex],Clip)));
    LocomotionState=EBreachLocomotion::Idle;
    LocomotionTime=0; LocomotionBlendTime=0;
    LocomotionBlendFrom=BodyPose.Reference;
}
bool ABreachCharacter::HasLocomotionAnimations() const
{
    if(LocomotionAnimations.Num()!=int32(EBreachLocomotion::Count)) return false;
    for(const auto& Animation:LocomotionAnimations) if(!Animation) return false;
    return true;
}
void ABreachCharacter::UpdateLocomotion(float Dt)
{
    const bool Falling=GetCharacterMovement()->IsFalling();
    const float Speed=GetVelocity().Size2D();
    if(Falling && !bWasFalling) { AirTime=0; bJumpTakingOff=GetVelocity().Z>0; }
    if(!Falling && bWasFalling) LandTime=0;
    bWasFalling=Falling;
    if(Falling) AirTime+=Dt; else LandTime+=Dt;
    EBreachLocomotion Next=EBreachLocomotion::Idle;
    if(Falling) Next=bJumpTakingOff && AirTime<.28f?EBreachLocomotion::JumpStart:EBreachLocomotion::JumpLoop;
    else if(IsSliding()) Next=EBreachLocomotion::Slide;
    else if(bIsCrouched) Next=Speed>10?EBreachLocomotion::CrouchWalk:EBreachLocomotion::CrouchIdle;
    else if(LandTime<.32f) Next=EBreachLocomotion::JumpLand;
    else if(Speed>10) Next=bUnarmed?EBreachLocomotion::Sprint:EBreachLocomotion::Jog;
    if(Next!=LocomotionState)
    {
        LocomotionBlendFrom=BodyPose.Local; LocomotionBlendTime=0;
        LocomotionState=Next; LocomotionTime=0;
    }
    auto* Animation=LocomotionAnimations.IsValidIndex(int32(Next))?LocomotionAnimations[int32(Next)].Get():nullptr;
    float Rate=1;
    if(Next==EBreachLocomotion::Jog) Rate=FMath::Clamp(Speed/510.f,.55f,1.4f);
    if(Next==EBreachLocomotion::Sprint) Rate=FMath::Clamp(Speed/790.f,.55f,1.4f);
    if(Next==EBreachLocomotion::CrouchWalk) Rate=FMath::Clamp(Speed/200.f,.55f,1.4f);
    LocomotionTime+=Dt*Rate;
    float Time=LocomotionTime;
    // Reuse the low crouch pose without stepping while momentum carries the body.
    if(Next==EBreachLocomotion::Slide) Time=.25f;
    const bool Once=Next==EBreachLocomotion::JumpStart || Next==EBreachLocomotion::JumpLoop || Next==EBreachLocomotion::JumpLand;
    if(Animation && Next==EBreachLocomotion::JumpStart) Time=FMath::Clamp(AirTime/.28f,0.f,1.f)*Animation->GetPlayLength();
    if(Animation && Next==EBreachLocomotion::JumpLoop)
    {
        // Female_Jump moves from tucked knees at the apex to extended legs
        // before contact. Follow the physical arc instead of repeating that
        // motion while falling. Dropping from a ledge skips the takeoff tuck.
        const float JumpSpeed=FMath::Max(GetCharacterMovement()->JumpZVelocity,1.f);
        const float StartSpeed=FMath::Max(0.f,JumpSpeed+GetCharacterMovement()->GetGravityZ()*.28f);
        float Progress=FMath::Clamp((StartSpeed-GetVelocity().Z)/(StartSpeed+JumpSpeed),0.f,1.f);
        if(!bJumpTakingOff) Progress=FMath::Max(Progress,.65f);
        Time=Progress*Animation->GetPlayLength();
    }
    if(Animation && Next==EBreachLocomotion::JumpLand) Time=FMath::Clamp(LandTime/.32f,0.f,1.f)*Animation->GetPlayLength();
    if(!BodyPose.Sample(Animation,Time,!Once)) BodyPose.Walk(Bob,Speed);
    LocomotionBlendTime+=Dt;
    const float Blend=FMath::Clamp(LocomotionBlendTime/.12f,0.f,1.f);
    if(Blend<1 && LocomotionBlendFrom.Num()==BodyPose.Local.Num())
    {
        for(int32 I=0;I<BodyPose.Local.Num();++I)
        {
            FTransform Mixed; Mixed.Blend(LocomotionBlendFrom[I],BodyPose.Local[I],Blend); BodyPose.Local[I]=Mixed;
        }
        BodyPose.Rebuild();
    }
}
