using UnrealBuildTool;
using System.Collections.Generic;
public class UnderTideTarget : TargetRules
{
    public UnderTideTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("UnderTide");
    }
}
