#include "BreachSelectionStage.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
    UMaterialInterface* CatMaterial(const TCHAR* Name)
    {
        return LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Characters/SelectionCat/%s.%s"),Name,Name));
    }
}

void ABreachSelectionStage::SetupCat()
{
    auto* Asset=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/SelectionCat/SK_SelectionCat.SK_SelectionCat"));
    Cat->SetSkinnedAssetAndUpdate(Asset);
    bCatReady=CatPose.InitSkeleton(Asset);
    if(!bCatReady) return;
    for(const TCHAR* Name:{TEXT("head"),TEXT("neck"),TEXT("body_1"),TEXT("body_2"),TEXT("pelvis")})
        bCatReady&=Cat->GetBoneIndex(Name)!=INDEX_NONE;
    if(!bCatReady) return;
    Cat->SetMaterial(0,CatMaterial(TEXT("M_CatFur")));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    const auto Detail=[&](const TCHAR* Bone,FVector Position,FVector Size,const TCHAR* Material)
    {
        const int32 Index=Cat->GetBoneIndex(Bone);
        if(!Sphere || !CatPose.ReferenceCS.IsValidIndex(Index)) return;
        auto* Part=NewObject<UStaticMeshComponent>(this);Part->SetupAttachment(Cat);
        Part->SetStaticMesh(Sphere);Part->SetMaterial(0,CatMaterial(Material));
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetCastShadow(false);
        Part->SetVisibility(false);Part->RegisterComponent();
        CatDetails.Add(Part);CatDetailBones.Add(Index);
        CatDetailBindings.Add(FTransform(FQuat::Identity,Position,Size/100.f)*CatPose.ReferenceCS[Index].Inverse());
    };
    for(float Side:{-1.f,1.f})
    {
        Detail(TEXT("head"),FVector(25.0f,Side*3.7f,22.8f),FVector(1.9f,1.1f,1.9f),TEXT("M_CatEye"));
        Detail(TEXT("head"),FVector(25.3f,Side*4.15f,23.25f),FVector(.44f,.3f,.44f),TEXT("M_CatFur"));
        Detail(Side<0?TEXT("ear_left"):TEXT("ear_right"),FVector(23.5f,Side*3.9f,25.6f),FVector(1.5f,.65f,2.5f),TEXT("M_CatPink"));
    }
    Detail(TEXT("head"),FVector(27.8f,0,21.0f),FVector(.8f,1.3f,.85f),TEXT("M_CatPink"));
    // A small teal collar follows the neck; the cat remains a fully skinned model.
    for(int32 I=0;I<20;++I)
    {
        const float Angle=2.f*PI*I/20.f;
        Detail(TEXT("neck"),FVector(15.8f,FMath::Sin(Angle)*4.1f,18.8f+FMath::Cos(Angle)*4.6f),FVector(1.35f,1.35f,1.35f),TEXT("M_CatCollar"));
    }
}

FVector ABreachSelectionStage::CatSupportPoint(int32 Side) const
{
    return GetActorTransform().TransformPosition(CatSupports[FMath::Clamp(Side,0,1)]);
}

