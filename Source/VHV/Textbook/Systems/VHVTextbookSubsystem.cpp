#include "VHVTextbookSubsystem.h"

#include "Save/VHVSaveTypes.h"
#include "VHV/Story/Systems/VHVStoryStateSubsystem.h"

void UVHVTextbookSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    StoryStateSubsystem = Collection.InitializeDependency<UVHVStoryStateSubsystem>();
    CurrentLevelData = nullptr;
    RuntimeState = FTextbookRuntimeState();
    ProgressionMode = EVHVTextbookProgressionMode::InternalTextbook;
}

void UVHVTextbookSubsystem::Deinitialize()
{
    CurrentLevelData = nullptr;
    StoryStateSubsystem = nullptr;

    Super::Deinitialize();
}

// ============================================================
// JOURNEY CONTROL
// ============================================================

void UVHVTextbookSubsystem::StartJourney(UVHVLevelData* InLevelData)
{
    if (!InLevelData)
    {
        UE_LOG(LogTemp, Warning, TEXT("VHVTextbookSubsystem: StartJourney called with null LevelData."));
        return;
    }

    CurrentLevelData = InLevelData;

    RuntimeState = FTextbookRuntimeState();
    RuntimeState.bJourneyStarted = true;

    StartDay();
}

void UVHVTextbookSubsystem::StartDay()
{
    if (!CurrentLevelData)
    {
        UE_LOG(LogTemp, Warning, TEXT("VHVTextbookSubsystem: Cannot start day without LevelData."));
        return;
    }

    RuntimeState.CurrentDayIndex = 0;
    RuntimeState.CurrentTopicIndex = 0;
    RuntimeState.CurrentActivityIndex = 0;

    RuntimeState.CurrentPhase = ELearningPhase::Ask;
    RuntimeState.bActivityActive = false;

    RuntimeState.bDayStarted = true;
    RuntimeState.bDayCompleted = false;
    RuntimeState.bTopicCompleted = false;
    RuntimeState.bActivityCompleted = false;

    RuntimeState.AttemptCount = 0;
    RuntimeState.HintLevel = 0;
    RuntimeState.bAnswerSubmitted = false;
    RuntimeState.bAnswerCorrect = false;
}

// ============================================================
// LEARNING PHASE CONTROL
// ============================================================

void UVHVTextbookSubsystem::EnterAskPhase()
{
    RuntimeState.CurrentPhase = ELearningPhase::Ask;
    RuntimeState.bAnswerSubmitted = false;
    RuntimeState.bAnswerCorrect = false;
    RuntimeState.bAnswerPartial = false;

    OnLearningPhaseChanged.Broadcast(RuntimeState.CurrentPhase);
}

void UVHVTextbookSubsystem::EnterFeedbackPhase()
{
    RuntimeState.CurrentPhase = ELearningPhase::Feedback;

    OnLearningPhaseChanged.Broadcast(RuntimeState.CurrentPhase);
}

void UVHVTextbookSubsystem::EnterHintPhase()
{
    RuntimeState.CurrentPhase = ELearningPhase::Hint;

    OnLearningPhaseChanged.Broadcast(RuntimeState.CurrentPhase);
}

void UVHVTextbookSubsystem::EnterTeachPhase()
{
    RuntimeState.CurrentPhase = ELearningPhase::Teach;

    OnLearningPhaseChanged.Broadcast(RuntimeState.CurrentPhase);
}

// ============================================================
// ACTIVITY CONTROL
// ============================================================

