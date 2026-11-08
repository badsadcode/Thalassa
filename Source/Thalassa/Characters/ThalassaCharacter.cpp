#include "Characters/ThalassaCharacter.h"

#include "Camera/CameraComponent.h"
#include "Characters/ThalassaMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Tools/ThalassaTestConfig.h"
#include "UObject/ConstructorHelpers.h"
#include "Core/ThalassaPlayerController.h"
#include "Vessel/ThalassaHelmComponent.h"
#include "Vessel/ThalassaStationComponent.h"
#include "Vessel/ThalassaCameraStation.h"
#include "Vessel/ThalassaVessel.h"

namespace ThalassaCrew
{
	constexpr float CameraHeightCm = 64.f;      // Above the capsule's centre.
	constexpr float CameraCrouchEaseSpeed = 12.f;
}

AThalassaCharacter::AThalassaCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UThalassaMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);

	// Crew do not block each other. Each client sees the others slightly in the past, so hard
	// collisions between players disagree and force constant corrections. See ApplyCrewNudge.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	bUseControllerRotationYaw = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, ThalassaCrew::CameraHeightCm));
	Camera->bUsePawnControlRotation = true;

	// Placeholder body seen by other players. Parented to the skeletal mesh component so it
	// inherits CharacterMovement's network smoothing, as a real character mesh will.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetMesh());
	BodyMesh->SetStaticMesh(Cylinder.Object);
	BodyMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.76f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetOwnerNoSee(true);
}

UThalassaMovementComponent* AThalassaCharacter::GetThalassaMovement() const
{
	return Cast<UThalassaMovementComponent>(GetCharacterMovement());
}

void AThalassaCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Crouching and standing move the capsule at once. The view eases after it instead of snapping.
	if (CameraCrouchOffset != 0.f)
	{
		CameraCrouchOffset = FMath::FInterpTo(CameraCrouchOffset, 0.f, DeltaSeconds, ThalassaCrew::CameraCrouchEaseSpeed);
		if (FMath::Abs(CameraCrouchOffset) < 0.1f)
		{
			CameraCrouchOffset = 0.f;
		}
		Camera->SetRelativeLocation(FVector(0.f, 0.f, ThalassaCrew::CameraHeightCm + CameraCrouchOffset));
	}

	if (!BoundVessel)
	{
		BoundVessel = AThalassaVessel::Find(GetWorld());
		if (BoundVessel)
		{
			// The deck must move before the people standing on it do, or they lag it by a frame.
			GetCharacterMovement()->AddTickPrerequisiteActor(BoundVessel);
		}
	}

	if (IsLocallyControlled() && ThalassaTestConfig::IsWalkStraight() && BoundVessel && GetController()
		&& GetWorld()->GetGameState() && GetWorld()->GetGameState()->PlayerArray.Num() >= ThalassaTestConfig::ExpectedPlayers())
	{
		// Exactly what holding W does: forward along the view's yaw. Logged twice a second.
		const FRotator YawOnly(0.f, GetController()->GetControlRotation().Yaw, 0.f);
		AddMovementInput(YawOnly.RotateVector(FVector::ForwardVector), 1.f);
		BotSecondsOnWaypoint += DeltaSeconds;
		if (BotSecondsOnWaypoint >= 0.5f)
		{
			BotSecondsOnWaypoint = 0.f;
			const FVector Local = BoundVessel->GetActorTransform().InverseTransformPosition(GetActorLocation());
			float Nearest = -1.f;
			for (TActorIterator<AThalassaCharacter> It(GetWorld()); It; ++It)
			{
				if (*It != this)
				{
					const float Gap = static_cast<float>(FVector::Dist(It->GetActorLocation(), GetActorLocation()));
					Nearest = Nearest < 0.f ? Gap : FMath::Min(Nearest, Gap);
				}
			}
			UE_LOG(LogTemp, Display, TEXT("WALKSTRAIGHT local %.1f %.1f %.1f yaw %.3f velocity %s nearest %.0f"), Local.X, Local.Y, Local.Z,
				GetController()->GetControlRotation().Yaw, *GetVelocity().ToString(), Nearest);
		}
	}

	if (IsLocallyControlled())
	{
		if (ThalassaTestConfig::IsHelmBot())
		{
			TickHelmBot(DeltaSeconds);
		}
		else if (ThalassaTestConfig::IsDiverBot())
		{
			TickDiverBot(DeltaSeconds);
		}
		else if (ThalassaTestConfig::IsBot())
		{
			GetThalassaMovement()->SetWantsToSprint(ThalassaTestConfig::IsBotSprinting() || ThalassaTestConfig::IsBotSliding());
			if (ThalassaTestConfig::IsBotSliding())
			{
				// Every few seconds, a slide: crouch while sprinting, for a second and a half.
				BotSlideTimer -= DeltaSeconds;
				if (BotSlideTimer <= 0.f)
				{
					BotSlideTimer = 6.f;
					Crouch();
					BotSlideStart = BoundVessel ? BoundVessel->GetActorTransform().InverseTransformPosition(GetActorLocation()) : FVector::ZeroVector;
					BotSlideStartSpeed = static_cast<float>(GetCharacterMovement()->Velocity.Size2D());
				}
				else if (BotSlideTimer < 4.5f && bIsCrouched)
				{
					const FVector Here = BoundVessel ? BoundVessel->GetActorTransform().InverseTransformPosition(GetActorLocation()) : FVector::ZeroVector;
					UE_LOG(LogTemp, Display, TEXT("SLIDE from %.0f cm/s, after 1.5 s: %.0f cm/s, %s, moved %.0f cm"), BotSlideStartSpeed,
						GetCharacterMovement()->Velocity.Size2D(), GetThalassaMovement()->IsSliding() ? TEXT("still sliding") : TEXT("crouching"),
						FVector::Dist2D(Here, BotSlideStart));
					UnCrouch();
				}
			}
			TickWalkerBot(DeltaSeconds);
		}
		if (IsAtStation())
		{
			HoldStationPosition();
		}
		else if (!ThalassaTestConfig::IsCrewNudgeDisabled())
		{
			ApplyCrewNudge();
		}
	}
}

bool AThalassaCharacter::IsAtStation() const
{
	return BoundVessel && BoundVessel->FindStationOf(this);
}

void AThalassaCharacter::HoldStationPosition()
{
	// Walked there, like any other move, so it is predicted and needs no correction. A real seat and
	// a sitting animation can replace this later.
	const UThalassaStationComponent* Station = BoundVessel->FindStationOf(this);
	if (!Station)
	{
		return;
	}
	const FVector Offset = Station->GetSeatWorldLocation() - GetActorLocation();
	const FVector Flat = FVector::VectorPlaneProject(Offset, BoundVessel->GetActorUpVector());
	const float Distance = static_cast<float>(Flat.Size());
	if (Distance > 8.f)
	{
		AddMovementInput(Flat / Distance, FMath::Clamp(Distance / 60.f, 0.2f, 1.f));
	}
}

