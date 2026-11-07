#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Vessel/ThalassaSonarComponent.h"
#include "ThalassaSurveySave.generated.h"

/**
 * The ship's sonar chart on disk. Versioned, and stamped with the grid it was made on, so a chart
 * from a different grid is never loaded into the wrong cells.
 */
UCLASS()
class THALASSA_API UThalassaSurveySave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 Version = CurrentVersion;

	UPROPERTY()
	int32 GridSize = 0;

	UPROPERTY()
	float CellSizeCm = 0.f;

	UPROPERTY()
	TArray<FSonarCell> Cells;
};
