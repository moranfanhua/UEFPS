#include "BreachSelectionStage.h"
#include "BreachVisuals.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"

namespace
{
    void SetupModel(UPoseableMeshComponent* Mesh,int32 Index)
    {
        auto* Asset=Breach::CharacterMesh(Index);
        Mesh->SetSkinnedAssetAndUpdate(Asset);
        if(!Asset) return;
        for(int32 Slot:{10,11,12,13,14}) Mesh->SetMaterial(Slot,nullptr);
        if(Index==0)
            for(int32 Slot:{10,11,12,13,14}) Mesh->SetMaterial(Slot,Breach::Material(TEXT("M_Eula_CapeCorrect")));
        const auto Bounds=Asset->GetBounds();
        const float Scale=178.f/FMath::Max(1.f,float(Bounds.BoxExtent.Z*2));
        Mesh->SetRelativeScale3D(FVector(Scale));
        Mesh->SetRelativeRotation(FRotator(0,-90,0));
        FVector Position=Mesh->GetRelativeLocation();
        Position.Z=-(Bounds.Origin.Z-Bounds.BoxExtent.Z)*Scale;
        Mesh->SetRelativeLocation(Position);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(true); Mesh->bCastCinematicShadow=true;
        Mesh->SetBoundsScale(3);
    }
    UAnimSequence* Clip(int32 Index,const TCHAR* Name)
    {
        return LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Animations/Entrance/%s/A_%s_%s.A_%s_%s"),Breach::Keys[Index],Breach::Keys[Index],Name,Breach::Keys[Index],Name));
    }
}

ABreachSelectionStage::ABreachSelectionStage()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.bTickEvenWhenPaused=true;
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("StageRoot"));SetRootComponent(Root);
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("SelectionCamera"));Camera->SetupAttachment(Root);
    const FVector Position(315,65,134),Focus(0,0,124);
    Camera->SetRelativeLocation(Position);Camera->SetRelativeRotation((Focus-Position).Rotation());
    Camera->SetFieldOfView(42);Camera->bConstrainAspectRatio=false;
    Camera->SetAspectRatio(16.f/9.f);Camera->bOverrideAspectRatioAxisConstraint=true;
    Camera->SetAspectRatioAxisConstraint(EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV);
    Camera->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Camera->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Camera->PostProcessSettings.bOverride_AutoExposureBias=true;Camera->PostProcessSettings.AutoExposureBias=0;
    Camera->PostProcessSettings.bOverride_MotionBlurAmount=true;Camera->PostProcessSettings.MotionBlurAmount=0;
    Preview=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("SelectionCharacter"));Preview->SetupAttachment(Root);
    Preview->PrimaryComponentTick.bTickEvenWhenPaused=true;
    Cat=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("SelectionCat"));Cat->SetupAttachment(Root);
    Cat->PrimaryComponentTick.bTickEvenWhenPaused=true;
    Cat->SetCollisionEnabled(ECollisionEnabled::NoCollision);Cat->SetVisibility(false);
    Cat->SetCastShadow(true);Cat->SetBoundsScale(3);
    Sword=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("SelectionSword"));Sword->SetupAttachment(Root);
    Scabbard=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("SelectionScabbard"));Scabbard->SetupAttachment(Root);
    for(auto* Prop:{Sword.Get(),Scabbard.Get()})
    {
        Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);Prop->SetVisibility(false);
        Prop->SetCastShadow(true);Prop->SetBoundsScale(3);
    }
}

