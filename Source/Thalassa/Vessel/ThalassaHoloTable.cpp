#include "Vessel/ThalassaHoloTable.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Vessel/ThalassaSonarComponent.h"
#include "Vessel/ThalassaVessel.h"

UThalassaHoloTableComponent::UThalassaHoloTableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetMobility(EComponentMobility::Movable);
}

AThalassaVessel* UThalassaHoloTableComponent::GetVessel() const
{
	return Cast<AThalassaVessel>(GetOwner());
}

void UThalassaHoloTableComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Points || GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}
	const int32 Count = Samples * Samples + ShipPoints + DropPoints;
	Transforms.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), Count);
	Colours.Init(0.f, Count * 3);
	Points->AddInstances(Transforms, false);
	Rebuild();
}

void UThalassaHoloTableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Timer += DeltaTime;
	if (Timer >= 0.2f)
	{
		Timer = 0.f;
		Rebuild();
	}
}

void UThalassaHoloTableComponent::Rebuild()
{
	AThalassaVessel* Vessel = GetVessel();
	UThalassaSonarComponent* Sonar = Vessel ? Vessel->GetSonar() : nullptr;
	if (!Sonar || !Points)
	{
		return;
	}

	// Heading-up: the samples turn with the ship, so ahead on the table is ahead in the ship.
	const FVector Ship = Vessel->GetActorLocation();
	const FQuat Heading = FRotator(0.f, Vessel->GetActorRotation().Yaw, 0.f).Quaternion();
	const float Scale = HalfWidthCm / (ReachM * 100.f); // Table centimetres per world centimetre.

	TArray<float> Heights;
	Heights.Init(TNumericLimits<float>::Max(), Samples * Samples);
	float Low = TNumericLimits<float>::Max(), High = -TNumericLimits<float>::Max();
	for (int32 Row = 0; Row < Samples; ++Row)
	{
		for (int32 Column = 0; Column < Samples; ++Column)
		{
			const FVector Offset(((Row + 0.5f) / Samples - 0.5f) * 2.f * ReachM * 100.f, ((Column + 0.5f) / Samples - 0.5f) * 2.f * ReachM * 100.f, 0.f);
			float HeightM = 0.f;
			if (Sonar->SampleHeightM(Ship + Heading.RotateVector(Offset), HeightM))
			{
				Heights[Row * Samples + Column] = HeightM;
				Low = FMath::Min(Low, HeightM);
				High = FMath::Max(High, HeightM);
			}
		}
	}
	if (Low > High)
	{
		Low = High = Ship.Z / 100.f - 300.f;
	}
	const float ColourLow = (Low + High) * 0.5f - FMath::Max((High - Low) * 0.5f, 30.f);
	const float ColourHigh = (Low + High) * 0.5f + FMath::Max((High - Low) * 0.5f, 30.f);
	// Heights stretched for shape, but never taller than the hologram allows: steep ground is flattened to fit.
	const float Vertical = FMath::Min(Scale * Exaggeration, ReliefCm / FMath::Max((High - Low) * 100.f, 1.f));
	const auto TableZ = [&](float HeightM) { return 1.5f + FMath::Clamp((HeightM - Low) * 100.f * Vertical, 0.f, MaxHeightCm); };
	const auto SetColour = [&](int32 Index, const FLinearColor& Colour)
	{
		Colours[Index * 3 + 0] = Colour.R;
		Colours[Index * 3 + 1] = Colour.G;
		Colours[Index * 3 + 2] = Colour.B;
	};

	// The seabed: one point per sample, coloured by depth like the chart. Unknown ground shows nothing.
	const FVector PointSize(0.008f);
	for (int32 Row = 0; Row < Samples; ++Row)
	{
		for (int32 Column = 0; Column < Samples; ++Column)
		{
			const int32 Index = Row * Samples + Column;
			const float HeightM = Heights[Index];
			if (HeightM == TNumericLimits<float>::Max())
			{
				Transforms[Index].SetScale3D(FVector::ZeroVector);
				continue;
			}
			const FVector Place(((Row + 0.5f) / Samples - 0.5f) * 2.f * HalfWidthCm, ((Column + 0.5f) / Samples - 0.5f) * 2.f * HalfWidthCm, TableZ(HeightM));
			Transforms[Index] = FTransform(FQuat::Identity, Place, PointSize);
			SetColour(Index, UThalassaSonarComponent::DepthColour(HeightM, ColourLow, ColourHigh) * 0.22f + FLinearColor(0.004f, 0.015f, 0.02f));
		}
	}

	// The ship: an amber chevron at its own height, and a line dropped to the seabed beneath it.
	const float ShipZ = TableZ(Ship.Z / 100.f);
	const FLinearColor Amber(1.f, 0.45f, 0.08f);
	const FVector Chevron[ShipPoints] = { FVector(3.f, 0.f, 0.f), FVector(1.5f, -1.f, 0.f), FVector(1.5f, 1.f, 0.f), FVector(0.f, -2.f, 0.f),
		FVector(0.f, 2.f, 0.f), FVector(0.f, 0.f, 0.f), FVector(-1.5f, 0.f, 0.f) };
	int32 Index = Samples * Samples;
	for (const FVector& Point : Chevron)
	{
		// The ship to the table's scale: 280 m is about 35 cm.
		Transforms[Index] = FTransform(FQuat::Identity, FVector(0.f, 0.f, ShipZ) + Point * 6.f, FVector(0.02f));
		SetColour(Index++, Amber);
	}
	float FloorM = 0.f;
	const bool bFloor = Sonar->SampleHeightM(Ship, FloorM);
	const float FloorZ = bFloor ? TableZ(FloorM) : 1.5f;
	for (int32 Step = 0; Step < DropPoints; ++Step)
	{
		const float Z = FMath::Lerp(ShipZ, FloorZ, (Step + 1.f) / (DropPoints + 1.f));
		Transforms[Index] = FTransform(FQuat::Identity, FVector(0.f, 0.f, Z), FVector(0.005f));
		SetColour(Index++, Amber * 0.5f);
	}

	Points->BatchUpdateInstancesTransforms(0, Transforms, false, false);
	for (int32 Instance = 0; Instance < Transforms.Num(); ++Instance)
	{
		Points->SetCustomData(Instance, TArrayView<const float>(&Colours[Instance * 3], 3), false);
	}
	Points->MarkRenderStateDirty();
}
