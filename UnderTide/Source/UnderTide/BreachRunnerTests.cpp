#include "BreachGame.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void ABreachGameMode::RunRunnerTest()
{
    FString Report; int32 Failures=0;
    const auto Check=[&](bool Pass,const TCHAR* Message)
    {
        Report+=FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),Message);
        if(!Pass) ++Failures;
    };
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    auto* Player=Cast<ABreachCharacter>(PC->GetPawn());
    const FVector Location(-1150,-900,70);
    const FTransform Transform(FRotator::ZeroRotator,Location);
    auto* Runner=GetWorld()->SpawnActorDeferred<ABreachEnemy>(ABreachEnemy::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    Runner->bShellSeaRunner=true;
    UGameplayStatics::FinishSpawningActor(Runner,Transform);
    Runner->SetActorTickEnabled(false);
    Runner->GetCharacterMovement()->DisableMovement();
    Runner->SetActorLocation(Location,false,nullptr,ETeleportType::TeleportPhysics);
    Check(Runner->bShellSeaRunner && Runner->HasRunnerAnimation(),TEXT("Rigged runner and gallop asset loaded"));
    const auto Position=[&](const FName Name)
    {
        return Runner->RunnerVisual->GetBoneTransformByName(Name,EBoneSpaces::ComponentSpace).GetLocation();
    };
    const FName Feet[]={TEXT("fore_paw_l"),TEXT("fore_paw_r"),TEXT("hind_paw_l"),TEXT("hind_paw_r")};
    const float RunDuration=32.f/60.f;
    Check(Runner->RunnerVisual->GetBoneIndex(TEXT("tail_04"))!=INDEX_NONE,TEXT("Reference tail has a four-bone chain"));
    TArray<FVector> StartFeet;
    Runner->SampleRunnerPose(0.f);
    for(const FName Foot:Feet)
    {
        Check(Runner->RunnerVisual->GetBoneIndex(Foot)!=INDEX_NONE,*FString::Printf(TEXT("%s bone exists"),*Foot.ToString()));
        StartFeet.Add(Position(Foot));
    }
    const FVector Root=Position(TEXT("root"));
    float Excursion[4]={};
    bool Grounded=true,FixedLengths=true,StationaryRoot=true,Sampled=true;
    Runner->SampleRunnerPose(0.f,0.f);
    const FVector StandingFoot=Position(TEXT("fore_paw_l"));
    const float StandingAnkleZ=Runner->RunnerVisual->GetBoneLocationByName(TEXT("fore_paw_l"),EBoneSpaces::WorldSpace).Z;
    Check(StandingAnkleZ>7.f && StandingAnkleZ<10.f,TEXT("Standing paw tips align with the arena floor"));
    TArray<float> RestLengths;
    for(const TCHAR* Kind:{TEXT("fore"),TEXT("hind")}) for(const TCHAR* Side:{TEXT("l"),TEXT("r")})
    {
        RestLengths.Add(FVector::Distance(Position(FName(*FString::Printf(TEXT("%s_upper_%s"),Kind,Side))),Position(FName(*FString::Printf(TEXT("%s_lower_%s"),Kind,Side)))));
        RestLengths.Add(FVector::Distance(Position(FName(*FString::Printf(TEXT("%s_lower_%s"),Kind,Side))),Position(FName(*FString::Printf(TEXT("%s_paw_%s"),Kind,Side)))));
    }
    for(int32 Frame=0;Frame<32;++Frame)
    {
        Sampled&=Runner->SampleRunnerPose(Frame/60.f);
        StationaryRoot&=Position(TEXT("root")).Equals(Root,.01f);
        for(int32 Foot=0;Foot<4;++Foot)
        {
            Excursion[Foot]=FMath::Max(Excursion[Foot],float(FVector::Distance(Position(Feet[Foot]),StartFeet[Foot])));
            Grounded&=Position(Feet[Foot]).Z>=14.f;
        }
        int32 Index=0;
        for(const TCHAR* Kind:{TEXT("fore"),TEXT("hind")}) for(const TCHAR* Side:{TEXT("l"),TEXT("r")})
        {
            const FVector Upper=Position(FName(*FString::Printf(TEXT("%s_upper_%s"),Kind,Side)));
            const FVector Lower=Position(FName(*FString::Printf(TEXT("%s_lower_%s"),Kind,Side)));
            const FVector Paw=Position(FName(*FString::Printf(TEXT("%s_paw_%s"),Kind,Side)));
            FixedLengths&=FMath::Abs(FVector::Distance(Upper,Lower)-RestLengths[Index++])<.15f;
            FixedLengths&=FMath::Abs(FVector::Distance(Lower,Paw)-RestLengths[Index++])<.15f;
        }
    }
    Check(Sampled,TEXT("Gallop poses sample successfully"));
    for(int32 Foot=0;Foot<4;++Foot) Check(Excursion[Foot]>30.f,*FString::Printf(TEXT("%s has an independent stride"),*Feet[Foot].ToString()));
    Check(Grounded,TEXT("Paw origins stay above floor throughout the cycle"));
    Check(FixedLengths,TEXT("Upper and lower leg lengths remain fixed"));
    Check(StationaryRoot,TEXT("Gallop does not duplicate CharacterMovement translation"));
    Runner->SampleRunnerPose(RunDuration-.00001f);
    bool Seam=true;
    for(int32 Foot=0;Foot<4;++Foot) Seam&=Position(Feet[Foot]).Equals(StartFeet[Foot],.15f);
    Check(Seam,TEXT("All four feet meet continuously at the loop boundary"));
    Runner->SampleRunnerPose(.2f,0.f);
    Check(Position(TEXT("fore_paw_l")).Equals(StandingFoot,.1f),TEXT("Stopping blends back to the authored standing pose"));
    Runner->GetCharacterMovement()->Velocity=FVector(790,0,0);
    Runner->UpdatePose(.12f);
    Check(!Position(TEXT("fore_paw_l")).Equals(StandingFoot,10.f) && Position(TEXT("root")).Equals(Root,.01f),TEXT("Runtime movement speed advances the skeletal gait without root displacement"));
    Runner->GetCharacterMovement()->Velocity=FVector::ZeroVector;
    Runner->UpdatePose(.5f);
    Check(Position(TEXT("fore_paw_l")).Equals(StandingFoot,.1f),TEXT("Runtime zero speed settles the four-leg stance"));
    Runner->SampleRunnerPose(0.f,0.f);
    const FVector StandingTail=Position(TEXT("tail_04"));
    const FQuat StandingJaw=Runner->RunnerVisual->GetBoneTransformByName(TEXT("head"),EBoneSpaces::ComponentSpace).GetRotation().Inverse()*Runner->RunnerVisual->GetBoneTransformByName(TEXT("jaw_lower"),EBoneSpaces::ComponentSpace).GetRotation();
    Check(Runner->SampleRunnerAction(EBreachRunnerAction::Attack,.4f),TEXT("Bite animation loads and samples"));
    Check(Position(TEXT("tail_04")).Z>StandingTail.Z+30.f,TEXT("Bite raises the tail as in the reference"));
    const FQuat BiteJaw=Runner->RunnerVisual->GetBoneTransformByName(TEXT("head"),EBoneSpaces::ComponentSpace).GetRotation().Inverse()*Runner->RunnerVisual->GetBoneTransformByName(TEXT("jaw_lower"),EBoneSpaces::ComponentSpace).GetRotation();
    Check(StandingJaw.AngularDistance(BiteJaw)>FMath::DegreesToRadians(10.f),TEXT("Bite closes the upper and lower jaws relative to each other"));
    bool ActionRoot=true,ActionFeet=true,ActionLengths=true;
    for(EBreachRunnerAction Action:{EBreachRunnerAction::Attack,EBreachRunnerAction::Die})
        for(int32 Frame=0;Frame<60;++Frame)
        {
            Runner->SampleRunnerAction(Action,Frame/60.f);
            ActionRoot&=Position(TEXT("root")).Equals(Root,.01f);
            for(FName Foot:Feet) ActionFeet&=Position(Foot).Z>=14.f;
            int32 Index=0;
            for(const TCHAR* Kind:{TEXT("fore"),TEXT("hind")}) for(const TCHAR* Side:{TEXT("l"),TEXT("r")})
            {
                const FVector Upper=Position(FName(*FString::Printf(TEXT("%s_upper_%s"),Kind,Side)));
                const FVector Lower=Position(FName(*FString::Printf(TEXT("%s_lower_%s"),Kind,Side)));
                const FVector Paw=Position(FName(*FString::Printf(TEXT("%s_paw_%s"),Kind,Side)));
                ActionLengths&=FMath::Abs(FVector::Distance(Upper,Lower)-RestLengths[Index++])<.15f;
                ActionLengths&=FMath::Abs(FVector::Distance(Lower,Paw)-RestLengths[Index++])<.15f;
            }
        }
    Check(ActionRoot && ActionFeet && ActionLengths,TEXT("Bite and collapse preserve root, grounded claws and leg lengths"));
    Runner->SampleRunnerPose(.2f);
    const float PlayerHealth=Player?Player->Health:0.f;
    Check(Runner->StartRunnerAttack(),TEXT("Runtime bite starts from a moving pose"));
    Runner->UpdatePose(.4f);
    Check(Runner->IsRunnerAttacking() && Runner->GetVelocity().IsNearlyZero(),TEXT("Bite stops movement and holds the action"));
    Runner->UpdatePose(.7f);
    Check(!Runner->IsRunnerAttacking() && (!Player || Player->Health==PlayerHealth),TEXT("Bite recovers without applying damage"));
    if(Player)
    {
        const FVector PreviousLocation=Player->GetActorLocation();
        Player->SetActorLocation(Location+FVector(140,0,19),false,nullptr,ETeleportType::TeleportPhysics);
        const bool PreviousGallery=bGallery;
        bGallery=false;
        Runner->AttackCooldown=0.f;
        Runner->Tick(.016f);
        Check(Runner->IsRunnerAttacking(),TEXT("Live AI starts a bite when the visible player is close and movement has stopped"));
        bGallery=PreviousGallery;
        Player->SetActorLocation(PreviousLocation,false,nullptr,ETeleportType::TeleportPhysics);
        Runner->UpdatePose(1.1f);
    }
    Runner->SampleRunnerAction(EBreachRunnerAction::Die,59.f/60.f);
    const FVector FallenBody=Position(TEXT("body")),FallenTail=Position(TEXT("tail_04"));
    Check(FallenBody.Z<60.f,TEXT("Death folds the legs and lowers the body"));
    Runner->SampleRunnerAction(EBreachRunnerAction::Die,2.f);
    Check(Position(TEXT("body")).Equals(FallenBody,.01f) && Position(TEXT("tail_04")).Equals(FallenTail,.01f),TEXT("Death holds the final pose without looping"));
    Runner->SampleRunnerPose(.2f);
    Runner->StartRunnerAttack();
    FDamageEvent DamageEvent;
    Runner->TakeDamage(10000.f,DamageEvent,nullptr,nullptr);
    Runner->UpdateDeathPose(1.2f);
    Check(Runner->bDefeated && !Runner->IsRunnerAttacking() && Runner->HasDeathAnimation() && Position(TEXT("body")).Equals(FallenBody,.1f),TEXT("Defeat interrupts bite and plays the skeletal collapse"));
    Report+=FString::Printf(TEXT("FAILURES=%d\n"),Failures);
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectDir()/TEXT("Saved/runner_test.txt")));
    UE_LOG(LogTemp,Display,TEXT("BREACH_RUNNER_TEST\n%s"),*Report);
    if(!FParse::Param(FCommandLine::Get(),TEXT("BreachRunnerCapture"))) { PC->ConsoleCommand(TEXT("quit")); return; }
    const FVector Focus=Location+FVector(14,0,-10);
    const FVector CameraPosition=Focus+FVector(220,-550,170);
    auto* Camera=GetWorld()->SpawnActor<ACameraActor>(CameraPosition,(Focus-CameraPosition).Rotation());
    Camera->GetCameraComponent()->SetFieldOfView(46.f);
    PC->bAutoManageActiveCameraTarget=false; PC->SetViewTarget(Camera);
    if(PC->GetHUD()) PC->GetHUD()->bShowHUD=false;
    if(Player) Player->SetActorHiddenInGame(true);
    for(int32 Frame=0;Frame<5;++Frame)
    {
        FTimerHandle Capture;
        GetWorldTimerManager().SetTimer(Capture,[this,Runner,Frame]()
        {
            Runner->SampleRunnerPose(Frame<4?Frame*(8.f/60.f):0.f,Frame<4?1.f:0.f);
            FTimerHandle RenderReady;
            GetWorldTimerManager().SetTimer(RenderReady,[Frame]()
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Saved/ShellRunner_%s.png"),Frame<4?*FString::Printf(TEXT("Run_%d"),Frame):TEXT("Idle")),false,false);
            },.1f,false);
        },1.f+Frame*.5f,false);
    }
    for(int32 Frame=0;Frame<6;++Frame)
    {
        FTimerHandle Capture;
        GetWorldTimerManager().SetTimer(Capture,[this,Runner,Frame]()
        {
            Runner->SampleRunnerAction(Frame<3?EBreachRunnerAction::Attack:EBreachRunnerAction::Die,Frame%3==0?.2f:(Frame%3==1?.4f:59.f/60.f));
            FTimerHandle RenderReady;
            GetWorldTimerManager().SetTimer(RenderReady,[Frame]()
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Saved/ShellRunner_%s_%d.png"),Frame<3?TEXT("Attack"):TEXT("Die"),Frame%3),false,false);
            },.1f,false);
        },3.5f+Frame*.5f,false);
    }
    FTimerHandle FirstPerson;
    GetWorldTimerManager().SetTimer(FirstPerson,[this,Runner,Player,PC,Location]()
    {
        if(!Player) return;
        Runner->SampleRunnerPose(.1f);
        Player->SetActorHiddenInGame(false);
        Player->SetActorLocation(Location+FVector(450,0,19));
        PC->SetControlRotation(FRotator(-12.f,180.f,0));
        PC->SetViewTarget(Player);
        FTimerHandle RenderReady;
        GetWorldTimerManager().SetTimer(RenderReady,[]()
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("Saved/ShellRunner_FirstPerson.png"),false,false);
        },.1f,false);
    },6.5f,false);
    FTimerHandle Exit;
    GetWorldTimerManager().SetTimer(Exit,[PC]() { PC->ConsoleCommand(TEXT("quit")); },7.5f,false);
}
