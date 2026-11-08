#include "World/ThalassaOcean.h"

#include "Async/Async.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Vessel/ThalassaVessel.h"

namespace ThalassaOceanStream
{
	constexpr int32 TopReach = 2;          // The coarsest level covers this many tiles each way around the viewer.
	constexpr int32 FinerReach = 1;        // Each finer level replaces the coarser tiles within this many of the viewer.
	constexpr int32 SolidReachTiles = 1;   // Collision tiles around a player: a 768 m square.
	constexpr int32 ShipReachTiles = 2;    // Around the ship, which is 280 m long: a 1.28 km square.
	constexpr double CommitBudgetSeconds = 0.004;
	constexpr int32 MaxInFlight = 12;
}

AThalassaOcean::AThalassaOcean()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = false; // Every machine builds the same ocean from the seed.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Ground(TEXT("/Game/World/Materials/M_SeabedGround.M_SeabedGround"));
	SeabedMaterial = Ground.Object;
}

const FThalassaSeabedGenerator& AThalassaOcean::GetGenerator() const
{
	if (!Shared.IsValid())
	{
		Shared = MakeShared<FShared, ESPMode::ThreadSafe>(static_cast<uint64>(Seed));
	}
	return Shared->Generator;
}

AThalassaOcean* AThalassaOcean::Find(const UWorld* World)
{
	if (World)
	{
		for (TActorIterator<AThalassaOcean> It(const_cast<UWorld*>(World)); It; ++It)
		{
			return *It;
		}
	}
	return nullptr;
}

double AThalassaOcean::GroundZ(const FVector& WorldLocation, double MinWavelengthM) const
{
	return GetGenerator().HeightM(WorldLocation.X / 100.0, WorldLocation.Y / 100.0, MinWavelengthM) * 100.0;
}

bool AThalassaOcean::Raycast(const FVector& Start, const FVector& End, FVector& OutHit, double MinWavelengthM) const
{
	// March along the ray until it is under the ground, then narrow down to where it crossed. Steps are long
	// while the ray is far above the ground and short as it closes in, so a 3 km beam costs a few dozen samples.
	const double Length = (End - Start).Size();
	const double MinStep = FMath::Max(MinWavelengthM * 50.0, 500.0);
	double Previous = 0.0;
	double T = 0.0;
	while (T < Length)
	{
		const FVector Here = Start + (End - Start) * (T / Length);
		const double Clearance = Here.Z - GroundZ(Here, MinWavelengthM);
		const double Along = FMath::Min(Length, T + FMath::Clamp(Clearance * 0.5, MinStep, 40000.0));
		const FVector Point = Start + (End - Start) * (Along / Length);
		T = Along;
		if (Point.Z <= GroundZ(Point, MinWavelengthM))
		{
			double Low = Previous, High = Along;
			for (int32 Iteration = 0; Iteration < 12; ++Iteration)
			{
				const double Mid = (Low + High) * 0.5;
				const FVector Probe = Start + (End - Start) * (Mid / Length);
				(Probe.Z <= GroundZ(Probe, MinWavelengthM) ? High : Low) = Mid;
			}
			OutHit = Start + (End - Start) * (High / Length);
			return true;
		}
		Previous = Along;
	}
	return false;
}

