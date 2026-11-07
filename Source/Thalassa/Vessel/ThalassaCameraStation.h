#pragma once

#include "CoreMinimal.h"
#include "Vessel/ThalassaConsoleScreen.h"
#include "Vessel/ThalassaStationComponent.h"
#include "ThalassaCameraStation.generated.h"

class UCanvas;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/**
 * The camera station: cameras on the hull, shown on the station's screens. Whoever is at the station
 * chooses which camera the big screen shows, and everyone sees that choice.
 *
 * Which camera is selected is replicated. The pictures are not: every machine renders its own, from its
 * own copy of the world, and only while someone there can see a screen.
 */
UCLASS()
class THALASSA_API UThalassaCameraStationComponent : public UThalassaStationComponent
{
	GENERATED_BODY()

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server: the person at the station steps to the next camera, or the previous one. */
	void Select(const APawn* From, int32 Step);

	int32 GetSelected() const { return Selected; }
	int32 GetCameraCount() const { return Cameras.Num(); }
	FString GetLabel(int32 Index) const { return Labels.IsValidIndex(Index) ? Labels[Index] : FString(); }

	/** The latest picture from a camera, or null on machines that draw nothing. */
	UTextureRenderTarget2D* GetFeed(int32 Index) const { return Feeds.IsValidIndex(Index) ? Feeds[Index].Get() : nullptr; }

	/** Something is showing the feeds: keep them coming for a moment. Called by screens and the HUD. */
	void RequestFeeds();

	/** Set by the vessel when it builds the cameras. */
	UPROPERTY()
	TArray<TObjectPtr<USceneCaptureComponent2D>> Cameras;

	UPROPERTY()
	TArray<FString> Labels;

private:
	void CreateFeeds();

	UPROPERTY(Replicated)
	int32 Selected = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> Feeds;

	bool bFeedsCreated = false;
	double LastRequestTime = -100.0;
	float SelectedTimer = 0.f;
	float OthersTimer = 0.f;
	int32 NextOther = 0;
};

/** How camera feeds are drawn. The station's screens and the HUD both use this. */
namespace ThalassaCameraDisplay
{
	/** One camera's picture, letterboxed into the rectangle, with its name. */
	void DrawFeed(UCanvas* Canvas, const FVector2D& TopLeft, const FVector2D& Size, const UThalassaCameraStationComponent& Station, int32 Index, bool bHighlight);

	/** Every camera at once, in a grid, the selected one outlined. */
	void DrawGrid(UCanvas* Canvas, const FVector2D& TopLeft, const FVector2D& Size, const UThalassaCameraStationComponent& Station);
}

/** One of the camera station's screens: the selected camera, or all of them. */
UCLASS()
class THALASSA_API UThalassaCameraScreenComponent : public UThalassaConsoleScreenComponent
{
	GENERATED_BODY()

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True for the screen showing every camera; false for the one showing the selected camera. */
	UPROPERTY()
	bool bShowsAll = false;

protected:
	virtual void DrawContent(UCanvas* Canvas, float Size, const AThalassaVessel& Vessel) override;
};
