#include "BreachGame.h"
#include "BreachWeapons.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

void ABreachGameMode::RunWeaponTest()
{
    auto* P=Cast<ABreachCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!P || !PC) return;
    // Optional reference review: every firearm on every firearm-capable rig,
    // from front, side, owner hip and owner ADS, after each pose has settled.
    if(FParse::Param(FCommandLine::Get(),TEXT("BreachGunPoseReview")))
    {
        struct FReview { int32 Step=0,Failures=0; FString Report; FTimerHandle Timer; };
        auto Review=MakeShared<FReview>();
        auto* Preview=GetWorld()->SpawnActor<ACameraActor>();
        Preview->GetCameraComponent()->SetFieldOfView(38.f);
        P->SetActorLocation(FVector(-1200,-1250,94));
        PC->SetControlRotation(FRotator::ZeroRotator);
        PC->bAutoManageActiveCameraTarget=false;
        PC->GetHUD()->bShowHUD=false;
        GetWorldTimerManager().SetTimer(Review->Timer,[=,this]()
        {
            if(Review->Step==108)
            {
                Review->Report+=FString::Printf(TEXT("FAILURES=%d\n"),Review->Failures);
                FFileHelper::SaveStringToFile(Review->Report,*(FPaths::ProjectDir()/TEXT("Saved/gun_pose_review.txt")));
                GetWorldTimerManager().ClearTimer(Review->Timer);
                Preview->Destroy();PC->ConsoleCommand(TEXT("quit"));return;
            }
            const int32 Entry=Review->Step/9,Stage=Review->Step%9;
            const int32 Operator=Entry/4==0?0:Entry/4+1,Weapon=Entry%4;
            const FVector Focus=P->GetActorLocation()+FVector(10,0,32);
            const auto PositionCamera=[&](FVector Offset)
            {
                Preview->SetActorLocationAndRotation(Focus+Offset,(-Offset).Rotation());
                PC->SetViewTarget(Preview);
            };
            if(Stage==0)
            {
                P->SelectOperator(Operator);P->SelectWeapon(Weapon);P->DrawRifle();P->SetAim(false);
                PositionCamera(FVector(310,0,0));
            }
            else
            {
                const TCHAR* Name=Stage==1?TEXT("Front"):Stage==2?TEXT("Side"):Stage==3?TEXT("Hip"):
                    Stage==4?TEXT("Aim"):Stage==5?TEXT("AimRepeat"):Stage==6?TEXT("HipReturn"):
                    Stage==7?TEXT("AimFront"):TEXT("AimSide");
                const float OwnerError=P->GripError(),WorldError=P->GripError(false);
                const bool Pass=OwnerError<6.f && WorldError<6.f;
                Review->Failures+=!Pass;
                Review->Report+=FString::Printf(TEXT("%s Operator %d %s %s wrist reach: owner %.2f cm, world %.2f cm\n"),
                    Pass?TEXT("PASS"):TEXT("FAIL"),Operator,Breach::WeaponNames[Weapon],Name,OwnerError,WorldError);
                if(Stage>=4)
                {
                    const float Expected=Stage==6?0.f:1.f;
                    const bool Settled=FMath::IsNearlyEqual(P->GetAimBlend(),Expected,.001f) &&
                        FMath::IsNearlyEqual(P->Camera->FieldOfView,Expected>0?P->AimFieldOfView:P->BaseFieldOfView,.01f);
                    Review->Failures+=!Settled;
                    Review->Report+=FString::Printf(TEXT("%s %s %s synchronized ADS endpoint\n"),
                        Settled?TEXT("PASS"):TEXT("FAIL"),Breach::WeaponNames[Weapon],Name);
                }
                if(FParse::Param(FCommandLine::Get(),TEXT("BreachWeaponCapture")))
                    FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Saved/GunPose_%d_%s_%s.png"),Operator,Breach::WeaponNames[Weapon],Name),false,false);
                // Schedule the next view on a separate tick, after this frame's capture.
                FTimerHandle Next;
                GetWorldTimerManager().SetTimer(Next,[=,this]()
                {
                    if(Stage==1)
                    {
                        const FVector Offset(0,310,0);
                        Preview->SetActorLocationAndRotation(Focus+Offset,(-Offset).Rotation());
                    }
                    if(Stage==2) PC->SetViewTarget(P);
                    if(Stage==3 || Stage==4)
                    {
                        P->SetAim(Stage==3);
                        FTimerHandle Mid;
                        GetWorldTimerManager().SetTimer(Mid,[=]()
                        {
                            const float Blend=P->GetAimBlend();
                            const bool Moving=Blend>0.f && Blend<1.f &&
                                FMath::IsNearlyEqual(P->Camera->FieldOfView,FMath::Lerp(P->BaseFieldOfView,P->AimFieldOfView,Blend),.01f);
                            Review->Failures+=!Moving;
                            Review->Report+=FString::Printf(TEXT("%s %s %s mid-transition zoom, blend %.3f\n"),
                                Moving?TEXT("PASS"):TEXT("FAIL"),Breach::WeaponNames[Weapon],Stage==3?TEXT("enter"):TEXT("reverse"),Blend);
                            if(FParse::Param(FCommandLine::Get(),TEXT("BreachWeaponCapture")))
                                FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(
                                    TEXT("Saved/GunPose_%d_%s_%s.png"),Operator,Breach::WeaponNames[Weapon],Stage==3?TEXT("Lift"):TEXT("Reverse")),false,false);
                            if(Stage==4)
                            {
                                P->SetAim(true);
                                const bool Continuous=FMath::IsNearlyEqual(P->GetAimBlend(),Blend,.001f);
                                Review->Failures+=!Continuous;
                                Review->Report+=FString::Printf(TEXT("%s %s reversal preserves current pose\n"),Continuous?TEXT("PASS"):TEXT("FAIL"),Breach::WeaponNames[Weapon]);
                            }
                        },.10f,false);
                    }
                    if(Stage==5) P->SetAim(false);
                    if(Stage==6 || Stage==7)
                    {
                        P->SetAim(true);
                        const FVector Offset=Stage==6?FVector(310,0,0):FVector(0,310,0);
                        Preview->SetActorLocationAndRotation(Focus+Offset,(-Offset).Rotation());
                        PC->SetViewTarget(Preview);
                    }
                },.2f,false);
            }
            ++Review->Step;
        },.8f,true,1.f);
        return;
    }
    struct FRun
    {
        FString Report;
        int32 Failures=0,Shots=0,Reserve=0,M4Reserve=0,MP5Reserve=0,AA12Reserve=0,Hits=0;
        float Spread=0,AKPitch=0,M4Spread=0,M4Pitch=0,MP5Spread=0,AA12Spread=0;
        TWeakObjectPtr<ABreachEnemy> Target;
        TWeakObjectPtr<ACameraActor> WorldCamera;
    };
    auto Run=MakeShared<FRun>();
    const auto Check=[Run](bool Pass,const FString& Message)
    {
        Run->Report+=FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),*Message);
        if(!Pass) ++Run->Failures;
    };
    const auto At=[this](float Delay,TFunction<void()> Fn)
    {
        FTimerHandle Handle;GetWorldTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda(MoveTemp(Fn)),Delay,false);
    };
    const auto Capture=[=](const TCHAR* Weapon,const TCHAR* Name)
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("BreachWeaponCapture")))
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Saved/%s_%s.png"),Weapon,Name),true,false);
    };
    P->SetActorLocation(FVector(-1200,-1250,94));PC->SetControlRotation(FRotator::ZeroRotator);
    At(.4f,[=]()
    {
        Check(P->UsesAK() && P->Ammo==25 && P->MagazineSize==25,TEXT("Default ordinary operator starts with a full 25-round AK"));
        for(int32 Operator:{0,2,3})
        {
            P->SelectOperator(Operator);P->DrawRifle();
            Check(P->AK->IsVisible() && P->WorldAK->IsVisible() && P->WorldAK->bCastHiddenShadow && P->GripError()<6.f,
                FString::Printf(TEXT("Operator %d holds the AK in both representations"),Operator));
        }
        P->SelectOperator(0);P->DrawRifle();Run->Spread=P->GetShotSpread();
        Check(Run->Spread>.004f*3,TEXT("AK hip spread exceeds prototype spread"));
        P->SetAim(true);Check(P->GetShotSpread()<Run->Spread && P->GetShotSpread()>.001f*3,TEXT("ADS tightens AK spread while retaining more dispersion than the prototype"));
        P->SetAim(false);Capture(TEXT("AK"),TEXT("Hip"));
    });
    At(.9f,[=]() { P->SetAim(true); });
    At(1.5f,[=]() { Capture(TEXT("AK"),TEXT("Aim"));P->SetAim(false); });
    At(2.f,[=,this]()
    {
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Target=GetWorld()->SpawnActor<ABreachEnemy>(P->Camera->GetComponentLocation()+FVector(180,0,0),FRotator::ZeroRotator,Params);
        Target->SetActorTickEnabled(false);Target->GetCharacterMovement()->DisableMovement();Target->Health=10000;
        Run->Target=Target;Run->Shots=P->ShotsFired;Run->Reserve=P->Reserve;
        PC->SetControlRotation(FRotator::ZeroRotator);P->Fire();
        Check(FMath::IsNearlyEqual(Target->Health,10000.f-38.f),TEXT("An actual AK body hit deals 38 damage"));
    });
    At(2.09f,[=]()
    {
        Run->AKPitch=FMath::Abs(FRotator::NormalizeAxis(PC->GetControlRotation().Pitch));
        Check(Run->AKPitch>.3f,TEXT("AK shot produces visible camera recoil"));
        Capture(TEXT("AK"),TEXT("Recoil"));
    });
    // Leave enough time after the recoil capture for render-thread screenshot work;
    // otherwise several timer callbacks can collapse into one frame and hit the fire-rate guard.
    for(int32 Shot=1;Shot<25;++Shot)
        At(2.7f+Shot*.16f,[=]() { PC->SetControlRotation(FRotator::ZeroRotator);P->Fire(); });
    At(3.5f,[=]() { Check(P->GetShotSpread()>Run->Spread,TEXT("Sustained fire increases the AK dispersion cone")); });
    At(6.9f,[=]()
    {
        Check(P->Ammo==0 && P->ShotsFired-Run->Shots==25,TEXT("Exactly 25 shots empty the AK magazine"));
        P->Fire();Check(P->bReloading && P->ShotsFired-Run->Shots==25,TEXT("Empty AK begins reloading without firing a 26th shot"));
    });
    At(7.5f,[=]() { Capture(TEXT("AK"),TEXT("Reload")); });
    At(8.7f,[=]()
    {
        Check(P->Ammo==25 && P->Reserve==Run->Reserve-25 && !P->bReloading,TEXT("Reload transfers exactly 25 rounds from reserve"));
        Check(FMath::IsNearlyEqual(P->GetShotSpread(),Run->Spread,.0001f),TEXT("AK spread recovers when shooting stops"));
        const int32 Total=P->GetTotalGunAmmo();P->SelectWeapon(1);P->SelectWeapon(0);
        Check(P->Ammo==25 && P->GetTotalGunAmmo()==Total,TEXT("Switching through M4 does not grant ammunition"));
        P->Ammo=7;P->Reload();P->SelectWeapon(1);
        Check(!P->bReloading && P->UsesM4() && P->M4->IsVisible() && P->WorldM4->bCastHiddenShadow &&
            !P->AK->IsVisible() && !P->WorldAK->CastShadow && !P->WorldAK->bCastHiddenShadow,
            TEXT("Switching to M4 cancels AK reload and disables both AK shadows"));
    });
    At(10.5f,[=]()
    {
        P->SelectWeapon(0);Check(P->Ammo==7 && P->Reserve==Run->Reserve-25,TEXT("Cancelled reload cannot refill the stowed AK"));
        P->HolsterRifle();Check(!P->AK->IsVisible() && !P->WorldAK->CastShadow && !P->WorldAK->bCastHiddenShadow,TEXT("Holstered AK has no ghost shadow"));
        P->DrawRifle();P->SelectOperator(1);P->SelectWeapon(0);
        Check(P->UsesSword() && !P->UsesAK() && !P->UsesM4() && !P->UsesMP5() && !P->UsesAA12() &&
            !P->AK->IsVisible() && !P->M4->IsVisible() && !P->MP5->IsVisible() && !P->AA12->IsVisible() &&
            !P->WorldAK->bCastHiddenShadow && !P->WorldM4->bCastHiddenShadow && !P->WorldMP5->bCastHiddenShadow &&
            !P->WorldAA12->bCastHiddenShadow,
            TEXT("Acheron refuses firearm selection and keeps the blade"));
        P->SelectOperator(0);Check(P->UsesAK() && P->Ammo==7 && P->AK->IsVisible(),TEXT("Leaving Acheron restores AK model and remaining ammo"));
        P->SelectWeapon(1);P->DrawRifle();Run->M4Spread=P->GetShotSpread();
        Check(P->UsesM4() && P->Ammo==30 && P->MagazineSize==Breach::M4MagazineSize &&
            FMath::IsNearlyEqual(P->ShotDamage,Breach::M4Damage) && FMath::IsNearlyEqual(P->FireInterval,Breach::M4FireInterval),
            TEXT("M4 equips with a 30-round magazine, 35 damage and 0.09-second fire interval"));
        Check(P->M4->IsVisible() && P->WorldM4->IsVisible() && P->WorldM4->bCastHiddenShadow &&
            !P->AK->IsVisible() && !P->WorldAK->bCastHiddenShadow && P->GripError()<6.f,
            TEXT("M4 uses both fitted representations without an AK ghost shadow"));
        Check(Run->M4Spread<Breach::AKHipSpread,TEXT("M4 hip spread is tighter than AK hip spread"));
        Capture(TEXT("M4"),TEXT("Hip"));
    });
    At(11.f,[=]()
    {
        P->SetAim(true);
        Check(P->GetShotSpread()<Breach::AKAimSpread && P->GetShotSpread()<Run->M4Spread,
            TEXT("M4 ADS spread is tighter than AK ADS and M4 hip spread"));
        Capture(TEXT("M4"),TEXT("Aim"));P->SetAim(false);
    });
    At(11.5f,[=]()
    {
        if(!Run->Target.IsValid()) return;
        Run->Target->Health=10000.f;Run->Shots=P->ShotsFired;
        PC->SetControlRotation(FRotator::ZeroRotator);P->Fire();
        Check(P->Ammo==29 && P->ShotsFired==Run->Shots+1,TEXT("An M4 shot consumes one of 30 rounds"));
        Check(FMath::IsNearlyEqual(Run->Target->Health,10000.f-Breach::M4Damage),TEXT("An actual M4 body hit deals 35 damage"));
    });
    At(11.59f,[=]()
    {
        Run->M4Pitch=FMath::Abs(FRotator::NormalizeAxis(PC->GetControlRotation().Pitch));
        Check(Run->M4Pitch>.15f && Run->M4Pitch<Run->AKPitch,TEXT("M4 camera recoil is visible but lower than AK recoil"));
        Capture(TEXT("M4"),TEXT("Recoil"));
    });
    for(int32 Shot=1;Shot<6;++Shot)
        At(12.2f+Shot*.1f,[=]()
        {
            PC->SetControlRotation(FRotator::ZeroRotator);P->Fire();
            if(Shot==5) Check(P->GetShotSpread()>Run->M4Spread,TEXT("Sustained M4 fire increases dispersion"));
        });
    At(13.f,[=]()
    {
        P->Ammo=0;Run->M4Reserve=P->Reserve;P->Reload();
    });
    At(13.6f,[=]() { Capture(TEXT("M4"),TEXT("Reload")); });
    At(14.8f,[=,this]()
    {
        Check(P->Ammo==30 && P->Reserve==Run->M4Reserve-30 && !P->bReloading,TEXT("M4 reload fills exactly 30 rounds"));
        P->Ammo=7;P->Reload();P->SelectWeapon(0);
        Check(!P->bReloading && P->UsesAK() && !P->M4->IsVisible() && !P->WorldM4->CastShadow && !P->WorldM4->bCastHiddenShadow,
            TEXT("Switching away cancels M4 reload and disables both M4 shadows"));
        P->SelectWeapon(1);
        Check(P->Ammo==7 && P->UsesM4(),TEXT("Returning to M4 restores its partial magazine without free ammunition"));
        P->HolsterRifle();
        Check(!P->M4->IsVisible() && !P->WorldM4->CastShadow && !P->WorldM4->bCastHiddenShadow,TEXT("Holstered M4 has no ghost shadow"));
        P->DrawRifle();
        auto* Preview=GetWorld()->SpawnActor<ACameraActor>();
        const FVector Focus=P->GetActorLocation()+FVector(0,0,35);
        const FVector Position=Focus+FVector(260,280,100);
        Preview->SetActorLocationAndRotation(Position,(Focus-Position).Rotation());
        Preview->GetCameraComponent()->SetFieldOfView(52.f);
        Run->WorldCamera=Preview;PC->bAutoManageActiveCameraTarget=false;PC->SetViewTarget(Preview);
        PC->GetHUD()->bShowHUD=false;
    });
    At(15.15f,[=]()
    {
        Check(P->WorldM4->IsVisible() && P->WorldM4->CastShadow && P->WorldM4->bCastHiddenShadow,
            TEXT("M4 world representation remains visible with both shadow modes"));
        Capture(TEXT("M4"),TEXT("World"));
    });
    At(15.55f,[=]()
    {
        if(Run->WorldCamera.IsValid()) Run->WorldCamera->Destroy();
        PC->SetViewTarget(P);PC->GetHUD()->bShowHUD=true;
        P->SelectWeapon(2);P->DrawRifle();Run->MP5Spread=P->GetShotSpread();
        Check(P->UsesMP5() && P->Ammo==40 && P->MagazineSize==Breach::MP5MagazineSize &&
            FMath::IsNearlyEqual(P->ShotDamage,Breach::MP5Damage) && FMath::IsNearlyEqual(P->FireInterval,Breach::MP5FireInterval),
            TEXT("MP5 equips with a 40-round magazine, 30 close damage and 0.06-second fire interval"));
        Check(P->MP5->IsVisible() && P->WorldMP5->IsVisible() && P->WorldMP5->bCastHiddenShadow &&
            !P->AK->IsVisible() && !P->M4->IsVisible() && !P->WorldAK->bCastHiddenShadow &&
            !P->WorldM4->bCastHiddenShadow && P->GripError()<6.f,
            TEXT("MP5 uses both fitted representations without rifle ghost shadows"));
        Check(Run->MP5Spread<Breach::M4HipSpread,TEXT("MP5 hip spread is tighter than M4 hip spread"));
        const float AKDPS=Breach::AKDamage/Breach::AKFireInterval;
        const float M4DPS=Breach::M4Damage/Breach::M4FireInterval;
        const float CloseDPS=P->GetShotDamage(500.f)/Breach::MP5FireInterval;
        const float MidDPS=P->GetShotDamage(2000.f)/Breach::MP5FireInterval;
        const float FarDPS=P->GetShotDamage(4000.f)/Breach::MP5FireInterval;
        Check(FMath::IsNearlyEqual(P->GetShotDamage(1000.f),30.f) && FMath::IsNearlyEqual(P->GetShotDamage(1001.f),23.f),
            TEXT("MP5 close damage drops from 30 to 23 immediately beyond 10 metres"));
        Check(CloseDPS>AKDPS && CloseDPS>M4DPS,TEXT("MP5 DPS within 10 metres exceeds both rifles"));
        Check(FMath::Abs(MidDPS-AKDPS)/AKDPS<.08f && FMath::Abs(MidDPS-M4DPS)/M4DPS<.08f,
            TEXT("MP5 DPS from 10 to 30 metres stays within eight percent of both rifles"));
        Check(FarDPS<AKDPS*.6f && FarDPS<M4DPS*.6f,TEXT("MP5 DPS at 40 metres is substantially below both rifles"));
    });
    At(16.05f,[=]() { Capture(TEXT("MP5"),TEXT("Hip")); });
    At(16.2f,[=]() { P->SetAim(true); });
    At(16.6f,[=]()
    {
        Check(P->GetShotSpread()<Breach::M4AimSpread && P->GetShotSpread()<Run->MP5Spread,
            TEXT("MP5 ADS spread is tighter than M4 ADS and MP5 hip spread"));
        Capture(TEXT("MP5"),TEXT("Aim"));P->SetAim(false);
    });
    At(16.9f,[=]()
    {
        if(!Run->Target.IsValid()) return;
        Run->Target->Health=10000.f;Run->Shots=P->ShotsFired;
        PC->SetControlRotation(FRotator::ZeroRotator);P->Fire();
        Check(P->Ammo==39 && P->ShotsFired==Run->Shots+1,TEXT("An MP5 shot consumes one of 40 rounds"));
        Check(FMath::IsNearlyEqual(Run->Target->Health,10000.f-Breach::MP5Damage),TEXT("An actual close MP5 body hit deals 30 damage"));
    });
    At(16.99f,[=]()
    {
        const float Pitch=FMath::Abs(FRotator::NormalizeAxis(PC->GetControlRotation().Pitch));
        Check(Pitch>.1f && Pitch<Run->M4Pitch,TEXT("MP5 camera recoil is visible but lower than M4 recoil"));
        Capture(TEXT("MP5"),TEXT("Recoil"));
    });
    for(int32 Shot=1;Shot<7;++Shot)
        At(17.55f+Shot*.07f,[=]()
        {
            PC->SetControlRotation(FRotator::ZeroRotator);P->Fire();
            if(Shot==6) Check(P->GetShotSpread()>Run->MP5Spread,TEXT("Sustained MP5 fire still increases dispersion"));
        });
    At(18.15f,[=]() { P->Ammo=0;Run->MP5Reserve=P->Reserve;P->Reload(); });
    At(18.75f,[=]() { Capture(TEXT("MP5"),TEXT("Reload")); });
    At(19.95f,[=,this]()
    {
        Check(P->Ammo==40 && P->Reserve==Run->MP5Reserve-40 && !P->bReloading,TEXT("MP5 reload fills exactly 40 rounds"));
        P->Ammo=9;P->Reload();P->SelectWeapon(1);
        Check(!P->bReloading && P->UsesM4() && !P->MP5->IsVisible() && !P->WorldMP5->CastShadow && !P->WorldMP5->bCastHiddenShadow,
            TEXT("Switching away cancels MP5 reload and disables both MP5 shadows"));
        P->SelectWeapon(2);
        Check(P->Ammo==9 && P->UsesMP5(),TEXT("Returning to MP5 restores its partial magazine without free ammunition"));
        P->HolsterRifle();
        Check(!P->MP5->IsVisible() && !P->WorldMP5->CastShadow && !P->WorldMP5->bCastHiddenShadow,TEXT("Holstered MP5 has no ghost shadow"));
        P->DrawRifle();
        auto* Preview=GetWorld()->SpawnActor<ACameraActor>();
        const FVector Focus=P->GetActorLocation()+FVector(0,0,35);
        const FVector Position=Focus+FVector(260,280,100);
        Preview->SetActorLocationAndRotation(Position,(Focus-Position).Rotation());
        Preview->GetCameraComponent()->SetFieldOfView(52.f);
        Run->WorldCamera=Preview;PC->SetViewTarget(Preview);PC->GetHUD()->bShowHUD=false;
    });
    At(20.3f,[=]()
    {
        Check(P->WorldMP5->IsVisible() && P->WorldMP5->CastShadow && P->WorldMP5->bCastHiddenShadow,
            TEXT("MP5 world representation remains visible with both shadow modes"));
        Capture(TEXT("MP5"),TEXT("World"));
    });
    At(20.7f,[=]()
    {
        if(Run->WorldCamera.IsValid()) Run->WorldCamera->Destroy();
        PC->SetViewTarget(P);PC->GetHUD()->bShowHUD=true;
        P->SelectWeapon(3);P->DrawRifle();Run->AA12Spread=P->GetShotSpread();
        Check(P->UsesAA12() && P->Ammo==Breach::AA12MagazineSize && P->MagazineSize==Breach::AA12MagazineSize &&
            FMath::IsNearlyEqual(P->ShotDamage,Breach::AA12PelletDamage) && FMath::IsNearlyEqual(P->FireInterval,Breach::AA12FireInterval),
            TEXT("AA12 equips with an 8-shell drum, eight pellets and 0.22-second fire interval"));
        Check(P->AA12->IsVisible() && P->WorldAA12->IsVisible() && P->WorldAA12->bCastHiddenShadow &&
            !P->AK->IsVisible() && !P->M4->IsVisible() && !P->MP5->IsVisible() &&
            !P->WorldAK->bCastHiddenShadow && !P->WorldM4->bCastHiddenShadow && !P->WorldMP5->bCastHiddenShadow,
            TEXT("AA12 uses both representations without another gun's ghost shadow"));
        const float MP5CloseDPS=Breach::MP5Damage/Breach::MP5FireInterval;
        const float AA12CloseDPS=Breach::AA12PelletDamage*Breach::AA12PelletCount/Breach::AA12FireInterval;
        Check(AA12CloseDPS>MP5CloseDPS && AA12CloseDPS<MP5CloseDPS*1.05f,
            TEXT("AA12 full-pellet DPS within 10 metres is only slightly above MP5 DPS"));
        Check(FMath::IsNearlyEqual(P->GetShotDamage(1000.f),14.f) && P->GetShotDamage(1250.f)<8.f &&
            FMath::IsNearlyEqual(P->GetShotDamage(1500.f),1.f) && FMath::IsNearlyEqual(P->GetShotDamage(5000.f),1.f),
            TEXT("AA12 pellet damage rapidly falls from 14 after 10 metres to a minimum of one at 15 metres"));
    });
    At(21.05f,[=]()
    {
        Check(P->GripError()<6.f,FString::Printf(TEXT("AA12 hands reach both weapon grips after the weapon settles (%.2f cm)"),P->GripError()));
        Capture(TEXT("AA12"),TEXT("Hip"));
    });
    At(21.15f,[=]() { P->SetAim(true); });
    At(21.55f,[=]()
    {
        Check(P->GetShotSpread()<Run->AA12Spread && P->GetShotSpread()>Breach::MP5HipSpread,
            TEXT("AA12 ADS tightens the pellet cone while remaining wider than the MP5"));
        Capture(TEXT("AA12"),TEXT("Aim"));P->SetAim(false);
    });
    At(21.85f,[=]()
    {
        if(!Run->Target.IsValid()) return;
        Run->Target->Health=10000.f;Run->Shots=P->ShotsFired;Run->Hits=P->ShotsHit;
        PC->SetControlRotation(FRotator::ZeroRotator);P->Fire();
        Check(P->Ammo==Breach::AA12MagazineSize-1 && P->ShotsFired==Run->Shots+1,
            TEXT("One AA12 trigger pull consumes one shell"));
        Check(P->ShotsHit==Run->Hits+Breach::AA12PelletCount &&
            FMath::IsNearlyEqual(Run->Target->Health,10000.f-Breach::AA12PelletDamage*Breach::AA12PelletCount),
            TEXT("All eight close AA12 pellets trace and deal damage independently"));
    });
    At(21.94f,[=]()
    {
        const float Pitch=FMath::Abs(FRotator::NormalizeAxis(PC->GetControlRotation().Pitch));
        Check(Pitch>Run->AKPitch,TEXT("AA12 produces stronger camera recoil than the AK"));
        Capture(TEXT("AA12"),TEXT("Recoil"));
    });
    At(22.5f,[=]() { P->Ammo=0;Run->AA12Reserve=P->Reserve;P->Reload(); });
    At(23.1f,[=]() { Capture(TEXT("AA12"),TEXT("Reload")); });
    At(24.3f,[=,this]()
    {
        Check(P->Ammo==Breach::AA12MagazineSize && P->Reserve==Run->AA12Reserve-Breach::AA12MagazineSize && !P->bReloading,
            TEXT("AA12 reload fills exactly eight shells"));
        P->Ammo=3;P->Reload();P->SelectWeapon(2);
        Check(!P->bReloading && P->UsesMP5() && !P->AA12->IsVisible() && !P->WorldAA12->CastShadow && !P->WorldAA12->bCastHiddenShadow,
            TEXT("Switching away cancels AA12 reload and disables both AA12 shadows"));
        P->SelectWeapon(3);
        Check(P->Ammo==3 && P->UsesAA12(),TEXT("Returning to AA12 restores its partial drum without free ammunition"));
        P->HolsterRifle();
        Check(!P->AA12->IsVisible() && !P->WorldAA12->CastShadow && !P->WorldAA12->bCastHiddenShadow,
            TEXT("Holstered AA12 has no ghost shadow"));
        P->DrawRifle();
        auto* Preview=GetWorld()->SpawnActor<ACameraActor>();
        const FVector Focus=P->GetActorLocation()+FVector(0,0,35);
        const FVector Position=Focus+FVector(260,280,100);
        Preview->SetActorLocationAndRotation(Position,(Focus-Position).Rotation());
        Preview->GetCameraComponent()->SetFieldOfView(52.f);
        Run->WorldCamera=Preview;PC->SetViewTarget(Preview);PC->GetHUD()->bShowHUD=false;
    });
    At(24.65f,[=]()
    {
        Check(P->WorldAA12->IsVisible() && P->WorldAA12->CastShadow && P->WorldAA12->bCastHiddenShadow,
            TEXT("AA12 world representation remains visible with both shadow modes"));
        Capture(TEXT("AA12"),TEXT("World"));
    });
    At(25.05f,[=]()
    {
        if(Run->Target.IsValid()) Run->Target->Destroy();
        if(Run->WorldCamera.IsValid()) Run->WorldCamera->Destroy();
        Run->Report+=FString::Printf(TEXT("FAILURES=%d\n"),Run->Failures);
        FFileHelper::SaveStringToFile(Run->Report,*(FPaths::ProjectDir()/TEXT("Saved/weapon_test.txt")));
        UE_LOG(LogTemp,Display,TEXT("WEAPON_TEST\n%s"),*Run->Report);
        PC->ConsoleCommand(TEXT("quit"));
    });
}
