#include "Vessel/ThalassaVesselLayout.h"

namespace
{
	const FLinearColor HullColor(0.10f, 0.13f, 0.16f);
	const FLinearColor FloorColor(0.20f, 0.24f, 0.27f);
	const FLinearColor CorridorColor(0.28f, 0.25f, 0.20f);
	const FLinearColor CrewColor(0.12f, 0.26f, 0.24f);
	const FLinearColor GatherColor(0.22f, 0.15f, 0.30f);
	const FLinearColor BridgeColor(0.08f, 0.22f, 0.30f);
	const FLinearColor EngineColor(0.35f, 0.18f, 0.10f);
	const FLinearColor MachineColor(0.45f, 0.30f, 0.10f);
	const FLinearColor AirlockColor(0.45f, 0.36f, 0.08f);
	const FLinearColor ConsoleColor(0.03f, 0.04f, 0.05f);

	constexpr float CeilingZ = 350.f;
	constexpr float DoorHeight = 250.f;
	constexpr float WallThickness = 30.f;
	constexpr float PlateTop = -100.f; // Under the floor tiles, so that sunken areas have somewhere to sink to.

	/** Builds hull pieces with stable, predictable names, so server and clients always agree. */
	struct FBuilder
	{
		FVesselLayout& Layout;
		int32 Next = 0;

		void Box(const FVector& Min, const FVector& Max, const FLinearColor& Color, bool bVisible = true, bool bCollision = true)
		{
			FHullBox Piece;
			Piece.Name = FName(*FString::Printf(TEXT("Hull%03d"), Next++));
			Piece.Center = (Min + Max) * 0.5f;
			Piece.Size = Max - Min;
			Piece.Color = Color;
			Piece.bVisible = bVisible;
			Piece.bCollision = bCollision;
			Layout.Boxes.Add(Piece);
		}

		/** A wall along X at a given Y, with doorways. Each doorway is a centre X and a width. */
		void WallX(float Y, float X0, float X1, const FLinearColor& Color, std::initializer_list<FVector2f> Doors = {}, float Bottom = 0.f)
		{
			float Cursor = X0;
			for (const FVector2f& Door : Doors)
			{
				const float Left = Door.X - Door.Y * 0.5f;
				const float Right = Door.X + Door.Y * 0.5f;
				Box(FVector(Cursor, Y - WallThickness * 0.5f, Bottom), FVector(Left, Y + WallThickness * 0.5f, CeilingZ), Color);
				Box(FVector(Left, Y - WallThickness * 0.5f, DoorHeight), FVector(Right, Y + WallThickness * 0.5f, CeilingZ), Color);
				Cursor = Right;
			}
			Box(FVector(Cursor, Y - WallThickness * 0.5f, Bottom), FVector(X1, Y + WallThickness * 0.5f, CeilingZ), Color);
		}

		/** A wall along Y at a given X, with doorways. Each doorway is a centre Y and a width. */
		void WallY(float X, float Y0, float Y1, const FLinearColor& Color, std::initializer_list<FVector2f> Doors = {}, float Bottom = 0.f)
		{
			float Cursor = Y0;
			for (const FVector2f& Door : Doors)
			{
				const float Left = Door.X - Door.Y * 0.5f;
				const float Right = Door.X + Door.Y * 0.5f;
				Box(FVector(X - WallThickness * 0.5f, Cursor, Bottom), FVector(X + WallThickness * 0.5f, Left, CeilingZ), Color);
				Box(FVector(X - WallThickness * 0.5f, Left, DoorHeight), FVector(X + WallThickness * 0.5f, Right, CeilingZ), Color);
				Cursor = Right;
			}
			Box(FVector(X - WallThickness * 0.5f, Cursor, Bottom), FVector(X + WallThickness * 0.5f, Y1, CeilingZ), Color);
		}

