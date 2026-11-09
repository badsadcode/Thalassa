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

	// The meshes, by index. Species and the leviathan's pieces name their mesh by this index.
	const TCHAR* const MeshPaths[] =
	{
		TEXT("/Game/Creatures/SM_Glider.SM_Glider"),
		TEXT("/Game/Creatures/SM_Fish_Dart.SM_Fish_Dart"),
		TEXT("/Game/Creatures/SM_Fish_Disc.SM_Fish_Disc"),
		TEXT("/Game/Creatures/SM_Fish_Lantern.SM_Fish_Lantern"),
		TEXT("/Game/Creatures/SM_Fish_Grouper.SM_Fish_Grouper"),
		TEXT("/Game/Creatures/SM_Fish_Eel.SM_Fish_Eel"),
		TEXT("/Game/Creatures/SM_Fish_Cruiser.SM_Fish_Cruiser"),
		TEXT("/Game/Creatures/SM_Jelly_Giant.SM_Jelly_Giant"),
		TEXT("/Game/Creatures/SM_Jelly_Glow.SM_Jelly_Glow"),
		TEXT("/Game/Creatures/SM_Jelly_Mote.SM_Jelly_Mote"),
		TEXT("/Game/Creatures/SM_Leviathan_Head.SM_Leviathan_Head"),
		TEXT("/Game/Creatures/SM_Leviathan_Body.SM_Leviathan_Body"),
		TEXT("/Game/Creatures/SM_Leviathan_Tail.SM_Leviathan_Tail"),
		TEXT("/Game/Creatures/SM_Jelly_Helmet.SM_Jelly_Helmet"),
		TEXT("/Game/Creatures/SM_Jelly_Comb.SM_Jelly_Comb"),
		TEXT("/Game/Creatures/SM_Kraken.SM_Kraken"),
	};
	constexpr int32 MeshCount = UE_ARRAY_COUNT(MeshPaths);
	constexpr int32 LeviathanHead = 10;
	constexpr int32 CustomFloats = 7;   // Per instance: phase, rate, flash, tint (3), glow. See import_creatures.py.

	enum class EKind : uint8 { Fish, Glider, Jelly, Kraken };

	// Colours a school can be: the tint, and in alpha how much it glows. Each school picks one from its kind's list.
	const FLinearColor GlowPalette[] =
	{
		{ 0.25f, 0.55f, 1.0f, 1.f },  // Blue.
		{ 0.62f, 0.30f, 1.0f, 1.f },  // Violet.
		{ 0.30f, 1.0f, 0.50f, 1.f },  // Green.
		{ 1.0f, 0.62f, 0.20f, 1.f },  // Amber.
		{ 1.0f, 0.35f, 0.70f, 1.f },  // Pink.
		{ 0.20f, 0.95f, 0.90f, 1.f },  // Cyan.
	};
	const FLinearColor GiantPalette[] =
	{
		{ 0.92f, 0.86f, 0.82f, 0.f },  // Pale, not glowing.
		{ 0.70f, 0.78f, 0.86f, 0.f },  // Nearly clear.
		{ 0.55f, 0.06f, 0.05f, 0.f },  // Deep red: in the deep, red light is gone, so red is dark.
		{ 0.92f, 0.86f, 0.82f, 0.4f }, // Pale, its rim faintly glowing.
	};
	const FLinearColor HelmetPalette[] =
	{
		{ 0.55f, 0.06f, 0.05f, 0.f },  // Deep red.
		{ 0.32f, 0.14f, 0.28f, 0.f },  // Dusky purple.
		{ 1.0f, 0.62f, 0.20f, 1.f },   // A glowing amber ring.
		{ 0.62f, 0.30f, 1.0f, 1.f },   // A glowing violet ring.
	};

	/** A kind of animal: where it lives, how many to a school, how it moves, how far away it is drawn. */
	struct FSpecies
	{
		const TCHAR* Name;
		int32 Mesh;
		EKind Kind;
		double CellM;            // One school at most per cell of this size.
		double Chance[6];        // How likely a cell holds a school, by province: plains, hills, plateaus, ridges, canyons, volcanic.
		int32 CountMin, CountMax;
		double SpacingM;         // Animals keep at least this far apart (small schools only).
		double SpreadM;          // The school's size, times the square root of its count.
		double AltMin, AltMax;   // Height above the ground, metres.
		double RoamMin, RoamMax; // How far it wanders from home.
		double SpeedMin, SpeedMax;
		double ScaleMin, ScaleMax;
		double DrawM;            // Drawn when nearer the camera than this.
		double KeepClearM;       // Room it gives the ship.
		double BoltM;            // How far a ping makes it bolt.
		float SonarRadiusCm;     // How big a contact each animal makes; 0: too small for the ping to show.
		double SizeM;            // Its mesh's size: big animals stroke slowly.
		const FLinearColor* Palette = nullptr;   // Colours its schools can be; none: as modelled, glowing as modelled.
		int32 PaletteCount = 0;
	};

	// The species, by index; the index is also its mesh's. Small schooling fish in their hundreds near the camera;
	// bigger fish near the ground; big, slow cruisers and gliders; jellyfish drifting, some glowing.
	const FSpecies Species[] =
	{
		//  Name           Mesh Kind            Cell   Plains Hills Plat. Ridge Canyon Volc.    Count     Spacing Spread  Altitude   Roam      Speed        Scale        Draw  Clear Bolt  Sonar   Size  Palette
		{ TEXT("glider"),      0, EKind::Glider, 600, { 0.30, 0.50, 0.60, 0.40, 0.50, 0.12 },  5,  11,   9.0,  8.0,  25, 55,   120, 220,  1.2, 2.2,    0.8, 1.35,  700,  80,  70, 250.f,  5.0 },
		{ TEXT("dart"),        1, EKind::Fish,    90, { 0.45, 0.50, 0.50, 0.55, 0.50, 0.30 }, 30,  80,   0.5,  1.0,   3, 15,    25,  50,  0.8, 1.4,    0.8, 1.2,   110,  20,  25,   0.f,  0.3 },
		{ TEXT("disc"),        2, EKind::Fish,   140, { 0.20, 0.40, 0.45, 0.40, 0.35, 0.15 }, 15,  40,   0.7,  1.2,   2,  8,    20,  40,  0.6, 1.0,    0.85, 1.15, 110,  15,  20,   0.f,  0.42 },
		{ TEXT("lantern"),     3, EKind::Fish,   110, { 0.30, 0.35, 0.35, 0.45, 0.55, 0.60 }, 20,  60,   0.45, 0.9,   8, 40,    30,  60,  0.5, 0.9,    0.8, 1.2,   160,  15,  20,   0.f,  0.2 },
		{ TEXT("grouper"),     4, EKind::Fish,   160, { 0.15, 0.35, 0.35, 0.40, 0.35, 0.20 },  2,   6,   2.5,  3.0, 0.8,  3,    25,  50,  0.35, 0.7,   0.8, 1.3,   150,  25,  15,   0.f,  1.4 },
		{ TEXT("eel"),         5, EKind::Fish,   200, { 0.05, 0.20, 0.20, 0.35, 0.45, 0.30 },  1,   3,   3.0,  3.0, 0.4, 1.5,   15,  40,  0.35, 0.6,   0.8, 1.3,   110,  15,  10,   0.f,  2.2 },
		{ TEXT("cruiser"),     6, EKind::Fish,   500, { 0.15, 0.25, 0.25, 0.25, 0.30, 0.10 },  1,   3,  14.0, 12.0,  15, 60,   150, 300,  1.0, 1.8,    0.8, 1.3,   450,  60,  60, 350.f,  7.0 },
		{ TEXT("giant jelly"), 7, EKind::Jelly,  400, { 0.25, 0.20, 0.20, 0.20, 0.30, 0.15 },  1,   3,  25.0, 20.0,  25, 90,    20,  50,  0.12, 0.25,  0.8, 1.3,   400,  40,   8, 300.f,  6.0, GiantPalette, UE_ARRAY_COUNT(GiantPalette) },
		{ TEXT("glow jelly"),  8, EKind::Jelly,  250, { 0.25, 0.25, 0.25, 0.30, 0.40, 0.45 },  3,   9,   7.0,  7.0,  12, 60,    15,  40,  0.08, 0.2,   0.8, 1.3,   350,  25,   6,   0.f,  2.2, GlowPalette, UE_ARRAY_COUNT(GlowPalette) },
		{ TEXT("mote"),        9, EKind::Jelly,  150, { 0.20, 0.20, 0.20, 0.25, 0.35, 0.40 }, 40, 110,   1.0,  2.5,   5, 50,    10,  25,  0.05, 0.12,  0.7, 1.4,   160,  10,   4,   0.f,  0.3, GlowPalette, UE_ARRAY_COUNT(GlowPalette) },
		{ TEXT("helmet jelly"), 13, EKind::Jelly, 300, { 0.15, 0.20, 0.20, 0.30, 0.40, 0.40 },  4,  14,   3.0,  3.5,  10, 70,    15,  35,  0.08, 0.18,  0.8, 1.3,   300,  20,   5,   0.f,  1.6, HelmetPalette, UE_ARRAY_COUNT(HelmetPalette) },
		{ TEXT("comb jelly"),  14, EKind::Jelly, 200, { 0.25, 0.25, 0.25, 0.25, 0.35, 0.35 },  6,  20,   2.0,  2.5,   5, 40,    10,  30,  0.06, 0.15,  0.8, 1.4,   220,  15,   4,   0.f,  1.2, GlowPalette, UE_ARRAY_COUNT(GlowPalette) },
		{ TEXT("kraken"),      15, EKind::Kraken, 2500, { 0.04, 0.12, 0.12, 0.30, 0.45, 0.30 }, 1,  1,  80.0,  0.0,  50, 140,  400, 800,  2.5, 4.5,    0.9, 1.5,  2200, 150, 250, 1500.f, 60.0 },
	};
	constexpr int32 SpeciesCount = UE_ARRAY_COUNT(Species);

	constexpr double PingSpeedMS = UThalassaSonarComponent::PulseSpeedMS;

	// The leviathan. One, near the start: a long body of segments following its head's path.
	constexpr int32 LeviathanSegments = 16;
	constexpr double LeviathanSampleS = 0.5;        // The path behind the head, sampled every half second.
	constexpr double LeviathanHearsM = 2200.0;      // It comes to a ping heard from this far.
	constexpr double LeviathanScale = 2.6;          // Its meshes are modelled at about 155 m long; it is drawn at about 400 m, longer than the ship.
	constexpr double LeviathanCircleM = 450.0;      // ...and circles this far from where it came from.
	constexpr double LeviathanDrawM = 2500.0;

	FORCEINLINE uint64 FaunaHash(uint64 X)
	{
		X += 0x9E3779B97F4A7C15ull;
		X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
		X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
		return X ^ (X >> 31);
	}

	/** A stream of uniform numbers from one hash. */
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

	/** The cell a point is in, on a lattice. Cells are centred on their coordinates: the start is the middle of cell (0, 0). */
	FORCEINLINE int64 FaunaCellOf(double M, double CellM) { return static_cast<int64>(FMath::FloorToDouble(M / CellM + 0.5)); }

	/** A test switch of the form -Name=X,Y in world centimetres, returned in metres. */
	bool SwitchAt(const TCHAR* Name, FVector2D& OutM)
	{
		FString Value;
		if (!FParse::Value(FCommandLine::Get(), Name, Value, false))
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

	/** The seabed's height in metres, ignoring features shorter than a wavelength. */
	FORCEINLINE double GroundM(const AThalassaOcean& Ocean, const FVector2D& AtM, double WavelengthM)
	{
		return Ocean.GroundZ(FVector(AtM.X * 100.0, AtM.Y * 100.0, 0.0), WavelengthM) / 100.0;
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

	const AThalassaOcean* Ocean = Cast<AThalassaOcean>(GetOwner());
	bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ThalassaNoFauna")) || !Ocean;

	// The leviathan's home: a big loop about 1.6 km from the start, from the seed.
	if (Ocean)
	{
		FFaunaDraws Draw(FaunaHash(static_cast<uint64>(Ocean->Seed) ^ 0x1E71A7A4ull));
		const double Bearing = Draw.Range(0.0, UE_DOUBLE_TWO_PI);
		LeviathanHomeM = FVector2D(FMath::Cos(Bearing), FMath::Sin(Bearing)) * 1600.0;
		LeviathanLoopM = Draw.Range(900.0, 1200.0);
		LeviathanSpeedMS = Draw.Range(7.0, 9.0);
		LeviathanPhase = Draw.Range(0.0, UE_DOUBLE_TWO_PI);
		FVector2D Placed;
		if (SwitchAt(TEXT("ThalassaLeviathanAt="), Placed))
		{
			LeviathanHomeM = Placed;
			LeviathanLoopM = 250.0;
			LeviathanSpeedMS = 4.0;
		}
	}

	// A dedicated server draws nothing, but still works out contacts for the chart.
	if (bDisabled || GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogThalassa, Display, TEXT("THALASSA: ocean fauna %s"), bDisabled ? TEXT("off") : TEXT("not drawn (dedicated server)"));
		return;
	}
	Drawn.SetNum(MeshCount);
	for (int32 Index = 0; Index < MeshCount; ++Index)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPaths[Index]);
		if (!Mesh)
		{
			UE_LOG(LogThalassa, Warning, TEXT("THALASSA: ocean fauna has no mesh at %s"), MeshPaths[Index]);
		}
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(GetOwner(), *FString::Printf(TEXT("Fauna%d"), Index));
		Component->SetStaticMesh(Mesh);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		double DrawM = LeviathanDrawM;
		for (const FSpecies& Spec : Species)
		{
			DrawM = Spec.Mesh == Index ? Spec.DrawM : DrawM;
		}
		Component->SetCullDistances(0, static_cast<int32>(DrawM * 100.0));
		Component->NumCustomDataFloats = CustomFloats;
		Component->SetupAttachment(GetOwner()->GetRootComponent());
		Component->SetAbsolute(true, true, true);
		Component->RegisterComponent();
		Instances.Add(Component);
	}
	UE_LOG(LogThalassa, Display, TEXT("THALASSA: ocean fauna on: %d kinds of animal, and a leviathan whose round is centred at %s m"), SpeciesCount, *LeviathanHomeM.ToString());
}

