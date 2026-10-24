#include "Quest/Systems/VHVQuestSubsystem.h"

#include "HBCT/Intro/VHVMarketObservationTrigger.h"

#include "Core/VHVConversationDataAsset.h"
#include "Quest/Data/VHVQuestArcData.h"
#include "NPC/VHVNPCCharacter.h"
#include "NPC/Components/VHVNPCBehaviorComponent.h"
#include "NPC/Components/VHVNPCQuestCommandComponent.h"
#include "NPC/Components/VHVNPCInteractionComponent.h"
#include "NPC/Components/VHVNPCNameplateComponent.h"
#include "NPC/Quest/VHVNPCBehaviorTarget.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "Save/VHVSaveTypes.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "Textbook/Data/VHVLevelData.h"
#include "Textbook/Systems/VHVTextbookSubsystem.h"
#include "World/Systems/VHVWorldActionSubsystem.h"
#include "World/Location/VHVQuestLocationVolume.h"
#include "Components/BoxComponent.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "VHV.h"

void UVHVQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    TextbookSubsystem = Collection.InitializeDependency<UVHVTextbookSubsystem>();
    StoryStateSubsystem = Collection.InitializeDependency<UVHVStoryStateSubsystem>();
    if (TextbookSubsystem)
    {
        TextbookSubsystem->OnActivityCompleted.AddDynamic(this, &UVHVQuestSubsystem::HandleTextbookActivityCompleted);
    }
    EnsureStoryStateDelegateBindings();
}

void UVHVQuestSubsystem::Deinitialize()
{
    ClearMotivationFinalDebriefInstructorWait();
    ClearActiveObjectiveNPCReadinessTracking();
    ClearActiveObjectiveNPCMoveTracking(true);
    ClearActiveNPCActionTracking();
    ClearActiveWorldActionTracking();
    if (TextbookSubsystem)
    {
        TextbookSubsystem->OnActivityCompleted.RemoveDynamic(this, &UVHVQuestSubsystem::HandleTextbookActivityCompleted);
    }
    if (StoryStateSubsystem)
    {
        StoryStateSubsystem->OnStoryFlagChanged.RemoveDynamic(this, &UVHVQuestSubsystem::HandleStoryFlagChanged);
        StoryStateSubsystem->OnStoryCounterChanged.RemoveDynamic(this, &UVHVQuestSubsystem::HandleStoryCounterChanged);
    }
    ActiveQuestArc = nullptr;
    TextbookSubsystem = nullptr;
    StoryStateSubsystem = nullptr;
    RuntimeState = FVHVQuestArcRuntimeState();
    BehaviorTargetsByWorld.Empty();
    NPCCommandsByWorld.Empty();
    Super::Deinitialize();
}

bool UVHVQuestSubsystem::RegisterQuestArc(UVHVQuestArcData* QuestArc)
{
    if (!ValidateQuestArc(QuestArc))
    {
        return false;
    }
    if (ActiveQuestArc != QuestArc)
    {
        ClearActiveWorldActionTracking();
    }
    ActiveQuestArc = QuestArc;
    return true;
}

bool UVHVQuestSubsystem::StartQuestArc(UVHVQuestArcData* QuestArc)
{
    if (!RegisterQuestArc(QuestArc))
    {
        return false;
    }

    RuntimeState = FVHVQuestArcRuntimeState();
    RuntimeState.QuestArcID = QuestArc->QuestArcID;
    RuntimeState.bArcActive = true;
    for (int32 Index = 0; Index < QuestArc->Quests.Num(); ++Index)
    {
        FVHVQuestRuntimeState State;
        State.QuestID = QuestArc->Quests[Index].QuestID;
        State.Status = Index == 0 ? EVHVQuestStatus::NotStarted : EVHVQuestStatus::Locked;
        RuntimeState.QuestStates.Add(State);
    }

    if (!QuestArc->AssociatedLevelData.IsNull())
    {
        UVHVLevelData* LevelData = QuestArc->AssociatedLevelData.LoadSynchronous();
        if (LevelData && TextbookSubsystem)
        {
            TextbookSubsystem->StartJourney(LevelData);
        }
        else
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Arc '%s' could not load its associated level data."), *QuestArc->QuestArcID.ToString());
        }
    }

    if (TextbookSubsystem)
    {
        TextbookSubsystem->SetProgressionMode(EVHVTextbookProgressionMode::QuestManaged);
    }

    RestorePersistentNPCMoves();

    UE_LOG(LogVHV, Log, TEXT("[VHVQuest] Started arc '%s'."), *QuestArc->QuestArcID.ToString());
    return StartQuest(QuestArc->Quests[0].QuestID);
}

bool UVHVQuestSubsystem::StartQuest(FName QuestID)
{
    const FVHVQuestDefinition* Definition = FindQuestDefinition(QuestID);
    FVHVQuestRuntimeState* State = FindQuestState(QuestID);
    if (!Definition || !State || State->Status == EVHVQuestStatus::Locked || State->Status == EVHVQuestStatus::Completed || State->Status == EVHVQuestStatus::Failed)
    {
        return false;
    }

    if (!RuntimeState.ActiveQuestID.IsNone() && RuntimeState.ActiveQuestID != QuestID)
    {
        FVHVQuestRuntimeState* Previous = FindQuestState(RuntimeState.ActiveQuestID);
        if (Previous && Previous->Status == EVHVQuestStatus::Active)
        {
            Previous->Status = EVHVQuestStatus::NotStarted;
        }
    }

    State->Status = EVHVQuestStatus::Active;
    RuntimeState.ActiveQuestID = QuestID;
    if (Definition->bAutoTrack || RuntimeState.TrackedQuestID.IsNone())
    {
        RuntimeState.TrackedQuestID = QuestID;
        OnTrackedQuestChanged.Broadcast(QuestID);
    }

    UE_LOG(LogVHV, Log, TEXT("[VHVQuest] Started quest '%s'."), *QuestID.ToString());
    OnQuestStarted.Broadcast(QuestID);
    ActivateCurrentObjective();
    return true;
}

bool UVHVQuestSubsystem::SetTrackedQuest(FName QuestID)
{
    const FVHVQuestRuntimeState* State = FindQuestState(QuestID);
    if (!State || State->Status == EVHVQuestStatus::Locked || State->Status == EVHVQuestStatus::Failed)
    {
        return false;
    }
    RuntimeState.TrackedQuestID = QuestID;
    OnTrackedQuestChanged.Broadcast(QuestID);
    OnQuestUpdated.Broadcast(QuestID);
    return true;
}

bool UVHVQuestSubsystem::GetTrackedQuest(FVHVQuestJournalEntry& OutQuest) const
{
    const FVHVQuestDefinition* Definition = FindQuestDefinition(RuntimeState.TrackedQuestID);
    const FVHVQuestRuntimeState* State = FindQuestState(RuntimeState.TrackedQuestID);
    return Definition && State && BuildJournalEntry(*Definition, *State, OutQuest);
}

bool UVHVQuestSubsystem::GetCurrentObjective(FVHVQuestObjectiveDefinition& OutObjective) const
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective)
    {
        OutObjective = FVHVQuestObjectiveDefinition();
        return false;
    }
    OutObjective = *Objective;
    return true;
}

bool UVHVQuestSubsystem::ResolveCurrentObjectiveTrackingTarget(
    const UWorld* World,
    FName& OutQuestID,
    FName& OutObjectiveID,
    FName& OutLocationID,
    FVector& OutWorldLocation,
    AActor*& OutTargetActor) const
{
    OutQuestID = NAME_None;
    OutObjectiveID = NAME_None;
    OutLocationID = NAME_None;
    OutWorldLocation = FVector::ZeroVector;
    OutTargetActor = nullptr;

    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!World || !Objective)
    {
        return false;
    }

    // Q3's final application is an interaction with the placed roadside
    // villager, not a static semantic volume. Resolve the exact full participant
    // tag so this cannot alias the separate Goal Setting RoadVillager whose tag
    // currently has the same leaf name.
    static const FName RoleModelQuestID(TEXT("Q_HBCT_03_ROLE_MODEL"));
    static const FName FinalApplicationObjectiveID(TEXT("O15_FinalApplication"));
    if (RuntimeState.ActiveQuestID == RoleModelQuestID
        && Objective->ObjectiveID == FinalApplicationObjectiveID)
    {
        AVHVNPCCharacter* ExactParticipant = nullptr;
        for (TActorIterator<AVHVNPCCharacter> It(World); It; ++It)
        {
            const UVHVQuestParticipantComponent* Participant = It->GetQuestParticipantComponent();
            if (!Participant || Participant->ParticipantTag != Objective->ParticipantTag)
            {
                continue;
            }
            if (ExactParticipant)
            {
                UE_LOG(LogVHV, Error,
                    TEXT("[VHVTracking] Q3 O15 has multiple NPCs with participant tag '%s'."),
                    *Objective->ParticipantTag.ToString());
                return false;
            }
            ExactParticipant = *It;
        }
        if (!ExactParticipant)
        {
            UE_LOG(LogVHV, Warning,
                TEXT("[VHVTracking] Q3 O15 roadside villager '%s' is unavailable."),
                *Objective->ParticipantTag.ToString());
            return false;
        }

        OutQuestID = RuntimeState.ActiveQuestID;
        OutObjectiveID = Objective->ObjectiveID;
        OutLocationID = Objective->GetEffectiveParticipantID();
        OutWorldLocation = ExactParticipant->GetActorLocation();
        OutTargetActor = ExactParticipant;
        return true;
    }

    if (!Objective->bEnableLocationTracking)
    {
        return false;
    }

    // The Prologue's first meeting and market debrief are both conversations
    // with the persistent Instructor.  Their authored tracking tag retains
    // the shared semantic identity ("Instructor"), but that identity is not
    // a guaranteed runtime location-volume registry entry in Lvl_Village_Main.
    // Resolve the already-authored participant directly so the beacon follows
    // the Instructor's current placed position without moving or duplicating
    // the NPC.  Keep this narrowly scoped: all other Prologue and quest
    // objectives continue through the normal semantic-location resolver.
    static const FName IntroQuestID(TEXT("Q_HBCT_00_INTRO"));
    static const FName MeetInstructorObjectiveID(TEXT("O01_MeetInstructor"));
    static const FName ReportToInstructorObjectiveID(TEXT("O04_ReportToInstructor"));
    static const FName PrologueInstructorParticipantID(TEXT("Instructor"));
    if (RuntimeState.ActiveQuestID == IntroQuestID
        && (Objective->ObjectiveID == MeetInstructorObjectiveID
            || Objective->ObjectiveID == ReportToInstructorObjectiveID))
    {
        AVHVNPCCharacter* Instructor = nullptr;
        for (TActorIterator<AVHVNPCCharacter> It(World); It; ++It)
        {
            const UVHVQuestParticipantComponent* Participant = It->GetQuestParticipantComponent();
            if (!Participant || Participant->GetEffectiveParticipantID() != PrologueInstructorParticipantID)
            {
                continue;
            }
            if (Instructor)
            {
                UE_LOG(LogVHV, Error,
                    TEXT("[VHVTracking] Prologue Instructor target is ambiguous: multiple participants use '%s'."),
                    *PrologueInstructorParticipantID.ToString());
                return false;
            }
            Instructor = *It;
        }
        if (!Instructor)
        {
            UE_LOG(LogVHV, Warning,
                TEXT("[VHVTracking] Prologue Instructor participant '%s' is unavailable for objective '%s'."),
                *PrologueInstructorParticipantID.ToString(), *Objective->ObjectiveID.ToString());
            return false;
        }

        OutQuestID = RuntimeState.ActiveQuestID;
        OutObjectiveID = Objective->ObjectiveID;
        OutLocationID = PrologueInstructorParticipantID;
        OutWorldLocation = Instructor->GetActorLocation();
        OutTargetActor = Instructor;
        return true;
    }

    // Q1's final debrief is staged from an editable TargetPoint at runtime, so its
    // waypoint must follow the persistent Instructor instead of the older authored
    // semantic location. All other objectives retain the normal location resolver.
    static const FName MotivationQuestID(TEXT("Q_HBCT_01_MOTIVATION"));
    static const FName FinalDebriefObjectiveID(TEXT("O14_FinalDebrief"));
    static const FName InstructorParticipantID(TEXT("Instructor"));
    if (RuntimeState.ActiveQuestID == MotivationQuestID
        && Objective->ObjectiveID == FinalDebriefObjectiveID)
    {
        const UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(
            World, InstructorParticipantID);
        AActor* Instructor = CommandComponent ? CommandComponent->GetOwner() : nullptr;
        if (!Instructor)
        {
            UE_LOG(LogVHV, Warning,
                TEXT("[VHVTracking] Q1 final-debrief Instructor is unavailable; refusing to use the stale semantic location."));
            return false;
        }

        OutQuestID = RuntimeState.ActiveQuestID;
        OutObjectiveID = Objective->ObjectiveID;
        OutLocationID = InstructorParticipantID;
        OutWorldLocation = Instructor->GetActorLocation();
        OutTargetActor = Instructor;
        return true;
    }

    const FName LocationID = GetFirstIncompleteTrackingLocationID(*Objective);
    if (LocationID.IsNone())
    {
        return false;
    }

    AActor* TargetActor = FindNPCMovementTarget(World, LocationID);
    if (!TargetActor)
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHVTracking] Could not resolve semantic location '%s' for objective '%s'."),
            *LocationID.ToString(), *Objective->ObjectiveID.ToString());
        return false;
    }

    OutQuestID = RuntimeState.ActiveQuestID;
    OutObjectiveID = Objective->ObjectiveID;
    OutLocationID = LocationID;
    OutWorldLocation = TargetActor->GetActorLocation();
    OutTargetActor = TargetActor;
    return true;
}

FName UVHVQuestSubsystem::GetFirstIncompleteTrackingLocationID(
    const FVHVQuestObjectiveDefinition& Objective) const
{
    if (!Objective.TrackingDestinations.IsEmpty())
    {
        for (const FVHVQuestTrackingDestination& Destination : Objective.TrackingDestinations)
        {
            if (!IsTrackingDestinationComplete(Objective, Destination))
            {
                return Destination.GetEffectiveLocationID();
            }
        }
        return NAME_None;
    }
    return Objective.GetEffectiveTrackingLocationID();
}

bool UVHVQuestSubsystem::IsTrackingDestinationComplete(
    const FVHVQuestObjectiveDefinition& Objective,
    const FVHVQuestTrackingDestination& Destination) const
{
    if (!StoryStateSubsystem)
    {
        return false;
    }
    if (StoryStateSubsystem->EvaluateConditionSet(Destination.CompletionConditions))
    {
        return true;
    }

    // Q0 market observations originally persisted fully-qualified FName keys.
    // Accept those keys when restoring an older partial save while newly
    // completed observations also publish the semantic leaf used by the data.
    static const FName IntroQuestID(TEXT("Q_HBCT_00_INTRO"));
    static const FName ObserveMarketObjectiveID(TEXT("O03_ObserveMarket"));
    const FName DestinationID = Destination.GetEffectiveLocationID();
    return RuntimeState.ActiveQuestID == IntroQuestID
        && Objective.ObjectiveID == ObserveMarketObjectiveID
        && AVHVMarketObservationTrigger::IsObservationComplete(
            StoryStateSubsystem, DestinationID);
}

bool UVHVQuestSubsystem::DoesTrackingDestinationReferenceState(
    const FVHVQuestObjectiveDefinition& Objective,
    const FName StateID) const
{
    if (StateID.IsNone())
    {
        return false;
    }
    for (const FVHVQuestTrackingDestination& Destination : Objective.TrackingDestinations)
    {
        for (const FVHVStoryCondition& Condition : Destination.CompletionConditions.Conditions)
        {
            if (Condition.GetEffectiveStateID() == StateID)
            {
                return true;
            }
        }
    }
    return false;
}

int32 UVHVQuestSubsystem::CountCompletedTrackingDestinations(
    const FVHVQuestObjectiveDefinition& Objective) const
{
    if (!StoryStateSubsystem)
    {
        return 0;
    }
    int32 CompletedCount = 0;
    for (const FVHVQuestTrackingDestination& Destination : Objective.TrackingDestinations)
    {
        if (IsTrackingDestinationComplete(Objective, Destination))
        {
            ++CompletedCount;
        }
    }
    return CompletedCount;
}

bool UVHVQuestSubsystem::GetQuestDefinition(
    const FName QuestID, FVHVQuestDefinition& OutDefinition) const
{
    const FVHVQuestDefinition* Definition = FindQuestDefinition(QuestID);
    if (!Definition)
    {
        OutDefinition = FVHVQuestDefinition();
        return false;
    }

    OutDefinition = *Definition;
    return true;
}

bool UVHVQuestSubsystem::TryGetCurrentContextualConversation(
    const FName ParticipantID,
    UVHVConversationDataAsset*& OutConversation,
    FName& OutEntryNodeID) const
{
    OutConversation = nullptr;
    OutEntryNodeID = NAME_None;
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective || ParticipantID.IsNone() || !bCurrentObjectiveActivated
        || bCurrentObjectiveWaitingOnNPCMoves || bCompletingCurrentObjective)
    {
        return false;
    }

    const FVHVQuestContextualConversation* Contextual = Objective->ContextualConversations.FindByPredicate(
        [this, ParticipantID](const FVHVQuestContextualConversation& Candidate)
        {
            return Candidate.GetEffectiveParticipantID() == ParticipantID
                && (Candidate.AvailabilityConditions.Conditions.IsEmpty()
                    || (StoryStateSubsystem
                        && StoryStateSubsystem->EvaluateConditionSet(Candidate.AvailabilityConditions)));
        });
    if (!Contextual)
    {
        return false;
    }

    OutConversation = Contextual->Conversation.LoadSynchronous();
    OutEntryNodeID = Contextual->EntryNodeID;
    return OutConversation != nullptr;
}

bool UVHVQuestSubsystem::CompleteCurrentObjective()
{
    FVHVQuestRuntimeState* QuestState = FindQuestState(RuntimeState.ActiveQuestID);
    const FVHVQuestDefinition* Quest = FindQuestDefinition(RuntimeState.ActiveQuestID);
    if (!QuestState || !Quest || QuestState->Status != EVHVQuestStatus::Active || !Quest->Objectives.IsValidIndex(QuestState->CurrentObjectiveIndex)
        || !bCurrentObjectiveActivated || bCompletingCurrentObjective)
    {
        return false;
    }

    const FName QuestID = Quest->QuestID;
    const FVHVQuestObjectiveDefinition& Objective = Quest->Objectives[QuestState->CurrentObjectiveIndex];
    const FName ObjectiveID = Objective.ObjectiveID;
    if (QuestState->CompletedObjectiveIDs.Contains(ObjectiveID))
    {
        return false;
    }

    bCompletingCurrentObjective = true;
    bCurrentObjectiveActivated = false;
    bCurrentObjectiveWaitingOnConditions = false;
    ClearActiveObjectiveNPCReadinessTracking();
    ClearActiveObjectiveNPCMoveTracking(false);
    ClearActiveNPCActionTracking();
    ClearActiveWorldActionTracking();
    QuestState->CompletedObjectiveIDs.Add(ObjectiveID);

    if (!Objective.CompletionEffects.IsEmpty() && !StoryStateSubsystem)
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Quest '%s' objective '%s' could not apply Story State completion effects because the Story State subsystem is unavailable."),
            *QuestID.ToString(), *ObjectiveID.ToString());
    }
    else if (StoryStateSubsystem && !Objective.CompletionEffects.IsEmpty()
        && !StoryStateSubsystem->ApplyEffects(Objective.CompletionEffects))
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' had one or more invalid Story State completion effects."),
            *QuestID.ToString(), *ObjectiveID.ToString());
    }

    // Story movement is fire-and-forget. A command issued while the participant is
    // still Talking queues in its existing command component and starts on Idle.
    DispatchNPCMoves(Objective.CompletionNPCMoves);

    OnObjectiveCompleted.Broadcast(QuestID, ObjectiveID);

    ++QuestState->CurrentObjectiveIndex;
    bCompletingCurrentObjective = false;
    if (Quest->Objectives.IsValidIndex(QuestState->CurrentObjectiveIndex))
    {
        ActivateCurrentObjective();
    }
    else
    {
        OnQuestUpdated.Broadcast(QuestID);
        CompleteCurrentQuest();
    }
    return true;
}

