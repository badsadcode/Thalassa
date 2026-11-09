#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

/**
 * Game-thread timing for finding hitches, with -ThalassaPerf on the command line: every two seconds the log
 * gets the worst frame and the worst time spent in each timed piece of work in that window. Costs nothing
 * measurable when off. Use THALASSA_PERF_SCOPE(Name) at the top of a function to time it.
 */
namespace ThalassaPerf
{
	inline bool IsOn()
	{
		static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("ThalassaPerf"));
		return bOn;
	}

	struct FWindow
	{
		TMap<FString, double> WorstMs;
		TMap<FString, int32> Count;
		double WorstFrameMs = 0.0;
		double Elapsed = 0.0;
	};

	inline FWindow& Window()
	{
		static FWindow Instance;
		return Instance;
	}

	struct FScope
	{
		const TCHAR* Name;
		double Start;
		explicit FScope(const TCHAR* InName) : Name(InName), Start(IsOn() ? FPlatformTime::Seconds() : 0.0) {}
		~FScope()
		{
			if (IsOn())
			{
				const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
				double& Worst = Window().WorstMs.FindOrAdd(Name);
				Worst = FMath::Max(Worst, Ms);
				++Window().Count.FindOrAdd(Name);
			}
		}
	};

	/** Once per frame, from the game thread. */
	inline void Frame(float DeltaSeconds)
	{
		if (!IsOn())
		{
			return;
		}
		FWindow& W = Window();
		W.WorstFrameMs = FMath::Max(W.WorstFrameMs, DeltaSeconds * 1000.0);
		W.Elapsed += DeltaSeconds;
		if (W.Elapsed >= 2.0)
		{
			FString Line = FString::Printf(TEXT("THALASSAPERF worst frame %.1f ms"), W.WorstFrameMs);
			for (const TPair<FString, double>& Pair : W.WorstMs)
			{
				Line += FString::Printf(TEXT(" | %s %.2f ms x%d"), *Pair.Key, Pair.Value, W.Count.FindRef(Pair.Key));
			}
			UE_LOG(LogTemp, Display, TEXT("%s"), *Line);
			W = FWindow();
		}
	}
}

#define THALASSA_PERF_SCOPE(Name) ThalassaPerf::FScope UE_JOIN(ThalassaPerfScope, __LINE__)(TEXT(Name))
