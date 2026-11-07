#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "ThalassaSonarDisplay.generated.h"

class AThalassaVessel;
class UCanvas;
class UCanvasRenderTarget2D;
class UMaterialInstanceDynamic;
class UThalassaSonarPingComponent;

/**
 * How the sonar survey is drawn. The HUD and the console screen on the bridge both use this, so they
 * always show the same picture. The survey itself is UThalassaSonarComponent; nothing here changes it.
 */
namespace ThalassaSonarDisplay
{
	/** Seconds between audible pings, and between the rings drawn on the chart. */
	constexpr float PingIntervalSeconds = 2.f;

	/** Seconds since the last ping, the same on every machine, or a negative number when the sonar is off. */
	float GetPingAge(const AThalassaVessel& Vessel);

	/** Which ping this is, counted from the start of the server's clock. Changes once per ping. */
	int64 GetPingNumber(const AThalassaVessel& Vessel);

	/** Draws the chart around the ship into a square on the canvas, with its legend inside the square. */
	void Draw(UCanvas* Canvas, const FVector2D& TopLeft, float Size, const AThalassaVessel& Vessel);
}

/**
 * The sonar chart on a screen in the world. Each process draws its own copy into a render target from
 * the replicated survey, and plays the ping from the screen. Nothing here is replicated.
 */
UCLASS()
class THALASSA_API UThalassaSonarScreenComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UThalassaSonarScreenComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Where the ping is heard from. Set by the vessel when it builds the screen. */
	UPROPERTY()
	TObjectPtr<UThalassaSonarPingComponent> PingSound;

private:
	UFUNCTION()
	void DrawScreen(UCanvas* Canvas, int32 Width, int32 Height);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasRenderTarget2D> RenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScreenMaterial;

	int64 LastPingNumber = -1;
	float RedrawTimer = 0.f;
};
