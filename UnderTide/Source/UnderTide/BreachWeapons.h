#pragma once
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"

namespace Breach
{
    inline constexpr int32 WeaponCount=4;
    inline constexpr const TCHAR* WeaponNames[]={TEXT("AK"),TEXT("M4"),TEXT("MP5"),TEXT("AA12")};
    inline constexpr const TCHAR* WeaponTypes[]={TEXT("突击步枪"),TEXT("突击步枪"),TEXT("冲锋枪"),TEXT("自动霰弹枪")};
    inline constexpr int32 AKMagazineSize=25;
    inline constexpr float AKDamage=38.f;
    inline constexpr float AKFireInterval=.105f;
    inline constexpr float AKHipSpread=.018f,AKAimSpread=.005f;
    inline constexpr float AKBloomPerShot=.003f,AKMaxBloom=.014f,AKBloomRecovery=.018f;
    inline constexpr int32 M4MagazineSize=30;
    inline constexpr float M4Damage=35.f,M4FireInterval=.09f;
    inline constexpr float M4HipSpread=.012f,M4AimSpread=.0035f;
    inline constexpr float M4BloomPerShot=.0024f,M4MaxBloom=.009f,M4BloomRecovery=.02f;
    inline constexpr int32 MP5MagazineSize=40;
    inline constexpr float MP5Damage=30.f,MP5MidDamage=23.f,MP5MinimumDamage=12.f,MP5FireInterval=.06f;
    inline constexpr float MP5HipSpread=.009f,MP5AimSpread=.0025f;
    inline constexpr float MP5BloomPerShot=.0018f,MP5MaxBloom=.006f,MP5BloomRecovery=.024f;
    inline constexpr float MP5CloseRange=1000.f,MP5FarRange=3000.f,MP5MinimumDamageRange=4000.f;
    inline constexpr int32 AA12MagazineSize=8,AA12PelletCount=8;
    inline constexpr float AA12PelletDamage=14.f,AA12MinimumPelletDamage=1.f,AA12FireInterval=.22f;
    inline constexpr float AA12HipSpread=.055f,AA12AimSpread=.035f;
    inline constexpr float AA12BloomPerShot=.006f,AA12MaxBloom=.018f,AA12BloomRecovery=.025f;
    inline constexpr float AA12CloseRange=1000.f,AA12MinimumDamageRange=1500.f;
    // Muzzle point converted from the supplied PMX, in the imported mesh's centimetres.
    inline const FVector AKMuzzle(63.3346f,0.f,4.9775f);
    inline const FVector AKMeshOffset(-27.f,0.f,0.f);
    // The supplied M4 OBJ has no sockets; these points match its imported centimetre bounds.
    inline const FVector M4Muzzle(61.8f,0.f,2.4f);
    inline const FVector M4MeshOffset(-27.f,0.f,-2.f);
    inline const FVector MP5Muzzle(31.5f,0.f,1.8f);
    inline const FVector MP5MeshOffset(-6.f,0.f,-2.f);
    inline const FVector AA12Muzzle(55.f,0.f,2.f);
    inline const FVector AA12MeshOffset(-21.f,0.f,-3.f);

    // Grip references from TEMP's front/side pairs. Points are in imported
    // mesh centimetres (before MeshOffset and the weapon root's 0.8 scale).
    // Hand points locate the wrist, leaving room for the palm around the grip.
    struct FGunHold
    {
        FVector MeshOffset,Stock,LeftWrist,RightWrist;
        FVector LeftFingers,LeftPalm;
        FVector Hip,Aim;
        float LeftCurl;
    };
    inline const FGunHold GunHolds[WeaponCount]={
        {AKMeshOffset,FVector(-11,0,3),FVector(30,-4,1),FVector(5,4,-6),
            FVector(.3f,1,-.15f),FVector(0,0,1),FVector(34,11,-14),FVector(34,0,-9.8f),.68f},
        {M4MeshOffset,FVector(-18,0,2),FVector(24,-4,-3),FVector(-1,4,-5),
            FVector(.9f,.25f,.1f),FVector(0,1,0),FVector(36,11,-14),FVector(34,0,-7.75f),.78f},
        {MP5MeshOffset,FVector(-26,0,3),FVector(12,-4,0),FVector(-13,4,-5),
            FVector(.25f,1,-.1f),FVector(0,0,1),FVector(33,10,-13),FVector(34,0,-8.8f),.68f},
        {AA12MeshOffset,FVector(-23,0,7),FVector(17,-4,-4),FVector(-4,4,-4),
            FVector(1,.15f,.1f),FVector(0,1,0),FVector(36,11,-15),FVector(34,0,-9.55f),.85f}
    };

    // Authored from both repeats in TEMP/{AK,M4,MP5,AA12}.mov. The TPS
    // camera cut is not copied: only the lift, inward cant and sight settling.
    struct FGunAim
    {
        float EnterTime,ExitTime;
        FVector LiftArc;
        FRotator Cant;
        float SightPitch;
    };
    inline const FGunAim GunAims[WeaponCount]={
        {.30f,.22f,FVector(-2,1,-1.8f),FRotator(-5,-4,-16),2.f},
        {.30f,.22f,FVector(-1.5f,1,-1.5f),FRotator(-4,-3,-19),2.5f},
        {.24f,.18f,FVector(-1,1,-1),FRotator(-3,-3,-13),4.f},
        {.26f,.20f,FVector(-2,1,-2),FRotator(-4,-2,-11),0.f}
    };

    inline UStaticMesh* WeaponMesh(int32 Index)
    {
        if(Index<0 || Index>=WeaponCount) return nullptr;
        const TCHAR* Key=WeaponNames[Index];
        return LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Weapons/%s/SM_%s.SM_%s"),Key,Key,Key));
    }
}
