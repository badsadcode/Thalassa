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
