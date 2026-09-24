#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Quest/Types/VHVQuestTypes.h"
#include "VHVQuestArcData.generated.h"

class UVHVLevelData;

UCLASS(BlueprintType)
class VHV_API UVHVQuestArcData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Arc")
    FName QuestArcID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Arc")
    FText QuestArcTitle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Arc")
    FText QuestArcDescription;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Arc")
    TSoftObjectPtr<UVHVLevelData> AssociatedLevelData;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Arc")
    TArray<FVHVQuestDefinition> Quests;

    /** Initial semantic destinations for persistent story NPCs. Reapplied when a new arc starts or an earlier save is restored. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Arc|NPC Movement")
    TArray<FVHVQuestNPCMoveRequest> InitialNPCMoves;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest Arc")
    bool bAutoStartNextQuest = true;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
