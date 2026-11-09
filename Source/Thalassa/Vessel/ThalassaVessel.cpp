#include "Vessel/ThalassaVessel.h"

#include "Vessel/ThalassaShipDamage.h"
#include "Vessel/ThalassaWaterSurface.h"
#include "Vessel/ThalassaSonarComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Vessel/ThalassaSonarDisplay.h"
#include "Vessel/ThalassaSonarPingComponent.h"
#include "Vessel/ThalassaHelmComponent.h"
#include "Vessel/ThalassaHelmDisplay.h"
#include "Vessel/ThalassaCameraStation.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Vessel/ThalassaVesselLayout.h"
#include "Vessel/ThalassaShipMarkers.h"
#include "Vessel/ThalassaHoloTable.h"
#include "Vessel/ThalassaShipAudio.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Characters/ThalassaCharacter.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Tools/ThalassaTestConfig.h"
#include "UObject/ConstructorHelpers.h"

namespace ThalassaVesselLayout
{
	struct FRigBox { const TCHAR* Name; FVector Center; FVector Size; FLinearColor Color; bool bVisible; };

	const FLinearColor Hull(0.10f, 0.13f, 0.16f);
	const FLinearColor Deck(0.20f, 0.24f, 0.27f);
	const FLinearColor Bridge(0.08f, 0.22f, 0.30f);
	const FLinearColor Corridor(0.28f, 0.25f, 0.20f);
	const FLinearColor Engineering(0.35f, 0.18f, 0.10f);
	const FLinearColor AirlockColor(0.45f, 0.36f, 0.08f);
	const FLinearColor Console(0.03f, 0.04f, 0.05f); // Dark, or the readout's text washes out.

	// Ship frame: +X forward, +Z up, deck surface at Z = 0, centimetres.
	// Bridge X 1000..2000, corridor X -1000..1000, engineering X -2000..-1000.
	const FRigBox Boxes[] =
	{
		{ TEXT("Deck"),            FVector(0, 0, -25),        FVector(4000, 900, 50),  Deck, true },
		{ TEXT("Ceiling"),         FVector(0, 0, 375),        FVector(4000, 900, 50),  Hull, true },
		{ TEXT("BowWall"),         FVector(2025, 0, 175),     FVector(50, 900, 450),   Bridge, true },
		{ TEXT("SternWall"),       FVector(-2025, 0, 175),    FVector(50, 900, 450),   Engineering, true },

		// Bridge: the port wall carries the observation window opening (X 1200..1800, Z 100..280).
		{ TEXT("BridgeStarboard"), FVector(1500, 425, 175),   FVector(1000, 50, 450),  Bridge, true },
		{ TEXT("BridgePortFwd"),   FVector(1900, -425, 175),  FVector(200, 50, 450),   Bridge, true },
		{ TEXT("BridgePortAft"),   FVector(1100, -425, 175),  FVector(200, 50, 450),   Bridge, true },
		{ TEXT("BridgePortLow"),   FVector(1500, -425, 50),   FVector(600, 50, 100),   Bridge, true },
		{ TEXT("BridgePortHigh"),  FVector(1500, -425, 325),  FVector(600, 50, 90),    Bridge, true },
		{ TEXT("WindowPane"),      FVector(1500, -425, 190),  FVector(600, 20, 180),   Hull, false },

		// Corridor walls and the two doorway bulkheads (doorway 200 wide, 250 high).
		{ TEXT("CorridorPort"),    FVector(0, -225, 175),     FVector(2000, 50, 350),  Corridor, true },
		{ TEXT("CorridorStbdFwd"), FVector(550, 225, 175),    FVector(900, 50, 350),   Corridor, true },
		{ TEXT("CorridorStbdAft"), FVector(-550, 225, 175),   FVector(900, 50, 350),   Corridor, true },

		// Airlock, off the corridor to starboard: inner doorway in the corridor wall, outer to the sea.
		{ TEXT("AirlockInnerHead"), FVector(0, 225, 300),     FVector(200, 50, 100),   AirlockColor, true },
		{ TEXT("AirlockFwd"),      FVector(225, 350, 175),    FVector(50, 250, 350),   AirlockColor, true },
		{ TEXT("AirlockAft"),      FVector(-225, 350, 175),   FVector(50, 250, 350),   AirlockColor, true },
		{ TEXT("AirlockOuterFwd"), FVector(175, 450, 175),    FVector(150, 50, 350),   AirlockColor, true },
		{ TEXT("AirlockOuterAft"), FVector(-175, 450, 175),   FVector(150, 50, 350),   AirlockColor, true },
		{ TEXT("AirlockOuterHead"), FVector(0, 450, 300),     FVector(200, 50, 100),   AirlockColor, true },
		{ TEXT("BulkFwdPort"),     FVector(1000, -275, 175),  FVector(50, 350, 350),   Corridor, true },
		{ TEXT("BulkFwdStbd"),     FVector(1000, 275, 175),   FVector(50, 350, 350),   Corridor, true },
		{ TEXT("BulkFwdHead"),     FVector(1000, 0, 300),     FVector(50, 200, 100),   Corridor, true },
		{ TEXT("BulkAftPort"),     FVector(-1000, -275, 175), FVector(50, 350, 350),   Corridor, true },
		{ TEXT("BulkAftStbd"),     FVector(-1000, 275, 175),  FVector(50, 350, 350),   Corridor, true },
		{ TEXT("BulkAftHead"),     FVector(-1000, 0, 300),    FVector(50, 200, 100),   Corridor, true },

		// Engineering: outer walls, plus three steps to exercise step-up on a moving base.
		{ TEXT("EngPort"),         FVector(-1500, -425, 175), FVector(1000, 50, 450),  Engineering, true },
		{ TEXT("EngStbd"),         FVector(-1500, 425, 175),  FVector(1000, 50, 450),  Engineering, true },
		{ TEXT("Step1"),           FVector(-1650, 0, 10),     FVector(100, 400, 20),   Engineering, true },
		{ TEXT("Step2"),           FVector(-1750, 0, 20),     FVector(100, 400, 40),   Engineering, true },
		{ TEXT("Step3"),           FVector(-1850, 0, 30),     FVector(100, 400, 60),   Engineering, true },

		{ TEXT("BeepConsole"),     FVector(1950, 0, 60),      FVector(60, 200, 120),   Console, true },
	};

	// The hull is a sealed box, so nothing from outside lights the interior. Warm in the working
	// spaces, in keeping with the contrast the art direction wants against cold water.
	struct FRigLight { const TCHAR* Name; FVector Location; FColor Color; };
	const FRigLight Lights[] =
	{
		{ TEXT("LightBridge"),      FVector(1500, 0, 310),  FColor(255, 214, 170) },
		{ TEXT("LightCorridorFwd"), FVector(500, 0, 310),   FColor(255, 224, 190) },
		{ TEXT("LightCorridorAft"), FVector(-500, 0, 310),  FColor(255, 224, 190) },
		{ TEXT("LightEngineering"), FVector(-1500, 0, 310), FColor(255, 170, 120) },
		{ TEXT("LightAirlock"),     FVector(0, 340, 310),   FColor(255, 240, 200) },
	};
	const float InteriorLightIntensity = 12000.f;
	const float InteriorLightRadius = 1400.f;

	// Everything outside these ship-frame boxes is sea. Inner and outer door thickness belongs to the
	// compartment on the ship's side of it, so stepping through the outer door is what leaves the ship.
	const FBox DryCompartments[] =
	{
		FBox(FVector(1000, -425, -50), FVector(2000, 425, 400)),   // Bridge
		FBox(FVector(-1000, -200, -50), FVector(1000, 200, 400)),  // Corridor
		FBox(FVector(-100, 200, -50), FVector(100, 250, 400)),     // Inner doorway
		FBox(FVector(-2000, -425, -50), FVector(-1000, 425, 400)), // Engineering
	};
	const FBox AirlockChamber(FVector(-200, 250, -50), FVector(200, 425, 400));
	const float AirlockCeilingCm = 350.f;

	const FVector AirlockPanelLocation(0.f, 340.f, 100.f);
	const float AirlockPanelRadius = 420.f;
	const float AirlockFloodSeconds = 6.f;
	const float DoorSeconds = 1.f;
	const float DoorInnerY = 225.f;
	const float DoorOuterY = 450.f;
	const float DoorClearanceCm = 55.f;       // Capsule radius, half the door, and a little margin.
	const FVector DoorSize(200.f, 30.f, 250.f);
	const float DoorOpenRaiseCm = 245.f;      // Slides up into the bulkhead above the doorway.

	const FVector ConsoleLocation(1920.f, 0.f, 60.f);
	const float ConsoleUseRadius = 300.f;

	const FVector HullBoundsMin(-2050.f, -450.f, -50.f);
	const FVector HullBoundsMax(2050.f, 450.f, 400.f);

	// Every condition from the design document's test matrix that a kinematic hull can be driven
	// through. Duration seconds, speed m/s, yaw rate deg/s, pitch deg, roll deg.
	struct FSegment { float Duration; float SpeedMS; float YawRate; float Pitch; float Roll; };
	const FSegment Profile[] =
	{
		{ 15.f,  0.f,  0.f,  0.f,  0.f }, // rest
		{ 15.f, 10.f,  0.f,  0.f,  0.f }, // accelerate
		{ 15.f, 10.f,  0.f,  0.f,  0.f }, // cruise
		{ 20.f, 10.f,  3.f,  0.f,  0.f }, // turn to starboard
		{ 20.f, 10.f, -3.f,  0.f,  0.f }, // turn to port
		{ 15.f, 10.f,  0.f, 10.f,  0.f }, // pitch up
		{ 15.f, 10.f,  0.f,  0.f,  0.f }, // level out
		{ 15.f, 10.f,  0.f,  0.f, 10.f }, // roll
		{ 15.f, 10.f,  0.f,  0.f,  0.f }, // level out
		{ 20.f, 25.f,  8.f,  5.f,  5.f }, // hard manoeuvre
		{ 20.f,  0.f,  0.f,  0.f,  0.f }, // brake
	};

