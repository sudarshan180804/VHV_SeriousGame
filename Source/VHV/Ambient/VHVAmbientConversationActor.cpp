#include "Ambient/VHVAmbientConversationActor.h"

#include "Ambient/VHVAmbientConversationData.h"
#include "Ambient/VHVAmbientSpeechLog.h"
#include "Core/VHVAuthoringReferences.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "NPC/Components/VHVAmbientSpeechComponent.h"
#include "NPC/Components/VHVNPCBehaviorComponent.h"
#include "NPC/Components/VHVNPCPresentationComponent.h"
#include "NPC/VHVNPCAIController.h"
#include "NPC/VHVNPCCharacter.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "TimerManager.h"
#include "UI/Ambient/VHVAmbientSpeechStyle.h"
#include "World/Components/VHVWorldActionReceiverComponent.h"
#include "World/Location/VHVQuestLocationVolume.h"
#include "World/Systems/VHVWorldActionSubsystem.h"

#if WITH_EDITOR
#include "EngineUtils.h"
#endif

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
    const FName StartConversationAction(TEXT("StartConversation"));
    const FName StopConversationAction(TEXT("StopConversation"));
    const FName ResetConversationAction(TEXT("ResetConversation"));
}

AVHVAmbientConversationActor::AVHVAmbientConversationActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    WorldActionReceiver = CreateDefaultSubobject<UVHVWorldActionReceiverComponent>(TEXT("WorldActionReceiver"));
    // Local/auto-start ambient scenes do not participate in the global WorldAction registry by default.
    // Quest-driven scenes opt in on the component and author a stable receiver tag.
    WorldActionReceiver->bWorldActionsEnabled = false;
}

void AVHVAmbientConversationActor::BeginPlay()
{
    Super::BeginPlay();
    WorldActionReceiver->OnWorldActionRequested.AddUniqueDynamic(this,
        &AVHVAmbientConversationActor::HandleWorldActionRequested);
    if (bAutoStartOnBeginPlay && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]
        {
            StartConversation();
        }));
    }
}

void AVHVAmbientConversationActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SequenceTimerHandle);
    if (bPlaying) CompleteConversation(false);
    WorldActionReceiver->OnWorldActionRequested.RemoveDynamic(this,
        &AVHVAmbientConversationActor::HandleWorldActionRequested);
    Super::EndPlay(EndPlayReason);
}

void AVHVAmbientConversationActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bPlaying) return;

    for (const TPair<FName, TObjectPtr<AVHVNPCCharacter>>& Pair : ResolvedParticipants)
    {
        if (!IsValid(Pair.Value))
        {
            UE_LOG(LogVHVAmbientSpeech, Warning,
                TEXT("Conversation '%s' cancelled: participant slot '%s' became invalid."),
                *GetNameSafe(this), *Pair.Key.ToString());
            CompleteConversation(false);
            return;
        }
    }

    const bool bOutside = IsPlayerOutsideObservationRadius();
    if (bOutside && PlayerLeavePolicy == EVHVAmbientConversationLeavePolicy::Cancel)
    {
        UE_LOG(LogVHVAmbientSpeech, Verbose, TEXT("Conversation '%s' cancelled because the player left its observation radius."), *GetName());
        CompleteConversation(false);
        return;
    }
    if (PlayerLeavePolicy == EVHVAmbientConversationLeavePolicy::Pause && GetWorld())
    {
        if (bOutside && !bPausedForDistance)
        {
            GetWorld()->GetTimerManager().PauseTimer(SequenceTimerHandle);
            bPausedForDistance = true;
        }
        else if (!bOutside && bPausedForDistance)
        {
            GetWorld()->GetTimerManager().UnPauseTimer(SequenceTimerHandle);
            bPausedForDistance = false;
        }
    }
    UpdateBubbleSeparation();
}

bool AVHVAmbientConversationActor::StartConversation()
{
    return StartConversationInternal(FGuid());
}

void AVHVAmbientConversationActor::StopConversation()
{
    if (bPlaying) CompleteConversation(false);
}

void AVHVAmbientConversationActor::ResetConversation()
{
    if (bPlaying) CompleteConversation(false);
    bHasPlayed = false;
    CurrentLineIndex = INDEX_NONE;
}

