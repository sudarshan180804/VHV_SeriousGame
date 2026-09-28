#include "NPC/Components/VHVNPCInteractionComponent.h"

#include "NPC/Components/VHVNPCBehaviorComponent.h"
#include "NPC/Components/VHVNPCDialogueComponent.h"
#include "NPC/Types/VHVNPCBehaviorTypes.h"
#include "NPC/VHVNPCAIController.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "GameFramework/Pawn.h"

UVHVNPCInteractionComponent::UVHVNPCInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	DefaultInteractionPrompt = NSLOCTEXT("VHVNPCInteraction", "DefaultPrompt", "Talk");
}

bool UVHVNPCInteractionComponent::CanInteract() const
{
	return ResolveAvailableInteraction(false).IsAvailable();
}

EVHVNPCAvailableInteraction UVHVNPCInteractionComponent::GetAvailableInteraction() const
{
	return ResolveAvailableInteraction(false).Type;
}

UVHVNPCInteractionComponent::FResolvedInteraction UVHVNPCInteractionComponent::ResolveAvailableInteraction(
	const bool bAllowPendingInteraction) const
{
	if (!bInteractionEnabled || !GetOwner() || (bInteractionPending && !bAllowPendingInteraction))
	{
		return FResolvedInteraction();
	}

	const UVHVNPCBehaviorComponent* BehaviorComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UVHVNPCBehaviorComponent>()
		: nullptr;
	if (BehaviorComponent)
	{
		const EVHVNPCBehaviorState BehaviorState = BehaviorComponent->GetBehaviorState();
		const bool bExpectedPendingState = bAllowPendingInteraction && bInteractionPending
			&& BehaviorState == EVHVNPCBehaviorState::Engaging;
		if (!bExpectedPendingState
			&& (!BehaviorComponent->IsAvailableForInteraction()
				|| BehaviorState == EVHVNPCBehaviorState::Engaging
				|| BehaviorState == EVHVNPCBehaviorState::Talking))
		{
			return FResolvedInteraction();
		}
	}

	const UVHVQuestParticipantComponent* QuestParticipant = GetOwner()->FindComponentByClass<UVHVQuestParticipantComponent>();
	const FName ParticipantID = QuestParticipant ? QuestParticipant->GetEffectiveParticipantID() : NAME_None;
	if (QuestParticipant && QuestParticipant->IsQuestParticipationEnabled() && !ParticipantID.IsNone())
	{
		const UWorld* World = GetWorld();
		const UVHVQuestSubsystem* QuestSubsystem = World && World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>()
			: nullptr;
		if (QuestSubsystem && QuestSubsystem->IsParticipantInActiveFreeRoamNPCTravel(ParticipantID))
		{
			return FResolvedInteraction();
		}
		if (QuestSubsystem && QuestSubsystem->CanParticipantInteract(ParticipantID))
		{
			FVHVQuestObjectiveDefinition Objective;
			if (QuestSubsystem->GetCurrentObjective(Objective))
			{
				FResolvedInteraction Result;
				Result.Type = EVHVNPCAvailableInteraction::Quest;
				Result.QuestID = QuestSubsystem->GetRuntimeState().ActiveQuestID;
				Result.ObjectiveID = Objective.ObjectiveID;
				return Result;
			}
		}
	}

	const UVHVNPCDialogueComponent* DialogueComponent = GetOwner()->FindComponentByClass<UVHVNPCDialogueComponent>();
	if (UVHVConversationDataAsset* Conversation = DialogueComponent
		? DialogueComponent->GetAvailableDefaultConversation()
		: nullptr)
	{
		FResolvedInteraction Result;
		Result.Type = EVHVNPCAvailableInteraction::DefaultDialogue;
		Result.Conversation = Conversation;
		return Result;
	}
	return FResolvedInteraction();
}

FText UVHVNPCInteractionComponent::GetInteractionPrompt() const
{
	const FResolvedInteraction AvailableInteraction = ResolveAvailableInteraction(false);
	if (!AvailableInteraction.IsAvailable())
	{
		return FText::GetEmpty();
	}

	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	const UVHVQuestParticipantComponent* QuestParticipant = Owner
		? Owner->FindComponentByClass<UVHVQuestParticipantComponent>()
		: nullptr;

	const FName ParticipantID = QuestParticipant ? QuestParticipant->GetEffectiveParticipantID() : NAME_None;
	if (AvailableInteraction.Type == EVHVNPCAvailableInteraction::Quest
		&& bUseQuestObjectiveTextAsPrompt && !ParticipantID.IsNone() && World && World->GetGameInstance())
	{
		const UVHVQuestSubsystem* QuestSubsystem = World->GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>();
		FVHVQuestObjectiveDefinition Objective;
		if (QuestSubsystem
			&& QuestSubsystem->GetCurrentObjective(Objective)
			&& Objective.GetEffectiveTargetID() == ParticipantID
			&& !Objective.ObjectiveText.IsEmpty())
		{
			return Objective.ObjectiveText;
		}
	}

	return DefaultInteractionPrompt;
}

