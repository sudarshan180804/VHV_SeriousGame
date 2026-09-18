#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHV/Textbook/Types/VHVTextbookRuntimeTypes.h"
#include "VHV/Textbook/Data/VHVLevelData.h"
#include "VHVTextbookSubsystem.generated.h"

class UVHVStoryStateSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLearningPhaseChanged, ELearningPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActivityCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTopicCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDayCompleted);

UCLASS()
class VHV_API UVHVTextbookSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    // ========================================================
    // LIFECYCLE
    // ========================================================

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ========================================================
    // JOURNEY CONTROL
    // ========================================================

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void StartJourney(UVHVLevelData* InLevelData);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void StartDay();

    // ========================================================
    // LEARNING PHASE CONTROL
    // ========================================================

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void EnterAskPhase();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void EnterFeedbackPhase();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void EnterHintPhase();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void EnterTeachPhase();

    // ========================================================
    // ACTIVITY CONTROL
    // ========================================================

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SubmitAnswer(bool bWasCorrect, bool bWasPartial = false);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    bool SubmitMultiChoice(const TArray<int32>& SelectedOptionIndices);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    bool SubmitOrdering(const TArray<FString>& OrderedItemIDs);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    bool SubmitMatching(const TArray<FMatchingPair>& SubmittedMatches);

    /** Records that the current observation prompt has been reviewed. */
    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    bool SubmitObservation();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void CompleteCurrentActivity();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void AdvanceToNextActivity();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void AdvanceTeach();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void RetryCurrentActivityAfterHint();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetProgressionMode(EVHVTextbookProgressionMode InMode);

    UFUNCTION(BlueprintPure, Category = "VHV|Textbook")
    EVHVTextbookProgressionMode GetProgressionMode() const;

    // ========================================================
    // HINTS
    // ========================================================

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void RequestHint();

    // ========================================================
    // CURRENT CONTENT
    // ========================================================

    UFUNCTION(BlueprintPure, Category = "VHV|Textbook")
    UVHVLevelData* GetCurrentLevelData() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Textbook")
    FDayData GetCurrentDay() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Textbook")
    FTopicData GetCurrentTopic() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Textbook")
    FTextbookActivityData GetCurrentActivity() const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    bool TryGetActivityByID(const FString& ActivityID, FTextbookActivityData& OutActivity) const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    bool StartActivityByID(const FString& ActivityID);

    // ========================================================
    // RUNTIME STATE
    // ========================================================

    UFUNCTION(BlueprintPure, Category = "VHV|Textbook")
    FTextbookRuntimeState GetRuntimeState() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Textbook")
    ELearningPhase GetCurrentPhase() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Textbook")
    bool IsLearningActivityActive() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Textbook")
    bool IsActivityActive() const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void ExitCurrentLearningSession();

    // ========================================================
    // EVENTS
    // ========================================================

    UPROPERTY(BlueprintAssignable, Category = "VHV|Textbook|Events")
    FOnLearningPhaseChanged OnLearningPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Textbook|Events")
    FOnActivityCompleted OnActivityCompleted;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Textbook|Events")
    FOnTopicCompleted OnTopicCompleted;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Textbook|Events")
    FOnDayCompleted OnDayCompleted;

private:

    bool IsValidOrderingDefinition(const FTextbookActivityData& Activity) const;
    bool IsValidMatchingDefinition(const FTextbookActivityData& Activity) const;
    void ApplyCurrentActivityResultEffects(bool bWasCorrect, bool bWasPartial);

    // ========================================================
    // CURRICULUM DATA
    // ========================================================

    UPROPERTY()
    TObjectPtr<UVHVLevelData> CurrentLevelData;

    // ========================================================
    // RUNTIME STATE
    // ========================================================

    UPROPERTY()
    FTextbookRuntimeState RuntimeState;

    UPROPERTY()
    EVHVTextbookProgressionMode ProgressionMode = EVHVTextbookProgressionMode::InternalTextbook;

    UPROPERTY()
    TObjectPtr<UVHVStoryStateSubsystem> StoryStateSubsystem;
};
