#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVLevelData.generated.h"

UCLASS(BlueprintType)
class VHV_API UVHVLevelData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// ========================================================
	// LEVEL / UNIT IDENTITY
	// ========================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	int32 LevelNumber = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FText LevelTitle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	FText LevelDescription;

	// ========================================================
	// DAILY JOURNEY
	// ========================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Day")
	FDayData DayData;
};