AVHVNPCCharacter* AVHVAmbientConversationActor::GetCurrentSpeaker() const
{
    return ConversationData && ConversationData->Lines.IsValidIndex(CurrentLineIndex)
        ? FindParticipant(ConversationData->Lines[CurrentLineIndex].SpeakerSlot)
        : nullptr;
}

bool AVHVAmbientConversationActor::AreParticipantsValid() const
{
    if (!ConversationData || ConversationData->Participants.IsEmpty()) return false;
    for (const FVHVAmbientParticipantDefinition& Required : ConversationData->Participants)
    {
        const FVHVAmbientParticipantBinding* Binding = Participants.FindByPredicate([&Required](const FVHVAmbientParticipantBinding& Item)
        {
            return Item.SlotID == Required.SlotID;
        });
        if (!Binding || !IsValid(Binding->NPC)) return false;
    }
    return true;
}

void AVHVAmbientConversationActor::HandleWorldActionRequested(const FGuid RequestID, const FName ActionID)
{
    if (ActionID == StartConversationAction)
    {
        WorldActionReceiver->SetWorldActionExecutionResult(RequestID,
            StartConversationInternal(RequestID)
                ? EVHVWorldActionExecutionResult::StartedAsync
                : EVHVWorldActionExecutionResult::Rejected);
        return;
    }
    if (ActionID == StopConversationAction)
    {
        StopConversation();
        WorldActionReceiver->SetWorldActionExecutionResult(RequestID, EVHVWorldActionExecutionResult::Completed);
        return;
    }
    if (ActionID == ResetConversationAction)
    {
        ResetConversation();
        WorldActionReceiver->SetWorldActionExecutionResult(RequestID, EVHVWorldActionExecutionResult::Completed);
        return;
    }
    WorldActionReceiver->SetWorldActionExecutionResult(RequestID, EVHVWorldActionExecutionResult::Rejected);
}

void AVHVAmbientConversationActor::HandleWorldActionCompleted(const FGuid RequestID, const FName ReceiverID,
    const FName ActionID, const bool bSuccess)
{
    (void)ReceiverID;
    (void)ActionID;
    if (bPlaying && ActiveWorldActionRequestID == RequestID && !bSuccess)
    {
        ActiveWorldActionRequestID.Invalidate();
        CompleteConversation(false);
    }
}

bool AVHVAmbientConversationActor::StartConversationInternal(const FGuid WorldActionRequestID)
{
    if (bPlaying || (bPlayOnce && bHasPlayed))
    {
        UE_LOG(LogVHVAmbientSpeech, Warning,
            TEXT("Conversation '%s' refused start: state=%s, PlayOnce=%s, HasPlayed=%s, asset='%s'."),
            *GetName(), bPlaying ? TEXT("playing") : TEXT("stopped"),
            bPlayOnce ? TEXT("true") : TEXT("false"), bHasPlayed ? TEXT("true") : TEXT("false"),
            *GetNameSafe(ConversationData));
        return false;
    }
    FString Error;
    if (!ResolveAndValidateParticipants(Error))
    {
        UE_LOG(LogVHVAmbientSpeech, Warning,
            TEXT("Conversation '%s' could not start: %s Asset='%s', bindings=%d, resolved=%d."),
            *GetName(), *Error, *GetNameSafe(ConversationData), Participants.Num(), ResolvedParticipants.Num());
        return false;
    }

    ActiveWorldActionRequestID = WorldActionRequestID;
    if (ActiveWorldActionRequestID.IsValid())
    {
        if (UVHVWorldActionSubsystem* Subsystem = GetWorld()->GetSubsystem<UVHVWorldActionSubsystem>())
        {
            Subsystem->OnWorldActionCompleted.AddUniqueDynamic(this,
                &AVHVAmbientConversationActor::HandleWorldActionCompleted);
        }
    }
    CurrentLineIndex = INDEX_NONE;
    bPlaying = true;
    bHasPlayed = true;
    bPausedForDistance = false;
    PrepareParticipants();
    SetActorTickEnabled(true);
    OnConversationStarted.Broadcast();
    UE_LOG(LogVHVAmbientSpeech, Log,
        TEXT("Started ambient conversation '%s' on '%s' (asset='%s', bindings=%d, resolved=%d)."),
        *ConversationData->ConversationID.ToString(), *GetName(), *GetNameSafe(ConversationData),
        Participants.Num(), ResolvedParticipants.Num());
    AdvanceSequence();
    return true;
}

