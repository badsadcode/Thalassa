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

	/** What the helmsman is pressing, each from -1 to 1. Sent to the server only when it changes. */
	void SetHelmInput(float Throttle, float Turn, float Dive);
	void RequestHelmMode();
	void RequestHelmAllStop();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	UInputAction* MakeAction(const TCHAR* Name, EInputActionValueType ValueType);
	void MapAxisKey(UInputAction* Action, FKey Key, bool bNegate, bool bSwizzle);

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnSwimVertical(const FInputActionValue& Value);
	void OnJumpStart();
	void OnJumpStop();
	void OnInteract();
	void OnToggleStationView();
	void OnStartProfile();
	void OnToggleSonar();

	UFUNCTION(Server, Reliable)
	void ServerToggleSonar();

	UFUNCTION(Server, Reliable)
	void ServerSetHelmInput(float Throttle, float Turn, float Dive);

	UFUNCTION(Server, Reliable)
	void ServerHelmMode();

	UFUNCTION(Server, Reliable)
	void ServerHelmAllStop();

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
	FVector SentHelmInput = FVector::ZeroVector;
	bool bWasAtHelm = false;
	bool bStationViewExpanded = false;
};
