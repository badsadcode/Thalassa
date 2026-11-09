#include "Vessel/ThalassaWaterSurface.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace ThalassaWater
{
	constexpr float CellCm = 12.f;          // Grid spacing: fine enough for the waves to bend it smoothly.
	constexpr int32 MaxCells = 256;         // Along one side, for the biggest compartments.
	constexpr float Gravity = 980.f;        // cm/s².
	constexpr float SloshDamping = 0.07f;   // Of critical: water keeps sloshing a long time.
	constexpr float MaxEdgeRiseCm = 35.f;   // However hard the ship moves, the water climbs a wall no more than this.
}

UThalassaWaterSurfaceComponent::UThalassaWaterSurfaceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(false);
	bUseAsyncCooking = false;
	SetMobility(EComponentMobility::Movable);
}

void UThalassaWaterSurfaceComponent::Build(const FVector2D& SizeCm)
{
	using namespace ThalassaWater;
	if (SizeCm.Equals(BuiltSize, 1.f))
	{
		return;
	}
	BuiltSize = SizeCm;

	const int32 Nx = FMath::Clamp(FMath::CeilToInt(SizeCm.X / CellCm), 2, MaxCells);
	const int32 Ny = FMath::Clamp(FMath::CeilToInt(SizeCm.Y / CellCm), 2, MaxCells);
	GridCm = FMath::Max(SizeCm.X / Nx, SizeCm.Y / Ny);
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;
	Vertices.Reserve((Nx + 1) * (Ny + 1));
	for (int32 Y = 0; Y <= Ny; ++Y)
	{
		for (int32 X = 0; X <= Nx; ++X)
		{
			const FVector2D UV(static_cast<float>(X) / Nx, static_cast<float>(Y) / Ny);
			// Real centimetres in the component's own space: the material makes its waves from them.
			Vertices.Add(FVector((UV.X - 0.5f) * SizeCm.X, (UV.Y - 0.5f) * SizeCm.Y, 0.f));
			Normals.Add(FVector::UpVector);
			UVs.Add(UV);
			Tangents.Add(FProcMeshTangent(1.f, 0.f, 0.f));
		}
	}
	Triangles.Reserve(Nx * Ny * 6);
	for (int32 Y = 0; Y < Ny; ++Y)
	{
		for (int32 X = 0; X < Nx; ++X)
		{
			const int32 A = Y * (Nx + 1) + X;
			const int32 B = A + 1;
			const int32 C = A + Nx + 1;
			const int32 D = C + 1;
			// Facing up.
			Triangles.Append({ A, C, B, B, C, D });
		}
	}
	CreateMeshSection(0, Vertices, Triangles, Normals, UVs, TArray<FColor>(), Tangents, false);
	// Waves lift it a little; keep it from being culled while they do.
	SetBoundsScale(1.5f);

	if (!Look)
	{
		if (UMaterialInterface* Water = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ship/Water/M_InteriorWater.M_InteriorWater")))
		{
			Look = UMaterialInstanceDynamic::Create(Water, this);
		}
	}
	if (Look)
	{
		SetMaterial(0, Look);
	}
}

void UThalassaWaterSurfaceComponent::UpdateWater(float DeltaSeconds, float Churn, float DepthCm)
{
	using namespace ThalassaWater;
	if (DeltaSeconds <= 0.f)
	{
		return;
	}

	// How the ship is moving, from where this surface has been. Smoothed: two differences of a frame are noisy.
	const FVector Location = GetComponentLocation();
	const FVector Velocity = FramesSeen > 0 ? (Location - LastLocation) / DeltaSeconds : FVector::ZeroVector;
	const FVector RawAcceleration = FramesSeen > 1 ? (Velocity - LastVelocity) / DeltaSeconds : FVector::ZeroVector;
	Acceleration = FMath::Lerp(Acceleration, RawAcceleration.GetClampedToMaxSize(3000.f), 1.f - FMath::Exp(-DeltaSeconds / 0.12f));
	LastLocation = Location;
	LastVelocity = Velocity;
	FramesSeen = FMath::Min(FramesSeen + 1, 2);

	// Water settles square to the gravity it feels: real gravity, less how the ship is accelerating, as the
	// ship's own tilt sees it. Where it would settle is a tilt; the water swings towards it and past it.
	const FVector Felt = GetComponentTransform().InverseTransformVectorNoScale(FVector(0.f, 0.f, -Gravity) - Acceleration);
	const FVector2D Target = Felt.Z < -1.f ? FVector2D(-Felt.X / Felt.Z, -Felt.Y / Felt.Z) : FVector2D::ZeroVector;
	// A tank's slosh is slower the longer and shallower it is: w² = g pi / L tanh(pi d / L).
	const float Depth = FMath::Max(DepthCm, 5.f);
	const auto Omega = [Depth](float Length)
	{
		const float L = FMath::Max(Length, 50.f);
		return FMath::Sqrt(Gravity * PI / L * FMath::Tanh(PI * Depth / L));
	};
	const FVector2D W(Omega(BuiltSize.X), Omega(BuiltSize.Y));
	// Steps of at most a sixtieth, so a slow frame can't throw the spring.
	float Left = FMath::Min(DeltaSeconds, 0.25f);
	while (Left > 0.f)
	{
		const float Step = FMath::Min(Left, 1.f / 60.f);
		Left -= Step;
		const FVector2D Pull = (Target - Tilt) * W * W - TiltRate * W * (2.f * SloshDamping);
		TiltRate += Pull * Step;
		Tilt += TiltRate * Step;
	}
	// No higher up a wall than the water could go.
	const float MaxRise = FMath::Min(MaxEdgeRiseCm, Depth);
	Tilt.X = FMath::Clamp(Tilt.X, -MaxRise / FMath::Max(BuiltSize.X * 0.5f, 1.f), MaxRise / FMath::Max(BuiltSize.X * 0.5f, 1.f));
	Tilt.Y = FMath::Clamp(Tilt.Y, -MaxRise / FMath::Max(BuiltSize.Y * 0.5f, 1.f), MaxRise / FMath::Max(BuiltSize.Y * 0.5f, 1.f));

	if (Look)
	{
		// Shallow water can't make big waves.
		Look->SetScalarParameterValue(TEXT("Churn"), FMath::Clamp(Churn, 0.f, 1.f));
		Look->SetScalarParameterValue(TEXT("WaveScale"), FMath::Clamp(Depth / 40.f, 0.1f, 1.f));
		// Waves too short for the grid to bend smoothly are left to the ripples in the pixel shader.
		Look->SetScalarParameterValue(TEXT("GridCm"), GridCm);
		Look->SetVectorParameterValue(TEXT("Slosh"), FLinearColor(Tilt.X, Tilt.Y, 0.f, 0.f));
	}
}
