#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "BreachPose.h"
#include "BreachSeabornEnemy.generated.h"

class ABreachCharacter;
class UPoseableMeshComponent;
class UAnimSequence;
class UBoxComponent;
class UBreachEnemyAwareness;

UENUM(BlueprintType)
enum class EBreachSeabornSpecies : uint8 { ShellSeaRunner, DeepSeaSlider, SpinalSeaSpitter, SeaDrifter, BowlSeaReaper, FirstSeaPiercer };
UENUM(BlueprintType)
enum class EBreachSeabornAction : uint8 { Idle, Move, Attack, Wake, Dead };

// Level-0 PRTS data, with a single conversion to the arena's units.
struct FBreachSeabornProfile
{
    FString Key;
    float Health=3000, Attack=280, Defense=0, ArtsResistance=20;
    float Speed=1.9f, Interval=1.3f, Range=160, NerveFraction=0;
    bool bFlying=false, bRanged=false, bLowestHealthTarget=false, bDormant=false;
    static constexpr float CombatScale=.05f;
    static constexpr float TileSize=200.f;
};

UCLASS()
class UNDERTIDE_API UBreachArtsDamage : public UDamageType { GENERATED_BODY() };
UCLASS()
class UNDERTIDE_API UBreachTrueDamage : public UDamageType { GENERATED_BODY() };
UCLASS()
class UNDERTIDE_API UBreachArmorPiercingDamage : public UDamageType { GENERATED_BODY() };

// Six arena enemy species; standalone actors still require explicit activation.
UCLASS()
class UNDERTIDE_API ABreachSeabornEnemy : public ACharacter
{
    GENERATED_BODY()
public:
    ABreachSeabornEnemy();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float Damage,const FDamageEvent& Event,AController* Instigator,AActor* Causer) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> DamageHitbox;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBreachEnemyAwareness> Awareness;
    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRepSpecies, Category="Seaborn") EBreachSeabornSpecies Species=EBreachSeabornSpecies::ShellSeaRunner;
    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRepSpecies, Category="Seaborn") bool bMechanicsEnabled=false;
    UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Seaborn") bool bWaveEnemy=false;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Seaborn") float Health=0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Seaborn") float MaxHealth=0;
    UPROPERTY(ReplicatedUsing=OnRepAction, BlueprintReadOnly, Category="Seaborn") EBreachSeabornAction Action=EBreachSeabornAction::Idle;
    UPROPERTY(Replicated) bool bAwake=false;
    UFUNCTION(BlueprintCallable, Category="Seaborn") bool ActivateSpecies(EBreachSeabornSpecies Kind);
    const FBreachSeabornProfile& GetProfile() const { return Profile; }
    bool IsDefeated() const { return Action==EBreachSeabornAction::Dead; }
    bool HasRig() const { return bRigReady; }
    float GetActionTime() const { return ActionTime; }
    float GetAttackCooldown() const { return AttackCooldown; }
    float GetAttackDuration() const;
    float GetWakeDuration() const;
    ABreachCharacter* SelectTarget() const;
    bool CanHit(const ABreachCharacter* Player) const;
    // Server simulation also used by the deterministic diagnostic scene.
    void AdvanceMechanics(float DeltaSeconds);
    UFUNCTION(BlueprintCallable, Category="Seaborn") void ApplyIncapacitation(float Seconds);
    UFUNCTION(BlueprintCallable, Category="Seaborn") void ApplyDisarm(float Seconds);
private:
    FBreachSeabornProfile Profile;
    FBreachPose Pose;
    UPROPERTY() TObjectPtr<UAnimSequence> IdleClip;
    UPROPERTY() TObjectPtr<UAnimSequence> MoveClip;
    UPROPERTY() TObjectPtr<UAnimSequence> AttackClip;
    UPROPERTY() TObjectPtr<UAnimSequence> WakeClip;
    UPROPERTY() TObjectPtr<UAnimSequence> AwakeMoveClip;
    UPROPERTY() TObjectPtr<UAnimSequence> DieClip;
    UPROPERTY(Replicated) float ActionStarted=0;
    UPROPERTY(Replicated) float Incapacitated=0;
    float Disarmed=0, DormantTime=0, AttackCooldown=0, ActionTime=0, MoveTime=0;
    float FlightAnchorHeight=0;
    bool bRigReady=false, bHitApplied=false;
    TWeakObjectPtr<ABreachCharacter> AttackTarget;
    UFUNCTION() void OnRepSpecies();
    UFUNCTION() void OnRepAction();
    void LoadPresentation();
    void SetProfile();
    void SetAction(EBreachSeabornAction Next);
    void UpdatePresentation(float DeltaSeconds);
    void ResolveAttack();
    void TryWake();
    void ApplyNerveAura(float DeltaSeconds);
    void Die(bool bHeadshot=false);
};
