#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThalassaOceanFauna.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class AThalassaOcean;
class AThalassaVessel;

/** Something alive the ping can return from: where it is and how big, in world centimetres. */
struct FThalassaSonarTarget
{
	FVector Location = FVector::ZeroVector;
	float RadiusCm = 0.f;
};

/** A ping the animals react to: where and when (server clock) it went out, and how far it reached. */
struct FFaunaPing
{
	FVector Origin = FVector::ZeroVector;
	double Time = 0.0;
	float RangeM = 0.f;
};

/** A school's address: its kind of animal and its cell on that kind's lattice. */
struct FFaunaSchoolKey
{
	uint8 Species = 0;
	int64 X = 0;
	int64 Y = 0;
	bool operator==(const FFaunaSchoolKey& Other) const { return Species == Other.Species && X == Other.X && Y == Other.Y; }
	friend uint32 GetTypeHash(const FFaunaSchoolKey& Key) { return HashCombine(HashCombine(::GetTypeHash(Key.Species), ::GetTypeHash(Key.X)), ::GetTypeHash(Key.Y)); }
};

/** A school, as the seed lays it out: where it lives and who is in it. Nothing in it changes with time. */
struct FFaunaSchool
{
	FFaunaSchoolKey Key;
	FVector2D HomeM = FVector2D::ZeroVector;   // The middle of the area it roams.
	double AltitudeM = 40.0;                   // How high above the ground it swims.
	double RoamM = 150.0;                      // How far it wanders from home.
	double SpeedMS = 1.5;                      // How fast it cruises.
	double Phase[4] = { 0.0, 0.0, 0.0, 0.0 };  // Where in its wandering it was when the clock started.
	struct FMember
	{
		FVector OffsetM = FVector::ZeroVector; // Its place in the school, in the school's frame.
		double Scale = 1.0;
		double Drift[3] = { 0.0, 0.0, 0.0 };   // Phases of its slow drift within the school.
		float Beat = 0.f;                      // Where in its stroke it is.
	};
	TArray<FMember> Members;
};

/** Where a school is at a moment, and how startled. */
struct FFaunaSchoolState
{
	FVector CentreCm = FVector::ZeroVector;
	double GroundCm = 0.0;
	double YawDeg = 0.0;
	double PitchDeg = 0.0;
	double TurnDegS = 0.0;       // How fast it is turning: the animals bank into it.
	double Spread = 0.0;         // 0 calm; up to 1 scattered by a ping.
	double Rate = 1.0;           // Stroke rate.
	double Flash = 0.0;          // The glow's flash when a ping reaches it.
	double FleeYawDeg = 0.0;     // Which way it fled.
	double Fleeing = 0.0;        // How much it is still turned away.
};

/** One piece of the leviathan, placed along its body. */
struct FLeviathanPiece
{
	uint8 Mesh = 0;              // 0 head, 1 body segment, 2 tail.
	FTransform Transform;
	float RadiusCm = 0.f;
	float Flash = 0.f;           // Its lights flaring as a ping's wave passes over.
};

/**
 * Life in the open ocean. See docs/CREATURES.md.
 *
 * Schools of many kinds: small schooling fish, bigger fish near the ground, big slow cruisers, gliders, giant and
 * glowing jellyfish, swarms of tiny glowing ones. And one leviathan, near the start, far bigger than the ship.
 *
 * Like the seabed and the rocks, nothing is sent over the network but the pings. Where each school lives follows
 * from the seed; where it is at any moment follows from the server's clock, which every machine shares; and how
 * it reacts to pings follows from the sonar's replicated list of recent pings. So every player sees the same
 * animals in the same places doing the same thing. A ping startles schools when its wave reaches them; it draws
 * the leviathan, which comes to circle where the ping came from, then leaves.
 *
 * Drawn as instances, one component per mesh; strokes, wingbeats and jellyfish pulses are in the materials.
 */
UCLASS()
class THALASSA_API UThalassaOceanFauna : public UActorComponent
{
	GENERATED_BODY()

public:
	UThalassaOceanFauna();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The school of a kind in a cell, if the seed puts one there. Pure: the same seed and cell always give the same school. */
	static bool MakeSchool(const AThalassaOcean& Ocean, uint8 Species, int64 CellX, int64 CellY, FFaunaSchool& OutSchool);

	/** Where a school is at a time on the server's clock, given the recent pings. Pure. */
	static FFaunaSchoolState SchoolAt(const AThalassaOcean& Ocean, const FFaunaSchool& School, double Time,
		TConstArrayView<FFaunaPing> Pings, const FVector* ShipLocation);

	/** Everything alive the ping could return from within a range of a point, at a time on the server's clock. */
	void GatherSonarTargets(const FVector& Origin, float RangeCm, double Time, TArray<FThalassaSonarTarget>& OutTargets);

	/** Where the leviathan's head is, in world centimetres, at a time on the server's clock. */
	FVector LeviathanHeadAt(double Time);

	/** Animals being drawn now. */
	int32 GetVisibleAnimals() const { return VisibleAnimals; }

private:
	const FFaunaSchool* FindSchool(uint8 Species, int64 CellX, int64 CellY);
	void ReadPings();
	double Now() const;

	/** The leviathan's pieces at a time, along the path its head has taken. */
	void LeviathanPieces(double Time, TArray<FLeviathanPiece>& OutPieces);
	/** Where the head was at a past sample time, cached: the path behind it doesn't change. */
	FVector LeviathanSample(int64 Index);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Instances;

	/** Schools laid out so far, including empty cells (no members). Cheap; kept. */
	TMap<FFaunaSchoolKey, FFaunaSchool> Schools;

	// What each instance component drew last frame, so an unchanged set only moves its instances.
	struct FDrawn
	{
		TArray<FTransform> Transforms;
		TArray<float> CustomData;
		uint32 Signature = 0;
	};
	TArray<FDrawn> Drawn;
	int32 VisibleAnimals = 0;

	TArray<FFaunaPing> Pings;
	TMap<int64, FVector> LeviathanTrail;
	FVector2D LeviathanHomeM = FVector2D::ZeroVector;
	double LeviathanLoopM = 1000.0;
	double LeviathanSpeedMS = 5.0;
	double LeviathanPhase = 0.0;

	TWeakObjectPtr<AThalassaVessel> Vessel;
	bool bDisabled = false;
};
