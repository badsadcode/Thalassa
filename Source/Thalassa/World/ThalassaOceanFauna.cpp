#include "World/ThalassaOceanFauna.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Thalassa.h"
#include "Tools/ThalassaPerf.h"
#include "Vessel/ThalassaSonarComponent.h"
#include "Vessel/ThalassaVessel.h"
#include "World/ThalassaOcean.h"

namespace ThalassaFauna
{
	using EProvince = FThalassaSeabedGenerator::EProvince;

	const TCHAR* const GliderMesh = TEXT("/Game/Creatures/SM_Glider.SM_Glider");

	// How likely a fauna cell is to hold a school of gliders, by province: plains, hills, plateaus, ridges,
	// canyons, volcanic. They like broken ground with room to glide over it, and avoid the volcanic fields.
	constexpr double GliderChance[6] = { 0.30, 0.50, 0.60, 0.40, 0.50, 0.12 };

	constexpr double DrawM = 700.0;           // Schools nearer the camera than this are drawn.
	constexpr float CullCm = 70000.f;
	constexpr double PingSpeedMS = UThalassaSonarComponent::PulseSpeedMS;
	constexpr double KeepClearM = 80.0;       // Schools give the ship this much room.

	/** -ThalassaGlidersAt=X,Y (world centimetres): the start's school lives here, roaming only a little. For screenshots. */
	bool GlidersAt(FVector2D& OutM)
	{
		FString Value;
		if (!FParse::Value(FCommandLine::Get(), TEXT("ThalassaGlidersAt="), Value, false))
		{
			return false;
		}
		TArray<FString> Parts;
		Value.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() < 2)
		{
			return false;
		}
		OutM = FVector2D(FCString::Atod(*Parts[0]), FCString::Atod(*Parts[1])) / 100.0;
		return true;
	}

	FORCEINLINE uint64 FaunaHash(uint64 X)
	{
		X += 0x9E3779B97F4A7C15ull;
		X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
		X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
		return X ^ (X >> 31);
	}

	/** A stream of uniform numbers from one cell's hash. */
	struct FFaunaDraws
	{
		uint64 State;
		explicit FFaunaDraws(uint64 H) : State(H) {}
		double Next() { State = FaunaHash(State); return static_cast<double>(State >> 11) * (1.0 / 9007199254740992.0); }
		double Range(double Low, double High) { return Low + (High - Low) * Next(); }
	};

	FORCEINLINE double FaunaSmooth(double Edge0, double Edge1, double X)
	{
		const double T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	/** The fauna cell a point is in. Cells are centred on their coordinates, so the start sits in the middle of cell (0, 0). */
	FORCEINLINE int64 FaunaCellOf(double M) { return static_cast<int64>(FMath::FloorToDouble(M / UThalassaOceanFauna::CellM + 0.5)); }

	/** Where a school's wandering takes it, in metres, and how fast it is going and turning. */
	void Wander(const FFaunaSchool& S, double T, FVector2D& OutP, FVector2D& OutV, FVector2D& OutA)
	{
		// Two loops added together: a big slow one round home and a smaller, quicker one, so the path never
		// quite repeats and has bends in it. The big loop is run at the school's cruising speed.
		const double R1 = S.RoamM;
		const double R2 = S.RoamM * 0.35;
		const double W1 = S.SpeedMS / R1 * (S.Phase[3] < 0.5 ? 1.0 : -1.0);
		const double W2 = W1 * 2.3;
		const double W3 = W1 * 1.7;
		const double A1 = S.Phase[0] + W1 * T, A2 = S.Phase[1] + W2 * T, A3 = S.Phase[2] + W3 * T;
		OutP = S.HomeM + FVector2D(R1 * FMath::Cos(A1) + R2 * FMath::Cos(A2), R1 * FMath::Sin(A1) + R2 * FMath::Sin(A3));
		OutV = FVector2D(-R1 * W1 * FMath::Sin(A1) - R2 * W2 * FMath::Sin(A2), R1 * W1 * FMath::Cos(A1) + R2 * W3 * FMath::Cos(A3));
		OutA = FVector2D(-R1 * W1 * W1 * FMath::Cos(A1) - R2 * W2 * W2 * FMath::Cos(A2), -R1 * W1 * W1 * FMath::Sin(A1) - R2 * W3 * W3 * FMath::Sin(A3));
	}
}