void UVHVTextbookSubsystem::SubmitAnswer(const bool bWasCorrect, const bool bWasPartial)
{
    if (!CurrentLevelData || !RuntimeState.bActivityActive || RuntimeState.CurrentPhase != ELearningPhase::Ask)
    {
        return;
    }

    RuntimeState.AttemptCount++;
    RuntimeState.bAnswerSubmitted = true;
    RuntimeState.bAnswerCorrect = bWasCorrect;
    RuntimeState.bAnswerPartial = bWasPartial;

    const FTextbookActivityData CurrentActivity = GetCurrentActivity();
    const bool bAssessedChoice = CurrentActivity.ActivityType == ETextbookActivityType::SingleChoice
        || CurrentActivity.ActivityType == ETextbookActivityType::MultiChoice;
    const bool bRetryEnabled = bAssessedChoice && CurrentActivity.AttemptPolicy.bEnabled;
    const int32 MaxAttempts = FMath::Max(1, CurrentActivity.AttemptPolicy.MaxAttempts);
    const bool bNeedsRetry = bRetryEnabled && !bWasCorrect && RuntimeState.AttemptCount < MaxAttempts;

    if (bNeedsRetry)
    {
        UE_LOG(LogTemp, Log, TEXT("[VHVTextbook] Choice attempt=%d/%d result=Wrong -> Hint"),
            RuntimeState.AttemptCount, MaxAttempts);
        EnterHintPhase();
        return;
    }

    // Result effects are terminal for retry-enabled questions. This prevents a
    // first-attempt miss from applying failure state before the retry is used.
    ApplyCurrentActivityResultEffects(bWasCorrect, bWasPartial);
    UE_LOG(LogTemp, Log, TEXT("[VHVTextbook] Choice attempt=%d/%d result=%s -> Feedback"),
        RuntimeState.AttemptCount, bRetryEnabled ? MaxAttempts : RuntimeState.AttemptCount,
        bWasCorrect ? TEXT("Correct") : TEXT("Wrong"));
    EnterFeedbackPhase();
}

bool UVHVTextbookSubsystem::SubmitMultiChoice(const TArray<int32>& SelectedOptionIndices)
{
    if (!CurrentLevelData || !RuntimeState.bActivityActive || RuntimeState.CurrentPhase != ELearningPhase::Ask)
    {
        return false;
    }

    const FTextbookActivityData CurrentActivity = GetCurrentActivity();
    if (CurrentActivity.ActivityType != ETextbookActivityType::MultiChoice || SelectedOptionIndices.Num() == 0)
    {
        return false;
    }

    TSet<int32> SubmittedIndices;
    for (const int32 OptionIndex : SelectedOptionIndices)
    {
        if (!CurrentActivity.Question.Options.IsValidIndex(OptionIndex) || SubmittedIndices.Contains(OptionIndex))
        {
            return false;
        }
        SubmittedIndices.Add(OptionIndex);
    }

    int32 CorrectOptionCount = 0;
    int32 SelectedCorrectOptionCount = 0;
    bool bSelectedIncorrectOption = false;
    for (int32 OptionIndex = 0; OptionIndex < CurrentActivity.Question.Options.Num(); ++OptionIndex)
    {
        const bool bIsCorrectOption = CurrentActivity.Question.Options[OptionIndex].bIsCorrect;
        const bool bIsSelected = SubmittedIndices.Contains(OptionIndex);
        if (bIsCorrectOption)
        {
            ++CorrectOptionCount;
            if (bIsSelected)
            {
                ++SelectedCorrectOptionCount;
            }
        }
        else if (bIsSelected)
        {
            bSelectedIncorrectOption = true;
        }
    }

    const bool bWasCorrect = !bSelectedIncorrectOption && SelectedCorrectOptionCount == CorrectOptionCount;
    const bool bWasPartial = !bSelectedIncorrectOption && SelectedCorrectOptionCount < CorrectOptionCount;

    UE_LOG(LogTemp, Log, TEXT("[VHVTextbook] MultiChoice submitted SelectedCount=%d"), SubmittedIndices.Num());
    UE_LOG(LogTemp, Log, TEXT("[VHVTextbook] MultiChoice result=%s"), bWasCorrect ? TEXT("Correct") : (bWasPartial ? TEXT("Partial") : TEXT("Incorrect")));
    SubmitAnswer(bWasCorrect, bWasPartial);
    return bWasCorrect;
}

