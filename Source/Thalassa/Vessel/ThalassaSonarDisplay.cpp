#include "Vessel/ThalassaSonarDisplay.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/CanvasRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "TextureResource.h"
#include "UObject/ConstructorHelpers.h"
#include "Vessel/ThalassaSonarComponent.h"
#include "Vessel/ThalassaSonarPingComponent.h"
#include "Vessel/ThalassaVessel.h"

namespace ThalassaSonarDisplay
{
	// The full chart is 6.4 km across; show the 1.6 km around the ship, so cells are big enough to read.
	constexpr float ViewFraction = 0.25f;
	constexpr float ViewMeters = ViewFraction * UThalassaSonarComponent::GridSize * UThalassaSonarComponent::CellSizeCm / 100.f;

	// The ping's ring spreads at the speed of sound in sea water, so it crosses the view in about half a second.
	constexpr float SoundSpeedMS = 1500.f;

	const FLinearColor Cyan(0.35f, 0.84f, 0.83f);
	const FLinearColor Amber(0.91f, 0.73f, 0.45f);

	/** Canvas lines ignore alpha, so faint lines are drawn in a darker colour instead. */
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

	/** A circle, leaving out any segment that would stray outside the chart's square. */
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

	float GetPingAge(const AThalassaVessel& Vessel)
	{
		const UThalassaSonarComponent* Sonar = Vessel.GetSonar();
		if (!Sonar || !Sonar->IsSurveying())
		{
			return -1.f;
		}
		return static_cast<float>(FMath::Fmod(Vessel.GetSyncedTime(), static_cast<double>(PingIntervalSeconds)));
	}

	int64 GetPingNumber(const AThalassaVessel& Vessel)
	{
		return static_cast<int64>(FMath::FloorToDouble(Vessel.GetSyncedTime() / PingIntervalSeconds));
	}

	void Draw(UCanvas* Canvas, const FVector2D& TopLeft, float Size, const AThalassaVessel& Vessel)
	{
		UThalassaSonarComponent* Sonar = Vessel.GetSonar();
		UTexture2D* Chart = Sonar ? Sonar->GetChartTexture() : nullptr;
		if (!Canvas || !Chart || !Chart->GetResource())
		{
			return;
		}

		const FVector2D Ship = Sonar->WorldToChart(Vessel.GetActorLocation());
		const FVector2D Corner = Ship - FVector2D(ViewFraction * 0.5f);

		// The texture already has world +X up and world +Y to the right, so this is a plain crop.
		const FVector2D UV0(Corner.Y, 1.f - (Corner.X + ViewFraction));
		FCanvasTileItem Tile(TopLeft, Chart->GetResource(), FVector2D(Size), UV0, UV0 + FVector2D(ViewFraction), FLinearColor::White);
		Tile.BlendMode = SE_BLEND_Opaque;
		Canvas->DrawItem(Tile);

		const FBox2D Clip(TopLeft, TopLeft + FVector2D(Size));
		const FVector2D Middle = TopLeft + FVector2D(Size * 0.5f);
		const float PixelsPerMeter = Size / ViewMeters;
		const float Thin = FMath::Max(1.f, Size / 400.f);

		// Range rings every 200 m.
		for (float RangeM = 200.f; RangeM < ViewMeters * 0.5f + 1.f; RangeM += 200.f)
		{
			Ring(Canvas, Middle, RangeM * PixelsPerMeter, Clip, Dim(Cyan, 0.3f), Thin);
		}

		// The latest ping, spreading out from the ship and fading as it goes.
		const float PingAge = GetPingAge(Vessel);
		const float CornerDistanceM = ViewMeters * UE_INV_SQRT_2;
		if (PingAge >= 0.f && PingAge * SoundSpeedMS < CornerDistanceM)
		{
			const float RadiusM = PingAge * SoundSpeedMS;
			const float Fade = 1.f - RadiusM / CornerDistanceM;
			Ring(Canvas, Middle, RadiusM * PixelsPerMeter, Clip, Dim(FLinearColor(0.6f, 1.f, 0.95f), Fade), Thin * 2.f);
		}

		// Border.
		const FVector2D TR = TopLeft + FVector2D(Size, 0.f), BL = TopLeft + FVector2D(0.f, Size), BR = TopLeft + FVector2D(Size);
		const FLinearColor Border = Dim(Cyan, 0.6f);
		Line(Canvas, TopLeft, TR, Border, Thin);
		Line(Canvas, TR, BR, Border, Thin);
		Line(Canvas, BR, BL, Border, Thin);
		Line(Canvas, BL, TopLeft, Border, Thin);

		// The ship sits in the middle, with a line showing its heading.
		const FVector Heading = Vessel.GetActorForwardVector();
		const float Marker = Size * 0.04f;
		Line(Canvas, Middle, Middle + FVector2D(Heading.Y, -Heading.X) * Marker * 2.f, Amber, Thin * 2.f);
		FCanvasTileItem Dot(Middle - FVector2D(Marker * 0.25f), FVector2D(Marker * 0.5f), Amber);
		Canvas->DrawItem(Dot);

		const float TextScale = Size / 400.f;
		const float Margin = Size * 0.025f;
		Text(Canvas, Sonar->IsSurveying() ? TEXT("SONAR  ACTIVE") : TEXT("SONAR  OFF  (N)"), TopLeft + FVector2D(Margin), Cyan, TextScale * 1.3f);
		Text(Canvas, FString::Printf(TEXT("%d cells surveyed     %.1f km     N up"), Sonar->GetSurveyedCellCount(), ViewMeters / 1000.f),
			BL + FVector2D(Margin, -Margin - 16.f * TextScale), Cyan, TextScale);
	}
}

