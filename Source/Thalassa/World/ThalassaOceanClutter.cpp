#include "World/ThalassaOceanClutter.h"

#include "Async/Async.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tools/ThalassaPerf.h"
#include "Vessel/ThalassaVessel.h"
#include "World/ThalassaOcean.h"

namespace ThalassaClutter
{
	using EProvince = FThalassaSeabedGenerator::EProvince;

	// The meshes, by index. Instances name their mesh by this index, so the order is part of the rules.
	const TCHAR* const MeshPaths[] =
	{
		// 0-4: small Poly Haven scans, used as stones and boulders.
		TEXT("/Game/World/Rocks/moon_rock_01/moon_rock_01/StaticMeshes/moon_rock_01_LOD0"),
		TEXT("/Game/World/Rocks/moon_rock_03/moon_rock_03/StaticMeshes/moon_rock_03"),
		TEXT("/Game/World/Rocks/moon_rock_05/moon_rock_05/StaticMeshes/moon_rock_05"),
		TEXT("/Game/World/Rocks/rock_07/rock_07/StaticMeshes/rock_07"),
		TEXT("/Game/World/Rocks/rock_09/rock_09/StaticMeshes/rock_09"),
		// 5-7: the big scans: two boulders and a coastal rock mass.
		TEXT("/Game/World/Rocks/namaqualand_boulder_03/namaqualand_boulder_03/StaticMeshes/namaqualand_boulder_03"),
		TEXT("/Game/World/Rocks/namaqualand_boulder_05/namaqualand_boulder_05/StaticMeshes/namaqualand_boulder_05"),
		TEXT("/Game/World/Rocks/coast_rocks_05/coast_rocks_05/StaticMeshes/coast_rocks_05"),
		// 8-13: fractured dark boulders, for volcanic ground.
		TEXT("/Game/World/DarkBiome/SM_DarkBoulder_01"), TEXT("/Game/World/DarkBiome/SM_DarkBoulder_02"), TEXT("/Game/World/DarkBiome/SM_DarkBoulder_03"),
		TEXT("/Game/World/DarkBiome/SM_DarkBoulder_04"), TEXT("/Game/World/DarkBiome/SM_DarkBoulder_05"), TEXT("/Game/World/DarkBiome/SM_DarkBoulder_06"),
		// 14-19: slab towers, 15 to 24 m tall at scale 1.
		TEXT("/Game/World/Formations/SM_SlabTower_01"), TEXT("/Game/World/Formations/SM_SlabTower_02"), TEXT("/Game/World/Formations/SM_SlabTower_03"),
		TEXT("/Game/World/Formations/SM_SlabTower_04"), TEXT("/Game/World/Formations/SM_SlabTower_05"), TEXT("/Game/World/Formations/SM_SlabTower_06"),
		// 20-24: broken slab fragments.
		TEXT("/Game/World/Formations/SM_SlabFragment_01"), TEXT("/Game/World/Formations/SM_SlabFragment_02"), TEXT("/Game/World/Formations/SM_SlabFragment_03"),
		TEXT("/Game/World/Formations/SM_SlabFragment_04"), TEXT("/Game/World/Formations/SM_SlabFragment_05"),
		// 25-28: Meshy AI landmarks, made by the owner (import_meshy.py): a layered ledge, a weathered arch, a cluster
		// of rounded stones, a tall monolith. Optional: any that are missing are simply left out.
		TEXT("/Game/World/Meshy/RockyCliffLedge/SM_Meshy_RockyCliffLedge"), TEXT("/Game/World/Meshy/WeatheredStoneArch/SM_Meshy_WeatheredStoneArch"),
		TEXT("/Game/World/Meshy/WeatheredStoneCluster/SM_Meshy_WeatheredStoneCluster"), TEXT("/Game/World/Meshy/WeatheredStoneMonolith/SM_Meshy_WeatheredStoneMonolith"),
	};
	constexpr int32 MeshCount = UE_ARRAY_COUNT(MeshPaths);
	constexpr uint8 SmallScans = 0, BigBoulders = 5, RockMass = 7, DarkBoulders = 8, SlabTowers = 14, SlabFragments = 20, Ledge = 25, Arch = 26, Cluster = 27, Monolith = 28;
	constexpr uint8 FirstOptionalMesh = Ledge;

