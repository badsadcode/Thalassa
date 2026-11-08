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

	/**
	 * Before play: make this the active ping, a deep, long, loud pulse heard everywhere aboard, with slower,
	 * longer echoes, instead of the survey's light tick at the screen.
	 */
	void MakeActivePulse();

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	std::atomic<int32> PendingPings{ 0 };

	// The sound's shape, fixed before play.
	float StartHz = 1650.f;
	float FallHzPerSecond = 140.f;   // A slight fall in pitch, so it sounds struck rather than electronic.
	float DecaySeconds = 0.32f;
	float LengthSeconds = 2.5f;
	float Volume = 0.45f;
	float EchoDelaySeconds = 0.37f;
	float EchoSend = 0.35f;
	float EchoFeedback = 0.5f;

	// Audio thread only.
	float Rate = 48000.f;
	int32 SampleInPing = -1;
	float Phase = 0.f;
	TArray<float> Echo;
	int32 EchoIndex = 0;
	float EchoLowPass = 0.f;
};
