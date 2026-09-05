using UnrealBuildTool;
public class NeonBreach : ModuleRules
{
    public NeonBreach(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "NavigationSystem" });
        if (Target.bBuildEditor)
            PrivateDependencyModuleNames.AddRange(new string[] { "MeshDescription", "StaticMeshDescription", "SkeletalMeshDescription", "AnimationCore" });
    }
}
