#pragma once

#include "CoreMinimal.h"

/** Command-line switches for the automated motion test. All off in normal play. */
namespace ThalassaTestConfig
{
	/** -ThalassaBot: the local player walks a scripted tour of the ship. */
	bool IsBot();

	/** -ThalassaDiver: the local player repeatedly goes out through the airlock and back. */
	bool IsDiverBot();

	/** -ThalassaEvaProfile: server drives the ship slowly enough for a swimmer to catch. */
	bool IsEvaProfile();

	/** -ThalassaSpawnAtAirlock: start in the corridor facing the airlock, for screenshots. */
	bool IsSpawnAtAirlock();

	/**
	 * -ThalassaHullStretch=N: stretch the hull lengthwise, to test how a longer ship behaves.
	 * For walkers only: it also stretches the airlock chamber, so its doors no longer fit.
	 */
	float HullStretch();

	/** -ThalassaTestRig: use the original three-compartment rig instead of the real deck. */
	bool UseTestRig();

	/** -ThalassaSpawnAt=X,Y,Z: start at this point in ship coordinates, for screenshots. */
	bool GetSpawnOverride(FVector& OutLocation);

	/** -ThalassaWalkStraight: the local player holds forward and logs where they go. */
	bool IsWalkStraight();

	/** -ThalassaSonar: the motion test runs with the sonar surveying. */
	bool IsSonarOnForTest();

	/** -ThalassaSurveySlot=name: save and load the sonar chart in this slot during a test. */
	bool GetSurveySaveSlot(FString& OutSlot);

	/** -ThalassaHelmTest: the timed test runs with the ship under helm control instead of the script. */
	bool IsHelmTest();

	/** -ThalassaHelmBot: the local player walks to the helm, sits, and drives a scripted course. */
	bool IsHelmBot();

	/** -ThalassaShipAt=X,Y,Z: start the ship here, in world centimetres, instead of 200 m down at the origin. */
	bool GetShipStartOverride(FVector& OutLocation);

	/** -ThalassaHelmCourse=name: which course the scripted helmsman drives: tour (default), dive or rise. */
	FString HelmCourse();

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
