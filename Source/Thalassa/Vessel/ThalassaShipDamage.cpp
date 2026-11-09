#include "Vessel/ThalassaShipDamage.h"
#include "Vessel/ThalassaWaterSurface.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "Thalassa.h"
#include "Vessel/ThalassaVessel.h"

namespace ThalassaDamage
{
	constexpr int32 SystemCount = static_cast<int32>(EShipSystem::Count);
	constexpr int32 BreachTargetBase = 100;

	constexpr float InflowPerSecond = 0.02f;      // A breach of severity 1 fills its compartment in 50 s.
	constexpr float PumpPerSecond = 0.008f;       // Working pumps drain 0.8% of every flooded compartment a second.
	constexpr float SystemRepairPerSecond = 7.f;  // Percent a second, per person: about 14 s from wrecked to whole.
	constexpr float BreachRepairPerSecond = 0.05f;// Severity a second, per person: 20 s to close the worst breach.
	constexpr float BreachFromSeverity = 0.2f;    // Hits weaker than this dent the hull without breaching it.
	constexpr float SystemDamageRadiusCm = 4000.f;
	constexpr float MergeBreachCm = 500.f;        // A hit this near an existing breach widens it instead.
}

UThalassaShipDamageComponent::UThalassaShipDamageComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UThalassaShipDamageComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UThalassaShipDamageComponent, SystemHealth);
	DOREPLIFETIME(UThalassaShipDamageComponent, Breaches);
	DOREPLIFETIME(UThalassaShipDamageComponent, FloodLevel);
	DOREPLIFETIME(UThalassaShipDamageComponent, Jobs);
	DOREPLIFETIME(UThalassaShipDamageComponent, HitCount);
}

AThalassaVessel* UThalassaShipDamageComponent::GetVessel() const
{
	return Cast<AThalassaVessel>(GetOwner());
}

void UThalassaShipDamageComponent::BeginPlay()
{
	using namespace ThalassaDamage;
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		SystemHealth.Init(100, SystemCount);
		HealthExact.Init(100.f, SystemCount);
		const AThalassaVessel* Vessel = GetVessel();
		FloodLevel.Init(0.f, Vessel ? Vessel->GetCompartmentCount() : 0);
	}
}

const TCHAR* UThalassaShipDamageComponent::SystemName(EShipSystem System)
{
	static const TCHAR* const Names[] = { TEXT("engines"), TEXT("steering"), TEXT("sonar"), TEXT("lights"), TEXT("pumps") };
	return Names[FMath::Clamp(static_cast<int32>(System), 0, ThalassaDamage::SystemCount - 1)];
}

FVector UThalassaShipDamageComponent::SystemSpot(EShipSystem System)
{
	// On the main deck (ThalassaMainDeckLayout.cpp), about a metre above each floor.
	switch (System)
	{
	case EShipSystem::Engines:  return FVector(-11250.f, 0.f, -300.f);     // The engine room's machinery, on its lower floor.
	case EShipSystem::Steering: return FVector(-13400.f, 0.f, -300.f);     // The steering gear, at the stern end of the engine room.
	case EShipSystem::Sonar:    return FVector(11200.f, 900.f, 100.f);     // At the sonar console on the bridge.
	case EShipSystem::Lights:   return FVector(0.f, 2200.f, 100.f);        // The power and light panel, midships, starboard.
	case EShipSystem::Pumps:    return FVector(-7200.f, 1700.f, 100.f);    // The pump room, aft of midships.
	default:                    return FVector::ZeroVector;
	}
}

float UThalassaShipDamageComponent::GetHealth(EShipSystem System) const
{
	const int32 Index = static_cast<int32>(System);
	return SystemHealth.IsValidIndex(Index) ? SystemHealth[Index] / 100.f : 1.f;
}

FString UThalassaShipDamageComponent::CompartmentName(int32 Compartment) const
{
	// The main deck's sections, as ThalassaMainDeckLayout.cpp makes them.
	static const TCHAR* const Names[] = { TEXT("engine room"), TEXT("aft"), TEXT("midships"), TEXT("forward"), TEXT("bridge"), TEXT("lounge") };
	const AThalassaVessel* Vessel = GetVessel();
	if (Vessel && Vessel->GetCompartmentCount() == UE_ARRAY_COUNT(Names) && Compartment >= 0 && Compartment < UE_ARRAY_COUNT(Names))
	{
		return Names[Compartment];
	}
	return FString::Printf(TEXT("compartment %d"), Compartment + 1);
}

