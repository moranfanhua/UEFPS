#include "BreachSeabornProjectile.h"
#include "BreachGame.h"
#include "BreachNerveDamageComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ABreachSeabornProjectile::ABreachSeabornProjectile()
{
    PrimaryActorTick.bCanEverTick=true;
    bReplicates=true; SetReplicateMovement(true);
    SetNetUpdateFrequency(60); SetMinNetUpdateFrequency(30);
    InitialLifeSpan=4;
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("ProjectileCollision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(8);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
    Collision->SetGenerateOverlapEvents(false);
    Collision->SetCanEverAffectNavigation(false);
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileVisual"));
    Visual->SetupAttachment(Collision);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetCastShadow(false);
    Visual->SetCanEverAffectNavigation(false);
    Visual->SetRelativeScale3D(FVector(.24f,.16f,.16f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Materials/M_Cyan.M_Cyan"));
    if(Mesh.Succeeded()) Visual->SetStaticMesh(Mesh.Object);
    if(Material.Succeeded()) Visual->SetMaterial(0,Material.Object);
    Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    Movement->SetUpdatedComponent(Collision);
    Movement->ProjectileGravityScale=0;
    Movement->bRotationFollowsVelocity=true;
    Movement->bInitialVelocityInLocalSpace=false;
    Movement->bShouldBounce=false;
    Movement->bSweepCollision=true;
    Movement->OnProjectileStop.AddDynamic(this,&ABreachSeabornProjectile::OnStopped);
}

void ABreachSeabornProjectile::Launch(FVector Direction,float Damage,float Nerve,float Speed,float Lifetime,bool FromWave)
{
    if(!HasAuthority() || Direction.ContainsNaN() || Direction.IsNearlyZero() ||
        !FMath::IsFinite(Damage) || Damage<=0 || !FMath::IsFinite(Nerve) || Nerve<0 ||
        !FMath::IsFinite(Speed) || Speed<=0 || !FMath::IsFinite(Lifetime) || Lifetime<=0)
    {
        Destroy(); return;
    }
    PhysicalDamage=Damage; NerveDamage=Nerve; bFromWave=FromWave;
    Movement->InitialSpeed=Movement->MaxSpeed=Speed;
    Movement->Velocity=Direction.GetSafeNormal()*Speed;
    // Deferred spawning copies the lifetime before BeginPlay installs its timer.
    InitialLifeSpan=Lifetime;
}

void ABreachSeabornProjectile::BeginPlay()
{
    Super::BeginPlay();
    if(HasAuthority())
    {
        if(GetOwner()) Collision->IgnoreActorWhenMoving(GetOwner(),true);
        if(GetInstigator()) Collision->IgnoreActorWhenMoving(GetInstigator(),true);
    }
    else
    {
        Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Movement->Deactivate(); SetActorTickEnabled(false);
    }
}

void ABreachSeabornProjectile::Tick(float Dt)
{
    Super::Tick(Dt);
    if(HasAuthority() && bFromWave)
        if(const auto* Mode=GetWorld()->GetAuthGameMode<ABreachGameMode>(); Mode && (Mode->bGameOver || Mode->bGallery))
        {
            Movement->Deactivate(); Destroy();
        }
}

void ABreachSeabornProjectile::OnStopped(const FHitResult& Hit)
{
    if(!HasAuthority() || bResolved) return;
    bResolved=true;
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    bool CanDamage=!UGameplayStatics::IsGamePaused(this);
    if(bFromWave)
        if(const auto* Mode=GetWorld()->GetAuthGameMode<ABreachGameMode>()) CanDamage&=!Mode->bGameOver && !Mode->bGallery;
    if(auto* Player=Cast<ABreachCharacter>(Hit.GetActor()); CanDamage && IsValid(Player) && Player->Health>0)
    {
        UGameplayStatics::ApplyDamage(Player,PhysicalDamage,GetInstigatorController(),this,UDamageType::StaticClass());
        if(Player->NerveDamage) Player->NerveDamage->ApplyNerveDamage(NerveDamage,this);
    }
    Destroy();
}