double UThalassaOceanFauna::Now() const
{
	if (const AThalassaVessel* Ship = Vessel.Get())
	{
		return Ship->GetSyncedTime();
	}
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UThalassaOceanFauna::ReadPings()
{
	if (!Vessel.IsValid())
	{
		Vessel = AThalassaVessel::Find(GetWorld());
	}
	Pings.Reset();
	const AThalassaVessel* Ship = Vessel.Get();
	if (const UThalassaSonarComponent* Sonar = Ship ? Ship->GetSonar() : nullptr)
	{
		for (const FSonarPulse& Pulse : Sonar->GetRecentPulses())
		{
			Pings.Add({ Pulse.Origin, Pulse.StartTime, Pulse.RangeM });
		}
	}
}

bool UThalassaOceanFauna::MakeSchool(const AThalassaOcean& Ocean, uint8 SpeciesIndex, int64 CellX, int64 CellY, FFaunaSchool& OutSchool)
{
	using namespace ThalassaFauna;
	const FSpecies& Kind = Species[SpeciesIndex];
	const uint64 Seed = static_cast<uint64>(Ocean.Seed);
	// The glider keeps its first salt, so the schools already met stay where they were.
	const uint64 Salt = 0xF15Bull + 0x1000ull * SpeciesIndex;
	FFaunaDraws Draw(FaunaHash(FaunaHash(static_cast<uint64>(CellX) ^ (Seed * 0x9FB21C651E98DF25ull) ^ Salt) ^ (static_cast<uint64>(CellY) * 0xC2B2AE3D27D4EB4Full)));

	OutSchool = FFaunaSchool();
	OutSchool.Key = { SpeciesIndex, CellX, CellY };
	const bool bStart = CellX == 0 && CellY == 0; // Every kind lives near the start, so there is something to meet.
	const double Jitter = bStart ? FMath::Min(100.0, Kind.CellM * 0.25) : Kind.CellM * 0.25;
	OutSchool.HomeM = FVector2D(CellX * Kind.CellM + Draw.Range(-Jitter, Jitter), CellY * Kind.CellM + Draw.Range(-Jitter, Jitter));

	EProvince Neighbour;
	double Weight = 1.0;
	const EProvince Province = Ocean.GetGenerator().ProvinceAt(OutSchool.HomeM.X, OutSchool.HomeM.Y, Neighbour, Weight);
	if (!bStart && Draw.Next() >= Kind.Chance[static_cast<int32>(Province)])
	{
		return false;
	}

	OutSchool.AltitudeM = Draw.Range(Kind.AltMin, Kind.AltMax);
	OutSchool.RoamM = Draw.Range(Kind.RoamMin, Kind.RoamMax);
	OutSchool.SpeedMS = Draw.Range(Kind.SpeedMin, Kind.SpeedMax);
	for (double& Phase : OutSchool.Phase)
	{
		Phase = Draw.Range(0.0, UE_DOUBLE_TWO_PI);
	}
	OutSchool.Phase[3] = Draw.Next();
	OutSchool.Tint = Kind.Palette ? Kind.Palette[FMath::Min(Kind.PaletteCount - 1, static_cast<int32>(Draw.Next() * Kind.PaletteCount))] : FLinearColor::White;
	FVector2D Placed;
	if (bStart && SpeciesIndex == 0 && SwitchAt(TEXT("ThalassaGlidersAt="), Placed))
	{
		OutSchool.HomeM = Placed;
		OutSchool.RoamM = 15.0;
		OutSchool.SpeedMS = 0.4;
	}
	// For screenshots: every kind's school from the start, gathered round one spot, barely roaming.
	if (bStart && SwitchAt(TEXT("ThalassaFaunaAt="), Placed))
	{
		const double Bearing = SpeciesIndex * 2.4;
		OutSchool.HomeM = Placed + FVector2D(FMath::Cos(Bearing), FMath::Sin(Bearing)) * (6.0 + 3.0 * SpeciesIndex);
		OutSchool.RoamM = FMath::Min(OutSchool.RoamM, 8.0);
		OutSchool.SpeedMS *= 0.3;
		OutSchool.AltitudeM = FMath::Min(OutSchool.AltitudeM, Kind.Kind == EKind::Jelly ? 30.0 : (Kind.Kind == EKind::Kraken ? 45.0 : 12.0));
	}

	// The animals: loosely spread through a flattened oval (jellyfish: a rounder cloud), apart from each other.
	const int32 Count = Kind.CountMin + static_cast<int32>(Draw.Next() * (Kind.CountMax - Kind.CountMin + 1));
	const double Size = Kind.SpreadM * FMath::Sqrt(static_cast<double>(FMath::Min(Count, 40)));
	const FVector Oval = Kind.Kind == EKind::Jelly ? FVector(Size, Size, Size * 0.6) : FVector(Size * 0.75, Size, Size * 0.25);
	const int32 Tries = Count <= 12 ? 20 : (Count <= 40 ? 4 : 1);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FFaunaSchool::FMember Member;
		for (int32 Try = 0; Try < Tries; ++Try)
		{
			Member.OffsetM = FVector(Draw.Range(-1.0, 1.0) * Oval.X, Draw.Range(-1.0, 1.0) * Oval.Y, Draw.Range(-1.0, 1.0) * Oval.Z);
			bool bClear = true;
			for (int32 Other = 0; bClear && Tries > 1 && Other < OutSchool.Members.Num(); ++Other)
			{
				bClear = FVector::Dist(OutSchool.Members[Other].OffsetM, Member.OffsetM) > Kind.SpacingM;
			}
			if (bClear)
			{
				break;
			}
		}
		Member.Scale = Draw.Range(Kind.ScaleMin, Kind.ScaleMax);
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
	TConstArrayView<FFaunaPing> PingList, const FVector* ShipLocation)
{
	using namespace ThalassaFauna;
	const FSpecies& Kind = Species[School.Key.Species];
	FFaunaSchoolState State;

	FVector2D P, V, A;
	Wander(School, Time, P, V, A);
	const double Speed2 = FMath::Max(V.SizeSquared(), 1.0e-6);
	State.YawDeg = FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
	State.TurnDegS = FMath::RadiansToDegrees((V.X * A.Y - V.Y * A.X) / Speed2);

	// Swim at its height over the ground, following its shape (closely, for fish near the ground), rising and
	// falling a little.
	const double Wavelength = Kind.AltMax < 5.0 ? 4.0 : 60.0;
	const double Ahead = FMath::Clamp(School.RoamM * 0.15, 4.0, 25.0);
	const double Bob = FMath::Min(4.0, School.AltitudeM * 0.2) * FMath::Sin(Time * 0.05 + School.Phase[2]);
	const double Ground = GroundM(Ocean, P, Wavelength);
	const double Z = Ground + School.AltitudeM + Bob;
	const double ZAhead = GroundM(Ocean, P + V.GetSafeNormal() * Ahead, Wavelength) + School.AltitudeM;
	State.PitchDeg = Kind.Kind == EKind::Jelly ? 0.0 : FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ZAhead - Z, Ahead)) * 0.5, -12.0, 12.0);
	FVector Centre(P.X, P.Y, FMath::Min(Z, -15.0));
	State.GroundCm = Ground * 100.0;

	// Pings: when one's wave reaches the school, the school bolts away from where it came from, its glow flashes,
	// it spreads out, and over the next half minute it settles and drifts back to its path. Pings close together
	// add up: a school pinged again while still fleeing bolts further. Jellyfish only drift a little, and flash.
	const FVector Calm = Centre;
	double Latest = -1.0e9;
	for (const FFaunaPing& Ping : PingList)
	{
		if (Ping.RangeM <= 0.f || Time - Ping.Time > 60.0)
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
		const double Bolt = Kind.BoltM * (1.0 - FMath::Exp(-Age / 7.0)) * (1.0 - FaunaSmooth(14.0, 45.0, Age));
		Centre += FVector(Away * Bolt, 0.0);
		State.Spread = FMath::Max(State.Spread, (1.0 - FMath::Exp(-Age / 1.5)) * (1.0 - FaunaSmooth(8.0, 35.0, Age)));
		State.Flash = FMath::Max(State.Flash, FMath::Exp(-Age / (Kind.Kind == EKind::Jelly ? 2.5 : 1.2)));
		State.Rate = FMath::Max(State.Rate, 1.0 + (Kind.Kind == EKind::Jelly ? 1.0 : 2.0) * FMath::Exp(-Age / 7.0));
		if (Ping.Time > Latest && Kind.Kind != EKind::Jelly)
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
		if (Distance < Kind.KeepClearM)
		{
			Centre += (Distance > 1.0 ? Offset / Distance : FVector(0.0, 0.0, -1.0)) * (Kind.KeepClearM - Distance);
		}
	}

	State.CentreCm = Centre * 100.0;
	return State;
}