void UThalassaShipDamageComponent::ApplyImpact(const FVector& Local, float Severity, const TCHAR* Cause)
{
	using namespace ThalassaDamage;
	AThalassaVessel* Vessel = GetVessel();
	if (!Vessel || !GetOwner()->HasAuthority() || Vessel->GetCompartmentCount() == 0)
	{
		return;
	}
	Severity = FMath::Clamp(Severity, 0.f, 1.f);
	++HitCount;

	// Systems near the hit are damaged, the nearer the worse; the shock always rattles the lights.
	for (int32 Index = 0; Index < SystemCount; ++Index)
	{
		const float Distance = FVector::Dist(SystemSpot(static_cast<EShipSystem>(Index)), Local);
		float Loss = Severity * 70.f * FMath::Max(0.f, 1.f - Distance / SystemDamageRadiusCm);
		if (static_cast<EShipSystem>(Index) == EShipSystem::Lights)
		{
			Loss += Severity * 25.f;
		}
		HealthExact[Index] = FMath::Max(0.f, HealthExact[Index] - Loss);
		SystemHealth[Index] = static_cast<uint8>(FMath::CeilToInt(HealthExact[Index]));
	}

	// The hull: a breach in the nearest compartment, at its wall nearest the hit.
	int32 Nearest = 0;
	float NearestDistance = TNumericLimits<float>::Max();
	FVector NearestPoint = Local;
	for (int32 Index = 0; Index < Vessel->GetCompartmentCount(); ++Index)
	{
		const FShipZone& Zone = Vessel->GetCompartment(Index);
		const FVector P = Zone.Transform.InverseTransformPositionNoScale(Local);
		const FVector Clamped(FMath::Clamp(P.X, -Zone.Extent.X, Zone.Extent.X), FMath::Clamp(P.Y, -Zone.Extent.Y, Zone.Extent.Y), FMath::Clamp(P.Z, -Zone.Extent.Z, Zone.Extent.Z));
		const float Distance = FVector::Dist(P, Clamped);
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = Index;
			NearestPoint = Clamped;
		}
	}
	const FShipZone& Zone = Vessel->GetCompartment(Nearest);
	// Pushed in from the nearest side wall, about a metre above the floor, so someone can reach it.
	FVector Spot = NearestPoint;
	if (Zone.Extent.X - FMath::Abs(Spot.X) < Zone.Extent.Y - FMath::Abs(Spot.Y))
	{
		Spot.X = FMath::Sign(Spot.X) * (Zone.Extent.X - 60.f);
	}
	else
	{
		Spot.Y = FMath::Sign(Spot.Y == 0.f ? 1.f : Spot.Y) * (Zone.Extent.Y - 60.f);
	}
	// A compartment's box starts under its floor plates (about 1.5 m on the main deck), so 2.6 m up from its bottom is
	// about chest height above most floors.
	Spot.Z = -Zone.Extent.Z + 260.f;
	const FVector BreachLocal = Zone.Transform.TransformPositionNoScale(Spot);

	if (Severity >= BreachFromSeverity)
	{
		FShipBreach* Widened = nullptr;
		for (FShipBreach& Breach : Breaches)
		{
			if (Breach.Compartment == Nearest && FVector::Dist(Breach.Local, BreachLocal) < MergeBreachCm)
			{
				Widened = &Breach;
			}
		}
		if (Widened)
		{
			Widened->Severity = FMath::Min(1.f, Widened->Severity + Severity * 0.6f);
		}
		else
		{
			FShipBreach& Breach = Breaches.AddDefaulted_GetRef();
			Breach.Id = NextBreachId++;
			Breach.Compartment = Nearest;
			Breach.Local = BreachLocal;
			Breach.Severity = Severity * 0.9f;
		}
	}
	UE_LOG(LogThalassa, Display, TEXT("DAMAGE: %s, severity %.2f at %s, %s; engines %d steering %d sonar %d lights %d pumps %d; %d breaches"),
		Cause, Severity, *Local.ToString(), *CompartmentName(Nearest), SystemHealth[0], SystemHealth[1], SystemHealth[2], SystemHealth[3], SystemHealth[4], Breaches.Num());
}

