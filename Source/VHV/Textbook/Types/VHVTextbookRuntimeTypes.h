#pragma once

#include "CoreMinimal.h"
#include "VHVTextbookTypes.h"
#include "VHVTextbookRuntimeTypes.generated.h"

UENUM(BlueprintType)
enum class EVHVTextbookProgressionMode : uint8
{
    InternalTextbook,
    QuestManaged
};

// ============================================================
// TEXTBOOK RUNTIME STATE
// ============================================================

USTRUCT(BlueprintType)
struct FTextbookRuntimeState
{
    GENERATED_BODY()

    // --------------------------------------------------------
    // Current Journey Position
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journey")
    int32 CurrentDayIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journey")
    int32 CurrentTopicIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journey")
    int32 CurrentActivityIndex = 0;

    // --------------------------------------------------------
    // Current Learning Phase
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Learning")
    ELearningPhase CurrentPhase = ELearningPhase::Ask;

    // --------------------------------------------------------
    // Journey State
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journey")
    bool bJourneyStarted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journey")
    bool bDayStarted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journey")
    bool bDayCompleted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journey")
    bool bTopicCompleted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Journey")
    bool bActivityCompleted = false;

    // --------------------------------------------------------
    // Activity State
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
    int32 AttemptCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
    int32 HintLevel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
    bool bAnswerSubmitted = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
    bool bAnswerCorrect = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
    bool bAnswerPartial = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
    bool bActivityActive = false;

    // --------------------------------------------------------
    // Progress Tracking
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progress")
    TSet<FString> CompletedActivityIDs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progress")
    TSet<FString> MasteredTopicIDs;
};
