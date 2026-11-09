#include "Vessel/ThalassaSonarComponent.h"
#include "World/ThalassaOcean.h"
#include "World/ThalassaOceanFauna.h"

#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Thalassa.h"
#include "Tools/ThalassaPerf.h"
#include "Tools/ThalassaTestConfig.h"
#include "Vessel/ThalassaSonarPingComponent.h"
#include "Vessel/ThalassaShipDamage.h"
#include "Vessel/ThalassaSurveySave.h"
#include "Vessel/ThalassaVessel.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "TextureResource.h"
#include "UObject/ConstructorHelpers.h"

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
	constexpr int32 SeabedGeneration = 4; // 3: the open ocean's generator revision 2. 4: Meshy landmarks, solid, near the start and scattered.

	constexpr float RangeCm = 300000.f;         // 3 km, like a real survey sonar: the open ocean is often well over 1 km deep.

	// Grades, from none to the best. Tests start at the best; real play will start lower and upgrade.
	const FSonarGrade Grades[] = {
		{ TEXT("survey only"), 0.f, 0.f },
		{ TEXT("short ping"), 600.f, 12.f },
		{ TEXT("long ping"), 1200.f, 8.f },
	};

	// The ping's shadow: ground is hidden if the line to a point this far above it is blocked well short of it,
	// before this fraction of the way there, less the slack. Only real ridges and walls cast shadows, not the
	// small bumps a grazing line touches near its end.
	constexpr float ShadowAimCm = 1500.f;
	constexpr float ShadowFraction = 0.85f;
	constexpr float ShadowSlackCm = 3000.f;
}

namespace ThalassaSonarCells
{
	// A cell's whole-world coordinates in one number: 16 bits each, so ±819 km in survey cells.
	int32 Pack(const FIntPoint& Cell)
	{
		return static_cast<int32>((static_cast<uint32>(static_cast<uint16>(Cell.X)) << 16) | static_cast<uint16>(Cell.Y));
	}

	FIntPoint Unpack(int32 Index)
	{
		const uint32 Bits = static_cast<uint32>(Index);
		return FIntPoint(static_cast<int16>(Bits >> 16), static_cast<int16>(Bits & 0xFFFF));
	}

	uint64 Key(int32 Layer, int32 Index)
	{
		return (static_cast<uint64>(Layer) << 32) | static_cast<uint32>(Index);
	}

	float LayerCellCm(int32 Layer)
	{
		return Layer == 0 ? UThalassaSonarComponent::CellSizeCm : UThalassaSonarComponent::CellSizeCm * UThalassaSonarComponent::PingCellCells;
	}

	FIntPoint CellOf(int32 Layer, const FVector& World)
	{
		const float Size = LayerCellCm(Layer);
		return FIntPoint(FMath::FloorToInt(World.X / Size), FMath::FloorToInt(World.Y / Size));
	}

	bool InRange(const FIntPoint& Cell)
	{
		return Cell.X >= -32768 && Cell.X <= 32767 && Cell.Y >= -32768 && Cell.Y <= 32767;
	}
}

void FSonarCell::PostReplicatedAdd(const FSonarCellArray& Array)
{
	if (Array.Owner)
	{
		Array.Owner->NoteCellArrived(*this);
	}
}

void FSonarCell::PostReplicatedChange(const FSonarCellArray& Array)
{
	if (Array.Owner)
	{
		Array.Owner->NoteCellArrived(*this);
	}
}

UThalassaSonarComponent::FPage::FPage()
{
	FMemory::Memzero(Height);
	FMemory::Memzero(Known);
	for (float& Time : Changed)
	{
		Time = -1000.f;
	}
}

UThalassaSonarComponent::UThalassaSonarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	Chart.Owner = this;

	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> PulseCollection(TEXT("/Game/Ship/Sonar/MPC_SonarPulse.MPC_SonarPulse"));
	if (PulseCollection.Succeeded())
	{
		PulseParameters = PulseCollection.Object;
	}
}

void UThalassaSonarComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UThalassaSonarComponent, bSurveying);
	DOREPLIFETIME(UThalassaSonarComponent, Grade);
	DOREPLIFETIME(UThalassaSonarComponent, Chart);
	DOREPLIFETIME(UThalassaSonarComponent, Pulse);
	DOREPLIFETIME(UThalassaSonarComponent, RecentPulses);
}

