#include "NPC/Components/VHVNPCDialogueComponent.h"

#include "Core/VHVConversationDataAsset.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NPC/Components/VHVNPCBehaviorComponent.h"
#include "NPC/Types/VHVNPCBehaviorTypes.h"
#include "NPC/VHVNPCAIController.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "UI/VHVUIManagerComponent.h"

UVHVNPCDialogueComponent::UVHVNPCDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UVHVNPCDialogueComponent::StartDialogue(AActor* InteractingActor)
{
	UVHVUIManagerComponent* UIManager = ResolveUIManager(InteractingActor);
	if (!UIManager)
	{
		return false;
	}

	if (ActiveUIManager && ActiveUIManager != UIManager)
	{
		ActiveUIManager->OnConversationEnded.RemoveDynamic(this, &UVHVNPCDialogueComponent::HandleConversationEnded);
	}
	ActiveUIManager = UIManager;
	ActiveUIManager->OnConversationEnded.AddUniqueDynamic(this, &UVHVNPCDialogueComponent::HandleConversationEnded);

	bool bHandledByQuest = false;
	if (const AActor* Owner = GetOwner())
	{
		if (UVHVQuestParticipantComponent* QuestParticipant = Owner->FindComponentByClass<UVHVQuestParticipantComponent>())
		{
			bHandledByQuest = QuestParticipant->NotifyInteracted();
		}
	}

	if (bHandledByQuest)
	{
		return ActiveUIManager->IsConversationActive();
	}

	UVHVConversationDataAsset* ConversationAsset = DefaultConversation.Get();
	if (!ConversationAsset)
	{
		ConversationAsset = DefaultConversation.LoadSynchronous();
	}

	return ConversationAsset && ActiveUIManager->StartConversationFromAsset(ConversationAsset);
}

void UVHVNPCDialogueComponent::EndDialogue()
{
	UVHVUIManagerComponent* UIManager = ActiveUIManager;
	if (UIManager && UIManager->IsConversationActive())
	{
		UIManager->ExitConversation();
		return;
	}

	RestoreNPCState();
}

void UVHVNPCDialogueComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ActiveUIManager)
	{
		ActiveUIManager->OnConversationEnded.RemoveDynamic(this, &UVHVNPCDialogueComponent::HandleConversationEnded);
		ActiveUIManager = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UVHVNPCDialogueComponent::HandleConversationEnded()
{
	RestoreNPCState();
}

UVHVUIManagerComponent* UVHVNPCDialogueComponent::ResolveUIManager(AActor* InteractingActor) const
{
	APlayerController* PlayerController = Cast<APlayerController>(InteractingActor);
	if (!PlayerController)
	{
		if (const APawn* Pawn = Cast<APawn>(InteractingActor))
		{
			PlayerController = Cast<APlayerController>(Pawn->GetController());
		}
	}
	if (!PlayerController && InteractingActor)
	{
		PlayerController = Cast<APlayerController>(InteractingActor->GetInstigatorController());
	}
	if (!PlayerController && GetWorld())
	{
		PlayerController = GetWorld()->GetFirstPlayerController();
	}

	return PlayerController ? PlayerController->FindComponentByClass<UVHVUIManagerComponent>() : nullptr;
}

void UVHVNPCDialogueComponent::RestoreNPCState()
{
	if (ActiveUIManager)
	{
		ActiveUIManager->OnConversationEnded.RemoveDynamic(this, &UVHVNPCDialogueComponent::HandleConversationEnded);
		ActiveUIManager = nullptr;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (AVHVNPCAIController* AIController = OwnerPawn ? Cast<AVHVNPCAIController>(OwnerPawn->GetController()) : nullptr)
	{
		AIController->ClearInteractionFocus();
		if (UVHVNPCBehaviorComponent* BehaviorComponent = GetOwner()->FindComponentByClass<UVHVNPCBehaviorComponent>())
		{
			BehaviorComponent->SetBehaviorState(EVHVNPCBehaviorState::Idle);
		}
		AIController->ResumeBehavior();
		return;
	}

	if (GetOwner())
	{
		if (UVHVNPCBehaviorComponent* BehaviorComponent = GetOwner()->FindComponentByClass<UVHVNPCBehaviorComponent>())
		{
			BehaviorComponent->SetBehaviorState(EVHVNPCBehaviorState::Idle);
		}
	}
}
