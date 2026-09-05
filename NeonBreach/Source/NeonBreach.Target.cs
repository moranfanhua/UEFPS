using UnrealBuildTool;
using System.Collections.Generic;
public class NeonBreachTarget : TargetRules
{
    public NeonBreachTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("NeonBreach");
    }
}
