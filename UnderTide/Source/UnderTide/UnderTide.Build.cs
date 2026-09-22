using UnrealBuildTool;
public class UnderTide : ModuleRules
{
    public UnderTide(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "AIModule", "NavigationSystem", "SlateCore" });
        PrivateDependencyModuleNames.Add("RenderCore");
        if (Target.bBuildEditor)
            PrivateDependencyModuleNames.AddRange(new string[] { "MeshDescription", "StaticMeshDescription", "SkeletalMeshDescription", "AnimationCore", "Json" });
    }
}
