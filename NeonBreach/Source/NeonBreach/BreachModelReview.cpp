#include "BreachGame.h"
#include "BreachVisuals.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void ABreachGameMode::RunModelReview()
{
    bGallery=true;
    FString Path,Prefix=TEXT("Ascalon_Review");
    FParse::Value(FCommandLine::Get(),TEXT("BreachReviewMesh="),Path);
    FParse::Value(FCommandLine::Get(),TEXT("BreachReviewPrefix="),Prefix);
    Prefix=FPaths::GetCleanFilename(Prefix);
    USkeletalMesh* Asset=Path.IsEmpty()?Breach::CharacterMesh(3):LoadObject<USkeletalMesh>(nullptr,*Path);
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!Asset || !PC)
    {
        UE_LOG(LogTemp,Error,TEXT("BREACH_MODEL_REVIEW: missing mesh or controller"));
        FPlatformMisc::RequestExitWithStatus(true,1);
        return;
    }
    if(PC->GetPawn()) PC->GetPawn()->SetActorHiddenInGame(true);
    if(PC->GetHUD()) PC->GetHUD()->bShowHUD=false;
    auto* Stage=GetWorld()->SpawnActor<AActor>();
    auto* Mesh=NewObject<UPoseableMeshComponent>(Stage);
    Stage->SetRootComponent(Mesh);
    Mesh->SetSkinnedAssetAndUpdate(Asset);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->RegisterComponent();
    const float Scale=178.f/FMath::Max(1.f,float(Asset->GetBounds().BoxExtent.Z*2));
    Mesh->SetWorldTransform(FTransform(FRotator(0,-90,0),FVector(0,0,10000),FVector(Scale)));
    Mesh->SetCastShadow(false);
    const bool Head=FParse::Param(FCommandLine::Get(),TEXT("BreachReviewHead"));
    const FVector Focus(0,0,10000+(Head?161:89));
    for(const FVector Offset:{FVector(230,-200,140),FVector(200,220,90),FVector(-240,40,180),FVector(-80,-240,40)})
    {
        auto* L=NewObject<UPointLightComponent>(Stage);
        L->SetIntensity(6000);
        L->SetAttenuationRadius(1000);
        L->SetSourceRadius(90);
        L->SetCastShadows(false);
        L->RegisterComponent();L->SetWorldLocation(Focus+Offset);
    }
    auto* Camera=GetWorld()->SpawnActor<ACameraActor>();
    auto* Lens=Camera->GetCameraComponent();
    Lens->SetProjectionMode(ECameraProjectionMode::Orthographic);
    Lens->SetOrthoWidth(Head?46:205);
    Lens->bConstrainAspectRatio=false;
    Lens->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Lens->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Lens->PostProcessSettings.bOverride_AutoExposureBias=true;
    Lens->PostProcessSettings.AutoExposureBias=0;
    Lens->PostProcessSettings.bOverride_MotionBlurAmount=true;
    Lens->PostProcessSettings.MotionBlurAmount=0;
    PC->bAutoManageActiveCameraTarget=false;
    PC->SetViewTarget(Camera);
    const FVector Views[]={FVector(500,0,0),FVector(0,500,0),FVector(-500,0,0)};
    const TCHAR* Names[]={TEXT("Front"),TEXT("Side"),TEXT("Back")};
    for(int32 I=0;I<3;++I)
    {
        FTimerHandle Move,Capture;
        const FVector Position=Focus+Views[I];
        GetWorldTimerManager().SetTimer(Move,[Camera,Focus,Position]()
        {
            Camera->SetActorLocationAndRotation(Position,(Focus-Position).Rotation());
        },.25f+I*2.5f,false);
        const FString File=FPaths::ProjectDir()/TEXT("Saved")/(Prefix+TEXT("_")+Names[I]+TEXT(".png"));
        GetWorldTimerManager().SetTimer(Capture,[File]() { FScreenshotRequest::RequestScreenshot(File,false,false); },2.f+I*2.5f,false);
    }
    FTimerHandle Exit;
    GetWorldTimerManager().SetTimer(Exit,[PC]() { PC->ConsoleCommand(TEXT("quit")); },8.5f,false);
}