bool UVHVTextbookSubsystem::SubmitOrdering(const TArray<FString>& OrderedItemIDs)
{
    if (!CurrentLevelData || !RuntimeState.bActivityActive || RuntimeState.CurrentPhase != ELearningPhase::Ask || RuntimeState.AttemptCount >= 3)
    {
        return false;
    }

    const FTextbookActivityData CurrentActivity = GetCurrentActivity();
    if (CurrentActivity.ActivityType != ETextbookActivityType::Ordering)
    {
        return false;
    }

    if (!IsValidOrderingDefinition(CurrentActivity))
    {
        return false;
    }

    if (OrderedItemIDs.Num() != CurrentActivity.OrderingItems.Num())
    {
        return false;
    }

    TSet<FString> SubmittedIDs;
    for (const FString& ItemID : OrderedItemIDs)
    {
        if (ItemID.IsEmpty() || SubmittedIDs.Contains(ItemID))
        {
            return false;
        }

        SubmittedIDs.Add(ItemID);

        bool bFound = false;
        for (const FOrderingItem& OrderingItem : CurrentActivity.OrderingItems)
        {
            if (OrderingItem.ItemID == ItemID)
            {
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            return false;
        }
    }

    bool bCorrect = true;
    if (OrderedItemIDs.Num() == CurrentActivity.CorrectOrder.Num())
    {
        for (int32 Index = 0; Index < OrderedItemIDs.Num(); ++Index)
        {
            if (OrderedItemIDs[Index] != CurrentActivity.CorrectOrder[Index])
            {
                bCorrect = false;
                break;
            }
        }
    }
    else
    {
        bCorrect = false;
    }

    RuntimeState.AttemptCount++;
    RuntimeState.bAnswerSubmitted = true;
    RuntimeState.bAnswerCorrect = bCorrect;
    RuntimeState.bAnswerPartial = false;

    ApplyCurrentActivityResultEffects(bCorrect, false);

    if (bCorrect)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VHVTextbook] Ordering attempt=%d result=Correct -> Feedback"), RuntimeState.AttemptCount);
        EnterFeedbackPhase();
    }
    else if (RuntimeState.AttemptCount < 3)
    {
        RequestHint();
        UE_LOG(LogTemp, Warning, TEXT("[VHVTextbook] Ordering attempt=%d result=Wrong -> Hint"), RuntimeState.AttemptCount);
        EnterHintPhase();
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VHVTextbook] Ordering attempt=3 result=Wrong -> Feedback"));
        EnterFeedbackPhase();
    }

    return bCorrect;
}

bool UVHVTextbookSubsystem::IsValidOrderingDefinition(const FTextbookActivityData& Activity) const
{
    if (Activity.ActivityType != ETextbookActivityType::Ordering)
    {
        return false;
    }

    if (Activity.OrderingItems.Num() == 0)
    {
        return false;
    }

    TSet<FString> ItemIDs;
    for (const FOrderingItem& Item : Activity.OrderingItems)
    {
        if (Item.ItemID.IsEmpty() || ItemIDs.Contains(Item.ItemID))
        {
            return false;
        }

        ItemIDs.Add(Item.ItemID);
    }

    if (Activity.CorrectOrder.Num() != Activity.OrderingItems.Num())
    {
        return false;
    }

    TSet<FString> OrderedItemIDs;
    for (const FString& ItemID : Activity.CorrectOrder)
    {
        if (ItemID.IsEmpty())
        {
            return false;
        }

        if (OrderedItemIDs.Contains(ItemID))
        {
            return false;
        }

        OrderedItemIDs.Add(ItemID);

        bool bExists = false;
        for (const FOrderingItem& Item : Activity.OrderingItems)
        {
            if (Item.ItemID == ItemID)
            {
                bExists = true;
                break;
            }
        }

        if (!bExists)
        {
            return false;
        }
    }

    return true;
}

