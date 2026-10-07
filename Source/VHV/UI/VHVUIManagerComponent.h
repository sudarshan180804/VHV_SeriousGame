#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/VHVDialogueTypes.h"
#include "Core/VHVConversationDataAsset.h"
#include "Containers/Map.h"
#include "UI/VHVMajorQuestStingerTypes.h"
#include "VHVUIManagerComponent.generated.h"

class UVHVPlayerInteractionComponent;
class UVHVInteractionComponent;
class UVHVMainHUD;
class UVHVDialogueWidget;
class UVHVQuestionWidget;
class UVHVFeedbackWidget;
class UVHVTeachWidget;
class UVHVObservationWidget;
class UVHVOrderingWidget;
class UVHVOrderingCardWidget;
class UVHVMatchingWidget;
class UVHVMatchingCardWidget;
class UVHVTextbookSubsystem;
class UVHVQuestSubsystem;
class UVHVQuestTrackerWidget;
class UVHVObjectiveDirectionWidget;
class UVHVMajorQuestStingerWidget;
class UVHVStoryStateSubsystem;
class UVHVSupportTypeOverlayWidget;
class UVHVSupportNetworkWidget;
class UVHVSocialSupportHUDWidget;
class UUserWidget;
class AVHVObjectiveTrackingMarker;
class AVHVQuestLocationVolume;
class AActor;
struct FVHVDialogueCheckpointSaveState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVHVConversationSessionEnded);

UENUM()
enum class EVHVUIState : uint8
{
    Gameplay,
    Dialogue,
    LearningAsk,
    LearningHint,
    LearningFeedback,
    LearningTeach,
    MajorStinger
};

UENUM(BlueprintType)
enum class EVHVObjectiveTrackingMode : uint8
{
    Off,
    WorldMarker,
    WorldMarkerAndDirection
};

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVUIManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVHVUIManagerComponent();

