#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThalassaStationComponent.generated.h"

class AThalassaVessel;

/**
 * A station aboard: a spot one person can take with E, and leave with E again. Whoever is there works
 * the station; nobody else can. Someone who walks or is pushed away, or disconnects, leaves it.
 *
 * The server owns who is at a station. Subclasses add what the station does.
 */
UCLASS(Abstract)
class THALASSA_API UThalassaStationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThalassaStationComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: take the station if it is free and within reach, or leave it if already there. */
	bool TrySitOrStand(APawn* Who);

	APawn* GetOccupant() const { return Occupant; }
	bool IsSeated(const APawn* Pawn) const { return Pawn && Occupant == Pawn; }

	/** Where the person at this station stands, in world space. */
	FVector GetSeatWorldLocation() const;

	/** Where the person at this station stands, in ship coordinates. Set by the vessel when it builds the station. */
	UPROPERTY()
	FVector SeatLocal = FVector::ZeroVector;

	/** False for a station this ship does not have. Set by the vessel. */
	UPROPERTY()
	bool bAvailable = false;

	static constexpr float SeatReachCm = 200.f;
	static constexpr float LeaveSeatCm = 300.f;

protected:
	/** Server: someone took the station or left it. */
	virtual void OnOccupantChanged() {}

	/** Server: true if this pawn is the one at the station. */
	bool IsOccupant(const APawn* Pawn) const;

	AThalassaVessel* GetVessel() const;

private:
	UPROPERTY(Replicated)
	TObjectPtr<APawn> Occupant;
};
