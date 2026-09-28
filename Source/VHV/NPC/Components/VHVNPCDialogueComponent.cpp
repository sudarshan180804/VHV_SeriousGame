#include "NPC/Components/VHVNPCDialogueComponent.h"

#include "Core/VHVConversationDataAsset.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NPC/Components/VHVNPCBehaviorComponent.h"
#include "NPC/Types/VHVNPCBehaviorTypes.h"
#include "NPC/VHVNPCAIController.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "UI/VHVUIManagerComponent.h"

UVHVNPCDialogueComponent::UVHVNPCDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVHVNPCDialogueComponent::BeginPlay()
{
	Super::BeginPlay();
	CachedDefaultConversation = DefaultConversation.IsNull() ? nullptr : DefaultConversation.LoadSynchronous();
}

bool UVHVNPCDialogueComponent::StartDialogue(AActor* InteractingActor)
{
	if (const AActor* Owner = GetOwner())
	{
		if (const UVHVQuestParticipantComponent* QuestParticipant = Owner->FindComponentByClass<UVHVQuestParticipantComponent>())
		{
			const UWorld* World = GetWorld();
			const UVHVQuestSubsystem* QuestSubsystem = World && World->GetGameInstance()
				? World->GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>()
				: nullptr;
			if (QuestParticipant->IsQuestParticipationEnabled() && QuestSubsystem
				&& QuestSubsystem->CanParticipantInteract(QuestParticipant->GetEffectiveParticipantID()))
			{
				return StartQuestInteraction(InteractingActor);
			}
		}
	}
	return StartDefaultDialogue(InteractingActor);
}

bool UVHVNPCDialogueComponent::StartQuestInteraction(
	AActor* InteractingActor,
	const FName ExpectedQuestID,
	const FName ExpectedObjectiveID)
{
	UVHVUIManagerComponent* UIManager = ResolveUIManager(InteractingActor);
	if (!UIManager || !BeginDialogueSession(UIManager))
	{
		return false;
	}

	bool bHandled = false;
	if (const AActor* Owner = GetOwner())
	{
		if (UVHVQuestParticipantComponent* QuestParticipant = Owner->FindComponentByClass<UVHVQuestParticipantComponent>())
		{
			UVHVQuestSubsystem* QuestSubsystem = GetWorld() && GetWorld()->GetGameInstance()
				? GetWorld()->GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>()
				: nullptr;
			FVHVQuestObjectiveDefinition CurrentObjective;
			const bool bExpectedInteractionStillCurrent = QuestSubsystem
				&& (ExpectedQuestID.IsNone() || QuestSubsystem->GetRuntimeState().ActiveQuestID == ExpectedQuestID)
				&& (ExpectedObjectiveID.IsNone()
					|| (QuestSubsystem->GetCurrentObjective(CurrentObjective) && CurrentObjective.ObjectiveID == ExpectedObjectiveID));
			bHandled = bExpectedInteractionStillCurrent && QuestParticipant->NotifyInteracted();
		}
	}

	if (!bHandled)
	{
		CancelDialogueSessionBinding();
		return false;
	}

	if (!UIManager->IsConversationSessionActive())
	{
		RestoreNPCState();
	}
	return true;
}

bool UVHVNPCDialogueComponent::StartDefaultDialogue(
	AActor* InteractingActor,
	UVHVConversationDataAsset* ExpectedConversation)
{
	UVHVUIManagerComponent* UIManager = ResolveUIManager(InteractingActor);
	UVHVConversationDataAsset* ConversationAsset = CachedDefaultConversation.Get();
	if (!ConversationAsset && !DefaultConversation.IsNull())
	{
		ConversationAsset = DefaultConversation.LoadSynchronous();
		CachedDefaultConversation = ConversationAsset;
	}
	if (!UIManager || !ConversationAsset || (ExpectedConversation && ConversationAsset != ExpectedConversation)
		|| !UIManager->CanStartConversationFromAsset(ConversationAsset)
		|| !BeginDialogueSession(UIManager))
	{
		return false;
	}

	if (!UIManager->StartConversationFromAsset(ConversationAsset))
	{
		CancelDialogueSessionBinding();
		return false;
	}
	return true;
}

bool UVHVNPCDialogueComponent::CanStartDefaultDialogue(AActor* InteractingActor) const
{
	return GetAvailableDefaultConversation(InteractingActor) != nullptr;
}

UVHVConversationDataAsset* UVHVNPCDialogueComponent::GetAvailableDefaultConversation(AActor* InteractingActor) const
{
	UVHVConversationDataAsset* ConversationAsset = CachedDefaultConversation.Get();
	const UVHVUIManagerComponent* UIManager = ResolveUIManager(InteractingActor);
	return ConversationAsset && UIManager && UIManager->CanStartConversationFromAsset(ConversationAsset)
		? ConversationAsset
		: nullptr;
}

bool UVHVNPCDialogueComponent::BeginDialogueSession(UVHVUIManagerComponent* UIManager)
{
	if (!UIManager)
	{
		return false;
	}
	if (ActiveUIManager && ActiveUIManager != UIManager)
	{
		ActiveUIManager->OnConversationSessionEnded.RemoveDynamic(this, &UVHVNPCDialogueComponent::HandleConversationSessionEnded);
	}
	ActiveUIManager = UIManager;
	ActiveUIManager->OnConversationSessionEnded.AddUniqueDynamic(this, &UVHVNPCDialogueComponent::HandleConversationSessionEnded);
	return true;
}

void UVHVNPCDialogueComponent::CancelDialogueSessionBinding()
{
	if (ActiveUIManager)
	{
		ActiveUIManager->OnConversationSessionEnded.RemoveDynamic(this, &UVHVNPCDialogueComponent::HandleConversationSessionEnded);
		ActiveUIManager = nullptr;
	}
}

void UVHVNPCDialogueComponent::EndDialogue()
{
	UVHVUIManagerComponent* UIManager = ActiveUIManager;
	if (UIManager && UIManager->IsConversationSessionActive())
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
		ActiveUIManager->OnConversationSessionEnded.RemoveDynamic(this, &UVHVNPCDialogueComponent::HandleConversationSessionEnded);
		ActiveUIManager = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UVHVNPCDialogueComponent::HandleConversationSessionEnded()
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
		ActiveUIManager->OnConversationSessionEnded.RemoveDynamic(this, &UVHVNPCDialogueComponent::HandleConversationSessionEnded);
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
