#include "BreachGame.h"
#include "BreachSeabornEnemy.h"
#include "BreachNerveDamageComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void ABreachGameMode::RunSeabornMechanismTest()
{
    FString Key=TEXT("ShellSeaRunner");
    FParse::Value(FCommandLine::Get(),TEXT("BreachMechanismEnemy="),Key);
    auto Failures=MakeShared<int32>(0); auto Report=MakeShared<FString>();
    const auto Check=[Failures,Report](bool Pass,const TCHAR* Message)
    {
        *Report+=FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),Message);
        if(!Pass) ++*Failures;
    };
    const bool Slider=Key==TEXT("DeepSeaSlider");
    const bool Spitter=Key==TEXT("SpinalSeaSpitter");
    const bool Drifter=Key==TEXT("SeaDrifter");
    const bool Reaper=Key==TEXT("BowlSeaReaper");
    Check(Key==TEXT("ShellSeaRunner") || Slider || Spitter || Drifter || Reaper,TEXT("Requested species is implemented"));
    const EBreachSeabornSpecies Kind=Reaper?EBreachSeabornSpecies::BowlSeaReaper:(Drifter?EBreachSeabornSpecies::SeaDrifter:(Spitter?EBreachSeabornSpecies::SpinalSeaSpitter:(Slider?EBreachSeabornSpecies::DeepSeaSlider:EBreachSeabornSpecies::ShellSeaRunner)));
    auto* Player=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Player) { FPlatformMisc::RequestExitWithStatus(true,1); return; }
    const FVector Origin(0,0,10000);
    Player->SetActorTickEnabled(false);
    Player->GetCharacterMovement()->DisableMovement();
    Player->SetActorLocation(Origin+FVector(100,0,0));
    Player->Health=100;
    Player->NerveDamage->SetComponentTickEnabled(false);
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy=GetWorld()->SpawnActor<ABreachSeabornEnemy>(Origin,FRotator::ZeroRotator,Params);
    Enemy->SetActorTickEnabled(false);
    Check(!Enemy->bMechanicsEnabled && Enemy->Health==0,TEXT("New actors require explicit activation"));
    Check(Enemy->ActivateSpecies(Kind) && Enemy->HasRig(),TEXT("Activation loads existing rig and actions"));
    if(Drifter) Check(Enemy->GetCharacterMovement()->MovementMode==MOVE_Flying && Enemy->GetActorLocation().Z>Origin.Z,TEXT("Drifter activates with true flying movement above its ground plane"));
    Enemy->SetActorLocation(Origin);
    const auto Profile=Enemy->GetProfile();
    Enemy->GetCharacterMovement()->DisableMovement();
    const float Duration=Enemy->GetAttackDuration();
    const float AfterHit=Reaper?100.f:(Drifter?89.f:86.f);
    FDamageEvent True(UBreachTrueDamage::StaticClass());
    if(Reaper)
    {
        Check(Enemy->Health==1000 && Profile.Attack==400 && Profile.Defense==800 && Profile.ArtsResistance==75,TEXT("Reaper uses its level-0 elite profile"));
        Enemy->AdvanceMechanics(29.9f);
        Check(Enemy->Action==EBreachSeabornAction::Idle && !Enemy->bAwake && Player->Health==100,TEXT("Dormant reaper is rooted and disarmed for the first thirty seconds"));
        Enemy->AdvanceMechanics(.2f);
        Check(Enemy->Action==EBreachSeabornAction::Move && !Enemy->bAwake && Player->Health==100 && Enemy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Ignore,TEXT("Dormant reaper can move after thirty seconds but cannot attack or block players"));
        Enemy->ApplyDisarm(20);
        Enemy->TakeDamage(.05f,True,nullptr,Player); Enemy->AdvanceMechanics(.01f);
        Check(Enemy->Action!=EBreachSeabornAction::Wake,TEXT("Health at or above 99.99 percent does not wake the reaper"));
        Enemy->ApplyIncapacitation(1);
        Enemy->TakeDamage(.06f,True,nullptr,Player); Enemy->AdvanceMechanics(.5f);
        Check(Enemy->Action!=EBreachSeabornAction::Wake,TEXT("Incapacitation defers the health-triggered wake"));
        Enemy->AdvanceMechanics(.5f); Enemy->AdvanceMechanics(.01f);
        Check(Enemy->Action==EBreachSeabornAction::Wake && !Enemy->bAwake,TEXT("Health-triggered wake ignores disarm and plays Skill_Begin first"));
        Enemy->AdvanceMechanics(Enemy->GetWakeDuration());
        Check(Enemy->bAwake && FMath::IsNearlyEqual(Enemy->GetCharacterMovement()->MaxWalkSpeed,790.f*.3f/1.9f*6.f,.001f),TEXT("Completed wake enables combat and adds 500 percent movement speed"));
        auto* Other=GetWorld()->SpawnActor<ABreachCharacter>(Origin+FVector(200,100,0),FRotator::ZeroRotator,Params);
        Other->SetActorTickEnabled(false); Other->GetCharacterMovement()->DisableMovement(); Other->NerveDamage->SetComponentTickEnabled(false);
        auto* Wall=GetWorld()->SpawnActor<AActor>();
        auto* Blocker=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Blocker);
        Blocker->SetBoxExtent(FVector(5,300,200)); Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Blocker->SetCollisionResponseToAllChannels(ECR_Ignore); Blocker->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        Blocker->RegisterComponent(); Blocker->SetWorldLocation(Origin+FVector(50,0,0));
        const float Before=Enemy->Health;
        Enemy->AdvanceMechanics(.5f);
        Check(FMath::IsNearlyEqual(Enemy->Health,Before-20.f,.001f),TEXT("Awake reaper loses four percent maximum HP per second"));
        Check(Player->Health==100 && Player->NerveDamage->GetAccumulated()==40 && Other->NerveDamage->GetAccumulated()==40,TEXT("Aura applies 20 percent ATK per second to every player through cover"));
        Other->SetActorLocation(Origin+FVector(501,0,0));
        Enemy->ApplyIncapacitation(1); Enemy->AdvanceMechanics(.5f);
        Check(Player->NerveDamage->GetAccumulated()==80 && Other->NerveDamage->GetAccumulated()==40,TEXT("Passive aura persists during incapacitation and respects its 2.5-tile boundary"));
        Wall->Destroy(); Other->Destroy();
        Enemy->ActivateSpecies(Kind); Enemy->TakeDamage(1,True,nullptr,Player); Enemy->AdvanceMechanics(Enemy->GetWakeDuration());
        Player->NerveDamage->RecoverNerveDamage(10000);
        Enemy->AdvanceMechanics(.01f); Enemy->AdvanceMechanics(Duration*.42f+.01f);
        Check(Player->Health==80 && Player->NerveDamage->GetAccumulated()>40,TEXT("Awakened melee deals 20 damage plus ten percent ATK neural damage and aura"));
        Player->SetActorLocation(Origin+FVector(5000,0,0));
        Enemy->AdvanceMechanics(25);
        Check(Enemy->IsDefeated(),TEXT("Awakened maximum-HP drain eventually defeats the reaper"));
        Player->Health=100; Player->SetActorLocation(Origin+FVector(100,0,0));
        Player->NerveDamage->RecoverNerveDamage(10000); Enemy->AdvanceMechanics(1);
        Check(Player->NerveDamage->GetAccumulated()==0,TEXT("Reaper death immediately stops the neural aura"));
        Enemy->ActivateSpecies(Kind); Enemy->TakeDamage(10000,True,nullptr,Player);
        Check(Enemy->IsDefeated() && !Enemy->bAwake,TEXT("Lethal damage while dormant goes straight to death"));
    }
    else
    {
    Check(Enemy->Health==(Spitter?220.f:(Slider?140.f:150.f)) && FMath::IsNearlyEqual(Enemy->GetCharacterMovement()->MaxWalkSpeed,790.f*Profile.Speed/1.9f,.001f),TEXT("Level-0 HP and speed use documented FPS conversion"));
    Enemy->AdvanceMechanics(.01f);
    Check(Enemy->Action==EBreachSeabornAction::Attack && Player->Health==100,TEXT("Attack starts with a visible windup"));
    Enemy->AdvanceMechanics(Duration*.42f+.01f);
    Check(FMath::IsNearlyEqual(Player->Health,AfterHit),TEXT("Attack hit phase deals the species physical damage"));
    Enemy->AdvanceMechanics(.05f);
    Check(FMath::IsNearlyEqual(Player->Health,AfterHit),TEXT("An attack hits once"));
    Check(FMath::IsNearlyEqual(Player->NerveDamage->GetAccumulated(),Drifter?44.f:(Slider?42.f:0.f)),TEXT("Attack adds the specified raw-scale nerve damage once"));
    Check(Enemy->GetAttackCooldown()>0,TEXT("Attack interval includes recovery"));
    Enemy->ActivateSpecies(Kind); Enemy->AdvanceMechanics(.01f);
    Player->SetActorLocation(Origin+FVector(1000,0,0));
    Enemy->AdvanceMechanics(Duration*.42f+.01f);
    Check(FMath::IsNearlyEqual(Player->Health,AfterHit),TEXT("Leaving attack range avoids the queued hit"));
    Player->SetActorLocation(Origin+FVector(100,0,0));
    Enemy->ActivateSpecies(Kind); Enemy->AdvanceMechanics(.01f);
    Enemy->ApplyIncapacitation(1);
    Enemy->AdvanceMechanics(.5f);
    Check(FMath::IsNearlyEqual(Player->Health,AfterHit),TEXT("Incapacitation cancels queued attacks"));
    auto* Wall=GetWorld()->SpawnActor<AActor>();
    auto* Blocker=NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Blocker);
    Blocker->SetBoxExtent(FVector(5,100,150));
    Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
    Blocker->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Blocker->RegisterComponent(); Blocker->SetWorldLocation(Origin+FVector(50,0,0));
    Enemy->ActivateSpecies(Kind); Enemy->AdvanceMechanics(.01f); Enemy->AdvanceMechanics(Duration);
    Check(!Enemy->CanHit(Player) && Player->Health==AfterHit,TEXT("Opaque cover prevents attacks and damage"));
    Wall->Destroy();
    if(Spitter)
    {
        Player->SetActorLocation(Origin+FVector(490,0,0));
        Check(Enemy->CanHit(Player),TEXT("Spitter can attack within its 2.5-tile ranged area"));
        Player->SetActorLocation(Origin+FVector(501,0,0));
        Check(!Enemy->CanHit(Player) && Profile.NerveFraction==0,TEXT("Spitter respects 500cm range and has no invented neural talent"));
        Player->SetActorLocation(Origin+FVector(100,0,0));
    }
    FDamageEvent Arts(UBreachArtsDamage::StaticClass());
    Check(FMath::IsNearlyEqual(Enemy->TakeDamage(10,Arts,nullptr,Player),10.f*(1-Profile.ArtsResistance/100.f)),TEXT("Arts resistance reduces incoming arts damage"));
    Check(FMath::IsNearlyEqual(Enemy->TakeDamage(10,True,nullptr,Player),10.f),TEXT("True damage bypasses armor and resistance"));
    Enemy->TakeDamage(10000,True,nullptr,Player);
    Enemy->AdvanceMechanics(3);
    Check(Enemy->IsDefeated() && Enemy->Health==0 && Player->Health==AfterHit && Enemy->GetCapsuleComponent()->GetCollisionEnabled()==ECollisionEnabled::NoCollision,TEXT("Death cancels attacks and disables collision"));
    }
    Check(RemainingToSpawn==0 && Wave==0 && EnemiesAlive==0,TEXT("Diagnostic mechanisms do not enter regular waves"));

    auto* Nerve=Player->NerveDamage.Get();
    Nerve->RecoverNerveDamage(10000);
    Check(Nerve->GetMeterFraction()==0,TEXT("Nerve HUD starts with an empty white line"));
    Nerve->ApplyNerveDamage(999,Enemy);
    Check(!Nerve->IsBurstActive() && Player->Health==AfterHit && Nerve->GetAccumulated()==999 && FMath::IsNearlyEqual(Nerve->GetMeterFraction(),.999f,.0001f),TEXT("999 accumulation does not burst"));
    Nerve->ApplyNerveDamage(1,Enemy);
    Check(Nerve->IsBurstActive() && Nerve->GetMeterFraction()==1 && Player->Health==AfterHit-50,TEXT("Threshold bursts once for 50 true damage and displays full blue"));
    Nerve->ApplyNerveDamage(5000,Enemy);
    Check(Player->Health==AfterHit-50 && Nerve->GetAccumulated()==0,TEXT("Burst cooldown prevents repeated damage and accumulation"));
    UGameplayStatics::SetGamePaused(this,true); Nerve->AdvanceRecovery(1);
    Check(Nerve->GetBurstRemaining()==10,TEXT("Pause freezes the impairment timer"));
    UGameplayStatics::SetGamePaused(this,false);
    Nerve->AdvanceRecovery(9.99f);
    Check(Nerve->IsBurstActive(),TEXT("Impairment persists for the full ten seconds"));
    Nerve->AdvanceRecovery(.02f);
    Check(!Nerve->IsBurstActive() && Nerve->GetMeterFraction()==0 && Nerve->GetFireIntervalMultiplier()==1,TEXT("Recovery clears blue fill and restores fire rate"));
    Nerve->ApplyNerveDamage(500,Enemy); Nerve->RecoverNerveDamage(100);
    Check(Nerve->GetAccumulated()==400,TEXT("Elemental recovery reduces accumulation without healing HP"));
    Nerve->RecoverNerveDamage(10000);
    const auto PCM=UBreachNerveDamageComponent::MakeTinnitusPCM();
    Check(PCM.Num()==48000*10*2 && PCM[0]==0 && PCM[PCM.Num()-1]==0,TEXT("Tinnitus has ten seconds of stereo PCM with smooth endpoints"));

    const FString Output=FPaths::ProjectDir()/TEXT("Saved/SeabornMechanisms")/Key;
    IFileManager::Get().MakeDirectory(*Output,true);
    const auto Finish=[Report,Failures,Output,Key]()
    {
        *Report+=FString::Printf(TEXT("FAILURES=%d\n"),*Failures);
        FFileHelper::SaveStringToFile(*Report,*(Output/TEXT("test.txt")));
        UE_LOG(LogTemp,Display,TEXT("SEABORN_MECHANISMS %s\n%s"),*Key,**Report);
        FPlatformMisc::RequestExitWithStatus(false,*Failures?1:0);
    };
    if(!FParse::Param(FCommandLine::Get(),TEXT("BreachMechanismCapture"))) { Finish(); return; }
    Enemy->Destroy();
    if(!Slider)
    {
        const FVector StageOrigin(0,0,10000);
        auto* Floor=GetWorld()->SpawnActor<AActor>();
        auto* Surface=NewObject<UStaticMeshComponent>(Floor);
        Floor->SetRootComponent(Surface);
        Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Surface->RegisterComponent();
        Surface->SetWorldTransform(FTransform(FRotator::ZeroRotator,StageOrigin-FVector(0,0,5),FVector(30,30,.1)));
        auto* Subject=GetWorld()->SpawnActor<ABreachSeabornEnemy>(StageOrigin+FVector(0,0,72),FRotator::ZeroRotator,Params);
        Subject->ActivateSpecies(Kind); Subject->SetActorTickEnabled(false);
        Subject->GetCharacterMovement()->StopMovementImmediately();
        Subject->GetCharacterMovement()->DisableMovement();
        Player->Health=100; Player->DamageFlash=0;
        Player->SetActorLocation(Subject->GetActorLocation()+FVector(100,0,0));
        auto* PC=Cast<APlayerController>(Player->GetController());
        PC->GetHUD()->bShowHUD=false; Player->SetActorHiddenInGame(true);
        auto* Camera=GetWorld()->SpawnActor<ACameraActor>();
        const FVector Focus=StageOrigin+FVector(0,0,125);
        Camera->SetActorLocation(Focus+FVector(470,-540,260));
        Camera->SetActorRotation((Focus-Camera->GetActorLocation()).Rotation());
        Camera->GetCameraComponent()->SetFieldOfView(48);
        PC->bAutoManageActiveCameraTarget=false; PC->SetViewTarget(Camera);
        for(const FVector Offset:{FVector(250,-350,300),FVector(200,350,180),FVector(-300,80,240)})
        {
            auto* Light=NewObject<UPointLightComponent>(Floor);
            Light->SetIntensity(12000); Light->SetAttenuationRadius(1800); Light->SetSourceRadius(90);
            Light->RegisterComponent(); Light->SetWorldLocation(Focus+Offset);
        }
        Subject->Tick(0);
        const FVector Scale=Subject->Visual->GetRelativeScale3D();
        const FVector StartRoot=Subject->GetActorLocation();
        const auto Shot=[Output](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(Output/(FString(Name)+TEXT(".png")),false,false); };
        FTimerHandle Idle,Attack,AttackShot,Death,DeathShot,Hold,View,ViewShot,Exit;
        GetWorldTimerManager().SetTimer(Idle,[Shot]() { Shot(TEXT("Enemy_Idle")); },3.f,false);
        GetWorldTimerManager().SetTimer(Attack,[Subject,Duration,Reaper,True,Player]()
        {
            if(Reaper) { Subject->TakeDamage(1,True,nullptr,Player); Subject->AdvanceMechanics(Subject->GetWakeDuration()); }
            Subject->AdvanceMechanics(.01f); Subject->AdvanceMechanics(Duration*.42f); Subject->Tick(0);
        },4.f,false);
        GetWorldTimerManager().SetTimer(AttackShot,[Shot]() { Shot(TEXT("Enemy_Attack")); },5.f,false);
        GetWorldTimerManager().SetTimer(Death,[Subject,True,Player]() { Subject->TakeDamage(10000,True,nullptr,Player); Subject->Tick(2); },6.f,false);
        GetWorldTimerManager().SetTimer(DeathShot,[Shot]() { Shot(TEXT("Enemy_Death")); },7.f,false);
        GetWorldTimerManager().SetTimer(Hold,[Subject,Scale,StartRoot,Check,Shot]()
        {
            Subject->Tick(2);
            Check(Subject->Visual->GetRelativeScale3D().Equals(Scale,.001f) && Subject->GetActorLocation().Equals(StartRoot,.001f),TEXT("Death uses authored pose without root drift or shrinking"));
            Shot(TEXT("Enemy_DeathHold"));
        },9.f,false);
        GetWorldTimerManager().SetTimer(View,[Player,PC,Subject]()
        {
            Player->SetActorHiddenInGame(false); Player->Health=100; Player->DamageFlash=0;
            Player->SetActorLocation(FVector(480,0,10092));
            Player->GetController()->SetControlRotation((Subject->GetActorLocation()-Player->Camera->GetComponentLocation()).Rotation());
            Player->UpdateOperatorPose(0); PC->SetViewTarget(Player); PC->GetHUD()->bShowHUD=true;
        },10.f,false);
        GetWorldTimerManager().SetTimer(ViewShot,[Shot]() { Shot(TEXT("Enemy_FirstPerson")); },11.f,false);
        GetWorldTimerManager().SetTimer(Exit,Finish,13.f,false);
        return;
    }
    Player->Health=100;
    Player->DamageFlash=0;
    Player->SetActorLocation(FVector(-500,0,92));
    Player->GetController()->SetControlRotation(FRotator(-5,0,0));
    Player->UpdateOperatorPose(0);
    const auto Shot=[Output](const TCHAR* Name) { FScreenshotRequest::RequestScreenshot(Output/(FString(Name)+TEXT(".png")),true,false); };
    FTimerHandle White,Partial,PartialShot,Burst,BurstShot,Recover,RecoverShot,Exit;
    GetWorldTimerManager().SetTimer(White,[Shot]() { Shot(TEXT("Nerve_White")); },5.f,false);
    GetWorldTimerManager().SetTimer(Partial,[Nerve]() { Nerve->ApplyNerveDamage(500,nullptr); },7.f,false);
    GetWorldTimerManager().SetTimer(PartialShot,[Shot]() { Shot(TEXT("Nerve_Partial")); },8.f,false);
    GetWorldTimerManager().SetTimer(Burst,[Nerve,Player,Check,Shot]()
    {
        const int32 Shots=Player->ShotsFired;
        Nerve->ApplyNerveDamage(500,nullptr);
        Player->Fire();
        Check(Player->ShotsFired==Shots && Nerve->GetFireIntervalMultiplier()==2.5f,TEXT("Burst stretches pending rifle cadence"));
        Check(Nerve->HasBlurFeedback() && Nerve->IsTinnitusPlaying(),TEXT("Local burst attaches blur and starts stereo tinnitus audio"));
    },10.f,false);
    GetWorldTimerManager().SetTimer(BurstShot,[Shot]() { Shot(TEXT("Nerve_Burst")); },11.f,false);
    GetWorldTimerManager().SetTimer(Recover,[Nerve,Check,Shot]()
    {
        Nerve->AdvanceRecovery(10);
        Check(!Nerve->HasBlurFeedback() && !Nerve->IsTinnitusPlaying(),TEXT("Recovery removes only the nerve blur and stops tinnitus"));
    },13.f,false);
    GetWorldTimerManager().SetTimer(RecoverShot,[Shot]() { Shot(TEXT("Nerve_Recovered")); },14.f,false);
    GetWorldTimerManager().SetTimer(Exit,Finish,16.f,false);
}
