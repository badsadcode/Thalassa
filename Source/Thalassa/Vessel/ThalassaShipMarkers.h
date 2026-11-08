#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "ThalassaShipMarkers.generated.h"

/*
 * Gameplay spots on the ship, as components you can see and move in BP_Thalassa. They are never seen in
 * play and never collide: the ship reads where they are when it starts. See docs/SHIP_ART.md.
 */

/**
 * A space with air in it. Anywhere inside one of these, players walk and move with the ship. Anywhere
 * outside all of them is sea, and players swim. Every place anyone can stand must be inside one.
 */
UCLASS(ClassGroup = (Thalassa), meta = (BlueprintSpawnableComponent, DisplayName = "Thalassa Dry Compartment"))
class THALASSA_API UThalassaDryCompartmentComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UThalassaDryCompartmentComponent();
};

/** Where a character can take hold of a ladder: about 25 cm deep in front of it, its full width, from its foot to just above its top. */
UCLASS(ClassGroup = (Thalassa), meta = (BlueprintSpawnableComponent, DisplayName = "Thalassa Ladder Zone"))
class THALASSA_API UThalassaLadderZoneComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UThalassaLadderZoneComponent();
};

/** Where a player appears when they join. Players take spawn points in order of their names. */
UCLASS(ClassGroup = (Thalassa), meta = (BlueprintSpawnableComponent, DisplayName = "Thalassa Spawn Point"))
class THALASSA_API UThalassaSpawnPointComponent : public UCapsuleComponent
{
	GENERATED_BODY()

public:
	UThalassaSpawnPointComponent();
};

/** Where the person working a station stands. Each station has exactly one, made by the ship. */
UCLASS(ClassGroup = (Thalassa))
class THALASSA_API UThalassaStationSpotComponent : public UCapsuleComponent
{
	GENERATED_BODY()

public:
	UThalassaStationSpotComponent();
};

namespace ThalassaShipMarkers
{
	/** True for any of the marker components above. */
	bool IsMarker(const UActorComponent* Component);
}