void UThalassaSonarComponent::BeginPlay()
{
	Super::BeginPlay();
	Chart.Owner = this;
	LoadSurvey();

	float Every = 0.f;
	if (ThalassaTestConfig::GetPingEvery(Every))
	{
		TestPingTimer = FMath::Min(Every, 3.f);
	}
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
	if (!Saved || Saved->Version != UThalassaSurveySave::CurrentVersion || Saved->CellSizeCm != CellSizeCm || Saved->World != GetWorldStamp())
	{
		UE_LOG(LogThalassa, Warning, TEXT("Sonar: chart in slot %s is unreadable, from an older version or from a different world (%s, now %s); starting a new one"),
			*Slot, Saved ? *Saved->World : TEXT("?"), *GetWorldStamp());
		return;
	}

	Chart.Items.Reset();
	CellLookup.Reset();
	for (const FSonarCell& Stored : Saved->Cells)
	{
		const uint64 Key = ThalassaSonarCells::Key(Stored.Layer, Stored.Index);
		if (Stored.Layer <= 1 && !CellLookup.Contains(Key))
		{
			FSonarCell& Cell = Chart.Items.AddDefaulted_GetRef();
			Cell.Index = Stored.Index;
			Cell.HeightM = Stored.HeightM;
			Cell.Layer = Stored.Layer;
			Chart.MarkItemDirty(Cell);
			CellLookup.Add(Key, Chart.Items.Num() - 1);
			Arrived.Add(Cell);
		}
	}
	bTextureDirty = true;
	UE_LOG(LogThalassa, Display, TEXT("Sonar: loaded %d charted cells from slot %s"), Chart.Items.Num(), *Slot);
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
	Save->GridSize = 0; // The chart has no edges any more.
	Save->CellSizeCm = CellSizeCm;
	Save->World = GetWorldStamp();
	Save->Cells = Chart.Items;
	if (UGameplayStatics::SaveGameToSlot(Save, Slot, 0))
	{
		bUnsavedChanges = false;
		UE_LOG(LogThalassa, Display, TEXT("Sonar: saved %d charted cells to slot %s"), Chart.Items.Num(), *Slot);
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

const FSonarGrade& UThalassaSonarComponent::GetGradeInfo(int32 InGrade)
{
	return ThalassaSonar::Grades[FMath::Clamp(InGrade, 0, int32(UE_ARRAY_COUNT(ThalassaSonar::Grades)) - 1)];
}

void UThalassaSonarComponent::CycleGrade()
{
#if !UE_BUILD_SHIPPING
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Grade = (Grade + 1) % int32(UE_ARRAY_COUNT(ThalassaSonar::Grades));
		UE_LOG(LogThalassa, Display, TEXT("Sonar: test grade %d, %s"), Grade, GetGradeInfo(Grade).Name);
	}
#endif
}

double UThalassaSonarComponent::GetServerTime() const
{
	if (const AThalassaVessel* Vessel = Cast<AThalassaVessel>(GetOwner()))
	{
		return Vessel->GetSyncedTime();
	}
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

float UThalassaSonarComponent::GetPingCooldownLeft() const
{
	const FSonarGrade& Info = GetGradeInfo(Grade);
	return FMath::Max(0.f, static_cast<float>(Pulse.StartTime + Info.PingCooldownS - GetServerTime()));
}

bool UThalassaSonarComponent::TryPing()
{
	const AActor* Ship = GetOwner();
	const FSonarGrade& Info = GetGradeInfo(Grade);
	if (!Ship || !Ship->HasAuthority() || Info.PingRangeM <= 0.f || GetPingCooldownLeft() > 0.f)
	{
		return false;
	}
	const AThalassaVessel* Owner = Cast<AThalassaVessel>(Ship);
	if (Owner && Owner->GetDamage() && !Owner->GetDamage()->IsSonarWorking())
	{
		return false; // Too badly damaged to ping.
	}

	Pulse.Origin = Ship->GetActorLocation();
	Pulse.StartTime = GetServerTime();
	Pulse.RangeM = Info.PingRangeM;
	++Pulse.Serial;
	RecentPulses.Add(Pulse);
	RecentPulses.RemoveAll([this](const FSonarPulse& Old) { return Old.StartTime < Pulse.StartTime - RecentPulseSeconds; });
	if (RecentPulses.Num() > 80)
	{
		RecentPulses.RemoveAt(0, RecentPulses.Num() - 80);
	}

	// Every ping cell within range, nearest first: charted as the wave reaches it.
	PendingPing.Reset();
	PendingPingNext = 0;
	const float CellM = ThalassaSonarCells::LayerCellCm(1) / 100.f;
	const FIntPoint Centre = ThalassaSonarCells::CellOf(1, Pulse.Origin);
	const int32 Reach = FMath::CeilToInt(Info.PingRangeM / CellM) + 1;
	for (int32 DX = -Reach; DX <= Reach; ++DX)
	{
		for (int32 DY = -Reach; DY <= Reach; ++DY)
		{
			const FIntPoint Cell(Centre.X + DX, Centre.Y + DY);
			const FVector2D Middle((Cell.X + 0.5) * CellM, (Cell.Y + 0.5) * CellM);
			const float DistanceM = FVector2D::Distance(Middle, FVector2D(Pulse.Origin.X, Pulse.Origin.Y) / 100.f);
			if (DistanceM <= Info.PingRangeM && ThalassaSonarCells::InRange(Cell))
			{
				PendingPing.Add({ Cell, DistanceM });
			}
		}
	}
	PendingPing.Sort([](const FPendingPingCell& A, const FPendingPingCell& B) { return A.DistanceM < B.DistanceM; });

	UE_LOG(LogThalassa, Display, TEXT("Sonar: ping %d from %s, %.0f m, %d cells to chart"), Pulse.Serial, *Pulse.Origin.ToString(), Info.PingRangeM, PendingPing.Num());
	OnRep_Pulse(); // The host hears and sees its own ping too.
	return true;
}

void UThalassaSonarComponent::OnRep_Pulse()
{
	if (Pulse.Serial != HeardPulse)
	{
		HeardPulse = Pulse.Serial;
		// A ping that went out long ago (when joining, say) is not heard again.
		if (PulseSound && GetServerTime() - Pulse.StartTime < 2.0)
		{
			PulseSound->Ping();
		}
	}
}

bool UThalassaSonarComponent::GetPulseFront(FVector& OutOrigin, float& OutRadiusM, float& OutRangeM) const
{
	const float Age = static_cast<float>(GetServerTime() - Pulse.StartTime);
	float Radius = Age * PulseSpeedMS;
	float Hold = 0.f;
	if (ThalassaTestConfig::GetPulseHold(Hold))
	{
		Radius = FMath::Min(Radius, Hold); // Screenshots only: the ring waits to be photographed.
	}
	if (Pulse.Serial == 0 || Age < 0.f || Radius > Pulse.RangeM)
	{
		return false;
	}
	OutOrigin = Pulse.Origin;
	OutRadiusM = Radius;
	OutRangeM = Pulse.RangeM;
	return true;
}

void UThalassaSonarComponent::UpdatePulseVisuals()
{
	if (!PulseParameters || !GetWorld())
	{
		return;
	}
	FVector Origin = FVector::ZeroVector;
	float RadiusM = 0.f, RangeM = 1.f;
	float Strength = 0.f;
	if (GetPulseFront(Origin, RadiusM, RangeM))
	{
		// Bright as it leaves, fading as it spreads, gone at the edge of its range.
		const float Spent = RadiusM / RangeM;
		Strength = (1.f - Spent * Spent) * FMath::Clamp(RadiusM / 15.f, 0.f, 1.f);
	}
	UKismetMaterialLibrary::SetVectorParameterValue(this, PulseParameters, TEXT("PulseOrigin"), FLinearColor(Origin.X, Origin.Y, Origin.Z, 0.f));
	UKismetMaterialLibrary::SetScalarParameterValue(this, PulseParameters, TEXT("PulseRadius"), RadiusM * 100.f);
	UKismetMaterialLibrary::SetScalarParameterValue(this, PulseParameters, TEXT("PulseStrength"), Strength);
}

void UThalassaSonarComponent::UpdateContacts()
{
	if (Pulse.Serial == 0 || Pulse.Serial == ContactPulse)
	{
		return;
	}
	ContactPulse = Pulse.Serial;
	Contacts.Reset();
	// A ping from long ago (one that was already out when this machine joined) shows nothing.
	if (GetServerTime() - Pulse.StartTime > Pulse.RangeM / PulseSpeedMS + ContactFadeSeconds)
	{
		return;
	}
	AThalassaOcean* Ocean = AThalassaOcean::Find(GetWorld());
	if (!Ocean || !Ocean->Fauna)
	{
		return;
	}
	TArray<FThalassaSonarTarget> Targets;
	Ocean->Fauna->GatherSonarTargets(Pulse.Origin, Pulse.RangeM * 100.f, Pulse.StartTime, Targets);
	uint32 Fingerprint = 0;
	for (const FThalassaSonarTarget& Target : Targets)
	{
		const double Distance = FVector::Dist(Target.Location, Pulse.Origin) / 100.0;
		Contacts.Add({ Target.Location, Pulse.StartTime + Distance / PulseSpeedMS, Target.RadiusCm });
		// To the metre, for comparing machines in tests.
		Fingerprint = HashCombine(Fingerprint, GetTypeHash(FIntVector(FMath::RoundToInt(Target.Location.X / 100.0), FMath::RoundToInt(Target.Location.Y / 100.0), FMath::RoundToInt(Target.Location.Z / 100.0))));
	}
	UE_LOG(LogThalassa, Display, TEXT("Sonar: ping %d returned from %d animals, fingerprint %08x"), Pulse.Serial, Contacts.Num(), Fingerprint);
}

void UThalassaSonarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatePulseVisuals();
	UpdateContacts();

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

	TickPulseServer();

	float Every = 0.f;
	if (ThalassaTestConfig::GetPingEvery(Every))
	{
		TestPingTimer -= DeltaTime;
		if (TestPingTimer <= 0.f && TryPing())
		{
			TestPingTimer = Every;
		}
	}

	if (!bSurveying)
	{
		return;
	}

	const AThalassaVessel* Owner = Cast<AThalassaVessel>(GetOwner());
	if (Owner && Owner->GetDamage() && !Owner->GetDamage()->IsSonarWorking())
	{
		return; // Too badly damaged to survey.
	}
	SweepTimer -= DeltaTime;
	if (SweepTimer <= 0.f)
	{
		SweepTimer = ThalassaSonar::SweepInterval;
		SweepServer();
	}
}

void UThalassaSonarComponent::TickPulseServer()
{
	THALASSA_PERF_SCOPE("ping charting");
	using namespace ThalassaSonar;

	if (PendingPingNext >= PendingPing.Num())
	{
		return;
	}

	const AActor* Ship = GetOwner();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ThalassaSonarPing), false, Ship);
	if (const AThalassaVessel* Vessel = Cast<AThalassaVessel>(Ship))
	{
		Vessel->IgnoreShipInQuery(Params);
	}
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	const AThalassaOcean* Ocean = AThalassaOcean::Find(GetWorld());
	const FVector Origin = Pulse.Origin;
	const float Front = static_cast<float>(GetServerTime() - Pulse.StartTime) * PulseSpeedMS;
	const float CellCm = ThalassaSonarCells::LayerCellCm(1);

	while (PendingPingNext < PendingPing.Num() && PendingPing[PendingPingNext].DistanceM <= Front)
	{
		const FIntPoint Cell = PendingPing[PendingPingNext++].Cell;
		const FVector Above((Cell.X + 0.5) * CellCm, (Cell.Y + 0.5) * CellCm, Origin.Z);

		// The ground at the cell's centre: the open ocean's seabed, or else whatever solid is below.
		FVector Ground;
		if (Ocean)
		{
			Ground = FVector(Above.X, Above.Y, Ocean->GroundZ(Above, 20.0));
		}
		else
		{
			FHitResult Down;
			if (!GetWorld()->LineTraceSingleByObjectType(Down, Above, Above - FVector(0.f, 0.f, RangeCm), Objects, Params))
			{
				continue;
			}
			Ground = Down.ImpactPoint;
		}
		const float Distance = FVector::Distance(Origin, Ground);
		if (Distance > Pulse.RangeM * 100.f)
		{
			continue;
		}

		// Ground behind a ridge or a rock is in the ping's shadow: the wave never reaches it.
		const FVector Aim = Ground + FVector(0.f, 0.f, ShadowAimCm);
		const float Shadowed = Distance * ShadowFraction - ShadowSlackCm;
		FHitResult Blocked;
		if (GetWorld()->LineTraceSingleByObjectType(Blocked, Origin, Aim, Objects, Params) && Blocked.Distance < Shadowed)
		{
			continue;
		}
		FVector Hit;
		if (Ocean && Ocean->Raycast(Origin, Aim, Hit, 20.0) && FVector::Distance(Origin, Hit) < Shadowed)
		{
			continue;
		}

		RecordCell(1, Cell, static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Ground.Z / 100.f), -32000, 32000)), false);
	}
}