void ABreachSelectionStage::BeginPlay()
{
    Super::BeginPlay();
    const auto Shape=[this](const TCHAR* Name,FVector Position,FVector Size,const TCHAR* Material,bool Round=false)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(this,Name);Mesh->SetupAttachment(RootComponent);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Round?TEXT("/Engine/BasicShapes/Cylinder.Cylinder"):TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetRelativeLocation(Position);Mesh->SetRelativeScale3D(Size/100);
        Mesh->SetMaterial(0,Breach::Material(Material));Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);Mesh->RegisterComponent();return Mesh;
    };
    Shape(TEXT("StudioFloor"),FVector(0,0,-14),FVector(2400,2400,12),TEXT("M_Gun"));
    Shape(TEXT("Backdrop"),FVector(-230,0,380),FVector(12,2600,1200),TEXT("M_Gun"));
    Shape(TEXT("StageRim"),FVector(0,0,-5),FVector(280,280,6),TEXT("M_Cyan"),true);
    Shape(TEXT("StageSurface"),FVector(0,0,-1),FVector(276,276,4),TEXT("M_Metal"),true);
    const auto Light=[this](FVector Position,FLinearColor Color,float Intensity,float Radius)
    {
        auto* L=NewObject<UPointLightComponent>(this);L->SetupAttachment(RootComponent);
        L->SetRelativeLocation(Position);L->SetLightColor(Color);L->SetIntensity(Intensity);
        L->SetAttenuationRadius(Radius);L->SetSourceRadius(60);L->SetCastShadows(false);L->RegisterComponent();
    };
    Light(FVector(230,-180,300),FLinearColor(1,.92f,.84f),9000,900);
    Light(FVector(130,250,180),FLinearColor(.35f,.65f,1),4500,700);
    Light(FVector(-90,-70,240),FLinearColor(.15f,.75f,1),4500,650);
    Light(FVector(230,0,90),FLinearColor(.82f,.9f,1),2800,500);
    for(int32 I=0;I<4;++I)
    {
        const FVector Origin(0,1800+I*700,0);
        auto* Avatar=NewObject<UPoseableMeshComponent>(this);Avatar->SetupAttachment(RootComponent);
        Avatar->SetRelativeLocation(Origin);SetupModel(Avatar,I);Avatar->SetVisibleInSceneCaptureOnly(true);Avatar->RegisterComponent();PortraitBodies.Add(Avatar);
        FBreachPose Rig;Rig.Init(Breach::CharacterMesh(I),I);Rig.Sample(Clip(I,TEXT("Female_Idle")),1,true);Rig.Apply(Avatar);Avatar->RefreshBoneTransforms();
        Light(Origin+FVector(190,-100,230),FLinearColor(1,.95f,.9f),5000,500);
        Light(Origin+FVector(40,130,170),FLinearColor(.3f,.65f,1),3000,450);
        auto* Target=NewObject<UTextureRenderTarget2D>(this);Target->ClearColor=FLinearColor(.015f,.025f,.04f,1);
        Target->InitCustomFormat(320,200,PF_B8G8R8A8,false);Target->UpdateResourceImmediate(true);PortraitTargets.Add(Target);
        auto* Capture=NewObject<USceneCaptureComponent2D>(this);Capture->SetupAttachment(RootComponent);
        const FVector Eye=Origin+FVector(180,18,149),Look=Origin+FVector(0,0,146);
        Capture->SetRelativeLocation(Eye);Capture->SetRelativeRotation((Look-Eye).Rotation());Capture->FOVAngle=32;
        Capture->TextureTarget=Target;Capture->CaptureSource=ESceneCaptureSource::SCS_FinalColorLDR;
        Capture->bCaptureEveryFrame=false;Capture->bCaptureOnMovement=false;
        Capture->PrimitiveRenderMode=ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
        Capture->ShowOnlyComponent(Avatar);Capture->PostProcessSettings=Camera->PostProcessSettings;
        Capture->RegisterComponent();PortraitCameras.Add(Capture);
    }
    SetupCat();SetupSword();Select(0);
}
void ABreachSelectionStage::Select(int32 Index)
{
    SelectedIndex=FMath::Clamp(Index,0,3);
    Preview->SetRelativeLocation(FVector::ZeroVector);SetupModel(Preview,SelectedIndex);
    bRigReady=Pose.Init(Breach::CharacterMesh(SelectedIndex),SelectedIndex);
    Cloth.Init(Breach::CharacterMesh(SelectedIndex),SelectedIndex);ClothTime=0;
    static const TCHAR* Entrances[]={TEXT("Eula_VMD_Entry"),TEXT("Sword_Attack"),TEXT("Female_Punch"),TEXT("Catwalk_Sequence_03")};
    Entrance=Clip(SelectedIndex,Entrances[SelectedIndex]);
    if(HasCatEntrance()) Entrance=Clip(SelectedIndex,TEXT("Female_Idle"));
    Cat->SetVisibility(bOpen && HasCatEntrance(),true);
    Sword->SetVisibility(bOpen && HasSwordEntrance());Scabbard->SetVisibility(bOpen && HasSwordEntrance());
    static const float EndTimes[]={2.6f,.9f,.933333f,7.f};
    SourceEndTime=Entrance?FMath::Min(EndTimes[SelectedIndex],Entrance->GetPlayLength()):0.f;
    // Blend the supplied Eula motion into the supplied finishing pose.
    FinishPose=SelectedIndex==0?Clip(SelectedIndex,TEXT("Eula_VMD_Finish")):nullptr;
    if(bRigReady)
    {
        const auto Bounds=Preview->GetSkinnedAsset()->GetBounds();
        HeadTopOffset=FMath::Max(12.f,float(Bounds.Origin.Z+Bounds.BoxExtent.Z-Pose.ReferenceCS[Pose.Bone(EBreachBone::Head)].GetLocation().Z)*Preview->GetRelativeScale3D().Z);
    }
    AnimationTime=0;PreviousTime=FPlatformTime::Seconds();
    UpdatePreview();
}

