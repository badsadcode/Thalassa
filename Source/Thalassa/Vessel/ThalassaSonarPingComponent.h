#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "ThalassaSonarPingComponent.generated.h"

/**
 * The sonar's ping, synthesised as it plays, so it needs no sound asset: a short falling tone with a
 * long tail, and fainter repeats like the hull ringing. Placeholder sound, in the spirit of the grey deck.
 */
UCLASS()
class THALASSA_API UThalassaSonarPingComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UThalassaSonarPingComponent(const FObjectInitializer& ObjectInitializer);

	/** Game thread: start a ping. The audio thread picks it up on its next buffer. */
	void Ping();

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	std::atomic<int32> PendingPings{ 0 };

	// Audio thread only.
	float Rate = 48000.f;
	int32 SampleInPing = -1;
	float Phase = 0.f;
	TArray<float> Echo;
	int32 EchoIndex = 0;
	float EchoLowPass = 0.f;
};
