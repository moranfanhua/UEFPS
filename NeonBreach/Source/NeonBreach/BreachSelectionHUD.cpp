#include "BreachGame.h"
#include "BreachSelectionStage.h"
#include "BreachVisuals.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CanvasItem.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

namespace
{
    void Gradient(UCanvas* Canvas,float X,float Y,float Width,float Height,FLinearColor Start,FLinearColor End,bool Horizontal=false)
    {
        const float S=Canvas->SizeY/900.f;
        const FVector2D Points[]={FVector2D(X,Y)*S,FVector2D(X+Width,Y)*S,FVector2D(X+Width,Y+Height)*S,FVector2D(X,Y+Height)*S};
        const FLinearColor Colors[]={Start,Horizontal?End:Start,End,Horizontal?Start:End};
        TArray<FCanvasUVTri> Triangles;
        for(int32 I=1;I<3;++I)
        {
            FCanvasUVTri T;T.V0_Pos=Points[0];T.V1_Pos=Points[I];T.V2_Pos=Points[I+1];
            T.V0_UV=T.V1_UV=T.V2_UV=FVector2D::ZeroVector;
            T.V0_Color=Colors[0];T.V1_Color=Colors[I];T.V2_Color=Colors[I+1];Triangles.Add(T);
        }
        FCanvasTriangleItem Item(Triangles,GWhiteTexture);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
    }
}

