#include "BreachGame.h"
#include "BreachVisuals.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/MorphTarget.h"
#include "Components/PoseableMeshComponent.h"

void ABreachGameMode::RunDeformationChecks(TFunctionRef<void(bool,const FString&)> Check)
{
    auto* Mesh=Breach::CharacterMesh(0);FBreachPose Expression;Expression.Init(Mesh,0);
    auto* Entry=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Animations/Entrance/Eula/A_Eula_Eula_VMD_Entry.A_Eula_Eula_VMD_Entry"));
    auto* Finish=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Animations/Entrance/Eula/A_Eula_Eula_VMD_Finish.A_Eula_Eula_VMD_Finish"));
    Check(Mesh && Mesh->GetMorphTargets().Num()>=60,TEXT("Eula retains 60 supplied vertex expressions"));
    const bool Blink=Expression.Sample(Entry,1.2f,false);
    Check(Blink && Expression.MorphWeights.FindRef(TEXT("まばたき"))>.99f,TEXT("VMD authored blink reaches full closure at source 1.2s"));
    Expression.Sample(Entry,0,false);
    Check(Expression.MorphWeights.FindRef(TEXT("まばたき"))<.01f,TEXT("VMD eyes reopen outside blink keys"));
    Expression.Sample(Finish,0,false);
    Check(FMath::IsNearlyEqual(Expression.MorphWeights.FindRef(TEXT("にやり")),.27f,.001f) &&
          FMath::IsNearlyEqual(Expression.MorphWeights.FindRef(TEXT("怒り")),.15f,.001f),TEXT("Finishing VMD preserves authored mouth and eyebrow weights"));
    auto* Component=NewObject<UPoseableMeshComponent>(this);Component->SetSkinnedAssetAndUpdate(Mesh);
    Expression.Apply(Component);
    Check(Component->ActiveMorphTargets.Num()==2,TEXT("Poseable renderer receives finishing morph weights"));
    Expression.Reset();Expression.Apply(Component);
    bool Cleared=Component->ActiveMorphTargets.IsEmpty() && Component->MorphTargetWeights.Num()==Mesh->GetMorphTargets().Num();
    for(float Weight:Component->MorphTargetWeights) Cleared&=Weight==0;
    Check(Cleared,TEXT("Reset clears stale expressions while preserving GPU morph layout"));
    for(int32 Model=0;Model<4;++Model)
    {
        Mesh=Breach::CharacterMesh(Model);FBreachPose Base;Base.Init(Mesh,Model);
        auto* Idle=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Animations/Entrance/%s/A_%s_Female_Idle.A_%s_Female_Idle"),Breach::Keys[Model],Breach::Keys[Model],Breach::Keys[Model]));
        if(!Base.Sample(Idle,1,false)) Base.Walk(0,0);
        FBreachCloth Cloth;Cloth.Init(Mesh,Model);
        Check(Cloth.Joints.Num()>10,FString::Printf(TEXT("%s cloth chains resolve against current skeleton"),Breach::Keys[Model]));
        FBreachPose Sim=Base;FTransform Transform=FTransform::Identity;Cloth.Update(Sim,Transform,0);
        float MaxAngle=0;bool Stable=true,CoreUnchanged=true,Lengths=true;
        for(int32 Frame=0;Frame<240;++Frame)
        {
            Sim=Base;Transform.SetLocation(FVector(Frame<100?Frame*5.f:500.f,0,0));
            Cloth.Update(Sim,Transform,1.f/60);
            for(const auto& Joint:Cloth.Joints)
            {
                MaxAngle=FMath::Max(MaxAngle,float(Joint.Delta.GetAngle()));
                Stable&=!Joint.Tip.ContainsNaN() && !Joint.Velocity.ContainsNaN();
                Lengths&=FMath::IsNearlyEqual(FVector::Distance(Sim.CS[Joint.Bone].GetLocation(),Sim.CS[Joint.Child].GetLocation()),
                    FVector::Distance(Base.CS[Joint.Bone].GetLocation(),Base.CS[Joint.Child].GetLocation()),.01);
            }
            for(int32 B:Base.Bones) if(B!=INDEX_NONE) CoreUnchanged&=Base.CS[B].Equals(Sim.CS[B],.001);
        }
        Check(Stable && MaxAngle>.01f && MaxAngle<.54f,FString::Printf(TEXT("%s cloth responds to movement with bounded swing (%.1f degrees)"),Breach::Keys[Model],FMath::RadiansToDegrees(MaxAngle)));
        Check(CoreUnchanged && Lengths,FString::Printf(TEXT("%s cloth keeps chain lengths and never modifies core/limb bones"),Breach::Keys[Model]));
        FBreachPose OwnerPose=Base;Cloth.CopyTo(OwnerPose);bool Shared=true;
        for(int32 B=0;B<OwnerPose.CS.Num();++B) Shared&=OwnerPose.CS[B].Equals(Sim.CS[B],.001);
        Check(Shared,FString::Printf(TEXT("%s owner shares world cloth rotations without camera offsets"),Breach::Keys[Model]));
        Transform.AddToTranslation(FVector(2000,0,0));Sim=Base;Cloth.Update(Sim,Transform,1.f/60);
        bool Reset=true;for(const auto& J:Cloth.Joints) Reset&=J.Velocity.IsNearlyZero() && J.Delta.Equals(FQuat::Identity,.001);
        Check(Reset,FString::Printf(TEXT("%s teleport resets cloth safely"),Breach::Keys[Model]));
        Sim=Base;Cloth.Update(Sim,Transform,.5f);bool Hitch=true;
        for(const auto& J:Cloth.Joints) Hitch&=J.Velocity.IsNearlyZero() && !J.Tip.ContainsNaN();
        Check(Hitch,FString::Printf(TEXT("%s long frame resets simulation rather than exploding"),Breach::Keys[Model]));
    }
}