bool UVHVQuestSubsystem::GetQuestState(FName QuestID, FVHVQuestRuntimeState& OutState) const
{
    const FVHVQuestRuntimeState* State = FindQuestState(QuestID);
    if (!State)
    {
        OutState = FVHVQuestRuntimeState();
        return false;
    }
    OutState = *State;
    return true;
}

FVHVQuestArcRuntimeState UVHVQuestSubsystem::GetRuntimeState() const
{
    return RuntimeState;
}

TArray<FVHVQuestJournalEntry> UVHVQuestSubsystem::GetQuestJournalEntries() const
{
    TArray<FVHVQuestJournalEntry> Entries;
    if (!ActiveQuestArc)
    {
        return Entries;
    }
    for (const FVHVQuestDefinition& Quest : ActiveQuestArc->Quests)
    {
        const FVHVQuestRuntimeState* State = FindQuestState(Quest.QuestID);
        FVHVQuestJournalEntry Entry;
        if (State && BuildJournalEntry(Quest, *State, Entry))
        {
            Entries.Add(Entry);
        }
    }
    return Entries;
}

bool UVHVQuestSubsystem::IsQuestFlowActive() const
{
    return ActiveQuestArc && RuntimeState.bArcActive && !RuntimeState.bArcCompleted;
}

bool UVHVQuestSubsystem::IsCurrentObjectiveWaitingForActivation() const
{
    return GetActiveObjective()
        && (bCurrentObjectiveWaitingOnConditions
            || bCurrentObjectiveWaitingOnNPCReadiness)
        && !bCurrentObjectiveActivated;
}

bool UVHVQuestSubsystem::IsTransientObjectiveExecutionActive() const
{
    return bCurrentObjectiveWaitingOnNPCMoves
        || ActiveNPCObjectiveCommandComponent != nullptr
        || ActiveWorldActionRequestID.IsValid();
}

bool UVHVQuestSubsystem::IsCurrentObjectiveFreeRoamNPCTravelActive() const
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    return Objective
        && Objective->NPCTravelMode == EVHVQuestNPCTravelMode::FreeRoam
        && bCurrentObjectiveWaitingOnNPCMoves
        && Objective->NPCMoveStages.IsValidIndex(ActiveObjectiveNPCMoveStageIndex);
}

bool UVHVQuestSubsystem::IsParticipantInActiveFreeRoamNPCTravel(const FName ParticipantID) const
{
    return !ParticipantID.IsNone()
        && IsCurrentObjectiveFreeRoamNPCTravelActive()
        && (PendingObjectiveNPCMoveParticipants.Contains(ParticipantID)
            || ActiveObjectiveNPCMoveParticipants.FindKey(ParticipantID) != nullptr);
}

#if !UE_BUILD_SHIPPING
bool UVHVQuestSubsystem::DebugCompleteCurrentObjective()
{
    if (bDeveloperObjectiveSkipInProgress || bDeveloperQuestSkipInProgress || bCompletingCurrentObjective)
    {
        UE_LOG(LogVHV, Verbose,
            TEXT("[VHVQuest] Ignoring developer skip while an objective transition is already in progress."));
        return false;
    }

    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Cannot complete objective: there is no active objective."));
        return false;
    }

    TGuardValue<bool> DeveloperSkipGuard(bDeveloperObjectiveSkipInProgress, true);

    const FName SkippedQuestID = RuntimeState.ActiveQuestID;
    const FName SkippedObjectiveID = Objective->ObjectiveID;
    const bool bWasWaitingOnReadiness = bCurrentObjectiveWaitingOnNPCReadiness;
    const int32 ReadinessBindingCount = ActiveObjectiveNPCReadinessBindings.Num();
    const bool bWasWaitingOnMovement = bCurrentObjectiveWaitingOnNPCMoves;
    const int32 SkippedMovementStage = ActiveObjectiveNPCMoveStageIndex;
    const int32 PendingMovementParticipants = PendingObjectiveNPCMoveParticipants.Num();

    UE_LOG(LogVHV, Log,
        TEXT("[VHVQuest] Developer skip requested: Quest='%s' Objective='%s'"),
        *SkippedQuestID.ToString(),
        *SkippedObjectiveID.ToString());

    if (bWasWaitingOnReadiness)
    {
        UE_LOG(LogVHV, Log,
            TEXT("[VHVQuest] Abandoned readiness wait for skipped objective: PendingParticipants=%d"),
            ReadinessBindingCount);
    }

    if (bWasWaitingOnMovement)
    {
        UE_LOG(LogVHV, Log,
            TEXT("[VHVQuest] Abandoned movement wait for skipped objective: Stage=%d PendingParticipants=%d"),
            SkippedMovementStage,
            PendingMovementParticipants);
    }

    // Reject deferred callbacks captured by this activation before detaching delegates or
    // cancelling objective-owned commands. The next objective receives a new activation serial.
    ++ObjectiveActivationSerial;
    DebugCancelActiveObjectiveExecution();
    bCurrentObjectiveWaitingOnConditions = false;

    // Readiness-gated objectives intentionally remain inactive until their participants arrive.
    // Developer skip abandons that prerequisite, then uses the established completion path so
    // effects, persistence, broadcasts, and next-objective activation remain identical.
    const FVHVQuestObjectiveDefinition* ObjectiveAfterCleanup = GetActiveObjective();
    if (RuntimeState.ActiveQuestID != SkippedQuestID
        || !ObjectiveAfterCleanup
        || ObjectiveAfterCleanup->ObjectiveID != SkippedObjectiveID)
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHVQuest] Developer skip aborted because the active objective changed during cleanup."));
        return false;
    }

    bCurrentObjectiveActivated = true;
    const bool bCompleted = CompleteCurrentObjective();
    const FVHVQuestObjectiveDefinition* NextObjective = GetActiveObjective();
    if (bCompleted)
    {
        UE_LOG(LogVHV, Log,
            TEXT("[VHVQuest] Developer skip complete: NextObjective='%s'"),
            NextObjective ? *NextObjective->ObjectiveID.ToString() : TEXT("None"));
    }
    return bCompleted;
}

bool UVHVQuestSubsystem::DebugCompleteCurrentQuest(const FName ExpectedQuestID)
{
    if (bDeveloperObjectiveSkipInProgress || bDeveloperQuestSkipInProgress || bCompletingCurrentObjective)
    {
        UE_LOG(LogVHV, Verbose,
            TEXT("[VHVQuest] Ignoring whole-quest developer skip while another transition is in progress."));
        return false;
    }

    FVHVQuestRuntimeState* QuestState = FindQuestState(ExpectedQuestID);
    const FVHVQuestDefinition* Quest = FindQuestDefinition(ExpectedQuestID);
    if (ExpectedQuestID.IsNone() || RuntimeState.ActiveQuestID != ExpectedQuestID
        || !QuestState || !Quest || QuestState->Status != EVHVQuestStatus::Active)
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHVDeveloper] Whole-quest skip rejected because captured quest '%s' is no longer active."),
            *ExpectedQuestID.ToString());
        return false;
    }

    const int32 QuestIndex = ActiveQuestArc
        ? ActiveQuestArc->Quests.IndexOfByPredicate([ExpectedQuestID](const FVHVQuestDefinition& Candidate)
        {
            return Candidate.QuestID == ExpectedQuestID;
        })
        : INDEX_NONE;
    const FName NextQuestID = ActiveQuestArc && ActiveQuestArc->Quests.IsValidIndex(QuestIndex + 1)
        ? ActiveQuestArc->Quests[QuestIndex + 1].QuestID
        : NAME_None;

    const int32 FirstRemainingObjective = FMath::Clamp(
        QuestState->CurrentObjectiveIndex, 0, Quest->Objectives.Num());
    bool bRemainingEffectsRequireStoryState = false;
    for (int32 ObjectiveIndex = FirstRemainingObjective;
         ObjectiveIndex < Quest->Objectives.Num(); ++ObjectiveIndex)
    {
        if (!Quest->Objectives[ObjectiveIndex].CompletionEffects.IsEmpty())
        {
            bRemainingEffectsRequireStoryState = true;
            break;
        }
    }
    if (bRemainingEffectsRequireStoryState && !StoryStateSubsystem)
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVDeveloper] Whole-quest skip cannot establish required Story State for '%s'."),
            *ExpectedQuestID.ToString());
        return false;
    }

    TGuardValue<bool> ObjectiveSkipGuard(bDeveloperObjectiveSkipInProgress, true);
    TGuardValue<bool> QuestSkipGuard(bDeveloperQuestSkipInProgress, true);
    ++ObjectiveActivationSerial;
    DebugCancelActiveObjectiveExecution();
    bCurrentObjectiveActivated = false;
    bCurrentObjectiveWaitingOnConditions = false;
    bCurrentObjectiveWaitingOnNPCReadiness = false;
    bCurrentObjectiveWaitingOnNPCMoves = false;

    int32 SkippedObjectiveCount = 0;
    for (int32 ObjectiveIndex = FirstRemainingObjective; ObjectiveIndex < Quest->Objectives.Num(); ++ObjectiveIndex)
    {
        const FVHVQuestObjectiveDefinition& Objective = Quest->Objectives[ObjectiveIndex];
        if (!QuestState->CompletedObjectiveIDs.Contains(Objective.ObjectiveID))
        {
            QuestState->CompletedObjectiveIDs.Add(Objective.ObjectiveID);
            ++SkippedObjectiveCount;
        }
        if (StoryStateSubsystem && !Objective.CompletionEffects.IsEmpty()
            && !StoryStateSubsystem->ApplyEffects(Objective.CompletionEffects))
        {
            UE_LOG(LogVHV, Warning,
                TEXT("[VHVDeveloper] Quest '%s' objective '%s' had an invalid debug completion effect."),
                *ExpectedQuestID.ToString(), *Objective.ObjectiveID.ToString());
        }
    }
    QuestState->CurrentObjectiveIndex = Quest->Objectives.Num();

    UE_LOG(LogVHV, Log,
        TEXT("[VHVDeveloper] Completing captured quest '%s' directly; skipped %d remaining objectives."),
        *ExpectedQuestID.ToString(), SkippedObjectiveCount);
    CompleteCurrentQuest();

    // Normal completion unlocks the successor. Debug whole-quest skip always starts it,
    // even when an authored transition normally waits for external presentation.
    if (!NextQuestID.IsNone() && RuntimeState.ActiveQuestID.IsNone())
    {
        StartQuest(NextQuestID);
    }

    const bool bReachedExpectedState = NextQuestID.IsNone()
        ? RuntimeState.bArcCompleted && RuntimeState.ActiveQuestID.IsNone()
        : RuntimeState.ActiveQuestID == NextQuestID;
    if (bReachedExpectedState)
    {
        UE_LOG(LogVHV, Log,
            TEXT("[VHVDeveloper] Whole-quest skip result: Completed='%s' Next='%s' Active='%s' ArcCompleted=%s"),
            *ExpectedQuestID.ToString(),
            NextQuestID.IsNone() ? TEXT("EndOfArc") : *NextQuestID.ToString(),
            RuntimeState.ActiveQuestID.IsNone() ? TEXT("None") : *RuntimeState.ActiveQuestID.ToString(),
            RuntimeState.bArcCompleted ? TEXT("true") : TEXT("false"));
    }
    else
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVDeveloper] Whole-quest skip result mismatch: Completed='%s' ExpectedNext='%s' Active='%s'."),
            *ExpectedQuestID.ToString(),
            NextQuestID.IsNone() ? TEXT("EndOfArc") : *NextQuestID.ToString(),
            RuntimeState.ActiveQuestID.IsNone() ? TEXT("None") : *RuntimeState.ActiveQuestID.ToString());
    }
    return bReachedExpectedState;
}

bool UVHVQuestSubsystem::DebugRestartCurrentQuest()
{
    const FName QuestID = RuntimeState.ActiveQuestID;
    const FVHVQuestDefinition* Definition = FindQuestDefinition(QuestID);
    FVHVQuestRuntimeState* State = FindQuestState(QuestID);
    if (!Definition || !State || Definition->Objectives.IsEmpty())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Cannot restart quest: there is no active authored quest."));
        return false;
    }

    DebugCancelActiveObjectiveExecution();
    State->Status = EVHVQuestStatus::Active;
    State->CurrentObjectiveIndex = 0;
    State->CompletedObjectiveIDs.Reset();
    RuntimeState.CompletedQuestIDs.Remove(QuestID);
    RuntimeState.ActiveQuestID = QuestID;
    RuntimeState.bArcActive = true;
    RuntimeState.bArcCompleted = false;
    bCurrentObjectiveActivated = false;
    bCurrentObjectiveWaitingOnConditions = false;
    bCompletingCurrentObjective = false;

    // Q0's five market observations are persisted in Story State so an in-progress
    // save/load retains them. An explicit developer restart is the one quest
    // lifecycle operation that must discard that scoped progress.
    if (QuestID == FName(TEXT("Q_HBCT_00_INTRO")))
    {
        AVHVMarketObservationTrigger::ResetObservationProgress(StoryStateSubsystem);
    }

    if (RuntimeState.TrackedQuestID.IsNone())
    {
        RuntimeState.TrackedQuestID = QuestID;
        OnTrackedQuestChanged.Broadcast(QuestID);
    }
    ActivateCurrentObjective();
    return true;
}

void UVHVQuestSubsystem::DebugCancelActiveObjectiveExecution()
{
    ClearActiveObjectiveNPCReadinessTracking();
    ClearActiveObjectiveNPCMoveTracking(true);
    if (ActiveNPCObjectiveCommandComponent)
    {
        ActiveNPCObjectiveCommandComponent->OnQuestCommandCompleted.RemoveDynamic(this, &UVHVQuestSubsystem::HandleNPCObjectiveCommandCompleted);
        ActiveNPCObjectiveCommandComponent->CancelQuestCommand();
    }
    ClearActiveNPCActionTracking();

    if (ActiveWorldActionSubsystem && ActiveWorldActionRequestID.IsValid())
    {
        ActiveWorldActionSubsystem->OnWorldActionCompleted.RemoveDynamic(this, &UVHVQuestSubsystem::HandleWorldActionCompleted);
        ActiveWorldActionSubsystem->CompleteWorldAction(ActiveWorldActionRequestID, false);
    }
    ClearActiveWorldActionTracking();
}
#endif

void UVHVQuestSubsystem::ExportSaveState(FVHVQuestSaveState& OutSaveState) const
{
    OutSaveState = FVHVQuestSaveState();
    if (!ActiveQuestArc)
    {
        return;
    }

    OutSaveState.bHasState = true;
    OutSaveState.QuestArcAssetPath = FSoftObjectPath(ActiveQuestArc->GetPathName());
    OutSaveState.QuestArcID = RuntimeState.QuestArcID;
    OutSaveState.ActiveQuestID = RuntimeState.ActiveQuestID;
    OutSaveState.TrackedQuestID = RuntimeState.TrackedQuestID;
    OutSaveState.CompletedQuestIDs = RuntimeState.CompletedQuestIDs.Array();
    OutSaveState.CompletedQuestIDs.Sort(FNameLexicalLess());
    OutSaveState.bArcActive = RuntimeState.bArcActive;
    OutSaveState.bArcCompleted = RuntimeState.bArcCompleted;

    for (const FVHVQuestRuntimeState& RuntimeQuest : RuntimeState.QuestStates)
    {
        FVHVQuestSaveEntry SavedQuest;
        SavedQuest.QuestID = RuntimeQuest.QuestID;
        SavedQuest.Status = RuntimeQuest.Status;
        SavedQuest.CompletedObjectiveIDs = RuntimeQuest.CompletedObjectiveIDs.Array();
        SavedQuest.CompletedObjectiveIDs.Sort(FNameLexicalLess());
        if (const FVHVQuestDefinition* Definition = FindQuestDefinition(RuntimeQuest.QuestID);
            Definition && Definition->Objectives.IsValidIndex(RuntimeQuest.CurrentObjectiveIndex))
        {
            SavedQuest.CurrentObjectiveID = Definition->Objectives[RuntimeQuest.CurrentObjectiveIndex].ObjectiveID;
        }
        OutSaveState.QuestStates.Add(MoveTemp(SavedQuest));
    }
}

