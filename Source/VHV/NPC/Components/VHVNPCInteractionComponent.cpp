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
	if (!bInteractionEnabled)
	{
		return false;
	}

	const UVHVNPCBehaviorComponent* BehaviorComponent = GetOwner()
		? GetOwner()->FindComponentByClass<UVHVNPCBehaviorComponent>()
		: nullptr;
	return !BehaviorComponent
		|| (BehaviorComponent->IsAvailableForInteraction()
			&& BehaviorComponent->GetBehaviorState() != EVHVNPCBehaviorState::Talking);
}

FText UVHVNPCInteractionComponent::GetInteractionPrompt() const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	const UVHVQuestParticipantComponent* QuestParticipant = Owner
		? Owner->FindComponentByClass<UVHVQuestParticipantComponent>()
		: nullptr;

	if (QuestParticipant && !QuestParticipant->ParticipantID.IsNone() && World && World->GetGameInstance())
	{
		const UVHVQuestSubsystem* QuestSubsystem = World->GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>();
		FVHVQuestObjectiveDefinition Objective;
		if (QuestSubsystem
			&& QuestSubsystem->GetCurrentObjective(Objective)
			&& Objective.TargetID == QuestParticipant->ParticipantID
			&& !Objective.ObjectiveText.IsEmpty())
		{
			return Objective.ObjectiveText;
		}
	}

	return DefaultInteractionPrompt;
}

void UVHVNPCInteractionComponent::Interact(AActor* InteractingActor)
{
	if (!CanInteract() || !GetOwner())
	{
		return;
	}

	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AVHVNPCAIController* PawnAIController = Cast<AVHVNPCAIController>(OwnerPawn->GetController()))
		{
			PawnAIController->StopForInteraction();
			PawnAIController->FaceActor(InteractingActor);
		}
	}

	if (UVHVNPCBehaviorComponent* BehaviorComponent = GetOwner()->FindComponentByClass<UVHVNPCBehaviorComponent>())
	{
		BehaviorComponent->SetBehaviorState(EVHVNPCBehaviorState::Talking);
	}

	UVHVNPCDialogueComponent* DialogueComponent = GetOwner()->FindComponentByClass<UVHVNPCDialogueComponent>();
	if (!DialogueComponent || !DialogueComponent->StartDialogue(InteractingActor))
	{
		if (DialogueComponent)
		{
			DialogueComponent->EndDialogue();
		}
		else
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
	}
}
