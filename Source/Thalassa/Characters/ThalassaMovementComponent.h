#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ThalassaMovementComponent.generated.h"

/**
 * Character movement for a moving vessel. Stock based movement plus the measurements the
 * moving-ship spike needs, and an optional ship-relative gravity mode to compare against.
 */
UCLASS()
class THALASSA_API UThalassaMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UThalassaMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual bool ClientUpdatePositionAfterServerUpdate() override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	virtual float GetMaxSpeed() const override;

	/** Swimming uses MOVE_Flying: no gravity, full 3D input, and it needs no water volume to exist. */
	bool IsSwimmingInSea() const;

	/** Ladders use MOVE_Flying too, in a ladder zone and out of the water, held in place with no input. */
	bool IsClimbingLadder() const;

	/** True if this character is close enough to grab a ladder. */
	bool IsAtLadder() const;

	using UCharacterMovementComponent::OnClientCorrectionReceived;
	virtual void OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, FMovementBaseInterfaceData* NewMovementBaseInterfaceData, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override;

	int32 CorrectionCount = 0;
	float MaxCorrectionCm = 0.f;
	float LastCorrectionCm = 0.f;

	// What the player actually sees: how far a correction moved them once their unacknowledged moves
	// were replayed. CorrectionCount's distance is not this. It compares the current position with an
	// acknowledgement one round trip old, so it grows with speed * ping even when nothing visibly jumps.
	int32 VisibleSnapCount = 0;
	float MaxVisibleSnapCm = 0.f;
	int32 UnexpectedAirborneCount = 0;

protected:
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	void LogCorrection(const FVector& NewLocation, const FMovementBaseInterfaceData* NewBase, uint8 ServerMovementMode) const;

	FVector ShipFrameLocationBeforeCorrection = FVector::ZeroVector;
	bool bHasLocationBeforeCorrection = false;

	float RecentWorstFrameSeconds = 0.f;
	float RecentFrameWindowSeconds = 0.f;
};
