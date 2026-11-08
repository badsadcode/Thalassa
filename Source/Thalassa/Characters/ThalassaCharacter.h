#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ThalassaCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UThalassaMovementComponent;
class AThalassaVessel;

/** First-person crew member. In a test run, a scripted walker that tours the ship. */
UCLASS()
class THALASSA_API AThalassaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AThalassaCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	/** Local player: try to use whatever console is in reach. */
	void Interact();

	/** Jump, from a crouch or a slide too. Leaving a slide this way keeps its speed. */
	void JumpFromAnywhere();

protected:
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

public:

	UThalassaMovementComponent* GetThalassaMovement() const;

	/** Waypoints the test walker failed to reach in time. Reported by telemetry. */
	int32 BotStuckCount = 0;

	/** Complete trips out through the airlock and back aboard, by the test diver. */
	int32 BotEvaCount = 0;

private:
	UFUNCTION(Server, Reliable)
	void ServerInteract();

	void TickWalkerBot(float DeltaSeconds);
	void TickDiverBot(float DeltaSeconds);
	void TickHelmBot(float DeltaSeconds);

	/** At a station, walk onto its spot and stay there. */
	void HoldStationPosition();
	bool IsAtStation() const;
	void ApplyCrewNudge();

	/** Crew closer than this, centre to centre, are nudged apart. Capsules touch at 68 cm. */
	UPROPERTY(EditDefaultsOnly, Category = "Crew")
	float PersonalSpaceCm = 90.f;

	/** Strongest nudge, as a fraction of full movement input, when standing right on top of someone. */
	UPROPERTY(EditDefaultsOnly, Category = "Crew")
	float MaxNudgeInput = 0.35f;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(Transient)
	TObjectPtr<AThalassaVessel> BoundVessel;

	int32 BotDiveStep = 0;
	float BotDiveStepSeconds = 0.f;
	float BotPressCooldown = 0.f;

	int32 BotWaypoint = 0;
	bool bTourStarted = false;
	bool bHelmRouteStarted = false;
	float BotJumpTimer = 6.f;
	float BotSecondsOnWaypoint = 0.f;
	float BotHelmSeconds = -1.f;
	int32 BotHelmCue = 0;
	float BotHelmLogTimer = 0.f;
	/** How far the view still has to travel after a crouch or stand moved the capsule. Eased to zero. */
	float CameraCrouchOffset = 0.f;

	float BotSlideTimer = 5.f;
	FVector BotSlideStart = FVector::ZeroVector;
	float BotSlideStartSpeed = 0.f;
};
