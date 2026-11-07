#include "Vessel/ThalassaHelmDisplay.h"

#include "Engine/Canvas.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Vessel/ThalassaHelmComponent.h"
#include "Vessel/ThalassaVessel.h"

namespace ThalassaHelmDisplay
{
	using namespace ThalassaScreenDraw;

	/** A bar that fills from its middle toward the value: up or right is positive. */
	void Gauge(UCanvas* Canvas, const FVector2D& TopLeft, const FVector2D& Size, float Value, const FLinearColor& Color, float Thin)
	{
		const bool bVertical = Size.Y > Size.X;
		const FVector2D Middle = TopLeft + Size * 0.5f;
		const float Clamped = FMath::Clamp(Value, -1.f, 1.f);

		if (bVertical)
		{
			const float Length = Clamped * Size.Y * 0.5f;
			const float Top = Length >= 0.f ? Middle.Y - Length : Middle.Y;
			Fill(Canvas, FVector2D(TopLeft.X, Top), FVector2D(Size.X, FMath::Abs(Length)), Dim(Color, 0.8f));
			Line(Canvas, FVector2D(TopLeft.X - Size.X * 0.3f, Middle.Y), FVector2D(TopLeft.X + Size.X * 1.3f, Middle.Y), Dim(Cyan, 0.6f), Thin);
		}
		else
		{
			const float Length = Clamped * Size.X * 0.5f;
			const float Left = Length >= 0.f ? Middle.X : Middle.X + Length;
			Fill(Canvas, FVector2D(Left, TopLeft.Y), FVector2D(FMath::Abs(Length), Size.Y), Dim(Color, 0.8f));
			Line(Canvas, FVector2D(Middle.X, TopLeft.Y - Size.Y * 0.3f), FVector2D(Middle.X, TopLeft.Y + Size.Y * 1.3f), Dim(Cyan, 0.6f), Thin);
		}
		Frame(Canvas, TopLeft, Size, Dim(Cyan, 0.6f), Thin);
	}

	FString Percent(float Value, const TCHAR* Positive, const TCHAR* Negative)
	{
		const int32 Amount = FMath::RoundToInt(FMath::Abs(Value) * 100.f);
		return Amount == 0 ? FString(TEXT("neutral")) : FString::Printf(TEXT("%d%% %s"), Amount, Value > 0.f ? Positive : Negative);
	}

