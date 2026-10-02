#include "BreachGame.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "Animation/IAnimationSequenceCompiler.h"
#endif

// This command-line review scene samples assets only. It does not spawn enemy AI
// or change the arena, enemy selection, navigation, damage or combat state.
void ABreachGameMode::RunSeabornReview()
{
    FString Key;
    FParse::Value(FCommandLine::Get(),TEXT("BreachReviewEnemy="),Key);
    const TArray<FString> Allowed={TEXT("SeaDrifter"),TEXT("DeepSeaSlider"),TEXT("SpinalSeaSpitter"),TEXT("BowlSeaReaper"),TEXT("FirstSeaPiercer")};
    const FString Folder=TEXT("/Game/Enemies/Seaborn/")+Key+TEXT("/Rig/");
    auto* Asset=Allowed.Contains(Key)?LoadObject<USkeletalMesh>(nullptr,*(Folder+TEXT("SK_")+Key+TEXT(".SK_")+Key)):nullptr;
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!Asset || !PC)
    {
        UE_LOG(LogTemp,Error,TEXT("SEABORN_REVIEW: invalid enemy key or missing skeletal mesh"));
        FPlatformMisc::RequestExitWithStatus(true,1);
        return;
    }
    bGallery=true;
    if(PC->GetPawn()) PC->GetPawn()->SetActorHiddenInGame(true);
    if(PC->GetHUD()) PC->GetHUD()->bShowHUD=false;
    const FVector Origin(0,0,10000);
    auto* Stage=GetWorld()->SpawnActor<AActor>();
    auto* Mesh=NewObject<UPoseableMeshComponent>(Stage);
    Stage->SetRootComponent(Mesh);
    Mesh->SetSkinnedAssetAndUpdate(Asset);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->RegisterComponent();
    Mesh->SetWorldTransform(FTransform(FRotator(0,-90,0),Origin));
    Mesh->SetCastShadow(true);
    auto* Floor=GetWorld()->SpawnActor<AActor>();
    auto* Surface=NewObject<UStaticMeshComponent>(Floor);
    Floor->SetRootComponent(Surface);
    Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Surface->RegisterComponent();
    Surface->SetWorldTransform(FTransform(FRotator::ZeroRotator,Origin-FVector(0,0,5),FVector(30,30,.1)));
    const FVector Focus=Origin+FVector(0,0,135);
    for(const FVector Offset:{FVector(250,-350,300),FVector(200,350,180),FVector(-300,80,240)})
    {
        auto* Light=NewObject<UPointLightComponent>(Stage);
        Light->SetIntensity(12000);
        Light->SetAttenuationRadius(1800);
        Light->SetSourceRadius(90);
        Light->RegisterComponent();
        Light->SetWorldLocation(Focus+Offset);
    }
    auto* Camera=GetWorld()->SpawnActor<ACameraActor>();
    Camera->GetCameraComponent()->SetFieldOfView(48);
    Camera->GetCameraComponent()->bConstrainAspectRatio=false;
    auto& Settings=Camera->GetCameraComponent()->PostProcessSettings;
    Settings.bOverride_AutoExposureMethod=true;
    Settings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Settings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
    Settings.AutoExposureApplyPhysicalCameraExposure=false;
    Settings.bOverride_MotionBlurAmount=true;
    Settings.MotionBlurAmount=0;
    PC->bAutoManageActiveCameraTarget=false;
    PC->SetViewTarget(Camera);
    auto Pose=MakeShared<FBreachPose>();
    Pose->InitSkeleton(Asset);
    FString Report;
    int32 Failures=0;
    const auto Check=[&](bool Pass,const FString& Message)
    {
        Report+=FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),*Message);
        if(!Pass) ++Failures;
    };
    Check(Pose->Reference.Num()>10,TEXT("Skeletal mesh has the authored deformation bones"));
    const int32 Root=Asset->GetRefSkeleton().FindBoneIndex(TEXT("root"));
    Check(Root!=INDEX_NONE,TEXT("Stationary root exists"));
    auto Clips=MakeShared<TArray<TStrongObjectPtr<UAnimSequence>>>();
    TArray<FString> Names={TEXT("Idle"),TEXT("Move"),TEXT("Attack"),TEXT("Die")};
    if(Key==TEXT("BowlSeaReaper")) Names={TEXT("Idle"),TEXT("Move"),TEXT("Skill_Begin"),TEXT("Skill_Move"),TEXT("Skill_Attack"),TEXT("Die")};
    const bool Capture=FParse::Param(FCommandLine::Get(),TEXT("BreachSeabornCapture"));
    const FString Output=FPaths::ProjectDir()/TEXT("Saved/SeabornActions")/Key;
    IFileManager::Get().MakeDirectory(*(Output/TEXT("RuntimeCaptures")),true);
    int32 Shot=0;
    for(const FString& Name:Names)
    {
        auto* Clip=LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("A_")+Key+TEXT("_")+Name+TEXT(".A_")+Key+TEXT("_")+Name));
        Check(Clip && Clip->GetSkeleton()==Asset->GetSkeleton(),Name+TEXT(" uses the mesh skeleton"));
        if(!Clip) continue;
        Clips->Emplace(Clip);
#if WITH_EDITOR
        // UE editor loads/compresses animations asynchronously. A sample before
        // completion can contain the reference pose, invalidating the seam test.
        TArray<UAnimSequence*> Pending={Clip};
        UE::Anim::IAnimSequenceCompilingManager::FinishCompilation(Pending);
