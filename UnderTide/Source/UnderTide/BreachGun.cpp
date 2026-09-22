#include "BreachGame.h"
#include "BreachWeapons.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "TimerManager.h"

void ABreachCharacter::SelectWeapon(int32 Index)
{
    if(UsesSword() || Index<0 || Index>=Breach::WeaponCount || Health<=0) return;
    if(SelectedWeapons[OperatorIndex]==Index) return;
    SelectedWeapons[OperatorIndex]=Index;
    ConfigureSelectedGun();
    UpdateOperatorPose(0);
}

void ABreachCharacter::ConfigureSelectedGun()
{
    if(UsesSword())
    {
        UpdateGunVisibility();
        return;
    }
    UStaticMesh* AKAsset=Breach::WeaponMesh(0);
    UStaticMesh* M4Asset=Breach::WeaponMesh(1);
    UStaticMesh* MP5Asset=Breach::WeaponMesh(2);
    UStaticMesh* AA12Asset=Breach::WeaponMesh(3);
    const int32 Selected=GetSelectedWeaponIndex();
    const int32 NextConfigured=(Selected==0 && AKAsset)?0:(Selected==1 && M4Asset)?1:
        (Selected==2 && MP5Asset)?2:(Selected==3 && AA12Asset)?3:INDEX_NONE;
    if(NextConfigured!=ConfiguredGunIndex)
    {
        if(ConfiguredGunIndex==0) AKAmmo=Ammo;
        else if(ConfiguredGunIndex==1) M4Ammo=Ammo;
        else if(ConfiguredGunIndex==2) MP5Ammo=Ammo;
        else if(ConfiguredGunIndex==3) AA12Ammo=Ammo;
        ConfiguredGunIndex=NextConfigured;
        Ammo=ConfiguredGunIndex==0?AKAmmo:ConfiguredGunIndex==1?M4Ammo:ConfiguredGunIndex==2?MP5Ammo:
            ConfiguredGunIndex==3?AA12Ammo:0;
        StopFire();SetAim(false);bReloading=false;ReloadProgress=0;
        GetWorldTimerManager().ClearTimer(ReloadTimer);
        Recoil=0;ShotBloom=0;MuzzleFlashTime=0;MuzzleLight->SetIntensity(0);
    }
    AK->SetStaticMesh(AKAsset);WorldAK->SetStaticMesh(AKAsset);
    M4->SetStaticMesh(M4Asset);WorldM4->SetStaticMesh(M4Asset);
    MP5->SetStaticMesh(MP5Asset);WorldMP5->SetStaticMesh(MP5Asset);
    AA12->SetStaticMesh(AA12Asset);WorldAA12->SetStaticMesh(AA12Asset);
    MagazineSize=UsesAK()?Breach::AKMagazineSize:UsesM4()?Breach::M4MagazineSize:
        UsesMP5()?Breach::MP5MagazineSize:UsesAA12()?Breach::AA12MagazineSize:30;
    ShotDamage=UsesAK()?Breach::AKDamage:UsesM4()?Breach::M4Damage:UsesMP5()?Breach::MP5Damage:
        UsesAA12()?Breach::AA12PelletDamage:34.f;
    FireInterval=UsesAK()?Breach::AKFireInterval:UsesM4()?Breach::M4FireInterval:
        UsesMP5()?Breach::MP5FireInterval:UsesAA12()?Breach::AA12FireInterval:.105f;
    Ammo=FMath::Clamp(Ammo,0,MagazineSize);
    MuzzleLight->SetRelativeLocation(UsesAK()?Breach::AKMeshOffset+Breach::AKMuzzle:
        UsesM4()?Breach::M4MeshOffset+Breach::M4Muzzle:
        UsesMP5()?Breach::MP5MeshOffset+Breach::MP5Muzzle:
        UsesAA12()?Breach::AA12MeshOffset+Breach::AA12Muzzle:FVector(44,0,1));
    UpdateGunVisibility();
}

