#include "BreachGame.h"
#include "BreachSelectionStage.h"
#include "BreachVisuals.h"
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
                FBreachPose Rig;Rig.Init(Breach::CharacterMesh(I),I);
                Check(Stage->Preview->GetBoneTransformByName(Stage->Preview->GetBoneName(Rig.Bone(EBreachBone::Head)),EBoneSpaces::ComponentSpace).GetScale3D().GetMin()>.5f,TEXT("Preview retains the full head"));
            }
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Saved/Selection_%d_Entrance.png"),I),true,false);
        });
        Add(3.2f,[=]() { FScreenshotRequest::RequestScreenshot(FPaths::ProjectDir()/FString::Printf(TEXT("Saved/Selection_%d_Idle.png"),I),true,false); });
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
    Add(.3f,[=]() { Check(HUD()->IsSelectionOpen(),TEXT("H reopens selection during gameplay"));Key(EKeys::H,IE_Pressed); });
    Add(.2f,[=]() { Key(EKeys::H,IE_Released); });
    Add(.3f,[=,this]() { Check(!HUD()->IsSelectionOpen() && !UGameplayStatics::IsGamePaused(this),TEXT("H returns to gameplay after reopening selection")); });
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