	void Draw(UCanvas* Canvas, const FVector2D& TopLeft, float Size, const AThalassaVessel& Vessel)
	{
		const UThalassaHelmComponent* Helm = Vessel.GetHelm();
		if (!Canvas || !Helm)
		{
			return;
		}

		const FHelmLevers& Levers = Helm->GetLevers();
		const float Thin = FMath::Max(1.f, Size / 400.f);
		const float TextScale = Size / 400.f;
		const float Margin = Size * 0.04f;
		const float LineHeight = 18.f * TextScale;

		Fill(Canvas, TopLeft, FVector2D(Size), FLinearColor(0.01f, 0.02f, 0.03f));
		Frame(Canvas, TopLeft, FVector2D(Size), Dim(Cyan, 0.6f), Thin);

		// Who is steering, and how.
		const APawn* Occupant = Helm->GetOccupant();
		const APlayerState* State = Occupant ? Occupant->GetPlayerState() : nullptr;
		Text(Canvas, FString::Printf(TEXT("HELM  %s"), Helm->GetMode() == EHelmMode::Precise ? TEXT("PRECISE") : TEXT("SET AND GO")),
			TopLeft + FVector2D(Margin), Cyan, TextScale * 1.3f);
		Text(Canvas, Occupant ? FString::Printf(TEXT("at the helm: %s"), State ? *State->GetPlayerName() : TEXT("someone")) : FString(TEXT("nobody at the helm")),
			TopLeft + FVector2D(Margin, Margin + LineHeight * 1.5f), Occupant ? Amber : Dim(Cyan, 0.7f), TextScale);

		// Throttle on the left, planes on the right, rudder along the bottom.
		const FVector2D BarSize(Size * 0.08f, Size * 0.5f);
		const float BarTop = TopLeft.Y + Size * 0.25f;
		const FVector2D ThrottleAt(TopLeft.X + Size * 0.08f, BarTop);
		const FVector2D PlanesAt(TopLeft.X + Size * 0.84f, BarTop);
		Gauge(Canvas, ThrottleAt, BarSize, Levers.Throttle, Amber, Thin);
		Gauge(Canvas, PlanesAt, BarSize, Levers.Planes, Cyan, Thin);
		Text(Canvas, TEXT("THROTTLE"), FVector2D(ThrottleAt.X - Size * 0.03f, BarTop + BarSize.Y + Margin * 0.5f), Cyan, TextScale * 0.8f);
		Text(Canvas, TEXT("PLANES"), FVector2D(PlanesAt.X - Size * 0.02f, BarTop + BarSize.Y + Margin * 0.5f), Cyan, TextScale * 0.8f);

		const FVector2D RudderSize(Size * 0.5f, Size * 0.05f);
		const FVector2D RudderAt(TopLeft.X + Size * 0.25f, TopLeft.Y + Size * 0.86f);
		Gauge(Canvas, RudderAt, RudderSize, Levers.Rudder, Amber, Thin);
		Text(Canvas, TEXT("RUDDER"), FVector2D(RudderAt.X, RudderAt.Y - LineHeight * 1.1f), Cyan, TextScale * 0.8f);

		// The ship's answer, in the middle.
		const float Speed = Vessel.GetForwardSpeedMetersPerSecond();
		const TArray<FString> Lines =
		{
			FString::Printf(TEXT("THROTTLE  %s"), *Percent(Levers.Throttle, TEXT("ahead"), TEXT("astern"))),
			FString::Printf(TEXT("RUDDER    %s"), *Percent(Levers.Rudder, TEXT("starboard"), TEXT("port"))),
			FString::Printf(TEXT("PLANES    %s"), *Percent(Levers.Planes, TEXT("rise"), TEXT("dive"))),
			TEXT(""),
			FString::Printf(TEXT("SPEED     %.1f m/s  %.0f kn"), Speed, Speed * 1.944f),
			FString::Printf(TEXT("HEADING   %03.0f"), FRotator::ClampAxis(Vessel.GetActorRotation().Yaw)),
			FString::Printf(TEXT("DEPTH     %.0f m"), Vessel.GetDepthMeters()),
			FString::Printf(TEXT("PITCH     %+.1f"), Vessel.GetActorRotation().Pitch),
		};

		// Warnings, under the title, where the helmsman looks first.
		const float SinceCollision = Vessel.GetSecondsSinceCollision();
		const bool bCollisionWarning = SinceCollision >= 0.f && SinceCollision < 4.f;
		const FString Warning = bCollisionWarning ? FString(TEXT("COLLISION")) : Helm->IsFullStop() ? FString(TEXT("FULL STOP"))
			: Vessel.IsSurfaced() ? FString(TEXT("SURFACED")) : FString();
		if (!Warning.IsEmpty())
		{
			// A collision flashes; surfacing is steady.
			const bool bLit = !bCollisionWarning || FMath::Fmod(SinceCollision, 0.5f) < 0.3f;
			Text(Canvas, Warning, TopLeft + FVector2D(Size * 0.6f, Margin), bLit ? (bCollisionWarning ? FLinearColor(1.f, 0.3f, 0.2f) : Amber) : Dim(Amber, 0.3f), TextScale * 1.3f);
		}
		float Y = BarTop;
		for (const FString& Line : Lines)
		{
			Text(Canvas, Line, FVector2D(TopLeft.X + Size * 0.24f, Y), Cyan, TextScale);
			Y += LineHeight;
		}
	}
}

void UThalassaHelmScreenComponent::DrawContent(UCanvas* Canvas, float Size, const AThalassaVessel& Vessel)
{
	ThalassaHelmDisplay::Draw(Canvas, FVector2D::ZeroVector, Size, Vessel);
}
