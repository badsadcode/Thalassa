#include "World/ThalassaMarineSnow.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Vessel/ThalassaVessel.h"

AThalassaMarineSnow::AThalassaMarineSnow()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // After the camera has moved this frame.
	bReplicates = false;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Tintable(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Specks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Specks"));
	RootComponent = Specks;
	Specks->SetMobility(EComponentMobility::Movable);
	Specks->SetStaticMesh(Cube.Object); // 12 triangles: at a centimetre across, the shape never shows.
	Specks->SetMaterial(0, Tintable.Object);
	Specks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Specks->SetCastShadow(false);
	Specks->SetCanEverAffectNavigation(false);
	Specks->SetUsingAbsoluteLocation(true);
	Specks->SetUsingAbsoluteRotation(true);
	Specks->SetUsingAbsoluteScale(true);
}

void AThalassaMarineSnow::BeginPlay()
{
	Super::BeginPlay();

	if (UMaterialInstanceDynamic* Material = Specks->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.8f, 0.72f));
	}

	// The same specks every time, so frames can be compared.
	FRandomStream Random(1871);
	Seeds.SetNum(Count);
	Drifts.SetNum(Count);
	Transforms.SetNum(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Seeds[Index] = FVector(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * BoxHalfCm;
		// Sinking slowly, wandering a little.
		Drifts[Index] = FVector(Random.FRandRange(-4.f, 4.f), Random.FRandRange(-4.f, 4.f), Random.FRandRange(-8.f, -2.f));
		const FRotator Spin(Random.FRandRange(0.f, 360.f), Random.FRandRange(0.f, 360.f), 0.f);
		Transforms[Index] = FTransform(Spin, FVector::ZeroVector, FVector(SpeckCm * Random.FRandRange(0.5f, 1.3f) / 100.f));
	}
	Specks->AddInstances(Transforms, false, true);
}

void AThalassaMarineSnow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;

	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Camera)
	{
		return;
	}
	const FVector Eye = Camera->GetCameraLocation();
	const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld());
	const float Size = BoxHalfCm * 2.f;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Fixed in the water, drifting, and wrapped into the box around the eye.
		FVector P = Seeds[Index] + Drifts[Index] * Clock - Eye;
		P.X = FMath::Fmod(FMath::Fmod(P.X + BoxHalfCm, Size) + Size, Size) - BoxHalfCm;
		P.Y = FMath::Fmod(FMath::Fmod(P.Y + BoxHalfCm, Size) + Size, Size) - BoxHalfCm;
		P.Z = FMath::Fmod(FMath::Fmod(P.Z + BoxHalfCm, Size) + Size, Size) - BoxHalfCm;
		const FVector World = Eye + P;

		const bool bIndoors = Vessel && Vessel->IsInCompartment(World);
		FTransform& Speck = Transforms[Index];
		Speck.SetLocation(bIndoors ? Eye - FVector(0.f, 0.f, 100000.f) : World); // Indoors: parked far below, out of sight.
	}
	Specks->BatchUpdateInstancesTransforms(0, Transforms, true, true);
}
