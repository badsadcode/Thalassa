#include "Vessel/ThalassaStationComponent.h"

#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Vessel/ThalassaVessel.h"

UThalassaStationComponent::UThalassaStationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UThalassaStationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UThalassaStationComponent, Occupant);
}

AThalassaVessel* UThalassaStationComponent::GetVessel() const
{
	return Cast<AThalassaVessel>(GetOwner());
}

FVector UThalassaStationComponent::GetSeatWorldLocation() const
{
	return GetOwner()->GetActorTransform().TransformPosition(SeatLocal);
}

bool UThalassaStationComponent::IsOccupant(const APawn* Pawn) const
{
	return GetOwner()->HasAuthority() && Pawn && Occupant == Pawn;
}

bool UThalassaStationComponent::TrySitOrStand(APawn* Who)
{
	if (!GetOwner()->HasAuthority() || !Who || !bAvailable)
	{
		return false;
	}

	if (Occupant == Who)
	{
		Occupant = nullptr;
		OnOccupantChanged();
		return true;
	}

	if (Occupant || FVector::Dist2D(Who->GetActorLocation(), GetSeatWorldLocation()) > SeatReachCm)
	{
		return false;
	}

	Occupant = Who;
	OnOccupantChanged();
	return true;
}

void UThalassaStationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Someone who leaves, is pushed away, or disconnects is no longer at the station.
	if (GetOwner()->HasAuthority() && Occupant
		&& (!IsValid(Occupant) || FVector::Dist2D(Occupant->GetActorLocation(), GetSeatWorldLocation()) > LeaveSeatCm))
	{
		Occupant = nullptr;
		OnOccupantChanged();
	}
}