bool AVHVAmbientConversationActor::ResolveAndValidateParticipants(FString& OutError)
{
    ResolvedParticipants.Empty();
    if (!ConversationData || ConversationData->Lines.IsEmpty() || ConversationData->Participants.IsEmpty())
    {
        OutError = TEXT("conversation data is missing or empty.");
        return false;
    }
    for (const FVHVAmbientParticipantBinding& Binding : Participants)
    {
        if (!Binding.SlotID.IsNone() && IsValid(Binding.NPC))
        {
            ResolvedParticipants.Add(Binding.SlotID, Binding.NPC);
        }
    }
    TSet<AVHVNPCCharacter*> UniqueNPCs;
    for (const FVHVAmbientParticipantDefinition& Required : ConversationData->Participants)
    {
        AVHVNPCCharacter* NPC = FindParticipant(Required.SlotID);
        if (!IsValid(NPC))
        {
            OutError = FString::Printf(TEXT("slot '%s' has no valid NPC binding."), *Required.SlotID.ToString());
            return false;
        }
        if (UniqueNPCs.Contains(NPC))
        {
            OutError = FString::Printf(TEXT("NPC '%s' is assigned to more than one slot."), *GetNameSafe(NPC));
            return false;
        }
        UniqueNPCs.Add(NPC);
        if (!NPC->FindComponentByClass<UVHVAmbientSpeechComponent>())
        {
            OutError = FString::Printf(TEXT("NPC '%s' has no Ambient Speech component."), *GetNameSafe(NPC));
            return false;
        }
        if (const UVHVNPCBehaviorComponent* Behavior = NPC->GetBehaviorComponent())
        {
            const EVHVNPCBehaviorState State = Behavior->GetBehaviorState();
            if (State == EVHVNPCBehaviorState::Engaging || State == EVHVNPCBehaviorState::Talking
                || State == EVHVNPCBehaviorState::Unavailable)
            {
                OutError = FString::Printf(TEXT("NPC '%s' is busy or unavailable."), *GetNameSafe(NPC));
                return false;
            }
        }
    }
    return true;
}

void AVHVAmbientConversationActor::PrepareParticipants()
{
    PreviousBehaviorStates.Empty();
    if (bPreserveParticipantBehavior)
    {
        return;
    }
    for (const TPair<FName, TObjectPtr<AVHVNPCCharacter>>& Pair : ResolvedParticipants)
    {
        if (UVHVNPCBehaviorComponent* Behavior = Pair.Value->GetBehaviorComponent())
        {
            PreviousBehaviorStates.Add(Pair.Value, Behavior->GetBehaviorState());
            Behavior->SetBehaviorState(EVHVNPCBehaviorState::Talking);
        }
    }
}

void AVHVAmbientConversationActor::RestoreParticipants()
{
    ClearFacing();
    for (const TPair<TWeakObjectPtr<AVHVNPCCharacter>, FName>& Pair : StartedPresentationActions)
    {
        if (AVHVNPCCharacter* NPC = Pair.Key.Get())
        {
            if (UVHVNPCPresentationComponent* Presentation = NPC->GetPresentationComponent();
                Presentation && Presentation->GetCurrentActionID() == Pair.Value)
            {
                Presentation->StopCurrentActionSilently();
            }
        }
    }
    StartedPresentationActions.Empty();
    for (const TPair<TWeakObjectPtr<AVHVNPCCharacter>, EVHVNPCBehaviorState>& Pair : PreviousBehaviorStates)
    {
        if (AVHVNPCCharacter* NPC = Pair.Key.Get())
        {
            if (UVHVNPCBehaviorComponent* Behavior = NPC->GetBehaviorComponent())
            {
                Behavior->SetBehaviorState(Pair.Value == EVHVNPCBehaviorState::Unavailable
                    ? EVHVNPCBehaviorState::Unavailable : EVHVNPCBehaviorState::Idle);
            }
        }
    }
    PreviousBehaviorStates.Empty();
}

