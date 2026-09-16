#include "VHVInteractionComponent.h"

#include "VHV.h"
#include "GameFramework/Pawn.h"
#include "NPC/Components/VHVNPCInteractionComponent.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"

UVHVInteractionComponent::UVHVInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	InteractionPrompt = FText::FromString(TEXT("Interact"));
	bCanInteract = true;
	bEnableTemporaryDialogueTest = false;
}

void UVHVInteractionComponent::Interact()
{
	AActor* InteractingActor = nullptr;
	if (GetWorld())
	{
		if (const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
		{
			InteractingActor = PlayerController->GetPawn();
		}
	}
	InteractWithActor(InteractingActor);
}

void UVHVInteractionComponent::InteractWithActor(AActor* InteractingActor)
{
	if (!CanInteract())
	{
		return;
	}

	UE_LOG(
		LogVHV,
		Log,
		TEXT("VHV Interaction fired on actor: %s"),
		*GetNameSafe(GetOwner())
	);

    OnInteracted.Broadcast();

	if (UVHVNPCInteractionComponent* NPCInteraction = GetOwner()->FindComponentByClass<UVHVNPCInteractionComponent>())
	{
		NPCInteraction->Interact(InteractingActor);
		return;
	}

    bool bHandledByQuest = false;
    if (UVHVQuestParticipantComponent* QuestParticipant = GetOwner()->FindComponentByClass<UVHVQuestParticipantComponent>())
    {
        bHandledByQuest = QuestParticipant->NotifyInteracted();
    }

    if (bEnableTemporaryDialogueTest && !bHandledByQuest)
    {
        TriggerTemporaryDialogueTest();
    }
}

FText UVHVInteractionComponent::GetInteractionPrompt() const
{
	if (const UVHVNPCInteractionComponent* NPCInteraction = GetOwner()
		? GetOwner()->FindComponentByClass<UVHVNPCInteractionComponent>()
		: nullptr)
	{
		return NPCInteraction->GetInteractionPrompt();
	}

	return InteractionPrompt;
}

bool UVHVInteractionComponent::CanInteract() const
{
	if (!bCanInteract)
	{
		return false;
	}

	const UVHVNPCInteractionComponent* NPCInteraction = GetOwner()
		? GetOwner()->FindComponentByClass<UVHVNPCInteractionComponent>()
		: nullptr;
	return !NPCInteraction || NPCInteraction->CanInteract();
}

void UVHVInteractionComponent::TriggerTemporaryDialogueTest()
{
	if (!GetWorld())
	{
		return;
	}

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	UVHVUIManagerComponent* UIManager = PlayerController->FindComponentByClass<UVHVUIManagerComponent>();
	if (!UIManager)
	{
		return;
	}

	UVHVConversationDataAsset* ConversationAsset = TemporaryDialogueConversation.Get();
	if (!ConversationAsset)
	{
		ConversationAsset = TemporaryDialogueConversation.LoadSynchronous();
	}

	if (!ConversationAsset)
	{
		return;
	}

	UIManager->StartConversationFromAsset(ConversationAsset);
}
