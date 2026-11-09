#include "Tools/ThalassaHUD.h"
#include "Tools/ThalassaPerf.h"

#include "Characters/ThalassaCharacter.h"
#include "Characters/ThalassaMovementComponent.h"
#include "Core/ThalassaPlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformMisc.h"
#include "Tools/ThalassaTestConfig.h"
#include "UnrealClient.h"
#include "Vessel/ThalassaHelmDisplay.h"
#include "Vessel/ThalassaCameraStation.h"
#include "Vessel/ThalassaSonarDisplay.h"
#include "Vessel/ThalassaShipDamage.h"
#include "Vessel/ThalassaVessel.h"

void AThalassaHUD::DrawLines(const TArray<FString>& Lines, float X, float Y, const FLinearColor& Color, float Scale)
{
	const float LineHeight = 16.f * Scale;
	for (const FString& Line : Lines)
	{
		DrawText(Line, Color, X, Y, GEngine->GetSmallFont(), Scale);
		Y += LineHeight;
	}
}

void AThalassaHUD::TickScreenshotRequest()
{
	FString Path;
	if (!ThalassaTestConfig::GetScreenshotPath(Path))
	{
		return;
	}

	// Long enough for shaders and streaming to settle, then long enough for the file to be written.
	constexpr float SettleSeconds = 15.f;
	constexpr float WriteSeconds = 3.f;

	ScreenshotClock += GetWorld()->GetDeltaSeconds();

	// Face the requested way ahead of the capture: a rotation set now only reaches the camera next frame.
	if (APlayerController* Controller = GetOwningPlayerController())
	{
		Controller->SetControlRotation(FRotator(ThalassaTestConfig::LookPitch(), ThalassaTestConfig::LookYaw(), 0.f));
	}

	if (!bScreenshotRequested && ScreenshotClock > SettleSeconds)
	{
		bScreenshotRequested = true;
		FScreenshotRequest::RequestScreenshot(Path, true, false);
	}
	else if (bScreenshotRequested && ScreenshotClock > SettleSeconds + WriteSeconds)
	{
		FPlatformMisc::RequestExit(false, TEXT("ThalassaScreenshot"));
	}
}

void AThalassaHUD::DrawSonarChart(const AThalassaVessel* Vessel)
{
	// The same picture as the screen on the bridge.
	const float Size = FMath::Min(Canvas->ClipX, Canvas->ClipY) * 0.6f;
	ThalassaSonarDisplay::Draw(Canvas, FVector2D(Canvas->ClipX - Size - 40.f, (Canvas->ClipY - Size) * 0.5f), Size, *Vessel);
}

