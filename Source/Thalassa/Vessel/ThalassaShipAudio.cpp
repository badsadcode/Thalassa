#include "Vessel/ThalassaShipAudio.h"

namespace ThalassaAudio
{
	void FBandPass::Set(float Hz, float Q, float Rate)
	{
		const float W = 2.f * PI * FMath::Clamp(Hz, 20.f, Rate * 0.45f) / Rate;
		const float Alpha = FMath::Sin(W) / (2.f * Q);
		const float A0 = 1.f + Alpha;
		B0 = Alpha / A0;
		B2 = -Alpha / A0;
		A1 = -2.f * FMath::Cos(W) / A0;
		A2 = (1.f - Alpha) / A0;
	}

	float FBandPass::Run(float X)
	{
		const float Y = B0 * X + B2 * X2 - A1 * Y1 - A2 * Y2;
		X2 = X1; X1 = X;
		Y2 = Y1; Y1 = Y;
		return Y;
	}

	float Wrap(float Phase) { return Phase > 2.f * PI ? Phase - 2.f * PI : Phase; }

	/** Spatial sound that carries this far before it is silent. */
	void Attenuate(USynthComponent* Synth, float FullCm, float FalloffCm)
	{
		Synth->bAllowSpatialization = true;
		Synth->bOverrideAttenuation = true;
		Synth->AttenuationOverrides.bAttenuate = true;
		Synth->AttenuationOverrides.bSpatialize = true;
		Synth->AttenuationOverrides.AttenuationShape = EAttenuationShape::Sphere;
		Synth->AttenuationOverrides.AttenuationShapeExtents = FVector(FullCm, 0.f, 0.f);
		Synth->AttenuationOverrides.FalloffDistance = FalloffCm;
	}
}

using namespace ThalassaAudio;

// ------------------------------------------------------------------------------------------- Machinery

UThalassaMachineryComponent::UThalassaMachineryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bAutoActivate = true;
	Attenuate(this, 1500.f, 7000.f);
}

bool UThalassaMachineryComponent::Init(int32& SampleRate)
{
	NumChannels = 1;
	Rate = static_cast<float>(SampleRate);
	return true;
}

int32 UThalassaMachineryComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const float Target = FMath::Clamp(TargetLoad.load(), 0.f, 1.f);
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		Load += (Target - Load) * (1.f / (Rate * 3.f)); // The machinery takes seconds to spin up or down.
		const float Base = 46.f + 10.f * Load;
		const float Hz[5] = { Base, Base * 1.012f, Base * 2.f, Base * 3.f, 760.f + 420.f * Load };
		const float Gain[5] = { 0.45f, 0.25f, 0.28f, 0.12f, 0.018f + 0.03f * Load };
		float Sample = 0.f;
		for (int32 P = 0; P < 5; ++P)
		{
			Phase[P] = Wrap(Phase[P] + 2.f * PI * Hz[P] / Rate);
			Sample += Gain[P] * FMath::Sin(Phase[P]);
		}

		// The pump: a low thump on its cycle, quicker under load.
		const float Period = 1.6f - 0.6f * Load;
		PumpTime += 1.f / Rate;
		if (PumpTime > Period)
		{
			PumpTime -= Period;
		}
		Sample += 0.55f * FMath::Exp(-PumpTime / 0.11f) * FMath::Sin(2.f * PI * 38.f * PumpTime);

		// Air handling: a soft, dark hiss.
		Brown = (Brown + 0.02f * Noise.White()) * 0.998f;
		Sample += 0.6f * Brown;

		OutAudio[Index] = 0.32f * (0.65f + 0.35f * Load) * Sample;
	}
	return NumSamples;
}

// ------------------------------------------------------------------------------------------- Hull ambience

UThalassaHullAmbienceComponent::UThalassaHullAmbienceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bAutoActivate = true;
	bAllowSpatialization = false; // Each player hears their own, as if all around them.
}

void UThalassaHullAmbienceComponent::SetState(float InInside, float SpeedMS, float DepthM, float TurnDegS)
{
	TargetInside.store(InInside);
	TargetSpeed.store(SpeedMS);
	TargetDepth.store(DepthM);
	TargetTurn.store(TurnDegS);
}

bool UThalassaHullAmbienceComponent::Init(int32& SampleRate)
{
	NumChannels = 1;
	Rate = static_cast<float>(SampleRate);
	Noise.State = 777u;
	return true;
}

