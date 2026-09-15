#include "Quest/Systems/VHVQuestSubsystem.h"

#include "Core/VHVConversationDataAsset.h"
#include "Quest/Data/VHVQuestArcData.h"
#include "Textbook/Data/VHVLevelData.h"
#include "Textbook/Systems/VHVTextbookSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "VHV.h"

void UVHVQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UVHVTextbookSubsystem>();
    TextbookSubsystem = GetGameInstance()->GetSubsystem<UVHVTextbookSubsystem>();
    if (TextbookSubsystem)
    {
        TextbookSubsystem->OnActivityCompleted.AddDynamic(this, &UVHVQuestSubsystem::HandleTextbookActivityCompleted);
    }
}

void UVHVQuestSubsystem::Deinitialize()
{
    if (TextbookSubsystem)
    {
        TextbookSubsystem->OnActivityCompleted.RemoveDynamic(this, &UVHVQuestSubsystem::HandleTextbookActivityCompleted);
    }
    ActiveQuestArc = nullptr;
    TextbookSubsystem = nullptr;
    RuntimeState = FVHVQuestArcRuntimeState();
    Super::Deinitialize();
}

bool UVHVQuestSubsystem::RegisterQuestArc(UVHVQuestArcData* QuestArc)
{
    if (!ValidateQuestArc(QuestArc))
    {
        return false;
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

bool UVHVQuestSubsystem::CompleteCurrentObjective()
{
    FVHVQuestRuntimeState* QuestState = FindQuestState(RuntimeState.ActiveQuestID);
    const FVHVQuestDefinition* Quest = FindQuestDefinition(RuntimeState.ActiveQuestID);
    if (!QuestState || !Quest || QuestState->Status != EVHVQuestStatus::Active || !Quest->Objectives.IsValidIndex(QuestState->CurrentObjectiveIndex))
    {
        return false;
    }

    const FName QuestID = Quest->QuestID;
    const FName ObjectiveID = Quest->Objectives[QuestState->CurrentObjectiveIndex].ObjectiveID;
    QuestState->CompletedObjectiveIDs.Add(ObjectiveID);
    OnObjectiveCompleted.Broadcast(QuestID, ObjectiveID);

    ++QuestState->CurrentObjectiveIndex;
    OnQuestUpdated.Broadcast(QuestID);
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

bool UVHVQuestSubsystem::RequestCurrentObjectiveActivation()
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!Objective)
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

    if (Objective->ObjectiveType == EVHVQuestObjectiveType::Interact && Objective->TargetID == ParticipantID)
    {
        CompleteCurrentObjective();
        return true;
    }

    const bool bLaunchable = Objective->ObjectiveType == EVHVQuestObjectiveType::Talk
        || Objective->ObjectiveType == EVHVQuestObjectiveType::Conversation
        || Objective->ObjectiveType == EVHVQuestObjectiveType::LearningActivity;
    if (bLaunchable && (Objective->TargetID.IsNone() || Objective->TargetID == ParticipantID))
    {
        RequestCurrentObjectiveActivation();
        return true;
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
    if (Objective && Objective->ObjectiveType == EVHVQuestObjectiveType::LearningActivity && Objective->ActivityID == ActivityID)
    {
        CompleteCurrentObjective();
    }
}

void UVHVQuestSubsystem::NotifyLocationReached(FName LocationID)
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (Objective && Objective->ObjectiveType == EVHVQuestObjectiveType::ReachLocation && Objective->TargetID == LocationID)
    {
        CompleteCurrentObjective();
    }
}

void UVHVQuestSubsystem::NotifyCustomEvent(FName EventID)
{
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (Objective && Objective->ObjectiveType == EVHVQuestObjectiveType::CustomEvent && Objective->TargetID == EventID)
    {
        CompleteCurrentObjective();
    }
}

void UVHVQuestSubsystem::HandleTextbookActivityCompleted()
{
    if (TextbookSubsystem)
    {
        NotifyActivityCompleted(FName(*TextbookSubsystem->GetCurrentActivity().ActivityID));
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

            if (Objective.ObjectiveType == EVHVQuestObjectiveType::LearningActivity && Objective.ActivityID.IsNone())
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
            if (Objective.ObjectiveType == EVHVQuestObjectiveType::Talk && Objective.TargetID.IsNone())
            {
                UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Talk objective '%s' has no participant Target ID."), *Objective.ObjectiveID.ToString());
                bValid = false;
            }
            if ((Objective.ObjectiveType == EVHVQuestObjectiveType::Interact || Objective.ObjectiveType == EVHVQuestObjectiveType::ReachLocation || Objective.ObjectiveType == EVHVQuestObjectiveType::CustomEvent) && Objective.TargetID.IsNone())
            {
                UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Objective '%s' requires a Target ID."), *Objective.ObjectiveID.ToString());
                bValid = false;
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
    const FVHVQuestRuntimeState* State = FindQuestState(RuntimeState.ActiveQuestID);
    const FVHVQuestObjectiveDefinition* Objective = GetActiveObjective();
    if (!State || !Objective)
    {
        return;
    }
    OnObjectiveChanged.Broadcast(RuntimeState.ActiveQuestID, Objective->ObjectiveID);
    OnQuestUpdated.Broadcast(RuntimeState.ActiveQuestID);
    if ((Objective->ObjectiveType == EVHVQuestObjectiveType::Conversation || Objective->ObjectiveType == EVHVQuestObjectiveType::LearningActivity) && Objective->bAutoStart)
    {
        const FName ExpectedQuestID = RuntimeState.ActiveQuestID;
        const FName ExpectedObjectiveID = Objective->ObjectiveID;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, ExpectedQuestID, ExpectedObjectiveID]()
            {
                const FVHVQuestObjectiveDefinition* CurrentObjective = GetActiveObjective();
                if (RuntimeState.ActiveQuestID == ExpectedQuestID && CurrentObjective && CurrentObjective->ObjectiveID == ExpectedObjectiveID)
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
