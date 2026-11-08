#include "Vessel/ThalassaConsoleScreen.h"
#include "Tools/ThalassaPerf.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "UObject/ConstructorHelpers.h"
#include "Vessel/ThalassaVessel.h"

namespace ThalassaScreenDraw
{
	const FLinearColor Cyan(0.35f, 0.84f, 0.83f);
	const FLinearColor Amber(0.91f, 0.73f, 0.45f);

	FLinearColor Dim(const FLinearColor& Color, float Strength)
	{
		return FLinearColor(Color.R * Strength, Color.G * Strength, Color.B * Strength, 1.f);
	}

	void Line(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness)
	{
		FCanvasLineItem Item(A, B);
		Item.SetColor(Color);
		Item.LineThickness = Thickness;
		Canvas->DrawItem(Item);
	}

	void Ring(UCanvas* Canvas, const FVector2D& Center, float Radius, const FBox2D& Clip, const FLinearColor& Color, float Thickness)
	{
		constexpr int32 Segments = 96;
		FVector2D Previous = Center + FVector2D(Radius, 0.f);
		for (int32 Index = 1; Index <= Segments; ++Index)
		{
			const float Angle = 2.f * PI * Index / Segments;
			const FVector2D Next = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
			if (Clip.IsInside(Previous) && Clip.IsInside(Next))
			{
				Line(Canvas, Previous, Next, Color, Thickness);
			}
			Previous = Next;
		}
	}

	void Text(UCanvas* Canvas, const FString& String, const FVector2D& Position, const FLinearColor& Color, float Scale)
	{
		FCanvasTextItem Item(Position, FText::FromString(String), GEngine->GetSmallFont(), Color);
		Item.Scale = FVector2D(Scale);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
	}

	void Fill(UCanvas* Canvas, const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color)
	{
		FCanvasTileItem Item(TopLeft, Size, Color);
		Item.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(Item);
	}

	void Frame(UCanvas* Canvas, const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Color, float Thickness)
	{
		const FVector2D TR = TopLeft + FVector2D(Size.X, 0.f), BL = TopLeft + FVector2D(0.f, Size.Y), BR = TopLeft + Size;
		Line(Canvas, TopLeft, TR, Color, Thickness);
		Line(Canvas, TR, BR, Color, Thickness);
		Line(Canvas, BR, BL, Color, Thickness);
		Line(Canvas, BL, TopLeft, Color, Thickness);
	}
}

namespace ThalassaConsoleScreen
{
	constexpr int32 Resolution = 512;

	// The material is unlit, and the dimly lit bridge makes the camera's exposure rise, so a screen at
	// full strength glows white. This keeps it a lit display rather than a lamp.
	constexpr float Brightness = 0.12f;

	// 30 times a second keeps moving things, like the sonar's ring, smooth.
	constexpr float RedrawSeconds = 1.f / 30.f;
}

UThalassaConsoleScreenComponent::UThalassaConsoleScreenComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	// An unlit material with a texture parameter, shipped with the engine for 3D widgets. It is enough for a screen.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ScreenBase(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided.Widget3DPassThrough_Opaque_OneSided"));

	SetMobility(EComponentMobility::Movable);
	SetStaticMesh(PlaneMesh.Object);
	SetMaterial(0, ScreenBase.Object);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(false);
}

FRotator UThalassaConsoleScreenComponent::FacingRotation(const FVector& Facing)
{
	// The plane's texture runs along its +X and +Y: put +X to the viewer's right and +Y downward.
	return FRotationMatrix::MakeFromXZ(Facing ^ FVector::UpVector, Facing).Rotator();
}

AThalassaVessel* UThalassaConsoleScreenComponent::GetVessel() const
{
	return Cast<AThalassaVessel>(GetOwner());
}

void UThalassaConsoleScreenComponent::BeginPlay()
{
	Super::BeginPlay();

	// Tests run without a renderer, and a dedicated server has nobody to show anything to.
	if (GetNetMode() == NM_DedicatedServer || !FApp::CanEverRender())
	{
		return;
	}

	using namespace ThalassaConsoleScreen;
	RenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(this, UCanvasRenderTarget2D::StaticClass(), Resolution, Resolution);
	if (!RenderTarget)
	{
		return;
	}
	RenderTarget->ClearColor = FLinearColor(0.01f, 0.02f, 0.03f);
	RenderTarget->OnCanvasRenderTargetUpdate.AddDynamic(this, &UThalassaConsoleScreenComponent::DrawScreen);

	ScreenMaterial = CreateAndSetMaterialInstanceDynamic(0);
	if (ScreenMaterial)
	{
		ScreenMaterial->SetTextureParameterValue(TEXT("SlateUI"), RenderTarget);
		ScreenMaterial->SetVectorParameterValue(TEXT("TintColorAndOpacity"), FLinearColor(Brightness, Brightness, Brightness, 1.f));
	}
	RenderTarget->UpdateResource();
}

void UThalassaConsoleScreenComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Redraw only while someone can see the screen.
	RedrawTimer -= DeltaTime;
	if (RenderTarget && RedrawTimer <= 0.f && WasRecentlyRendered(0.5f))
	{
		RedrawTimer = ThalassaConsoleScreen::RedrawSeconds;
		RenderTarget->UpdateResource();
	}
}

void UThalassaConsoleScreenComponent::DrawScreen(UCanvas* Canvas, int32 Width, int32 Height)
{
	THALASSA_PERF_SCOPE("screen draw");
	if (const AThalassaVessel* Vessel = GetVessel())
	{
		DrawContent(Canvas, static_cast<float>(FMath::Min(Width, Height)), *Vessel);
	}
}
