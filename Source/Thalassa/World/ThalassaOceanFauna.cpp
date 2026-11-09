#include "World/ThalassaOceanFauna.h"

#include "Camera/PlayerCameraManager.h"
#include "Animation/AnimSequence.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
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
		TEXT("/Game/Creatures/KrakenElder/SM_KrakenElder.SM_KrakenElder"),
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
		double BoltM;            // How far a ping makes it bolt. Negative: it comes towards the ping instead.
		float SonarRadiusCm;     // How big a contact each animal makes; 0: too small for the ping to show.
		double SizeM;            // Its mesh's size: big animals stroke slowly.
		const FLinearColor* Palette = nullptr;   // Colours its schools can be; none: as modelled, glowing as modelled.
		int32 PaletteCount = 0;
		// Animated kinds (Fab Marketplace creatures, skinned and rigged): their mesh and swim loop, and how far their
		// mesh is turned from facing +X. Mesh is then -1: they are drawn as animated components, not instances.
		const TCHAR* Skeletal = nullptr;
		const TCHAR* SwimAnim = nullptr;
		double YawOffsetDeg = 0.0;
		// A companion keeps another kind's school exactly (the whale calf, its mother), this far off each member.
		int32 CompanionOf = -1;
		FVector CompanionOffsetM = FVector::ZeroVector;
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
		// The elder kraken: the owner's model, 140 to 220 m long, rarer, deeper, and it doesn't flee a ping: it comes closer.
		{ TEXT("elder kraken"), 16, EKind::Kraken, 6000, { 0.02, 0.05, 0.05, 0.12, 0.20, 0.15 }, 1, 1, 200.0,  0.0,  80, 200,  600, 1200, 3.0, 5.0,    1.4, 2.2,  3000, 250, -150, 3000.f, 100.0 },

		// Animated Fab Marketplace creatures (publisher Existence), in their own Content folders. Their meshes are made
		// small; Scale is what makes them their size here. A negative Bolt means a ping draws them closer.
		//  Name             Mesh Kind          Cell   Plains Hills Plat. Ridge Canyon Volc.    Count   Spacing Spread  Altitude    Roam       Speed       Scale       Draw  Clear Bolt  Sonar  Size
		{ TEXT("megalodon"),   -1, EKind::Fish, 3000, { 0.05, 0.08, 0.08, 0.10, 0.15, 0.10 },  1, 2,  40.0, 25.0,  30, 120,  400, 900,  3.0, 5.0,    9.0, 12.0,  600,  60, -100, 600.f, 16.0, nullptr, 0,
			TEXT("/Game/Shark_Megalodon/Base_mesh/SK_Shark_Megalodon.SK_Shark_Megalodon"), TEXT("/Game/Shark_Megalodon/Animations/Anim_Shark_Megalodon_Swim_03_WithoutMoving.Anim_Shark_Megalodon_Swim_03_WithoutMoving"), -90.0 },
		{ TEXT("shark goblin"), -1, EKind::Fish, 1200, { 0.10, 0.20, 0.20, 0.25, 0.30, 0.20 },  2, 5,  10.0,  8.0,  10,  60,  150, 300,  2.5, 4.0,    4.0, 6.0,   350,  30,  -60, 200.f,  4.0, nullptr, 0,
			TEXT("/Game/SharkGoblin/Base_mesh/SK_SharkGoblin.SK_SharkGoblin"), TEXT("/Game/SharkGoblin/Animations/Anim_Swimming_02_WithoutMoving.Anim_Swimming_02_WithoutMoving"), -90.0 },
		{ TEXT("hammerhead"),  -1, EKind::Fish, 1000, { 0.15, 0.20, 0.20, 0.20, 0.20, 0.10 },  3, 8,   8.0,  7.0,  15,  60,  200, 350,  2.0, 3.0,    3.0, 4.5,   350,  40,   40, 200.f,  4.5, nullptr, 0,
			TEXT("/Game/HammerheadShark/BaseMesh/SK_HammerheadShark.SK_HammerheadShark"), TEXT("/Game/HammerheadShark/Animations/AS_Swim_03_InPlace.AS_Swim_03_InPlace"), -90.0 },
		// The whale is far bigger than any real one, on purpose: 60 to 90 m. Its calf is 20 m.
		{ TEXT("blue whale"),  -1, EKind::Fish, 4000, { 0.20, 0.15, 0.15, 0.10, 0.10, 0.05 },  1, 1,  60.0,  0.0,  70, 160, 1000, 1800, 3.0, 4.0,    4.0, 6.0,  1500, 200,   50, 2500.f, 75.0, nullptr, 0,
			TEXT("/Game/BlueWhaleAndWhaleCalf/BaseMesh/BlueWhale/SK_blue_whale.SK_blue_whale"), TEXT("/Game/BlueWhaleAndWhaleCalf/Animations/Whale/AS_While_swimming_03.AS_While_swimming_03"), -90.0 },
		{ TEXT("whale calf"),  -1, EKind::Fish, 4000, { 0.20, 0.15, 0.15, 0.10, 0.10, 0.05 },  1, 1,  60.0,  0.0,  70, 160, 1000, 1800, 3.0, 4.0,    4.0, 4.4,  1500, 200,   50, 800.f, 20.0, nullptr, 0,
			TEXT("/Game/BlueWhaleAndWhaleCalf/BaseMesh/BlueWhaleCalf/SK_blue_whale_calf.SK_blue_whale_calf"), TEXT("/Game/BlueWhaleAndWhaleCalf/Animations/WhaleCalf/AS_WhileCalf_swimming_03.AS_WhileCalf_swimming_03"), -90.0,
			/* CompanionOf the blue whale (species 17), beside and a little below it: */ 17, FVector(-15.0, 45.0, -15.0) },
		{ TEXT("dolphin"),     -1, EKind::Fish,  800, { 0.25, 0.30, 0.30, 0.25, 0.20, 0.10 },  3, 8,   6.0,  5.0,  10,  50,  200, 400,  3.0, 5.0,    0.4, 0.5,   300,  25,   60, 150.f,  3.0, nullptr, 0,
			TEXT("/Game/DolphinAndRemora/BaseMesh/Dolphin/SK_Dolphin.SK_Dolphin"), TEXT("/Game/DolphinAndRemora/Animations/Dolphin/AS_swimming_02_inplace.AS_swimming_02_inplace"), -90.0 },
		{ TEXT("sea turtle"),  -1, EKind::Fish,  600, { 0.15, 0.20, 0.20, 0.15, 0.15, 0.05 },  1, 2,   5.0,  4.0,   3,  25,  100, 250,  0.6, 1.0,    1.4, 2.0,   200,  15,   20,   0.f,  1.5, nullptr, 0,
			TEXT("/Game/SeaTurtle/BaseMesh/SK_SeaTurtle.SK_SeaTurtle"), TEXT("/Game/SeaTurtle/Animations/AS_swimming.AS_swimming"), -90.0 },
		{ TEXT("stingray"),    -1, EKind::Fish,  500, { 0.30, 0.20, 0.15, 0.10, 0.15, 0.05 },  1, 4,   4.0,  3.0, 0.5,   2,   50, 120,  0.6, 1.2,    3.0, 4.0,   180,  15,   25,   0.f,  2.5, nullptr, 0,
			TEXT("/Game/Stingray/Base_mesh/SK_Stingray.SK_Stingray"), TEXT("/Game/Stingray/Animations/Anim_Swim_01_WithoutMoving.Anim_Swim_01_WithoutMoving"), -90.0 },
		{ TEXT("moray eel"),   -1, EKind::Fish,  300, { 0.02, 0.15, 0.15, 0.30, 0.35, 0.20 },  1, 1,   3.0,  0.0, 0.3,   1,    5,  15,  0.2, 0.4,    2.0, 3.0,   120,   0,  -10,   0.f,  3.0, nullptr, 0,
			TEXT("/Game/MorayEel/Base_mesh/SK_MorayEel.SK_MorayEel"), TEXT("/Game/MorayEel/Animations/anim_swim_01.anim_swim_01"), -90.0 },
		{ TEXT("anglerfish"),  -1, EKind::Fish,  700, { 0.05, 0.10, 0.10, 0.20, 0.35, 0.40 },  1, 1,   3.0,  0.0,   5,  40,   30,  80,  0.3, 0.6,    4.0, 6.0,   150,   0,  -30,   0.f,  3.0, nullptr, 0,
			TEXT("/Game/AnglerFish/BaseMesh/SK_AnglerFish.SK_AnglerFish"), TEXT("/Game/AnglerFish/Animations/Anim_AnglerFish_Swimming_1.Anim_AnglerFish_Swimming_1"), -90.0 },
		{ TEXT("marlin"),      -1, EKind::Fish,  900, { 0.20, 0.20, 0.20, 0.15, 0.10, 0.05 },  1, 3,  10.0,  8.0,  20,  80,  300, 500,  4.0, 6.0,    3.0, 4.0,   300,  30,   80, 150.f,  3.5, nullptr, 0,
			TEXT("/Game/MarlinSailfish/Base_mesh/SK_MarlinSailfish.SK_MarlinSailfish"), TEXT("/Game/MarlinSailfish/Animations/Anim_MarlinSailfish_Swim_01_WithoutMoving.Anim_MarlinSailfish_Swim_01_WithoutMoving"), -90.0 },
	};
	constexpr int32 SpeciesCount = UE_ARRAY_COUNT(Species);

	/** Animated kinds: how often a school is gigantic, and how many times bigger. */
	struct FGiantRule { const TCHAR* Name; double Chance; double Times; };
	const FGiantRule GiantRules[] =
	{
		{ TEXT("megalodon"), 0.15, 3.5 },     // 50 m and more.
		{ TEXT("shark goblin"), 0.10, 3.0 },
		{ TEXT("hammerhead"), 0.10, 3.0 },
		{ TEXT("blue whale"), 0.20, 2.0 },    // Up to about 180 m.
		{ TEXT("whale calf"), 0.0, 1.0 },     // Its mother decides.
		{ TEXT("dolphin"), 0.05, 4.0 },
		{ TEXT("sea turtle"), 0.10, 5.0 },    // An 8 m turtle.
		{ TEXT("stingray"), 0.15, 5.0 },      // A 15 m ray.
		{ TEXT("moray eel"), 0.10, 5.0 },     // A 15 m eel in the rocks.
		{ TEXT("anglerfish"), 0.20, 5.0 },    // A monster in the canyons.
		{ TEXT("marlin"), 0.10, 3.0 },
	};

	/** The looks a school of an animated kind can have: a tint on its own colours. Mostly as made. */
	const FLinearColor FabTints[] =
	{
		{ 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f },
		{ 1.25f, 1.22f, 1.18f },  // Pale.
		{ 0.55f, 0.58f, 0.62f },  // Dark.
		{ 0.75f, 0.90f, 1.20f },  // Bluish.
		{ 0.80f, 1.05f, 0.80f },  // Greenish.
		{ 1.15f, 0.80f, 0.65f },  // Rusty.
		{ 1.60f, 1.55f, 1.50f },  // Nearly white, rare.
	};

	/** Checks the table at start: a companion must point at the kind it follows. */
	bool SpeciesTableIsSound()
	{
		for (int32 Index = 0; Index < SpeciesCount; ++Index)
		{
			const int32 Of = Species[Index].CompanionOf;
			if (Of >= SpeciesCount || (Of >= 0 && (Of == Index || Species[Of].CellM != Species[Index].CellM)))
			{
				UE_LOG(LogThalassa, Error, TEXT("THALASSA: fauna kind %s follows kind %d, which isn't a kind it can follow"), Species[Index].Name, Of);
				return false;
			}
		}
		return true;
	}

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
	bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ThalassaNoFauna")) || !Ocean || !SpeciesTableIsSound();

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

	// The animated kinds' meshes and swim loops, loaded now, not the first time one swims into view: that load
	// stalled a frame for about 100 ms.
	if (!bDisabled && GetNetMode() != NM_DedicatedServer)
	{
		for (const FSpecies& Spec : Species)
		{
			if (Spec.Skeletal)
			{
				AnimatedAssets.Add(LoadObject<UObject>(nullptr, Spec.Skeletal));
			}
			if (Spec.SwimAnim)
			{
				AnimatedAssets.Add(LoadObject<UObject>(nullptr, Spec.SwimAnim));
			}
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
	if (Kind.CompanionOf >= 0 && Kind.CompanionOf != SpeciesIndex)
	{
		if (!MakeSchool(Ocean, static_cast<uint8>(Kind.CompanionOf), CellX, CellY, OutSchool))
		{
			return false;
		}
		OutSchool.Key.Species = SpeciesIndex;
		for (FFaunaSchool::FMember& Member : OutSchool.Members)
		{
			Member.OffsetM += Kind.CompanionOffsetM * OutSchool.SizeMul;
			Member.Scale = (Kind.ScaleMin + Kind.ScaleMax) * 0.5 * OutSchool.SizeMul;
		}
		return true;
	}
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
		OutSchool.AltitudeM = FMath::Min(OutSchool.AltitudeM, Kind.Kind == EKind::Jelly ? 30.0 : (Kind.Kind == EKind::Kraken ? Kind.AltMin : 12.0));
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

	// Animated kinds come in looks and sizes: each school a tint, and now and then a giant.
	if (Kind.Mesh < 0)
	{
		OutSchool.Tint = FabTints[FMath::Min(static_cast<int32>(UE_ARRAY_COUNT(FabTints)) - 1, static_cast<int32>(Draw.Next() * UE_ARRAY_COUNT(FabTints)))];
		const double Roll = Draw.Next();
		const double Spread = Draw.Range(0.8, 1.2);
		for (const FGiantRule& Rule : GiantRules)
		{
			if (FCString::Strcmp(Rule.Name, Kind.Name) == 0 && Roll < Rule.Chance)
			{
				OutSchool.SizeMul = Rule.Times * Spread;
			}
		}
		if (OutSchool.SizeMul != 1.0)
		{
			for (FFaunaSchool::FMember& Member : OutSchool.Members)
			{
				Member.Scale *= OutSchool.SizeMul;
				Member.OffsetM *= OutSchool.SizeMul;
			}
			OutSchool.AltitudeM *= FMath::Pow(OutSchool.SizeMul, 0.7);
		}
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
			// Fleeing, it faces away from the ping; coming closer, towards it.
			const FVector2D Facing = Kind.BoltM < 0.0 ? -Away : Away;
			State.FleeYawDeg = FMath::RadiansToDegrees(FMath::Atan2(Facing.Y, Facing.X));
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
						// Animated kinds' scale is their mesh's, not their size: their contact grows only if they are giants.
						OutTargets.Add({ Location, static_cast<float>(Spec.SonarRadiusCm * (Spec.Mesh < 0 ? School->SizeMul : Member.Scale)) });
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
		const bool bAnimated = Spec.Mesh < 0;
		if (bAnimated && AnimatedDrawn.Num() < SpeciesCount)
		{
			AnimatedDrawn.SetNum(SpeciesCount);
		}
		FDrawn& Out = bAnimated ? AnimatedDrawn[Kind] : Drawn[Spec.Mesh];
		if (bAnimated)
		{
			Out.Transforms.Reset();
			Out.CustomData.Reset();
			Out.Ids.Reset();
		}
		const int64 CX = FaunaCellOf(Eye.X / 100.0, Spec.CellM), CY = FaunaCellOf(Eye.Y / 100.0, Spec.CellM);
		const double ReachDrawM = Spec.DrawM * (bAnimated ? 5.0 : 1.0);   // Animated kinds may be giants, seen from further.
		const int32 Reach = FMath::CeilToInt((ReachDrawM + Spec.RoamMax * 1.35 + 50.0) / Spec.CellM);
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
				if (FVector::Dist(State.CentreCm, Eye) > (Spec.DrawM * School->SizeMul + 30.0) * 100.0)
				{
					continue;
				}
				if (!bAnimated)
				{
					Signatures[Spec.Mesh] = HashCombine(Signatures[Spec.Mesh], GetTypeHash(School->Key));
				}

				const FRotator Facing(0.0, State.YawDeg, 0.0);
				const double Bank = Spec.Kind == EKind::Jelly ? 0.0 : FMath::Clamp(-State.TurnDegS * 6.0, -25.0, 25.0);
				const double Floor = State.GroundCm + 100.0 * (Spec.Kind == EKind::Jelly ? Spec.AltMin * 0.8 : FMath::Max(0.25, Spec.AltMin * 0.3));
				for (int32 MemberIndex = 0; MemberIndex < School->Members.Num(); ++MemberIndex)
				{
					const FFaunaSchool::FMember& Member = School->Members[MemberIndex];
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
					if (bAnimated)
					{
						// The mesh faces along its own axis; turned to face forward. Its swim loop runs faster when fleeing.
						Out.Transforms.Add(FTransform(Rotation.Quaternion() * FRotator(0.0, Spec.YawOffsetDeg, 0.0).Quaternion(), Location, FVector(Member.Scale)));
						// Play rate, then the school's tint. Giants swim slower strokes.
						Out.CustomData.Add(static_cast<float>(State.Rate / FMath::Sqrt(School->SizeMul)));
						Out.CustomData.Add(School->Tint.R);
						Out.CustomData.Add(School->Tint.G);
						Out.CustomData.Add(School->Tint.B);
						Out.Ids.Add(HashCombine(GetTypeHash(School->Key), static_cast<uint32>(MemberIndex)));
						continue;
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

	DrawAnimated(Eye);

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

void UThalassaOceanFauna::DrawAnimated(const FVector& Eye)
{
	using namespace ThalassaFauna;
	constexpr int32 MaxPerKind = 20;   // The nearest of each kind are animated; farther ones aren't drawn.

	TSet<uint32> Shown;
	for (int32 Kind = 0; Kind < AnimatedDrawn.Num(); ++Kind)
	{
		const FSpecies& Spec = Species[Kind];
		FDrawn& Out = AnimatedDrawn[Kind];
		if (Spec.Mesh >= 0 || !Spec.Skeletal || Out.Transforms.Num() == 0)
		{
			continue;
		}
		// Nearest first.
		TArray<int32> Order;
		for (int32 Index = 0; Index < Out.Transforms.Num(); ++Index)
		{
			Order.Add(Index);
		}
		Order.Sort([&Out, &Eye](int32 A, int32 B) { return FVector::DistSquared(Out.Transforms[A].GetLocation(), Eye) < FVector::DistSquared(Out.Transforms[B].GetLocation(), Eye); });
		Order.SetNum(FMath::Min(Order.Num(), MaxPerKind));

		for (int32 Index : Order)
		{
			const uint32 Id = HashCombine(Out.Ids[Index], static_cast<uint32>(Kind));
			Shown.Add(Id);
			// Each animal keeps its own component while it is shown, so its swimming carries on smoothly.
			int32* Slot = AnimatedSlots.Find(Id);
			if (!Slot)
			{
				int32 Free = INDEX_NONE;
				for (int32 Pool = 0; Pool < AnimatedPool.Num(); ++Pool)
				{
					if (AnimatedPoolKind[Pool] == Kind && !AnimatedPoolBusy[Pool])
					{
						Free = Pool;
						break;
					}
				}
				if (Free == INDEX_NONE)
				{
					USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, Spec.Skeletal);
					UAnimSequence* Swim = Spec.SwimAnim ? LoadObject<UAnimSequence>(nullptr, Spec.SwimAnim) : nullptr;
					if (!Mesh)
					{
						continue;
					}
					USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(GetOwner());
					Component->SetSkeletalMeshAsset(Mesh);
					Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Component->SetCastShadow(false);
					Component->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
					Component->SetupAttachment(GetOwner()->GetRootComponent());
					Component->SetAbsolute(true, true, true);
					Component->RegisterComponent();
					if (Swim)
					{
						Component->PlayAnimation(Swim, true);
						Component->SetPosition(FMath::FRand() * Swim->GetPlayLength(), false);
					}
					for (int32 Material = 0; Material < Component->GetNumMaterials(); ++Material)
					{
						Component->CreateDynamicMaterialInstance(Material);
					}
					Free = AnimatedPool.Add(Component);
					AnimatedPoolKind.Add(Kind);
					AnimatedPoolBusy.Add(false);
				}
				AnimatedPoolBusy[Free] = true;
				Slot = &AnimatedSlots.Add(Id, Free);
				// Its school's look (tint_fab_materials.py added the parameter to the creature's materials).
				const FLinearColor Tint(Out.CustomData[Index * 4 + 1], Out.CustomData[Index * 4 + 2], Out.CustomData[Index * 4 + 3]);
				for (int32 Material = 0; Material < AnimatedPool[Free]->GetNumMaterials(); ++Material)
				{
					if (UMaterialInstanceDynamic* Look = Cast<UMaterialInstanceDynamic>(AnimatedPool[Free]->GetMaterial(Material)))
					{
						Look->SetVectorParameterValue(TEXT("ThalassaTint"), Tint);
					}
				}
			}
			USkeletalMeshComponent* Component = AnimatedPool[*Slot];
			Component->SetWorldTransform(Out.Transforms[Index]);
			Component->SetPlayRate(Out.CustomData[Index * 4]);
			Component->SetVisibility(true);
		}
	}

	// Animals no longer shown give their components back.
	for (auto It = AnimatedSlots.CreateIterator(); It; ++It)
	{
		if (!Shown.Contains(It.Key()))
		{
			AnimatedPoolBusy[It.Value()] = false;
			if (USkeletalMeshComponent* Component = AnimatedPool[It.Value()])
			{
				Component->SetVisibility(false);
			}
			It.RemoveCurrent();
		}
	}
	AnimatedShown = Shown.Num();
}