	// Layers: each has its own lattice, how far away it is drawn, and its own salt.
	enum ELayer : uint8 { Stones, Boulders, Masses, Towers, Landmarks, LayerCount };
	constexpr double LayerCellM[LayerCount] = { 8.0, 32.0, 128.0, 256.0, 256.0 };
	constexpr float CullCm[LayerCount] = { 25000.f, 80000.f, 200000.f, 250000.f, 300000.f };

	// How likely a cell is to hold something, by province: plains, hills, plateaus, ridges, canyons, volcanic.
	// The plains stay mostly empty on purpose; ridges and volcanic fields are strewn with rock.
	constexpr double Density[LayerCount][6] =
	{
		{ 0.02, 0.08, 0.06, 0.16, 0.12, 0.25 },     // Stones, per 8 m cell.
		{ 0.03, 0.14, 0.10, 0.30, 0.22, 0.30 },     // Boulders, per 32 m cell.
		{ 0.02, 0.08, 0.10, 0.22, 0.15, 0.12 },     // Rock masses, per 128 m cell.
		{ 0.004, 0.03, 0.08, 0.06, 0.04, 0.0 },     // Slab tower clusters, per 256 m cell.
		{ 0.01, 0.03, 0.04, 0.05, 0.06, 0.02 },     // Landmarks (a ledge, an arch, a stone cluster or a monolith), per 256 m cell.
	};

	constexpr double SolidFromCm = 300.0;   // Boulders this big and up are solid.
	constexpr int32 NearReach = 1;          // Stones: patches this many each way around the camera.
	constexpr int32 FarReach = 3;           // Everything bigger.
	constexpr int32 SolidReach = 1;         // On the host, around every player and the ship.
	constexpr double CommitBudgetSeconds = 0.003;
	constexpr int32 MaxInFlight = 8;

	FORCEINLINE uint64 Hash64(uint64 X)
	{
		X += 0x9E3779B97F4A7C15ull;
		X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
		X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
		return X ^ (X >> 31);
	}

	FORCEINLINE uint64 CellHash(int64 X, int64 Y, uint64 Salt)
	{
		return Hash64(Hash64(static_cast<uint64>(X) ^ (Salt * 0xD6E8FEB86659FD93ull)) ^ (static_cast<uint64>(Y) * 0xA0761D6478BD642Full));
	}

	/** A stream of uniform numbers from one cell's hash. */
	struct FDraws
	{
		uint64 State;
		explicit FDraws(uint64 H) : State(H) {}
		double Next() { State = Hash64(State); return static_cast<double>(State >> 11) * (1.0 / 9007199254740992.0); }
		double Range(double Low, double High) { return Low + (High - Low) * Next(); }
		int32 Pick(int32 First, int32 Count) { return First + FMath::Min(Count - 1, static_cast<int32>(Next() * Count)); }
	};

