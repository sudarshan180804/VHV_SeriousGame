#include "Cinematics/VHVTimeSkipSequence.h"

#include "Ambient/VHVAmbientConversationActor.h"
#include "Cinematics/VHVTimeSkipOverlayWidget.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/DataValidation.h"
#include "NavigationSystem.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "TimerManager.h"
#include "World/Location/VHVQuestLocationVolume.h"
#include "VHV.h"

AVHVTimeSkipSequence::AVHVTimeSkipSequence()
{
    PrimaryActorTick.bCanEverTick = false;
    OverlayWidgetClass = UVHVTimeSkipOverlayWidget::StaticClass();
}

void AVHVTimeSkipSequence::BeginPlay()
{
    Super::BeginPlay();
    if (const UGameInstance* GameInstance = GetGameInstance())
    {
        QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>();
        StoryStateSubsystem = GameInstance->GetSubsystem<UVHVStoryStateSubsystem>();
    }

    for (FVHVTimeSkipStage& Stage : Stages)
    {
        if (Stage.ObservationScene)
        {
            Stage.ObservationScene->OnConversationStarted.AddUniqueDynamic(
                this, &AVHVTimeSkipSequence::HandleConversationStarted);
            Stage.ObservationScene->OnConversationFinished.AddUniqueDynamic(
                this, &AVHVTimeSkipSequence::HandleConversationFinished);
        }
        for (AActor* StageActor : Stage.StageActors)
        {
            if (StageActor && !OriginalActorHiddenStates.Contains(StageActor))
            {
                OriginalActorHiddenStates.Add(StageActor, StageActor->IsHidden());
                OriginalActorCollisionStates.Add(StageActor, StageActor->GetActorEnableCollision());
                if (const ACharacter* Character = Cast<ACharacter>(StageActor))
                {
                    if (USkeletalMeshComponent* Mesh = Character->GetMesh())
                    {
                        OriginalMeshVisibilityStates.Add(Mesh, Mesh->IsVisible());
                        OriginalMeshHiddenStates.Add(Mesh, Mesh->bHiddenInGame);
                    }
                }
            }
        }
        if (Stage.LegacyTrigger && !OriginalTriggerEnabledStates.Contains(Stage.LegacyTrigger))
        {
            OriginalTriggerEnabledStates.Add(Stage.LegacyTrigger, Stage.LegacyTrigger->bEnabled);
            OriginalTriggerCollisionStates.Add(
                Stage.LegacyTrigger,
                Stage.LegacyTrigger->BoxComponent
                    ? Stage.LegacyTrigger->BoxComponent->GetCollisionEnabled()
                    : ECollisionEnabled::NoCollision);
        }
    }
    for (AActor* SequenceActor : SequenceActors)
    {
        if (SequenceActor && !OriginalActorHiddenStates.Contains(SequenceActor))
        {
            OriginalActorHiddenStates.Add(SequenceActor, SequenceActor->IsHidden());
            OriginalActorCollisionStates.Add(SequenceActor, SequenceActor->GetActorEnableCollision());
            if (const ACharacter* Character = Cast<ACharacter>(SequenceActor))
            {
                if (USkeletalMeshComponent* Mesh = Character->GetMesh())
                {
                    OriginalMeshVisibilityStates.Add(Mesh, Mesh->IsVisible());
                    OriginalMeshHiddenStates.Add(Mesh, Mesh->bHiddenInGame);
                }
            }
        }
    }

    if (QuestSubsystem)
    {
        QuestSubsystem->OnObjectiveChanged.AddUniqueDynamic(this, &AVHVTimeSkipSequence::HandleObjectiveChanged);
        QuestSubsystem->OnObjectiveReady.AddUniqueDynamic(this, &AVHVTimeSkipSequence::HandleObjectiveReady);
        QuestSubsystem->OnObjectiveCompleted.AddUniqueDynamic(this, &AVHVTimeSkipSequence::HandleObjectiveCompleted);
        QuestSubsystem->OnQuestStarted.AddUniqueDynamic(this, &AVHVTimeSkipSequence::HandleQuestStarted);
        QuestSubsystem->OnQuestCompleted.AddUniqueDynamic(this, &AVHVTimeSkipSequence::HandleQuestCompleted);
    }
    RefreshForCurrentObjective();
}

void AVHVTimeSkipSequence::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TransitionFailSafeTimer);
        GetWorld()->GetTimerManager().ClearTimer(PostTransitionObservationTimer);
    }
    RestorePlayerInput();
    if (OverlayWidget)
    {
        OverlayWidget->CancelTransition();
        OverlayWidget->OnFullyObscured.RemoveAll(this);
        OverlayWidget->OnTransitionFinished.RemoveAll(this);
        OverlayWidget->RemoveFromParent();
    }
    for (FVHVTimeSkipStage& Stage : Stages)
    {
        if (Stage.ObservationScene)
        {
            Stage.ObservationScene->OnConversationStarted.RemoveDynamic(
                this, &AVHVTimeSkipSequence::HandleConversationStarted);
            Stage.ObservationScene->OnConversationFinished.RemoveDynamic(
                this, &AVHVTimeSkipSequence::HandleConversationFinished);
        }
    }
    if (QuestSubsystem)
    {
        QuestSubsystem->OnObjectiveChanged.RemoveDynamic(this, &AVHVTimeSkipSequence::HandleObjectiveChanged);
        QuestSubsystem->OnObjectiveReady.RemoveDynamic(this, &AVHVTimeSkipSequence::HandleObjectiveReady);
        QuestSubsystem->OnObjectiveCompleted.RemoveDynamic(this, &AVHVTimeSkipSequence::HandleObjectiveCompleted);
        QuestSubsystem->OnQuestStarted.RemoveDynamic(this, &AVHVTimeSkipSequence::HandleQuestStarted);
        QuestSubsystem->OnQuestCompleted.RemoveDynamic(this, &AVHVTimeSkipSequence::HandleQuestCompleted);
    }
    RestoreOriginalActorState();
    Super::EndPlay(EndPlayReason);
}