UThalassaSonarScreenComponent::UThalassaSonarScreenComponent()
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

void UThalassaSonarScreenComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}

	// Tests run without a renderer; they still get the ping, but there is nothing to draw on.
	if (!FApp::CanEverRender())
	{
		return;
	}

	constexpr int32 Resolution = 512;
	constexpr float ScreenBrightness = 0.12f;
	RenderTarget = UCanvasRenderTarget2D::CreateCanvasRenderTarget2D(this, UCanvasRenderTarget2D::StaticClass(), Resolution, Resolution);
	if (!RenderTarget)
	{
		return;
	}
	RenderTarget->ClearColor = FLinearColor(0.01f, 0.02f, 0.03f);
	RenderTarget->OnCanvasRenderTargetUpdate.AddDynamic(this, &UThalassaSonarScreenComponent::DrawScreen);

	ScreenMaterial = CreateAndSetMaterialInstanceDynamic(0);
	if (ScreenMaterial)
	{
		ScreenMaterial->SetTextureParameterValue(TEXT("SlateUI"), RenderTarget);
		// The material is unlit, and the dimly lit bridge makes the camera's exposure rise, so a screen at
		// full strength glows white. This keeps it a lit display rather than a lamp.
		ScreenMaterial->SetVectorParameterValue(TEXT("TintColorAndOpacity"), FLinearColor(ScreenBrightness, ScreenBrightness, ScreenBrightness, 1.f));
	}
	RenderTarget->UpdateResource();
}

void UThalassaSonarScreenComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AThalassaVessel* Vessel = Cast<AThalassaVessel>(GetOwner());
	if (!Vessel)
	{
		return;
	}

	// Every machine pings on the same beat of the server's clock, so the crew hear one ping, not one each.
	const int64 PingNumber = ThalassaSonarDisplay::GetPingNumber(*Vessel);
	if (PingNumber != LastPingNumber)
	{
		LastPingNumber = PingNumber;
		const float PingAge = ThalassaSonarDisplay::GetPingAge(*Vessel);
		if (PingSound && PingAge >= 0.f && PingAge < 0.25f)
		{
			PingSound->Ping();
		}
	}

	// Redraw only while someone can see the screen. 30 times a second keeps the ping's ring smooth.
	RedrawTimer -= DeltaTime;
	if (RenderTarget && RedrawTimer <= 0.f && WasRecentlyRendered(0.5f))
	{
		RedrawTimer = 1.f / 30.f;
		RenderTarget->UpdateResource();
	}
}

void UThalassaSonarScreenComponent::DrawScreen(UCanvas* Canvas, int32 Width, int32 Height)
{
	if (const AThalassaVessel* Vessel = Cast<AThalassaVessel>(GetOwner()))
	{
		ThalassaSonarDisplay::Draw(Canvas, FVector2D::ZeroVector, static_cast<float>(FMath::Min(Width, Height)), *Vessel);
	}
}