void UThalassaHullAmbienceComponent::StartCreak()
{
	// Steel under pressure: a resonance sliding between two pitches, excited by uneven stick-slip clicks.
	const float Pick = (Noise.White() + 1.f) * 0.5f;
	CreakTime = 0.f;
	CreakLength = 0.6f + 1.6f * Pick;
	CreakFrom = 140.f + 260.f * (Noise.White() + 1.f) * 0.5f;
	CreakTo = CreakFrom * (0.7f + 0.6f * (Noise.White() + 1.f) * 0.5f);
	CreakClickRate = 25.f + 90.f * (Noise.White() + 1.f) * 0.5f;
	CreakLoud = 0.5f + 0.5f * (Noise.White() + 1.f) * 0.5f;
	UntilClick = 0.f;
}

int32 UThalassaHullAmbienceComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const float Smooth = 1.f / (Rate * 0.5f);
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		Inside += (FMath::Clamp(TargetInside.load(std::memory_order_relaxed), 0.f, 1.f) - Inside) * Smooth;
		Speed += (TargetSpeed.load(std::memory_order_relaxed) - Speed) * Smooth;
		Depth += (TargetDepth.load(std::memory_order_relaxed) - Depth) * Smooth;
		Turn += (FMath::Abs(TargetTurn.load(std::memory_order_relaxed)) - Turn) * Smooth;

		const float White = Noise.White();
		Brown = (Brown + 0.02f * White) * 0.998f;

		// Inside: the ship's faint, ever-present hum, and a low rumble through the hull.
		HumPhase = Wrap(HumPhase + 2.f * PI * 47.f / Rate);
		float Sample = Inside * (0.06f * FMath::Sin(HumPhase) + 0.5f * RumbleFilter.Run(Brown, 60.f, Rate));

		// Water rushing past: brighter and louder with speed, much louder out in it.
		const float Flow = FMath::Pow(FMath::Clamp(Speed / 15.f, 0.f, 1.f), 1.4f);
		Sample += Flow * FMath::Lerp(1.4f, 0.35f, Inside) * FlowFilter.Run(White, 150.f + 55.f * Speed, Rate);
		// Out in the water there is always a little of it: the sea is never quiet.
		Sample += (1.f - Inside) * 0.35f * RumbleFilter.Y;

		// Creaks: rare near the surface, often in the deep, more while the hull is twisting through a turn.
		UntilNextCreak -= 1.f / Rate;
		if (UntilNextCreak <= 0.f && CreakTime < 0.f)
		{
			StartCreak();
			const float Deep = FMath::Clamp(Depth / 600.f, 0.f, 1.f);
			const float Mean = FMath::Lerp(28.f, 7.f, Deep) / (1.f + 0.4f * Turn);
			UntilNextCreak = Mean * (0.4f + 1.2f * (Noise.White() + 1.f) * 0.5f);
		}
		if (CreakTime >= 0.f)
		{
			const float T = CreakTime / CreakLength;
			Creak.Set(FMath::Lerp(CreakFrom, CreakTo, T), 9.f, Rate);
			UntilClick -= 1.f / Rate;
			float Click = 0.f;
			if (UntilClick <= 0.f)
			{
				Click = 1.f;
				UntilClick = 1.f / (CreakClickRate * (0.6f + 0.8f * (Noise.White() + 1.f) * 0.5f));
			}
			const float Envelope = FMath::Sin(PI * FMath::Clamp(T, 0.f, 1.f));
			Sample += CreakLoud * Envelope * FMath::Lerp(0.5f, 1.f, Inside) * 6.f * Creak.Run(Click + 0.05f * White);
			CreakTime += 1.f / Rate;
			if (CreakTime > CreakLength)
			{
				CreakTime = -1.f;
			}
		}

		OutAudio[Index] = 0.4f * Sample;
	}
	return NumSamples;
}

// ------------------------------------------------------------------------------------------- Bleeps

UThalassaBleepComponent::UThalassaBleepComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bAutoActivate = true;
	Attenuate(this, 300.f, 1800.f);
}

bool UThalassaBleepComponent::Init(int32& SampleRate)
{
	NumChannels = 1;
	Rate = static_cast<float>(SampleRate);
	return true;
}

