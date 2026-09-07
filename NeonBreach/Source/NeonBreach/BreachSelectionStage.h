#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreachPose.h"
#include "BreachSelectionStage.generated.h"

class UCameraComponent;
class UPoseableMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UAnimSequence;

UCLASS()
class NEONBREACH_API ABreachSelectionStage : public AActor
{
    GENERATED_BODY()
public:
    ABreachSelectionStage();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void SetOpen(bool Open);
    void Select(int32 Index);
    int32 SelectedIndex=0;
    float AnimationTime=0;
    bool HasEntrance() const { return Entrance!=nullptr && Idle!=nullptr; }
    UTextureRenderTarget2D* Portrait(int32 Index) const;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Preview;
private:
    UPROPERTY() TArray<TObjectPtr<UPoseableMeshComponent>> PortraitBodies;
    UPROPERTY() TArray<TObjectPtr<USceneCaptureComponent2D>> PortraitCameras;
    UPROPERTY() TArray<TObjectPtr<UTextureRenderTarget2D>> PortraitTargets;
    UPROPERTY() TObjectPtr<UAnimSequence> Entrance;
    UPROPERTY() TObjectPtr<UAnimSequence> Idle;
    FBreachPose Pose;
    double PreviousTime=0;
    int32 WarmupFrames=0;
    bool bOpen=false;
};
