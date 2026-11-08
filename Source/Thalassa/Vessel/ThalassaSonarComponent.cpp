#include "Vessel/ThalassaSonarComponent.h"
#include "World/ThalassaOcean.h"

#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Thalassa.h"
#include "Tools/ThalassaTestConfig.h"
#include "Vessel/ThalassaSurveySave.h"
#include "Vessel/ThalassaVessel.h"
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
	constexpr float SaveIntervalSeconds = 30.f; // Autosave, so a crash loses at most this much survey.
	const TCHAR* DefaultSaveSlot = TEXT("ShipSurvey");

	// Bump this whenever the test map's seabed, rocks or anything else solid out there changes shape, so old
	// charts are not drawn over the new world. 2: the generated seabed, formations and dark biome (2026-11-08).
	constexpr int32 SeabedGeneration = 3; // 3: the open ocean's generator revision 2.

	constexpr float RangeCm = 300000.f;         // 3 km, like a real survey sonar: the open ocean is often well over 1 km deep.

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

void UThalassaSonarComponent::BeginPlay()
{
	Super::BeginPlay();
	LoadSurvey();
}

void UThalassaSonarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveSurvey();
	Super::EndPlay(EndPlayReason);
}

FString UThalassaSonarComponent::GetSaveSlot() const
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return FString();
	}

	// Tests never read or overwrite the real chart, and a real chart never changes a test's result.
	FString Slot;
	if (ThalassaTestConfig::GetSurveySaveSlot(Slot))
	{
		return Slot;
	}
	FString TelemetryPath;
	return ThalassaTestConfig::GetTelemetryPath(TelemetryPath) ? FString() : FString(ThalassaSonar::DefaultSaveSlot);
}

void UThalassaSonarComponent::LoadSurvey()
{
	const FString Slot = GetSaveSlot();
	if (Slot.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(Slot, 0))
	{
		return;
	}

	const UThalassaSurveySave* Saved = Cast<UThalassaSurveySave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Saved || Saved->Version != UThalassaSurveySave::CurrentVersion || Saved->GridSize != GridSize || Saved->CellSizeCm != CellSizeCm
		|| Saved->World != GetWorldStamp())
	{
		UE_LOG(LogThalassa, Warning, TEXT("Sonar: chart in slot %s is unreadable, from a different grid or from a different world (%s, now %s); starting a new one"),
			*Slot, Saved ? *Saved->World : TEXT("?"), *GetWorldStamp());
		return;
	}

	Cells.Reset();
	CellLookup.Reset();
	for (const FSonarCell& Cell : Saved->Cells)
	{
		if (Cell.Index >= 0 && Cell.Index < GridSize * GridSize && !CellLookup.Contains(Cell.Index))
		{
			CellLookup.Add(Cell.Index, Cells.Add(Cell));
		}
	}
	bTextureDirty = true;
	UE_LOG(LogThalassa, Display, TEXT("Sonar: loaded %d surveyed cells from slot %s"), Cells.Num(), *Slot);
}

FString UThalassaSonarComponent::GetWorldStamp() const
{
	const FString Map = GetWorld() ? UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()) : FString();
	return FString::Printf(TEXT("%s#%d"), *Map, ThalassaSonar::SeabedGeneration);
}

