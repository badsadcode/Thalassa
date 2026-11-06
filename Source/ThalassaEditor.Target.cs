using UnrealBuildTool;

public class ThalassaEditorTarget : TargetRules
{
	public ThalassaEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Thalassa");
	}
}
