#include "Quest/Systems/VHVQuestSubsystem.h"

#include "Core/VHVConversationDataAsset.h"
#include "Quest/Data/VHVQuestArcData.h"
#include "NPC/Components/VHVNPCQuestCommandComponent.h"
#include "NPC/Quest/VHVNPCBehaviorTarget.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "Save/VHVSaveTypes.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "Textbook/Data/VHVLevelData.h"
#include "Textbook/Systems/VHVTextbookSubsystem.h"
#include "World/Systems/VHVWorldActionSubsystem.h"
#include "Engine/World.h"
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
    OnQuestUpdated.Broadcast(QuestID);
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

    OnObjectiveCompleted.Broadcast(QuestID, ObjectiveID);

    ++QuestState->CurrentObjectiveIndex;
    OnQuestUpdated.Broadcast(QuestID);
    bCompletingCurrentObjective = false;
    if (Quest->Objectives.IsValidIndex(QuestState->CurrentObjectiveIndex))
    {
        ActivateCurrentObjective();
    }
    else
    {
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
    return GetActiveObjective() && bCurrentObjectiveWaitingOnConditions && !bCurrentObjectiveActivated;
}

bool UVHVQuestSubsystem::IsTransientObjectiveExecutionActive() const
{
    return ActiveNPCObjectiveCommandComponent != nullptr || ActiveWorldActionRequestID.IsValid();
}

#if !UE_BUILD_SHIPPING
bool UVHVQuestSubsystem::DebugCompleteCurrentObjective()
{
    if (!GetActiveObjective())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Cannot complete objective: there is no active objective."));
        return false;
    }

    DebugCancelActiveObjectiveExecution();
    return CompleteCurrentObjective();
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

    if (RuntimeState.TrackedQuestID.IsNone())
    {
        RuntimeState.TrackedQuestID = QuestID;
        OnTrackedQuestChanged.Broadcast(QuestID);
    }
    OnQuestUpdated.Broadcast(QuestID);
    ActivateCurrentObjective();
    return true;
}

void UVHVQuestSubsystem::DebugCancelActiveObjectiveExecution()
{
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
        OnQuestUpdated.Broadcast(QuestState.QuestID);
    }
    if (!RuntimeState.ActiveQuestID.IsNone())
    {
        ActivateCurrentObjective();
    }
    return true;
}

bool UVHVQuestSubsystem::RequestCurrentObjectiveActivation()
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective || !bCurrentObjectiveActivated)
    {
        return false;
    }
    OnObjectiveActivationRequested.Broadcast(RuntimeState.ActiveQuestID, Objective->ObjectiveID);
    return true;
}

bool UVHVQuestSubsystem::NotifyParticipantInteracted(FName ParticipantID)
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective || ParticipantID.IsNone())
    {
        return false;
    }

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

bool UVHVQuestSubsystem::RequestNPCMove(const FName ParticipantID, const FName TargetID)
{
    UVHVNPCQuestCommandComponent* CommandComponent = FindNPCCommandComponent(GetWorld(), ParticipantID);
    return CommandComponent && CommandComponent->MoveToTarget(TargetID);
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
    return true;
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
    (void)FlagID;
    (void)bValue;
    ReevaluateWaitingObjective();
}

void UVHVQuestSubsystem::HandleStoryCounterChanged(const FName CounterID, const int32 NewValue)
{
    (void)CounterID;
    (void)NewValue;
    ReevaluateWaitingObjective();
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
    ClearActiveNPCActionTracking();
    ClearActiveWorldActionTracking();
    bCurrentObjectiveActivated = false;
    bCurrentObjectiveWaitingOnConditions = false;
    const FVHVQuestRuntimeState* State = FindQuestState(RuntimeState.ActiveQuestID);
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!State || !Objective)
    {
        return;
    }
    OnObjectiveChanged.Broadcast(RuntimeState.ActiveQuestID, Objective->ObjectiveID);
    OnQuestUpdated.Broadcast(RuntimeState.ActiveQuestID);
    TryActivateCurrentObjective();
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

    if (!Objective->ActivationConditions.Conditions.IsEmpty())
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
    bCurrentObjectiveActivated = true;
    if (Objective->ObjectiveType == EVHVQuestObjectiveType::NPCAction)
    {
        ExecuteActiveNPCAction();
        return;
    }
    if (Objective->ObjectiveType == EVHVQuestObjectiveType::WorldAction)
    {
        ExecuteActiveWorldAction();
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

void UVHVQuestSubsystem::ReevaluateWaitingObjective()
{
    if (bCurrentObjectiveWaitingOnConditions && !bCurrentObjectiveActivated)
    {
        TryActivateCurrentObjective();
    }
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

void UVHVQuestSubsystem::ExecuteActiveWorldAction()
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
        return;
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
    OutEntry.CurrentObjectiveText = Definition.Objectives.IsValidIndex(State.CurrentObjectiveIndex) ? Definition.Objectives[State.CurrentObjectiveIndex].ObjectiveText : FText::GetEmpty();
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