void AThalassaCharacter::TickHelmBot(float DeltaSeconds)
{
	if (!BoundVessel || !BoundVessel->GetHelm() || BoundVessel->GetProfileState() != EVesselProfileState::Running)
	{
		return;
	}
	AThalassaPlayerController* Helmsman = Cast<AThalassaPlayerController>(GetController());
	if (!Helmsman)
	{
		return;
	}

	const FTransform Ship = BoundVessel->GetActorTransform();
	const UThalassaHelmComponent* Helm = BoundVessel->GetHelm();

	// The "cameras" course goes to the camera station instead, and steps through the cameras.
	static const bool bCameraCourse = ThalassaTestConfig::HelmCourse() == TEXT("cameras");
	const UThalassaStationComponent* Station = bCameraCourse ? static_cast<const UThalassaStationComponent*>(BoundVessel->GetCameraStation()) : Helm;
	if (!Station)
	{
		return;
	}

	if (BotHelmSeconds < 0.f)
	{
		// Walk forward along the corridors to the bridge, then to the station, starting from the waypoint
		// nearest the spawn: players spawn on the bridge now, so usually it's only the last steps.
		const FVector Route[] =
		{
			FVector(-1000.f, -1500.f, 90.f), FVector(2000.f, -1500.f, 90.f), FVector(2800.f, -1500.f, 90.f), FVector(2800.f, -300.f, 90.f),
			FVector(4500.f, -300.f, 90.f), FVector(9000.f, -300.f, 90.f), FVector(10300.f, -300.f, 90.f), FVector(12000.f, -300.f, 40.f),
			bCameraCourse ? FVector(12400.f, 500.f, 90.f) : FVector(13000.f, 0.f, 90.f), Station->SeatLocal,
		};
		const int32 RouteLength = UE_ARRAY_COUNT(Route);
		const FVector Here = Ship.InverseTransformPosition(GetActorLocation());
		if (!bHelmRouteStarted)
		{
			bHelmRouteStarted = true;
			for (int32 Index = 1; Index < RouteLength; ++Index)
			{
				if (FVector::DistSquared(Route[Index], Here) < FVector::DistSquared(Route[BotWaypoint], Here))
				{
					BotWaypoint = Index;
				}
			}
		}

		if (Station->IsSeated(this))
		{
			BotHelmSeconds = 0.f;
			UE_LOG(LogTemp, Display, TEXT("HELMBOT seated at %s"), *Here.ToString());
			return;
		}

		if (BotWaypoint >= RouteLength)
		{
			BotPressCooldown -= DeltaSeconds;
			if (BotPressCooldown <= 0.f)
			{
				BotPressCooldown = 2.f;
				Interact();
			}
			return;
		}

		const FVector Offset = Route[BotWaypoint] - Here;
		if (FVector2D(Offset.X, Offset.Y).SizeSquared() < FMath::Square(BotWaypoint == RouteLength - 1 ? 40.f : 80.f))
		{
			++BotWaypoint;
			BotSecondsOnWaypoint = 0.f;
			return;
		}
		BotSecondsOnWaypoint += DeltaSeconds;
		if (BotSecondsOnWaypoint > 40.f)
		{
			++BotStuckCount;
			UE_LOG(LogTemp, Display, TEXT("WALKERSTUCK helm route %d at %s"), BotWaypoint, *Here.ToString());
			++BotWaypoint;
			BotSecondsOnWaypoint = 0.f;
		}
		AddMovementInput(Ship.TransformVectorNoScale(FVector(Offset.X, Offset.Y, 0.f).GetSafeNormal()));
		return;
	}

	if (bCameraCourse)
	{
		// Step forward through every camera, one every three seconds, then back once, then stand up.
		BotHelmSeconds += DeltaSeconds;
		const UThalassaCameraStationComponent* Cameras = BoundVessel->GetCameraStation();
		const int32 Steps = FMath::FloorToInt(BotHelmSeconds / 3.f);
		if (Steps > BotHelmCue && BotHelmCue < 8)
		{
			++BotHelmCue;
			Helmsman->RequestCameraStep(BotHelmCue == 8 ? -1 : 1);
		}
		else if (BotHelmCue == 8 && BotHelmSeconds > 30.f && Cameras->IsSeated(this))
		{
			Interact();
		}
		BotHelmLogTimer -= DeltaSeconds;
		if (BotHelmLogTimer <= 0.f)
		{
			BotHelmLogTimer = 1.f;
			UE_LOG(LogTemp, Display, TEXT("CAMERABOT t %.1f seated %d steps sent %d selected %d (%s)"), BotHelmSeconds, Cameras->IsSeated(this) ? 1 : 0,
				BotHelmCue, Cameras->GetSelected(), *Cameras->GetLabel(Cameras->GetSelected()));
		}
		return;
	}

	// At the helm: a scripted course through both modes. Seconds after sitting down.
	enum class EPress : uint8 { None, Mode, FullStop, Stand, Boost };
	struct FCue { float At; float Throttle; float Turn; float Dive; EPress Press; float Heave = 0.f; };
	// Boost: test boost on, full ahead for 40 s, let go, boost off.
	static const FCue BoostCues[] =
	{
		{ 2.f,  0.f, 0.f, 0.f, EPress::Boost },
		{ 3.f,  1.f, 0.f, 0.f, EPress::None },
		{ 43.f, 0.f, 0.f, 0.f, EPress::None },
		{ 70.f, 0.f, 0.f, 0.f, EPress::Boost },
	};
	// Sink: thrusters full down at zero throttle until the seabed stops the ship, then back up.
	static const FCue SinkCues[] =
	{
		{ 2.f,  0.f, 0.f, 0.f, EPress::None, -1.f },
		{ 60.f, 0.f, 0.f, 0.f, EPress::None,  1.f },
		{ 80.f, 0.f, 0.f, 0.f, EPress::None,  0.f },
	};
	// Heave: thrusters down, let go and drift, thrusters up, let go. The ship should stay level throughout.
	static const FCue HeaveCues[] =
	{
		{ 2.f,  0.f, 0.f, 0.f, EPress::None, -1.f },  // Thrusters down...
		{ 27.f, 0.f, 0.f, 0.f, EPress::None,  0.f },  // ...let go: it drifts on and slows...
		{ 45.f, 0.f, 0.f, 0.f, EPress::None,  1.f },  // ...thrusters up...
		{ 75.f, 0.f, 0.f, 0.f, EPress::None,  0.f },  // ...and let go.
	};
	// Dive: full ahead and full dive into the seabed, then all stop and back away from it.
	static const FCue DiveCues[] =
	{
		{ 2.f,   0.f,  0.f,  0.f, EPress::Mode },
		{ 3.f,   1.f,  0.f, -1.f, EPress::None },
		{ 5.5f,  0.f,  0.f,  0.f, EPress::None },
		{ 45.f,  0.f,  0.f,  0.f, EPress::FullStop },
		{ 47.f,  0.f,  0.f,  1.f, EPress::None },    // Planes to rise...
		{ 48.f, -1.f,  0.f,  0.f, EPress::None },    // ...and astern.
		{ 50.5f, 0.f,  0.f,  0.f, EPress::None },
		{ 80.f,  0.f,  0.f,  0.f, EPress::FullStop },
		{ 90.f,  0.f,  0.f,  0.f, EPress::Stand },
	};
	// Rise: full ahead and full rise until the ship surfaces, then hold it there.
	static const FCue RiseCues[] =
	{
		{ 2.f,   0.f,  0.f,  0.f, EPress::Mode },
		{ 3.f,   1.f,  0.f,  1.f, EPress::None },
		{ 5.5f,  0.f,  0.f,  0.f, EPress::None },
		{ 80.f,  0.f,  0.f,  0.f, EPress::FullStop },
		{ 90.f,  0.f,  0.f,  0.f, EPress::Stand },
	};
	static const FCue TourCues[] =
	{
		{ 2.f,   0.f,  0.f,  0.f, EPress::Mode },    // To set-and-go.
		{ 3.f,   1.f,  0.f,  0.f, EPress::None },    // Push the throttle up...
		{ 5.5f,  0.f,  0.f,  0.f, EPress::None },    // ...to full, and leave it.
		{ 25.f,  0.f,  1.f,  0.f, EPress::None },    // Turn to starboard.
		{ 37.f,  0.f,  0.f,  0.f, EPress::None },
		{ 40.f,  0.f,  0.f, -1.f, EPress::None },    // Planes to dive...
		{ 42.f,  0.f,  0.f,  0.f, EPress::None },
		{ 50.f,  0.f,  0.f,  1.f, EPress::None },    // ...and back to level.
		{ 52.f,  0.f,  0.f,  0.f, EPress::None },
		{ 55.f,  0.f, -1.f,  0.f, EPress::None },    // Turn to port.
		{ 65.f,  0.f,  0.f,  0.f, EPress::None },
		{ 70.f,  0.f,  0.f,  0.f, EPress::FullStop },
		{ 78.f,  0.f,  0.f,  0.f, EPress::Mode },    // To precise.
		{ 80.f,  1.f,  1.f,  0.f, EPress::None },    // Hold ahead and starboard...
		{ 95.f,  0.f,  0.f,  0.f, EPress::None },    // ...and let go.
		{ 100.f, 0.f,  0.f,  0.f, EPress::Stand },
	};
	static const FString Course = ThalassaTestConfig::HelmCourse();
	const FCue* Cues = Course == TEXT("dive") ? DiveCues : Course == TEXT("rise") ? RiseCues : Course == TEXT("heave") ? HeaveCues : Course == TEXT("sink") ? SinkCues : Course == TEXT("boost") ? BoostCues : TourCues;
	const int32 CueCount = Course == TEXT("dive") ? UE_ARRAY_COUNT(DiveCues) : Course == TEXT("rise") ? UE_ARRAY_COUNT(RiseCues)
		: Course == TEXT("heave") ? UE_ARRAY_COUNT(HeaveCues) : Course == TEXT("sink") ? UE_ARRAY_COUNT(SinkCues) : Course == TEXT("boost") ? UE_ARRAY_COUNT(BoostCues) : UE_ARRAY_COUNT(TourCues);

	BotHelmSeconds += DeltaSeconds;
	while (BotHelmCue < CueCount && BotHelmSeconds >= Cues[BotHelmCue].At)
	{
		const FCue& Cue = Cues[BotHelmCue++];
		Helmsman->SetHelmInput(Cue.Throttle, Cue.Turn, Cue.Dive, Cue.Heave);
		if (Cue.Press == EPress::Mode)
		{
			Helmsman->RequestHelmMode();
		}
		if (Cue.Press == EPress::Boost)
		{
			Helmsman->RequestHelmBoost();
		}
		else if (Cue.Press == EPress::FullStop)
		{
			Helmsman->RequestHelmFullStop();
		}
		else if (Cue.Press == EPress::Stand)
		{
			Interact();
		}
		UE_LOG(LogTemp, Display, TEXT("HELMBOT cue %d at %.1f s"), BotHelmCue - 1, BotHelmSeconds);
	}

	// What this client sees of the helm and the ship, once a second, to compare with the host.
	BotHelmLogTimer -= DeltaSeconds;
	if (BotHelmLogTimer <= 0.f)
	{
		BotHelmLogTimer = 1.f;
		const FHelmLevers& Levers = Helm->GetLevers();
		UE_LOG(LogTemp, Display, TEXT("HELMBOT t %.1f seated %d mode %d throttle %.2f rudder %.2f planes %.2f speed %.2f heading %.1f pitch %.1f depth %.1f surfaced %d collisions %d"),
			BotHelmSeconds, Helm->IsSeated(this) ? 1 : 0, static_cast<int32>(Helm->GetMode()), Levers.Throttle, Levers.Rudder, Levers.Planes,
			BoundVessel->GetForwardSpeedMetersPerSecond(), FRotator::ClampAxis(BoundVessel->GetActorRotation().Yaw),
			BoundVessel->GetActorRotation().Pitch, BoundVessel->GetDepthMeters(), BoundVessel->IsSurfaced() ? 1 : 0, BoundVessel->GetCollisionCount());
	}
}