void UThalassaSonarComponent::SaveSurvey()
{
	const FString Slot = GetSaveSlot();
	if (Slot.IsEmpty() || !bUnsavedChanges)
	{
		return;
	}

	UThalassaSurveySave* Save = Cast<UThalassaSurveySave>(UGameplayStatics::CreateSaveGameObject(UThalassaSurveySave::StaticClass()));
	Save->GridSize = GridSize;
	Save->CellSizeCm = CellSizeCm;
	Save->World = GetWorldStamp();
	Save->Cells = Cells;
	if (UGameplayStatics::SaveGameToSlot(Save, Slot, 0))
	{
		bUnsavedChanges = false;
		UE_LOG(LogThalassa, Display, TEXT("Sonar: saved %d surveyed cells to slot %s"), Cells.Num(), *Slot);
	}
	else
	{
		UE_LOG(LogThalassa, Error, TEXT("Sonar: could not save the chart to slot %s"), *Slot);
	}
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

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	SaveTimer -= DeltaTime;
	if (SaveTimer <= 0.f)
	{
		SaveTimer = ThalassaSonar::SaveIntervalSeconds;
		SaveSurvey();
	}

	if (!bSurveying)
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
	if (const AThalassaVessel* Vessel = Cast<AThalassaVessel>(Ship))
	{
		Vessel->IgnoreShipInQuery(Params); // Never map the ship's own hull.
	}
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);

	const AThalassaOcean* Ocean = AThalassaOcean::Find(GetWorld());
	const float ForwardTilt = FMath::DegreesToRadians(ForwardTiltDeg);
	for (int32 Beam = 0; Beam < BeamsPerSweep; ++Beam)
	{
		// Evenly across the fan, in the ship's frame: down, spread to either side, tilted forward.
		const float Across = FMath::DegreesToRadians(FMath::Lerp(-FanHalfAngleDeg, FanHalfAngleDeg, Beam / float(BeamsPerSweep - 1)));
		const FVector LocalDirection = FVector(FMath::Sin(ForwardTilt), FMath::Sin(Across), -FMath::Cos(Across)).GetSafeNormal();
		const FVector End = Origin + ShipTransform.TransformVectorNoScale(LocalDirection) * RangeCm;

		// Anything solid in the world, then the open ocean's seabed, which reaches past the loaded tiles.
		FHitResult Hit;
		bool bHit = GetWorld()->LineTraceSingleByObjectType(Hit, Origin, End, Objects, Params);
		FVector Return = bHit ? FVector(Hit.ImpactPoint) : End;
		FVector Ground;
		if (Ocean && Ocean->Raycast(Origin, Return, Ground, 20.0))
		{
			Return = Ground;
			bHit = true;
		}
		if (bHit)
		{
			RecordHit(Return);
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
			bUnsavedChanges = true;
		}
		return;
	}

	FSonarCell Cell;
	Cell.Index = Index;
	Cell.HeightM = HeightM;
	CellLookup.Add(Index, Cells.Add(Cell));
	bTextureDirty = true;
	bUnsavedChanges = true;
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

FLinearColor UThalassaSonarComponent::DepthColour(float HeightM, float DeepM, float ShallowM)
{
	// Deep navy, blue, teal, aqua, pale: the shallower, the brighter.
	static const FLinearColor Stops[] = {
		FLinearColor(0.008f, 0.02f, 0.09f), FLinearColor(0.02f, 0.10f, 0.32f), FLinearColor(0.03f, 0.33f, 0.50f),
		FLinearColor(0.18f, 0.68f, 0.70f), FLinearColor(0.80f, 0.98f, 0.88f) };
	const float T = FMath::Clamp((HeightM - DeepM) / FMath::Max(ShallowM - DeepM, 1.f), 0.f, 1.f) * (UE_ARRAY_COUNT(Stops) - 1);
	const int32 I = FMath::Min(FMath::FloorToInt(T), int32(UE_ARRAY_COUNT(Stops)) - 2);
	return FMath::Lerp(Stops[I], Stops[I + 1], T - I);
}

void UThalassaSonarComponent::SyncDense()
{
	bTextureDirty = false;
	const int32 Total = GridSize * GridSize;
	if (DenseHeight.Num() != Total)
	{
		DenseHeight.Init(0, Total);
		DenseKnown.Init(false, Total);
		DenseChanged.Init(-1000.f, Total);
	}
	// The first time, nothing glows: a chart loaded or received on joining is not news.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (const FSonarCell& Cell : Cells)
	{
		if (!DenseHeight.IsValidIndex(Cell.Index))
		{
			continue;
		}
		if (!DenseKnown[Cell.Index] || DenseHeight[Cell.Index] != Cell.HeightM)
		{
			DenseKnown[Cell.Index] = true;
			DenseHeight[Cell.Index] = Cell.HeightM;
			DenseChanged[Cell.Index] = bDenseSeeded ? Now : -1000.f;
		}
	}
	bDenseSeeded = true;
}

bool UThalassaSonarComponent::GetSurveyedHeightM(const FVector& WorldLocation, float& OutHeightM)
{
	if (bTextureDirty)
	{
		SyncDense();
	}
	const FVector2D Chart = WorldToChart(WorldLocation) * GridSize;
	const int32 X = FMath::FloorToInt(Chart.X), Y = FMath::FloorToInt(Chart.Y);
	if (X < 0 || Y < 0 || X >= GridSize || Y >= GridSize || !DenseKnown.IsValidIndex(Y * GridSize + X) || !DenseKnown[Y * GridSize + X])
	{
		return false;
	}
	OutHeightM = DenseHeight[Y * GridSize + X];
	return true;
}