void AThalassaOcean::BeginPlay()
{
	Super::BeginPlay();
	GetGenerator();
	UE_LOG(LogTemp, Display, TEXT("THALASSA: ocean seed %lld, generator revision %d, fingerprint %llu"),
		Seed, FThalassaSeabedGenerator::Revision, GetGenerator().Fingerprint());

	// Landmarks near the start, for testing and for the log: the shallowest and deepest ground within 20 km.
	double High = -1e9, Low = 1e9;
	FVector2D HighAt, LowAt;
	for (int32 J = -80; J <= 80; ++J)
	{
		for (int32 I = -80; I <= 80; ++I)
		{
			const double H = GetGenerator().HeightM(I * 250.0, J * 250.0, 500.0);
			if (H > High) { High = H; HighAt = FVector2D(I * 250.0, J * 250.0); }
			if (H < Low) { Low = H; LowAt = FVector2D(I * 250.0, J * 250.0); }
		}
	}
	UE_LOG(LogTemp, Display, TEXT("THALASSA: within 20 km: shallowest %.0f m at (%.0f, %.0f) m, deepest %.0f m at (%.0f, %.0f) m"),
		-High, HighAt.X, HighAt.Y, -Low, LowAt.X, LowAt.Y);

	// The nearest example of each province, for testing and for finding your way.
	for (int32 Kind = 0; Kind < static_cast<int32>(FThalassaSeabedGenerator::EProvince::Count); ++Kind)
	{
		for (int32 Ring = 0; Ring <= 40; ++Ring)
		{
			bool bFound = false;
			for (int32 Step = 0; Step < FMath::Max(1, Ring * 8) && !bFound; ++Step)
			{
				const double Angle = Step * (2.0 * PI / FMath::Max(1, Ring * 8));
				const double X = FMath::Cos(Angle) * Ring * 5000.0, Y = FMath::Sin(Angle) * Ring * 5000.0;
				FThalassaSeabedGenerator::EProvince Next;
				double W;
				if (static_cast<int32>(GetGenerator().ProvinceAt(X, Y, Next, W)) == Kind && W > 0.95)
				{
					UE_LOG(LogTemp, Display, TEXT("THALASSA: nearest %s at (%.0f, %.0f) m, seabed %.0f m"),
						FThalassaSeabedGenerator::ProvinceName(static_cast<FThalassaSeabedGenerator::EProvince>(Kind)), X, Y, -GetGenerator().HeightM(X, Y, 50.0));
					bFound = true;
				}
			}
			if (bFound) break;
		}
	}
}

void AThalassaOcean::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Shared.IsValid())
	{
		Shared->bAlive.store(false); // Workers still running drop their results.
	}
	Super::EndPlay(EndPlayReason);
}

FOceanTileMesh AThalassaOcean::BuildMesh(const FThalassaSeabedGenerator& Gen, const FOceanTileKey& Key, bool bCollision)
{
	const double SizeM = TileSizeM(Key.Level);
	const double SpacingM = SizeM / Quads;
	const double MinWavelengthM = Key.Level == 0 ? 0.0 : SpacingM * 2.0;
	const double OriginX = Key.X * SizeM, OriginY = Key.Y * SizeM;

	// Heights with a one-sample border, so normals at the edges match the neighbouring tile's.
	constexpr int32 N = Quads + 1;
	TArray<double> H;
	H.SetNumUninitialized((N + 2) * (N + 2));
	for (int32 J = -1; J <= N; ++J)
	{
		for (int32 I = -1; I <= N; ++I)
		{
			H[(J + 1) * (N + 2) + (I + 1)] = Gen.HeightM(OriginX + I * SpacingM, OriginY + J * SpacingM, MinWavelengthM);
		}
	}
	const auto At = [&](int32 I, int32 J) { return H[(J + 1) * (N + 2) + (I + 1)]; };

	FOceanTileMesh Mesh;
	Mesh.Key = Key;
	Mesh.bCollision = bCollision;
	Mesh.Vertices.Reserve(N * N + 4 * N);
	Mesh.Normals.Reserve(N * N + 4 * N);
	for (int32 J = 0; J < N; ++J)
	{
		for (int32 I = 0; I < N; ++I)
		{
			Mesh.Vertices.Add(FVector(I * SpacingM, J * SpacingM, At(I, J)) * 100.0);
			const FVector Normal(At(I - 1, J) - At(I + 1, J), At(I, J - 1) - At(I, J + 1), 2.0 * SpacingM);
			Mesh.Normals.Add(Normal.GetSafeNormal());
		}
	}
	// Two triangles per quad, wound to face up.
	Mesh.Triangles.Reserve(Quads * Quads * 6 + Quads * 4 * 6);
	for (int32 J = 0; J < Quads; ++J)
	{
		for (int32 I = 0; I < Quads; ++I)
		{
			const int32 A = J * N + I, B = A + 1, C = A + N, D = C + 1;
			Mesh.Triangles.Append({ A, C, B, B, C, D });
		}
	}

	if (!bCollision)
	{
		// Skirts: the edge, copied and dropped, so a gap to a coarser neighbour shows ground, not water.
		const double DropCm = SpacingM * 100.0 * 4.0;
		const auto Skirt = [&](TFunctionRef<int32(int32)> Edge, bool bFlip)
		{
			const int32 First = Mesh.Vertices.Num();
			for (int32 K = 0; K < N; ++K)
			{
				const int32 Top = Edge(K);
				const FVector Above = Mesh.Vertices[Top]; // Copied first: adding to an array from itself is unsafe.
				const FVector Normal = Mesh.Normals[Top];
				Mesh.Vertices.Add(Above - FVector(0.0, 0.0, DropCm));
				Mesh.Normals.Add(Normal);
			}
			for (int32 K = 0; K < Quads; ++K)
			{
				const int32 A = Edge(K), B = Edge(K + 1), C = First + K, D = First + K + 1;
				if (bFlip)
				{
					Mesh.Triangles.Append({ A, B, C, B, D, C });
				}
				else
				{
					Mesh.Triangles.Append({ A, C, B, B, C, D });
				}
			}
		};
		Skirt([&](int32 K) { return K; }, false);                 // South edge.
		Skirt([&](int32 K) { return (N - 1) * N + K; }, true);    // North edge.
		Skirt([&](int32 K) { return K * N; }, true);              // West edge.
		Skirt([&](int32 K) { return K * N + (N - 1); }, false);   // East edge.
	}
	return Mesh;
}