bool UVHVQuestSubsystem::ValidateSaveState(const FVHVQuestSaveState& SaveState) const
{
    if (!SaveState.bHasState)
    {
        return true;
    }

    UVHVQuestArcData* QuestArc = Cast<UVHVQuestArcData>(SaveState.QuestArcAssetPath.TryLoad());
    if (!QuestArc || !ValidateQuestArc(QuestArc) || QuestArc->QuestArcID != SaveState.QuestArcID)
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Saved quest arc '%s' at '%s' is missing, invalid, or has a mismatched Arc ID."),
            *SaveState.QuestArcID.ToString(), *SaveState.QuestArcAssetPath.ToString());
        return false;
    }

    TSet<FName> SavedQuestIDs;
    for (const FVHVQuestSaveEntry& SavedQuest : SaveState.QuestStates)
    {
        const FVHVQuestDefinition* Definition = QuestArc->Quests.FindByPredicate([&SavedQuest](const FVHVQuestDefinition& Quest)
        {
            return Quest.QuestID == SavedQuest.QuestID;
        });
        if (!Definition || SavedQuest.QuestID.IsNone() || SavedQuestIDs.Contains(SavedQuest.QuestID))
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Saved quest entry '%s' is missing from the authored arc or duplicated."), *SavedQuest.QuestID.ToString());
            return false;
        }
        SavedQuestIDs.Add(SavedQuest.QuestID);

        if (!SavedQuest.CurrentObjectiveID.IsNone()
            && !Definition->Objectives.ContainsByPredicate([&SavedQuest](const FVHVQuestObjectiveDefinition& Objective)
            {
                return Objective.ObjectiveID == SavedQuest.CurrentObjectiveID;
            }))
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Saved current Objective ID '%s' does not exist in quest '%s'."),
                *SavedQuest.CurrentObjectiveID.ToString(), *SavedQuest.QuestID.ToString());
            return false;
        }
        if (SavedQuest.Status == EVHVQuestStatus::Active && SavedQuest.CurrentObjectiveID.IsNone())
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Active saved quest '%s' has no current Objective ID."), *SavedQuest.QuestID.ToString());
            return false;
        }
        for (const FName CompletedObjectiveID : SavedQuest.CompletedObjectiveIDs)
        {
            if (CompletedObjectiveID.IsNone() || !Definition->Objectives.ContainsByPredicate([CompletedObjectiveID](const FVHVQuestObjectiveDefinition& Objective)
            {
                return Objective.ObjectiveID == CompletedObjectiveID;
            }))
            {
                UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Saved completed Objective ID '%s' does not exist in quest '%s'."),
                    *CompletedObjectiveID.ToString(), *SavedQuest.QuestID.ToString());
                return false;
            }
        }
    }

    if (SavedQuestIDs.Num() != QuestArc->Quests.Num())
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Saved quest state does not contain every quest in arc '%s'."), *QuestArc->QuestArcID.ToString());
        return false;
    }
    if (!SaveState.ActiveQuestID.IsNone())
    {
        const FVHVQuestSaveEntry* ActiveEntry = SaveState.QuestStates.FindByPredicate([&SaveState](const FVHVQuestSaveEntry& Entry)
        {
            return Entry.QuestID == SaveState.ActiveQuestID;
        });
        if (!ActiveEntry || ActiveEntry->Status != EVHVQuestStatus::Active)
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Saved Active Quest ID '%s' is not an active saved quest."), *SaveState.ActiveQuestID.ToString());
            return false;
        }
    }
    if (!SaveState.TrackedQuestID.IsNone() && !SavedQuestIDs.Contains(SaveState.TrackedQuestID))
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Saved Tracked Quest ID '%s' does not exist in the saved arc."), *SaveState.TrackedQuestID.ToString());
        return false;
    }
    for (const FName CompletedQuestID : SaveState.CompletedQuestIDs)
    {
        if (CompletedQuestID.IsNone() || !SavedQuestIDs.Contains(CompletedQuestID))
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Saved Completed Quest ID '%s' does not exist in the saved arc."), *CompletedQuestID.ToString());
            return false;
        }
    }
    return true;
}

bool UVHVQuestSubsystem::ImportSaveState(const FVHVQuestSaveState& SaveState)
{
    if (!ValidateSaveState(SaveState))
    {
        return false;
    }

    ClearActiveObjectiveNPCReadinessTracking();
    ClearActiveObjectiveNPCMoveTracking(true);
    if (ActiveNPCObjectiveCommandComponent)
    {
        ActiveNPCObjectiveCommandComponent->OnQuestCommandCompleted.RemoveDynamic(this, &UVHVQuestSubsystem::HandleNPCObjectiveCommandCompleted);
        ActiveNPCObjectiveCommandComponent->CancelQuestCommand();
    }
    ClearActiveNPCActionTracking();
    if (ActiveWorldActionSubsystem && ActiveWorldActionRequestID.IsValid())
    {
        ActiveWorldActionSubsystem->OnWorldActionCompleted.RemoveDynamic(this, &UVHVQuestSubsystem::HandleWorldActionCompleted);
        ActiveWorldActionSubsystem->CompleteWorldAction(ActiveWorldActionRequestID, false);
    }
    ClearActiveWorldActionTracking();
    bCurrentObjectiveActivated = false;
    bCurrentObjectiveWaitingOnConditions = false;
    bCurrentObjectiveWaitingOnNPCReadiness = false;
    bCurrentObjectiveWaitingOnNPCMoves = false;
    bCompletingCurrentObjective = false;

    if (!SaveState.bHasState)
    {
        ActiveQuestArc = nullptr;
        RuntimeState = FVHVQuestArcRuntimeState();
        if (TextbookSubsystem)
        {
            TextbookSubsystem->SetProgressionMode(EVHVTextbookProgressionMode::InternalTextbook);
        }
        OnTrackedQuestChanged.Broadcast(NAME_None);
        return true;
    }

    UVHVQuestArcData* QuestArc = Cast<UVHVQuestArcData>(SaveState.QuestArcAssetPath.TryLoad());
    if (!QuestArc)
    {
        return false;
    }
    ActiveQuestArc = QuestArc;
    FVHVQuestArcRuntimeState RestoredState;
    RestoredState.QuestArcID = SaveState.QuestArcID;
    RestoredState.ActiveQuestID = SaveState.ActiveQuestID;
    RestoredState.TrackedQuestID = SaveState.TrackedQuestID;
    RestoredState.bArcActive = SaveState.bArcActive;
    RestoredState.bArcCompleted = SaveState.bArcCompleted;
    for (const FName CompletedQuestID : SaveState.CompletedQuestIDs)
    {
        RestoredState.CompletedQuestIDs.Add(CompletedQuestID);
    }

    for (const FVHVQuestDefinition& Definition : QuestArc->Quests)
    {
        const FVHVQuestSaveEntry* SavedQuest = SaveState.QuestStates.FindByPredicate([&Definition](const FVHVQuestSaveEntry& Entry)
        {
            return Entry.QuestID == Definition.QuestID;
        });
        check(SavedQuest);

        FVHVQuestRuntimeState RuntimeQuest;
        RuntimeQuest.QuestID = SavedQuest->QuestID;
        RuntimeQuest.Status = SavedQuest->Status;
        RuntimeQuest.CurrentObjectiveIndex = Definition.Objectives.IndexOfByPredicate([SavedQuest](const FVHVQuestObjectiveDefinition& Objective)
        {
            return Objective.ObjectiveID == SavedQuest->CurrentObjectiveID;
        });
        if (RuntimeQuest.CurrentObjectiveIndex == INDEX_NONE)
        {
            RuntimeQuest.CurrentObjectiveIndex = Definition.Objectives.Num();
        }
        for (const FName ObjectiveID : SavedQuest->CompletedObjectiveIDs)
        {
            RuntimeQuest.CompletedObjectiveIDs.Add(ObjectiveID);
        }
        RestoredState.QuestStates.Add(MoveTemp(RuntimeQuest));
    }

    RuntimeState = MoveTemp(RestoredState);
    EnsureStoryStateDelegateBindings();
    if (TextbookSubsystem)
    {
        TextbookSubsystem->SetProgressionMode(RuntimeState.bArcActive
            ? EVHVTextbookProgressionMode::QuestManaged
            : EVHVTextbookProgressionMode::InternalTextbook);
    }

    OnTrackedQuestChanged.Broadcast(RuntimeState.TrackedQuestID);
    for (const FVHVQuestRuntimeState& QuestState : RuntimeState.QuestStates)
    {
        if (QuestState.QuestID != RuntimeState.ActiveQuestID)
        {
            OnQuestUpdated.Broadcast(QuestState.QuestID);
        }
    }
    RestorePersistentNPCMoves();
    if (!RuntimeState.ActiveQuestID.IsNone())
    {
        ActivateCurrentObjective();
    }
    return true;
}

bool UVHVQuestSubsystem::RequestCurrentObjectiveActivation()
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective || !bCurrentObjectiveActivated || bCurrentObjectiveWaitingOnNPCMoves)
    {
        return false;
    }
    if (!Objective->NPCMoveStages.IsEmpty()
        && !Objective->NPCMoveStageActivationConditions.Conditions.IsEmpty()
        && (!StoryStateSubsystem
            || !StoryStateSubsystem->EvaluateConditionSet(Objective->NPCMoveStageActivationConditions)))
    {
        return false;
    }
    OnObjectiveActivationRequested.Broadcast(RuntimeState.ActiveQuestID, Objective->ObjectiveID);
    return true;
}

bool UVHVQuestSubsystem::NotifyParticipantInteracted(FName ParticipantID)
{
    if (!CanParticipantInteract(ParticipantID))
    {
        return false;
    }

    UVHVConversationDataAsset* ContextualConversation = nullptr;
    FName ContextualEntryNode;
    if (TryGetCurrentContextualConversation(ParticipantID, ContextualConversation, ContextualEntryNode))
    {
        return true;
    }

    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (Objective->ObjectiveType == EVHVQuestObjectiveType::Interact && Objective->GetEffectiveTargetID() == ParticipantID)
    {
        return CompleteCurrentObjective();
    }

    const bool bLaunchable = Objective->ObjectiveType == EVHVQuestObjectiveType::Talk
        || Objective->ObjectiveType == EVHVQuestObjectiveType::Conversation
        || Objective->ObjectiveType == EVHVQuestObjectiveType::LearningActivity;
    const FName TargetParticipantID = Objective->GetEffectiveTargetID();
    if (bLaunchable && (TargetParticipantID.IsNone() || TargetParticipantID == ParticipantID))
    {
        return RequestCurrentObjectiveActivation();
    }
    return false;
}

bool UVHVQuestSubsystem::CanParticipantInteract(const FName ParticipantID) const
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective || ParticipantID.IsNone() || !bCurrentObjectiveActivated
        || bCurrentObjectiveWaitingOnNPCMoves || bCompletingCurrentObjective)
    {
        return false;
    }

    UVHVConversationDataAsset* ContextualConversation = nullptr;
    FName ContextualEntryNode;
    if (TryGetCurrentContextualConversation(ParticipantID, ContextualConversation, ContextualEntryNode))
    {
        return true;
    }

    const FName TargetParticipantID = Objective->GetEffectiveTargetID();
    if (TargetParticipantID != ParticipantID)
    {
        return false;
    }

    switch (Objective->ObjectiveType)
    {
    case EVHVQuestObjectiveType::Interact:
        return true;
    case EVHVQuestObjectiveType::Talk:
        return !GetRequiredConversationID(*Objective).IsNone();
    case EVHVQuestObjectiveType::Conversation:
        return Objective->bAllowParticipantActivation
            && !Objective->bAutoStart && !GetRequiredConversationID(*Objective).IsNone();
    case EVHVQuestObjectiveType::LearningActivity:
        return Objective->bAllowParticipantActivation
            && !Objective->bAutoStart && !Objective->GetEffectiveActivityID().IsNone();
    default:
        return false;
    }
}

void UVHVQuestSubsystem::NotifyConversationCompleted(FName ConversationID)
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (Objective && (Objective->ObjectiveType == EVHVQuestObjectiveType::Talk || Objective->ObjectiveType == EVHVQuestObjectiveType::Conversation)
        && GetRequiredConversationID(*Objective) == ConversationID)
    {
        CompleteCurrentObjective();
    }
}

void UVHVQuestSubsystem::NotifyActivityCompleted(FName ActivityID)
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (Objective && Objective->ObjectiveType == EVHVQuestObjectiveType::LearningActivity && Objective->GetEffectiveActivityID() == ActivityID)
    {
        CompleteCurrentObjective();
    }
}

void UVHVQuestSubsystem::NotifyLocationReached(FName LocationID)
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (Objective && Objective->ObjectiveType == EVHVQuestObjectiveType::ReachLocation && Objective->GetEffectiveTargetID() == LocationID)
    {
        CompleteCurrentObjective();
    }
}

void UVHVQuestSubsystem::NotifyCustomEvent(FName EventID)
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (Objective && Objective->ObjectiveType == EVHVQuestObjectiveType::CustomEvent && Objective->GetEffectiveTargetID() == EventID)
    {
        CompleteCurrentObjective();
    }
}

bool UVHVQuestSubsystem::RequestNPCMove(
    const FName ParticipantID,
    const FName TargetID,
    const bool bFaceDestinationRotation,
    const float MoveSpeedOverride)
{
    UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(GetWorld(), ParticipantID);
    return CommandComponent && CommandComponent->MoveToTarget(
        TargetID, bFaceDestinationRotation, MoveSpeedOverride);
}

bool UVHVQuestSubsystem::RequestNPCWait(const FName ParticipantID, const float Duration)
{
    UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(GetWorld(), ParticipantID);
    return CommandComponent && CommandComponent->Wait(Duration);
}

bool UVHVQuestSubsystem::RequestNPCReturnToPost(const FName ParticipantID)
{
    UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(GetWorld(), ParticipantID);
    return CommandComponent && CommandComponent->ReturnToPost();
}

bool UVHVQuestSubsystem::RequestNPCPlayAction(const FName ParticipantID, const FName ActionID)
{
    UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(GetWorld(), ParticipantID);
    return CommandComponent && CommandComponent->PlayAction(ActionID);
}

bool UVHVQuestSubsystem::ReleaseNPCFromQuest(const FName ParticipantID)
{
    UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(GetWorld(), ParticipantID);
    return CommandComponent && CommandComponent->ReleaseToPatrol();
}

bool UVHVQuestSubsystem::CancelNPCQuestCommand(const FName ParticipantID)
{
    UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(GetWorld(), ParticipantID);
    if (!CommandComponent)
    {
        return false;
    }
    CommandComponent->CancelQuestCommand();
    return true;
}

bool UVHVQuestSubsystem::RegisterNPCBehaviorTarget(AVHVNPCBehaviorTarget* Target)
{
    const FName TargetID = IsValid(Target) ? Target->GetEffectiveTargetID() : NAME_None;
    if (!IsValid(Target) || TargetID.IsNone() || !Target->GetWorld())
    {
        return false;
    }

    FBehaviorTargetRegistry& Registry = BehaviorTargetsByWorld.FindOrAdd(Target->GetWorld());
    if (const TWeakObjectPtr<AVHVNPCBehaviorTarget>* Existing = Registry.Find(TargetID))
    {
        if (Existing->IsValid() && Existing->Get() != Target)
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Duplicate NPC behavior Target ID '%s' in world '%s' on '%s' and '%s'."),
                *TargetID.ToString(), *GetNameSafe(Target->GetWorld()), *GetNameSafe(Existing->Get()), *GetNameSafe(Target));
            return false;
        }
    }
    Registry.Add(TargetID, Target);
    return true;
}

void UVHVQuestSubsystem::UnregisterNPCBehaviorTarget(AVHVNPCBehaviorTarget* Target)
{
    if (!Target || !Target->GetWorld())
    {
        return;
    }
    if (FBehaviorTargetRegistry* Registry = BehaviorTargetsByWorld.Find(Target->GetWorld()))
    {
        const FName TargetID = Target->GetEffectiveTargetID();
        if (const TWeakObjectPtr<AVHVNPCBehaviorTarget>* Existing = Registry->Find(TargetID);
            Existing && Existing->Get() == Target)
        {
            Registry->Remove(TargetID);
        }
        if (Registry->IsEmpty())
        {
            BehaviorTargetsByWorld.Remove(Target->GetWorld());
        }
    }
}

AVHVNPCBehaviorTarget* UVHVQuestSubsystem::FindNPCBehaviorTarget(const UWorld* World, const FName TargetID) const
{
    if (!World || TargetID.IsNone())
    {
        return nullptr;
    }
    const FBehaviorTargetRegistry* Registry = BehaviorTargetsByWorld.Find(World);
    const TWeakObjectPtr<AVHVNPCBehaviorTarget>* Target = Registry ? Registry->Find(TargetID) : nullptr;
    return Target ? Target->Get() : nullptr;
}

AActor* UVHVQuestSubsystem::FindNPCMovementTarget(const UWorld* World, const FName TargetID) const
{
    if (!World || TargetID.IsNone())
    {
        return nullptr;
    }
    if (AVHVNPCBehaviorTarget* BehaviorTarget = FindNPCBehaviorTarget(World, TargetID))
    {
        return BehaviorTarget;
    }

    AVHVQuestLocationVolume* FoundLocation = nullptr;
    for (TActorIterator<AVHVQuestLocationVolume> It(World); It; ++It)
    {
        if (It->GetEffectiveLocationID() != TargetID)
        {
            continue;
        }
        if (FoundLocation)
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Multiple semantic location actors resolve movement destination '%s'."), *TargetID.ToString());
            return nullptr;
        }
        FoundLocation = *It;
    }
    if (FoundLocation)
    {
        return FoundLocation;
    }

    AVHVMarketObservationTrigger* FoundMarketTrigger = nullptr;
    for (TActorIterator<AVHVMarketObservationTrigger> It(World); It; ++It)
    {
        if (It->GetTrackingLocationID() != TargetID)
        {
            continue;
        }
        if (FoundMarketTrigger)
        {
            UE_LOG(LogVHV, Error,
                TEXT("[VHVTracking] Multiple market observation triggers resolve destination '%s'."),
                *TargetID.ToString());
            return nullptr;
        }
        FoundMarketTrigger = *It;
    }
    return FoundMarketTrigger;
}

bool UVHVQuestSubsystem::RegisterNPCCommandComponent(UVHVNPCQuestCommandComponent* CommandComponent)
{
    if (!IsValid(CommandComponent) || !CommandComponent->GetWorld())
    {
        return false;
    }
    const UVHVQuestParticipantComponent* Participant = CommandComponent->GetOwner()
        ? CommandComponent->GetOwner()->FindComponentByClass<UVHVQuestParticipantComponent>()
        : nullptr;
    if (Participant && !Participant->IsQuestParticipationEnabled())
    {
        // Ambient/background NPCs retain the reusable quest command component,
        // but do not enter the stable-ID registry until quest participation is enabled.
        return false;
    }
    const FName ParticipantID = Participant ? Participant->GetEffectiveParticipantID() : NAME_None;
    if (!Participant || ParticipantID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] NPC command component on '%s' requires a non-empty Participant ID."), *GetNameSafe(CommandComponent->GetOwner()));
        return false;
    }

    FNPCCommandRegistry& Registry = NPCCommandsByWorld.FindOrAdd(CommandComponent->GetWorld());
    if (const TWeakObjectPtr<UVHVNPCQuestCommandComponent>* Existing = Registry.Find(ParticipantID))
    {
        if (Existing->IsValid() && Existing->Get() != CommandComponent)
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Duplicate NPC Participant ID '%s' in world '%s' on '%s' and '%s'."),
                *ParticipantID.ToString(), *GetNameSafe(CommandComponent->GetWorld()),
                *GetNameSafe(Existing->Get()->GetOwner()), *GetNameSafe(CommandComponent->GetOwner()));
            return false;
        }
    }
    Registry.Add(ParticipantID, CommandComponent);
    RestorePersistentNPCMoveForParticipant(ParticipantID);
    ReevaluateWaitingObjective();
    return true;
}

void UVHVQuestSubsystem::DispatchNPCMoves(const TArray<FVHVQuestNPCMoveRequest>& Moves)
{
    for (const FVHVQuestNPCMoveRequest& Move : Moves)
    {
        const FName ParticipantID = Move.GetEffectiveParticipantID();
        const FName DestinationID = Move.GetEffectiveDestinationID();
        if (!RequestNPCMove(
            ParticipantID, DestinationID, Move.bFaceDestinationRotation, Move.MoveSpeedOverride))
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Could not move participant '%s' to semantic location '%s'."),
                *ParticipantID.ToString(), *DestinationID.ToString());
        }
    }
}

