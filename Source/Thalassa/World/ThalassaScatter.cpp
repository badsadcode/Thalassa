#include "World/ThalassaScatter.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AThalassaScatter::AThalassaScatter()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // Every machine loads the same level.

	Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Instances"));
	RootComponent = Instances;
	Instances->SetMobility(EComponentMobility::Static);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetCastShadow(false); // Small and countless; the floodlights cast no shadows anyway.
}

void AThalassaScatter::SetScatterMesh(UStaticMesh* Mesh, float CullDistanceCm)
{
	Instances->SetStaticMesh(Mesh);
	Instances->SetCullDistances(0, FMath::RoundToInt(CullDistanceCm));
}

void AThalassaScatter::AddWorldInstances(const TArray<FTransform>& Transforms)
{
	Instances->AddInstances(Transforms, false, true);
}