void AVHVTimeSkipSequence::HandleObjectiveChanged(const FName QuestID, const FName ObjectiveID)
{
    if (QuestID == RequiredQuestID && ObjectiveID == DiagnosticDestinationObjectiveID)
    {
        bDiagnosticDestinationActive = true;
        LogQ5Diagnostic(TEXT("DestinationActive"));
    }
    if (bTransitionOnNextObjectiveActivation
        && bEntryTransitionArmed
        && !bWaitForObjectiveReadyForEntryTransition
        && QuestID == RequiredQuestID
        && Stages.IsValidIndex(0)
        && Stages[0].ObjectiveID == ObjectiveID)
    {
        bEntryTransitionArmed = false;
        BeginEntryTransition(0);
        return;
    }
    RefreshForCurrentObjective();
}

void AVHVTimeSkipSequence::HandleObjectiveReady(const FName QuestID, const FName ObjectiveID)
{
    if (!bTransitionOnNextObjectiveActivation
        || !bWaitForObjectiveReadyForEntryTransition
        || !bEntryTransitionArmed
        || QuestID != RequiredQuestID
        || !Stages.IsValidIndex(0)
        || Stages[0].ObjectiveID != ObjectiveID)
    {
        return;
    }
    bEntryTransitionArmed = false;
    BeginEntryTransition(0);
}

void AVHVTimeSkipSequence::HandleObjectiveCompleted(const FName QuestID, const FName ObjectiveID)
{
    if (bTransitionOnNextObjectiveActivation)
    {
        if (QuestID == RequiredQuestID
            && ObjectiveID == RequiredObjectiveID
            && SequenceState != EVHVTimeSkipSequenceState::Transitioning)
        {
            BeginQ5Diagnostic(ObjectiveID, Stages.IsValidIndex(0) ? Stages[0].ObjectiveID : NAME_None);
            bDiagnosticSourceComplete = true;
            LogQ5Diagnostic(TEXT("SourceComplete"));
            bEntryTransitionArmed = true;
        }
        return;
    }
    if (!bAdvanceOnObjectiveCompletion || QuestID != RequiredQuestID
        || SequenceState != EVHVTimeSkipSequenceState::PlayingStage
        || !Stages.IsValidIndex(CurrentStageIndex)
        || Stages[CurrentStageIndex].ObjectiveID != ObjectiveID)
    {
        return;
    }
    if (Stages.IsValidIndex(CurrentStageIndex + 1))
    {
        BeginQ5Diagnostic(ObjectiveID, Stages[CurrentStageIndex + 1].ObjectiveID);
        bDiagnosticSourceComplete = true;
        LogQ5Diagnostic(TEXT("SourceComplete"));
    }
    NotifyCurrentStageCompleted(Stages[CurrentStageIndex].StageID, true);
}

void AVHVTimeSkipSequence::HandleQuestStarted(const FName QuestID)
{
    (void)QuestID;
    RefreshForCurrentObjective();
}

void AVHVTimeSkipSequence::HandleQuestCompleted(const FName QuestID)
{
    if (QuestID == RequiredQuestID)
    {
        bEntryTransitionArmed = false;
        ConfigureInactivePresentation(bKeepFinalStageAfterObjective);
    }
}

void AVHVTimeSkipSequence::RefreshForCurrentObjective()
{
    if (!QuestSubsystem || Stages.IsEmpty())
    {
        ConfigureInactivePresentation(false);
        return;
    }
    const FVHVQuestArcRuntimeState Runtime = QuestSubsystem->GetRuntimeState();
    FVHVQuestObjectiveDefinition Objective;
    const bool bHasObjective = QuestSubsystem->GetCurrentObjective(Objective);
    if (bTransitionOnNextObjectiveActivation)
    {
        if (SequenceState == EVHVTimeSkipSequenceState::Transitioning)
        {
            return;
        }
        const bool bTargetObjective = Runtime.ActiveQuestID == RequiredQuestID
            && bHasObjective
            && Stages.IsValidIndex(0)
            && Objective.ObjectiveID == Stages[0].ObjectiveID;
        if (!bTargetObjective && SequenceState == EVHVTimeSkipSequenceState::PlayingStage)
        {
            ConfigureInactivePresentation(false);
        }
        return;
    }
    const bool bStageObjectiveMatches = bAdvanceOnObjectiveCompletion
        && Stages.ContainsByPredicate([&Objective](const FVHVTimeSkipStage& Stage)
        {
            return Stage.ObjectiveID == Objective.ObjectiveID;
        });
    const bool bMatches = Runtime.ActiveQuestID == RequiredQuestID
        && bHasObjective
        && (bAdvanceOnObjectiveCompletion ? bStageObjectiveMatches
                                          : Objective.ObjectiveID == RequiredObjectiveID);
    if (bMatches)
    {
        ActivateSequence();
    }
    else if (SequenceState != EVHVTimeSkipSequenceState::Completed || !bKeepFinalStageAfterObjective)
    {
        ConfigureInactivePresentation(false);
    }
}

void AVHVTimeSkipSequence::ActivateSequence()
{
    if (SequenceState == EVHVTimeSkipSequenceState::PlayingStage
        || SequenceState == EVHVTimeSkipSequenceState::Transitioning)
    {
        return;
    }
    if (bAdvanceOnObjectiveCompletion && QuestSubsystem)
    {
        FVHVQuestObjectiveDefinition Objective;
        QuestSubsystem->GetCurrentObjective(Objective);
        CurrentStageIndex = Stages.IndexOfByPredicate([&Objective](const FVHVTimeSkipStage& Stage)
        {
            return Stage.ObjectiveID == Objective.ObjectiveID;
        });
        if (!Stages.IsValidIndex(CurrentStageIndex))
        {
            ConfigureInactivePresentation(false);
            return;
        }
    }
    else
    {
        const int32 CompletedCount = StoryStateSubsystem && !ProgressCounterID.IsNone()
            ? StoryStateSubsystem->GetCounter(ProgressCounterID)
            : 0;
        CurrentStageIndex = FMath::Clamp(CompletedCount, 0, Stages.Num() - 1);
    }
    CompletedStageIDs.Reset();
    for (int32 Index = 0; Index < CurrentStageIndex; ++Index)
    {
        CompletedStageIDs.Add(Stages[Index].StageID);
    }
    PendingStageIndex = INDEX_NONE;
    bCurrentStageConversationStarted = false;
    SequenceState = EVHVTimeSkipSequenceState::PlayingStage;
    ApplyStagePresentation(CurrentStageIndex);
    for (int32 Index = 0; Index < Stages.Num(); ++Index)
    {
        SetLegacyTriggerEnabled(Index, Index == CurrentStageIndex);
    }
}

