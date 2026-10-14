#include "HBCT/SelfMonitoring/VHVQ5DisplayVisibilityController.h"

#include "Cinematics/VHVTimeSkipSequence.h"
#include "Components/TextRenderComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Misc/DataValidation.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "VHV.h"

namespace
{
bool IsGameplayDisplayComponent(const UPrimitiveComponent* Component)
{
    return Component
        && (Component->IsA<UTextRenderComponent>() || Component->IsA<UWidgetComponent>());
}
}

AVHVQ5DisplayVisibilityController::AVHVQ5DisplayVisibilityController()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AVHVQ5DisplayVisibilityController::BeginPlay()
{
    Super::BeginPlay();
    if (const UGameInstance* GameInstance = GetGameInstance())
    {
        QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>();
    }

    for (const FVHVQ5ObjectiveDisplayGroup& Group : ObjectiveDisplays)
    {
        for (AActor* DisplayActor : Group.DisplayActors)
        {
            if (!DisplayActor || OriginalActorHiddenStates.Contains(DisplayActor))
            {
                continue;
            }
            OriginalActorHiddenStates.Add(DisplayActor, DisplayActor->IsHidden());
            OriginalActorCollisionStates.Add(DisplayActor, DisplayActor->GetActorEnableCollision());
            TInlineComponentArray<UPrimitiveComponent*> Components;
            DisplayActor->GetComponents(Components);
            for (UPrimitiveComponent* Component : Components)
            {
                if (!Component || OriginalComponentVisibilityStates.Contains(Component))
                {
                    continue;
                }
                OriginalComponentVisibilityStates.Add(Component, Component->IsVisible());
                OriginalComponentHiddenStates.Add(Component, Component->bHiddenInGame);
                OriginalComponentCollisionStates.Add(Component, Component->GetCollisionEnabled());
            }
        }
    }

    if (QuestSubsystem)
    {
        QuestSubsystem->OnObjectiveChanged.AddUniqueDynamic(
            this, &AVHVQ5DisplayVisibilityController::HandleObjectiveChanged);
        QuestSubsystem->OnQuestStarted.AddUniqueDynamic(
            this, &AVHVQ5DisplayVisibilityController::HandleQuestStarted);
        QuestSubsystem->OnQuestCompleted.AddUniqueDynamic(
            this, &AVHVQ5DisplayVisibilityController::HandleQuestCompleted);
    }
    if (TimeSkipSequence)
    {
        TimeSkipSequence->OnStagePresentationActivated.AddUniqueDynamic(
            this, &AVHVQ5DisplayVisibilityController::HandleTimeSkipStageActivated);
    }
    for (AVHVTimeSkipSequence* AdditionalSequence : AdditionalTimeSkipSequences)
    {
        if (AdditionalSequence && AdditionalSequence != TimeSkipSequence)
        {
            AdditionalSequence->OnStagePresentationActivated.AddUniqueDynamic(
                this, &AVHVQ5DisplayVisibilityController::HandleTimeSkipStageActivated);
        }
    }

    HideAllDisplays();
    RefreshFromCurrentObjective();
}

void AVHVQ5DisplayVisibilityController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (QuestSubsystem)
    {
        QuestSubsystem->OnObjectiveChanged.RemoveDynamic(
            this, &AVHVQ5DisplayVisibilityController::HandleObjectiveChanged);
        QuestSubsystem->OnQuestStarted.RemoveDynamic(
            this, &AVHVQ5DisplayVisibilityController::HandleQuestStarted);
        QuestSubsystem->OnQuestCompleted.RemoveDynamic(
            this, &AVHVQ5DisplayVisibilityController::HandleQuestCompleted);
    }
    if (TimeSkipSequence)
    {
        TimeSkipSequence->OnStagePresentationActivated.RemoveDynamic(
            this, &AVHVQ5DisplayVisibilityController::HandleTimeSkipStageActivated);
    }
    for (AVHVTimeSkipSequence* AdditionalSequence : AdditionalTimeSkipSequences)
    {
        if (AdditionalSequence && AdditionalSequence != TimeSkipSequence)
        {
            AdditionalSequence->OnStagePresentationActivated.RemoveDynamic(
                this, &AVHVQ5DisplayVisibilityController::HandleTimeSkipStageActivated);
        }
    }
    RestoreOriginalStates();
    Super::EndPlay(EndPlayReason);
}

void AVHVQ5DisplayVisibilityController::HandleObjectiveChanged(
    const FName QuestID, const FName ObjectiveID)
{
    HideAllDisplays();
    PendingObjectiveID = NAME_None;
    if (QuestID != RequiredQuestID)
    {
        return;
    }
    if (ShouldDeferForTimeSkip(ObjectiveID))
    {
        PendingObjectiveID = ObjectiveID;
        return;
    }
    ApplyObjectiveVisibility(ObjectiveID);
}

