#pragma once

#include "CoreMinimal.h"

/** One graybox hull piece, in ship coordinates: +X forward, +Y starboard, +Z up, centimetres. */
struct FHullBox
{
	FName Name;
	FVector Center;
	FVector Size;
	FLinearColor Color;
	FRotator Rotation = FRotator::ZeroRotator;
	bool bVisible = true;
	bool bCollision = true;
};

struct FInteriorLight
{
	FName Name;
	FVector Location;
	FColor Color;
};

/**
 * Everything that makes one ship's graybox: hull pieces, compartments and test routes. Layouts are
 * data, so changing the ship's plan never touches the code that moves it or replicates it.
 */
struct FVesselLayout
{
	TArray<FHullBox> Boxes;
	TArray<FInteriorLight> Lights;

	/** Everything outside these, and outside the airlock chamber, is sea. */
	TArray<FBox> DryCompartments;

	/** Where a character can hold on and climb. */
	TArray<FBox> LadderZones;

	FBox HullBounds = FBox(ForceInit);
	FVector ConsoleLocation = FVector::ZeroVector;
	FVector ConsoleTextLocation = FVector::ZeroVector;

	/** The sonar chart's screen: its centre, the way it faces, and its width. A width of zero means no screen. */
	FVector SonarScreenLocation = FVector::ZeroVector;
	FVector SonarScreenFacing = FVector::BackwardVector;
	float SonarScreenSizeCm = 0.f;

	/** Added to the airlock's own coordinates, which assume a corridor along Y = 0 and the sea to starboard. */
	FVector AirlockOffset = FVector::ZeroVector;

	/** Spawn points, cycled through in order. */
	TArray<FVector> Spawns;
	FVector AirlockViewpoint = FVector::ZeroVector;

	/** A route around the ship for the scripted walker. Empty means use the test rig's simple route. */
	TArray<FVector> TourRoute;
};

/** The ship's first real deck: see docs/MOVING_SHIP_SPIKE.md. A sketch that is expected to change. */
FVesselLayout BuildMainDeckLayout();