bool UVHVTextbookSubsystem::SubmitMatching(const TArray<FMatchingPair>& SubmittedMatches)
{
    if (!CurrentLevelData || !RuntimeState.bActivityActive || RuntimeState.CurrentPhase != ELearningPhase::Ask || RuntimeState.AttemptCount >= 3)
    {
        return false;
    }

    const FTextbookActivityData CurrentActivity = GetCurrentActivity();
    if (!IsValidMatchingDefinition(CurrentActivity))
    {
        return false;
    }

    TMap<FString, FString> SubmittedByLeft;
    TSet<FString> SubmittedRights;
    bool bHasIncorrectMatch = false;
    for (const FMatchingPair& SubmittedMatch : SubmittedMatches)
    {
        if (SubmittedMatch.LeftText.IsEmpty() || SubmittedMatch.RightText.IsEmpty() || SubmittedByLeft.Contains(SubmittedMatch.LeftText) || SubmittedRights.Contains(SubmittedMatch.RightText))
        {
            return false;
        }
        SubmittedByLeft.Add(SubmittedMatch.LeftText, SubmittedMatch.RightText);
        SubmittedRights.Add(SubmittedMatch.RightText);

        const FMatchingPair* AuthoredMatch = CurrentActivity.MatchingPairs.FindByPredicate([&SubmittedMatch](const FMatchingPair& Pair)
        {
            return Pair.LeftText == SubmittedMatch.LeftText;
        });
        if (!AuthoredMatch || AuthoredMatch->RightText != SubmittedMatch.RightText)
        {
            bHasIncorrectMatch = true;
        }
    }

    const bool bWasCorrect = !bHasIncorrectMatch && SubmittedByLeft.Num() == CurrentActivity.MatchingPairs.Num();
    const bool bWasPartial = !bHasIncorrectMatch && SubmittedByLeft.Num() < CurrentActivity.MatchingPairs.Num();
    SubmitAnswer(bWasCorrect, bWasPartial);
    UE_LOG(LogTemp, Log, TEXT("[VHVTextbook] Matching submitted MatchCount=%d result=%s"), SubmittedMatches.Num(), bWasCorrect ? TEXT("Correct") : (bWasPartial ? TEXT("Partial") : TEXT("Incorrect")));
    return bWasCorrect;
}

