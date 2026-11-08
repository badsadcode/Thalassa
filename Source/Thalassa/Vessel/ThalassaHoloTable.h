#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "ThalassaHoloTable.generated.h"

class UInstancedStaticMeshComponent;
class AThalassaVessel;

/**
 * The holotable: a hologram of the charted seabed around the ship, standing on a table where the crew
 * can gather. A point cloud, heading-up, so the bow always points the same way across the room. It is
 * built from the sonar survey, so it shows only what has been charted, and it fills in as the sonar maps
 * more. Each machine draws its own from the replicated survey: nothing about it is replicated.
 *
 * This component is the table's top centre. Move it in the Blueprint to move the table.
 */
UCLASS(ClassGroup = (Thalassa))
class THALASSA_API UThalassaHoloTableComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UThalassaHoloTableComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The points. Made by the vessel, attached to this component. */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Points;

	static constexpr int32 Samples = 96;          // Points along each side of the terrain.
	static constexpr float ReachM = 800.f;         // Shows the seabed this far from the ship, ahead, astern and abeam.
	static constexpr float HalfWidthCm = 100.f;    // The hologram is 2 m across.
	static constexpr float Exaggeration = 3.f;     // Heights are stretched, so gentle ground still has shape.
	static constexpr float ReliefCm = 40.f;        // The seabed's relief never stands taller than this.
	static constexpr float MaxHeightCm = 60.f;     // Nothing, the ship included, rises higher above the table than this.

private:
	void Rebuild();
	AThalassaVessel* GetVessel() const;

	TArray<FTransform> Transforms;
	TArray<float> Colours;
	float Timer = 0.f;

	static constexpr int32 ShipPoints = 7;
	static constexpr int32 DropPoints = 18;
};