void AVHVAmbientConversationActor::AdvanceSequence()
{
    if (!bPlaying || !GetWorld()) return;
    ++CurrentLineIndex;
    if (!ConversationData->Lines.IsValidIndex(CurrentLineIndex))
    {
        CompleteConversation(true);
        return;
    }
    const float Delay = FMath::Max(0.0f, ConversationData->Lines[CurrentLineIndex].DelayBefore);
    if (Delay <= KINDA_SMALL_NUMBER)
    {
        ShowCurrentLine();
    }
    else
    {
        GetWorld()->GetTimerManager().SetTimer(SequenceTimerHandle, this,
            &AVHVAmbientConversationActor::ShowCurrentLine, Delay, false);
    }
}

void AVHVAmbientConversationActor::ShowCurrentLine()
{
    if (!bPlaying || !ConversationData || !ConversationData->Lines.IsValidIndex(CurrentLineIndex) || !GetWorld())
    {
        UE_LOG(LogVHVAmbientSpeech, Warning,
            TEXT("Conversation '%s' refused line presentation: playing=%s, asset='%s', line=%d."),
            *GetName(), bPlaying ? TEXT("true") : TEXT("false"), *GetNameSafe(ConversationData), CurrentLineIndex);
        return;
    }
    const FVHVAmbientSpeechLine& Line = ConversationData->Lines[CurrentLineIndex];
    AVHVNPCCharacter* Speaker = FindParticipant(Line.SpeakerSlot);
    UVHVAmbientSpeechComponent* Speech = Speaker
        ? Speaker->FindComponentByClass<UVHVAmbientSpeechComponent>() : nullptr;
    if (!Speech)
    {
        UE_LOG(LogVHVAmbientSpeech, Warning,
            TEXT("Conversation '%s' cancelled at line %d: speaker slot '%s' resolved=%s, AmbientSpeechComponent=false."),
            *GetName(), CurrentLineIndex, *Line.SpeakerSlot.ToString(), Speaker ? TEXT("true") : TEXT("false"));
        CompleteConversation(false);
        return;
    }
    if (!Line.bKeepPreviousBubbleVisible) HideAllBubbles(false);
    if (!Speech->ShowBubble(FindSpeakerName(Line.SpeakerSlot), Line.Text, Line.SpeechType,
        ConversationData->bShowSpeakerName))
    {
        UE_LOG(LogVHVAmbientSpeech, Warning,
            TEXT("Conversation '%s' cancelled at line %d: speaker slot '%s' resolved, but its speech widget could not initialize."),
            *GetName(), CurrentLineIndex, *Line.SpeakerSlot.ToString());
        CompleteConversation(false);
        return;
    }
    ApplyLineFacing(Speaker, Line.bLookAtOtherParticipant);

    if (Line.NPCActionTag.IsValid())
    {
        const FName ActionID = VHVAuthoringReferences::ResolveTagLeaf(Line.NPCActionTag);
        if (UVHVNPCPresentationComponent* Presentation = Speaker->GetPresentationComponent();
            Presentation && !Presentation->IsActionPlaying() && Presentation->PlayAction(ActionID))
        {
            StartedPresentationActions.Add(Speaker, ActionID);
        }
    }
    OnLineStarted.Broadcast(CurrentLineIndex, Speaker, Line.PresentationTag);
    UE_LOG(LogVHVAmbientSpeech, Log,
        TEXT("Conversation '%s': presenting line %d, speaker slot '%s', resolved=true, type=%s."),
        *ConversationData->ConversationID.ToString(), CurrentLineIndex, *Line.SpeakerSlot.ToString(),
        Line.SpeechType == EVHVAmbientSpeechType::Thought ? TEXT("Thought") : TEXT("Speech"));
    GetWorld()->GetTimerManager().SetTimer(SequenceTimerHandle, this,
        &AVHVAmbientConversationActor::AdvanceSequence, GetLineDuration(Line), false);
}