void AVHVTimeSkipSequence::ConfigureInactivePresentation(const bool bKeepFinalStage)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TransitionFailSafeTimer);
        GetWorld()->GetTimerManager().ClearTimer(PostTransitionObservationTimer);
    }
    RestorePlayerInput();
    if (OverlayWidget)
    {
        OverlayWidget->CancelTransition();
    }
    SequenceState = bKeepFinalStage ? EVHVTimeSkipSequenceState::Completed : EVHVTimeSkipSequenceState::Idle;
    CurrentStageIndex = bKeepFinalStage && !Stages.IsEmpty() ? Stages.Num() - 1 : INDEX_NONE;
    PendingStageIndex = INDEX_NONE;
    bCurrentStageConversationStarted = false;
    if (bStopObservationsWhenDeactivated)
    {
        for (FVHVTimeSkipStage& Stage : Stages)
        {
            if (Stage.ObservationScene && Stage.ObservationScene->IsPlaying())
            {
                Stage.ObservationScene->StopConversation();
            }
        }
    }
    ApplyStagePresentation(CurrentStageIndex);
    for (int32 Index = 0; Index < Stages.Num(); ++Index)
    {
        SetLegacyTriggerEnabled(Index, false);
    }
}

void AVHVTimeSkipSequence::ApplyStagePresentation(const int32 StageIndex)
{
    const bool bSequenceActive = Stages.IsValidIndex(StageIndex);
    for (AActor* SequenceActor : SequenceActors)
    {
        if (!SequenceActor)
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVTimeSkip] Sequence has a missing shared actor reference."));
            continue;
        }
        const bool bOriginallyCollidable = OriginalActorCollisionStates.FindRef(SequenceActor);
        SequenceActor->SetActorHiddenInGame(!bSequenceActive);
        SequenceActor->SetActorEnableCollision(bSequenceActive && bOriginallyCollidable);
        if (bSequenceActive)
        {
            if (ACharacter* Character = Cast<ACharacter>(SequenceActor))
            {
                if (USkeletalMeshComponent* Mesh = Character->GetMesh())
                {
                    Mesh->SetVisibility(true, true);
                    Mesh->SetHiddenInGame(false, true);
                }
            }
        }
    }
    for (int32 Index = 0; Index < Stages.Num(); ++Index)
    {
        const bool bActive = Index == StageIndex;
        for (AActor* StageActor : Stages[Index].StageActors)
        {
            if (!StageActor)
            {
                UE_LOG(LogVHV, Error, TEXT("[VHVTimeSkip] Stage '%s' has a missing stage actor reference."),
                    *Stages[Index].StageID.ToString());
                continue;
            }
            const bool bOriginallyCollidable = OriginalActorCollisionStates.FindRef(StageActor);
            // The sequence owns visibility while active. Original hidden state is
            // retained only for EndPlay restoration; honoring it here made an NPC
            // captured as hidden impossible to reveal at its later timeline stage.
            StageActor->SetActorHiddenInGame(!bActive);
            StageActor->SetActorEnableCollision(bActive && bOriginallyCollidable);
            if (bActive)
            {
                if (ACharacter* Character = Cast<ACharacter>(StageActor))
                {
                    if (USkeletalMeshComponent* Mesh = Character->GetMesh())
                    {
                        Mesh->SetVisibility(true, true);
                        Mesh->SetHiddenInGame(false, true);
                    }
                }
                if (StageActor->IsHidden())
                {
                    UE_LOG(LogVHV, Error,
                        TEXT("[VHVTimeSkip] Stage '%s' actor '%s' remained hidden after activation."),
                        *Stages[Index].StageID.ToString(), *GetNameSafe(StageActor));
                }
            }
        }
    }
}

void AVHVTimeSkipSequence::SetLegacyTriggerEnabled(const int32 StageIndex, const bool bEnabled)
{
    if (!Stages.IsValidIndex(StageIndex) || !Stages[StageIndex].LegacyTrigger)
    {
        return;
    }
    AVHVQuestLocationVolume* Trigger = Stages[StageIndex].LegacyTrigger;
    Trigger->bEnabled = bEnabled;
    if (Trigger->BoxComponent)
    {
        const ECollisionEnabled::Type EnabledCollision = OriginalTriggerCollisionStates.Contains(Trigger)
            ? OriginalTriggerCollisionStates.FindRef(Trigger).GetValue()
            : ECollisionEnabled::QueryOnly;
        Trigger->BoxComponent->SetCollisionEnabled(
            bEnabled ? EnabledCollision : ECollisionEnabled::NoCollision);
    }
}

void AVHVTimeSkipSequence::HandleConversationStarted()
{
    if (SequenceState != EVHVTimeSkipSequenceState::PlayingStage
        || !Stages.IsValidIndex(CurrentStageIndex)
        || !Stages[CurrentStageIndex].ObservationScene
        || !Stages[CurrentStageIndex].ObservationScene->IsPlaying())
    {
        return;
    }
    bCurrentStageConversationStarted = true;
    SetLegacyTriggerEnabled(CurrentStageIndex, false);
}

void AVHVTimeSkipSequence::HandleConversationFinished(const bool bSuccess)
{
    if (SequenceState != EVHVTimeSkipSequenceState::PlayingStage
        || !bCurrentStageConversationStarted
        || !Stages.IsValidIndex(CurrentStageIndex))
    {
        return;
    }
    bCurrentStageConversationStarted = false;
    if (bAdvanceOnObjectiveCompletion)
    {
        if (!bSuccess)
        {
            SetLegacyTriggerEnabled(CurrentStageIndex, true);
        }
        return;
    }
    NotifyCurrentStageCompleted(Stages[CurrentStageIndex].StageID, bSuccess);
}

