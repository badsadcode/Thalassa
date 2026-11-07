#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThalassaVessel.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

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

	/** Server: apply manual helm input, each axis in [-1, 1]. Ignored during the scripted test. */
	void SetHelmCommand(float Throttle, float Turn, float Dive);

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
	bool IsLadderAt(const FVector& WorldLocation) const;

	/** The ship's route for the scripted walker, in ship coordinates. Empty on the test rig. */
	const TArray<FVector>& GetTourRoute() const;

	/** The hull's one movement base. See the class comment. */
	UPrimitiveComponent* GetMovementBaseComponent() const;

	FVector GetWorldVelocity() const { return Snapshot.Velocity; }
	EAirlockState GetAirlockState() const { return AirlockState; }
	float GetAirlockWaterLevel() const { return AirlockWaterLevel; }
	bool IsAirlockDoorwayBlocked() const { return bAirlockDoorwayBlocked; }

	/** Server time as seen by this process: on clients, as of the latest snapshots, filtered so the ship does not shake. */
	double GetSyncedTime() const;

	EVesselProfileState GetProfileState() const { return ProfileState; }
	int32 GetBeepCount() const { return BeepCount; }
	float GetSpeedMetersPerSecond() const { return static_cast<float>(Snapshot.Velocity.Size()) / 100.f; }
	float GetDepthMeters() const { return static_cast<float>(-GetActorLocation().Z) / 100.f; }

private:
	void TickServer(float DeltaSeconds);
	void TickClient(float DeltaSeconds);
	void UpdateProfileTargets();
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
