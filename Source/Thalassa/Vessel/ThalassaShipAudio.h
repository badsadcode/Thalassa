#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "ThalassaShipAudio.generated.h"

/*
 * The ship's sounds, synthesised as they play, like the sonar ping: no sound assets. Each process plays its
 * own from replicated state, so nothing about sound is replicated. The game thread sets parameters and fires
 * events through atomics; the audio thread reads them. See docs/SOUND.md.
 */

namespace ThalassaAudio
{
	/** A small, fast noise source for the audio thread. */
	struct FNoise
	{
		uint32 State = 22222u;
		float White()
		{
			State ^= State << 13;
			State ^= State >> 17;
			State ^= State << 5;
			return static_cast<float>(State) / 2147483648.f - 1.f;
		}
	};

	/** A two-pole band-pass filter (the classic audio "cookbook" design). */
	struct FBandPass
	{
		float B0 = 0, B2 = 0, A1 = 0, A2 = 0, X1 = 0, X2 = 0, Y1 = 0, Y2 = 0;
		void Set(float Hz, float Q, float Rate);
		float Run(float X);
	};

	/** A one-pole low-pass filter. */
	struct FLowPass
	{
		float Y = 0.f;
		float Run(float X, float Hz, float Rate) { Y += (X - Y) * FMath::Clamp(2.f * PI * Hz / Rate, 0.f, 1.f); return Y; }
	};
}

/**
 * The engine room: a deep hum with a slow beat, a pump thumping on its cycle, a whine that rises with the
 * load, and the hiss of air handling. Loud in engineering, faint across the ship.
 */
UCLASS()
class THALASSA_API UThalassaMachineryComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UThalassaMachineryComponent(const FObjectInitializer& ObjectInitializer);

	/** Game thread: how hard the machinery is working, 0 to 1. */
	void SetLoad(float NewLoad) { TargetLoad.store(NewLoad); }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	std::atomic<float> TargetLoad{ 0.f };
	float Rate = 48000.f;
	float Load = 0.f;
	float Phase[5] = {};
	float PumpTime = 0.f;
	float Brown = 0.f;
	ThalassaAudio::FNoise Noise;
};

/**
 * What one player hears of the hull, wherever they are on or off the ship: inside, a faint hum and a low
 * rumble; water rushing past, louder with speed and much louder outside; and the hull creaking and groaning,
 * more often the deeper the ship is and while it turns.
 */
UCLASS()
class THALASSA_API UThalassaHullAmbienceComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UThalassaHullAmbienceComponent(const FObjectInitializer& ObjectInitializer);

	/** Game thread: 1 inside the hull, 0 out in the water; speed in m/s; depth in metres; turning in degrees a second. */
	void SetState(float Inside, float SpeedMS, float DepthM, float TurnDegS);

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	void StartCreak();

	std::atomic<float> TargetInside{ 1.f }, TargetSpeed{ 0.f }, TargetDepth{ 0.f }, TargetTurn{ 0.f };
	float Rate = 48000.f;
	float Inside = 1.f, Speed = 0.f, Depth = 0.f, Turn = 0.f;
	float HumPhase = 0.f;
	float Brown = 0.f;
	ThalassaAudio::FLowPass FlowFilter, RumbleFilter;
	ThalassaAudio::FNoise Noise;

	// The current creak, if any: a resonance sweeping between two pitches, excited by uneven clicks.
	ThalassaAudio::FBandPass Creak;
	float UntilNextCreak = 4.f;
	float CreakTime = -1.f, CreakLength = 1.f, CreakFrom = 200.f, CreakTo = 300.f, CreakClickRate = 60.f, UntilClick = 0.f, CreakLoud = 1.f;
};

UENUM()
enum class EThalassaBleep : uint8
{
	On,     // Rising: a station taken, something switched on.
	Off,    // Falling: a station left, something switched off.
	Click,  // A mode changed.
	Alarm,  // Something important: full stop.
};

/** A console's bleeps. */
UCLASS()
class THALASSA_API UThalassaBleepComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UThalassaBleepComponent(const FObjectInitializer& ObjectInitializer);

	/** Game thread: play a bleep. */
	void Bleep(EThalassaBleep Which) { Pending.store(static_cast<int32>(Which) + 1); }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	std::atomic<int32> Pending{ 0 };
	float Rate = 48000.f;
	int32 Kind = -1;
	float Time = 0.f;
	float Phase = 0.f;
};

/**
 * The hull striking something: a heavy thud and crash, the steel ringing for seconds after, and a groan.
 * Heard everywhere aboard.
 */
UCLASS()
class THALASSA_API UThalassaImpactComponent : public USynthComponent
{
	GENERATED_BODY()

public:
	UThalassaImpactComponent(const FObjectInitializer& ObjectInitializer);

	/** Game thread: the hull has hit something. */
	void Strike() { Pending.fetch_add(1); }

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	std::atomic<int32> Pending{ 0 };
	float Rate = 48000.f;
	float Time = -1.f;
	float Phases[6] = {};
	ThalassaAudio::FLowPass CrashFilter;
	ThalassaAudio::FBandPass Groan;
	ThalassaAudio::FNoise Noise;
};
