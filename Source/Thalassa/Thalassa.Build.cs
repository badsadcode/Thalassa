using UnrealBuildTool;

public class Thalassa : ModuleRules
{
	public Thalassa(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Headers are included relative to the module root, e.g. "Vessel/ThalassaVessel.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AudioMixer"
		});

		// The sonar display draws filled triangles with the engine's white texture.
		PrivateDependencyModuleNames.Add("RenderCore");

		// The open ocean's seabed is built at runtime, tile by tile.
		PrivateDependencyModuleNames.Add("ProceduralMeshComponent");
	}
}
