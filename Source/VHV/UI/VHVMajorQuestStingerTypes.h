#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundBase.h"
#include "VHVMajorQuestStingerTypes.generated.h"

/** Reusable presentation data for a major quest or technique introduction. */
USTRUCT(BlueprintType)
struct FVHVMajorQuestStingerData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Major Stinger")
    FString Label;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Major Stinger")
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Major Stinger")
    FString Subtitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Major Stinger")
    TSoftObjectPtr<USoundBase> IntroSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Major Stinger", meta = (ClampMin = "0.5", UIMin = "0.5", UIMax = "4.0"))
    float HoldDuration = 2.1f;

    bool IsConfigured() const { return !Title.TrimStartAndEnd().IsEmpty(); }
};
