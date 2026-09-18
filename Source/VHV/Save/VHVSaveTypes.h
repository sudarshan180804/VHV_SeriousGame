#pragma once

#include "CoreMinimal.h"
#include "Quest/Types/VHVQuestTypes.h"
#include "Textbook/Types/VHVTextbookRuntimeTypes.h"
#include "VHVSaveTypes.generated.h"

USTRUCT(BlueprintType)
struct VHV_API FVHVStoryStateSaveState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame)
    TArray<FName> SetFlagIDs;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    TMap<FName, int32> CounterValues;
};

USTRUCT(BlueprintType)
struct VHV_API FVHVQuestSaveEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FName QuestID;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    EVHVQuestStatus Status = EVHVQuestStatus::Locked;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FName CurrentObjectiveID;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    TArray<FName> CompletedObjectiveIDs;
};

USTRUCT(BlueprintType)
struct VHV_API FVHVQuestSaveState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame)
    bool bHasState = false;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FSoftObjectPath QuestArcAssetPath;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FName QuestArcID;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FName ActiveQuestID;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FName TrackedQuestID;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    TArray<FVHVQuestSaveEntry> QuestStates;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    TArray<FName> CompletedQuestIDs;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    bool bArcActive = false;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    bool bArcCompleted = false;
};

USTRUCT(BlueprintType)
struct VHV_API FVHVTextbookSaveState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame)
    bool bHasState = false;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FSoftObjectPath LevelDataAssetPath;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FString CurrentTopicID;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FString CurrentActivityID;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    TArray<FString> CompletedActivityIDs;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    TArray<FString> MasteredTopicIDs;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    EVHVTextbookProgressionMode ProgressionMode = EVHVTextbookProgressionMode::InternalTextbook;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    bool bJourneyStarted = false;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    bool bDayStarted = false;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    bool bDayCompleted = false;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    bool bTopicCompleted = false;
};

USTRUCT(BlueprintType)
struct VHV_API FVHVDialogueCheckpointSaveState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FString ConversationID;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FString LatestCheckpointID;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    FString CurrentNodeID;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    bool bCompleted = false;
};
