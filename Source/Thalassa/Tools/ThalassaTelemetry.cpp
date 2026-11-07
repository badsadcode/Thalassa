#include "Tools/ThalassaTelemetry.h"

#include "Characters/ThalassaCharacter.h"
#include "Characters/ThalassaMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Thalassa.h"
#include "Tools/ThalassaTestConfig.h"
#include "Vessel/ThalassaVessel.h"

namespace
{
	// A travel back to the default map after a disconnect would otherwise truncate the results.
	bool GTelemetryStarted = false;

	constexpr double RowIntervalSeconds = 0.1;
	constexpr double ServerLingerSeconds = 5.0;  // Let clients see the end of the test and leave first.
	constexpr float PopAllowanceFactor = 1.25f;  // Speed margin before a ship-frame step counts as a pop.
	constexpr float PopThresholdCm = 5.f;
	constexpr float StallLogSeconds = 0.05f;     // Three missed frames at 60 fps.
	constexpr int32 RowsPerFlush = 100;          // About every 10 seconds.

	void WriteLine(FArchive& Ar, const FString& Line)
	{
		const FTCHARToUTF8 Utf8(*(Line + TEXT("\n")));
		Ar.Serialize(const_cast<ANSICHAR*>(Utf8.Get()), Utf8.Length());
	}
}

bool UThalassaTelemetry::ShouldCreateSubsystem(UObject* Outer) const
{
	FString Path;
	return ThalassaTestConfig::GetTelemetryPath(Path) && Super::ShouldCreateSubsystem(Outer);
}

bool UThalassaTelemetry::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game;
}

void UThalassaTelemetry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The engine boots through a transient world before loading the real map, so a world only counts
	// as "the test" once a vessel exists in it. See OpenWriter.
	if (GTelemetryStarted)
	{
		RequestExit(TEXT("world reloaded during test (connection lost?)"));
	}
}

void UThalassaTelemetry::OpenWriter()
{
	GTelemetryStarted = true;

	FString Path;
	ThalassaTestConfig::GetTelemetryPath(Path);
	Writer.Reset(IFileManager::Get().CreateFileWriter(*Path));
	if (Writer)
	{
		WriteLine(*Writer, TEXT("t,player,local_role,lx,ly,lz,wx,wy,wz,mode,based,ship_x,ship_y,ship_z,ship_yaw,ship_pitch,ship_roll,ship_speed,profile,beeps,corr,corr_max,corr_last,pops,pop_max,airborne,stuck,outside,worst_frame,ping,eva"));
	}
	else
	{
		UE_LOG(LogThalassa, Error, TEXT("Telemetry: could not open %s"), *Path);
	}
}

void UThalassaTelemetry::Deinitialize()
{
	Writer.Reset();
	Super::Deinitialize();
}

TStatId UThalassaTelemetry::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UThalassaTelemetry, STATGROUP_Tickables);
}

void UThalassaTelemetry::RequestExit(const TCHAR* Reason)
{
	if (!bExitRequested)
	{
		bExitRequested = true;
		UE_LOG(LogThalassa, Display, TEXT("Telemetry: exiting: %s"), Reason);
		if (Writer)
		{
			Writer->Flush();
		}
		FPlatformMisc::RequestExit(false, TEXT("ThalassaTelemetry"));
	}
}

void UThalassaTelemetry::TrackLocalFrameStep(float DeltaTime)
{
	const UWorld* World = GetWorld();
	const AThalassaVessel* Vessel = AThalassaVessel::Find(World);
	const APlayerController* Controller = World->GetFirstPlayerController();
	const AThalassaCharacter* Character = Controller ? Cast<AThalassaCharacter>(Controller->GetPawn()) : nullptr;
	if (!Vessel || !Character || Vessel->GetProfileState() != EVesselProfileState::Running)
	{
		return;
	}

	WorstFrameSeconds = FMath::Max(WorstFrameSeconds, DeltaTime);

	const FVector Local = Vessel->GetActorTransform().InverseTransformPosition(Character->GetActorLocation());
	if (bHasLastLocalPosition)
	{
		// In the ship's frame a walker moves at walking speed. Anything well beyond that is a
		// visible pop for this player, whatever caused it.
		const UThalassaMovementComponent* Movement = Character->GetThalassaMovement();
		const float Allowed = FMath::Max(Movement->MaxWalkSpeed, Movement->JumpZVelocity) * PopAllowanceFactor * DeltaTime;
		const float Excess = static_cast<float>(FVector::Dist(Local, LastLocalPosition)) - Allowed;
		if (Excess > PopThresholdCm)
		{
			++PopCount;
			MaxPopCm = FMath::Max(MaxPopCm, Excess);
		}
	}
	LastLocalPosition = Local;
	bHasLastLocalPosition = true;

	// Being outside the hull only counts as a fault if you are not meant to be: swimmers are.
	if (!Character->GetThalassaMovement()->IsSwimmingInSea() && !Vessel->IsInsideHull(Character->GetActorLocation()))
	{
		++OutsideHullFrames;
	}
}