void UThalassaShipDamageComponent::ApplyTestImpact()
{
	const AThalassaVessel* Vessel = GetVessel();
	if (!Vessel)
	{
		return;
	}
	// A point on the hull's sides or keel, at random.
	const FBox Hull = Vessel->GetHullBoundsLocal();
	FVector Point(TestRandom.FRandRange(Hull.Min.X, Hull.Max.X), TestRandom.FRandRange(Hull.Min.Y, Hull.Max.Y), TestRandom.FRandRange(Hull.Min.Z, Hull.Max.Z));
	const int32 Face = TestRandom.RandRange(0, 2);
	Point.Y = Face == 0 ? Hull.Min.Y : Face == 1 ? Hull.Max.Y : Point.Y;
	Point.Z = Face == 2 ? Hull.Min.Z : Point.Z;
	ApplyImpact(Point, TestRandom.FRandRange(0.35f, 0.9f), TEXT("test knock"));
}

int32 UThalassaShipDamageComponent::GetRepairTarget(const APawn* Pawn) const
{
	for (const FShipRepairJob& Job : Jobs)
	{
		if (Job.Pawn == Pawn)
		{
			return Job.Target;
		}
	}
	return -1;
}

bool UThalassaShipDamageComponent::TargetSpot(int32 Target, FVector& OutLocal) const
{
	using namespace ThalassaDamage;
	if (Target >= 0 && Target < SystemCount)
	{
		OutLocal = SystemSpot(static_cast<EShipSystem>(Target));
		return true;
	}
	for (const FShipBreach& Breach : Breaches)
	{
		if (Breach.Id + BreachTargetBase == Target)
		{
			OutLocal = Breach.Local;
			return true;
		}
	}
	return false;
}

int32 UThalassaShipDamageComponent::FindRepairTarget(const APawn* Pawn) const
{
	using namespace ThalassaDamage;
	const AThalassaVessel* Vessel = GetVessel();
	if (!Vessel || !Pawn)
	{
		return -1;
	}
	const FVector Local = Vessel->GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
	int32 Best = -1;
	float BestDistance = ReachCm;
	for (int32 Index = 0; Index < SystemCount; ++Index)
	{
		const float Distance = FVector::Dist(SystemSpot(static_cast<EShipSystem>(Index)), Local);
		if (SystemHealth.IsValidIndex(Index) && SystemHealth[Index] < 100 && Distance < BestDistance)
		{
			Best = Index;
			BestDistance = Distance;
		}
	}
	for (const FShipBreach& Breach : Breaches)
	{
		const float Distance = FVector::Dist(FVector(Breach.Local), Local);
		if (Distance < BestDistance)
		{
			Best = Breach.Id + BreachTargetBase;
			BestDistance = Distance;
		}
	}
	return Best;
}

bool UThalassaShipDamageComponent::DescribeTarget(int32 Target, FString& OutName, float& OutDone) const
{
	using namespace ThalassaDamage;
	if (Target >= 0 && Target < SystemCount)
	{
		OutName = SystemName(static_cast<EShipSystem>(Target));
		OutDone = GetHealth(static_cast<EShipSystem>(Target));
		return true;
	}
	for (const FShipBreach& Breach : Breaches)
	{
		if (Breach.Id + BreachTargetBase == Target)
		{
			OutName = FString::Printf(TEXT("breach, %s"), *CompartmentName(Breach.Compartment));
			OutDone = 1.f - Breach.Severity;
			return true;
		}
	}
	return false;
}

bool UThalassaShipDamageComponent::TryStartRepair(APawn* Who)
{
	if (!GetOwner()->HasAuthority() || !Who)
	{
		return false;
	}
	// E again stops repairing.
	for (int32 Index = 0; Index < Jobs.Num(); ++Index)
	{
		if (Jobs[Index].Pawn == Who)
		{
			Jobs.RemoveAt(Index);
			return true;
		}
	}
	const int32 Target = FindRepairTarget(Who);
	if (Target < 0)
	{
		return false;
	}
	Jobs.Add({ Who, Target });
	FString Name;
	float Done = 0.f;
	DescribeTarget(Target, Name, Done);
	UE_LOG(LogThalassa, Display, TEXT("DAMAGE: %s starts repairing %s (%.0f%%)"), *Who->GetName(), *Name, Done * 100.f);
	return true;
}

void UThalassaShipDamageComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner()->HasAuthority())
	{
		TickServer(DeltaTime);
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		TickVisuals(DeltaTime);
	}

	// For tests: the state, every 5 s of the shared clock, on every machine, to compare them.
	if (const AThalassaVessel* Vessel = GetVessel())
	{
		const double Now = Vessel->GetSyncedTime();
		if (HitCount > 0 && FMath::FloorToInt(Now / 5.0) != FMath::FloorToInt((Now - DeltaTime) / 5.0))
		{
			UE_LOG(LogThalassa, Display, TEXT("DAMAGESTATE,%.1f,%08x,%d,%d"), Now, GetFingerprint(), Breaches.Num(), HitCount);
		}
	}
}