		/** A floor tile with its top at TopZ, with a rectangular hole cut out of it. */
		void FloorAround(const FBox& Area, const FBox& Hole, float TopZ, const FLinearColor& Color)
		{
			const float Bottom = PlateTop;
			Box(FVector(Area.Min.X, Area.Min.Y, Bottom), FVector(Hole.Min.X, Area.Max.Y, TopZ), Color);
			Box(FVector(Hole.Max.X, Area.Min.Y, Bottom), FVector(Area.Max.X, Area.Max.Y, TopZ), Color);
			Box(FVector(Hole.Min.X, Area.Min.Y, Bottom), FVector(Hole.Max.X, Hole.Min.Y, TopZ), Color);
			Box(FVector(Hole.Min.X, Hole.Max.Y, Bottom), FVector(Hole.Max.X, Area.Max.Y, TopZ), Color);
		}

		/** Hull section: base plate, ceiling, and optionally the floor tile. Y extents differ by section. */
		void Section(float X0, float X1, float Y0, float Y1, float FloorTop, bool bFloorTile)
		{
			Box(FVector(X0, Y0, FloorTop - 50.f), FVector(X1, Y1, FloorTop), FloorColor);
			if (bFloorTile)
			{
				Box(FVector(X0, Y0, FloorTop), FVector(X1, Y1, 0.f), FloorColor);
			}
			Box(FVector(X0, Y0, CeilingZ), FVector(X1, Y1, CeilingZ + 50.f), HullColor);
			Layout.DryCompartments.Add(FBox(FVector(X0, Y0, FloorTop - 50.f), FVector(X1, Y1, CeilingZ + 50.f)));
			Layout.HullBounds += FBox(FVector(X0, Y0, FloorTop - 50.f), FVector(X1, Y1, CeilingZ + 50.f));
		}

		/**
		 * A straight flight of steps along X, starting at StartX at the top. The steps are only seen.
		 * You walk on a hidden ramp: on real steps, character movement goes downstairs as a run of
		 * tiny falls, which looks like stumbling and shows up as corrections.
		 */
		void Stairs(float StartX, float Y0, float Y1, float TopZ, float BottomZ, float RunPerStep, const FLinearColor& Color)
		{
			constexpr float Rise = 20.f;
			const int32 Count = FMath::RoundToInt((TopZ - BottomZ) / Rise);
			for (int32 Step = 0; Step < Count; ++Step)
			{
				const float Near = StartX + RunPerStep * Step;
				const float Far = Near + RunPerStep;
				Box(FVector(FMath::Min(Near, Far), Y0, BottomZ), FVector(FMath::Max(Near, Far), Y1, TopZ - Rise * Step), Color, true, false);
			}

			// The ramp runs from the top front edge to the bottom of the last step.
			const float Run = FMath::Abs(RunPerStep) * Count;
			const float Drop = TopZ - BottomZ;
			const float Length = FMath::Sqrt(Run * Run + Drop * Drop);
			constexpr float Thickness = 10.f;
			// Positive pitch raises the +X end. Steps running toward -X are highest at their +X end.
			const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Drop, Run)) * (RunPerStep < 0.f ? 1.f : -1.f);
			FHullBox Ramp;
			Ramp.Name = FName(*FString::Printf(TEXT("Hull%03d"), Next++));
			Ramp.Center = FVector(StartX + RunPerStep * Count * 0.5f, (Y0 + Y1) * 0.5f, (TopZ + BottomZ) * 0.5f - Thickness * 0.5f);
			Ramp.Size = FVector(Length, Y1 - Y0, Thickness);
			Ramp.Rotation = FRotator(Pitch, 0.f, 0.f);
			Ramp.Color = Color;
			Ramp.bVisible = false;
			Layout.Boxes.Add(Ramp);
		}

		void Light(const FVector& Location, const FColor& Color)
		{
			FInteriorLight Lamp;
			Lamp.Name = FName(*FString::Printf(TEXT("Lamp%02d"), Layout.Lights.Num()));
			Lamp.Location = Location;
			Lamp.Color = Color;
			Layout.Lights.Add(Lamp);
		}
	};
}

