#include "BreachSelectionStage.h"
#include "BreachVisuals.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"

void ABreachSelectionStage::SetupSword()
{
    auto* Asset=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/AcheronSword/SK_AcheronSword.SK_AcheronSword"));
    bSwordReady=Asset && SwordPose.InitSkeleton(Asset);
    if(!bSwordReady) return;
    for(auto* Prop:{Sword.Get(),Scabbard.Get()})
    {
        Prop->SetSkinnedAssetAndUpdate(Asset);SwordPose.Apply(Prop);Prop->RefreshBoneTransforms();
    }
    bSwordReady=Sword->GetBoneIndex(TEXT("bone_002"))!=INDEX_NONE && Sword->GetBoneIndex(TEXT("bone_003"))!=INDEX_NONE;
    Sword->SetMaterial(0,Breach::Material(TEXT("M_ShadowOverlay")));
    Scabbard->SetMaterial(1,Breach::Material(TEXT("M_ShadowOverlay")));
}

void ABreachSelectionStage::UpdateSwordEntrance()
{
    const FTransform ToStage=Preview->GetRelativeTransform();
    const FVector Hip=Pose.CS[Pose.Bone(EBreachBone::Pelvis)].GetLocation();
    // The free hand in the source sword attack is not holding a scabbard.
    // Keep the supplied sheath at the left hip, clear of the face and slash.
    Pose.SolveArm(0,Hip+FVector(23,10,9),Hip+FVector(40,-9,25));
    for(int32 Side=0;Side<2;++Side)
    {
        const int32 Hand=Pose.Bone(Side?EBreachBone::RHand:EBreachBone::LHand);
        const FVector HandPosition=Pose.CS[Hand].GetLocation();
        FVector Along=(Pose.CS[Pose.Fingers[Side][6]].GetLocation()-HandPosition).GetSafeNormal();
        FVector Across=(Pose.CS[Pose.Fingers[Side][3]].GetLocation()-Pose.CS[Pose.Fingers[Side][12]].GetLocation()).GetSafeNormal();
        Across=Side?FMath::Lerp(Across,FVector(.8f,-.5f,.15f).GetSafeNormal(),FMath::SmoothStep(2.35f,3.5f,AnimationTime)).GetSafeNormal():FVector(.2f,-.55f,-.81f).GetSafeNormal();
        Along=(Along-Across*FVector::DotProduct(Along,Across)).GetSafeNormal();
        const FVector Normal=FVector::CrossProduct(Along,Across).GetSafeNormal()*(Side?1.f:-1.f);
        Pose.PoseHand(Side,Along,Normal,.95f);
        auto* Prop=Side?Sword.Get():Scabbard.Get();
        // The supplied blade runs along +Y in its imported bind pose. Across the
        // knuckles is the grip axis; the source animation supplies the wrist swing.
        const FVector Axis=ToStage.TransformVectorNoScale(Across).GetSafeNormal();
        const FQuat Rotation=FRotationMatrix::MakeFromYZ(Axis,ToStage.TransformVectorNoScale(Along)).ToQuat();
        const FVector Grip=ToStage.TransformPosition(HandPosition+Along*3.f+Normal*1.5f);
        const int32 Handle=Prop->GetBoneIndex(Side?TEXT("bone_002"):TEXT("bone_003"));
        const FVector Anchor=SwordPose.ReferenceCS[Handle].GetLocation();
        Prop->SetRelativeRotation(Rotation);Prop->SetRelativeLocation(Grip-Rotation.RotateVector(Anchor));
    }
}
