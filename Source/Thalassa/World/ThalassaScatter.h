#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThalassaScatter.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Many copies of one mesh on the seabed, drawn as instances: sea pens, tube worms, glowing growths.
 * Placed by the map script (Scripts/unreal/create_test_map.py) and saved in the level, so every player
 * sees the same seabed. Scenery only: no collision, so neither the ship nor the sonar meets it.
 */
UCLASS()
class THALASSA_API AThalassaScatter : public AActor
{
	GENERATED_BODY()

public:
	AThalassaScatter();

	/** Editor scripts: the mesh to scatter, and how far away it stops being drawn (0: never). */
	UFUNCTION(BlueprintCallable, Category = "Thalassa|Scatter")
	void SetScatterMesh(UStaticMesh* Mesh, float CullDistanceCm);

	/** Editor scripts: adds copies at these world transforms. */
	UFUNCTION(BlueprintCallable, Category = "Thalassa|Scatter")
	void AddWorldInstances(const TArray<FTransform>& Transforms);

	UPROPERTY(VisibleAnywhere, Category = "Thalassa|Scatter")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Instances;
};
