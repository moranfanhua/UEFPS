#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreachSeabornProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

// Damage and collision run on the server; clients receive the flying mesh's movement.
UCLASS()
class UNDERTIDE_API ABreachSeabornProjectile : public AActor
{
    GENERATED_BODY()
public:
    ABreachSeabornProjectile();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void Launch(FVector Direction,float PhysicalDamage,float NerveDamage,float Speed,float Lifetime,bool bFromWave);
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> Movement;
private:
    float PhysicalDamage=0, NerveDamage=0;
    bool bFromWave=false, bResolved=false;
    UFUNCTION() void OnStopped(const FHitResult& Hit);
};
