#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "BreachPose.h"
#include "BreachCloth.h"
#include "BreachGame.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UPoseableMeshComponent;
class USoundBase;
class USkeletalMesh;
class USkeleton;
class UAnimSequence;
class ABreachEnemy;
class ABreachSelectionStage;

UCLASS()
class NEONBREACH_API ABreachPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    bool SetSelectionPauseTick(bool Enabled)
    {
        const bool Previous=bShouldPerformFullTickWhenPaused;
        bShouldPerformFullTickWhenPaused=Enabled;
        return Previous;
    }
};

UCLASS()
class NEONBREACH_API ABreachCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ABreachCharacter(const FObjectInitializer& ObjectInitializer=FObjectInitializer::Get());
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual bool CanJumpInternal_Implementation() const override;
    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* Instigator, AActor* Causer) override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> WeaponRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> WorldWeaponRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Sword;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> WorldSword;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Scabbard;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> WorldScabbard;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> MuzzleLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> WorldBody;
    UPROPERTY(EditAnywhere, Category="Camera") float BaseFieldOfView = 110.f;
    UPROPERTY(EditAnywhere, Category="Camera") float AimFieldOfView = 76.f;
    UPROPERTY(EditAnywhere, Category="Weapon") float FireInterval = 0.105f;
    UPROPERTY(EditAnywhere, Category="Weapon") float ShotDamage = 34.f;
    UPROPERTY(EditAnywhere, Category="Weapon") int32 MagazineSize = 30;
    UPROPERTY(EditAnywhere, Category="Sword") float SwordDamage = 180.f;
    UPROPERTY(EditAnywhere, Category="Sword") float SwordAttackInterval = .72f;
    UPROPERTY(EditAnywhere, Category="Sword") float SwordRange = 260.f;
    UPROPERTY(EditAnywhere, Category="Sword") float SwordRadius = 70.f;
    UPROPERTY(EditAnywhere, Category="Sword") float SwordVisualScale = .72f;
    UPROPERTY(EditAnywhere, Category="Sword", meta=(ClampMin="0.0", ClampMax="1.0")) float FirstPersonSwordMotionScale = .35f;
    UPROPERTY(EditAnywhere, Category="Sword", meta=(ClampMin="0.1")) float FirstPersonSwordAnchorSpeed = 4.f;
    float Health = 100.f;
    int32 Ammo = 30;
    int32 Reserve = 180;
    int32 OperatorIndex = 0;
    int32 ShotsFired = 0;
    int32 ShotsHit = 0;
    bool bReloading = false;
    bool bAiming = false;
    bool bSprint = false;
    bool bUnarmed = false;
    EBreachLocomotion LocomotionState=EBreachLocomotion::Idle;
    void HolsterRifle();
    void DrawRifle();
    void SetUnarmed(bool Enabled);
    void CrouchOn();
    void CrouchOff();
    bool IsSliding() const;
    bool HasLocomotionAnimations() const;
    float ReloadProgress = 0.f;
    float HitMarker = 0.f;
    float DamageFlash = 0.f;
    bool bLastHeadshot = false;
    void Fire();
    void Reload();
    void FinishReload();
    void SelectOperator(int32 Index);
    bool UsesSword() const { return OperatorIndex==1; }
    bool HasSwordRig() const { return bSwordRigReady; }
    bool IsSwordAttacking() const { return SwordAttackTime>=0.f; }
    void SetAim(bool bEnabled);
    void RestartRun();
    void TogglePause();
    void ToggleSelection();
    void UpdateOperatorPose(float DeltaSeconds);
    bool HasFirstPersonRig() const { return bBodyRigReady; }
    float GripError() const;
private:
    FBreachPose BodyPose;
    FBreachPose SwordPose;
    FBreachCloth BodyCloth;
    UPROPERTY() TArray<TObjectPtr<UAnimSequence>> LocomotionAnimations;
    UPROPERTY() TObjectPtr<UAnimSequence> SwordAttackAnimation;
    void LoadLocomotionAnimations();
    void UpdateLocomotion(float DeltaSeconds);
    TArray<FTransform> LocomotionBlendFrom;
    float LocomotionTime=0.f,LocomotionBlendTime=1.f;
    float AirTime=0.f,LandTime=10.f;
    float CrouchEyeDrop=0.f;
    FQuat SlideFloorTilt=FQuat::Identity;
    float SlideFloorOffset=0.f;
    bool bWasFalling=false,bJumpTakingOff=false;
    bool bBodyRigReady=false;
    bool bSwordRigReady=false,bSwordDamageApplied=false;
    bool bLoadoutBeforeSword=false;
    float SwordAttackTime=-1.f;
    FVector SwordAttackOrigin=FVector::ZeroVector;
    FVector SwordAttackDirection=FVector::ForwardVector;
    FTransform FirstPersonSwordAnchor=FTransform::Identity;
    FTransform FirstPersonSwordGrip=FTransform::Identity;
    bool bFirstPersonSwordGripReady=false;
    void ConfigureSwordLoadout();
    void UpdateSwordAttack(float DeltaSeconds);
    void PerformSwordHit();
    void ApplySwordAttackPose();
    void UpdateSwordVisual(const FBreachPose& Pose,UPoseableMeshComponent* CharacterMesh,UPoseableMeshComponent* SwordMesh,float DeltaSeconds);
    void UpdateScabbardVisual(const FBreachPose& Pose,UPoseableMeshComponent* CharacterMesh,UPoseableMeshComponent* ScabbardMesh);
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void StartFire();
    void StopFire();
    void AimOn() { SetAim(true); }
    void AimOff() { SetAim(false); }
    bool bTrigger = false;
    float NextShot = 0.f;
    float ReloadStarted = 0.f;
    float Recoil = 0.f;
    float Bob = 0.f;
    FTimerHandle ReloadTimer;
    UPROPERTY() TObjectPtr<USoundBase> FireSound;
    UPROPERTY() TObjectPtr<USoundBase> ReloadSound;
};

