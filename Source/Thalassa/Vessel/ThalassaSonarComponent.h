#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThalassaSonarComponent.generated.h"

class UTexture2D;

/** One surveyed patch of seabed. Only cells that have been measured exist. */
USTRUCT()
struct FSonarCell
{
	GENERATED_BODY()

	/** Row-major index into the survey grid. */
	UPROPERTY()
	int32 Index = 0;

	/** Height of the highest return in the cell, whole metres. */
	UPROPERTY()
	int16 HeightM = 0;
};

/**
 * Multibeam survey sonar. While active, the server sweeps a fan of beams below and beside the ship
 * through the real world geometry and records the seabed it actually hits, cell by cell. Nothing is
 * known until it has been measured. The survey replicates, so everyone aboard shares one chart.
 *
 * The survey and how it is drawn are separate: clients turn the replicated cells into a texture.
 */
UCLASS()
class THALASSA_API UThalassaSonarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThalassaSonarComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: switch the survey on or off. */
	void SetSurveying(bool bOn);
	bool IsSurveying() const { return bSurveying; }

	int32 GetSurveyedCellCount() const { return Cells.Num(); }

	/** The chart as a texture: one pixel per cell, unknown cells dark. Rebuilt when the survey changes. */
	UTexture2D* GetChartTexture();

	/** Where a world location falls on the survey grid, 0..1 along world X and world Y. */
	FVector2D WorldToChart(const FVector& WorldLocation) const;

	/** A number that changes whenever the survey does, for comparing processes in tests. */
	int64 GetSurveyChecksum() const;

	static constexpr int32 GridSize = 256;         // Cells per side.
	static constexpr float CellSizeCm = 2500.f;    // 25 m cells: 6.4 km of chart.

private:
	void SweepServer();
	void RecordHit(const FVector& WorldHit);
	void RebuildTexture();

	/** Server: the save slot to use, or empty for none. Real play uses one; tests only if asked. */
	FString GetSaveSlot() const;
	void LoadSurvey();
	void SaveSurvey();

	UFUNCTION()
	void OnRep_Cells();

	UPROPERTY(Replicated)
	bool bSurveying = false;

	UPROPERTY(ReplicatedUsing = OnRep_Cells)
	TArray<FSonarCell> Cells;

	/** Server: cell index to position in Cells, so a cell is updated, never duplicated. */
	TMap<int32, int32> CellLookup;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ChartTexture;

	bool bTextureDirty = true;
	bool bUnsavedChanges = false;
	float SaveTimer = 0.f;
	float SweepTimer = 0.f;
};