	FORCEINLINE double Smooth(double Edge0, double Edge1, double X)
	{
		const double T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	/** Where to put things, worked out once per spot. */
	struct FGround
	{
		const FThalassaSeabedGenerator& Gen;

		double Height(double XM, double YM) const { return Gen.HeightM(XM, YM); }

		/** The slope's gradient: rise over run. */
		FVector2D Gradient(double XM, double YM) const
		{
			constexpr double D = 2.0;
			return FVector2D((Height(XM + D, YM) - Height(XM - D, YM)) / (2.0 * D), (Height(XM, YM + D) - Height(XM, YM - D)) / (2.0 * D));
		}

		/** The province here, with borders dithered so they never show as a line. */
		EProvince Province(double XM, double YM, double Dither) const
		{
			EProvince Neighbour;
			double Weight;
			const EProvince Here = Gen.ProvinceAt(XM, YM, Neighbour, Weight);
			return Dither < Weight ? Here : Neighbour;
		}

		/** Clumping: some stretches of ground are strewn with rock and others nearly bare. About 0.9 on average. */
		double Clumping(double XM, double YM, uint64 Seed) const
		{
			const uint64 H = CellHash(static_cast<int64>(FMath::FloorToDouble(XM / 128.0)), static_cast<int64>(FMath::FloorToDouble(YM / 128.0)), Seed ^ 0xC1A0u);
			const double C = static_cast<double>(Hash64(H) >> 11) * (1.0 / 9007199254740992.0);
			return 0.15 + 2.2 * C * C;
		}
	};

	/** A rock settled into the ground: sunk to the lowest point under it, leaning partly with the slope. */
	FTransform Settle(const FGround& Ground, FDraws& Draw, double XM, double YM, double SizeM, double Sink, const FVector& Scale, double Tilt)
	{
		const double R = SizeM * 0.4;
		double Z = Ground.Height(XM, YM);
		Z = FMath::Min(Z, FMath::Min(FMath::Min(Ground.Height(XM + R, YM), Ground.Height(XM - R, YM)), FMath::Min(Ground.Height(XM, YM + R), Ground.Height(XM, YM - R))));
		const FVector2D G = Ground.Gradient(XM, YM);
		const FVector Normal = FVector(-G.X, -G.Y, 1.0).GetSafeNormal();
		const FVector Up = (FVector::UpVector + Normal).GetSafeNormal();
		const FQuat Lean = FQuat::FindBetweenNormals(FVector::UpVector, Up);
		const FQuat Wobble = FRotator(Draw.Range(-Tilt, Tilt), Draw.Range(0.0, 360.0), Draw.Range(-Tilt, Tilt)).Quaternion();
		return FTransform(Lean * Wobble, FVector(XM, YM, Z - Sink * SizeM) * 100.0, Scale);
	}

	void Add(FClutterPatch& Patch, uint8 Mesh, ELayer Layer, bool bSolid, const FTransform& Transform)
	{
		FClutterInstance& Instance = Patch.Instances.AddDefaulted_GetRef();
		Instance.Mesh = Mesh;
		Instance.Layer = Layer;
		Instance.bSolid = bSolid;
		Instance.Transform = Transform;
	}

	/** A scanned or modelled rock scaled so its largest dimension is SizeM. */
	void Rock(FClutterPatch& Patch, const FGround& Ground, FDraws& Draw, const TArray<double>& Sizes, uint8 Mesh, ELayer Layer,
		double XM, double YM, double SizeM, double Sink = 0.15)
	{
		const double Scale = SizeM * 100.0 / FMath::Max(Sizes[Mesh], 1.0);
		const FVector Scale3(Scale, Scale, Scale * Draw.Range(0.75, 1.1));
		Add(Patch, Mesh, Layer, SizeM * 100.0 >= SolidFromCm, Settle(Ground, Draw, XM, YM, SizeM, Sink, Scale3, 12.0));
	}

	uint8 BoulderMesh(FDraws& Draw, EProvince Province)
	{
		if (Province == EProvince::Volcanic)
		{
			return static_cast<uint8>(Draw.Pick(DarkBoulders, 6));
		}
		static const uint8 Kinds[] = { BigBoulders, BigBoulders + 1, SmallScans, SmallScans + 3 };
		return Kinds[Draw.Pick(0, 4)];
	}
}

UThalassaOceanClutter::UThalassaOceanClutter()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UThalassaOceanClutter::Place(const FThalassaSeabedGenerator& Generator, uint64 Seed, const TArray<double>& Sizes, const TArray<double>& Bases, FClutterPatch& Patch)
{
	using namespace ThalassaClutter;
	const FGround Ground{ Generator };
	const double OriginX = Patch.Key.X * PatchM, OriginY = Patch.Key.Y * PatchM;

	// A ledge and an arch near the start, clear of the ship, so the landmarks can be seen straight away.
	if (!Patch.Key.bNear && Patch.Key.X == 0 && Patch.Key.Y == 0 && Sizes[Ledge] > 0.0 && Sizes[Arch] > 0.0)
	{
		FDraws Draw(CellHash(0, 0, Seed ^ 0x1A4D3A4Bu));
		const double ArchScale = 5500.0 / Sizes[Arch], LedgeScale = 8000.0 / Sizes[Ledge];
		const FTransform ArchAt = Settle(Ground, Draw, 200.0, 150.0, 55.0, 0.08, FVector(ArchScale), 1.0);
		Add(Patch, Arch, Landmarks, true, FTransform(FRotator(0.0, 90.0, 0.0), ArchAt.GetLocation() + FVector(0.0, 0.0, Bases[Arch] * ArchScale), ArchAt.GetScale3D()));
		const FTransform LedgeAt = Settle(Ground, Draw, 70.0, 220.0, 80.0, 0.1, FVector(LedgeScale), 1.0);
		Add(Patch, Ledge, Landmarks, true, FTransform(FRotator(0.0, -20.0, 0.0), LedgeAt.GetLocation() + FVector(0.0, 0.0, Bases[Ledge] * LedgeScale), LedgeAt.GetScale3D()));
	}
	// A monolith and a stone cluster on the other side of the start.
	if (!Patch.Key.bNear && Patch.Key.X == 0 && Patch.Key.Y == -1 && Sizes[Monolith] > 0.0 && Sizes[Cluster] > 0.0)
	{
		FDraws Draw(CellHash(0, -1, Seed ^ 0x1A4D3A4Bu));
		const double MonolithScale = 5500.0 / Sizes[Monolith], ClusterScale = 3500.0 / Sizes[Cluster];
		const FTransform MonolithAt = Settle(Ground, Draw, 120.0, -140.0, 30.0, 0.05, FVector(MonolithScale), 1.0);
		Add(Patch, Monolith, Landmarks, true, FTransform(FRotator(0.0, 10.0, 0.0), MonolithAt.GetLocation() + FVector(0.0, 0.0, Bases[Monolith] * MonolithScale), MonolithAt.GetScale3D()));
		const FTransform ClusterAt = Settle(Ground, Draw, 200.0, -90.0, 35.0, 0.1, FVector(ClusterScale), 1.0);
		Add(Patch, Cluster, Landmarks, true, FTransform(FRotator(0.0, 60.0, 0.0), ClusterAt.GetLocation() + FVector(0.0, 0.0, Bases[Cluster] * ClusterScale), ClusterAt.GetScale3D()));
	}

	for (int32 L = Patch.Key.bNear ? Stones : Boulders; L < (Patch.Key.bNear ? Boulders : LayerCount); ++L)
	{
		const ELayer Layer = static_cast<ELayer>(L);
		const int32 Cells = FMath::RoundToInt(PatchM / LayerCellM[L]);
		for (int32 J = 0; J < Cells; ++J)
		{
			for (int32 I = 0; I < Cells; ++I)
			{
				const int64 CX = Patch.Key.X * Cells + I, CY = Patch.Key.Y * Cells + J;
				FDraws Draw(CellHash(CX, CY, Seed ^ (0x5CA7u + L * 0x1000193u)));
				const double XM = OriginX + (I + Draw.Range(0.1, 0.9)) * LayerCellM[L];
				const double YM = OriginY + (J + Draw.Range(0.1, 0.9)) * LayerCellM[L];
				const EProvince Province = Ground.Province(XM, YM, Draw.Next());
				const double Roll = Draw.Next();
				const double Base = Density[L][static_cast<int32>(Province)];
				if (Roll >= Base * 9.0)
				{
					continue; // Cheap rejection before looking at the ground: nothing here even at the steepest.
				}
				const double Slope = Ground.Gradient(XM, YM).Size();
				double Chance = Base;
				switch (Layer)
				{
				case Stones:
				case Boulders:
					// Scree: loose rock gathers on slopes, but cliffs are too steep to hold it.
					Chance *= (1.0 + 2.5 * Smooth(0.25, 0.7, Slope)) * (Slope > 1.4 ? 0.0 : 1.0) * Ground.Clumping(XM, YM, Seed);
					break;
				case Masses:
					Chance *= 1.0 + Smooth(0.15, 0.5, Slope);
					break;
				case Towers:
					Chance *= Slope < 0.35 ? 1.0 : 0.0; // They stand on level ground.
					break;
				case Landmarks:
					Chance *= Slope < 0.6 ? 1.0 : 0.0;
					break;
				default:
					break;
				}
				if (Roll >= Chance)
				{
					continue;
				}

				switch (Layer)
				{
				case Stones:
				{
					const uint8 Mesh = Province == EProvince::Volcanic ? static_cast<uint8>(Draw.Pick(DarkBoulders, 6)) : static_cast<uint8>(Draw.Pick(SmallScans, 5));
					Rock(Patch, Ground, Draw, Sizes, Mesh, Layer, XM, YM, Draw.Range(0.3, 2.0));
					break;
				}
				case Boulders:
				{
					// One boulder, often with smaller pieces broken off around it.
					const double Size = 1.5 + 6.0 * Draw.Next() * Draw.Next();
					Rock(Patch, Ground, Draw, Sizes, BoulderMesh(Draw, Province), Layer, XM, YM, Size);
					if (Draw.Next() < 0.4)
					{
						const int32 Pieces = Draw.Pick(1, 4);
						for (int32 K = 0; K < Pieces; ++K)
						{
							const double Angle = Draw.Range(0.0, 2.0 * PI), Distance = Size * Draw.Range(0.8, 2.0);
							Rock(Patch, Ground, Draw, Sizes, BoulderMesh(Draw, Province), Layer,
								XM + FMath::Cos(Angle) * Distance, YM + FMath::Sin(Angle) * Distance, Size * Draw.Range(0.2, 0.5));
						}
					}
					break;
				}
				case Masses:
				{
					const uint8 Mesh = Province == EProvince::Volcanic ? static_cast<uint8>(Draw.Pick(DarkBoulders, 6))
						: Draw.Next() < 0.6 ? RockMass : BigBoulders;
					Rock(Patch, Ground, Draw, Sizes, Mesh, Layer, XM, YM, Draw.Range(10.0, 28.0), 0.2);
					break;
				}
				case Landmarks:
				{
					// A big formation on its own: a layered ledge, 50 to 90 m long; an arch, 35 to 60 m across; a cluster
					// of rounded stones, 20 to 40 m; or a monolith, 30 to 70 m tall. Only those whose meshes exist.
					struct FKind { uint8 Mesh; double Low, High; };
					static const FKind Kinds[] = { { Ledge, 50.0, 90.0 }, { Arch, 35.0, 60.0 }, { Cluster, 20.0, 40.0 }, { Monolith, 30.0, 70.0 } };
					const FKind& Kind = Kinds[Draw.Pick(0, UE_ARRAY_COUNT(Kinds))];
					if (Sizes[Kind.Mesh] <= 0.0)
					{
						break;
					}
					const double Size = Draw.Range(Kind.Low, Kind.High);
					const double Scale = Size * 100.0 / Sizes[Kind.Mesh];
					// Big formations sink a little deeper, so on a slope their low edge doesn't float.
					FTransform Placed = Settle(Ground, Draw, XM, YM, Size, 0.1, FVector(Scale), 2.0);
					Placed.AddToTranslation(Placed.GetRotation().RotateVector(FVector(0.0, 0.0, Bases[Kind.Mesh] * Scale)));
					Add(Patch, Kind.Mesh, Layer, true, Placed);
					break;
				}
				case Towers:
				{
					// A cluster: a few slab towers, fallen fragments, and boulders around their feet.
					const int32 Count = Draw.Pick(1, 3);
					for (int32 K = 0; K < Count; ++K)
					{
						const double Angle = Draw.Range(0.0, 2.0 * PI), Distance = K == 0 ? 0.0 : Draw.Range(15.0, 40.0);
						const double TX = XM + FMath::Cos(Angle) * Distance, TY = YM + FMath::Sin(Angle) * Distance;
						const double Scale = Draw.Range(0.8, 1.6);
						const FTransform Tower = Settle(Ground, Draw, TX, TY, 9.0 * Scale, 0.05, FVector(Scale), 3.0);
						Add(Patch, static_cast<uint8>(Draw.Pick(SlabTowers, 6)), Layer, true, FTransform(FRotator(Draw.Range(-3.0, 3.0), Draw.Range(0.0, 360.0), Draw.Range(-3.0, 3.0)).Quaternion(), Tower.GetLocation(), Tower.GetScale3D()));
						const int32 Fragments = Draw.Pick(1, 3);
						for (int32 F = 0; F < Fragments; ++F)
						{
							const double A = Draw.Range(0.0, 2.0 * PI), D = Draw.Range(8.0, 20.0) * Scale;
							const double FScale = Draw.Range(0.7, 1.3);
							Add(Patch, static_cast<uint8>(Draw.Pick(SlabFragments, 5)), Layer, true,
								Settle(Ground, Draw, TX + FMath::Cos(A) * D, TY + FMath::Sin(A) * D, 4.0 * FScale, 0.1, FVector(FScale), 25.0));
						}
						const int32 Rubble = Draw.Pick(4, 5);
						for (int32 B = 0; B < Rubble; ++B)
						{
							const double A = Draw.Range(0.0, 2.0 * PI), D = Draw.Range(12.0, 35.0);
							Rock(Patch, Ground, Draw, Sizes, static_cast<uint8>(Draw.Pick(SmallScans, 5)), Layer, TX + FMath::Cos(A) * D, TY + FMath::Sin(A) * D, Draw.Range(1.0, 5.0));
						}
					}
					break;
				}
				default:
					break;
				}
			}
		}
	}
}

void UThalassaOceanClutter::BeginPlay()
{
	using namespace ThalassaClutter;
	Super::BeginPlay();
	bDisabled = FParse::Param(FCommandLine::Get(), TEXT("ThalassaNoClutter"));

	TArray<double> Sizes, Bases;
	for (int32 Index = 0; Index < MeshCount; ++Index)
	{
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPaths[Index]);
		if (!Mesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("THALASSA: ocean clutter has no mesh at %s%s"), MeshPaths[Index], Index >= FirstOptionalMesh ? TEXT(" (optional: left out)") : TEXT(""));
			bDisabled |= Index < FirstOptionalMesh;
		}
		Meshes.Add(Mesh);
		// A missing optional mesh has size 0, which its placement rules check.
		Sizes.Add(Mesh ? Mesh->GetBounds().BoxExtent.GetMax() * 2.0 : (Index >= FirstOptionalMesh ? 0.0 : 100.0));
		Bases.Add(Mesh ? -(Mesh->GetBounds().Origin.Z - Mesh->GetBounds().BoxExtent.Z) : 0.0);
	}

	const AThalassaOcean* Ocean = Cast<AThalassaOcean>(GetOwner());
	Shared = MakeShared<FShared, ESPMode::ThreadSafe>(static_cast<uint64>(Ocean ? Ocean->Seed : 0), Sizes, Bases);
	UE_LOG(LogTemp, Display, TEXT("THALASSA: ocean clutter %s, %d meshes"), bDisabled ? TEXT("off") : TEXT("on"), MeshCount);
}