void AVHVQ5DisplayVisibilityController::HandleQuestStarted(const FName QuestID)
{
    if (QuestID == RequiredQuestID)
    {
        RefreshFromCurrentObjective();
    }
}

void AVHVQ5DisplayVisibilityController::HandleQuestCompleted(const FName QuestID)
{
    if (QuestID == RequiredQuestID)
    {
        PendingObjectiveID = NAME_None;
        HideAllDisplays();
    }
}

void AVHVQ5DisplayVisibilityController::HandleTimeSkipStageActivated(
    const FName StageID, const FName ObjectiveID)
{
    (void)StageID;
    if (ObjectiveID.IsNone() || ObjectiveID != PendingObjectiveID)
    {
        return;
    }
    FVHVQuestObjectiveDefinition CurrentObjective;
    if (!QuestSubsystem
        || QuestSubsystem->GetRuntimeState().ActiveQuestID != RequiredQuestID
        || !QuestSubsystem->GetCurrentObjective(CurrentObjective)
        || CurrentObjective.ObjectiveID != ObjectiveID)
    {
        RefreshFromCurrentObjective();
        return;
    }
    PendingObjectiveID = NAME_None;
    ApplyObjectiveVisibility(ObjectiveID);
}

void AVHVQ5DisplayVisibilityController::RefreshFromCurrentObjective()
{
    HideAllDisplays();
    PendingObjectiveID = NAME_None;
    if (!QuestSubsystem || QuestSubsystem->GetRuntimeState().ActiveQuestID != RequiredQuestID)
    {
        return;
    }
    FVHVQuestObjectiveDefinition Objective;
    if (!QuestSubsystem->GetCurrentObjective(Objective))
    {
        return;
    }
    if (ShouldDeferForTimeSkip(Objective.ObjectiveID))
    {
        PendingObjectiveID = Objective.ObjectiveID;
        return;
    }
    ApplyObjectiveVisibility(Objective.ObjectiveID);
}

void AVHVQ5DisplayVisibilityController::ApplyObjectiveVisibility(const FName ObjectiveID)
{
    HideAllDisplays();
    const FVHVQ5ObjectiveDisplayGroup* Group = ObjectiveDisplays.FindByPredicate(
        [ObjectiveID](const FVHVQ5ObjectiveDisplayGroup& Candidate)
        {
            return Candidate.ObjectiveID == ObjectiveID;
        });
    if (!Group)
    {
        UE_LOG(LogVHV, Verbose, TEXT("[Q5DisplayVisibility] Objective '%s' has no world display."),
            *ObjectiveID.ToString());
        return;
    }
    for (AActor* DisplayActor : Group->DisplayActors)
    {
        SetDisplayActorActive(DisplayActor, true);
    }
    UE_LOG(LogVHV, Log, TEXT("[Q5DisplayVisibility] Objective '%s' activated %d display actor(s)."),
        *ObjectiveID.ToString(), Group->DisplayActors.Num());
    if (ObjectiveID == FName(TEXT("O08_RecordAfternoon")))
    {
        for (const AActor* DisplayActor : Group->DisplayActors)
        {
            int32 GameplayComponentCount = 0;
            int32 VisibleGameplayComponentCount = 0;
            TInlineComponentArray<UPrimitiveComponent*> Components;
            if (DisplayActor)
            {
                DisplayActor->GetComponents(Components);
            }
            for (const UPrimitiveComponent* Component : Components)
            {
                if (IsGameplayDisplayComponent(Component))
                {
                    ++GameplayComponentCount;
                    VisibleGameplayComponentCount += Component->IsVisible()
                        && !Component->bHiddenInGame ? 1 : 0;
                }
            }
            UE_LOG(LogVHV, Log,
                TEXT("Q5 Tracker O08: ActiveObjective=O08_RecordAfternoon, DisplayGroup=%s, ActorHidden=%s, GameplayComponentsVisible=%d/%d"),
                *GetNameSafe(DisplayActor),
                DisplayActor && !DisplayActor->IsHidden() ? TEXT("false") : TEXT("true"),
                VisibleGameplayComponentCount,
                GameplayComponentCount);
        }
    }
}

void AVHVQ5DisplayVisibilityController::HideAllDisplays()
{
    for (const FVHVQ5ObjectiveDisplayGroup& Group : ObjectiveDisplays)
    {
        for (AActor* DisplayActor : Group.DisplayActors)
        {
            SetDisplayActorActive(DisplayActor, false);
        }
    }
}

