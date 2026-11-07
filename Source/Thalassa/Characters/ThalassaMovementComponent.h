#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ThalassaMovementComponent.generated.h"

/** A saved move that remembers whether the player was sprinting, so replays and the server agree. */
class FSavedMove_Thalassa : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;

	bool bSavedWantsToSprint = false;
	bool bSavedSliding = false;
};

class FNetworkPredictionData_Client_Thalassa : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_Thalassa(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement) {}
	virtual FSavedMovePtr AllocateNewMove() override;
};

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
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	/**
	 * Sprinting. Part of every saved move, so the server runs the same speed the client predicted;
	 * a sprint set any other way would be corrected on every step.
	 */
	void SetWantsToSprint(bool bSprint) { bWantsToSprint = bSprint; }
	bool IsSprinting() const;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SprintSpeed = 800.f;

	static constexpr float LadderSpeedCm = 200.f;

	/**
	 * Sliding: crouching while moving faster than this on foot. You keep your speed and lose it slowly, can
	 * steer a little, and come up into a crouch once it is gone. There is no slide timer or boost: whether
	 * you slide follows from your speed and the crouch flag, both of which the network already corrects,
	 * so client and server cannot disagree about it.
	 */
	bool IsSliding() const;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SlideMinSpeed = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SlideDeceleration = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SlideSteerDegreesPerSecond = 45.f;

	bool bWantsToSprint = false;

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
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

private:
	void LogCorrection(const FVector& NewLocation, const FMovementBaseInterfaceData* NewBase, uint8 ServerMovementMode) const;

	FVector ShipFrameLocationBeforeCorrection = FVector::ZeroVector;
	bool bHasLocationBeforeCorrection = false;

	float RecentWorstFrameSeconds = 0.f;
	float RecentFrameWindowSeconds = 0.f;
};
