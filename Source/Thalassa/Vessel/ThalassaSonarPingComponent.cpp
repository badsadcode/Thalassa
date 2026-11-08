#include "Vessel/ThalassaSonarPingComponent.h"

namespace ThalassaPing
{
	constexpr float AttackSeconds = 0.004f;
	constexpr float EchoDamping = 0.25f;      // Each repeat is duller than the last, as in water.
}

UThalassaSonarPingComponent::UThalassaSonarPingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bAutoActivate = true;

	// Heard around the console and across the bridge, not through the whole ship.
	bAllowSpatialization = true;
	bOverrideAttenuation = true;
	AttenuationOverrides.bAttenuate = true;
	AttenuationOverrides.bSpatialize = true;
	AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
	AttenuationOverrides.AttenuationShapeExtents = FVector(400.f, 0.f, 0.f);
	AttenuationOverrides.FalloffDistance = 2600.f;
}

void UThalassaSonarPingComponent::MakeActivePulse()
{
	// Lower, longer and louder, with echoes coming back slowly, as from distant ground.
	StartHz = 880.f;
	FallHzPerSecond = 60.f;
	DecaySeconds = 0.9f;
	LengthSeconds = 6.f;
	Volume = 0.6f;
	EchoDelaySeconds = 0.62f;
	EchoSend = 0.45f;
	EchoFeedback = 0.62f;

	// Heard all through the ship: it's the hull itself ringing.
	bAllowSpatialization = false;
	bOverrideAttenuation = false;
}

void UThalassaSonarPingComponent::Ping()
{
	PendingPings.fetch_add(1);
}

bool UThalassaSonarPingComponent::Init(int32& SampleRate)
{
	NumChannels = 1;
	Rate = static_cast<float>(SampleRate);
	Echo.SetNumZeroed(FMath::Max(1, FMath::RoundToInt(Rate * EchoDelaySeconds)));
	EchoIndex = 0;
	return true;
}

int32 UThalassaSonarPingComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	using namespace ThalassaPing;

	if (PendingPings.exchange(0) > 0)
	{
		SampleInPing = 0;
		Phase = 0.f;
	}

	const int32 LengthSamples = FMath::RoundToInt(LengthSeconds * Rate);
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		float Dry = 0.f;
		if (SampleInPing >= 0 && SampleInPing < LengthSamples)
		{
			const float Time = SampleInPing / Rate;
			const float Envelope = (1.f - FMath::Exp(-Time / AttackSeconds)) * FMath::Exp(-Time / DecaySeconds);
			Phase += 2.f * PI * (StartHz - FallHzPerSecond * Time) / Rate;
			if (Phase > 2.f * PI)
			{
				Phase -= 2.f * PI;
			}
			Dry = Volume * Envelope * (FMath::Sin(Phase) + 0.15f * FMath::Sin(2.f * Phase));
			++SampleInPing;
		}

		// A damped feedback delay: the repeats.
		const float Delayed = Echo[EchoIndex];
		EchoLowPass += (Delayed - EchoLowPass) * (1.f - EchoDamping);
		Echo[EchoIndex] = Dry * EchoSend + EchoLowPass * EchoFeedback;
		EchoIndex = (EchoIndex + 1) % Echo.Num();

		OutAudio[Index] = Dry + Delayed;
	}
	return NumSamples;
}
