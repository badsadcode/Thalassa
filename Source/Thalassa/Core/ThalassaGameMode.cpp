#include "Core/ThalassaGameMode.h"

#include "Characters/ThalassaCharacter.h"
#include "Core/ThalassaPlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Tools/ThalassaHUD.h"
#include "Tools/ThalassaTestConfig.h"
#include "Vessel/ThalassaVessel.h"

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
	ThalassaTestConfig::GetShipStartOverride(Start);
	Vessel = GetWorld()->SpawnActor<AThalassaVessel>(Start, FRotator::ZeroRotator, Params);
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