	// Slow enough for a swimmer to catch: EVA runs through the same kinds of motion at low speed.
	const FSegment EvaProfile[] =
	{
		{ 10.f, 0.f,  0.f, 0.f, 0.f },
		{ 70.f, 1.f,  0.f, 0.f, 0.f },
		{ 70.f, 1.5f, 1.f, 0.f, 0.f },
		{ 70.f, 2.f, -1.f, 3.f, 0.f },
		{ 70.f, 2.f,  0.f, 0.f, 3.f },
		{ 30.f, 0.f,  0.f, 0.f, 0.f },
	};

	const float AccelerationCmS2 = 200.f;
	const float YawAccelerationDegS2 = 2.f;
	const float AttitudeRateDegS = 2.f;

	// The helm's motion model. The ship is heavy: thrust against water resistance, so it takes about half
	// a minute to reach full speed and coasts a long way when the throttle comes off. Upgrades would
	// change these numbers.
	const float HelmAheadAccelMS2 = 0.5f;      // Full throttle, from rest.
	const float HelmAsternAccelMS2 = 0.1f;     // Propellers are weaker going astern.
	const float HelmLinearDrag = 0.01f;        // Per second: lets the ship come to rest eventually.
	const float HelmQuadraticDrag = (HelmAheadAccelMS2 - HelmLinearDrag * 15.f) / (15.f * 15.f); // Top speed 15 m/s.
	const float HelmMaxYawRateDegS = 4.f;
	const float HelmYawAccelDegS2 = 1.f;
	const float HelmFullSteerageMS = 6.f;      // The rudder needs water flowing past it to bite,
	const float HelmMinSteerage = 0.15f;       // but a little turning is possible at rest.
	const float HelmMaxPitchDeg = 12.f;
	const float HelmMaxHeaveMS = 1.5f;        // Vertical thrusters at full: the ship rises or sinks level.
	const float HelmHeaveAccelMS2 = 0.12f;    // Heavy: about 12 s to full vertical speed, and a long drift after.
	const float HelmLeverSwingDeg = 35.f;
	const float HelmTestSeconds = 160.f;

	// Full stop brakes hard, with everything the ship has: about 15 seconds from top speed.
	const float FullStopDecelMS2 = 1.f;
	const FLinearColor FullStopIdleColor(0.25f, 0.02f, 0.02f);
	const FLinearColor FullStopLitColor(1.f, 0.05f, 0.03f);

	// The sea's surface is world Z = 0. Surfaced, the ship floats with its origin this far below it.
	const float SurfacedDepthCm = 1500.f;
	const float CollisionPullBackCm = 5.f;
	const float PoseSmoothingRate = 6.f;      // 1/s, decay of client pose corrections
	const float ClockSmoothingRate = 2.f;     // 1/s, how quickly the client follows snapshot timing
	const double ClockSnapErrorSeconds = 0.25; // Beyond this (a stall), jump rather than drift back
	const float SnapErrorDistanceCm = 500.f;  // Beyond this, snap rather than visibly slide
}

namespace
{
	/** The original three-compartment rig. Kept so test results stay comparable over time. */
	FVesselLayout BuildTestRigLayout()
	{
		using namespace ThalassaVesselLayout;

		const auto Stretched = [](const FVector& Layout)
		{
			return FVector(Layout.X * ThalassaTestConfig::HullStretch(), Layout.Y, Layout.Z);
		};

		FVesselLayout Layout;
		for (const FRigBox& Box : Boxes)
		{
			FHullBox Piece;
			Piece.Name = Box.Name;
			Piece.Center = Stretched(Box.Center);
			Piece.Size = Stretched(Box.Size);
			Piece.Color = Box.Color;
			Piece.bVisible = Box.bVisible;
			Layout.Boxes.Add(Piece);
		}
		for (const FRigLight& Light : Lights)
		{
			Layout.Lights.Add({ Light.Name, Stretched(Light.Location), Light.Color });
		}
		for (const FBox& Compartment : DryCompartments)
		{
			Layout.DryCompartments.Add(FBox(Stretched(Compartment.Min), Stretched(Compartment.Max)));
		}
		Layout.HullBounds = FBox(Stretched(HullBoundsMin), Stretched(HullBoundsMax));
		Layout.ConsoleLocation = Stretched(ConsoleLocation);
		Layout.ConsoleTextLocation = Stretched(FVector(1918.f, 0.f, 90.f));
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Layout.Spawns.Add(Stretched(FVector(1500.f - 100.f * Index, Index % 2 == 0 ? -70.f : 70.f, 120.f)));
		}
		Layout.AirlockViewpoint = FVector(0.f, -100.f, 120.f);
		return Layout;
	}

	const FVesselLayout& GetLayout()
	{
		static const FVesselLayout Layout = ThalassaTestConfig::UseTestRig() ? BuildTestRigLayout() : BuildMainDeckLayout();
		return Layout;
	}
}

