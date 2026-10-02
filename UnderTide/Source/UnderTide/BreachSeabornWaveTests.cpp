#include "BreachGame.h"
#include "BreachSeabornEnemy.h"
#include "BreachSeabornProjectile.h"
#include "BreachEnemyAwareness.h"
#include "BreachNerveDamageComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void ABreachGameMode::RunSeabornWaveTest()
{
    auto Report=MakeShared<FString>(); auto Failures=MakeShared<int32>(0);
    const auto Check=[Report,Failures](bool Pass,const TCHAR* Message)
    {
        *Report+=FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),Message);
        if(!Pass) ++*Failures;
    };
    auto* Player=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!Player) { FPlatformMisc::RequestExitWithStatus(false,1); return; }
    Player->SetActorTickEnabled(false); Player->GetCharacterMovement()->DisableMovement();
    const auto Freeze=[](ABreachSeabornEnemy* Enemy)
    {
        Enemy->SetActorTickEnabled(false); Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
    };
    const auto Collect=[this,Freeze]()
    {
        TArray<ABreachSeabornEnemy*> Result;
        for(TActorIterator<ABreachSeabornEnemy> It(GetWorld());It;++It)
            if(It->bWaveEnemy && !It->IsDefeated()) { Freeze(*It); Result.Add(*It); }
        return Result;
    };
    StartWave();
    Check(Wave==1 && RemainingToSpawn==6 && EnemiesAlive==0,TEXT("First combat wave schedules six enemies"));
    for(int32 I=0;I<6;++I) SpawnEnemy();
    auto Enemies=Collect(); TSet<uint8> Species;
    bool Activated=true,Spaced=true;
    ABreachSeabornEnemy* Runner=nullptr; ABreachSeabornEnemy* Reaper=nullptr; ABreachSeabornEnemy* Spitter=nullptr;
    for(auto* Enemy:Enemies)
    {
        Species.Add(uint8(Enemy->Species));
        Activated&=Enemy->bMechanicsEnabled && Enemy->HasRig() && Enemy->Health==Enemy->MaxHealth && Enemy->MaxHealth>0;
        Spaced&=FVector::Dist2D(Enemy->GetActorLocation(),Player->GetActorLocation())>=700;
        if(Enemy->Species==EBreachSeabornSpecies::ShellSeaRunner) Runner=Enemy;
        if(Enemy->Species==EBreachSeabornSpecies::BowlSeaReaper) Reaper=Enemy;
        if(Enemy->Species==EBreachSeabornSpecies::SpinalSeaSpitter) Spitter=Enemy;
        if(Enemy->GetProfile().bFlying) Check(Enemy->GetCharacterMovement()->MovementMode==MOVE_Flying,TEXT("Wave drifter uses flying movement"));
        if(Enemy->GetProfile().bDormant) Check(!Enemy->bAwake && Enemy->Action==EBreachSeabornAction::Idle,TEXT("Wave reaper starts dormant"));
    }
    Check(Enemies.Num()==6 && EnemiesAlive==6 && RemainingToSpawn==0 && Species.Num()==6,TEXT("Normal spawner activates all six species and updates active/inbound counts"));
    Check(Activated && Spaced,TEXT("Every wave enemy has working resources and spawns at least seven meters from the player"));
    if(Runner)
    {
        const FVector Head=Runner->Visual->GetBoneLocationByName(TEXT("head"),EBoneSpaces::WorldSpace);
        const FVector Tail=Runner->Visual->GetBoneLocationByName(TEXT("tail_04"),EBoneSpaces::WorldSpace);
        Check(FVector::DotProduct((Head-Tail).GetSafeNormal2D(),Runner->GetActorForwardVector())>.9f,
            TEXT("Runner model faces the same direction as movement and attacks"));
    }
    bool Legacy=false;
    for(TActorIterator<ABreachEnemy> It(GetWorld());It;++It) Legacy|=!It->bDisplayOnly;
    Check(!Legacy && Displays.Num()==4,TEXT("Combat uses Seaborn actors while all four operator displays remain"));

    // Move fixtures away from the perimeter so the cap and failure cases can use real spawn points.
    for(int32 I=0;I<Enemies.Num();++I) Enemies[I]->SetActorLocation(FVector(I*500,0,10000));
    RemainingToSpawn=4; SpawnEnemy(); SpawnEnemy();
    const int32 AtCap=EnemiesAlive,InboundAtCap=RemainingToSpawn;
    SpawnEnemy();
    Check(AtCap==8 && EnemiesAlive==8 && RemainingToSpawn==InboundAtCap,TEXT("Direct spawn requests also respect the eight-live-enemy cap"));
    Enemies=Collect();
    for(int32 I=0;I<Enemies.Num();++I) Enemies[I]->SetActorLocation(FVector(I*500,0,10000));

    if(Runner)
    {
        Runner->SetActorLocation(FVector(0,0,13000)); Runner->SetActorRotation(FRotator::ZeroRotator);
        Player->SetActorLocation(Runner->GetActorLocation()+FVector(100,0,0)); Player->Health=100;
        bGallery=false; Runner->AdvanceMechanics(.01f); Runner->AdvanceMechanics(Runner->GetAttackDuration()*.42f+.01f);
        Check(Player->Health==86,TEXT("Wave runner's real attack deals damage instead of the legacy animation-only bite"));
        bGallery=true; Player->Health=100; Player->SetActorLocation(FVector(0,5000,10000));
    }
    if(Spitter)
    {
        const FVector Origin(0,0,14000);
        const auto Shoot=[this,Spitter,Player,Origin]()
        {
            Spitter->ActivateSpecies(EBreachSeabornSpecies::SpinalSeaSpitter);
            Spitter->SetActorLocation(Origin); Spitter->SetActorRotation(FRotator::ZeroRotator);
            Player->SetActorLocation(Origin+FVector(2000,0,0)); Player->Health=100;
            bGallery=false;
            Spitter->AdvanceMechanics(.01f); Spitter->AdvanceMechanics(Spitter->GetAttackDuration()*.42f+.01f);
            for(TActorIterator<ABreachSeabornProjectile> It(GetWorld());It;++It)
                if(It->GetOwner()==Spitter) return *It;
            return static_cast<ABreachSeabornProjectile*>(nullptr);
        };
        auto* Shot=Shoot();
        Check(Shot && Player->Health==100,TEXT("Normal wave spitter launches a real projectile at twenty meters without instant damage"));
        if(Shot) Shot->Movement->TickComponent(1.5f,LEVELTICK_All,nullptr);
        Check(Player->Health==86 && (!Shot || Shot->IsActorBeingDestroyed()),TEXT("Normal wave projectile applies the original fourteen damage on collision"));
        Shot=Shoot(); bGameOver=true;
        if(Shot) { Shot->Tick(.01f); Shot->Movement->TickComponent(1.5f,LEVELTICK_All,nullptr); }
        Check(Shot && Shot->IsActorBeingDestroyed() && Player->Health==100,TEXT("Game over removes an in-flight wave projectile without extra damage"));
        bGameOver=false; Shot=Shoot(); bGallery=true;
        if(Shot) Shot->Tick(.01f);
        Check(Shot && Shot->IsActorBeingDestroyed() && Player->Health==100,TEXT("Entering gallery mode also clears in-flight wave projectiles"));
        Player->SetActorLocation(FVector(0,5000,10000));
    }
    const int32 KillsBefore=Kills,ScoreBefore=Score;
    FDamageEvent True(UBreachTrueDamage::StaticClass());
    if(Reaper)
    {
        Reaper->TakeDamage(1,True,nullptr,Player); bGallery=false;
        Reaper->AdvanceMechanics(Reaper->GetWakeDuration());
        Check(Reaper->bAwake,TEXT("Damaging a wave reaper completes its existing wake sequence"));
        const float BeforePause=Reaper->Health;
        UGameplayStatics::SetGamePaused(this,true); Reaper->AdvanceMechanics(1);
        Check(Reaper->Health==BeforePause,TEXT("Pause freezes wave enemy mechanisms"));
        UGameplayStatics::SetGamePaused(this,false); bGameOver=true; Reaper->AdvanceMechanics(1);
        Check(Reaper->Health==BeforePause && Reaper->GetVelocity().IsNearlyZero(),TEXT("Game over freezes wave movement and HP drain"));
        bGameOver=false; Reaper->AdvanceMechanics(25); bGallery=true;
        Check(Reaper->IsDefeated() && Kills==KillsBefore+1 && EnemiesAlive==AtCap-1,TEXT("Reaper self-drain death releases its wave slot exactly once"));
    }
    for(auto* Enemy:Enemies) Enemy->TakeDamage(10000,True,nullptr,Player);
    Check(EnemiesAlive==0 && Kills==KillsBefore+AtCap && Score==ScoreBefore+AtCap*100,TEXT("All Seaborn deaths update wave counts, kills and score"));
    for(auto* Enemy:Enemies) Enemy->TakeDamage(10000,True,nullptr,Player);
    Check(Kills==KillsBefore+AtCap && Score==ScoreBefore+AtCap*100,TEXT("Defeated actors cannot award duplicate kills or points"));
    bool Cleanup=true;
    for(auto* Enemy:Enemies) Cleanup&=Enemy->GetLifeSpan()>0 && Enemy->GetLifeSpan()<=9.f && Enemy->GetCapsuleComponent()->GetCollisionEnabled()==ECollisionEnabled::NoCollision;
    Check(Cleanup,TEXT("Wave corpses clear collision and schedule nine-second cleanup"));
    RemainingToSpawn=0; bGallery=false; Intermission=0; Tick(.01f); bGallery=true;
    Check(Wave==2 && RemainingToSpawn+EnemiesAlive==8,TEXT("Clearing a wave advances the normal game flow to the next wave"));
    for(auto* Enemy:Collect()) { Enemy->TakeDamage(10000,True,nullptr,Player); Enemy->Destroy(); }

    // A blocked perimeter must retain the planned species and inbound count for retry.
    auto* Block=GetWorld()->SpawnActor<AActor>(); auto* Box=NewObject<UBoxComponent>(Block); Block->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(6000,6000,5000)); Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Box->RegisterComponent(); Box->SetWorldLocation(FVector::ZeroVector);
    Check(GetWorld()->OverlapBlockingTestByChannel(FVector(1900,1150,100),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(10)),
        TEXT("Blocked-spawn fixture physically obstructs the arena perimeter"));
    const int32 BeforeBlocked=RemainingToSpawn; SpawnEnemy();
    Check(EnemiesAlive==0 && RemainingToSpawn==BeforeBlocked,TEXT("Blocked spawn points do not consume inbound enemies or count invisible enemies"));
    Block->Destroy(); SpawnEnemy();
    Check(EnemiesAlive==1 && RemainingToSpawn==BeforeBlocked-1,TEXT("Removing a spawn obstruction allows the queued enemy to spawn"));
    auto Retry=Collect(); for(auto* Enemy:Retry) { Enemy->TakeDamage(10000,True,nullptr,Player); Enemy->Destroy(); }
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Diagnostic=GetWorld()->SpawnActor<ABreachSeabornEnemy>(FVector(0,0,15000),FRotator::ZeroRotator,Params);
    Diagnostic->ActivateSpecies(EBreachSeabornSpecies::DeepSeaSlider);
    const int32 BeforeDiagnostic=Kills; Diagnostic->TakeDamage(10000,True,nullptr,Player);
    Check(Kills==BeforeDiagnostic && EnemiesAlive==0,TEXT("Standalone diagnostic enemies remain outside wave accounting")); Diagnostic->Destroy();

    const auto Finish=[Report,Failures]()
    {
        *Report+=FString::Printf(TEXT("FAILURES=%d\n"),*Failures);
        FFileHelper::SaveStringToFile(*Report,*(FPaths::ProjectDir()/TEXT("Saved/seaborn_wave_test.txt")));
        UE_LOG(LogTemp,Display,TEXT("BREACH_SEABORN_WAVE_TEST\n%s"),**Report);
        FPlatformMisc::RequestExitWithStatus(false,*Failures?1:0);
    };
    if(!FParse::Param(FCommandLine::Get(),TEXT("BreachSeabornWaveCapture"))) { Finish(); return; }
    // Capture each species from the actual arena spawner with combat HUD visible.
    for(auto* Enemy:Enemies) Enemy->Destroy();
    Player->SelectOperator(0); Player->Health=100; Player->DamageFlash=0; Player->NerveDamage->RecoverNerveDamage(10000);
    Player->SetActorLocation(FVector(-1300,0,100)); WaveSpeciesPool.Reset(); RemainingToSpawn=6;
    for(int32 I=0;I<6;++I) SpawnEnemy();
    const auto Subjects=Collect(); TSet<uint8> CapturedSpecies;
    for(auto* Subject:Subjects) CapturedSpecies.Add(uint8(Subject->Species));
    Check(Subjects.Num()==6 && CapturedSpecies.Num()==6,TEXT("Capture scene contains all six normally spawned live species"));
    bGallery=false; NoticeTime=0; SetActorTickEnabled(false);
    auto* PC=Cast<APlayerController>(Player->GetController()); PC->SetViewTarget(Player);
    auto* ReviewCamera=GetWorld()->SpawnActor<ACameraActor>(); ReviewCamera->GetCameraComponent()->SetFieldOfView(65);
    int32 Index=0;
    for(auto* Subject:Subjects)
    {
        const float Time=2.f+Index*3.f;
        FTimerHandle View,Shot,WorldView,WorldShot;
        GetWorldTimerManager().SetTimer(View,[Player,PC,Subject,Subjects,Check]()
        {
            Player->SetActorHiddenInGame(false); PC->SetViewTarget(Player); PC->GetHUD()->bShowHUD=true;
            for(auto* Other:Subjects) Other->SetActorHiddenInGame(Other!=Subject);
            const auto Bounds=Subject->Visual->GetSkinnedAsset()->GetBounds();
            const FVector Focus=Subject->Visual->GetComponentTransform().TransformPosition(Bounds.Origin);
            const float Distance=FMath::Max(650.f,float(Bounds.BoxExtent.GetMax())*Subject->Visual->GetComponentScale().GetMax()*2.5f);
            Player->SetActorLocation(Focus+FVector(-Distance,0,20));
            Player->GetController()->SetControlRotation((Focus-Player->Camera->GetComponentLocation()).Rotation());
            Player->UpdateOperatorPose(0);
            Check(Subject->Health>0 && Subject->DamageHitbox->GetCollisionEnabled()==ECollisionEnabled::QueryOnly,TEXT("Captured enemy remains alive and shootable"));
        },Time,false);
        GetWorldTimerManager().SetTimer(Shot,[Subject]()
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("Saved")/(TEXT("Wave_")+Subject->GetProfile().Key+TEXT(".png")),true,false);
        },Time+1.f,false);
        GetWorldTimerManager().SetTimer(WorldView,[this,Player,PC,ReviewCamera,Subject,Check]()
        {
            Player->SetActorHiddenInGame(true); PC->GetHUD()->bShowHUD=false;
            const auto Bounds=Subject->Visual->GetSkinnedAsset()->GetBounds();
            const FVector Focus=Subject->Visual->GetComponentTransform().TransformPosition(Bounds.Origin);
            const float Distance=FMath::Max(650.f,float(Bounds.BoxExtent.GetMax())*Subject->Visual->GetComponentScale().GetMax()*2.5f);
            const float Side=Subject->GetActorLocation().Y>=0?-.55f:.55f;
            ReviewCamera->SetActorLocation(Focus+FVector(-.8f,Side,.24f)*Distance);
            ReviewCamera->SetActorRotation((Focus-ReviewCamera->GetActorLocation()).Rotation()); PC->SetViewTarget(ReviewCamera);
            FHitResult Obstacle;
            Check(!GetWorld()->LineTraceSingleByObjectType(Obstacle,ReviewCamera->GetActorLocation(),Focus,FCollisionObjectQueryParams(ECC_WorldStatic)),
                TEXT("External review camera has an unobstructed view inside the arena"));
        },Time+1.25f,false);
        GetWorldTimerManager().SetTimer(WorldShot,[Subject]()
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("Saved")/(TEXT("WaveWorld_")+Subject->GetProfile().Key+TEXT(".png")),false,false);
        },Time+2.f,false);
        ++Index;
    }
    FTimerHandle Exit; GetWorldTimerManager().SetTimer(Exit,Finish,3.f+Index*3.f,false);
}
