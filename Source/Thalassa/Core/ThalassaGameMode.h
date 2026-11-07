#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ThalassaGameMode.generated.h"

class AThalassaVessel;

/** Spawns the vessel, puts every player aboard it, and starts the motion test when configured. */
UCLASS(Config = Game)
class THALASSA_API AThalassaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AThalassaGameMode();

	virtual void InitGameState() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

private:
	void StartProfile();

	/**
	 * The ship to spawn: the ship Blueprint the art is built in (DefaultGame.ini). If it is missing or
	 * fails to load, the plain C++ ship is used instead, and the log says so.
	 */
	UPROPERTY(Config)
	TSoftClassPtr<AThalassaVessel> VesselClass;

	UPROPERTY(Transient)
	TObjectPtr<AThalassaVessel> Vessel;

	int32 PlayersSpawned = 0;
	FTimerHandle StartProfileTimer;
};
