#include "BreachGame.h"
#include "BreachMovementComponent.h"
#include "BreachNerveDamageComponent.h"
#include "BreachVisuals.h"
#include "Animation/AnimSequence.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    using Clip=ECombatGirlsClip;
    bool IsShot(Clip Value) { return Value>=Clip::Shoot && Value<=Clip::Crouch_AutoShoot; }
    bool IsAuto(Clip Value) { return Value==Clip::AimIdle_AutoShoot || Value==Clip::Crouch_AutoShoot; }
    bool IsTurn(Clip Value)
    {
        return Value==Clip::AimTurn_L90 || Value==Clip::AimTurn_R90 || Value==Clip::Crouch_AimTurn_L90 || Value==Clip::Crouch_AimTurn_R90;
    }
    Clip DirectionalWalk(const FVector& Local,bool Crouched)
    {
        const int32 Direction=FMath::Abs(Local.Y)<.35f?(Local.X>=0?0:3):
            (Local.X>=-.01f?(Local.Y<0?1:2):(Local.Y<0?4:5));
        return Clip(int32(Crouched?Clip::Crouch_AimWalk_F:Clip::AimWalk_F)+Direction);
    }
}

void ABreachCharacter::LoadRifleClips()
{
    ClearRifleAction();
    RiflePose=Clip::None;RifleCycle=0;TurnAnchor=GetControlRotation().Yaw;
    RifleClips.Reset();
    if(UsesSword()) return;
    const TCHAR* Key=Breach::Keys[OperatorIndex];
    RifleClips.SetNum(int32(Clip::None));
    for(int32 Index=0;Index<RifleClips.Num();++Index)
    {
        const FString Name=StaticEnum<Clip>()->GetNameStringByValue(Index);
        const FString Base=FString::Printf(TEXT("A_%s_R_%s"),Key,*Name);
        RifleClips[Index]=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Animations/CombatGirls/%s/%s.%s"),Key,*Base,*Base));
    }
}

bool ABreachCharacter::HasCombatGirlsAnimations() const
{
    if(RifleClips.Num()!=int32(Clip::None) || !Body->GetSkinnedAsset()) return false;
    for(const auto& Sequence:RifleClips)
        if(!Sequence || Sequence->GetSkeleton()!=Body->GetSkinnedAsset()->GetSkeleton() || Sequence->GetPlayLength()<=0) return false;
    return true;
}

void ABreachCharacter::ClearRifleAction()
{
    RifleAction=Clip::None;ActionElapsed=0;TriggerShots=0;
}

void ABreachCharacter::StartRifleAction(Clip Value)
{
    if(!RifleClips.IsValidIndex(int32(Value)) || !RifleClips[int32(Value)]) return;
    if(Value==RifleAction && IsAuto(Value)) return;
    RifleAction=Value;ActionElapsed=0;
}

void ABreachCharacter::SlowWalkOn(){ CastChecked<UBreachMovementComponent>(GetCharacterMovement())->bSlowWalk=true; }
void ABreachCharacter::SlowWalkOff(){ CastChecked<UBreachMovementComponent>(GetCharacterMovement())->bSlowWalk=false; }

void ABreachCharacter::Evade()
{
    if(!HasAuthority()){ServerEvade();return;}
    const float Now=GetWorld()->GetTimeSeconds();
    auto* Move=GetCharacterMovement();
    if(UsesSword() || bUnarmed || Health<=0 || bReloading || bIsCrouched || IsSliding() || !Move->IsMovingOnGround() ||
        UGameplayStatics::IsGamePaused(this) || Now<NextEvade || !RifleClips.IsValidIndex(int32(Clip::Evade)) || !RifleClips[int32(Clip::Evade)]) return;
    NextEvade=Now+1.5f;StartRifleAction(Clip::Evade);
    auto Motion=MakeShared<FRootMotionSource_ConstantForce>();
    Motion->InstanceName=TEXT("CombatGirlsEvade");Motion->AccumulateMode=ERootMotionAccumulateMode::Override;
    Motion->Priority=500;Motion->Duration=.28f;Motion->Force=-GetActorRightVector()*360.f;
    Motion->FinishVelocityParams.Mode=ERootMotionFinishVelocityMode::SetVelocity;
    Motion->FinishVelocityParams.SetVelocity=FVector::ZeroVector;
    Move->ApplyRootMotionSource(Motion);
}

void ABreachCharacter::ServerEvade_Implementation(){Evade();}

