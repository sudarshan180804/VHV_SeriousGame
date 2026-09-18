#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "VHV/Story/Types/VHVStoryStateTypes.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVDialogueTypes.generated.h"

UENUM(BlueprintType)
enum class EVHVDialogueNodeType : uint8
{
    Text,
    Choice,
    LearningActivity,
    Unknown
};

USTRUCT(BlueprintType)
struct VHV_API FDialogueChoiceOption
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString OptionID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FText OptionText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString NextNodeID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    bool bIsDefault = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State")
    FVHVStoryConditionSet AvailabilityConditions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State")
    TArray<FVHVStoryEffect> SelectionEffects;
};

USTRUCT(BlueprintType)
struct VHV_API FDialogueNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString NodeID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    EVHVDialogueNodeType NodeType = EVHVDialogueNodeType::Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString SpeakerID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FText SpeakerName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FText Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    TArray<FDialogueChoiceOption> Choices;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString NextNodeID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    bool bIsCheckpoint = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString CheckpointID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Learning")
    FTextbookActivityReference LinkedActivity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State")
    FVHVStoryConditionSet ActivationConditions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State")
    TArray<FVHVStoryEffect> CompletionEffects;
};

USTRUCT(BlueprintType)
struct VHV_API FDialogueConversation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString ConversationID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString StartNodeID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    TArray<FDialogueNode> Nodes;
};

USTRUCT(BlueprintType)
struct VHV_API FConversationRuntimeState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString ConversationID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString LatestCheckpointID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    bool bCompleted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString CurrentNodeID;
};

USTRUCT(BlueprintType)
struct VHV_API FDialogueLine
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString SpeakerID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FText SpeakerName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FText Text;
};

USTRUCT(BlueprintType)
struct VHV_API FDialogueData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    FString DialogueID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
    TArray<FDialogueLine> Lines;
};
