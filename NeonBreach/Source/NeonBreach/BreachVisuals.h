#pragma once
#include "CoreMinimal.h"
class USkeletalMesh;
class UMaterialInterface;
namespace Breach
{
    extern const TCHAR* Keys[4];
    extern const TCHAR* Names[4];
    USkeletalMesh* CharacterMesh(int32 Index);
    UMaterialInterface* Material(const TCHAR* Name);
    void Beam(UWorld* World, const FVector& From, const FVector& To, const FLinearColor& Color, float Width=2.f, float Life=0.1f);
}
