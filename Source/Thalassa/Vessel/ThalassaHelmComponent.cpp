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
	constexpr float HeaveLeverPerSecond = 1.f;

	// In set-and-go, holding a key moves the setting this fast.
	constexpr float SetThrottlePerSecond = 0.4f;
	constexpr float SetPlanesPerSecond = 0.5f;
	constexpr float SetHeavePerSecond = 0.5f;

	// How close you must stand to press the full stop button.
	constexpr float ButtonReachCm = 130.f;

	// Touching a control this firmly takes the ship out of full stop.
	constexpr float ResumeInput = 0.1f;
}

UThalassaHelmComponent::UThalassaHelmComponent()
{
}

void UThalassaHelmComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UThalassaHelmComponent, Mode);
	DOREPLIFETIME(UThalassaHelmComponent, Levers);
	DOREPLIFETIME(UThalassaHelmComponent, bFullStop);
	DOREPLIFETIME(UThalassaHelmComponent, bTestBoost);
}

void UThalassaHelmComponent::OnOccupantChanged()
{
	// A new helmsman starts with their hands off the controls.
	Input = FHelmLevers();
}

void UThalassaHelmComponent::SetInput(const APawn* From, float Throttle, float Turn, float Dive, float Heave)
{
	if (IsOccupant(From))
	{
		Input.Throttle = FMath::Clamp(Throttle, -1.f, 1.f);
		Input.Rudder = FMath::Clamp(Turn, -1.f, 1.f);
		Input.Planes = FMath::Clamp(Dive, -1.f, 1.f);
		Input.Heave = FMath::Clamp(Heave, -1.f, 1.f);

		// Taking the controls again is what ends a full stop.
		using namespace ThalassaHelm;
		if (bFullStop && (FMath::Abs(Input.Throttle) > ResumeInput || FMath::Abs(Input.Rudder) > ResumeInput || FMath::Abs(Input.Planes) > ResumeInput || FMath::Abs(Input.Heave) > ResumeInput))
		{
			bFullStop = false;
			Commanded = FHelmLevers();
		}
	}
}

FVector UThalassaHelmComponent::GetFullStopButtonWorldLocation() const
{
	const AThalassaVessel* Vessel = GetVessel();
	return Vessel ? Vessel->GetActorTransform().TransformPosition(Vessel->GetFullStopButtonLocal()) : FVector::ZeroVector;
}

bool UThalassaHelmComponent::TryPressFullStopButton(const APawn* Who)
{
	const AThalassaVessel* Vessel = GetVessel();
	if (!GetOwner()->HasAuthority() || !Who || !Vessel || !bAvailable
		|| FVector::Dist2D(Who->GetActorLocation(), GetFullStopButtonWorldLocation()) > ThalassaHelm::ButtonReachCm)
	{
		return false;
	}
	EngageFullStop();
	return true;
}

void UThalassaHelmComponent::EngageFullStop()
{
	bFullStop = true;
	Commanded = FHelmLevers();
}

void UThalassaHelmComponent::ToggleMode(const APawn* From)
{
	if (IsOccupant(From))
	{
		// Switching never lurches the ship: the new mode starts from wherever the levers are now.
		Mode = Mode == EHelmMode::Precise ? EHelmMode::SetAndGo : EHelmMode::Precise;
		Commanded = Levers;
	}
}

void UThalassaHelmComponent::ToggleTestBoost(const APawn* From)
{
#if !UE_BUILD_SHIPPING
	if (IsOccupant(From))
	{
		bTestBoost = !bTestBoost;
	}
#endif
}

void UThalassaHelmComponent::FullStop(const APawn* From)
{
	if (IsOccupant(From))
	{
		EngageFullStop();
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

	if (bFullStop)
	{
		// Every control to neutral and held there. The vessel does the braking.
		Commanded = FHelmLevers();
	}
	else if (Mode == EHelmMode::Precise)
	{
		// The ship does what you hold, and nothing once you let go. Nobody at the helm is letting go.
		Commanded = Input;
	}
	else
	{
		Commanded.Throttle = FMath::Clamp(Commanded.Throttle + Input.Throttle * SetThrottlePerSecond * DeltaTime, -1.f, 1.f);
		Commanded.Planes = FMath::Clamp(Commanded.Planes + Input.Planes * SetPlanesPerSecond * DeltaTime, -1.f, 1.f);
		Commanded.Heave = FMath::Clamp(Commanded.Heave + Input.Heave * SetHeavePerSecond * DeltaTime, -1.f, 1.f);
		Commanded.Rudder = Input.Rudder; // The rudder always centres itself when let go.
	}

	Levers.Throttle = FMath::FInterpConstantTo(Levers.Throttle, Commanded.Throttle, DeltaTime, ThrottleLeverPerSecond);
	Levers.Rudder = FMath::FInterpConstantTo(Levers.Rudder, Commanded.Rudder, DeltaTime, RudderLeverPerSecond);
	Levers.Planes = FMath::FInterpConstantTo(Levers.Planes, Commanded.Planes, DeltaTime, PlanesLeverPerSecond);
	Levers.Heave = FMath::FInterpConstantTo(Levers.Heave, Commanded.Heave, DeltaTime, HeaveLeverPerSecond);
}