protected:
    struct FQueuedMajorStinger
    {
        FVHVMajorQuestStingerData Data;
        bool bCompletesLearningActivity = false;
    };

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UPROPERTY(EditAnywhere, Category = "VHV|UI")
    TSubclassOf<UVHVMainHUD> MainHUDClass;

    UPROPERTY(EditAnywhere, Category = "VHV|UI")
    TSubclassOf<UVHVDialogueWidget> DialogueWidgetClass;

    UPROPERTY(EditAnywhere, Category = "VHV|UI")
    TSubclassOf<UVHVQuestionWidget> QuestionWidgetClass;

    UPROPERTY(EditAnywhere, Category = "VHV|UI")
    TSubclassOf<UVHVFeedbackWidget> FeedbackWidgetClass;

    UPROPERTY(EditAnywhere, Category = "VHV|UI")
    TSubclassOf<UVHVTeachWidget> TeachWidgetClass;

    /** Optional styled subclass. The native observation widget is used when unset. */
    UPROPERTY(EditAnywhere, Category = "VHV|UI")
    TSubclassOf<UVHVObservationWidget> ObservationWidgetClass;

    UPROPERTY(EditAnywhere, Category = "VHV|UI")
    TSubclassOf<UVHVOrderingWidget> OrderingWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VHV|UI")
    TSubclassOf<UVHVOrderingCardWidget> OrderingCardWidgetClass;

    UPROPERTY(EditAnywhere, Category = "VHV|UI")
    TSubclassOf<UVHVMatchingWidget> MatchingWidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VHV|UI")
    TSubclassOf<UVHVMatchingCardWidget> MatchingCardWidgetClass;

    UPROPERTY(EditAnywhere, Category = "VHV|UI")
    TSubclassOf<UVHVQuestTrackerWidget> QuestTrackerWidgetClass;

    UPROPERTY()
    TObjectPtr<UVHVMainHUD> MainHUD;

    UPROPERTY()
    TObjectPtr<UVHVDialogueWidget> DialogueWidget;

    UPROPERTY()
    TObjectPtr<UVHVQuestionWidget> QuestionWidget;

    UPROPERTY()
    TObjectPtr<UVHVFeedbackWidget> FeedbackWidget;

    UPROPERTY()
    TObjectPtr<UVHVTeachWidget> TeachWidget;

    UPROPERTY()
    TObjectPtr<UVHVObservationWidget> ObservationWidget;

    UPROPERTY()
    TObjectPtr<UVHVOrderingWidget> OrderingWidget;

    UPROPERTY()
    TObjectPtr<UVHVMatchingWidget> MatchingWidget;

    UPROPERTY()
    TObjectPtr<UVHVQuestTrackerWidget> QuestTrackerWidget;

    UPROPERTY()
    TObjectPtr<UVHVMajorQuestStingerWidget> MajorQuestStingerWidget;

    UPROPERTY()
    TObjectPtr<UVHVSupportTypeOverlayWidget> SupportTypeOverlayWidget;

    UPROPERTY()
    TObjectPtr<UVHVSupportNetworkWidget> SupportNetworkWidget;

    UPROPERTY()
    TObjectPtr<UVHVSocialSupportHUDWidget> SocialSupportHUDWidget;

    UPROPERTY()
    TObjectPtr<UVHVInteractionComponent> CurrentInteractionTarget;

    UPROPERTY()
    EVHVUIState CurrentUIState = EVHVUIState::Gameplay;

    UPROPERTY()
    FDialogueConversation ActiveConversation;

    UPROPERTY()
    FConversationRuntimeState ActiveConversationState;

    UPROPERTY()
    bool bConversationActive = false;

    UPROPERTY()
    bool bConversationSessionActive = false;

    UPROPERTY()
    bool bQuestManagedLearningActivityActive = false;

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    bool StartConversation(const FDialogueConversation& InConversation);

    /** Starts a conversation at an authored node, without replacing it with a checkpoint or start node. */
    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    bool StartConversationAtNode(const FDialogueConversation& InConversation, const FString& NodeID);

    UFUNCTION(BlueprintCallable, Category = "VHV|UI", meta = (DisplayName = "Start Conversation From Asset"))
    bool StartConversationFromAsset(UVHVConversationDataAsset* ConversationAsset);

    /** Read-only check used by world interaction prompts before offering a conversation. */
    UFUNCTION(BlueprintPure, Category = "VHV|UI")
    bool CanStartConversationFromAsset(const UVHVConversationDataAsset* ConversationAsset) const;

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void AdvanceConversation();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void SelectNextChoice();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void SelectPreviousChoice();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    bool ConfirmChoice();

    bool IsDialogueChoiceAvailable(const FDialogueChoiceOption& Choice) const;

    /** True only for the choice node currently owned by a textbook DialogueChoice activity. */
    bool IsDialogueChoiceLearningActivity(const FString& ConversationID, const FString& ChoiceNodeID) const;

    UFUNCTION()
    void ConfirmChoiceInput();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void ExitConversation();

    UFUNCTION(BlueprintPure, Category = "VHV|UI")
    bool IsConversationActive() const;

    UFUNCTION(BlueprintPure, Category = "VHV|UI")
    bool IsConversationSessionActive() const;

    UPROPERTY(BlueprintAssignable, Category = "VHV|UI")
    FOnVHVConversationSessionEnded OnConversationSessionEnded;

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    FConversationRuntimeState GetConversationState() const;

    void ExportDialogueCheckpointSaveState(TArray<FVHVDialogueCheckpointSaveState>& OutSaveStates) const;
    bool ValidateDialogueCheckpointSaveState(const TArray<FVHVDialogueCheckpointSaveState>& SaveStates) const;
    bool ImportDialogueCheckpointSaveState(const TArray<FVHVDialogueCheckpointSaveState>& SaveStates);

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void ShowDialogue(const FDialogueData& InDialogue);

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void HideDialogue();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void AdvanceDialogue();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    bool StartLinkedLearningActivity(const FTextbookActivityReference& Reference);

    /** Starts a standalone quest-owned activity through the normal textbook/UI lifecycle. */
    bool StartQuestManagedLearningActivity(const FTextbookActivityReference& Reference);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    bool TrySubmitCurrentQuestionAnswer();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SubmitOrderingAnswer(const TArray<FString>& OrderedItemIDs);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SubmitMatchingAnswer(const TArray<FMatchingPair>& SubmittedMatches);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    bool SubmitObservation();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    bool ShowMajorQuestStinger(const FVHVMajorQuestStingerData& StingerData);

    UFUNCTION(BlueprintPure, Category = "VHV|UI|Diagnostics")
    bool IsMajorQuestStingerSequenceActive() const { return bMajorStingerSequenceActive; }

    UFUNCTION(BlueprintPure, Category = "VHV|UI|Diagnostics")
    bool IsGameplayUIActive() const { return CurrentUIState == EVHVUIState::Gameplay; }

    UFUNCTION(BlueprintPure, Category = "VHV|UI|Diagnostics")
    bool IsLearningAskUIActive() const { return CurrentUIState == EVHVUIState::LearningAsk; }

    UFUNCTION(BlueprintPure, Category = "VHV|UI|Diagnostics")
    bool IsQuestTrackerSuppressedForMajorStinger() const;

    /** Routes the existing IA_Interact action to the active modal activity. */
    bool HandleActivityInteractionInput();

    /** True when an unused/background left click has the same meaning as Enter. */
    UFUNCTION(BlueprintPure, Category = "VHV|UI|Input")
    bool CanAdvanceFromBackgroundClick() const;

    /**
     * Consumes an unhandled modal background click and advances only when the
     * active presentation is semantically passive.
     */
    bool HandleModalBackgroundClick();

    /** Development-only teardown used before skipping the current quest objective. */
    bool PrepareForDeveloperObjectiveSkip();

    /** Enhanced Input entry point for the Off -> beam -> beam+arrow cycle. */
    void ToggleObjectiveTracking();

    void ShowSocialSupportObservationProgress(int32 CompletedCount);
    void ShowSocialSupportSupporterProgress(int32 CompletedCount);
    void HideSocialSupportProgress();
    void ShowSupportTypeReveal();
    void HideSupportTypeOverlay();

    bool IsSupportNetworkSubmissionCorrect(const TArray<FMatchingPair>& Matches) const;
    void CompleteSupportNetworkPlanning(const TArray<FMatchingPair>& Matches);
    void CompleteSocialSupportNetworkPayoff();

