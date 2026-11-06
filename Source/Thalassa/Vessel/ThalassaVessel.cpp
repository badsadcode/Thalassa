#include "Vessel/ThalassaVessel.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Tools/ThalassaTestConfig.h"
#include "UObject/ConstructorHelpers.h"

namespace ThalassaVesselLayout
{
	struct FHullBox
	{
		const TCHAR* Name;
		FVector Center;
		FVector Size;
		FLinearColor Color;
		bool bVisible;
	};

	const FLinearColor Hull(0.10f, 0.13f, 0.16f);
	const FLinearColor Deck(0.20f, 0.24f, 0.27f);
	const FLinearColor Bridge(0.08f, 0.22f, 0.30f);
	const FLinearColor Corridor(0.28f, 0.25f, 0.20f);
	const FLinearColor Engineering(0.35f, 0.18f, 0.10f);
	const FLinearColor Console(0.03f, 0.04f, 0.05f); // Dark, or the readout's text washes out.

	// Ship frame: +X forward, +Z up, deck surface at Z = 0, centimetres.
	// Bridge X 1000..2000, corridor X -1000..1000, engineering X -2000..-1000.
	const FHullBox Boxes[] =
	{
		{ TEXT("Deck"),            FVector(0, 0, -25),        FVector(4000, 900, 50),  Deck, true },
		{ TEXT("Ceiling"),         FVector(0, 0, 375),        FVector(4000, 900, 50),  Hull, true },
		{ TEXT("BowWall"),         FVector(2025, 0, 175),     FVector(50, 900, 450),   Bridge, true },
		{ TEXT("SternWall"),       FVector(-2025, 0, 175),    FVector(50, 900, 450),   Engineering, true },

		// Bridge: the port wall carries the observation window opening (X 1200..1800, Z 100..280).
		{ TEXT("BridgeStarboard"), FVector(1500, 425, 175),   FVector(1000, 50, 450),  Bridge, true },
		{ TEXT("BridgePortFwd"),   FVector(1900, -425, 175),  FVector(200, 50, 450),   Bridge, true },
		{ TEXT("BridgePortAft"),   FVector(1100, -425, 175),  FVector(200, 50, 450),   Bridge, true },
		{ TEXT("BridgePortLow"),   FVector(1500, -425, 50),   FVector(600, 50, 100),   Bridge, true },
		{ TEXT("BridgePortHigh"),  FVector(1500, -425, 325),  FVector(600, 50, 90),    Bridge, true },
		{ TEXT("WindowPane"),      FVector(1500, -425, 190),  FVector(600, 20, 180),   Hull, false },

		// Corridor walls and the two doorway bulkheads (doorway 200 wide, 250 high).
		{ TEXT("CorridorPort"),    FVector(0, -225, 175),     FVector(2000, 50, 350),  Corridor, true },
		{ TEXT("CorridorStbd"),    FVector(0, 225, 175),      FVector(2000, 50, 350),  Corridor, true },
		{ TEXT("BulkFwdPort"),     FVector(1000, -275, 175),  FVector(50, 350, 350),   Corridor, true },
		{ TEXT("BulkFwdStbd"),     FVector(1000, 275, 175),   FVector(50, 350, 350),   Corridor, true },
		{ TEXT("BulkFwdHead"),     FVector(1000, 0, 300),     FVector(50, 200, 100),   Corridor, true },
		{ TEXT("BulkAftPort"),     FVector(-1000, -275, 175), FVector(50, 350, 350),   Corridor, true },
		{ TEXT("BulkAftStbd"),     FVector(-1000, 275, 175),  FVector(50, 350, 350),   Corridor, true },
		{ TEXT("BulkAftHead"),     FVector(-1000, 0, 300),    FVector(50, 200, 100),   Corridor, true },

		// Engineering: outer walls, plus three steps to exercise step-up on a moving base.
		{ TEXT("EngPort"),         FVector(-1500, -425, 175), FVector(1000, 50, 450),  Engineering, true },
		{ TEXT("EngStbd"),         FVector(-1500, 425, 175),  FVector(1000, 50, 450),  Engineering, true },
		{ TEXT("Step1"),           FVector(-1650, 0, 10),     FVector(100, 400, 20),   Engineering, true },
		{ TEXT("Step2"),           FVector(-1750, 0, 20),     FVector(100, 400, 40),   Engineering, true },
		{ TEXT("Step3"),           FVector(-1850, 0, 30),     FVector(100, 400, 60),   Engineering, true },

		{ TEXT("BeepConsole"),     FVector(1950, 0, 60),      FVector(60, 200, 120),   Console, true },
	};

