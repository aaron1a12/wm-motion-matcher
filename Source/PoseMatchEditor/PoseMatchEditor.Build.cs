// Some copyright should be here...

using UnrealBuildTool;

public class PoseMatchEditor : ModuleRules
{
	public PoseMatchEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
				"PoseMatchEditor/Public",
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
				"PoseMatchEditor/Private"
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", "PoseMatch"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"AnimGraph",
				"BlueprintGraph",
				"UnrealEd",
				"ToolMenus",
				"AnimationModifiers",
				// ... add private dependencies that you statically link with here ...	
			}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"UnrealEd",
				"ToolMenus",
				"AnimationModifiers",
			});


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
				"AssetTools"
			}
			);
	}
}