void ABreachCharacter::UpdateGunVisibility()
{
    const bool Armed=!bUnarmed && !UsesSword();
    WeaponRoot->SetVisibility(Armed);WorldWeaponRoot->SetVisibility(Armed);
    for(USceneComponent* Root:{WeaponRoot.Get(),WorldWeaponRoot.Get()})
    {
        const bool World=Root==WorldWeaponRoot;
        TArray<USceneComponent*> Parts;Root->GetChildrenComponents(true,Parts);
        for(auto* Part:Parts) if(auto* GunPart=Cast<UStaticMeshComponent>(Part))
        {
            const bool IsAK=GunPart==AK || GunPart==WorldAK;
            const bool IsM4=GunPart==M4 || GunPart==WorldM4;
            const bool IsMP5=GunPart==MP5 || GunPart==WorldMP5;
            const bool IsAA12=GunPart==AA12 || GunPart==WorldAA12;
            const bool Visible=Armed && (IsAK?UsesAK():IsM4?UsesM4():IsMP5?UsesMP5():IsAA12?UsesAA12():
                !UsesAK() && !UsesM4() && !UsesMP5() && !UsesAA12());
            GunPart->SetVisibility(Visible);
            GunPart->SetCastShadow(World && Visible);
            GunPart->SetCastHiddenShadow(World && Visible);
        }
    }
}

float ABreachCharacter::GetShotSpread() const
{
    return UsesAK()?(bAiming?Breach::AKAimSpread:Breach::AKHipSpread)+ShotBloom*(bAiming?.45f:1.f):
        UsesM4()?(bAiming?Breach::M4AimSpread:Breach::M4HipSpread)+ShotBloom*(bAiming?.4f:1.f):
        UsesMP5()?(bAiming?Breach::MP5AimSpread:Breach::MP5HipSpread)+ShotBloom*(bAiming?.35f:1.f):
        UsesAA12()?(bAiming?Breach::AA12AimSpread:Breach::AA12HipSpread)+ShotBloom*(bAiming?.5f:1.f):
        (bAiming?.001f:.004f);
}

float ABreachCharacter::GetShotDamage(float Distance) const
{
    if(UsesAA12())
    {
        if(Distance<=Breach::AA12CloseRange) return Breach::AA12PelletDamage;
        if(Distance>=Breach::AA12MinimumDamageRange) return Breach::AA12MinimumPelletDamage;
        return FMath::Lerp(Breach::AA12PelletDamage,Breach::AA12MinimumPelletDamage,
            (Distance-Breach::AA12CloseRange)/(Breach::AA12MinimumDamageRange-Breach::AA12CloseRange));
    }
    if(!UsesMP5()) return ShotDamage;
    if(Distance<=Breach::MP5CloseRange) return Breach::MP5Damage;
    if(Distance<=Breach::MP5FarRange) return Breach::MP5MidDamage;
    if(Distance<Breach::MP5MinimumDamageRange)
        return FMath::Lerp(Breach::MP5MidDamage,Breach::MP5MinimumDamage,
            (Distance-Breach::MP5FarRange)/(Breach::MP5MinimumDamageRange-Breach::MP5FarRange));
    return Breach::MP5MinimumDamage;
}

FVector ABreachCharacter::GunMuzzleLocation() const
{
    return UsesAK()?AK->GetComponentTransform().TransformPosition(Breach::AKMuzzle):
        UsesM4()?M4->GetComponentTransform().TransformPosition(Breach::M4Muzzle):
        UsesMP5()?MP5->GetComponentTransform().TransformPosition(Breach::MP5Muzzle):
        UsesAA12()?AA12->GetComponentTransform().TransformPosition(Breach::AA12Muzzle):
        WeaponRoot->GetComponentTransform().TransformPosition(FVector(42,0,1));
}

int32 ABreachCharacter::GetTotalGunAmmo() const
{
    return Reserve+(ConfiguredGunIndex==0?Ammo:AKAmmo)+(ConfiguredGunIndex==1?Ammo:M4Ammo)+
        (ConfiguredGunIndex==2?Ammo:MP5Ammo)+
        (ConfiguredGunIndex==3?Ammo:AA12Ammo);
}
