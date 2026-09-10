#include "BreachGame.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#endif

UAnimSequence* ABreachGameMode::BakeLocalAnimation(USkeletalMesh* Asset,const FString& MotionFile,const FString& PackageName)
{
#if WITH_EDITOR
    FBreachPose Rig;FString Text;TSharedPtr<FJsonObject> Data;
    if(!Asset || !Asset->GetSkeleton() || !Rig.InitSkeleton(Asset) ||
       !FFileHelper::LoadFileToString(Text,*MotionFile) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data)) return nullptr;
    if(Data->GetStringField(TEXT("coordinate_space"))!=TEXT("PMX_to_UE_X_negZ_Y")) return nullptr;
    const auto& Names=Data->GetArrayField(TEXT("bones"));const auto& Frames=Data->GetArrayField(TEXT("frames"));
    if(Frames.Num()<2) return nullptr;
    auto* Sequence=NewObject<UAnimSequence>(CreatePackage(*PackageName),FName(*FPackageName::GetLongPackageAssetName(PackageName)),RF_Public|RF_Standalone);
    Sequence->SetSkeleton(Asset->GetSkeleton());Sequence->SetPreviewMesh(Asset);
    auto& Controller=Sequence->GetController();Controller.InitializeModel();
    Controller.OpenBracket(FText::FromString(TEXT("Import supplied VMD pose tracks")),false);
    Controller.SetFrameRate(FFrameRate(30,1),false);Controller.SetNumberOfFrames(FFrameNumber(Frames.Num()-1),false);
    for(int32 I=0;I<Names.Num();++I)
    {
        const FName Name(*Names[I]->AsString());const int32 B=Asset->GetRefSkeleton().FindBoneIndex(Name);
        if(B==INDEX_NONE) continue;
        const FQuat Parent=Rig.Parents[B]>=0?Rig.ReferenceCS[Rig.Parents[B]].GetRotation():FQuat::Identity;
        TArray<FVector3f> P,S;TArray<FQuat4f> Q;
        for(const auto& Frame:Frames)
        {
            if(Frame->AsArray().Num()!=Names.Num()) return nullptr;
            const auto& T=Frame->AsArray()[I]->AsArray();if(T.Num()!=7) return nullptr;
            FVector Offset(T[0]->AsNumber(),T[1]->AsNumber(),T[2]->AsNumber());
            FQuat Rotation(T[3]->AsNumber(),T[4]->AsNumber(),T[5]->AsNumber(),T[6]->AsNumber());
            P.Add(FVector3f(Rig.Reference[B].GetLocation()+Parent.UnrotateVector(Offset)));
            Q.Add(FQuat4f((Parent.Inverse()*Rotation.GetNormalized()*Parent*Rig.Reference[B].GetRotation()).GetNormalized()));
            S.Add(FVector3f(Rig.Reference[B].GetScale3D()));
        }
        Controller.AddBoneCurve(Name,false);Controller.SetBoneTrackKeys(Name,P,Q,S,false);
    }
    Controller.NotifyPopulated();Controller.CloseBracket(false);Sequence->PostEditChange();Sequence->MarkPackageDirty();return Sequence;
#else
    return nullptr;
#endif
}