int32 UThalassaBleepComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const int32 New = Pending.exchange(0);
	if (New > 0)
	{
		Kind = New - 1;
		Time = 0.f;
	}
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		float Sample = 0.f;
		if (Kind >= 0)
		{
			// Each bleep is a few short tones: (start, length, pitch).
			static const float Tones[4][3][3] = {
				{ { 0.f, 0.08f, 880.f }, { 0.1f, 0.1f, 1320.f }, { 0.f, 0.f, 0.f } },          // On
				{ { 0.f, 0.08f, 1320.f }, { 0.1f, 0.1f, 880.f }, { 0.f, 0.f, 0.f } },          // Off
				{ { 0.f, 0.018f, 2400.f }, { 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f } },             // Click
				{ { 0.f, 0.12f, 1760.f }, { 0.2f, 0.12f, 1760.f }, { 0.4f, 0.12f, 1760.f } },  // Alarm
			};
			bool bAny = false;
			for (const auto& Tone : Tones[Kind])
			{
				if (Tone[1] <= 0.f)
				{
					continue;
				}
				const float Local = Time - Tone[0];
				if (Local >= 0.f && Local < Tone[1] + 0.05f)
				{
					bAny = true;
					const float Envelope = FMath::Min(Local / 0.003f, 1.f) * (Local < Tone[1] ? 1.f : FMath::Exp(-(Local - Tone[1]) / 0.012f));
					Phase = Wrap(Phase + 2.f * PI * Tone[2] / Rate);
					Sample += Envelope * (FMath::Sin(Phase) + 0.2f * FMath::Sin(3.f * Phase));
				}
				else if (Local < 0.f)
				{
					bAny = true;
				}
			}
			Time += 1.f / Rate;
			if (!bAny)
			{
				Kind = -1;
			}
		}
		OutAudio[Index] = 0.22f * Sample;
	}
	return NumSamples;
}

// ------------------------------------------------------------------------------------------- Impact

UThalassaImpactComponent::UThalassaImpactComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bAutoActivate = true;
	bAllowSpatialization = false; // The whole hull rings: everyone aboard hears it.
}

bool UThalassaImpactComponent::Init(int32& SampleRate)
{
	NumChannels = 1;
	Rate = static_cast<float>(SampleRate);
	Noise.State = 4321u;
	return true;
}

int32 UThalassaImpactComponent::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	if (Pending.exchange(0) > 0)
	{
		Time = 0.f;
	}
	// The steel's ring: a few out-of-tune partials, each dying away at its own pace.
	static const float RingHz[6] = { 73.f, 173.f, 281.f, 437.f, 611.f, 893.f };
	static const float RingDecay[6] = { 3.0f, 2.2f, 1.6f, 1.1f, 0.8f, 0.5f };
	static const float RingGain[6] = { 0.22f, 0.14f, 0.11f, 0.08f, 0.06f, 0.04f };
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		float Sample = 0.f;
		if (Time >= 0.f)
		{
			const float White = Noise.White();
			// The thud: a heavy low blow, its pitch dropping as it dies.
			const float ThudHz = 30.f + 55.f * FMath::Exp(-Time * 3.f);
			Phases[0] = Wrap(Phases[0] + 2.f * PI * ThudHz / Rate);
			Sample += 1.0f * FMath::Exp(-Time / 0.28f) * FMath::Sin(Phases[0]);
			// The crash: a burst of noise, quickly gone.
			Sample += 0.7f * FMath::Exp(-Time / 0.1f) * CrashFilter.Run(White, 900.f, Rate);
			for (int32 P = 1; P < 6; ++P)
			{
				Phases[P] = Wrap(Phases[P] + 2.f * PI * RingHz[P] / Rate);
				Sample += RingGain[P] * FMath::Exp(-Time / RingDecay[P]) * FMath::Sin(Phases[P]);
			}
			// The groan afterwards: the hull settling back, deep and slow.
			if (Time > 0.3f && Time < 3.5f)
			{
				const float G = (Time - 0.3f) / 3.2f;
				Groan.Set(110.f - 40.f * G, 7.f, Rate);
				Sample += 1.2f * FMath::Sin(PI * G) * Groan.Run(White);
			}
			Time += 1.f / Rate;
			if (Time > 5.f)
			{
				Time = -1.f;
			}
		}
		OutAudio[Index] = 0.75f * FMath::Tanh(Sample); // A soft limit, so it is heavy without clipping.
	}
	return NumSamples;
}
