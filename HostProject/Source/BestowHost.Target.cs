using UnrealBuildTool;

public class BestowHostTarget : TargetRules
{
	public BestowHostTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("BestowHost");
	}
}
