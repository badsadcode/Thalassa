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

	/**
	 * The chart around a point, as a texture, north up: ViewCells across plus a margin, smoothed between
	 * cells, hill-shaded, with depth contours and a glow on freshly surveyed ground. Built locally from the
	 * replicated survey, at most ten times a second. OutUV0 and OutUVSize crop it to ViewCells across,
	 * centred on the point, so the picture scrolls smoothly between rebuilds.
	 */
	UTexture2D* GetLocalChart(const FVector& WorldCentre, int32 ViewCells, FVector2D& OutUV0, float& OutUVSize);

	/** The surveyed seabed height at a world location, smoothed between cell centres, in metres. False if unmeasured. */
	bool SampleHeightM(const FVector& WorldLocation, float& OutHeightM);

	/** The surveyed seabed height under a world location, in metres, if that cell has been measured. */
	bool GetSurveyedHeightM(const FVector& WorldLocation, float& OutHeightM);

	/** The chart's colour for a seabed height, in metres, within a range: deep navy through teal to pale aqua. */
	static FLinearColor DepthColour(float HeightM, float DeepM, float ShallowM);

	/** The heights the local chart's colours span: whatever is in view, at least 60 m. */
	float GetChartDeepM() const { return LocalDeepM; }
	float GetChartShallowM() const { return LocalShallowM; }

	/** Where a world location falls on the survey grid, 0..1 along world X and world Y. */
	FVector2D WorldToChart(const FVector& WorldLocation) const;

	/** A number that changes whenever the survey does, for comparing processes in tests. */
	int64 GetSurveyChecksum() const;

	static constexpr int32 GridSize = 256;         // Cells per side.
	static constexpr float CellSizeCm = 2500.f;    // 25 m cells: 6.4 km of chart.

private:
	void SweepServer();
	void RecordHit(const FVector& WorldHit);
	/** Brings the dense copy of the survey up to date, noting when each cell last changed. */
	void SyncDense();
	void BuildLocalChart();

	/**
	 * Which world a chart belongs to: the map's name and the seabed's generation. A saved chart from any other
	 * world is not loaded, or the old seabed would be drawn over the new one.
	 */
	FString GetWorldStamp() const;

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

	// The survey as a full grid, for drawing: heights, which cells are known, and when each last changed here.
	TArray<int16> DenseHeight;
	TArray<bool> DenseKnown;
	TArray<float> DenseChanged;
	bool bDenseSeeded = false;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LocalChart;
	FIntPoint LocalOrigin = FIntPoint(0, 0);   // The cell at the image's south-west corner.
	int32 LocalCells = 0;                      // Cells across the image.
	double LocalBuiltAt = -1.0;
	float LocalDeepM = -700.f;
	float LocalShallowM = -100.f;
	static constexpr int32 PixelsPerCell = 6;

	bool bTextureDirty = true;
	bool bUnsavedChanges = false;
	float SaveTimer = 0.f;
	float SweepTimer = 0.f;
};
