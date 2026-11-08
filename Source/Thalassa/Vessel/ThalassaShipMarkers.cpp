#include "Vessel/ThalassaShipMarkers.h"

#include "Engine/CollisionProfile.h"

namespace
{
	/** Seen only in the editor, never collides, and moves with the ship. */
	void SetUpMarker(UShapeComponent* Marker, const FColor& Color)
	{
		Marker->SetMobility(EComponentMobility::Movable);
		Marker->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Marker->SetGenerateOverlapEvents(false);
		Marker->CanCharacterStepUpOn = ECB_No;
		Marker->SetCanEverAffectNavigation(false);
		Marker->SetHiddenInGame(true);
		Marker->ShapeColor = Color;
	}

	// The same size as a crew member.
	constexpr float CrewRadius = 34.f;
	constexpr float CrewHalfHeight = 88.f;
}

UThalassaDryCompartmentComponent::UThalassaDryCompartmentComponent()
{
	SetUpMarker(this, FColor(60, 160, 255));
}

UThalassaLadderZoneComponent::UThalassaLadderZoneComponent()
{
	SetUpMarker(this, FColor(255, 200, 40));
	InitBoxExtent(FVector(12.5f, 60.f, 250.f));
}

UThalassaSpawnPointComponent::UThalassaSpawnPointComponent()
{
	SetUpMarker(this, FColor(80, 230, 120));
	InitCapsuleSize(CrewRadius, CrewHalfHeight);
}

UThalassaStationSpotComponent::UThalassaStationSpotComponent()
{
	SetUpMarker(this, FColor(230, 90, 230));
	InitCapsuleSize(CrewRadius, CrewHalfHeight);
}

bool ThalassaShipMarkers::IsMarker(const UActorComponent* Component)
{
	return Component && (Component->IsA<UThalassaDryCompartmentComponent>() || Component->IsA<UThalassaLadderZoneComponent>()
		|| Component->IsA<UThalassaSpawnPointComponent>() || Component->IsA<UThalassaStationSpotComponent>());
}
