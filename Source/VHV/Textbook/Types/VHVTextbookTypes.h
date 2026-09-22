#pragma once

#include "CoreMinimal.h"
#include "VHV/Story/Types/VHVStoryStateTypes.h"
#include "VHVTextbookTypes.generated.h"

// ============================================================
// LEARNING PHASE
// ============================================================

UENUM(BlueprintType)
enum class ELearningPhase : uint8
{
    Ask,
    Hint,
    Feedback,
    Teach
};

// ============================================================
// ACTIVITY TYPE
// ============================================================

UENUM(BlueprintType)
enum class ETextbookActivityType : uint8
{
    SingleChoice,
    MultiChoice,
    Ordering,
    Matching,
    Observation,
    DialogueChoice
};

// ============================================================
// ACTIVITY REFERENCE
// ============================================================

USTRUCT(BlueprintType)
struct FTextbookActivityReference
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Textbook Reference", meta = (DisplayName = "Activity", Categories = "VHV.Activity"))
    FGameplayTag ActivityTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "Textbook Reference", meta = (DisplayName = "Activity ID (Legacy)"))
    FString ActivityID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Textbook Reference")
    FString TopicID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Textbook Reference")
    bool bRequiredForProgress = false;

    FString GetEffectiveActivityID() const { return VHVAuthoringReferences::ResolveStringID(ActivityTag, ActivityID, TEXT("VHV.Activity")); }
};

// ============================================================
// DIALOGUE CHOICE ACTIVITY REFERENCE
// ============================================================

/**
 * Points a textbook activity at a choice node owned by a conversation graph.
 * The conversation remains the single source of truth for the options and
 * their branch targets.
 */
USTRUCT(BlueprintType)
struct FDialogueChoiceActivityReference
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Choice Reference")
    FString ConversationID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Choice Reference")
    FString ChoiceNodeID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Choice Reference")
    bool bRequiredForProgress = false;
};

// ============================================================
// MEDIA TYPE
// ============================================================

UENUM(BlueprintType)
enum class ETextbookMediaType : uint8
{
    Image,
    Video,
    Diagram,
    Animation
};

// ============================================================
// QUESTION OPTION
// ============================================================

USTRUCT(BlueprintType)
struct FQuestionOption
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Question")
    FString OptionText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Question")
    bool bIsCorrect = false;
};

// ============================================================
// QUESTION DATA
// ============================================================

USTRUCT(BlueprintType)
struct FQuestionData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Question")
    FString QuestionID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Question")
    FString QuestionText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Question")
    TArray<FQuestionOption> Options;
};

// ============================================================
// MEDIA REFERENCE
// ============================================================

USTRUCT(BlueprintType)
struct FTextbookMediaReference
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
    FString MediaID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
    ETextbookMediaType MediaType = ETextbookMediaType::Image;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
    FSoftObjectPath Asset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media")
    bool bRequired = false;
};

// ============================================================
// ORDERING ITEM
// ============================================================

USTRUCT(BlueprintType)
struct FOrderingItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ordering")
    FString ItemID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ordering")
    FString ItemText;
};

// ============================================================
// MATCHING PAIR
// ============================================================

USTRUCT(BlueprintType)
struct FMatchingPair
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matching")
    FString LeftText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matching")
    FString RightText;
};

// ============================================================
// TEACHING CONTENT
// ============================================================

UENUM(BlueprintType)
enum class ETextbookTeachingCategory : uint8
{
    Lesson UMETA(DisplayName = "Lesson"),
    KeyIdea UMETA(DisplayName = "Key Idea"),
    Technique UMETA(DisplayName = "Technique"),
    Reflection UMETA(DisplayName = "Reflection")
};

USTRUCT(BlueprintType)
struct FTeachingContent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teaching")
    ETextbookTeachingCategory Category = ETextbookTeachingCategory::Lesson;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teaching")
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teaching")
    FString Content;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teaching")
    TArray<FString> KeyTakeaways;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teaching")
    TArray<FTextbookMediaReference> Media;
};

