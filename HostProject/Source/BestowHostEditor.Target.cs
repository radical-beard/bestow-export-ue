using UnrealBuildTool;

public class BestowHostEditorTarget : TargetRules
{
	public BestowHostEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BestowHost");
	}
}