void UThalassaOceanClutter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Shared.IsValid())
	{
		Shared->bAlive.store(false);
	}
	Super::EndPlay(EndPlayReason);
}

void UThalassaOceanClutter::UpdateWanted()
{
	using namespace ThalassaClutter;
	Wanted.Reset();
	const auto PatchOf = [](const FVector& Location)
	{
		const double SizeCm = PatchM * 100.0;
		return FIntVector2(static_cast<int32>(FMath::FloorToDouble(Location.X / SizeCm)), static_cast<int32>(FMath::FloorToDouble(Location.Y / SizeCm)));
	};
	const auto AddAround = [&](const FVector& Location, bool bNear, int32 Reach)
	{
		const FIntVector2 Here = PatchOf(Location);
		for (int32 DY = -Reach; DY <= Reach; ++DY)
		{
			for (int32 DX = -Reach; DX <= Reach; ++DX)
			{
				Wanted.Add({ bNear, Here.X + DX, Here.Y + DY });
			}
		}
	};

	if (GetNetMode() != NM_DedicatedServer)
	{
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			AddAround(Camera->GetCameraLocation(), true, NearReach);
			AddAround(Camera->GetCameraLocation(), false, FarReach);
		}
	}
	// The host also needs the solid rocks wherever anyone is, so the ship and divers meet them there.
	if (GetOwner()->HasAuthority())
	{
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			AddAround(It->GetActorLocation(), false, SolidReach);
		}
		if (const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld()))
		{
			AddAround(Vessel->GetActorLocation(), false, SolidReach + 1);
		}
	}
	else if (const APlayerController* Local = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (const APawn* Pawn = Local->GetPawn())
		{
			AddAround(Pawn->GetActorLocation(), false, SolidReach);
		}
	}
}

