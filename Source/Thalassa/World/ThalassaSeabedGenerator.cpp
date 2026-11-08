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

double FThalassaSeabedGenerator::Cones(double XM, double YM) const
{
	// A 5 km lattice; most cells hold a small volcanic cone, some with a crater at the top.
	constexpr double CellM = 5000.0;
	const int64 CX = static_cast<int64>(FMath::FloorToDouble(XM / CellM));
	const int64 CY = static_cast<int64>(FMath::FloorToDouble(YM / CellM));
	double Height = 0.0;
	for (int64 DY = -1; DY <= 1; ++DY)
	{
		for (int64 DX = -1; DX <= 1; ++DX)
		{
			const uint64 H = HashCell(CX + DX, CY + DY, Seed ^ 0x51ED27A9F00Dull);
			if (Unit(H) > 0.7)
			{
				continue;
			}
			const double PX = (CX + DX + 0.2 + 0.6 * Unit(Mix(H ^ 1))) * CellM;
			const double PY = (CY + DY + 0.2 + 0.6 * Unit(Mix(H ^ 2))) * CellM;
			const double Radius = 500.0 + 1300.0 * Unit(Mix(H ^ 3));
			const double Rise = 120.0 + 480.0 * Unit(Mix(H ^ 4));
			const double D2 = ((XM - PX) * (XM - PX) + (YM - PY) * (YM - PY)) / (Radius * Radius);
			if (D2 < 1.0)
			{
				double Cone = (1.0 - D2) * (1.0 - D2) * (1.0 - D2);
				if (Unit(Mix(H ^ 5)) < 0.5 && D2 < 0.04)
				{
					Cone -= 0.35 * (1.0 - D2 / 0.04) * (1.0 - D2 / 0.04); // A crater at the summit.
				}
				Height += Rise * Cone;
			}
		}
	}
	return Height;
}

FThalassaSeabedGenerator::EProvince FThalassaSeabedGenerator::ProvinceAt(double XM, double YM, EProvince& OutNeighbour, double& OutWeight) const
{
	// Provinces are the cells of a jittered 40 km lattice: each point belongs to the nearest cell centre.
	constexpr double CellM = 40000.0;
	constexpr double BlendM = 5000.0;
	const int64 CX = static_cast<int64>(FMath::FloorToDouble(XM / CellM));
	const int64 CY = static_cast<int64>(FMath::FloorToDouble(YM / CellM));
	double Best = 1e30, Second = 1e30;
	uint64 BestHash = 0, SecondHash = 0;
	for (int64 DY = -1; DY <= 1; ++DY)
	{
		for (int64 DX = -1; DX <= 1; ++DX)
		{
			const uint64 H = HashCell(CX + DX, CY + DY, Seed ^ 0x9B0B1A57E5ull);
			const double PX = (CX + DX + 0.1 + 0.8 * Unit(Mix(H ^ 7))) * CellM;
			const double PY = (CY + DY + 0.1 + 0.8 * Unit(Mix(H ^ 8))) * CellM;
			const double D = FMath::Sqrt((XM - PX) * (XM - PX) + (YM - PY) * (YM - PY));
			if (D < Best)
			{
				Second = Best; SecondHash = BestHash;
				Best = D; BestHash = H;
			}
			else if (D < Second)
			{
				Second = D; SecondHash = H;
			}
		}
	}
	const auto Kind = [](uint64 H) { return static_cast<EProvince>((H >> 40) % static_cast<uint64>(EProvince::Count)); };
	// The distance to the border between two cells is half the difference of the distances to their centres.
	OutWeight = 0.5 + 0.5 * SmoothStep(0.0, BlendM, (Second - Best) * 0.5);
	OutNeighbour = Kind(SecondHash);
	return Kind(BestHash);
}

const TCHAR* FThalassaSeabedGenerator::ProvinceName(EProvince Province)
{
	switch (Province)
	{
	case EProvince::Plains: return TEXT("plains");
	case EProvince::Hills: return TEXT("hills");
	case EProvince::Plateaus: return TEXT("plateaus");
	case EProvince::Ridges: return TEXT("ridges");
	case EProvince::Canyons: return TEXT("canyons");
	case EProvince::Volcanic: return TEXT("volcanic");
	default: return TEXT("?");
	}
}

namespace ThalassaSeabed
{
	/** What a province does to the ground. Blended between neighbouring provinces at their borders. */
	struct FCharacter
	{
		double Hills = 0.0;      // Amplitude of broad hills, metres.
		double Ridges = 0.0;     // Amplitude of local ridgelines, metres.
		double Canyons = 0.0;    // Depth of canyons, metres.
		double Terraces = 0.0;   // 1: the ground breaks into terraces and risers.
		double Outcrops = 0.0;   // Amplitude of craggy outcrops, metres.
		double Roughness = 0.0;  // Amplitude of fine detail, metres.
		double Cones = 0.0;      // 1: volcanic cones.
	};

