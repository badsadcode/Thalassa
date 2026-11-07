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

	bool IsHelmTest()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaHelmTest"));
		return bValue;
	}

	bool IsHelmBot()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaHelmBot"));
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

	bool UseTestRig()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaTestRig"));
		return bValue;
	}

	bool GetSpawnOverride(FVector& OutLocation)
	{
		FString Text;
		TArray<FString> Parts;
		if (!FParse::Value(FCommandLine::Get(), TEXT("ThalassaSpawnAt="), Text, /*bShouldStopOnSeparator=*/ false) || Text.ParseIntoArray(Parts, TEXT(",")) != 3)
		{
			return false;
		}
		OutLocation = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
		return true;
	}

	bool IsWalkStraight()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaWalkStraight"));
		return bValue;
	}

	bool IsSonarOnForTest()
	{
		static const bool bValue = FParse::Param(FCommandLine::Get(), TEXT("ThalassaSonar"));
		return bValue;
	}

	bool GetSurveySaveSlot(FString& OutSlot)
	{
		return FParse::Value(FCommandLine::Get(), TEXT("ThalassaSurveySlot="), OutSlot);
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