bool AVHVTimeSkipSequence::NotifyCurrentStageCompleted(const FName StageID, const bool bSuccess)
{
    if (!bSuccess || SequenceState != EVHVTimeSkipSequenceState::PlayingStage
        || !Stages.IsValidIndex(CurrentStageIndex)
        || Stages[CurrentStageIndex].StageID != StageID
        || CompletedStageIDs.Contains(StageID))
    {
        if (!bSuccess && Stages.IsValidIndex(CurrentStageIndex))
        {
            SetLegacyTriggerEnabled(CurrentStageIndex, true);
        }
        return false;
    }

    CompletedStageIDs.Add(StageID);
    if (CurrentStageIndex >= Stages.Num() - 1)
    {
        SequenceState = EVHVTimeSkipSequenceState::Completed;
        for (int32 Index = 0; Index < Stages.Num(); ++Index)
        {
            SetLegacyTriggerEnabled(Index, false);
        }
        RestorePlayerInput();
        OnSequenceCompleted.Broadcast();
        if (bRequestObjectiveActivationOnSequenceCompleted && QuestSubsystem)
        {
            if (!QuestSubsystem->RequestCurrentObjectiveActivation())
            {
                UE_LOG(LogVHV, Error,
                    TEXT("[VHVTimeSkip] Final stage '%s' completed, but the required objective activity could not be activated."),
                    *StageID.ToString());
            }
        }
        return true;
    }
    return BeginTransition(CurrentStageIndex + 1);
}

bool AVHVTimeSkipSequence::BeginTransition(const int32 NextStageIndex)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PostTransitionObservationTimer);
    }
    if (SequenceState != EVHVTimeSkipSequenceState::PlayingStage
        || !Stages.IsValidIndex(NextStageIndex))
    {
        DiagnosticBlockReason = TEXT("invalid sequence state or destination stage");
        LogQ5Diagnostic(TEXT("Blocked"));
        EnableFallbackForStage(NextStageIndex, TEXT("transition preflight failed"));
        return false;
    }
    if (!EnsureOverlay())
    {
        DiagnosticBlockReason = TEXT("overlay could not be created");
        LogQ5Diagnostic(TEXT("Blocked"));
        EnableFallbackForStage(NextStageIndex, TEXT("transition overlay unavailable"));
        return false;
    }
    if (!ResolvePlayerArrival(NextStageIndex, PendingPlayerTransform))
    {
        LogQ5Diagnostic(TEXT("Blocked"));
        EnableFallbackForStage(NextStageIndex, TEXT("arrival validation failed"));
        return false;
    }

    bPendingPlayerRelocation = Stages[NextStageIndex].PlayerArrivalTarget != nullptr
        || Stages[NextStageIndex].PlayerArrivalRelativeActor != nullptr;
    PendingStageIndex = NextStageIndex;
    SequenceState = EVHVTimeSkipSequenceState::Transitioning;
    LockPlayerInput();
    GetWorld()->GetTimerManager().SetTimer(
        TransitionFailSafeTimer,
        this,
        &AVHVTimeSkipSequence::HandleTransitionFailSafe,
        TransitionFailSafeDuration,
        false);
    OverlayWidget->PlayTransition(
        Stages[CurrentStageIndex].TransitionText,
        FadeInDuration,
        TextHoldDuration,
        FadeOutDuration);
    bDiagnosticTransitionRequested = true;
    bDiagnosticOverlayStarted = true;
    DiagnosticBlockReason.Reset();
    LogQ5Diagnostic(TEXT("OverlayStarted"));
    return true;
}

bool AVHVTimeSkipSequence::BeginEntryTransition(const int32 StageIndex)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(PostTransitionObservationTimer);
    }
    if (SequenceState == EVHVTimeSkipSequenceState::Transitioning
        || !Stages.IsValidIndex(StageIndex))
    {
        DiagnosticBlockReason = TEXT("invalid sequence state or destination stage");
        LogQ5Diagnostic(TEXT("Blocked"));
        EnableFallbackForStage(StageIndex, TEXT("entry transition preflight failed"));
        return false;
    }
    if (!EnsureOverlay())
    {
        DiagnosticBlockReason = TEXT("overlay could not be created");
        LogQ5Diagnostic(TEXT("Blocked"));
        EnableFallbackForStage(StageIndex, TEXT("entry transition overlay unavailable"));
        return false;
    }
    if (!ResolvePlayerArrival(StageIndex, PendingPlayerTransform))
    {
        LogQ5Diagnostic(TEXT("Blocked"));
        EnableFallbackForStage(StageIndex, TEXT("entry arrival validation failed"));
        return false;
    }

    bPendingPlayerRelocation = Stages[StageIndex].PlayerArrivalTarget != nullptr
        || Stages[StageIndex].PlayerArrivalRelativeActor != nullptr;
    CurrentStageIndex = INDEX_NONE;
    PendingStageIndex = StageIndex;
    SequenceState = EVHVTimeSkipSequenceState::Transitioning;
    LockPlayerInput();
    GetWorld()->GetTimerManager().SetTimer(
        TransitionFailSafeTimer,
        this,
        &AVHVTimeSkipSequence::HandleTransitionFailSafe,
        TransitionFailSafeDuration,
        false);
    OverlayWidget->PlayTransition(
        Stages[StageIndex].TransitionText,
        FadeInDuration,
        TextHoldDuration,
        FadeOutDuration);
    bDiagnosticTransitionRequested = true;
    bDiagnosticOverlayStarted = true;
    DiagnosticBlockReason.Reset();
    LogQ5Diagnostic(TEXT("OverlayStarted"));
    return true;
}

bool AVHVTimeSkipSequence::EnsureOverlay()
{
    if (OverlayWidget)
    {
        return true;
    }
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PlayerController || !OverlayWidgetClass)
    {
        return false;
    }
    OverlayWidget = CreateWidget<UVHVTimeSkipOverlayWidget>(PlayerController, OverlayWidgetClass);
    if (!OverlayWidget)
    {
        return false;
    }
    OverlayWidget->AddToViewport(1000);
    OverlayWidget->SetVisibility(ESlateVisibility::Collapsed);
    OverlayWidget->OnFullyObscured.AddUObject(this, &AVHVTimeSkipSequence::HandleOverlayFullyObscured);
    OverlayWidget->OnTransitionFinished.AddUObject(this, &AVHVTimeSkipSequence::HandleOverlayFinished);
    return true;
}

