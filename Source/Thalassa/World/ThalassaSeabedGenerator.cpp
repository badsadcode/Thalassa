#include "World/ThalassaSeabedGenerator.h"

namespace ThalassaSeabed
{
	/** SplitMix64: a fast, well-mixed integer hash. The only source of randomness in the ocean. */
	FORCEINLINE uint64 Mix(uint64 X)
	{
		X += 0x9E3779B97F4A7C15ull;
		X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
		X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
		return X ^ (X >> 31);
	}

	FORCEINLINE uint64 HashCell(int64 X, int64 Y, uint64 Salt)
	{
		return Mix(Mix(static_cast<uint64>(X) ^ (Salt * 0xD6E8FEB86659FD93ull)) ^ (static_cast<uint64>(Y) * 0xA0761D6478BD642Full));
	}

	/** A uniform number in 0..1 from a hash. */
	FORCEINLINE double Unit(uint64 H) { return static_cast<double>(H >> 11) * (1.0 / 9007199254740992.0); }

	/** Quintic fade: smooth, and polynomial. */
	FORCEINLINE double Fade(double T) { return T * T * T * (T * (T * 6.0 - 15.0) + 10.0); }

	FORCEINLINE double SmoothStep(double Edge0, double Edge1, double X)
	{
		const double T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	/** Eight gradient directions, the same everywhere: no trigonometry needed. */
	constexpr double GX[8] = { 1.0, -1.0, 0.0, 0.0, 0.70710678118654752, -0.70710678118654752, 0.70710678118654752, -0.70710678118654752 };
	constexpr double GY[8] = { 0.0, 0.0, 1.0, -1.0, 0.70710678118654752, 0.70710678118654752, -0.70710678118654752, -0.70710678118654752 };

	/** Layers: each kind of feature draws from its own independent noise. */
	enum : uint64 { LayerBasin = 1, LayerWarpX, LayerWarpY, LayerRidge, LayerCanyon, LayerHills, LayerDetail, LayerSeamount, LayerGrit };
}

using namespace ThalassaSeabed;

FThalassaSeabedGenerator::FThalassaSeabedGenerator(uint64 InSeed)
	: Seed(Mix(InSeed))
{
}

double FThalassaSeabedGenerator::Noise(double X, double Y, uint64 Layer) const
{
	const double FloorX = FMath::FloorToDouble(X), FloorY = FMath::FloorToDouble(Y);
	const int64 CX = static_cast<int64>(FloorX), CY = static_cast<int64>(FloorY);
	const double FX = X - FloorX, FY = Y - FloorY;
	const uint64 Salt = Seed ^ (Layer * 0x9E3779B97F4A7C15ull);

	double Corner[4];
	for (int32 I = 0; I < 4; ++I)
	{
		const int64 DX = I & 1, DY = I >> 1;
		const uint64 H = HashCell(CX + DX, CY + DY, Salt);
		const int32 G = static_cast<int32>(H & 7);
		Corner[I] = GX[G] * (FX - DX) + GY[G] * (FY - DY);
	}
	const double U = Fade(FX), V = Fade(FY);
	const double Bottom = Corner[0] + (Corner[1] - Corner[0]) * U;
	const double Top = Corner[2] + (Corner[3] - Corner[2]) * U;
	return (Bottom + (Top - Bottom) * V) * 1.41421356237309505; // Scaled to roughly -1..1.
}

double FThalassaSeabedGenerator::Fractal(double XM, double YM, double WavelengthM, int32 Octaves, double MinWavelengthM, uint64 Layer) const
{
	double Sum = 0.0, Amplitude = 1.0, Wavelength = WavelengthM;
	for (int32 Octave = 0; Octave < Octaves; ++Octave)
	{
		// Octaves too fine for the caller are left out; the one at the threshold fades, so detail never pops.
		const double Keep = MinWavelengthM > 0.0 ? FMath::Clamp((Wavelength - MinWavelengthM) / MinWavelengthM, 0.0, 1.0) : 1.0;
		if (Keep <= 0.0)
		{
			break;
		}
		Sum += Amplitude * Keep * Noise(XM / Wavelength, YM / Wavelength, Layer * 31 + Octave);
		Amplitude *= 0.5;
		Wavelength *= 0.5;
	}
	// Normalised by the full set of octaves, so leaving fine ones out never changes the large shapes.
	double Full = 0.0, A = 1.0;
	for (int32 Octave = 0; Octave < Octaves; ++Octave, A *= 0.5)
	{
		Full += A;
	}
	return Sum / Full;
}

double FThalassaSeabedGenerator::Ridged(double XM, double YM, double WavelengthM, int32 Octaves, double MinWavelengthM, uint64 Layer) const
{
	double Sum = 0.0, Amplitude = 1.0, Full = 0.0, Wavelength = WavelengthM, Weight = 1.0;
	for (int32 Octave = 0; Octave < Octaves; ++Octave)
	{
		Full += Amplitude;
		const double Keep = MinWavelengthM > 0.0 ? FMath::Clamp((Wavelength - MinWavelengthM) / MinWavelengthM, 0.0, 1.0) : 1.0;
		if (Keep > 0.0)
		{
			double R = 1.0 - FMath::Abs(Noise(XM / Wavelength, YM / Wavelength, Layer * 31 + Octave));
			R = R * R * Weight;
			Weight = FMath::Clamp(R * 1.5, 0.0, 1.0); // Finer crests follow the big ones.
			Sum += Amplitude * Keep * R;
		}
		Amplitude *= 0.5;
		Wavelength *= 0.5;
	}
	return Sum / Full;
}

double FThalassaSeabedGenerator::Seamounts(double XM, double YM) const
{
	// One lattice cell of 24 km may hold a seamount; look in this cell and its neighbours.
	constexpr double CellM = 24000.0;
	const int64 CX = static_cast<int64>(FMath::FloorToDouble(XM / CellM));
	const int64 CY = static_cast<int64>(FMath::FloorToDouble(YM / CellM));
	double Height = 0.0;
	for (int64 DY = -1; DY <= 1; ++DY)
	{
		for (int64 DX = -1; DX <= 1; ++DX)
		{
			const uint64 H = HashCell(CX + DX, CY + DY, Seed ^ (LayerSeamount * 0x9E3779B97F4A7C15ull));
			if (Unit(H) > 0.55)
			{
				continue; // Most cells have none: they are landmarks, not wallpaper.
			}
			const double PX = (CX + DX + 0.15 + 0.7 * Unit(Mix(H ^ 1))) * CellM;
			const double PY = (CY + DY + 0.15 + 0.7 * Unit(Mix(H ^ 2))) * CellM;
			const double Radius = 1800.0 + 3500.0 * Unit(Mix(H ^ 3));
			const double Rise = 500.0 + 1700.0 * Unit(Mix(H ^ 4));
			const double D2 = ((XM - PX) * (XM - PX) + (YM - PY) * (YM - PY)) / (Radius * Radius);
			if (D2 < 1.0)
			{
				const double Falloff = (1.0 - D2) * (1.0 - D2) * (1.0 - D2); // A smooth cone, polynomial.
				Height += Rise * Falloff;
			}
		}
	}
	return Height;
}

double FThalassaSeabedGenerator::HeightM(double XM, double YM, double MinWavelengthM) const
{
	// Warp the ground a little, so nothing lines up with the noise grid.
	const double WX = XM + 3000.0 * Fractal(XM, YM, 30000.0, 3, MinWavelengthM, LayerWarpX);
	const double WY = YM + 3000.0 * Fractal(XM, YM, 30000.0, 3, MinWavelengthM, LayerWarpY);

	// Basins and shelves: the broad shape of the ocean, from abyssal plain to continental shelf.
	const double Basin = Fractal(WX, WY, 160000.0, 4, MinWavelengthM, LayerBasin);
	const double Shelf = SmoothStep(-0.15, 0.45, Basin);
	double Height = -2400.0 + 2000.0 * Shelf; // -2400 m in the abyss, -400 m on the shelf.

	// Mid-ocean ridges: long, sharp-crested ranges rising out of the deep, rarer on the shelf.
	const double Ridge = Ridged(WX, WY, 26000.0, 5, MinWavelengthM, LayerRidge);
	Height += 1300.0 * Ridge * Ridge * (1.0 - 0.6 * Shelf);

	// Canyons: winding cuts down the shelf's edge and across it.
	const double Canyon = 1.0 - FMath::Abs(Fractal(WX, WY, 9000.0, 3, MinWavelengthM, LayerCanyon));
	const double Cut = Canyon * Canyon * Canyon * Canyon * Canyon * Canyon;
	Height -= 380.0 * Cut * SmoothStep(0.05, 0.6, Shelf);

	// Seamounts, standing alone.
	Height += Seamounts(XM, YM);

	// Rolling hills, then the fine detail you see under the floodlights.
	Height += 90.0 * Fractal(WX, WY, 3500.0, 4, MinWavelengthM, LayerHills);
	Height += 14.0 * Fractal(XM, YM, 400.0, 4, MinWavelengthM, LayerDetail);
	Height += 1.5 * Fractal(XM, YM, 30.0, 3, MinWavelengthM, LayerGrit);

	// Nothing breaks the surface: peaks flatten out as they near it.
	if (Height > ShallowestM - 200.0)
	{
		const double Over = Height - (ShallowestM - 200.0);
		Height = ShallowestM - 200.0 * 200.0 / (200.0 + Over);
	}
	return Height;
}

uint64 FThalassaSeabedGenerator::Fingerprint() const
{
	// A coarse grid across 400 km, rounded to the centimetre: any change to the ocean changes this.
	uint64 Sum = 0;
	for (int32 J = -20; J <= 20; ++J)
	{
		for (int32 I = -20; I <= 20; ++I)
		{
			const double Height = HeightM(I * 10007.0, J * 9973.0, 0.0);
			Sum = Mix(Sum ^ static_cast<uint64>(static_cast<int64>(FMath::RoundToDouble(Height * 100.0))));
		}
	}
	return Sum;
}
