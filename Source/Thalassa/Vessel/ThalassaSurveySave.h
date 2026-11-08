#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Vessel/ThalassaSonarComponent.h"
#include "ThalassaSurveySave.generated.h"

/**
 * The ship's sonar chart on disk. Versioned, and stamped with the grid and the world it was made in, so
 * a chart is never loaded into the wrong cells or over a different seabed.
 */
UCLASS()
class THALASSA_API UThalassaSurveySave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 3; // 2: added World. 3: whole-world cell coordinates, and the ping's layer.

	UPROPERTY()
	int32 Version = CurrentVersion;

	/** Unused since version 3: the chart has no edges. */
	UPROPERTY()
	int32 GridSize = 0;

	UPROPERTY()
	float CellSizeCm = 0.f;

	/** The map and seabed generation the chart was surveyed in. See UThalassaSonarComponent::GetWorldStamp. */
	UPROPERTY()
	FString World;

	UPROPERTY()
	TArray<FSonarCell> Cells;
};