bool AVHVTimeSkipSequence::ResolvePlayerArrival(const int32 StageIndex, FTransform& OutTransform)
{
    if (!Stages.IsValidIndex(StageIndex))
    {
        DiagnosticBlockReason = TEXT("destination stage index is invalid");
        return false;
    }
    const FVHVTimeSkipStage& Stage = Stages[StageIndex];
    const AActor* RelativeActor = Stage.PlayerArrivalRelativeActor;
    const AActor* ArrivalTarget = RelativeActor ? RelativeActor : Stage.PlayerArrivalTarget;
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    ACharacter* PlayerCharacter = PlayerController ? Cast<ACharacter>(PlayerController->GetPawn()) : nullptr;
    if (!PlayerCharacter)
    {
        DiagnosticBlockReason = TEXT("player character is unavailable");
        return false;
    }
    if (!ArrivalTarget)
    {
        OutTransform = PlayerCharacter->GetActorTransform();
        bDiagnosticArrivalResolved = true;
        bDiagnosticCollisionValid = true;
        LogQ5Diagnostic(TEXT("ArrivalValidated"));
        return true;
    }

    bDiagnosticArrivalResolved = IsValid(ArrivalTarget);
    if (!bDiagnosticArrivalResolved)
    {
        DiagnosticBlockReason = TEXT("arrival target reference is invalid");
        return false;
    }

    FVector ArrivalAnchor = ArrivalTarget->GetActorLocation();
    FRotator ArrivalRotation(0.0f, ArrivalTarget->GetActorRotation().Yaw, 0.0f);
    if (RelativeActor)
    {
        FVector Forward = RelativeActor->GetActorForwardVector().GetSafeNormal2D();
        if (Forward.IsNearlyZero())
        {
            Forward = FVector::ForwardVector;
        }
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal2D();
        const float BehindDistance = FMath::Clamp(Stage.PlayerArrivalBehindDistance, 150.0f, 260.0f);
        const float LateralOffset = FMath::Clamp(Stage.PlayerArrivalLateralOffset, -100.0f, 100.0f);
        ArrivalAnchor = RelativeActor->GetActorLocation() - Forward * BehindDistance + Right * LateralOffset;
        ArrivalRotation = (RelativeActor->GetActorLocation() - ArrivalAnchor).Rotation();
        ArrivalRotation.Pitch = 0.0f;
        ArrivalRotation.Roll = 0.0f;
    }

    FNavLocation ProjectedLocation(ArrivalAnchor);
    if (bRequireNavigationProjection)
    {
        UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        if (!Navigation || !Navigation->ProjectPointToNavigation(
                ArrivalAnchor, ProjectedLocation, FVector(75.0f, 75.0f, 500.0f)))
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVTimeSkip] Stage '%s' has no nearby Recast arrival point."),
                *Stages[StageIndex].StageID.ToString());
            return false;
        }
    }

    const UCapsuleComponent* Capsule = PlayerCharacter->GetCapsuleComponent();
    if (!Capsule)
    {
        DiagnosticBlockReason = TEXT("player capsule is unavailable");
        return false;
    }

    const FVector CapsuleLocalOffset = PlayerCharacter->GetActorTransform().InverseTransformPosition(
        Capsule->GetComponentLocation());
    const FVector RotatedCapsuleOffset = ArrivalRotation.RotateVector(CapsuleLocalOffset);
    constexpr float GroundClearance = 3.0f;
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const float Radius = Capsule->GetScaledCapsuleRadius();

    // Q3 keeps its authored Recast/Landscape behavior. Q5 cinematic arrivals and
    // runtime-relative arrivals accept any genuinely walkable Pawn-blocking floor
    // (including house meshes and platforms), followed by a full capsule test.
    if (!bRequireNavigationProjection || RelativeActor)
    {
        const FVector Anchor = RelativeActor ? ProjectedLocation.Location : ArrivalAnchor;
        const FVector TraceStart = Anchor + FVector(0.0f, 0.0f, 25.0f);
        const FVector TraceEnd = Anchor - FVector(0.0f, 0.0f, FMath::Max(1000.0f, HalfHeight * 6.0f));
        FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(VHVTimeSkipArrivalWalkableGround), true, PlayerCharacter);
        GroundQuery.AddIgnoredActor(this);
        GroundQuery.AddIgnoredActor(ArrivalTarget);
        if (Stages[StageIndex].LegacyTrigger)
        {
            GroundQuery.AddIgnoredActor(Stages[StageIndex].LegacyTrigger);
        }

        FHitResult GroundHit;
        bool bFoundWalkableSupport = false;
        const UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement();
        const float WalkableFloorZ = Movement ? Movement->GetWalkableFloorZ() : 0.70f;
        for (int32 TraceAttempt = 0; TraceAttempt < 16; ++TraceAttempt)
        {
            FHitResult Hit;
            if (!GetWorld()->LineTraceSingleByChannel(
                    Hit, TraceStart, TraceEnd, ECC_Pawn, GroundQuery))
            {
                break;
            }
            const AActor* RequiredSupport = Stages[StageIndex].PlayerArrivalSupportActor;
            const bool bSupportMatches = !RequiredSupport || Hit.GetActor() == RequiredSupport;
            const bool bWalkable = Hit.bBlockingHit && Hit.ImpactNormal.Z >= WalkableFloorZ;
            if (bSupportMatches && bWalkable)
            {
                GroundHit = Hit;
                bFoundWalkableSupport = true;
                break;
            }
            if (Hit.GetActor())
            {
                GroundQuery.AddIgnoredActor(Hit.GetActor());
            }
            if (Hit.GetComponent())
            {
                GroundQuery.AddIgnoredComponent(Hit.GetComponent());
            }
        }
        if (!bFoundWalkableSupport)
        {
            DiagnosticBlockReason = TEXT("no walkable Pawn-blocking floor below arrival anchor");
            UE_LOG(LogVHV, Warning,
                TEXT("[VHVTimeSkip] Stage '%s' has no walkable floor below arrival '%s'."),
                *Stages[StageIndex].StageID.ToString(), *GetNameSafe(ArrivalTarget));
            return false;
        }

        const FVector ActorDestination(
            Anchor.X,
            Anchor.Y,
            GroundHit.ImpactPoint.Z + HalfHeight + GroundClearance - RotatedCapsuleOffset.Z);
        const FVector CapsuleCenter = ActorDestination + RotatedCapsuleOffset;
        FCollisionQueryParams CapsuleQuery(SCENE_QUERY_STAT(VHVTimeSkipArrivalCapsule), false, PlayerCharacter);
        CapsuleQuery.AddIgnoredActor(this);
        CapsuleQuery.AddIgnoredActor(ArrivalTarget);
        if (GetWorld()->OverlapBlockingTestByProfile(
                CapsuleCenter,
                ArrivalRotation.Quaternion(),
                Capsule->GetCollisionProfileName(),
                FCollisionShape::MakeCapsule(Radius, HalfHeight),
                CapsuleQuery))
        {
            DiagnosticBlockReason = TEXT("player capsule is blocked at resolved arrival");
            UE_LOG(LogVHV, Warning, TEXT("[VHVTimeSkip] Stage '%s' player arrival capsule is blocked."),
                *Stages[StageIndex].StageID.ToString());
            return false;
        }

        OutTransform = FTransform(ArrivalRotation, ActorDestination, FVector::OneVector);
        bDiagnosticCollisionValid = true;
        DiagnosticBlockReason.Reset();
        UE_LOG(LogVHV, Log,
            TEXT("[VHVTimeSkip] Stage '%s' arrival '%s' validated on support '%s/%s' at %s."),
            *Stages[StageIndex].StageID.ToString(), *GetNameSafe(ArrivalTarget),
            *GetNameSafe(GroundHit.GetActor()), *GetNameSafe(GroundHit.GetComponent()),
            *ActorDestination.ToCompactString());
        LogQ5Diagnostic(TEXT("ArrivalValidated"));
        return true;
    }

    // A Recast projection describes a support surface, not the center of a
    // character capsule. Resolve either the explicitly configured platform or,
    // for existing Q3 instances, Landscape without changing their behavior.
    const FVector IntendedLocation = RelativeActor ? ProjectedLocation.Location : ArrivalAnchor;
    const FVector IntendedXY(IntendedLocation.X, IntendedLocation.Y, 0.0f);
    const FVector TraceStart(IntendedXY.X, IntendedXY.Y,
        FMath::Max(ArrivalAnchor.Z, ProjectedLocation.Location.Z) + 3000.0f);
    const FVector TraceEnd(IntendedXY.X, IntendedXY.Y, TraceStart.Z - 10000.0f);
    FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(VHVTimeSkipArrivalGround), true, PlayerCharacter);
    GroundQuery.AddIgnoredActor(this);
    GroundQuery.AddIgnoredActor(ArrivalTarget);

    FHitResult GroundHit;
    bool bFoundLandscape = false;
    for (int32 TraceAttempt = 0; TraceAttempt < 16; ++TraceAttempt)
    {
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, GroundQuery))
        {
            break;
        }

        const AActor* RequiredSupport = Stages[StageIndex].PlayerArrivalSupportActor;
        const UClass* ActorClass = Hit.GetActor() ? Hit.GetActor()->GetClass() : nullptr;
        const UClass* ComponentClass = Hit.GetComponent() ? Hit.GetComponent()->GetClass() : nullptr;
        const bool bLandscapeActor = ActorClass && ActorClass->GetName().Contains(TEXT("Landscape"));
        const bool bLandscapeComponent = ComponentClass && ComponentClass->GetName().Contains(TEXT("Landscape"));
        const bool bRequiredSupportHit = RequiredSupport && Hit.GetActor() == RequiredSupport;
        const bool bAcceptedSupport = RequiredSupport
            ? bRequiredSupportHit
            : (bLandscapeActor || bLandscapeComponent);
        if (Hit.bBlockingHit && bAcceptedSupport)
        {
            GroundHit = Hit;
            bFoundLandscape = true;
            break;
        }

        // Continue through roofs and scenery solely to find the actual support
        // height. The final full-size capsule test still rejects those blockers.
        if (Hit.GetActor())
        {
            GroundQuery.AddIgnoredActor(Hit.GetActor());
        }
        if (Hit.GetComponent())
        {
            GroundQuery.AddIgnoredComponent(Hit.GetComponent());
        }
    }
    if (!bFoundLandscape)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVTimeSkip] Stage '%s' has no valid configured support at its intended XY."),
            *Stages[StageIndex].StageID.ToString());
        return false;
    }

    const FVector ActorDestination(
        IntendedXY.X,
        IntendedXY.Y,
        GroundHit.ImpactPoint.Z + HalfHeight + GroundClearance - RotatedCapsuleOffset.Z);
    const FVector CapsuleCenter = ActorDestination + RotatedCapsuleOffset;

    FCollisionQueryParams CapsuleQuery(SCENE_QUERY_STAT(VHVTimeSkipArrivalCapsule), false, PlayerCharacter);
    CapsuleQuery.AddIgnoredActor(this);
    CapsuleQuery.AddIgnoredActor(ArrivalTarget);
    if (GetWorld()->OverlapBlockingTestByProfile(
            CapsuleCenter,
            ArrivalRotation.Quaternion(),
            Capsule->GetCollisionProfileName(),
            FCollisionShape::MakeCapsule(Radius, HalfHeight),
            CapsuleQuery))
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVTimeSkip] Stage '%s' player arrival capsule is blocked."),
            *Stages[StageIndex].StageID.ToString());
        return false;
    }

    OutTransform = FTransform(ArrivalRotation, ActorDestination, FVector::OneVector);
    bDiagnosticCollisionValid = true;
    DiagnosticBlockReason.Reset();
    LogQ5Diagnostic(TEXT("ArrivalValidated"));
    return true;
}