FVesselLayout BuildMainDeckLayout()
{
	FVesselLayout Layout;
	FBuilder B{ Layout };

	const FColor Warm(255, 224, 190);
	const FColor Amber(255, 170, 120);
	const FColor Cool(205, 230, 255);

	// ---------------------------------------------- Hull: deliberately stepped and lopsided, not a tube.
	const FBox MessPit(FVector(-4200.f, -1000.f, 0.f), FVector(200.f, 600.f, 0.f));
	const FBox BridgePit(FVector(11000.f, -800.f, 0.f), FVector(13300.f, 800.f, 0.f));

	B.Section(-14000.f, -8500.f, -1500.f, 1500.f, -400.f, false); // Engineering hall: tall, with a lower floor.
	B.Section(-8500.f, -6000.f, -2200.f, 2200.f, PlateTop, true);
	B.Section(-6000.f, 6000.f, -2500.f, 2500.f, PlateTop, false);
	B.FloorAround(FBox(FVector(-6000.f, -2500.f, 0.f), FVector(6000.f, 2500.f, 0.f)), MessPit, 0.f, FloorColor);
	B.Section(6000.f, 9500.f, -1800.f, 2000.f, PlateTop, true);
	B.Section(9500.f, 14000.f, -1200.f, 1200.f, PlateTop, false);
	B.FloorAround(FBox(FVector(9500.f, -1200.f, 0.f), FVector(14000.f, 1200.f, 0.f)), BridgePit, 0.f, FloorColor);
	B.Section(500.f, 3500.f, -3100.f, -2500.f, PlateTop, true); // Lounge blister, port side only.

	// Outer walls, section by section.
	// Engineering's floor is 4 m lower, so its walls must reach all the way down to it.
	constexpr float EngineFloor = -450.f;
	B.WallX(-1500.f, -14000.f, -8500.f, EngineColor, {}, EngineFloor);
	B.WallX(1500.f, -14000.f, -8500.f, EngineColor, {}, EngineFloor);
	B.WallX(-2200.f, -8500.f, -6000.f, HullColor);
	B.WallX(-2500.f, -6000.f, 500.f, HullColor);
	B.WallX(-2500.f, 3500.f, 6000.f, HullColor);
	B.WallX(2500.f, -6000.f, 6000.f, HullColor);
	B.WallX(-1800.f, 6000.f, 9500.f, HullColor);
	B.WallX(2000.f, 6000.f, 9500.f, HullColor);
	B.WallX(-1200.f, 9500.f, 14000.f, BridgeColor);
	B.WallX(1200.f, 9500.f, 14000.f, BridgeColor);
	B.Box(FVector(-14015.f, -1500.f, -400.f), FVector(-13985.f, 1500.f, CeilingZ), EngineColor);
	B.WallY(-6000.f, -2500.f, -2200.f, HullColor);
	B.WallY(-6000.f, 2200.f, 2500.f, HullColor);
	B.WallY(6000.f, -2500.f, -1800.f, HullColor);
	B.WallY(6000.f, 2000.f, 2500.f, HullColor);
	B.WallY(9500.f, -1800.f, -1200.f, HullColor);
	B.WallY(9500.f, 1200.f, 2000.f, HullColor);
	B.WallY(500.f, -3100.f, -2500.f, HullColor);
	B.WallY(3500.f, -3100.f, -2500.f, HullColor);

	// The lounge's great window, looking out to port. The pane is solid but not rendered.
	B.Box(FVector(500.f, -3115.f, 0.f), FVector(3500.f, -3085.f, 60.f), GatherColor);
	B.Box(FVector(500.f, -3115.f, 290.f), FVector(3500.f, -3085.f, CeilingZ), GatherColor);
	B.Box(FVector(500.f, -3115.f, 60.f), FVector(3500.f, -3085.f, 290.f), HullColor, false);

	// The bridge's forward window, the same way.
	B.Box(FVector(13985.f, -1200.f, 0.f), FVector(14015.f, 1200.f, 90.f), BridgeColor);
	B.Box(FVector(13985.f, -1200.f, 300.f), FVector(14015.f, 1200.f, CeilingZ), BridgeColor);
	B.Box(FVector(13985.f, -1200.f, 90.f), FVector(14015.f, 1200.f, 300.f), HullColor, false);

	// ---------------------------------------------- The ring corridor, off-centre on purpose.
	// Outer edge X -5000..3000, Y -1700..1300. Corridor 400 wide. Inner block X -4600..2600, Y -1300..900.
	B.WallX(-1700.f, -6000.f, 500.f, CorridorColor, { FVector2f(-4000.f, 200.f), FVector2f(-2400.f, 200.f), FVector2f(-800.f, 200.f) });
	B.WallX(-1700.f, 500.f, 3500.f, GatherColor, { FVector2f(2000.f, 500.f) });
	B.WallX(1300.f, -6000.f, 3000.f, CorridorColor, { FVector2f(-4200.f, 200.f), FVector2f(-2000.f, 200.f), FVector2f(1800.f, 200.f) });
	B.WallY(-5000.f, -1700.f, 1300.f, CorridorColor, { FVector2f(-300.f, 400.f) });
	B.WallY(3000.f, -1700.f, 1300.f, CorridorColor, { FVector2f(-300.f, 400.f) });

	// Inner block: mess hall aft, galley and lifts forward.
	B.WallX(-1300.f, -4600.f, 2600.f, GatherColor, { FVector2f(-1000.f, 400.f), FVector2f(1800.f, 200.f) });
	B.WallX(900.f, -4600.f, 2600.f, GatherColor, { FVector2f(-2000.f, 400.f), FVector2f(1200.f, 200.f) });
	B.WallY(-4600.f, -1300.f, 900.f, GatherColor, { FVector2f(-200.f, 400.f) });
	B.WallY(2600.f, -1300.f, 900.f, CrewColor, { FVector2f(-300.f, 200.f) });
	B.WallY(600.f, -1300.f, 900.f, CrewColor, { FVector2f(-500.f, 200.f) });
	B.WallX(-300.f, 600.f, 2600.f, CrewColor, { FVector2f(1500.f, 200.f) }); // Galley / lift lobby.

	// Mess hall floor: two steps down all the way round, into a sunken centre.
	const float PitMidX0 = MessPit.Min.X + 60.f, PitMidX1 = MessPit.Max.X - 60.f;
	const float PitMidY0 = MessPit.Min.Y + 60.f, PitMidY1 = MessPit.Max.Y - 60.f;
	B.FloorAround(FBox(MessPit.Min, MessPit.Max), FBox(FVector(PitMidX0, PitMidY0, 0.f), FVector(PitMidX1, PitMidY1, 0.f)), -25.f, FloorColor);
	B.Box(FVector(PitMidX0, PitMidY0, PlateTop), FVector(PitMidX1, PitMidY1, -50.f), GatherColor);
	for (const FVector2f& Table : { FVector2f(-3000.f, -500.f), FVector2f(-2000.f, 200.f), FVector2f(-900.f, -600.f) })
	{
		B.Box(FVector(Table.X - 200.f, Table.Y - 80.f, -50.f), FVector(Table.X + 200.f, Table.Y + 80.f, 25.f), HullColor);
	}

	// Side rooms: uneven on each side, so the two halves of the ring do not look alike.
	B.WallY(-3200.f, -2500.f, -1700.f, CrewColor);
	B.WallY(-1600.f, -2500.f, -1700.f, CrewColor);
	B.WallY(-4800.f, -2500.f, -1700.f, CrewColor);
	B.WallY(-3000.f, 1300.f, 2500.f, CrewColor);
	B.WallY(0.f, 1300.f, 2500.f, CrewColor);

	// ---------------------------------------------- Aft: spine, stores, EVA room and airlock.
	B.WallX(-500.f, -6000.f, -5000.f, CorridorColor);
	B.WallX(-100.f, -6000.f, -5000.f, CorridorColor);
	B.WallY(-6000.f, -2200.f, 2200.f, CorridorColor, { FVector2f(-300.f, 400.f) });
	B.WallX(-800.f, -8500.f, -6000.f, CrewColor, { FVector2f(-7000.f, 200.f) });
	B.WallX(500.f, -8500.f, -6000.f, AirlockColor, { FVector2f(-7000.f, 400.f) });

	// The airlock opens through the hull at Y 2200. Same pieces as the test rig, moved here.
	Layout.AirlockOffset = FVector(-7000.f, 1750.f, 0.f);
	const FVector A = Layout.AirlockOffset;
	B.WallX(1975.f, -8500.f, -7100.f, AirlockColor);
	B.WallX(1975.f, -6900.f, -6000.f, AirlockColor);
	B.Box(FVector(A.X - 100.f, 1960.f, DoorHeight), FVector(A.X + 100.f, 1990.f, CeilingZ), AirlockColor);
	B.Box(A + FVector(200.f, 225.f, 0.f), A + FVector(250.f, 475.f, CeilingZ), AirlockColor);
	B.Box(A + FVector(-250.f, 225.f, 0.f), A + FVector(-200.f, 475.f, CeilingZ), AirlockColor);
	B.WallX(2200.f, -8500.f, -7250.f, HullColor);
	B.WallX(2200.f, -6750.f, -6000.f, HullColor);
	B.Box(A + FVector(100.f, 425.f, 0.f), A + FVector(250.f, 475.f, CeilingZ), AirlockColor);
	B.Box(A + FVector(-250.f, 425.f, 0.f), A + FVector(-100.f, 475.f, CeilingZ), AirlockColor);
	B.Box(A + FVector(-100.f, 425.f, DoorHeight), A + FVector(100.f, 475.f, CeilingZ), AirlockColor);

	// ---------------------------------------------- Engineering: a tall hall below a catwalk.
	B.WallY(-8500.f, -2200.f, 2200.f, EngineColor, { FVector2f(-300.f, 400.f) });
	B.Box(FVector(-8515.f, -2200.f, EngineFloor), FVector(-8485.f, 2200.f, 0.f), EngineColor); // Below the doorway too.
	B.Box(FVector(-9500.f, -1500.f, -40.f), FVector(-8500.f, 1500.f, 0.f), MachineColor); // Catwalk.

	// Stairs down from the catwalk, port side. Twenty steps of 20 cm.
	B.Stairs(-9500.f, -1300.f, -1000.f, 0.f, -400.f, -32.f, MachineColor);

	// A ladder from the hall floor up to the catwalk, starboard side. Rails and rungs are only visual.
	Layout.LadderZones.Add(FBox(FVector(-9660.f, 860.f, -400.f), FVector(-9500.f, 1120.f, 110.f)));
	B.Box(FVector(-9520.f, 900.f, -400.f), FVector(-9510.f, 910.f, 90.f), MachineColor, true, false);
	B.Box(FVector(-9520.f, 1090.f, -400.f), FVector(-9510.f, 1100.f, 90.f), MachineColor, true, false);
	for (float Rung = -370.f; Rung < 90.f; Rung += 40.f)
	{
		B.Box(FVector(-9520.f, 910.f, Rung), FVector(-9512.f, 1090.f, Rung + 6.f), MachineColor, true, false);
	}

	// Machinery, so the hall has something to walk around.
	B.Box(FVector(-12800.f, -600.f, -400.f), FVector(-11200.f, 600.f, 200.f), EngineColor); // Reactor block.
	B.Box(FVector(-13500.f, 800.f, -400.f), FVector(-12300.f, 1400.f, -100.f), MachineColor);
	B.Box(FVector(-11000.f, -1400.f, -400.f), FVector(-10500.f, -1000.f, 50.f), MachineColor);

	// ---------------------------------------------- Forward: spine, branch, and side rooms.
	B.WallX(-500.f, 3000.f, 9500.f, CorridorColor, { FVector2f(4000.f, 200.f), FVector2f(7600.f, 200.f) });
	B.WallX(-100.f, 3000.f, 4300.f, CorridorColor);
	B.WallX(-100.f, 4700.f, 9500.f, CorridorColor, { FVector2f(6800.f, 200.f), FVector2f(8700.f, 200.f) });
	B.WallY(4300.f, -100.f, 2500.f, CorridorColor, { FVector2f(1100.f, 200.f) });
	B.WallY(4700.f, -100.f, 2000.f, CorridorColor, { FVector2f(600.f, 200.f) });
	B.WallX(2000.f, 4300.f, 4700.f, CorridorColor);
	B.WallY(5300.f, -2500.f, -500.f, CrewColor, { FVector2f(-1300.f, 200.f) });
	B.WallY(7400.f, -100.f, 2000.f, CrewColor);
	B.WallY(6000.f, -500.f, -100.f, CorridorColor, { FVector2f(-300.f, 400.f) });

	// ---------------------------------------------- The bridge: tiered, stepping down toward the window.
	B.WallY(9500.f, -1200.f, 1200.f, BridgeColor, { FVector2f(-300.f, 400.f) });
	const float BridgeMidX0 = BridgePit.Min.X + 90.f, BridgeMidX1 = BridgePit.Max.X - 90.f;
	const float BridgeMidY0 = BridgePit.Min.Y + 90.f, BridgeMidY1 = BridgePit.Max.Y - 90.f;
	B.FloorAround(FBox(BridgePit.Min, BridgePit.Max), FBox(FVector(BridgeMidX0, BridgeMidY0, 0.f), FVector(BridgeMidX1, BridgeMidY1, 0.f)), -20.f, BridgeColor);
	const float BridgeLowX0 = BridgeMidX0 + 90.f, BridgeLowX1 = BridgeMidX1 - 90.f;
	const float BridgeLowY0 = BridgeMidY0 + 90.f, BridgeLowY1 = BridgeMidY1 - 90.f;
	B.FloorAround(FBox(FVector(BridgeMidX0, BridgeMidY0, 0.f), FVector(BridgeMidX1, BridgeMidY1, 0.f)),
		FBox(FVector(BridgeLowX0, BridgeLowY0, 0.f), FVector(BridgeLowX1, BridgeLowY1, 0.f)), -40.f, BridgeColor);
	B.Box(FVector(BridgeLowX0, BridgeLowY0, PlateTop), FVector(BridgeLowX1, BridgeLowY1, -60.f), HullColor);

	// Stations around the upper tier, the captain's position on the lowest, the helm by the window.
	for (const FVector2f& Station : { FVector2f(10000.f, 950.f), FVector2f(11200.f, 1000.f), FVector2f(12400.f, 1000.f), FVector2f(10000.f, -1000.f), FVector2f(11700.f, -1000.f), FVector2f(13000.f, -1000.f) })
	{
		B.Box(FVector(Station.X - 250.f, Station.Y - 50.f, 0.f), FVector(Station.X + 250.f, Station.Y + 50.f, 110.f), ConsoleColor);
	}
	// The sonar station: the starboard console in the middle, with the chart on a screen above it, facing the bridge.
	B.Box(FVector(11110.f, 1036.f, 110.f), FVector(11290.f, 1050.f, 290.f), ConsoleColor, true, false);
	Layout.SonarScreenLocation = FVector(11200.f, 1034.f, 200.f);
	Layout.SonarScreenFacing = FVector(0.f, -1.f, 0.f);
	Layout.SonarScreenSizeCm = 160.f;

	B.Box(FVector(11900.f, -60.f, -60.f), FVector(12020.f, 60.f, 40.f), MachineColor);
	B.Box(FVector(13400.f, -400.f, 0.f), FVector(13560.f, 400.f, 120.f), ConsoleColor);

	// The helm: you stand at the middle of the console by the window. Its screen is to port, so the
	// view ahead stays clear, and its throttle lever to starboard.
	Layout.bHasHelm = true;
	Layout.HelmSeatLocation = FVector(13330.f, 0.f, 90.f);
	B.Box(FVector(13486.f, -330.f, 120.f), FVector(13500.f, -170.f, 260.f), ConsoleColor, true, false);
	Layout.HelmScreenLocation = FVector(13484.f, -250.f, 190.f);
	Layout.HelmScreenFacing = FVector(-1.f, 0.f, 0.f);
	Layout.HelmScreenSizeCm = 130.f;
	Layout.HelmLeverLocation = FVector(13450.f, 160.f, 120.f);
	Layout.ConsoleLocation = FVector(13300.f, 0.f, 60.f);
	Layout.ConsoleTextLocation = FVector(13396.f, 0.f, 90.f);

	// ---------------------------------------------- Lights, spawns, and the walker's tour.
	for (const FVector& Lamp : TArray<FVector>{
		FVector(-4800.f, -1500.f, 320.f), FVector(-1000.f, -1500.f, 320.f), FVector(2800.f, -1500.f, 320.f),
		FVector(-4800.f, 1100.f, 320.f), FVector(-1000.f, 1100.f, 320.f), FVector(2800.f, 1100.f, 320.f),
		FVector(-5500.f, -300.f, 320.f), FVector(-7300.f, -300.f, 320.f), FVector(4200.f, -300.f, 320.f), FVector(7500.f, -300.f, 320.f),
		FVector(4500.f, 1000.f, 320.f), FVector(-7000.f, 2090.f, 320.f), FVector(-7300.f, 1200.f, 320.f),
		FVector(1500.f, 400.f, 320.f), FVector(1500.f, -800.f, 320.f) })
	{
		B.Light(Lamp, Warm);
	}
	for (const FVector& Lamp : TArray<FVector>{ FVector(-3000.f, -200.f, 320.f), FVector(-500.f, -200.f, 320.f), FVector(2000.f, -2600.f, 320.f) })
	{
		B.Light(Lamp, Warm);
	}
	for (const FVector& Lamp : TArray<FVector>{ FVector(-9000.f, 0.f, 320.f), FVector(-10500.f, 800.f, 320.f), FVector(-13000.f, 0.f, 320.f), FVector(-10500.f, -800.f, 320.f) })
	{
		B.Light(Lamp, Amber);
	}
	for (const FVector& Lamp : TArray<FVector>{ FVector(10300.f, -300.f, 320.f), FVector(12000.f, 0.f, 320.f), FVector(13300.f, 0.f, 320.f) })
	{
		B.Light(Lamp, Cool);
	}

	Layout.Spawns = { FVector(-800.f, -1450.f, 120.f), FVector(-400.f, -1550.f, 120.f), FVector(0.f, -1450.f, 120.f), FVector(400.f, -1550.f, 120.f),
		FVector(800.f, -1450.f, 120.f), FVector(1200.f, -1550.f, 120.f), FVector(1600.f, -1450.f, 120.f), FVector(2000.f, -1550.f, 120.f) };
	Layout.AirlockViewpoint = FVector(-7000.f, 1300.f, 120.f);

	// Everywhere the plan says you can go. If a waypoint is unreachable, the test reports it as stuck.
	Layout.TourRoute = {
		FVector(-1000.f, -1500.f, 90.f), FVector(-1000.f, -1100.f, 90.f), FVector(-1500.f, -100.f, 40.f), FVector(-1000.f, -1100.f, 90.f), // Mess hall and back.
		FVector(-1000.f, -1500.f, 90.f), FVector(2000.f, -1500.f, 90.f), FVector(2000.f, -2800.f, 90.f), FVector(2000.f, -1500.f, 90.f),    // Lounge blister.
		FVector(2800.f, -1500.f, 90.f), FVector(2800.f, -300.f, 90.f), FVector(4500.f, -300.f, 90.f),                                         // East side of the ring.
		FVector(4500.f, 1000.f, 90.f), FVector(4500.f, -300.f, 90.f), FVector(9000.f, -300.f, 90.f), FVector(10300.f, -300.f, 90.f),          // Branch, spine.
		FVector(12000.f, -300.f, 40.f), FVector(10300.f, -300.f, 90.f), FVector(4500.f, -300.f, 90.f), FVector(2800.f, -300.f, 90.f),         // Bridge and back.
		FVector(2800.f, 1100.f, 90.f), FVector(-4800.f, 1100.f, 90.f), FVector(-4800.f, -300.f, 90.f), FVector(-7000.f, -300.f, 90.f),        // Round to the aft.
		FVector(-9000.f, -300.f, 90.f), FVector(-9300.f, -1150.f, 90.f), FVector(-10300.f, -1150.f, -310.f),                                  // Stairs down.
		FVector(-10000.f, 1000.f, -310.f), FVector(-9570.f, 1000.f, -310.f), FVector(-9570.f, 1000.f, 90.f), FVector(-9200.f, 1000.f, 90.f),  // Ladder up.
		FVector(-9000.f, -300.f, 90.f), FVector(-7000.f, -300.f, 90.f), FVector(-4800.f, -300.f, 90.f), FVector(-4800.f, -1500.f, 90.f),      // Home.
	};

	return Layout;
}
