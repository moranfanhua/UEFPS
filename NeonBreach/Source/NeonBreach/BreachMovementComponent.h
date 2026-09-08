#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BreachMovementComponent.generated.h"

UCLASS()
class NEONBREACH_API UBreachMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    UBreachMovementComponent();
    static constexpr float RifleSpeed=510.f;
    static constexpr float UnarmedSpeed=790.f;
    UPROPERTY(EditDefaultsOnly, Category="Locomotion") float AimSpeed=300.f;
    UPROPERTY(EditDefaultsOnly, Category="Locomotion") float SpeedRiseRate=1000.f;
    UPROPERTY(EditDefaultsOnly, Category="Locomotion") float SpeedFallRate=1200.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideEntrySpeed=650.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideEntryBoost=200.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideCooldownDuration=0.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideExitSpeed=240.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideDeceleration=300.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideBoostDuration=.12f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideBoostRecoveryTime=1.25f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideMaxSpeed=1200.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideTurnRate=55.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideGravityScale=1.6f;
    // Only controls the animation's entry-to-hold timing, never ends a slide.
    UPROPERTY(EditDefaultsOnly, Category="Slide|Animation") float SlidePoseDuration=1.5f;
    bool IsSliding() const { return bSliding; }
    void SetLocomotionIntent(bool Unarmed,bool Aiming);
    float GetTargetMoveSpeed() const;
    float GetSmoothedMoveSpeed() const { return SmoothedMoveSpeed; }
    virtual float GetMaxSpeed() const override;
    virtual bool CanAttemptJump() const override;
    virtual bool DoJump(bool bReplayingMoves,float DeltaTime) override;
    virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode,uint8 PreviousCustomMode) override;
    virtual void UpdateFromCompressedFlags(uint8 Flags) override;
    virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
    virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;
    virtual void CalcVelocity(float DeltaTime,float Friction,bool bFluid,float BrakingDeceleration) override;
    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    friend class FSavedMove_Breach;
    // Crouch intent already travels in CharacterMovement's compressed input.
    // The server checks actual speed; simulated proxies only receive the pose state.
    UPROPERTY(Replicated) bool bSliding=false;
    bool bPreviousCrouchRequest=false;
    float SlideElapsed=0.f;
    float SlideCooldown=0.f;
    float SlideBoostCooldown=0.f;
    float SlideBoostRemaining=0.f;
    float SmoothedMoveSpeed=RifleSpeed;
    bool bWantsUnarmed=false,bWantsAim=false;
    void TryStartSlide();
    void EndSlide();
};
