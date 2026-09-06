#include "BreachGame.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#if WITH_EDITOR
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#endif

int32 ABreachGameMode::PrepareFirstPersonArms(USkeletalMesh* Asset,int32 ModelIndex)
{
#if WITH_EDITOR
    if(!Asset || ModelIndex<0 || ModelIndex>3) return -1;
    FMeshDescription* Description=Asset->GetMeshDescription(0);
    if(!Description) return -1;
    FBreachPose Rig;
    if(!Rig.Init(Asset,ModelIndex)) return -1;
    FSkeletalMeshAttributes Attributes(*Description);
    auto Weights=Attributes.GetVertexSkinWeights();
    TArray<FPolygonID> Remove;
    for(const FPolygonID Polygon:Description->Polygons().GetElementIDs())
    {
        float ArmWeight=0; int32 Count=0;
        for(const FVertexInstanceID Instance:Description->GetPolygonVertexInstances(Polygon))
        {
            ++Count;
            for(const auto Weight:Weights.Get(Description->GetVertexInstanceVertex(Instance)))
                if(Rig.IsUnder(Weight.GetBoneIndex(),Rig.Bone(EBreachBone::LArm)) || Rig.IsUnder(Weight.GetBoneIndex(),Rig.Bone(EBreachBone::RArm))) ArmWeight+=Weight.GetWeight();
        }
        if(ArmWeight/FMath::Max(1,Count)<.75f) Remove.Add(Polygon);
    }
    if(Remove.IsEmpty() || Remove.Num()==Description->Polygons().Num()) return -1;
    Description->DeletePolygons(Remove);
    FElementIDRemappings Remappings;
    Description->Compact(Remappings);
    const int32 Kept=Description->Triangles().Num();
    Asset->CommitMeshDescription(0);
    Asset->PostEditChange();
    Asset->MarkPackageDirty();
    return Kept;
#else
    return -1;
#endif
}