AThalassaVessel::AThalassaVessel()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bReplicates = true;
	SetReplicatingMovement(false); // The snapshot is the single source of the ship's pose.
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30.f);
	NetPriority = 3.f;

	ShipRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ShipRoot"));
	ShipRoot->SetMobility(EComponentMobility::Movable);
	RootComponent = ShipRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	// The cube's default material has no colour parameter; this one does. See BeginPlay.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TintableMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	const FVesselLayout& Layout = GetLayout();
	for (const FHullBox& Box : Layout.Boxes)
	{
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(Box.Name);
		Piece->SetupAttachment(ShipRoot);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetStaticMesh(CubeMesh.Object);
		Piece->SetMaterial(0, TintableMaterial.Object);
		Piece->SetRelativeLocation(Box.Center);
		Piece->SetRelativeRotation(Box.Rotation);
		Piece->SetRelativeScale3D(Box.Size / 100.f); // Engine cube is 100 cm per side.
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision); // HullCollision does this.
		Piece->SetVisibility(Box.bVisible);
		HullPieces.Add(Piece);
	}

	// Gameplay spots, as components that can be moved in the Blueprint. ReadMarkers reads them back.
	Airlock = CreateDefaultSubobject<USceneComponent>(TEXT("Airlock"));
	Airlock->SetupAttachment(ShipRoot);
	Airlock->SetMobility(EComponentMobility::Movable);
	Airlock->SetRelativeLocation(Layout.AirlockOffset);

	for (int32 Index = 0; Index < Layout.DryCompartments.Num(); ++Index)
	{
		const FBox& Box = Layout.DryCompartments[Index];
		UThalassaDryCompartmentComponent* Zone = CreateDefaultSubobject<UThalassaDryCompartmentComponent>(FName(*FString::Printf(TEXT("DryCompartment%02d"), Index)));
		Zone->SetupAttachment(ShipRoot);
		Zone->SetRelativeLocation(Box.GetCenter());
		Zone->InitBoxExtent(Box.GetExtent());
		DryCompartments.Add(Zone);
	}
	for (int32 Index = 0; Index < Layout.LadderZones.Num(); ++Index)
	{
		const FBox& Box = Layout.LadderZones[Index];
		UThalassaLadderZoneComponent* Zone = CreateDefaultSubobject<UThalassaLadderZoneComponent>(FName(*FString::Printf(TEXT("LadderZone%02d"), Index)));
		Zone->SetupAttachment(ShipRoot);
		Zone->SetRelativeLocation(Box.GetCenter());
		Zone->InitBoxExtent(Box.GetExtent());
		LadderZones.Add(Zone);
	}
	for (int32 Index = 0; Index < Layout.Spawns.Num(); ++Index)
	{
		UThalassaSpawnPointComponent* Spawn = CreateDefaultSubobject<UThalassaSpawnPointComponent>(FName(*FString::Printf(TEXT("SpawnPoint%02d"), Index)));
		Spawn->SetupAttachment(ShipRoot);
		Spawn->SetRelativeLocation(Layout.Spawns[Index]);
		SpawnPoints.Add(Spawn);
	}

	const auto MakeDoor = [&](const TCHAR* Name, float DoorY)
	{
		UStaticMeshComponent* Door = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Door->SetupAttachment(Airlock);
		Door->SetMobility(EComponentMobility::Movable);
		Door->SetStaticMesh(CubeMesh.Object);
		Door->SetMaterial(0, TintableMaterial.Object);
		Door->SetRelativeLocation(FVector(0.f, DoorY, 125.f));
		Door->SetRelativeScale3D(ThalassaVesselLayout::DoorSize / 100.f);
		Door->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		return Door;
	};
	AirlockInnerDoor = MakeDoor(TEXT("AirlockInnerDoor"), ThalassaVesselLayout::DoorInnerY);
	AirlockOuterDoor = MakeDoor(TEXT("AirlockOuterDoor"), ThalassaVesselLayout::DoorOuterY);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	AirlockWater = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AirlockWater"));
	AirlockWater->SetupAttachment(Airlock);
	AirlockWater->SetMobility(EComponentMobility::Movable);
	AirlockWater->SetStaticMesh(PlaneMesh.Object);
	AirlockWater->SetMaterial(0, TintableMaterial.Object);
	AirlockWater->SetRelativeLocation(FVector(0.f, 337.f, 0.f));
	AirlockWater->SetRelativeScale3D(FVector(4.f, 1.75f, 1.f));
	AirlockWater->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AirlockWater->SetCastShadow(false);
	AirlockWater->SetVisibility(false);

	HullCollision = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HullCollision"));
	HullCollision->SetupAttachment(ShipRoot);
	HullCollision->SetMobility(EComponentMobility::Movable);
	HullCollision->SetStaticMesh(CubeMesh.Object);
	HullCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	HullCollision->SetHiddenInGame(true);

	ShellCollision = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShellCollision"));
	ShellCollision->SetupAttachment(ShipRoot);
	ShellCollision->SetMobility(EComponentMobility::Movable);
	ShellCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShellCollision->SetVisibility(false); // Collision only. "Show Collision" in the editor draws it.
	ShellCollision->SetCastShadow(false);

	for (const FInteriorLight& Light : Layout.Lights)
	{
		UPointLightComponent* Lamp = CreateDefaultSubobject<UPointLightComponent>(Light.Name);
		Lamp->SetupAttachment(ShipRoot);
		Lamp->SetMobility(EComponentMobility::Movable);
		Lamp->SetRelativeLocation(Light.Location);
		Lamp->SetLightColor(Light.Color);
		Lamp->SetIntensity(ThalassaVesselLayout::InteriorLightIntensity);
		Lamp->SetAttenuationRadius(ThalassaVesselLayout::InteriorLightRadius);
		Lamp->SetCastShadows(false);
	}

	// Floodlights outside the hull: two at the bow lighting ahead of the bridge window, three under the keel
	// lighting the seabed. Placed from the hull's bounds, so they follow the deck plan.
	{
		// Each lamp a little different in strength, colour and spread, so where beams overlap the light is
		// uneven, as with real lamps of different ages. A hot centre fading to a wide soft edge, not an even
		// disc, and a mottled pattern inside each beam (MLF_Floodlight, a light function; see docs/SEABED.md).
		struct FLamp { const TCHAR* Name; FVector Location; FRotator Aim; float Candelas; float Outer; FLinearColor Color; };
		const FBox& Hull = Layout.HullBounds;
		const float Below = Hull.Min.Z - 60.f;
		const FLamp Lamps[] = {
			{ TEXT("FloodBowPort"),      FVector(Hull.Max.X + 40.f, -700.f, -60.f), FRotator(-20.f, -8.f, 0.f), 12000.f, 24.f, FLinearColor(0.80f, 0.92f, 1.f) },
			{ TEXT("FloodBowStarboard"), FVector(Hull.Max.X + 40.f,  700.f, -60.f), FRotator(-18.f,  9.f, 0.f), 10000.f, 27.f, FLinearColor(0.74f, 0.90f, 1.f) },
			{ TEXT("FloodKeelFore"),     FVector(Hull.Max.X * 0.6f, 0.f, Below),     FRotator(-78.f, 3.f, 0.f), 6000.f, 30.f, FLinearColor(0.76f, 0.90f, 1.f) },
			{ TEXT("FloodKeelMid"),      FVector(Hull.GetCenter().X, 0.f, Below),    FRotator(-90.f, 0.f, 0.f), 7500.f, 28.f, FLinearColor(0.82f, 0.93f, 1.f) },
			{ TEXT("FloodKeelAft"),      FVector(Hull.Min.X * 0.6f, 0.f, Below),     FRotator(-101.f, -4.f, 0.f), 5000.f, 32.f, FLinearColor(0.70f, 0.87f, 1.f) },
		};
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> FloodPattern(TEXT("/Game/Ship/Lights/MLF_Floodlight.MLF_Floodlight"));
		for (const FLamp& Spec : Lamps)
		{
			USpotLightComponent* Lamp = CreateDefaultSubobject<USpotLightComponent>(Spec.Name);
			Lamp->SetupAttachment(ShipRoot);
			Lamp->SetMobility(EComponentMobility::Movable);
			Lamp->SetRelativeLocationAndRotation(Spec.Location, Spec.Aim);
			Lamp->SetIntensityUnits(ELightUnits::Candelas);
			Lamp->SetIntensity(Spec.Candelas);
			Lamp->SetAttenuationRadius(9000.f);
			Lamp->SetInnerConeAngle(2.f);
			Lamp->SetOuterConeAngle(Spec.Outer);
			Lamp->SetLightColor(Spec.Color); // Cool white: warm light looks sickly green through the water.
			if (FloodPattern.Succeeded())
			{
				Lamp->SetLightFunctionMaterial(FloodPattern.Object);
			}
			Lamp->SetVolumetricScatteringIntensity(4.f); // Dimmer lamps than before, but the beams still show in the water.
			Lamp->SetCastShadows(false); // Each points away from the hull; shadows would cost more than they show.
			Floodlights.Add(Lamp);
		}
	}

	if (Layout.bHasHoloTable)
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> HoloMaterial(TEXT("/Game/Ship/Holo/M_HoloPoint.M_HoloPoint"));

		HoloTable = CreateDefaultSubobject<UThalassaHoloTableComponent>(TEXT("HoloTable"));
		HoloTable->SetupAttachment(ShipRoot);
		HoloTable->SetRelativeLocation(Layout.HoloTableLocation);

		// The table itself: scenery for now, so players pass through it (docs/HOLOTABLE.md).
		HoloTableBase = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HoloTableBase"));
		HoloTableBase->SetupAttachment(HoloTable);
		HoloTableBase->SetMobility(EComponentMobility::Movable);
		HoloTableBase->SetStaticMesh(CubeMesh.Object);
		HoloTableBase->SetMaterial(0, TintableMaterial.Object);
		HoloTableBase->SetRelativeLocation(FVector(0.f, 0.f, -45.f));
		HoloTableBase->SetRelativeScale3D(FVector(2.3f, 2.3f, 0.9f));
		HoloTableBase->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		HoloPoints = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HoloPoints"));
		HoloPoints->SetupAttachment(HoloTable);
		HoloPoints->SetMobility(EComponentMobility::Movable);
		HoloPoints->SetStaticMesh(CubeMesh.Object);
		if (HoloMaterial.Succeeded())
		{
			HoloPoints->SetMaterial(0, HoloMaterial.Object);
		}
		HoloPoints->NumCustomDataFloats = 3;
		HoloPoints->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HoloPoints->SetCastShadow(false);
		HoloPoints->SetCanEverAffectNavigation(false);
		HoloTable->Points = HoloPoints;
	}

	// Sound: the engine room, the hull around each listener, and the hull striking things.
	MachinerySound = CreateDefaultSubobject<UThalassaMachineryComponent>(TEXT("MachinerySound"));
	MachinerySound->SetupAttachment(ShipRoot);
	MachinerySound->SetRelativeLocation(Layout.MachineryLocation);
	HullAmbience = CreateDefaultSubobject<UThalassaHullAmbienceComponent>(TEXT("HullAmbience"));
	HullAmbience->SetupAttachment(ShipRoot);
	ImpactSound = CreateDefaultSubobject<UThalassaImpactComponent>(TEXT("ImpactSound"));
	ImpactSound->SetupAttachment(ShipRoot);

	Sonar = CreateDefaultSubobject<UThalassaSonarComponent>(TEXT("Sonar"));
	Damage = CreateDefaultSubobject<UThalassaShipDamageComponent>(TEXT("Damage"));
	SonarPulse = CreateDefaultSubobject<UThalassaSonarPingComponent>(TEXT("SonarPulse"));
	SonarPulse->SetupAttachment(ShipRoot);
	SonarPulse->MakeActivePulse();
	Helm = CreateDefaultSubobject<UThalassaHelmComponent>(TEXT("Helm"));
	Helm->SeatLocal = Layout.HelmSeatLocation;
	Helm->bAvailable = Layout.bHasHelm;
	if (Layout.bHasHelm)
	{
		HelmSpot = CreateDefaultSubobject<UThalassaStationSpotComponent>(TEXT("HelmSpot"));
		HelmSpot->SetupAttachment(ShipRoot);
		HelmSpot->SetRelativeLocation(Layout.HelmSeatLocation);
	}

	if (Layout.bHasCameraStation)
	{
		CameraStation = CreateDefaultSubobject<UThalassaCameraStationComponent>(TEXT("CameraStation"));
		CameraStation->SeatLocal = Layout.CameraSeatLocation;
		CameraStation->bAvailable = true;

		CameraSpot = CreateDefaultSubobject<UThalassaStationSpotComponent>(TEXT("CameraSpot"));
		CameraSpot->SetupAttachment(ShipRoot);
		CameraSpot->SetRelativeLocation(Layout.CameraSeatLocation);

		const auto MakeCameraScreen = [&](const TCHAR* Name, const FVector& Location, float SizeCm, bool bShowsAll)
		{
			UThalassaCameraScreenComponent* Screen = CreateDefaultSubobject<UThalassaCameraScreenComponent>(Name);
			Screen->SetupAttachment(ShipRoot);
			Screen->SetRelativeLocation(Location);
			Screen->SetRelativeRotation(UThalassaConsoleScreenComponent::FacingRotation(Layout.CameraScreensFacing));
			Screen->SetRelativeScale3D(FVector(SizeCm / 100.f));
			Screen->bShowsAll = bShowsAll;
			return Screen;
		};
		CameraScreen = MakeCameraScreen(TEXT("CameraScreen"), Layout.CameraScreenLocation, Layout.CameraScreenSizeCm, false);
		CameraBleep = CreateDefaultSubobject<UThalassaBleepComponent>(TEXT("CameraBleep"));
		CameraBleep->SetupAttachment(CameraScreen);
		CameraGrid = MakeCameraScreen(TEXT("CameraGrid"), Layout.CameraGridLocation, Layout.CameraGridSizeCm, true);

		int32 Index = 0;
		for (const FHullCamera& Spec : Layout.HullCameras)
		{
			USceneCaptureComponent2D* Camera = CreateDefaultSubobject<USceneCaptureComponent2D>(FName(*FString::Printf(TEXT("HullCamera%d"), Index++)));
			Camera->SetupAttachment(ShipRoot);
			Camera->SetRelativeLocationAndRotation(Spec.Location, Spec.Rotation);
			Camera->FOVAngle = 80.f;
			// Captured on demand by the station, only while someone can see a screen.
			Camera->bCaptureEveryFrame = false;
			Camera->bCaptureOnMovement = false;
			Camera->bAlwaysPersistRenderingState = true; // Lets exposure adapt between captures.
			Camera->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
			HullCameras.Add(Camera);
			CameraStation->Cameras.Add(Camera);
			CameraStation->Labels.Add(Spec.Label);
		}
	}

	if (Layout.bHasHelm)
	{
		HelmScreen = CreateDefaultSubobject<UThalassaHelmScreenComponent>(TEXT("HelmScreen"));
		HelmScreen->SetupAttachment(ShipRoot);
		HelmScreen->SetRelativeLocation(Layout.HelmScreenLocation);
		HelmScreen->SetRelativeRotation(UThalassaConsoleScreenComponent::FacingRotation(Layout.HelmScreenFacing));
		HelmScreen->SetRelativeScale3D(FVector(Layout.HelmScreenSizeCm / 100.f));
		HelmBleep = CreateDefaultSubobject<UThalassaBleepComponent>(TEXT("HelmBleep"));
		HelmBleep->SetupAttachment(HelmScreen);

		HelmLeverPivot = CreateDefaultSubobject<USceneComponent>(TEXT("HelmLeverPivot"));
		HelmLeverPivot->SetupAttachment(ShipRoot);
		HelmLeverPivot->SetMobility(EComponentMobility::Movable);
		HelmLeverPivot->SetRelativeLocation(Layout.HelmLeverLocation);

		HelmLever = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HelmLever"));
		HelmLever->SetupAttachment(HelmLeverPivot);
		HelmLever->SetMobility(EComponentMobility::Movable);
		HelmLever->SetStaticMesh(CubeMesh.Object);
		HelmLever->SetMaterial(0, TintableMaterial.Object);
		HelmLever->SetRelativeLocation(FVector(0.f, 0.f, 25.f));
		HelmLever->SetRelativeScale3D(FVector(0.06f, 0.08f, 0.5f));
		HelmLever->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		FullStopButton = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FullStopButton"));
		FullStopButton->SetupAttachment(ShipRoot);
		FullStopButton->SetMobility(EComponentMobility::Movable);
		FullStopButton->SetStaticMesh(CubeMesh.Object);
		FullStopButton->SetMaterial(0, TintableMaterial.Object);
		FullStopButton->SetRelativeLocation(Layout.FullStopButtonLocation);
		FullStopButton->SetRelativeScale3D(FVector(0.18f, 0.18f, 0.06f));
		FullStopButton->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (Layout.SonarScreenSizeCm > 0.f)
	{
		SonarScreen = CreateDefaultSubobject<UThalassaSonarScreenComponent>(TEXT("SonarScreen"));
		SonarScreen->SetupAttachment(ShipRoot);
		SonarScreen->SetRelativeLocation(Layout.SonarScreenLocation);
		SonarScreen->SetRelativeRotation(UThalassaConsoleScreenComponent::FacingRotation(Layout.SonarScreenFacing));
		SonarScreen->SetRelativeScale3D(FVector(Layout.SonarScreenSizeCm / 100.f));

		SonarPing = CreateDefaultSubobject<UThalassaSonarPingComponent>(TEXT("SonarPing"));
		SonarPing->SetupAttachment(SonarScreen);
		SonarScreen->PingSound = SonarPing;
		SonarBleep = CreateDefaultSubobject<UThalassaBleepComponent>(TEXT("SonarBleep"));
		SonarBleep->SetupAttachment(SonarScreen);
	}

	ConsoleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("ConsoleText"));
	ConsoleText->SetupAttachment(ShipRoot);
	ConsoleText->SetRelativeLocation(Layout.ConsoleTextLocation);
	ConsoleText->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	ConsoleText->SetHorizontalAlignment(EHTA_Center);
	ConsoleText->SetVerticalAlignment(EVRTA_TextCenter);
	ConsoleText->SetWorldSize(11.f);
	ConsoleText->SetTextRenderColor(FColor(88, 215, 211));
	ConsoleText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AThalassaVessel::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AThalassaVessel, Snapshot);
	DOREPLIFETIME(AThalassaVessel, ProfileState);
	DOREPLIFETIME(AThalassaVessel, BeepCount);
	DOREPLIFETIME(AThalassaVessel, CollisionCount);
	DOREPLIFETIME(AThalassaVessel, LastCollisionTime);
	DOREPLIFETIME(AThalassaVessel, AirlockState);
	DOREPLIFETIME(AThalassaVessel, AirlockWaterLevel);
	DOREPLIFETIME(AThalassaVessel, bAirlockDoorwayBlocked);
}