bool UVHVTextbookSubsystem::SubmitObservation()
{
    if (!CurrentLevelData || !RuntimeState.bActivityActive || RuntimeState.CurrentPhase != ELearningPhase::Ask)
    {
        return false;
    }

    const FTextbookActivityData CurrentActivity = GetCurrentActivity();
    if (CurrentActivity.ActivityType != ETextbookActivityType::Observation)
    {
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[VHVTextbook] Observation submitted ActivityID=%s"), *CurrentActivity.GetEffectiveActivityID());
    if (CurrentActivity.EvidenceTagging.bUseEvidenceTagging)
    {
        RuntimeState.AttemptCount++;
        RuntimeState.bAnswerSubmitted = true;
        RuntimeState.bAnswerCorrect = true;
        RuntimeState.bAnswerPartial = false;
        AdvanceToNextActivity();
        return true;
    }

    SubmitAnswer(true);
    return true;
}

bool UVHVTextbookSubsystem::IsValidMatchingDefinition(const FTextbookActivityData& Activity) const
{
    if (Activity.ActivityType != ETextbookActivityType::Matching || Activity.MatchingPairs.Num() == 0)
    {
        return false;
    }

    TSet<FString> LeftTexts;
    TSet<FString> RightTexts;
    for (const FMatchingPair& Pair : Activity.MatchingPairs)
    {
        if (Pair.LeftText.IsEmpty() || Pair.RightText.IsEmpty() || LeftTexts.Contains(Pair.LeftText) || RightTexts.Contains(Pair.RightText))
        {
            return false;
        }
        LeftTexts.Add(Pair.LeftText);
        RightTexts.Add(Pair.RightText);
    }
    return true;
}

void UVHVTextbookSubsystem::ApplyCurrentActivityResultEffects(const bool bWasCorrect, const bool bWasPartial)
{
    if (!StoryStateSubsystem || bWasPartial)
    {
        return;
    }

    const FTextbookActivityData CurrentActivity = GetCurrentActivity();
    if (CurrentActivity.ActivityType == ETextbookActivityType::Observation
        || CurrentActivity.ActivityType == ETextbookActivityType::DialogueChoice)
    {
        return;
    }

    StoryStateSubsystem->ApplyEffects(bWasCorrect ? CurrentActivity.SuccessEffects : CurrentActivity.FailureEffects);
}

void UVHVTextbookSubsystem::CompleteCurrentActivity()
{
    const FTextbookActivityData CurrentActivity = GetCurrentActivity();

    RuntimeState.bActivityCompleted = true;
    RuntimeState.bActivityActive = false;

    const FString CurrentActivityID = CurrentActivity.GetEffectiveActivityID();
    if (!CurrentActivityID.IsEmpty())
    {
        RuntimeState.CompletedActivityIDs.Add(CurrentActivityID);
        UE_LOG(LogTemp, Warning, TEXT("[VHVTextbook] Completed ActivityID=%s"), *CurrentActivityID);
    }

    OnActivityCompleted.Broadcast();
}

void UVHVTextbookSubsystem::AdvanceToNextActivity()
{
    if (!CurrentLevelData)
    {
        return;
    }

    if (ProgressionMode == EVHVTextbookProgressionMode::QuestManaged)
    {
        if (RuntimeState.bActivityActive)
        {
            CompleteCurrentActivity();
        }
        return;
    }

    FDayData& DayData = CurrentLevelData->DayData;

    if (!DayData.Topics.IsValidIndex(RuntimeState.CurrentTopicIndex))
    {
        return;
    }

    FTopicData& CurrentTopic = DayData.Topics[RuntimeState.CurrentTopicIndex];

    if (RuntimeState.bActivityActive)
    {
        CompleteCurrentActivity();
    }

    // --------------------------------------------------------
    // Move to the next activity in the current topic
    // --------------------------------------------------------
    const int32 NextActivityIndex = RuntimeState.CurrentActivityIndex + 1;
    if (CurrentTopic.Activities.IsValidIndex(NextActivityIndex))
    {
        RuntimeState.CurrentActivityIndex = NextActivityIndex;
        RuntimeState.bActivityCompleted = false;
        RuntimeState.bActivityActive = true;
        RuntimeState.bAnswerSubmitted = false;
        RuntimeState.bAnswerCorrect = false;
        RuntimeState.AttemptCount = 0;
        RuntimeState.HintLevel = 0;

        const FTextbookActivityData NextActivity = GetCurrentActivity();
        UE_LOG(LogTemp, Warning, TEXT("[VHVTextbook] Advancing to ActivityID=%s"), *NextActivity.GetEffectiveActivityID());

        RuntimeState.CurrentPhase = ELearningPhase::Ask;
        OnLearningPhaseChanged.Broadcast(RuntimeState.CurrentPhase);
        return;
    }

    // --------------------------------------------------------
    // Current topic is complete
    // --------------------------------------------------------
    RuntimeState.bTopicCompleted = true;

    if (!CurrentTopic.TopicID.IsEmpty())
    {
        RuntimeState.MasteredTopicIDs.Add(CurrentTopic.TopicID);
        UE_LOG(LogTemp, Warning, TEXT("[VHVTextbook] Topic completed=%s"), *CurrentTopic.TopicID);
    }

    OnTopicCompleted.Broadcast();

    // --------------------------------------------------------
    // Move to the next topic
    // --------------------------------------------------------
    RuntimeState.CurrentTopicIndex++;
    RuntimeState.CurrentActivityIndex = 0;

    if (DayData.Topics.IsValidIndex(RuntimeState.CurrentTopicIndex))
    {
        FTopicData& NextTopic = DayData.Topics[RuntimeState.CurrentTopicIndex];
        RuntimeState.bTopicCompleted = false;
        RuntimeState.bActivityCompleted = false;
        RuntimeState.bActivityActive = true;
        RuntimeState.bAnswerSubmitted = false;
        RuntimeState.bAnswerCorrect = false;
        RuntimeState.AttemptCount = 0;
        RuntimeState.HintLevel = 0;

        UE_LOG(LogTemp, Warning, TEXT("[VHVTextbook] Starting next TopicID=%s"), *NextTopic.TopicID);

        const FTextbookActivityData NextActivity = GetCurrentActivity();
        if (!NextActivity.GetEffectiveActivityID().IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("[VHVTextbook] Advancing to ActivityID=%s"), *NextActivity.GetEffectiveActivityID());
        }

        RuntimeState.CurrentPhase = ELearningPhase::Ask;
        OnLearningPhaseChanged.Broadcast(RuntimeState.CurrentPhase);
        return;
    }

    // --------------------------------------------------------
    // No more topics = day reached its final activity
    // --------------------------------------------------------
    RuntimeState.bDayCompleted = true;
    RuntimeState.bActivityActive = false;
    RuntimeState.bActivityCompleted = true;

    UE_LOG(LogTemp, Warning, TEXT("[VHVTextbook] Day reached final activity"));
    OnDayCompleted.Broadcast();
}

void UVHVTextbookSubsystem::AdvanceTeach()
{
    if (!RuntimeState.bActivityActive)
    {
        return;
    }

    CompleteCurrentActivity();
    AdvanceToNextActivity();
}