void UVHVQuestSubsystem::RestorePersistentNPCMoves()
{
    if (!ActiveQuestArc)
    {
        return;
    }

    TMap<FName, FVHVQuestNPCMoveRequest> LatestMoves;
    for (const FVHVQuestNPCMoveRequest& Move : ActiveQuestArc->InitialNPCMoves)
    {
        LatestMoves.Add(Move.GetEffectiveParticipantID(), Move);
    }
    for (const FVHVQuestDefinition& Quest : ActiveQuestArc->Quests)
    {
        const FVHVQuestRuntimeState* QuestState = FindQuestState(Quest.QuestID);
        if (!QuestState)
        {
            continue;
        }
        for (const FVHVQuestObjectiveDefinition& Objective : Quest.Objectives)
        {
            if (!QuestState->CompletedObjectiveIDs.Contains(Objective.ObjectiveID))
            {
                continue;
            }
            if (!Objective.NPCMoveStages.IsEmpty())
            {
                for (const FVHVQuestNPCMoveRequest& Move : Objective.NPCMoveStages.Last().Moves)
                {
                    LatestMoves.Add(Move.GetEffectiveParticipantID(), Move);
                }
            }
            for (const FVHVQuestNPCMoveRequest& Move : Objective.CompletionNPCMoves)
            {
                LatestMoves.Add(Move.GetEffectiveParticipantID(), Move);
            }
        }
    }

    TArray<FVHVQuestNPCMoveRequest> Moves;
    LatestMoves.GenerateValueArray(Moves);
    DispatchNPCMoves(Moves);
}

void UVHVQuestSubsystem::RestorePersistentNPCMoveForParticipant(const FName ParticipantID)
{
    if (!ActiveQuestArc || ParticipantID.IsNone())
    {
        return;
    }

    const FVHVQuestNPCMoveRequest* LatestMove = ActiveQuestArc->InitialNPCMoves.FindByPredicate(
        [ParticipantID](const FVHVQuestNPCMoveRequest& Move)
        {
            return Move.GetEffectiveParticipantID() == ParticipantID;
        });
    for (const FVHVQuestDefinition& Quest : ActiveQuestArc->Quests)
    {
        const FVHVQuestRuntimeState* QuestState = FindQuestState(Quest.QuestID);
        if (!QuestState)
        {
            continue;
        }
        for (const FVHVQuestObjectiveDefinition& Objective : Quest.Objectives)
        {
            if (!QuestState->CompletedObjectiveIDs.Contains(Objective.ObjectiveID))
            {
                continue;
            }
            if (!Objective.NPCMoveStages.IsEmpty())
            {
                if (const FVHVQuestNPCMoveRequest* Move = Objective.NPCMoveStages.Last().Moves.FindByPredicate(
                    [ParticipantID](const FVHVQuestNPCMoveRequest& Candidate)
                    {
                        return Candidate.GetEffectiveParticipantID() == ParticipantID;
                    }))
                {
                    LatestMove = Move;
                }
            }
            if (const FVHVQuestNPCMoveRequest* Move = Objective.CompletionNPCMoves.FindByPredicate(
                [ParticipantID](const FVHVQuestNPCMoveRequest& Candidate)
                {
                    return Candidate.GetEffectiveParticipantID() == ParticipantID;
                }))
            {
                LatestMove = Move;
            }
        }
    }
    if (LatestMove)
    {
        RequestNPCMove(
            ParticipantID,
            LatestMove->GetEffectiveDestinationID(),
            LatestMove->bFaceDestinationRotation,
            LatestMove->MoveSpeedOverride);
    }
}

void UVHVQuestSubsystem::UnregisterNPCCommandComponent(UVHVNPCQuestCommandComponent* CommandComponent)
{
    if (!CommandComponent || !CommandComponent->GetWorld())
    {
        return;
    }
    const UVHVQuestParticipantComponent* Participant = CommandComponent->GetOwner()
        ? CommandComponent->GetOwner()->FindComponentByClass<UVHVQuestParticipantComponent>()
        : nullptr;
    if (!Participant)
    {
        return;
    }
    if (FNPCCommandRegistry* Registry = NPCCommandsByWorld.Find(CommandComponent->GetWorld()))
    {
        const FName ParticipantID = Participant->GetEffectiveParticipantID();
        if (const TWeakObjectPtr<UVHVNPCQuestCommandComponent>* Existing = Registry->Find(ParticipantID);
            Existing && Existing->Get() == CommandComponent)
        {
            Registry->Remove(ParticipantID);
        }
        if (Registry->IsEmpty())
        {
            NPCCommandsByWorld.Remove(CommandComponent->GetWorld());
        }
    }
}

UVHVNPCQuestCommandComponent* UVHVQuestSubsystem::FindNPCCommandComponent(const UWorld* World, const FName ParticipantID) const
{
    if (!World || ParticipantID.IsNone())
    {
        return nullptr;
    }
    const FNPCCommandRegistry* Registry = NPCCommandsByWorld.Find(World);
    const TWeakObjectPtr<UVHVNPCQuestCommandComponent>* Command = Registry ? Registry->Find(ParticipantID) : nullptr;
    return Command ? Command->Get() : nullptr;
}

void UVHVQuestSubsystem::HandleTextbookActivityCompleted()
{
    if (TextbookSubsystem)
    {
        NotifyActivityCompleted(FName(*TextbookSubsystem->GetCurrentActivity().GetEffectiveActivityID()));
    }
}

void UVHVQuestSubsystem::HandleStoryFlagChanged(const FName FlagID, const bool bValue)
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    const bool bTrackingTargetMayHaveChanged = Objective
        && DoesTrackingDestinationReferenceState(*Objective, FlagID);
    static const FName SelfMonitoringQuestID(TEXT("Q_HBCT_05_SELF_MONITORING"));
    static const FName AdjustedRoutineObjectiveID(TEXT("O14_PracticeAdjustedRoutine"));
    static const FName ConversationCompleteFlag(TEXT("SMAdjustedWalkConversationComplete"));
    static const FName RouteCompleteFlag(TEXT("SMAdjustedWalkRouteComplete"));
    static const FName RecordedFlag(TEXT("SMAdjustedWalkRecordedAccurately"));
    static const FName ReflectionCompleteFlag(TEXT("SMAdjustedRoutineReflectionComplete"));
    const bool bAdjustedRoutineObjective = Objective
        && RuntimeState.ActiveQuestID == SelfMonitoringQuestID
        && Objective->ObjectiveID == AdjustedRoutineObjectiveID;
    const bool bAdjustedRoutinePhaseChanged = bAdjustedRoutineObjective
        && (FlagID == ConversationCompleteFlag
            || FlagID == RouteCompleteFlag
            || FlagID == RecordedFlag
            || FlagID == ReflectionCompleteFlag);

    // This flag is applied immediately before the final intro bubble closes.
    // Defer route dispatch one frame so Chai cannot walk away under that bubble.
    if (bAdjustedRoutineObjective && bValue && FlagID == ConversationCompleteFlag && GetWorld())
    {
        const uint32 ExpectedActivationSerial = ObjectiveActivationSerial;
        GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
            this, [this, ExpectedActivationSerial]()
            {
                if (ObjectiveActivationSerial == ExpectedActivationSerial)
                {
                    TryStartActiveObjectiveNPCMoveSequenceFromConditions();
                }
            }));
    }
    else
    {
        TryStartActiveObjectiveNPCMoveSequenceFromConditions();
    }
    if (!TryCompleteCurrentObjectiveFromStoryState())
    {
        if (bTrackingTargetMayHaveChanged && Objective)
        {
            OnObjectiveTrackingTargetChanged.Broadcast(RuntimeState.ActiveQuestID, Objective->ObjectiveID);
            OnQuestUpdated.Broadcast(RuntimeState.ActiveQuestID);
        }
        ReevaluateWaitingObjective();
    }
    if (bAdjustedRoutinePhaseChanged)
    {
        OnQuestUpdated.Broadcast(RuntimeState.ActiveQuestID);
    }
}

void UVHVQuestSubsystem::HandleStoryCounterChanged(const FName CounterID, const int32 NewValue)
{
    (void)NewValue;
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    const bool bTrackingTargetMayHaveChanged = Objective
        && DoesTrackingDestinationReferenceState(*Objective, CounterID);
    TryStartActiveObjectiveNPCMoveSequenceFromConditions();
    if (!TryCompleteCurrentObjectiveFromStoryState())
    {
        if (bTrackingTargetMayHaveChanged && Objective)
        {
            OnObjectiveTrackingTargetChanged.Broadcast(RuntimeState.ActiveQuestID, Objective->ObjectiveID);
            OnQuestUpdated.Broadcast(RuntimeState.ActiveQuestID);
        }
        ReevaluateWaitingObjective();
    }
}

bool UVHVQuestSubsystem::ValidateQuestArc(const UVHVQuestArcData* QuestArc) const
{
    if (!QuestArc)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Cannot register a null quest arc."));
        return false;
    }

    bool bValid = true;
    if (QuestArc->QuestArcID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest arc '%s' has an empty Arc ID."), *GetNameSafe(QuestArc));
        bValid = false;
    }
    if (QuestArc->Quests.Num() == 0)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest arc '%s' has no quests."), *QuestArc->QuestArcID.ToString());
        bValid = false;
    }

    TSet<FName> InitialMoveParticipants;
    for (const FVHVQuestNPCMoveRequest& Move : QuestArc->InitialNPCMoves)
    {
        const FName ParticipantID = Move.GetEffectiveParticipantID();
        const FName DestinationID = Move.GetEffectiveDestinationID();
        if (!VHVAuthoringReferences::IsValidReferenceTag(Move.ParticipantTag, TEXT("VHV.Participant"))
            || !VHVAuthoringReferences::IsValidReferenceTag(Move.DestinationLocationTag, TEXT("VHV.Location"))
            || ParticipantID.IsNone() || DestinationID.IsNone())
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Arc '%s' has an invalid initial NPC semantic-location move."), *QuestArc->QuestArcID.ToString());
            bValid = false;
        }
        else if (InitialMoveParticipants.Contains(ParticipantID))
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Arc '%s' has multiple initial moves for participant '%s'."),
                *QuestArc->QuestArcID.ToString(), *ParticipantID.ToString());
            bValid = false;
        }
        InitialMoveParticipants.Add(ParticipantID);
    }

    TSet<FName> QuestIDs;
    for (const FVHVQuestDefinition& Quest : QuestArc->Quests)
    {
        if (Quest.QuestID.IsNone())
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Arc '%s' contains an empty Quest ID."), *QuestArc->QuestArcID.ToString());
            bValid = false;
        }
        else if (QuestIDs.Contains(Quest.QuestID))
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Duplicate Quest ID '%s'."), *Quest.QuestID.ToString());
            bValid = false;
        }
        QuestIDs.Add(Quest.QuestID);

        if (Quest.Objectives.Num() == 0)
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' has zero objectives."), *Quest.QuestID.ToString());
            bValid = false;
        }

        TSet<FName> ObjectiveIDs;
        for (const FVHVQuestObjectiveDefinition& Objective : Quest.Objectives)
        {
            if (Objective.ObjectiveID.IsNone())
            {
                UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' contains an empty Objective ID."), *Quest.QuestID.ToString());
                bValid = false;
            }
            else if (ObjectiveIDs.Contains(Objective.ObjectiveID))
            {
                UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' has duplicate Objective ID '%s'."), *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                bValid = false;
            }
            ObjectiveIDs.Add(Objective.ObjectiveID);

            const FName EffectiveTargetID = Objective.GetEffectiveTargetID();
            const FName EffectiveActivityID = Objective.GetEffectiveActivityID();
            const FName EffectiveNPCParticipantID = Objective.GetEffectiveNPCParticipantID();
            const FName EffectiveNPCTargetID = Objective.GetEffectiveNPCTargetID();
            const FName EffectiveNPCActionID = Objective.GetEffectiveNPCActionID();
            const FName EffectiveWorldReceiverID = Objective.GetEffectiveWorldActionReceiverID();
            const FName EffectiveWorldActionID = Objective.GetEffectiveWorldActionID();

            auto ValidateReference = [&bValid, &Quest, &Objective](const FGameplayTag& Tag, const FName LegacyID, const TCHAR* Category, const TCHAR* FieldName)
            {
                if (!VHVAuthoringReferences::IsValidReferenceTag(Tag, Category))
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' field %s uses tag '%s', which must be a concrete tag beneath %s."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), FieldName, *Tag.ToString(), Category);
                    bValid = false;
                }
                if (VHVAuthoringReferences::HasConflict(Tag, LegacyID, Category))
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' field %s tag '%s' resolves to '%s' while legacy ID is '%s'; the tag wins."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), FieldName, *Tag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Tag).ToString(), *LegacyID.ToString());
                }
            };

            if (Objective.ObjectiveType == EVHVQuestObjectiveType::LearningActivity)
            {
                ValidateReference(Objective.ActivityTag, Objective.ActivityID, TEXT("VHV.Activity"), TEXT("Activity"));
            }
            if (Objective.ObjectiveType == EVHVQuestObjectiveType::ReachLocation)
            {
                ValidateReference(Objective.LocationTag, Objective.TargetID, TEXT("VHV.Location"), TEXT("Location"));
            }
            else if (Objective.ObjectiveType == EVHVQuestObjectiveType::CustomEvent)
            {
                ValidateReference(Objective.CustomEventTag, Objective.TargetID, TEXT("VHV.CustomEvent"), TEXT("CustomEvent"));
            }
            else if (Objective.ObjectiveType == EVHVQuestObjectiveType::Talk
                || Objective.ObjectiveType == EVHVQuestObjectiveType::Conversation
                || Objective.ObjectiveType == EVHVQuestObjectiveType::LearningActivity
                || Objective.ObjectiveType == EVHVQuestObjectiveType::Interact)
            {
                ValidateReference(Objective.ParticipantTag, Objective.TargetID, TEXT("VHV.Participant"), TEXT("Participant"));
            }

            if (Objective.ObjectiveType == EVHVQuestObjectiveType::LearningActivity && EffectiveActivityID.IsNone())
            {
                UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Learning objective '%s' has no Activity ID."), *Objective.ObjectiveID.ToString());
                bValid = false;
            }
            if ((Objective.ObjectiveType == EVHVQuestObjectiveType::Talk || Objective.ObjectiveType == EVHVQuestObjectiveType::Conversation) && Objective.Conversation.IsNull())
            {
                UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Conversation objective '%s' has no conversation asset."), *Objective.ObjectiveID.ToString());
                bValid = false;
            }
            else if (Objective.ObjectiveType == EVHVQuestObjectiveType::Talk || Objective.ObjectiveType == EVHVQuestObjectiveType::Conversation)
            {
                UVHVConversationDataAsset* ConversationAsset = Objective.Conversation.LoadSynchronous();
                if (!ConversationAsset || ConversationAsset->Conversation.ConversationID.IsEmpty())
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Conversation objective '%s' has an invalid asset or empty Conversation ID."), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                else if (!Objective.EntryNodeID.IsNone() && !ConversationAsset->Conversation.Nodes.ContainsByPredicate([&Objective](const FDialogueNode& Node)
                {
                    return Node.NodeID == Objective.EntryNodeID.ToString();
                }))
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Objective '%s' references missing entry node '%s'."), *Objective.ObjectiveID.ToString(), *Objective.EntryNodeID.ToString());
                    bValid = false;
                }
            }
            if (Objective.ObjectiveType == EVHVQuestObjectiveType::Talk && EffectiveTargetID.IsNone())
            {
                UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Talk objective '%s' has no participant Target ID."), *Objective.ObjectiveID.ToString());
                bValid = false;
            }
            if ((Objective.ObjectiveType == EVHVQuestObjectiveType::Interact || Objective.ObjectiveType == EVHVQuestObjectiveType::ReachLocation || Objective.ObjectiveType == EVHVQuestObjectiveType::CustomEvent) && EffectiveTargetID.IsNone())
            {
                UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Objective '%s' requires a Target ID."), *Objective.ObjectiveID.ToString());
                bValid = false;
            }
            if (Objective.ObjectiveType == EVHVQuestObjectiveType::NPCAction)
            {
                ValidateReference(Objective.NPCParticipantTag, Objective.NPCParticipantID, TEXT("VHV.Participant"), TEXT("NPCParticipant"));
                if (EffectiveNPCParticipantID.IsNone())
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' NPCAction objective '%s' requires an NPC Participant ID."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                if (Objective.NPCCommandType == EVHVNPCQuestCommandType::None)
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' NPCAction objective '%s' requires an NPC command type."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                else if (Objective.NPCCommandType == EVHVNPCQuestCommandType::MoveToTarget && EffectiveNPCTargetID.IsNone())
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' NPCAction objective '%s' with MoveToTarget requires an NPC Target ID."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                else if (Objective.NPCCommandType == EVHVNPCQuestCommandType::Wait && Objective.NPCWaitDuration < 0.0f)
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' NPCAction objective '%s' has a negative NPC Wait Duration (%g)."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), Objective.NPCWaitDuration);
                    bValid = false;
                }
                else if (Objective.NPCCommandType == EVHVNPCQuestCommandType::PlayAction && EffectiveNPCActionID.IsNone())
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' NPCAction objective '%s' with PlayAction requires an NPC Action ID."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                if (Objective.NPCCommandType == EVHVNPCQuestCommandType::MoveToTarget)
                {
                    ValidateReference(Objective.NPCBehaviorTargetTag, Objective.NPCTargetID, TEXT("VHV.BehaviorTarget"), TEXT("NPCBehaviorTarget"));
                }
                else if (Objective.NPCCommandType == EVHVNPCQuestCommandType::PlayAction)
                {
                    ValidateReference(Objective.NPCActionTag, Objective.NPCActionID, TEXT("VHV.NPCAction"), TEXT("NPCAction"));
                }
            }
            if (Objective.ObjectiveType == EVHVQuestObjectiveType::WorldAction)
            {
                ValidateReference(Objective.WorldActionReceiverTag, Objective.WorldActionReceiverID, TEXT("VHV.WorldReceiver"), TEXT("WorldReceiver"));
                ValidateReference(Objective.WorldActionTag, Objective.WorldActionID, TEXT("VHV.WorldAction"), TEXT("WorldAction"));
                if (EffectiveWorldReceiverID.IsNone())
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' WorldAction objective '%s' requires a World Action Receiver ID."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                if (EffectiveWorldActionID.IsNone())
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' WorldAction objective '%s' requires a World Action ID."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
            }

            TSet<FName> ReadinessParticipants;
            for (const FVHVQuestNPCMoveRequest& Requirement : Objective.ActivationNPCReadiness)
            {
                const FName ParticipantID = Requirement.GetEffectiveParticipantID();
                const FName DestinationID = Requirement.GetEffectiveDestinationID();
                if (!VHVAuthoringReferences::IsValidReferenceTag(Requirement.ParticipantTag, TEXT("VHV.Participant"))
                    || !VHVAuthoringReferences::IsValidReferenceTag(Requirement.DestinationLocationTag, TEXT("VHV.Location"))
                    || ParticipantID.IsNone() || DestinationID.IsNone())
                {
                    UE_LOG(LogVHV, Warning,
                        TEXT("[VHVQuest] Quest '%s' objective '%s' has an invalid activation NPC readiness requirement."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                else if (ReadinessParticipants.Contains(ParticipantID))
                {
                    UE_LOG(LogVHV, Warning,
                        TEXT("[VHVQuest] Quest '%s' objective '%s' has multiple activation requirements for participant '%s'."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), *ParticipantID.ToString());
                    bValid = false;
                }
                ReadinessParticipants.Add(ParticipantID);
            }

            if (Objective.bEnsureParticipantPresentAtLocation)
            {
                if (Objective.bPrepareActivationNPCReadinessBeforeExposure)
                {
                    UE_LOG(LogVHV, Warning,
                        TEXT("[VHVQuest] Quest '%s' objective '%s' cannot combine stationary participant presence with one-time activation readiness preparation."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                if (!Objective.RequiredParticipantSpawnClass.IsNull()
                    && !Objective.ParticipantTag.IsValid())
                {
                    UE_LOG(LogVHV, Warning,
                        TEXT("[VHVQuest] Quest '%s' objective '%s' has a participant fallback class but no semantic participant tag."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                if (!Objective.NPCMoveStages.IsEmpty()
                    || Objective.NPCTravelMode == EVHVQuestNPCTravelMode::FreeRoam)
                {
                    UE_LOG(LogVHV, Warning,
                        TEXT("[VHVQuest] Quest '%s' objective '%s' cannot guarantee stationary participant presence while also owning travel stages."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                if (Objective.ActivationNPCReadiness.IsEmpty())
                {
                    ValidateReference(Objective.ParticipantTag, NAME_None, TEXT("VHV.Participant"), TEXT("PresenceParticipant"));
                    ValidateReference(Objective.LocationTag, NAME_None, TEXT("VHV.Location"), TEXT("PresenceLocation"));
                    if (Objective.GetEffectiveParticipantID().IsNone()
                        || Objective.GetEffectiveLocationID().IsNone())
                    {
                        UE_LOG(LogVHV, Warning,
                            TEXT("[VHVQuest] Quest '%s' objective '%s' enables participant presence without a participant/location pair."),
                            *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                        bValid = false;
                    }
                }
            }
            else if (Objective.bPrepareActivationNPCReadinessBeforeExposure
                && Objective.ActivationNPCReadiness.IsEmpty())
            {
                UE_LOG(LogVHV, Warning,
                    TEXT("[VHVQuest] Quest '%s' objective '%s' enables activation readiness preparation without any readiness requirements."),
                    *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                bValid = false;
            }
            else if (Objective.bPrepareActivationNPCReadinessBeforeExposure
                && !Objective.NPCMoveStages.IsEmpty()
                && Objective.NPCMoveStageActivationConditions.Conditions.IsEmpty())
            {
                UE_LOG(LogVHV, Warning,
                    TEXT("[VHVQuest] Quest '%s' objective '%s' prepares activation readiness before exposure but has ungated travel stages."),
                    *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                bValid = false;
            }

            if (Objective.bEnableLocationTracking)
            {
                if (Objective.TrackingLocationTag.IsValid())
                {
                    ValidateReference(
                        Objective.TrackingLocationTag, NAME_None,
                        TEXT("VHV.Location"), TEXT("TrackingLocation"));
                }
                if (Objective.GetEffectiveTrackingLocationID().IsNone()
                    && Objective.TrackingDestinations.IsEmpty())
                {
                    UE_LOG(LogVHV, Warning,
                        TEXT("[VHVQuest] Quest '%s' objective '%s' enables location tracking without one resolvable authored location."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                TSet<FName> TrackingLocationIDs;
                for (const FVHVQuestTrackingDestination& Destination : Objective.TrackingDestinations)
                {
                    const FName DestinationID = Destination.GetEffectiveLocationID();
                    if (!VHVAuthoringReferences::IsValidReferenceTag(
                            Destination.DestinationLocationTag, TEXT("VHV.Location"))
                        || DestinationID.IsNone()
                        || Destination.CompletionConditions.Conditions.IsEmpty()
                        || TrackingLocationIDs.Contains(DestinationID))
                    {
                        UE_LOG(LogVHV, Warning,
                            TEXT("[VHVQuest] Quest '%s' objective '%s' has an invalid or duplicate ordered tracking destination '%s'."),
                            *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(),
                            *Destination.DestinationLocationTag.ToString());
                        bValid = false;
                    }
                    TrackingLocationIDs.Add(DestinationID);
                }
            }
            else if (Objective.TrackingLocationTag.IsValid()
                || !Objective.TrackingDestinations.IsEmpty())
            {
                UE_LOG(LogVHV, Warning,
                    TEXT("[VHVQuest] Quest '%s' objective '%s' authors tracking destinations while location tracking is disabled."),
                    *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                bValid = false;
            }

            TSet<FName> CompletionMoveParticipants;
            for (const FVHVQuestNPCMoveRequest& Move : Objective.CompletionNPCMoves)
            {
                const FName ParticipantID = Move.GetEffectiveParticipantID();
                const FName DestinationID = Move.GetEffectiveDestinationID();
                if (!VHVAuthoringReferences::IsValidReferenceTag(Move.ParticipantTag, TEXT("VHV.Participant"))
                    || !VHVAuthoringReferences::IsValidReferenceTag(Move.DestinationLocationTag, TEXT("VHV.Location"))
                    || ParticipantID.IsNone() || DestinationID.IsNone())
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' has an invalid completion NPC semantic-location move."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                else if (CompletionMoveParticipants.Contains(ParticipantID))
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' has multiple completion moves for participant '%s'."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), *ParticipantID.ToString());
                    bValid = false;
                }
                CompletionMoveParticipants.Add(ParticipantID);
            }
            for (int32 StageIndex = 0; StageIndex < Objective.NPCMoveStages.Num(); ++StageIndex)
            {
                const FVHVQuestNPCMoveStage& Stage = Objective.NPCMoveStages[StageIndex];
                if (Stage.Moves.IsEmpty())
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' has an empty NPC move stage at index %d."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), StageIndex);
                    bValid = false;
                    continue;
                }

                if (Stage.AmbientConversationReceiverTag.IsValid()
                    && !VHVAuthoringReferences::IsValidReferenceTag(
                        Stage.AmbientConversationReceiverTag, TEXT("VHV.WorldReceiver")))
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' stage %d has an invalid ambient conversation receiver tag."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), StageIndex);
                    bValid = false;
                }

                TSet<FName> StageParticipants;
                for (const FVHVQuestNPCMoveRequest& Move : Stage.Moves)
                {
                    const FName ParticipantID = Move.GetEffectiveParticipantID();
                    const FName DestinationID = Move.GetEffectiveDestinationID();
                    if (!VHVAuthoringReferences::IsValidReferenceTag(Move.ParticipantTag, TEXT("VHV.Participant"))
                        || !VHVAuthoringReferences::IsValidReferenceTag(Move.DestinationLocationTag, TEXT("VHV.Location"))
                        || ParticipantID.IsNone() || DestinationID.IsNone())
                    {
                        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' has an invalid NPC move in stage %d."),
                            *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), StageIndex);
                        bValid = false;
                    }
                    if (Move.MoveSpeedOverride < 0.0f)
                    {
                        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' stage %d has a negative NPC move speed override."),
                            *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), StageIndex);
                        bValid = false;
                    }
                    else if (StageParticipants.Contains(ParticipantID))
                    {
                        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' stage %d moves participant '%s' more than once."),
                            *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), StageIndex, *ParticipantID.ToString());
                        bValid = false;
                    }
                    StageParticipants.Add(ParticipantID);
                }
            }
            if (Objective.NPCTravelMode == EVHVQuestNPCTravelMode::FreeRoam
                && Objective.NPCMoveStages.IsEmpty())
            {
                UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' enables free-roam NPC travel without movement stages."),
                    *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                bValid = false;
            }
            for (const FVHVStoryCondition& Condition : Objective.ActivationConditions.Conditions)
            {
                const FName StateID = Condition.GetEffectiveStateID();
                if (StateID.IsNone())
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' has an Activation Condition with an empty Story State ID."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                const bool bFlagCondition = Condition.ConditionType == EVHVStoryConditionType::FlagSet || Condition.ConditionType == EVHVStoryConditionType::FlagNotSet;
                const TCHAR* ExpectedCategory = bFlagCondition ? TEXT("VHV.Story.Flag") : TEXT("VHV.Story.Counter");
                if (!VHVAuthoringReferences::IsValidReferenceTag(Condition.StateTag, ExpectedCategory))
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' Activation Condition tag '%s' must be a concrete tag beneath %s."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), *Condition.StateTag.ToString(), ExpectedCategory);
                    bValid = false;
                }
                if (VHVAuthoringReferences::HasConflict(Condition.StateTag, Condition.StateID, ExpectedCategory))
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' Activation Condition tag '%s' resolves to '%s' while legacy StateID is '%s'; the tag wins."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), *Condition.StateTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Condition.StateTag).ToString(), *Condition.StateID.ToString());
                }
            }
            for (const FVHVStoryCondition& Condition : Objective.CompletionConditions.Conditions)
            {
                const FName StateID = Condition.GetEffectiveStateID();
                const bool bFlagCondition = Condition.ConditionType == EVHVStoryConditionType::FlagSet || Condition.ConditionType == EVHVStoryConditionType::FlagNotSet;
                const TCHAR* ExpectedCategory = bFlagCondition ? TEXT("VHV.Story.Flag") : TEXT("VHV.Story.Counter");
                if (StateID.IsNone() || !VHVAuthoringReferences::IsValidReferenceTag(Condition.StateTag, ExpectedCategory))
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' has an invalid Completion Condition tag '%s' beneath %s."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), *Condition.StateTag.ToString(), ExpectedCategory);
                    bValid = false;
                }
            }
            for (const FVHVStoryEffect& Effect : Objective.CompletionEffects)
            {
                const FName StateID = Effect.GetEffectiveStateID();
                if (StateID.IsNone())
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' has a Completion Effect with an empty Story State ID."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString());
                    bValid = false;
                }
                const bool bFlagEffect = Effect.EffectType == EVHVStoryEffectType::SetFlag || Effect.EffectType == EVHVStoryEffectType::ClearFlag;
                const TCHAR* ExpectedCategory = bFlagEffect ? TEXT("VHV.Story.Flag") : TEXT("VHV.Story.Counter");
                if (!VHVAuthoringReferences::IsValidReferenceTag(Effect.StateTag, ExpectedCategory))
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' Completion Effect tag '%s' must be a concrete tag beneath %s."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), *Effect.StateTag.ToString(), ExpectedCategory);
                    bValid = false;
                }
                if (VHVAuthoringReferences::HasConflict(Effect.StateTag, Effect.StateID, ExpectedCategory))
                {
                    UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' Completion Effect tag '%s' resolves to '%s' while legacy StateID is '%s'; the tag wins."),
                        *Quest.QuestID.ToString(), *Objective.ObjectiveID.ToString(), *Effect.StateTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Effect.StateTag).ToString(), *Effect.StateID.ToString());
                }
            }
        }
    }
    return bValid;
}