UCLASS()
class NEONBREACH_API ABreachEnemy : public ACharacter
{
    GENERATED_BODY()
public:
    ABreachEnemy();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* Instigator, AActor* Causer) override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Visual;
    UPROPERTY(EditAnywhere) int32 ModelIndex = 0;
    UPROPERTY(EditAnywhere) bool bDisplayOnly = false;
    float Health = 100.f;
    float MaxHealth = 100.f;
    bool bDefeated = false;
    float AttackCooldown = 1.f;
    float PathCooldown = 0.f;
    float Phase = 0.f;
    float DespawnTime = 0.f;
    void Configure(int32 Index, int32 Wave);
    void UpdatePose(float DeltaSeconds);
    void UpdateDeathPose(float DeltaSeconds);
    bool HasDeathAnimation() const { return DeathAnimation!=nullptr; }
private:
    UPROPERTY() TObjectPtr<UAnimSequence> DeathAnimation;
    TArray<int32> DeathBoneIndices;
    FBreachPose Pose;
    FBreachCloth Cloth;
    TArray<FTransform> DeathStartPose;
    float DeathFloorZ=0.f;
    float DeathDirection=1.f;
    FVector DeathScale=FVector::OneVector;
};

UCLASS()
class NEONBREACH_API ABreachGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ABreachGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    int32 Wave = 0;
    int32 Score = 0;
    int32 Kills = 0;
    int32 EnemiesAlive = 0;
    int32 RemainingToSpawn = 0;
    bool bGameOver = false;
    float Intermission = 8.f;
    float SpawnDelay = 0.f;
    FString Notice = TEXT("TRAINING LINK ESTABLISHED");
    float NoticeTime = 4.f;
    void EnemyDefeated(ABreachEnemy* Enemy, bool bHeadshot);
    void EndRun();
    void StartWave();
    void SpawnEnemy();
    void RunSmokeTest();
    void RunMovementTest();
    void RunModelReview();
    void TickSelectionTest();
    TFunction<void()> SelectionTestStep;
    UFUNCTION(Exec) void BreachSmokeTest();
    UFUNCTION(Exec) void BreachGallery();
    UFUNCTION(Exec) void BreachAutoPlay();
    bool bGallery = false;
    bool bAutoPlay = false;
    int32 AutoShots = 0;
    float AutoTime = 0.f;
    UFUNCTION(BlueprintCallable, CallInEditor) void BuildArena();
    UFUNCTION(BlueprintCallable) static void BakeArena(UObject* WorldContext);
    UFUNCTION(BlueprintCallable) static void BindSkeleton(USkeletalMesh* CharacterAsset, USkeleton* SkeletonAsset);
    UFUNCTION(BlueprintCallable) static USkeleton* EnsureSkeleton(USkeletalMesh* CharacterAsset);
    UFUNCTION(BlueprintCallable) static int32 PrepareFirstPersonArms(USkeletalMesh* CharacterAsset,int32 ModelIndex);
    UFUNCTION(BlueprintCallable) static bool CopyCharacterGeometry(USkeletalMesh* Target,USkeletalMesh* Source);
    UFUNCTION(BlueprintCallable) static UAnimSequence* BakeDeathAnimation(USkeletalMesh* CharacterAsset,int32 ModelIndex,const FString& MotionFile,const FString& PackageName);
    UFUNCTION(BlueprintCallable) static UAnimSequence* BakeCharacterAnimation(USkeletalMesh* CharacterAsset,int32 ModelIndex,const FString& MotionFile,const FString& PackageName);
    UFUNCTION(BlueprintCallable) static UAnimSequence* BakeLocalAnimation(USkeletalMesh* CharacterAsset,const FString& MotionFile,const FString& PackageName);
    UFUNCTION(BlueprintCallable) static bool ImportVMDExpressions(USkeletalMesh* Asset,const FString& SourceFile);
    void RunDeformationChecks(TFunctionRef<void(bool,const FString&)> Check);
    UPROPERTY() TArray<TObjectPtr<ABreachEnemy>> Displays;
};

UCLASS()
class NEONBREACH_API ABreachHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void Tick(float DeltaSeconds) override;
    virtual void DrawHUD() override;
    virtual void NotifyHitBoxClick(FName BoxName) override;
    virtual void NotifyHitBoxBeginCursorOver(FName BoxName) override;
    virtual void NotifyHitBoxEndCursorOver(FName BoxName) override;
    void ToggleSelection();
    void ChooseOperator(int32 Index);
    bool IsSelectionOpen() const { return bSelectionOpen; }
    ABreachSelectionStage* GetSelectionStage() const { return SelectionStage; }
    FVector2D OperatorCardCenter(int32 Index) const;
private:
    UPROPERTY() TObjectPtr<ABreachSelectionStage> SelectionStage;
    TWeakObjectPtr<AActor> PreviousViewTarget;
    bool bSelectionOpen=false,bWasPaused=false,bWasAutoCamera=true,bWasPauseTick=false,bWasClickEvents=false;
    bool bStartupPending=true,bInitialSelection=false;
    int32 HoveredOperator=INDEX_NONE;
    void DrawSelection();
    void EnsureSelectionStage();
    void DrawPlayerVitals(const ABreachCharacter* Player);
    void MenuText(const FString& Value,float X,float Y,float Size,FLinearColor Color,bool Chinese=false);
    void Text(const FString& Value, float X, float Y, float Size, FLinearColor Color);
    void Box(float X, float Y, float W, float H, FLinearColor Color);
};
