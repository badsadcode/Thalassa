#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "ThalassaSonarComponent.generated.h"

class UTexture2D;
class UMaterialParameterCollection;
class UThalassaSonarComponent;

/**
 * One measured patch of seabed. Only cells that have been measured exist.
 *
 * The chart has two layers, fixed to the world: the survey's fine 25 m cells, from the multibeam strip under the
 * ship, and the ping's coarse 100 m cells, from the active pulse all around it. Where both know a place, the
 * survey wins. Cells are addressed by their whole-world cell coordinates, so the chart covers the whole ocean.
 */
USTRUCT()
struct FSonarCell : public FFastArraySerializerItem
{
	GENERATED_BODY()

	/** The cell's world coordinates packed into one number (see ThalassaSonarCells::Pack), at its layer's size. */
	UPROPERTY()
	int32 Index = 0;

	/** Height of the seabed in the cell, whole metres: the highest return for the survey, the centre for a ping. */
	UPROPERTY()
	int16 HeightM = 0;

	/** 0: survey (25 m), 1: ping (100 m). */
	UPROPERTY()
	uint8 Layer = 0;

	void PostReplicatedAdd(const struct FSonarCellArray& Array);
	void PostReplicatedChange(const struct FSonarCellArray& Array);
};

/** The whole chart, replicated as changes only, so it can grow as large as the ocean the crew explores. */
USTRUCT()
struct FSonarCellArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FSonarCell> Items;

	UPROPERTY(NotReplicated)
	TObjectPtr<UThalassaSonarComponent> Owner;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FSonarCell, FSonarCellArray>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FSonarCellArray> : public TStructOpsTypeTraitsBase2<FSonarCellArray>
{
	enum { WithNetDeltaSerializer = true };
};

/** An active ping: where it started, when (on the server's clock) and how far it reaches. */
USTRUCT()
struct FSonarPulse
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	UPROPERTY()
	double StartTime = -1000.0;

	UPROPERTY()
	float RangeM = 0.f;

	/** Counts up with every ping, so a repeat from the same place still registers. */
	UPROPERTY()
	int32 Serial = 0;
};

/** What a sonar of a given grade can do. Grades come with upgrades; test builds can switch between them. */
struct FSonarGrade
{
	const TCHAR* Name;
	float PingRangeM;     // 0: no active ping.
	float PingCooldownS;
};

/**
 * The ship's sonar. Two ways of seeing the seabed, both charted into one shared map:
 *  - the multibeam survey: quiet, while switched on, a detailed strip below and beside the ship;
 *  - the active ping (if the ship's grade has one): one loud pulse in every direction. The server charts the
 *    ground the pulse reaches, in coarse cells, as the wave spreads, skipping ground hidden behind terrain. Every
 *    machine draws the ring sweeping over the seabed (MPC_SonarPulse) and plays the sound.
 *
 * The chart replicates, so everyone aboard shares it. Clients turn it into pictures themselves: the chart texture,
 * the holotable. See docs/SONAR.md.
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

	/** Server: send an active ping, if this grade has one and it has recharged. True if it went out. */
	bool TryPing();

	/** Server, test builds only: step to the next grade, wrapping round. */
	void CycleGrade();

	int32 GetGrade() const { return Grade; }
	static const FSonarGrade& GetGradeInfo(int32 InGrade);

	/** Seconds until the ping can be sent again (0: ready), by the server's clock. */
	float GetPingCooldownLeft() const;

	/** The ping in flight, if any: its origin and how far the wave has spread, in metres. False if none. */
	bool GetPulseFront(FVector& OutOrigin, float& OutRadiusM, float& OutRangeM) const;

	int32 GetSurveyedCellCount() const { return Chart.Items.Num(); }

	/**
	 * The chart around a point, as a texture, north up: ViewCells across plus a margin, smoothed between
	 * cells, hill-shaded, with depth contours and a glow on freshly measured ground. Built locally from the
	 * replicated chart, at most ten times a second. OutUV0 and OutUVSize crop it to ViewCells across,
	 * centred on the point, so the picture scrolls smoothly between rebuilds.
	 */
	UTexture2D* GetLocalChart(const FVector& WorldCentre, int32 ViewCells, FVector2D& OutUV0, float& OutUVSize);

	/** The charted seabed height at a world location, smoothed between cell centres, in metres. False if unmeasured. */
	bool SampleHeightM(const FVector& WorldLocation, float& OutHeightM);

	/** The charted seabed height under a world location, in metres, if that cell has been measured. */
	bool GetSurveyedHeightM(const FVector& WorldLocation, float& OutHeightM);

	/** The chart's colour for a seabed height, in metres, within a range: deep navy through teal to pale aqua. */
	static FLinearColor DepthColour(float HeightM, float DeepM, float ShallowM);

	/** The heights the local chart's colours span: whatever is in view, at least 60 m. */
	float GetChartDeepM() const { return LocalDeepM; }
	float GetChartShallowM() const { return LocalShallowM; }

	/** A number that changes whenever the chart does, for comparing processes in tests. */
	int64 GetSurveyChecksum() const;

	/** Called by the replicated chart when cells arrive or change. */
	void NoteCellArrived(const FSonarCell& Cell);

	/** The size of the area the chart screen shows is set from this many survey cells. */
	static constexpr int32 GridSize = 256;
	static constexpr float CellSizeCm = 2500.f;     // Survey cells: 25 m.
	static constexpr int32 PingCellCells = 4;       // A ping cell is 4 × 4 survey cells: 100 m.
	static constexpr float PulseSpeedMS = 500.f;    // How fast the ping's wave spreads. Slower than sound in water (1,500 m/s), so it can be watched.

