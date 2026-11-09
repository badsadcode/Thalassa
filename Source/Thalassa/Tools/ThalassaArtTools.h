// Editor tools for the ship's art: rooms built as ordinary levels, packed into Blueprints the ship carries.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ThalassaArtTools.generated.h"

/**
 * A room of the ship's art is built as an ordinary level, /Game/Ship/Art/L_Art_<Room>, in ship coordinates, so it
 * can be edited with the normal editor tools. Packing turns it into /Game/Ship/Art/BPP_<Room>, a Packed Level
 * Actor Blueprint (its meshes drawn as instances), which BP_Thalassa holds as a Child Actor.
 *
 * Packing on its own would leave two traps, which this takes care of:
 *  - Pieces dropped into a level are Static. The ship moves, so everything packed is made Movable.
 *  - Packing keeps only meshes. Lights in the level are copied into the Blueprint as well.
 *
 * In the editor: the console command Thalassa.PackRooms packs every L_Art_ level. Editor only; does nothing in
 * the game.
 */
UCLASS()
class THALASSA_API UThalassaArtTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Packs one room level (e.g. /Game/Ship/Art/L_Art_AftCorridor). Returns the Blueprint's path, or "" on failure. */
	UFUNCTION(BlueprintCallable, Category = "Thalassa|Art")
	static FString PackRoom(const FString& LevelPath);

	/** Packs every /Game/Ship/Art/L_Art_* level. Returns how many were packed. */
	UFUNCTION(BlueprintCallable, Category = "Thalassa|Art")
	static int32 PackAllRooms();
};