void UThalassaOceanClutter::Launch(const FClutterPatchKey& Key)
{
	InFlight.Add(Key);
	TSharedPtr<FShared, ESPMode::ThreadSafe> State = Shared;
	Async(EAsyncExecution::ThreadPool, [State, Key]()
	{
		if (!State->bAlive.load())
		{
			return;
		}
		FClutterPatch Patch;
		Patch.Key = Key;
		Place(State->Generator, State->Seed, State->MeshSizesCm, State->MeshBaseCm, Patch);
		if (State->bAlive.load())
		{
			State->Finished.Enqueue(MoveTemp(Patch));
		}
	});
}

void UThalassaOceanClutter::Commit(FClutterPatch&& Patch)
{
	using namespace ThalassaClutter;
	THALASSA_PERF_SCOPE("clutter commit");
	InFlight.Remove(Patch.Key);
	if (!Wanted.Contains(Patch.Key) || Built.Contains(Patch.Key))
	{
		return;
	}

	// One instanced component per mesh, solidity and layer.
	TMap<uint32, TArray<FTransform>> Groups;
	for (const FClutterInstance& Instance : Patch.Instances)
	{
		Groups.FindOrAdd(Instance.Mesh | (Instance.bSolid ? 0x100u : 0u) | (static_cast<uint32>(Instance.Layer) << 9)).Add(Instance.Transform);
	}

	TArray<TObjectPtr<UInstancedStaticMeshComponent>>& Components = Built.Add(Patch.Key);
	for (TPair<uint32, TArray<FTransform>>& Group : Groups)
	{
		const uint8 Mesh = static_cast<uint8>(Group.Key & 0xFF);
		const bool bSolid = (Group.Key & 0x100u) != 0;
		const int32 Layer = static_cast<int32>(Group.Key >> 9);
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(GetOwner());
		Component->SetupAttachment(GetOwner()->GetRootComponent());
		Component->SetMobility(EComponentMobility::Static);
		Component->SetStaticMesh(Meshes[Mesh]);
		Component->SetCastShadow(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetCullDistances(FMath::RoundToInt(CullCm[Layer] * 0.8f), FMath::RoundToInt(CullCm[Layer]));
		if (bSolid)
		{
			Component->SetCollisionProfileName(TEXT("BlockAll"));
		}
		else
		{
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Component->RegisterComponent();
		Component->AddInstances(Group.Value, false, true);
		Components.Add(Component);
		if (Layer == Landmarks)
		{
			for (const FTransform& Placed : Group.Value)
			{
				UE_LOG(LogTemp, Display, TEXT("THALASSA: landmark %s at %s m, %.0f m across"), *GetNameSafe(Meshes[Mesh]),
					*(Placed.GetLocation() / 100.0).ToString(), Meshes[Mesh] ? Meshes[Mesh]->GetBounds().BoxExtent.GetMax() * 2.0 * Placed.GetScale3D().X / 100.0 : 0.0);
			}
		}
		TotalInstances += Group.Value.Num();
	}
}

void UThalassaOceanClutter::Remove(const FClutterPatchKey& Key)
{
	if (TArray<TObjectPtr<UInstancedStaticMeshComponent>>* Components = Built.Find(Key))
	{
		for (UInstancedStaticMeshComponent* Component : *Components)
		{
			TotalInstances -= Component->GetInstanceCount();
			Component->DestroyComponent();
		}
		Built.Remove(Key);
	}
}

void UThalassaOceanClutter::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	using namespace ThalassaClutter;
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bDisabled || !Shared.IsValid())
	{
		return;
	}

	UpdateWanted();

	// Solid rocks around the ship are placed at once on the first frame, so it never starts inside one unseen.
	if (Built.Num() == 0 && InFlight.Num() == 0)
	{
		for (const FClutterPatchKey& Key : Wanted)
		{
			if (!Key.bNear)
			{
				FClutterPatch Patch;
				Patch.Key = Key;
				Place(Shared->Generator, Shared->Seed, Shared->MeshSizesCm, Shared->MeshBaseCm, Patch);
				InFlight.Add(Key);
				Commit(MoveTemp(Patch));
			}
		}
	}

	for (const FClutterPatchKey& Key : Wanted)
	{
		if (InFlight.Num() >= MaxInFlight)
		{
			break;
		}
		if (!Built.Contains(Key) && !InFlight.Contains(Key))
		{
			Launch(Key);
		}
	}

	const double Start = FPlatformTime::Seconds();
	FClutterPatch Patch;
	while (FPlatformTime::Seconds() - Start < CommitBudgetSeconds && Shared->Finished.Dequeue(Patch))
	{
		Commit(MoveTemp(Patch));
	}

	// Patches nobody wants any more go, one step beyond where they are wanted so a viewer on a border does not churn them.
	TArray<FClutterPatchKey> Gone;
	for (const TPair<FClutterPatchKey, TArray<TObjectPtr<UInstancedStaticMeshComponent>>>& Pair : Built)
	{
		if (Wanted.Contains(Pair.Key))
		{
			continue;
		}
		bool bNearWanted = false;
		for (int32 DY = -1; DY <= 1 && !bNearWanted; ++DY)
		{
			for (int32 DX = -1; DX <= 1 && !bNearWanted; ++DX)
			{
				bNearWanted = Wanted.Contains({ Pair.Key.bNear, Pair.Key.X + DX, Pair.Key.Y + DY });
			}
		}
		if (!bNearWanted)
		{
			Gone.Add(Pair.Key);
		}
	}
	for (const FClutterPatchKey& Key : Gone)
	{
		Remove(Key);
	}
}
