#include "Tools/ThalassaArtTools.h"

#include "Thalassa.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "PackedLevelActor/PackedLevelActorBuilder.h"

namespace ThalassaArt
{
	const TCHAR* const Folder = TEXT("/Game/Ship/Art");
	const TCHAR* const LevelPrefix = TEXT("L_Art_");
	const TCHAR* const BlueprintPrefix = TEXT("BPP_");

	/** A light in the room level, as a component of the room's Blueprint: what it looks like, not where it hangs in the level. */
	void CopyLight(const UPointLightComponent* From, UPointLightComponent* To)
	{
		To->SetRelativeTransform(From->GetComponentTransform());   // The level is in ship coordinates.
		To->Mobility = EComponentMobility::Movable;                 // The ship moves.
		To->IntensityUnits = From->IntensityUnits;
		To->Intensity = From->Intensity;
		To->LightColor = From->LightColor;
		To->bUseTemperature = From->bUseTemperature;
		To->Temperature = From->Temperature;
		To->AttenuationRadius = From->AttenuationRadius;
		To->SourceRadius = From->SourceRadius;
		To->SoftSourceRadius = From->SoftSourceRadius;
		To->SourceLength = From->SourceLength;
		To->CastShadows = From->CastShadows;
		To->bUseInverseSquaredFalloff = From->bUseInverseSquaredFalloff;
		To->LightFalloffExponent = From->LightFalloffExponent;
		To->VolumetricScatteringIntensity = From->VolumetricScatteringIntensity;
		if (const USpotLightComponent* Spot = Cast<USpotLightComponent>(From))
		{
			USpotLightComponent* ToSpot = CastChecked<USpotLightComponent>(To);
			ToSpot->InnerConeAngle = Spot->InnerConeAngle;
			ToSpot->OuterConeAngle = Spot->OuterConeAngle;
		}
	}
}
#endif

FString UThalassaArtTools::PackRoom(const FString& LevelPath)
{
#if WITH_EDITOR
	using namespace ThalassaArt;
	const FString LevelName = FPackageName::GetShortName(LevelPath);
	if (!LevelName.StartsWith(LevelPrefix))
	{
		UE_LOG(LogThalassa, Warning, TEXT("ART: %s isn't a room level (they are named %s<Room>)"), *LevelPath, LevelPrefix);
		return FString();
	}
	const FString Room = LevelName.RightChop(FCString::Strlen(LevelPrefix));
	const FString BlueprintName = FString(BlueprintPrefix) + Room;
	const FString BlueprintPath = FPackageName::GetLongPackagePath(LevelPath) / BlueprintName;
	const TSoftObjectPtr<UWorld> WorldAsset(FSoftObjectPath(LevelPath + TEXT(".") + LevelName));
	const TSoftObjectPtr<UBlueprint> BlueprintAsset(FSoftObjectPath(BlueprintPath + TEXT(".") + BlueprintName));

	UBlueprint* Blueprint = BlueprintAsset.LoadSynchronous();
	if (!Blueprint)
	{
		Blueprint = FPackedLevelActorBuilder::CreatePackedLevelActorBlueprint(BlueprintAsset, WorldAsset, true);
	}
	if (!Blueprint || !FPackedLevelActorBuilder::CreateDefaultBuilder()->CreateOrUpdateBlueprint(WorldAsset, BlueprintAsset, false, false))
	{
		UE_LOG(LogThalassa, Error, TEXT("ART: packing %s failed (see the PackedLevelActor message log)"), *LevelPath);
		return FString();
	}
	Blueprint = BlueprintAsset.LoadSynchronous();
	USimpleConstructionScript* Construction = Blueprint ? Blueprint->SimpleConstructionScript.Get() : nullptr;
	UWorld* World = WorldAsset.LoadSynchronous();
	if (!Construction || !World || !World->PersistentLevel)
	{
		return FString();
	}

	// Everything moves with the ship. Lights from an earlier packing go; the level's lights come in again.
	for (USCS_Node* Node : Construction->GetAllNodes())
	{
		if (Node->ComponentTemplate && Node->ComponentTemplate->IsA<ULightComponent>())
		{
			Construction->RemoveNodeAndPromoteChildren(Node);
		}
		else if (USceneComponent* Scene = Cast<USceneComponent>(Node->ComponentTemplate))
		{
			Scene->Mobility = EComponentMobility::Movable;
			// Scenery, with no collision at all: players never stand on art (docs/SHIP_ART.md). Only ignoring
			// pawns wasn't enough: on a joining player's machine the room can exist before the ship makes it
			// scenery, and the walkers stood on its floor there and not on the host's.
			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Scene))
			{
				Primitive->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision, false);
				Primitive->CanCharacterStepUpOn = ECB_No;
				Primitive->SetCanEverAffectNavigation(false);
			}
		}
	}
	int32 Lights = 0;
	for (const AActor* Actor : World->PersistentLevel->Actors)
	{
		if (!Actor)
		{
			continue;
		}
		TInlineComponentArray<UPointLightComponent*> Found(Actor);
		for (const UPointLightComponent* Light : Found)
		{
			USCS_Node* Node = Construction->CreateNode(Light->GetClass(), FName(TEXT("RoomLight"), Lights + 1));
			ThalassaArt::CopyLight(Light, CastChecked<UPointLightComponent>(Node->ComponentTemplate));
			Construction->AddNode(Node);
			++Lights;
		}
	}
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	UEditorLoadingAndSavingUtils::SavePackages({ Blueprint->GetPackage() }, false);
	UE_LOG(LogThalassa, Display, TEXT("ART: packed %s into %s, with %d lights"), *LevelPath, *BlueprintPath, Lights);
	return BlueprintPath;
#else
	return FString();
#endif
}

int32 UThalassaArtTools::PackAllRooms()
{
	int32 Packed = 0;
#if WITH_EDITOR
	TArray<FAssetData> Assets;
	FAssetRegistryModule::GetRegistry().GetAssetsByPath(FName(ThalassaArt::Folder), Assets, true);
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetClassPath == UWorld::StaticClass()->GetClassPathName() && Asset.AssetName.ToString().StartsWith(ThalassaArt::LevelPrefix))
		{
			Packed += PackRoom(Asset.PackageName.ToString()).IsEmpty() ? 0 : 1;
		}
	}
#endif
	return Packed;
}

#if WITH_EDITOR
static FAutoConsoleCommand GThalassaPackRooms(
	TEXT("Thalassa.PackRooms"),
	TEXT("Packs every room of the ship's art (/Game/Ship/Art/L_Art_*) into the Blueprints the ship carries."),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		const int32 Packed = UThalassaArtTools::PackAllRooms();
		UE_LOG(LogThalassa, Display, TEXT("ART: %d room(s) packed"), Packed);
	}));
#endif
