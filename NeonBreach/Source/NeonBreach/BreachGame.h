#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "BreachPose.h"
#include "BreachGame.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UPoseableMeshComponent;
class USoundBase;
class USkeletalMesh;
class USkeleton;
class ABreachEnemy;

UCLASS()
class NEONBREACH_API ABreachCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ABreachCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual float TakeDamage(float Damage, const FDamageEvent& Event, AController* Instigator, AActor* Causer) override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> WeaponRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> MuzzleLight;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UPoseableMeshComponent> FirstPersonArms;
    UPROPERTY(EditAnywhere, Category="Weapon") float FireInterval = 0.105f;
    UPROPERTY(EditAnywhere, Category="Weapon") float ShotDamage = 34.f;
    UPROPERTY(EditAnywhere, Category="Weapon") int32 MagazineSize = 30;
    float Health = 100.f;
    int32 Ammo = 30;
    int32 Reserve = 180;
    int32 OperatorIndex = 0;
    int32 ShotsFired = 0;
    int32 ShotsHit = 0;
    bool bReloading = false;
    bool bAiming = false;
    bool bSprint = false;
    float ReloadProgress = 0.f;
    float HitMarker = 0.f;
    float DamageFlash = 0.f;
    bool bLastHeadshot = false;
    void Fire();
    void Reload();
    void FinishReload();
    void SelectOperator(int32 Index);
    void SetAim(bool bEnabled);
    void RestartRun();
    void TogglePause();
    void UpdateOperatorPose(float DeltaSeconds);
    bool HasFirstPersonRig() const { return bArmsRigReady; }
    float GripError() const;
private:
    FBreachPose BodyPose,ArmsPose;
    bool bArmsRigReady=false;
    float OperatorScale=1.f;
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void StartFire();
    void StopFire();
    void AimOn() { SetAim(true); }
    void AimOff() { SetAim(false); }
    void SprintOn() { bSprint = true; }
    void SprintOff() { bSprint = false; }
    void Select1() { SelectOperator(0); }
    void Select2() { SelectOperator(1); }
    void Select3() { SelectOperator(2); }
    void Select4() { SelectOperator(3); }
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
private:
    FBreachPose Pose;
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
    UPROPERTY() TArray<TObjectPtr<ABreachEnemy>> Displays;
};

UCLASS()
class NEONBREACH_API ABreachHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
private:
    void Text(const FString& Value, float X, float Y, float Size, FLinearColor Color);
    void Box(float X, float Y, float W, float H, FLinearColor Color);
};