UThalassaOceanFauna::UThalassaOceanFauna()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // After the ship has moved this frame.
}

void UThalassaOceanFauna::BeginPlay()
{
	using namespace ThalassaFauna;
	Super::BeginPlay();

	bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ThalassaNoFauna")) || GetNetMode() == NM_DedicatedServer;
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, GliderMesh);
	if (!Mesh)
	{
		UE_LOG(LogThalassa, Warning, TEXT("THALASSA: ocean fauna has no mesh at %s"), GliderMesh);
		bDisabled = true;
	}
	if (!bDisabled)
	{
		Gliders = NewObject<UInstancedStaticMeshComponent>(GetOwner(), TEXT("Gliders"));
		Gliders->SetStaticMesh(Mesh);
		Gliders->SetMobility(EComponentMobility::Movable);
		Gliders->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Gliders->SetCastShadow(false);
		Gliders->SetCullDistances(0, CullCm);
		Gliders->NumCustomDataFloats = 3;
		Gliders->SetupAttachment(GetOwner()->GetRootComponent());
		Gliders->SetAbsolute(true, true, true);
		Gliders->RegisterComponent();
	}
	UE_LOG(LogThalassa, Display, TEXT("THALASSA: ocean fauna %s"), bDisabled ? TEXT("off") : TEXT("on: gliders"));
}

double UThalassaOceanFauna::Now() const
{
	if (const AThalassaVessel* Ship = Vessel.Get())
	{
		return Ship->GetSyncedTime();
	}
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UThalassaOceanFauna::NotePings()
{
	const AThalassaVessel* Ship = Vessel.Get();
	const UThalassaSonarComponent* Sonar = Ship ? Ship->GetSonar() : nullptr;
	if (!Sonar)
	{
		return;
	}
	const FSonarPulse& Pulse = Sonar->GetLastPulse();
	if (Pulse.Serial == 0 || (Pulse.Serial == (Pings.Num() > 0 ? Pings.Last().Serial : 0)))
	{
		return;
	}
	Pings.Add({ Pulse.Origin, Pulse.StartTime, Pulse.RangeM, Pulse.Serial });
	// A school settles within 45 s of a ping; pings come no closer than 8 s apart.
	while (Pings.Num() > 6)
	{
		Pings.RemoveAt(0);
	}
}

bool UThalassaOceanFauna::MakeSchool(const AThalassaOcean& Ocean, int64 CellX, int64 CellY, FFaunaSchool& OutSchool)
{
	using namespace ThalassaFauna;
	const uint64 Seed = static_cast<uint64>(Ocean.Seed);
	FFaunaDraws Draw(FaunaHash(FaunaHash(static_cast<uint64>(CellX) ^ (Seed * 0x9FB21C651E98DF25ull) ^ 0xF15Bull) ^ (static_cast<uint64>(CellY) * 0xC2B2AE3D27D4EB4Full)));

	OutSchool = FFaunaSchool();
	OutSchool.Key = { CellX, CellY };
	const bool bStart = CellX == 0 && CellY == 0; // A school always lives near the start, so there is something to meet.
	const double Jitter = bStart ? 100.0 : CellM * 0.25;
	OutSchool.HomeM = FVector2D(CellX * CellM + Draw.Range(-Jitter, Jitter), CellY * CellM + Draw.Range(-Jitter, Jitter));

	EProvince Neighbour;
	double Weight = 1.0;
	const EProvince Province = Ocean.GetGenerator().ProvinceAt(OutSchool.HomeM.X, OutSchool.HomeM.Y, Neighbour, Weight);
	if (!bStart && Draw.Next() >= GliderChance[static_cast<int32>(Province)])
	{
		return false;
	}

	OutSchool.AltitudeM = Draw.Range(25.0, 55.0);
	OutSchool.RoamM = Draw.Range(120.0, 220.0);
	OutSchool.SpeedMS = Draw.Range(1.2, 2.2);
	for (double& Phase : OutSchool.Phase)
	{
		Phase = Draw.Range(0.0, UE_DOUBLE_TWO_PI);
	}
	OutSchool.Phase[3] = Draw.Next();
	FVector2D Placed;
	if (bStart && GlidersAt(Placed))
	{
		OutSchool.HomeM = Placed;
		OutSchool.RoamM = 15.0;
		OutSchool.SpeedMS = 0.4;
	}

	// The animals: loosely spread through a flattened oval, never nearer each other than their own size.
	const int32 Count = 5 + static_cast<int32>(Draw.Next() * 7.0);
	const double Spread = FMath::Sqrt(static_cast<double>(Count));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FFaunaSchool::FMember Member;
		for (int32 Try = 0; Try < 20; ++Try)
		{
			Member.OffsetM = FVector(Draw.Range(-6.0, 6.0) * Spread, Draw.Range(-8.0, 8.0) * Spread, Draw.Range(-3.0, 3.0) * Spread * 0.6);
			bool bClear = true;
			for (const FFaunaSchool::FMember& Other : OutSchool.Members)
			{
				bClear &= FVector::Dist(Other.OffsetM, Member.OffsetM) > 9.0;
			}
			if (bClear)
			{
				break;
			}
		}
		Member.Scale = Draw.Range(0.8, 1.35);
		for (double& Drift : Member.Drift)
		{
			Drift = Draw.Range(0.0, UE_DOUBLE_TWO_PI);
		}
		Member.Beat = static_cast<float>(Draw.Range(0.0, UE_DOUBLE_TWO_PI));
		OutSchool.Members.Add(Member);
	}
	return true;
}