void UVHVQuestSubsystem::CompleteCurrentQuest()
{
    const FName CompletedQuestID = RuntimeState.ActiveQuestID;
    FVHVQuestRuntimeState* State = FindQuestState(CompletedQuestID);
    const FVHVQuestDefinition* Quest = FindQuestDefinition(CompletedQuestID);
    if (!State || !Quest)
    {
        return;
    }

    State->Status = EVHVQuestStatus::Completed;
    RuntimeState.CompletedQuestIDs.Add(CompletedQuestID);
    RuntimeState.ActiveQuestID = NAME_None;
    OnQuestCompleted.Broadcast(CompletedQuestID);
    OnQuestUpdated.Broadcast(CompletedQuestID);

    const int32 CompletedIndex = ActiveQuestArc ? ActiveQuestArc->Quests.IndexOfByPredicate([CompletedQuestID](const FVHVQuestDefinition& Candidate)
    {
        return Candidate.QuestID == CompletedQuestID;
    }) : INDEX_NONE;
    const int32 NextIndex = CompletedIndex + 1;
    if (ActiveQuestArc && ActiveQuestArc->Quests.IsValidIndex(NextIndex))
    {
        FVHVQuestRuntimeState* NextState = FindQuestState(ActiveQuestArc->Quests[NextIndex].QuestID);
        if (NextState)
        {
            NextState->Status = EVHVQuestStatus::NotStarted;
        }
        if (ActiveQuestArc->bAutoStartNextQuest && Quest->bAutoStartNextQuest)
        {
            StartQuest(ActiveQuestArc->Quests[NextIndex].QuestID);
        }
        return;
    }

    RuntimeState.bArcActive = false;
    RuntimeState.bArcCompleted = true;
    RuntimeState.TrackedQuestID = NAME_None;
    OnTrackedQuestChanged.Broadcast(NAME_None);
    OnQuestArcCompleted.Broadcast(RuntimeState.QuestArcID);
    if (TextbookSubsystem)
    {
        TextbookSubsystem->SetProgressionMode(EVHVTextbookProgressionMode::InternalTextbook);
    }
}

void UVHVQuestSubsystem::ActivateCurrentObjective()
{
    ++ObjectiveActivationSerial;
    ClearActiveObjectiveNPCReadinessTracking();
    ClearActiveObjectiveNPCMoveTracking(true);
    ClearActiveNPCActionTracking();
    ClearActiveWorldActionTracking();
    bCurrentObjectiveActivated = false;
    bCurrentObjectiveWaitingOnConditions = false;
    bCurrentObjectiveWaitingOnNPCReadiness = false;
    bCurrentObjectiveWaitingOnNPCMoves = false;
    const FVHVQuestRuntimeState* State = FindQuestState(RuntimeState.ActiveQuestID);
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!State || !Objective)
    {
        return;
    }

    const bool bRequiredParticipantsReady = EnsureRequiredParticipantsPresent(*Objective);
    if (!bRequiredParticipantsReady)
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVQuest] Presence preparation failed: Quest='%s' Objective='%s'. Exposing a recoverable failure state instead of hiding the objective."),
            *RuntimeState.ActiveQuestID.ToString(), *Objective->ObjectiveID.ToString());
        ExposeCurrentObjective();
        return;
    }

    if (Objective->bEnsureParticipantPresentAtLocation
        || Objective->bPrepareActivationNPCReadinessBeforeExposure)
    {
        UE_LOG(LogVHV, Log,
            TEXT("[VHVQuest] Exposing objective '%s'; required participants ready."),
            *Objective->ObjectiveID.ToString());
    }
    ExposeCurrentObjective();
}

void UVHVQuestSubsystem::ExposeCurrentObjective()
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective)
    {
        return;
    }

    OnObjectiveChanged.Broadcast(RuntimeState.ActiveQuestID, Objective->ObjectiveID);
    OnQuestUpdated.Broadcast(RuntimeState.ActiveQuestID);
    TryActivateCurrentObjective();
}

bool UVHVQuestSubsystem::EnsureRequiredParticipantsPresent(
    const FVHVQuestObjectiveDefinition& Objective)
{
    if (!Objective.bEnsureParticipantPresentAtLocation
        && !Objective.bPrepareActivationNPCReadinessBeforeExposure)
    {
        return true;
    }

    if (!Objective.ActivationNPCReadiness.IsEmpty())
    {
        bool bAllPresent = true;
        for (const FVHVQuestNPCMoveRequest& Requirement : Objective.ActivationNPCReadiness)
        {
            bAllPresent = EnsureParticipantPresentAtLocation(
                Requirement.ParticipantTag,
                Requirement.GetEffectiveParticipantID(),
                Requirement.GetEffectiveDestinationID(),
                Objective.ObjectiveID,
                Objective.RequiredParticipantSpawnClass) && bAllPresent;
        }
        return bAllPresent;
    }

    if (Objective.bPrepareActivationNPCReadinessBeforeExposure)
    {
        return false;
    }

    return EnsureParticipantPresentAtLocation(
        Objective.ParticipantTag,
        Objective.GetEffectiveParticipantID(),
        Objective.GetEffectiveLocationID(),
        Objective.ObjectiveID,
        Objective.RequiredParticipantSpawnClass);
}