void UThalassaShipDamageComponent::TickServer(float DeltaTime)
{
	using namespace ThalassaDamage;
	AThalassaVessel* Vessel = GetVessel();
	if (!Vessel)
	{
		return;
	}

	// Test knocks, on a timer.
	float Every = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("ThalassaTestDamage="), Every) && Every > 0.f)
	{
		TestTimer -= DeltaTime;
		if (TestTimer <= 0.f)
		{
			TestTimer = Every;
			ApplyTestImpact();
		}
	}

	// For tests: -ThalassaAutoRepair makes every player start repairing whatever is in reach, as if they pressed E.
	if (FParse::Param(FCommandLine::Get(), TEXT("ThalassaAutoRepair")))
	{
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			if (GetRepairTarget(*It) < 0 && FindRepairTarget(*It) >= 0)
			{
				TryStartRepair(*It);
			}
		}
	}

	// Repairs: each person on a job moves it on, while they stay within reach and aren't at a station.
	for (int32 Index = Jobs.Num() - 1; Index >= 0; --Index)
	{
		FShipRepairJob& Job = Jobs[Index];
		FVector Spot;
		const bool bValid = IsValid(Job.Pawn) && TargetSpot(Job.Target, Spot) && !Vessel->FindStationOf(Job.Pawn)
			&& FVector::Dist(Vessel->GetActorTransform().InverseTransformPosition(Job.Pawn->GetActorLocation()), Spot) < ReachCm + 100.f;
		if (!bValid)
		{
			Jobs.RemoveAt(Index);
			continue;
		}
		if (Job.Target < SystemCount)
		{
			HealthExact[Job.Target] = FMath::Min(100.f, HealthExact[Job.Target] + SystemRepairPerSecond * DeltaTime);
			SystemHealth[Job.Target] = static_cast<uint8>(FMath::CeilToInt(HealthExact[Job.Target]));
			if (SystemHealth[Job.Target] >= 100)
			{
				UE_LOG(LogThalassa, Display, TEXT("DAMAGE: %s repaired"), SystemName(static_cast<EShipSystem>(Job.Target)));
				Jobs.RemoveAt(Index);
			}
			continue;
		}
		for (int32 B = 0; B < Breaches.Num(); ++B)
		{
			if (Breaches[B].Id + BreachTargetBase == Job.Target)
			{
				Breaches[B].Severity -= BreachRepairPerSecond * DeltaTime;
				if (Breaches[B].Severity <= 0.f)
				{
					UE_LOG(LogThalassa, Display, TEXT("DAMAGE: breach in %s closed"), *CompartmentName(Breaches[B].Compartment));
					Breaches.RemoveAt(B);
				}
				break;
			}
		}
	}
	// Jobs on breaches that closed under someone else's hands go too.
	Jobs.RemoveAll([this](const FShipRepairJob& Job) { FVector Spot; return !TargetSpot(Job.Target, Spot); });

	// Water: in through the breaches, out through the pumps.
	if (FloodLevel.Num() != Vessel->GetCompartmentCount())
	{
		FloodLevel.SetNumZeroed(Vessel->GetCompartmentCount());
	}
	if (FloodExact.Num() != FloodLevel.Num())
	{
		FloodExact.SetNumZeroed(FloodLevel.Num());
	}
	const float Pumping = PumpPerSecond * GetHealth(EShipSystem::Pumps) * DeltaTime;
	for (int32 Index = 0; Index < FloodLevel.Num(); ++Index)
	{
		float Inflow = 0.f;
		for (const FShipBreach& Breach : Breaches)
		{
			Inflow += Breach.Compartment == Index ? Breach.Severity * InflowPerSecond * DeltaTime : 0.f;
		}
		// The exact level stays here; what replicates moves in steps of a tenth of a percent, not every frame's sliver.
		FloodExact[Index] = FMath::Clamp(FloodExact[Index] + Inflow - Pumping, 0.f, 1.f);
		// For looking at the water: -ThalassaFlood=<compartment>,<level> holds one compartment at a level (0 to 1).
		FString Hold;
		if (FParse::Value(FCommandLine::Get(), TEXT("ThalassaFlood="), Hold, false))
		{
			FString Which, Level;
			if (Hold.Split(TEXT(","), &Which, &Level) && FCString::Atoi(*Which) == Index)
			{
				FloodExact[Index] = FMath::Clamp(FCString::Atof(*Level), 0.f, 1.f);
			}
		}
		if (FMath::Abs(FloodExact[Index] - FloodLevel[Index]) >= 0.001f || (FloodExact[Index] == 0.f && FloodLevel[Index] != 0.f))
		{
			FloodLevel[Index] = FloodExact[Index];
		}
	}

}