void UThalassaSonarComponent::SweepServer()
{
	THALASSA_PERF_SCOPE("survey sweep");
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
			const FIntPoint Cell = ThalassaSonarCells::CellOf(0, Return);
			if (ThalassaSonarCells::InRange(Cell))
			{
				RecordCell(0, Cell, static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Return.Z / 100.f), -32000, 32000)), true);
			}
		}
	}
}

void UThalassaSonarComponent::RecordCell(int32 Layer, const FIntPoint& Where, int16 HeightM, bool bHighest)
{
	const int32 Index = ThalassaSonarCells::Pack(Where);
	const uint64 Key = ThalassaSonarCells::Key(Layer, Index);

	// The survey keeps the highest return in a cell, the one a ship would hit; a ping keeps its latest
	// measurement. Only real changes replicate.
	if (const int32* Existing = CellLookup.Find(Key))
	{
		FSonarCell& Cell = Chart.Items[*Existing];
		if (bHighest ? HeightM > Cell.HeightM : HeightM != Cell.HeightM)
		{
			Cell.HeightM = HeightM;
			Chart.MarkItemDirty(Cell);
			Arrived.Add(Cell);
			bTextureDirty = true;
			bUnsavedChanges = true;
		}
		return;
	}

	FSonarCell& Cell = Chart.Items.AddDefaulted_GetRef();
	Cell.Index = Index;
	Cell.HeightM = HeightM;
	Cell.Layer = static_cast<uint8>(Layer);
	Chart.MarkItemDirty(Cell);
	CellLookup.Add(Key, Chart.Items.Num() - 1);
	Arrived.Add(Cell);
	bTextureDirty = true;
	bUnsavedChanges = true;
}