bool UVHVQuestSubsystem::EnsureParticipantPresentAtLocation(
    const FGameplayTag& ParticipantTag,
    const FName ParticipantID,
    const FName LocationID,
    const FName ObjectiveID,
    const TSoftClassPtr<AVHVNPCCharacter>& FallbackSpawnClass)
{
    UWorld* World = GetWorld();
    if (!World || ParticipantID.IsNone() || LocationID.IsNone())
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVQuest] Presence hard failure: Quest='%s' Objective='%s' Participant='%s' Location='%s' Reason='invalid world or authored identity'."),
            *RuntimeState.ActiveQuestID.ToString(), *ObjectiveID.ToString(),
            *ParticipantID.ToString(), *LocationID.ToString());
        return false;
    }

    UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(World, ParticipantID);
    AVHVNPCCharacter* ParticipantActor = CommandComponent
        ? Cast<AVHVNPCCharacter>(CommandComponent->GetOwner())
        : nullptr;
    bool bSpawnedParticipant = false;
    if (ParticipantActor)
    {
        UE_LOG(LogVHV, Log,
            TEXT("[VHVQuest] Required participant '%s' resolved from registry as actor '%s' at %s."),
            *ParticipantID.ToString(), *GetNameSafe(ParticipantActor),
            *ParticipantActor->GetActorLocation().ToCompactString());
    }

    // Registry misses are exceptional. Search VHV NPCs exactly once for this
    // objective activation, using semantic participant identity only.
    if (!ParticipantActor)
    {
        AVHVNPCCharacter* WorldMatch = nullptr;
        for (TActorIterator<AVHVNPCCharacter> It(World); It; ++It)
        {
            const UVHVQuestParticipantComponent* Participant = It->GetQuestParticipantComponent();
            if (!Participant || Participant->GetEffectiveParticipantID() != ParticipantID)
            {
                continue;
            }
            if (WorldMatch)
            {
                UE_LOG(LogVHV, Error,
                    TEXT("[VHVQuest] Presence hard failure: Quest='%s' Objective='%s' Participant='%s' Location='%s' Reason='multiple matching VHV NPC actors'."),
                    *RuntimeState.ActiveQuestID.ToString(), *ObjectiveID.ToString(),
                    *ParticipantID.ToString(), *LocationID.ToString());
                return false;
            }
            WorldMatch = *It;
        }

        if (WorldMatch)
        {
            CommandComponent = WorldMatch->GetQuestCommandComponent();
            if (!CommandComponent || !RegisterNPCCommandComponent(CommandComponent))
            {
                UE_LOG(LogVHV, Error,
                    TEXT("[VHVQuest] Presence hard failure: Quest='%s' Objective='%s' Participant='%s' Location='%s' Reason='matching world actor could not register'."),
                    *RuntimeState.ActiveQuestID.ToString(), *ObjectiveID.ToString(),
                    *ParticipantID.ToString(), *LocationID.ToString());
                return false;
            }
            ParticipantActor = WorldMatch;
            UE_LOG(LogVHV, Log,
                TEXT("[VHVQuest] Required participant '%s' missing from registry; recovered actor '%s' at %s and re-registered it."),
                *ParticipantID.ToString(), *GetNameSafe(ParticipantActor),
                *ParticipantActor->GetActorLocation().ToCompactString());
        }
    }

    AVHVQuestLocationVolume* RequiredLocation = nullptr;
    for (TActorIterator<AVHVQuestLocationVolume> It(World); It; ++It)
    {
        if (It->GetEffectiveLocationID() != LocationID)
        {
            continue;
        }
        if (RequiredLocation)
        {
            UE_LOG(LogVHV, Error,
                TEXT("[VHVQuest] Presence hard failure: Quest='%s' Objective='%s' Participant='%s' Location='%s' Reason='multiple matching location volumes'."),
                *RuntimeState.ActiveQuestID.ToString(), *ObjectiveID.ToString(),
                *ParticipantID.ToString(), *LocationID.ToString());
            return false;
        }
        RequiredLocation = *It;
    }

    if (!RequiredLocation || !RequiredLocation->BoxComponent)
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVQuest] Presence hard failure: Quest='%s' Objective='%s' Participant='%s' Location='%s' Reason='required location volume missing'."),
            *RuntimeState.ActiveQuestID.ToString(), *ObjectiveID.ToString(),
            *ParticipantID.ToString(), *LocationID.ToString());
        return false;
    }

    if (!ParticipantActor)
    {
        UClass* SpawnClass = FallbackSpawnClass.LoadSynchronous();
        if (!SpawnClass || !SpawnClass->IsChildOf(AVHVNPCCharacter::StaticClass()) || !ParticipantTag.IsValid())
        {
            UE_LOG(LogVHV, Error,
                TEXT("[VHVQuest] Presence hard failure: Quest='%s' Objective='%s' Participant='%s' Location='%s' Reason='participant absent and no valid fallback spawn class'."),
                *RuntimeState.ActiveQuestID.ToString(), *ObjectiveID.ToString(),
                *ParticipantID.ToString(), *LocationID.ToString());
            return false;
        }

        const FTransform SpawnTransform = RequiredLocation->GetActorTransform();
        ParticipantActor = World->SpawnActorDeferred<AVHVNPCCharacter>(
            SpawnClass, SpawnTransform, nullptr, nullptr,
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!ParticipantActor)
        {
            UE_LOG(LogVHV, Error,
                TEXT("[VHVQuest] Presence hard failure: Quest='%s' Objective='%s' Participant='%s' Location='%s' Reason='fallback spawn failed'."),
                *RuntimeState.ActiveQuestID.ToString(), *ObjectiveID.ToString(),
                *ParticipantID.ToString(), *LocationID.ToString());
            return false;
        }
        bSpawnedParticipant = true;

        UVHVQuestParticipantComponent* Participant = ParticipantActor->GetQuestParticipantComponent();
        CommandComponent = ParticipantActor->GetQuestCommandComponent();
        if (!Participant || !CommandComponent)
        {
            ParticipantActor->Destroy();
            UE_LOG(LogVHV, Error,
                TEXT("[VHVQuest] Presence hard failure: Quest='%s' Objective='%s' Participant='%s' Location='%s' Reason='fallback class lacks required participant/command components'."),
                *RuntimeState.ActiveQuestID.ToString(), *ObjectiveID.ToString(),
                *ParticipantID.ToString(), *LocationID.ToString());
            return false;
        }

        Participant->ParticipantTag = ParticipantTag;
        Participant->bQuestParticipationEnabled = true;
        if (UVHVNPCInteractionComponent* Interaction = ParticipantActor->GetNPCInteractionComponent())
        {
            Interaction->bInteractionEnabled = true;
            Interaction->bUseQuestObjectiveTextAsPrompt = false;
            Interaction->DefaultInteractionPrompt = NSLOCTEXT("VHVQuest", "FallbackTalkPrompt", "Talk");
        }
        if (UVHVNPCNameplateComponent* Nameplate = ParticipantActor->GetNameplateComponent())
        {
            FString DisplayName = ParticipantID.ToString();
            for (int32 Index = DisplayName.Len() - 1; Index > 0; --Index)
            {
                if (FChar::IsUpper(DisplayName[Index]) && FChar::IsLower(DisplayName[Index - 1]))
                {
                    DisplayName.InsertAt(Index, TEXT(' '));
                }
            }
            Nameplate->NameplateDisplayName = FText::FromString(DisplayName);
            Nameplate->bShowNameplate = true;
        }

        // Set the posture while spawning is still deferred so BeginPlay applies it
        // with the actor's animation profile before the NPC can be rendered.
        ParticipantActor->SetIsSitting(RequiredLocation->bIsSitting);
        ParticipantActor->FinishSpawning(SpawnTransform);
        if (!RegisterNPCCommandComponent(CommandComponent))
        {
            ParticipantActor->Destroy();
            UE_LOG(LogVHV, Error,
                TEXT("[VHVQuest] Presence hard failure: Quest='%s' Objective='%s' Participant='%s' Location='%s' Reason='spawned fallback could not register'."),
                *RuntimeState.ActiveQuestID.ToString(), *ObjectiveID.ToString(),
                *ParticipantID.ToString(), *LocationID.ToString());
            return false;
        }
        UE_LOG(LogVHV, Warning,
            TEXT("[VHVQuest] Required participant '%s' absent from registry/world; spawned fallback participant for objective '%s'."),
            *ParticipantID.ToString(), *ObjectiveID.ToString());
    }

    // A placed or previously spawned participant is reactivated through the same
    // presence path. Apply the trigger's initial request once for this activation.
    if (!bSpawnedParticipant)
    {
        ParticipantActor->SetIsSitting(RequiredLocation->bIsSitting);
    }

    const FVector LocalPosition = RequiredLocation->BoxComponent->GetComponentTransform()
        .InverseTransformPosition(ParticipantActor->GetActorLocation());
    const FVector Extent = RequiredLocation->BoxComponent->GetUnscaledBoxExtent();
    const bool bInside = FMath::Abs(LocalPosition.X) <= Extent.X
        && FMath::Abs(LocalPosition.Y) <= Extent.Y
        && FMath::Abs(LocalPosition.Z) <= Extent.Z;
    if (bInside)
    {
        UE_LOG(LogVHV, Log,
            TEXT("[VHVQuest] Required participant '%s' actor '%s' already present at %s for objective '%s'."),
            *ParticipantID.ToString(), *GetNameSafe(ParticipantActor),
            *ParticipantActor->GetActorLocation().ToCompactString(), *ObjectiveID.ToString());
        return true;
    }

    UE_LOG(LogVHV, Log,
        TEXT("[VHVQuest] Relocating participant '%s' actor '%s' from %s to anchor '%s' actor '%s' at %s before exposing objective '%s'."),
        *ParticipantID.ToString(), *GetNameSafe(ParticipantActor),
        *ParticipantActor->GetActorLocation().ToCompactString(), *LocationID.ToString(),
        *GetNameSafe(RequiredLocation), *RequiredLocation->GetActorLocation().ToCompactString(),
        *ObjectiveID.ToString());

    // ObjectiveActivationSerial has already advanced and objective-owned delegates were
    // detached by ActivateCurrentObjective. Cancelling here therefore cannot let the old
    // request's synchronous or late completion mutate this newly selected objective.
    CommandComponent->CancelQuestCommand();
    if (ACharacter* Character = Cast<ACharacter>(ParticipantActor))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
        }
    }

    const bool bRelocated = ParticipantActor->SetActorLocationAndRotation(
        RequiredLocation->GetActorLocation(),
        RequiredLocation->GetActorRotation(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    if (bRelocated)
    {
        ParticipantActor->ForceNetUpdate();
        UE_LOG(LogVHV, Log,
            TEXT("[VHVQuest] Relocated participant '%s' actor '%s' to %s (rotation %s) before exposing objective."),
            *ParticipantID.ToString(), *GetNameSafe(ParticipantActor),
            *ParticipantActor->GetActorLocation().ToCompactString(),
            *ParticipantActor->GetActorRotation().ToCompactString());
    }
    else
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVQuest] Presence hard failure: Quest='%s' Objective='%s' Participant='%s' Location='%s' Reason='relocation failed'."),
            *RuntimeState.ActiveQuestID.ToString(), *ObjectiveID.ToString(),
            *ParticipantID.ToString(), *LocationID.ToString());
    }
    return bRelocated;
}

void UVHVQuestSubsystem::TryActivateCurrentObjective()
{
    if (bCurrentObjectiveActivated)
    {
        return;
    }

    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective)
    {
        bCurrentObjectiveWaitingOnConditions = false;
        return;
    }

    if (!Objective->ActivationConditions.Conditions.IsEmpty()
        || !Objective->CompletionConditions.Conditions.IsEmpty()
        || !Objective->NPCMoveStageActivationConditions.Conditions.IsEmpty())
    {
        EnsureStoryStateDelegateBindings();
    }

    if (!Objective->ActivationConditions.Conditions.IsEmpty() && !StoryStateSubsystem)
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Quest '%s' objective '%s' cannot evaluate Story State activation conditions because the Story State subsystem is unavailable."),
            *RuntimeState.ActiveQuestID.ToString(), *Objective->ObjectiveID.ToString());
        bCurrentObjectiveWaitingOnConditions = true;
        return;
    }

    if (StoryStateSubsystem && !StoryStateSubsystem->EvaluateConditionSet(Objective->ActivationConditions))
    {
        bCurrentObjectiveWaitingOnConditions = true;
        return;
    }

    bCurrentObjectiveWaitingOnConditions = false;
    if (!AreActiveObjectiveNPCReadinessRequirementsMet())
    {
        bCurrentObjectiveWaitingOnNPCReadiness = true;
        BindActiveObjectiveNPCReadinessCallbacks();
        return;
    }

    ClearActiveObjectiveNPCReadinessTracking();
    bCurrentObjectiveActivated = true;
    OnObjectiveReady.Broadcast(RuntimeState.ActiveQuestID, Objective->ObjectiveID);
    if (TryCompleteCurrentObjectiveFromStoryState())
    {
        return;
    }

    if (!Objective->NPCMoveStages.IsEmpty())
    {
        if (Objective->NPCMoveStageActivationConditions.Conditions.IsEmpty()
            || (StoryStateSubsystem
                && StoryStateSubsystem->EvaluateConditionSet(Objective->NPCMoveStageActivationConditions)))
        {
            StartActiveObjectiveNPCMoveSequence();
        }
        return;
    }

    ContinueActiveObjectiveAfterNPCMoves();
}

void UVHVQuestSubsystem::ContinueActiveObjectiveAfterNPCMoves()
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective || !bCurrentObjectiveActivated || bCurrentObjectiveWaitingOnNPCMoves)
    {
        return;
    }

    TryStartMotivationInstructorWalk();
    TryStageMotivationFinalDebriefInstructor();
    FaceAdjustedRoutineChaiTowardPlayer();

    if (Objective->ObjectiveType == EVHVQuestObjectiveType::NPCAction)
    {
        ExecuteActiveNPCAction();
        return;
    }
    if (Objective->ObjectiveType == EVHVQuestObjectiveType::WorldAction)
    {
        if (Objective->WorldActionStartPolicy == EVHVWorldActionStartPolicy::Immediate)
        {
            ExecuteActiveWorldAction();
        }
        return;
    }
    if ((Objective->ObjectiveType == EVHVQuestObjectiveType::Conversation || Objective->ObjectiveType == EVHVQuestObjectiveType::LearningActivity) && Objective->bAutoStart)
    {
        const FName ExpectedQuestID = RuntimeState.ActiveQuestID;
        const FName ExpectedObjectiveID = Objective->ObjectiveID;
        const uint32 ExpectedActivationSerial = ObjectiveActivationSerial;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, ExpectedQuestID, ExpectedObjectiveID, ExpectedActivationSerial]()
            {
                const FVHVQuestObjectiveDefinition* CurrentObjective = GetActiveObjective();
                if (ObjectiveActivationSerial == ExpectedActivationSerial
                    && RuntimeState.ActiveQuestID == ExpectedQuestID
                    && CurrentObjective
                    && CurrentObjective->ObjectiveID == ExpectedObjectiveID)
                {
                    RequestCurrentObjectiveActivation();
                }
            }));
        }
        else
        {
            RequestCurrentObjectiveActivation();
        }
    }
}

void UVHVQuestSubsystem::TryStartMotivationInstructorWalk()
{
    static const FName MotivationQuestID(TEXT("Q_HBCT_01_MOTIVATION"));
    static const FName ReachPrasertObjectiveID(TEXT("O01_ReachPrasertHouse"));
    static const FName InstructorParticipantID(TEXT("Instructor"));
    constexpr float ArrivalTolerance = 90.0f;
    constexpr float MaximumProjectionOffset = 100.0f;
    const FVector RequestedDestination(-37.0, 4194.0, 186.0);

    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective
        || RuntimeState.ActiveQuestID != MotivationQuestID
        || Objective->ObjectiveID != ReachPrasertObjectiveID
        || MotivationInstructorWalkActivationSerial == ObjectiveActivationSerial)
    {
        return;
    }

    // Mark this activation before any recoverable failure. This is a one-shot
    // scene cue and must not issue movement repeatedly while the objective lives.
    MotivationInstructorWalkActivationSerial = ObjectiveActivationSerial;

    UWorld* World = GetWorld();
    UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(
        World, InstructorParticipantID);
    AActor* Instructor = CommandComponent ? CommandComponent->GetOwner() : nullptr;
    UNavigationSystemV1* Navigation = World
        ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)
        : nullptr;
    if (!World || !CommandComponent || !Instructor || !Navigation)
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHVQuest] Q1 Instructor walk was not started: movement participant or navigation system unavailable."));
        return;
    }

    if (FVector::Dist2D(Instructor->GetActorLocation(), RequestedDestination) <= ArrivalTolerance)
    {
        UE_LOG(LogVHV, Log,
            TEXT("[VHVQuest] Q1 Instructor is already within %.0f cm of the Prasert destination; no move issued."),
            ArrivalTolerance);
        return;
    }

    FNavLocation ProjectedDestination;
    if (!Navigation->ProjectPointToNavigation(
            RequestedDestination,
            ProjectedDestination,
            FVector(MaximumProjectionOffset, MaximumProjectionOffset, 500.0f)))
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVQuest] Q1 Instructor walk rejected: no Recast point within %.0f cm of %s."),
            MaximumProjectionOffset, *RequestedDestination.ToCompactString());
        return;
    }

    const float ProjectionOffset = FVector::Dist2D(RequestedDestination, ProjectedDestination.Location);
    if (ProjectionOffset > MaximumProjectionOffset)
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVQuest] Q1 Instructor walk rejected: nearest Recast point is %.1f cm from the intended XY."),
            ProjectionOffset);
        return;
    }

    UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
        World, Instructor->GetActorLocation(), ProjectedDestination.Location, Instructor);
    if (!Path || !Path->IsValid() || Path->IsPartial())
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVQuest] Q1 Instructor walk rejected: Recast returned no complete path to %s."),
            *ProjectedDestination.Location.ToCompactString());
        return;
    }

    const FVector PrasertHouseDirection = FVector(612.0, 4960.0, ProjectedDestination.Location.Z)
        - ProjectedDestination.Location;
    const FRotator ArrivalRotation = PrasertHouseDirection.Rotation();
    if (!CommandComponent->MoveToLocation(
            ProjectedDestination.Location,
            ArrivalTolerance,
            true,
            ArrivalRotation,
            0.0f))
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHVQuest] Q1 Instructor walk command was rejected; quest progression remains unchanged."));
        return;
    }

    UE_LOG(LogVHV, Log,
        TEXT("[VHVQuest] Q1 Instructor walking to Prasert staging point %s (requested=%s, projection offset=%.1f cm, acceptance=%.0f cm)."),
        *ProjectedDestination.Location.ToCompactString(), *RequestedDestination.ToCompactString(),
        ProjectionOffset, ArrivalTolerance);
}

void UVHVQuestSubsystem::TryStageMotivationFinalDebriefInstructor()
{
    static const FName MotivationQuestID(TEXT("Q_HBCT_01_MOTIVATION"));
    static const FName FinalDebriefObjectiveID(TEXT("O14_FinalDebrief"));
    static const FName InstructorParticipantID(TEXT("Instructor"));
    static const FName TargetIdentity(TEXT("VHV_Q1_FinalDebrief_InstructorTarget"));

    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective
        || RuntimeState.ActiveQuestID != MotivationQuestID
        || Objective->ObjectiveID != FinalDebriefObjectiveID
        || MotivationFinalDebriefStagingActivationSerial == ObjectiveActivationSerial)
    {
        return;
    }

    UWorld* World = GetWorld();
    UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(
        World, InstructorParticipantID);
    AActor* Instructor = CommandComponent ? CommandComponent->GetOwner() : nullptr;
    if (!World || !CommandComponent || !Instructor)
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVQuest] Q1 final-debrief staging failed: persistent Instructor is unavailable."));
        return;
    }

    ATargetPoint* DebriefTarget = nullptr;
    for (TActorIterator<ATargetPoint> It(World); It; ++It)
    {
        if (It->GetFName() == TargetIdentity || It->ActorHasTag(TargetIdentity))
        {
            DebriefTarget = *It;
            break;
        }
    }
    if (!DebriefTarget)
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVQuest] Q1 final-debrief staging failed: TargetPoint '%s' is missing."),
            *TargetIdentity.ToString());
        return;
    }

    UVHVNPCBehaviorComponent* Behavior = Instructor->FindComponentByClass<UVHVNPCBehaviorComponent>();
    if (Behavior)
    {
        const EVHVNPCBehaviorState State = Behavior->GetBehaviorState();
        if (State == EVHVNPCBehaviorState::Engaging || State == EVHVNPCBehaviorState::Talking)
        {
            ClearMotivationFinalDebriefInstructorWait();
            MotivationFinalDebriefPendingBehavior = Behavior;
            MotivationFinalDebriefPendingActivationSerial = ObjectiveActivationSerial;
            Behavior->OnBehaviorStateChanged.AddUniqueDynamic(
                this, &UVHVQuestSubsystem::HandleMotivationFinalDebriefInstructorStateChanged);
            UE_LOG(LogVHV, Log,
                TEXT("[VHVQuest] Q1 final-debrief Instructor staging deferred until active dialogue ends."));
            return;
        }
    }

    ClearMotivationFinalDebriefInstructorWait();

    // This objective follows the Mali-house conversation, away from the debrief site.
    // Cancel the earlier Q1 walk before relocating so no stale AI request can pull the
    // persistent Instructor away from the designer-authored TargetPoint afterward.
    CommandComponent->CancelQuestCommand();
    if (ACharacter* Character = Cast<ACharacter>(Instructor))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
        }
    }

    const bool bRelocated = Instructor->SetActorLocationAndRotation(
        DebriefTarget->GetActorLocation(),
        DebriefTarget->GetActorRotation(),
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    if (!bRelocated)
    {
        UE_LOG(LogVHV, Error,
            TEXT("[VHVQuest] Q1 final-debrief Instructor staging failed at TargetPoint '%s'."),
            *TargetIdentity.ToString());
        return;
    }

    MotivationFinalDebriefStagingActivationSerial = ObjectiveActivationSerial;
    Instructor->ForceNetUpdate();
    OnObjectiveTrackingTargetChanged.Broadcast(RuntimeState.ActiveQuestID, Objective->ObjectiveID);
    UE_LOG(LogVHV, Log,
        TEXT("[VHVQuest] Q1 final-debrief Instructor staged from TargetPoint '%s' at %s."),
        *TargetIdentity.ToString(), *DebriefTarget->GetActorLocation().ToCompactString());
}

