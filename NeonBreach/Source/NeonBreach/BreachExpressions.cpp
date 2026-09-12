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
#include "Materials/Material.h"
#endif

bool ABreachGameMode::ImportVMDExpressions(USkeletalMesh* Asset,const FString& SourceFile)
{
#if WITH_EDITOR
    FString Text;TSharedPtr<FJsonObject> Data;
    if(!Asset || !Asset->GetMeshDescription(0) || !FFileHelper::LoadFileToString(Text,*SourceFile) ||
       !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data)) return false;
    // Work on a copy. Existing positions, UVs, weights, material slots and skeleton stay intact.
    FMeshDescription Copy(*Asset->GetMeshDescription(0));FSkeletalMeshAttributes Attr(Copy);
    const auto Positions=Attr.GetVertexPositions();const auto UVs=Attr.GetVertexInstanceUVs();
    const auto& Vertices=Data->GetArrayField(TEXT("vertices"));
    TArray<FVector> SourcePositions;TArray<FVector2f> SourceUVs;TMap<FIntVector,TArray<int32>> Grid;
    const auto Cell=[](const FVector& P) { return FIntVector(FMath::RoundToInt(P.X*100),FMath::RoundToInt(P.Y*100),FMath::RoundToInt(P.Z*100)); };
    for(const auto& Vertex:Vertices)
    {
        const auto& V=Vertex->AsArray();const int32 I=SourcePositions.Add(FVector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber()));
        SourceUVs.Add(FVector2f(V[3]->AsNumber(),V[4]->AsNumber()));Grid.FindOrAdd(Cell(SourcePositions[I])).Add(I);
    }
    TMap<int32,TArray<FVertexID>> Mapping;
    for(const FVertexID V:Copy.Vertices().GetElementIDs())
    {
        const FVector P(Positions[V]);const FIntVector C=Cell(P);int32 Best=INDEX_NONE;double Score=1.e20;
        const auto Instances=Copy.GetVertexVertexInstanceIDs(V);
        const FVector2f UV=Instances.Num()?UVs.Get(Instances[0],0):FVector2f::ZeroVector;
        for(int32 X=-1;X<=1;++X) for(int32 Y=-1;Y<=1;++Y) for(int32 Z=-1;Z<=1;++Z)
            if(const auto* Candidates=Grid.Find(C+FIntVector(X,Y,Z))) for(const int32 I:*Candidates)
            {
                const double Distance=FVector::DistSquared(P,SourcePositions[I]);
                const double S=Distance+FVector2f::DistSquared(UV,SourceUVs[I])*.001;
                if(Distance<.0004 && S<Score) { Score=S;Best=I; }
            }
        if(Best!=INDEX_NONE) Mapping.FindOrAdd(Best).Add(V);
    }
    // PMX seams can contain identical position/UV vertices which UE welds.
    TMap<int32,TArray<FVertexID>> SeamMapping;
    for(int32 I=0;I<SourcePositions.Num();++I) if(!Mapping.Contains(I))
        if(const auto* Candidates=Grid.Find(Cell(SourcePositions[I]))) for(const int32 C:*Candidates)
            if(FVector::DistSquared(SourcePositions[I],SourcePositions[C])<1.e-8 &&
               FVector2f::DistSquared(SourceUVs[I],SourceUVs[C])<1.e-8)
                if(const auto* Targets=Mapping.Find(C)) { SeamMapping.Add(I,*Targets);break; }
    Mapping.Append(SeamMapping);
    int32 Total=0,Matched=0;
    for(const auto& Value:Data->GetArrayField(TEXT("morphs")))
    {
        const auto Morph=Value->AsObject();const FName Name(*Morph->GetStringField(TEXT("name")));
        Attr.RegisterMorphTargetAttribute(Name,false);auto Deltas=Attr.GetVertexMorphPositionDelta(Name);
        for(const FVertexID V:Copy.Vertices().GetElementIDs()) Deltas[V]=FVector3f::ZeroVector;
        for(const auto& ValueDelta:Morph->GetArrayField(TEXT("deltas")))
        {
            const auto& D=ValueDelta->AsArray();++Total;
            if(const auto* Targets=Mapping.Find(int32(D[0]->AsNumber())))
            {
                ++Matched;for(const FVertexID V:*Targets) Deltas[V]=FVector3f(D[1]->AsNumber(),D[2]->AsNumber(),D[3]->AsNumber());
            }
        }
    }
    UE_LOG(LogTemp,Display,TEXT("VMD_MORPHS mapped=%d/%d meshVertices=%d sourceVertices=%d"),Matched,Total,Copy.Vertices().Num(),Vertices.Num());
    if(Total==0 || Matched<float(Total)*.98f) return false;
    Asset->Modify();Asset->CreateMeshDescription(0,MoveTemp(Copy));Asset->CommitMeshDescription(0);Asset->PostEditChange();Asset->MarkPackageDirty();
    for(const auto& Slot:Asset->GetMaterials()) if(Slot.MaterialInterface)
        if(auto* Material=Slot.MaterialInterface->GetMaterial()) { bool Recompile=false;Material->SetMaterialUsage(Recompile,MATUSAGE_MorphTargets); }
    for(const auto& Value:Data->GetArrayField(TEXT("clips")))
    {
        const auto Clip=Value->AsObject();const FString Name=Clip->GetStringField(TEXT("name"));
        const FString Path=FString::Printf(TEXT("/Game/Animations/Entrance/Eula/A_Eula_%s.A_Eula_%s"),*Name,*Name);
        auto* Animation=LoadObject<UAnimSequence>(nullptr,*Path);if(!Animation) return false;
        auto& Controller=Animation->GetController();Controller.OpenBracket(FText::FromString(TEXT("VMD expressions")),false);
        for(const auto& Track:Clip->GetObjectField(TEXT("tracks"))->Values)
        {
            if(!Asset->FindMorphTarget(FName(*Track.Key))) continue;
            FRichCurve Source;
            for(const auto& ValueKey:Track.Value->AsArray())
            {
                const auto& K=ValueKey->AsArray();const auto Handle=Source.AddKey(K[0]->AsNumber(),K[1]->AsNumber());Source.SetKeyInterpMode(Handle,RCIM_Linear);
            }
            TArray<FRichCurveKey> Keys;Keys.Add(FRichCurveKey(0,Source.Eval(0)));
            for(const auto& K:Source.GetConstRefOfKeys()) if(K.Time>0 && K.Time<Animation->GetPlayLength()) Keys.Add(K);
            Keys.Add(FRichCurveKey(Animation->GetPlayLength(),Source.Eval(Animation->GetPlayLength())));
            for(auto& K:Keys) K.InterpMode=RCIM_Linear;
            const FAnimationCurveIdentifier Id(FName(*Track.Key),ERawCurveTrackTypes::RCT_Float);
            Controller.AddCurve(Id,4,false);Controller.SetCurveKeys(Id,Keys,false);
        }
        Controller.CloseBracket(false);Animation->PostEditChange();Animation->MarkPackageDirty();
    }
    return true;
#else
    return false;
#endif
}
