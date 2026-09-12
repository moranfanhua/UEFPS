#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreachPose.h"
#include "BreachCloth.h"
#include "BreachSelectionStage.generated.h"

class UCameraComponent;
class UPoseableMeshComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UAnimSequence;
class UStaticMeshComponent;

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
    static constexpr float EntranceDuration=3.5f;
    bool IsHoldingEntrance() const { return AnimationTime>=EntranceDuration; }
    bool HasEntrance() const { return Entrance!=nullptr; }
    bool HasCatEntrance() const { return bCatReady && bRigReady && SelectedIndex==2; }
    bool HasSwordEntrance() const { return bSwordReady && bRigReady && SelectedIndex==1; }
    FVector CatSupportPoint(int32 Side) const;
    UTextureRenderTarget2D* Portrait(int32 Index) const;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Preview;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Cat;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Sword;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Scabbard;
private:
    UPROPERTY() TArray<TObjectPtr<UPoseableMeshComponent>> PortraitBodies;
    UPROPERTY() TArray<TObjectPtr<USceneCaptureComponent2D>> PortraitCameras;
    UPROPERTY() TArray<TObjectPtr<UTextureRenderTarget2D>> PortraitTargets;
    UPROPERTY() TObjectPtr<UAnimSequence> Entrance;
    UPROPERTY() TObjectPtr<UAnimSequence> FinishPose;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CatDetails;
    TArray<int32> CatDetailBones;
    TArray<FTransform> CatDetailBindings;
    FBreachPose CatPose;
    FVector CatSupports[2]={FVector::ZeroVector,FVector::ZeroVector};
    bool bCatReady=false;
    void SetupCat();
    void UpdateCatEntrance();
    void SetupSword();
    void UpdateSwordEntrance();
    FBreachPose SwordPose;
    bool bSwordReady=false;
    FBreachPose Pose;
    FBreachCloth Cloth;
    float ClothTime=0;
    float SourceEndTime=0;
    float HeadTopOffset=0;
    bool bRigReady=false;
    void UpdatePreview();
    double PreviousTime=0;
    int32 WarmupFrames=0;
    bool bOpen=false;
};