void UVHVQuestSubsystem::HandleMotivationFinalDebriefInstructorStateChanged(
    const EVHVNPCBehaviorState PreviousState,
    const EVHVNPCBehaviorState NewState)
{
    (void)PreviousState;
    if (NewState != EVHVNPCBehaviorState::Idle)
    {
        return;
    }

    const uint32 PendingActivationSerial = MotivationFinalDebriefPendingActivationSerial;
    ClearMotivationFinalDebriefInstructorWait();
    if (PendingActivationSerial == ObjectiveActivationSerial)
    {
        TryStageMotivationFinalDebriefInstructor();
    }
}

void UVHVQuestSubsystem::ClearMotivationFinalDebriefInstructorWait()
{
    if (UVHVNPCBehaviorComponent* Behavior = MotivationFinalDebriefPendingBehavior.Get())
    {
        Behavior->OnBehaviorStateChanged.RemoveDynamic(
            this, &UVHVQuestSubsystem::HandleMotivationFinalDebriefInstructorStateChanged);
    }
    MotivationFinalDebriefPendingBehavior.Reset();
    MotivationFinalDebriefPendingActivationSerial = 0;
}

void UVHVQuestSubsystem::ReevaluateWaitingObjective()
{
    if ((bCurrentObjectiveWaitingOnConditions || bCurrentObjectiveWaitingOnNPCReadiness)
        && !bCurrentObjectiveActivated)
    {
        TryActivateCurrentObjective();
    }
}

bool UVHVQuestSubsystem::GetActiveObjectiveNPCReadinessRequirements(
    TArray<TPair<FName, FName>>& OutRequirements) const
{
    OutRequirements.Reset();
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective)
    {
        return false;
    }

    // Stationary-presence and one-time scene-preparation objectives consume these
    // pairs before exposure, so they must not enter the asynchronous move wait.
    if (Objective->bEnsureParticipantPresentAtLocation
        || Objective->bPrepareActivationNPCReadinessBeforeExposure)
    {
        return false;
    }

    for (const FVHVQuestNPCMoveRequest& Readiness : Objective->ActivationNPCReadiness)
    {
        const FName ParticipantID = Readiness.GetEffectiveParticipantID();
        const FName DestinationID = Readiness.GetEffectiveDestinationID();
        if (!ParticipantID.IsNone() && !DestinationID.IsNone())
        {
            OutRequirements.Emplace(ParticipantID, DestinationID);
        }
    }

    return !OutRequirements.IsEmpty();
}

bool UVHVQuestSubsystem::AreActiveObjectiveNPCReadinessRequirementsMet() const
{
    TArray<TPair<FName, FName>> Requirements;
    if (!GetActiveObjectiveNPCReadinessRequirements(Requirements))
    {
        return true;
    }

    for (const TPair<FName, FName>& Requirement : Requirements)
    {
        const UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(
            GetWorld(), Requirement.Key);
        if (!CommandComponent || !CommandComponent->HasReachedTarget(Requirement.Value))
        {
            return false;
        }
    }
    return true;
}

void UVHVQuestSubsystem::BindActiveObjectiveNPCReadinessCallbacks()
{
    ClearActiveObjectiveNPCReadinessTracking();
    bCurrentObjectiveWaitingOnNPCReadiness = true;

    TArray<TPair<FName, FName>> Requirements;
    GetActiveObjectiveNPCReadinessRequirements(Requirements);
    for (const TPair<FName, FName>& Requirement : Requirements)
    {
        UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(
            GetWorld(), Requirement.Key);
        if (!CommandComponent || CommandComponent->HasReachedTarget(Requirement.Value))
        {
            continue;
        }
        const TWeakObjectPtr<UVHVNPCQuestCommandComponent> WeakCommand(CommandComponent);
        if (!ActiveObjectiveNPCReadinessBindings.Contains(WeakCommand))
        {
            ActiveObjectiveNPCReadinessBindings.Add(
                WeakCommand,
                CommandComponent->OnQuestCommandCompletedNative.AddUObject(
                    this, &UVHVQuestSubsystem::HandleObjectiveNPCReadinessMoveCompleted));
        }
    }
}

void UVHVQuestSubsystem::HandleObjectiveNPCReadinessMoveCompleted(
    UVHVNPCQuestCommandComponent* CommandComponent,
    const EVHVNPCQuestCommandType Command,
    const bool bSuccess)
{
    if (!CommandComponent
        || Command != EVHVNPCQuestCommandType::MoveToTarget
        || !bCurrentObjectiveWaitingOnNPCReadiness
        || bCurrentObjectiveActivated)
    {
        if (CommandComponent && Command == EVHVNPCQuestCommandType::MoveToTarget)
        {
            UE_LOG(LogVHV, Log,
                TEXT("[VHVQuest] Ignoring stale readiness callback from a skipped or previous objective."));
        }
        return;
    }

    if (!bSuccess)
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHVQuest] NPC pre-stage move failed for '%s'; the gated objective remains unavailable."),
            *GetNameSafe(CommandComponent->GetOwner()));
    }

    const uint32 ExpectedActivationSerial = ObjectiveActivationSerial;
    const FName ExpectedQuestID = RuntimeState.ActiveQuestID;
    const FVHVQuestObjectiveDefinition* ExpectedObjective = GetActiveObjective();
    const FName ExpectedObjectiveID = ExpectedObjective ? ExpectedObjective->ObjectiveID : NAME_None;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
            this,
            [this, ExpectedActivationSerial, ExpectedQuestID, ExpectedObjectiveID]()
            {
                const FVHVQuestObjectiveDefinition* CurrentObjective = GetActiveObjective();
                if (ObjectiveActivationSerial != ExpectedActivationSerial
                    || RuntimeState.ActiveQuestID != ExpectedQuestID
                    || !CurrentObjective
                    || CurrentObjective->ObjectiveID != ExpectedObjectiveID)
                {
                    UE_LOG(LogVHV, Log,
                        TEXT("[VHVQuest] Ignoring stale readiness callback from skipped objective: Quest='%s' Objective='%s'"),
                        *ExpectedQuestID.ToString(),
                        *ExpectedObjectiveID.ToString());
                    return;
                }

                if (bCurrentObjectiveWaitingOnNPCReadiness && !bCurrentObjectiveActivated)
                {
                    TryActivateCurrentObjective();
                }
            }));
    }
    else
    {
        TryActivateCurrentObjective();
    }
}

void UVHVQuestSubsystem::ClearActiveObjectiveNPCReadinessTracking()
{
    for (const TPair<TWeakObjectPtr<UVHVNPCQuestCommandComponent>, FDelegateHandle>& Pair
        : ActiveObjectiveNPCReadinessBindings)
    {
        if (UVHVNPCQuestCommandComponent* CommandComponent = Pair.Key.Get())
        {
            CommandComponent->OnQuestCommandCompletedNative.Remove(Pair.Value);
        }
    }
    ActiveObjectiveNPCReadinessBindings.Reset();
    bCurrentObjectiveWaitingOnNPCReadiness = false;
}

bool UVHVQuestSubsystem::TryCompleteCurrentObjectiveFromStoryState()
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective || !bCurrentObjectiveActivated || bCompletingCurrentObjective
        || Objective->CompletionConditions.Conditions.IsEmpty() || !StoryStateSubsystem)
    {
        return false;
    }
    return StoryStateSubsystem->EvaluateConditionSet(Objective->CompletionConditions)
        && CompleteCurrentObjective();
}

void UVHVQuestSubsystem::EnsureStoryStateDelegateBindings()
{
    if (!StoryStateSubsystem && GetGameInstance())
    {
        StoryStateSubsystem = GetGameInstance()->GetSubsystem<UVHVStoryStateSubsystem>();
    }
    if (!StoryStateSubsystem)
    {
        return;
    }

    StoryStateSubsystem->OnStoryFlagChanged.AddUniqueDynamic(this, &UVHVQuestSubsystem::HandleStoryFlagChanged);
    StoryStateSubsystem->OnStoryCounterChanged.AddUniqueDynamic(this, &UVHVQuestSubsystem::HandleStoryCounterChanged);
}

void UVHVQuestSubsystem::StartActiveObjectiveNPCMoveSequence()
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective || Objective->NPCMoveStages.IsEmpty() || !bCurrentObjectiveActivated)
    {
        return;
    }

    ClearActiveObjectiveNPCMoveTracking(true);
    ActiveObjectiveNPCMoveQuestID = RuntimeState.ActiveQuestID;
    ActiveObjectiveNPCMoveObjectiveID = Objective->ObjectiveID;
    ActiveObjectiveNPCMoveStageIndex = 0;
    ActiveObjectiveNPCMoveActivationSerial = ObjectiveActivationSerial;
    bCurrentObjectiveWaitingOnNPCMoves = true;
    StartActiveObjectiveNPCMoveStage();
}

bool UVHVQuestSubsystem::TryStartActiveObjectiveNPCMoveSequenceFromConditions()
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective || !bCurrentObjectiveActivated || bCurrentObjectiveWaitingOnNPCMoves
        || Objective->NPCMoveStages.IsEmpty()
        || Objective->NPCMoveStageActivationConditions.Conditions.IsEmpty()
        || !StoryStateSubsystem
        || !StoryStateSubsystem->EvaluateConditionSet(Objective->NPCMoveStageActivationConditions))
    {
        return false;
    }

    StartActiveObjectiveNPCMoveSequence();
    return true;
}

void UVHVQuestSubsystem::StartActiveObjectiveNPCMoveStage()
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective
        || RuntimeState.ActiveQuestID != ActiveObjectiveNPCMoveQuestID
        || Objective->ObjectiveID != ActiveObjectiveNPCMoveObjectiveID
        || ObjectiveActivationSerial != ActiveObjectiveNPCMoveActivationSerial
        || !Objective->NPCMoveStages.IsValidIndex(ActiveObjectiveNPCMoveStageIndex))
    {
        FailActiveObjectiveNPCMoveSequence(NAME_None, TEXT("the active quest objective changed"));
        return;
    }

    const FVHVQuestNPCMoveStage& Stage = Objective->NPCMoveStages[ActiveObjectiveNPCMoveStageIndex];
    ActiveObjectiveNPCMoveParticipants.Reset();
    PendingObjectiveNPCMoveParticipants.Reset();
    bObjectiveNPCMoveStageFailed = false;

    for (const FVHVQuestNPCMoveRequest& Move : Stage.Moves)
    {
        const FName ParticipantID = Move.GetEffectiveParticipantID();
        UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(GetWorld(), ParticipantID);
        if (!CommandComponent)
        {
            FailActiveObjectiveNPCMoveSequence(ParticipantID, TEXT("the participant command component was not found"));
            return;
        }

        const TWeakObjectPtr<UVHVNPCQuestCommandComponent> WeakCommand(CommandComponent);
        if (PendingObjectiveNPCMoveParticipants.Contains(ParticipantID)
            || ActiveObjectiveNPCMoveParticipants.Contains(WeakCommand))
        {
            FailActiveObjectiveNPCMoveSequence(ParticipantID, TEXT("the stage contains a duplicate participant"));
            return;
        }
        ActiveObjectiveNPCMoveParticipants.Add(WeakCommand, ParticipantID);
        PendingObjectiveNPCMoveParticipants.Add(ParticipantID);
    }

    // Replace any previous fire-and-forget route before binding this stage so the
    // cancellation cannot be mistaken for a failure of the newly authored move.
    for (const TPair<TWeakObjectPtr<UVHVNPCQuestCommandComponent>, FName>& Pair : ActiveObjectiveNPCMoveParticipants)
    {
        if (UVHVNPCQuestCommandComponent* CommandComponent = Pair.Key.Get())
        {
            CommandComponent->CancelQuestCommand();
            CommandComponent->OnQuestCommandCompletedNative.AddUObject(
                this, &UVHVQuestSubsystem::HandleObjectiveNPCMoveCompleted);
        }
    }

    UE_LOG(LogVHV, Log, TEXT("[VHVQuest] Quest '%s' objective '%s' starting NPC move stage %d with %d participant(s)."),
        *ActiveObjectiveNPCMoveQuestID.ToString(), *ActiveObjectiveNPCMoveObjectiveID.ToString(),
        ActiveObjectiveNPCMoveStageIndex, Stage.Moves.Num());

    bDispatchingObjectiveNPCMoveStage = true;
    for (const FVHVQuestNPCMoveRequest& Move : Stage.Moves)
    {
        const FName ParticipantID = Move.GetEffectiveParticipantID();
        if (!RequestNPCMove(
            ParticipantID,
            Move.GetEffectiveDestinationID(),
            Move.bFaceDestinationRotation,
            Move.MoveSpeedOverride))
        {
            // Synchronous command failures broadcast through the native delegate.
            // Cover registry/request failures that could not broadcast here.
            if (PendingObjectiveNPCMoveParticipants.Contains(ParticipantID))
            {
                bObjectiveNPCMoveStageFailed = true;
                PendingObjectiveNPCMoveParticipants.Remove(ParticipantID);
            }
        }
    }
    bDispatchingObjectiveNPCMoveStage = false;

    if (bObjectiveNPCMoveStageFailed)
    {
        FailActiveObjectiveNPCMoveSequence(NAME_None, TEXT("one or more movement requests failed to start"));
    }
    else
    {
        StartActiveObjectiveStageAmbientConversation(Stage);
        if (PendingObjectiveNPCMoveParticipants.IsEmpty())
        {
            FinishActiveObjectiveNPCMoveStage();
        }
    }
}

void UVHVQuestSubsystem::StartActiveObjectiveStageAmbientConversation(
    const FVHVQuestNPCMoveStage& Stage)
{
    if (!Stage.AmbientConversationReceiverTag.IsValid() || !GetWorld())
    {
        return;
    }

    const FName ReceiverID = VHVAuthoringReferences::ResolveID(
        Stage.AmbientConversationReceiverTag, NAME_None, TEXT("VHV.WorldReceiver"));
    const FName ExpectedQuestID = ActiveObjectiveNPCMoveQuestID;
    const FName ExpectedObjectiveID = ActiveObjectiveNPCMoveObjectiveID;
    const int32 ExpectedStageIndex = ActiveObjectiveNPCMoveStageIndex;
    const uint32 ExpectedActivationSerial = ActiveObjectiveNPCMoveActivationSerial;
    GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
        this,
        [this, ReceiverID, ExpectedQuestID, ExpectedObjectiveID, ExpectedStageIndex, ExpectedActivationSerial]()
        {
            if (ReceiverID.IsNone()
                || RuntimeState.ActiveQuestID != ExpectedQuestID
                || ActiveObjectiveNPCMoveObjectiveID != ExpectedObjectiveID
                || ActiveObjectiveNPCMoveStageIndex != ExpectedStageIndex
                || ActiveObjectiveNPCMoveActivationSerial != ExpectedActivationSerial
                || !bCurrentObjectiveWaitingOnNPCMoves
                || !GetWorld())
            {
                return;
            }

            UVHVWorldActionSubsystem* WorldActions = GetWorld()->GetSubsystem<UVHVWorldActionSubsystem>();
            FGuid RequestID;
            if (!WorldActions
                || WorldActions->RequestWorldAction(
                    ReceiverID, FName(TEXT("StartConversation")), RequestID)
                    == EVHVWorldActionExecutionResult::Rejected)
            {
                UE_LOG(LogVHV, Warning,
                    TEXT("[VHVQuest] Quest '%s' objective '%s' NPC move stage %d could not start optional ambient conversation receiver '%s'; NPC travel continues."),
                    *ExpectedQuestID.ToString(), *ExpectedObjectiveID.ToString(),
                    ExpectedStageIndex, *ReceiverID.ToString());
            }
        }));
}

void UVHVQuestSubsystem::HandleObjectiveNPCMoveCompleted(
    UVHVNPCQuestCommandComponent* CommandComponent,
    const EVHVNPCQuestCommandType Command,
    const bool bSuccess)
{
    if (!CommandComponent || Command != EVHVNPCQuestCommandType::MoveToTarget)
    {
        return;
    }

    const FVHVQuestObjectiveDefinition* CurrentObjective = GetActiveObjective();
    if (!bCurrentObjectiveWaitingOnNPCMoves
        || !CurrentObjective
        || RuntimeState.ActiveQuestID != ActiveObjectiveNPCMoveQuestID
        || CurrentObjective->ObjectiveID != ActiveObjectiveNPCMoveObjectiveID
        || ObjectiveActivationSerial != ActiveObjectiveNPCMoveActivationSerial)
    {
        UE_LOG(LogVHV, Log,
            TEXT("[VHVQuest] Ignoring stale movement callback from skipped objective: Quest='%s' Objective='%s' Stage=%d"),
            *ActiveObjectiveNPCMoveQuestID.ToString(),
            *ActiveObjectiveNPCMoveObjectiveID.ToString(),
            ActiveObjectiveNPCMoveStageIndex);
        return;
    }

    const TWeakObjectPtr<UVHVNPCQuestCommandComponent> WeakCommand(CommandComponent);
    const FName* ParticipantID = ActiveObjectiveNPCMoveParticipants.Find(WeakCommand);
    if (!ParticipantID || !PendingObjectiveNPCMoveParticipants.Contains(*ParticipantID))
    {
        return;
    }

    PendingObjectiveNPCMoveParticipants.Remove(*ParticipantID);
    if (!bSuccess)
    {
        bObjectiveNPCMoveStageFailed = true;
        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' NPC move stage %d failed for participant '%s'; objective remains active."),
            *ActiveObjectiveNPCMoveQuestID.ToString(), *ActiveObjectiveNPCMoveObjectiveID.ToString(),
            ActiveObjectiveNPCMoveStageIndex, *ParticipantID->ToString());
    }

    if (bDispatchingObjectiveNPCMoveStage)
    {
        return;
    }
    if (bObjectiveNPCMoveStageFailed)
    {
        FailActiveObjectiveNPCMoveSequence(*ParticipantID, TEXT("path following did not reach the authored destination"));
    }
    else if (PendingObjectiveNPCMoveParticipants.IsEmpty())
    {
        FinishActiveObjectiveNPCMoveStage();
    }
}

void UVHVQuestSubsystem::FinishActiveObjectiveNPCMoveStage()
{
    for (const TPair<TWeakObjectPtr<UVHVNPCQuestCommandComponent>, FName>& Pair : ActiveObjectiveNPCMoveParticipants)
    {
        if (UVHVNPCQuestCommandComponent* CommandComponent = Pair.Key.Get())
        {
            CommandComponent->OnQuestCommandCompletedNative.RemoveAll(this);
        }
    }
    ActiveObjectiveNPCMoveParticipants.Reset();
    PendingObjectiveNPCMoveParticipants.Reset();
    bObjectiveNPCMoveStageFailed = false;

    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    ++ActiveObjectiveNPCMoveStageIndex;
    if (Objective && Objective->NPCMoveStages.IsValidIndex(ActiveObjectiveNPCMoveStageIndex))
    {
        StartActiveObjectiveNPCMoveStage();
        return;
    }

    UE_LOG(LogVHV, Log, TEXT("[VHVQuest] Quest '%s' objective '%s' completed all NPC movement stages."),
        *ActiveObjectiveNPCMoveQuestID.ToString(), *ActiveObjectiveNPCMoveObjectiveID.ToString());
    const bool bCompleteAfterStages = Objective && Objective->bCompleteAfterNPCMoveStages;
    const bool bFinishedAdjustedRoutineRoute = Objective
        && RuntimeState.ActiveQuestID == FName(TEXT("Q_HBCT_05_SELF_MONITORING"))
        && Objective->ObjectiveID == FName(TEXT("O14_PracticeAdjustedRoutine"));
    ClearActiveObjectiveNPCMoveTracking(false);
    if (bFinishedAdjustedRoutineRoute && StoryStateSubsystem)
    {
        StoryStateSubsystem->SetFlag(TEXT("SMAdjustedWalkRouteComplete"));
        FaceAdjustedRoutineChaiTowardPlayer();
        UE_LOG(LogVHV, Log,
            TEXT("[VHVQuest] O14 adjusted walk reached its endpoint; recording and reflection remain required."));
    }
    if (bCompleteAfterStages)
    {
        CompleteCurrentObjective();
        return;
    }
    ContinueActiveObjectiveAfterNPCMoves();
}

