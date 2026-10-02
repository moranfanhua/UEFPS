#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BreachEnemyAwareness.generated.h"

class ABreachCharacter;
class AController;

UENUM(BlueprintType)
enum class EBreachEnemyIntent : uint8 { Wander, Pursue, Investigate, Wait, Dead };

// Server-only decisions shared by wave enemies and opt-in Seaborn actors.
UCLASS(ClassGroup=(AI), meta=(BlueprintSpawnableComponent))
class UNDERTIDE_API UBreachEnemyAwareness : public UActorComponent
{
    GENERATED_BODY()
public:
    UBreachEnemyAwareness();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Awareness", meta=(ClampMin="0")) float SightRadius=1600;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Awareness", meta=(ClampMin="0", ClampMax="180")) float SightHalfAngle=60;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Awareness", meta=(ClampMin="0")) float SignalRadius=5000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Awareness", meta=(ClampMin="0.05")) float SignalInterval=.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Awareness", meta=(ClampMin="0", ClampMax="1")) float WanderSpeedFraction=.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Awareness", meta=(ClampMin="0")) float WanderMinRadius=700;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Awareness", meta=(ClampMin="0")) float WanderRadius=3000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Awareness", meta=(ClampMin="1")) float ArrivalRadius=80;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Awareness") EBreachEnemyIntent Intent=EBreachEnemyIntent::Wander;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Awareness") FVector Destination=FVector::ZeroVector;
    UFUNCTION(BlueprintCallable, Category="Awareness") void ReceiveSignal(FVector Position);
    void NotifyDamage(AController* DamageInstigator,AActor* DamageCauser);
    bool CanSee(const ABreachCharacter* Player) const;
    ABreachCharacter* GetVisibleTarget() const { return VisibleTarget.Get(); }
    float GetCombatSpeed() const { return CombatSpeed; }
    void Reset(float Speed);
    void SetCombatSpeed(float Speed);
    void Advance(float DeltaSeconds);
    bool MoveTowardDestination(float DeltaSeconds,float StopDistance=0,bool bFlying=false,float FlightHeight=0);
    void Stop();
    void Die();
private:
    TWeakObjectPtr<ABreachCharacter> VisibleTarget;
    float CombatSpeed=0, BroadcastCooldown=0, WanderTime=0, AvoidSide=0;
    bool bWanderPoint=false;
    bool IsActive() const;
    void Broadcast(FVector Position);
    void PickWanderPoint();
};
