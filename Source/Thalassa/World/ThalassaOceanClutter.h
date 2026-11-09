#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "World/ThalassaSeabedGenerator.h"
#include "ThalassaOceanClutter.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

/** A clutter patch's address: which set it belongs to (near: small stones; far: everything bigger) and its 256 m cell. */
struct FClutterPatchKey
{
	bool bNear = false;
	int64 X = 0;
	int64 Y = 0;
	bool operator==(const FClutterPatchKey& Other) const { return bNear == Other.bNear && X == Other.X && Y == Other.Y; }
	friend uint32 GetTypeHash(const FClutterPatchKey& Key) { return HashCombine(HashCombine(::GetTypeHash(Key.bNear), ::GetTypeHash(Key.X)), ::GetTypeHash(Key.Y)); }
};

/** One rock: which mesh, whether it is solid, how far away it stops being drawn, and where it is in world centimetres. */
struct FClutterInstance
{
	uint8 Mesh = 0;
	bool bSolid = false;
	uint8 Layer = 0;
	FTransform Transform;
};

/** A patch's rocks, worked out on a worker thread. */
struct FClutterPatch
{
	FClutterPatchKey Key;
	TArray<FClutterInstance> Instances;
};

/**
 * Rocks on the open ocean's floor: stones, boulders, rock masses and the occasional cluster of slab towers,
 * placed by rules from the seed. See docs/OCEAN.md.
 *
 * Like the seabed itself, nothing is sent over the network: every rock follows from the seed and its 256 m
 * patch, so every machine places the same ones. Each kind of rock has its own lattice of cells; a cell's hash
 * decides whether something is there, where and how big, against its province's rules and the slope. Rocks
 * are drawn as instances, a component per mesh per patch. Boulders of 3 m and more are solid, so the ship and
 * divers meet them; smaller stones are scenery.
 */
UCLASS()
class THALASSA_API UThalassaOceanClutter : public UActorComponent
{
	GENERATED_BODY()

public:
	UThalassaOceanClutter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static constexpr double PatchM = 256.0;

	/** The rocks in one patch. Pure: the same seed and patch always give the same rocks. Safe on any thread. */
	static void Place(const FThalassaSeabedGenerator& Generator, uint64 Seed, const TArray<double>& MeshSizesCm, const TArray<double>& MeshBaseCm, FClutterPatch& Patch);

private:
	void UpdateWanted();
	void Launch(const FClutterPatchKey& Key);
	void Commit(FClutterPatch&& Patch);
	void Remove(const FClutterPatchKey& Key);

	/** Shared with the worker threads, which keep it alive until they finish. */
	struct FShared
	{
		FShared(uint64 InSeed, const TArray<double>& InSizes, const TArray<double>& InBases) : Generator(InSeed), Seed(InSeed), MeshSizesCm(InSizes), MeshBaseCm(InBases) {}
		const FThalassaSeabedGenerator Generator;
		const uint64 Seed;
		const TArray<double> MeshSizesCm;
		const TArray<double> MeshBaseCm;   // How far each mesh reaches below its origin, at scale 1.
		std::atomic<bool> bAlive{ true };
		TQueue<FClutterPatch, EQueueMode::Mpsc> Finished;
	};
	TSharedPtr<FShared, ESPMode::ThreadSafe> Shared;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> Meshes;

	TMap<FClutterPatchKey, TArray<TObjectPtr<UInstancedStaticMeshComponent>>> Built;
	TSet<FClutterPatchKey> Wanted;
	TSet<FClutterPatchKey> InFlight;
	bool bDisabled = false;
	int32 TotalInstances = 0;
};
