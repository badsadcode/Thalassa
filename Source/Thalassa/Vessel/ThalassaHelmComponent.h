#pragma once

#include "CoreMinimal.h"
#include "Vessel/ThalassaStationComponent.h"
#include "ThalassaHelmComponent.generated.h"

class AThalassaVessel;

UENUM()
enum class EHelmMode : uint8
{
	Precise,  // The ship answers only while you hold a key. Let go and the levers return to neutral.
	SetAndGo  // Keys move the throttle and planes, and they stay where you leave them.
};

/** The helm's controls as they physically stand: each from -1 to 1. Positive is ahead, starboard, and rise. */
USTRUCT()
struct FHelmLevers
{
	GENERATED_BODY()

	UPROPERTY() float Throttle = 0.f;
	UPROPERTY() float Rudder = 0.f;
	UPROPERTY() float Planes = 0.f;

	/** Vertical thrusters: up positive. They lift or sink the ship level, without tipping it. */
	UPROPERTY() float Heave = 0.f;
};

/**
 * The helm station. Only whoever is sitting at it can steer, and everything they do goes through the
 * levers, which move at a lever's speed and are replicated, so the HUD, the console screen and the
 * physical lever all show the same state to everyone aboard.
 *
 * The server owns all of it. Clients only send what the helmsman is pressing.
 */
UCLASS()
class THALASSA_API UThalassaHelmComponent : public UThalassaStationComponent
{
	GENERATED_BODY()

public:
	UThalassaHelmComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: what the helmsman is pressing, each from -1 to 1. Ignored from anyone else. */
	void SetInput(const APawn* From, float Throttle, float Turn, float Dive, float Heave = 0.f);

	/** Server: switch between precise and set-and-go. */
	void ToggleMode(const APawn* From);

	/**
	 * Server: full stop, from the helmsman. The engines are cut and the ship brakes to a standstill and holds
	 * there, until the helmsman touches a control again.
	 */
	void FullStop(const APawn* From);

	/** Server: the full stop button on the helm console, which anyone within reach can press. */
	bool TryPressFullStopButton(const APawn* Who);

	bool IsFullStop() const { return bFullStop; }

	/** Test builds only: ten times the speed, for crossing the open ocean while testing. Never in a shipped game. */
	void ToggleTestBoost(const APawn* From);
	bool IsTestBoost() const { return bTestBoost; }
	static constexpr float TestBoostFactor = 10.f;
	FVector GetFullStopButtonWorldLocation() const;

	EHelmMode GetMode() const { return Mode; }
	const FHelmLevers& GetLevers() const { return Levers; }

protected:
	virtual void OnOccupantChanged() override;

private:
	UPROPERTY(Replicated)
	EHelmMode Mode = EHelmMode::Precise;

	UPROPERTY(Replicated)
	FHelmLevers Levers;

	UPROPERTY(Replicated)
	bool bFullStop = false;

	UPROPERTY(Replicated)
	bool bTestBoost = false;

	void EngageFullStop();

	// Server only.
	FHelmLevers Input;
	FHelmLevers Commanded;
};