void ABreachSelectionStage::UpdatePreview()
{
    if(!bRigReady) return;
    const float MotionDuration=FinishPose?2.6f:EntranceDuration;
    const float Progress=FMath::SmoothStep(0.f,1.f,FMath::Clamp(AnimationTime/MotionDuration,0.f,1.f));
    float SourceTime=SourceEndTime*Progress;
    // Catwalk: the final approach, turn and hand-on-hip presentation. Leave out the walk away.
    if(SelectedIndex==3) SourceTime=FMath::Lerp(3.5f,SourceEndTime,Progress);
    // Give the wind-up time to read, keep the slash brisk, then settle into the follow-through.
    if(SelectedIndex==1)
        SourceTime=AnimationTime<1.35f?FMath::Lerp(0.f,.32f,AnimationTime/1.35f):
            (AnimationTime<1.7f?FMath::Lerp(.32f,.6f,(AnimationTime-1.35f)/.35f):
             FMath::Lerp(.6f,SourceEndTime,FMath::SmoothStep(1.7f,3.5f,AnimationTime)));
    if(!Pose.Sample(Entrance,HasCatEntrance()?1.f:SourceTime,false)) Pose.Walk(0,0);
    if(FinishPose && AnimationTime>MotionDuration)
    {
        const auto Entry=Pose.Local;
        const auto EntryMorphs=Pose.MorphWeights;
        if(Pose.Sample(FinishPose,0,false))
        {
            const float Blend=FMath::SmoothStep(0.f,1.f,FMath::Clamp((AnimationTime-MotionDuration)/(EntranceDuration-MotionDuration),0.f,1.f));
            for(int32 I=0;I<Entry.Num();++I) { FTransform Mixed;Mixed.Blend(Entry[I],Pose.Local[I],Blend);Pose.Local[I]=Mixed; }
            for(const FName Name:Pose.MorphNames) Pose.MorphWeights.FindOrAdd(Name)=FMath::Lerp(EntryMorphs.FindRef(Name),Pose.MorphWeights.FindRef(Name),Blend);
            Pose.Rebuild();
        }
    }
    if(HasCatEntrance()) UpdateCatEntrance();
    // Face the catwalk's stopping mark toward the camera while retaining its turn.
    Preview->SetRelativeRotation(FRotator(0,SelectedIndex==3?0.f:-90.f,0));
    if(HasSwordEntrance()) UpdateSwordEntrance();
    Cloth.Update(Pose,Preview->GetComponentTransform(),FMath::Max(0.f,AnimationTime-ClothTime),GetWorld(),this);ClothTime=AnimationTime;
    Pose.Apply(Preview);Preview->RefreshBoneTransforms();

    // Frame the waist, face and raised hands in the space above the cards.
    // Fit in stage coordinates so jumps keep the upper body inside the shot.
    const FTransform ToStage=Preview->GetRelativeTransform();
    const auto Point=[&](EBreachBone Bone) { return ToStage.TransformPosition(Pose.CS[Pose.Bone(Bone)].GetLocation()); };
    const FVector Head=Point(EBreachBone::Head),Pelvis=Point(EBreachBone::Pelvis);
    const FVector Left=Point(EBreachBone::LHand),Right=Point(EBreachBone::RHand);
    const float Top=FMath::Max(float(Head.Z)+HeadTopOffset,FMath::Max(float(Left.Z),float(Right.Z))+8.f)+5.f;
    const float Bottom=float(Pelvis.Z);
    const float Height=FMath::Max(85.f,Top-Bottom);
    const float ViewHeight=Height/.70f;
    const float Distance=ViewHeight*(16.f/9.f)/(2.f*FMath::Tan(FMath::DegreesToRadians(Camera->FieldOfView*.5f)));
    const FVector Focus((Head.X+Pelvis.X)*.5,(Head.Y+Pelvis.Y)*.5,(Top+Bottom)*.5-ViewHeight*.065f);
    const FVector Eye=Focus+FVector(1,.206f,0).GetSafeNormal()*Distance;
    Camera->SetRelativeLocation(Eye);Camera->SetRelativeRotation((Focus-Eye).Rotation());
}
void ABreachSelectionStage::SetOpen(bool Open)
{
    bOpen=Open;Preview->SetVisibility(Open);
    Cat->SetVisibility(Open && HasCatEntrance(),true);
    Sword->SetVisibility(Open && HasSwordEntrance());Scabbard->SetVisibility(Open && HasSwordEntrance());
    // Finish the one-time portrait captures even if the player immediately
    // leaves selection; the gameplay HUD reuses these cached textures.
    SetActorTickEnabled(Open || WarmupFrames<24);
    PreviousTime=FPlatformTime::Seconds();
}
void ABreachSelectionStage::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(bOpen)
    {
        const double Now=FPlatformTime::Seconds();
        const bool WasHolding=IsHoldingEntrance();
        AnimationTime=FMath::Min(EntranceDuration,AnimationTime+FMath::Max(0.f,float(Now-PreviousTime)));PreviousTime=Now;
        if(!WasHolding) UpdatePreview();
    }
    if(WarmupFrames>=24) return;
    ++WarmupFrames;
    if(WarmupFrames==7 || WarmupFrames==23)
        for(int32 I=0;I<4;++I)
        {
            FBreachPose Rig;Rig.Init(Breach::CharacterMesh(I),I);Rig.Sample(Clip(I,TEXT("Female_Idle")),1,true);
            Rig.Apply(PortraitBodies[I]);PortraitBodies[I]->RefreshBoneTransforms();
        }
    if(WarmupFrames==8 || WarmupFrames==24)
        for(const auto& Capture:PortraitCameras) Capture->CaptureScene();
    if(WarmupFrames==24 && !bOpen) SetActorTickEnabled(false);
}
UTextureRenderTarget2D* ABreachSelectionStage::Portrait(int32 Index) const
{
    return PortraitTargets.IsValidIndex(Index)?PortraitTargets[Index].Get():nullptr;
}
