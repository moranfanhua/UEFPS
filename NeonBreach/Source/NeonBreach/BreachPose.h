#pragma once
#include "CoreMinimal.h"
class USkeletalMesh;
class UPoseableMeshComponent;

enum class EBreachBone : uint8 { Pelvis,Spine,Chest,Neck,Head,LArm,LElbow,LHand,RArm,RElbow,RHand,LThigh,LKnee,LFoot,RThigh,RKnee,RFoot,Count };

// A small rig shared by the four supplied skeletons. Rotations use component
// space so that the PMX and FBX joint orientation conventions can differ.
struct FBreachPose
{
    TArray<FTransform> Reference,ReferenceCS,Local,CS;
    TArray<int32> Parents;
    int32 Bones[17];
    int32 Fingers[2][15];
    bool Init(USkeletalMesh* Asset,int32 ModelIndex);
    int32 Bone(EBreachBone B) const { return Bones[int32(B)]; }
    bool IsUnder(int32 Index,int32 Ancestor) const;
    void Reset();
    void Rebuild();
    void Rotate(int32 Index,const FQuat& Delta);
    void Rotate(EBreachBone B,const FVector& Axis,float Degrees);
    void Aim(int32 Index,int32 Child,const FVector& Direction);
    void SolveArm(int32 Side,const FVector& HandTarget,const FVector& ElbowHint);
    void PoseHand(int32 Side,const FVector& FingerDirection,const FVector& PalmNormal,float Curl);
    void Walk(float Phase,float Speed);
    void Apply(UPoseableMeshComponent* Mesh,bool HideHead=false,bool HideArms=false) const;
};