void AThalassaOcean::Launch(const FOceanTileKey& Key, bool bCollision)
{
	(bCollision ? InFlightSolid : InFlightVisible).Add(Key);
	GetGenerator();
	TSharedPtr<FShared, ESPMode::ThreadSafe> State = Shared;
	Async(EAsyncExecution::ThreadPool, [State, Key, bCollision]()
	{
		if (!State->bAlive.load())
		{
			return;
		}
		FOceanTileMesh Mesh = BuildMesh(State->Generator, Key, bCollision);
		if (State->bAlive.load())
		{
			State->Finished.Enqueue(MoveTemp(Mesh));
		}
	});
}

void AThalassaOcean::Commit(FOceanTileMesh&& Mesh)
{
	TSet<FOceanTileKey>& InFlight = Mesh.bCollision ? InFlightSolid : InFlightVisible;
	TMap<FOceanTileKey, TObjectPtr<UProceduralMeshComponent>>& Built = Mesh.bCollision ? Solid : Visible;
	InFlight.Remove(Mesh.Key);
	const TSet<FOceanTileKey>& Wanted = Mesh.bCollision ? WantedSolid : WantedVisible;
	if (!Wanted.Contains(Mesh.Key) || Built.Contains(Mesh.Key))
	{
		return; // No longer needed by the time it was done.
	}

	UProceduralMeshComponent* Tile = NewObject<UProceduralMeshComponent>(this);
	Tile->SetupAttachment(RootComponent);
	Tile->bUseAsyncCooking = true;
	Tile->SetCastShadow(false);
	const double SizeM = TileSizeM(Mesh.Key.Level);
	// Each tile sits at its own corner, so its vertices stay small numbers however far out the ocean goes.
	Tile->SetRelativeLocation(FVector(Mesh.Key.X * SizeM, Mesh.Key.Y * SizeM, 0.0) * 100.0);
	Tile->RegisterComponent();
	Tile->CreateMeshSection(0, Mesh.Vertices, Mesh.Triangles, Mesh.Normals, {}, {}, {}, Mesh.bCollision);
	if (Mesh.bCollision)
	{
		Tile->SetVisibility(false);
		Tile->SetCollisionProfileName(TEXT("BlockAll"));
	}
	else
	{
		Tile->SetMaterial(0, SeabedMaterial);
		Tile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Built.Add(Mesh.Key, Tile);
}

void AThalassaOcean::UpdateWanted()
{
	using namespace ThalassaOceanStream;
	WantedVisible.Reset();
	WantedSolid.Reset();

	const auto TileOf = [](const FVector& Location, int32 Level)
	{
		const double SizeCm = TileSizeM(Level) * 100.0;
		return FIntVector2(static_cast<int32>(FMath::FloorToDouble(Location.X / SizeCm)), static_cast<int32>(FMath::FloorToDouble(Location.Y / SizeCm)));
	};

	// What this machine sees: nested rings around its camera.
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
		{
			const FVector Eye = Camera->GetCameraLocation();
			const int32 Top = FMath::Clamp(Levels, 1, 12) - 1;
			// The coarsest level: a block around the viewer, minus the hole the next level fills.
			TArray<FOceanTileKey> Parents;
			const FIntVector2 Centre = TileOf(Eye, Top);
			for (int32 DY = -TopReach; DY <= TopReach; ++DY)
			{
				for (int32 DX = -TopReach; DX <= TopReach; ++DX)
				{
					Parents.Add({ Top, Centre.X + DX, Centre.Y + DY });
				}
			}
			for (int32 Level = Top; Level >= 0; --Level)
			{
				const FIntVector2 Here = TileOf(Eye, Level);
				TArray<FOceanTileKey> Finer;
				for (const FOceanTileKey& Tile : Parents)
				{
					const bool bInHole = Level > 0 && FMath::Abs(Tile.X - Here.X) <= FinerReach && FMath::Abs(Tile.Y - Here.Y) <= FinerReach;
					if (!bInHole)
					{
						WantedVisible.Add(Tile);
						continue;
					}
					for (int32 C = 0; C < 4; ++C)
					{
						Finer.Add({ Level - 1, Tile.X * 2 + (C & 1), Tile.Y * 2 + (C >> 1) });
					}
				}
				Parents = MoveTemp(Finer);
				if (Parents.IsEmpty())
				{
					break;
				}
			}
		}
	}

	// What this machine collides with: the local player always; on the host, every player and the ship too.
	const auto AddSolid = [&](const FVector& Location, int32 Reach)
	{
		const FIntVector2 Here = TileOf(Location, 0);
		for (int32 DY = -Reach; DY <= Reach; ++DY)
		{
			for (int32 DX = -Reach; DX <= Reach; ++DX)
			{
				WantedSolid.Add({ 0, Here.X + DX, Here.Y + DY });
			}
		}
	};
	if (HasAuthority())
	{
		for (TActorIterator<APawn> It(GetWorld()); It; ++It)
		{
			AddSolid(It->GetActorLocation(), SolidReachTiles);
		}
		if (const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld()))
		{
			AddSolid(Vessel->GetActorLocation(), ShipReachTiles);
		}
	}
	else if (const APlayerController* Local = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (const APawn* Pawn = Local->GetPawn())
		{
			AddSolid(Pawn->GetActorLocation(), SolidReachTiles);
		}
	}
}

