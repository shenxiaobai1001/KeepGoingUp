// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class KeepGoingUp : ModuleRules
{
	public KeepGoingUp(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"KeepGoingUp",
			"KeepGoingUp/Map",
			"KeepGoingUp/UI",
			"KeepGoingUp/Prank",
			"KeepGoingUp/Variant_Platforming",
			"KeepGoingUp/Variant_Platforming/Animation",
			"KeepGoingUp/Variant_Combat",
			"KeepGoingUp/Variant_Combat/AI",
			"KeepGoingUp/Variant_Combat/Animation",
			"KeepGoingUp/Variant_Combat/Gameplay",
			"KeepGoingUp/Variant_Combat/Interfaces",
			"KeepGoingUp/Variant_Combat/UI",
			"KeepGoingUp/Variant_SideScrolling",
			"KeepGoingUp/Variant_SideScrolling/AI",
			"KeepGoingUp/Variant_SideScrolling/Gameplay",
			"KeepGoingUp/Variant_SideScrolling/Interfaces",
			"KeepGoingUp/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
