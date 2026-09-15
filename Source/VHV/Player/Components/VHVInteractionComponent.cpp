#include "VHVInteractionComponent.h"

#include "VHV.h"
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
	if (!bCanInteract)
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
	return InteractionPrompt;
}

bool UVHVInteractionComponent::CanInteract() const
{
	return bCanInteract;
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