void AThalassaCharacter::ApplyCrewNudge()
{
	// Never on or near a ladder: it would push you sideways off it, or keep two people at its foot
	// pushing each other away from it, so that neither can climb.
	constexpr float LadderClearanceCm = 150.f;
	if (GetThalassaMovement()->IsClimbingLadder() || (BoundVessel && BoundVessel->IsLadderAt(GetActorLocation(), LadderClearanceCm)))
	{
		return;
	}

	// Applied as movement input by whoever controls this character. Input is already predicted and
	// sent to the server, so the nudge needs no extra replication and causes no disagreement, unlike
	// a push applied on the server.
	FVector Nudge = FVector::ZeroVector;
	const FVector Up = GetActorUpVector();
	for (TActorIterator<AThalassaCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == this)
		{
			continue;
		}

		FVector Away = FVector::VectorPlaneProject(GetActorLocation() - It->GetActorLocation(), Up);
		const float Distance = static_cast<float>(Away.Size());
		if (Distance >= PersonalSpaceCm)
		{
			continue;
		}

		if (Distance < 1.f)
		{
			// Exactly on top of each other there is no "away", so each picks a direction of their own.
			const int32 Id = GetPlayerState() ? GetPlayerState()->GetPlayerId() : 0;
			Away = FVector::ForwardVector.RotateAngleAxis(Id * 137.5f, Up);
		}

		Nudge += Away.GetSafeNormal() * (1.f - Distance / PersonalSpaceCm);
	}

	if (!Nudge.IsNearlyZero())
	{
		AddMovementInput(Nudge.GetClampedToMaxSize(1.f), MaxNudgeInput);
	}
}

void AThalassaCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	// The capsule's centre just dropped. Hold the view where it was, then let it follow down smoothly.
	CameraCrouchOffset += ScaledHalfHeightAdjust;
}

void AThalassaCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CameraCrouchOffset -= ScaledHalfHeightAdjust;
}

bool AThalassaCharacter::CanJumpInternal_Implementation() const
{
	// The engine refuses to jump while crouched. Here a crouch, or a slide, can always be jumped out of.
	return JumpIsAllowedInternal();
}

void AThalassaCharacter::JumpFromAnywhere()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	Jump();
}

void AThalassaCharacter::Interact()
{
	ServerInteract();
}

void AThalassaCharacter::ServerInteract_Implementation()
{
	if (AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld()))
	{
		// Leaving a station comes first, so E always gets you up again, even beside the full stop button.
		if (Vessel->FindStationOf(this) && Vessel->TryUseStation(this))
		{
			return;
		}
		if ((Vessel->GetHelm() && Vessel->GetHelm()->TryPressFullStopButton(this)) || Vessel->TryUseStation(this))
		{
			return;
		}
		if (!Vessel->TryPressBeep(GetActorLocation()))
		{
			Vessel->TryCycleAirlock(GetActorLocation());
		}
	}
}

void AThalassaCharacter::TickDiverBot(float DeltaSeconds)
{
	if (!BoundVessel || BoundVessel->GetProfileState() != EVesselProfileState::Running)
	{
		return;
	}

	// A full EVA in ship coordinates: into the airlock, out to sea, wait, catch the ship, come home.
	// Each step: where to go, how close counts, and the airlock state it needs before moving on.
	struct FDiveStep { FVector Target; float Radius; int32 NeedState; };
	const int32 AnyState = -1;
	const int32 Inner = static_cast<int32>(EAirlockState::InnerOpen);
	const int32 Outer = static_cast<int32>(EAirlockState::OuterOpen);
	const FDiveStep Steps[] =
	{
		{ FVector(0, 100, 90),   100.f, Inner },    // 0: Wait in the corridor for the inner door
		{ FVector(0, 340, 90),   40.f,  Inner },    // 1: Into the chamber
		{ FVector(0, 340, 90),   40.f,  Outer },    // 2: Cycle, wait to flood
		{ FVector(0, 650, 100),  120.f, Outer },    // 3: Out through the outer door
		{ FVector(0, 2000, 200), 150.f, AnyState }, // 4: Swim away from the ship
		{ FVector(0, 2000, 200), -1.f,  AnyState }, // 5: Tread water; the ship carries on
		{ FVector(0, 650, 100),  120.f, Outer },    // 6: Catch the ship, back to the outer door
		{ FVector(0, 340, 100),  40.f,  Outer },    // 7: Into the chamber
		{ FVector(0, 340, 90),   40.f,  Inner },    // 8: Cycle, wait to drain
		{ FVector(0, 0, 90),     100.f, Inner },    // 9: Back into the corridor
	};
	const int32 StepCount = UE_ARRAY_COUNT(Steps);
	const FDiveStep& Step = Steps[BotDiveStep];

	const FTransform ShipTransform = BoundVessel->GetActorTransform();
	const FVector ToTarget = ShipTransform.TransformPosition(Step.Target) - GetActorLocation();
	const bool bReached = Step.Radius > 0.f && ToTarget.SizeSquared() < FMath::Square(Step.Radius);
	const int32 State = static_cast<int32>(BoundVessel->GetAirlockState());
	const bool bStateReady = Step.NeedState == AnyState || State == Step.NeedState;

	BotDiveStepSeconds += DeltaSeconds;
	BotPressCooldown -= DeltaSeconds;

	bool bAdvance = bReached && bStateReady;
	if (Step.Radius < 0.f)
	{
		bAdvance = BotDiveStepSeconds > 8.f; // Stopped swimming for a while.
	}
	else if (!bStateReady && BotPressCooldown <= 0.f && (State == Inner || State == Outer))
	{
		// The airlock is the wrong way round for this step: ask for a cycle, like a player would.
		BotPressCooldown = 3.f;
		Interact();
	}

	if (bAdvance)
	{
		if (BotDiveStep == StepCount - 1)
		{
			++BotEvaCount;
		}
		BotDiveStep = (BotDiveStep + 1) % StepCount;
		BotDiveStepSeconds = 0.f;
		BotPressCooldown = 0.f;
		return;
	}

	if (BotDiveStepSeconds > 90.f)
	{
		// Reported, not hidden. Keep trying the same step: a diver cannot skip getting back aboard.
		++BotStuckCount;
		BotDiveStepSeconds = 0.f;
		UE_LOG(LogTemp, Display, TEXT("DIVERSTUCK step %d local %s airlock %d"), BotDiveStep,
			*ShipTransform.InverseTransformPosition(GetActorLocation()).ToString(), State);
	}

	if (Step.Radius > 0.f && !bReached)
	{
		AddMovementInput(ToTarget.GetSafeNormal());
	}
}

