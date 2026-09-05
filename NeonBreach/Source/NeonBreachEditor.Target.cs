using UnrealBuildTool;
using System.Collections.Generic;
public class NeonBreachEditorTarget : TargetRules
{
    public NeonBreachEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("NeonBreach");
    }
}