void UVHVQuestSubsystem::FaceAdjustedRoutineChaiTowardPlayer()
{
    static const FName SelfMonitoringQuestID(TEXT("Q_HBCT_05_SELF_MONITORING"));
    static const FName AdjustedRoutineObjectiveID(TEXT("O14_PracticeAdjustedRoutine"));
    static const FName RouteCompleteFlag(TEXT("SMAdjustedWalkRouteComplete"));
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective
        || RuntimeState.ActiveQuestID != SelfMonitoringQuestID
        || Objective->ObjectiveID != AdjustedRoutineObjectiveID
        || !StoryStateSubsystem
        || !StoryStateSubsystem->HasFlag(RouteCompleteFlag))
    {
        return;
    }

    UVHVNPCQuestCommandComponent* ChaiCommand = FindNPCCommandComponent(GetWorld(), TEXT("UncleChai"));
    AActor* Chai = ChaiCommand ? ChaiCommand->GetOwner() : nullptr;
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!Chai || !PlayerPawn)
    {
        return;
    }

    const FVector ToPlayer = PlayerPawn->GetActorLocation() - Chai->GetActorLocation();
    if (ToPlayer.IsNearlyZero())
    {
        return;
    }
    FRotator FacingRotation = ToPlayer.Rotation();
    FacingRotation.Pitch = 0.0f;
    FacingRotation.Roll = 0.0f;
    Chai->SetActorRotation(FacingRotation, ETeleportType::TeleportPhysics);
    Chai->ForceNetUpdate();
}

void UVHVQuestSubsystem::FailActiveObjectiveNPCMoveSequence(
    const FName ParticipantID,
    const TCHAR* Reason)
{
    if (ParticipantID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' NPC move stage %d failed: %s; objective remains active."),
            *ActiveObjectiveNPCMoveQuestID.ToString(), *ActiveObjectiveNPCMoveObjectiveID.ToString(),
            ActiveObjectiveNPCMoveStageIndex, Reason);
    }
    else
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' objective '%s' NPC move stage %d failed for participant '%s': %s; objective remains active."),
            *ActiveObjectiveNPCMoveQuestID.ToString(), *ActiveObjectiveNPCMoveObjectiveID.ToString(),
            ActiveObjectiveNPCMoveStageIndex, *ParticipantID.ToString(), Reason);
    }

    ClearActiveObjectiveNPCMoveTracking(true);
    bCurrentObjectiveWaitingOnNPCMoves = true;
    bObjectiveNPCMoveStageFailed = true;
}

void UVHVQuestSubsystem::ClearActiveObjectiveNPCMoveTracking(const bool bCancelCommands)
{
    TArray<TWeakObjectPtr<UVHVNPCQuestCommandComponent>> Commands;
    ActiveObjectiveNPCMoveParticipants.GenerateKeyArray(Commands);
    for (const TWeakObjectPtr<UVHVNPCQuestCommandComponent>& WeakCommand : Commands)
    {
        if (UVHVNPCQuestCommandComponent* CommandComponent = WeakCommand.Get())
        {
            CommandComponent->OnQuestCommandCompletedNative.RemoveAll(this);
            if (bCancelCommands)
            {
                CommandComponent->CancelQuestCommand();
            }
        }
    }

    ActiveObjectiveNPCMoveParticipants.Reset();
    PendingObjectiveNPCMoveParticipants.Reset();
    ActiveObjectiveNPCMoveQuestID = NAME_None;
    ActiveObjectiveNPCMoveObjectiveID = NAME_None;
    ActiveObjectiveNPCMoveStageIndex = INDEX_NONE;
    ActiveObjectiveNPCMoveActivationSerial = 0;
    bCurrentObjectiveWaitingOnNPCMoves = false;
    bDispatchingObjectiveNPCMoveStage = false;
    bObjectiveNPCMoveStageFailed = false;
}

void UVHVQuestSubsystem::ExecuteActiveNPCAction()
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective || Objective->ObjectiveType != EVHVQuestObjectiveType::NPCAction)
    {
        return;
    }

    const FName ParticipantID = Objective->GetEffectiveNPCParticipantID();
    const FName TargetID = Objective->GetEffectiveNPCTargetID();
    const FName ActionID = Objective->GetEffectiveNPCActionID();
    UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(GetWorld(), ParticipantID);
    if (!CommandComponent)
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Quest '%s' NPCAction objective '%s' could not find NPC participant '%s'; objective remains active."),
            *RuntimeState.ActiveQuestID.ToString(), *Objective->ObjectiveID.ToString(), *ParticipantID.ToString());
        return;
    }

    // The command API replaces any prior quest command. Cancel it before binding
    // so its synchronous failure notification cannot be mistaken for this objective.
    CancelNPCQuestCommand(ParticipantID);

    ActiveNPCObjectiveCommandComponent = CommandComponent;
    ActiveNPCObjectiveQuestID = RuntimeState.ActiveQuestID;
    ActiveNPCObjectiveID = Objective->ObjectiveID;
    ActiveNPCObjectiveParticipantID = ParticipantID;
    ActiveNPCObjectiveCommandType = Objective->NPCCommandType;
    CommandComponent->OnQuestCommandCompleted.AddUniqueDynamic(this, &UVHVQuestSubsystem::HandleNPCObjectiveCommandCompleted);

    bool bStarted = false;
    switch (Objective->NPCCommandType)
    {
    case EVHVNPCQuestCommandType::MoveToTarget:
        bStarted = RequestNPCMove(ParticipantID, TargetID);
        break;
    case EVHVNPCQuestCommandType::Wait:
        bStarted = RequestNPCWait(ParticipantID, Objective->NPCWaitDuration);
        break;
    case EVHVNPCQuestCommandType::ReturnToPost:
        bStarted = RequestNPCReturnToPost(ParticipantID);
        break;
    case EVHVNPCQuestCommandType::ReleaseToPatrol:
        bStarted = ReleaseNPCFromQuest(ParticipantID);
        break;
    case EVHVNPCQuestCommandType::PlayAction:
        bStarted = RequestNPCPlayAction(ParticipantID, ActionID);
        break;
    default:
        break;
    }
    if (!bStarted && ActiveNPCObjectiveQuestID == RuntimeState.ActiveQuestID && ActiveNPCObjectiveID == Objective->ObjectiveID)
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Quest '%s' NPCAction objective '%s' failed to start command '%s' for participant '%s'; objective remains active."),
            *RuntimeState.ActiveQuestID.ToString(), *Objective->ObjectiveID.ToString(), *UEnum::GetValueAsString(Objective->NPCCommandType), *ParticipantID.ToString());
        ClearActiveNPCActionTracking();
    }
}

void UVHVQuestSubsystem::HandleNPCObjectiveCommandCompleted(const EVHVNPCQuestCommandType Command, const bool bSuccess)
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective
        || Objective->ObjectiveType != EVHVQuestObjectiveType::NPCAction
        || RuntimeState.ActiveQuestID != ActiveNPCObjectiveQuestID
        || Objective->ObjectiveID != ActiveNPCObjectiveID
        || Objective->GetEffectiveNPCParticipantID() != ActiveNPCObjectiveParticipantID
        || Objective->NPCCommandType != ActiveNPCObjectiveCommandType
        || Command != ActiveNPCObjectiveCommandType)
    {
        return;
    }

    const FName QuestID = ActiveNPCObjectiveQuestID;
    const FName ObjectiveID = ActiveNPCObjectiveID;
    const FName ParticipantID = ActiveNPCObjectiveParticipantID;
    if (!bSuccess)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' NPCAction objective '%s' command '%s' failed for participant '%s'; objective remains active."),
            *QuestID.ToString(), *ObjectiveID.ToString(), *UEnum::GetValueAsString(Command), *ParticipantID.ToString());
        ClearActiveNPCActionTracking();
        return;
    }

    CompleteCurrentObjective();
}

void UVHVQuestSubsystem::ClearActiveNPCActionTracking()
{
    if (ActiveNPCObjectiveCommandComponent)
    {
        ActiveNPCObjectiveCommandComponent->OnQuestCommandCompleted.RemoveDynamic(this, &UVHVQuestSubsystem::HandleNPCObjectiveCommandCompleted);
    }
    ActiveNPCObjectiveCommandComponent = nullptr;
    ActiveNPCObjectiveQuestID = NAME_None;
    ActiveNPCObjectiveID = NAME_None;
    ActiveNPCObjectiveParticipantID = NAME_None;
    ActiveNPCObjectiveCommandType = EVHVNPCQuestCommandType::None;
}

bool UVHVQuestSubsystem::RequestExplicitWorldAction(
    const FName ReceiverID,
    const FName ActionID,
    const FName RequiredQuestID,
    const FName RequiredObjectiveID)
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective
        || !bCurrentObjectiveActivated
        || bCurrentObjectiveWaitingOnNPCMoves
        || Objective->ObjectiveType != EVHVQuestObjectiveType::WorldAction
        || Objective->WorldActionStartPolicy != EVHVWorldActionStartPolicy::ExplicitTrigger
        || ActiveWorldActionRequestID.IsValid())
    {
        return false;
    }

    if ((!RequiredQuestID.IsNone() && RuntimeState.ActiveQuestID != RequiredQuestID)
        || (!RequiredObjectiveID.IsNone() && Objective->ObjectiveID != RequiredObjectiveID)
        || Objective->GetEffectiveWorldActionReceiverID() != ReceiverID
        || Objective->GetEffectiveWorldActionID() != ActionID)
    {
        return false;
    }

    return ExecuteActiveWorldAction();
}

bool UVHVQuestSubsystem::ExecuteActiveWorldAction()
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    UWorld* World = GetWorld();
    UVHVWorldActionSubsystem* WorldActionSubsystem = World ? World->GetSubsystem<UVHVWorldActionSubsystem>() : nullptr;
    if (!Objective || Objective->ObjectiveType != EVHVQuestObjectiveType::WorldAction || !WorldActionSubsystem)
    {
        if (Objective && Objective->ObjectiveType == EVHVQuestObjectiveType::WorldAction)
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Quest '%s' WorldAction objective '%s' could not access the World Action subsystem; objective remains active."),
                *RuntimeState.ActiveQuestID.ToString(), *Objective->ObjectiveID.ToString());
        }
        return false;
    }

    ActiveWorldActionSubsystem = WorldActionSubsystem;
    ActiveWorldActionRequestID.Invalidate();
    ActiveWorldActionQuestID = RuntimeState.ActiveQuestID;
    ActiveWorldActionObjectiveID = Objective->ObjectiveID;
    const FName ReceiverID = Objective->GetEffectiveWorldActionReceiverID();
    const FName ActionID = Objective->GetEffectiveWorldActionID();
    ActiveWorldActionReceiverID = ReceiverID;
    ActiveWorldActionID = ActionID;
    WorldActionSubsystem->OnWorldActionCompleted.AddUniqueDynamic(this, &UVHVQuestSubsystem::HandleWorldActionCompleted);

    const FName ExpectedQuestID = ActiveWorldActionQuestID;
    const FName ExpectedObjectiveID = ActiveWorldActionObjectiveID;
    const EVHVWorldActionExecutionResult Result = WorldActionSubsystem->RequestWorldAction(
        ReceiverID,
        ActionID,
        ActiveWorldActionRequestID);

    if (Result == EVHVWorldActionExecutionResult::Rejected
        && ActiveWorldActionQuestID == ExpectedQuestID
        && ActiveWorldActionObjectiveID == ExpectedObjectiveID)
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVQuest] Quest '%s' WorldAction objective '%s' request '%s' was rejected by receiver '%s'; objective remains active."),
            *ExpectedQuestID.ToString(), *ExpectedObjectiveID.ToString(), *ActionID.ToString(), *ReceiverID.ToString());
        ClearActiveWorldActionTracking();
    }
    return Result != EVHVWorldActionExecutionResult::Rejected;
}

void UVHVQuestSubsystem::HandleWorldActionCompleted(
    const FGuid RequestID,
    const FName ReceiverID,
    const FName ActionID,
    const bool bSuccess)
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective
        || Objective->ObjectiveType != EVHVQuestObjectiveType::WorldAction
        || RuntimeState.ActiveQuestID != ActiveWorldActionQuestID
        || Objective->ObjectiveID != ActiveWorldActionObjectiveID
        || RequestID != ActiveWorldActionRequestID
        || ReceiverID != ActiveWorldActionReceiverID
        || ActionID != ActiveWorldActionID
        || Objective->GetEffectiveWorldActionReceiverID() != ActiveWorldActionReceiverID
        || Objective->GetEffectiveWorldActionID() != ActiveWorldActionID)
    {
        return;
    }

    const FName QuestID = ActiveWorldActionQuestID;
    const FName ObjectiveID = ActiveWorldActionObjectiveID;
    if (!bSuccess)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest '%s' WorldAction objective '%s' action '%s' failed on receiver '%s'; objective remains active."),
            *QuestID.ToString(), *ObjectiveID.ToString(), *ActionID.ToString(), *ReceiverID.ToString());
        ClearActiveWorldActionTracking();
        return;
    }

    CompleteCurrentObjective();
}

void UVHVQuestSubsystem::ClearActiveWorldActionTracking()
{
    if (ActiveWorldActionSubsystem)
    {
        ActiveWorldActionSubsystem->OnWorldActionCompleted.RemoveDynamic(this, &UVHVQuestSubsystem::HandleWorldActionCompleted);
    }
    ActiveWorldActionSubsystem = nullptr;
    ActiveWorldActionRequestID.Invalidate();
    ActiveWorldActionQuestID = NAME_None;
    ActiveWorldActionObjectiveID = NAME_None;
    ActiveWorldActionReceiverID = NAME_None;
    ActiveWorldActionID = NAME_None;
}

bool UVHVQuestSubsystem::BuildJournalEntry(const FVHVQuestDefinition& Definition, const FVHVQuestRuntimeState& State, FVHVQuestJournalEntry& OutEntry) const
{
    OutEntry.QuestID = Definition.QuestID;
    OutEntry.QuestTitle = Definition.QuestTitle;
    OutEntry.QuestDescription = Definition.QuestDescription;
    OutEntry.Status = State.Status;
    OutEntry.Category = Definition.Category;
    OutEntry.CurrentObjectiveIndex = State.CurrentObjectiveIndex;
    OutEntry.TotalObjectiveCount = Definition.Objectives.Num();
    OutEntry.bTracked = RuntimeState.TrackedQuestID == Definition.QuestID;
    OutEntry.CurrentObjectiveText = FText::GetEmpty();
    if (Definition.Objectives.IsValidIndex(State.CurrentObjectiveIndex))
    {
        const FVHVQuestObjectiveDefinition& Objective = Definition.Objectives[State.CurrentObjectiveIndex];
        OutEntry.CurrentObjectiveText = Objective.ObjectiveText;
        static const FName SelfMonitoringQuestID(TEXT("Q_HBCT_05_SELF_MONITORING"));
        static const FName AdjustedRoutineObjectiveID(TEXT("O14_PracticeAdjustedRoutine"));
        if (Definition.QuestID == SelfMonitoringQuestID
            && Objective.ObjectiveID == AdjustedRoutineObjectiveID
            && StoryStateSubsystem)
        {
            if (StoryStateSubsystem->HasFlag(TEXT("SMAdjustedWalkRecordedAccurately")))
            {
                OutEntry.CurrentObjectiveText = NSLOCTEXT(
                    "VHVQuest", "AdjustedRoutineReflect", "Review the result with Uncle Chai.");
            }
            else if (StoryStateSubsystem->HasFlag(TEXT("SMAdjustedWalkRouteComplete")))
            {
                OutEntry.CurrentObjectiveText = NSLOCTEXT(
                    "VHVQuest", "AdjustedRoutineRecord", "Record what actually happened during Uncle Chai's shorter walk.");
            }
            else if (StoryStateSubsystem->HasFlag(TEXT("SMAdjustedWalkConversationComplete")))
            {
                OutEntry.CurrentObjectiveText = NSLOCTEXT(
                    "VHVQuest", "AdjustedRoutineWalk", "Walk with Uncle Chai and observe his adjusted routine.");
            }
        }
        if (!Objective.TrackingDestinations.IsEmpty())
        {
            OutEntry.CurrentObjectiveText = FText::Format(
                NSLOCTEXT("VHVQuest", "MultiDestinationObjectiveProgress", "{0} ({1}/{2})"),
                Objective.ObjectiveText,
                FText::AsNumber(CountCompletedTrackingDestinations(Objective)),
                FText::AsNumber(Objective.TrackingDestinations.Num()));
        }
    }
    return true;
}

FName UVHVQuestSubsystem::GetRequiredConversationID(const FVHVQuestObjectiveDefinition& Objective) const
{
    UVHVConversationDataAsset* Asset = Objective.Conversation.Get();
    if (!Asset)
    {
        Asset = Objective.Conversation.LoadSynchronous();
    }
    return Asset ? FName(*Asset->Conversation.ConversationID) : NAME_None;
}

const FVHVQuestDefinition* UVHVQuestSubsystem::FindQuestDefinition(FName QuestID) const
{
    return ActiveQuestArc ? ActiveQuestArc->Quests.FindByPredicate([QuestID](const FVHVQuestDefinition& Quest) { return Quest.QuestID == QuestID; }) : nullptr;
}

FVHVQuestRuntimeState* UVHVQuestSubsystem::FindQuestState(FName QuestID)
{
    return RuntimeState.QuestStates.FindByPredicate([QuestID](const FVHVQuestRuntimeState& State) { return State.QuestID == QuestID; });
}

const FVHVQuestRuntimeState* UVHVQuestSubsystem::FindQuestState(FName QuestID) const
{
    return RuntimeState.QuestStates.FindByPredicate([QuestID](const FVHVQuestRuntimeState& State) { return State.QuestID == QuestID; });
}

const FVHVQuestObjectiveDefinition* UVHVQuestSubsystem::GetActiveObjective() const
{
    const FVHVQuestDefinition* Quest = FindQuestDefinition(RuntimeState.ActiveQuestID);
    const FVHVQuestRuntimeState* State = FindQuestState(RuntimeState.ActiveQuestID);
    return Quest && State && State->Status == EVHVQuestStatus::Active && Quest->Objectives.IsValidIndex(State->CurrentObjectiveIndex)
        ? &Quest->Objectives[State->CurrentObjectiveIndex] : nullptr;
}
