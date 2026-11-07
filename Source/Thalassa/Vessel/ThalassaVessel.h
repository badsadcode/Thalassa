#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThalassaVessel.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UThalassaSonarComponent;
class UThalassaSonarScreenComponent;
class UThalassaSonarPingComponent;
class UThalassaHelmComponent;
class UThalassaHelmScreenComponent;

/** Authoritative vessel pose, published by the server and dead-reckoned by clients. */
USTRUCT()
struct FVesselSnapshot
{
	GENERATED_BODY()

	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() FVector Velocity = FVector::ZeroVector;        // cm/s, world space
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;

	// Pitch, yaw, roll in deg/s. Deliberately not an FRotator: FRotator's net serialization wraps
	// each axis into [0, 360), so a rate of -2 deg/s would arrive as +358 deg/s.
	UPROPERTY() FVector RotationRate = FVector::ZeroVector;

	UPROPERTY() double ServerTime = 0.0;
};

/** The airlock's cycle. The inner and outer doors are never open at the same time. */
UENUM()
enum class EAirlockState : uint8
{
	InnerOpen,  // Dry, open to the ship.
	Flooding,   // Both doors shut, filling.
	OuterOpen,  // Flooded, open to the sea.
	Draining    // Both doors shut, emptying.
};

UENUM()
enum class EVesselProfileState : uint8
{
	Idle,      // Under manual helm control.
	Running,   // Following the scripted motion test.
	Finished   // Scripted test complete; the ship has stopped.
};

/**
 * Moving-ship spike, approach A: a genuinely moving kinematic actor in one persistent world.
 *
 * The server integrates a deliberately simple motion model and publishes snapshots. Characters
 * stand on it through CharacterMovementComponent based movement.
 *
 * All hull collision is deliberately one component, so a character's movement base never changes
 * as they walk around the ship. Replicated position is relative to the base, and under packet loss
 * the base and the position can arrive separately: with several bases, that shows a player many
 * metres from where they really are. Any real hull mesh must keep this property.
 */
UCLASS()
class THALASSA_API AThalassaVessel : public AActor
{
	GENERATED_BODY()

public:
	AThalassaVessel();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static AThalassaVessel* Find(const UWorld* World);

	/** World-space spawn point for the Nth player. */
	FTransform GetSpawnTransform(int32 PlayerIndex) const;

	/** Server: begin the scripted motion test. */
	void StartProfile();

	/** The helm station: who is steering, and its levers. Null on ships without one. */
	UThalassaHelmComponent* GetHelm() const { return Helm; }
	bool HasHelm() const;

	/** Where the helmsman stands, in ship coordinates. */
	FVector GetHelmSeatLocal() const;

	/** True while the ship is floating at the surface and cannot rise further. */
	bool IsSurfaced() const;

	/** How many times the ship has run into the seabed or anything else, and how long ago the last time was (negative: never). */
	int32 GetCollisionCount() const { return CollisionCount; }
	float GetSecondsSinceCollision() const;

	/** Server: if a character at this world location can reach the console, increment the counter. */
	bool TryPressBeep(const FVector& WorldLocation);

	/** True if a world location lies within the hull's ship-frame bounds. */
	bool IsInsideHull(const FVector& WorldLocation) const;

	/** Server: if a character at this world location can reach the panel, start the next airlock cycle. */
	bool TryCycleAirlock(const FVector& WorldLocation);

	/** True if this world location is under water: the open sea, or a flooded compartment. */
	bool IsWaterAt(const FVector& WorldLocation) const;

	/** True if this world location is in a ship compartment, so anyone there moves with the ship. */
	bool IsInCompartment(const FVector& WorldLocation) const;

	/** True if a character here can hold onto a ladder. */
	bool IsLadderAt(const FVector& WorldLocation, float MarginCm = 0.f) const;

	/** The ship's route for the scripted walker, in ship coordinates. Empty on the test rig. */
	const TArray<FVector>& GetTourRoute() const;

	/** The hull's one movement base. See the class comment. */
	UPrimitiveComponent* GetMovementBaseComponent() const;

	FVector GetWorldVelocity() const { return Snapshot.Velocity; }
	UThalassaSonarComponent* GetSonar() const { return Sonar; }
	EAirlockState GetAirlockState() const { return AirlockState; }
	float GetAirlockWaterLevel() const { return AirlockWaterLevel; }
	bool IsAirlockDoorwayBlocked() const { return bAirlockDoorwayBlocked; }