bool UThalassaSonarComponent::SampleHeightM(const FVector& WorldLocation, float& OutHeightM)
{
	if (bTextureDirty)
	{
		SyncDense();
	}
	if (DenseKnown.Num() == 0)
	{
		return false;
	}
	const FVector2D Chart = WorldToChart(WorldLocation) * GridSize - FVector2D(0.5f);
	const int32 X0 = FMath::FloorToInt(Chart.X), Y0 = FMath::FloorToInt(Chart.Y);
	const float FX = Chart.X - X0, FY = Chart.Y - Y0;
	float Sum = 0.f, Weight = 0.f;
	for (int32 DX = 0; DX < 2; ++DX)
	{
		for (int32 DY = 0; DY < 2; ++DY)
		{
			const int32 X = X0 + DX, Y = Y0 + DY;
			if (X >= 0 && Y >= 0 && X < GridSize && Y < GridSize && DenseKnown[Y * GridSize + X])
			{
				const float Wt = (DX ? FX : 1.f - FX) * (DY ? FY : 1.f - FY);
				Sum += DenseHeight[Y * GridSize + X] * Wt;
				Weight += Wt;
			}
		}
	}
	if (Weight < 0.3f)
	{
		return false;
	}
	OutHeightM = Sum / Weight;
	return true;
}

UTexture2D* UThalassaSonarComponent::GetLocalChart(const FVector& WorldCentre, int32 ViewCells, FVector2D& OutUV0, float& OutUVSize)
{
	if (bTextureDirty)
	{
		SyncDense();
	}

	// Continuous cell coordinates of the centre: X along world X, Y along world Y.
	const FVector2D Centre = WorldToChart(WorldCentre) * GridSize;
	const int32 Across = ViewCells + 8; // A margin, so the crop can follow the ship between rebuilds.
	const FIntPoint Wanted(FMath::FloorToInt(Centre.X) - Across / 2, FMath::FloorToInt(Centre.Y) - Across / 2);
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const bool bMoved = LocalCells != Across || FMath::Abs(Wanted.X - LocalOrigin.X) > 3 || FMath::Abs(Wanted.Y - LocalOrigin.Y) > 3;
	if (bMoved || !LocalChart || Now - LocalBuiltAt >= 0.1)
	{
		if (bMoved)
		{
			LocalOrigin = Wanted;
			LocalCells = Across;
		}
		LocalBuiltAt = Now;
		BuildLocalChart();
	}

	// North up: world +X runs up the image, world +Y to the right.
	const float Width = LocalCells * PixelsPerCell;
	const float CentreU = (Centre.Y - LocalOrigin.Y) * PixelsPerCell;
	const float CentreV = (LocalOrigin.X + LocalCells - Centre.X) * PixelsPerCell;
	const float Half = ViewCells * 0.5f * PixelsPerCell;
	OutUV0 = FVector2D((CentreU - Half) / Width, (CentreV - Half) / Width);
	OutUVSize = ViewCells * PixelsPerCell / Width;
	return LocalChart;
}

