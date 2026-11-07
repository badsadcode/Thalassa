#pragma once

#include "CoreMinimal.h"

/** Command-line switches for the automated motion test. All off in normal play. */
namespace ThalassaTestConfig
{
	/** -ThalassaBot: the local player walks a scripted tour of the ship. */
	bool IsBot();

	/** -ThalassaNoJump: the scripted walker never jumps. */
	bool IsBotJumpDisabled();

	/** -ThalassaNoNudge: crew pass through each other without being nudged apart. */
	bool IsCrewNudgeDisabled();

	/** -ThalassaSpawnTogether: every player spawns at the same spot, to test separation. */
	bool IsSpawnTogether();

	/** -ThalassaGravity=ship: pull characters toward the deck instead of world down. */
	bool UseShipGravity();

	/** -ThalassaStaticShip: run the test with the ship held still, as a control. */
	bool IsShipStatic();

	/** -ThalassaExpectPlayers=N: server starts the motion test once N players are connected. */
	int32 ExpectedPlayers();

	/** -ThalassaTelemetry=path: write a telemetry CSV and exit when the motion test ends. */
	bool GetTelemetryPath(FString& OutPath);

	/** -ThalassaScreenshot=path: save one rendered frame after the level settles, then exit. */
	bool GetScreenshotPath(FString& OutPath);

	/** -ThalassaLookYaw=degrees: initial view direction relative to the ship, for screenshots. */
	float LookYaw();

	/** -ThalassaTimeout=seconds: hard stop for a test process. */
	double TimeoutSeconds();
}