void UThalassaSonarComponent::NoteCellArrived(const FSonarCell& Cell)
{
	Arrived.Add(Cell);
	bTextureDirty = true;
}

int64 UThalassaSonarComponent::GetSurveyChecksum() const
{
	int64 Sum = 0;
	for (const FSonarCell& Cell : Chart.Items)
	{
		Sum += static_cast<int64>(static_cast<uint32>(Cell.Index)) * 31 + Cell.HeightM + Cell.Layer * 7919;
	}
	return Sum;
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
	THALASSA_PERF_SCOPE("chart sync");
	bTextureDirty = false;
	// The first time, nothing glows: a chart loaded or received on joining is not news.
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (const FSonarCell& Cell : Arrived)
	{
		const int32 Layer = FMath::Min<int32>(Cell.Layer, 1);
		const FIntPoint Where = ThalassaSonarCells::Unpack(Cell.Index);
		const FIntPoint PageKey(FMath::FloorToInt(Where.X / float(FPage::Size)), FMath::FloorToInt(Where.Y / float(FPage::Size)));
		TUniquePtr<FPage>& Page = Pages[Layer].FindOrAdd(PageKey);
		if (!Page)
		{
			Page = MakeUnique<FPage>();
		}
		const int32 Local = (Where.Y - PageKey.Y * FPage::Size) * FPage::Size + (Where.X - PageKey.X * FPage::Size);
		if (!Page->Known[Local] || Page->Height[Local] != Cell.HeightM)
		{
			Page->Known[Local] = true;
			Page->Height[Local] = Cell.HeightM;
			Page->Changed[Local] = bDenseSeeded ? Now : -1000.f;
		}
	}
	Arrived.Reset();
	bDenseSeeded = true;
}

