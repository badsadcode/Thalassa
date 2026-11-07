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

	const bool bGrabbingLadder = bOnLadder && !bInWater && MovementMode != MOVE_Flying;
	if ((bInWater || bOnLadder) && MovementMode != MOVE_Flying)
	{
		// On the hull, Velocity is relative to the ship. Flying lets go of the base without changing it,
		// and the code below treats a character with no base as moving in the world's frame, so put it
		// in that frame first. Without this, the ship's velocity was taken away twice, and anyone
		// reaching a ladder on a moving ship was thrown astern, out of the climbing zone, every time.
		// The engine's own GetImpartedMovementBaseVelocity() reads zero here: the hull is moved by
		// setting its pose, so its components report no velocity.
		if (CharacterOwner->GetMovementBaseObject() == Vessel->GetMovementBaseComponent())
		{
			Velocity += Vessel->GetWorldVelocity();
		}
		SetMovementMode(MOVE_Flying); // Also clears the base; it is restored below if still aboard.
	}
	else if (!bInWater && !bOnLadder && MovementMode == MOVE_Flying)
	{
		SetMovementMode(MOVE_Falling); // Drained: drop to the deck, or stepped indoors.
	}

	FString LadderTracePath;
	if (Vessel->IsLadderAt(Location, 150.f) && ThalassaTestConfig::GetTelemetryPath(LadderTracePath))
	{
		const FVector Local = Vessel->GetActorTransform().InverseTransformPosition(Location);
		const FVector LocalVelocity = Vessel->GetActorTransform().InverseTransformVectorNoScale(Velocity);
		const UObject* Base = CharacterOwner->GetMovementBaseObject();
		UE_LOG(LogThalassa, Display, TEXT("LADDERTRACE,%s,%d,%.3f,%.1f,%.1f,%.1f,%.0f,%.0f,%.0f,%d,%d,%s,%d,%.1f,%s"),
			*CharacterOwner->GetName(), static_cast<int32>(CharacterOwner->GetLocalRole()), Vessel->GetSyncedTime(),
			Local.X, Local.Y, Local.Z, LocalVelocity.X, LocalVelocity.Y, LocalVelocity.Z,
			static_cast<int32>(MovementMode.GetValue()), bOnLadder ? 1 : 0, Base ? *Base->GetName() : TEXT("none"),
			CurrentFloor.bBlockingHit ? 1 : 0, CurrentFloor.FloorDist,
			CurrentFloor.HitResult.GetComponent() ? *CurrentFloor.HitResult.GetComponent()->GetName() : TEXT("-"));
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

	if (bGrabbingLadder)
	{
		// Taking hold of a ladder stops you. Arriving at a sprint, you would otherwise cross its climbing
		// zone in a frame or two and bounce off the wall behind it. Velocity is in the ship's frame again here.
		Velocity = Velocity.GetClampedToMaxSize(LadderSpeedCm);
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
	if (IsClimbingLadder())
	{
		return LadderSpeedCm;
	}
	return IsSprinting() ? SprintSpeed : Super::GetMaxSpeed();
}

bool UThalassaMovementComponent::IsSprinting() const
{
	// On foot only: not while climbing, swimming or in the air.
	return bWantsToSprint && MovementMode == MOVE_Walking;
}

void UThalassaMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UThalassaMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UThalassaMovementComponent* Mutable = const_cast<UThalassaMovementComponent*>(this);
		Mutable->ClientPredictionData = new FNetworkPredictionData_Client_Thalassa(*this);
	}
	return ClientPredictionData;
}

FSavedMovePtr FNetworkPredictionData_Client_Thalassa::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Thalassa());
}

void FSavedMove_Thalassa::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = false;
}

uint8 FSavedMove_Thalassa::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Flags |= FLAG_Custom_0;
	}
	return Flags;
}

bool FSavedMove_Thalassa::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	if (bSavedWantsToSprint != static_cast<const FSavedMove_Thalassa*>(NewMove.Get())->bSavedWantsToSprint)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Thalassa::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const UThalassaMovementComponent* Movement = Cast<UThalassaMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Movement->bWantsToSprint;
	}
}

void FSavedMove_Thalassa::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (UThalassaMovementComponent* Movement = Cast<UThalassaMovementComponent>(C->GetCharacterMovement()))
	{
		Movement->bWantsToSprint = bSavedWantsToSprint;
	}
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
