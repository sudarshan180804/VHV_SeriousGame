#pragma once

#include "CoreMinimal.h"
#include "VHVWorldActionTypes.generated.h"

UENUM(BlueprintType)
enum class EVHVWorldActionExecutionResult : uint8
{
    Rejected,
    Completed,
    StartedAsync
};

USTRUCT(BlueprintType)
struct VHV_API FVHVWorldActionRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "World Action")
    FGuid RequestID;

    UPROPERTY(BlueprintReadOnly, Category = "World Action")
    FName ReceiverID;

    UPROPERTY(BlueprintReadOnly, Category = "World Action")
    FName ActionID;
};
