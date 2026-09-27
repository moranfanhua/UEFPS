#include "BreachGame.h"
#include "Camera/CameraComponent.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Kismet/GameplayStatics.h"

float ABreachHUD::GetAimReticleOpacity() const
{
    const auto* P=Cast<ABreachCharacter>(GetOwningPawn());
    const auto* PC=GetOwningPlayerController();
    const auto* G=GetWorld()->GetAuthGameMode<ABreachGameMode>();
    if(!bShowHUD || bSelectionOpen || !P || !PC || PC->GetViewTarget()!=P ||
        !G || G->bGameOver || UGameplayStatics::IsGamePaused(this) ||
        P->Health<=0 || !P->bAiming || P->bReloading || P->bUnarmed || P->UsesSword() ||
        !(P->UsesAK() || P->UsesM4() || P->UsesMP5() || P->UsesAA12())) return 0.f;
    // Reveal only as the sight settles; releasing aim hides it immediately.
    const float T=FMath::Clamp((P->GetAimBlend()-.65f)/.35f,0.f,1.f);
    return T*T*(3.f-2.f*T);
}

void ABreachHUD::DrawAimReticle(const ABreachCharacter* P)
{
    const float Opacity=GetAimReticleOpacity();
    if(!Canvas || Opacity<=0.f) return;
    const FVector2D Center(Canvas->SizeX*.5f,Canvas->SizeY*.5f);
    // Uniform reference enlargement, independent of aspect ratio. AA12 must
    // remain inside its physical lens; the other sights are holographic frames.
    const float ScreenScale=FMath::Min(Canvas->SizeY/900.f,Canvas->SizeX/900.f);
    constexpr float Enlargement=1.20f;
    // The authored outlines matched the previous 76-degree camera. Follow the
    // current projection through ADS so reticle, lens and world enlarge together.
    const float ProjectionScale=FMath::Tan(FMath::DegreesToRadians(76.f*.5f))/
        FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(P->Camera->FieldOfView,5.f,170.f)*.5f));
    const float Scale=ScreenScale*Enlargement*ProjectionScale;
    const auto Point=[&](FVector2D V) { return Center+V*Scale; };
    TArray<FCanvasUVTri> Triangles;
    Triangles.Reserve(1800);
    // All supplied pieces are convex, so a triangle fan preserves their shape.
    const auto Fill=[&](std::initializer_list<FVector2D> Vertices,FLinearColor Color)
    {
        if(Vertices.size()<3) return;
        Color.A*=Opacity;
        const FVector2D* V=Vertices.begin();
        for(int32 I=1;I+1<static_cast<int32>(Vertices.size());++I)
        {
            FCanvasUVTri T;
            T.V0_Pos=Point(V[0]);T.V1_Pos=Point(V[I]);T.V2_Pos=Point(V[I+1]);
            T.V0_UV=T.V1_UV=T.V2_UV=FVector2D::ZeroVector;
            T.V0_Color=T.V1_Color=T.V2_Color=Color;
            Triangles.Add(T);
        }
    };
    // Canvas line primitives ignore translucency. Use batched triangle strips
    // so the fine core, soft halo and ADS fade keep their authored alpha.
    const auto Line=[&](FVector2D A,FVector2D B,FLinearColor Color,float Width)
    {
        const FVector2D Direction=(B-A).GetSafeNormal();
        const FVector2D Normal(-Direction.Y*Width*.5f,Direction.X*Width*.5f);
        Fill({A-Normal,B-Normal,B+Normal,A+Normal},Color);
    };
    const auto GlowLine=[&](FVector2D A,FVector2D B,FLinearColor Color,float Width)
    {
        FLinearColor Glow=Color;Glow.A*=.08f;Line(A,B,Glow,Width+5.f);
        Glow.A=Color.A*.16f;Line(A,B,Glow,Width+2.5f);
        Line(A,B,Color,Width);
    };
    const auto Arc=[&](float Radius,float From,float To,FLinearColor Color,float Width)
    {
        const int32 Steps=FMath::Max(8,FMath::CeilToInt((To-From)*Radius/110.f));
        const auto OnCircle=[&](float Degrees)
        {
            const float A=FMath::DegreesToRadians(Degrees);
            return FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius;
        };
        for(int32 I=0;I<Steps;++I)
            GlowLine(OnCircle(FMath::Lerp(From,To,float(I)/Steps)),
                OnCircle(FMath::Lerp(From,To,float(I+1)/Steps)),Color,Width);
    };
    // A small circular red emitter, with a soft halo rather than a square tile.
    const auto Dot=[&]()
    {
        for(int32 Layer=3;Layer>=0;--Layer)
        {
            const float R=Layer==0?1.35f:1.7f+Layer*.75f;
            const FLinearColor Color(1.f,.025f,.015f,Layer==0?1.f:.10f);
            for(int32 I=0;I<24;++I)
            {
                const float A=2.f*PI*I/24.f,B=2.f*PI*(I+1)/24.f;
                Fill({FVector2D::ZeroVector,FVector2D(FMath::Cos(A),FMath::Sin(A))*R,
                    FVector2D(FMath::Cos(B),FMath::Sin(B))*R},Color);
            }
        }
    };
    if(P->UsesAA12())
    {
        const FLinearColor Amber(1.f,.57f,.16f,.94f);
        for(int32 I=0;I<4;++I) Arc(10.5f,I*90.f+5.f,I*90.f+85.f,Amber,.85f);
    }
    else if(P->UsesAK())
    {
        const FLinearColor Gold(1.f,.80f,.19f,.98f);
        for(float Side:{-1.f,1.f})
        {
            const auto V=[&](float X,float Y) { return FVector2D(Side*X,Y); };
            // Crown tips and tapered, broken lightning wings from reference 2.
            Fill({V(5,-106),V(21,-85),V(30,-87),V(12,-92)},Gold);
            Fill({V(30,-87),V(36,-92),V(33,-75),V(30,-76)},Gold);
            Fill({V(101,-101),V(78,-61),V(86,-58)},Gold);
            Fill({V(86,-58),V(83,-17),V(78,-8),V(80,-59)},Gold);
            Fill({V(78,-8),V(83,-17),V(102,-32)},Gold);
            Fill({V(112,-33),V(101,3),V(74,37),V(88,9)},Gold);
            Fill({V(74,37),V(88,9),V(80,34)},Gold);
            Fill({V(74,37),V(80,34),V(56,63),V(49,81)},Gold);
            Fill({V(79,48),V(66,55),V(72,46),V(86,40)},Gold);
        }
    }
    else if(P->UsesM4())
    {
        const FLinearColor Blue(.015f,.56f,1.f,.96f),Edge(.025f,.18f,1.f,.85f);
        for(float Side:{-1.f,1.f})
        {
            const auto V=[&](float X,float Y) { return FVector2D(Side*X,Y); };
            GlowLine(V(91,-51),V(91,27),Blue,2.2f);
            GlowLine(V(94,-49),V(94,27),Edge,1.1f);
            GlowLine(V(91,-51),V(95,-48),Edge,1.1f);
            Fill({V(90,25),V(94,27),V(72,70),V(67,63)},Blue);
            GlowLine(V(84,-40),V(84,-22),Edge,1.2f);
            GlowLine(V(87,-41),V(87,-22),Blue,1.f);
            GlowLine(V(84,-40),V(87,-41),Blue,1.f);
            GlowLine(V(84,-22),V(87,-22),Blue,1.f);
            Fill({V(102,30),V(117,30),V(114,35),V(100,35)},Blue);
        }
        GlowLine(FVector2D(-27,72),FVector2D(-20,79),Blue,4.f);
        GlowLine(FVector2D(-20,79),FVector2D(-14,76),Blue,4.f);
        GlowLine(FVector2D(-14,76),FVector2D(12,76),Blue,4.f);
        GlowLine(FVector2D(12,76),FVector2D(18,82),Blue,4.f);
        GlowLine(FVector2D(18,82),FVector2D(29,69),Blue,4.f);
    }
    else if(P->UsesMP5())
    {
        const FLinearColor Ice(.61f,.94f,1.f,.97f);
        Arc(99.f,-86.f,56.f,Ice,1.9f);
        Arc(99.f,124.f,266.f,Ice,1.9f);
        for(float Side:{-1.f,1.f})
        {
            const auto V=[&](float X,float Y) { return FVector2D(Side*X,Y); };
            GlowLine(V(98,-18),V(80,0),Ice,1.5f);
            GlowLine(V(80,0),V(98,18),Ice,1.5f);
            GlowLine(V(96,-8),V(87,0),Ice,1.2f);
            GlowLine(V(87,0),V(96,8),Ice,1.2f);
            Fill({V(129,-5),V(141,0),V(129,5),V(117,0)},Ice);
            Fill({V(129,-7),V(133,0),V(129,7),V(125,0)},Ice);
        }
    }
    Dot();
    FCanvasTriangleItem Item(Triangles,GWhiteTexture);
    Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
}
