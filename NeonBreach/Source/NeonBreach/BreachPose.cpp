#include "BreachPose.h"
#include "CharacterRigData.h"
#include "Engine/SkeletalMesh.h"
#include "Components/PoseableMeshComponent.h"

bool FBreachPose::Init(USkeletalMesh* Asset,int32 ModelIndex)
{
    if(!Asset) return false;
    const FReferenceSkeleton& Ref=Asset->GetRefSkeleton();
    Reference=Ref.GetRefBonePose(); Parents.SetNum(Reference.Num());
    for(int32 i=0;i<Parents.Num();++i) Parents[i]=Ref.GetParentIndex(i);
    const auto Find=[&](const TCHAR* Name)
    {
        int32 I=Ref.FindBoneIndex(FName(Name));
        if(I!=INDEX_NONE) return I;
        FString Alternate(Name); Alternate.ReplaceInline(TEXT("-"),TEXT("_"));
        I=Ref.FindBoneIndex(FName(*Alternate));
        if(I!=INDEX_NONE) return I;
        Alternate.ReplaceInline(TEXT("_"),TEXT(" "));
        return Ref.FindBoneIndex(FName(*Alternate));
    };
    bool Valid=true;
    for(int32 i=0;i<17;++i) { Bones[i]=Find(BreachRigNames[ModelIndex][i]); Valid&=Bones[i]!=INDEX_NONE; }
    for(int32 s=0;s<2;++s) for(int32 i=0;i<15;++i) Fingers[s][i]=Find(BreachFingerNames[ModelIndex][s][i]);
    Reset(); ReferenceCS=CS;
    return Valid;
}
bool FBreachPose::IsUnder(int32 I,int32 Ancestor) const
{
    if(Ancestor==INDEX_NONE) return false;
    while(Parents.IsValidIndex(I)) { if(I==Ancestor) return true; I=Parents[I]; }
    return false;
}
void FBreachPose::Reset() { Local=Reference; Rebuild(); }
void FBreachPose::Rebuild()
{
    CS.SetNum(Local.Num());
    for(int32 i=0;i<Local.Num();++i) CS[i]=Parents[i]>=0?Local[i]*CS[Parents[i]]:Local[i];
}
void FBreachPose::Rotate(int32 I,const FQuat& Delta)
{
    if(!CS.IsValidIndex(I)) return;
    const FQuat Parent=Parents[I]>=0?CS[Parents[I]].GetRotation():FQuat::Identity;
    Local[I].SetRotation((Parent.Inverse()*Delta*CS[I].GetRotation()).GetNormalized());
    Rebuild();
}
void FBreachPose::Rotate(EBreachBone B,const FVector& Axis,float Degrees) { Rotate(Bone(B),FQuat(Axis,FMath::DegreesToRadians(Degrees))); }
void FBreachPose::Aim(int32 I,int32 Child,const FVector& Direction)
{
    if(!CS.IsValidIndex(I) || !CS.IsValidIndex(Child)) return;
    Rotate(I,FQuat::FindBetweenNormals((CS[Child].GetLocation()-CS[I].GetLocation()).GetSafeNormal(),Direction.GetSafeNormal()));
}
void FBreachPose::SolveArm(int32 Side,const FVector& Target,const FVector& Hint)
{
    const int32 Upper=Bone(Side?EBreachBone::RArm:EBreachBone::LArm);
    const int32 Lower=Bone(Side?EBreachBone::RElbow:EBreachBone::LElbow);
    const int32 Hand=Bone(Side?EBreachBone::RHand:EBreachBone::LHand);
    if(!CS.IsValidIndex(Upper) || !CS.IsValidIndex(Lower) || !CS.IsValidIndex(Hand)) return;
    const FVector Shoulder=CS[Upper].GetLocation();
    const float A=FVector::Distance(Shoulder,CS[Lower].GetLocation());
    const float B=FVector::Distance(CS[Lower].GetLocation(),CS[Hand].GetLocation());
    const float D=FMath::Clamp(float(FVector::Distance(Shoulder,Target)),FMath::Abs(A-B)+.001f,(A+B)*.995f);
    const FVector Direction=(Target-Shoulder).GetSafeNormal();
    FVector Bend=Hint-Shoulder; Bend=(Bend-Direction*FVector::DotProduct(Bend,Direction)).GetSafeNormal();
    const float Along=(D*D+A*A-B*B)/(2*D);
    const FVector Elbow=Shoulder+Direction*Along+Bend*FMath::Sqrt(FMath::Max(0.f,A*A-Along*Along));
    Aim(Upper,Lower,Elbow-Shoulder);
    Aim(Lower,Hand,Shoulder+Direction*D-CS[Lower].GetLocation());
}
void FBreachPose::PoseHand(int32 Side,const FVector& Direction,const FVector& Normal,float Curl)
{
    const int32 Hand=Bone(Side?EBreachBone::RHand:EBreachBone::LHand);
    const int32 Middle=Fingers[Side][6],Index=Fingers[Side][3],Pinky=Fingers[Side][12];
    if(!CS.IsValidIndex(Middle) || !CS.IsValidIndex(Index) || !CS.IsValidIndex(Pinky)) return;
    Aim(Hand,Middle,Direction);
    const FVector Along=(CS[Middle].GetLocation()-CS[Hand].GetLocation()).GetSafeNormal();
    FVector CurrentNormal=FVector::CrossProduct(Along,CS[Index].GetLocation()-CS[Pinky].GetLocation()).GetSafeNormal();
    if(Side==0) CurrentNormal*=-1;
    const FVector TargetNormal=(Normal-Along*FVector::DotProduct(Normal,Along)).GetSafeNormal();
    const float Angle=FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(CurrentNormal,TargetNormal),Along),FVector::DotProduct(CurrentNormal,TargetNormal));
    Rotate(Hand,FQuat(Along,Angle));
    const FVector Axis=FVector::CrossProduct(Along,TargetNormal).GetSafeNormal();
    for(int32 Finger=0;Finger<5;++Finger) for(int32 Joint=0;Joint<3;++Joint)
    {
        const float Amount=(Finger==0?.25f:(Side==1 && Finger==1?.4f:1.f))*Curl;
        Rotate(Fingers[Side][Finger*3+Joint],FQuat(Axis,FMath::DegreesToRadians((Joint==1?65.f:42.f)*Amount)));
    }
}
void FBreachPose::Walk(float Phase,float Speed)
{
    Reset();
    for(int32 S=0;S<2;++S)
    {
        const int32 Arm=Bone(S?EBreachBone::RArm:EBreachBone::LArm),Elbow=Bone(S?EBreachBone::RElbow:EBreachBone::LElbow);
        if(CS.IsValidIndex(Arm) && CS.IsValidIndex(Elbow))
        {
            const FVector Direction=(CS[Elbow].GetLocation()-CS[Arm].GetLocation()).GetSafeNormal();
            Aim(Arm,Elbow,Direction*.3f+FVector(0,0,-.9f));
        }
        const float Swing=FMath::Sin(Phase+(S?PI:0))*FMath::Clamp(Speed/220.f,0.f,1.f);
        Rotate(S?EBreachBone::RThigh:EBreachBone::LThigh,FVector::ForwardVector,Swing*23.f);
        Rotate(S?EBreachBone::RKnee:EBreachBone::LKnee,FVector::ForwardVector,-FMath::Max(0.f,-Swing)*30.f);
    }
}
void FBreachPose::Apply(UPoseableMeshComponent* Mesh,bool HideHead,bool HideArms) const
{
    for(int32 i=0;i<CS.Num();++i)
    {
        FTransform Transform=CS[i];
        for(EBreachBone Root:{EBreachBone::Head,EBreachBone::LArm,EBreachBone::RArm})
            if((Root==EBreachBone::Head?HideHead:HideArms) && IsUnder(i,Bone(Root)))
                Transform=FTransform(FQuat::Identity,CS[Bone(Root)].GetLocation(),FVector::ZeroVector);
        Mesh->SetBoneTransformByName(Mesh->GetBoneName(i),Transform,EBoneSpaces::ComponentSpace);
    }
}