bool UThalassaSonarComponent::DenseAt(int32 Layer, int32 X, int32 Y, int16& OutHeight, float* OutChanged) const
{
	const FIntPoint PageKey(FMath::FloorToInt(X / float(FPage::Size)), FMath::FloorToInt(Y / float(FPage::Size)));
	const TUniquePtr<FPage>* Page = Pages[Layer].Find(PageKey);
	if (!Page)
	{
		return false;
	}
	const int32 Local = (Y - PageKey.Y * FPage::Size) * FPage::Size + (X - PageKey.X * FPage::Size);
	if (!(*Page)->Known[Local])
	{
		return false;
	}
	OutHeight = (*Page)->Height[Local];
	if (OutChanged)
	{
		*OutChanged = (*Page)->Changed[Local];
	}
	return true;
}

bool UThalassaSonarComponent::SmoothHeightAt(float CellX, float CellY, float& OutHeightM, bool* OutFromPing) const
{
	// The survey first, between the centres of its 25 m cells; else the ping's 100 m cells, the same way.
	for (int32 Layer = 0; Layer < 2; ++Layer)
	{
		const float Scale = Layer == 0 ? 1.f : 1.f / PingCellCells;
		const float PX = CellX * Scale - 0.5f, PY = CellY * Scale - 0.5f;
		const int32 X0 = FMath::FloorToInt(PX), Y0 = FMath::FloorToInt(PY);
		const float FX = PX - X0, FY = PY - Y0;
		float Sum = 0.f, Weight = 0.f;
		for (int32 DX = 0; DX < 2; ++DX)
		{
			for (int32 DY = 0; DY < 2; ++DY)
			{
				int16 Height;
				if (DenseAt(Layer, X0 + DX, Y0 + DY, Height))
				{
					const float Wt = (DX ? FX : 1.f - FX) * (DY ? FY : 1.f - FY);
					Sum += Height * Wt;
					Weight += Wt;
				}
			}
		}
		if (Weight > 0.3f)
		{
			OutHeightM = Sum / Weight;
			if (OutFromPing)
			{
				*OutFromPing = Layer == 1;
			}
			return true;
		}
	}
	return false;
}