AThalassaVessel* AThalassaVessel::Find(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<AThalassaVessel> It(const_cast<UWorld*>(World)); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

void AThalassaVessel::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// The greybox can be hidden to see the art alone. Runs in the Blueprint editor too.
	int32 PieceIndex = 0;
	for (const FHullBox& Box : GetLayout().Boxes)
	{
		if (HullPieces.IsValidIndex(PieceIndex))
		{
			HullPieces[PieceIndex]->SetVisibility(Box.bVisible && bShowGreybox);
		}
		++PieceIndex;
	}

	MakeArtVisualOnly();

	// Runs on server and clients alike, so both see identical collision. Exactly one of the two is solid.
	if (WalkableShell)
	{
		ShellCollision->SetStaticMesh(WalkableShell);
		ShellCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		HullCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		ShellCollision->SetStaticMesh(nullptr);
		ShellCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HullCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		if (HullCollision->GetInstanceCount() == 0)
		{
			for (const FTransform& Box : GetGreyboxCollisionTransforms())
			{
				HullCollision->AddInstance(Box);
			}
		}
	}
}

TArray<FTransform> AThalassaVessel::GetGreyboxCollisionTransforms()
{
	TArray<FTransform> Transforms;
	for (const FHullBox& Box : GetLayout().Boxes)
	{
		if (Box.bCollision)
		{
			Transforms.Add(FTransform(Box.Rotation, Box.Center, Box.Size / 100.f));
		}
	}
	return Transforms;
}

void AThalassaVessel::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ReadMarkers();
}

FTransform AThalassaVessel::GetShipLocalTransform(const USceneComponent* Component) const
{
	if (Component->GetAttachParent() == RootComponent)
	{
		return Component->GetRelativeTransform(); // Exact: no round trip through world space.
	}
	return Component->GetComponentTransform().GetRelativeTransform(RootComponent->GetComponentTransform());
}

void AThalassaVessel::ReadMarkers()
{
	const auto ReadZone = [this](const UBoxComponent* Box)
	{
		FShipZone Zone;
		Zone.Transform = GetShipLocalTransform(Box);
		Zone.Extent = Box->GetUnscaledBoxExtent() * Zone.Transform.GetScale3D().GetAbs();
		Zone.Transform.SetScale3D(FVector::OneVector);
		return Zone;
	};

	// Every marker on the ship, including any added in the Blueprint, not just the ones made here.
	TInlineComponentArray<UThalassaDryCompartmentComponent*> DryMarkers(this);
	TInlineComponentArray<UThalassaLadderZoneComponent*> LadderMarkers(this);
	TInlineComponentArray<UThalassaSpawnPointComponent*> SpawnMarkers(this);

	DryZones.Reset();
	HullBoundsLocal = GetLayout().HullBounds;
	for (const UThalassaDryCompartmentComponent* Marker : DryMarkers)
	{
		const FShipZone Zone = ReadZone(Marker);
		DryZones.Add(Zone);
		HullBoundsLocal += FBox::BuildAABB(FVector::ZeroVector, Zone.Extent).TransformBy(Zone.Transform);
	}

	LadderZoneShapes.Reset();
	for (const UThalassaLadderZoneComponent* Marker : LadderMarkers)
	{
		LadderZoneShapes.Add(ReadZone(Marker));
	}

	// In name order, so the first player always gets the same spot.
	SpawnMarkers.Sort([](const UThalassaSpawnPointComponent& A, const UThalassaSpawnPointComponent& B) { return A.GetName() < B.GetName(); });
	SpawnLocals.Reset();
	for (const UThalassaSpawnPointComponent* Marker : SpawnMarkers)
	{
		SpawnLocals.Add(GetShipLocalTransform(Marker).GetLocation());
	}
	if (SpawnLocals.IsEmpty())
	{
		SpawnLocals.Add(FVector(0.f, 0.f, 120.f));
		UE_LOG(LogTemp, Warning, TEXT("THALASSA: the ship has no spawn points, so everyone starts at its centre."));
	}

	if (HelmSpot)
	{
		Helm->SeatLocal = GetShipLocalTransform(HelmSpot).GetLocation();
	}
	if (CameraSpot && CameraStation)
	{
		CameraStation->SeatLocal = GetShipLocalTransform(CameraSpot).GetLocation();
	}
	AirlockLocal = GetShipLocalTransform(Airlock).GetLocation();
	if (FullStopButton)
	{
		FullStopButtonLocal = FullStopButton->GetRelativeLocation();
	}
}

FBox AThalassaVessel::AirlockChamberBox() const
{
	return ThalassaVesselLayout::AirlockChamber.ShiftBy(AirlockLocal);
}

