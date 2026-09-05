#include "BreachGame.h"
#include "BreachVisuals.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/HUD.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

ABreachGameMode::ABreachGameMode()
{
    PrimaryActorTick.bCanEverTick=true;
    DefaultPawnClass=ABreachCharacter::StaticClass();
    HUDClass=ABreachHUD::StaticClass();
}
void ABreachGameMode::BeginPlay()
{
    Super::BeginPlay(); BuildArena();
    for(int32 i=0;i<4;++i)
    {
        const FTransform T(FRotator(0,0,0),FVector(-1900,-660+i*440,119));
        auto* E=GetWorld()->SpawnActorDeferred<ABreachEnemy>(ABreachEnemy::StaticClass(),T);
        E->bDisplayOnly=true; E->ModelIndex=i;
        UGameplayStatics::FinishSpawningActor(E,T);
        Displays.Add(E);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("BreachGallery"))) BreachGallery();
    int32 InspectIndex=INDEX_NONE;
    if(FParse::Value(FCommandLine::Get(),TEXT("BreachInspect="),InspectIndex) && Displays.IsValidIndex(InspectIndex))
    {
        bGallery=true;
        auto* Display=Displays[InspectIndex].Get();
        FVector Face=Display->Visual->Bounds.Origin+FVector(0,0,Display->Visual->Bounds.BoxExtent.Z*.8f);
        if(InspectIndex==3) Face=Display->Visual->GetBoneLocationByName(FName(TEXT("Bip001-Head")),EBoneSpaces::WorldSpace)+FVector(0,0,7);
        float InspectYaw=0;
        FParse::Value(FCommandLine::Get(),TEXT("BreachInspectYaw="),InspectYaw);
        const FVector Position=Face+FRotator(0,InspectYaw,0).RotateVector(FVector(115,0,2));
        auto* PortraitCamera=GetWorld()->SpawnActor<ACameraActor>(Position,(Face-Position).Rotation());
        PortraitCamera->GetCameraComponent()->SetFieldOfView(28.f);
        auto* PC=UGameplayStatics::GetPlayerController(this,0);
        PC->bAutoManageActiveCameraTarget=false;
        PC->SetViewTarget(PortraitCamera);
        if(PC->GetHUD()) PC->GetHUD()->bShowHUD=false;
        if(auto* P=Cast<ABreachCharacter>(PC->GetPawn())) P->SetActorHiddenInGame(true);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("BreachAutoPlay"))) BreachAutoPlay();
    if(FParse::Param(FCommandLine::Get(),TEXT("BreachViewTest")))
    {
        bGallery=true;
        int32 Operator=0; float Pitch=0;
        FParse::Value(FCommandLine::Get(),TEXT("BreachOperator="),Operator);
        FParse::Value(FCommandLine::Get(),TEXT("BreachLookPitch="),Pitch);
        FTimerHandle ViewSetup;
        GetWorldTimerManager().SetTimer(ViewSetup,[this,Operator,Pitch]()
        {
            if(auto* P=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
            {
                P->SelectOperator(FMath::Clamp(Operator,0,3));
                P->GetController()->SetControlRotation(FRotator(Pitch,0,0));
                if(FParse::Param(FCommandLine::Get(),TEXT("BreachAim"))) P->SetAim(true);
            }
        },.2f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("BreachDeathPreview")))
    {
        bGallery=true;
        auto* PC=UGameplayStatics::GetPlayerController(this,0);
        const FVector Location(-1130,0,360),Target(-420,0,70);
        auto* Preview=GetWorld()->SpawnActor<ACameraActor>(Location,(Target-Location).Rotation());
        Preview->GetCameraComponent()->SetFieldOfView(80);
        PC->bAutoManageActiveCameraTarget=false; PC->SetViewTarget(Preview);
        PC->GetHUD()->bShowHUD=false; PC->GetPawn()->SetActorHiddenInGame(true);
        for(int32 I=0;I<4;++I)
        {
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Enemy=GetWorld()->SpawnActor<ABreachEnemy>(FVector(-450,-390+I*260,89),FRotator(0,180,0),Params);
            Enemy->Configure(I,1);
            FTimerHandle Kill;
            GetWorldTimerManager().SetTimer(Kill,[Enemy,PC]() { UGameplayStatics::ApplyDamage(Enemy,1000,PC,PC->GetPawn(),UDamageType::StaticClass()); },4.f,false);
        }
        const float Times[]={3.5f,4.25f,4.65f,5.05f,5.6f,9.f};
        for(int32 I=0;I<6;++I)
        {
            FTimerHandle Frame;
            GetWorldTimerManager().SetTimer(Frame,[I]() { FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Saved/Death_%02d.png"),I),false,false); },Times[I],false);
        }
        FTimerHandle Exit;
        GetWorldTimerManager().SetTimer(Exit,[PC]() { PC->ConsoleCommand(TEXT("quit")); },10.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("BreachTest")))
    {
        FTimerHandle TestTimer;
        GetWorldTimerManager().SetTimer(TestTimer,this,&ABreachGameMode::RunSmokeTest,3.f,false);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("BreachCapture")))
    {
        FTimerHandle CaptureTimer;
        float CaptureTime=20.f;
        FParse::Value(FCommandLine::Get(),TEXT("BreachCaptureTime="),CaptureTime);
        GetWorldTimerManager().SetTimer(CaptureTimer,[this]()
        {
            FString CaptureName=bGallery?TEXT("Gallery"):TEXT("Arena");
            FParse::Value(FCommandLine::Get(),TEXT("BreachCaptureName="),CaptureName);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/TEXT("Saved")/(CaptureName+TEXT(".png")),true,false);
            FTimerHandle ExitTimer;
            GetWorldTimerManager().SetTimer(ExitTimer,[this]() { UGameplayStatics::GetPlayerController(this,0)->ConsoleCommand(TEXT("quit")); },3.f,false);
        },FMath::Max(1.f,CaptureTime),false);
    }
    UE_LOG(LogTemp,Display,TEXT("BREACH_READY: 4 character bays, FPS pawn and arena initialized"));
}
void ABreachGameMode::Tick(float Dt)
{
    Super::Tick(Dt);
    NoticeTime=FMath::Max(0.f,NoticeTime-Dt);
    if(bGallery || bGameOver) return;
    if(EnemiesAlive==0 && RemainingToSpawn==0)
    {
        Intermission-=Dt;
        if(Intermission<=0) StartWave();
    }
    if(RemainingToSpawn>0)
    {
        SpawnDelay-=Dt;
        if(SpawnDelay<=0 && EnemiesAlive<8) { SpawnEnemy(); SpawnDelay=1.8f; }
    }
    if(bAutoPlay)
    {
        AutoTime+=Dt;
        if(auto* P=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
        {
            P->Health=100;
            ABreachEnemy* Target=nullptr;
            for(TActorIterator<ABreachEnemy> It(GetWorld());It;++It) if(!It->bDisplayOnly && !It->bDefeated) { Target=*It; break; }
            if(Target)
            {
                const FVector Aim=Target->GetActorLocation()+FVector(0,0,25)-P->Camera->GetComponentLocation();
                P->GetController()->SetControlRotation(Aim.Rotation());
                P->Fire();
                // Move slightly to test collision/movement while firing.
                P->AddMovementInput(P->GetActorRightVector(),FMath::Sin(AutoTime)*.4f);
            }
            if(AutoTime>35)
            {
                FString Result=FString::Printf(TEXT("AUTOPLAY: wave=%d kills=%d shots=%d hits=%d ammo=%d reserve=%d"),Wave,Kills,P->ShotsFired,P->ShotsHit,P->Ammo,P->Reserve);
                FFileHelper::SaveStringToFile(Result,*(FPaths::ProjectDir()/TEXT("Saved/autoplay_result.txt")));
                UE_LOG(LogTemp,Display,TEXT("%s"),*Result);
                UGameplayStatics::GetPlayerController(this,0)->ConsoleCommand(TEXT("quit"));
            }
        }
    }
}
void ABreachGameMode::StartWave()
{
    ++Wave; RemainingToSpawn=4+Wave*2; SpawnDelay=0; Intermission=6;
    Notice=FString::Printf(TEXT("WAVE %02d  /  HOSTILE PROJECTIONS ACTIVE"),Wave); NoticeTime=3.5f;
    if(auto* P=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
    {
        P->Health=FMath::Min(100.f,P->Health+25);
        P->Reserve=FMath::Min(300,P->Reserve+60);
    }
}
void ABreachGameMode::SpawnEnemy()
{
    auto* P=UGameplayStatics::GetPlayerPawn(this,0);
    const FVector Points[]={FVector(1900,-1150,100),FVector(1900,1150,100),FVector(1000,-1400,100),FVector(1000,1400,100),FVector(-600,-1450,100),FVector(-600,1450,100)};
    FVector Location=Points[FMath::RandRange(0,5)];
    for(int32 i=0;i<6 && P && FVector::Dist2D(Location,P->GetActorLocation())<700;++i) Location=Points[i];
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    auto* E=GetWorld()->SpawnActor<ABreachEnemy>(Location,FRotator(0,180,0),Params);
    if(E) { E->Configure((Wave+RemainingToSpawn)%4,Wave); --RemainingToSpawn; ++EnemiesAlive; }
}
void ABreachGameMode::EnemyDefeated(ABreachEnemy* E,bool Head)
{
    EnemiesAlive=FMath::Max(0,EnemiesAlive-1); ++Kills; Score+=Head?150:100;
    Notice=Head?TEXT("PRECISION HIT  +150"):TEXT("PROJECTION CLEARED  +100"); NoticeTime=1.3f;
    if(auto* P=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
    {
        P->Reserve=FMath::Min(300,P->Reserve+12);
        if(Kills%3==0) P->Health=FMath::Min(100.f,P->Health+12);
    }
    if(EnemiesAlive==0 && RemainingToSpawn==0)
    {
        Intermission=6; Notice=TEXT("SECTOR CLEAR  /  HEALTH & AMMO REPLENISHED"); NoticeTime=5;
    }
}
void ABreachGameMode::EndRun()
{
    bGameOver=true; Notice=TEXT("SIMULATION COMPLETE"); NoticeTime=1000;
}
void ABreachGameMode::BreachSmokeTest() { RunSmokeTest(); }
void ABreachGameMode::BreachGallery()
{
    bGallery=true;
    if(auto* P=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))
    {
        P->SetActorLocation(FVector(-1100,0,100));
        P->GetController()->SetControlRotation(FRotator(-2,180,0));
    }
}
void ABreachGameMode::BreachAutoPlay() { bAutoPlay=true; Intermission=1; }
void ABreachGameMode::RunSmokeTest()
{
    FString Report; int32 Failed=0;
    const auto Check=[&](bool Pass,const TCHAR* Name)
    {
        Report+=FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),Name);
        if(!Pass) ++Failed;
    };
    auto* P=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    Check(P!=nullptr,TEXT("FPS player spawned"));
    for(int32 i=0;i<4;++i) Check(Breach::CharacterMesh(i)!=nullptr,*FString::Printf(TEXT("Character %s loaded"),Breach::Keys[i]));
    Check(Displays.Num()==4,TEXT("All four gallery characters spawned"));
    if(P)
    {
        Check(P->GetActorLocation().Z>85 && P->GetActorLocation().Z<140,TEXT("Player supported by arena floor"));
        for(int32 I=0;I<4;++I)
        {
            P->SelectOperator(I); P->UpdateOperatorPose(0);
            Check(P->HasFirstPersonRig(),*FString::Printf(TEXT("%s first-person rig and arms loaded"),Breach::Keys[I]));
            Check(P->GripError()<5.f,*FString::Printf(TEXT("%s hands reach both weapon grips (%.2f cm)"),Breach::Keys[I],P->GripError()));
        }
        P->SelectOperator(3); Check(P->Body->GetSkinnedAsset()==Breach::CharacterMesh(3),TEXT("Operator switching uses supplied mesh"));
        P->Ammo=3; P->Reserve=7; P->Reload();
        Check(P->bReloading,TEXT("Reload starts"));
        P->FinishReload(); Check(P->Ammo==10 && P->Reserve==0,TEXT("Partial reload conserves ammo"));
        P->Ammo=30; P->Reserve=180;
        P->SetActorLocation(FVector(-1400,0,100));
        P->GetController()->SetControlRotation(FRotator(0,0,0));
        const float X=P->Camera->GetComponentLocation().X;
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E=GetWorld()->SpawnActor<ABreachEnemy>(FVector(X+360,0,100),FRotator(0,180,0),Params);
        E->Configure(0,1); ++EnemiesAlive;
        const float Before=E->Health;
        P->Fire();
        Check(P->Ammo==29,TEXT("Fire consumes one round"));
        Check(E->Health<Before && P->ShotsHit>0,TEXT("Hitscan damages target through real collision"));
        UGameplayStatics::ApplyDamage(E,1000,P->GetController(),P,UDamageType::StaticClass());
        Check(E->bDefeated && Kills>0,TEXT("Enemy defeat increments score"));
        for(int32 I=0;I<4;++I)
        {
            auto* Fallen=I==0?E:GetWorld()->SpawnActor<ABreachEnemy>(FVector(-1000,-300+I*200,89),FRotator(0,180,0),Params);
            if(I) { Fallen->Configure(I,1); UGameplayStatics::ApplyDamage(Fallen,1000,P->GetController(),P,UDamageType::StaticClass()); }
            const FVector Scale=Fallen->Visual->GetRelativeScale3D();
            const int32 ScoreBefore=Score;
            Fallen->UpdateDeathPose(.7f);
            Check(IsValid(Fallen) && Fallen->Visual->GetRelativeScale3D().Equals(Scale),*FString::Printf(TEXT("%s fall keeps character size"),Breach::Keys[I]));
            Fallen->UpdateDeathPose(.8f);
            FBreachPose Rig; Rig.Init(Breach::CharacterMesh(I),I);
            const FVector Pelvis=Fallen->Visual->GetBoneLocationByName(Fallen->Visual->GetBoneName(Rig.Bone(EBreachBone::Pelvis)),EBoneSpaces::WorldSpace);
            Check(Pelvis.Z>=0 && Pelvis.Z<45,*FString::Printf(TEXT("%s body reaches floor (pelvis %.2f cm)"),Breach::Keys[I],Pelvis.Z));
            UGameplayStatics::ApplyDamage(Fallen,1000,P->GetController(),P,UDamageType::StaticClass());
            Check(Score==ScoreBefore && Fallen->GetCapsuleComponent()->GetCollisionEnabled()==ECollisionEnabled::NoCollision,*FString::Printf(TEXT("%s defeated body cannot score twice or block player"),Breach::Keys[I]));
        }
        UGameplayStatics::ApplyDamage(P,25,E->GetController(),E,UDamageType::StaticClass());
        Check(P->Health==75,TEXT("Player damage updates health"));
        UGameplayStatics::ApplyDamage(P,1000,E->GetController(),E,UDamageType::StaticClass());
        Check(bGameOver,TEXT("Zero health ends simulation"));
    }
    Report+=FString::Printf(TEXT("FAILURES=%d\n"),Failed);
    FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectDir()/TEXT("Saved/smoke_test.txt")));
    UE_LOG(LogTemp,Display,TEXT("BREACH_TEST\n%s"),*Report);
    if(FParse::Param(FCommandLine::Get(),TEXT("BreachTest"))) UGameplayStatics::GetPlayerController(this,0)->ConsoleCommand(TEXT("quit"));
}