FFaunaSchoolState UThalassaOceanFauna::SchoolAt(const AThalassaOcean& Ocean, const FFaunaSchool& School, double Time,
	TConstArrayView<FFaunaPing> Pings, const FVector* ShipLocation)
{
	using namespace ThalassaFauna;
	FFaunaSchoolState State;

	FVector2D P, V, A;
	Wander(School, Time, P, V, A);
	const double Speed2 = FMath::Max(V.SizeSquared(), 1.0e-6);
	State.YawDeg = FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
	State.TurnDegS = FMath::RadiansToDegrees((V.X * A.Y - V.Y * A.X) / Speed2);

	// Glide at its height over the ground, ignoring the ground's smallest bumps, with a slow rise and fall.
	const auto GroundM = [&Ocean](const FVector2D& At) { return Ocean.GroundZ(FVector(At.X * 100.0, At.Y * 100.0, 0.0), 60.0) / 100.0; };
	const double Bob = 4.0 * FMath::Sin(Time * 0.05 + School.Phase[2]);
	const FVector2D Ahead = P + V.GetSafeNormal() * 25.0;
	const double Z = GroundM(P) + School.AltitudeM + Bob;
	const double ZAhead = GroundM(Ahead) + School.AltitudeM;
	State.PitchDeg = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ZAhead - Z, 25.0)) * 0.5, -12.0, 12.0);
	FVector Centre(P.X, P.Y, FMath::Min(Z, -15.0));

	// A ping: when its wave reaches the school, the school bolts away from where it came from, its glow flashes,
	// it spreads out, and over the next half minute it settles and drifts back to its path. Pings close together
	// add up: a school pinged again while still fleeing bolts further.
	const FVector Calm = Centre;
	double Latest = -1.0;
	for (const FFaunaPing& Ping : Pings)
	{
		if (Ping.RangeM <= 0.f)
		{
			continue;
		}
		const FVector OriginM = Ping.Origin / 100.0;
		const double DistanceM = FVector::Dist(OriginM, Calm);
		const double Age = Time - (Ping.Time + DistanceM / PingSpeedMS);
		if (DistanceM > Ping.RangeM || Age < 0.0 || Age >= 45.0)
		{
			continue;
		}
		FVector2D Away = FVector2D(Calm.X - OriginM.X, Calm.Y - OriginM.Y).GetSafeNormal();
		if (Away.IsNearlyZero())
		{
			Away = FVector2D(1.0, 0.0);
		}
		const double Bolt = 70.0 * (1.0 - FMath::Exp(-Age / 7.0)) * (1.0 - FaunaSmooth(14.0, 45.0, Age));
		Centre += FVector(Away * Bolt, 0.0);
		State.Spread = FMath::Max(State.Spread, (1.0 - FMath::Exp(-Age / 1.5)) * (1.0 - FaunaSmooth(8.0, 35.0, Age)));
		State.Flash = FMath::Max(State.Flash, FMath::Exp(-Age / 1.2));
		State.Rate = FMath::Max(State.Rate, 1.0 + 2.0 * FMath::Exp(-Age / 7.0));
		if (Ping.Time > Latest)
		{
			Latest = Ping.Time;
			State.FleeYawDeg = FMath::RadiansToDegrees(FMath::Atan2(Away.Y, Away.X));
			State.Fleeing = (1.0 - FMath::Exp(-Age)) * (1.0 - FaunaSmooth(10.0, 25.0, Age));
		}
	}

	// Give the ship room: a school it comes near drifts aside.
	if (ShipLocation)
	{
		const FVector ShipM = *ShipLocation / 100.0;
		const FVector Offset = Centre - ShipM;
		const double Distance = Offset.Size();
		if (Distance < KeepClearM)
		{
			Centre += (Distance > 1.0 ? Offset / Distance : FVector(0.0, 0.0, -1.0)) * (KeepClearM - Distance);
		}
	}

	State.CentreCm = Centre * 100.0;
	return State;
}

