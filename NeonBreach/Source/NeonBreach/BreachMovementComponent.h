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
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideEntrySpeed=650.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideEntryBoost=200.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideCooldownDuration=0.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideExitSpeed=240.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideDeceleration=300.f;
    UPROPERTY(EditDefaultsOnly, Category="Slide") float SlideMaxDuration=1.5f;
    bool IsSliding() const { return bSliding; }
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
};