void UThalassaTelemetry::WriteRows(float DeltaTime)
{
	UWorld* World = GetWorld();
	const AThalassaVessel* Vessel = AThalassaVessel::Find(World);
	if (!Vessel)
	{
		return;
	}
	if (!Writer)
	{
		OpenWriter();
		if (!Writer)
		{
			return;
		}
	}

	RowTimer -= DeltaTime;
	if (RowTimer > 0.0)
	{
		return;
	}
	RowTimer = RowIntervalSeconds;

	const FTransform ShipTransform = Vessel->GetActorTransform();
	const FVector ShipLocation = Vessel->GetActorLocation();
	const FRotator ShipRotation = Vessel->GetActorRotation();

	for (TActorIterator<AThalassaCharacter> It(World); It; ++It)
	{
		const AThalassaCharacter* Character = *It;
		const APlayerState* PlayerState = Character->GetPlayerState();
		const UThalassaMovementComponent* Movement = Character->GetThalassaMovement();
		if (!PlayerState || !Movement)
		{
			continue;
		}

		const FVector WorldPos = Character->GetActorLocation();
		const FVector Local = ShipTransform.InverseTransformPosition(WorldPos);
		const bool bLocal = Character->IsLocallyControlled();

		WriteLine(*Writer, FString::Printf(
			TEXT("%.3f,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%.2f,%d,%d,%d,%.2f,%.2f,%d,%.2f,%d,%d,%d,%.4f,%.1f,%d"),
			Vessel->GetSyncedTime(), PlayerState->GetPlayerId(), static_cast<int32>(Character->GetLocalRole()),
			Local.X, Local.Y, Local.Z, WorldPos.X, WorldPos.Y, WorldPos.Z,
			static_cast<int32>(Movement->MovementMode.GetValue()), Character->GetMovementBaseObject() != nullptr ? 1 : 0,
			ShipLocation.X, ShipLocation.Y, ShipLocation.Z, ShipRotation.Yaw, ShipRotation.Pitch, ShipRotation.Roll,
			Vessel->GetSpeedMetersPerSecond(), static_cast<int32>(Vessel->GetProfileState()), Vessel->GetBeepCount(),
			Movement->CorrectionCount, Movement->MaxCorrectionCm, Movement->LastCorrectionCm,
			bLocal ? PopCount : 0, bLocal ? MaxPopCm : 0.f, Movement->UnexpectedAirborneCount, Character->BotStuckCount,
			bLocal ? OutsideHullFrames : 0, bLocal ? WorstFrameSeconds : 0.f, PlayerState->GetPingInMilliseconds(), Character->BotEvaCount));
	}

	// Rarely: frequent flushes to a slow disk stall the game and ruin the very thing being measured.
	++RowsSinceFlush;
	if (RowsSinceFlush >= RowsPerFlush)
	{
		RowsSinceFlush = 0;
		Writer->Flush();
	}
}

void UThalassaTelemetry::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bExitRequested)
	{
		return;
	}

	TrackLocalFrameStep(DeltaTime);
	WriteRows(DeltaTime);

	// Slow frames, to compare across processes and to tell how often they really happen.
	const AThalassaVessel* FrameVessel = AThalassaVessel::Find(GetWorld());
	if (DeltaTime >= StallLogSeconds && FrameVessel && FrameVessel->GetProfileState() == EVesselProfileState::Running)
	{
		UE_LOG(LogThalassa, Display, TEXT("STALL,%.3f,%.4f"), FrameVessel->GetSyncedTime(), DeltaTime);
	}

	ElapsedSeconds += DeltaTime;
	if (ElapsedSeconds > ThalassaTestConfig::TimeoutSeconds())
	{
		RequestExit(TEXT("timeout"));
		return;
	}

	const UWorld* World = GetWorld();
	const AThalassaVessel* Vessel = AThalassaVessel::Find(World);
	if (Vessel && Vessel->GetProfileState() == EVesselProfileState::Finished)
	{
		if (FinishedSeconds < 0.0)
		{
			FinishedSeconds = ElapsedSeconds;
		}
		const double Linger = World->GetNetMode() == NM_Client ? 0.0 : ServerLingerSeconds;
		if (ElapsedSeconds - FinishedSeconds >= Linger)
		{
			RequestExit(TEXT("motion test finished"));
		}
	}
}
