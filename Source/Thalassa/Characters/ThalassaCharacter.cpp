#include "Characters/ThalassaCharacter.h"

#include "Camera/CameraComponent.h"
#include "Characters/ThalassaMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
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

	if (IsLocallyControlled())
	{
		if (ThalassaTestConfig::IsBot())
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
		Vessel->TryPressBeep(GetActorLocation());
	}
}

void AThalassaCharacter::TickWalkerBot(float DeltaSeconds)
{
	if (!BoundVessel || BoundVessel->GetProfileState() != EVesselProfileState::Running)
	{
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
	const FVector2D Target = Route[BotWaypoint] + FVector2D(0.f, 45.f * BotSide);
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
	else if (BotSecondsOnWaypoint > 20.f)
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
