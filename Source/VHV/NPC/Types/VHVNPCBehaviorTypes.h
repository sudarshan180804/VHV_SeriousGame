#pragma once

#include "CoreMinimal.h"
#include "VHVNPCBehaviorTypes.generated.h"

UENUM(BlueprintType)
enum class EVHVNPCBehaviorState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Moving UMETA(DisplayName = "Moving"),
	Talking UMETA(DisplayName = "Talking"),
	Waiting UMETA(DisplayName = "Waiting"),
	Unavailable UMETA(DisplayName = "Unavailable"),
	Engaging UMETA(DisplayName = "Engaging")
};

UENUM(BlueprintType)
enum class EVHVNPCBehaviorOperation : uint8
{
	None UMETA(Hidden),
	MoveTo UMETA(DisplayName = "Move To"),
	Wait UMETA(DisplayName = "Wait"),
	ReturnToPost UMETA(DisplayName = "Return To Post")
};
