#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/VHVDialogueTypes.h"
#include "Core/VHVConversationDataAsset.h"
#include "Containers/Map.h"
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

UENUM()
enum class EVHVUIState : uint8
{
    Gameplay,
    Dialogue,
    LearningAsk,
    LearningHint,
    LearningFeedback,
    LearningTeach
};

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVUIManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVHVUIManagerComponent();

protected:
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
    bool bQuestManagedLearningActivityActive = false;

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    bool StartConversation(const FDialogueConversation& InConversation);

    /** Starts a conversation at an authored node, without replacing it with a checkpoint or start node. */
    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    bool StartConversationAtNode(const FDialogueConversation& InConversation, const FString& NodeID);

    UFUNCTION(BlueprintCallable, Category = "VHV|UI", meta = (DisplayName = "Start Conversation From Asset"))
    bool StartConversationFromAsset(UVHVConversationDataAsset* ConversationAsset);

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void AdvanceConversation();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void SelectNextChoice();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void SelectPreviousChoice();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    bool ConfirmChoice();

    UFUNCTION()
    void ConfirmChoiceInput();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void ExitConversation();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    FConversationRuntimeState GetConversationState() const;

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void ShowDialogue(const FDialogueData& InDialogue);

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void HideDialogue();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void AdvanceDialogue();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    bool StartLinkedLearningActivity(const FTextbookActivityReference& Reference);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    bool TrySubmitCurrentQuestionAnswer();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SubmitOrderingAnswer(const TArray<FString>& OrderedItemIDs);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SubmitMatchingAnswer(const TArray<FMatchingPair>& SubmittedMatches);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    bool SubmitObservation();

protected:
    UPROPERTY()
    TObjectPtr<UVHVPlayerInteractionComponent> PlayerInteractionComponent;

    UPROPERTY()
    TMap<FString, FConversationRuntimeState> ConversationStates;

    UPROPERTY()
    TObjectPtr<UVHVTextbookSubsystem> TextbookSubsystem;

    UPROPERTY()
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    UFUNCTION()
    void HandleInteractionTargetChanged(UVHVInteractionComponent* NewTarget);

    UFUNCTION()
    void HandleLearningPhaseChanged(ELearningPhase NewPhase);

    UFUNCTION()
    void HandleTextbookActivityCompleted();

    UFUNCTION()
    void HandleQuestObjectiveActivationRequested(FName QuestID, FName ObjectiveID);

    void EnsureFeedbackWidget();
    void EnsureObservationWidget();
    void EnsureOrderingWidget();
    void EnsureMatchingWidget();
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
    void AdvanceFeedback();
    void AdvanceHint();
    void AdvanceTeach();
    void SetMovementLocked(bool bLocked);
    void SetQuestionInputState(bool bActive);
    void SetDialogueInputState(bool bActive);
    void RestoreGameplayAfterQuestModalIfNeeded();
    void CommitCurrentConversationState();
    const FDialogueNode* FindNodeByID(const FString& ConversationID, const FString& NodeID) const;
    FDialogueNode* FindNodeByIDMutable(const FString& ConversationID, const FString& NodeID);
    bool StartConversationInternal(const FDialogueConversation& InConversation, const FString& ExplicitStartNodeID);
    bool StartDialogueChoiceActivity(const FDialogueChoiceActivityReference& Reference);
    bool TraverseToNode(const FString& NodeID, bool bLogBranchEntry = false);
    void CompleteConversation();
};
