#include "BreachNerveDamageComponent.h"
#include "BreachGame.h"
#include "BreachSeabornEnemy.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundWaveProcedural.h"
#include "Net/UnrealNetwork.h"

UBreachNerveDamageComponent::UBreachNerveDamageComponent()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.bTickEvenWhenPaused=true;
    SetIsReplicatedByDefault(true);
}

float UBreachNerveDamageComponent::GetBurstRemaining() const
{
    return GetOwner()->HasAuthority()?BurstRemaining:LocalBurstRemaining;
}

void UBreachNerveDamageComponent::ApplyNerveDamage(float Amount,AActor* Source)
{
    auto* Player=Cast<ABreachCharacter>(GetOwner());
    if(!Player || !Player->HasAuthority() || Player->Health<=0 || IsBurstActive() || !FMath::IsFinite(Amount) || Amount<=0) return;
    Accumulated=FMath::Min(Threshold,Accumulated+Amount);
    if(Accumulated>=Threshold)
    {
        Accumulated=0; BurstRemaining=BurstDuration;
        Player->OnNerveBurst();
        UGameplayStatics::ApplyDamage(Player,BurstDamage,Source?Source->GetInstigatorController():nullptr,Source,UBreachTrueDamage::StaticClass());
        RefreshFeedback();
    }
    Player->ForceNetUpdate();
}

void UBreachNerveDamageComponent::RecoverNerveDamage(float Amount)
{
    if(GetOwner()->HasAuthority() && !IsBurstActive() && FMath::IsFinite(Amount) && Amount>0)
    {
        Accumulated=FMath::Max(0.f,Accumulated-Amount);
        GetOwner()->ForceNetUpdate();
    }
}

void UBreachNerveDamageComponent::AdvanceRecovery(float Dt)
{
    if(!FMath::IsFinite(Dt) || Dt<=0) return;
    if(UGameplayStatics::IsGamePaused(this)) { RefreshFeedback(); return; }
    if(GetOwner()->HasAuthority()) BurstRemaining=FMath::Max(0.f,BurstRemaining-Dt);
    else LocalBurstRemaining=FMath::Max(0.f,LocalBurstRemaining-Dt);
    RefreshFeedback();
}

void UBreachNerveDamageComponent::OnRepBurst()
{
    const bool WasActive=LocalBurstRemaining>0;
    LocalBurstRemaining=BurstRemaining;
    if(!WasActive && IsBurstActive()) if(auto* Player=Cast<ABreachCharacter>(GetOwner())) Player->OnNerveBurst();
    RefreshFeedback();
}

TArray<int16> UBreachNerveDamageComponent::MakeTinnitusPCM()
{
    constexpr int32 Rate=48000, Frames=Rate*10;
    TArray<int16> PCM; PCM.SetNumUninitialized(Frames*2);
    for(int32 I=0;I<Frames;++I)
    {
        const float Time=float(I)/Rate;
        const float Fade=FMath::Min(FMath::Clamp(Time/.15f,0.f,1.f),FMath::Clamp((10.f-Time)/1.5f,0.f,1.f));
        for(int32 Channel=0;Channel<2;++Channel)
        {
            const float Tone=FMath::Sin(2*PI*(Channel?3180.f:3100.f)*Time);
            PCM[I*2+Channel]=int16(Tone*Fade*1800.f);
        }
    }
    return PCM;
}

void UBreachNerveDamageComponent::RefreshFeedback()
{
    auto* Player=Cast<ABreachCharacter>(GetOwner());
    if(!Player || !Player->IsLocallyControlled() || Player->Health<=0 || !IsBurstActive()) { StopFeedback(); return; }
    if(!bFeedbackActive)
    {
        bFeedbackActive=true;
        if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Effects/PP_NerveBlur.PP_NerveBlur")))
        {
            BlurMaterial=UMaterialInstanceDynamic::Create(Material,this);
            Player->Camera->AddOrUpdateBlendable(BlurMaterial,1.f);
        }
        TinnitusWave=NewObject<USoundWaveProcedural>(this);
        TinnitusWave->SetSampleRate(48000);
        TinnitusWave->NumChannels=2;
        TinnitusWave->Duration=BurstDuration;
        TinnitusWave->bLooping=false;
        const TArray<int16> PCM=MakeTinnitusPCM();
        TinnitusWave->QueueAudio(reinterpret_cast<const uint8*>(PCM.GetData()),PCM.Num()*sizeof(int16));
        TinnitusAudio=UGameplayStatics::SpawnSound2D(this,TinnitusWave,.3f,1.f,0.f,nullptr,false,false);
    }
    if(BlurMaterial) BlurMaterial->SetScalarParameterValue(TEXT("Strength"),FMath::Clamp(GetBurstRemaining(),0.f,1.f));
    if(TinnitusAudio) TinnitusAudio->SetPaused(UGameplayStatics::IsGamePaused(this));
}

void UBreachNerveDamageComponent::StopFeedback()
{
    if(BlurMaterial) if(auto* Player=Cast<ABreachCharacter>(GetOwner())) Player->Camera->RemoveBlendable(BlurMaterial);
    BlurMaterial=nullptr;
    if(TinnitusAudio) { TinnitusAudio->Stop(); TinnitusAudio->DestroyComponent(); }
    TinnitusAudio=nullptr; TinnitusWave=nullptr; bFeedbackActive=false;
}

bool UBreachNerveDamageComponent::IsTinnitusPlaying() const { return TinnitusAudio && TinnitusAudio->IsPlaying(); }
void UBreachNerveDamageComponent::TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(Dt,TickType,ThisTickFunction);
    AdvanceRecovery(Dt);
}
void UBreachNerveDamageComponent::EndPlay(const EEndPlayReason::Type Reason) { StopFeedback(); Super::EndPlay(Reason); }
void UBreachNerveDamageComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UBreachNerveDamageComponent,Accumulated);
    DOREPLIFETIME(UBreachNerveDamageComponent,BurstRemaining);
}