#endif
        Check(Clip->IsBoneCompressedDataValid(),Name+TEXT(" compressed data is ready"));
        Report+=FString::Printf(TEXT("%s duration=%.6f rate=%.3f keys=%d\n"),*Name,Clip->GetPlayLength(),Clip->GetSamplingFrameRate().AsDecimal(),Clip->GetNumberOfSampledKeys());
        bool Sampled=true,FixedRoot=true,FixedScale=true,Changed=false;
        Pose->Sample(Clip,0,false);
        const TArray<FTransform> First=Pose->CS;
        for(int32 Frame=0;Frame<=60;++Frame)
        {
            Sampled&=Pose->Sample(Clip,Clip->GetPlayLength()*Frame/60.f,false);
            if(Root!=INDEX_NONE) FixedRoot&=Pose->CS[Root].Equals(Pose->ReferenceCS[Root],.02f);
            for(int32 Bone=0;Bone<Pose->CS.Num();++Bone)
            {
                FixedScale&=Pose->CS[Bone].GetScale3D().Equals(Pose->ReferenceCS[Bone].GetScale3D(),.001f);
                Changed|=!Pose->CS[Bone].Equals(First[Bone],.05f);
            }
        }
        Check(Sampled && FixedRoot && FixedScale && Changed,Name+TEXT(" samples, moves, preserves scale and stationary root"));
        if(Name==TEXT("Idle") || Name==TEXT("Move") || Name==TEXT("Skill_Move"))
        {
            bool Seam=true;
            float PositionError=0.f,AngleError=0.f;
            for(int32 Bone=0;Bone<Pose->CS.Num();++Bone)
            {
                PositionError=FMath::Max(PositionError,float(FVector::Distance(Pose->CS[Bone].GetLocation(),First[Bone].GetLocation())));
                AngleError=FMath::Max(AngleError,float(Pose->CS[Bone].GetRotation().AngularDistance(First[Bone].GetRotation())));
                Seam&=FVector::Distance(Pose->CS[Bone].GetLocation(),First[Bone].GetLocation())<.25f &&
                    Pose->CS[Bone].GetRotation().AngularDistance(First[Bone].GetRotation())<.002f;
            }
            Report+=FString::Printf(TEXT("%s seam position_cm=%.6f angle_rad=%.6f\n"),*Name,PositionError,AngleError);
            if(!Seam)
            {
                for(int32 Bone=0;Bone<Pose->CS.Num();++Bone)
                    if(!Pose->CS[Bone].Equals(First[Bone],.05f))
                        Report+=FString::Printf(TEXT("seam %s start=%s end=%s\n"),*Asset->GetRefSkeleton().GetBoneName(Bone).ToString(),*First[Bone].ToHumanReadableString(),*Pose->CS[Bone].ToHumanReadableString());
            }
            Check(Seam,Name+TEXT(" loop joins continuously"));
        }
        if(Name==TEXT("Die"))
        {
            const auto Last=Pose->CS;
            Pose->Sample(Clip,Clip->GetPlayLength()+10,false);
            bool Held=true;
            for(int32 Bone=0;Bone<Last.Num();++Bone) Held&=Pose->CS[Bone].Equals(Last[Bone],.001f);
            Check(Held,TEXT("Death holds its last pose"));
        }
        if(!Capture) continue;
        for(int32 Frame=0;Frame<5;++Frame)
        {
            const float Fraction=Frame<4?Frame/3.f:.5f;
            const float At=1.f+Shot*.55f;
            const FString File=Output/TEXT("RuntimeCaptures")/(Name+FString::Printf(TEXT("_%d.png"),Frame));
            const float Distance=FMath::Max(620.f,float(Asset->GetBounds().BoxExtent.Size()*3.6));
            const FVector Position=Frame==4?Origin+FVector(460,0,165):Focus+FVector(Distance*.75,-Distance*.65,Distance*.2);
            FTimerHandle Setup,Screenshot;
            GetWorldTimerManager().SetTimer(Setup,[Pose,Clips,Mesh,Clip,Camera,Position,Focus,Fraction,Frame]()
            {
                Pose->Sample(Clip,Clip->GetPlayLength()*Fraction,false);
                Pose->Apply(Mesh);
                Camera->SetActorLocationAndRotation(Position,(Focus-Position).Rotation());
                Camera->GetCameraComponent()->SetFieldOfView(Frame==4?76.f:48.f);
            },At,false);
            GetWorldTimerManager().SetTimer(Screenshot,[File]() { FScreenshotRequest::RequestScreenshot(File,false,false); },At+.3f,false);
            ++Shot;
        }
    }
    Report+=FString::Printf(TEXT("FAILURES=%d\n"),Failures);
    FFileHelper::SaveStringToFile(Report,*(Output/TEXT("runtime_test.txt")));
    UE_LOG(LogTemp,Display,TEXT("SEABORN_REVIEW %s\n%s"),*Key,*Report);
    if(!Capture) { FPlatformMisc::RequestExitWithStatus(false,Failures?1:0); return; }
    FTimerHandle Exit;
    GetWorldTimerManager().SetTimer(Exit,[Clips,Failures]() { FPlatformMisc::RequestExitWithStatus(false,Failures?1:0); },2.f+Shot*.55f,false);
}
