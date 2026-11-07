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
	void OnHelmThrottle(const FInputActionValue& Value);
	void OnHelmTurn(const FInputActionValue& Value);
	void OnHelmDive(const FInputActionValue& Value);
	void OnStartProfile();

	UFUNCTION(Server, Reliable)
	void ServerSetHelm(float Throttle, float Turn, float Dive);

	UFUNCTION(Server, Reliable)
	void ServerStartProfile();

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> Actions;

	float HelmThrottle = 0.f;
	float HelmTurn = 0.f;
	float HelmDive = 0.f;
	bool bStationViewExpanded = false;
};
