#include "NPC/Components/VHVNPCQuestCommandComponent.h"

#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NPC/Components/VHVNPCBehaviorComponent.h"
#include "NPC/Components/VHVNPCPatrolComponent.h"
#include "NPC/Components/VHVNPCPresentationComponent.h"
#include "NPC/Quest/VHVNPCBehaviorTarget.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "World/Location/VHVQuestLocationVolume.h"
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
	PresentationComponent = GetOwner() ? GetOwner()->FindComponentByClass<UVHVNPCPresentationComponent>() : nullptr;
	if (BehaviorComponent)
	{
		BehaviorComponent->OnBehaviorCompleted.AddUniqueDynamic(this, &UVHVNPCQuestCommandComponent::HandleBehaviorCompleted);
		BehaviorComponent->OnBehaviorStateChanged.AddUniqueDynamic(this, &UVHVNPCQuestCommandComponent::HandleBehaviorStateChanged);
	}
	if (PresentationComponent)
	{
		PresentationComponent->OnPresentationActionCompleted.AddUniqueDynamic(this, &UVHVNPCQuestCommandComponent::HandlePresentationActionCompleted);
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
	if (PresentationComponent)
	{
		PresentationComponent->OnPresentationActionCompleted.RemoveDynamic(this, &UVHVNPCQuestCommandComponent::HandlePresentationActionCompleted);
		PresentationComponent->StopCurrentActionSilently();
	}
	Super::EndPlay(EndPlayReason);
}

