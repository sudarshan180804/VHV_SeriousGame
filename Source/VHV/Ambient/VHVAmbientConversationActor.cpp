#include "Ambient/VHVAmbientConversationActor.h"

#include "Ambient/VHVAmbientConversationData.h"
#include "Ambient/VHVAmbientSpeechLog.h"
#include "Core/VHVAuthoringReferences.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NPC/Components/VHVAmbientSpeechComponent.h"
#include "NPC/Components/VHVNPCBehaviorComponent.h"
#include "NPC/Components/VHVNPCPresentationComponent.h"
#include "NPC/Animation/VHVNPCAnimationProfile.h"
#include "NPC/VHVNPCAIController.h"
#include "NPC/VHVNPCCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "TimerManager.h"
#include "UI/Ambient/VHVAmbientSpeechStyle.h"
#include "World/Components/VHVWorldActionReceiverComponent.h"
#include "World/Location/VHVQuestLocationVolume.h"
#include "World/Systems/VHVWorldActionSubsystem.h"
#include "VHVCharacter.h"

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
    ResetWorldPresentation();
    HideControlledParticipants();
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
    RestoreTemporarilySuppressedActors();
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

    if (bControlParticipantVisibility && !AreResolvedParticipantsVisible())
    {
        UE_LOG(LogVHVAmbientSpeech, Error,
            TEXT("Conversation '%s' cancelled because presentation ownership was lost and a required participant became hidden."),
            *GetNameSafe(this));
        CompleteConversation(false);
        return;
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

bool AVHVAmbientConversationActor::StartConversationRange(const int32 FirstLineIndex, const int32 LineCount)
{
    return StartConversationRangeInternal(FGuid(), FirstLineIndex, LineCount, false, false);
}

void AVHVAmbientConversationActor::StopConversation()
{
    if (bPlaying) CompleteConversation(false);
}

void AVHVAmbientConversationActor::ResetConversation()
{
    if (bPlaying) CompleteConversation(false);
    RestoreTemporarilySuppressedActors();
    bHasPlayed = false;
    CurrentLineIndex = INDEX_NONE;
    ActiveLineEndExclusive = INDEX_NONE;
    bApplyCompletionEffectsForActivePlayback = true;
    ResetWorldPresentation();
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
    const int32 LineCount = ConversationData ? ConversationData->Lines.Num() : 0;
    return StartConversationRangeInternal(WorldActionRequestID, 0, LineCount, true, true);
}

bool AVHVAmbientConversationActor::StartConversationRangeInternal(
    const FGuid WorldActionRequestID,
    const int32 FirstLineIndex,
    const int32 LineCount,
    const bool bHonorPlayOnce,
    const bool bApplyCompletionEffects)
{
    if (bPlaying || (bHonorPlayOnce && bPlayOnce && bHasPlayed))
    {
        UE_LOG(LogVHVAmbientSpeech, Warning,
            TEXT("Conversation '%s' refused start: state=%s, PlayOnce=%s, HasPlayed=%s, asset='%s'."),
            *GetName(), bPlaying ? TEXT("playing") : TEXT("stopped"),
            bPlayOnce ? TEXT("true") : TEXT("false"), bHasPlayed ? TEXT("true") : TEXT("false"),
            *GetNameSafe(ConversationData));
        return false;
    }
    if (!ConversationData || FirstLineIndex < 0 || LineCount <= 0
        || FirstLineIndex >= ConversationData->Lines.Num()
        || LineCount > ConversationData->Lines.Num() - FirstLineIndex)
    {
        UE_LOG(LogVHVAmbientSpeech, Warning,
            TEXT("Conversation '%s' refused invalid line range First=%d Count=%d (asset='%s', lines=%d)."),
            *GetName(), FirstLineIndex, LineCount, *GetNameSafe(ConversationData),
            ConversationData ? ConversationData->Lines.Num() : 0);
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
    // Claim presentation ownership before revealing the local cast. The final
    // visibility write for every required participant therefore belongs to this
    // scene, while identity-equivalent persistent actors remain suppressed.
    ApplyStartedWorldPresentation();
    ApplyTemporaryActorSuppression();
    if (!ActivateControlledParticipants(Error))
    {
        HideControlledParticipants();
        RestoreTemporarilySuppressedActors();
        ResetWorldPresentation();
        UE_LOG(LogVHVAmbientSpeech, Error,
            TEXT("Conversation '%s' could not activate its complete cast: %s Asset='%s'."),
            *GetName(), *Error, *GetNameSafe(ConversationData));
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
    CurrentLineIndex = FirstLineIndex - 1;
    ActiveLineEndExclusive = FirstLineIndex + LineCount;
    bApplyCompletionEffectsForActivePlayback = bApplyCompletionEffects;
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

void AVHVAmbientConversationActor::ResetWorldPresentation()
{
    for (AActor* Actor : ActorsHiddenUntilConversationStarts)
    {
        if (IsValid(Actor))
        {
            Actor->SetActorHiddenInGame(true);
        }
    }
    for (AActor* Actor : ActorsHiddenWhenConversationStarts)
    {
        if (IsValid(Actor))
        {
            Actor->SetActorHiddenInGame(false);
        }
    }
}

void AVHVAmbientConversationActor::ApplyStartedWorldPresentation()
{
    for (AActor* Actor : ActorsHiddenUntilConversationStarts)
    {
        if (IsValid(Actor))
        {
            Actor->SetActorHiddenInGame(false);
        }
    }
    for (AActor* Actor : ActorsHiddenWhenConversationStarts)
    {
        if (IsValid(Actor))
        {
            Actor->SetActorHiddenInGame(true);
        }
    }
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

bool AVHVAmbientConversationActor::AreResolvedParticipantsVisible() const
{
    if (ResolvedParticipants.IsEmpty())
    {
        return false;
    }
    for (const TPair<FName, TObjectPtr<AVHVNPCCharacter>>& Pair : ResolvedParticipants)
    {
        const AVHVNPCCharacter* NPC = Pair.Value.Get();
        const USkeletalMeshComponent* Mesh = NPC ? NPC->GetMesh() : nullptr;
        if (!IsValid(NPC) || NPC->IsHidden() || !IsValid(Mesh)
            || !Mesh->IsVisible() || Mesh->bHiddenInGame)
        {
            return false;
        }
    }
    return true;
}

AVHVNPCCharacter* AVHVAmbientConversationActor::GetResolvedParticipant(const FName SlotID) const
{
    return FindParticipant(SlotID);
}

bool AVHVAmbientConversationActor::ActivateControlledParticipants(FString& OutError)
{
    if (!bControlParticipantVisibility)
    {
        return true;
    }

    for (const TPair<FName, TObjectPtr<AVHVNPCCharacter>>& Pair : ResolvedParticipants)
    {
        AVHVNPCCharacter* NPC = Pair.Value.Get();
        USkeletalMeshComponent* Mesh = NPC ? NPC->GetMesh() : nullptr;
        if (!IsValid(NPC) || !IsValid(Mesh) || !Mesh->GetSkeletalMeshAsset())
        {
            OutError = FString::Printf(
                TEXT("slot '%s' has no initialized NPC skeletal appearance."), *Pair.Key.ToString());
            return false;
        }

        if (ActorsTemporarilyHiddenWhileConversationPlays.Contains(NPC))
        {
            OutError = FString::Printf(
                TEXT("slot '%s' is also configured as a suppressed persistent actor."), *Pair.Key.ToString());
            return false;
        }

        // Scene-local timeline actors are intentionally dormant while hidden.
        // Restore the BeginPlay transform in case a pre-fix/runtime transition
        // allowed gravity to move them before this checkpoint became active.
        if (const FTransform* AuthoredTransform = ControlledParticipantAuthoredTransforms.Find(NPC))
        {
            NPC->SetActorTransform(*AuthoredTransform, false, nullptr, ETeleportType::TeleportPhysics);
        }
        if (UCharacterMovementComponent* Movement = NPC->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
            Movement->DisableMovement();
        }

        // A level-placed Character can retain component-level hidden/inactive state
        // even after its Actor bHidden flag is cleared. Claim the complete render
        // lifecycle for this scene-local representation before dialogue can begin.
        NPC->SetActorHiddenInGame(false);
        if (!Mesh->IsRegistered())
        {
            Mesh->RegisterComponent();
        }
        Mesh->Activate(true);
        Mesh->SetComponentTickEnabled(true);
        Mesh->SetHiddenInGame(false, true);
        Mesh->SetVisibility(true, true);
        Mesh->UpdateBounds();
        Mesh->MarkRenderTransformDirty();
        Mesh->MarkRenderDynamicDataDirty();
        Mesh->MarkRenderStateDirty();

        if (NPC->IsHidden() || !Mesh->IsRegistered() || !Mesh->IsActive()
            || !Mesh->IsVisible() || Mesh->bHiddenInGame || !Mesh->IsRenderStateCreated())
        {
            OutError = FString::Printf(
                TEXT("slot '%s' failed render activation (ActorHidden=%s Registered=%s Active=%s Visible=%s ComponentHidden=%s RenderState=%s)."),
                *Pair.Key.ToString(), NPC->IsHidden() ? TEXT("true") : TEXT("false"),
                Mesh->IsRegistered() ? TEXT("true") : TEXT("false"),
                Mesh->IsActive() ? TEXT("true") : TEXT("false"),
                Mesh->IsVisible() ? TEXT("true") : TEXT("false"),
                Mesh->bHiddenInGame ? TEXT("true") : TEXT("false"),
                Mesh->IsRenderStateCreated() ? TEXT("true") : TEXT("false"));
            return false;
        }

        UE_LOG(LogVHVAmbientSpeech, Log,
            TEXT("Conversation '%s' activated slot '%s': Actor='%s' Location=%s Mesh='%s' Registered=true Active=true Visible=true ComponentHidden=false RenderState=true."),
            *GetNameSafe(this), *Pair.Key.ToString(), *GetNameSafe(NPC),
            *NPC->GetActorLocation().ToCompactString(), *GetPathNameSafe(Mesh->GetSkeletalMeshAsset()));
    }
    return true;
}

void AVHVAmbientConversationActor::HideControlledParticipants()
{
    if (!bControlParticipantVisibility)
    {
        return;
    }
    for (const FVHVAmbientParticipantBinding& Binding : Participants)
    {
        if (IsValid(Binding.NPC))
        {
            if (!ControlledParticipantAuthoredTransforms.Contains(Binding.NPC))
            {
                ControlledParticipantAuthoredTransforms.Add(Binding.NPC, Binding.NPC->GetActorTransform());
            }
            // Hidden ACharacter instances still simulate gravity unless movement
            // is explicitly suspended. Keeping these scene representations
            // dormant prevents them falling below the map while awaiting a later
            // checkpoint (the Timeline 2/4 Saeng failure).
            if (UCharacterMovementComponent* Movement = Binding.NPC->GetCharacterMovement())
            {
                Movement->StopMovementImmediately();
                Movement->DisableMovement();
            }
            Binding.NPC->SetActorHiddenInGame(true);
        }
    }
}

void AVHVAmbientConversationActor::ApplyTemporaryActorSuppression()
{
    RestoreTemporarilySuppressedActors();
    for (AActor* Actor : ActorsTemporarilyHiddenWhileConversationPlays)
    {
        if (!IsValid(Actor))
        {
            continue;
        }
        TemporarilySuppressedActorHiddenStates.Add(Actor, Actor->IsHidden());
        Actor->SetActorHiddenInGame(true);
    }
}

void AVHVAmbientConversationActor::RestoreTemporarilySuppressedActors()
{
    for (const TPair<TWeakObjectPtr<AActor>, bool>& Pair : TemporarilySuppressedActorHiddenStates)
    {
        if (AActor* Actor = Pair.Key.Get())
        {
            Actor->SetActorHiddenInGame(Pair.Value);
        }
    }
    TemporarilySuppressedActorHiddenStates.Empty();
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
	for (const TPair<FName, TObjectPtr<AVHVNPCCharacter>>& Pair : ResolvedParticipants)
	{
		if (Pair.Value)
		{
			const UVHVNPCAnimationProfile* Profile = Pair.Value->GetAnimationProfile();
			Pair.Value->SetSpecialAnimationActionActive(
				Profile && Profile->bUseSpecialActionOutsideDialogue);
		}
	}
	if (GetWorld())
	{
		if (const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
		{
			if (AVHVCharacter* Player = Cast<AVHVCharacter>(PlayerController->GetPawn()))
			{
				Player->SetConversationTalking(false);
			}
		}
	}
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
    if (!ConversationData->Lines.IsValidIndex(CurrentLineIndex)
        || (ActiveLineEndExclusive != INDEX_NONE && CurrentLineIndex >= ActiveLineEndExclusive))
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
	const FText SpeakerName = FindSpeakerName(Line.SpeakerSlot);
	const bool bPlayerBubbleOwner = SpeakerName.ToString().Equals(TEXT("PLAYER"), ESearchCase::IgnoreCase);
	if (const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		if (AVHVCharacter* Player = Cast<AVHVCharacter>(PlayerController->GetPawn()))
		{
			Player->SetConversationTalking(bPlayerBubbleOwner);
		}
	}
	for (const TPair<FName, TObjectPtr<AVHVNPCCharacter>>& Pair : ResolvedParticipants)
	{
		if (Pair.Value)
		{
			const UVHVNPCAnimationProfile* Profile = Pair.Value->GetAnimationProfile();
			Pair.Value->SetSpecialAnimationActionActive(
				Profile && Profile->bUseSpecialActionOutsideDialogue);
		}
	}
	if (Speaker && !bPlayerBubbleOwner)
	{
		const UVHVNPCAnimationProfile* Profile = Speaker->GetAnimationProfile();
		Speaker->SetSpecialAnimationActionActive(Profile && Profile->bUseSpecialActionDuringDialogue);
	}
    UVHVAmbientSpeechComponent* Speech = nullptr;
    if (bPlayerBubbleOwner)
    {
        if (const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
        {
            if (const APawn* PlayerPawn = PlayerController->GetPawn())
            {
                Speech = PlayerPawn->FindComponentByClass<UVHVAmbientSpeechComponent>();
            }
        }
    }
    else if (Speaker)
    {
        Speech = Speaker->FindComponentByClass<UVHVAmbientSpeechComponent>();
    }
    if (!Speech)
    {
        UE_LOG(LogVHVAmbientSpeech, Warning,
            TEXT("Conversation '%s' cancelled at line %d: speaker slot '%s' resolved=%s, AmbientSpeechComponent=false."),
            *GetName(), CurrentLineIndex, *Line.SpeakerSlot.ToString(), Speaker ? TEXT("true") : TEXT("false"));
        CompleteConversation(false);
        return;
    }
    if (!Line.bKeepPreviousBubbleVisible) HideAllBubbles(false);
    if (!Speech->ShowBubble(SpeakerName, Line.Text, Line.SpeechType,
        ConversationData->bShowSpeakerName))
    {
        UE_LOG(LogVHVAmbientSpeech, Warning,
            TEXT("Conversation '%s' cancelled at line %d: speaker slot '%s' resolved, but its speech widget could not initialize."),
            *GetName(), CurrentLineIndex, *Line.SpeakerSlot.ToString());
        CompleteConversation(false);
        return;
    }
    ApplyLineFacing(Speaker, !bPlayerBubbleOwner && Line.bLookAtOtherParticipant);

    if (!bPlayerBubbleOwner && Speaker && Line.NPCActionTag.IsValid())
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
    HideControlledParticipants();
    RestoreTemporarilySuppressedActors();

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
    if (bSuccess && bApplyCompletionEffectsForActivePlayback
        && ConversationData && !ConversationData->CompletionEffects.IsEmpty()
        && GetWorld() && GetWorld()->GetGameInstance())
    {
        if (UVHVStoryStateSubsystem* Story = GetWorld()->GetGameInstance()->GetSubsystem<UVHVStoryStateSubsystem>())
        {
            Story->ApplyEffects(ConversationData->CompletionEffects);
        }
    }
    ActiveLineEndExclusive = INDEX_NONE;
    bApplyCompletionEffectsForActivePlayback = true;
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
    if (ConversationData && ConversationData->Participants.ContainsByPredicate(
            [](const FVHVAmbientParticipantDefinition& Participant)
            {
                return Participant.SpeakerName.ToString().Equals(TEXT("PLAYER"), ESearchCase::IgnoreCase);
            }))
    {
        if (const APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
        {
            if (const APawn* PlayerPawn = PlayerController->GetPawn())
            {
                if (UVHVAmbientSpeechComponent* Speech =
                        PlayerPawn->FindComponentByClass<UVHVAmbientSpeechComponent>())
                {
                    Speech->HideBubble(bImmediate);
                    Speech->SetSeparationOffset(0.0f);
                }
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
