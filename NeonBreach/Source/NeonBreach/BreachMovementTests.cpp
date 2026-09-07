#include "BreachGame.h"
#include "BreachVisuals.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void ABreachGameMode::RunMovementTest()
{
    auto* P=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!P || !PC) return;
    int32 Index=0; FParse::Value(FCommandLine::Get(),TEXT("BreachOperator="),Index); Index=FMath::Clamp(Index,0,3);
    P->SelectOperator(Index); P->SetUnarmed(false);
    P->SetActorLocation(FVector(-1200,-1250,94));
    float LookPitch=0; FParse::Value(FCommandLine::Get(),TEXT("BreachLookPitch="),LookPitch);
    PC->SetControlRotation(FRotator(LookPitch,0,0));
    struct FResults
    {
        FString Text; int32 Failed=0; float StandingEye=0; TWeakObjectPtr<AActor> Roof;
        FBox RunEye{ForceInit},CrouchEye{ForceInit},JumpEye{ForceInit},JogEye{ForceInit};
        int32 RunSamples=0,CrouchSamples=0,JumpSamples=0,JogSamples=0;
    };
    auto Results=MakeShared<FResults>();
    FString Prefix=FString::Printf(TEXT("%s_%s"),FParse::Param(FCommandLine::Get(),TEXT("BreachMovementFirstPerson"))?TEXT("MovementFPS"):TEXT("Movement"),Breach::Keys[Index]);
    if(!FMath::IsNearlyZero(LookPitch)) Prefix+=FString::Printf(TEXT("_Pitch%d"),FMath::RoundToInt(LookPitch));
    const auto Check=[Results](bool Pass,const FString& Name)
    {
        Results->Text+=FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),*Name);
        if(!Pass) ++Results->Failed;
    };
    const auto At=[this](float Delay,TFunction<void()> Function)
    {
        // Allow the newly selected mesh and its first animation pose to render
        // before injecting keys, so loading hitches cannot coalesce the tap
        // and the assertion into the same input-processing frame.
        FTimerHandle Handle; GetWorldTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda(MoveTemp(Function)),Delay+.6f,false);
    };
    const auto Key=[PC](FKey Button,EInputEvent Event)
    {
        PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Button,Event,Event==IE_Released?0.f:1.f,false,FPlatformTime::Cycles64()));
    };
    const auto StableEye=[Check](const FBox& Bounds,int32 Samples,const TCHAR* Motion)
    {
        const FVector Drift=Bounds.IsValid?Bounds.GetSize():FVector(BIG_NUMBER);
        Check(Samples>=4 && Drift.GetMax()<.5f,FString::Printf(TEXT("%s camera stays stable (drift %s cm, %d samples)"),Motion,*Drift.ToString(),Samples));
    };
    const double CameraTestStart=GetWorld()->GetTimeSeconds();
    FTimerHandle CameraMonitor;
    GetWorldTimerManager().SetTimer(CameraMonitor,[=,this]()
    {
        const double Time=GetWorld()->GetTimeSeconds()-CameraTestStart-.6;
        const FVector Eye=P->GetActorTransform().InverseTransformPosition(P->Camera->GetComponentLocation());
        if(Time>=.6 && Time<1.04) { Results->RunEye+=Eye; ++Results->RunSamples; }
        if(Time>=1.43 && Time<2.65) { Results->JumpEye+=Eye; ++Results->JumpSamples; }
        if(Time>=3.58 && Time<3.84) { Results->CrouchEye+=Eye; ++Results->CrouchSamples; }
        if(Time>=6.65 && Time<6.94) { Results->JogEye+=Eye; ++Results->JogSamples; }
    },.016f,true);
    const bool Capture=FParse::Param(FCommandLine::Get(),TEXT("BreachMovementCapture"));
    const auto Screenshot=[Prefix,Capture,P,Check](const TCHAR* Name)
    {
        FBreachPose Rig; Rig.Init(Breach::CharacterMesh(P->OperatorIndex),P->OperatorIndex);
        const FName NeckName=P->Body->GetBoneName(Rig.Bone(EBreachBone::Neck));
        const FVector Collar=P->Body->GetBoneLocationByName(NeckName,EBoneSpaces::WorldSpace);
        Check(FVector::DotProduct(Collar-P->Camera->GetComponentLocation(),P->Camera->GetForwardVector())<0,
            FString::Printf(TEXT("%s collar stays behind the first person eye"),Name));
        if(Capture)
        {
            const FTransform ToActor=P->GetActorTransform().Inverse();
            const FVector Neck=ToActor.TransformPosition(P->WorldBody->GetBoneLocationByName(P->WorldBody->GetBoneName(Rig.Bone(EBreachBone::Neck)),EBoneSpaces::WorldSpace));
            const FVector Head=ToActor.TransformPosition(P->WorldBody->GetBoneLocationByName(P->WorldBody->GetBoneName(Rig.Bone(EBreachBone::Head)),EBoneSpaces::WorldSpace));
            UE_LOG(LogTemp,Display,TEXT("MOVEMENT_VIEW %s camera=%s neck=%s head=%s"),Name,*P->Camera->GetRelativeLocation().ToString(),*Neck.ToString(),*Head.ToString());
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("Saved")/(Prefix+TEXT("_")+Name+TEXT(".png")),true,false);
        }
    };
    if(Capture && !FParse::Param(FCommandLine::Get(),TEXT("BreachMovementFirstPerson")))
    {
        auto* Preview=GetWorld()->SpawnActor<ACameraActor>();
        Preview->GetCameraComponent()->SetFieldOfView(45);
        PC->bAutoManageActiveCameraTarget=false; PC->SetViewTarget(Preview);
        PC->GetHUD()->bShowHUD=false;
        FTimerHandle Follow;
        GetWorldTimerManager().SetTimer(Follow,[P,Preview]()
        {
            const FVector Focus=P->GetActorLocation()+FVector(0,0,-5);
            const FVector Position=Focus+FVector(390,320,130);
            Preview->SetActorLocationAndRotation(Position,(Focus-Position).Rotation());
        },.016f,true);
    }
    Check(P->HasLocomotionAnimations(),TEXT("All eight locomotion assets loaded"));
    At(.1f,[=]() { Results->StandingEye=P->Camera->GetComponentLocation().Z; Key(EKeys::Three,IE_Pressed); });
    At(.16f,[=]() { Key(EKeys::Three,IE_Released); });
    At(.22f,[=]()
    {
        Check(P->bUnarmed && P->OperatorIndex==Index,TEXT("3 enters unarmed mode without changing character"));
        Check(!P->WeaponRoot->IsVisible() && !P->WorldWeaponRoot->IsVisible(),TEXT("Both weapon representations are hidden"));
        bool ShadowsOff=true; TArray<USceneComponent*> Parts; P->WorldWeaponRoot->GetChildrenComponents(true,Parts);
        for(auto* Part:Parts) if(auto* WeaponPart=Cast<UStaticMeshComponent>(Part)) ShadowsOff&=!WeaponPart->CastShadow;
        Check(ShadowsOff,TEXT("Holstered rifle does not cast a ghost shadow"));
        const int32 Ammo=P->Ammo; P->Fire(); P->Reload(); P->SetAim(true);
        Check(P->Ammo==Ammo && !P->bAiming && !P->bReloading,TEXT("Unarmed mode blocks shooting aiming and reload"));
        Key(EKeys::W,IE_Pressed);
    });
    At(.95f,[=]() { Screenshot(TEXT("Run")); });
    At(1.05f,[=]()
    {
        Check(P->GetVelocity().Size2D()>700 && P->LocomotionState==EBreachLocomotion::Sprint,TEXT("W in unarmed mode reaches sprint speed and animation"));
        StableEye(Results->RunEye,Results->RunSamples,TEXT("Sprint"));
        Key(EKeys::W,IE_Released);
    });
    At(1.35f,[=]() { Key(EKeys::SpaceBar,IE_Pressed); });
    At(1.40f,[=]() { Key(EKeys::SpaceBar,IE_Released); });
    At(1.51f,[=]()
    {
        Check(P->GetCharacterMovement()->IsFalling() && P->GetVelocity().Z>0 && P->LocomotionState==EBreachLocomotion::JumpStart,TEXT("Space triggers physical jump and takeoff animation"));
        Screenshot(TEXT("JumpStart"));
    });
    At(1.83f,[=]() { Check(P->LocomotionState==EBreachLocomotion::JumpLoop,TEXT("Takeoff transitions to airborne loop")); Screenshot(TEXT("JumpLoop")); });
    At(2.54f,[=]() { Check(!P->GetCharacterMovement()->IsFalling() && P->LocomotionState==EBreachLocomotion::JumpLand,TEXT("Ground contact triggers landing animation")); Screenshot(TEXT("Land")); });
    At(2.75f,[=]() { StableEye(Results->JumpEye,Results->JumpSamples,TEXT("Jump and landing relative to physical player motion")); });
    At(2.9f,[=]() { Key(EKeys::LeftControl,IE_Pressed); });
    At(3.35f,[=]()
    {
        Check(P->bIsCrouched && P->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()<60,TEXT("Ctrl crouches and reduces collision capsule"));
        Check(P->Camera->GetComponentLocation().Z<Results->StandingEye-35,TEXT("Crouch lowers the camera"));
        Check(P->LocomotionState==EBreachLocomotion::CrouchIdle,TEXT("Stationary crouch uses crouch idle animation"));
        Screenshot(TEXT("Crouch")); Key(EKeys::W,IE_Pressed);
    });
    At(3.85f,[=]()
    {
        Check(P->GetVelocity().Size2D()>100 && P->GetVelocity().Size2D()<230 && P->LocomotionState==EBreachLocomotion::CrouchWalk,TEXT("Crouch movement uses reduced speed and crouch walk animation"));
        StableEye(Results->CrouchEye,Results->CrouchSamples,TEXT("Crouch walk"));
        Screenshot(TEXT("CrouchWalk")); Key(EKeys::W,IE_Released);
    });
    At(4.05f,[=,this]()
    {
        auto* Roof=GetWorld()->SpawnActor<AActor>();
        auto* Collision=NewObject<UBoxComponent>(Roof); Roof->SetRootComponent(Collision);
        Collision->SetBoxExtent(FVector(90,90,15)); Collision->SetCollisionProfileName(TEXT("BlockAll")); Collision->RegisterComponent();
        const float Floor=P->GetActorLocation().Z-P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Roof->SetActorLocation(FVector(P->GetActorLocation().X,P->GetActorLocation().Y,Floor+130));
        Results->Roof=Roof; Key(EKeys::LeftControl,IE_Released);
    });
    At(4.4f,[=]() { Check(P->bIsCrouched,TEXT("Low ceiling prevents standing through collision")); if(Results->Roof.IsValid()) Results->Roof->Destroy(); });
    At(4.8f,[=]()
    {
        Check(!P->bIsCrouched && P->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()>90,TEXT("Standing resumes when overhead space is clear"));
        Key(EKeys::One,IE_Pressed);
    });
    At(4.86f,[=]() { Key(EKeys::One,IE_Released); });
    At(5.1f,[=]()
    {
        Check(!P->bUnarmed && P->WeaponRoot->IsVisible() && P->WorldWeaponRoot->IsVisible(),TEXT("1 restores rifle"));
        const int32 Ammo=P->Ammo; P->Fire(); Check(P->Ammo==Ammo-1,TEXT("Restored rifle can fire"));
        P->Ammo=5; P->Reserve=100; P->Reload();
        Check(P->bReloading,TEXT("Armed reload starts")); Key(EKeys::Three,IE_Pressed);
    });
    At(5.16f,[=]() { Key(EKeys::Three,IE_Released); Key(EKeys::F3,IE_Pressed); });
    At(5.22f,[=]() { Key(EKeys::F3,IE_Released); });
    At(5.4f,[=]() { Check(P->OperatorIndex==2 && P->bUnarmed && P->HasLocomotionAnimations(),TEXT("F3 changes character while preserving unarmed mode")); });
    At(5.55f,[=]() { P->SelectOperator(Index); P->CrouchOn(); PC->SetControlRotation(FRotator(-35,0,0)); });
    At(5.95f,[=,this]()
    {
        auto* Wall=GetWorld()->SpawnActor<AActor>();
        auto* Collision=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Collision);
        Collision->SetBoxExtent(FVector(10,90,150)); Collision->SetCollisionProfileName(TEXT("BlockAll")); Collision->RegisterComponent();
        Wall->SetActorLocation(P->GetActorLocation()+FVector(45,0,0)); Results->Roof=Wall;
    });
    At(6.15f,[=]()
    {
        Check(P->Camera->GetComponentLocation().X<P->GetActorLocation().X+30.f,TEXT("Leaning eye stops before a nearby wall"));
        Screenshot(TEXT("WallCrouch"));
        if(Results->Roof.IsValid()) Results->Roof->Destroy();
        P->CrouchOff();
    });
    At(6.4f,[=]() { Key(EKeys::One,IE_Pressed); Key(EKeys::W,IE_Pressed); });
    At(6.46f,[=]() { Key(EKeys::One,IE_Released); });
    At(6.96f,[=]() { Key(EKeys::W,IE_Released); StableEye(Results->JogEye,Results->JogSamples,TEXT("Armed jog")); });
    At(7.1f,[=]() { Check(!P->bReloading && P->Ammo==5 && P->Reserve==100,TEXT("Holstering cancels delayed reload without changing ammo")); });
    At(7.3f,[=,this]() mutable
    {
        GetWorldTimerManager().ClearTimer(CameraMonitor);
        Results->Text+=FString::Printf(TEXT("FAILURES=%d\n"),Results->Failed);
        FFileHelper::SaveStringToFile(Results->Text,*(FPaths::ProjectDir()/TEXT("Saved")/(Prefix+TEXT(".txt"))));
        UE_LOG(LogTemp,Display,TEXT("MOVEMENT_TEST %s\n%s"),*Prefix,*Results->Text);
        PC->ConsoleCommand(TEXT("quit"));
    });
}
