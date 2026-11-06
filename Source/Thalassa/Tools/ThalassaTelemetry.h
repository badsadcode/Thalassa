#pragma once

#include "CoreMinimal.h"
#include "Serialization/Archive.h"
#include "Subsystems/WorldSubsystem.h"
#include "ThalassaTelemetry.generated.h"

/**
 * Records what each process believes about the ship and every character aboard. Comparing the CSVs
 * from different processes is what turns "looks fine" into a measured result. See
 * Scripts/build/analyze_motion_test.py.
 */
UCLASS()
class THALASSA_API UThalassaTelemetry : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void OpenWriter();
	void WriteRows(float DeltaTime);
	void TrackLocalFrameStep(float DeltaTime);
	void RequestExit(const TCHAR* Reason);

	TUniquePtr<FArchive> Writer;

	FVector LastLocalPosition = FVector::ZeroVector;
	bool bHasLastLocalPosition = false;
	int32 PopCount = 0;
	float MaxPopCm = 0.f;
	float WorstFrameSeconds = 0.f;
	int32 OutsideHullFrames = 0;
	double RowTimer = 0.0;
	int32 RowsSinceFlush = 0;
	double ElapsedSeconds = 0.0;
	double FinishedSeconds = -1.0;
	bool bExitRequested = false;
};