void AThalassaCharacter::TickWalkerBot(float DeltaSeconds)
{
	if (!BoundVessel || BoundVessel->GetProfileState() != EVesselProfileState::Running)
	{
		return;
	}

	const TArray<FVector>& Tour = BoundVessel->GetTourRoute();
	if (Tour.Num() > 0)
	{
		const FTransform Ship = BoundVessel->GetActorTransform();
		const FVector Here = Ship.InverseTransformPosition(GetActorLocation());
		if (!bTourStarted)
		{
			// Start at the waypoint nearest the spawn, wherever the spawns are. Every second walker starts one
			// further on, so walkers spread out along the route, as a crew would, instead of moving as one.
			int32 Nearest = 0;
			for (int32 Index = 1; Index < Tour.Num(); ++Index)
			{
				if (FVector::DistSquared(Tour[Index], Here) < FVector::DistSquared(Tour[Nearest], Here))
				{
					Nearest = Index;
				}
			}
			const int32 Id = GetPlayerState() ? GetPlayerState()->GetPlayerId() : 0;
			BotWaypoint = (Nearest + Id % 2) % Tour.Num();
			bTourStarted = true;
		}
		const FVector Target = Tour[BotWaypoint % Tour.Num()];

		// Height only matters on a ladder. Elsewhere your feet decide how high you are.
		const FVector Offset = Target - Here;
		const bool bClimbing = GetThalassaMovement()->IsClimbingLadder();
		const bool bReached = FVector2D(Offset.X, Offset.Y).SizeSquared() < FMath::Square(80.f)
			&& (FMath::Abs(Offset.Z) < 150.f);

		BotSecondsOnWaypoint += DeltaSeconds;
		if (bReached)
		{
			Interact();
			BotWaypoint = (BotWaypoint + 1) % Tour.Num();
			BotSecondsOnWaypoint = 0.f;
		}
		else if (BotSecondsOnWaypoint > 40.f)
		{
			++BotStuckCount;
			UE_LOG(LogTemp, Display, TEXT("WALKERSTUCK waypoint %d at %s heading for %s"), BotWaypoint, *Here.ToString(), *Target.ToString());
			BotWaypoint = (BotWaypoint + 1) % Tour.Num();
			BotSecondsOnWaypoint = 0.f;
		}

		const FVector Direction = bClimbing ? Offset.GetSafeNormal() : FVector(Offset.X, Offset.Y, 0.f).GetSafeNormal();
		AddMovementInput(Ship.TransformVectorNoScale(Direction));
		return;
	}

	// A tour in ship coordinates: console, both doorways, the engineering steps, and back.
	static const FVector2D Route[] =
	{
		FVector2D(1750.f, 0.f), FVector2D(1150.f, 0.f), FVector2D(-1150.f, 0.f), FVector2D(-1500.f, 0.f),
		FVector2D(-1850.f, 0.f), FVector2D(-1500.f, 0.f), FVector2D(-1150.f, 0.f), FVector2D(1150.f, 0.f),
	};
	static const int32 RouteLength = UE_ARRAY_COUNT(Route);
	const int32 BotSide = (GetPlayerState() && GetPlayerState()->GetPlayerId() % 2 == 0) ? -1 : 1;

	const FTransform ShipTransform = BoundVessel->GetActorTransform();
	const FVector Local = ShipTransform.InverseTransformPosition(GetActorLocation());
	const float Stretch = ThalassaTestConfig::HullStretch();
	const FVector2D Target(Route[BotWaypoint].X * Stretch, Route[BotWaypoint].Y + 45.f * BotSide);
	const FVector2D ToTarget = Target - FVector2D(Local.X, Local.Y);

	BotSecondsOnWaypoint += DeltaSeconds;
	if (ToTarget.SizeSquared() < FMath::Square(80.f))
	{
		if (BotWaypoint == 0)
		{
			Interact();
		}
		BotWaypoint = (BotWaypoint + 1) % RouteLength;
		BotSecondsOnWaypoint = 0.f;
	}
	else if (BotSecondsOnWaypoint > 20.f * Stretch)
	{
		// Reported, not hidden: moving on keeps the run going so the rest of it is still measured.
		++BotStuckCount;
		BotWaypoint = (BotWaypoint + 1) % RouteLength;
		BotSecondsOnWaypoint = 0.f;
	}

	const FVector2D Direction = ToTarget.GetSafeNormal();
	AddMovementInput(ShipTransform.TransformVectorNoScale(FVector(Direction.X, Direction.Y, 0.f)));

	if (ThalassaTestConfig::IsBotJumpDisabled())
	{
		return;
	}

	BotJumpTimer -= DeltaSeconds;
	if (BotJumpTimer <= 0.f)
	{
		BotJumpTimer = 9.f;
		Jump();
	}
	else if (BotJumpTimer < 8.7f)
	{
		StopJumping();
	}
}