uint32 UThalassaShipDamageComponent::GetFingerprint() const
{
	uint32 Hash = 0;
	for (uint8 Health : SystemHealth)
	{
		Hash = HashCombine(Hash, Health);
	}
	for (const FShipBreach& Breach : Breaches)
	{
		Hash = HashCombine(Hash, HashCombine(GetTypeHash(Breach.Id), GetTypeHash(FMath::RoundToInt(Breach.Severity * 100.f))));
	}
	for (float Level : FloodLevel)
	{
		Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt(Level * 100.f)));
	}
	return Hash;
}

void UThalassaShipDamageComponent::TickVisuals(float DeltaTime)
{
	AThalassaVessel* Vessel = GetVessel();
	if (!Vessel)
	{
		return;
	}
	USceneComponent* Root = Vessel->GetRootComponent();

	// Water in the compartments: a surface at each one's level that moves and shades like water. Rough while
	// breaches pour into it, and sloshing as the ship moves.
	while (WaterPlanes.Num() < Vessel->GetCompartmentCount())
	{
		UThalassaWaterSurfaceComponent* Plane = NewObject<UThalassaWaterSurfaceComponent>(Vessel);
		Plane->SetupAttachment(Root);
		Plane->RegisterComponent();
		Plane->SetVisibility(false);
		WaterPlanes.Add(Plane);
	}
	for (int32 Index = 0; Index < WaterPlanes.Num(); ++Index)
	{
		const float Level = GetFloodLevel(Index);
		UThalassaWaterSurfaceComponent* Plane = WaterPlanes[Index];
		// Not drawn full to the ceiling either: there's no top to see.
		const bool bShown = Level > 0.003f && Level < 0.995f;
		Plane->SetVisibility(bShown);
		if (bShown)
		{
			const FShipZone& Zone = Vessel->GetCompartment(Index);
			Plane->Build(FVector2D(Zone.Extent.X * 2.f, Zone.Extent.Y * 2.f));
			const FVector Centre = Zone.Transform.TransformPositionNoScale(FVector(0.f, 0.f, -Zone.Extent.Z + Level * 2.f * Zone.Extent.Z));
			Plane->SetRelativeLocationAndRotation(Centre, Zone.Transform.GetRotation());
			float Pouring = 0.f;
			for (const FShipBreach& Breach : Breaches)
			{
				Pouring += Breach.Compartment == Index ? Breach.Severity : 0.f;
			}
			Plane->UpdateWater(DeltaTime, 0.08f + FMath::Clamp(Pouring, 0.f, 1.f) * 0.92f, Level * 2.f * Zone.Extent.Z);
		}
	}

	// Breaches: a jet of water bursting in from the hull, its size by how bad it is.
	while (BreachMarks.Num() < Breaches.Num())
	{
		UStaticMeshComponent* Jet = NewObject<UStaticMeshComponent>(Vessel);
		Jet->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")));
		// Glowing, see-through water (create_damage_materials.py), so a breach shows in the dark red of the alarm.
		Jet->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ship/Damage/M_BreachSpray.M_BreachSpray")));
		Jet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Jet->SetCastShadow(false);
		Jet->SetupAttachment(Root);
		Jet->RegisterComponent();
		BreachMarks.Add(Jet);
	}
	AlarmPhase += DeltaTime;
	for (int32 Index = 0; Index < BreachMarks.Num(); ++Index)
	{
		UStaticMeshComponent* Jet = BreachMarks[Index];
		const bool bShown = Breaches.IsValidIndex(Index);
		Jet->SetVisibility(bShown);
		if (bShown)
		{
			const FShipBreach& Breach = Breaches[Index];
			const FShipZone& Zone = Vessel->GetCompartment(FMath::Clamp(Breach.Compartment, 0, Vessel->GetCompartmentCount() - 1));
			// Pointing inward, from the wall towards the middle of the compartment.
			const FVector Inward = (Zone.Transform.GetLocation() - FVector(Breach.Local)) * FVector(1.f, 1.f, 0.f);
			const FRotator Facing = FRotationMatrix::MakeFromZ(-Inward.GetSafeNormal()).Rotator();
			const float Size = 0.4f + Breach.Severity * (1.2f + 0.25f * FMath::Sin(AlarmPhase * 23.f + Index));
			Jet->SetRelativeLocationAndRotation(FVector(Breach.Local), Facing);
			Jet->SetRelativeScale3D(FVector(Size * 0.6f, Size * 0.6f, Size * 1.6f));
		}
	}

	// Interior lights: red, pulsing, while the hull is breached. The floodlights dim and flicker with the lights' health.
	if (InteriorLamps.Num() == 0)
	{
		TArray<UPointLightComponent*> Lamps;
		Vessel->GetComponents<UPointLightComponent>(Lamps);
		for (UPointLightComponent* Lamp : Lamps)
		{
			if (Lamp && !Lamp->IsA<USpotLightComponent>())
			{
				InteriorLamps.Add(Lamp);
				LampColours.Add(Lamp->GetLightColor());
				LampIntensities.Add(Lamp->Intensity);
			}
		}
		for (const USpotLightComponent* Flood : Vessel->GetFloodlights())
		{
			FloodIntensities.Add(Flood ? Flood->Intensity : 0.f);
		}
	}
	const bool bAlarm = Breaches.Num() > 0;
	const float Pulse = 0.7f + 0.3f * FMath::Sin(AlarmPhase * 4.f); // Never dark: you still have to see to work.
	for (int32 Index = 0; Index < InteriorLamps.Num(); ++Index)
	{
		if (UPointLightComponent* Lamp = InteriorLamps[Index])
		{
			Lamp->SetLightColor(bAlarm ? FLinearColor(1.f, 0.08f, 0.04f) : LampColours[Index]);
			Lamp->SetIntensity(bAlarm ? LampIntensities[Index] * Pulse : LampIntensities[Index]);
		}
	}
	const float Lights = GetHealth(EShipSystem::Lights);
	const TArray<TObjectPtr<USpotLightComponent>>& Floods = Vessel->GetFloodlights();
	for (int32 Index = 0; Index < Floods.Num() && Index < FloodIntensities.Num(); ++Index)
	{
		if (USpotLightComponent* Flood = Floods[Index])
		{
			// Below 60% a lamp stutters, more as it gets worse; at 0 they're out.
			const bool bFlicker = Lights < 0.6f && FMath::FRand() < (0.6f - Lights) * 0.5f;
			Flood->SetIntensity(FloodIntensities[Index] * (bFlicker ? 0.1f : Lights));
		}
	}

	// For checking the drawing: where the first breach's jet and the fullest compartment's water are, now and then.
	if (FParse::Param(FCommandLine::Get(), TEXT("ThalassaDamageDebug")) && FMath::FloorToInt(AlarmPhase / 3.f) != FMath::FloorToInt((AlarmPhase - DeltaTime) / 3.f))
	{
		const UStaticMeshComponent* Jet = BreachMarks.Num() > 0 ? BreachMarks[0].Get() : nullptr;
		int32 Fullest = 0;
		for (int32 Index = 1; Index < WaterPlanes.Num(); ++Index)
		{
			Fullest = GetFloodLevel(Index) > GetFloodLevel(Fullest) ? Index : Fullest;
		}
		const UThalassaWaterSurfaceComponent* Water = WaterPlanes.IsValidIndex(Fullest) ? WaterPlanes[Fullest].Get() : nullptr;
		UE_LOG(LogThalassa, Display, TEXT("DAMAGEDRAW: root %s; jet %s visible %d at %s scale %s mesh %s; water %d visible %d at %s scale %s"),
			*GetNameSafe(Root), *GetNameSafe(Jet), Jet ? Jet->IsVisible() : -1, Jet ? *Jet->GetComponentLocation().ToString() : TEXT("-"),
			Jet ? *Jet->GetComponentScale().ToString() : TEXT("-"), Jet ? *GetNameSafe(Jet->GetStaticMesh()) : TEXT("-"),
			Fullest, Water ? Water->IsVisible() : -1, Water ? *Water->GetComponentLocation().ToString() : TEXT("-"), Water ? *Water->GetComponentScale().ToString() : TEXT("-"));
	}

	// A hit: the thud, and the alarm.
	if (HeardHits >= 0 && HitCount > HeardHits)
	{
		Vessel->PlayDamageAlarm();
	}
	HeardHits = HitCount;
}
