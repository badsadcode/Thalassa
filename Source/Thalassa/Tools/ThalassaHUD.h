#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ThalassaHUD.generated.h"

/**
 * Diagnostic overlay plus a stand-in for an expanded station view (Tab). Shows the same replicated
 * data as the physical console. A canvas is enough to prove that; the real station UI will be UMG.
 */
UCLASS()
class THALASSA_API AThalassaHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void TickScreenshotRequest();

	float ScreenshotClock = 0.f;
	bool bScreenshotRequested = false;

	void DrawLines(const TArray<FString>& Lines, float X, float Y, const FLinearColor& Color, float Scale);
};
