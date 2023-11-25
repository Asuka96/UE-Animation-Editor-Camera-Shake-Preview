// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CameraShakePreview : ModuleRules
{
	public CameraShakePreview(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "GameplayCameras", "UnrealEd" });
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "HeadMountedDisplay" });
	}
}
