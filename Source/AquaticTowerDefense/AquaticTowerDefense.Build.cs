// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AquaticTowerDefense : ModuleRules
{
	public AquaticTowerDefense(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore", "RHI", "RenderCore", "MeshDescription", "StaticMeshDescription" });

		PublicIncludePaths.AddRange(new string[] {
			"AquaticTowerDefense",
			"AquaticTowerDefense/TowerDefense"
		});
	}
}
