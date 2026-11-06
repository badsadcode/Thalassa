using UnrealBuildTool;

public class ThalassaTarget : TargetRules
{
	public ThalassaTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("Thalassa");
	}
}
