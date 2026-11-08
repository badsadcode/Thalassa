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

	/** A filled triangle, added on top of what is there: light, not paint. */
	void Glow(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Colour)
	{
		FCanvasTriangleItem Item(A, B, C, GWhiteTexture);
		Item.SetColor(Colour);
		Item.BlendMode = SE_BLEND_Additive;
		Canvas->DrawItem(Item);
	}

	/** A see-through rectangle. */
	void Shade(UCanvas* Canvas, const FVector2D& TopLeft, const FVector2D& Size, const FLinearColor& Colour)
	{
		FCanvasTileItem Item(TopLeft, Size, Colour);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}

	/** A point on a circle, with 0 degrees straight up (north) and angles running clockwise. */
	FVector2D OnCircle(const FVector2D& Centre, float Radius, float Degrees)
	{
		const float Radians = FMath::DegreesToRadians(Degrees);
		return Centre + FVector2D(FMath::Sin(Radians), -FMath::Cos(Radians)) * Radius;
	}

	void Draw(UCanvas* Canvas, const FVector2D& TopLeft, float Size, const AThalassaVessel& Vessel)
	{
		UThalassaSonarComponent* Sonar = Vessel.GetSonar();
		if (!Canvas || !Sonar)
		{
			return;
		}

		const FBox2D Clip(TopLeft, TopLeft + FVector2D(Size));
		const FVector2D Middle = TopLeft + FVector2D(Size * 0.5f);
		const float PixelsPerMeter = Size / ViewMeters;
		const float Thin = FMath::Max(1.f, Size / 400.f);
		const float TextScale = Size / 400.f;
		const float Margin = Size * 0.025f;
		const float Radius = Size * 0.5f;

		// The chart: built around the ship, cropped so the ship sits in the middle.
		Fill(Canvas, TopLeft, FVector2D(Size), FLinearColor(0.004f, 0.012f, 0.02f));
		FVector2D UV0;
		float UVSize = 0.f;
		const int32 ViewCells = FMath::RoundToInt(ViewMeters * 100.f / UThalassaSonarComponent::CellSizeCm);
		UTexture2D* Chart = Sonar->GetLocalChart(Vessel.GetActorLocation(), ViewCells, UV0, UVSize);
		if (Chart && Chart->GetResource())
		{
			FCanvasTileItem Tile(TopLeft, Chart->GetResource(), FVector2D(Size), UV0, UV0 + FVector2D(UVSize), FLinearColor::White);
			Tile.BlendMode = SE_BLEND_Opaque;
			Canvas->DrawItem(Tile);
		}

		// Range rings every 200 m, labelled to the north-east.
		for (float RangeM = 200.f; RangeM < ViewMeters * 0.5f + 1.f; RangeM += 200.f)
		{
			Ring(Canvas, Middle, RangeM * PixelsPerMeter, Clip, Dim(Cyan, 0.28f), Thin);
			Text(Canvas, FString::Printf(TEXT("%.0f"), RangeM), OnCircle(Middle, RangeM * PixelsPerMeter, 45.f) + FVector2D(3.f, -6.f) * TextScale, Dim(Cyan, 0.6f), TextScale * 0.75f);
		}

		const float PingAge = GetPingAge(Vessel);
		if (PingAge >= 0.f)
		{
			// The sweep: one turn per ping, with a fading trail, in time with every other screen aboard.
			const float Angle = PingAge / PingIntervalSeconds * 360.f;
			constexpr int32 TrailSteps = 24;
			constexpr float TrailDegrees = 70.f;
			for (int32 Step = 0; Step < TrailSteps; ++Step)
			{
				const float A0 = Angle - TrailDegrees * (Step + 1) / TrailSteps;
				const float A1 = Angle - TrailDegrees * Step / TrailSteps;
				const float Strength = FMath::Square(1.f - float(Step) / TrailSteps) * 0.16f;
				Glow(Canvas, Middle, OnCircle(Middle, Radius, A0), OnCircle(Middle, Radius, A1), FLinearColor(0.1f, 0.55f, 0.5f) * Strength);
			}
			Line(Canvas, Middle, OnCircle(Middle, Radius, Angle), FLinearColor(0.45f, 1.f, 0.9f), Thin * 1.5f);

			// The ping itself, spreading at the speed of sound and fading as it goes.
			const float CornerDistanceM = ViewMeters * UE_INV_SQRT_2;
			const float RadiusM = PingAge * SoundSpeedMS;
			if (RadiusM < CornerDistanceM)
			{
				Ring(Canvas, Middle, RadiusM * PixelsPerMeter, Clip, Dim(FLinearColor(0.6f, 1.f, 0.95f), 1.f - RadiusM / CornerDistanceM), Thin * 2.f);
			}
		}

		// The compass: a tick every 10 degrees around the edge, longer every 30, and the four points.
		for (int32 Degrees = 0; Degrees < 360; Degrees += 10)
		{
			const float Length = Size * (Degrees % 30 == 0 ? 0.03f : 0.014f);
			Line(Canvas, OnCircle(Middle, Radius - 2.f, Degrees), OnCircle(Middle, Radius - 2.f - Length, Degrees), Dim(Cyan, Degrees % 90 == 0 ? 0.9f : 0.45f), Thin);
		}
		const TCHAR* Points[] = { TEXT("N"), TEXT("E"), TEXT("S"), TEXT("W") };
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Text(Canvas, Points[Index], OnCircle(Middle, Radius - Size * 0.065f, Index * 90.f) - FVector2D(4.f, 7.f) * TextScale, Cyan, TextScale);
		}

		// The ship: a chevron pointing along its heading, and the heading run out to the edge.
		const FVector Forward = Vessel.GetActorForwardVector();
		const float Heading = FMath::RadiansToDegrees(FMath::Atan2(Forward.Y, Forward.X));
		for (float Dash = Size * 0.05f; Dash < Radius - Size * 0.04f; Dash += Size * 0.03f)
		{
			Line(Canvas, OnCircle(Middle, Dash, Heading), OnCircle(Middle, Dash + Size * 0.015f, Heading), Dim(Amber, 0.7f), Thin);
		}
		const float Chevron = Size * 0.03f;
		Glow(Canvas, OnCircle(Middle, Chevron, Heading), OnCircle(Middle, Chevron * 0.8f, Heading + 140.f), Middle, Amber);
		Glow(Canvas, OnCircle(Middle, Chevron, Heading), Middle, OnCircle(Middle, Chevron * 0.8f, Heading - 140.f), Amber);

		// Readouts, top left.
		const float Depth = Vessel.GetDepthMeters();
		float SeabedM = 0.f;
		const bool bKnowSeabed = Sonar->GetSurveyedHeightM(Vessel.GetActorLocation(), SeabedM);
		const float LineHeight = 17.f * TextScale;
		Shade(Canvas, TopLeft + FVector2D(Margin * 0.5f), FVector2D(Size * 0.36f, LineHeight * 5.4f), FLinearColor(0.f, 0.02f, 0.03f, 0.65f));
		FVector2D At = TopLeft + FVector2D(Margin);
		Text(Canvas, Sonar->IsSurveying() ? TEXT("SONAR  ACTIVE") : TEXT("SONAR  OFF  (N)"), At, Sonar->IsSurveying() ? FLinearColor(0.5f, 1.f, 0.9f) : Amber, TextScale * 1.15f);
		At.Y += LineHeight * 1.3f;
		Text(Canvas, FString::Printf(TEXT("DEPTH   %4.0f m"), Depth), At, Cyan, TextScale * 0.9f);
		At.Y += LineHeight;
		Text(Canvas, bKnowSeabed ? FString::Printf(TEXT("KEEL    %4.0f m"), -SeabedM - Depth) : FString(TEXT("KEEL       --")), At, Cyan, TextScale * 0.9f);
		At.Y += LineHeight;
		Text(Canvas, FString::Printf(TEXT("HDG     %03.0f    SPD %4.1f m/s"), FMath::Fmod(Heading + 360.f, 360.f), Vessel.GetForwardSpeedMetersPerSecond()), At, Cyan, TextScale * 0.9f);

		// The depth legend, bottom right: what the colours mean.
		const FVector2D LegendSize(Size * 0.3f, Size * 0.022f);
		const FVector2D LegendAt = TopLeft + FVector2D(Size - Margin - LegendSize.X, Size - Margin - LegendSize.Y);
		Shade(Canvas, LegendAt - FVector2D(Margin * 0.5f, LineHeight * 1.2f), LegendSize + FVector2D(Margin, LineHeight * 1.2f + Margin * 0.5f), FLinearColor(0.f, 0.02f, 0.03f, 0.65f));
		constexpr int32 Swatches = 24;
		for (int32 Index = 0; Index < Swatches; ++Index)
		{
			const float HeightM = FMath::Lerp(Sonar->GetChartDeepM(), Sonar->GetChartShallowM(), (Index + 0.5f) / Swatches);
			Fill(Canvas, LegendAt + FVector2D(LegendSize.X * Index / Swatches, 0.f), FVector2D(LegendSize.X / Swatches + 1.f, LegendSize.Y),
				UThalassaSonarComponent::DepthColour(HeightM, Sonar->GetChartDeepM(), Sonar->GetChartShallowM()));
		}
		Text(Canvas, FString::Printf(TEXT("%.0f m"), -Sonar->GetChartDeepM()), LegendAt - FVector2D(0.f, LineHeight), Dim(Cyan, 0.8f), TextScale * 0.75f);
		Text(Canvas, FString::Printf(TEXT("%.0f m"), -Sonar->GetChartShallowM()), LegendAt + FVector2D(LegendSize.X - 34.f * TextScale, -LineHeight), Dim(Cyan, 0.8f), TextScale * 0.75f);

		Text(Canvas, FString::Printf(TEXT("%d cells   %.1f km   N up"), Sonar->GetSurveyedCellCount(), ViewMeters / 1000.f),
			TopLeft + FVector2D(Margin, Size - Margin - LineHeight), Dim(Cyan, 0.7f), TextScale * 0.75f);

		// Scanlines and a frame: a screen, not a printout.
		for (float Y = 0.f; Y < Size; Y += FMath::Max(3.f, Size / 170.f))
		{
			Shade(Canvas, TopLeft + FVector2D(0.f, Y), FVector2D(Size, 1.f), FLinearColor(0.f, 0.f, 0.f, 0.18f));
		}
		Frame(Canvas, TopLeft, FVector2D(Size), Dim(Cyan, 0.6f), Thin);
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
