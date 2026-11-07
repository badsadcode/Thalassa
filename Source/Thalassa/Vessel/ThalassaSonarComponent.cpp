#include "Vessel/ThalassaSonarComponent.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "TextureResource.h"

namespace ThalassaSonar
{
	constexpr float SweepInterval = 0.1f;       // Seconds between fans.
	constexpr int32 BeamsPerSweep = 48;
	constexpr float FanHalfAngleDeg = 65.f;     // Across the ship's track.
	constexpr float ForwardTiltDeg = 10.f;      // A little ahead, so the chart grows in front of the bow.
	constexpr float RangeCm = 80000.f;          // 800 m: reaches the seabed at a slant from 300 m above it.

	// Chart colours run from deep to shallow: deep blue up to pale cyan.
	constexpr float DeepZ = -50000.f;
	constexpr float ShallowZ = -30000.f;
}

UThalassaSonarComponent::UThalassaSonarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UThalassaSonarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UThalassaSonarComponent, bSurveying);
	DOREPLIFETIME(UThalassaSonarComponent, Cells);
}

void UThalassaSonarComponent::SetSurveying(bool bOn)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		bSurveying = bOn;
	}
}

void UThalassaSonarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bSurveying || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	SweepTimer -= DeltaTime;
	if (SweepTimer <= 0.f)
	{
		SweepTimer = ThalassaSonar::SweepInterval;
		SweepServer();
	}
}

void UThalassaSonarComponent::SweepServer()
{
	using namespace ThalassaSonar;

	const AActor* Ship = GetOwner();
	const FTransform ShipTransform = Ship->GetActorTransform();
	const FVector Origin = Ship->GetActorLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ThalassaSonar), false, Ship);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);

	const float ForwardTilt = FMath::DegreesToRadians(ForwardTiltDeg);
	for (int32 Beam = 0; Beam < BeamsPerSweep; ++Beam)
	{
		// Evenly across the fan, in the ship's frame: down, spread to either side, tilted forward.
		const float Across = FMath::DegreesToRadians(FMath::Lerp(-FanHalfAngleDeg, FanHalfAngleDeg, Beam / float(BeamsPerSweep - 1)));
		const FVector LocalDirection = FVector(FMath::Sin(ForwardTilt), FMath::Sin(Across), -FMath::Cos(Across)).GetSafeNormal();
		const FVector End = Origin + ShipTransform.TransformVectorNoScale(LocalDirection) * RangeCm;

		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Origin, End, Objects, Params))
		{
			RecordHit(Hit.ImpactPoint);
		}
	}
}

void UThalassaSonarComponent::RecordHit(const FVector& WorldHit)
{
	const FVector2D Chart = WorldToChart(WorldHit);
	const int32 X = FMath::FloorToInt(Chart.X * GridSize);
	const int32 Y = FMath::FloorToInt(Chart.Y * GridSize);
	if (X < 0 || Y < 0 || X >= GridSize || Y >= GridSize)
	{
		return;
	}

	const int32 Index = Y * GridSize + X; // Survey order: X fastest. The texture is laid out differently.
	const int16 HeightM = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(WorldHit.Z / 100.f), -32000, 32000));

	// The highest return in a cell is the one a ship would hit. Only real changes replicate.
	if (const int32* Existing = CellLookup.Find(Index))
	{
		if (HeightM > Cells[*Existing].HeightM)
		{
			Cells[*Existing].HeightM = HeightM;
			bTextureDirty = true;
		}
		return;
	}

	FSonarCell Cell;
	Cell.Index = Index;
	Cell.HeightM = HeightM;
	CellLookup.Add(Index, Cells.Add(Cell));
	bTextureDirty = true;
}

FVector2D UThalassaSonarComponent::WorldToChart(const FVector& WorldLocation) const
{
	// The chart is fixed to the world, centred on the origin, so it stays put as the ship moves.
	const float HalfExtent = GridSize * CellSizeCm * 0.5f;
	return FVector2D((WorldLocation.X + HalfExtent) / (GridSize * CellSizeCm), (WorldLocation.Y + HalfExtent) / (GridSize * CellSizeCm));
}

int64 UThalassaSonarComponent::GetSurveyChecksum() const
{
	int64 Sum = 0;
	for (const FSonarCell& Cell : Cells)
	{
		Sum += static_cast<int64>(Cell.Index) * 31 + Cell.HeightM;
	}
	return Sum;
}

void UThalassaSonarComponent::OnRep_Cells()
{
	bTextureDirty = true;
}

UTexture2D* UThalassaSonarComponent::GetChartTexture()
{
	if (!ChartTexture)
	{
		ChartTexture = UTexture2D::CreateTransient(GridSize, GridSize, PF_B8G8R8A8);
		if (!ChartTexture)
		{
			return nullptr;
		}
		ChartTexture->Filter = TF_Nearest; // Hard-edged cells: you can see what was measured and what was not.
		ChartTexture->SRGB = true;
		bTextureDirty = true;
	}
	if (bTextureDirty)
	{
		RebuildTexture();
	}
	return ChartTexture;
}

void UThalassaSonarComponent::RebuildTexture()
{
	using namespace ThalassaSonar;

	bTextureDirty = false;

	TArray<FColor> Pixels;
	Pixels.Init(FColor(6, 19, 29, 255), GridSize * GridSize); // Unknown: the same near-black as the deep.
	for (const FSonarCell& Cell : Cells)
	{
		const float Depth = FMath::Clamp((Cell.HeightM * 100.f - DeepZ) / (ShallowZ - DeepZ), 0.f, 1.f);
		const FLinearColor Deep(0.02f, 0.10f, 0.35f);
		const FLinearColor Shallow(0.55f, 0.95f, 0.92f);
		// Laid out like a chart: world +X (north at zero heading) up the image, world +Y to the right.
		const int32 WorldX = Cell.Index % GridSize;
		const int32 WorldY = Cell.Index / GridSize;
		Pixels[(GridSize - 1 - WorldX) * GridSize + WorldY] = FMath::Lerp(Deep, Shallow, Depth).ToFColor(true);
	}

	FTexture2DMipMap& Mip = ChartTexture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	ChartTexture->UpdateResource();
}
