#include "Characters/ThalassaMovementComponent.h"

#include "Characters/ThalassaCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Thalassa.h"
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

	MaxFlySpeed = 400.f;                // Swimming speed.
	BrakingDecelerationFlying = 600.f;  // Water stops you quickly when you stop swimming.
}

void UThalassaMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Worst frame over about the last second, kept for correction reports.
	RecentFrameWindowSeconds += DeltaTime;
	if (RecentFrameWindowSeconds > 1.f)
	{
		RecentFrameWindowSeconds = 0.f;
		RecentWorstFrameSeconds = 0.f;
	}
	RecentWorstFrameSeconds = FMath::Max(RecentWorstFrameSeconds, DeltaTime);

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

void UThalassaMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// Runs inside every move, on the client, on the server, and in replays, which makes these
	// transitions predicted: they should need no correction beyond small differences in the ship's pose.
	const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld());
	if (!Vessel || !CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	const FVector Location = UpdatedComponent->GetComponentLocation();
	const bool bInWater = Vessel->IsWaterAt(Location);

	// You take hold of a ladder by moving while next to it, and stay on it until you leave its zone.
	const bool bOnLadder = Vessel->IsLadderAt(Location) && (MovementMode == MOVE_Flying || !Acceleration.IsNearlyZero());

	if ((bInWater || bOnLadder) && MovementMode != MOVE_Flying)
	{
		SetMovementMode(MOVE_Flying); // Also clears the base; it is restored below if still aboard.
	}
	else if (!bInWater && !bOnLadder && MovementMode == MOVE_Flying)
	{
		SetMovementMode(MOVE_Falling); // Drained: drop to the deck, or stepped indoors.
	}

	if (MovementMode == MOVE_Flying)
	{
		// In a flooded compartment you move with the ship, and the sea does not. Velocity is kept in
		// the world's frame across the change, so crossing the threshold is not a sudden kick.
		UPrimitiveComponent* Hull = Vessel->GetMovementBaseComponent();
		const bool bOnHull = CharacterOwner->GetMovementBaseObject() == Hull;
		const bool bInCompartment = Vessel->IsInCompartment(Location);
		if (bInCompartment && !bOnHull)
		{
			Velocity -= Vessel->GetWorldVelocity();
			FMovementBaseInterfaceData HullBase(Hull);
			SetBase(&HullBase);
		}
		else if (!bInCompartment && bOnHull)
		{
			Velocity += Vessel->GetWorldVelocity();
			FMovementBaseInterfaceData NoBase;
			SetBase(&NoBase);
		}
	}
}

bool UThalassaMovementComponent::IsAtLadder() const
{
	const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld());
	return Vessel && UpdatedComponent && Vessel->IsLadderAt(UpdatedComponent->GetComponentLocation());
}

bool UThalassaMovementComponent::IsClimbingLadder() const
{
	return MovementMode == MOVE_Flying && !IsSwimmingInSea();
}

bool UThalassaMovementComponent::IsSwimmingInSea() const
{
	if (MovementMode != MOVE_Flying)
	{
		return false;
	}
	const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld());
	return !Vessel || !UpdatedComponent || Vessel->IsWaterAt(UpdatedComponent->GetComponentLocation());
}

float UThalassaMovementComponent::GetMaxSpeed() const
{
	constexpr float LadderSpeed = 200.f;
	return IsClimbingLadder() ? LadderSpeed : Super::GetMaxSpeed();
}

bool UThalassaMovementComponent::ClientUpdatePositionAfterServerUpdate()
{
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();

	const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld());
	if (bHasLocationBeforeCorrection && Vessel)
	{
		bHasLocationBeforeCorrection = false;
		const FVector Local = Vessel->GetActorTransform().InverseTransformPosition(UpdatedComponent->GetComponentLocation());
		const float SnapCm = static_cast<float>(FVector::Dist(Local, ShipFrameLocationBeforeCorrection));
		++VisibleSnapCount;
		MaxVisibleSnapCm = FMath::Max(MaxVisibleSnapCm, SnapCm);

		FString TelemetryPath;
		if (ThalassaTestConfig::GetTelemetryPath(TelemetryPath))
		{
			// Parsed by Scripts/build/analyze_corrections.py.
			UE_LOG(LogThalassa, Display, TEXT("SNAP,%.3f,%.2f,%d"), Vessel->GetSyncedTime(), SnapCm, static_cast<int32>(Vessel->GetProfileState()));
		}
	}

	return bResult;
}

void UThalassaMovementComponent::LogCorrection(const FVector& NewLocation, const FMovementBaseInterfaceData* NewBase, uint8 ServerMovementMode) const
{
	FString TelemetryPath;
	const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld());
	if (!Vessel || !CharacterOwner || !ThalassaTestConfig::GetTelemetryPath(TelemetryPath))
	{
		return;
	}

	float NearestPawnCm = -1.f;
	for (TActorIterator<AThalassaCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != CharacterOwner)
		{
			const float Distance = static_cast<float>(FVector::Dist(It->GetActorLocation(), NewLocation));
			NearestPawnCm = NearestPawnCm < 0.f ? Distance : FMath::Min(NearestPawnCm, Distance);
		}
	}

	const FVector Local = Vessel->GetActorTransform().InverseTransformPosition(NewLocation);
	const UObject* ClientBase = CharacterOwner->GetMovementBaseObject();
	const UObject* ServerBase = NewBase ? NewBase->GetMovementBaseObject() : nullptr;

	// One line per correction, parsed by Scripts/build/analyze_corrections.py.
	UE_LOG(LogThalassa, Display, TEXT("CORR,%.3f,%.2f,%d,%d,%.0f,%.0f,%.0f,%.0f,%.4f,%s,%s,%d"),
		Vessel->GetSyncedTime(), LastCorrectionCm, static_cast<int32>(MovementMode.GetValue()), static_cast<int32>(ServerMovementMode),
		Local.X, Local.Y, Local.Z, NearestPawnCm, RecentWorstFrameSeconds,
		ClientBase ? *ClientBase->GetName() : TEXT("none"), ServerBase ? *ServerBase->GetName() : TEXT("none"),
		static_cast<int32>(Vessel->GetProfileState()));
}

void UThalassaMovementComponent::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp, FVector NewLocation, FVector NewVelocity, FMovementBaseInterfaceData* NewMovementBaseInterfaceData, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode, FVector ServerGravityDirection)
{
	if (!bHasLocationBeforeCorrection)
	{
		if (const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld()))
		{
			ShipFrameLocationBeforeCorrection = Vessel->GetActorTransform().InverseTransformPosition(UpdatedComponent->GetComponentLocation());
			bHasLocationBeforeCorrection = true;
		}
	}

	// NewLocation is world space here, and the component has not been moved yet.
	LastCorrectionCm = static_cast<float>((UpdatedComponent->GetComponentLocation() - NewLocation).Size());
	MaxCorrectionCm = FMath::Max(MaxCorrectionCm, LastCorrectionCm);
	++CorrectionCount;
	LogCorrection(NewLocation, NewMovementBaseInterfaceData, ServerMovementMode);

	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewMovementBaseInterfaceData, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode, ServerGravityDirection);
}