void AVHVAmbientConversationActor::CompleteConversation(const bool bSuccess)
{
    if (!bPlaying) return;
    bPlaying = false;
    bPausedForDistance = false;
    SetActorTickEnabled(false);
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SequenceTimerHandle);
    HideAllBubbles(false);
    RestoreParticipants();

    const FGuid CompletedRequestID = ActiveWorldActionRequestID;
    ActiveWorldActionRequestID.Invalidate();
    if (GetWorld())
    {
        if (UVHVWorldActionSubsystem* Subsystem = GetWorld()->GetSubsystem<UVHVWorldActionSubsystem>())
        {
            Subsystem->OnWorldActionCompleted.RemoveDynamic(this,
                &AVHVAmbientConversationActor::HandleWorldActionCompleted);
        }
    }
    if (bSuccess && ConversationData && !ConversationData->CompletionEffects.IsEmpty()
        && GetWorld() && GetWorld()->GetGameInstance())
    {
        if (UVHVStoryStateSubsystem* Story = GetWorld()->GetGameInstance()->GetSubsystem<UVHVStoryStateSubsystem>())
        {
            Story->ApplyEffects(ConversationData->CompletionEffects);
        }
    }
    if (CompletedRequestID.IsValid()) WorldActionReceiver->FinishWorldAction(CompletedRequestID, bSuccess);
    OnConversationFinished.Broadcast(bSuccess);
    UE_LOG(LogVHVAmbientSpeech, Log, TEXT("Ambient conversation '%s' finished (%s)."),
        *GetName(), bSuccess ? TEXT("success") : TEXT("cancelled"));
}

void AVHVAmbientConversationActor::HideAllBubbles(const bool bImmediate)
{
    for (const TPair<FName, TObjectPtr<AVHVNPCCharacter>>& Pair : ResolvedParticipants)
    {
        if (IsValid(Pair.Value))
        {
            if (UVHVAmbientSpeechComponent* Speech = Pair.Value->FindComponentByClass<UVHVAmbientSpeechComponent>())
            {
                Speech->HideBubble(bImmediate);
                Speech->SetSeparationOffset(0.0f);
            }
        }
    }
}

void AVHVAmbientConversationActor::ApplyLineFacing(AVHVNPCCharacter* Speaker, const bool bEnabled)
{
    ClearFacing();
    if (!bEnabled || !Speaker) return;
    if (ResolvedParticipants.Num() < 2)
    {
        UE_LOG(LogVHVAmbientSpeech, Warning,
            TEXT("Conversation '%s' line %d requested participant facing with only one resolved participant; facing skipped."),
            *GetName(), CurrentLineIndex);
        return;
    }
    AVHVNPCCharacter* Other = nullptr;
    for (const TPair<FName, TObjectPtr<AVHVNPCCharacter>>& Pair : ResolvedParticipants)
    {
        if (Pair.Value != Speaker)
        {
            Other = Pair.Value;
            break;
        }
    }
    if (!Other) return;
    if (AVHVNPCAIController* SpeakerAI = Cast<AVHVNPCAIController>(Speaker->GetController()))
        SpeakerAI->BeginFaceActor(Other);
    if (AVHVNPCAIController* OtherAI = Cast<AVHVNPCAIController>(Other->GetController()))
        OtherAI->BeginFaceActor(Speaker);
}

void AVHVAmbientConversationActor::ClearFacing()
{
    for (const TPair<FName, TObjectPtr<AVHVNPCCharacter>>& Pair : ResolvedParticipants)
    {
        if (IsValid(Pair.Value))
        {
            if (AVHVNPCAIController* AI = Cast<AVHVNPCAIController>(Pair.Value->GetController()))
                AI->CancelFacing();
        }
    }
}

void AVHVAmbientConversationActor::UpdateBubbleSeparation()
{
    TArray<UVHVAmbientSpeechComponent*> Visible;
    for (const TPair<FName, TObjectPtr<AVHVNPCCharacter>>& Pair : ResolvedParticipants)
    {
        if (UVHVAmbientSpeechComponent* Speech = IsValid(Pair.Value)
            ? Pair.Value->FindComponentByClass<UVHVAmbientSpeechComponent>() : nullptr)
        {
            Speech->SetSeparationOffset(0.0f);
            if (Speech->IsBubbleActive()) Visible.Add(Speech);
        }
    }
    if (Visible.Num() != 2) return;
    FVector2D A;
    FVector2D B;
    if (Visible[0]->GetAnchorScreenPosition(A) && Visible[1]->GetAnchorScreenPosition(B)
        && FMath::Abs(A.X - B.X) < 300.0f && FMath::Abs(A.Y - B.Y) < 135.0f)
    {
        (A.Y >= B.Y ? Visible[0] : Visible[1])->SetSeparationOffset(-56.0f);
    }
}

