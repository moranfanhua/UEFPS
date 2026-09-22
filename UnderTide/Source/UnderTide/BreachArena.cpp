#include "BreachGame.h"
#include "BreachVisuals.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SkyLight.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

USkeleton* ABreachGameMode::EnsureSkeleton(USkeletalMesh* CharacterAsset)
{
    if(!CharacterAsset) return nullptr;
    if(CharacterAsset->GetSkeleton()) return CharacterAsset->GetSkeleton();
    const FString PackageName=CharacterAsset->GetOutermost()->GetName()+TEXT("_Skeleton");
    UPackage* Package=CreatePackage(*PackageName);
    auto* SkeletonAsset=NewObject<USkeleton>(Package,FName(*FPackageName::GetLongPackageAssetName(PackageName)),RF_Public|RF_Standalone);
    BindSkeleton(CharacterAsset,SkeletonAsset);
    return SkeletonAsset;
}

void ABreachGameMode::BindSkeleton(USkeletalMesh* CharacterAsset, USkeleton* SkeletonAsset)
{
    if(!CharacterAsset || !SkeletonAsset) return;
    CharacterAsset->SetSkeleton(SkeletonAsset);
    SkeletonAsset->MergeAllBonesToBoneTree(CharacterAsset);
    CharacterAsset->MarkPackageDirty();
    SkeletonAsset->MarkPackageDirty();
}

void ABreachGameMode::BakeArena(UObject* WorldContext)
{
    UWorld* World=GEngine->GetWorldFromContextObjectChecked(WorldContext);
    auto* Builder=World->SpawnActor<ABreachGameMode>();
    Builder->BuildArena();
    Builder->Destroy();
}

