#include "Vessel/ThalassaHelmComponent.h"

#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Vessel/ThalassaVessel.h"

namespace ThalassaHelm
{
	// Levers have a speed of their own, so nothing on the ship jumps from one setting to another.
	constexpr float ThrottleLeverPerSecond = 0.8f;
	constexpr float RudderLeverPerSecond = 1.2f;
	constexpr float PlanesLeverPerSecond = 1.f;

	// In set-and-go, holding a key moves the setting this fast.
	constexpr float SetThrottlePerSecond = 0.4f;
	constexpr float SetPlanesPerSecond = 0.5f;

	// Pushed this far from the seat, you are no longer sitting at it.
	constexpr float LeaveSeatCm = 300.f;
}

UThalassaHelmComponent::UThalassaHelmComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UThalassaHelmComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UThalassaHelmComponent, Occupant);
	DOREPLIFETIME(UThalassaHelmComponent, Mode);
	DOREPLIFETIME(UThalassaHelmComponent, Levers);
}

AThalassaVessel* UThalassaHelmComponent::GetVessel() const
{
	return Cast<AThalassaVessel>(GetOwner());
}

FVector UThalassaHelmComponent::GetSeatWorldLocation() const
{
	const AThalassaVessel* Vessel = GetVessel();
	return Vessel ? Vessel->GetActorTransform().TransformPosition(Vessel->GetHelmSeatLocal()) : FVector::ZeroVector;
}

bool UThalassaHelmComponent::IsHelmsman(const APawn* Pawn) const
{
	return GetOwner()->HasAuthority() && Pawn && Occupant == Pawn;
}

bool UThalassaHelmComponent::TrySitOrStand(APawn* Who)
{
	const AThalassaVessel* Vessel = GetVessel();
	if (!GetOwner()->HasAuthority() || !Who || !Vessel || !Vessel->HasHelm())
	{
		return false;
	}

	if (Occupant == Who)
	{
		Occupant = nullptr;
		Input = FHelmLevers();
		return true;
	}

	if (Occupant || FVector::Dist2D(Who->GetActorLocation(), GetSeatWorldLocation()) > SeatReachCm)
	{
		return false;
	}

	Occupant = Who;
	Input = FHelmLevers();
	return true;
}

void UThalassaHelmComponent::SetInput(const APawn* From, float Throttle, float Turn, float Dive)
{
	if (IsHelmsman(From))
	{
		Input.Throttle = FMath::Clamp(Throttle, -1.f, 1.f);
		Input.Rudder = FMath::Clamp(Turn, -1.f, 1.f);
		Input.Planes = FMath::Clamp(Dive, -1.f, 1.f);
	}
}

void UThalassaHelmComponent::ToggleMode(const APawn* From)
{
	if (IsHelmsman(From))
	{
		// Switching never lurches the ship: the new mode starts from wherever the levers are now.
		Mode = Mode == EHelmMode::Precise ? EHelmMode::SetAndGo : EHelmMode::Precise;
		Commanded = Levers;
	}
}

void UThalassaHelmComponent::AllStop(const APawn* From)
{
	if (IsHelmsman(From))
	{
		Commanded.Throttle = 0.f;
		Commanded.Planes = 0.f;
	}
}

void UThalassaHelmComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	using namespace ThalassaHelm;

	// Someone who leaves, is pushed away, or disconnects is no longer at the helm.
	if (Occupant && (!IsValid(Occupant) || FVector::Dist2D(Occupant->GetActorLocation(), GetSeatWorldLocation()) > LeaveSeatCm))
	{
		Occupant = nullptr;
		Input = FHelmLevers();
	}

	if (Mode == EHelmMode::Precise)
	{
		// The ship does what you hold, and nothing once you let go. Nobody at the helm is letting go.
		Commanded = Input;
	}
	else
	{
		Commanded.Throttle = FMath::Clamp(Commanded.Throttle + Input.Throttle * SetThrottlePerSecond * DeltaTime, -1.f, 1.f);
		Commanded.Planes = FMath::Clamp(Commanded.Planes + Input.Planes * SetPlanesPerSecond * DeltaTime, -1.f, 1.f);
		Commanded.Rudder = Input.Rudder; // The rudder always centres itself when let go.
	}

	Levers.Throttle = FMath::FInterpConstantTo(Levers.Throttle, Commanded.Throttle, DeltaTime, ThrottleLeverPerSecond);
	Levers.Rudder = FMath::FInterpConstantTo(Levers.Rudder, Commanded.Rudder, DeltaTime, RudderLeverPerSecond);
	Levers.Planes = FMath::FInterpConstantTo(Levers.Planes, Commanded.Planes, DeltaTime, PlanesLeverPerSecond);
}