const FFaunaSchool* UThalassaOceanFauna::FindSchool(int64 CellX, int64 CellY)
{
	const FFaunaSchoolKey Key{ CellX, CellY };
	if (const FFaunaSchool* Found = Schools.Find(Key))
	{
		return Found->Members.Num() > 0 ? Found : nullptr;
	}
	const AThalassaOcean* Ocean = Cast<AThalassaOcean>(GetOwner());
	FFaunaSchool School;
	if (!Ocean || !MakeSchool(*Ocean, CellX, CellY, School))
	{
		School = FFaunaSchool();
		School.Key = Key;
	}
	const FFaunaSchool& Added = Schools.Add(Key, MoveTemp(School));
	return Added.Members.Num() > 0 ? &Added : nullptr;
}

void UThalassaOceanFauna::GatherSonarTargets(const FVector& Origin, float RangeCm, double Time, TArray<FThalassaSonarTarget>& OutTargets)
{
	using namespace ThalassaFauna;
	const AThalassaOcean* Ocean = Cast<AThalassaOcean>(GetOwner());
	if (!Ocean)
	{
		return;
	}
	if (!Vessel.IsValid())
	{
		Vessel = AThalassaVessel::Find(GetWorld());
	}
	NotePings();
	// The ship was where the ping went out. Its replicated position differs a little between machines, so it is
	// not used here: every machine must find the same contacts.
	const FVector ShipLocation = Origin;

	// A school can roam this far from its cell's middle.
	const int32 Reach = FMath::CeilToInt((RangeCm / 100.0 + 400.0) / CellM);
	const int64 CX = FaunaCellOf(Origin.X / 100.0), CY = FaunaCellOf(Origin.Y / 100.0);
	for (int64 Y = CY - Reach; Y <= CY + Reach; ++Y)
	{
		for (int64 X = CX - Reach; X <= CX + Reach; ++X)
		{
			const FFaunaSchool* School = FindSchool(X, Y);
			if (!School)
			{
				continue;
			}
			const FFaunaSchoolState State = SchoolAt(*Ocean, *School, Time, Pings, &ShipLocation);
			if (FVector::Dist(State.CentreCm, Origin) > RangeCm + 6000.0)
			{
				continue;
			}
			const FRotator Facing(0.0, State.YawDeg, 0.0);
			for (const FFaunaSchool::FMember& Member : School->Members)
			{
				const FVector Location = State.CentreCm + Facing.RotateVector(Member.OffsetM * 100.0);
				if (FVector::Dist(Location, Origin) <= RangeCm)
				{
					OutTargets.Add({ Location, static_cast<float>(250.0 * Member.Scale) });
				}
			}
		}
	}
}

