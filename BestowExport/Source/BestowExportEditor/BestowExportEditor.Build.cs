using UnrealBuildTool;

public class BestowExportEditor : ModuleRules
{
	public BestowExportEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"ToolMenus",
			"Projects",
			"InputCore",
			"DesktopPlatform",
			"ApplicationCore",
			"ImageWrapper",
			"Landscape",
			"Foliage",
			"WorkspaceMenuStructure",
			"GLTFExporter",
			"Json",
		});
	}
}