	// The hull is a sealed box, so nothing from outside lights the interior. Warm in the working
	// spaces, in keeping with the contrast the art direction wants against cold water.
	struct FInteriorLight { const TCHAR* Name; FVector Location; FColor Color; };
	const FInteriorLight Lights[] =
	{
		{ TEXT("LightBridge"),      FVector(1500, 0, 310),  FColor(255, 214, 170) },
		{ TEXT("LightCorridorFwd"), FVector(500, 0, 310),   FColor(255, 224, 190) },
		{ TEXT("LightCorridorAft"), FVector(-500, 0, 310),  FColor(255, 224, 190) },
		{ TEXT("LightEngineering"), FVector(-1500, 0, 310), FColor(255, 170, 120) },
	};
	const float InteriorLightIntensity = 12000.f;
	const float InteriorLightRadius = 1400.f;

	const FVector ConsoleLocation(1920.f, 0.f, 60.f);
	const float ConsoleUseRadius = 300.f;

	const FVector HullBoundsMin(-2050.f, -450.f, -50.f);
	const FVector HullBoundsMax(2050.f, 450.f, 400.f);

	// Every condition from the design document's test matrix that a kinematic hull can be driven
	// through. Duration seconds, speed m/s, yaw rate deg/s, pitch deg, roll deg.
	struct FSegment { float Duration; float SpeedMS; float YawRate; float Pitch; float Roll; };
	const FSegment Profile[] =
	{
		{ 15.f,  0.f,  0.f,  0.f,  0.f }, // rest
		{ 15.f, 10.f,  0.f,  0.f,  0.f }, // accelerate
		{ 15.f, 10.f,  0.f,  0.f,  0.f }, // cruise
		{ 20.f, 10.f,  3.f,  0.f,  0.f }, // turn to starboard
		{ 20.f, 10.f, -3.f,  0.f,  0.f }, // turn to port
		{ 15.f, 10.f,  0.f, 10.f,  0.f }, // pitch up
		{ 15.f, 10.f,  0.f,  0.f,  0.f }, // level out
		{ 15.f, 10.f,  0.f,  0.f, 10.f }, // roll
		{ 15.f, 10.f,  0.f,  0.f,  0.f }, // level out
		{ 20.f, 25.f,  8.f,  5.f,  5.f }, // hard manoeuvre
		{ 20.f,  0.f,  0.f,  0.f,  0.f }, // brake
	};

	const float AccelerationCmS2 = 200.f;
	const float YawAccelerationDegS2 = 2.f;
	const float AttitudeRateDegS = 2.f;
	const float PoseSmoothingRate = 6.f;      // 1/s, decay of client pose corrections
	const float ClockSmoothingRate = 2.f;     // 1/s, how quickly the client follows snapshot timing
	const double ClockSnapErrorSeconds = 0.25; // Beyond this (a stall), jump rather than drift back
	const float SnapErrorDistanceCm = 500.f;  // Beyond this, snap rather than visibly slide
}