void ABreachHUD::ToggleSelection()
{
    auto* PC=Cast<ABreachPlayerController>(GetOwningPlayerController());auto* Player=Cast<ABreachCharacter>(GetOwningPawn());
    if(!PC || !Player) return;
    if(!bSelectionOpen)
    {
        if(!SelectionStage)
            SelectionStage=GetWorld()->SpawnActor<ABreachSelectionStage>(FVector(0,0,-20000),FRotator::ZeroRotator);
        if(!SelectionStage) return;
        bWasPaused=UGameplayStatics::IsGamePaused(this);
        bWasAutoCamera=PC->bAutoManageActiveCameraTarget;bWasPauseTick=PC->SetSelectionPauseTick(true);bWasClickEvents=PC->bEnableClickEvents;
        PreviousViewTarget=PC->GetViewTarget();
        bSelectionOpen=true;HoveredOperator=INDEX_NONE;
        SelectionStage->SetOpen(true);SelectionStage->Select(Player->OperatorIndex);
        PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);
        Player->GetCharacterMovement()->StopMovementImmediately();
        PC->bAutoManageActiveCameraTarget=false;PC->SetViewTarget(SelectionStage);
        PC->bEnableClickEvents=true;PC->bShowMouseCursor=true;
        FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Mode);
        UGameplayStatics::SetGamePaused(this,true);
        const FVector2D Center=OperatorCardCenter(Player->OperatorIndex);PC->SetMouseLocation(FMath::RoundToInt(Center.X),FMath::RoundToInt(Center.Y));
    }
    else
    {
        bSelectionOpen=false;HoveredOperator=INDEX_NONE;
        SelectionStage->SetOpen(false);
        PC->SetViewTarget(PreviousViewTarget.IsValid()?PreviousViewTarget.Get():Player);
        PC->bAutoManageActiveCameraTarget=bWasAutoCamera;PC->SetSelectionPauseTick(bWasPauseTick);
        PC->bEnableClickEvents=bWasClickEvents;PC->bShowMouseCursor=false;
        PC->SetIgnoreMoveInput(false);PC->SetIgnoreLookInput(false);PC->SetInputMode(FInputModeGameOnly());
        UGameplayStatics::SetGamePaused(this,bWasPaused);
    }
}
void ABreachHUD::ChooseOperator(int32 Index)
{
    if(!bSelectionOpen || !SelectionStage || Index<0 || Index>3) return;
    if(auto* Player=Cast<ABreachCharacter>(GetOwningPawn())) Player->SelectOperator(Index);
    SelectionStage->Select(Index);
}
FVector2D ABreachHUD::OperatorCardCenter(int32 Index) const
{
    int32 Width=1600,Height=900;if(auto* PC=GetOwningPlayerController()) PC->GetViewportSize(Width,Height);
    const float Scale=FMath::Max(Height,1)/900.f,VirtualWidth=Width/Scale;
    return FVector2D((VirtualWidth*.5f-349+Index*178+82)*Scale,785*Scale);
}
void ABreachHUD::MenuText(const FString& Value,float X,float Y,float Size,FLinearColor Color,bool Chinese)
{
    const float Scale=Canvas->SizeY/900.f;
    static const auto ChineseFont=MakeShared<FCompositeFont>(TEXT("Regular"),FPaths::EngineContentDir()/TEXT("Slate/Fonts/DroidSansFallback.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
    static const auto LatinFont=MakeShared<FCompositeFont>(TEXT("Regular"),FPaths::EngineContentDir()/TEXT("Slate/Fonts/Roboto-BoldCondensed.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
    FCanvasTextItem Item(FVector2D(X*Scale,Y*Scale),FText::FromString(Value),FSlateFontInfo(Chinese?ChineseFont:LatinFont,FMath::RoundToInt(Size*Scale)),Color);
    // Canvas needs a UFont to select the runtime cache even with a composite font override.
    Item.Font=GEngine->GetMediumFont();
    Item.EnableShadow(FLinearColor(0,0,0,.25f));Canvas->DrawItem(Item);
}
void ABreachHUD::DrawSelection()
{
    if(!SelectionStage) return;
    const float Scale=Canvas->SizeY/900.f,W=Canvas->SizeX/Scale;
    const FLinearColor Cyan(.24f,.86f,1),White(.94f,.97f,1),Muted(.45f,.58f,.65f),Dark(.006f,.014f,.023f,.94f);
    // Keep the right-hand area available for a later character details panel.
    Gradient(Canvas,0,0,W,160,FLinearColor(.004f,.012f,.025f,.62f),FLinearColor(.004f,.012f,.025f,0));
    Gradient(Canvas,W*.73f,160,W*.27f,530,FLinearColor(.004f,.01f,.019f,0),FLinearColor(.004f,.01f,.019f,.32f),true);
    Box(54,47,3,79,Cyan);
    MenuText(TEXT("NEON BREACH  /  OPERATORS"),73,43,12,Cyan);
    MenuText(TEXT("选择角色"),70,65,30,White,true);
    MenuText(TEXT("OPERATOR ARCHIVE     /     04 AVAILABLE"),73,112,10,Muted);
    Box(W-190,46,138,40,FLinearColor(.03f,.06f,.08f,.85f));
    MenuText(TEXT("H   /   返回"),W-173,55,14,White,true);
    AddHitBox(FVector2D((W-190)*Scale,46*Scale),FVector2D(138*Scale,40*Scale),TEXT("SelectionClose"),true,10);
    const int32 Selected=SelectionStage->SelectedIndex;
    static const TCHAR* Titles[]={TEXT("EULA"),TEXT("EULA / CITY"),TEXT("LI ZHIYAN"),TEXT("MARIONETTE")};
    static const TCHAR* ChineseNames[]={TEXT("优菈"),TEXT("联动优菈"),TEXT("李织烟"),TEXT("Marionette")};
    MenuText(FString::Printf(TEXT("0%d  /  OPERATOR"),Selected+1),64,280,12,Cyan);
    MenuText(Titles[Selected],60,305,40,White);
    MenuText(ChineseNames[Selected],65,373,22,FLinearColor(.66f,.81f,.88f),Selected!=3);
    Box(65,420,48,2,Cyan);Box(117,420,162,1,FLinearColor(.22f,.4f,.49f,.45f));
    Gradient(Canvas,0,676,W,48,FLinearColor(.004f,.01f,.018f,0),FLinearColor(.004f,.01f,.018f,.95f));
    Box(0,724,W,176,Dark);Box(52,700,W-104,1,FLinearColor(.2f,.45f,.55f,.28f));
    const float Start=W*.5f-349;
    for(int32 I=0;I<4;++I)
    {
        const float X=Start+I*178,Y=716;const bool Active=I==Selected,Hover=I==HoveredOperator;
        const FLinearColor Border=Active?Cyan:(Hover?FLinearColor(.7f,.88f,1):FLinearColor(.17f,.27f,.33f));
        Box(X-2,Y-2,168,146,Border);Box(X,Y,164,142,Dark);
        if(auto* Texture=SelectionStage->Portrait(I))
            DrawTexture(Texture,(X+1)*Scale,(Y+1)*Scale,162*Scale,101*Scale,0,0,1,1,FLinearColor::White,BLEND_Opaque);
        if(!Active && !Hover) Box(X+1,Y+1,162,101,FLinearColor(.015f,.04f,.06f,.22f));
        Box(X,Y,164,3,Border);
        MenuText(ChineseNames[I],X+11,Y+111,13,Active?White:Muted,I!=3);
        if(Active) { Box(X+136,Y+118,12,3,Cyan);Box(X+145,Y+113,3,8,Cyan); }
        AddHitBox(FVector2D(X*Scale,Y*Scale),FVector2D(164*Scale,142*Scale),FName(*FString::Printf(TEXT("Operator_%d"),I)),true,5);
    }
    MenuText(TEXT("点击头像选择角色 · 再次点击重播动作"),54,869,12,Muted,true);
    MenuText(TEXT("H / ESC  返回作战"),W-208,869,12,Muted,true);
}
void ABreachHUD::NotifyHitBoxClick(FName BoxName)
{
    Super::NotifyHitBoxClick(BoxName);if(!bSelectionOpen) return;
    if(BoxName==TEXT("SelectionClose")) { ToggleSelection(); return; }
    const FString Name=BoxName.ToString();if(Name.StartsWith(TEXT("Operator_"))) ChooseOperator(FCString::Atoi(*Name.RightChop(9)));
}
void ABreachHUD::NotifyHitBoxBeginCursorOver(FName BoxName)
{
    Super::NotifyHitBoxBeginCursorOver(BoxName);
    const FString Name=BoxName.ToString();if(Name.StartsWith(TEXT("Operator_"))) HoveredOperator=FCString::Atoi(*Name.RightChop(9));
}
void ABreachHUD::NotifyHitBoxEndCursorOver(FName BoxName)
{
    Super::NotifyHitBoxEndCursorOver(BoxName);
    const FString Name=BoxName.ToString();if(Name.StartsWith(TEXT("Operator_")) && HoveredOperator==FCString::Atoi(*Name.RightChop(9))) HoveredOperator=INDEX_NONE;
}