void AVHVTimeSkipSequence::HandleOverlayFullyObscured()
{
    if (SequenceState != EVHVTimeSkipSequenceState::Transitioning
        || !Stages.IsValidIndex(PendingStageIndex))
    {
        return;
    }
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (bPendingPlayerRelocation
        && (!PlayerPawn || !PlayerPawn->TeleportTo(
            PendingPlayerTransform.GetLocation(), PendingPlayerTransform.Rotator(), false, true)))
    {
        HandleTransitionFailSafe();
        return;
    }
    if (PlayerController && bPendingPlayerRelocation)
    {
        PlayerController->SetControlRotation(PendingPlayerTransform.Rotator());
    }
    FVHVTimeSkipStage& PendingStage = Stages[PendingStageIndex];
    if (bPendingPlayerRelocation
        && PendingStage.bFaceRelativeActorTowardPlayer
        && IsValid(PendingStage.PlayerArrivalRelativeActor))
    {
        const FVector ToPlayer = PendingPlayerTransform.GetLocation()
            - PendingStage.PlayerArrivalRelativeActor->GetActorLocation();
        if (!ToPlayer.IsNearlyZero())
        {
            FRotator FacingRotation = ToPlayer.Rotation();
            FacingRotation.Pitch = 0.0f;
            FacingRotation.Roll = 0.0f;
            PendingStage.PlayerArrivalRelativeActor->SetActorRotation(
                FacingRotation, ETeleportType::TeleportPhysics);
            PendingStage.PlayerArrivalRelativeActor->ForceNetUpdate();
        }
    }
    const int32 PreviousStageIndex = CurrentStageIndex;
    ApplyStagePresentation(PendingStageIndex);
    for (int32 Index = 0; Index < Stages.Num(); ++Index)
    {
        SetLegacyTriggerEnabled(Index, false);
    }
    CurrentStageIndex = PendingStageIndex;
    int32 ActivatedActorCount = 0;
    for (const AActor* Actor : Stages[CurrentStageIndex].StageActors)
    {
        ActivatedActorCount += IsValid(Actor) ? 1 : 0;
    }
    OnStagePresentationActivated.Broadcast(
        Stages[CurrentStageIndex].StageID,
        Stages[CurrentStageIndex].ObjectiveID);
    UE_LOG(LogVHV, Log, TEXT("Q3 TimeSkip: %s -> %s | ActiveStage=%d | ActorsActivated=%d"),
        Stages.IsValidIndex(PreviousStageIndex)
            ? *Stages[PreviousStageIndex].StageID.ToString() : TEXT("None"),
        *Stages[CurrentStageIndex].StageID.ToString(), CurrentStageIndex, ActivatedActorCount);
}