bool AVHVAmbientConversationActor::IsPlayerOutsideObservationRadius() const
{
    if (ObservationRadius <= 0.0f || PlayerLeavePolicy == EVHVAmbientConversationLeavePolicy::Continue) return false;
    const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn) return false;
    float ClosestSquared = TNumericLimits<float>::Max();
    for (const TPair<FName, TObjectPtr<AVHVNPCCharacter>>& Pair : ResolvedParticipants)
    {
        if (IsValid(Pair.Value))
            ClosestSquared = FMath::Min(ClosestSquared, FVector::DistSquared(Pawn->GetActorLocation(), Pair.Value->GetActorLocation()));
    }
    return ClosestSquared > FMath::Square(ObservationRadius);
}

AVHVNPCCharacter* AVHVAmbientConversationActor::FindParticipant(const FName SlotID) const
{
    const TObjectPtr<AVHVNPCCharacter>* Found = ResolvedParticipants.Find(SlotID);
    return Found ? Found->Get() : nullptr;
}

FText AVHVAmbientConversationActor::FindSpeakerName(const FName SlotID) const
{
    const FVHVAmbientParticipantDefinition* Participant = ConversationData
        ? ConversationData->FindParticipant(SlotID) : nullptr;
    return Participant ? Participant->SpeakerName : FText::GetEmpty();
}

float AVHVAmbientConversationActor::GetLineDuration(const FVHVAmbientSpeechLine& Line) const
{
    return Line.DisplayDuration > 0.0f
        ? Line.DisplayDuration
        : FMath::Clamp(Line.Text.ToString().Len() * VHVAmbientSpeechStyle::AutoDurationPerCharacter,
            VHVAmbientSpeechStyle::AutoDurationMin, VHVAmbientSpeechStyle::AutoDurationMax);
}

#if WITH_EDITOR
EDataValidationResult AVHVAmbientConversationActor::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    if (!ConversationData)
    {
        Context.AddError(FText::FromString(TEXT("Ambient Conversation actor has no Conversation Data.")));
        return EDataValidationResult::Invalid;
    }
    TSet<FName> BoundSlots;
    TSet<const AVHVNPCCharacter*> BoundNPCs;
    for (const FVHVAmbientParticipantBinding& Binding : Participants)
    {
        if (Binding.SlotID.IsNone() || !IsValid(Binding.NPC))
        {
            Context.AddError(FText::FromString(TEXT("Every ambient participant binding requires a Slot ID and NPC.")));
            Result = EDataValidationResult::Invalid;
        }
        if (BoundSlots.Contains(Binding.SlotID) || (IsValid(Binding.NPC) && BoundNPCs.Contains(Binding.NPC)))
        {
            Context.AddError(FText::FromString(TEXT("Ambient participant Slot IDs and NPC bindings must each be unique.")));
            Result = EDataValidationResult::Invalid;
        }
        BoundSlots.Add(Binding.SlotID);
        if (IsValid(Binding.NPC)) BoundNPCs.Add(Binding.NPC);
    }
    for (const FVHVAmbientParticipantDefinition& Required : ConversationData->Participants)
    {
        if (!BoundSlots.Contains(Required.SlotID))
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("Participant slot '%s' is not bound."), *Required.SlotID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
    }
    if (bRequiresExplicitTrigger)
    {
        if (bAutoStartOnBeginPlay)
        {
            Context.AddError(FText::FromString(TEXT("An Explicit Trigger ambient conversation cannot auto-start on BeginPlay.")));
            Result = EDataValidationResult::Invalid;
        }

        const FName ReceiverID = WorldActionReceiver
            ? WorldActionReceiver->GetEffectiveReceiverID() : NAME_None;
        bool bFoundTrigger = false;
        if (UWorld* World = GetWorld())
        {
            for (TActorIterator<AVHVQuestLocationVolume> It(World); It; ++It)
            {
                if (It->bTriggerWorldAction
                    && VHVAuthoringReferences::ResolveID(
                        It->WorldActionReceiverTag, NAME_None, TEXT("VHV.WorldReceiver")) == ReceiverID
                    && VHVAuthoringReferences::ResolveID(
                        It->WorldActionTag, NAME_None, TEXT("VHV.WorldAction")) == StartConversationAction)
                {
                    bFoundTrigger = true;
                    break;
                }
            }
        }
        if (!bFoundTrigger)
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Explicit ambient conversation '%s' has no location trigger routed to receiver '%s' with StartConversation."),
                *GetNameSafe(this), *ReceiverID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
    }
    return Result;
}
#endif
