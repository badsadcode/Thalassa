#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThalassaVessel.generated.h"

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
 * stand on it through CharacterMovementComponent based movement. Hull geometry is a graybox made
 * of default subobjects so every piece has a stable name and can be referenced as a movement base.
 */
UCLASS()
class THALASSA_API AThalassaVessel : public AActor
{
	GENERATED_BODY()

public:
	AThalassaVessel();

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

	/** Server time as seen by this process, low-pass filtered on clients so the ship does not shake. */
	double GetSyncedTime() const;

	EVesselProfileState GetProfileState() const { return ProfileState; }
	int32 GetBeepCount() const { return BeepCount; }
	float GetSpeedMetersPerSecond() const { return static_cast<float>(Snapshot.Velocity.Size()) / 100.f; }
	float GetDepthMeters() const { return static_cast<float>(-GetActorLocation().Z) / 100.f; }

private:
	void TickServer(float DeltaSeconds);
	void TickClient(float DeltaSeconds);
	void UpdateProfileTargets();
	void GetExtrapolatedPose(double Time, FVector& OutPosition, FQuat& OutRotation) const;
	void UpdateConsoleText(float DeltaSeconds);

	UFUNCTION()
	void OnRep_Snapshot();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> ShipRoot;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> HullPieces;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> ConsoleText;

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
	bool bHasClockOffset = false;
	bool bHasSnapshot = false;

	float ConsoleTextTimer = 0.f;
};
