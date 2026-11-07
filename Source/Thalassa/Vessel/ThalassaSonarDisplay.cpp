#include "Vessel/ThalassaSonarDisplay.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "TextureResource.h"
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

	using namespace ThalassaScreenDraw;

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

void UThalassaSonarScreenComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AThalassaVessel* Vessel = GetVessel();
	if (!Vessel || GetNetMode() == NM_DedicatedServer)
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
}

void UThalassaSonarScreenComponent::DrawContent(UCanvas* Canvas, float Size, const AThalassaVessel& Vessel)
{
	ThalassaSonarDisplay::Draw(Canvas, FVector2D::ZeroVector, Size, Vessel);
}