AThalassaVessel::AThalassaVessel()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bReplicates = true;
	SetReplicatingMovement(false); // The snapshot is the single source of the ship's pose.
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30.f);
	NetPriority = 3.f;

	ShipRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
	ShipRoot->SetMobility(EComponentMobility::Movable);
	RootComponent = ShipRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// The cube's default material has no colour parameter; this one does. See BeginPlay.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TintableMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	for (const ThalassaVesselLayout::FHullBox& Box : ThalassaVesselLayout::Boxes)
	{
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Box.Name);
		Piece->SetupAttachment(ShipRoot);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetStaticMesh(CubeMesh.Object);
		Piece->SetMaterial(0, TintableMaterial.Object);
		Piece->SetRelativeLocation(Box.Center);
		Piece->SetRelativeScale3D(Box.Size / 100.f); // Engine cube is 100 cm per side.
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision); // HullCollision does this.
		Piece->SetVisibility(Box.bVisible);
		HullPieces.Add(Piece);
	}

	HullCollision = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HullCollision"));
	HullCollision->SetupAttachment(ShipRoot);
	HullCollision->SetMobility(EComponentMobility::Movable);
	HullCollision->SetStaticMesh(CubeMesh.Object);
	HullCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	HullCollision->SetHiddenInGame(true);

	for (const ThalassaVesselLayout::FInteriorLight& Light : ThalassaVesselLayout::Lights)
	{
		UPointLightComponent* Lamp = CreateDefaultSubobject<UPointLightComponent>(Light.Name);
		Lamp->SetupAttachment(ShipRoot);
		Lamp->SetMobility(EComponentMobility::Movable);
		Lamp->SetRelativeLocation(Light.Location);
		Lamp->SetLightColor(Light.Color);
		Lamp->SetIntensity(ThalassaVesselLayout::InteriorLightIntensity);
		Lamp->SetAttenuationRadius(ThalassaVesselLayout::InteriorLightRadius);
		Lamp->SetCastShadows(false);
	}

	ConsoleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ConsoleText"));
	ConsoleText->SetupAttachment(ShipRoot);
	ConsoleText->SetRelativeLocation(FVector(1918.f, 0.f, 90.f));
	ConsoleText->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	ConsoleText->SetHorizontalAlignment(EHTA_Center);
	ConsoleText->SetVerticalAlignment(EVRTA_TextCenter);
	ConsoleText->SetWorldSize(11.f);
	ConsoleText->SetTextRenderColor(FColor(88, 215, 211));
	ConsoleText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AThalassaVessel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AThalassaVessel, Snapshot);
	DOREPLIFETIME(AThalassaVessel, ProfileState);
	DOREPLIFETIME(AThalassaVessel, BeepCount);
}

AThalassaVessel* AThalassaVessel::Find(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<AThalassaVessel> It(const_cast<UWorld*>(World)); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void AThalassaVessel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Runs on server and clients alike, so both see identical collision.
	if (HullCollision->GetInstanceCount() == 0)
	{
		for (const ThalassaVesselLayout::FHullBox& Box : ThalassaVesselLayout::Boxes)
		{
			HullCollision->AddInstance(FTransform(FRotator::ZeroRotator, Box.Center, Box.Size / 100.f));
		}
	}
}

void AThalassaVessel::BeginPlay()
{
	Super::BeginPlay();

	PoseRotation = GetActorRotation();

	// Tint the graybox so compartments are recognisable. No effect on simulation.
	if (GetNetMode() != NM_DedicatedServer)
	{
		int32 Index = 0;
		for (const ThalassaVesselLayout::FHullBox& Box : ThalassaVesselLayout::Boxes)
		{
			if (UMaterialInstanceDynamic* Material = HullPieces[Index]->CreateAndSetMaterialInstanceDynamic(0))
			{
				Material->SetVectorParameterValue(TEXT("Color"), Box.Color);
			}
			++Index;
		}
	}
}

void AThalassaVessel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		TickServer(DeltaSeconds);
	}
	else
	{
		TickClient(DeltaSeconds);
	}

	UpdateConsoleText(DeltaSeconds);
}

void AThalassaVessel::StartProfile()
{
	if (HasAuthority() && ProfileState == EVesselProfileState::Idle)
	{
		ProfileState = EVesselProfileState::Running;
		ProfileStartTime = GetWorld()->GetTimeSeconds();
	}
}

void AThalassaVessel::SetHelmCommand(float Throttle, float Turn, float Dive)
{
	if (!HasAuthority() || ProfileState == EVesselProfileState::Running)
	{
		return;
	}
	TargetSpeedCmS = FMath::Clamp(Throttle, -1.f, 1.f) * 2500.f;
	TargetYawRate = FMath::Clamp(Turn, -1.f, 1.f) * 8.f;
	TargetPitch = FMath::Clamp(Dive, -1.f, 1.f) * -15.f;
	TargetRoll = 0.f;
}

