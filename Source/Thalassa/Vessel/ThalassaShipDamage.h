#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ThalassaShipDamage.generated.h"

class AThalassaVessel;
class UStaticMeshComponent;
class UPointLightComponent;

/** The ship's systems that can be damaged. */
UENUM()
enum class EShipSystem : uint8
{
	Engines,
	Steering,
	Sonar,
	Lights,
	Pumps,
	Count UMETA(Hidden)
};

/** A hole in the hull: which compartment it lets water into, where it is, and how bad. */
USTRUCT()
struct FShipBreach
{
	GENERATED_BODY()

	/** Unique, so a repair keeps hold of the right breach as others open and close. */
	UPROPERTY()
	int32 Id = 0;

	UPROPERTY()
	int32 Compartment = 0;

	/** Where it is, in ship coordinates: at the hull, about chest height above the floor. */
	UPROPERTY()
	FVector_NetQuantize10 Local = FVector::ZeroVector;

	/** 0 to 1: how fast water comes in. Repairs bring it down; at 0 the breach is closed. */
	UPROPERTY()
	float Severity = 0.f;
};

/** Someone repairing something: a system (0 to 4) or a breach (its id plus 100). */
USTRUCT()
struct FShipRepairJob
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<APawn> Pawn;

	UPROPERTY()
	int32 Target = -1;
};

/**
 * Damage and repairs. See docs/HUNT.md, step 1.
 *
 * The server owns all of it: systems' health, breaches, how much water is in each compartment, who is repairing
 * what. All of it replicates, and each machine draws it: water in the compartments, the breaches, red interior
 * lights while the hull is breached, flickering floodlights. The ship reads the systems' health to limit its
 * engines, steering, sonar and pumps. A compartment's water is real water: below its level, players swim.
 */
UCLASS()
class THALASSA_API UThalassaShipDamageComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UThalassaShipDamageComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: something hit the ship here (ship coordinates), from 0 (a scrape) to 1 (as bad as it gets). */
	void ApplyImpact(const FVector& Local, float Severity, const TCHAR* Cause);

	/** Server, test builds: a hit somewhere on the hull, at random. */
	void ApplyTestImpact();

	/** Server: E pressed. Starts repairing whatever is nearest within reach, if anything needs it. */
	bool TryStartRepair(APawn* Who);

	/** A system's health, 0 to 1. */
	float GetHealth(EShipSystem System) const;

	/** How much each system still does, 0 to 1: a little even when wrecked, so the ship can limp home. */
	float GetEngineFactor() const { return FMath::Max(0.12f, GetHealth(EShipSystem::Engines)); }
	float GetSteeringFactor() const { return FMath::Max(0.25f, GetHealth(EShipSystem::Steering)); }
	bool IsSonarWorking() const { return GetHealth(EShipSystem::Sonar) >= 0.3f; }

	/** How full of water a compartment is, 0 to 1 of its height. */
	float GetFloodLevel(int32 Compartment) const { return FloodLevel.IsValidIndex(Compartment) ? FloodLevel[Compartment] : 0.f; }

	const TArray<FShipBreach>& GetBreaches() const { return Breaches; }

	/** What this pawn is repairing (a system 0 to 4, a breach's id plus 100) or -1. */
	int32 GetRepairTarget(const APawn* Pawn) const;

	/** What this pawn could repair from where it stands, as for GetRepairTarget, or -1. */
	int32 FindRepairTarget(const APawn* Pawn) const;

	/** A repair target's name and how far along it is (0 to 1 repaired). False if there is no such target. */
	bool DescribeTarget(int32 Target, FString& OutName, float& OutDone) const;

	/** Compartment names for the panel: the main deck's from bow to stern, or numbers. */
	FString CompartmentName(int32 Compartment) const;

	/** A number that changes when any of the replicated state does, to the percent. For tests. */
	uint32 GetFingerprint() const;

	int32 GetHitCount() const { return HitCount; }

	static const TCHAR* SystemName(EShipSystem System);

	/** Where each system is repaired, in ship coordinates. */
	static FVector SystemSpot(EShipSystem System);

	static constexpr float ReachCm = 350.f;

private:
	AThalassaVessel* GetVessel() const;
	void TickServer(float DeltaTime);
	void TickVisuals(float DeltaTime);
	bool TargetSpot(int32 Target, FVector& OutLocal) const;

	UPROPERTY(Replicated)
	TArray<uint8> SystemHealth;

	UPROPERTY(Replicated)
	TArray<FShipBreach> Breaches;

	UPROPERTY(Replicated)
	TArray<float> FloodLevel;

	UPROPERTY(Replicated)
	TArray<FShipRepairJob> Jobs;

	UPROPERTY(Replicated)
	int32 HitCount = 0;

	int32 NextBreachId = 1;
	float TestTimer = 0.f;
	float LogTimer = 0.f;
	FRandomStream TestRandom{ 20261109 };
	// Fractional repair and damage, kept on the server between whole percents.
	TArray<float> HealthExact;
	TArray<float> FloodExact;

	// Drawing, on every machine.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> WaterPlanes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BreachMarks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> InteriorLamps;
	TArray<FLinearColor> LampColours;
	TArray<float> LampIntensities;
	TArray<float> FloodIntensities;
	float AlarmPhase = 0.f;
	int32 HeardHits = -1;
};
