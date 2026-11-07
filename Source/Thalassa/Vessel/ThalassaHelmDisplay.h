#pragma once

#include "CoreMinimal.h"
#include "Vessel/ThalassaConsoleScreen.h"
#include "ThalassaHelmDisplay.generated.h"

class AThalassaVessel;
class UCanvas;

/** How the helm is drawn. The HUD and the helm's console screen both use this, so they always agree. */
namespace ThalassaHelmDisplay
{
	/** The helm's levers and the ship's motion, drawn into a square on the canvas. */
	void Draw(UCanvas* Canvas, const FVector2D& TopLeft, float Size, const AThalassaVessel& Vessel);
}

/** The helm's screen on the bridge. */
UCLASS()
class THALASSA_API UThalassaHelmScreenComponent : public UThalassaConsoleScreenComponent
{
	GENERATED_BODY()

protected:
	virtual void DrawContent(UCanvas* Canvas, float Size, const AThalassaVessel& Vessel) override;
};
