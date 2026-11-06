#include "Characters/ThalassaMovementComponent.h"

#include "GameFramework/Character.h"
#include "Tools/ThalassaTestConfig.h"
#include "Vessel/ThalassaVessel.h"

UThalassaMovementComponent::UThalassaMovementComponent()
{
	MaxWalkSpeed = 400.f;
	bStayBasedInAir = true;        // A jump must land on the same spot of a moving deck.
	StayBasedInAirHeight = 1000.f;
	bImpartBaseVelocityX = true;
	bImpartBaseVelocityY = true;
	bImpartBaseVelocityZ = true;
	bImpartBaseAngularVelocity = true;
}

void UThalassaMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (ThalassaTestConfig::UseShipGravity())
	{
		if (const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld()))
		{
			SetGravityDirection(-Vessel->GetActorUpVector());
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UThalassaMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (MovementMode == MOVE_Falling && CharacterOwner && CharacterOwner->JumpCurrentCount == 0 && !CharacterOwner->bPressedJump)
	{
		++UnexpectedAirborneCount;
	}
}

void UThalassaMovementComponent::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, FMovementBaseInterfaceData* NewMovementBaseInterfaceData, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	// NewLocation is world space here, and the component has not been moved yet.
	LastCorrectionCm = static_cast<float>((UpdatedComponent->GetComponentLocation() - NewLocation).Size());
	MaxCorrectionCm = FMath::Max(MaxCorrectionCm, LastCorrectionCm);
	++CorrectionCount;

	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewMovementBaseInterfaceData, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode, ServerGravityDirection);
}
