#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/ThalassaSeabedGenerator.h"
#include "ThalassaOcean.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/** A seabed tile's address: its detail level and its column and row on that level's grid. */
struct FOceanTileKey
{
	int32 Level = 0;
	int64 X = 0;
	int64 Y = 0;
	bool operator==(const FOceanTileKey& Other) const { return Level == Other.Level && X == Other.X && Y == Other.Y; }
	friend uint32 GetTypeHash(const FOceanTileKey& Key) { return HashCombine(HashCombine(::GetTypeHash(Key.Level), ::GetTypeHash(Key.X)), ::GetTypeHash(Key.Y)); }
};

/** A tile's mesh, built on a worker thread, in centimetres relative to the tile's corner. */
struct FOceanTileMesh
{
	FOceanTileKey Key;
	bool bCollision = false;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
};

/**
 * The open ocean's floor, generated from a seed and streamed around whoever is there. See docs/OCEAN.md.
 *
 * Every machine builds its own seabed from the seed, so the terrain is never sent over the network:
 *  - what you see: tiles in nested rings around the local camera, 256 m tiles near and doubling outward,
 *    distant ones without the detail too fine for them, with skirts hiding the seams between levels;
 *  - what you collide with: full-detail collision tiles around the local player and, on the host, around
 *    every player and the ship, so the hull check and divers meet real ground anywhere.
 * Tiles are built on worker threads and added a few each frame; old ones stay until their replacements exist.
 */
UCLASS()
class THALASSA_API AThalassaOcean : public AActor
{
	GENERATED_BODY()

public:
	AThalassaOcean();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** The ocean in this world, if there is one. */
	static AThalassaOcean* Find(const UWorld* World);

	/** The seabed's height in world centimetres under a world location. Exact, whether or not a tile is loaded. */
	double GroundZ(const FVector& WorldLocation, double MinWavelengthM = 0.0) const;

	/** Follows a ray to where it first meets the seabed. For the sonar, which reaches far past the loaded tiles. */
	bool Raycast(const FVector& Start, const FVector& End, FVector& OutHit, double MinWavelengthM = 10.0) const;

	const FThalassaSeabedGenerator& GetGenerator() const;

	/** The ocean's seed: the whole world follows from it. */
	UPROPERTY(EditAnywhere, Category = "Thalassa|Ocean")
	int64 Seed = 20261108;

	/** Detail levels: level 0 tiles are 256 m, each level up doubles. */
	UPROPERTY(EditAnywhere, Category = "Thalassa|Ocean")
	int32 Levels = 5;

	UPROPERTY(EditAnywhere, Category = "Thalassa|Ocean")
	TObjectPtr<UMaterialInterface> SeabedMaterial;

	static constexpr double TileM = 256.0;       // Level 0.
	static constexpr int32 Quads = 64;            // Per tile side: 4 m apart at level 0.

private:
	void UpdateWanted();
	void Launch(const FOceanTileKey& Key, bool bCollision);
	void Commit(FOceanTileMesh&& Mesh);
	static FOceanTileMesh BuildMesh(const FThalassaSeabedGenerator& Generator, const FOceanTileKey& Key, bool bCollision);
	static double TileSizeM(int32 Level) { return TileM * static_cast<double>(1 << Level); }

	/** Shared with the worker threads, which keep it alive until they finish, even if the ocean is gone. */
	struct FShared
	{
		explicit FShared(uint64 Seed) : Generator(Seed) {}
		const FThalassaSeabedGenerator Generator;
		std::atomic<bool> bAlive{ true };
		TQueue<FOceanTileMesh, EQueueMode::Mpsc> Finished;
	};
	mutable TSharedPtr<FShared, ESPMode::ThreadSafe> Shared;

	TMap<FOceanTileKey, TObjectPtr<UProceduralMeshComponent>> Visible;
	TMap<FOceanTileKey, TObjectPtr<UProceduralMeshComponent>> Solid;
	TSet<FOceanTileKey> WantedVisible;
	TSet<FOceanTileKey> WantedSolid;
	TSet<FOceanTileKey> InFlightVisible;
	TSet<FOceanTileKey> InFlightSolid;

};
