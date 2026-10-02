#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BreachNerveDamageComponent.generated.h"

class USoundWaveProcedural;
class UAudioComponent;
class UMaterialInstanceDynamic;

UCLASS(ClassGroup=(Breach), meta=(BlueprintSpawnableComponent))
class UNDERTIDE_API UBreachNerveDamageComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UBreachNerveDamageComponent();
    virtual void TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    static constexpr float Threshold=1000.f;
    static constexpr float BurstDuration=10.f;
    static constexpr float BurstDamage=50.f;
    static constexpr float FireIntervalMultiplier=2.5f;
    UFUNCTION(BlueprintCallable, Category="Nerve Damage") void ApplyNerveDamage(float Amount,AActor* Source);
    UFUNCTION(BlueprintCallable, Category="Nerve Damage") void RecoverNerveDamage(float Amount);
    bool IsBurstActive() const { return GetBurstRemaining()>0; }
    float GetBurstRemaining() const;
    float GetAccumulated() const { return Accumulated; }
    float GetMeterFraction() const { return IsBurstActive()?1.f:FMath::Clamp(Accumulated/Threshold,0.f,1.f); }
    float GetFireIntervalMultiplier() const { return IsBurstActive()?FireIntervalMultiplier:1.f; }
    void AdvanceRecovery(float Dt);
    void RefreshFeedback();
    void StopFeedback();
    bool HasBlurFeedback() const { return BlurMaterial!=nullptr; }
    bool IsTinnitusPlaying() const;
    static TArray<int16> MakeTinnitusPCM();
private:
    UPROPERTY(Replicated) float Accumulated=0;
    UPROPERTY(ReplicatedUsing=OnRepBurst) float BurstRemaining=0;
    float LocalBurstRemaining=0;
    bool bFeedbackActive=false;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> TinnitusWave;
    UPROPERTY() TObjectPtr<UAudioComponent> TinnitusAudio;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BlurMaterial;
    UFUNCTION() void OnRepBurst();
};