// ============================================================
// HINTS
// ============================================================

void UVHVTextbookSubsystem::RequestHint()
{
    const FTextbookActivityData CurrentActivity = GetCurrentActivity();

    if (RuntimeState.HintLevel < CurrentActivity.Hints.Num())
    {
        RuntimeState.HintLevel++;
    }
}

// ============================================================
// CURRENT CONTENT
// ============================================================

UVHVLevelData* UVHVTextbookSubsystem::GetCurrentLevelData() const
{
    return CurrentLevelData;
}

FDayData UVHVTextbookSubsystem::GetCurrentDay() const
{
    if (!CurrentLevelData)
    {
        return FDayData();
    }

    return CurrentLevelData->DayData;
}

FTopicData UVHVTextbookSubsystem::GetCurrentTopic() const
{
    if (!CurrentLevelData)
    {
        return FTopicData();
    }

    const FDayData& DayData = CurrentLevelData->DayData;

    if (!DayData.Topics.IsValidIndex(RuntimeState.CurrentTopicIndex))
    {
        return FTopicData();
    }

    return DayData.Topics[RuntimeState.CurrentTopicIndex];
}

FTextbookActivityData UVHVTextbookSubsystem::GetCurrentActivity() const
{
    const FTopicData CurrentTopic = GetCurrentTopic();

    if (!CurrentTopic.Activities.IsValidIndex(RuntimeState.CurrentActivityIndex))
    {
        return FTextbookActivityData();
    }

    return CurrentTopic.Activities[RuntimeState.CurrentActivityIndex];
}

bool UVHVTextbookSubsystem::TryGetActivityByID(const FString& ActivityID, FTextbookActivityData& OutActivity) const
{
    if (ActivityID.IsEmpty() || !CurrentLevelData)
    {
        return false;
    }

    const FDayData& DayData = CurrentLevelData->DayData;
    for (const FTopicData& Topic : DayData.Topics)
    {
        for (const FTextbookActivityData& Activity : Topic.Activities)
        {
            if (Activity.GetEffectiveActivityID() == ActivityID)
            {
                OutActivity = Activity;
                return true;
            }
        }
    }

    OutActivity = FTextbookActivityData();
    return false;
}

bool UVHVTextbookSubsystem::StartActivityByID(const FString& ActivityID)
{
    if (ActivityID.IsEmpty() || !CurrentLevelData)
    {
        return false;
    }

    const FDayData& DayData = CurrentLevelData->DayData;
    for (int32 TopicIndex = 0; TopicIndex < DayData.Topics.Num(); ++TopicIndex)
    {
        const FTopicData& Topic = DayData.Topics[TopicIndex];
        for (int32 ActivityIndex = 0; ActivityIndex < Topic.Activities.Num(); ++ActivityIndex)
        {
            if (Topic.Activities[ActivityIndex].GetEffectiveActivityID() == ActivityID)
            {
                RuntimeState.CurrentTopicIndex = TopicIndex;
                RuntimeState.CurrentActivityIndex = ActivityIndex;
                RuntimeState.CurrentPhase = ELearningPhase::Ask;
                RuntimeState.bActivityActive = true;

                RuntimeState.bJourneyStarted = true;
                RuntimeState.bDayStarted = true;
                RuntimeState.bDayCompleted = false;
                RuntimeState.bTopicCompleted = false;
                RuntimeState.bActivityCompleted = false;

                RuntimeState.AttemptCount = 0;
                RuntimeState.HintLevel = 0;
                RuntimeState.bAnswerSubmitted = false;
                RuntimeState.bAnswerCorrect = false;

                OnLearningPhaseChanged.Broadcast(RuntimeState.CurrentPhase);
                return true;
            }
        }
    }

    return false;
}

// ============================================================
// RUNTIME STATE
// ============================================================

FTextbookRuntimeState UVHVTextbookSubsystem::GetRuntimeState() const
{
    return RuntimeState;
}

ELearningPhase UVHVTextbookSubsystem::GetCurrentPhase() const
{
    return RuntimeState.CurrentPhase;
}

bool UVHVTextbookSubsystem::IsLearningActivityActive() const
{
    return RuntimeState.bActivityActive;
}

bool UVHVTextbookSubsystem::IsActivityActive() const
{
    return RuntimeState.bActivityActive;
}

