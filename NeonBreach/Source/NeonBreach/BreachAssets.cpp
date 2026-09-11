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

bool ABreachGameMode::CopyCharacterGeometry(USkeletalMesh* Target,USkeletalMesh* Source)
{
#if WITH_EDITOR
    if(!Target || !Source || Target==Source || !Target->GetSkeleton()) return false;
    // Geometry revisions must use the exact existing bind pose. Never replace the
    // skeleton: locomotion, entrance clips and both player meshes reference it.
    const FReferenceSkeleton& A=Target->GetRefSkeleton();
    const FReferenceSkeleton& B=Source->GetRefSkeleton();
    if(A.GetRawBoneNum()!=B.GetRawBoneNum()) return false;
    for(int32 I=0;I<A.GetRawBoneNum();++I)
        if(A.GetBoneName(I)!=B.GetBoneName(I) || A.GetParentIndex(I)!=B.GetParentIndex(I) ||
           !A.GetRawRefBonePose()[I].Equals(B.GetRawRefBonePose()[I],.001f)) return false;
    const FMeshDescription* Description=Source->GetMeshDescription(0);
    if(!Description || Description->Triangles().Num()==0 || Source->GetMaterials().IsEmpty()) return false;
    for(const FSkeletalMaterial& Slot:Source->GetMaterials()) if(!Slot.MaterialInterface) return false;
    FMeshDescription Copy(*Description);
    Target->Modify();
    Target->CreateMeshDescription(0,MoveTemp(Copy));
    Target->SetMaterials(Source->GetMaterials());
    Target->CommitMeshDescription(0);
    // Retain the original bounds used to normalize character height and camera offsets.
    Target->PostEditChange();
    Target->MarkPackageDirty();
    return true;
#else
    return false;
#endif
}

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
    return BakeCharacterAnimation(Asset,ModelIndex,MotionFile,PackageName);
}

