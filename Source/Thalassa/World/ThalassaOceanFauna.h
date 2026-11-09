#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThalassaOceanFauna.generated.h"

class UInstancedStaticMeshComponent;
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
	int32 Serial = 0;
};

/** A school's address on the fauna lattice: its cell. */
struct FFaunaSchoolKey
{
	int64 X = 0;
	int64 Y = 0;
	bool operator==(const FFaunaSchoolKey& Other) const { return X == Other.X && Y == Other.Y; }
	friend uint32 GetTypeHash(const FFaunaSchoolKey& Key) { return HashCombine(::GetTypeHash(Key.X), ::GetTypeHash(Key.Y)); }
};

/** A school, as the seed lays it out: where it lives and who is in it. Nothing in it changes with time. */
struct FFaunaSchool
{
	FFaunaSchoolKey Key;
	FVector2D HomeM = FVector2D::ZeroVector;   // The middle of the area it roams.
	double AltitudeM = 40.0;                   // How high above the ground it glides.
	double RoamM = 150.0;                      // How far it wanders from home.
	double SpeedMS = 1.5;                      // How fast it cruises.
	double Phase[4] = { 0.0, 0.0, 0.0, 0.0 };  // Where in its wandering it was when the clock started.
	struct FMember
	{
		FVector OffsetM = FVector::ZeroVector; // Its place in the school, in the school's frame.
		double Scale = 1.0;
		double Drift[3] = { 0.0, 0.0, 0.0 };   // Phases of its slow drift within the school.
		float Beat = 0.f;                      // Where in its wingbeat it is.
	};
	TArray<FMember> Members;
};

/** Where a school is at a moment, and how startled. */
struct FFaunaSchoolState
{
	FVector CentreCm = FVector::ZeroVector;
	double YawDeg = 0.0;
	double PitchDeg = 0.0;
	double TurnDegS = 0.0;       // How fast it is turning: the animals bank into it.
	double Spread = 0.0;         // 0 calm; up to 1 scattered by a ping.
	double Rate = 1.0;           // Wingbeat rate.
	double Flash = 0.0;          // The glow's flash when a ping reaches it.
	double FleeYawDeg = 0.0;     // Which way it fled.
	double Fleeing = 0.0;        // How much it is still turned away.
};

/**
 * Life in the open ocean. The first creature: schools of big, peaceful gliders. See docs/CREATURES.md.
 *
 * Like the seabed and the rocks, nothing is sent over the network. Where each school lives follows from the seed;
 * where it is at any moment follows from the server's clock, which every machine shares; and how it reacts to a
 * ping follows from the replicated ping. So every player sees the same animals in the same places doing the same
 * thing, and none of it costs bandwidth. Each school wanders a loop round its home, gliding at a height above the
 * ground. When a ping's wave reaches it, it scatters away from the ship, its glow flashes, and it gathers again.
 *
 * The animals are drawn as instances, one component per kind; their wingbeats are in the material.
 */
UCLASS()
class THALASSA_API UThalassaOceanFauna : public UActorComponent
{
	GENERATED_BODY()

public:
	UThalassaOceanFauna();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Fauna lattice cells are this big: at most one school in each. */
	static constexpr double CellM = 600.0;

	/** The school in a cell, if the seed puts one there. Pure: the same seed and cell always give the same school. */
	static bool MakeSchool(const AThalassaOcean& Ocean, int64 CellX, int64 CellY, FFaunaSchool& OutSchool);

	/** Where a school is at a time on the server's clock, given the recent pings. Pure. */
	static FFaunaSchoolState SchoolAt(const AThalassaOcean& Ocean, const FFaunaSchool& School, double Time,
		TConstArrayView<FFaunaPing> Pings, const FVector* ShipLocation);

	/** Everything alive the ping could return from within a range of a point, at a time on the server's clock. */
	void GatherSonarTargets(const FVector& Origin, float RangeCm, double Time, TArray<FThalassaSonarTarget>& OutTargets);

	/** Schools being drawn now, and animals in them. */
	int32 GetVisibleSchools() const { return Visible.Num(); }
	int32 GetVisibleAnimals() const { return VisibleAnimals; }

private:
	const FFaunaSchool* FindSchool(int64 CellX, int64 CellY);
	/** Notes a new ping, if one has gone out, keeping the last few: a startled school takes most of a minute to settle. */
	void NotePings();
	double Now() const;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Gliders;

	/** Schools laid out so far, by cell, including empty cells (an empty school). Cheap; kept. */
	TMap<FFaunaSchoolKey, FFaunaSchool> Schools;

	/** The schools drawn last frame, in the order their animals are in the instances. */
	TArray<FFaunaSchoolKey> Visible;
	int32 VisibleAnimals = 0;
	TArray<FTransform> Transforms;
	TArray<float> CustomData;

	TArray<FFaunaPing> Pings;
	TWeakObjectPtr<AThalassaVessel> Vessel;
	bool bDisabled = false;
};