bool ABreachCharacter::SampleRiflePose(EBreachLocomotion State,float Speed,float Dt)
{
    const bool Ground=State==EBreachLocomotion::Idle || State==EBreachLocomotion::Jog || State==EBreachLocomotion::CrouchIdle || State==EBreachLocomotion::CrouchWalk;
    const bool Death=Health<=0 && (RifleAction==Clip::Die_B || RifleAction==Clip::Die_F);
    const bool PutAway=RifleAction==Clip::PutGun && !IsPunchAttacking();
    const float Yaw=GetControlRotation().Yaw;
    if(UsesSword() || (!Death && ((!Ground && !PutAway) || (bUnarmed && !PutAway))) || RifleClips.IsEmpty())
    {
        RiflePose=Clip::None;TurnAnchor=Yaw;
        if(!Death) RifleAction=Clip::None;
        return false;
    }
    const FVector Local=GetActorTransform().InverseTransformVectorNoScale(GetVelocity()).GetSafeNormal2D();
    Clip Base=bIsCrouched?(bAiming?Clip::Crouch_AimIdle:Clip::Crouch_Idle):(bAiming?Clip::AimIdle:Clip::Idle);
    float StrideSpeed=0;
    if(Speed>10)
    {
        if(bAiming && !bIsCrouched && Local.X>.9f && Speed>225){ Base=Clip::AimJog;StrideSpeed=295; }
        else if(bAiming){Base=DirectionalWalk(Local,bIsCrouched);StrideSpeed=bIsCrouched?160:155;}
        else if(bIsCrouched){Base=Speed>160?Clip::Crouch_Jog:Clip::Crouch_Walk;StrideSpeed=Base==Clip::Crouch_Jog?299:163;}
        else {Base=Speed>260?Clip::Run:Clip::Walk;StrideSpeed=Base==Clip::Run?513:142;}
    }
    if(IsTurn(RifleAction) && (!bAiming || Speed>10)) RifleAction=Clip::None;
    if(!bAiming || Speed>10 || bUnarmed) TurnAnchor=Yaw;
    else if((RifleAction==Clip::None || IsTurn(RifleAction)) && FMath::Abs(FMath::FindDeltaAngleDegrees(TurnAnchor,Yaw))>=48.f)
    {
        const bool Right=FMath::FindDeltaAngleDegrees(TurnAnchor,Yaw)>0;
        StartRifleAction(bIsCrouched?(Right?Clip::Crouch_AimTurn_R90:Clip::Crouch_AimTurn_L90):(Right?Clip::AimTurn_R90:Clip::AimTurn_L90));
        TurnAnchor=Yaw;
    }
    const bool ReloadAction=RifleAction==Clip::Reload || RifleAction==Clip::Crouch_Reload;
    if(ReloadAction && !bReloading) RifleAction=Clip::None;
    if(bReloading) RifleAction=bIsCrouched?Clip::Crouch_Reload:Clip::Reload;
    if(IsAuto(RifleAction) && !bTrigger) RifleAction=Clip::None;
    auto* Action=RifleClips.IsValidIndex(int32(RifleAction))?RifleClips[int32(RifleAction)].Get():nullptr;
    if(Action && !Death && !bReloading && !IsAuto(RifleAction) && ActionElapsed>=Action->GetPlayLength())
    {
        Action=nullptr;RifleAction=Clip::None;
    }
    Clip Selected=Action?RifleAction:(NerveDamage->IsBurstActive() && !bUnarmed?Clip::Stun:Base);
    if(!Action && bUnarmed) Selected=Clip::None;
    auto* Sequence=RifleClips.IsValidIndex(int32(Selected))?RifleClips[int32(Selected)].Get():nullptr;
    if(Selected!=RiflePose)
    {
        LocomotionBlendFrom=BodyPose.Local;LocomotionBlendTime=0;
        RiflePose=Selected;
    }
    if(!Sequence) return false;
    auto* BaseSequence=RifleClips[int32(Base)].Get();
    const float Interval=BaseSequence?BaseSequence->GetPlayLength():1.f;
    RifleCycle=FMath::Fmod(RifleCycle+Dt*(StrideSpeed>0?Speed/StrideSpeed:1.f)/FMath::Max(.01f,Interval),1.f);
    const bool Layer=Speed>10 && (IsShot(Selected) || bReloading || Selected==Clip::Hit_Low || Selected==Clip::Hit_Upper || Selected==Clip::Stun);
    TArray<FTransform> Legs;
    if(Layer && BodyPose.Sample(BaseSequence,RifleCycle*Interval,true)) Legs=BodyPose.Local;
    const float Time=bReloading?ReloadProgress*Sequence->GetPlayLength():(Action?ActionElapsed:(Selected==Clip::Stun?GetWorld()->GetTimeSeconds():RifleCycle*Interval));
    ActionElapsed+=Dt;
    if(!BodyPose.Sample(Sequence,Time,!Action || IsAuto(Selected))) return false;
    if(IsTurn(Selected))
    {
        const int32 Pelvis=BodyPose.Bone(EBreachBone::Pelvis);
        const FQuat Delta=BodyPose.CS[Pelvis].GetRotation()*BodyPose.ReferenceCS[Pelvis].GetRotation().Inverse();
        FQuat Swing,Twist;Delta.ToSwingTwist(FVector::UpVector,Swing,Twist);
        for(int32 Index=0;Index<BodyPose.Parents.Num();++Index)
            if(BodyPose.Parents[Index]==INDEX_NONE) BodyPose.Rotate(Index,Twist.Inverse());
    }
    if(!Legs.IsEmpty())
    {
        const int32 Spine=BodyPose.Bone(EBreachBone::Spine);
        for(int32 Index=0;Index<Legs.Num();++Index)
            if(!BodyPose.IsUnder(Index,Spine)) BodyPose.Local[Index]=Legs[Index];
        BodyPose.Rebuild();
    }
    return true;
}