void UVHVNPCInteractionComponent::Interact(AActor* InteractingActor)
{
	const FResolvedInteraction AvailableInteraction = ResolveAvailableInteraction(false);
	if (!AvailableInteraction.IsAvailable() || !GetOwner())
	{
		return;
	}

	bInteractionPending = true;
	PendingInteraction = AvailableInteraction;
	PendingInteractingActor = InteractingActor;

	if (UVHVNPCBehaviorComponent* BehaviorComponent = GetOwner()->FindComponentByClass<UVHVNPCBehaviorComponent>())
	{
		BehaviorComponent->SetBehaviorState(EVHVNPCBehaviorState::Engaging);
	}

	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AVHVNPCAIController* PawnAIController = Cast<AVHVNPCAIController>(OwnerPawn->GetController()))
		{
			PawnAIController->StopForInteraction();
			if (bFacePlayerBeforeDialogue && IsValid(InteractingActor))
			{
				FacingAIController = PawnAIController;
				FacingAIController->OnFacingCompleted.RemoveAll(this);
				FacingAIController->OnFacingCompleted.AddUObject(this, &UVHVNPCInteractionComponent::HandleFacingCompleted);
				if (FacingAIController->BeginFaceActor(InteractingActor, FacingToleranceDegrees, FacingTimeoutSeconds))
				{
					return;
				}
				FacingAIController->OnFacingCompleted.RemoveAll(this);
				FacingAIController = nullptr;
			}
			PawnAIController->FaceActor(InteractingActor);
		}
	}

	StartDialogueAfterFacing();
}

void UVHVNPCInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FacingAIController)
	{
		FacingAIController->OnFacingCompleted.RemoveAll(this);
		FacingAIController->CancelFacing();
	}
	FacingAIController = nullptr;
	PendingInteractingActor = nullptr;
	bInteractionPending = false;
	PendingInteraction = FResolvedInteraction();
	Super::EndPlay(EndPlayReason);
}

void UVHVNPCInteractionComponent::HandleFacingCompleted(const bool bSuccess)
{
	(void)bSuccess;
	if (FacingAIController)
	{
		FacingAIController->OnFacingCompleted.RemoveAll(this);
	}
	FacingAIController = nullptr;
	StartDialogueAfterFacing();
}

void UVHVNPCInteractionComponent::StartDialogueAfterFacing()
{
	if (!bInteractionPending || !GetOwner())
	{
		return;
	}

	const FResolvedInteraction ResolvedInteraction = ResolveAvailableInteraction(true);
	const FResolvedInteraction InteractionToExecute = PendingInteraction;
	AActor* InteractingActor = PendingInteractingActor.Get();
	bInteractionPending = false;
	PendingInteraction = FResolvedInteraction();
	PendingInteractingActor = nullptr;
	if (!ResolvedInteraction.IsAvailable() || !(ResolvedInteraction == InteractionToExecute))
	{
		RestoreNPCStateWithoutDialogue();
		return;
	}

	if (UVHVNPCBehaviorComponent* BehaviorComponent = GetOwner()->FindComponentByClass<UVHVNPCBehaviorComponent>())
	{
		BehaviorComponent->SetBehaviorState(EVHVNPCBehaviorState::Talking);
	}

	UVHVNPCDialogueComponent* DialogueComponent = GetOwner()->FindComponentByClass<UVHVNPCDialogueComponent>();
	const bool bStarted = DialogueComponent
		&& (InteractionToExecute.Type == EVHVNPCAvailableInteraction::Quest
			? DialogueComponent->StartQuestInteraction(
				InteractingActor, InteractionToExecute.QuestID, InteractionToExecute.ObjectiveID)
			: DialogueComponent->StartDefaultDialogue(InteractingActor, InteractionToExecute.Conversation));
	if (!bStarted)
	{
		if (DialogueComponent)
		{
			DialogueComponent->EndDialogue();
		}
		else
		{
			RestoreNPCStateWithoutDialogue();
		}
	}
}

void UVHVNPCInteractionComponent::RestoreNPCStateWithoutDialogue()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AVHVNPCAIController* AIController = OwnerPawn
		? Cast<AVHVNPCAIController>(OwnerPawn->GetController())
		: nullptr;
	if (AIController)
	{
		AIController->ClearInteractionFocus();
	}
	if (UVHVNPCBehaviorComponent* BehaviorComponent = GetOwner()->FindComponentByClass<UVHVNPCBehaviorComponent>())
	{
		BehaviorComponent->SetBehaviorState(EVHVNPCBehaviorState::Idle);
	}
	if (AIController)
	{
		AIController->ResumeBehavior();
	}
}
