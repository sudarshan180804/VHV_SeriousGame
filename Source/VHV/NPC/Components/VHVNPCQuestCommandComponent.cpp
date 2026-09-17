#include "NPC/Components/VHVNPCQuestCommandComponent.h"

#include "Engine/GameInstance.h"
#include "NPC/Components/VHVNPCBehaviorComponent.h"
#include "NPC/Components/VHVNPCPatrolComponent.h"
#include "NPC/Quest/VHVNPCBehaviorTarget.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "VHV.h"

UVHVNPCQuestCommandComponent::UVHVNPCQuestCommandComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVHVNPCQuestCommandComponent::BeginPlay()
{
	Super::BeginPlay();
	BehaviorComponent = GetOwner() ? GetOwner()->FindComponentByClass<UVHVNPCBehaviorComponent>() : nullptr;
	PatrolComponent = GetOwner() ? GetOwner()->FindComponentByClass<UVHVNPCPatrolComponent>() : nullptr;
	if (BehaviorComponent)
	{
		BehaviorComponent->OnBehaviorCompleted.AddUniqueDynamic(this, &UVHVNPCQuestCommandComponent::HandleBehaviorCompleted);
		BehaviorComponent->OnBehaviorStateChanged.AddUniqueDynamic(this, &UVHVNPCQuestCommandComponent::HandleBehaviorStateChanged);
	}
	RegisterWithQuestSubsystem();
}

void UVHVNPCQuestCommandComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromQuestSubsystem();
	if (BehaviorComponent)
	{
		BehaviorComponent->OnBehaviorCompleted.RemoveDynamic(this, &UVHVNPCQuestCommandComponent::HandleBehaviorCompleted);
		BehaviorComponent->OnBehaviorStateChanged.RemoveDynamic(this, &UVHVNPCQuestCommandComponent::HandleBehaviorStateChanged);
	}
	Super::EndPlay(EndPlayReason);
}

bool UVHVNPCQuestCommandComponent::MoveToTarget(const FName TargetID)
{
	if (TargetID.IsNone() || !BehaviorComponent)
	{
		return false;
	}
	BeginQuestOwnership();
	AbortActiveCommand(true);
	ActiveCommand = EVHVNPCQuestCommandType::MoveToTarget;
	ActiveTargetID = TargetID;
	return QueueOrExecuteActiveCommand();
}

bool UVHVNPCQuestCommandComponent::Wait(const float Duration)
{
	if (Duration <= 0.0f || !BehaviorComponent)
	{
		return false;
	}
	BeginQuestOwnership();
	AbortActiveCommand(true);
	ActiveCommand = EVHVNPCQuestCommandType::Wait;
	ActiveWaitDuration = Duration;
	return QueueOrExecuteActiveCommand();
}

bool UVHVNPCQuestCommandComponent::ReturnToPost()
{
	if (!BehaviorComponent)
	{
		return false;
	}
	BeginQuestOwnership();
	AbortActiveCommand(true);
	ActiveCommand = EVHVNPCQuestCommandType::ReturnToPost;
	return QueueOrExecuteActiveCommand();
}

bool UVHVNPCQuestCommandComponent::ReleaseToPatrol()
{
	if (!bHasQuestOwnership)
	{
		return false;
	}

	AbortActiveCommand(true);
	bHasQuestOwnership = false;
	const bool bShouldResumePatrol = bPatrolWasActive && PatrolComponent;
	bPatrolWasActive = false;
	if (bShouldResumePatrol)
	{
		const EVHVNPCBehaviorState State = BehaviorComponent
			? BehaviorComponent->GetBehaviorState()
			: EVHVNPCBehaviorState::Idle;
		if (State == EVHVNPCBehaviorState::Engaging || State == EVHVNPCBehaviorState::Talking)
		{
			bResumePatrolWhenIdle = true;
		}
		else
		{
			PatrolComponent->ResumePatrol();
		}
	}
	OnQuestCommandCompleted.Broadcast(EVHVNPCQuestCommandType::ReleaseToPatrol, true);
	return true;
}

void UVHVNPCQuestCommandComponent::CancelQuestCommand()
{
	AbortActiveCommand(true);
}

bool UVHVNPCQuestCommandComponent::HasQuestCommandOwnership() const
{
	return bHasQuestOwnership;
}

void UVHVNPCQuestCommandComponent::BeginQuestOwnership()
{
	if (!bHasQuestOwnership)
	{
		bHasQuestOwnership = true;
		bPatrolWasActive = PatrolComponent && PatrolComponent->IsPatrolling();
	}
	bResumePatrolWhenIdle = false;
	if (bPatrolWasActive && PatrolComponent)
	{
		PatrolComponent->PausePatrol();
	}
}

bool UVHVNPCQuestCommandComponent::QueueOrExecuteActiveCommand()
{
	if (!bHasQuestOwnership || ActiveCommand == EVHVNPCQuestCommandType::None || !BehaviorComponent)
	{
		return false;
	}

	const EVHVNPCBehaviorState State = BehaviorComponent->GetBehaviorState();
	if (State == EVHVNPCBehaviorState::Engaging || State == EVHVNPCBehaviorState::Talking)
	{
		bBehaviorOperationActive = false;
		bCommandPending = true;
		return true;
	}

	return ExecuteActiveCommand();
}