	FCharacter CharacterOf(FThalassaSeabedGenerator::EProvince Province)
	{
		using P = FThalassaSeabedGenerator::EProvince;
		FCharacter C;
		switch (Province)
		{
		case P::Plains:   C.Hills = 25.0;  C.Canyons = 60.0;  C.Roughness = 5.0;  break;
		case P::Hills:    C.Hills = 260.0; C.Canyons = 120.0; C.Outcrops = 7.0;  C.Roughness = 14.0; break;
		case P::Plateaus: C.Hills = 90.0;  C.Canyons = 150.0; C.Terraces = 1.0;  C.Outcrops = 9.0; C.Roughness = 10.0; break;
		case P::Ridges:   C.Hills = 80.0;  C.Ridges = 350.0;  C.Outcrops = 12.0; C.Roughness = 16.0; break;
		case P::Canyons:  C.Hills = 70.0;  C.Canyons = 700.0; C.Outcrops = 8.0;  C.Roughness = 12.0; break;
		case P::Volcanic: C.Hills = 60.0;  C.Canyons = 80.0;  C.Cones = 1.0;     C.Outcrops = 10.0; C.Roughness = 14.0; break;
		default: break;
		}
		return C;
	}

	FCharacter Blend(const FCharacter& A, const FCharacter& B, double T)
	{
		FCharacter C;
		C.Hills = A.Hills + (B.Hills - A.Hills) * T;
		C.Ridges = A.Ridges + (B.Ridges - A.Ridges) * T;
		C.Canyons = A.Canyons + (B.Canyons - A.Canyons) * T;
		C.Terraces = A.Terraces + (B.Terraces - A.Terraces) * T;
		C.Outcrops = A.Outcrops + (B.Outcrops - A.Outcrops) * T;
		C.Roughness = A.Roughness + (B.Roughness - A.Roughness) * T;
		C.Cones = A.Cones + (B.Cones - A.Cones) * T;
		return C;
	}
}

double FThalassaSeabedGenerator::HeightM(double XM, double YM, double MinWavelengthM) const
{
	// Warp the ground a little, so nothing lines up with the noise grid.
	const double WX = XM + 3000.0 * Fractal(XM, YM, 30000.0, 3, MinWavelengthM, LayerWarpX);
	const double WY = YM + 3000.0 * Fractal(XM, YM, 30000.0, 3, MinWavelengthM, LayerWarpY);

	// Basins and shelves: the broad shape of the ocean. Around the start there is always a continental shelf,
	// so a new crew can reach the bottom; the abyss lies beyond it.
	const double FromStart = FMath::Sqrt(XM * XM + YM * YM);
	const double Basin = Fractal(WX, WY, 160000.0, 4, MinWavelengthM, LayerBasin) + 0.5 * SmoothStep(30000.0, 8000.0, FromStart);
	const double Shelf = SmoothStep(-0.15, 0.45, Basin);
	double Height = -2100.0 + 1650.0 * Shelf; // About -2100 m in the abyss, -450 m on the shelf.

	// Each province has its own character, blended with its neighbour's near the border.
	EProvince Neighbour;
	double Weight;
	const EProvince Province = ProvinceAt(WX, WY, Neighbour, Weight);
	const FCharacter Look = Blend(CharacterOf(Neighbour), CharacterOf(Province), Weight);

	// Mid-ocean ridges, rarer on the shelf, and the province's own ridgelines.
	const double Ridge = Ridged(WX, WY, 26000.0, 5, MinWavelengthM, LayerRidge);
	Height += 1100.0 * Ridge * Ridge * (1.0 - 0.6 * Shelf);
	Height += Look.Ridges * Ridged(WX, WY, 4500.0, 4, MinWavelengthM, LayerRidge + 40);

	// Hills.
	Height += Look.Hills * Fractal(WX, WY, 3000.0, 4, MinWavelengthM, LayerHills);

	// Plateaus: the ground breaks into flat terraces with steep risers. Each step is about 90 m.
	if (Look.Terraces > 0.0)
	{
		constexpr double StepM = 90.0;
		const double Level = Height / StepM;
		const double Floor = FMath::FloorToDouble(Level);
		const double Rise = SmoothStep(0.72, 0.98, Level - Floor);
		Height += Look.Terraces * ((Floor + Rise) * StepM - Height);
	}

	// Canyons: winding cuts with tributaries.
	const double Canyon = 1.0 - FMath::Abs(Fractal(WX, WY, 9000.0, 3, MinWavelengthM, LayerCanyon));
	const double Tributary = 1.0 - FMath::Abs(Fractal(WX, WY, 2600.0, 3, MinWavelengthM, LayerCanyon + 40));
	const double Cut = FMath::Max(Canyon * Canyon * Canyon * Canyon * Canyon * Canyon, 0.45 * Tributary * Tributary * Tributary * Tributary * Tributary * Tributary * Tributary * Tributary);
	Height -= Look.Canyons * Cut;

	// Seamounts everywhere, rarely; volcanic cones where the province is volcanic.
	Height += Seamounts(XM, YM);
	if (Look.Cones > 0.0)
	{
		Height += Look.Cones * Cones(WX, WY);
	}

	// What the floodlights see: craggy outcrops where the ground is rugged, then fine detail.
	const double Crag = Ridged(XM, YM, 140.0, 3, MinWavelengthM, LayerDetail + 40);
	Height += Look.Outcrops * Crag * Crag * Crag * 3.0;
	Height += Look.Roughness * Fractal(XM, YM, 400.0, 4, MinWavelengthM, LayerDetail);
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