void UThalassaOceanFauna::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	using namespace ThalassaFauna;
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	THALASSA_PERF_SCOPE("fauna");

	const AThalassaOcean* Ocean = Cast<AThalassaOcean>(GetOwner());
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (bDisabled || !Ocean || !Gliders || !Camera)
	{
		return;
	}
	if (!Vessel.IsValid())
	{
		Vessel = AThalassaVessel::Find(GetWorld());
	}

	const double Time = Now();
	NotePings();
	const FVector ShipLocation = Vessel.IsValid() ? Vessel->GetActorLocation() : FVector::ZeroVector;
	const FVector* Ship = Vessel.IsValid() ? &ShipLocation : nullptr;

	// Every school near the camera, in a fixed order, so each animal keeps its instance while the set is unchanged.
	const FVector Eye = Camera->GetCameraLocation();
	const int64 CX = FaunaCellOf(Eye.X / 100.0), CY = FaunaCellOf(Eye.Y / 100.0);
	const int32 Reach = FMath::CeilToInt((DrawM + 400.0) / CellM);
	TArray<FFaunaSchoolKey> Drawn;
	Transforms.Reset();
	CustomData.Reset();
	for (int64 Y = CY - Reach; Y <= CY + Reach; ++Y)
	{
		for (int64 X = CX - Reach; X <= CX + Reach; ++X)
		{
			const FFaunaSchool* School = FindSchool(X, Y);
			if (!School)
			{
				continue;
			}
			const FFaunaSchoolState State = SchoolAt(*Ocean, *School, Time, Pings, Ship);
			if (FVector::Dist(State.CentreCm, Eye) > DrawM * 100.0)
			{
				continue;
			}
			Drawn.Add(School->Key);
			if (!Visible.Contains(School->Key))
			{
				UE_LOG(LogThalassa, Display, TEXT("Fauna: school (%lld, %lld) of %d in view at %s, %.0f m from the camera"), School->Key.X, School->Key.Y,
					School->Members.Num(), *(State.CentreCm / 100.0).ToString(), FVector::Dist(State.CentreCm, Eye) / 100.0);
			}

			const FRotator Facing(0.0, State.YawDeg, 0.0);
			const double GroundCm = Ocean->GroundZ(State.CentreCm, 60.0);
			const double Bank = FMath::Clamp(-State.TurnDegS * 6.0, -25.0, 25.0);
			for (const FFaunaSchool::FMember& Member : School->Members)
			{
				// Its place in the school, opening out when startled, with a slow drift of its own.
				const FVector Drift(3.0 * FMath::Sin(Time * 0.07 + Member.Drift[0]), 3.0 * FMath::Sin(Time * 0.05 + Member.Drift[1]), 1.5 * FMath::Sin(Time * 0.09 + Member.Drift[2]));
				const FVector Offset = Member.OffsetM * (1.0 + 1.4 * State.Spread) + Drift;
				FVector Location = State.CentreCm + Facing.RotateVector(Offset * 100.0);
				Location.Z = FMath::Max(Location.Z, GroundCm + 800.0);

				// Facing along the school's path, or away from the ping while fleeing, each a little its own way.
				double Yaw = State.YawDeg + 6.0 * FMath::Sin(Time * 0.11 + Member.Drift[0]);
				if (State.Fleeing > 0.0)
				{
					Yaw = Yaw + FMath::FindDeltaAngleDegrees(Yaw, State.FleeYawDeg + 20.0 * FMath::Sin(Member.Drift[1])) * State.Fleeing;
				}
				const double Pitch = State.PitchDeg + 3.0 * FMath::Sin(Time * 0.13 + Member.Drift[2]);
				const double Roll = Bank + 4.0 * FMath::Sin(Time * 0.17 + Member.Drift[1]);
				Transforms.Add(FTransform(FRotator(Pitch, Yaw, Roll), Location, FVector(Member.Scale)));
				CustomData.Add(Member.Beat);
				CustomData.Add(static_cast<float>(State.Rate));
				CustomData.Add(static_cast<float>(State.Flash));
			}
		}
	}

	// Draw them: the same set moves its instances; a changed set is laid out afresh.
	if (Drawn != Visible || Transforms.Num() != Gliders->GetInstanceCount())
	{
		Gliders->ClearInstances();
		Gliders->AddInstances(Transforms, false, true);
		if (Drawn.Num() != Visible.Num())
		{
			UE_LOG(LogThalassa, Display, TEXT("Fauna: drawing %d schools, %d animals"), Drawn.Num(), Transforms.Num());
		}
		Visible = MoveTemp(Drawn);
	}
	else if (Transforms.Num() > 0)
	{
		Gliders->BatchUpdateInstancesTransforms(0, Transforms, true, false, true);
	}
	for (int32 Index = 0; Index < Transforms.Num(); ++Index)
	{
		Gliders->SetCustomData(Index, TArrayView<const float>(&CustomData[Index * 3], 3), false);
	}
	if (Transforms.Num() > 0)
	{
		Gliders->MarkRenderStateDirty();
	}
	VisibleAnimals = Transforms.Num();
}