void AVHVTimeSkipSequence::HandleOverlayFinished()
{
    if (SequenceState != EVHVTimeSkipSequenceState::Transitioning
        || !Stages.IsValidIndex(CurrentStageIndex))
    {
        return;
    }
    GetWorld()->GetTimerManager().ClearTimer(TransitionFailSafeTimer);
    PendingStageIndex = INDEX_NONE;
    bPendingPlayerRelocation = false;
    SequenceState = EVHVTimeSkipSequenceState::PlayingStage;
    RestorePlayerInput();
    bDiagnosticInputRestored = true;
    LogQ5Diagnostic(TEXT("InputRestored"));

    FVHVTimeSkipStage& Stage = Stages[CurrentStageIndex];
    if (Stage.bAutoStartObservationAfterTransition && Stage.ObservationScene)
    {
        if (Stage.PostTransitionObservationDelay > 0.0f)
        {
            GetWorld()->GetTimerManager().SetTimer(
                PostTransitionObservationTimer,
                this,
                &AVHVTimeSkipSequence::StartPendingAutoObservation,
                Stage.PostTransitionObservationDelay,
                false);
        }
        else
        {
            StartPendingAutoObservation();
        }
    }
    else
    {
        SetLegacyTriggerEnabled(CurrentStageIndex, true);
    }
}

void AVHVTimeSkipSequence::StartPendingAutoObservation()
{
    if (SequenceState != EVHVTimeSkipSequenceState::PlayingStage
        || !Stages.IsValidIndex(CurrentStageIndex))
    {
        return;
    }
    FVHVTimeSkipStage& Stage = Stages[CurrentStageIndex];
    if (!Stage.bAutoStartObservationAfterTransition || !Stage.ObservationScene
        || !Stage.ObservationScene->StartConversation())
    {
        EnableFallbackForStage(CurrentStageIndex, TEXT("automatic observation start was rejected"));
    }
}

void AVHVTimeSkipSequence::HandleTransitionFailSafe()
{
    if (SequenceState != EVHVTimeSkipSequenceState::Transitioning)
    {
        return;
    }
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(TransitionFailSafeTimer);
    }
    if (OverlayWidget)
    {
        OverlayWidget->CancelTransition();
    }
    RestorePlayerInput();
    bDiagnosticInputRestored = true;
    DiagnosticBlockReason = TEXT("transition fail-safe fired");
    LogQ5Diagnostic(TEXT("FailSafe"));
    EnableFallbackForStage(PendingStageIndex, TEXT("transition fail-safe fired"));
}

void AVHVTimeSkipSequence::LockPlayerInput()
{
    if (bInputLocked)
    {
        return;
    }
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerController || !PlayerPawn)
    {
        return;
    }
    bChangedMoveIgnore = !PlayerController->IsMoveInputIgnored();
    bChangedLookIgnore = !PlayerController->IsLookInputIgnored();
    if (bChangedMoveIgnore) PlayerController->SetIgnoreMoveInput(true);
    if (bChangedLookIgnore) PlayerController->SetIgnoreLookInput(true);
    bDisabledPawnInput = PlayerPawn->InputEnabled();
    if (bDisabledPawnInput) PlayerPawn->DisableInput(PlayerController);
    if (ACharacter* Character = Cast<ACharacter>(PlayerPawn))
    {
        Character->StopJumping();
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
        }
    }
    bInputLocked = true;
}

void AVHVTimeSkipSequence::RestorePlayerInput()
{
    if (!bInputLocked)
    {
        return;
    }
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (PlayerController)
    {
        if (bChangedMoveIgnore) PlayerController->SetIgnoreMoveInput(false);
        if (bChangedLookIgnore) PlayerController->SetIgnoreLookInput(false);
        if (bDisabledPawnInput && PlayerPawn) PlayerPawn->EnableInput(PlayerController);
    }
    bChangedMoveIgnore = false;
    bChangedLookIgnore = false;
    bDisabledPawnInput = false;
    bInputLocked = false;
}

void AVHVTimeSkipSequence::EnableFallbackForStage(const int32 StageIndex, const TCHAR* Reason)
{
    UE_LOG(LogVHV, Warning, TEXT("[VHVTimeSkip] Sequence '%s' using safe fallback at stage %d: %s."),
        *GetName(), StageIndex, Reason);
    if (Stages.IsValidIndex(StageIndex))
    {
        CurrentStageIndex = StageIndex;
        ApplyStagePresentation(CurrentStageIndex);
        for (int32 Index = 0; Index < Stages.Num(); ++Index)
        {
            SetLegacyTriggerEnabled(Index, Index == CurrentStageIndex);
        }
    }
    PendingStageIndex = INDEX_NONE;
    bPendingPlayerRelocation = false;
    bCurrentStageConversationStarted = false;
    SequenceState = EVHVTimeSkipSequenceState::PlayingStage;
}