void UThalassaSonarComponent::BuildLocalChart()
{
	const int32 W = LocalCells * PixelsPerCell;
	if (!LocalChart || LocalChart->GetSizeX() != W)
	{
		LocalChart = UTexture2D::CreateTransient(W, W, PF_B8G8R8A8);
		if (!LocalChart)
		{
			return;
		}
		LocalChart->Filter = TF_Bilinear;
		LocalChart->SRGB = true;
		LocalChart->UpdateResource();
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float PixelM = CellSizeCm / 100.f / PixelsPerCell;
	const auto CellAt = [this](int32 X, int32 Y) { return (X >= 0 && Y >= 0 && X < GridSize && Y < GridSize) ? Y * GridSize + X : -1; };

	// Pass 1: heights, smoothed between cell centres. Unmeasured cells take no part.
	TArray<float> Heights;
	Heights.Init(TNumericLimits<float>::Max(), W * W);
	for (int32 V = 0; V < W; ++V)
	{
		const float CellX = LocalOrigin.X + LocalCells - (V + 0.5f) / PixelsPerCell - 0.5f;
		const int32 X0 = FMath::FloorToInt(CellX);
		const float FX = CellX - X0;
		for (int32 U = 0; U < W; ++U)
		{
			const float CellY = LocalOrigin.Y + (U + 0.5f) / PixelsPerCell - 0.5f;
			const int32 Y0 = FMath::FloorToInt(CellY);
			const float FY = CellY - Y0;
			float Sum = 0.f, Weight = 0.f;
			for (int32 DX = 0; DX < 2; ++DX)
			{
				for (int32 DY = 0; DY < 2; ++DY)
				{
					const int32 Index = CellAt(X0 + DX, Y0 + DY);
					if (Index >= 0 && DenseKnown[Index])
					{
						const float Wt = (DX ? FX : 1.f - FX) * (DY ? FY : 1.f - FY);
						Sum += DenseHeight[Index] * Wt;
						Weight += Wt;
					}
				}
			}
			if (Weight > 0.3f)
			{
				Heights[V * W + U] = Sum / Weight;
			}
		}
	}

	// Colours span what is in view, like a chart plotter: a gentle plain still shows its shape.
	float Low = TNumericLimits<float>::Max(), High = -TNumericLimits<float>::Max();
	for (const float H : Heights)
	{
		if (H != TNumericLimits<float>::Max())
		{
			Low = FMath::Min(Low, H);
			High = FMath::Max(High, H);
		}
	}
	if (Low <= High)
	{
		const float Middle = (Low + High) * 0.5f;
		const float HalfSpan = FMath::Max((High - Low) * 0.5f, 30.f);
		LocalDeepM = Middle - HalfSpan;
		LocalShallowM = Middle + HalfSpan;
	}
	// Contours: 10 m and 50 m over gentle ground, 20 m and 100 m over steep.
	const float Minor = LocalShallowM - LocalDeepM > 150.f ? 20.f : 10.f;
	const float Major = Minor * 5.f;

	// Pass 2: colour, light from the north-west, contours, and the glow of news.
	const FVector Light = FVector(0.6f, -0.6f, 0.55f).GetSafeNormal();
	constexpr float Exaggeration = 6.f;
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(W * W);
	for (int32 V = 0; V < W; ++V)
	{
		const float CellX = LocalOrigin.X + LocalCells - (V + 0.5f) / PixelsPerCell;
		for (int32 U = 0; U < W; ++U)
		{
			const float CellY = LocalOrigin.Y + (U + 0.5f) / PixelsPerCell;
			const float H = Heights[V * W + U];
			if (H == TNumericLimits<float>::Max())
			{
				// Unknown water: near black, with a faint 100 m grid fixed to the world.
				const bool bGrid = FMath::Fmod(CellX, 4.f) < 1.f / PixelsPerCell || FMath::Fmod(CellY, 4.f) < 1.f / PixelsPerCell;
				Pixels[V * W + U] = bGrid ? FColor(8, 20, 27) : FColor(3, 8, 12);
				continue;
			}
			const auto At = [&](int32 PU, int32 PV)
			{
				PU = FMath::Clamp(PU, 0, W - 1);
				PV = FMath::Clamp(PV, 0, W - 1);
				const float Value = Heights[PV * W + PU];
				return Value == TNumericLimits<float>::Max() ? H : Value;
			};
			// Up the image is world +X.
			const float SlopeX = (At(U, V - 1) - At(U, V + 1)) / (2.f * PixelM);
			const float SlopeY = (At(U + 1, V) - At(U - 1, V)) / (2.f * PixelM);
			const FVector Normal = FVector(-SlopeX * Exaggeration, -SlopeY * Exaggeration, 1.f).GetSafeNormal();
			const float Shade = FMath::Max(0.f, FVector::DotProduct(Normal, Light));
			FLinearColor Colour = DepthColour(H, LocalDeepM, LocalShallowM) * (0.3f + 0.95f * Shade);

			const float Right = At(U + 1, V), Below = At(U, V + 1);
			if (FMath::FloorToInt(H / Major) != FMath::FloorToInt(Right / Major) || FMath::FloorToInt(H / Major) != FMath::FloorToInt(Below / Major))
			{
				Colour = FMath::Lerp(Colour, FLinearColor(0.75f, 1.f, 0.95f), 0.6f);
			}
			else if (FMath::FloorToInt(H / Minor) != FMath::FloorToInt(Right / Minor) || FMath::FloorToInt(H / Minor) != FMath::FloorToInt(Below / Minor))
			{
				Colour = FMath::Lerp(Colour, FLinearColor(0.55f, 0.95f, 0.9f), 0.22f);
			}

			const int32 Nearest = CellAt(FMath::FloorToInt(CellX), FMath::FloorToInt(CellY));
			if (Nearest >= 0)
			{
				const float Glow = FMath::Exp(-(Now - DenseChanged[Nearest]) / 1.2f);
				Colour += FLinearColor(0.3f, 0.85f, 0.8f) * Glow * 0.7f;
			}
			Pixels[V * W + U] = Colour.ToFColor(true);
		}
	}

	// Upload without stalling: the renderer frees the copy when it is done with it.
	uint8* Data = static_cast<uint8*>(FMemory::Malloc(Pixels.Num() * sizeof(FColor)));
	FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, W, W);
	LocalChart->UpdateTextureRegions(0, 1, Region, W * sizeof(FColor), sizeof(FColor), Data,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			FMemory::Free(SrcData);
			delete Regions;
		});
}