int32 ABreachGameMode::OrganizeArenaOutliner(UObject* WorldContext)
{
#if WITH_EDITOR
    UWorld* World=GEngine->GetWorldFromContextObject(WorldContext,EGetWorldErrorMode::ReturnNull);
    if(!World || World->IsGameWorld()) return 0;
    int32 Changed=0;
    for(TActorIterator<AActor> It(World);It;++It)
    {
        AActor* Actor=*It;
        const bool PlayerStart=Actor->IsA<APlayerStart>();
        if(!PlayerStart && !Actor->ActorHasTag(TEXT("BreachArena"))) continue;
        const FVector P=Actor->GetActorLocation();
        FString Folder,Label;
        if(PlayerStart) { Folder=TEXT("06_Gameplay/Player_Start"); Label=TEXT("Player_Start_Facing_Arena"); }
        else if(auto* Sign=Actor->FindComponentByClass<UTextRenderComponent>())
        {
            Folder=TEXT("02_Props_and_Cover/Signs"); Label=TEXT("Sign_")+Sign->Text.ToString();
            if(P.X<-2100 && P.Z<450)
            {
                const int32 Bay=FMath::Clamp(FMath::RoundToInt((P.Y+660.f)/440.f),0,3);
                Folder=FString::Printf(TEXT("03_Operator_Displays/%02d_%s"),Bay+1,Breach::Keys[Bay]);
                Label=FString::Printf(TEXT("Operator_Placard_%s"),Breach::Keys[Bay]);
            }
        }
        else if(Actor->IsA<ADirectionalLight>()) { Folder=TEXT("04_Lighting/Key_Light"); Label=TEXT("Arena_Key_Light"); }
        else if(Actor->IsA<APointLight>())
        {
            Folder=P.Z>500?TEXT("04_Lighting/Area_Fill"):TEXT("04_Lighting/Display_Fill");
            Label=P.Z>500?TEXT("Area_Fill_Light"):TEXT("Display_Fill_Light");
        }
        else if(Actor->IsA<ASkyLight>()) { Folder=TEXT("04_Lighting/Ambient"); Label=TEXT("Sky_Ambient_Light"); }
        else if(Actor->IsA<APostProcessVolume>()) { Folder=TEXT("05_Post_Process"); Label=TEXT("Global_Post_Process"); }
        else if(auto* Mesh=Actor->FindComponentByClass<UStaticMeshComponent>())
        {
            const FString Mat=Mesh->GetMaterial(0)?Mesh->GetMaterial(0)->GetName():FString();
            const FVector Size=Actor->GetActorScale3D().GetAbs()*100.f;
            const bool Emissive=Mat==TEXT("M_Cyan") || Mat==TEXT("M_White") || Mat==TEXT("M_Orange");
            if(Mat==TEXT("M_Floor")) { Folder=TEXT("01_Structure/Floor"); Label=TEXT("Arena_Floor"); }
            else if(P.Z<5) { Folder=TEXT("01_Structure/Floor"); Label=Size.X<Size.Y?TEXT("Floor_Grid_Line_Y"):TEXT("Floor_Grid_Line_X"); }
            else if(P.Z>700)
            {
                Folder=TEXT("01_Structure/Ceiling"); Label=Emissive?TEXT("Ceiling_Light_Bar"):(Size.X>4000?TEXT("Ceiling_Panel"):TEXT("Ceiling_Beam"));
            }
            else if(P.X>=-2180 && P.X<-1800 && FMath::Abs(P.Y)<1000 && P.Z<450)
            {
                const int32 Bay=FMath::Clamp(FMath::RoundToInt((P.Y+660.f)/440.f),0,3);
                Folder=FString::Printf(TEXT("03_Operator_Displays/%02d_%s"),Bay+1,Breach::Keys[Bay]);
                Label=Emissive?TEXT("Display_Light_Strip"):(P.Z<50?TEXT("Display_Pedestal"):TEXT("Display_Backdrop"));
            }
            else if(FMath::Abs(P.X-250)<250 && FMath::Abs(P.Y)<250)
            {
                Folder=TEXT("02_Props_and_Cover/Central_Reactor");
                Label=Emissive?TEXT("Reactor_Emissive_Core"):(P.Z<50?TEXT("Reactor_Base"):(P.Z>300?TEXT("Reactor_Cap"):(Size.X<50?TEXT("Reactor_Support"):TEXT("Reactor_Body"))));
            }
            else if(P.X>2100 && FMath::Abs(P.Y)>900 && FMath::Abs(P.Y)<1400)
            {
                Folder=TEXT("02_Props_and_Cover/Exit_Frames"); Label=Emissive?TEXT("Exit_Outline_Light"):TEXT("Exit_Panel");
            }
            else if(FMath::Abs(P.X)<1800 && FMath::Abs(P.Y)<1100 && P.Z<200)
            {
                Folder=TEXT("02_Props_and_Cover/Combat_Cover");
                Label=Emissive?TEXT("Cover_Indicator_Light"):(Mat==TEXT("M_Wall")?TEXT("Cover_Side_Panel"):(Size.Z<10?TEXT("Cover_Top"):TEXT("Cover_Body")));
            }
            else
            {
                Folder=Emissive?TEXT("01_Structure/Boundary_Lighting"):TEXT("01_Structure/Walls_and_Supports");
                Label=Emissive?TEXT("Boundary_Light_Strip"):(Mat==TEXT("M_Wall")?TEXT("Perimeter_Wall"):(P.Z>600?TEXT("Ceiling_Brace"):TEXT("Wall_Support")));
            }
        }
        if(Folder.IsEmpty()) continue;
        if(!PlayerStart && !Actor->FindComponentByClass<UTextRenderComponent>())
            Label+=FString::Printf(TEXT(" [X%d Y%d Z%d]"),FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y),FMath::RoundToInt(P.Z));
        // Fill missing metadata only: later manual folder and label edits win.
        const bool SetFolder=Actor->GetFolderPath().IsNone();
        const FString ExistingLabel=Actor->GetActorLabel();
        const bool SetLabel=ExistingLabel==Actor->GetName() || ExistingLabel==Actor->GetName().Replace(TEXT("_"),TEXT(""));
        if(SetFolder || SetLabel)
        {
            Actor->Modify();
            if(SetFolder) Actor->SetFolderPath(FName(*Folder));
            if(SetLabel) Actor->SetActorLabel(Label);
            ++Changed;
        }
    }
    return Changed;