void UVHVTextbookSubsystem::ExitCurrentLearningSession()
{
    RuntimeState.bActivityActive = false;
    RuntimeState.bAnswerSubmitted = false;
    RuntimeState.bAnswerCorrect = false;
    RuntimeState.bAnswerPartial = false;
    RuntimeState.CurrentPhase = ELearningPhase::Ask;
}

void UVHVTextbookSubsystem::RetryCurrentActivityAfterHint()
{
    if (!RuntimeState.bActivityActive || RuntimeState.CurrentPhase != ELearningPhase::Hint)
    {
        return;
    }

    const FTextbookActivityData CurrentActivity = GetCurrentActivity();
    RuntimeState.bAnswerSubmitted = false;
    RuntimeState.bAnswerCorrect = false;
    RuntimeState.bAnswerPartial = false;
    RuntimeState.CurrentPhase = ELearningPhase::Ask;

    UE_LOG(LogTemp, Warning, TEXT("[VHVTextbook] Hint dismissed -> retry ActivityID=%s"), *CurrentActivity.GetEffectiveActivityID());
    OnLearningPhaseChanged.Broadcast(RuntimeState.CurrentPhase);
}

void UVHVTextbookSubsystem::SetProgressionMode(const EVHVTextbookProgressionMode InMode)
{
    ProgressionMode = InMode;
}

EVHVTextbookProgressionMode UVHVTextbookSubsystem::GetProgressionMode() const
{
    return ProgressionMode;
}

void UVHVTextbookSubsystem::ExportSaveState(FVHVTextbookSaveState& OutSaveState) const
{
    OutSaveState = FVHVTextbookSaveState();
    if (!CurrentLevelData)
    {
        return;
    }

    OutSaveState.bHasState = true;
    OutSaveState.LevelDataAssetPath = FSoftObjectPath(CurrentLevelData->GetPathName());
    OutSaveState.ProgressionMode = ProgressionMode;
    OutSaveState.bJourneyStarted = RuntimeState.bJourneyStarted;
    OutSaveState.bDayStarted = RuntimeState.bDayStarted;
    OutSaveState.bDayCompleted = RuntimeState.bDayCompleted;
    OutSaveState.bTopicCompleted = RuntimeState.bTopicCompleted;
    OutSaveState.CompletedActivityIDs = RuntimeState.CompletedActivityIDs.Array();
    OutSaveState.MasteredTopicIDs = RuntimeState.MasteredTopicIDs.Array();
    OutSaveState.CompletedActivityIDs.Sort();
    OutSaveState.MasteredTopicIDs.Sort();

    if (CurrentLevelData->DayData.Topics.IsValidIndex(RuntimeState.CurrentTopicIndex))
    {
        const FTopicData& Topic = CurrentLevelData->DayData.Topics[RuntimeState.CurrentTopicIndex];
        OutSaveState.CurrentTopicID = Topic.TopicID;
        if (Topic.Activities.IsValidIndex(RuntimeState.CurrentActivityIndex))
        {
            OutSaveState.CurrentActivityID = Topic.Activities[RuntimeState.CurrentActivityIndex].GetEffectiveActivityID();
        }
    }
}

