#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Quest/Types/VHVQuestTypes.h"
#include "VHVQuestSubsystem.generated.h"

class UVHVQuestArcData;
class UVHVStoryStateSubsystem;
class UVHVTextbookSubsystem;
class UVHVWorldActionSubsystem;
class AVHVNPCBehaviorTarget;
class UVHVNPCQuestCommandComponent;
class UWorld;
struct FVHVQuestSaveState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVHVQuestEvent, FName, QuestID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVHVQuestObjectiveEvent, FName, QuestID, FName, ObjectiveID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVHVTrackedQuestChanged, FName, TrackedQuestID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVHVQuestArcEvent, FName, QuestArcID);

UCLASS()
class VHV_API UVHVQuestSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool RegisterQuestArc(UVHVQuestArcData* QuestArc);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool StartQuestArc(UVHVQuestArcData* QuestArc);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool StartQuest(FName QuestID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool SetTrackedQuest(FName QuestID);

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool GetTrackedQuest(FVHVQuestJournalEntry& OutQuest) const;

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool GetCurrentObjective(FVHVQuestObjectiveDefinition& OutObjective) const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool CompleteCurrentObjective();

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool GetQuestState(FName QuestID, FVHVQuestRuntimeState& OutState) const;

    /** Read-only access to authored quest metadata for presentation and journal clients. */
    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool GetQuestDefinition(FName QuestID, FVHVQuestDefinition& OutDefinition) const;

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    FVHVQuestArcRuntimeState GetRuntimeState() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    TArray<FVHVQuestJournalEntry> GetQuestJournalEntries() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool IsQuestFlowActive() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool IsCurrentObjectiveWaitingForActivation() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool IsTransientObjectiveExecutionActive() const;

    void ExportSaveState(FVHVQuestSaveState& OutSaveState) const;
    bool ValidateSaveState(const FVHVQuestSaveState& SaveState) const;
    bool ImportSaveState(const FVHVQuestSaveState& SaveState);

#if !UE_BUILD_SHIPPING
    bool DebugCompleteCurrentObjective();
    bool DebugRestartCurrentQuest();
#endif

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool RequestCurrentObjectiveActivation();

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool NotifyParticipantInteracted(FName ParticipantID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyConversationCompleted(FName ConversationID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyActivityCompleted(FName ActivityID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyLocationReached(FName LocationID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyCustomEvent(FName EventID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest|NPC Commands")
    bool RequestNPCMove(FName ParticipantID, FName TargetID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest|NPC Commands")
    bool RequestNPCWait(FName ParticipantID, float Duration);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest|NPC Commands")
    bool RequestNPCReturnToPost(FName ParticipantID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest|NPC Commands")
    bool RequestNPCPlayAction(FName ParticipantID, FName ActionID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest|NPC Commands")
    bool ReleaseNPCFromQuest(FName ParticipantID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest|NPC Commands")
    bool CancelNPCQuestCommand(FName ParticipantID);

    bool RegisterNPCBehaviorTarget(AVHVNPCBehaviorTarget* Target);
    void UnregisterNPCBehaviorTarget(AVHVNPCBehaviorTarget* Target);
    AVHVNPCBehaviorTarget* FindNPCBehaviorTarget(const UWorld* World, FName TargetID) const;
    bool RegisterNPCCommandComponent(UVHVNPCQuestCommandComponent* CommandComponent);
    void UnregisterNPCCommandComponent(UVHVNPCQuestCommandComponent* CommandComponent);

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestEvent OnQuestStarted;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestEvent OnQuestUpdated;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestEvent OnQuestCompleted;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestObjectiveEvent OnObjectiveChanged;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestObjectiveEvent OnObjectiveCompleted;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestObjectiveEvent OnObjectiveActivationRequested;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVTrackedQuestChanged OnTrackedQuestChanged;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestArcEvent OnQuestArcCompleted;

private:
    UFUNCTION()
    void HandleTextbookActivityCompleted();

    UFUNCTION()
    void HandleNPCObjectiveCommandCompleted(EVHVNPCQuestCommandType Command, bool bSuccess);

    UFUNCTION()
    void HandleStoryFlagChanged(FName FlagID, bool bValue);

    UFUNCTION()
    void HandleStoryCounterChanged(FName CounterID, int32 NewValue);

    UFUNCTION()
    void HandleWorldActionCompleted(FGuid RequestID, FName ReceiverID, FName ActionID, bool bSuccess);

    bool ValidateQuestArc(const UVHVQuestArcData* QuestArc) const;
    void CompleteCurrentQuest();
    void ActivateCurrentObjective();
    void TryActivateCurrentObjective();
    void ReevaluateWaitingObjective();
    void EnsureStoryStateDelegateBindings();
    void ExecuteActiveNPCAction();
    void ClearActiveNPCActionTracking();
    void ExecuteActiveWorldAction();
    void ClearActiveWorldActionTracking();
#if !UE_BUILD_SHIPPING
    void DebugCancelActiveObjectiveExecution();
#endif
    bool BuildJournalEntry(const FVHVQuestDefinition& Definition, const FVHVQuestRuntimeState& State, FVHVQuestJournalEntry& OutEntry) const;
    FName GetRequiredConversationID(const FVHVQuestObjectiveDefinition& Objective) const;
    const FVHVQuestDefinition* FindQuestDefinition(FName QuestID) const;
    FVHVQuestRuntimeState* FindQuestState(FName QuestID);
    const FVHVQuestRuntimeState* FindQuestState(FName QuestID) const;
    const FVHVQuestObjectiveDefinition* GetActiveObjective() const;

    UPROPERTY()
    TObjectPtr<UVHVQuestArcData> ActiveQuestArc;

    UPROPERTY()
    TObjectPtr<UVHVTextbookSubsystem> TextbookSubsystem;

    UPROPERTY()
    TObjectPtr<UVHVStoryStateSubsystem> StoryStateSubsystem;

    UPROPERTY()
    FVHVQuestArcRuntimeState RuntimeState;

    UPROPERTY(Transient)
    TObjectPtr<UVHVNPCQuestCommandComponent> ActiveNPCObjectiveCommandComponent;

    FName ActiveNPCObjectiveQuestID;
    FName ActiveNPCObjectiveID;
    FName ActiveNPCObjectiveParticipantID;
    EVHVNPCQuestCommandType ActiveNPCObjectiveCommandType = EVHVNPCQuestCommandType::None;

    UPROPERTY(Transient)
    TObjectPtr<UVHVWorldActionSubsystem> ActiveWorldActionSubsystem;

    FGuid ActiveWorldActionRequestID;
    FName ActiveWorldActionQuestID;
    FName ActiveWorldActionObjectiveID;
    FName ActiveWorldActionReceiverID;
    FName ActiveWorldActionID;

    bool bCurrentObjectiveActivated = false;
    bool bCurrentObjectiveWaitingOnConditions = false;
    bool bCompletingCurrentObjective = false;
    uint32 ObjectiveActivationSerial = 0;

    using FBehaviorTargetRegistry = TMap<FName, TWeakObjectPtr<AVHVNPCBehaviorTarget>>;
    using FNPCCommandRegistry = TMap<FName, TWeakObjectPtr<UVHVNPCQuestCommandComponent>>;
    TMap<TWeakObjectPtr<UWorld>, FBehaviorTargetRegistry> BehaviorTargetsByWorld;
    TMap<TWeakObjectPtr<UWorld>, FNPCCommandRegistry> NPCCommandsByWorld;

    UVHVNPCQuestCommandComponent* FindNPCCommandComponent(const UWorld* World, FName ParticipantID) const;
};