void AThalassaOcean::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	using namespace ThalassaOceanStream;

	UpdateWanted();


	// On the first frame, all the collision anyone needs is built at once, so nothing starts over empty space.
	if (Solid.Num() == 0 && InFlightSolid.Num() == 0)
	{
		for (const FOceanTileKey& Key : WantedSolid)
		{
			InFlightSolid.Add(Key);
			Commit(BuildMesh(GetGenerator(), Key, true));
		}
	}

	// Start building what is wanted, nearest levels first, a limited number at a time.
	TArray<FOceanTileKey> Missing;
	for (const FOceanTileKey& Key : WantedSolid)
	{
		if (!Solid.Contains(Key) && !InFlightSolid.Contains(Key))
		{
			Launch(Key, true);
		}
	}
	for (const FOceanTileKey& Key : WantedVisible)
	{
		if (!Visible.Contains(Key) && !InFlightVisible.Contains(Key))
		{
			Missing.Add(Key);
		}
	}
	Missing.Sort([](const FOceanTileKey& A, const FOceanTileKey& B) { return A.Level < B.Level; });
	for (const FOceanTileKey& Key : Missing)
	{
		if (InFlightVisible.Num() >= MaxInFlight)
		{
			break;
		}
		Launch(Key, false);
	}

	// Add finished tiles within a small time budget, so the frame stays smooth.
	const double Start = FPlatformTime::Seconds();
	FOceanTileMesh Mesh;
	while (FPlatformTime::Seconds() - Start < CommitBudgetSeconds && Shared->Finished.Dequeue(Mesh))
	{
		Commit(MoveTemp(Mesh));
	}

	// Collision that nobody needs goes at once. Visible tiles go only when everything wanted is there,
	// so the ground never has holes while it changes.
	for (auto It = Solid.CreateIterator(); It; ++It)
	{
		if (!WantedSolid.Contains(It.Key()))
		{
			It.Value()->DestroyComponent();
			It.RemoveCurrent();
		}
	}
	bool bAllThere = true;
	for (const FOceanTileKey& Key : WantedVisible)
	{
		if (!Visible.Contains(Key))
		{
			bAllThere = false;
			break;
		}
	}
	if (bAllThere)
	{
		for (auto It = Visible.CreateIterator(); It; ++It)
		{
			if (!WantedVisible.Contains(It.Key()))
			{
				It.Value()->DestroyComponent();
				It.RemoveCurrent();
			}
		}
	}
}