void AThalassaVessel::UpdateProfileTargets()
{
	float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - ProfileStartTime);
	for (const ThalassaVesselLayout::FSegment& Segment : ThalassaVesselLayout::Profile)
	{
		if (Elapsed < Segment.Duration)
		{
			const float Motion = ThalassaTestConfig::IsShipStatic() ? 0.f : 1.f;
			TargetSpeedCmS = Motion * Segment.SpeedMS * 100.f;
			TargetYawRate = Motion * Segment.YawRate;
			TargetPitch = Motion * Segment.Pitch;
			TargetRoll = Motion * Segment.Roll;
			return;
		}
		Elapsed -= Segment.Duration;
	}

	TargetSpeedCmS = 0.f;
	TargetYawRate = 0.f;
	TargetPitch = 0.f;
	TargetRoll = 0.f;
	ProfileState = EVesselProfileState::Finished;
}

void AThalassaVessel::TickServer(float DeltaSeconds)
{
	using namespace ThalassaVesselLayout;

	if (ProfileState == EVesselProfileState::Running)
	{
		UpdateProfileTargets();
	}

	SpeedCmS = FMath::FInterpConstantTo(SpeedCmS, TargetSpeedCmS, DeltaSeconds, AccelerationCmS2);
	CurrentRate.Yaw = FMath::FInterpConstantTo(static_cast<float>(CurrentRate.Yaw), TargetYawRate, DeltaSeconds, YawAccelerationDegS2);

	const float OldPitch = static_cast<float>(PoseRotation.Pitch);
	const float OldRoll = static_cast<float>(PoseRotation.Roll);
	const float NewPitch = FMath::FInterpConstantTo(OldPitch, TargetPitch, DeltaSeconds, AttitudeRateDegS);
	const float NewRoll = FMath::FInterpConstantTo(OldRoll, TargetRoll, DeltaSeconds, AttitudeRateDegS);
	CurrentRate.Pitch = DeltaSeconds > 0.f ? (NewPitch - OldPitch) / DeltaSeconds : 0.f;
	CurrentRate.Roll = DeltaSeconds > 0.f ? (NewRoll - OldRoll) / DeltaSeconds : 0.f;

	PoseRotation.Pitch = NewPitch;
	PoseRotation.Roll = NewRoll;
	PoseRotation.Yaw = FRotator::NormalizeAxis(PoseRotation.Yaw + CurrentRate.Yaw * DeltaSeconds);

	const FVector Velocity = PoseRotation.Vector() * SpeedCmS;
	const FVector NewPosition = GetActorLocation() + Velocity * DeltaSeconds;
	SetActorLocationAndRotation(NewPosition, PoseRotation, false, nullptr, ETeleportType::None);

	Snapshot.Position = NewPosition;
	Snapshot.Velocity = Velocity;
	Snapshot.Rotation = PoseRotation;
	Snapshot.RotationRate = FVector(CurrentRate.Pitch, CurrentRate.Yaw, CurrentRate.Roll);
	Snapshot.ServerTime = GetWorld()->GetTimeSeconds();
}

double AThalassaVessel::GetSyncedTime() const
{
	const UWorld* World = GetWorld();
	return HasAuthority() ? World->GetTimeSeconds() : World->GetTimeSeconds() + ServerClockOffset;
}

void AThalassaVessel::GetExtrapolatedPose(double Time, FVector& OutPosition, FQuat& OutRotation) const
{
	// Bounded so a long network stall makes the ship hold course briefly rather than fly away.
	const float Age = FMath::Clamp(static_cast<float>(Time - Snapshot.ServerTime), -0.25f, 0.75f);
	OutPosition = Snapshot.Position + Snapshot.Velocity * Age;
	const FRotator Rotated = Snapshot.Rotation + FRotator(Snapshot.RotationRate.X * Age, Snapshot.RotationRate.Y * Age, Snapshot.RotationRate.Z * Age);
	OutRotation = Rotated.Quaternion();
}

