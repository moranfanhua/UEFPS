#include "BreachGame.h"
#include "BreachSelectionStage.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerState.h"
#include "TextureResource.h"

void ABreachHUD::DrawPlayerVitals(const ABreachCharacter* Player)
{
    const float S=Canvas->SizeY/900.f,X=28,Y=900-104;
    const FLinearColor Gold(.72f,.37f,.06f),Edge(.39f,.28f,.14f,.95f);
    const FLinearColor Dark(.023f,.021f,.025f,.94f),White(.94f,.95f,.97f);
    // Convex polygons keep the portrait and health fill inside the angled frame.
    const auto Polygon=[&](std::initializer_list<FVector2D> Vertices,FLinearColor Tint,const FTexture* Texture=nullptr)
    {
        TArray<FVector2D> Points(Vertices);TArray<FCanvasUVTri> Triangles;
        const auto Position=[&](FVector2D P) { return FVector2D(X+P.X,Y+P.Y)*S; };
        const auto UV=[](FVector2D P) { return FVector2D(.30+(P.X-6)/92*.40,(P.Y-4)/67*.64); };
        for(int32 I=1;I+1<Points.Num();++I)
        {
            FCanvasUVTri T;
            T.V0_Pos=Position(Points[0]);T.V1_Pos=Position(Points[I]);T.V2_Pos=Position(Points[I+1]);
            T.V0_UV=UV(Points[0]);T.V1_UV=UV(Points[I]);T.V2_UV=UV(Points[I+1]);
            T.V0_Color=T.V1_Color=T.V2_Color=Tint;Triangles.Add(T);
        }
        FCanvasTriangleItem Item(Triangles,Texture?Texture:GWhiteTexture);
        Item.BlendMode=Texture?SE_BLEND_Opaque:SE_BLEND_Translucent;Canvas->DrawItem(Item);
    };
    Polygon({{12,0},{347,0},{375,56},{365,75},{25,75},{0,17}},Edge);
    Polygon({{15,2},{345,2},{371,56},{363,72},{28,72},{4,17}},Dark);
    Polygon({{101,3},{344,3},{358,32},{93,32}},FLinearColor(.09f,.066f,.046f,.36f));
    Polygon({{342,10},{348,10},{373,56},{365,55}},FLinearColor(.62f,.4f,.13f,.67f));
    Polygon({{365,55},{373,56},{364,73},{354,73}},FLinearColor(.62f,.4f,.13f,.67f));
    // Broad gold edge around the character portrait, with a narrow inner bevel.
    Polygon({{12,0},{105,0},{83,75},{25,75},{0,17}},Gold);
    Polygon({{17,3},{100,3},{80,72},{29,72},{5,18}},FLinearColor(.22f,.16f,.09f));
    EnsureSelectionStage();
    auto* Portrait=SelectionStage?SelectionStage->Portrait(Player->OperatorIndex):nullptr;
    if(Portrait && Portrait->GetResource())
        Polygon({{17,4},{98,4},{78,71},{29,71},{6,18}},FLinearColor::White,Portrait->GetResource());
    Polygon({{12,0},{20,0},{8,18},{0,17}},FLinearColor(.96f,.58f,.12f));
    Polygon({{0,17},{8,18},{31,75},{25,75}},FLinearColor(.96f,.58f,.12f));
    DrawLine((X+29)*S,(Y+74)*S,(X+84)*S,(Y+74)*S,Gold,S);
    DrawLine((X+84)*S,(Y+74)*S,(X+106)*S,Y*S,Gold,S);

    const APlayerState* State=Player->GetPlayerState();
    FString PlayerID=State?State->GetPlayerName():FString();
    if(PlayerID.IsEmpty()) PlayerID=TEXT("PLAYER 01");
    float Width=0,Height=0;
    UCanvas::StrLen(GEngine->GetMediumFont(),PlayerID,Width,Height,false,Canvas->Canvas);
    const float NameSize=15.f;
    if(Width*NameSize/12.f>228)
    {
        do
        {
            PlayerID.LeftChopInline(1);
            UCanvas::StrLen(GEngine->GetMediumFont(),PlayerID+TEXT("..."),Width,Height,false,Canvas->Canvas);
        } while(PlayerID.Len()>1 && Width*NameSize/12.f>228);
        PlayerID+=TEXT("...");
    }
    Text(PlayerID,X+111,Y+13,NameSize,White);

    Polygon({{104,44},{344,44},{351,61},{99,61}},FLinearColor(.42f,.40f,.38f));
    Polygon({{107,46},{342,46},{348,59},{103,59}},FLinearColor(.055f,.049f,.05f));
    const float Health=FMath::Clamp(Player->Health/100.f,0.f,1.f);
    if(Health>0)
    {
        const float End=107+237*Health,Bevel=FMath::Min(4.f,237*Health*.5f);
        Polygon({{107,48},{End-Bevel,48},{End,57},{105,57}},Player->Health<30?FLinearColor(1,.24f,.15f):White);
    }
    Polygon({{96,68},{357,68},{354,70},{95,70}},FLinearColor(.72f,.57f,.32f,.5f));
}