const FFaunaSchool* UThalassaOceanFauna::FindSchool(uint8 SpeciesIndex, int64 CellX, int64 CellY)
{
	const FFaunaSchoolKey Key{ SpeciesIndex, CellX, CellY };
	if (const FFaunaSchool* Found = Schools.Find(Key))
	{
		return Found->Members.Num() > 0 ? Found : nullptr;
	}
	const AThalassaOcean* Ocean = Cast<AThalassaOcean>(GetOwner());
	FFaunaSchool School;
	if (!Ocean || !MakeSchool(*Ocean, SpeciesIndex, CellX, CellY, School))
	{
		School = FFaunaSchool();
		School.Key = Key;
	}
	const FFaunaSchool& Added = Schools.Add(Key, MoveTemp(School));
	return Added.Members.Num() > 0 ? &Added : nullptr;
}

FVector UThalassaOceanFauna::LeviathanHeadAt(double Time)
{
	using namespace ThalassaFauna;
	const AThalassaOcean* Ocean = Cast<AThalassaOcean>(GetOwner());
	if (!Ocean)
	{
		return FVector::ZeroVector;
	}

	// Its round: a big slow loop with a smaller one on top, at a steady height over the ground, never near the
	// surface.
	const auto Route = [this, Ocean](double T)
	{
		const double W1 = LeviathanSpeedMS / LeviathanLoopM;
		const double A1 = LeviathanPhase + W1 * T;
		const FVector2D P = LeviathanHomeM + FVector2D(LeviathanLoopM * FMath::Cos(A1) + 0.3 * LeviathanLoopM * FMath::Cos(2.3 * A1 + 1.0),
			LeviathanLoopM * FMath::Sin(A1) + 0.3 * LeviathanLoopM * FMath::Sin(1.7 * A1 + 2.0));
		return FVector(P.X, P.Y, FMath::Min(GroundM(*Ocean, P, 200.0) + 120.0, -60.0));
	};
	const FVector Round = Route(Time);

	// Pings draw it: one heard within 2.2 km brings it, over two and a half minutes, to circle where the ping came
	// from, about 230 m out and a little below; after five minutes or so it loses interest and goes back to its
	// round. Several pings close together hold its interest, and it circles between them.
	double Interest = 0.0, Total = 0.0;
	FVector Target = FVector::ZeroVector;
	for (const FFaunaPing& Ping : Pings)
	{
		if (Ping.RangeM <= 0.f || Time - Ping.Time > 500.0 || Time < Ping.Time)
		{
			continue;
		}
		const FVector OriginM = Ping.Origin / 100.0;
		const double Heard = FVector::Dist2D(Route(Ping.Time), OriginM);
		const double Age = Time - (Ping.Time + Heard / PingSpeedMS);
		if (Heard > LeviathanHearsM || Age <= 0.0)
		{
			continue;
		}
		const double Pull = FaunaSmooth(0.0, 150.0, Age) * (1.0 - FaunaSmooth(330.0, 480.0, Age));
		if (Pull <= 0.0)
		{
			continue;
		}
		const double Angle = Time * (LeviathanSpeedMS * 0.6 / LeviathanCircleM) + Ping.Time;
		const FVector2D Around = FVector2D(OriginM.X, OriginM.Y) + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * LeviathanCircleM;
		const double Z = FMath::Clamp(OriginM.Z - 30.0, GroundM(*Ocean, Around, 200.0) + 60.0, -60.0);
		Target += FVector(Around.X, Around.Y, Z) * Pull;
		Total += Pull;
		Interest = FMath::Max(Interest, Pull);
	}
	const FVector Head = Total > 0.0 ? FMath::Lerp(Round, Target / Total, FMath::Min(Interest, 1.0)) : Round;
	return Head * 100.0;
}