void AThalassaVessel::UpdateClockFromSnapshot()
{
	using namespace ThalassaVesselLayout;

	// The clock tracks server time as of each snapshot's arrival, not "true" server time. Everything
	// replicated reaches this client about equally late, so showing the ship that late keeps it
	// consistent with what is around it. GameState's GetServerWorldTimeSeconds is not used: it is a
	// long running average, which takes around 25 seconds to recover from a stall on either machine.
	const double LocalTime = GetWorld()->GetTimeSeconds();
	const double MeasuredOffset = Snapshot.ServerTime - LocalTime;

	if (!bHasClockOffset || FMath::Abs(MeasuredOffset - ServerClockOffset) > ClockSnapErrorSeconds)
	{
		ServerClockOffset = MeasuredOffset;
		bHasClockOffset = true;
	}
	else
	{
		const double Elapsed = FMath::Max(LocalTime - LastClockSampleTime, 0.0);
		const double Blend = 1.0 - FMath::Exp(-ClockSmoothingRate * Elapsed);
		ServerClockOffset += (MeasuredOffset - ServerClockOffset) * Blend;
	}
	LastClockSampleTime = LocalTime;
}

void AThalassaVessel::OnRep_Snapshot()
{
	UpdateClockFromSnapshot();

	if (!bHasSnapshot)
	{
		bHasSnapshot = true;
		SetActorLocationAndRotation(Snapshot.Position, Snapshot.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	// Keep what is on screen continuous: turn the jump to the new target into a decaying offset.
	FVector NewTargetPosition;
	FQuat NewTargetRotation;
	GetExtrapolatedPose(GetSyncedTime(), NewTargetPosition, NewTargetRotation);

	PoseErrorOffset = GetActorLocation() - NewTargetPosition;
	PoseErrorRotation = GetActorQuat() * NewTargetRotation.Inverse();

	if (PoseErrorOffset.SizeSquared() > FMath::Square(ThalassaVesselLayout::SnapErrorDistanceCm))
	{
		PoseErrorOffset = FVector::ZeroVector;
		PoseErrorRotation = FQuat::Identity;
	}
}

void AThalassaVessel::TickClient(float DeltaSeconds)
{
	using namespace ThalassaVesselLayout;

	if (!bHasSnapshot)
	{
		return;
	}

	const float Decay = FMath::Exp(-PoseSmoothingRate * DeltaSeconds);
	PoseErrorOffset *= Decay;
	PoseErrorRotation = FQuat::Slerp(FQuat::Identity, PoseErrorRotation, Decay);

	FVector TargetPosition;
	FQuat TargetRotation;
	GetExtrapolatedPose(GetSyncedTime(), TargetPosition, TargetRotation);

	SetActorLocationAndRotation(TargetPosition + PoseErrorOffset, PoseErrorRotation * TargetRotation, false, nullptr, ETeleportType::None);
}

FTransform AThalassaVessel::GetSpawnTransform(int32 PlayerIndex) const
{
	// Staggered along the bridge so players never spawn overlapping.
	const FVector Local(1500.f - 100.f * (PlayerIndex % 8), PlayerIndex % 2 == 0 ? -70.f : 70.f, 120.f);
	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(Local));
}

bool AThalassaVessel::TryPressBeep(const FVector& WorldLocation)
{
	if (!HasAuthority())
	{
		return false;
	}

	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	if (FVector::DistSquared(Local, ThalassaVesselLayout::ConsoleLocation) > FMath::Square(ThalassaVesselLayout::ConsoleUseRadius))
	{
		return false;
	}

	++BeepCount;
	return true;
}

bool AThalassaVessel::IsInsideHull(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	return FBox(ThalassaVesselLayout::HullBoundsMin, ThalassaVesselLayout::HullBoundsMax).IsInsideOrOn(Local);
}

void AThalassaVessel::UpdateConsoleText(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// A physical display does not need to refresh every frame.
	ConsoleTextTimer -= DeltaSeconds;
	if (ConsoleTextTimer > 0.f)
	{
		return;
	}
	ConsoleTextTimer = 0.25f;

	ConsoleText->SetText(FText::FromString(FString::Printf(
		TEXT("BEEP %d\nDEPTH %.0f m  SPD %.1f m/s\nHDG %03.0f"),
		BeepCount, GetDepthMeters(), GetSpeedMetersPerSecond(),
		FRotator::ClampAxis(GetActorRotation().Yaw))));
}