UAnimSequence* ABreachGameMode::BakeDeathAnimation(USkeletalMesh* Asset,int32 ModelIndex,const FString& MotionFile,const FString& PackageName)
{
#if WITH_EDITOR
    FBreachPose Rig;
    if(!Asset || !Rig.Init(Asset,ModelIndex)) return nullptr;
    FString Text; TSharedPtr<FJsonObject> Data;
    if(!FFileHelper::LoadFileToString(Text,*MotionFile) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data)) return nullptr;
    const auto& Frames=Data->GetArrayField(TEXT("frames"));
    const auto& SourceRef=Data->GetArrayField(TEXT("reference"));
    if(Frames.Num()<2 || SourceRef.Num()!=47) return nullptr;
    const auto Position=[](const TSharedPtr<FJsonValue>& V)
    {
        const auto& A=V->AsObject()->GetArrayField(TEXT("p")); return FVector(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber());
    };
    const auto Rotation=[](const TSharedPtr<FJsonValue>& V)
    {
        const auto& A=V->AsObject()->GetArrayField(TEXT("q")); return FQuat(A[0]->AsNumber(),A[1]->AsNumber(),A[2]->AsNumber(),A[3]->AsNumber()).GetNormalized();
    };
    TArray<int32> Mapping; for(int32 I=0;I<17;++I) Mapping.Add(Rig.Bones[I]);
    for(int32 S=0;S<2;++S) for(int32 I=0;I<15;++I) Mapping.Add(Rig.Fingers[S][I]);
    int32 Child[47]={1,2,3,4,-1,6,7,23,9,10,38,12,13,-1,15,16,-1};
    for(int32 I=17;I<47;++I) Child[I]=(I-17)%3<2?I+1:-1;
    TArray<FQuat> Alignment;
    for(int32 I=0;I<47;++I)
    {
        FQuat Correction=FQuat::Identity;
        if(Child[I]>=0 && Mapping[I]>=0 && Mapping[Child[I]]>=0)
            Correction=FQuat::FindBetweenNormals((Rig.ReferenceCS[Mapping[Child[I]]].GetLocation()-Rig.ReferenceCS[Mapping[I]].GetLocation()).GetSafeNormal(),(Position(SourceRef[Child[I]])-Position(SourceRef[I])).GetSafeNormal());
        Alignment.Add(Correction);
    }
    const auto Bounds=Asset->GetBounds();
    const float Ground=Bounds.Origin.Z-Bounds.BoxExtent.Z;
    const float WorldScale=178.f/FMath::Max(1.f,float(Bounds.BoxExtent.Z*2));
    const FVector Hip=Rig.ReferenceCS[Rig.Bones[0]].GetLocation();
    const float RetargetScale=(Hip.Z-Ground)/Position(SourceRef[0]).Z;
    struct FTrack { TArray<FVector3f> P,S; TArray<FQuat4f> Q; };
    TArray<FTrack> Tracks; Tracks.SetNum(Rig.Reference.Num());
    for(const auto& Frame:Frames)
    {
        const auto& Motion=Frame->AsArray(); Rig.Reset();
        // PMX cloth/control chains can branch above the pelvis. Give the whole
        // rig the fall rotation, then solve mapped joints in component space.
        Rig.Rotate(0,Rotation(Motion[0])*Rotation(SourceRef[0]).Inverse());
        for(int32 I=0;I<47;++I) if(Mapping[I]>=0)
        {
            const int32 B=Mapping[I];
            const FQuat Desired=Rotation(Motion[I])*Rotation(SourceRef[I]).Inverse()*Alignment[I]*Rig.ReferenceCS[B].GetRotation();
            Rig.Rotate(B,Desired*Rig.CS[B].GetRotation().Inverse());
        }
        const FVector Offset=Hip+(Position(Motion[0])-Position(SourceRef[0]))*RetargetScale-Rig.CS[Rig.Bones[0]].GetLocation();
        for(auto& Transform:Rig.CS) Transform.AddToTranslation(Offset);
        float Lift=0;
        for(EBreachBone Contact:{EBreachBone::Pelvis,EBreachBone::Chest,EBreachBone::Head,EBreachBone::LHand,EBreachBone::RHand,EBreachBone::LFoot,EBreachBone::RFoot})
        {
            const float Radius=Contact==EBreachBone::Head?9.f:(Contact==EBreachBone::Pelvis || Contact==EBreachBone::Chest?12.f:3.f);
            Lift=FMath::Max(Lift,float(Ground+Radius/WorldScale-Rig.CS[Rig.Bone(Contact)].GetLocation().Z));
        }
        for(auto& Transform:Rig.CS) Transform.AddToTranslation(FVector(0,0,Lift));
        for(int32 I=0;I<Tracks.Num();++I)
        {
            const FTransform Local=Rig.Parents[I]>=0?Rig.CS[I].GetRelativeTransform(Rig.CS[Rig.Parents[I]]):Rig.CS[I];
            Tracks[I].P.Add(FVector3f(Local.GetLocation())); Tracks[I].Q.Add(FQuat4f(Local.GetRotation())); Tracks[I].S.Add(FVector3f(Local.GetScale3D()));
        }
    }
    UPackage* Package=CreatePackage(*PackageName);
    auto* Sequence=NewObject<UAnimSequence>(Package,FName(*FPackageName::GetLongPackageAssetName(PackageName)),RF_Public|RF_Standalone);
    Sequence->SetSkeleton(Asset->GetSkeleton());
    Sequence->SetPreviewMesh(Asset);
    auto& Controller=Sequence->GetController();
    Controller.InitializeModel();
    Controller.OpenBracket(FText::FromString(TEXT("Retarget Quaternius Death01")),false);
    Controller.SetFrameRate(FFrameRate(int32(Data->GetNumberField(TEXT("fps"))),1),false);
    Controller.SetNumberOfFrames(FFrameNumber(Frames.Num()-1),false);
    for(int32 I=0;I<Tracks.Num();++I)
    {
        const FName Name=Asset->GetRefSkeleton().GetBoneName(I);
        Controller.AddBoneCurve(Name,false);
        Controller.SetBoneTrackKeys(Name,Tracks[I].P,Tracks[I].Q,Tracks[I].S,false);
    }
    Controller.NotifyPopulated(); Controller.CloseBracket(false);
    Sequence->PostEditChange(); Sequence->MarkPackageDirty();
    return Sequence;
#else
    return nullptr;
#endif
}
