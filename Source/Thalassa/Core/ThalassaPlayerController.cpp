#include "Core/ThalassaPlayerController.h"

#include "Characters/ThalassaCharacter.h"
#include "Characters/ThalassaMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tools/ThalassaTestConfig.h"
#include "Vessel/ThalassaHelmComponent.h"
#include "Vessel/ThalassaSonarComponent.h"
#include "Vessel/ThalassaVessel.h"

UInputAction* AThalassaPlayerController::MakeAction(const TCHAR* Name, EInputActionValueType ValueType)
{
	UInputAction* Action = NewObject<UInputAction>(this, Name);
	Action->ValueType = ValueType;
	Actions.Add(Action);
	return Action;
}

void AThalassaPlayerController::MapAxisKey(UInputAction* Action, FKey Key, bool bNegate, bool bSwizzle)
{
	FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(Action, Key);
	if (bSwizzle)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	}
	if (bNegate)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
}

void AThalassaPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("SpikeControls"));

	UInputAction* Move = MakeAction(TEXT("Move"), EInputActionValueType::Axis2D);
	MapAxisKey(Move, EKeys::W, false, true);
	MapAxisKey(Move, EKeys::S, true, true);
	MapAxisKey(Move, EKeys::D, false, false);
	MapAxisKey(Move, EKeys::A, true, false);

	UInputAction* Look = MakeAction(TEXT("Look"), EInputActionValueType::Axis2D);
	MappingContext->MapKey(Look, EKeys::Mouse2D);

	UInputAction* JumpAction = MakeAction(TEXT("JumpAction"), EInputActionValueType::Boolean);
	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);

	UInputAction* SwimVertical = MakeAction(TEXT("SwimVertical"), EInputActionValueType::Axis1D);
	MapAxisKey(SwimVertical, EKeys::SpaceBar, false, false);
	MapAxisKey(SwimVertical, EKeys::C, true, false);

	UInputAction* InteractAction = MakeAction(TEXT("InteractAction"), EInputActionValueType::Boolean);
	MappingContext->MapKey(InteractAction, EKeys::E);

	UInputAction* StationView = MakeAction(TEXT("StationView"), EInputActionValueType::Boolean);
	MappingContext->MapKey(StationView, EKeys::Tab);

	UInputAction* ToggleSonar = MakeAction(TEXT("ToggleSonar"), EInputActionValueType::Boolean);
	MappingContext->MapKey(ToggleSonar, EKeys::N);

	UInputAction* StartProfile = MakeAction(TEXT("StartProfile"), EInputActionValueType::Boolean);
	MappingContext->MapKey(StartProfile, EKeys::P);

	UInputAction* HelmMode = MakeAction(TEXT("HelmMode"), EInputActionValueType::Boolean);
	MappingContext->MapKey(HelmMode, EKeys::R);

	UInputAction* HelmAllStop = MakeAction(TEXT("HelmAllStop"), EInputActionValueType::Boolean);
	MappingContext->MapKey(HelmAllStop, EKeys::X);

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(InputComponent);
	Input->BindAction(Move, ETriggerEvent::Triggered, this, &AThalassaPlayerController::OnMove);
	Input->BindAction(Move, ETriggerEvent::Completed, this, &AThalassaPlayerController::OnMove); // Zero: let go.
	Input->BindAction(Look, ETriggerEvent::Triggered, this, &AThalassaPlayerController::OnLook);
	Input->BindAction(SwimVertical, ETriggerEvent::Triggered, this, &AThalassaPlayerController::OnSwimVertical);
	Input->BindAction(SwimVertical, ETriggerEvent::Completed, this, &AThalassaPlayerController::OnSwimVertical);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AThalassaPlayerController::OnJumpStart);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &AThalassaPlayerController::OnJumpStop);
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AThalassaPlayerController::OnInteract);
	Input->BindAction(StationView, ETriggerEvent::Started, this, &AThalassaPlayerController::OnToggleStationView);
	Input->BindAction(StartProfile, ETriggerEvent::Started, this, &AThalassaPlayerController::OnStartProfile);
	Input->BindAction(ToggleSonar, ETriggerEvent::Started, this, &AThalassaPlayerController::OnToggleSonar);
	Input->BindAction(HelmMode, ETriggerEvent::Started, this, &AThalassaPlayerController::RequestHelmMode);
	Input->BindAction(HelmAllStop, ETriggerEvent::Started, this, &AThalassaPlayerController::RequestHelmAllStop);
}

void AThalassaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
		SetInputMode(FInputModeGameOnly());
		bStationViewExpanded = FParse::Param(FCommandLine::Get(), TEXT("ThalassaStationView"));
	}
}