void AThalassaHUD::DrawHUD()
{
	Super::DrawHUD();
	TickScreenshotRequest();
	ThalassaPerf::Frame(GetWorld()->GetDeltaSeconds());

	const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld());
	const AThalassaPlayerController* Controller = Cast<AThalassaPlayerController>(GetOwningPlayerController());
	const AThalassaCharacter* Crew = Controller ? Cast<AThalassaCharacter>(Controller->GetPawn()) : nullptr;
	if (!Vessel || !Crew)
	{
		return;
	}

	const UThalassaMovementComponent* Movement = Crew->GetThalassaMovement();
	const FVector ShipLocation = Vessel->GetActorLocation();
	const FVector Local = Vessel->GetActorTransform().InverseTransformPosition(Crew->GetActorLocation());
	const FRotator ShipRotation = Vessel->GetActorRotation();
	const APlayerState* State = Crew->GetPlayerState();
	const UObject* Base = Crew->GetMovementBaseObject();

	const bool bSwimming = Movement->IsSwimmingInSea();
	if (bSwimming)
	{
		// A cue that you are under water, until there is real underwater rendering.
		DrawRect(FLinearColor(0.f, 0.22f, 0.32f, 0.3f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	}

	static const TCHAR* const AirlockNames[] = { TEXT("inner door open"), TEXT("flooding"), TEXT("outer door open"), TEXT("draining") };

	const TArray<FString> Diagnostics =
	{
		FString::Printf(TEXT("%.0f fps   ping %.0f ms   net %s"), 1.f / FMath::Max(GetWorld()->GetDeltaSeconds(), 0.001f),
			State ? State->GetPingInMilliseconds() : 0.f,
			GetNetMode() == NM_Client ? TEXT("client") : GetNetMode() == NM_ListenServer ? TEXT("listen host") : TEXT("standalone")),
		FString::Printf(TEXT("ship  pos %.0f %.0f %.0f m   spd %.1f m/s"), ShipLocation.X / 100.0, ShipLocation.Y / 100.0, ShipLocation.Z / 100.0, Vessel->GetSpeedMetersPerSecond()),
		FString::Printf(TEXT("ship  y %.1f  p %.1f  r %.1f"), ShipRotation.Yaw, ShipRotation.Pitch, ShipRotation.Roll),
		FString::Printf(TEXT("crew  ship-frame %.0f %.0f %.0f cm"), Local.X, Local.Y, Local.Z),
		FString::Printf(TEXT("base  %s   mode %d"), Base ? *Base->GetName() : TEXT("none"), static_cast<int32>(Movement->MovementMode.GetValue())),
		FString::Printf(TEXT("corrections %d  max %.1f cm  last %.1f cm"), Movement->CorrectionCount, Movement->MaxCorrectionCm, Movement->LastCorrectionCm),
		FString::Printf(TEXT("%s   airlock %s, water %.0f%%%s"), bSwimming ? TEXT("SWIMMING") : Movement->IsClimbingLadder() ? TEXT("ON LADDER") : TEXT("on foot"),
			AirlockNames[static_cast<int32>(Vessel->GetAirlockState())], Vessel->GetAirlockWaterLevel() * 100.f,
			Vessel->IsAirlockDoorwayBlocked() ? TEXT("   DOORWAY BLOCKED") : TEXT("")),
		TEXT("WASD/mouse move   SHIFT sprint   C crouch (sprinting: slide)   SPACE jump/up   E use helm, console or airlock   TAB station view"),
		TEXT("N sonar   F sonar ping   G test sonar grade   K test damage   P run motion test"),
	};
	DrawLines(Diagnostics, 16.f, 16.f, FLinearColor(0.35f, 0.84f, 0.83f), 1.f);
	DrawDamagePanel(Vessel, Crew);

	if (Controller->IsAtHelm())
	{
		// The same picture as the helm's screen, and what the keys do while you are here.
		const float Size = FMath::Min(Canvas->ClipX, Canvas->ClipY) * 0.42f;
		const FVector2D TopLeft(40.f, Canvas->ClipY - Size - 70.f);
		ThalassaHelmDisplay::Draw(Canvas, TopLeft, Size, *Vessel);
		DrawText(TEXT("AT THE HELM   W/S throttle   A/D rudder   SPACE/C thrusters up/down   Q/Z planes   R precise or set-and-go   X full stop   B test boost x10   E stand up"),
			FLinearColor(0.91f, 0.73f, 0.45f), TopLeft.X, TopLeft.Y + Size + 12.f, GEngine->GetSmallFont(), 1.3f);
	}

	UThalassaCameraStationComponent* Cameras = Vessel->GetCameraStation();
	if (Controller->IsAtCameraStation() && Cameras)
	{
		Cameras->RequestFeeds();
		if (Controller->IsStationViewExpanded())
		{
			// Expanded: the chosen camera fills the screen.
			ThalassaCameraDisplay::DrawFeed(Canvas, FVector2D(0.f, 0.f), FVector2D(Canvas->ClipX, Canvas->ClipY - 40.f), *Cameras, Cameras->GetSelected(), false);
		}
		else
		{
			const FVector2D Size(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
			ThalassaCameraDisplay::DrawGrid(Canvas, FVector2D(Canvas->ClipX - Size.X - 30.f, Canvas->ClipY - Size.Y - 60.f), Size, *Cameras);
		}
		DrawText(TEXT("CAMERA STATION   A/D choose camera   TAB full screen   E stand up"),
			FLinearColor(0.91f, 0.73f, 0.45f), 40.f, Canvas->ClipY - 32.f, GEngine->GetSmallFont(), 1.3f);
		return;
	}

	if (Controller->IsStationViewExpanded())
	{
		const TArray<FString> Station =
		{
			TEXT("HELM / STATUS (expanded view)"),
			FString::Printf(TEXT("BEEP COUNT   %d"), Vessel->GetBeepCount()),
			FString::Printf(TEXT("DEPTH        %.0f m"), Vessel->GetDepthMeters()),
			FString::Printf(TEXT("SPEED        %.1f m/s"), Vessel->GetSpeedMetersPerSecond()),
			FString::Printf(TEXT("HEADING      %03.0f"), FRotator::ClampAxis(ShipRotation.Yaw)),
		};
		DrawLines(Station, 40.f, Canvas->ClipY * 0.5f - 90.f, FLinearColor(0.91f, 0.73f, 0.45f), 2.f);
		DrawSonarChart(Vessel);
	}
}

void AThalassaHUD::DrawDamagePanel(const AThalassaVessel* Vessel, const APawn* Crew)
{
	const UThalassaShipDamageComponent* Damage = Vessel ? Vessel->GetDamage() : nullptr;
	if (!Damage)
	{
		return;
	}

	// What's broken and what's flooding: shown while anything is, top right.
	TArray<FString> Lines;
	for (int32 Index = 0; Index < static_cast<int32>(EShipSystem::Count); ++Index)
	{
		const EShipSystem System = static_cast<EShipSystem>(Index);
		const float Health = Damage->GetHealth(System);
		if (Health < 1.f)
		{
			const int32 Bars = FMath::RoundToInt(Health * 10.f);
			Lines.Add(FString::Printf(TEXT("%-9s %s%s %3.0f%%%s"), UThalassaShipDamageComponent::SystemName(System), *FString::ChrN(Bars, TEXT('|')),
				*FString::ChrN(10 - Bars, TEXT('.')), Health * 100.f, System == EShipSystem::Sonar && !Damage->IsSonarWorking() ? TEXT("  OUT") : TEXT("")));
		}
	}
	for (const FShipBreach& Breach : Damage->GetBreaches())
	{
		Lines.Add(FString::Printf(TEXT("BREACH    %s, %.0f%%"), *Damage->CompartmentName(Breach.Compartment), Breach.Severity * 100.f));
	}
	for (int32 Index = 0; Index < Vessel->GetCompartmentCount(); ++Index)
	{
		const float Level = Damage->GetFloodLevel(Index);
		if (Level > 0.005f)
		{
			Lines.Add(FString::Printf(TEXT("FLOODING  %s, %.0f%%"), *Damage->CompartmentName(Index), Level * 100.f));
		}
	}
	if (Lines.Num() > 0)
	{
		Lines.Insert(TEXT("DAMAGE CONTROL"), 0);
		DrawLines(Lines, Canvas->ClipX - 430.f, 16.f, FLinearColor(1.f, 0.45f, 0.3f), 1.2f);
	}

	// What you're repairing, or could repair from here.
	FString Name;
	float Done = 0.f;
	const int32 Working = Damage->GetRepairTarget(Crew);
	if (Working >= 0 && Damage->DescribeTarget(Working, Name, Done))
	{
		DrawText(FString::Printf(TEXT("REPAIRING %s   %.0f%%   (stay close; E to stop)"), *Name, Done * 100.f), FLinearColor(1.f, 0.85f, 0.4f),
			Canvas->ClipX * 0.5f - 220.f, Canvas->ClipY * 0.62f, GEngine->GetSmallFont(), 1.5f);
		return;
	}
	const int32 Nearby = Damage->FindRepairTarget(Crew);
	if (Nearby >= 0 && Damage->DescribeTarget(Nearby, Name, Done))
	{
		DrawText(FString::Printf(TEXT("E  repair %s   (%.0f%%)"), *Name, Done * 100.f), FLinearColor(1.f, 0.85f, 0.4f),
			Canvas->ClipX * 0.5f - 160.f, Canvas->ClipY * 0.62f, GEngine->GetSmallFont(), 1.5f);
	}
}