void ABreachSelectionStage::UpdateCatEntrance()
{
    // These paired poses are authored for the preview only. Both hands solve to
    // contacts on the same moving cat, so the animal cannot drift out of the grip.
    const float Lift=FMath::SmoothStep(0.f,.95f,AnimationTime);
    const float Nuzzle=FMath::SmoothStep(.8f,2.5f,AnimationTime);
    const float Settle=FMath::SmoothStep(2.5f,EntranceDuration,AnimationTime);
    const float Lean=Nuzzle*(1.f-.18f*Settle);
    const FTransform ToStage=Preview->GetRelativeTransform();
    const auto Axis=[&](FVector V) { return ToStage.InverseTransformVectorNoScale(V); };
    Pose.Rotate(EBreachBone::Spine,Axis(FVector::ForwardVector),-2.f*Lean);
    Pose.Rotate(EBreachBone::Chest,Axis(FVector::UpVector),-4.f+3.f*Lift);
    Pose.Rotate(EBreachBone::Neck,Axis(FVector::ForwardVector),-11.f*Lean);
    Pose.Rotate(EBreachBone::Neck,Axis(FVector::RightVector),5.f+8.f*Lean);
    Pose.Rotate(EBreachBone::Head,Axis(FVector::UpVector),9.f+24.f*Lean);
    Pose.Rotate(EBreachBone::Head,Axis(FVector::ForwardVector),-4.f*Lean);
    Pose.Rotate(EBreachBone::Head,Axis(FVector::RightVector),4.f*Lean);
    const FVector Head=ToStage.TransformPosition(Pose.CS[Pose.Bone(EBreachBone::Head)].GetLocation());

    CatPose.Reset();
    // The belly and face point towards the holder, with the back towards camera.
    const FQuat Rotation=FRotator(72.f-3.f*Lean,172.f+4.f*Lean,0).Quaternion();
    const auto Bone=[this](const TCHAR* Name) { return Cat->GetBoneIndex(Name); };
    const auto Turn=[&](const TCHAR* Name,FVector A,float Degrees) { CatPose.Rotate(Bone(Name),FQuat(A,FMath::DegreesToRadians(Degrees))); };
    Turn(TEXT("neck"),FVector::RightVector,45.f+6.f*Lean);
    Turn(TEXT("head"),FVector::RightVector,12.f);
    Turn(TEXT("head"),FVector::UpVector,8.f*Lean);
    Turn(TEXT("head"),Rotation.Inverse().RotateVector(FVector::UpVector),28.f+18.f*Lean);
    Turn(TEXT("ear_left"),FVector::ForwardVector,-5.f*Lean);
    Turn(TEXT("ear_right"),FVector::ForwardVector,5.f*Lean);
    const auto Aim=[&](const FString& A,const FString& B,FVector Direction)
    {
        CatPose.Aim(Cat->GetBoneIndex(FName(*A)),Cat->GetBoneIndex(FName(*B)),Rotation.Inverse().RotateVector(Direction));
    };
    for(const TCHAR* Side:{TEXT("left"),TEXT("right")})
    {
        const FString Front=FString::Printf(TEXT("leg_front_%s_"),Side),Back=FString::Printf(TEXT("leg_back_%s_"),Side);
        Aim(Front+TEXT("1"),Front+TEXT("2"),FVector(-.8f,-.12f,.6f));
        Aim(Front+TEXT("2"),Front+TEXT("3"),FVector(-1,-.1f,-.15f));
        Aim(Front+TEXT("3"),Front+TEXT("4"),FVector(-.7f,0,-.35f));
        Aim(Back+TEXT("1"),Back+TEXT("2"),FVector(.25f,0,-1));
        Aim(Back+TEXT("2"),Back+TEXT("3"),FVector(-.1f,0,-1));
        Aim(Back+TEXT("3"),Back+TEXT("4"),FVector(.1f,0,-1));
    }
    const float TailSweep=FMath::Sin(Lift*PI)*9.f+4.f*Lean;
    Turn(TEXT("tail_2"),FVector::UpVector,TailSweep);
    Turn(TEXT("tail_3"),FVector::UpVector,8.f+TailSweep*.5f);
    Turn(TEXT("tail_4"),FVector::UpVector,12.f);
    constexpr float Scale=.92f;
    const FVector TargetHead=Head+FVector(15.f-2.f*Lean,22.f-6.f*Lean,-12.f+8.f*Lift);
    const FVector Location=TargetHead-Rotation.RotateVector(CatPose.CS[Bone(TEXT("head"))].GetLocation()*Scale);
    const FTransform CatToStage(Rotation,Location,FVector(Scale));
    Cat->SetRelativeTransform(CatToStage);
    CatPose.Apply(Cat);Cat->RefreshBoneTransforms();
    for(int32 I=0;I<CatDetails.Num();++I)
        CatDetails[I]->SetRelativeTransform(CatDetailBindings[I]*CatPose.CS[CatDetailBones[I]]);

    const auto CatPoint=[&](const TCHAR* Name) { return CatToStage.TransformPosition(CatPose.CS[Bone(Name)].GetLocation()); };
    CatSupports[1]=CatPoint(TEXT("body_1"))+FVector(4.f,-2.f,-2.f);
    CatSupports[0]=CatPoint(TEXT("body_2"))+FVector(3.5f,-2.f,2.f);
    const FVector Chest=ToStage.TransformPosition(Pose.CS[Pose.Bone(EBreachBone::Chest)].GetLocation());
    Pose.SolveArm(1,ToStage.InverseTransformPosition(CatSupports[1]),ToStage.InverseTransformPosition(Chest+FVector(15,37,-5)));
    Pose.PoseHand(1,Axis(FVector(-.3f,-1,.15f)),Axis(FVector(0,0,1)),.32f);
    Pose.SolveArm(0,ToStage.InverseTransformPosition(CatSupports[0]),ToStage.InverseTransformPosition(Chest+FVector(23,-27,-4)));
    Pose.PoseHand(0,Axis(FVector(.05f,1,.3f)),Axis(FVector(-1,0,.1f)),.26f);
}