void AThalassaVessel::BeginPlay()
{
	Super::BeginPlay();

	PoseRotation = GetActorRotation();

	if (Sonar)
	{
		Sonar->PulseSound = SonarPulse;
	}

	// Each floodlight gets its own copy of the beam pattern, shifted, so no two beams are mottled alike.
	for (int32 Index = 0; Index < Floodlights.Num(); ++Index)
	{
		USpotLightComponent* Lamp = Floodlights[Index];
		if (Lamp && Lamp->LightFunctionMaterial && !Lamp->LightFunctionMaterial->IsA<UMaterialInstanceDynamic>())
		{
			UMaterialInstanceDynamic* Pattern = UMaterialInstanceDynamic::Create(Lamp->LightFunctionMaterial, this);
			Pattern->SetVectorParameterValue(TEXT("Offset"), FLinearColor(3.7f * (Index + 1), 1.9f * (Index + 1), 5.3f * (Index + 1)));
			Lamp->SetLightFunctionMaterial(Pattern);
		}
		if (Lamp)
		{
			UE_LOG(LogTemp, Display, TEXT("THALASSA: floodlight %s %.0f cd, cones %.0f/%.0f, pattern %s"), *Lamp->GetName(), Lamp->Intensity,
				Lamp->InnerConeAngle, Lamp->OuterConeAngle, Lamp->LightFunctionMaterial ? *Lamp->LightFunctionMaterial->GetName() : TEXT("none"));
		}
	}

	if (ThalassaTestConfig::IsArtProbe())
	{
		// A solid crate, as an artist might add one, in the corridor the walkers use. It must not stop them.
		AStaticMeshActor* Crate = GetWorld()->SpawnActor<AStaticMeshActor>(GetActorTransform().TransformPosition(FVector(500.f, -1500.f, 75.f)), GetActorRotation());
		if (Crate)
		{
			Crate->SetMobility(EComponentMobility::Movable);
			Crate->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Crate->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Crate->SetActorScale3D(FVector(1.5f));
			Crate->AttachToComponent(ShipRoot, FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	// Child actors, such as packaged rooms, exist by now. Construction ran before they did.
	MakeArtVisualOnly();

	if (HasAuthority() && ThalassaTestConfig::IsSonarOnForTest())
	{
		Sonar->SetSurveying(true);
	}

	// Tint the graybox so compartments are recognisable. No effect on simulation.
	if (GetNetMode() != NM_DedicatedServer)
	{
		int32 Index = 0;
		for (const FHullBox& Box : GetLayout().Boxes)
		{
			if (UMaterialInstanceDynamic* Material = HullPieces[Index]->CreateAndSetMaterialInstanceDynamic(0))
			{
				Material->SetVectorParameterValue(TEXT("Color"), Box.Color);
			}
			++Index;
		}

		const FLinearColor DoorColor(0.75f, 0.42f, 0.05f);
		for (UStaticMeshComponent* Door : { AirlockInnerDoor.Get(), AirlockOuterDoor.Get() })
		{
			if (UMaterialInstanceDynamic* Material = Door->CreateAndSetMaterialInstanceDynamic(0))
			{
				Material->SetVectorParameterValue(TEXT("Color"), DoorColor);
			}
		}
		if (FullStopButton)
		{
			FullStopButtonMaterial = FullStopButton->CreateAndSetMaterialInstanceDynamic(0);
			if (FullStopButtonMaterial)
			{
				FullStopButtonMaterial->SetVectorParameterValue(TEXT("Color"), ThalassaVesselLayout::FullStopIdleColor);
			}
		}
		if (HelmLever)
		{
			if (UMaterialInstanceDynamic* Material = HelmLever->CreateAndSetMaterialInstanceDynamic(0))
			{
				Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.42f, 0.05f));
			}
		}
		if (UMaterialInstanceDynamic* Material = AirlockWater->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.25f, 0.33f));
		}
	}
}

void AThalassaVessel::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		TickServer(DeltaSeconds);
		TickAirlockServer(DeltaSeconds);
	}
	else
	{
		TickClient(DeltaSeconds);
	}

	TickAirlockVisuals(DeltaSeconds);
	TickHelmVisuals(DeltaSeconds);
	TickSound(DeltaSeconds);
	UpdateConsoleText(DeltaSeconds);
}

void AThalassaVessel::TickSound(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer || !HullAmbience)
	{
		return;
	}

	// The hull as this machine's player hears it: inside or out in the water, and how the ship is moving.
	float Inside = 1.f;
	if (const APlayerController* Local = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (const APawn* Pawn = Local->GetPawn())
		{
			Inside = IsWaterAt(Pawn->GetActorLocation()) ? 0.f : 1.f;
		}
	}
	HullAmbience->SetState(Inside, GetSpeedMetersPerSecond(), GetDepthMeters(), FMath::RadiansToDegrees(Snapshot.RotationRate.Size()));
	if (MachinerySound)
	{
		MachinerySound->SetLoad(Helm ? FMath::Max(FMath::Abs(Helm->GetLevers().Throttle), GetSpeedMetersPerSecond() / 14.f) : 0.f);
	}

	// Events, noticed as changes in replicated state. The first look only remembers, so joining is silent.
	const auto Changed = [](int32& Heard, int32 Now)
	{
		const bool bChanged = Heard >= 0 && Heard != Now;
		Heard = Now;
		return bChanged;
	};
	const int32 Collisions = GetCollisionCount();
	if (HeardCollisions >= 0 && Collisions > HeardCollisions && ImpactSound)
	{
		ImpactSound->Strike();
	}
	HeardCollisions = Collisions;

	if (Helm && HelmBleep)
	{
		const int32 Occupied = Helm->GetOccupant() ? 1 : 0;
		if (Changed(HeardHelmOccupied, Occupied))
		{
			HelmBleep->Bleep(Occupied ? EThalassaBleep::On : EThalassaBleep::Off);
		}
		if (Changed(HeardHelmMode, static_cast<int32>(Helm->GetMode())))
		{
			HelmBleep->Bleep(EThalassaBleep::Click);
		}
		const int32 FullStop = Helm->IsFullStop() ? 1 : 0;
		if (Changed(HeardFullStop, FullStop) && FullStop)
		{
			HelmBleep->Bleep(EThalassaBleep::Alarm);
		}
	}
	if (CameraStation && CameraBleep)
	{
		const int32 Occupied = CameraStation->GetOccupant() ? 1 : 0;
		if (Changed(HeardCameraOccupied, Occupied))
		{
			CameraBleep->Bleep(Occupied ? EThalassaBleep::On : EThalassaBleep::Off);
		}
	}
	if (Sonar && SonarBleep)
	{
		const int32 On = Sonar->IsSurveying() ? 1 : 0;
		if (Changed(HeardSonar, On))
		{
			SonarBleep->Bleep(On ? EThalassaBleep::On : EThalassaBleep::Off);
		}
	}
}

void AThalassaVessel::StartProfile()
{
	if (HasAuthority() && ProfileState == EVesselProfileState::Idle)
	{
		ProfileState = EVesselProfileState::Running;
		ProfileStartTime = GetWorld()->GetTimeSeconds();
	}
}

namespace
{
	/** Scenery: no standing on it, no bumping into it. Line traces, such as looking at a console, still work. */
	void MakeVisualOnly(UPrimitiveComponent* Primitive)
	{
		if (Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			return;
		}
		if (Primitive->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Ignore || Primitive->CanCharacterStepUpOn != ECB_No)
		{
			UE_LOG(LogTemp, Display, TEXT("THALASSA: %s.%s is scenery on the ship, so players now pass through it. Walkable collision belongs in the ship's one collision component."),
				*GetNameSafe(Primitive->GetOwner()), *Primitive->GetName());
		}
		Primitive->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Primitive->CanCharacterStepUpOn = ECB_No;
		Primitive->SetCanEverAffectNavigation(false);
	}
}

void AThalassaVessel::MakeArtVisualOnly()
{
	// Components added in the Blueprint. The ship's own, made in C++, are already set up as they must be.
	TInlineComponentArray<UPrimitiveComponent*> Primitives(this);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive->CreationMethod != EComponentCreationMethod::Native && !ThalassaShipMarkers::IsMarker(Primitive))
		{
			MakeVisualOnly(Primitive);
		}
	}

	// Actors attached to the ship, such as rooms built as their own Blueprints. Never a crew member.
	ArtActors.Reset();
	TArray<AActor*> Attached;
	GetAttachedActors(Attached, true, true);
	for (AActor* Actor : Attached)
	{
		if (!Actor || Actor->IsA<APawn>())
		{
			continue;
		}
		ArtActors.Add(Actor);
		TInlineComponentArray<UPrimitiveComponent*> ActorPrimitives(Actor);
		for (UPrimitiveComponent* Primitive : ActorPrimitives)
		{
			MakeVisualOnly(Primitive);
		}
	}
}

void AThalassaVessel::IgnoreShipInQuery(FCollisionQueryParams& Params) const
{
	Params.AddIgnoredActor(this);
	for (const TWeakObjectPtr<AActor>& Actor : ArtActors)
	{
		if (Actor.IsValid())
		{
			Params.AddIgnoredActor(Actor.Get());
		}
	}
}

bool AThalassaVessel::HasHelm() const
{
	return GetLayout().bHasHelm;
}

FVector AThalassaVessel::GetHelmSeatLocal() const
{
	return Helm->SeatLocal;
}

UThalassaStationComponent* AThalassaVessel::FindStationOf(const APawn* Pawn) const
{
	for (UThalassaStationComponent* Station : { static_cast<UThalassaStationComponent*>(Helm.Get()), static_cast<UThalassaStationComponent*>(CameraStation.Get()) })
	{
		if (Station && Station->IsSeated(Pawn))
		{
			return Station;
		}
	}
	return nullptr;
}

bool AThalassaVessel::TryUseStation(APawn* Who)
{
	if (UThalassaStationComponent* Current = FindStationOf(Who))
	{
		return Current->TrySitOrStand(Who); // Stand up.
	}
	for (UThalassaStationComponent* Station : { static_cast<UThalassaStationComponent*>(Helm.Get()), static_cast<UThalassaStationComponent*>(CameraStation.Get()) })
	{
		if (Station && Station->TrySitOrStand(Who))
		{
			return true;
		}
	}
	return false;
}