	/** Server time as seen by this process: on clients, as of the latest snapshots, filtered so the ship does not shake. */
	double GetSyncedTime() const;

	EVesselProfileState GetProfileState() const { return ProfileState; }
	int32 GetBeepCount() const { return BeepCount; }
	float GetSpeedMetersPerSecond() const { return static_cast<float>(Snapshot.Velocity.Size()) / 100.f; }

	/** Speed along the ship's heading: negative when going astern. */
	float GetForwardSpeedMetersPerSecond() const { return static_cast<float>(FVector::DotProduct(Snapshot.Velocity, GetActorForwardVector())) / 100.f; }
	float GetDepthMeters() const { return static_cast<float>(-GetActorLocation().Z) / 100.f; }

private:
	void TickServer(float DeltaSeconds);
	void TickClient(float DeltaSeconds);
	void UpdateProfileTargets();
	void TickHelmMotion(float DeltaSeconds);

	/** Server: where the ship can actually get to on its way from one place to the next. Sets bOutHit if something stopped it. */
	FVector ConstrainMove(const FVector& From, const FVector& To, const FQuat& Rotation, bool& bOutHit);

	/** True if the hull, posed like this, would be inside the seabed or anything else in the world. */
	bool IsHullBlocked(const FVector& Location, const FQuat& Rotation) const;
	void TickHelmVisuals(float DeltaSeconds);
	void UpdateClockFromSnapshot();
	void GetExtrapolatedPose(double Time, FVector& OutPosition, FQuat& OutRotation) const;
	void UpdateConsoleText(float DeltaSeconds);
	void TickAirlockServer(float DeltaSeconds);
	void TickAirlockVisuals(float DeltaSeconds);
	bool IsDoorwayOccupied(float DoorY) const;

	UFUNCTION()
	void OnRep_Snapshot();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> ShipRoot;

	/** Graybox visuals only. */
	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> HullPieces;

	/** All hull collision, and the one movement base. Never rendered. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> HullCollision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ConsoleText;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UThalassaSonarComponent> Sonar;

	/** The chart on the bridge, and the ping heard from it. Absent on ships without a sonar station. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UThalassaSonarScreenComponent> SonarScreen;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UThalassaSonarPingComponent> SonarPing;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UThalassaHelmComponent> Helm;

	/** The helm's screen, and its throttle lever, which pivots at its foot. Absent on ships without a helm. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UThalassaHelmScreenComponent> HelmScreen;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> HelmLeverPivot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> HelmLever;

	/** The lever as drawn, eased toward the replicated setting so it moves smoothly between updates. */
	float ShownThrottle = 0.f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> AirlockInnerDoor;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> AirlockOuterDoor;

	/** The visible surface of the water in the chamber. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> AirlockWater;

	UPROPERTY(Replicated)
	EAirlockState AirlockState = EAirlockState::InnerOpen;

	UPROPERTY(Replicated)
	float AirlockWaterLevel = 0.f;

	UPROPERTY(Replicated)
	bool bAirlockDoorwayBlocked = false;

	float AirlockInnerDoorOpen = 1.f;
	float AirlockOuterDoorOpen = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FVesselSnapshot Snapshot;

	UPROPERTY(Replicated)
	EVesselProfileState ProfileState = EVesselProfileState::Idle;

	UPROPERTY(Replicated)
	int32 BeepCount = 0;

	UPROPERTY(Replicated)
	int32 CollisionCount = 0;

	/** Server time of the last collision, or negative for never. */
	UPROPERTY(Replicated)
	double LastCollisionTime = -1.0;

	/** Server: the hull was against something last tick. */
	bool bTouching = false;

	// Server motion state. Targets come from the helm or from the motion profile.
	float SpeedCmS = 0.f;
	float TargetSpeedCmS = 0.f;
	FRotator CurrentRate = FRotator::ZeroRotator;
	FRotator PoseRotation = FRotator::ZeroRotator;
	float TargetYawRate = 0.f;
	float TargetPitch = 0.f;
	float TargetRoll = 0.f;
	double ProfileStartTime = 0.0;

	// Client smoothing state.
	FVector PoseErrorOffset = FVector::ZeroVector;
	FQuat PoseErrorRotation = FQuat::Identity;
	double ServerClockOffset = 0.0;
	double LastClockSampleTime = 0.0;
	bool bHasClockOffset = false;
	bool bHasSnapshot = false;

	float ConsoleTextTimer = 0.f;
};
