#include "BreachGame.h"
#include "Engine/SkeletalMesh.h"
#if WITH_EDITOR
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
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
