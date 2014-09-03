// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class BrettPlugin : ModuleRules
	{
		public BrettPlugin(TargetInfo Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					"Developer/BrettPlugin/Public",
					// ... add public include paths required here ...
				}
				);

			PrivateIncludePaths.AddRange(
				new string[] {
					"Developer/BrettPlugin/Private",
					// ... add other private include paths required here ...
				}
				);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"UnrealEd",
					"Core",
					"CoreUObject",
					"Slate",
					"UnrealEd",
					"LevelEditor",
					"InputCore",
					"EditorStyle",
					"RawMesh",
					"MeshUtilities",
					"AssetRegistry",
					"LandscapeEditor",
					"TargetPlatform",
					"BlueprintGraph",
					"LandscapeEditor",
					// ... add private dependencies that you statically link with here ...
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);
		}
	}
}