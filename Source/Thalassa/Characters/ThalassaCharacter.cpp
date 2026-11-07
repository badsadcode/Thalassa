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
#include "Vessel/ThalassaVessel.h"

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
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
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
		if (ThalassaTestConfig::IsDiverBot())
		{
			TickDiverBot(DeltaSeconds);
		}
		else if (ThalassaTestConfig::IsBot())
		{
			TickWalkerBot(DeltaSeconds);
		}
		if (!ThalassaTestConfig::IsCrewNudgeDisabled())
		{
			ApplyCrewNudge();
		}
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

void AThalassaCharacter::Interact()
{
	ServerInteract();
}

void AThalassaCharacter::ServerInteract_Implementation()
{
	if (AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld()))
	{
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
			// Walkers spread out along the route, as a crew would, instead of moving as one. Both
			// starting points are in the corridor beside the spawn points, so both are reachable.
			const int32 Id = GetPlayerState() ? GetPlayerState()->GetPlayerId() : 0;
			BotWaypoint = (Id % 2) * 8;
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