UAnimSequence* ABreachGameMode::BakeCharacterAnimation(USkeletalMesh* Asset,int32 ModelIndex,const FString& MotionFile,const FString& PackageName)
{
#if WITH_EDITOR
    FBreachPose Rig;
    if(!Asset || !Rig.Init(Asset,ModelIndex)) return nullptr;
    FString Text; TSharedPtr<FJsonObject> Data;
    if(!FFileHelper::LoadFileToString(Text,*MotionFile) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data)) return nullptr;
    FString CoordinateSpace;
    if(!Data->TryGetStringField(TEXT("coordinate_space"),CoordinateSpace) || CoordinateSpace!=TEXT("UE_Interchange_XZY"))
    {
        UE_LOG(LogTemp,Error,TEXT("Death source uses an unverified coordinate basis. Run Tools/prepare_death_animation.py again."));
        return nullptr;
    }
    const auto& Frames=Data->GetArrayField(TEXT("frames"));
    FString MotionKind=TEXT("death"); Data->TryGetStringField(TEXT("motion_kind"),MotionKind);
    const bool bDeath=MotionKind==TEXT("death"), bAirborne=MotionKind==TEXT("airborne"), bSlide=MotionKind==TEXT("slide");
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
        // Match limb proportions / A-pose arms. Spine and pelvis positions
        // are rig-specific offsets, not orientation axes; aligning those
        // offsets tilts the hips independently of the feet in the first frame.
        if(I>=5 && Child[I]>=0 && Mapping[I]>=0 && Mapping[Child[I]]>=0)
            Correction=FQuat::FindBetweenNormals((Rig.ReferenceCS[Mapping[Child[I]]].GetLocation()-Rig.ReferenceCS[Mapping[I]].GetLocation()).GetSafeNormal(),(Position(SourceRef[Child[I]])-Position(SourceRef[I])).GetSafeNormal());
        Alignment.Add(Correction);
    }
    const auto Bounds=Asset->GetBounds();
    const float Ground=Bounds.Origin.Z-Bounds.BoxExtent.Z;
    const float WorldScale=178.f/FMath::Max(1.f,float(Bounds.BoxExtent.Z*2));
    const FVector Hip=Rig.ReferenceCS[Rig.Bones[0]].GetLocation();
    const float RetargetScale=(Hip.Z-Ground)/Position(SourceRef[0]).Z;
    // A sliding shoe is tilted sideways; its standing ankle height is no longer
    // a valid floor contact. Sample skinned lower legs / hands during baking so
    // each costume rests on its actual surface without runtime mesh queries.
    struct FContactWeight { int32 Bone; float Weight; FVector Position; };
    struct FContactVertex { TArray<FContactWeight> Weights; };
    TArray<FContactVertex> SlideContacts;
    if(FMeshDescription* Description=bSlide?Asset->GetMeshDescription(0):nullptr)
    {
        FSkeletalMeshAttributes Attributes(*Description);
        const auto Positions=Attributes.GetVertexPositions();
        const auto Weights=Attributes.GetVertexSkinWeights();
        for(const FVertexID Vertex:Description->Vertices().GetElementIDs())
        {
            float ContactWeight=0;
            for(const auto W:Weights.Get(Vertex))
                for(const EBreachBone Joint:{EBreachBone::LKnee,EBreachBone::RKnee,EBreachBone::LHand,EBreachBone::RHand})
                    if(Rig.IsUnder(W.GetBoneIndex(),Rig.Bone(Joint))) { ContactWeight+=W.GetWeight(); break; }
            if(ContactWeight<.5f) continue;
            FContactVertex Contact;
            for(const auto W:Weights.Get(Vertex))
                Contact.Weights.Add({int32(W.GetBoneIndex()),W.GetWeight(),Rig.ReferenceCS[W.GetBoneIndex()].InverseTransformPosition(FVector(Positions[Vertex]))});
            SlideContacts.Add(MoveTemp(Contact));
        }
        UE_LOG(LogTemp,Display,TEXT("SLIDE_CONTACTS %s vertices=%d"),*Asset->GetName(),SlideContacts.Num());
    }
    struct FTrack { TArray<FVector3f> P,S; TArray<FQuat4f> Q; };
    TArray<FTrack> Tracks; Tracks.SetNum(Rig.Reference.Num());
    for(const auto& Frame:Frames)
    {
        const auto& Motion=Frame->AsArray(); Rig.Reset();
        // PMX cloth/control chains can branch above the pelvis. Give the whole
        // rig the fall rotation, then solve mapped joints in component space.
        for(int32 I=0;I<Rig.Parents.Num();++I)
            if(Rig.Parents[I]<0) Rig.Rotate(I,Rotation(Motion[0])*Rotation(SourceRef[0]).Inverse());
        for(int32 I=0;I<47;++I) if(Mapping[I]>=0)
        {
            const int32 B=Mapping[I];
            const FQuat Desired=Rotation(Motion[I])*Rotation(SourceRef[I]).Inverse()*Alignment[I]*Rig.ReferenceCS[B].GetRotation();
            Rig.Rotate(B,Desired*Rig.CS[B].GetRotation().Inverse());
        }
        FVector HipOffset=(Position(Motion[0])-Position(SourceRef[0]))*RetargetScale;
        if(!bDeath) { HipOffset.X=0; HipOffset.Y=0; }
        // The movement component owns airborne displacement; retain the limb
        // pose without applying the source jump trajectory a second time.
        if(bAirborne) HipOffset.Z=0;
        const FVector Offset=Hip+HipOffset-Rig.CS[Rig.Bones[0]].GetLocation();
        for(auto& Transform:Rig.CS) Transform.AddToTranslation(Offset);
        float Lift=0;
        for(EBreachBone Contact:{EBreachBone::Pelvis,EBreachBone::Chest,EBreachBone::Head,EBreachBone::LHand,EBreachBone::RHand,EBreachBone::LFoot,EBreachBone::RFoot})
        {
            if(bAirborne || !SlideContacts.IsEmpty() || (!bDeath && Contact!=EBreachBone::LFoot && Contact!=EBreachBone::RFoot)) continue;
            const float Radius=Contact==EBreachBone::Head?9.f:(Contact==EBreachBone::Pelvis || Contact==EBreachBone::Chest?12.f:3.f);
            const float ContactHeight=bDeath?Ground+Radius/WorldScale:Rig.ReferenceCS[Rig.Bone(Contact)].GetLocation().Z;
            Lift=FMath::Max(Lift,float(ContactHeight-Rig.CS[Rig.Bone(Contact)].GetLocation().Z));
        }
        if(!SlideContacts.IsEmpty())
        {
            float Lowest=BIG_NUMBER;
            for(const auto& Vertex:SlideContacts)
            {
                FVector Skinned=FVector::ZeroVector;
                for(const auto& W:Vertex.Weights) Skinned+=Rig.CS[W.Bone].TransformPosition(W.Position)*W.Weight;
                Lowest=FMath::Min(Lowest,float(Skinned.Z));
            }
            Lift=Ground-Lowest;
        }
        for(auto& Transform:Rig.CS) Transform.AddToTranslation(FVector(0,0,Lift));
        for(int32 I=0;I<Tracks.Num();++I)
        {
            const FTransform Local=Rig.Parents[I]>=0?Rig.CS[I].GetRelativeTransform(Rig.CS[Rig.Parents[I]]):Rig.CS[I];
            Tracks[I].P.Add(FVector3f(Local.GetLocation())); Tracks[I].Q.Add(FQuat4f(Local.GetRotation())); Tracks[I].S.Add(FVector3f(Local.GetScale3D()));
        }
    }
    UPackage* Package=CreatePackage(*PackageName);
    if(FMeshDescription* Description=bDeath?Asset->GetMeshDescription(0):nullptr)
    {
        FSkeletalMeshAttributes Attributes(*Description);
        const auto Positions=Attributes.GetVertexPositions();
        const auto Weights=Attributes.GetVertexSkinWeights();
        float Highest=-BIG_NUMBER; FString Influences;
        for(FVertexID Vertex:Description->Vertices().GetElementIDs())
        {
            FVector Skinned=FVector::ZeroVector;
            for(const auto W:Weights.Get(Vertex))
                Skinned+=Rig.CS[W.GetBoneIndex()].TransformPosition(Rig.ReferenceCS[W.GetBoneIndex()].InverseTransformPosition(FVector(Positions[Vertex])))*W.GetWeight();
            if(Skinned.Z>Highest)
            {
                Highest=Skinned.Z; Influences.Empty();
                for(const auto W:Weights.Get(Vertex))
                    Influences+=FString::Printf(TEXT(" %s:%.2f"),*Asset->GetRefSkeleton().GetBoneName(W.GetBoneIndex()).ToString(),W.GetWeight());
            }
        }
        UE_LOG(LogTemp,Display,TEXT("DEATH_SKIN_AUDIT %s highest=%.2fcm weights:%s"),*Asset->GetName(),(Highest-Ground)*WorldScale,*Influences);
    }
    auto* Sequence=NewObject<UAnimSequence>(Package,FName(*FPackageName::GetLongPackageAssetName(PackageName)),RF_Public|RF_Standalone);
    Sequence->SetSkeleton(Asset->GetSkeleton());
    Sequence->SetPreviewMesh(Asset);
    auto& Controller=Sequence->GetController();
    Controller.InitializeModel();
    Controller.OpenBracket(FText::FromString(TEXT("Retarget character animation")),false);
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