FVector AThalassaVessel::GetFullStopButtonLocal() const
{
	return FullStopButtonLocal;
}

void AThalassaVessel::TickHelmMotion(float DeltaSeconds)
{
	using namespace ThalassaVesselLayout;

	const FHelmLevers& Levers = Helm->GetLevers();
	float Speed = SpeedCmS / 100.f;

	if (Helm->IsFullStop())
	{
		// Engines astern, rudder and planes against the motion: the ship stops where it is and stays there.
		SpeedCmS = FMath::FInterpConstantTo(Speed, 0.f, DeltaSeconds, FullStopDecelMS2) * 100.f;
		VerticalSpeedCmS = FMath::FInterpConstantTo(VerticalSpeedCmS / 100.f, 0.f, DeltaSeconds, FullStopDecelMS2) * 100.f;
		TargetYawRate = 0.f;
		CurrentRate.Yaw = FMath::FInterpConstantTo(static_cast<float>(CurrentRate.Yaw), 0.f, DeltaSeconds, HelmYawAccelDegS2 * 2.f);
		TargetPitch = 0.f;
		TargetRoll = 0.f;
		return;
	}

	// Test boost: ten times the top speed in about the same time, so it still feels like the same heavy ship.
	const float Boost = Helm->IsTestBoost() ? UThalassaHelmComponent::TestBoostFactor : 1.f;
	const float EngineFactor = Damage ? Damage->GetEngineFactor() : 1.f;
	const float SteeringFactor = Damage ? Damage->GetSteeringFactor() : 1.f;
	const float Thrust = Boost * EngineFactor * Levers.Throttle * (Levers.Throttle >= 0.f ? HelmAheadAccelMS2 : HelmAsternAccelMS2);
	const float Drag = HelmQuadraticDrag / Boost * Speed * FMath::Abs(Speed) + HelmLinearDrag * Speed;
	Speed += (Thrust - Drag) * DeltaSeconds;
	if (Levers.Throttle == 0.f && FMath::Abs(Speed) < 0.02f)
	{
		Speed = 0.f;
	}
	SpeedCmS = Speed * 100.f;

	// Going astern, the rudder turns the other way, as it does on a real ship.
	const float Steerage = FMath::Clamp(FMath::Abs(Speed) / HelmFullSteerageMS, HelmMinSteerage, 1.f) * (Speed < -0.05f ? -1.f : 1.f);
	TargetYawRate = Levers.Rudder * HelmMaxYawRateDegS * Steerage * SteeringFactor;
	CurrentRate.Yaw = FMath::FInterpConstantTo(static_cast<float>(CurrentRate.Yaw), TargetYawRate, DeltaSeconds, HelmYawAccelDegS2);
	TargetPitch = Levers.Planes * HelmMaxPitchDeg * SteeringFactor;
	TargetRoll = 0.f;

	// The vertical thrusters: the ship rises or sinks level, slowly, and keeps drifting after you let go.
	const float TargetHeave = Levers.Heave * HelmMaxHeaveMS * Boost;
	VerticalSpeedCmS = FMath::FInterpConstantTo(VerticalSpeedCmS / 100.f, TargetHeave, DeltaSeconds, HelmHeaveAccelMS2 * Boost) * 100.f;

	// At the surface the planes and thrusters cannot lift the ship any further, so it levels off.
	if (IsSurfaced())
	{
		TargetPitch = FMath::Min(TargetPitch, 0.f);
		VerticalSpeedCmS = FMath::Min(VerticalSpeedCmS, 0.f);
	}
}

bool AThalassaVessel::IsSurfaced() const
{
	return GetActorLocation().Z >= -ThalassaVesselLayout::SurfacedDepthCm - 10.f;
}

float AThalassaVessel::GetSecondsSinceCollision() const
{
	return LastCollisionTime < 0.0 ? -1.f : static_cast<float>(GetSyncedTime() - LastCollisionTime);
}

bool AThalassaVessel::IsHullBlocked(const FVector& Location, const FQuat& Rotation) const
{
	const FBox& HullBox = HullBoundsLocal;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ThalassaHullOverlap), false);
	IgnoreShipInQuery(Params);
	return GetWorld()->OverlapAnyTestByObjectType(Location + Rotation.RotateVector(HullBox.GetCenter()), Rotation,
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeBox(HullBox.GetExtent()), Params);
}

FVector AThalassaVessel::ConstrainMove(const FVector& From, const FVector& To, const FQuat& Rotation, bool& bOutHit)
{
	using namespace ThalassaVesselLayout;
	FVector Result = To;

	// The surface: the ship can float there, not rise out of the water.
	const float SurfaceZ = -SurfacedDepthCm;
	if (Result.Z > SurfaceZ)
	{
		Result.Z = FMath::Max(SurfaceZ, FMath::Min(From.Z, Result.Z));
	}

	// The seabed and anything else in the world: sweep the hull's box along the move. Only world
	// geometry stops the ship, never a swimmer. The box turns with the ship, but a turn on the spot is
	// not swept: only the move from one place to the next is.
	const FBox& HullBox = HullBoundsLocal;
	const FVector Center = Rotation.RotateVector(HullBox.GetCenter());
	const FVector Delta = Result - From;
	if (Delta.IsNearlyZero())
	{
		return Result;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ThalassaHull), false);
	IgnoreShipInQuery(Params);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByObjectType(Hit, From + Center, Result + Center, Rotation,
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeBox(HullBox.GetExtent()), Params);
	if (!bHit)
	{
		return Result;
	}

	// Already touching: moving away is allowed, pushing further in is not.
	if (Hit.bStartPenetrating)
	{
		if (FVector::DotProduct(Delta, Hit.Normal) > 0.f)
		{
			return Result;
		}
		Result = From;
	}
	else
	{
		// Stop just short of the contact, but never back away from it: a pull-back made the pinned ship
		// shudder a few centimetres twice a second, creeping in and being pushed out again.
		const float Length = static_cast<float>(Delta.Size());
		Result = From + Delta * FMath::Max(0.f, Hit.Time - CollisionPullBackCm / Length);
	}

	// A ship this heavy stops dead. Shown on the helm; later it can do damage.
	SpeedCmS = 0.f;
	bOutHit = true;
	return Result;
}

void AThalassaVessel::TickHelmVisuals(float DeltaSeconds)
{
	if (!HelmLeverPivot || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Forward is ahead. A positive pitch would tip the lever's top toward the stern.
	ShownThrottle = FMath::FInterpTo(ShownThrottle, Helm->GetLevers().Throttle, DeltaSeconds, 12.f);
	HelmLeverPivot->SetRelativeRotation(FRotator(-ShownThrottle * ThalassaVesselLayout::HelmLeverSwingDeg, 0.f, 0.f));

	// The full stop button lights up and stays pressed in while the ship is in full stop.
	const bool bFullStop = Helm->IsFullStop();
	if (FullStopButton && bFullStop != bShownFullStop)
	{
		bShownFullStop = bFullStop;
		FullStopButton->SetRelativeLocation(FullStopButtonLocal - FVector(0.f, 0.f, bFullStop ? 2.f : 0.f));
		if (FullStopButtonMaterial)
		{
			FullStopButtonMaterial->SetVectorParameterValue(TEXT("Color"), bFullStop ? ThalassaVesselLayout::FullStopLitColor : ThalassaVesselLayout::FullStopIdleColor);
		}
	}
}

void AThalassaVessel::UpdateProfileTargets()
{
	float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - ProfileStartTime);
	const TArrayView<const ThalassaVesselLayout::FSegment> Segments = ThalassaTestConfig::IsEvaProfile()
		? TArrayView<const ThalassaVesselLayout::FSegment>(ThalassaVesselLayout::EvaProfile)
		: TArrayView<const ThalassaVesselLayout::FSegment>(ThalassaVesselLayout::Profile);
	for (const ThalassaVesselLayout::FSegment& Segment : Segments)
	{
		if (Elapsed < Segment.Duration)
		{
			const float Motion = ThalassaTestConfig::IsShipStatic() ? 0.f : 1.f;
			TargetSpeedCmS = Motion * Segment.SpeedMS * 100.f;
			TargetYawRate = Motion * Segment.YawRate;
			TargetPitch = Motion * Segment.Pitch;
			TargetRoll = Motion * Segment.Roll;
			return;
		}
		Elapsed -= Segment.Duration;
	}

	TargetSpeedCmS = 0.f;
	TargetYawRate = 0.f;
	TargetPitch = 0.f;
	TargetRoll = 0.f;
	ProfileState = EVesselProfileState::Finished;
}

