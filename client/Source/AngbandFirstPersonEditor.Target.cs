using UnrealBuildTool;
using System.Collections.Generic;

public class AngbandFirstPersonEditorTarget : TargetRules
{
    public AngbandFirstPersonEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("AngbandFirstPerson");
    }
}