FVector UThalassaOceanFauna::LeviathanSample(int64 Index)
{
	if (const FVector* Found = LeviathanTrail.Find(Index))
	{
		return *Found;
	}
	return LeviathanTrail.Add(Index, LeviathanHeadAt(Index * ThalassaFauna::LeviathanSampleS));
}

void UThalassaOceanFauna::LeviathanPieces(double Time, TArray<FLeviathanPiece>& OutPieces)
{
	using namespace ThalassaFauna;

	// Where each piece sits along the body, measured back from the tip of the snout, and how big it is. In the
	// meshes' own metres first, then all scaled up together.
	constexpr double K = LeviathanScale;
	struct FSlot { uint8 Mesh; double ArcM; double Scale; };
	TArray<FSlot, TInlineAllocator<LeviathanSegments + 2>> Slots;
	Slots.Add({ 0, 9.0 * K, K });                  // The head's origin is 9 m behind its snout.
	double Start = 16.5 - 0.6;                     // It is 16.5 m long; the first segment overlaps it a little.
	double Scale = 1.0;
	for (int32 Segment = 0; Segment < LeviathanSegments; ++Segment)
	{
		Scale = 1.0 - 0.45 * FMath::Pow(static_cast<double>(Segment) / (LeviathanSegments - 1), 1.2);
		Slots.Add({ 1, (Start + 5.0 * Scale) * K, Scale * K });
		Start += 10.0 * Scale;
	}
	const double TailScale = Scale * 0.95;
	Slots.Add({ 2, (Start - 0.6 * TailScale + 6.0 * TailScale) * K, TailScale * K });
	const double Length = Slots.Last().ArcM + 18.0 * TailScale * K;

	// The path its head has taken, back far enough for the whole body.
	TArray<FVector, TInlineAllocator<512>> Path;
	TArray<double, TInlineAllocator<512>> Arc;
	Path.Add(LeviathanHeadAt(Time) / 100.0);
	Arc.Add(0.0);
	int64 Index = static_cast<int64>(FMath::FloorToDouble(Time / LeviathanSampleS));
	if (Index * LeviathanSampleS >= Time)
	{
		--Index;
	}
	while (Arc.Last() < Length + 6.0 && Path.Num() < 500)
	{
		const FVector Point = LeviathanSample(Index--) / 100.0;
		const double Step = FVector::Dist(Point, Path.Last());
		if (Step < 0.01)
		{
			continue;
		}
		Arc.Add(Arc.Last() + Step);
		Path.Add(Point);
	}
	const auto At = [&Path, &Arc](double S)
	{
		int32 I = 1;
		while (I < Arc.Num() - 1 && Arc[I] < S)
		{
			++I;
		}
		const double Span = FMath::Max(Arc[I] - Arc[I - 1], 1.0e-6);
		return FMath::Lerp(Path[I - 1], Path[I], FMath::Clamp((S - Arc[I - 1]) / Span, 0.0, 1.0));
	};
	// A slow wave runs back along the body from side to side, growing towards the tail.
	const auto Body = [&At, Time, Length](double S)
	{
		const FVector Here = At(S);
		const FVector Along = (At(S - 5.0) - At(S + 5.0)).GetSafeNormal2D();
		const FVector Side(-Along.Y, Along.X, 0.0);
		return Here + Side * (FMath::Sin(Time * 0.35 - S * 0.02) * 6.0 * (S / Length));
	};

	// Its lights flare as a ping's wave passes over it.
	double Flash = 0.0;
	for (const FFaunaPing& Ping : Pings)
	{
		const double DistanceM = FVector::Dist(Ping.Origin / 100.0, Path[0]);
		const double Age = Time - (Ping.Time + DistanceM / PingSpeedMS);
		if (Ping.RangeM > 0.f && DistanceM <= Ping.RangeM && Age >= 0.0)
		{
			Flash = FMath::Max(Flash, FMath::Exp(-Age / 2.0));
		}
	}

	for (const FSlot& Slot : Slots)
	{
		const FVector Here = Body(Slot.ArcM);
		const FVector Forward = (Body(Slot.ArcM - 8.0) - Body(Slot.ArcM + 8.0)).GetSafeNormal();
		const FRotator Facing = FRotationMatrix::MakeFromXZ(Forward.IsNearlyZero() ? FVector::ForwardVector : Forward, FVector::UpVector).Rotator();
		FLeviathanPiece Piece;
		Piece.Mesh = Slot.Mesh;
		Piece.Transform = FTransform(Facing, Here * 100.0, FVector(Slot.Scale));
		Piece.RadiusCm = static_cast<float>((Slot.Mesh == 0 ? 450.0 : 400.0) * Slot.Scale);   // Its body's radius, at its scale.
		Piece.Flash = static_cast<float>(Flash);
		OutPieces.Add(Piece);
	}

	// Forget the path far behind the tail.
	if (LeviathanTrail.Num() > 1500)
	{
		const int64 Oldest = Index - 50;
		for (auto It = LeviathanTrail.CreateIterator(); It; ++It)
		{
			if (It.Key() < Oldest)
			{
				It.RemoveCurrent();
			}
		}
	}
}

