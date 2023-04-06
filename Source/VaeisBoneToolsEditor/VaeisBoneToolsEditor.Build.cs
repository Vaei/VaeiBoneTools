// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VaeisBoneToolsEditor : ModuleRules
{
	public VaeisBoneToolsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		bEnforceIWYU = true;
		// bUseUnity = false;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
			}
			);
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
                "UnrealEd",
                "UMG",
                "Blutility",
                "RenderCore",
                "Projects",
                "AssetTools",
			}
			);
	}
}