bool UVHVNPCQuestCommandComponent::ExecuteActiveCommand()
{
	if (!bHasQuestOwnership || ActiveCommand == EVHVNPCQuestCommandType::None || !BehaviorComponent)
	{
		return false;
	}

	// A state change can synchronously interrupt an operation. Recheck immediately
	// before dispatch so a conversation always wins command ownership of movement.
	const EVHVNPCBehaviorState State = BehaviorComponent->GetBehaviorState();
	if (State == EVHVNPCBehaviorState::Engaging || State == EVHVNPCBehaviorState::Talking)
	{
		bBehaviorOperationActive = false;
		bCommandPending = true;
		return true;
	}

	bCommandPending = false;
	bBehaviorOperationActive = true;
	bool bStarted = false;
	switch (ActiveCommand)
	{
	case EVHVNPCQuestCommandType::MoveToTarget:
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				if (UVHVQuestSubsystem* QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>())
				{
					if (AVHVNPCBehaviorTarget* Target = QuestSubsystem->FindNPCBehaviorTarget(World, ActiveTargetID))
					{
						bStarted = BehaviorComponent->StartMoveToActor(Target);
					}
					else
					{
						UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Quest command for '%s' could not resolve behavior Target ID '%s'."), *GetNameSafe(GetOwner()), *ActiveTargetID.ToString());
					}
				}
			}
		}
		break;
	case EVHVNPCQuestCommandType::Wait:
		bStarted = BehaviorComponent->StartWait(ActiveWaitDuration);
		break;
	case EVHVNPCQuestCommandType::ReturnToPost:
		bStarted = BehaviorComponent->ReturnToPost();
		break;
	default:
		break;
	}

	if (!bStarted && ActiveCommand != EVHVNPCQuestCommandType::None)
	{
		CompleteActiveCommand(false);
	}
	return bStarted;
}

void UVHVNPCQuestCommandComponent::CompleteActiveCommand(const bool bSuccess)
{
	if (ActiveCommand == EVHVNPCQuestCommandType::None)
	{
		return;
	}
	const EVHVNPCQuestCommandType CompletedCommand = ActiveCommand;
	ActiveCommand = EVHVNPCQuestCommandType::None;
	ActiveTargetID = NAME_None;
	ActiveWaitDuration = 0.0f;
	bBehaviorOperationActive = false;
	bCommandPending = false;
	OnQuestCommandCompleted.Broadcast(CompletedCommand, bSuccess);
}

void UVHVNPCQuestCommandComponent::AbortActiveCommand(const bool bBroadcastFailure)
{
	if (ActiveCommand == EVHVNPCQuestCommandType::None)
	{
		bBehaviorOperationActive = false;
		bCommandPending = false;
		return;
	}

	const EVHVNPCQuestCommandType AbortedCommand = ActiveCommand;
	const bool bShouldCancelBehavior = bBehaviorOperationActive;
	ActiveCommand = EVHVNPCQuestCommandType::None;
	ActiveTargetID = NAME_None;
	ActiveWaitDuration = 0.0f;
	bBehaviorOperationActive = false;
	bCommandPending = false;
	if (bShouldCancelBehavior && BehaviorComponent)
	{
		BehaviorComponent->CancelCurrentBehavior();
	}
	if (bBroadcastFailure)
	{
		OnQuestCommandCompleted.Broadcast(AbortedCommand, false);
	}
}

void UVHVNPCQuestCommandComponent::RegisterWithQuestSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UVHVQuestSubsystem* QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>())
			{
				QuestSubsystem->RegisterNPCCommandComponent(this);
			}
		}
	}
}

void UVHVNPCQuestCommandComponent::UnregisterFromQuestSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UVHVQuestSubsystem* QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>())
			{
				QuestSubsystem->UnregisterNPCCommandComponent(this);
			}
		}
	}
}

EVHVNPCBehaviorOperation UVHVNPCQuestCommandComponent::GetExpectedBehaviorOperation() const
{
	switch (ActiveCommand)
	{
	case EVHVNPCQuestCommandType::MoveToTarget:
		return EVHVNPCBehaviorOperation::MoveTo;
	case EVHVNPCQuestCommandType::Wait:
		return EVHVNPCBehaviorOperation::Wait;
	case EVHVNPCQuestCommandType::ReturnToPost:
		return EVHVNPCBehaviorOperation::ReturnToPost;
	default:
		return EVHVNPCBehaviorOperation::None;
	}
}

void UVHVNPCQuestCommandComponent::HandleBehaviorCompleted(
	const EVHVNPCBehaviorOperation CompletedOperation,
	const bool bSuccess)
{
	if (bBehaviorOperationActive && CompletedOperation == GetExpectedBehaviorOperation())
	{
		CompleteActiveCommand(bSuccess);
	}
}

void UVHVNPCQuestCommandComponent::HandleBehaviorStateChanged(
	const EVHVNPCBehaviorState PreviousState,
	const EVHVNPCBehaviorState NewState)
{
	(void)PreviousState;
	if ((NewState == EVHVNPCBehaviorState::Engaging || NewState == EVHVNPCBehaviorState::Talking)
		&& bHasQuestOwnership
		&& ActiveCommand != EVHVNPCQuestCommandType::None)
	{
		bBehaviorOperationActive = false;
		bCommandPending = true;
		return;
	}

	if (NewState != EVHVNPCBehaviorState::Idle)
	{
		return;
	}

	if (!bHasQuestOwnership && bResumePatrolWhenIdle)
	{
		bResumePatrolWhenIdle = false;
		if (PatrolComponent)
		{
			PatrolComponent->ResumePatrol();
		}
		return;
	}

	if (bHasQuestOwnership && bCommandPending && ActiveCommand != EVHVNPCQuestCommandType::None)
	{
		bCommandPending = false;
		ExecuteActiveCommand();
	}
}
