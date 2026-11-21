using UnrealBuildTool;
using System.Collections.Generic;
using System.IO;
public class UnderTideEditorTarget : TargetRules
{
    public UnderTideEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("UnderTide");
        if (Target.ProjectFile != null)
        {
            string Plugin = Path.Combine(Target.ProjectFile.Directory.FullName, "Plugins", "MMD2Unreal");
            if (File.Exists(Path.Combine(Plugin, "MMD2Unreal.uplugin"))
                && !File.Exists(Path.Combine(Plugin, "Source", "MMD2UnrealCore", "MMD2UnrealCore.Build.cs")))
                DisablePlugins.Add("MMD2Unreal");
        }
    }
}