#else
    return 0;
#endif
}

namespace Breach
{
    const TCHAR* Keys[4]={TEXT("Eula"),TEXT("Acheron"),TEXT("Lizhiyan"),TEXT("Ascalon")};
    const TCHAR* Names[4]={TEXT("EULA / GLACIER"),TEXT("ACHERON"),TEXT("LIZHIYAN"),TEXT("ASCALON")};
    USkeletalMesh* CharacterMesh(int32 Index)
    {
        Index=FMath::Clamp(Index,0,3);
        return LoadObject<USkeletalMesh>(nullptr,*FString::Printf(TEXT("/Game/Characters/%s/SK_%s.SK_%s"),Keys[Index],Keys[Index],Keys[Index]));
    }
    UMaterialInterface* Material(const TCHAR* Name)
    {
        return LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Materials/%s.%s"),Name,Name));
    }
    void Beam(UWorld* World,const FVector& From,const FVector& To,const FLinearColor& Color,float Width,float Life)
    {
        DrawDebugLine(World,From,To,Color.ToFColor(true),false,Life,0,Width);
    }
}

void ABreachGameMode::BuildArena()
{
    bool HasArena=false;
    for(TActorIterator<AActor> It(GetWorld());It;++It) if(It->ActorHasTag(TEXT("BreachArena"))) HasArena=true;
    if(HasArena)
    {
        OrganizeArenaOutliner(this);
        // Existing baked maps keep their geometry and lighting edits, while
        // operator placards follow the current roster just like the HUD.
        for(TActorIterator<AActor> It(GetWorld());It;++It)
            if(It->ActorHasTag(TEXT("BreachArena")))
                if(auto* Sign=It->FindComponentByClass<UTextRenderComponent>())
                    for(int32 I=0;I<4;++I)
                    {
                        const FString Prefix=FString::Printf(TEXT("0%d / "),I+1);
                        if(Sign->Text.ToString().StartsWith(Prefix))
                            Sign->SetText(FText::FromString(Prefix+Breach::Names[I]));
                    }
        for(TActorIterator<ADirectionalLight> It(GetWorld());It;++It)
            if(It->ActorHasTag(TEXT("BreachArena")))
            {
                It->GetLightComponent()->bCastShadowsFromCinematicObjectsOnly=true;
                It->GetLightComponent()->SetCastShadows(true);
            }
        for(TActorIterator<APointLight> It(GetWorld());It;++It)
            if(It->ActorHasTag(TEXT("BreachArena")) && It->GetActorLocation().Z>500)
            {
                It->GetLightComponent()->SetCastShadows(true);
                Cast<UPointLightComponent>(It->GetLightComponent())->SetSourceRadius(12.f);
            }
        return;
    }
    UStaticMesh* Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    const auto Shape=[&](FVector Pos,FVector Size,const TCHAR* Mat,bool Collision=true,UStaticMesh* Mesh=nullptr,FRotator Rot=FRotator::ZeroRotator)
    {
        auto* A=GetWorld()->SpawnActor<AStaticMeshActor>(Pos,Rot);
        A->Tags.Add(TEXT("BreachArena"));
        auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable);
        C->SetStaticMesh(Mesh?Mesh:Cube); C->SetMaterial(0,Breach::Material(Mat));
        A->SetActorScale3D(Size/100.f);
        C->SetCollisionEnabled(Collision?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
        C->SetCollisionResponseToAllChannels(ECR_Block);
        return A;
    };
    const auto Sign=[&](FVector Pos,FRotator Rot,const FString& Label,float Size,FColor Color)
    {
        auto* A=GetWorld()->SpawnActor<AActor>(Pos,Rot); A->Tags.Add(TEXT("BreachArena"));
        auto* T=NewObject<UTextRenderComponent>(A,TEXT("Sign"));
        A->SetRootComponent(T); A->AddInstanceComponent(T); T->RegisterComponent();
        T->SetWorldLocationAndRotation(Pos,Rot);
        T->SetText(FText::FromString(Label)); T->SetWorldSize(Size);
        T->SetHorizontalAlignment(EHTA_Center); T->SetTextRenderColor(Color);
    };
    Shape(FVector(0,0,-35),FVector(4600,3600,70),TEXT("M_Floor"));
    for(int32 i=-5;i<=5;++i)
    {
        Shape(FVector(i*400,0,1),FVector(3,3440,1),TEXT("M_Metal"),false);
        Shape(FVector(0,i*300,1),FVector(4400,3,1),TEXT("M_Metal"),false);
    }
    // Solid perimeter, with bright low strips that make the playable boundary legible.
    for(int32 Side : {-1,1})
    {
        Shape(FVector(0,Side*1750,300),FVector(4600,100,600),TEXT("M_Wall"));
        Shape(FVector(Side*2250,0,300),FVector(100,3500,600),TEXT("M_Wall"));
        Shape(FVector(0,Side*1695,28),FVector(4390,5,7),TEXT("M_Cyan"),false);
        Shape(FVector(Side*2195,0,28),FVector(5,3390,7),TEXT("M_Cyan"),false);
        Shape(FVector(0,Side*1695,540),FVector(4390,6,4),TEXT("M_White"),false);
        for(int32 X=-1800;X<=1800;X+=600)
        {
            Shape(FVector(X,Side*1680,310),FVector(35,60,610),TEXT("M_Dark"));
            Shape(FVector(X,Side*1647,350),FVector(7,3,170),TEXT("M_Cyan"),false);
            Shape(FVector(X,Side*1570,650),FVector(50,330,50),TEXT("M_Metal"),false,nullptr,FRotator(0,0,Side*25));
        }
    }
    // Ceiling panels, structural beams and hanging light bars.
    Shape(FVector(0,0,870),FVector(4600,3600,50),TEXT("M_Dark"));
    for(int32 X=-1800;X<=1800;X+=900)
    {
        Shape(FVector(X,0,760),FVector(55,3500,60),TEXT("M_Metal"),false);
        for(int32 Y : {-1050,0,1050}) Shape(FVector(X,Y,725),FVector(22,600,9),TEXT("M_White"),false);
    }
    // Central reactor: tall silhouette, with clear corridors on either side.
    Shape(FVector(250,0,25),FVector(440,440,50),TEXT("M_Dark"),true,Cylinder);
    Shape(FVector(250,0,160),FVector(150,150,270),TEXT("M_Metal"),true,Cylinder);
    Shape(FVector(250,0,172),FVector(155,155,120),TEXT("M_Cyan"),false,Cylinder);
    Shape(FVector(250,0,342),FVector(210,210,50),TEXT("M_Dark"),true,Cylinder);
    for(int32 i=0;i<4;++i)
    {
        const float Angle=i*PI*.5f;
        Shape(FVector(250+FMath::Cos(Angle)*96,FMath::Sin(Angle)*96,175),FVector(36,36,310),TEXT("M_Dark"));
    }
    const FVector Cover[]={FVector(-550,-800,70),FVector(-550,800,70),FVector(1000,-850,70),FVector(1000,850,70),FVector(1550,0,70)};
    for(const FVector& P:Cover)
    {
        Shape(P,FVector(280,130,140),TEXT("M_Metal"));
        Shape(P+FVector(0,0,72),FVector(284,134,6),TEXT("M_Dark"),false);
        Shape(P+FVector(-143,0,25),FVector(4,95,8),TEXT("M_Orange"),false);
        for(int32 S : {-1,1}) Shape(P+FVector(0,S*66,0),FVector(240,3,90),TEXT("M_Wall"),false);
    }
    // Four source-character display bays behind the start position.
    for(int32 i=0;i<4;++i)
    {
        float Y=-660+i*440;
        Shape(FVector(-1900,Y,12),FVector(270,270,24),TEXT("M_Metal"),true,Cylinder);
        Shape(FVector(-1900,Y,27),FVector(235,235,6),TEXT("M_Cyan"),false,Cylinder);
        Shape(FVector(-2175,Y,200),FVector(22,390,370),TEXT("M_Dark"),false);
        Shape(FVector(-2160,Y-190,210),FVector(10,5,330),TEXT("M_Cyan"),false);
        Shape(FVector(-2160,Y+190,210),FVector(10,5,330),TEXT("M_Cyan"),false);
        Sign(FVector(-2145,Y,370),FRotator(0,0,0),FString::Printf(TEXT("0%d / %s"),i+1,Breach::Names[i]),25,FColor(130,220,255));
        auto* BayLight=GetWorld()->SpawnActor<APointLight>(FVector(-1650,Y,220),FRotator::ZeroRotator);
        BayLight->Tags.Add(TEXT("BreachArena"));
        auto* BayComponent=Cast<UPointLightComponent>(BayLight->GetLightComponent());
        BayComponent->SetIntensity(6000);
        BayComponent->SetAttenuationRadius(650);
        BayComponent->SetLightColor(FLinearColor(.85f,.92f,1));
        BayComponent->SetCastShadows(false);
    }
    Sign(FVector(-2180,0,495),FRotator(0,0,0),TEXT("OPERATOR ARCHIVE"),68,FColor(160,224,255));
    Sign(FVector(2180,0,405),FRotator(0,180,0),TEXT("NEON / BREACH"),100,FColor(220,238,255));
    Sign(FVector(2175,0,300),FRotator(0,180,0),TEXT("SECTOR 07     /     COMBAT SIMULATION"),28,FColor(130,200,230));
    for(int32 Side : {-1,1})
    {
        Shape(FVector(2170,Side*1120,190),FVector(35,350,380),TEXT("M_Dark"));
        Shape(FVector(2147,Side*1120-174,190),FVector(5,8,380),TEXT("M_Orange"),false);
        Shape(FVector(2147,Side*1120+174,190),FVector(5,8,380),TEXT("M_Orange"),false);
        Shape(FVector(2147,Side*1120,382),FVector(5,350,8),TEXT("M_Orange"),false);
    }
    for(int32 X : {-1600,-400,800,1750}) for(int32 Y : {-1050,1050})
    {
        auto* L=GetWorld()->SpawnActor<APointLight>(FVector(X,Y,610),FRotator::ZeroRotator);
        L->Tags.Add(TEXT("BreachArena"));
        Cast<UPointLightComponent>(L->GetLightComponent())->SetIntensity(42000);
        Cast<UPointLightComponent>(L->GetLightComponent())->SetAttenuationRadius(1800);
        Cast<UPointLightComponent>(L->GetLightComponent())->SetSourceRadius(12);
        Cast<UPointLightComponent>(L->GetLightComponent())->SetLightColor(FLinearColor(.73f,.85f,1));
        Cast<UPointLightComponent>(L->GetLightComponent())->SetCastShadows(true);
    }
    auto* Sun=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,500),FRotator(-62,-30,0));
    Sun->Tags.Add(TEXT("BreachArena"));
    Sun->GetLightComponent()->SetIntensity(2.0f);
    Sun->GetLightComponent()->SetLightColor(FLinearColor(.68f,.80f,1));
    Sun->GetLightComponent()->bCastShadowsFromCinematicObjectsOnly=true;
    Sun->GetLightComponent()->SetCastShadows(true);
    auto* PP=GetWorld()->SpawnActor<APostProcessVolume>(); PP->Tags.Add(TEXT("BreachArena")); PP->bUnbound=true;
    PP->Settings.bOverride_AutoExposureMethod=true; PP->Settings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    PP->Settings.bOverride_AutoExposureBias=true; PP->Settings.AutoExposureBias=1;
    PP->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure=true; PP->Settings.AutoExposureApplyPhysicalCameraExposure=false;
    PP->Settings.bOverride_BloomIntensity=true; PP->Settings.BloomIntensity=.45f;
    PP->Settings.bOverride_VignetteIntensity=true; PP->Settings.VignetteIntensity=.22f;
    OrganizeArenaOutliner(this);
}
