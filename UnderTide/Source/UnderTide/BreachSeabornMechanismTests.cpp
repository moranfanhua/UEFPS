#include "BreachGame.h"
#include "BreachSeabornEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

void ABreachGameMode::RunSeabornMechanismTest()
{
    FString Key=TEXT("ShellSeaRunner");
    FParse::Value(FCommandLine::Get(),TEXT("BreachMechanismEnemy="),Key);
    int32 Failures=0; FString Report;
    const auto Check=[&](bool Pass,const TCHAR* Message)
    {
        Report+=FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),Message);
        if(!Pass) ++Failures;
    };
    Check(Key==TEXT("ShellSeaRunner"),TEXT("Requested species is implemented"));
    auto* Player=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Player) { FPlatformMisc::RequestExitWithStatus(true,1); return; }
    const FVector Origin(0,0,10000);
    Player->SetActorTickEnabled(false);
    Player->GetCharacterMovement()->DisableMovement();
    Player->SetActorLocation(Origin+FVector(100,0,0));
    Player->Health=100;
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy=GetWorld()->SpawnActor<ABreachSeabornEnemy>(Origin,FRotator::ZeroRotator,Params);
    Enemy->SetActorTickEnabled(false);
    Check(!Enemy->bMechanicsEnabled && Enemy->Health==0,TEXT("New actors require explicit activation"));
    Check(Enemy->ActivateSpecies(EBreachSeabornSpecies::ShellSeaRunner) && Enemy->HasRig(),TEXT("Activation loads existing rig and actions"));
    Enemy->GetCharacterMovement()->DisableMovement();
    Check(Enemy->Health==150 && FMath::IsNearlyEqual(Enemy->GetCharacterMovement()->MaxWalkSpeed,790.f),TEXT("Level-0 HP and speed use documented FPS conversion"));
    Enemy->AdvanceMechanics(.01f);
    Check(Enemy->Action==EBreachSeabornAction::Attack && Player->Health==100,TEXT("Attack starts with a visible windup"));
    Enemy->AdvanceMechanics(.5f);
    Check(FMath::IsNearlyEqual(Player->Health,86.f),TEXT("Attack hit phase deals 14 physical damage"));
    Enemy->AdvanceMechanics(.05f);
    Check(FMath::IsNearlyEqual(Player->Health,86.f),TEXT("An attack hits once"));
    Enemy->AdvanceMechanics(.5f);
    Enemy->AdvanceMechanics(.01f);
    Check(Enemy->Action!=EBreachSeabornAction::Attack,TEXT("Attack interval includes recovery"));
    Enemy->AdvanceMechanics(.5f);
    Player->SetActorLocation(Origin+FVector(1000,0,0));
    Enemy->AdvanceMechanics(.5f);
    Check(FMath::IsNearlyEqual(Player->Health,86.f),TEXT("Leaving melee range avoids the queued hit"));
    Player->SetActorLocation(Origin+FVector(100,0,0));
    Enemy->AdvanceMechanics(2.f); Enemy->AdvanceMechanics(.01f);
    Enemy->ApplyIncapacitation(1);
    Enemy->AdvanceMechanics(.5f);
    Check(FMath::IsNearlyEqual(Player->Health,86.f),TEXT("Incapacitation cancels queued attacks"));
    FDamageEvent Arts(UBreachArtsDamage::StaticClass());
    Check(FMath::IsNearlyEqual(Enemy->TakeDamage(10,Arts,nullptr,Player),8.f),TEXT("Arts resistance reduces incoming arts damage"));
    FDamageEvent True(UBreachTrueDamage::StaticClass());
    Check(FMath::IsNearlyEqual(Enemy->TakeDamage(10,True,nullptr,Player),10.f),TEXT("True damage bypasses armor and resistance"));
    Enemy->TakeDamage(10000,True,nullptr,Player);
    Enemy->AdvanceMechanics(3);
    Check(Enemy->IsDefeated() && Enemy->Health==0 && Player->Health==86 && Enemy->GetCapsuleComponent()->GetCollisionEnabled()==ECollisionEnabled::NoCollision,TEXT("Death cancels attacks and disables collision"));
    Check(RemainingToSpawn==0 && Wave==0 && EnemiesAlive==0,TEXT("Diagnostic mechanisms do not enter regular waves"));
    Report+=FString::Printf(TEXT("FAILURES=%d\n"),Failures);
    const FString Output=FPaths::ProjectDir()/TEXT("Saved/SeabornMechanisms")/Key;
    IFileManager::Get().MakeDirectory(*Output,true);
    FFileHelper::SaveStringToFile(Report,*(Output/TEXT("test.txt")));
    UE_LOG(LogTemp,Display,TEXT("SEABORN_MECHANISMS %s\n%s"),*Key,*Report);
    FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);
}