bool UVHVNPCQuestCommandComponent::MoveToTarget(
	const FName TargetID,
	const bool bFaceDestinationRotation,
	const float MoveSpeedOverride)
{
	if (TargetID.IsNone() || !BehaviorComponent)
	{
		return false;
	}
	BeginQuestOwnership();
	AbortActiveCommand(true);
	ActiveCommand = EVHVNPCQuestCommandType::MoveToTarget;
	ActiveTargetID = TargetID;
	bFaceActiveDestinationRotation = bFaceDestinationRotation;
	ActiveMoveSpeedOverride = FMath::Max(0.0f, MoveSpeedOverride);
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

bool UVHVNPCQuestCommandComponent::PlayAction(const FName ActionID)
{
	if (ActionID.IsNone() || !BehaviorComponent || !PresentationComponent)
	{
		return false;
	}
	BeginQuestOwnership();
	AbortActiveCommand(true);
	ActiveCommand = EVHVNPCQuestCommandType::PlayAction;
	ActiveActionID = ActionID;
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
	OnQuestCommandCompletedNative.Broadcast(this, EVHVNPCQuestCommandType::ReleaseToPatrol, true);
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
		ApplyActiveMoveSpeedOverride();
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				if (UVHVQuestSubsystem* QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>())
				{
					if (AActor* Target = QuestSubsystem->FindNPCMovementTarget(World, ActiveTargetID))
					{
						ActiveMoveTarget = Target;
						// Semantic location volumes define an area for designers, but the
						// authored wait point is their transform rather than the box edge.
						bStarted = Target->IsA<AVHVQuestLocationVolume>()
							? BehaviorComponent->StartMoveToLocation(Target->GetActorLocation(), 25.0f, false)
							: BehaviorComponent->StartMoveToActor(Target, 25.0f, false);
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
	case EVHVNPCQuestCommandType::PlayAction:
		bStarted = PresentationComponent && PresentationComponent->PlayAction(ActiveActionID);
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

void UVHVNPCQuestCommandComponent::ApplyActiveMoveSpeedOverride()
{
	if (ActiveMoveSpeedOverride <= 0.0f || bMoveSpeedOverridden)
	{
		return;
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}
	PreviousMaxWalkSpeed = Movement->MaxWalkSpeed;
	Movement->MaxWalkSpeed = ActiveMoveSpeedOverride;
	bMoveSpeedOverridden = true;
}

void UVHVNPCQuestCommandComponent::RestoreMoveSpeedOverride()
{
	if (!bMoveSpeedOverridden)
	{
		return;
	}
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		Movement->MaxWalkSpeed = PreviousMaxWalkSpeed;
	}
	PreviousMaxWalkSpeed = 0.0f;
	bMoveSpeedOverridden = false;
}

void UVHVNPCQuestCommandComponent::CompleteActiveCommand(const bool bSuccess)
{
	if (ActiveCommand == EVHVNPCQuestCommandType::None)
	{
		return;
	}
	const EVHVNPCQuestCommandType CompletedCommand = ActiveCommand;
	if (bSuccess && CompletedCommand == EVHVNPCQuestCommandType::MoveToTarget)
	{
		LastSuccessfulMoveTargetID = ActiveTargetID;
	}
	if (bSuccess
		&& CompletedCommand == EVHVNPCQuestCommandType::MoveToTarget
		&& bFaceActiveDestinationRotation
		&& ActiveMoveTarget.IsValid()
		&& GetOwner())
	{
		const FRotator DestinationRotation = ActiveMoveTarget->GetActorRotation();
		FRotator OwnerRotation = GetOwner()->GetActorRotation();
		OwnerRotation.Yaw = DestinationRotation.Yaw;
		GetOwner()->SetActorRotation(OwnerRotation);
	}
	RestoreMoveSpeedOverride();
	ActiveCommand = EVHVNPCQuestCommandType::None;
	ActiveTargetID = NAME_None;
	ActiveActionID = NAME_None;
	ActiveWaitDuration = 0.0f;
	ActiveMoveSpeedOverride = 0.0f;
	bFaceActiveDestinationRotation = false;
	ActiveMoveTarget.Reset();
	bBehaviorOperationActive = false;
	bCommandPending = false;
	OnQuestCommandCompleted.Broadcast(CompletedCommand, bSuccess);
	OnQuestCommandCompletedNative.Broadcast(this, CompletedCommand, bSuccess);
}

void UVHVNPCQuestCommandComponent::AbortActiveCommand(const bool bBroadcastFailure)
{
	if (ActiveCommand == EVHVNPCQuestCommandType::None)
	{
		RestoreMoveSpeedOverride();
		bBehaviorOperationActive = false;
		bCommandPending = false;
		return;
	}

	const EVHVNPCQuestCommandType AbortedCommand = ActiveCommand;
	const bool bShouldCancelBehavior = bBehaviorOperationActive;
	RestoreMoveSpeedOverride();
	ActiveCommand = EVHVNPCQuestCommandType::None;
	ActiveTargetID = NAME_None;
	ActiveActionID = NAME_None;
	ActiveWaitDuration = 0.0f;
	ActiveMoveSpeedOverride = 0.0f;
	bFaceActiveDestinationRotation = false;
	ActiveMoveTarget.Reset();
	bBehaviorOperationActive = false;
	bCommandPending = false;
	if (bShouldCancelBehavior && AbortedCommand == EVHVNPCQuestCommandType::PlayAction && PresentationComponent)
	{
		PresentationComponent->StopCurrentAction();
	}
	else if (bShouldCancelBehavior && BehaviorComponent)
	{
		BehaviorComponent->CancelCurrentBehavior();
	}
	if (bBroadcastFailure)
	{
		OnQuestCommandCompleted.Broadcast(AbortedCommand, false);
		OnQuestCommandCompletedNative.Broadcast(this, AbortedCommand, false);
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
		if (ActiveCommand == EVHVNPCQuestCommandType::PlayAction && PresentationComponent)
		{
			PresentationComponent->StopCurrentActionSilently();
		}
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

void UVHVNPCQuestCommandComponent::HandlePresentationActionCompleted(const FName ActionID, const bool bSuccess)
{
	if (bBehaviorOperationActive
		&& ActiveCommand == EVHVNPCQuestCommandType::PlayAction
		&& ActiveActionID == ActionID)
	{
		CompleteActiveCommand(bSuccess);
	}
}
