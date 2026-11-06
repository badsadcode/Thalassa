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

	using UCharacterMovementComponent::OnClientCorrectionReceived;
	virtual void OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, FMovementBaseInterfaceData* NewMovementBaseInterfaceData, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection) override;

	int32 CorrectionCount = 0;
	float MaxCorrectionCm = 0.f;
	float LastCorrectionCm = 0.f;
	int32 UnexpectedAirborneCount = 0;

protected:
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
};