bool UVHVTextbookSubsystem::ValidateSaveState(const FVHVTextbookSaveState& SaveState) const
{
    if (!SaveState.bHasState)
    {
        return true;
    }

    UVHVLevelData* LevelData = Cast<UVHVLevelData>(SaveState.LevelDataAssetPath.TryLoad());
    if (!LevelData)
    {
        UE_LOG(LogTemp, Error, TEXT("[VHVTextbook] Saved Level Data '%s' could not be loaded."), *SaveState.LevelDataAssetPath.ToString());
        return false;
    }

    const FTopicData* CurrentTopic = LevelData->DayData.Topics.FindByPredicate([&SaveState](const FTopicData& Topic)
    {
        return Topic.TopicID == SaveState.CurrentTopicID;
    });
    if (!SaveState.CurrentTopicID.IsEmpty() && !CurrentTopic)
    {
        UE_LOG(LogTemp, Error, TEXT("[VHVTextbook] Saved Topic ID '%s' does not exist in Level Data '%s'."), *SaveState.CurrentTopicID, *LevelData->GetName());
        return false;
    }
    if (!SaveState.CurrentActivityID.IsEmpty()
        && (!CurrentTopic || !CurrentTopic->Activities.ContainsByPredicate([&SaveState](const FTextbookActivityData& Activity)
        {
            return Activity.GetEffectiveActivityID() == SaveState.CurrentActivityID;
        })))
    {
        UE_LOG(LogTemp, Error, TEXT("[VHVTextbook] Saved Activity ID '%s' does not exist in saved Topic '%s'."), *SaveState.CurrentActivityID, *SaveState.CurrentTopicID);
        return false;
    }

    for (const FString& TopicID : SaveState.MasteredTopicIDs)
    {
        if (TopicID.IsEmpty() || !LevelData->DayData.Topics.ContainsByPredicate([&TopicID](const FTopicData& Topic) { return Topic.TopicID == TopicID; }))
        {
            UE_LOG(LogTemp, Error, TEXT("[VHVTextbook] Saved mastered Topic ID '%s' is invalid for Level Data '%s'."), *TopicID, *LevelData->GetName());
            return false;
        }
    }
    for (const FString& ActivityID : SaveState.CompletedActivityIDs)
    {
        bool bFound = false;
        for (const FTopicData& Topic : LevelData->DayData.Topics)
        {
            bFound |= Topic.Activities.ContainsByPredicate([&ActivityID](const FTextbookActivityData& Activity) { return Activity.GetEffectiveActivityID() == ActivityID; });
        }
        if (ActivityID.IsEmpty() || !bFound)
        {
            UE_LOG(LogTemp, Error, TEXT("[VHVTextbook] Saved completed Activity ID '%s' is invalid for Level Data '%s'."), *ActivityID, *LevelData->GetName());
            return false;
        }
    }
    return true;
}

bool UVHVTextbookSubsystem::ImportSaveState(const FVHVTextbookSaveState& SaveState)
{
    if (!ValidateSaveState(SaveState))
    {
        return false;
    }

    if (!SaveState.bHasState)
    {
        CurrentLevelData = nullptr;
        RuntimeState = FTextbookRuntimeState();
        ProgressionMode = EVHVTextbookProgressionMode::InternalTextbook;
        return true;
    }

    UVHVLevelData* LevelData = Cast<UVHVLevelData>(SaveState.LevelDataAssetPath.TryLoad());
    if (!LevelData)
    {
        return false;
    }
    CurrentLevelData = LevelData;
    RuntimeState = FTextbookRuntimeState();
    ProgressionMode = SaveState.ProgressionMode;
    RuntimeState.bJourneyStarted = SaveState.bJourneyStarted;
    RuntimeState.bDayStarted = SaveState.bDayStarted;
    RuntimeState.bDayCompleted = SaveState.bDayCompleted;
    RuntimeState.bTopicCompleted = SaveState.bTopicCompleted;
    RuntimeState.CurrentPhase = ELearningPhase::Ask;

    RuntimeState.CurrentTopicIndex = LevelData->DayData.Topics.IndexOfByPredicate([&SaveState](const FTopicData& Topic)
    {
        return Topic.TopicID == SaveState.CurrentTopicID;
    });
    if (RuntimeState.CurrentTopicIndex == INDEX_NONE)
    {
        RuntimeState.CurrentTopicIndex = 0;
    }
    if (LevelData->DayData.Topics.IsValidIndex(RuntimeState.CurrentTopicIndex))
    {
        RuntimeState.CurrentActivityIndex = LevelData->DayData.Topics[RuntimeState.CurrentTopicIndex].Activities.IndexOfByPredicate(
            [&SaveState](const FTextbookActivityData& Activity) { return Activity.GetEffectiveActivityID() == SaveState.CurrentActivityID; });
        if (RuntimeState.CurrentActivityIndex == INDEX_NONE)
        {
            RuntimeState.CurrentActivityIndex = 0;
        }
    }

    for (const FString& ActivityID : SaveState.CompletedActivityIDs)
    {
        RuntimeState.CompletedActivityIDs.Add(ActivityID);
    }
    for (const FString& TopicID : SaveState.MasteredTopicIDs)
    {
        RuntimeState.MasteredTopicIDs.Add(TopicID);
    }
    return true;
}
