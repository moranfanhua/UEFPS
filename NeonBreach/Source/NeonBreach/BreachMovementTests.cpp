#include "BreachGame.h"
#include "BreachMovementComponent.h"
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
    const bool Sword=P->UsesSword();
    P->SetActorLocation(FVector(-1200,-1250,94));
    float LookPitch=0; FParse::Value(FCommandLine::Get(),TEXT("BreachLookPitch="),LookPitch);
    PC->SetControlRotation(FRotator(LookPitch,0,0));
    struct FResults
    {
        FString Text; int32 Failed=0; float StandingEye=0,AirLegReach=0,SlideEntry=0,SlideBoosted=0,SlideEntryLegReach=0,SlideWallX=0,TapSpeed=0,JumpSpeed=0,RampSpeed=0; TWeakObjectPtr<AActor> Roof,Ramp; TWeakObjectPtr<ABreachEnemy> SwordTarget;
        FBox RunEye{ForceInit},CrouchEye{ForceInit},JumpEye{ForceInit},JogEye{ForceInit},SlideEye{ForceInit};
        int32 RunSamples=0,CrouchSamples=0,JumpSamples=0,JogSamples=0,SlideSamples=0;
    };
    auto Results=MakeShared<FResults>();
    FString Prefix=FString::Printf(TEXT("%s_%s"),FParse::Param(FCommandLine::Get(),TEXT("BreachMovementFirstPerson"))?TEXT("MovementFPS"):TEXT("Movement"),Breach::Keys[Index]);
    if(!FMath::IsNearlyZero(LookPitch)) Prefix+=FString::Printf(TEXT("_Pitch%d"),FMath::RoundToInt(LookPitch));
    const auto Check=[Results](bool Pass,const FString& Name)
    {
        Results->Text+=FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),*Name);
        if(!Pass) ++Results->Failed;
    };
    auto* Move=CastChecked<UBreachMovementComponent>(P->GetCharacterMovement());
    const float FastSpeed=Sword?Move->SwordSpeed:Move->UnarmedSpeed;
    const float FlatSlideDuration=(FMath::Min(FastSpeed+Move->SlideEntryBoost,Move->SlideMaxSpeed)-Move->SlideExitSpeed)/FMath::Max(1.f,Move->SlideDeceleration);
    const float SlideTailDelay=FMath::Max(0.f,8.6f+FlatSlideDuration+.35f-9.75f);
    const auto At=[this,SlideTailDelay](float Delay,TFunction<void()> Function)
    {
        // Allow the newly selected mesh and its first animation pose to render
        // before injecting keys, so loading hitches cannot coalesce the tap
        // and the assertion into the same input-processing frame.
        if(Delay>=9.75f) Delay+=SlideTailDelay;
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
        if(Time>=9.05 && Time<9.3) { Results->SlideEye+=Eye; ++Results->SlideSamples; }
    },.016f,true);
    const bool Capture=FParse::Param(FCommandLine::Get(),TEXT("BreachMovementCapture"));
    const auto Screenshot=[Prefix,Capture,P,Check](const TCHAR* Name)
    {
        FBreachPose Rig; Rig.Init(Breach::CharacterMesh(P->OperatorIndex),P->OperatorIndex);
        const FName NeckName=P->Body->GetBoneName(Rig.Bone(EBreachBone::Neck));
        const FVector Collar=P->Body->GetBoneLocationByName(NeckName,EBoneSpaces::WorldSpace);
        Check(FVector::DotProduct(Collar-P->Camera->GetComponentLocation(),P->Camera->GetForwardVector())<0,
            FString::Printf(TEXT("%s collar stays behind the first person eye"),Name));
        if(FCString::Strcmp(Name,TEXT("SlideDownhill"))==0 || FCString::Strcmp(Name,TEXT("SlideUphill"))==0)
        {
            const auto& Floor=P->GetCharacterMovement()->CurrentFloor;
            float Lowest=BIG_NUMBER;
            for(EBreachBone Foot:{EBreachBone::LFoot,EBreachBone::RFoot})
            {
                const FVector Position=P->WorldBody->GetBoneLocationByName(P->WorldBody->GetBoneName(Rig.Bone(Foot)),EBoneSpaces::WorldSpace);
                Lowest=FMath::Min(Lowest,float(FVector::DotProduct(Position-Floor.HitResult.ImpactPoint,Floor.HitResult.ImpactNormal)));
            }
            Check(Floor.IsWalkableFloor() && Lowest>=-1.f,FString::Printf(TEXT("%s world ankles stay above the ramp (%.1f cm clearance)"),Name,Lowest));
        }
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
    Check(P->HasLocomotionAnimations(),TEXT("All locomotion states have animation assets"));
    Check(P->Camera->FieldOfView>=109.f && P->BaseFieldOfView>=109.f && P->AimFieldOfView>66.f,TEXT("All operators use the expanded first-person field of view"));
    At(.1f,[=]() { Results->StandingEye=P->Camera->GetComponentLocation().Z; Key(EKeys::Three,IE_Pressed); });
    At(.16f,[=]() { Key(EKeys::Three,IE_Released); });
    At(.22f,[=,this]()
    {
        Check(P->bUnarmed && P->OperatorIndex==Index,TEXT("3 enters unarmed mode without changing character"));
        Check(!P->WeaponRoot->IsVisible() && !P->WorldWeaponRoot->IsVisible(),TEXT("Both weapon representations are hidden"));
        bool ShadowsOff=true; TArray<USceneComponent*> Parts; P->WorldWeaponRoot->GetChildrenComponents(true,Parts);
        for(auto* Part:Parts) if(auto* WeaponPart=Cast<UStaticMeshComponent>(Part)) ShadowsOff&=!WeaponPart->CastShadow;
        Check(ShadowsOff,TEXT("Holstered rifle does not cast a ghost shadow"));
        const int32 Ammo=P->Ammo;
        if(Sword)
        {
            Check(P->HasSwordRig() && P->Sword->IsVisible() && P->WorldSword->IsVisible() && P->Scabbard->IsVisible() && P->WorldScabbard->IsVisible(),TEXT("Acheron carries the supplied blade and scabbard in both views"));
            Check(FVector::Distance(P->Sword->GetBoneLocationByName(TEXT("bone_002"),EBoneSpaces::WorldSpace),P->Scabbard->GetBoneLocationByName(TEXT("bone_003"),EBoneSpaces::WorldSpace))<1.f && P->Sword->GetComponentScale().X<1.f,TEXT("Acheron swings the reduced-size sword with its scabbard fitted"));
            Check(P->FirstPersonSwordMotionScale<.5f,TEXT("Acheron's owner-view sword motion is substantially reduced"));
            Check(Move->GetTargetMoveSpeed()==Move->SwordSpeed && Move->SwordSpeed>Move->UnarmedSpeed,TEXT("Acheron sword speed is faster than unarmed running"));
            PC->SetControlRotation(FRotator::ZeroRotator);
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Target=GetWorld()->SpawnActor<ABreachEnemy>(P->GetActorLocation()+P->Camera->GetForwardVector()*180.f,P->GetActorRotation(),Params);
            Target->Configure(0,1);Target->AttackCooldown=999.f;Target->GetCharacterMovement()->DisableMovement();Results->SwordTarget=Target;
            P->Fire();P->Reload();P->SetAim(true);
            PC->SetControlRotation(FRotator(LookPitch,0,0));
            Check(P->Ammo==Ammo && P->IsSwordAttacking() && !P->bAiming && !P->bReloading,TEXT("Acheron attacks without rifle ammo, aiming or reload"));
        }
        else
        {
            P->Fire(); P->Reload(); P->SetAim(true);
            Check(P->Ammo==Ammo && !P->bAiming && !P->bReloading,TEXT("Unarmed mode blocks shooting aiming and reload"));
        }
        Key(EKeys::W,IE_Pressed);
    });
    At(.35f,[=]() { if(Sword) { Check(P->IsSwordAttacking(),TEXT("Acheron remains in the slash animation through the damage window"));Screenshot(TEXT("SwordAttack")); } });
    At(.55f,[=]()
    {
        if(Sword) Check(Results->SwordTarget.IsValid() && Results->SwordTarget->bDefeated && P->SwordDamage>P->ShotDamage*5.f && P->ShotsHit>0,TEXT("Acheron slash lands at melee range with far higher damage than a bullet"));
    });
    At(.95f,[=]() { Screenshot(TEXT("Run")); });
    At(1.18f,[=]()
    {
        Check(P->GetVelocity().Size2D()>(Sword?Move->UnarmedSpeed+10.f:700.f) && P->LocomotionState==EBreachLocomotion::Sprint,Sword?TEXT("W with Acheron's sword exceeds unarmed speed and uses the sprint animation"):TEXT("W in unarmed mode reaches sprint speed and animation"));
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
    const auto LegReach=[P]()
    {
        FBreachPose Rig;Rig.Init(Breach::CharacterMesh(P->OperatorIndex),P->OperatorIndex);
        const auto Height=[&](EBreachBone Bone) { return P->WorldBody->GetBoneLocationByName(P->WorldBody->GetBoneName(Rig.Bone(Bone)),EBoneSpaces::WorldSpace).Z; };
        return float(Height(EBreachBone::Pelvis)-(Height(EBreachBone::LFoot)+Height(EBreachBone::RFoot))*.5);
    };
    At(1.83f,[=]() { Check(P->LocomotionState==EBreachLocomotion::JumpLoop,TEXT("Takeoff transitions to airborne pose")); Results->AirLegReach=LegReach();Screenshot(TEXT("JumpLoop")); });
    At(2.25f,[=]()
    {
        Check(P->GetVelocity().Z<0 && P->LocomotionState==EBreachLocomotion::JumpLoop,TEXT("Female jump remains airborne during descent"));
        Check(LegReach()>Results->AirLegReach+10.f,TEXT("Female jump extends legs for landing instead of repeating the knee tuck"));
        Screenshot(TEXT("JumpFall"));
    });
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
        if(Sword)
        {
            Check(P->bUnarmed && !P->WeaponRoot->IsVisible() && P->Sword->IsVisible(),TEXT("1 cannot give Acheron a rifle or remove her blade"));
            const int32 Ammo=P->Ammo;P->Fire();Check(P->Ammo==Ammo,TEXT("Acheron slash never consumes rifle ammunition"));
            P->Ammo=5;P->Reserve=100;P->Reload();Check(!P->bReloading,TEXT("Acheron has no rifle reload state"));Key(EKeys::Three,IE_Pressed);
        }
        else
        {
            Check(!P->bUnarmed && P->WeaponRoot->IsVisible() && P->WorldWeaponRoot->IsVisible(),TEXT("1 restores rifle"));
            const int32 Ammo=P->Ammo; P->Fire(); Check(P->Ammo==Ammo-1,TEXT("Restored rifle can fire"));
            P->Ammo=5; P->Reserve=100; P->Reload();
            Check(P->bReloading,TEXT("Armed reload starts")); Key(EKeys::Three,IE_Pressed);
        }
    });
    At(5.16f,[=]() { Key(EKeys::Three,IE_Released); P->SelectOperator(2); });
    At(5.4f,[=]() { Check(P->OperatorIndex==2 && P->bUnarmed==!Sword && P->HasLocomotionAnimations(),Sword?TEXT("Leaving Acheron restores the previous rifle loadout"):TEXT("Changing character preserves unarmed mode")); });
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
    At(7.25f,[=]()
    {
        P->GetCharacterMovement()->StopMovementImmediately();
        P->SetActorLocation(FVector(-1200,-1250,94));PC->SetControlRotation(FRotator(LookPitch,0,0));
        if(Sword) P->SelectOperator(0);
        P->SetUnarmed(false);Key(EKeys::W,IE_Pressed);
        const auto* Move=CastChecked<UBreachMovementComponent>(P->GetCharacterMovement());
        Check(Move->SlideEntrySpeed>Move->RifleSpeed && Move->SlideEntrySpeed<Move->UnarmedSpeed,TEXT("Slide threshold lies between rifle and unarmed speeds"));
        Check(Move->SwordSpeed>Move->UnarmedSpeed,TEXT("Acheron sword movement exceeds the former unarmed speed"));
    });
    At(7.7f,[=]() { Check(P->GetVelocity().Size2D()>490,TEXT("Rifle movement reaches normal speed before crouching"));Key(EKeys::LeftControl,IE_Pressed); });
    At(7.85f,[=]() { Check(P->bIsCrouched && !P->IsSliding(),TEXT("Ctrl at rifle speed crouches without a slide"));Key(EKeys::W,IE_Released);Key(EKeys::LeftControl,IE_Released); });
    At(8.1f,[=]() { if(Sword) P->SelectOperator(Index);P->SetUnarmed(true);Key(EKeys::W,IE_Pressed); });
    At(8.6f,[=]() { Results->SlideEntry=P->GetVelocity().Size2D();Key(EKeys::LeftControl,IE_Pressed); });
    At(8.66f,[=]()
    {
        Check(P->IsSliding() && P->bIsCrouched,TEXT("Ctrl at unarmed speed starts a slide with crouched collision"));
        Results->SlideBoosted=P->GetVelocity().Size2D();
        Results->SlideEntryLegReach=LegReach();
        Check(Results->SlideBoosted>Results->SlideEntry && Results->SlideBoosted<=Results->SlideEntry+Move->SlideEntryBoost+1,TEXT("Slide entry adds the configured forward speed boost"));
    });
    At(8.78f,[=]() { Screenshot(TEXT("SlideStart")); });
    At(8.9f,[=]() { Key(EKeys::W,IE_Released);Key(EKeys::D,IE_Pressed); });
    At(9.35f,[=]()
    {
        const float Turn=P->GetVelocity().Rotation().Yaw;
        Check(P->IsSliding() && P->GetVelocity().X>250 && P->GetVelocity().Y>30 && Turn<Move->SlideTurnRate*.55f,TEXT("Slide steering changes direction gradually without snapping to strafe input"));
        Check(P->GetVelocity().Size2D()<Results->SlideBoosted && P->LocomotionState==EBreachLocomotion::Slide,TEXT("Slide slows down and uses the slide animation"));
        Check(LegReach()<Results->SlideEntryLegReach-10.f,TEXT("Running Slide lowers the hips from entry into an extended-leg slide"));
        StableEye(Results->SlideEye,Results->SlideSamples,TEXT("Slide"));
        FBreachPose Rig;Rig.Init(Breach::CharacterMesh(Index),Index);
        const FName Head=P->WorldBody->GetBoneName(Rig.Bone(EBreachBone::Head));
        Check(P->WorldBody->GetBoneTransformByName(Head,EBoneSpaces::ComponentSpace).GetScale3D().GetMin()>.9f && P->WorldBody->CastShadow && P->WorldBody->bOwnerNoSee,
            TEXT("Sliding world body retains full head and owner-hidden shadow"));
        Check(P->Body->GetBoneTransformByName(Head,EBoneSpaces::ComponentSpace).GetScale3D().IsNearlyZero(),TEXT("Sliding owner body still hides its head"));
        Screenshot(TEXT("Slide"));
    });
    At(9.75f,[=]() { Check(!P->IsSliding() && P->bIsCrouched,TEXT("Holding Ctrl after slide ends remains crouched without retriggering"));Screenshot(TEXT("SlideExit"));Key(EKeys::D,IE_Released); });
    At(9.85f,[=,this]()
    {
        auto* Roof=GetWorld()->SpawnActor<AActor>();auto* Collision=NewObject<UBoxComponent>(Roof);Roof->SetRootComponent(Collision);
        Collision->SetBoxExtent(FVector(90,90,15));Collision->SetCollisionProfileName(TEXT("BlockAll"));Collision->RegisterComponent();
        const float Floor=P->GetActorLocation().Z-P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Roof->SetActorLocation(FVector(P->GetActorLocation().X,P->GetActorLocation().Y,Floor+130));Results->Roof=Roof;
        Key(EKeys::LeftControl,IE_Released);
    });
    At(10.05f,[=]() { Check(P->bIsCrouched && !P->IsSliding(),TEXT("Slide exit respects low ceiling instead of forcing standing"));if(Results->Roof.IsValid()) Results->Roof->Destroy(); });
    At(10.3f,[=]() { P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(FVector(-1200,-1250,94));Key(EKeys::W,IE_Pressed); });
    At(10.8f,[=]() { Key(EKeys::LeftControl,IE_Pressed); });
    At(10.87f,[=]() { Check(P->IsSliding(),TEXT("A fresh crouch press can start the next slide"));Key(EKeys::LeftControl,IE_Released); });
    At(10.94f,[=]() { Check(!P->IsSliding(),TEXT("Releasing Ctrl cancels the slide"));Results->TapSpeed=P->GetVelocity().Size2D();Key(EKeys::LeftControl,IE_Pressed); });
    At(11.03f,[=]()
    {
        if(Move->SlideCooldownDuration==0.f)
            Check(P->IsSliding(),TEXT("Zero configured cooldown allows a new qualifying crouch press"));
        else if(Move->SlideCooldownDuration>.25f)
            Check(!P->IsSliding(),TEXT("Configured cooldown prevents stacking slide boosts"));
        Check(P->GetVelocity().Size2D()<=Results->TapSpeed+2.f,TEXT("Rapid crouch presses do not repeatedly grant entry boosts"));
        Key(EKeys::LeftControl,IE_Released);Key(EKeys::W,IE_Released);
    });
    At(11.2f,[=]() { P->LaunchCharacter(FVector(790,0,540),true,true); });
    At(11.28f,[=]() { Key(EKeys::LeftControl,IE_Pressed); });
    At(11.4f,[=]() { Check(P->GetCharacterMovement()->IsFalling() && !P->IsSliding(),TEXT("High airborne speed cannot trigger a slide"));Key(EKeys::LeftControl,IE_Released); });
    At(12.5f,[=]() { P->GetCharacterMovement()->StopMovementImmediately();P->SetActorLocation(FVector(-1200,-1250,94));Key(EKeys::W,IE_Pressed); });
    At(13.f,[=,this]()
    {
        auto* Wall=GetWorld()->SpawnActor<AActor>();auto* Collision=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Collision);
        Collision->SetBoxExtent(FVector(10,120,150));Collision->SetCollisionProfileName(TEXT("BlockAll"));Collision->RegisterComponent();
        Results->SlideWallX=P->GetActorLocation().X+180;
        Wall->SetActorLocation(FVector(Results->SlideWallX,P->GetActorLocation().Y,110));Results->Roof=Wall;
        Key(EKeys::LeftControl,IE_Pressed);
    });
    At(13.4f,[=]()
    {
        Check(!P->IsSliding() && P->GetVelocity().Size2D()<10 && P->GetActorLocation().X<Results->SlideWallX-40,TEXT("Slide stops at a wall without tunnelling"));
        Key(EKeys::LeftControl,IE_Released);Key(EKeys::W,IE_Released);if(Results->Roof.IsValid()) Results->Roof->Destroy();
    });
    // Check actual velocity as well as the speed limit. This catches an eased
    // setting that still lets CharacterMovement clamp the player in one frame.
    const auto Between=[=](float Low,float High,const TCHAR* Name)
    {
        const float Speed=P->GetVelocity().Size2D();
        Check(Speed>Low+5 && Speed<High-5 && Move->GetSmoothedMoveSpeed()>Low && Move->GetSmoothedMoveSpeed()<High,
            FString::Printf(TEXT("%s transitions gradually (actual %.1f, limit %.1f)"),Name,Speed,Move->GetSmoothedMoveSpeed()));
    };
    const auto Settled=[=](float Target,const TCHAR* Name)
    {
        Check(FMath::IsNearlyEqual(float(P->GetVelocity().Size2D()),Target,8.f),FString::Printf(TEXT("%s reaches %.0f cm/s"),Name,Target));
    };
    const auto ResetRun=[=]()
    {
        P->CrouchOff();Move->UnCrouch(false);Move->StopMovementImmediately();
        P->SetActorLocation(FVector(-1200,-1250,94));PC->SetControlRotation(FRotator(LookPitch,0,0));
        Move->SetMovementMode(MOVE_Walking);
    };
    At(14.f,[=]()
    {
        ResetRun();if(Sword) P->SelectOperator(0);P->SetUnarmed(false);P->SetAim(true);P->TogglePause();P->TogglePause();
        Check(!P->bAiming && Move->GetTargetMoveSpeed()==Move->RifleSpeed,TEXT("Pause clears both aim visuals and the slow movement intent"));
        Key(EKeys::W,IE_Pressed);
    });
    At(14.5f,[=]() { Settled(Move->RifleSpeed,TEXT("Rifle"));P->SetUnarmed(true); });
    At(14.57f,[=]() { Between(Move->RifleSpeed,Move->UnarmedSpeed,TEXT("Holstering")); });
    At(14.9f,[=]() { Settled(Move->UnarmedSpeed,TEXT("Unarmed"));P->SetUnarmed(false); });
    At(14.97f,[=]() { Between(Move->RifleSpeed,Move->UnarmedSpeed,TEXT("Drawing rifle")); });
    At(15.3f,[=]() { Settled(Move->RifleSpeed,TEXT("Drawing rifle"));P->SetAim(true); });
    At(15.37f,[=]() { Between(Move->AimSpeed,Move->RifleSpeed,TEXT("Entering aim")); });
    At(15.65f,[=]() { Settled(Move->AimSpeed,TEXT("Aiming"));P->SetAim(false); });
    At(15.72f,[=]() { Between(Move->AimSpeed,Move->RifleSpeed,TEXT("Leaving aim")); });
    At(16.f,[=]() { Settled(Move->RifleSpeed,TEXT("Leaving aim"));Key(EKeys::LeftControl,IE_Pressed); });
    At(16.07f,[=]() { Between(Move->MaxWalkSpeedCrouched,Move->RifleSpeed,TEXT("Crouching"));Check(!P->IsSliding(),TEXT("Subthreshold crouch remains a normal crouch")); });
    At(16.4f,[=]() { Settled(Move->MaxWalkSpeedCrouched,TEXT("Crouched"));Key(EKeys::LeftControl,IE_Released); });
    At(16.47f,[=]() { Between(Move->MaxWalkSpeedCrouched,Move->RifleSpeed,TEXT("Standing")); });
    At(16.85f,[=]() { Settled(Move->RifleSpeed,TEXT("Standing")); });
    At(17.f,[=]() { ResetRun();if(Sword) P->SelectOperator(Index);P->SetUnarmed(true); });
    At(17.6f,[=]() { Key(EKeys::LeftControl,IE_Pressed); });
    At(17.83f,[=]() { Check(P->IsSliding(),TEXT("Slide jump begins from an active slide"));Results->JumpSpeed=P->GetVelocity().Size2D();Key(EKeys::SpaceBar,IE_Pressed); });
    At(17.91f,[=]()
    {
        Check(Move->IsFalling() && !P->IsSliding() && P->GetVelocity().Z>0,TEXT("Space cancels the slide into a physical jump while Ctrl is held"));
        Check(P->GetVelocity().Size2D()>=Results->JumpSpeed-40.f,TEXT("Slide jump preserves horizontal momentum"));
        Key(EKeys::SpaceBar,IE_Released);Screenshot(TEXT("SlideJump"));
    });
    At(18.4f,[=]() { Check(Move->IsFalling(),TEXT("Slide jump follows the airborne arc"));P->SetUnarmed(false); });
    At(18.47f,[=]() { Check(P->GetVelocity().Size2D()>=Results->JumpSpeed-50.f,Sword?TEXT("Acheron's locked sword stance does not erase slide jump momentum"):TEXT("Drawing the rifle in the air does not erase slide jump momentum")); });
    At(18.6f,[=]() { P->SetUnarmed(true); });
    At(19.08f,[=]() { Check(P->IsSliding() && P->GetVelocity().Size2D()>Move->SlideEntrySpeed,TEXT("Landing at speed with Ctrl held automatically continues sliding")); });
    At(19.3f,[=]() { Screenshot(TEXT("SlideLand")); });
    At(19.37f,[=]() { Key(EKeys::LeftControl,IE_Released); });
    At(19.46f,[=]() { Check(!P->IsSliding() && P->GetVelocity().Size2D()>FastSpeed,TEXT("Slide exit eases down toward running speed instead of clamping momentum")); });
    At(20.f,[=]() { ResetRun(); });
    At(20.5f,[=]() { Key(EKeys::LeftControl,IE_Pressed); });
    At(20.65f,[=,this]()
    {
        auto* Roof=GetWorld()->SpawnActor<AActor>();auto* Collision=NewObject<UBoxComponent>(Roof);Roof->SetRootComponent(Collision);
        Collision->SetBoxExtent(FVector(600,100,15));Collision->SetCollisionProfileName(TEXT("BlockAll"));Collision->RegisterComponent();
        const float Floor=P->GetActorLocation().Z-P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        Roof->SetActorLocation(FVector(P->GetActorLocation().X,P->GetActorLocation().Y,Floor+130));Results->Roof=Roof;
    });
    At(20.72f,[=]() { Key(EKeys::SpaceBar,IE_Pressed); });
    At(20.82f,[=]() { Check(!Move->IsFalling() && P->IsSliding() && P->bIsCrouched,TEXT("A low ceiling safely blocks a slide jump")); });
    At(20.86f,[=]() { Key(EKeys::SpaceBar,IE_Released);Key(EKeys::LeftControl,IE_Released);Key(EKeys::W,IE_Released);if(Results->Roof.IsValid()) Results->Roof->Destroy(); });
    const auto Ramp=[=,this](float Pitch)
    {
        ResetRun();
        if(Results->Ramp.IsValid()) Results->Ramp->Destroy();
        auto* Actor=GetWorld()->SpawnActor<AActor>();auto* Collision=NewObject<UBoxComponent>(Actor);Actor->SetRootComponent(Collision);
        Collision->SetBoxExtent(FVector(3500,500,30));Collision->SetCollisionProfileName(TEXT("BlockAll"));Collision->RegisterComponent();
        Actor->SetActorLocationAndRotation(FVector(8000,8000,1500),FRotator(Pitch,0,0));
        if(Capture)
        {
            auto* Surface=NewObject<UStaticMeshComponent>(Actor);Surface->SetupAttachment(Collision);
            Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
            Surface->SetRelativeScale3D(FVector(70,10,.6f));Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Surface->SetMaterial(0,Breach::Material(TEXT("M_Metal")));Surface->RegisterComponent();
        }
        Results->Ramp=Actor;
        P->SetActorLocation(Actor->GetActorTransform().TransformPosition(FVector(-2600,0,30))+FVector(0,0,112));
        Move->SetMovementMode(MOVE_Falling);
    };
    At(21.5f,[=]() { Ramp(-20); });
    At(22.2f,[=]() { Check(Move->IsMovingOnGround(),TEXT("Downhill test lands on the sloped floor"));Key(EKeys::W,IE_Pressed); });
    At(22.7f,[=]() { Key(EKeys::LeftControl,IE_Pressed); });
    At(23.f,[=]() { Results->RampSpeed=P->GetVelocity().Size2D(); });
    At(24.7f,[=]() { Check(P->IsSliding() && P->GetVelocity().Size2D()>Results->RampSpeed+30.f,TEXT("Downhill gravity sustains and accelerates sliding beyond the former time limit")); });
    At(26.f,[=]() { Check(P->IsSliding() && P->GetVelocity().Size2D()<=Move->SlideMaxSpeed+5.f,TEXT("Long downhill slide remains active with a bounded top speed"));Screenshot(TEXT("SlideDownhill"));Key(EKeys::LeftControl,IE_Released);Key(EKeys::W,IE_Released); });
    At(26.5f,[=]() { Ramp(20); });
    At(26.9f,[=]() { Check(Move->IsMovingOnGround(),TEXT("Uphill test lands on the sloped floor"));Key(EKeys::W,IE_Pressed); });
    At(27.4f,[=]() { Key(EKeys::LeftControl,IE_Pressed); });
    At(27.65f,[=]() { Check(P->IsSliding(),TEXT("Sufficient entry momentum starts an uphill slide"));Results->RampSpeed=P->GetVelocity().Size2D(); });
    At(27.85f,[=]() { Check(P->GetVelocity().Size2D()<Results->RampSpeed-60.f,TEXT("Uphill sliding loses speed faster than flat-ground sliding"));Screenshot(TEXT("SlideUphill")); });
    At(29.f,[=]() { Check(!P->IsSliding() && P->bIsCrouched,TEXT("Uphill slide naturally ends at low speed"));Key(EKeys::LeftControl,IE_Released);Key(EKeys::W,IE_Released); });
    At(29.4f,[=,this]() mutable
    {
        if(Results->Ramp.IsValid()) Results->Ramp->Destroy();
        GetWorldTimerManager().ClearTimer(CameraMonitor);
        Results->Text+=FString::Printf(TEXT("FAILURES=%d\n"),Results->Failed);
        FFileHelper::SaveStringToFile(Results->Text,*(FPaths::ProjectDir()/TEXT("Saved")/(Prefix+TEXT(".txt"))));
        UE_LOG(LogTemp,Display,TEXT("MOVEMENT_TEST %s\n%s"),*Prefix,*Results->Text);
        PC->ConsoleCommand(TEXT("quit"));
    });
}
