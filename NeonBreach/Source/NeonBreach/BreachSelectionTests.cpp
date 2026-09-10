#include "BreachGame.h"
#include "BreachSelectionStage.h"
#include "BreachVisuals.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/PoseableMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void ABreachGameMode::TickSelectionTest()
{
    struct FRun
    {
        TArray<TPair<float,TFunction<void()>>> Steps;
        int32 Step=0,Failures=0,Ammo=0;
        float Health=0;
        double WorldTime=0;
        double Last=FPlatformTime::Seconds();
        TArray<FTransform> HeldPose;
        TArray<FTransform> HeldCatPose;
        FTransform HeldCamera;
        FTransform HeldCatTransform;
        FString Report;
    };
    auto Run=MakeShared<FRun>();
    const auto Check=[Run](bool Pass,const FString& Message)
    {
        Run->Report+=FString::Printf(TEXT("%s %s\n"),Pass?TEXT("PASS"):TEXT("FAIL"),*Message);
        if(!Pass) ++Run->Failures;
    };
    const auto PC=[this]() { return UGameplayStatics::GetPlayerController(this,0); };
    const auto HUD=[PC]() { return Cast<ABreachHUD>(PC()->GetHUD()); };
    const auto Player=[PC]() { return Cast<ABreachCharacter>(PC()->GetPawn()); };
    const auto Key=[PC](FKey Button,EInputEvent Event)
    {
        PC()->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Button,Event,Event==IE_Released?0.f:1.f,false,FPlatformTime::Cycles64()));
    };
    const auto Add=[Run](float Delay,TFunction<void()> Fn) { Run->Steps.Emplace(Delay,MoveTemp(Fn)); };
    const auto FrameCheck=[PC,HUD,Check](int32 Index)
    {
        const auto* Stage=HUD()->GetSelectionStage();
        FBreachPose Rig; if(!Stage || !Rig.Init(Breach::CharacterMesh(Index),Index)) { Check(false,TEXT("Preview rig is available for framing"));return; }
        int32 Width=0,Height=0;PC()->GetViewportSize(Width,Height);
        if(Width<=0 || Height<=0) return; // No screen projection in null-RHI runs.
        const auto Screen=[&](EBreachBone Bone)
        {
            FVector2D Result(-1,-1);
            PC()->ProjectWorldLocationToScreen(Stage->Preview->GetBoneLocationByName(Stage->Preview->GetBoneName(Rig.Bone(Bone)),EBoneSpaces::WorldSpace),Result);
            return Result/FVector2D(Width,Height);
        };
        const auto Head=Screen(EBreachBone::Head),Waist=Screen(EBreachBone::Pelvis);
        Check(Head.X>.25 && Head.X<.75 && Head.Y>.05 && Head.Y<.5 && Waist.Y>.60 && Waist.Y<.84,
            FString::Printf(TEXT("Operator %d upper body fills the area above the cards (head %.2f, waist %.2f)"),Index,Head.Y,Waist.Y));
        const auto Left=Screen(EBreachBone::LHand),Right=Screen(EBreachBone::RHand);
        Check(Left.X>.1 && Left.X<.9 && Right.X>.1 && Right.X<.9 && Left.Y>.02 && Right.Y>.02 && Left.Y<.84 && Right.Y<.84,
            FString::Printf(TEXT("Operator %d hand gestures stay inside the shot"),Index));
        if(Index==2 && Stage->HasCatEntrance())
        {
            FBreachPose CatRig;CatRig.InitSkeleton(Cast<USkeletalMesh>(Stage->Cat->GetSkinnedAsset()));
            const int32 HeadBone=Stage->Cat->GetBoneIndex(TEXT("head"));
            const FTransform CatHeadWorld=Stage->Cat->GetBoneTransformByName(TEXT("head"),EBoneSpaces::WorldSpace);
            const FVector FaceDirection=CatHeadWorld.TransformVectorNoScale(CatRig.ReferenceCS[HeadBone].InverseTransformVectorNoScale(FVector::ForwardVector)).GetSafeNormal();
            const FVector HolderDirection=(Stage->Preview->GetBoneLocationByName(Stage->Preview->GetBoneName(Rig.Bone(EBreachBone::Head)),EBoneSpaces::WorldSpace)-CatHeadWorld.GetLocation()).GetSafeNormal();
            Check(FVector::DotProduct(FaceDirection,HolderDirection)>.3f,TEXT("Cat faces the holder rather than the camera"));
            FVector2D CatHead;
            PC()->ProjectWorldLocationToScreen(Stage->Cat->GetBoneLocationByName(TEXT("head"),EBoneSpaces::WorldSpace),CatHead);
            CatHead/=FVector2D(Width,Height);
            Check(CatHead.X>.15 && CatHead.X<.85 && CatHead.Y>.07 && CatHead.Y<.65,TEXT("Cat and character faces remain inside the upper-body shot"));
            for(int32 Side=0;Side<2;++Side)
            {
                const FName Hand=Stage->Preview->GetBoneName(Rig.Bone(Side?EBreachBone::RHand:EBreachBone::LHand));
                const float Gap=FVector::Distance(Stage->Preview->GetBoneLocationByName(Hand,EBoneSpaces::WorldSpace),Stage->CatSupportPoint(Side));
                Check(Gap<1.f,FString::Printf(TEXT("Cat contact %d remains held by the hand (%.2f cm)"),Side,Gap));
            }
        }
    };
    Add(2,[=]() { Run->Ammo=Player()->Ammo;Run->Health=Player()->Health; });
    Add(1,[=,this]()
    {
        Check(HUD()->IsSelectionOpen(),TEXT("Game starts in character selection without any key press"));
        Check(UGameplayStatics::IsGamePaused(this) && PC()->bShowMouseCursor,TEXT("Selection pauses combat and enables cursor"));
        Check(PC()->GetViewTarget()==HUD()->GetSelectionStage(),TEXT("Selection uses the preview camera"));
        Run->WorldTime=GetWorld()->GetTimeSeconds();
    });
    for(int32 I=0;I<4;++I)
    {
        Add(.4f,[=]() { const auto Center=HUD()->OperatorCardCenter(I);PC()->SetMouseLocation(Center.X,Center.Y); });
        Add(.2f,[=]() { Key(EKeys::LeftMouseButton,IE_Pressed); });
        Add(.2f,[=]() { Key(EKeys::LeftMouseButton,IE_Released); });
        Add(.3f,[=]()
        {
            auto* Stage=HUD()->GetSelectionStage();
            Check(Stage && Player()->OperatorIndex==I && Stage->SelectedIndex==I,FString::Printf(TEXT("Mouse selects operator %d in preview and gameplay"),I));
            if(Stage)
            {
                Check(Stage->HasEntrance() && Stage->AnimationTime>.1f,FString::Printf(TEXT("Operator %d entrance advances while paused"),I));
                Check(Stage->Cat && Stage->Cat->IsVisible()==(I==2) && Stage->HasCatEntrance()==(I==2),
                    FString::Printf(TEXT("Cat appears only for Lizhiyan, selected operator %d"),I));
                Check(Stage->Sword && Stage->Scabbard && Stage->Sword->IsVisible()==(I==1) && Stage->Scabbard->IsVisible()==(I==1) && Stage->HasSwordEntrance()==(I==1),
                    FString::Printf(TEXT("Supplied blade and sheath appear only for Acheron, selected operator %d"),I));
                FBreachPose Rig;Rig.Init(Breach::CharacterMesh(I),I);
                Check(Stage->Preview->GetBoneTransformByName(Stage->Preview->GetBoneName(Rig.Bone(EBreachBone::Head)),EBoneSpaces::ComponentSpace).GetScale3D().GetMin()>.5f,TEXT("Preview retains the full head"));
            }
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Saved/Selection_%d_Entrance.png"),I),true,false);
        });
        Add(1.9f,[=]()
        {
            const auto* Stage=HUD()->GetSelectionStage();
            Check(Stage && !Stage->IsHoldingEntrance() && Stage->AnimationTime>2.f && Stage->AnimationTime<3.f,
                FString::Printf(TEXT("Operator %d entrance is still playing before three seconds"),I));
            FrameCheck(I);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Saved/Selection_%d_Mid.png"),I),true,false);
        });
        Add(1.4f,[=]()
        {
            const auto* Stage=HUD()->GetSelectionStage();
            Check(Stage && Stage->IsHoldingEntrance() && Stage->AnimationTime>=3.f && Stage->AnimationTime<=4.f,
                FString::Printf(TEXT("Operator %d finishes within three to four seconds"),I));
            if(!Stage) return;
            Run->HeldPose.Reset();
            for(int32 Bone=0;Bone<Stage->Preview->GetNumBones();++Bone)
                Run->HeldPose.Add(Stage->Preview->GetBoneTransformByName(Stage->Preview->GetBoneName(Bone),EBoneSpaces::ComponentSpace));
            Run->HeldCamera=Stage->Camera->GetRelativeTransform();
            Run->HeldCatPose.Reset();
            if(I==2 && Stage->Cat)
            {
                Run->HeldCatTransform=Stage->Cat->GetRelativeTransform();
                for(int32 Bone=0;Bone<Stage->Cat->GetNumBones();++Bone)
                    Run->HeldCatPose.Add(Stage->Cat->GetBoneTransformByName(Stage->Cat->GetBoneName(Bone),EBoneSpaces::ComponentSpace));
            }
            FBreachPose Idle;const bool IdleReady=Idle.Init(Breach::CharacterMesh(I),I);
            auto* Clip=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Animations/Entrance/%s/A_%s_Female_Idle.A_%s_Female_Idle"),Breach::Keys[I],Breach::Keys[I],Breach::Keys[I]));
            float Difference=0;
            if(IdleReady && Idle.Sample(Clip,0,false))
                for(EBreachBone Hand:{EBreachBone::LHand,EBreachBone::RHand})
                    if(Run->HeldPose.IsValidIndex(Idle.Bone(Hand)))
                        Difference+=FVector::Distance(Run->HeldPose[Idle.Bone(Hand)].GetLocation(),Idle.CS[Idle.Bone(Hand)].GetLocation())*Stage->Preview->GetRelativeScale3D().Z;
            Check(Difference>10.f,FString::Printf(TEXT("Operator %d holds a gesture instead of neutral standing (hand difference %.1f cm)"),I,Difference));
            FrameCheck(I);
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Saved/Selection_%d_Finish.png"),I),true,false);
        });
        Add(1.2f,[=]()
        {
            const auto* Stage=HUD()->GetSelectionStage();
            bool Held=Stage && Stage->IsHoldingEntrance() && Stage->Preview->GetNumBones()==Run->HeldPose.Num();
            if(Held) for(int32 Bone=0;Bone<Run->HeldPose.Num();++Bone)
                Held&=Run->HeldPose[Bone].Equals(Stage->Preview->GetBoneTransformByName(Stage->Preview->GetBoneName(Bone),EBoneSpaces::ComponentSpace),.001f);
            Check(Held && Run->HeldCamera.Equals(Stage->Camera->GetRelativeTransform(),.001f),FString::Printf(TEXT("Operator %d pose and camera stay fixed after the entrance"),I));
            if(I==2)
            {
                bool CatHeld=Stage && Stage->Cat && Stage->Cat->GetNumBones()==Run->HeldCatPose.Num() && Run->HeldCatPose.Num()>0;
                if(CatHeld) for(int32 Bone=0;Bone<Run->HeldCatPose.Num();++Bone)
                    CatHeld&=Run->HeldCatPose[Bone].Equals(Stage->Cat->GetBoneTransformByName(Stage->Cat->GetBoneName(Bone),EBoneSpaces::ComponentSpace),.001f);
                Check(CatHeld && Run->HeldCatTransform.Equals(Stage->Cat->GetRelativeTransform(),.001f),TEXT("Cat keeps its held pose without drifting after the entrance"));
            }
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Saved/Selection_%d_Hold.png"),I),true,false);
        });
    }
    Add(.3f,[=,this]()
    {
        Check(FMath::IsNearlyEqual(GetWorld()->GetTimeSeconds(),Run->WorldTime),TEXT("World time remains paused while the menu animates"));
        Check(Player()->Health==Run->Health && Player()->Ammo==Run->Ammo,TEXT("Selection and clicks preserve health and ammo"));
        Key(EKeys::Enter,IE_Pressed);
    });
    Add(.2f,[=]() { Key(EKeys::Enter,IE_Released); });
    Add(.4f,[=,this]()
    {
        Check(!HUD()->IsSelectionOpen() && !UGameplayStatics::IsGamePaused(this),TEXT("Enter starts gameplay from the opening selection"));
        Check(Player()->OperatorIndex==3,TEXT("Starting gameplay keeps the chosen character"));
        Check(PC()->GetViewTarget()==Player() && !PC()->bShowMouseCursor && !PC()->IsMoveInputIgnored() && !PC()->IsLookInputIgnored(),TEXT("FPS camera and controls restored"));
        Check(!HUD()->GetSelectionStage()->Sword->IsVisible() && !HUD()->GetSelectionStage()->Scabbard->IsVisible(),TEXT("Selection weapons stay hidden during gameplay"));
        Key(EKeys::LeftMouseButton,IE_Pressed);
    });
    Add(.1f,[=]() { Key(EKeys::LeftMouseButton,IE_Released); });
    const FKey RemovedKeys[]={EKeys::F1,EKeys::F2,EKeys::F3,EKeys::F4};
    for(int32 I=0;I<4;++I)
    {
        const FKey Button=RemovedKeys[I];const int32 Selected=(I+1)%4;
        Add(.3f,[=]() { Player()->SelectOperator(Selected);Key(Button,IE_Pressed); });
        Add(.2f,[=]() { Key(Button,IE_Released); });
        Add(.2f,[=]() { Check(Player()->OperatorIndex==Selected,FString::Printf(TEXT("%s no longer switches characters"),*Button.ToString())); });
    }
    Add(.3f,[=]() { Key(EKeys::H,IE_Pressed); });
    Add(.2f,[=]() { Key(EKeys::H,IE_Released); });
    Add(.3f,[=]()
    {
        Check(HUD()->IsSelectionOpen(),TEXT("H reopens selection during gameplay"));
        const auto* Stage=HUD()->GetSelectionStage();
        Check(Stage && !Stage->IsHoldingEntrance() && Stage->AnimationTime<1.f,TEXT("Reopening selection restarts the entrance from the held pose"));
        HUD()->ChooseOperator(2);
    });
    Add(.4f,[=]()
    {
        const auto* Stage=HUD()->GetSelectionStage();
        Check(Stage && Stage->HasCatEntrance() && Stage->Cat->IsVisible() && Stage->AnimationTime<1.f,TEXT("Reselecting Lizhiyan restarts the paired cat entrance"));
        Key(EKeys::H,IE_Pressed);
    });
    Add(.2f,[=]() { Key(EKeys::H,IE_Released); });
    Add(.3f,[=,this]()
    {
        Check(!HUD()->IsSelectionOpen() && !UGameplayStatics::IsGamePaused(this),TEXT("H returns to gameplay after reopening selection"));
        const auto* Stage=HUD()->GetSelectionStage();
        Check(Stage && !Stage->Cat->IsVisible(),TEXT("Closing Lizhiyan selection hides the cat in gameplay"));
    });
    Add(.3f,[=]() { Check(Player()->Ammo<Run->Ammo,TEXT("Rifle fires after returning from selection"));Key(EKeys::Escape,IE_Pressed); });
    Add(.2f,[=]() { Key(EKeys::Escape,IE_Released);Key(EKeys::H,IE_Pressed); });
    Add(.2f,[=]() { Key(EKeys::H,IE_Released); });
    Add(.4f,[=]() { Check(HUD()->IsSelectionOpen(),TEXT("H also opens selection from paused game"));Key(EKeys::Escape,IE_Pressed); });
    Add(.2f,[=]() { Key(EKeys::Escape,IE_Released); });
    Add(.3f,[=,this]()
    {
        Check(!HUD()->IsSelectionOpen() && UGameplayStatics::IsGamePaused(this),TEXT("Escape closes selection and preserves previous paused state"));
        Run->Report+=FString::Printf(TEXT("FAILURES=%d\n"),Run->Failures);
        FFileHelper::SaveStringToFile(Run->Report,*(FPaths::ProjectDir()/TEXT("Saved/selection_test.txt")));
        UE_LOG(LogTemp,Display,TEXT("SELECTION_TEST %s"),*Run->Report);
        // Release the test callbacks before shutdown; they capture the shared report.
        Run->Steps.Reset();PC()->ConsoleCommand(TEXT("quit"));
    });
    SelectionTestStep=[Run]()
    {
        const double Now=FPlatformTime::Seconds();
        if(Run->Steps.IsValidIndex(Run->Step) && Now-Run->Last>=Run->Steps[Run->Step].Key)
        {
            auto Fn=MoveTemp(Run->Steps[Run->Step++].Value);Run->Last=Now;Fn();
        }
    };
}