private:
	void SweepServer();
	void TickPulseServer();
	void RecordCell(int32 Layer, const FIntPoint& Cell, int16 HeightM, bool bHighest);
	void UpdatePulseVisuals();

	/** Brings the local copy of the chart up to date, noting when each cell last changed here. */
	void SyncDense();
	/** The chart picture is built over several frames, so no one frame carries it: heights, then rows in slices, then the upload. */
	void StartChartBuild();
	void BuildChartRows(int32 Slice);
	void FinishChartBuild();

	/** A layer's height at a cell, from the local copy. False if unmeasured, or not yet reached by the ping's wave. */
	bool DenseAt(int32 Layer, int32 X, int32 Y, int16& OutHeight, float* OutChanged = nullptr) const;
	/** Smoothed height at a point in survey-cell coordinates, from the survey or else the ping. */
	bool SmoothHeightAt(float CellX, float CellY, float& OutHeightM, bool* OutFromPing = nullptr) const;

	/**
	 * Which world a chart belongs to: the map's name and the seabed's generation. A saved chart from any other
	 * world is not loaded, or the old seabed would be drawn over the new one.
	 */
	FString GetWorldStamp() const;

	/** Server: the save slot to use, or empty for none. Real play uses one; tests only if asked. */
	FString GetSaveSlot() const;
	void LoadSurvey();
	void SaveSurvey();

	double GetServerTime() const;

	UFUNCTION()
	void OnRep_Pulse();

	UPROPERTY(Replicated)
	bool bSurveying = false;

	/** Which sonar this ship has. Starts at the best, for testing; real play will start lower and upgrade. */
	UPROPERTY(Replicated)
	int32 Grade = 2;

	UPROPERTY(Replicated)
	FSonarCellArray Chart;

	UPROPERTY(ReplicatedUsing = OnRep_Pulse)
	FSonarPulse Pulse;

	/** Server: (layer, cell index) to position in Chart.Items, so a cell is updated, never duplicated. */
	TMap<uint64, int32> CellLookup;

	/** Server: ping cells still to chart as the wave reaches them, nearest first. */
	struct FPendingPingCell { FIntPoint Cell; float DistanceM; };
	TArray<FPendingPingCell> PendingPing;
	int32 PendingPingNext = 0;

	// The chart as pages of cells, for drawing: heights, which are known, and when each last changed here.
	struct FPage
	{
		static constexpr int32 Size = 64;
		int16 Height[Size * Size];
		bool Known[Size * Size];
		float Changed[Size * Size];
		FPage();
	};
	TMap<FIntPoint, TUniquePtr<FPage>> Pages[2];
	TArray<FSonarCell> Arrived;      // Cells that changed since the last sync.
	bool bDenseSeeded = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> PulseParameters;
	int32 HeardPulse = 0;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> LocalChart;
	FIntPoint LocalOrigin = FIntPoint(0, 0);   // The cell at the image's south-west corner.
	int32 LocalCells = 0;                      // Cells across the image.
	double LocalBuiltAt = -1.0;
	float LocalDeepM = -700.f;
	float LocalShallowM = -100.f;
	// The picture being built, and how far along it is (0: not building).
	FIntPoint BuildOrigin = FIntPoint(0, 0);
	int32 BuildCells = 0;
	int32 BuildStage = 0;
	uint64 BuildFrame = 0;
	TArray<float> BuildHeights;
	TArray<float> BuildChanged;
	TBitArray<> BuildFromPing;
	TArray<FColor> BuildPixels;
	float BuildDeepM = -700.f;
	float BuildShallowM = -100.f;
	static constexpr int32 ChartSlices = 4;
	static constexpr int32 PixelsPerCell = 4;   // About 420 pixels across the 2.4 km view: the screens draw it at 512 or less.

	bool bTextureDirty = true;
	bool bUnsavedChanges = false;
	float SaveTimer = 0.f;
	float SweepTimer = 0.f;
	float TestPingTimer = 0.f;

public:
	/** Played on every machine when a ping goes out. Set up by the ship. */
	UPROPERTY(Transient)
	TObjectPtr<class UThalassaSonarPingComponent> PulseSound;
};