protected:
    UPROPERTY()
    TObjectPtr<UVHVPlayerInteractionComponent> PlayerInteractionComponent;

    UPROPERTY()
    TMap<FString, FConversationRuntimeState> ConversationStates;

    UPROPERTY()
    TObjectPtr<UVHVTextbookSubsystem> TextbookSubsystem;

    UPROPERTY()
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    UPROPERTY()
    TObjectPtr<UVHVStoryStateSubsystem> StoryStateSubsystem;

    UPROPERTY(Transient)
    TObjectPtr<AVHVObjectiveTrackingMarker> ObjectiveTrackingMarker;

    UPROPERTY(Transient)
    TObjectPtr<UVHVObjectiveDirectionWidget> ObjectiveDirectionWidget;

    UPROPERTY(Transient)
    EVHVObjectiveTrackingMode ObjectiveTrackingMode = EVHVObjectiveTrackingMode::Off;

    FName TrackedLocationQuestID;
    FName TrackedLocationObjectiveID;
    FName TrackedLocationID;
    FName TrackedParticipantID;
    FVector TrackedLocation = FVector::ZeroVector;
    TWeakObjectPtr<AActor> TrackedLocationActor;
    TWeakObjectPtr<AVHVQuestLocationVolume> TrackedLocationVolume;
    FTimerHandle ObjectiveTrackingProximityTimer;
    FTimerHandle ObjectiveTrackingTemporaryRevealTimer;
    bool bObjectiveTrackingProximitySuppressed = false;
    bool bObjectiveTrackingTemporaryReveal = false;

    static constexpr float ObjectiveTrackingFallbackArrivalDistance = 300.0f;
    static constexpr float ObjectiveTrackingProximityCheckInterval = 0.10f;
    static constexpr float ObjectiveTrackingTemporaryRevealDuration = 2.5f;

    bool bCurrentDialogueNodeCompleted = false;
    bool bAwaitingContinuousTeachingResult = false;
    bool bStartingMajorStingerActivity = false;
    bool bMajorStingerSequenceActive = false;
    bool bCurrentMajorStingerCompletesActivity = false;
    TArray<FQueuedMajorStinger> PendingMajorStingers;

    UFUNCTION()
    void HandleInteractionTargetChanged(UVHVInteractionComponent* NewTarget);

    UFUNCTION()
    void HandleLearningPhaseChanged(ELearningPhase NewPhase);

    UFUNCTION()
    void HandleTextbookActivityCompleted();

    UFUNCTION()
    void HandleQuestObjectiveActivationRequested(FName QuestID, FName ObjectiveID);
    UFUNCTION()
    void HandleQuestObjectiveReady(FName QuestID, FName ObjectiveID);
    UFUNCTION()
    void HandleQuestObjectiveChanged(FName QuestID, FName ObjectiveID);
    UFUNCTION()
    void HandleQuestObjectiveTrackingTargetChanged(FName QuestID, FName ObjectiveID);
    UFUNCTION()
    void HandleQuestStarted(FName QuestID);
    UFUNCTION()
    void HandleQuestCompleted(FName QuestID);
    void HandleMajorQuestStingerFinished();
    bool QueueMajorQuestStinger(const FVHVMajorQuestStingerData& StingerData, bool bCompletesLearningActivity = false);
    bool PlayNextMajorQuestStinger();
    void FinishMajorQuestStingerSequence();
    void ReconcileUIStateAfterMajorStinger();

    void EnsureFeedbackWidget();
    void EnsureObservationWidget();
    void EnsureOrderingWidget();
    void EnsureMatchingWidget();
    void EnsureSocialSupportWidgets();
    bool EnsureObjectiveTrackingPresentation();
    bool RefreshActiveObjectiveTrackingTarget(FName QuestID, FName ObjectiveID);
    bool IsPlayerAtObjectiveTrackingTarget() const;
    void UpdateObjectiveTrackingProximity();
    void ApplyObjectiveTrackingVisualState();
    void BeginObjectiveTrackingTemporaryReveal();
    void EndObjectiveTrackingTemporaryReveal();
    void ClearObjectiveTracking(bool bObjectiveChanged = false);
    void StageSelfMonitoringParticipants(FName ObjectiveID);
    bool StageQuestParticipantAtLocation(FName ParticipantID, FName LocationID);
    void RefreshInteractionPrompt();
    void RefreshCurrentAskQuestionUI();
    void RefreshCurrentHintUI();
    void RefreshCurrentFeedbackUI();
    void RefreshCurrentTeachUI();
    void ApplyUIState(EVHVUIState NewState);
    void HideQuestionUI();
    void HideFeedbackUI();
    void HideTeachUI();
    void HideObservationUI();
    void HideOrderingUI();
    void HideMatchingUI();
    void HideSupportNetworkUI();
    void AdvanceFeedback();
    void AdvanceHint();
    void AdvanceTeach();
    void CompleteTeachingAdvanceAfterFade();
    void SetMovementLocked(bool bLocked);
    UUserWidget* ResolveModalFocusTarget(EVHVUIState State) const;
    void ApplyModalInputAndFocus(EVHVUIState State);
    void FocusModalWidget(EVHVUIState ExpectedState);
    void RestoreGameplayAfterQuestModalIfNeeded();
    void EndConversationSession();
    void CommitCurrentConversationState();
    const FDialogueNode* FindNodeByID(const FString& ConversationID, const FString& NodeID) const;
    FDialogueNode* FindNodeByIDMutable(const FString& ConversationID, const FString& NodeID);
    bool StartConversationInternal(const FDialogueConversation& InConversation, const FString& ExplicitStartNodeID);
    bool ResolveAvailableConversationStartNode(
        const FDialogueConversation& InConversation,
        const FString& ExplicitStartNodeID,
        FString& OutStartingNodeID) const;
    bool StartDialogueChoiceActivity(const FDialogueChoiceActivityReference& Reference);
    bool TraverseToNode(const FString& NodeID, bool bLogBranchEntry = false);
    void ApplyCurrentDialogueNodeCompletionEffects(const TArray<FVHVStoryEffect>* SelectionEffects = nullptr);
    void CompleteConversation();

    FTimerHandle SupportTypeOverlayTimer;
};