bool UThalassaSonarComponent::GetSurveyedHeightM(const FVector& WorldLocation, float& OutHeightM)
{
	if (bTextureDirty)
	{
		SyncDense();
	}
	for (int32 Layer = 0; Layer < 2; ++Layer)
	{
		const FIntPoint Cell = ThalassaSonarCells::CellOf(Layer, WorldLocation);
		int16 Height;
		if (DenseAt(Layer, Cell.X, Cell.Y, Height))
		{
			OutHeightM = Height;
			return true;
		}
	}
	return false;
}

bool UThalassaSonarComponent::SampleHeightM(const FVector& WorldLocation, float& OutHeightM)
{
	if (bTextureDirty)
	{
		SyncDense();
	}
	return SmoothHeightAt(WorldLocation.X / CellSizeCm, WorldLocation.Y / CellSizeCm, OutHeightM);
}

UTexture2D* UThalassaSonarComponent::GetLocalChart(const FVector& WorldCentre, int32 ViewCells, FVector2D& OutUV0, float& OutUVSize)
{
	if (bTextureDirty)
	{
		SyncDense();
	}

	// Continuous survey-cell coordinates of the centre: X along world X, Y along world Y.
	const FVector2D Centre(WorldCentre.X / CellSizeCm, WorldCentre.Y / CellSizeCm);
	const int32 Across = ViewCells + 8; // A margin, so the crop can follow the ship between rebuilds.
	const FIntPoint Wanted(FMath::FloorToInt(Centre.X) - Across / 2, FMath::FloorToInt(Centre.Y) - Across / 2);
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	// One step of building per frame, however many screens ask for the chart. The screens show the last
	// finished picture, at its own position, until the next is ready.
	if (GFrameCounter != BuildFrame)
	{
		BuildFrame = GFrameCounter;
		if (BuildStage == 0)
		{
			const bool bMoved = LocalCells != Across || FMath::Abs(Wanted.X - LocalOrigin.X) > 3 || FMath::Abs(Wanted.Y - LocalOrigin.Y) > 3;
			if (bMoved || !LocalChart || Now - LocalBuiltAt >= 0.2)
			{
				BuildOrigin = bMoved ? Wanted : LocalOrigin;
				BuildCells = Across;
				LocalBuiltAt = Now;
				StartChartBuild();
				BuildStage = 1;
			}
		}
		else
		{
			BuildChartRows(BuildStage - 1);
			if (++BuildStage > ChartSlices)
			{
				FinishChartBuild();
				BuildStage = 0;
			}
		}
	}

	if (!LocalChart || LocalCells == 0)
	{
		OutUV0 = FVector2D::ZeroVector;
		OutUVSize = 1.f;
		return nullptr;
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

void UThalassaSonarComponent::StartChartBuild()
{
	THALASSA_PERF_SCOPE("chart heights");
	const int32 W = BuildCells * PixelsPerCell;

	// The cells in view, copied out of the pages once into plain grids, so the hundreds of thousands of pixel
	// lookups below are array reads, not map searches. (Searching the pages per pixel cost 77 ms a rebuild.)
	struct FGrid
	{
		FIntPoint Origin;
		int32 Size = 0;
		TArray<int16> Height;
		TArray<float> Changed;
		TBitArray<> Known;
		bool At(int32 X, int32 Y, float& OutHeight) const
		{
			X -= Origin.X;
			Y -= Origin.Y;
			if (X < 0 || Y < 0 || X >= Size || Y >= Size || !Known[Y * Size + X])
			{
				return false;
			}
			OutHeight = Height[Y * Size + X];
			return true;
		}
		float ChangedAt(int32 X, int32 Y) const
		{
			X -= Origin.X;
			Y -= Origin.Y;
			return (X < 0 || Y < 0 || X >= Size || Y >= Size || !Known[Y * Size + X]) ? -1000.f : Changed[Y * Size + X];
		}
	};
	FGrid Grids[2];
	for (int32 Layer = 0; Layer < 2; ++Layer)
	{
		const int32 Per = Layer == 0 ? 1 : PingCellCells;
		FGrid& Grid = Grids[Layer];
		Grid.Origin = FIntPoint(FMath::FloorToInt((BuildOrigin.X - 1) / float(Per)) - 1, FMath::FloorToInt((BuildOrigin.Y - 1) / float(Per)) - 1);
		Grid.Size = (BuildCells + 2) / Per + 4;
		Grid.Height.SetNumZeroed(Grid.Size * Grid.Size);
		Grid.Changed.Init(-1000.f, Grid.Size * Grid.Size);
		Grid.Known.Init(false, Grid.Size * Grid.Size);
		for (int32 Y = 0; Y < Grid.Size; ++Y)
		{
			for (int32 X = 0; X < Grid.Size; ++X)
			{
				int16 Height;
				float Changed;
				if (DenseAt(Layer, Grid.Origin.X + X, Grid.Origin.Y + Y, Height, &Changed))
				{
					Grid.Height[Y * Grid.Size + X] = Height;
					Grid.Changed[Y * Grid.Size + X] = Changed;
					Grid.Known[Y * Grid.Size + X] = true;
				}
			}
		}
	}
	// The same rule as SmoothHeightAt: the survey between its cell centres, else the ping's cells.
	const auto Smooth = [&Grids](float CellX, float CellY, float& OutHeight, bool& bOutPing)
	{
		for (int32 Layer = 0; Layer < 2; ++Layer)
		{
			const float Scale = Layer == 0 ? 1.f : 1.f / PingCellCells;
			const float PX = CellX * Scale - 0.5f, PY = CellY * Scale - 0.5f;
			const int32 X0 = FMath::FloorToInt(PX), Y0 = FMath::FloorToInt(PY);
			const float FX = PX - X0, FY = PY - Y0;
			float Sum = 0.f, Weight = 0.f;
			for (int32 DX = 0; DX < 2; ++DX)
			{
				for (int32 DY = 0; DY < 2; ++DY)
				{
					float Height;
					if (Grids[Layer].At(X0 + DX, Y0 + DY, Height))
					{
						const float Wt = (DX ? FX : 1.f - FX) * (DY ? FY : 1.f - FY);
						Sum += Height * Wt;
						Weight += Wt;
					}
				}
			}
			if (Weight > 0.3f)
			{
				OutHeight = Sum / Weight;
				bOutPing = Layer == 1;
				return true;
			}
		}
		return false;
	};

	// Heights, smoothed between cell centres, and when each pixel's cell last changed. Unmeasured ground takes no part.
	BuildHeights.Init(TNumericLimits<float>::Max(), W * W);
	BuildChanged.Init(-1000.f, W * W);
	BuildFromPing.Init(false, W * W);
	BuildPixels.SetNumUninitialized(W * W);
	for (int32 V = 0; V < W; ++V)
	{
		const float CellX = BuildOrigin.X + BuildCells - (V + 0.5f) / PixelsPerCell;
		for (int32 U = 0; U < W; ++U)
		{
			const float CellY = BuildOrigin.Y + (U + 0.5f) / PixelsPerCell;
			float Height;
			bool bPing = false;
			if (Smooth(CellX, CellY, Height, bPing))
			{
				BuildHeights[V * W + U] = Height;
				BuildFromPing[V * W + U] = bPing;
				BuildChanged[V * W + U] = bPing
					? Grids[1].ChangedAt(FMath::FloorToInt(CellX / PingCellCells), FMath::FloorToInt(CellY / PingCellCells))
					: Grids[0].ChangedAt(FMath::FloorToInt(CellX), FMath::FloorToInt(CellY));
			}
		}
	}

	// Colours span what is in view, like a chart plotter: a gentle plain still shows its shape.
	float Low = TNumericLimits<float>::Max(), High = -TNumericLimits<float>::Max();
	for (const float H : BuildHeights)
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
		BuildDeepM = Middle - HalfSpan;
		BuildShallowM = Middle + HalfSpan;
	}
}

void UThalassaSonarComponent::BuildChartRows(int32 Slice)
{
	THALASSA_PERF_SCOPE("chart rows");
	const int32 W = BuildCells * PixelsPerCell;
	if (BuildHeights.Num() != W * W)
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float PixelM = CellSizeCm / 100.f / PixelsPerCell;

	// Contours: 10 m and 50 m over gentle ground, 20 m and 100 m over steep.
	const float Minor = BuildShallowM - BuildDeepM > 150.f ? 20.f : 10.f;
	const float Major = Minor * 5.f;

	// Linear colour to an sRGB byte through a table: the exact conversion's powers, per pixel, were most of the cost.
	static const TArray<uint8> SRGBTable = []
	{
		TArray<uint8> Table;
		Table.SetNumUninitialized(4096);
		for (int32 Index = 0; Index < 4096; ++Index)
		{
			Table[Index] = FLinearColor(Index / 4095.f, 0.f, 0.f).ToFColor(true).R;
		}
		return Table;
	}();
	const auto ToSRGB = [](float Linear) { return SRGBTable[FMath::Clamp(FMath::RoundToInt(Linear * 4095.f), 0, 4095)]; };

	// Colour, light from the north-west, contours, and the glow of news.
	const FVector Light = FVector(0.6f, -0.6f, 0.55f).GetSafeNormal();
	constexpr float Exaggeration = 6.f;
	const TArray<float>& Heights = BuildHeights;
	const int32 RowFrom = W * Slice / ChartSlices, RowTo = W * (Slice + 1) / ChartSlices;
	for (int32 V = RowFrom; V < RowTo; ++V)
	{
		const float CellX = BuildOrigin.X + BuildCells - (V + 0.5f) / PixelsPerCell;
		for (int32 U = 0; U < W; ++U)
		{
			const float CellY = BuildOrigin.Y + (U + 0.5f) / PixelsPerCell;
			const float H = Heights[V * W + U];
			if (H == TNumericLimits<float>::Max())
			{
				// Unknown water: near black, with a faint 100 m grid fixed to the world.
				const bool bGrid = FMath::Fmod(FMath::Abs(CellX), 4.f) < 1.f / PixelsPerCell || FMath::Fmod(FMath::Abs(CellY), 4.f) < 1.f / PixelsPerCell;
				BuildPixels[V * W + U] = bGrid ? FColor(8, 20, 27) : FColor(3, 8, 12);
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
			// Ping ground is shaded gently: its 100 m cells, lit as strongly as the survey's, show as square facets.
			const bool bPing = BuildFromPing[V * W + U];
			const float Relief = bPing ? Exaggeration * 0.3f : Exaggeration;
			const FVector Normal = FVector(-SlopeX * Relief, -SlopeY * Relief, 1.f).GetSafeNormal();
			const float Shade = FMath::Max(0.f, FVector::DotProduct(Normal, Light));
			FLinearColor Colour = DepthColour(H, BuildDeepM, BuildShallowM) * (0.3f + 0.95f * Shade);

			const float Right = At(U + 1, V), Below = At(U, V + 1);
			if (FMath::FloorToInt(H / Major) != FMath::FloorToInt(Right / Major) || FMath::FloorToInt(H / Major) != FMath::FloorToInt(Below / Major))
			{
				Colour = FMath::Lerp(Colour, FLinearColor(0.75f, 1.f, 0.95f), 0.6f);
			}
			else if (FMath::FloorToInt(H / Minor) != FMath::FloorToInt(Right / Minor) || FMath::FloorToInt(H / Minor) != FMath::FloorToInt(Below / Minor))
			{
				Colour = FMath::Lerp(Colour, FLinearColor(0.55f, 0.95f, 0.9f), 0.22f);
			}

			// Ground only the ping has seen is coarser: a little dimmer and greyer than surveyed ground.
			if (bPing)
			{
				Colour = FMath::Lerp(Colour, FLinearColor(Colour.GetLuminance(), Colour.GetLuminance(), Colour.GetLuminance()), 0.3f) * 0.8f;
			}

			const float Age = Now - BuildChanged[V * W + U];
			if (Age < 6.f)
			{
				Colour += FLinearColor(0.3f, 0.85f, 0.8f) * FMath::Exp(-Age / 1.2f) * 0.7f;
			}
			BuildPixels[V * W + U] = FColor(ToSRGB(Colour.R), ToSRGB(Colour.G), ToSRGB(Colour.B), 255);
		}
	}
}

void UThalassaSonarComponent::FinishChartBuild()
{
	THALASSA_PERF_SCOPE("chart upload");
	const int32 W = BuildCells * PixelsPerCell;
	if (BuildPixels.Num() != W * W)
	{
		return;
	}
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

	// Upload without stalling: the renderer frees the copy when it is done with it.
	uint8* Data = static_cast<uint8*>(FMemory::Malloc(BuildPixels.Num() * sizeof(FColor)));
	FMemory::Memcpy(Data, BuildPixels.GetData(), BuildPixels.Num() * sizeof(FColor));
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, W, W);
	LocalChart->UpdateTextureRegions(0, 1, Region, W * sizeof(FColor), sizeof(FColor), Data,
		[](uint8* SrcData, const FUpdateTextureRegion2D* Regions)
		{
			FMemory::Free(SrcData);
			delete Regions;
		});

	// The finished picture takes over: its place, its size, and its colour range for the legend.
	LocalOrigin = BuildOrigin;
	LocalCells = BuildCells;
	LocalDeepM = BuildDeepM;
	LocalShallowM = BuildShallowM;
}