void UThalassaOceanFauna::GatherSonarTargets(const FVector& Origin, float RangeCm, double Time, TArray<FThalassaSonarTarget>& OutTargets)
{
	using namespace ThalassaFauna;
	const AThalassaOcean* Ocean = Cast<AThalassaOcean>(GetOwner());
	if (!Ocean || bDisabled)
	{
		return;
	}
	ReadPings();
	// The ship was where the ping went out. Its replicated position differs a little between machines, so it is
	// not used here: every machine must find the same contacts.
	const FVector ShipLocation = Origin;

	for (uint8 Kind = 0; Kind < SpeciesCount; ++Kind)
	{
		const FSpecies& Spec = Species[Kind];
		if (Spec.SonarRadiusCm <= 0.f)
		{
			continue;
		}
		const int32 Reach = FMath::CeilToInt((RangeCm / 100.0 + Spec.RoamMax * 1.35 + 150.0) / Spec.CellM);
		const int64 CX = FaunaCellOf(Origin.X / 100.0, Spec.CellM), CY = FaunaCellOf(Origin.Y / 100.0, Spec.CellM);
		for (int64 Y = CY - Reach; Y <= CY + Reach; ++Y)
		{
			for (int64 X = CX - Reach; X <= CX + Reach; ++X)
			{
				const FFaunaSchool* School = FindSchool(Kind, X, Y);
				if (!School)
				{
					continue;
				}
				const FFaunaSchoolState State = SchoolAt(*Ocean, *School, Time, Pings, &ShipLocation);
				if (FVector::Dist(State.CentreCm, Origin) > RangeCm + 10000.0)
				{
					continue;
				}
				const FRotator Facing(0.0, State.YawDeg, 0.0);
				for (const FFaunaSchool::FMember& Member : School->Members)
				{
					const FVector Location = State.CentreCm + Facing.RotateVector(Member.OffsetM * 100.0);
					if (FVector::Dist(Location, Origin) <= RangeCm)
					{
						OutTargets.Add({ Location, static_cast<float>(Spec.SonarRadiusCm * Member.Scale) });
					}
				}
			}
		}
	}

	// The leviathan: a long chain of returns, one from each part of its body.
	if (FVector::Dist(LeviathanHeadAt(Time), Origin) <= RangeCm + 20000.0)
	{
		TArray<FLeviathanPiece> Pieces;
		LeviathanPieces(Time, Pieces);
		for (const FLeviathanPiece& Piece : Pieces)
		{
			if (FVector::Dist(Piece.Transform.GetLocation(), Origin) <= RangeCm)
			{
				OutTargets.Add({ Piece.Transform.GetLocation(), Piece.RadiusCm });
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
	if (bDisabled || !Ocean || Instances.Num() != MeshCount || !Camera)
	{
		return;
	}
	ReadPings();
	const double Time = Now();
	const FVector ShipLocation = Vessel.IsValid() ? Vessel->GetActorLocation() : FVector::ZeroVector;
	const FVector* Ship = Vessel.IsValid() ? &ShipLocation : nullptr;
	const FVector Eye = Camera->GetCameraLocation();

	for (FDrawn& Mesh : Drawn)
	{
		Mesh.Transforms.Reset();
		Mesh.CustomData.Reset();
	}
	uint32 Signatures[MeshCount] = {};

	// Every school near the camera, kind by kind, in a fixed order, so each animal keeps its instance while the set
	// of schools is unchanged.
	for (uint8 Kind = 0; Kind < SpeciesCount; ++Kind)
	{
		const FSpecies& Spec = Species[Kind];
		FDrawn& Out = Drawn[Spec.Mesh];
		const int64 CX = FaunaCellOf(Eye.X / 100.0, Spec.CellM), CY = FaunaCellOf(Eye.Y / 100.0, Spec.CellM);
		const int32 Reach = FMath::CeilToInt((Spec.DrawM + Spec.RoamMax * 1.35 + 50.0) / Spec.CellM);
		const bool bStroked = Spec.Kind == EKind::Jelly || Spec.Kind == EKind::Kraken;
		const double DriftM = Spec.SpacingM * (Spec.Kind == EKind::Jelly ? 0.5 : 0.33);
		const double Wobble = Spec.Kind == EKind::Glider ? 6.0 : 10.0;
		for (int64 Y = CY - Reach; Y <= CY + Reach; ++Y)
		{
			for (int64 X = CX - Reach; X <= CX + Reach; ++X)
			{
				const FFaunaSchool* School = FindSchool(Kind, X, Y);
				if (!School)
				{
					continue;
				}
				const FFaunaSchoolState State = SchoolAt(*Ocean, *School, Time, Pings, Ship);
				if (FVector::Dist(State.CentreCm, Eye) > (Spec.DrawM + 30.0) * 100.0)
				{
					continue;
				}
				Signatures[Spec.Mesh] = HashCombine(Signatures[Spec.Mesh], GetTypeHash(School->Key));

				const FRotator Facing(0.0, State.YawDeg, 0.0);
				const double Bank = Spec.Kind == EKind::Jelly ? 0.0 : FMath::Clamp(-State.TurnDegS * 6.0, -25.0, 25.0);
				const double Floor = State.GroundCm + 100.0 * (Spec.Kind == EKind::Jelly ? Spec.AltMin * 0.8 : FMath::Max(0.25, Spec.AltMin * 0.3));
				for (const FFaunaSchool::FMember& Member : School->Members)
				{
					// Its place in the school, opening out when startled, with a slow drift of its own.
					const FVector Drift(DriftM * FMath::Sin(Time * 0.07 + Member.Drift[0]), DriftM * FMath::Sin(Time * 0.05 + Member.Drift[1]), 0.5 * DriftM * FMath::Sin(Time * 0.09 + Member.Drift[2]));
					FVector Offset = Member.OffsetM * (1.0 + 1.4 * State.Spread) + Drift;

					// Jellyfish and krakens swim in strokes, on the game's clock, so every player sees the same stroke.
					// A jellyfish bobs up as its bell draws in and sinks as it relaxes; a kraken surges forward.
					double Stroke = 0.0;
					if (bStroked)
					{
						const double Period = 1.3 * FMath::Sqrt(Spec.SizeM * Member.Scale);
						Stroke = FMath::Fmod(Time * UE_DOUBLE_TWO_PI / Period + Member.Beat, UE_DOUBLE_TWO_PI);
						if (Spec.Kind == EKind::Jelly)
						{
							Offset.Z += 0.15 * Spec.SizeM * Member.Scale * FMath::Sin(Stroke - 0.8);
						}
						else
						{
							Offset.X += 0.05 * Spec.SizeM * Member.Scale * FMath::Sin(Stroke - 1.0);
						}
					}
					FVector Location = State.CentreCm + Facing.RotateVector(Offset * 100.0);
					Location.Z = FMath::Max(Location.Z, Floor);

					FRotator Rotation;
					if (Spec.Kind == EKind::Jelly)
					{
						// Upright, turning slowly, tilting a little as it drifts.
						Rotation = FRotator(6.0 * FMath::Sin(Time * 0.1 + Member.Drift[1]), FMath::RadiansToDegrees(Member.Drift[0]) + Time * 3.0, 6.0 * FMath::Sin(Time * 0.08 + Member.Drift[2]));
					}
					else
					{
						// Facing along the school's path, or away from the ping while fleeing, each a little its own way.
						double Yaw = State.YawDeg + Wobble * FMath::Sin(Time * 0.11 + Member.Drift[0]);
						if (State.Fleeing > 0.0)
						{
							Yaw += FMath::FindDeltaAngleDegrees(Yaw, State.FleeYawDeg + 20.0 * FMath::Sin(Member.Drift[1])) * State.Fleeing;
						}
						Rotation = FRotator(State.PitchDeg + 3.0 * FMath::Sin(Time * 0.13 + Member.Drift[2]), Yaw, Bank + 4.0 * FMath::Sin(Time * 0.17 + Member.Drift[1]));
					}
					Out.Transforms.Add(FTransform(Rotation, Location, FVector(Member.Scale)));
					// Each animal a little lighter or darker than its school.
					const float Shade = 0.92f + 0.16f * static_cast<float>(FMath::Frac(Member.Drift[2] * 7.0));
					Out.CustomData.Add(bStroked ? static_cast<float>(Stroke) : Member.Beat);
					// A jellyfish's tentacles sway at a steady rate of their own, between 1 and 2.
					Out.CustomData.Add(Spec.Kind == EKind::Jelly ? 1.f + static_cast<float>(FMath::Frac(Member.Drift[0])) : static_cast<float>(State.Rate));
					Out.CustomData.Add(static_cast<float>(State.Flash));
					Out.CustomData.Add(School->Tint.R * Shade);
					Out.CustomData.Add(School->Tint.G * Shade);
					Out.CustomData.Add(School->Tint.B * Shade);
					Out.CustomData.Add(School->Tint.A);
				}
			}
		}
	}

	// Where the leviathan is, now and then, for tests: does a ping bring it?
	const FVector LeviathanHead3 = LeviathanHeadAt(Time);
	if (FMath::FloorToInt(Time / 20.0) != FMath::FloorToInt((Time - DeltaTime) / 20.0))
	{
		UE_LOG(LogThalassa, Display, TEXT("LEVIATHAN,%.1f,%.0f,%.0f,%.0f,%.0f"), Time, LeviathanHead3.X / 100.0, LeviathanHead3.Y / 100.0, LeviathanHead3.Z / 100.0,
			Ship ? FVector::Dist(LeviathanHead3, ShipLocation) / 100.0 : -1.0);
	}

	// The leviathan, if it is near enough to see.
	if (FVector::Dist(LeviathanHead3, Eye) < LeviathanDrawM * 100.0)
	{
		TArray<FLeviathanPiece> Pieces;
		LeviathanPieces(Time, Pieces);
		if (Drawn[LeviathanHead].Signature == 0 && Pieces.Num() > 1)
		{
			UE_LOG(LogThalassa, Display, TEXT("Fauna: the leviathan is in view, head at %s m, tail at %s m, %.0f m from the camera"),
				*(Pieces[0].Transform.GetLocation() / 100.0).ToString(), *(Pieces.Last().Transform.GetLocation() / 100.0).ToString(),
				FVector::Dist(Pieces[0].Transform.GetLocation(), Eye) / 100.0);
		}
		for (const FLeviathanPiece& Piece : Pieces)
		{
			const int32 Mesh = LeviathanHead + Piece.Mesh;
			Drawn[Mesh].Transforms.Add(Piece.Transform);
			Drawn[Mesh].CustomData.Append({ 0.f, 1.f, Piece.Flash, 1.f, 1.f, 1.f, 1.f });
			Signatures[Mesh] = 1;
		}
	}

	// Draw them: an unchanged set moves its instances; a changed set is laid out afresh.
	VisibleAnimals = 0;
	for (int32 Mesh = 0; Mesh < MeshCount; ++Mesh)
	{
		FDrawn& Out = Drawn[Mesh];
		UInstancedStaticMeshComponent* Component = Instances[Mesh];
		if (!Component)
		{
			continue;
		}
		const int32 Count = Out.Transforms.Num();
		VisibleAnimals += (Mesh < LeviathanHead || Mesh > LeviathanHead + 2) ? Count : 0;
		if (Signatures[Mesh] != Out.Signature || Count != Component->GetInstanceCount())
		{
			Component->ClearInstances();
			if (Count > 0)
			{
				Component->AddInstances(Out.Transforms, false, true);
			}
			if (Count > 0 && Out.Signature == 0)
			{
				const FVector Nearest = Out.Transforms[0].GetLocation();
				UE_LOG(LogThalassa, Display, TEXT("Fauna: %d of %s in view, the first at %s m, %.0f m from the camera"), Count, MeshPaths[Mesh],
					*(Nearest / 100.0).ToString(), FVector::Dist(Nearest, Eye) / 100.0);
			}
			Out.Signature = Signatures[Mesh];
		}
		else if (Count > 0)
		{
			Component->BatchUpdateInstancesTransforms(0, Out.Transforms, true, false, true);
		}
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Component->SetCustomData(Index, TArrayView<const float>(&Out.CustomData[Index * CustomFloats], CustomFloats), false);
		}
		if (Count > 0)
		{
			Component->MarkRenderStateDirty();
		}
	}
}
