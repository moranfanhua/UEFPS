#include "BreachGame.h"
#include "BreachEnemyAwareness.h"
#include "BreachSeabornEnemy.h"
#include "BreachMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include <limits>

void ABreachGameMode::RunEnemyAwarenessTest()
{
    auto Failures=MakeShared<int32>(0); auto Report=MakeShared<FString>();
    const auto Check=[Failures,Report](bool Pass,const TCHAR* Message)
    {
        *Report+=FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),Message);
        if(!Pass) ++*Failures;
    };
    auto* Player=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Player) { FPlatformMisc::RequestExitWithStatus(false,1); return; }
    Player->SetActorTickEnabled(false); Player->GetCharacterMovement()->DisableMovement(); Player->Health=100;
    const FVector Origin(0,0,10000);
    Player->SetActorLocation(Origin+FVector(5000,0,0));
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const auto MakeRunner=[this](FVector Position)
    {
        const FTransform Transform(FRotator::ZeroRotator,Position);
        auto* Enemy=GetWorld()->SpawnActorDeferred<ABreachEnemy>(ABreachEnemy::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        Enemy->bShellSeaRunner=true; UGameplayStatics::FinishSpawningActor(Enemy,Transform);
        Enemy->SetActorTickEnabled(false); Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
        return Enemy;
    };
    auto* Runner=MakeRunner(Origin);
    auto* Sense=Runner->Awareness.Get();
    const FVector WanderStart=Origin+FVector(2000,0,0);
    Runner->SetActorLocation(WanderStart);
    Sense->Advance(.01f); Sense->MoveTowardDestination(.016f);
    Check(Sense->Intent==EBreachEnemyIntent::Wander && Runner->GetCharacterMovement()->MaxWalkSpeed==197.5f &&
        FVector::Dist2D(Sense->Destination,WanderStart)<=500 && FVector::Dist2D(Sense->Destination,Origin)>1500 &&
        !Runner->GetPendingMovementInputVector().IsNearlyZero(),TEXT("Unaware wave runner selects around its current position at one quarter speed"));
    const FVector NextCenter=Sense->Destination;
    Runner->SetActorLocation(NextCenter); Sense->Advance(.01f); Sense->Advance(4.01f);
    Check(FVector::Dist2D(Sense->Destination,NextCenter)>=160 && FVector::Dist2D(Sense->Destination,NextCenter)<=500,
        TEXT("Each subsequent wander point is centered on the new current position"));
    Runner->SetActorLocation(Origin); Runner->SetActorRotation(FRotator::ZeroRotator); Sense->Reset(Sense->GetCombatSpeed());
    Player->SetActorLocation(Origin+FVector(-400,0,0)); Sense->Advance(.01f);
    Check(!Sense->GetVisibleTarget(),TEXT("Players behind the enemy do not trigger sight"));
    Player->SetActorLocation(Origin+FRotator(0,70,0).RotateVector(FVector(500,0,0))); Sense->Advance(.01f);
    Check(!Sense->GetVisibleTarget(),TEXT("Sight excludes positions outside the 120 degree field"));
    Player->SetActorLocation(Origin+FVector(1601,0,0)); Sense->Advance(.01f);
    Check(!Sense->GetVisibleTarget(),TEXT("Sight excludes positions beyond sixteen meters"));
    auto* Near=GetWorld()->SpawnActor<ABreachSeabornEnemy>(Origin+FVector(0,500,0),FRotator(0,180,0),Params);
    Near->ActivateSpecies(EBreachSeabornSpecies::DeepSeaSlider); Near->SetActorLocation(Origin+FVector(0,500,0));
    Near->SetActorTickEnabled(false); Near->GetCharacterMovement()->SetComponentTickEnabled(false);
    auto* Far=MakeRunner(Origin+FVector(0,5001,0)); Far->SetActorRotation(FRotator(0,180,0));
    Player->SetActorLocation(Origin+FVector(600,0,0)); Sense->Advance(.01f);
    Check(Sense->Intent==EBreachEnemyIntent::Pursue && Sense->GetVisibleTarget()==Player && Runner->GetCharacterMovement()->MaxWalkSpeed==790,
        TEXT("Direct sight acquires the player and restores pursuit speed"));
    Check(Near->Awareness->Intent==EBreachEnemyIntent::Investigate && Near->Awareness->Destination.Equals(Player->GetActorLocation()) && Far->Awareness->Intent==EBreachEnemyIntent::Wander,
        TEXT("Wave runner signals opt-in enemies inside fifty meters and excludes enemies outside"));
    Near->Awareness->Advance(.01f);
    Check(Far->Awareness->Intent==EBreachEnemyIntent::Wander,TEXT("Signal receivers do not relay unseen player coordinates"));
    Player->SetActorLocation(Origin+FVector(650,0,0)); Sense->Advance(.1f);
    Check(Near->Awareness->Destination.Equals(Origin+FVector(600,0,0)),TEXT("Broadcast interval throttles updates"));
    Sense->Advance(.2f);
    Check(Near->Awareness->Destination.Equals(Player->GetActorLocation()),TEXT("Continued sight broadcasts fresh coordinates"));
    Far->SetActorLocation(Origin+FVector(0,5000,0)); Sense->Advance(.26f);
    Check(Far->Awareness->Intent==EBreachEnemyIntent::Investigate,TEXT("Signal radius includes its exact boundary"));
    Sense->ReceiveSignal(Origin+FVector(200,500,0));
    Check(Sense->Destination.Equals(Player->GetActorLocation()),TEXT("Direct sight takes priority over another enemy's signal"));

    auto* Wall=GetWorld()->SpawnActor<AActor>(); auto* Blocker=NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Blocker);
    Blocker->SetBoxExtent(FVector(10,500,300)); Blocker->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Blocker->SetCollisionResponseToAllChannels(ECR_Ignore); Blocker->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Blocker->RegisterComponent(); Blocker->SetWorldLocation(Origin+FVector(300,0,0));
    Player->SetActorLocation(Origin+FVector(900,0,0)); Sense->Advance(.26f);
    Check(Sense->Intent==EBreachEnemyIntent::Investigate && !Sense->GetVisibleTarget() && Sense->Destination.Equals(Origin+FVector(650,0,0)),
        TEXT("Cover breaks sight and pursuit retains only the last observed position"));
    Check(Near->Awareness->Destination.Equals(Origin+FVector(650,0,0)),TEXT("An occluded player is no longer broadcast"));
    Player->SetActorLocation(Origin+FVector(-900,0,0)); Sense->Advance(1);
    Check(Sense->Destination.Equals(Origin+FVector(650,0,0)),TEXT("Hidden player movement cannot update the destination"));
    Wall->Destroy();
    Runner->SetActorLocation(Sense->Destination); Runner->SetActorRotation(FRotator::ZeroRotator); Sense->Advance(.01f); Sense->MoveTowardDestination(.016f);
    Check(Sense->Intent==EBreachEnemyIntent::Wait && Runner->GetPendingMovementInputVector().IsNearlyZero() && Runner->GetVelocity().IsNearlyZero(),
        TEXT("Arrival at the last known position stops movement and waits"));
    const FVector NewSignal=Runner->GetActorLocation()+FVector(500,200,0);
    UGameplayStatics::SetGamePaused(this,true); Sense->ReceiveSignal(NewSignal); Sense->Advance(1);
    Check(Sense->Intent==EBreachEnemyIntent::Wait,TEXT("Pause freezes sensing and signal reception"));
    UGameplayStatics::SetGamePaused(this,false); Sense->ReceiveSignal(NewSignal); Sense->MoveTowardDestination(.016f);
    Check(Sense->Intent==EBreachEnemyIntent::Investigate && Sense->Destination.Equals(NewSignal) && !Runner->GetPendingMovementInputVector().IsNearlyZero(),
        TEXT("A new signal resumes movement from the waiting state"));
    Sense->ReceiveSignal(FVector(std::numeric_limits<double>::infinity(),0,0));
    Check(Sense->Destination.Equals(NewSignal),TEXT("Non-finite signals are rejected"));
    Player->SetActorLocation(Runner->GetActorLocation()+FVector(300,0,0)); Sense->Advance(.26f);
    Check(Sense->Intent==EBreachEnemyIntent::Pursue,TEXT("Seeing the player again resumes pursuit"));

    Runner->SetActorLocation(Origin); Runner->SetActorRotation(FRotator::ZeroRotator); Sense->Stop();
    Player->SetActorLocation(Origin+FVector(140,0,0)); Runner->AttackCooldown=0; bGallery=false;
    Runner->Tick(.26f); Player->SetActorLocation(Origin+FVector(150,0,0)); Runner->Tick(.26f); bGallery=true;
    Check(Runner->IsRunnerAttacking() && Near->Awareness->Destination.Equals(Player->GetActorLocation()),TEXT("Wave enemy keeps broadcasting throughout its bite animation"));

    // The opt-in attack path must also refresh signals during its windup.
    Far->SetActorLocation(Origin+FVector(0,4000,0));
    Near->SetActorLocation(Origin+FVector(1000,0,0)); Near->SetActorRotation(FRotator::ZeroRotator);
    Player->SetActorLocation(Origin+FVector(1100,0,0)); Near->AdvanceMechanics(.01f);
    Player->SetActorLocation(Origin+FVector(1110,0,0)); Near->AdvanceMechanics(.26f);
    Check(Near->Action==EBreachSeabornAction::Attack && Far->Awareness->Destination.Equals(Player->GetActorLocation()),TEXT("Opt-in enemy keeps broadcasting throughout attack windup"));
    Player->Health=100;

    // Forward and both side probes must be blocked before movement stops.
    auto* Cage=GetWorld()->SpawnActor<AActor>();
    auto* CageBox=NewObject<UBoxComponent>(Cage); Cage->SetRootComponent(CageBox);
    CageBox->SetBoxExtent(FVector(500,500,200)); CageBox->SetCollisionObjectType(ECC_WorldStatic);
    CageBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly); CageBox->RegisterComponent(); CageBox->SetWorldLocation(Origin);
    Player->SetActorLocation(Origin+FVector(5000,0,0)); Sense->Advance(.01f); Sense->ReceiveSignal(Origin+FVector(700,0,0));
    Check(!Sense->MoveTowardDestination(.016f) && Runner->GetPendingMovementInputVector().IsNearlyZero(),TEXT("Blocked forward and side probes stop instead of blindly steering into walls"));
    Cage->Destroy();
    FDamageEvent Damage; Runner->TakeDamage(10000,Damage,nullptr,nullptr); Sense->ReceiveSignal(NewSignal); Sense->Advance(1);
    Check(Sense->Intent==EBreachEnemyIntent::Dead && !Sense->GetVisibleTarget() && Runner->GetPendingMovementInputVector().IsNearlyZero(),TEXT("Death stops sensing, signals and queued movement"));
    Near->Destroy(); Far->Destroy(); Runner->Destroy();

    for(uint8 Index=0;Index<=uint8(EBreachSeabornSpecies::FirstSeaPiercer);++Index)
    {
        auto* Enemy=GetWorld()->SpawnActor<ABreachSeabornEnemy>(Origin,FRotator::ZeroRotator,Params);
        Enemy->SetActorTickEnabled(false); Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
        Player->SetActorLocation(Origin+FVector(-600,0,0));
        Enemy->ActivateSpecies(static_cast<EBreachSeabornSpecies>(Index)); Enemy->SetActorLocation(Origin);
        auto* Awareness=Enemy->Awareness.Get(); Awareness->Advance(.01f);
        Check(Awareness->Intent==EBreachEnemyIntent::Wander && FMath::IsNearlyEqual(Enemy->GetCharacterMovement()->MaxWalkSpeed,Awareness->GetCombatSpeed()*.25f),
            *FString::Printf(TEXT("%s starts with slow wandering"),*Enemy->GetProfile().Key));
        Awareness->ReceiveSignal(Origin+FVector(700,0,0)); Awareness->MoveTowardDestination(.016f);
        Check(Awareness->Intent==EBreachEnemyIntent::Investigate && Enemy->GetPendingMovementInputVector().X>0,TEXT("Opt-in enemy moves toward a received position"));
        Player->SetActorLocation(Origin+FVector(600,0,0)); Awareness->Advance(.26f);
        Check(Awareness->GetVisibleTarget()==Player,TEXT("Opt-in enemy acquires a visible player"));
        Player->SetActorLocation(Origin+FVector(-600,0,0)); Awareness->Advance(.26f);
        Enemy->SetActorLocation(Origin+FVector(600,0,0)); Awareness->Advance(.01f);
        Check(Awareness->Intent==EBreachEnemyIntent::Wait && Awareness->Destination.Equals(Origin+FVector(600,0,0)),TEXT("Opt-in enemy waits at the last observed position"));
        FDamageEvent True(UBreachTrueDamage::StaticClass()); Enemy->TakeDamage(10000,True,nullptr,nullptr); Awareness->ReceiveSignal(Origin);
        Check(Awareness->Intent==EBreachEnemyIntent::Dead,TEXT("Opt-in enemy death rejects new signals"));
        Enemy->Destroy();
    }

    // Exercise CharacterMovement over real world frames, including flying height.
    Player->SetActorLocation(Origin+FVector(5000,0,0));
    auto* Floor=GetWorld()->SpawnActor<AActor>(); auto* Surface=NewObject<UStaticMeshComponent>(Floor); Floor->SetRootComponent(Surface);
    Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Surface->RegisterComponent();
    Surface->SetWorldTransform(FTransform(FRotator::ZeroRotator,Origin-FVector(0,0,5),FVector(30,30,.1f)));
    auto* Live=MakeRunner(Origin+FVector(-500,-300,70));
    Live->SetActorTickEnabled(true); Live->GetCharacterMovement()->SetComponentTickEnabled(true);
    auto* Flyer=GetWorld()->SpawnActor<ABreachSeabornEnemy>(Origin+FVector(-500,300,72),FRotator::ZeroRotator,Params);
    Flyer->ActivateSpecies(EBreachSeabornSpecies::SeaDrifter);
    Live->Awareness->WanderRadius=250; Flyer->Awareness->WanderRadius=250;
    const FVector Initial=Live->GetActorLocation(),InitialFlight=Flyer->GetActorLocation();
    bGallery=false; Intermission=1000;
    FTimerHandle Wander,Arrive,Dead,FinishTimer;
    GetWorldTimerManager().SetTimer(Wander,[Live,Flyer,Initial,InitialFlight,Check]()
    {
        Check(FVector::Dist2D(Live->GetActorLocation(),Initial)>20 && Live->GetVelocity().Size2D()<=198,TEXT("Live wave enemy physically wanders at low speed"));
        Check(FVector::Dist2D(Flyer->GetActorLocation(),InitialFlight)>20 && FMath::Abs(Flyer->GetActorLocation().Z-InitialFlight.Z)<15,TEXT("Live drifter wanders while preserving flight height"));
        Live->Awareness->ReceiveSignal(Initial+FVector(700,0,0)); Flyer->Awareness->ReceiveSignal(InitialFlight+FVector(700,0,0));
    },2.f,false);
    GetWorldTimerManager().SetTimer(Arrive,[Live,Flyer,Initial,InitialFlight,Check,Damage]()
    {
        Check(Live->Awareness->Intent==EBreachEnemyIntent::Wait && FVector::Dist2D(Live->GetActorLocation(),Initial+FVector(700,0,0))<=80 && Live->GetVelocity().IsNearlyZero(),TEXT("Live runner reaches signal coordinates and stops"));
        Check(Flyer->Awareness->Intent==EBreachEnemyIntent::Wait && FVector::Dist2D(Flyer->GetActorLocation(),InitialFlight+FVector(700,0,0))<=80 && FMath::Abs(Flyer->GetActorLocation().Z-InitialFlight.Z)<15,TEXT("Live flyer reaches signal coordinates without landing"));
        Live->TakeDamage(10000,Damage,nullptr,nullptr); FDamageEvent True(UBreachTrueDamage::StaticClass()); Flyer->TakeDamage(10000,True,nullptr,nullptr);
    },6.f,false);
    GetWorldTimerManager().SetTimer(Dead,[Live,Flyer,Check]()
    {
        Live->Awareness->ReceiveSignal(FVector::ZeroVector); Flyer->Awareness->ReceiveSignal(FVector::ZeroVector);
        Check(Live->GetVelocity().IsNearlyZero() && Flyer->GetVelocity().IsNearlyZero() && Live->Awareness->Intent==EBreachEnemyIntent::Dead && Flyer->Awareness->Intent==EBreachEnemyIntent::Dead,
            TEXT("Dead actors stay stopped over subsequent world frames"));
    },7.f,false);
    GetWorldTimerManager().SetTimer(FinishTimer,[Report,Failures]()
    {
        *Report+=FString::Printf(TEXT("FAILURES=%d\n"),*Failures);
        FFileHelper::SaveStringToFile(*Report,*(FPaths::ProjectDir()/TEXT("Saved/enemy_awareness_test.txt")));
        UE_LOG(LogTemp,Display,TEXT("BREACH_ENEMY_AWARENESS_TEST\n%s"),**Report);
        FPlatformMisc::RequestExitWithStatus(false,*Failures?1:0);
    },8.f,false);
}