void AVHVTimeSkipSequence::RestoreOriginalActorState()
{
    for (const TPair<TWeakObjectPtr<AActor>, bool>& Pair : OriginalActorHiddenStates)
    {
        if (AActor* Actor = Pair.Key.Get())
        {
            Actor->SetActorHiddenInGame(Pair.Value);
            Actor->SetActorEnableCollision(OriginalActorCollisionStates.FindRef(Pair.Key));
        }
    }
    for (const TPair<TWeakObjectPtr<USkeletalMeshComponent>, bool>& Pair : OriginalMeshVisibilityStates)
    {
        if (USkeletalMeshComponent* Mesh = Pair.Key.Get())
        {
            Mesh->SetVisibility(Pair.Value, true);
            Mesh->SetHiddenInGame(OriginalMeshHiddenStates.FindRef(Pair.Key), true);
        }
    }
    for (const TPair<TWeakObjectPtr<AVHVQuestLocationVolume>, bool>& Pair : OriginalTriggerEnabledStates)
    {
        if (AVHVQuestLocationVolume* Trigger = Pair.Key.Get())
        {
            Trigger->bEnabled = Pair.Value;
            if (Trigger->BoxComponent)
            {
                Trigger->BoxComponent->SetCollisionEnabled(
                    OriginalTriggerCollisionStates.FindRef(Pair.Key).GetValue());
            }
        }
    }
}

void AVHVTimeSkipSequence::BeginQ5Diagnostic(
    const FName SourceObjectiveID, const FName DestinationObjectiveID)
{
    if (RequiredQuestID != FName(TEXT("Q_HBCT_05_SELF_MONITORING")))
    {
        return;
    }
    DiagnosticSourceObjectiveID = SourceObjectiveID;
    DiagnosticDestinationObjectiveID = DestinationObjectiveID;
    bDiagnosticSourceComplete = false;
    bDiagnosticDestinationActive = false;
    bDiagnosticTransitionRequested = false;
    bDiagnosticArrivalResolved = false;
    bDiagnosticCollisionValid = false;
    bDiagnosticOverlayStarted = false;
    bDiagnosticInputRestored = false;
    DiagnosticBlockReason.Reset();
}

void AVHVTimeSkipSequence::LogQ5Diagnostic(const TCHAR* Phase) const
{
    if (RequiredQuestID != FName(TEXT("Q_HBCT_05_SELF_MONITORING"))
        || DiagnosticSourceObjectiveID.IsNone()
        || DiagnosticDestinationObjectiveID.IsNone())
    {
        return;
    }
    UE_LOG(LogVHV, Log,
        TEXT("Q5 TimeSkip %s->%s [%s]: SourceComplete=%s, DestActive=%s, SequenceResolved=true, TransitionRequested=%s, ArrivalReferenceResolved=%s, CollisionValid=%s, OverlayStarted=%s, InputRestored=%s%s%s"),
        *DiagnosticSourceObjectiveID.ToString(),
        *DiagnosticDestinationObjectiveID.ToString(),
        Phase,
        bDiagnosticSourceComplete ? TEXT("true") : TEXT("false"),
        bDiagnosticDestinationActive ? TEXT("true") : TEXT("false"),
        bDiagnosticTransitionRequested ? TEXT("true") : TEXT("false"),
        bDiagnosticArrivalResolved ? TEXT("true") : TEXT("false"),
        bDiagnosticCollisionValid ? TEXT("true") : TEXT("false"),
        bDiagnosticOverlayStarted ? TEXT("true") : TEXT("false"),
        bDiagnosticInputRestored ? TEXT("true") : TEXT("false"),
        DiagnosticBlockReason.IsEmpty() ? TEXT("") : TEXT(", BlockReason="),
        DiagnosticBlockReason.IsEmpty() ? TEXT("") : *DiagnosticBlockReason);
}

#if WITH_EDITOR
EDataValidationResult AVHVTimeSkipSequence::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    if (RequiredQuestID.IsNone()
        || ((!bAdvanceOnObjectiveCompletion || bTransitionOnNextObjectiveActivation)
            && RequiredObjectiveID.IsNone())
        || Stages.IsEmpty())
    {
        Context.AddError(FText::FromString(TEXT("Time-skip sequence requires quest/objective gates and at least one stage.")));
        Result = EDataValidationResult::Invalid;
    }
    TSet<FName> StageIDs;
    for (const AActor* SequenceActor : SequenceActors)
    {
        if (!IsValid(SequenceActor))
        {
            Context.AddError(FText::FromString(TEXT("Time-skip sequence has a missing shared sequence actor reference.")));
            Result = EDataValidationResult::Invalid;
        }
    }
    for (const FVHVTimeSkipStage& Stage : Stages)
    {
        if (Stage.StageID.IsNone() || StageIDs.Contains(Stage.StageID)
            || (!Stage.bPresentationOnly && !Stage.ObservationScene))
        {
            Context.AddError(FText::FromString(TEXT("Every time-skip stage needs a unique ID and an observation scene.")));
            Result = EDataValidationResult::Invalid;
        }
        StageIDs.Add(Stage.StageID);
        if (bAdvanceOnObjectiveCompletion && Stage.ObjectiveID.IsNone())
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Time-skip stage '%s' needs an ObjectiveID in objective-completion mode."),
                *Stage.StageID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        if (!Stage.bPresentationOnly && Stage.StageActors.IsEmpty())
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Time-skip stage '%s' has no stage actors."), *Stage.StageID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        for (const AActor* StageActor : Stage.StageActors)
        {
            if (!IsValid(StageActor))
            {
                Context.AddError(FText::FromString(FString::Printf(
                    TEXT("Time-skip stage '%s' has a missing stage actor reference."),
                    *Stage.StageID.ToString())));
                Result = EDataValidationResult::Invalid;
            }
        }
        if (Stage.PlayerArrivalRelativeActor && !IsValid(Stage.PlayerArrivalRelativeActor))
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Time-skip stage '%s' has an invalid relative arrival actor."),
                *Stage.StageID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
    }
    return Result;
}
#endif
