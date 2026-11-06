#include "Characters/ThalassaCharacter.h"

#include "Camera/CameraComponent.h"
#include "Characters/ThalassaMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerState.h"
#include "Tools/ThalassaTestConfig.h"
#include "UObject/ConstructorHelpers.h"
#include "Vessel/ThalassaVessel.h"

AThalassaCharacter::AThalassaCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UThalassaMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
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

void AThalassaCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (ThalassaTestConfig::IsPawnCollisionDisabled())
	{
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
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

	if (IsLocallyControlled() && ThalassaTestConfig::IsBot())
	{
		TickWalkerBot(DeltaSeconds);
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