void AThalassaPlayerController::OnMove(const FInputActionValue& Value)
{
	MoveAxis = Value.Get<FVector2D>();
	if (IsAtHelm())
	{
		return; // PlayerTick sends it to the helm.
	}

	if (APawn* ControlledPawn = GetPawn())
	{
		const FVector2D Axis = MoveAxis;

		const AThalassaCharacter* Crew = Cast<AThalassaCharacter>(ControlledPawn);

		// On a ladder, forward climbs: up, or down if you are looking down. Pushing ahead as well as
		// up is what carries you off the top of the ladder when you reach it.
		if (Crew && Crew->GetThalassaMovement()->IsClimbingLadder())
		{
			const float LookPitch = FRotator::NormalizeAxis(GetControlRotation().Pitch);
			const float Climb = (LookPitch < -30.f ? -1.f : 1.f) * Axis.Y;
			const FRotator LadderFacing(0.f, GetControlRotation().Yaw, 0.f);
			ControlledPawn->AddMovementInput(FVector::UpVector, Climb);
			ControlledPawn->AddMovementInput(LadderFacing.RotateVector(FVector::ForwardVector), Axis.Y * 0.6f);
			ControlledPawn->AddMovementInput(LadderFacing.RotateVector(FVector::RightVector), Axis.X);
			return;
		}

		// On foot you walk where you face on the deck. In the water you swim where you look.
		const bool bSwimming = Crew && Crew->GetThalassaMovement()->IsSwimmingInSea();
		const FRotator Facing = bSwimming ? GetControlRotation() : FRotator(0.f, GetControlRotation().Yaw, 0.f);
		ControlledPawn->AddMovementInput(Facing.RotateVector(FVector::ForwardVector), Axis.Y);
		ControlledPawn->AddMovementInput(Facing.RotateVector(FVector::RightVector), Axis.X);
	}
}

void AThalassaPlayerController::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddYawInput(Axis.X);
	AddPitchInput(-Axis.Y);
}

void AThalassaPlayerController::OnSwimVertical(const FInputActionValue& Value)
{
	VerticalAxis = Value.Get<float>();
	if (IsAtHelm())
	{
		return; // Space and C work the planes. PlayerTick sends them.
	}
	const AThalassaCharacter* Crew = Cast<AThalassaCharacter>(GetPawn());
	if (Crew && Crew->GetThalassaMovement()->IsSwimmingInSea())
	{
		GetPawn()->AddMovementInput(FVector::UpVector, Value.Get<float>());
	}
}

void AThalassaPlayerController::OnJumpStart()
{
	if (IsAtHelm())
	{
		return;
	}
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->Jump();
	}
}

void AThalassaPlayerController::OnJumpStop()
{
	if (ACharacter* ControlledCharacter = GetCharacter())
	{
		ControlledCharacter->StopJumping();
	}
}

void AThalassaPlayerController::OnInteract()
{
	if (AThalassaCharacter* Crew = Cast<AThalassaCharacter>(GetPawn()))
	{
		Crew->Interact();
	}
}

void AThalassaPlayerController::OnToggleStationView()
{
	bStationViewExpanded = !bStationViewExpanded;
}

bool AThalassaPlayerController::IsAtHelm() const
{
	const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld());
	return Vessel && Vessel->GetHelm() && Vessel->GetHelm()->IsSeated(GetPawn());
}

void AThalassaPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalController())
	{
		return;
	}

	// Scripted helmsmen send their own input.
	const bool bAtHelm = IsAtHelm();
	if (bAtHelm && !ThalassaTestConfig::IsHelmBot())
	{
		SetHelmInput(MoveAxis.Y, MoveAxis.X, VerticalAxis);
	}
	else if (!bAtHelm && bWasAtHelm)
	{
		SentHelmInput = FVector::ZeroVector; // The server forgets it too when you stand up.
	}
	bWasAtHelm = bAtHelm;
}

void AThalassaPlayerController::SetHelmInput(float Throttle, float Turn, float Dive)
{
	const FVector Wanted(Throttle, Turn, Dive);
	if (!Wanted.Equals(SentHelmInput, 0.01f))
	{
		SentHelmInput = Wanted;
		ServerSetHelmInput(Throttle, Turn, Dive);
	}
}

void AThalassaPlayerController::RequestHelmMode()
{
	if (IsAtHelm())
	{
		ServerHelmMode();
	}
}

void AThalassaPlayerController::RequestHelmAllStop()
{
	if (IsAtHelm())
	{
		ServerHelmAllStop();
	}
}

void AThalassaPlayerController::OnStartProfile()
{
	ServerStartProfile();
}

void AThalassaPlayerController::OnToggleSonar()
{
	ServerToggleSonar();
}

void AThalassaPlayerController::ServerToggleSonar_Implementation()
{
	if (AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld()))
	{
		Vessel->GetSonar()->SetSurveying(!Vessel->GetSonar()->IsSurveying());
	}
}

void AThalassaPlayerController::ServerSetHelmInput_Implementation(float Throttle, float Turn, float Dive)
{
	const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld());
	if (Vessel && Vessel->GetHelm())
	{
		Vessel->GetHelm()->SetInput(GetPawn(), Throttle, Turn, Dive);
	}
}

void AThalassaPlayerController::ServerHelmMode_Implementation()
{
	const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld());
	if (Vessel && Vessel->GetHelm())
	{
		Vessel->GetHelm()->ToggleMode(GetPawn());
	}
}

void AThalassaPlayerController::ServerHelmAllStop_Implementation()
{
	const AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld());
	if (Vessel && Vessel->GetHelm())
	{
		Vessel->GetHelm()->AllStop(GetPawn());
	}
}

void AThalassaPlayerController::ServerStartProfile_Implementation()
{
	if (AThalassaVessel* Vessel = AThalassaVessel::Find(GetWorld()))
	{
		Vessel->StartProfile();
	}
}
