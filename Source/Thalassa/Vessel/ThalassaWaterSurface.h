// Water standing inside the ship: the airlock filling, a compartment flooding.
#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "ThalassaWaterSurface.generated.h"

class UMaterialInstanceDynamic;

/**
 * The top of a body of water inside the ship. A fine grid, so the water material can move it (waves, and the
 * slosh this component works out from how the ship moves), and shade it (refraction, colour by depth, foam where
 * it meets the walls). Drawing only: where the water is, for swimming, is AThalassaVessel::IsWaterAt.
 *
 * The material is /Game/Ship/Water/M_InteriorWater (Scripts/unreal/create_water_materials.py).
 */
UCLASS()
class THALASSA_API UThalassaWaterSurfaceComponent : public UProceduralMeshComponent
{
	GENERATED_BODY()

public:
	UThalassaWaterSurfaceComponent(const FObjectInitializer& ObjectInitializer);

	/** Makes the grid, SizeCm across and centred on the component. Again only if the size changes. */
	void Build(const FVector2D& SizeCm);

	/**
	 * Once a frame while shown. Churn: how rough it is, 0 still to 1 water pouring in.
	 * DepthCm: how deep the water is, which sets how fast it sloshes, and how high waves can get.
	 */
	void UpdateWater(float DeltaSeconds, float Churn, float DepthCm);

private:
	FVector2D BuiltSize = FVector2D::ZeroVector;
	float GridCm = 12.f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Look;

	// The slosh: the surface's tilt (rise per cm along x and y), and how fast it is changing.
	FVector2D Tilt = FVector2D::ZeroVector;
	FVector2D TiltRate = FVector2D::ZeroVector;
	// How the ship moves, from where this surface was the frames before.
	FVector LastLocation = FVector::ZeroVector;
	FVector LastVelocity = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;
	int32 FramesSeen = 0;
};
