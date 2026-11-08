#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "ThalassaPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Spike controls. Input objects are created in code so the test needs no authored assets;
 * they should become data assets once the control scheme settles.
 */
UCLASS()
class THALASSA_API AThalassaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	bool IsStationViewExpanded() const { return bStationViewExpanded; }

	/** True while this player's character is at the helm. */
	bool IsAtHelm() const;

	/** True while this player's character is at the camera station. */
	bool IsAtCameraStation() const;

	/** At the camera station: step to the next camera, or the previous one. */
	void RequestCameraStep(int32 Step) { ServerSelectCamera(Step); }

	/** What the helmsman is pressing, each from -1 to 1. Sent to the server only when it changes. */
	void SetHelmInput(float Throttle, float Turn, float Dive, float Heave = 0.f);
	void RequestHelmMode();
	void RequestHelmFullStop();
	void RequestHelmBoost();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	UInputAction* MakeAction(const TCHAR* Name, EInputActionValueType ValueType);
	void MapAxisKey(UInputAction* Action, FKey Key, bool bNegate, bool bSwizzle);

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnSwimVertical(const FInputActionValue& Value);
	void OnHelmPlanes(const FInputActionValue& Value);
	void OnJumpStart();
	void OnSprintStart();
	void OnSprintStop();
	void OnJumpStop();
	void OnInteract();
	void OnToggleStationView();
	void OnStartProfile();
	void OnToggleSonar();

	UFUNCTION(Server, Reliable)
	void ServerToggleSonar();

	void OnSonarPing();
	void OnSonarGrade();

	UFUNCTION(Server, Reliable)
	void ServerSonarPing();

	UFUNCTION(Server, Reliable)
	void ServerSonarGrade();

	UFUNCTION(Server, Reliable)
	void ServerSetHelmInput(float Throttle, float Turn, float Dive, float Heave);

	UFUNCTION(Server, Reliable)
	void ServerHelmMode();

	UFUNCTION(Server, Reliable)
	void ServerSelectCamera(int32 Step);

	UFUNCTION(Server, Reliable)
	void ServerHelmFullStop();

	UFUNCTION(Server, Reliable)
	void ServerHelmBoost();

	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(Server, Reliable)
	void ServerStartProfile();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> Actions;

	// At the helm, the movement keys drive the ship instead of the character.
	FVector2D MoveAxis = FVector2D::ZeroVector;
	float VerticalAxis = 0.f;
	FVector4 SentHelmInput = FVector4(0, 0, 0, 0);
	float PlanesAxis = 0.f; // Q and Z at the helm: the diving planes.
	bool bWasAtHelm = false;
	int32 LastCameraStep = 0;

	/** Test recording of this machine's sound output (-ThalassaRecordAudio). */
	void TickAudioRecording(float DeltaTime);
	float AudioRecordClock = 0.f;
	int32 AudioRecordStage = 0; // 0 waiting, 1 recording, 2 done.
	bool bStationViewExpanded = false;
};
