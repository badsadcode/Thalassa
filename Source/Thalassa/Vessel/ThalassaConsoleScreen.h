#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "ThalassaConsoleScreen.generated.h"

class AThalassaVessel;
class UCanvas;
class UCanvasRenderTarget2D;
class UMaterialInstanceDynamic;

/** Simple canvas drawing shared by the console screens and the HUD, so both look alike. */
namespace ThalassaScreenDraw
{
	extern const FLinearColor Cyan;
	extern const FLinearColor Amber;

	/** Canvas lines ignore alpha, so faint lines are drawn in a darker colour instead. */
	FLinearColor Dim(const FLinearColor& Color, float Strength);

	void Line(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness);

	/** A circle, leaving out any segment that would stray outside the clip box. */
	void Ring(UCanvas* Canvas, const FVector2D& Center, float Radius, const FBox2D& Clip, const FLinearColor& Color, float Thickness);

	void Text(UCanvas* Canvas, const FString& String, const FVector2D& Position, const FLinearColor& Color, float Scale);

	/** A filled rectangle. */
	void Fill(UCanvas* Canvas, const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color);

	/** A rectangle's outline. */
	void Frame(UCanvas* Canvas, const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color, float Thickness);
}

/**
 * A screen in the world. Each process draws its own picture into a render target from replicated
 * state, so nothing about the screen itself is replicated. Subclasses say what to draw.
 *
 * Built on the engine's 100 cm plane, which faces +Z: the vessel turns it to face the room.
 */
UCLASS(Abstract)
class THALASSA_API UThalassaConsoleScreenComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UThalassaConsoleScreenComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The relative rotation that turns the plane to face this way, with the picture's top edge up. */
	static FRotator FacingRotation(const FVector& Facing);

protected:
	/** Draw the picture into a square of this many pixels. */
	virtual void DrawContent(UCanvas* Canvas, float Size, const AThalassaVessel& Vessel) {}

	AThalassaVessel* GetVessel() const;

private:
	UFUNCTION()
	void DrawScreen(UCanvas* Canvas, int32 Width, int32 Height);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasRenderTarget2D> RenderTarget;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScreenMaterial;

	float RedrawTimer = 0.f;
};
