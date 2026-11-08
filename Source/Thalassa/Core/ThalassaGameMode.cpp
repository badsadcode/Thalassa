#include "Core/ThalassaGameMode.h"

#include "Characters/ThalassaCharacter.h"
#include "Core/ThalassaPlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Tools/ThalassaHUD.h"
#include "Tools/ThalassaTestConfig.h"
#include "Vessel/ThalassaVessel.h"
#include "World/ThalassaOcean.h"

AThalassaGameMode::AThalassaGameMode()
{
	DefaultPawnClass = AThalassaCharacter::StaticClass();
	PlayerControllerClass = AThalassaPlayerController::StaticClass();
	HUDClass = AThalassaHUD::StaticClass();
}

void AThalassaGameMode::InitGameState()
{
	Super::InitGameState();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// 200 m below the surface (Z = 0), well clear of the test seabed.
	FVector Start(0.f, 0.f, -20000.f);
	// In the open ocean, start above whatever the seabed is here: 120 m above it, but no shallower than 200 m
	// unless the ground forces it, and never within 30 m of the surface.
	if (const AThalassaOcean* Ocean = AThalassaOcean::Find(GetWorld()))
	{
		const double Ground = Ocean->GroundZ(Start);
		Start.Z = FMath::Min(FMath::Max(-20000.0, Ground + 12000.0), -3000.0);
		UE_LOG(LogTemp, Display, TEXT("THALASSA: open ocean: seabed %.0f m under the start, ship starts at %.0f m"), -Ground / 100.0, -Start.Z / 100.0);
	}
	ThalassaTestConfig::GetShipStartOverride(Start);
	float AboveGroundM = 0.f;
	if (const AThalassaOcean* Ocean = AThalassaOcean::Find(GetWorld()); Ocean && ThalassaTestConfig::GetShipAboveGround(AboveGroundM))
	{
		// Measured from the highest ground anywhere under the 280 m hull, so it never starts buried.
		double Highest = -1e12;
		for (int32 I = -4; I <= 4; ++I)
		{
			for (int32 J = -2; J <= 2; ++J)
			{
				Highest = FMath::Max(Highest, Ocean->GroundZ(Start + FVector(I * 3750.0, J * 1500.0, 0.0)));
			}
		}
		Start.Z = Highest + AboveGroundM * 100.0;
		UE_LOG(LogTemp, Display, TEXT("THALASSA: test start %.0f m above the highest ground under the hull (%.0f m), at %.0f m"), AboveGroundM, -Highest / 100.0, -Start.Z / 100.0);
	}
	// The test rig is a fixture: always the plain C++ ship, so spots moved in the Blueprint never reach it.
	UClass* ShipClass = (VesselClass.IsNull() || ThalassaTestConfig::UseTestRig()) ? nullptr : VesselClass.LoadSynchronous();
	if (!ShipClass)
	{
		if (!VesselClass.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("THALASSA: ship class %s did not load; using the plain C++ ship."), *VesselClass.ToString());
		}
		ShipClass = AThalassaVessel::StaticClass();
	}
	UE_LOG(LogTemp, Display, TEXT("THALASSA: spawning ship %s"), *ShipClass->GetName());
	Vessel = GetWorld()->SpawnActor<AThalassaVessel>(ShipClass, Start, FRotator::ZeroRotator, Params);
}

void AThalassaGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	const int32 Expected = ThalassaTestConfig::ExpectedPlayers();
	if (Expected > 0 && GetNumPlayers() >= Expected)
	{
		// Give the last client time to settle aboard before the deck starts moving.
		GetWorldTimerManager().SetTimer(StartProfileTimer, this, &AThalassaGameMode::StartProfile, 5.f, false);
	}
}

void AThalassaGameMode::StartProfile()
{
	if (Vessel)
	{
		Vessel->StartProfile();
	}
}

AActor* AThalassaGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	return Vessel;
}

APawn* AThalassaGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	if (!Vessel)
	{
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}
	const int32 SpawnIndex = ThalassaTestConfig::IsSpawnTogether() ? 0 : PlayersSpawned++;
	return SpawnDefaultPawnAtTransform(NewPlayer, Vessel->GetSpawnTransform(SpawnIndex));
}
