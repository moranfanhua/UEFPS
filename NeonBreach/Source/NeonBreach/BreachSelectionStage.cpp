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
    const FVector Position(480,120,160),Focus(0,0,85);
    Camera->SetRelativeLocation(Position);Camera->SetRelativeRotation((Focus-Position).Rotation());
    Camera->SetFieldOfView(42);Camera->bConstrainAspectRatio=false;
    Camera->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Camera->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Camera->PostProcessSettings.bOverride_AutoExposureBias=true;Camera->PostProcessSettings.AutoExposureBias=0;
    Camera->PostProcessSettings.bOverride_MotionBlurAmount=true;Camera->PostProcessSettings.MotionBlurAmount=0;
    Preview=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("SelectionCharacter"));Preview->SetupAttachment(Root);
    Preview->PrimaryComponentTick.bTickEvenWhenPaused=true;
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
    Select(0);
}
void ABreachSelectionStage::Select(int32 Index)
{
    SelectedIndex=FMath::Clamp(Index,0,3);
    Preview->SetRelativeLocation(FVector::ZeroVector);SetupModel(Preview,SelectedIndex);
    Pose.Init(Breach::CharacterMesh(SelectedIndex),SelectedIndex);
    static const TCHAR* Entrances[]={TEXT("Female_Standing"),TEXT("Female_Clapping"),TEXT("Female_Punch"),TEXT("Female_Jump")};
    Entrance=Clip(SelectedIndex,Entrances[SelectedIndex]);Idle=Clip(SelectedIndex,TEXT("Female_Idle"));
    AnimationTime=0;PreviousTime=FPlatformTime::Seconds();
    Pose.Sample(Entrance,0,false);Pose.Apply(Preview);Preview->RefreshBoneTransforms();
}
void ABreachSelectionStage::SetOpen(bool Open)
{
    bOpen=Open;Preview->SetVisibility(Open);
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
    const double Now=FPlatformTime::Seconds();AnimationTime+=FMath::Min(float(Now-PreviousTime),.1f);PreviousTime=Now;
    const float Duration=Entrance?Entrance->GetPlayLength()/.75f:0;
    if(AnimationTime<Duration) Pose.Sample(Entrance,AnimationTime*.75f,false);
    else
    {
        Pose.Sample(Entrance,Entrance?Entrance->GetPlayLength():0,false);
        const auto End=Pose.Local;
        Pose.Sample(Idle,AnimationTime-Duration,true);
        const float Blend=FMath::Clamp((AnimationTime-Duration)/.35f,0.f,1.f);
        if(Blend<1 && End.Num()==Pose.Local.Num())
        {
            for(int32 I=0;I<End.Num();++I) { FTransform Mixed;Mixed.Blend(End[I],Pose.Local[I],Blend);Pose.Local[I]=Mixed; }
            Pose.Rebuild();
        }
    }
    Pose.Apply(Preview);Preview->RefreshBoneTransforms();
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
