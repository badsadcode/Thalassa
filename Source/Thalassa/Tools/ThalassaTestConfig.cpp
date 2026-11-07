#include "Tools/ThalassaTestConfig.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace ThalassaTestConfig
{
	bool IsBot()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaBot"));
		return bValue;
	}

	bool IsDiverBot()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaDiver"));
		return bValue;
	}

	bool IsEvaProfile()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaEvaProfile"));
		return bValue;
	}

	bool IsSpawnAtAirlock()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaSpawnAtAirlock"));
		return bValue;
	}

	float HullStretch()
	{
		static const float Value = []
		{
			float Stretch = 1.f;
			FParse::Value(FCommandLine::Get(), TEXT("ThalassaHullStretch="), Stretch);
			return Stretch;
		}();
		return Value;
	}

	bool IsBotJumpDisabled()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaNoJump"));
		return bValue;
	}

	bool IsCrewNudgeDisabled()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaNoNudge"));
		return bValue;
	}

	bool IsSpawnTogether()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaSpawnTogether"));
		return bValue;
	}

	bool UseShipGravity()
	{
		static const bool bValue = []
		{
			FString Mode;
			return FParse::Value(FCommandLine::Get(), TEXT("ThalassaGravity="), Mode) && Mode == TEXT("ship");
		}();
		return bValue;
	}

	bool IsShipStatic()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaStaticShip"));
		return bValue;
	}

	int32 ExpectedPlayers()
	{
		int32 Value = 0;
		FParse::Value(FCommandLine::Get(), TEXT("ThalassaExpectPlayers="), Value);
		return Value;
	}

	bool GetTelemetryPath(FString& OutPath)
	{
		return FParse::Value(FCommandLine::Get(), TEXT("ThalassaTelemetry="), OutPath);
	}

	bool GetScreenshotPath(FString& OutPath)
	{
		return FParse::Value(FCommandLine::Get(), TEXT("ThalassaScreenshot="), OutPath);
	}

	float LookYaw()
	{
		float Value = 0.f;
		FParse::Value(FCommandLine::Get(), TEXT("ThalassaLookYaw="), Value);
		return Value;
	}

	double TimeoutSeconds()
	{
		double Value = 600.0;
		FParse::Value(FCommandLine::Get(), TEXT("ThalassaTimeout="), Value);
		return Value;
	}
}
