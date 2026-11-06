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

	UThalassaMovementComponent* GetThalassaMovement() const;

	/** Waypoints the test walker failed to reach in time. Reported by telemetry. */
	int32 BotStuckCount = 0;

private:
	UFUNCTION(Server, Reliable)
	void ServerInteract();

	void TickWalkerBot(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(Transient)
	TObjectPtr<AThalassaVessel> BoundVessel;

	int32 BotWaypoint = 0;
	float BotJumpTimer = 6.f;
	float BotSecondsOnWaypoint = 0.f;
};
