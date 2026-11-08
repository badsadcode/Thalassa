#pragma once

#include "CoreMinimal.h"

/**
 * The ocean floor as a pure function: give it a seed and a point, get the seabed's height there. Every machine
 * that knows the seed builds the identical ocean, so nothing about the terrain is ever sent over the network.
 * See docs/OCEAN.md.
 *
 * Determinism rules, which every change here must keep:
 *  - doubles throughout: the ocean is far wider than floats can address to the centimetre;
 *  - noise from integer hashing only, and polynomial smoothing only: no sin, cos, exp or pow, whose results
 *    differ between platforms and compilers (sqrt is fine: it is exactly rounded everywhere);
 *  - any change to the shapes it makes bumps Revision, and the fingerprint test is updated on purpose.
 *
 * Units: metres and world X/Y in metres; heights negative below the surface (0).
 */
class THALASSA_API FThalassaSeabedGenerator
{
public:
	/** Bump whenever the generator's output changes. Saved worlds record it. */
	static constexpr int32 Revision = 1;

	explicit FThalassaSeabedGenerator(uint64 InSeed);

	/**
	 * The seabed's height in metres at world X and Y, in metres. Features shorter than MinWavelengthM are left out,
	 * so a distant, coarse tile is the same ground with its small detail missing, not a resampled copy.
	 */
	double HeightM(double XM, double YM, double MinWavelengthM = 0.0) const;

	/** A number that changes if any height on a fixed grid of samples changes. For determinism tests. */
	uint64 Fingerprint() const;

	uint64 GetSeed() const { return Seed; }

	/** Nothing comes nearer the surface than this. */
	static constexpr double ShallowestM = -60.0;

private:
	/** Gradient noise in -1..1 at a point measured in noise cells, for one of several independent layers. */
	double Noise(double X, double Y, uint64 Layer) const;

	/** Layered noise: octaves from Wavelength down, skipping those shorter than MinWavelength. Roughly -1..1. */
	double Fractal(double XM, double YM, double WavelengthM, int32 Octaves, double MinWavelengthM, uint64 Layer) const;

	/** Ridged layered noise: sharp crests, broad valleys. 0..1. */
	double Ridged(double XM, double YM, double WavelengthM, int32 Octaves, double MinWavelengthM, uint64 Layer) const;

	/** Seamounts: isolated undersea mountains, at most one per lattice cell. Height in metres added. */
	double Seamounts(double XM, double YM) const;

	uint64 Seed;
};
