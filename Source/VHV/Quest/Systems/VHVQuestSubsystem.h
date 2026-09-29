#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Quest/Types/VHVQuestTypes.h"
#include "VHVQuestSubsystem.generated.h"

class UVHVQuestArcData;
class UVHVStoryStateSubsystem;
class UVHVTextbookSubsystem;
class UVHVWorldActionSubsystem;
class AActor;
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

    /** Resolve an independently authored conversation for a participant in the active objective. */
    bool TryGetCurrentContextualConversation(
        FName ParticipantID,
        UVHVConversationDataAsset*& OutConversation,
        FName& OutEntryNodeID) const;

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

    /** True while the active objective is running staged NPC travel that leaves the player in Gameplay input. */
    UFUNCTION(BlueprintPure, Category = "VHV|Quest|NPC Travel")
    bool IsCurrentObjectiveFreeRoamNPCTravelActive() const;

    /** Used by contextual interaction to suppress prompts on NPCs while they are required travel participants. */
    UFUNCTION(BlueprintPure, Category = "VHV|Quest|NPC Travel")
    bool IsParticipantInActiveFreeRoamNPCTravel(FName ParticipantID) const;

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

    /** Returns whether the active objective currently accepts an interaction from this participant. */
    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool CanParticipantInteract(FName ParticipantID) const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyConversationCompleted(FName ConversationID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyActivityCompleted(FName ActivityID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyLocationReached(FName LocationID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyCustomEvent(FName EventID);

    /** Dispatches the active Explicit Trigger WorldAction objective when the authored route and gate match. */
    UFUNCTION(BlueprintCallable, Category = "VHV|Quest|World Action")
    bool RequestExplicitWorldAction(
        FName ReceiverID,
        FName ActionID,
        FName RequiredQuestID = NAME_None,
        FName RequiredObjectiveID = NAME_None);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest|NPC Commands")
    bool RequestNPCMove(
        FName ParticipantID,
        FName TargetID,
        bool bFaceDestinationRotation = false,
        float MoveSpeedOverride = 0.0f);

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
    /** Resolves either a legacy behavior target or an editor-visible semantic location volume. */
    AActor* FindNPCMovementTarget(const UWorld* World, FName TargetID) const;
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

    /** Broadcast after activation conditions and NPC readiness gates pass. */
    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestObjectiveEvent OnObjectiveReady;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVTrackedQuestChanged OnTrackedQuestChanged;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestArcEvent OnQuestArcCompleted;

private:
    UFUNCTION()
    void HandleTextbookActivityCompleted();

    UFUNCTION()
    void HandleNPCObjectiveCommandCompleted(EVHVNPCQuestCommandType Command, bool bSuccess);

    void HandleObjectiveNPCMoveCompleted(
        UVHVNPCQuestCommandComponent* CommandComponent,
        EVHVNPCQuestCommandType Command,
        bool bSuccess);

    void HandleObjectiveNPCReadinessMoveCompleted(
        UVHVNPCQuestCommandComponent* CommandComponent,
        EVHVNPCQuestCommandType Command,
        bool bSuccess);

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
    bool TryCompleteCurrentObjectiveFromStoryState();
    void EnsureStoryStateDelegateBindings();
    bool GetActiveObjectiveNPCReadinessRequirements(TArray<TPair<FName, FName>>& OutRequirements) const;
    bool AreActiveObjectiveNPCReadinessRequirementsMet() const;
    void BindActiveObjectiveNPCReadinessCallbacks();
    void ClearActiveObjectiveNPCReadinessTracking();
    void ContinueActiveObjectiveAfterNPCMoves();
    void StartActiveObjectiveNPCMoveSequence();
    bool TryStartActiveObjectiveNPCMoveSequenceFromConditions();
    void StartActiveObjectiveNPCMoveStage();
    void StartActiveObjectiveStageAmbientConversation(const FVHVQuestNPCMoveStage& Stage);
    void FinishActiveObjectiveNPCMoveStage();
    void FailActiveObjectiveNPCMoveSequence(FName ParticipantID, const TCHAR* Reason);
    void ClearActiveObjectiveNPCMoveTracking(bool bCancelCommands = false);
    void ExecuteActiveNPCAction();
    void ClearActiveNPCActionTracking();
    bool ExecuteActiveWorldAction();
    void ClearActiveWorldActionTracking();
    void DispatchNPCMoves(const TArray<FVHVQuestNPCMoveRequest>& Moves);
    void RestorePersistentNPCMoves();
    void RestorePersistentNPCMoveForParticipant(FName ParticipantID);
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

    TMap<TWeakObjectPtr<UVHVNPCQuestCommandComponent>, FName> ActiveObjectiveNPCMoveParticipants;
    TSet<FName> PendingObjectiveNPCMoveParticipants;
    FName ActiveObjectiveNPCMoveQuestID;
    FName ActiveObjectiveNPCMoveObjectiveID;
    int32 ActiveObjectiveNPCMoveStageIndex = INDEX_NONE;
    uint32 ActiveObjectiveNPCMoveActivationSerial = 0;
    bool bCurrentObjectiveWaitingOnNPCMoves = false;
    bool bDispatchingObjectiveNPCMoveStage = false;
    bool bObjectiveNPCMoveStageFailed = false;

    TMap<TWeakObjectPtr<UVHVNPCQuestCommandComponent>, FDelegateHandle> ActiveObjectiveNPCReadinessBindings;
    bool bCurrentObjectiveWaitingOnNPCReadiness = false;

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
#if !UE_BUILD_SHIPPING
    /** Guards developer fast-forward across transient cleanup and synchronous objective activation. */
    bool bDeveloperObjectiveSkipInProgress = false;
#endif
    uint32 ObjectiveActivationSerial = 0;

    using FBehaviorTargetRegistry = TMap<FName, TWeakObjectPtr<AVHVNPCBehaviorTarget>>;
    using FNPCCommandRegistry = TMap<FName, TWeakObjectPtr<UVHVNPCQuestCommandComponent>>;
    TMap<TWeakObjectPtr<UWorld>, FBehaviorTargetRegistry> BehaviorTargetsByWorld;
    TMap<TWeakObjectPtr<UWorld>, FNPCCommandRegistry> NPCCommandsByWorld;

    UVHVNPCQuestCommandComponent* FindNPCCommandComponent(const UWorld* World, FName ParticipantID) const;
};