void AThalassaVessel::TickServer(float DeltaSeconds)
{
	using namespace ThalassaVesselLayout;

	const bool bHelmTest = ThalassaTestConfig::IsHelmTest();
	if (ProfileState == EVesselProfileState::Running && !bHelmTest)
	{
		// The scripted motion test: unchanged, so results stay comparable with earlier runs.
		UpdateProfileTargets();
		SpeedCmS = FMath::FInterpConstantTo(SpeedCmS, TargetSpeedCmS, DeltaSeconds, AccelerationCmS2);
		CurrentRate.Yaw = FMath::FInterpConstantTo(static_cast<float>(CurrentRate.Yaw), TargetYawRate, DeltaSeconds, YawAccelerationDegS2);
	}
	else
	{
		// The helm test is timed like the motion test, but someone at the helm drives the ship.
		if (bHelmTest && ProfileState == EVesselProfileState::Running && GetWorld()->GetTimeSeconds() - ProfileStartTime > HelmTestSeconds)
		{
			ProfileState = EVesselProfileState::Finished;
		}
		TickHelmMotion(DeltaSeconds);
	}

	const float OldPitch = static_cast<float>(PoseRotation.Pitch);
	const float OldRoll = static_cast<float>(PoseRotation.Roll);
	const float NewPitch = FMath::FInterpConstantTo(OldPitch, TargetPitch, DeltaSeconds, AttitudeRateDegS);
	const float NewRoll = FMath::FInterpConstantTo(OldRoll, TargetRoll, DeltaSeconds, AttitudeRateDegS);
	CurrentRate.Pitch = DeltaSeconds > 0.f ? (NewPitch - OldPitch) / DeltaSeconds : 0.f;
	CurrentRate.Roll = DeltaSeconds > 0.f ? (NewRoll - OldRoll) / DeltaSeconds : 0.f;

	const FRotator OldRotation = PoseRotation;
	PoseRotation.Pitch = NewPitch;
	PoseRotation.Roll = NewRoll;
	PoseRotation.Yaw = FRotator::NormalizeAxis(PoseRotation.Yaw + CurrentRate.Yaw * DeltaSeconds);

	const FVector OldPosition = GetActorLocation();
	const bool bConstrained = ProfileState != EVesselProfileState::Running || bHelmTest;
	bool bHit = false;

	// The scripted test's path is fixed, so earlier results stay comparable. Under the helm, the
	// surface and the seabed are in the way. A long hull swings a long way when it turns or pitches,
	// so a turn that would put it into rock is refused first: 12 degrees of pitch moves the bow 29 m.
	if (bConstrained && PoseRotation != OldRotation && IsHullBlocked(OldPosition, PoseRotation.Quaternion()) && !IsHullBlocked(OldPosition, OldRotation.Quaternion()))
	{
		PoseRotation = OldRotation;
		CurrentRate = FRotator::ZeroRotator;
		bHit = true;
	}

	FVector NewPosition = OldPosition + (PoseRotation.Vector() * SpeedCmS + FVector::UpVector * VerticalSpeedCmS) * DeltaSeconds;
	if (bConstrained)
	{
		NewPosition = ConstrainMove(OldPosition, NewPosition, PoseRotation.Quaternion(), bHit);
	}

	// One contact is one collision, however long the hull stays pressed against the rock, and however it
	// bumps while settling: thrusters pushing down can lift off and touch again several times a second.
	if (bHit && !bTouching && GetWorld()->GetTimeSeconds() - LastCollisionTime > 1.5)
	{
		++CollisionCount;
		LastCollisionTime = GetWorld()->GetTimeSeconds();
		if (Damage)
		{
			// Where it hit: the point of the hull furthest along the way it was going.
			const FVector Moving = FVector(SpeedCmS, 0.f, 0.f) + GetActorTransform().InverseTransformVectorNoScale(FVector::UpVector * VerticalSpeedCmS);
			const float ImpactMS = Moving.Size() / 100.f;
			const FVector Ahead = HullBoundsLocal.GetCenter() + Moving.GetSafeNormal() * 1.0e6f;
			Damage->ApplyImpact(HullBoundsLocal.GetClosestPointTo(Ahead), FMath::Clamp((ImpactMS - 0.5f) / 5.f, 0.05f, 1.f), TEXT("collision"));
		}
		UE_LOG(LogTemp, Display, TEXT("SHIPCOLLISION %d at %s depth %.1f m"), CollisionCount, *NewPosition.ToString(), -NewPosition.Z / 100.0);
	}
	if (bHit)
	{
		VerticalSpeedCmS = 0.f; // Pressed against rock or the surface: the drift stops.
	}
	bTouching = bHit;
	const FVector Velocity = DeltaSeconds > 0.f ? (NewPosition - OldPosition) / DeltaSeconds : FVector::ZeroVector;
	SetActorLocationAndRotation(NewPosition, PoseRotation, false, nullptr, ETeleportType::None);

	Snapshot.Position = NewPosition;
	Snapshot.Velocity = Velocity;
	Snapshot.Rotation = PoseRotation;
	Snapshot.RotationRate = FVector(CurrentRate.Pitch, CurrentRate.Yaw, CurrentRate.Roll);
	Snapshot.ServerTime = GetWorld()->GetTimeSeconds();
}

double AThalassaVessel::GetSyncedTime() const
{
	const UWorld* World = GetWorld();
	return HasAuthority() ? World->GetTimeSeconds() : World->GetTimeSeconds() + ServerClockOffset;
}

void AThalassaVessel::GetExtrapolatedPose(double Time, FVector& OutPosition, FQuat& OutRotation) const
{
	// Bounded so a long network stall makes the ship hold course briefly rather than fly away.
	const float Age = FMath::Clamp(static_cast<float>(Time - Snapshot.ServerTime), -0.25f, 0.75f);
	OutPosition = Snapshot.Position + Snapshot.Velocity * Age;
	const FRotator Rotated = Snapshot.Rotation + FRotator(Snapshot.RotationRate.X * Age, Snapshot.RotationRate.Y * Age, Snapshot.RotationRate.Z * Age);
	OutRotation = Rotated.Quaternion();
}

void AThalassaVessel::UpdateClockFromSnapshot()
{
	using namespace ThalassaVesselLayout;

	// The clock tracks server time as of each snapshot's arrival, not "true" server time. Everything
	// replicated reaches this client about equally late, so showing the ship that late keeps it
	// consistent with what is around it. GameState's GetServerWorldTimeSeconds is not used: it is a
	// long running average, which takes around 25 seconds to recover from a stall on either machine.
	const double LocalTime = GetWorld()->GetTimeSeconds();
	const double MeasuredOffset = Snapshot.ServerTime - LocalTime;

	if (!bHasClockOffset || FMath::Abs(MeasuredOffset - ServerClockOffset) > ClockSnapErrorSeconds)
	{
		ServerClockOffset = MeasuredOffset;
		bHasClockOffset = true;
	}
	else
	{
		const double Elapsed = FMath::Max(LocalTime - LastClockSampleTime, 0.0);
		const double Blend = 1.0 - FMath::Exp(-ClockSmoothingRate * Elapsed);
		ServerClockOffset += (MeasuredOffset - ServerClockOffset) * Blend;
	}
	LastClockSampleTime = LocalTime;
}

