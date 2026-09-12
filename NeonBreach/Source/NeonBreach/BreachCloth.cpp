#include "BreachCloth.h"
#include "BreachPose.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"

void FBreachCloth::Init(USkeletalMesh* Asset,int32 ModelIndex)
{
    Joints.Reset();bInitialized=false;
    if(!Asset) return;
    const auto& Ref=Asset->GetRefSkeleton();TSet<int32> Selected;
    // Supplied PMX cloth-chain indices. Find by exported name, never UE array index.
    const auto Select=[&](int32 First,int32 Last)
    {
        for(int32 I=First;I<=Last;++I)
        {
            const int32 B=Ref.FindBoneIndex(FName(*FString::Printf(TEXT("bone_%03d"),I)));
            if(B!=INDEX_NONE) Selected.Add(B);
        }
    };
    if(ModelIndex==0) { Select(113,138);Select(171,187);Select(198,203); }
    if(ModelIndex==1) { Select(112,120);Select(209,235);Select(238,358); }
    if(ModelIndex==2) Select(313,492);
    // Ascalon's revised mesh retains these original coat bones but has no PMX rigid bodies.
    if(ModelIndex==3) Select(294,343);
    for(int32 I=0;I<Ref.GetNum();++I) if(Selected.Contains(I))
        for(int32 C=I+1;C<Ref.GetNum();++C) if(Ref.GetParentIndex(C)==I && Selected.Contains(C))
        { FJoint Joint;Joint.Bone=I;Joint.Child=C;Joints.Add(Joint);break; }
    UE_LOG(LogTemp,Display,TEXT("CLOTH_RIG %s joints=%d"),*Asset->GetName(),Joints.Num());
}

void FBreachCloth::CopyTo(FBreachPose& Pose) const
{
    for(auto& Joint:Joints) if(Pose.Local.IsValidIndex(Joint.Bone))
        Pose.Local[Joint.Bone].SetRotation((Pose.Local[Joint.Bone].GetRotation()*Joint.Delta).GetNormalized());
    Pose.Rebuild();
}

