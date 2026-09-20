// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VHV : ModuleRules
{
	public VHV(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "SlateCore" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PublicIncludePaths.AddRange(new string[] {
			"VHV",
			"VHV/Variant_Platforming",
			"VHV/Variant_Platforming/Animation",
			"VHV/Variant_Combat",
			"VHV/Variant_Combat/AI",
			"VHV/Variant_Combat/Animation",
			"VHV/Variant_Combat/Gameplay",
			"VHV/Variant_Combat/Interfaces",
			"VHV/Variant_Combat/UI",
			"VHV/Variant_SideScrolling",
			"VHV/Variant_SideScrolling/AI",
			"VHV/Variant_SideScrolling/Gameplay",
			"VHV/Variant_SideScrolling/Interfaces",
			"VHV/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