void AVHVQ5DisplayVisibilityController::SetDisplayActorActive(
    AActor* DisplayActor, const bool bActive)
{
    if (!DisplayActor)
    {
        return;
    }
    if (bActive)
    {
        // Visibility is owned by this controller while Q5 is active. Do not
        // restore the BeginPlay hidden state here: the Q5 time-skip sequence
        // may already have hidden a future stage before this controller
        // captured it, which would keep the correct objective display hidden.
        DisplayActor->SetActorHiddenInGame(false);
        DisplayActor->SetActorEnableCollision(OriginalActorCollisionStates.FindRef(DisplayActor));
    }
    else
    {
        DisplayActor->SetActorHiddenInGame(true);
        DisplayActor->SetActorEnableCollision(false);
    }

    TInlineComponentArray<UPrimitiveComponent*> Components;
    DisplayActor->GetComponents(Components);
    for (UPrimitiveComponent* Component : Components)
    {
        if (!Component || !OriginalComponentVisibilityStates.Contains(Component))
        {
            continue;
        }
        if (bActive)
        {
            if (IsGameplayDisplayComponent(Component))
            {
                // Do not propagate to attached editor visualization helpers.
                Component->SetVisibility(true, false);
                Component->SetHiddenInGame(false, false);
                Component->SetCollisionEnabled(
                    OriginalComponentCollisionStates.FindRef(Component).GetValue());
            }
            else
            {
                // ATextRenderActor owns an editor-only BillboardComponent whose
                // sprite is the large "Tt" icon. It must never be re-enabled by
                // runtime objective presentation.
                Component->SetVisibility(false, false);
                Component->SetHiddenInGame(true, false);
                Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            }
        }
        else
        {
            Component->SetVisibility(false, false);
            Component->SetHiddenInGame(true, false);
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        }
    }
}

bool AVHVQ5DisplayVisibilityController::ShouldDeferForTimeSkip(const FName ObjectiveID) const
{
    const auto IsTransitioningToObjective = [ObjectiveID](const AVHVTimeSkipSequence* Sequence)
    {
        return Sequence
            && Sequence->GetSequenceState() == EVHVTimeSkipSequenceState::Transitioning
            && Sequence->Stages.ContainsByPredicate(
                [ObjectiveID](const FVHVTimeSkipStage& Stage)
                {
                    return Stage.ObjectiveID == ObjectiveID;
                });
    };
    if (IsTransitioningToObjective(TimeSkipSequence))
    {
        return true;
    }
    return AdditionalTimeSkipSequences.ContainsByPredicate(
        [&IsTransitioningToObjective](const TObjectPtr<AVHVTimeSkipSequence>& Sequence)
        {
            return IsTransitioningToObjective(Sequence);
        });
}

void AVHVQ5DisplayVisibilityController::RestoreOriginalStates()
{
    for (const TPair<TWeakObjectPtr<AActor>, bool>& Pair : OriginalActorHiddenStates)
    {
        if (AActor* DisplayActor = Pair.Key.Get())
        {
            DisplayActor->SetActorHiddenInGame(Pair.Value);
            DisplayActor->SetActorEnableCollision(OriginalActorCollisionStates.FindRef(Pair.Key));
        }
    }
    for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, bool>& Pair : OriginalComponentVisibilityStates)
    {
        if (UPrimitiveComponent* Component = Pair.Key.Get())
        {
            Component->SetVisibility(Pair.Value, true);
            Component->SetHiddenInGame(OriginalComponentHiddenStates.FindRef(Pair.Key), true);
            Component->SetCollisionEnabled(OriginalComponentCollisionStates.FindRef(Pair.Key).GetValue());
        }
    }
}

#if WITH_EDITOR
EDataValidationResult AVHVQ5DisplayVisibilityController::IsDataValid(
    FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    TSet<FName> ObjectiveIDs;
    TSet<const AActor*> DisplayActors;
    if (RequiredQuestID.IsNone() || ObjectiveDisplays.IsEmpty())
    {
        Context.AddError(FText::FromString(TEXT("Q5 display controller needs a quest gate and display groups.")));
        Result = EDataValidationResult::Invalid;
    }
    for (const FVHVQ5ObjectiveDisplayGroup& Group : ObjectiveDisplays)
    {
        if (Group.ObjectiveID.IsNone() || ObjectiveIDs.Contains(Group.ObjectiveID)
            || Group.DisplayActors.IsEmpty())
        {
            Context.AddError(FText::FromString(TEXT("Each Q5 display group needs a unique objective and actors.")));
            Result = EDataValidationResult::Invalid;
        }
        ObjectiveIDs.Add(Group.ObjectiveID);
        for (const AActor* DisplayActor : Group.DisplayActors)
        {
            if (!IsValid(DisplayActor) || DisplayActors.Contains(DisplayActor))
            {
                Context.AddError(FText::FromString(TEXT("Q5 display references must be valid and unique.")));
                Result = EDataValidationResult::Invalid;
            }
            DisplayActors.Add(DisplayActor);
        }
    }
    return Result;
}
#endif
