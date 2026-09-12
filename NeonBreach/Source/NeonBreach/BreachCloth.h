#pragma once
#include "CoreMinimal.h"
struct FBreachPose;
class USkeletalMesh;
class UWorld;
class AActor;

// Cosmetic bone-chain dynamics. Never changes movement, collision or the camera.
struct FBreachCloth
{
    struct FJoint
    {
        int32 Bone=INDEX_NONE,Child=INDEX_NONE;
        FVector Tip=FVector::ZeroVector,Velocity=FVector::ZeroVector;
        FQuat Delta=FQuat::Identity;
    };
    TArray<FJoint> Joints;
    FTransform PreviousTransform;
    bool bInitialized=false;
    void Init(USkeletalMesh* Asset,int32 ModelIndex);
    void Update(FBreachPose& Pose,const FTransform& Transform,float DeltaSeconds,UWorld* World=nullptr,const AActor* Owner=nullptr);
    void CopyTo(FBreachPose& Pose) const;
};