void FBreachCloth::Update(FBreachPose& Pose,const FTransform& Transform,float Dt,UWorld* World,const AActor* Owner)
{
    if(Joints.IsEmpty() || Pose.CS.IsEmpty()) return;
    if(!FMath::IsFinite(Dt) || Dt<0) return;
    const bool Reset=!bInitialized || Dt>.15f || FVector::DistSquared(Transform.GetLocation(),PreviousTransform.GetLocation())>FMath::Square(180.f) ||
        Transform.GetRotation().AngularDistance(PreviousTransform.GetRotation())>FMath::DegreesToRadians(100.f);
    // Some death tracks have a final ground correction in component space.
    for(int32 I=0;I<Pose.Local.Num();++I)
        Pose.Local[I]=Pose.Parents[I]>=0?Pose.CS[I].GetRelativeTransform(Pose.CS[Pose.Parents[I]]):Pose.CS[I];
    const auto Animated=Pose.Local;
    TMap<int32,int32> JointForBone;
    for(int32 I=0;I<Joints.Num();++I) JointForBone.Add(Joints[I].Bone,I);
    const int32 Steps=FMath::Clamp(FMath::CeilToInt(Dt*120.f),1,18);
    const float H=FMath::Min(Dt/Steps,1.f/120.f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BreachCloth),false,Owner);
    for(int32 I=0;I<Pose.Local.Num();++I)
    {
        const int32 Parent=Pose.Parents[I];
        Pose.CS[I]=Parent>=0?Pose.Local[I]*Pose.CS[Parent]:Pose.Local[I];
        const int32* JointIndex=JointForBone.Find(I);if(!JointIndex) continue;
        auto& Joint=Joints[*JointIndex];
        const FVector Anchor=Transform.TransformPosition(Pose.CS[I].GetLocation());
        const FVector RestTip=Transform.TransformPosition(Pose.CS[I].TransformPosition(Animated[Joint.Child].GetLocation()));
        const FVector RestDirection=(RestTip-Anchor).GetSafeNormal();const float Length=FVector::Distance(RestTip,Anchor);
        if(Length<.1f) continue;
        if(Reset) { Joint.Tip=RestTip;Joint.Velocity=FVector::ZeroVector; }
        for(int32 Step=0;Step<Steps && !Reset;++Step)
        {
            // Damped spring toward the authored pose, with reduced gravity for stiff costume fabric.
            Joint.Velocity+=((RestTip-Joint.Tip)*240.f+FVector(0,0,-147.f))*H;
            Joint.Velocity*=FMath::Exp(-12.f*H);
            Joint.Velocity=Joint.Velocity.GetClampedToMaxSize(350.f);
            FVector Next=Joint.Tip+Joint.Velocity*H;
            // Body capsules follow the animated pelvis, torso and both legs.
            for(const auto Pair:{TPair<EBreachBone,EBreachBone>(EBreachBone::Pelvis,EBreachBone::Chest),
                 {EBreachBone::LThigh,EBreachBone::LKnee},{EBreachBone::RThigh,EBreachBone::RKnee},
                 {EBreachBone::LKnee,EBreachBone::LFoot},{EBreachBone::RKnee,EBreachBone::RFoot}})
            {
                if(!Pose.CS.IsValidIndex(Pose.Bone(Pair.Key)) || !Pose.CS.IsValidIndex(Pose.Bone(Pair.Value))) continue;
                const FVector A=Transform.TransformPosition(Pose.CS[Pose.Bone(Pair.Key)].GetLocation());
                const FVector B=Transform.TransformPosition(Pose.CS[Pose.Bone(Pair.Value)].GetLocation());
                const FVector Closest=FMath::ClosestPointOnSegment(Next,A,B);
                const float Radius=Pair.Key==EBreachBone::Pelvis?11.f:5.f;
                // Do not push authored contact points outward through narrow clothing.
                const float Effective=FMath::Min(Radius,float(FVector::Distance(RestTip,FMath::ClosestPointOnSegment(RestTip,A,B))));
                if(FVector::DistSquared(Next,Closest)<FMath::Square(Effective))
                    Next=Closest+(Next-Closest).GetSafeNormal(UE_SMALL_NUMBER,(RestTip-Closest).GetSafeNormal())*Effective;
            }
            // Keep chain lengths exact and constrain excessive bend after collision projection.
            FQuat Swing=FQuat::FindBetweenNormals(RestDirection,(Next-Anchor).GetSafeNormal(UE_SMALL_NUMBER,RestDirection));
            const float Angle=Swing.GetAngle();
            if(Angle>FMath::DegreesToRadians(30.f)) Swing=FQuat::Slerp(FQuat::Identity,Swing,FMath::DegreesToRadians(30.f)/Angle);
            Next=Anchor+Swing.RotateVector(RestDirection)*Length;
            if(World && Step==Steps-1)
            {
                FHitResult Hit;
                if(World->SweepSingleByChannel(Hit,Joint.Tip,Next,FQuat::Identity,ECC_WorldStatic,FCollisionShape::MakeSphere(.7f),Params) && !Hit.bStartPenetrating)
                {
                    Next=Hit.Location+Hit.Normal*.2f;
                    Next=Anchor+(Next-Anchor).GetSafeNormal(UE_SMALL_NUMBER,RestDirection)*Length;
                    Joint.Velocity=FVector::VectorPlaneProject(Joint.Velocity,Hit.Normal)*.4f;
                }
            }
            Joint.Tip=Next;
        }
        const FVector Desired=Transform.InverseTransformVectorNoScale(Joint.Tip-Anchor).GetSafeNormal();
        const FVector Original=Pose.CS[I].TransformVectorNoScale(Animated[Joint.Child].GetLocation()).GetSafeNormal();
        const FQuat Swing=FQuat::FindBetweenNormals(Original,Desired);
        const FQuat ParentRotation=Parent>=0?Pose.CS[Parent].GetRotation():FQuat::Identity;
        Pose.Local[I].SetRotation((ParentRotation.Inverse()*Swing*Pose.CS[I].GetRotation()).GetNormalized());
        Joint.Delta=(Animated[I].GetRotation().Inverse()*Pose.Local[I].GetRotation()).GetNormalized();
        Pose.CS[I]=Parent>=0?Pose.Local[I]*Pose.CS[Parent]:Pose.Local[I];
    }
    PreviousTransform=Transform;bInitialized=true;
}