USTRUCT(BlueprintType)
struct FTextbookAttemptPolicy
{
    GENERATED_BODY()

    /** Enables retry behavior for assessed SingleChoice and MultiChoice activities. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attempts")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attempts", meta = (ClampMin = "1", EditCondition = "bEnabled"))
    int32 MaxAttempts = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attempts", meta = (MultiLine = "true", EditCondition = "bEnabled"))
    FString FirstIncorrectHint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attempts", meta = (MultiLine = "true", EditCondition = "bEnabled"))
    FString FinalIncorrectExplanation;
};

// ============================================================
// ACTIVITY DATA
// ============================================================

USTRUCT(BlueprintType)
struct FTextbookActivityData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity", meta = (DisplayName = "Activity", Categories = "VHV.Activity"))
    FGameplayTag ActivityTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "Activity", meta = (DisplayName = "Activity ID (Legacy)"))
    FString ActivityID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
    FString ActivityTitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
    ETextbookActivityType ActivityType = ETextbookActivityType::SingleChoice;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
    FString NarrativeContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity")
    FString PromptText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activity", meta = (EditCondition = "ActivityType == ETextbookActivityType::SingleChoice || ActivityType == ETextbookActivityType::MultiChoice", EditConditionHides))
    FQuestionData Question;

    /** Used only when ActivityType is DialogueChoice. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Choice", meta = (EditCondition = "ActivityType == ETextbookActivityType::DialogueChoice", EditConditionHides))
    FDialogueChoiceActivityReference DialogueChoice;

    // --------------------------------------------------------
    // Ordering
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ordering", meta = (EditCondition = "ActivityType == ETextbookActivityType::Ordering", EditConditionHides))
    TArray<FOrderingItem> OrderingItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ordering", meta = (EditCondition = "ActivityType == ETextbookActivityType::Ordering", EditConditionHides))
    TArray<FString> CorrectOrder;

    // --------------------------------------------------------
    // Matching
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Matching", meta = (EditCondition = "ActivityType == ETextbookActivityType::Matching", EditConditionHides))
    TArray<FMatchingPair> MatchingPairs;

    // --------------------------------------------------------
    // Feedback
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    FString CorrectFeedback;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    FString IncorrectFeedback;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    FString PartialFeedback;

    // --------------------------------------------------------
    // Teaching
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Teaching")
    FTeachingContent Teaching;

    // --------------------------------------------------------
    // Hints
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hints")
    TArray<FString> Hints;

    // --------------------------------------------------------
    // Activity Media
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Media", meta = (EditCondition = "ActivityType == ETextbookActivityType::Observation", EditConditionHides))
    TArray<FTextbookMediaReference> Media;

    // --------------------------------------------------------
    // Progression
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
    bool bRequireCorrectAnswerToAdvance = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression", meta = (EditCondition = "ActivityType == ETextbookActivityType::SingleChoice || ActivityType == ETextbookActivityType::MultiChoice", EditConditionHides))
    FTextbookAttemptPolicy AttemptPolicy;

    // --------------------------------------------------------
    // Story State Effects
    // --------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State")
    TArray<FVHVStoryEffect> SuccessEffects;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State")
    TArray<FVHVStoryEffect> FailureEffects;

    FString GetEffectiveActivityID() const { return VHVAuthoringReferences::ResolveStringID(ActivityTag, ActivityID, TEXT("VHV.Activity")); }
};

// ============================================================
// TOPIC DATA
// ============================================================

USTRUCT(BlueprintType)
struct FTopicData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topic")
    FString TopicID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topic")
    FString TopicTitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topic")
    FString NarrativeIntroduction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Topic")
    TArray<FTextbookActivityData> Activities;
};

// ============================================================
// DAY / UNIT DATA
// ============================================================

USTRUCT(BlueprintType)
struct FDayData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
    int32 DayNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
    FString DayTitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
    FString NarrativeRole;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
    FString IntroductionText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
    TArray<FTopicData> Topics;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
    FString CompletionSummary;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
    TArray<FString> CompletionTakeaways;
};
