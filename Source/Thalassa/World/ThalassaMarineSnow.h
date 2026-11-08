#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThalassaMarineSnow.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Marine snow: specks drifting in the water around the local camera, so the sea has depth and movement.
 * Purely local and visual: each player's own, never replicated, never collides.
 *
 * The specks fill a box around the camera and wrap around it, so there is always snow wherever you look.
 * They are lit like everything else, so they show up mostly where a lamp shines through them. Specks
 * inside the ship's dry compartments are hidden: there is no snow in the corridors.
 */
UCLASS()
class THALASSA_API AThalassaMarineSnow : public AActor
{
	GENERATED_BODY()

public:
	AThalassaMarineSnow();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Specks;

	/** Each speck's place in the wrapping box, and how it drifts. */
	TArray<FVector> Seeds;
	TArray<FVector> Drifts;
	TArray<FTransform> Transforms;
	double Clock = 0.0;

	static constexpr int32 Count = 1200;
	static constexpr float BoxHalfCm = 1200.f;
	static constexpr float SpeckCm = 1.4f;
};
