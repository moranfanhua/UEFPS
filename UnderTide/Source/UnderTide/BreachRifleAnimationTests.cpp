#include "BreachGame.h"
#include "BreachMovementComponent.h"
#include "BreachNerveDamageComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/DamageEvents.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void ABreachGameMode::RunCombatGirlsTest()
{
    using Clip=ECombatGirlsClip;
    auto* Player=CastChecked<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    auto* Move=CastChecked<UBreachMovementComponent>(Player->GetCharacterMovement());
    struct FRun {int32 Failures=0; TSet<Clip> Seen; FString Report;};
    auto Run=MakeShared<FRun>();
    const auto Check=[Run](bool Pass,const FString& Name){
        Run->Report+=(Pass?TEXT("PASS "):TEXT("FAIL "))+Name+TEXT("\n");Run->Failures+=!Pass;
    };
    for(int32 Index:{0,2,3,1}){
        Player->SelectOperator(Index);
        Check(Player->HasCombatGirlsAnimations()==(Index!=1),FString::Printf(TEXT("operator %d roster"),Index));
    }
    Player->SelectOperator(0);Player->DrawRifle();Player->SelectWeapon(1);
    Player->SetActorLocation(FVector(-1200,-1250,94));PC->SetControlRotation(FRotator::ZeroRotator);
    const bool Capture=FParse::Param(FCommandLine::Get(),TEXT("BreachWeaponCapture"));
    auto* Observer=GetWorld()->SpawnActor<ACameraActor>();
    Observer->GetCameraComponent()->SetFieldOfView(39);PC->bAutoManageActiveCameraTarget=false;
    const auto Inspect=[=](Clip Expected){
        const auto Actual=Player->GetRiflePose();
        Run->Seen.Add(Actual);const FString Name=StaticEnum<Clip>()->GetNameStringByValue(int32(Expected));
        Check(Actual==Expected,Name+TEXT(" actual=")+StaticEnum<Clip>()->GetNameStringByValue(int32(Actual)));
        if(Capture && (Expected==Clip::Reload || Expected==Clip::Evade || Expected==Clip::Die_B || Expected==Clip::Shoot ||
            Expected==Clip::Crouch_AimWalk_BR || Expected==Clip::AimIdle)){
            const FVector Offset(425,320,52);Observer->SetActorLocationAndRotation(Player->GetActorLocation()+Offset,(-Offset).Rotation());
            PC->SetViewTarget(Expected==Clip::AimIdle || Expected==Clip::Shoot?static_cast<AActor*>(Player):static_cast<AActor*>(Observer));
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("Saved")/(TEXT("CombatGirls_")+Name+TEXT(".png")),false,false);
        }
    };
    const auto Pose=[=](Clip Expected,FVector Velocity=FVector::ZeroVector){
        Move->Velocity=Player->GetActorTransform().TransformVectorNoScale(Velocity);
        Player->UpdateOperatorPose(.0333f);Inspect(Expected);
    };
    const auto Key=[PC](FKey Button,EInputEvent Event){
        PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Button,Event,Event==IE_Released?0.f:1.f,false,FPlatformTime::Cycles64()));
    };
    const auto At=[this](float Time,TFunction<void()> Fn){
        FTimerHandle Handle;GetWorldTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda(MoveTemp(Fn)),Time,false);
    };
    At(.5f,[=](){
        Pose(Clip::Idle);Pose(Clip::Walk,FVector(150,0,0));Pose(Clip::Run,FVector(510,0,0));Player->SetAim(true);
        Pose(Clip::AimJog,FVector(300,0,0));
        Key(EKeys::LeftShift,IE_Pressed);
    });
    const auto Directions=[=](bool Crouched){
        const FVector Vectors[]={FVector(150,0,0),FVector(100,-100,0),FVector(100,100,0),FVector(-150,0,0),FVector(-100,-100,0),FVector(-100,100,0)};
        for(int32 Dir=0;Dir<6;++Dir) Pose(Clip(int32(Crouched?Clip::Crouch_AimWalk_F:Clip::AimWalk_F)+Dir),Vectors[Dir]);
        Move->Velocity=FVector::ZeroVector;
    };
    At(.8f,[=](){
        Check(Move->bSlowWalk && Move->GetTargetMoveSpeed()<200,TEXT("Shift selects slow walk"));Pose(Clip::AimIdle);
    });
    At(.94f,[=](){Directions(false);
        Key(EKeys::LeftShift,IE_Released);PC->SetControlRotation(FRotator(0,90,0));Pose(Clip::AimTurn_R90);
    });
    At(2.f,[=](){PC->SetControlRotation(FRotator::ZeroRotator);Pose(Clip::AimTurn_L90);Player->SetAim(false);Player->CrouchOn();});
    At(2.4f,[=](){
        Pose(Clip::Crouch_Idle);Pose(Clip::Crouch_Walk,FVector(115,0,0));Pose(Clip::Crouch_Jog,FVector(200,0,0));
        Player->SetAim(true);Pose(Clip::Crouch_AimIdle);Directions(true);PC->SetControlRotation(FRotator(0,90,0));Pose(Clip::Crouch_AimTurn_R90);
    });
    At(3.65f,[=](){PC->SetControlRotation(FRotator::ZeroRotator);Pose(Clip::Crouch_AimTurn_L90);Key(EKeys::LeftMouseButton,IE_Pressed);});
    At(3.70f,[=](){Inspect(Clip::Crouch_Shoot);});
    At(3.98f,[=](){Inspect(Clip::Crouch_AutoShoot);Key(EKeys::LeftMouseButton,IE_Released);Player->Reload();});
    At(4.45f,[=](){Inspect(Clip::Crouch_Reload);Player->SelectWeapon(0);Player->CrouchOff();Player->DrawRifle();});
    At(4.85f,[=](){Key(EKeys::LeftMouseButton,IE_Pressed);});
    At(4.91f,[=](){Inspect(Clip::Shoot);});
    At(5.20f,[=](){Inspect(Clip::AimIdle_AutoShoot);Key(EKeys::LeftMouseButton,IE_Released);Player->Reload();});
    At(5.70f,[=](){Inspect(Clip::Reload);});
    At(5.79f,[=](){Player->HolsterRifle();});
    At(5.92f,[=](){Inspect(Clip::PutGun);Player->DrawRifle();});
    At(6.0f,[=](){Key(EKeys::Q,IE_Pressed);});
    At(6.1f,[=](){
        Check(Player->GetRiflePose()!=Clip::Evade && Player->GetVelocity().Size2D()<1.f,TEXT("Q does not trigger evade"));
        Key(EKeys::Q,IE_Released);
    });
    At(6.15f,[=](){Inspect(Clip::TakeGun);Player->Evade();});
    At(6.38f,[=](){Inspect(Clip::Evade);Check(Player->GetVelocity().Size2D()>100,TEXT("evade moves capsule"));});
    At(6.9f,[=](){
        FPointDamageEvent Hit;Hit.HitInfo.ImpactPoint=Player->GetActorLocation()-FVector(0,0,30);
        Player->TakeDamage(1,Hit,nullptr,nullptr);Pose(Clip::Hit_Low);
        Hit.HitInfo.ImpactPoint.Z+=75;Player->TakeDamage(1,Hit,nullptr,nullptr);Pose(Clip::Hit_Upper);
        Player->NerveDamage->ApplyNerveDamage(1000,nullptr);
    });
    At(8.55f,[=](){Inspect(Clip::Stun);Player->TakeDamage(1000,FDamageEvent(),nullptr,nullptr);});
    At(9.95f,[=](){
        Inspect(Clip::Die_B);
        auto* Other=Player->GetWorld()->SpawnActor<ABreachCharacter>(Player->GetActorLocation()+FVector(250,0,0),FRotator::ZeroRotator);
        if(Other){Other->TakeDamage(1000,FDamageEvent(),nullptr,Player);Other->UpdateOperatorPose(.1f);Check(Other->GetRiflePose()==Clip::Die_F,TEXT("rear hit selects forward death"));Run->Seen.Add(Other->GetRiflePose());Other->Destroy();}
        Check(Run->Seen.Num()==int32(Clip::None) && !Run->Seen.Contains(Clip::None),TEXT("39 distinct gameplay clips"));
        Run->Report+=FString::Printf(TEXT("FAILURES=%d\n"),Run->Failures);
        FFileHelper::SaveStringToFile(Run->Report,*(FPaths::ProjectDir()/TEXT("Saved/combat_girls_test.txt")));
        PC->ConsoleCommand(TEXT("quit"));
    });
}
