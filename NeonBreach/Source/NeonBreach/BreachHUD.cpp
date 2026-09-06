#include "BreachGame.h"
#include "BreachVisuals.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

void ABreachHUD::Box(float X,float Y,float W,float H,FLinearColor Color)
{
    const float S=Canvas->SizeY/900.f;
    DrawRect(Color,X*S,Y*S,W*S,H*S);
}
void ABreachHUD::Text(const FString& Value,float X,float Y,float Size,FLinearColor Color)
{
    const float S=Canvas->SizeY/900.f;
    FCanvasTextItem Item(FVector2D(X*S,Y*S),FText::FromString(Value),GEngine->GetMediumFont(),Color);
    Item.Scale=FVector2D(Size*S/12.f); Item.bOutlined=Size>=16;
    Item.OutlineColor=FLinearColor(0,0,0,.65f); Canvas->DrawItem(Item);
}
void ABreachHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* P=Cast<ABreachCharacter>(GetOwningPawn());
    auto* G=GetWorld()->GetAuthGameMode<ABreachGameMode>();
    if(!P || !G || !Canvas) return;
    const float S=Canvas->SizeY/900.f, W=Canvas->SizeX/S, H=900;
    const FLinearColor Cyan(.22f,.82f,1,1), White(.86f,.94f,1,1), Muted(.44f,.60f,.68f,1), Orange(1,.4f,.13f,1), Panel(.008f,.018f,.026f,.84f);
    Box(28,28,286,83,Panel); Box(28,28,3,83,Cyan);
    Text(TEXT("NEON / BREACH"),46,39,28,White);
    Text(TEXT("SECTOR 07   /   COMBAT SIMULATOR"),47,78,12,Cyan);
    Box(W-294,28,266,83,Panel);
    Text(FString::Printf(TEXT("%06d"),G->Score),W-270,36,34,White);
    Text(FString::Printf(TEXT("SCORE    /    %02d CLEARED"),G->Kills),W-270,82,12,Muted);
    Box(W*.5f-110,28,220,67,Panel);
    Text(G->bGallery?TEXT("OPERATOR ARCHIVE"):FString::Printf(TEXT("WAVE  %02d"),G->Wave),W*.5f-84,37,23,White);
    Text(G->bGallery?TEXT("4 SOURCE CHARACTERS"):FString::Printf(TEXT("%02d ACTIVE   /   %02d INBOUND"),G->EnemiesAlive,G->RemainingToSpawn),W*.5f-84,72,11,Cyan);
    if(!G->bGameOver && !G->bGallery && G->EnemiesAlive==0 && G->RemainingToSpawn==0)
        Text(FString::Printf(TEXT("NEXT WAVE IN %d"),FMath::CeilToInt(G->Intermission)),W*.5f-95,119,17,Cyan);
    if(G->NoticeTime>0 && !G->bGameOver) Text(G->Notice,W*.5f-G->Notice.Len()*4.1f,164,15,Cyan);
    // Crosshair expands during movement and contracts while aiming.
    const float Cx=W*.5f,Cy=H*.5f;
    const float Gap=P->bAiming?5:9+FMath::Clamp(P->GetVelocity().Size2D()/100.f,0.f,6.f);
    if(!G->bGameOver && !P->bUnarmed)
    {
        Box(Cx-1,Cy-1,2,2,White);
        if(!P->bAiming)
        {
            Box(Cx-Gap-7,Cy-1,7,2,White); Box(Cx+Gap,Cy-1,7,2,White);
            Box(Cx-1,Cy-Gap-7,2,7,White); Box(Cx-1,Cy+Gap,2,7,White);
        }
        if(P->HitMarker>0)
        {
            const FLinearColor Hit=P->bLastHeadshot?Orange:Cyan;
            for(int32 X : {-1,1}) for(int32 Y : {-1,1}) DrawLine((Cx+X*7)*S,(Cy+Y*7)*S,(Cx+X*14)*S,(Cy+Y*14)*S,Hit,2*S);
        }
    }
    Box(28,H-150,340,98,Panel); Box(28,H-150,3,98,Cyan);
    Text(TEXT("VITALS"),46,H-138,12,Muted);
    Text(FString::Printf(TEXT("%03d"),FMath::CeilToInt(P->Health)),46,H-121,37,P->Health<30?Orange:White);
    Text(TEXT("HP"),122,H-100,13,Muted);
    Box(159,H-108,184,8,FLinearColor(.1f,.15f,.2f,1));
    Box(159,H-108,184*P->Health/100.f,8,P->Health<30?Orange:Cyan);
    Text(Breach::Names[P->OperatorIndex],46,H-76,13,Cyan);
    Box(W-318,H-150,290,98,Panel); Box(W-31,H-150,3,98,Cyan);
    Text(P->bUnarmed?TEXT("UNARMED   /   FREE HANDS"):TEXT("VX-30   /   PULSE RIFLE"),W-298,H-138,12,Muted);
    if(P->bUnarmed)
    {
        Text(P->bIsCrouched?TEXT("CROUCH"):TEXT("RUN"),W-298,H-118,30,White);
        Text(TEXT("3  /  DRAW RIFLE"),W-298,H-74,12,Cyan);
    }
    else
    {
        Text(FString::Printf(TEXT("%02d"),P->Ammo),W-299,H-121,45,P->Ammo<=5?Orange:White);
        Text(FString::Printf(TEXT("/ %03d"),P->Reserve),W-225,H-101,20,Muted);
        Text(P->bReloading?TEXT("RELOADING"):P->Ammo==0?TEXT("R  /  RELOAD"):TEXT("AUTO    /    5.56 ENERGY"),W-298,H-74,12,P->bReloading?Orange:Cyan);
    }
    if(P->bReloading)
    {
        Box(Cx-85,Cy+54,170,4,Panel); Box(Cx-85,Cy+54,170*P->ReloadProgress,4,Cyan);
        Text(TEXT("RELOADING"),Cx-42,Cy+68,11,White);
    }
    Text(TEXT("WASD MOVE  /  3 UNARMED-RUN  /  CTRL CROUCH  /  SPACE JUMP  /  LMB FIRE  /  RMB AIM  /  R RELOAD"),28,H-30,11,Muted);
    Text(TEXT("F1-F4 OPERATOR  /  ESC PAUSE  /  ENTER RESTART"),W-400,H-30,11,Muted);
    for(TActorIterator<ABreachEnemy> It(GetWorld());It;++It)
    {
        if(It->bDisplayOnly || It->bDefeated) continue;
        FVector2D Screen;
        if(UGameplayStatics::ProjectWorldToScreen(GetOwningPlayerController(),It->GetActorLocation()+FVector(0,0,115),Screen,true))
        {
            const float X=Screen.X/S,Y=Screen.Y/S;
            if(Y<200 || Y>H-200 || X<40 || X>W-40) continue;
            Box(X-29,Y,58,4,Panel); Box(X-29,Y,58*It->Health/It->MaxHealth,4,Orange);
        }
    }
    if(P->DamageFlash>0)
    {
        const FLinearColor Red(1,.08f,.025f,P->DamageFlash*.6f);
        Box(0,0,W,6,Red); Box(0,H-6,W,6,Red); Box(0,0,6,H,Red); Box(W-6,0,6,H,Red);
    }
    if(G->bGameOver || UGameplayStatics::IsGamePaused(this))
    {
        Box(0,0,W,H,FLinearColor(.004f,.008f,.015f,.82f));
        Box(Cx-230,Cy-125,460,245,Panel); Box(Cx-230,Cy-125,460,3,Cyan);
        Text(G->bGameOver?TEXT("SIMULATION COMPLETE"):TEXT("SIMULATION PAUSED"),Cx-190,Cy-86,29,White);
        Text(FString::Printf(TEXT("WAVE %02d   /   %06d PTS   /   %02d CLEARED"),G->Wave,G->Score,G->Kills),Cx-180,Cy-30,15,Cyan);
        const int32 Accuracy=P->ShotsFired?FMath::RoundToInt(P->ShotsHit*100.f/P->ShotsFired):0;
        Text(FString::Printf(TEXT("ACCURACY %d%%"),Accuracy),Cx-72,Cy+4,16,Muted);
        Text(G->bGameOver?TEXT("ENTER  /  START A NEW RUN"):TEXT("ESC  /  RESUME     ENTER  /  RESTART"),Cx-171,Cy+67,15,White);
    }
}