void AThalassaVessel::OnRep_Snapshot()
{
	UpdateClockFromSnapshot();

	if (!bHasSnapshot)
	{
		bHasSnapshot = true;
		SetActorLocationAndRotation(Snapshot.Position, Snapshot.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	// Keep what is on screen continuous: turn the jump to the new target into a decaying offset.
	FVector NewTargetPosition;
	FQuat NewTargetRotation;
	GetExtrapolatedPose(GetSyncedTime(), NewTargetPosition, NewTargetRotation);

	PoseErrorOffset = GetActorLocation() - NewTargetPosition;
	PoseErrorRotation = GetActorQuat() * NewTargetRotation.Inverse();

	if (PoseErrorOffset.SizeSquared() > FMath::Square(ThalassaVesselLayout::SnapErrorDistanceCm))
	{
		PoseErrorOffset = FVector::ZeroVector;
		PoseErrorRotation = FQuat::Identity;
	}
}

void AThalassaVessel::TickClient(float DeltaSeconds)
{
	using namespace ThalassaVesselLayout;

	if (!bHasSnapshot)
	{
		return;
	}

	const float Decay = FMath::Exp(-PoseSmoothingRate * DeltaSeconds);
	PoseErrorOffset *= Decay;
	PoseErrorRotation = FQuat::Slerp(FQuat::Identity, PoseErrorRotation, Decay);

	FVector TargetPosition;
	FQuat TargetRotation;
	GetExtrapolatedPose(GetSyncedTime(), TargetPosition, TargetRotation);

	SetActorLocationAndRotation(TargetPosition + PoseErrorOffset, PoseErrorRotation * TargetRotation, false, nullptr, ETeleportType::None);
}

FTransform AThalassaVessel::GetSpawnTransform(int32 PlayerIndex) const
{
	const FVesselLayout& Layout = GetLayout();
	FVector Local = ThalassaTestConfig::IsSpawnAtAirlock()
		? AirlockLocal + (Layout.AirlockViewpoint - Layout.AirlockOffset)
		: SpawnLocals[PlayerIndex % SpawnLocals.Num()];
	ThalassaTestConfig::GetSpawnOverride(Local);
	return FTransform(GetActorRotation(), GetActorTransform().TransformPosition(Local));
}

bool AThalassaVessel::TryPressBeep(const FVector& WorldLocation)
{
	if (!HasAuthority())
	{
		return false;
	}

	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	if (FVector::DistSquared(Local, GetLayout().ConsoleLocation) > FMath::Square(ThalassaVesselLayout::ConsoleUseRadius))
	{
		return false;
	}

	++BeepCount;
	return true;
}

bool AThalassaVessel::IsInsideHull(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	return HullBoundsLocal.IsInsideOrOn(Local);
}

UPrimitiveComponent* AThalassaVessel::GetMovementBaseComponent() const
{
	if (WalkableShell)
	{
		return ShellCollision;
	}
	return HullCollision;
}

bool AThalassaVessel::IsInCompartment(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	if (AirlockChamberBox().IsInsideOrOn(Local))
	{
		return true;
	}
	for (const FShipZone& Compartment : DryZones)
	{
		if (Compartment.Contains(Local))
		{
			return true;
		}
	}
	return false;
}

bool AThalassaVessel::IsWaterAt(const FVector& WorldLocation) const
{
	if (!IsInCompartment(WorldLocation))
	{
		return true;
	}

	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	if (AirlockChamberBox().IsInsideOrOn(Local))
	{
		return Local.Z < AirlockWaterLevel * ThalassaVesselLayout::AirlockCeilingCm;
	}
	// A flooded compartment: water up to its level.
	if (Damage)
	{
		for (int32 Index = 0; Index < DryZones.Num(); ++Index)
		{
			const FShipZone& Zone = DryZones[Index];
			if (Zone.Contains(Local))
			{
				const float Level = Damage->GetFloodLevel(Index);
				return Level > 0.f && Zone.Transform.InverseTransformPositionNoScale(Local).Z < -Zone.Extent.Z + Level * 2.f * Zone.Extent.Z;
			}
		}
	}
	return false;
}

bool AThalassaVessel::GetInteriorWaterSurface(const FVector& WorldLocation, float& OutSurfaceZ) const
{
	if (!IsInCompartment(WorldLocation))
	{
		return false;
	}
	const FTransform& Ship = GetActorTransform();
	const FVector Local = Ship.InverseTransformPosition(WorldLocation);
	if (AirlockChamberBox().IsInsideOrOn(Local))
	{
		const float Top = AirlockWaterLevel * ThalassaVesselLayout::AirlockCeilingCm;
		if (AirlockWaterLevel <= 0.f || Local.Z >= Top)
		{
			return false;
		}
		// Full to the ceiling: the top is above the chamber, so the view is all water.
		const float TopCm = AirlockWaterLevel >= 0.995f ? Top + 500.f : Top;
		OutSurfaceZ = static_cast<float>(Ship.TransformPosition(FVector(Local.X, Local.Y, TopCm)).Z);
		return true;
	}
	if (Damage)
	{
		for (int32 Index = 0; Index < DryZones.Num(); ++Index)
		{
			const FShipZone& Zone = DryZones[Index];
			if (Zone.Contains(Local))
			{
				const float Level = Damage->GetFloodLevel(Index);
				FVector InZone = Zone.Transform.InverseTransformPositionNoScale(Local);
				const float Top = -Zone.Extent.Z + Level * 2.f * Zone.Extent.Z;
				if (Level <= 0.f || InZone.Z >= Top)
				{
					return false;
				}
				InZone.Z = Level >= 0.995f ? Top + 500.f : Top;
				OutSurfaceZ = static_cast<float>(Ship.TransformPosition(Zone.Transform.TransformPositionNoScale(InZone)).Z);
				return true;
			}
		}
	}
	return false;
}

bool AThalassaVessel::IsLadderAt(const FVector& WorldLocation, float MarginCm) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	for (const FShipZone& Ladder : LadderZoneShapes)
	{
		if (Ladder.Contains(Local, MarginCm))
		{
			return true;
		}
	}
	return false;
}

const TArray<FVector>& AThalassaVessel::GetTourRoute() const
{
	return GetLayout().TourRoute;
}

bool AThalassaVessel::IsDoorwayOccupied(float DoorY) const
{
	for (TActorIterator<AThalassaCharacter> It(GetWorld()); It; ++It)
	{
		const FVector Local = GetActorTransform().InverseTransformPosition(It->GetActorLocation()) - AirlockLocal;
		if (FMath::Abs(Local.Y - DoorY) < ThalassaVesselLayout::DoorClearanceCm && FMath::Abs(Local.X) < 130.f)
		{
			return true;
		}
	}
	return false;
}

bool AThalassaVessel::TryCycleAirlock(const FVector& WorldLocation)
{
	using namespace ThalassaVesselLayout;

	if (!HasAuthority())
	{
		return false;
	}

	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	if (FVector::DistSquared(Local, AirlockPanelLocation + AirlockLocal) > FMath::Square(AirlockPanelRadius))
	{
		return false;
	}

	// The interlock: a cycle only starts from a fully open state, and never shuts a door on someone.
	if (AirlockState == EAirlockState::InnerOpen)
	{
		bAirlockDoorwayBlocked = IsDoorwayOccupied(DoorInnerY);
		if (!bAirlockDoorwayBlocked)
		{
			AirlockState = EAirlockState::Flooding;
			return true;
		}
	}
	else if (AirlockState == EAirlockState::OuterOpen)
	{
		bAirlockDoorwayBlocked = IsDoorwayOccupied(DoorOuterY);
		if (!bAirlockDoorwayBlocked)
		{
			AirlockState = EAirlockState::Draining;
			return true;
		}
	}
	return false;
}

void AThalassaVessel::TickAirlockServer(float DeltaSeconds)
{
	using namespace ThalassaVesselLayout;

	// A refused press only explains itself while it is still true.
	if (bAirlockDoorwayBlocked)
	{
		const float DoorY = AirlockState == EAirlockState::InnerOpen ? DoorInnerY : DoorOuterY;
		bAirlockDoorwayBlocked = IsDoorwayOccupied(DoorY);
	}

	// For looking at the water: -ThalassaAirlockWater=<level> holds the chamber's water at a level (0 to 1).
	float HeldLevel = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("ThalassaAirlockWater="), HeldLevel))
	{
		AirlockWaterLevel = FMath::Clamp(HeldLevel, 0.f, 1.f);
		return;
	}

	// The water only moves once the door that was open has fully closed.
	const float WaterStep = DeltaSeconds / AirlockFloodSeconds;
	if (AirlockState == EAirlockState::Flooding && AirlockInnerDoorOpen <= 0.f)
	{
		AirlockWaterLevel = FMath::Min(AirlockWaterLevel + WaterStep, 1.f);
		if (AirlockWaterLevel >= 1.f)
		{
			AirlockState = EAirlockState::OuterOpen;
		}
	}
	else if (AirlockState == EAirlockState::Draining && AirlockOuterDoorOpen <= 0.f)
	{
		AirlockWaterLevel = FMath::Max(AirlockWaterLevel - WaterStep, 0.f);
		if (AirlockWaterLevel <= 0.f)
		{
			AirlockState = EAirlockState::InnerOpen;
		}
	}
}

void AThalassaVessel::TickAirlockVisuals(float DeltaSeconds)
{
	using namespace ThalassaVesselLayout;

	// Doors animate locally from the replicated state, with collision, on server and clients alike.
	const float DoorStep = DeltaSeconds / DoorSeconds;
	const auto MoveDoor = [DoorStep](UStaticMeshComponent* Door, float& Open, bool bWantOpen, float DoorY)
	{
		const float Target = bWantOpen ? 1.f : 0.f;
		if (Open != Target)
		{
			Open = FMath::FInterpConstantTo(Open, Target, 1.f, DoorStep);
		}
		Door->SetRelativeLocation(FVector(0.f, DoorY, 125.f + Open * DoorOpenRaiseCm));

		// A door stops being solid as soon as it starts opening. Clients hear about it late, so their
		// door is always lower than the server's. If it were solid, they would bump into a door the
		// server says is not there, and be corrected. A door that shuts is solid again immediately.
		Door->SetCollisionEnabled(bWantOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	};
	MoveDoor(AirlockInnerDoor, AirlockInnerDoorOpen, AirlockState == EAirlockState::InnerOpen, DoorInnerY);
	MoveDoor(AirlockOuterDoor, AirlockOuterDoorOpen, AirlockState == EAirlockState::OuterOpen, DoorOuterY);

	// The old flat plane only marks where the water is; the surface draws it.
	AirlockWater->SetVisibility(false);
	AirlockWater->SetRelativeLocation(FVector(0.f, 337.f, AirlockWaterLevel * AirlockCeilingCm));
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!AirlockSurface && AirlockWaterLevel > 0.f)
	{
		AirlockSurface = NewObject<UThalassaWaterSurfaceComponent>(this, TEXT("AirlockSurface"));
		AirlockSurface->SetupAttachment(AirlockWater->GetAttachParent());
		AirlockSurface->RegisterComponent();
		// The chamber's floor plan, as the plane covered it (100 cm, scaled).
		AirlockSurface->Build(FVector2D(400.f, 175.f));
	}
	if (AirlockSurface)
	{
		// Hidden right at the ceiling too: there is no top to see when it's full.
		const bool bShown = AirlockWaterLevel > 0.f && AirlockWaterLevel < 0.995f;
		AirlockSurface->SetVisibility(bShown);
		AirlockSurface->SetRelativeLocation(FVector(0.f, 337.f, AirlockWaterLevel * AirlockCeilingCm));
		if (bShown)
		{
			// Rough while the sea pours in or drains out, settling over a second or two when it stops. (On clients
			// the level arrives in steps, hence the easing.)
			const float Pouring = AirlockWaterLevel != AirlockLevelSeen ? 1.f : 0.f;
			AirlockChurn = FMath::Lerp(AirlockChurn, Pouring, 1.f - FMath::Exp(-DeltaSeconds / (Pouring > AirlockChurn ? 0.2f : 1.2f)));
			AirlockSurface->UpdateWater(DeltaSeconds, 0.05f + AirlockChurn * 0.8f, AirlockWaterLevel * AirlockCeilingCm);
		}
	}
	AirlockLevelSeen = AirlockWaterLevel;
}

void AThalassaVessel::UpdateConsoleText(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// A physical display does not need to refresh every frame.
	ConsoleTextTimer -= DeltaSeconds;
	if (ConsoleTextTimer > 0.f)
	{
		return;
	}
	ConsoleTextTimer = 0.25f;

	ConsoleText->SetText(FText::FromString(FString::Printf(
		TEXT("BEEP %d\nDEPTH %.0f m  SPD %.1f m/s\nHDG %03.0f"),
		BeepCount, GetDepthMeters(), GetSpeedMetersPerSecond(),
		FRotator::ClampAxis(GetActorRotation().Yaw))));
}

void AThalassaVessel::PlayDamageAlarm()
{
	// A collision has already thudded through the hull (TickSound); anything else that hits the ship thuds here.
	const float SinceCollision = GetSecondsSinceCollision();
	if (ImpactSound && (SinceCollision < 0.f || SinceCollision > 0.6f))
	{
		ImpactSound->Strike();
	}
	if (HelmBleep)
	{
		HelmBleep->Bleep(EThalassaBleep::Alarm);
	}
}
