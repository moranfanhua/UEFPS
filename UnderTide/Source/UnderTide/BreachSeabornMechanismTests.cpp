#include "BreachGame.h"
#include "BreachSeabornEnemy.h"
#include "BreachNerveDamageComponent.h"
#include "Camera/CameraComponent.h"
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
    Check(Key==TEXT("ShellSeaRunner") || Slider || Spitter,TEXT("Requested species is implemented"));
    const EBreachSeabornSpecies Kind=Spitter?EBreachSeabornSpecies::SpinalSeaSpitter:(Slider?EBreachSeabornSpecies::DeepSeaSlider:EBreachSeabornSpecies::ShellSeaRunner);
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
    const auto Profile=Enemy->GetProfile();
    Enemy->GetCharacterMovement()->DisableMovement();
    const float Duration=Enemy->GetAttackDuration();
    Check(Enemy->Health==(Spitter?220.f:(Slider?140.f:150.f)) && FMath::IsNearlyEqual(Enemy->GetCharacterMovement()->MaxWalkSpeed,790.f*Profile.Speed/1.9f,.001f),TEXT("Level-0 HP and speed use documented FPS conversion"));
    Enemy->AdvanceMechanics(.01f);
    Check(Enemy->Action==EBreachSeabornAction::Attack && Player->Health==100,TEXT("Attack starts with a visible windup"));
    Enemy->AdvanceMechanics(Duration*.42f+.01f);
    Check(FMath::IsNearlyEqual(Player->Health,86.f),TEXT("Attack hit phase deals 14 physical damage"));
    Enemy->AdvanceMechanics(.05f);
    Check(FMath::IsNearlyEqual(Player->Health,86.f),TEXT("An attack hits once"));
    Check(FMath::IsNearlyEqual(Player->NerveDamage->GetAccumulated(),Slider?42.f:0.f),TEXT("Attack adds the specified raw-scale nerve damage once"));
    Check(Enemy->GetAttackCooldown()>0,TEXT("Attack interval includes recovery"));
    Enemy->ActivateSpecies(Kind); Enemy->AdvanceMechanics(.01f);
    Player->SetActorLocation(Origin+FVector(1000,0,0));
    Enemy->AdvanceMechanics(Duration*.42f+.01f);
    Check(FMath::IsNearlyEqual(Player->Health,86.f),TEXT("Leaving melee range avoids the queued hit"));
    Player->SetActorLocation(Origin+FVector(100,0,0));
    Enemy->ActivateSpecies(Kind); Enemy->AdvanceMechanics(.01f);
    Enemy->ApplyIncapacitation(1);
    Enemy->AdvanceMechanics(.5f);
    Check(FMath::IsNearlyEqual(Player->Health,86.f),TEXT("Incapacitation cancels queued attacks"));
    auto* Wall=GetWorld()->SpawnActor<AActor>();
    auto* Blocker=NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Blocker);
    Blocker->SetBoxExtent(FVector(5,100,150));
    Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
    Blocker->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Blocker->RegisterComponent(); Blocker->SetWorldLocation(Origin+FVector(50,0,0));
    Enemy->ActivateSpecies(Kind); Enemy->AdvanceMechanics(.01f); Enemy->AdvanceMechanics(Duration);
    Check(!Enemy->CanHit(Player) && Player->Health==86,TEXT("Opaque cover prevents attacks and damage"));
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
    FDamageEvent True(UBreachTrueDamage::StaticClass());
    Check(FMath::IsNearlyEqual(Enemy->TakeDamage(10,True,nullptr,Player),10.f),TEXT("True damage bypasses armor and resistance"));
    Enemy->TakeDamage(10000,True,nullptr,Player);
    Enemy->AdvanceMechanics(3);
    Check(Enemy->IsDefeated() && Enemy->Health==0 && Player->Health==86 && Enemy->GetCapsuleComponent()->GetCollisionEnabled()==ECollisionEnabled::NoCollision,TEXT("Death cancels attacks and disables collision"));
    Check(RemainingToSpawn==0 && Wave==0 && EnemiesAlive==0,TEXT("Diagnostic mechanisms do not enter regular waves"));

    auto* Nerve=Player->NerveDamage.Get();
    Nerve->RecoverNerveDamage(10000);
    Check(Nerve->GetMeterFraction()==0,TEXT("Nerve HUD starts with an empty white line"));
    Nerve->ApplyNerveDamage(999,Enemy);
    Check(!Nerve->IsBurstActive() && Player->Health==86 && Nerve->GetAccumulated()==999 && FMath::IsNearlyEqual(Nerve->GetMeterFraction(),.999f,.0001f),TEXT("999 accumulation does not burst"));
    Nerve->ApplyNerveDamage(1,Enemy);
    Check(Nerve->IsBurstActive() && Nerve->GetMeterFraction()==1 && Player->Health==36,TEXT("Threshold bursts once for 50 true damage and displays full blue"));
    Nerve->ApplyNerveDamage(5000,Enemy);
    Check(Player->Health==36 && Nerve->GetAccumulated()==0,TEXT("Burst cooldown prevents repeated damage and accumulation"));
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
