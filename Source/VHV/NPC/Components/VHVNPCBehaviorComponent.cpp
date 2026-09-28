#include "NPC/Components/VHVNPCBehaviorComponent.h"

#include "GameFramework/Pawn.h"
#include "Navigation/PathFollowingComponent.h"
#include "NPC/VHVNPCAIController.h"
#include "TimerManager.h"
#include "VHV.h"

UVHVNPCBehaviorComponent::UVHVNPCBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVHVNPCBehaviorComponent::BeginPlay()
{
	Super::BeginPlay();
	BindToAIController();
}

void UVHVNPCBehaviorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(WaitTimerHandle);
	}
	if (BoundAIController)
	{
		BoundAIController->OnBehaviorMoveCompleted.RemoveAll(this);
		BoundAIController = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool UVHVNPCBehaviorComponent::StartMoveToLocation(
	const FVector Destination,
	const float AcceptanceRadius,
	const bool bAllowPartialPath)
{
	if (BehaviorState == EVHVNPCBehaviorState::Engaging || BehaviorState == EVHVNPCBehaviorState::Talking || BehaviorState == EVHVNPCBehaviorState::Unavailable)
	{
		return false;
	}

	ClearActiveOperations(true);
	ActiveOperation = EVHVNPCBehaviorOperation::MoveTo;
	AVHVNPCAIController* AIController = ResolveAIController();
	if (!AIController)
	{
		UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Movement failed for '%s': no VHV NPC AI controller."), *GetNameSafe(GetOwner()));
		CompleteActiveOperation(false);
		return false;
	}

	SetBehaviorState(EVHVNPCBehaviorState::Moving);
	UE_LOG(LogVHV, Log, TEXT("[VHVNPC] Movement started for '%s' to %s."), *GetNameSafe(GetOwner()), *Destination.ToCompactString());
	return HandleMoveRequestResult(
		AIController->RequestMoveToLocation(Destination, FMath::Max(0.0f, AcceptanceRadius), bAllowPartialPath),
		TEXT("location"));
}

bool UVHVNPCBehaviorComponent::StartMoveToActor(
	AActor* Target,
	const float AcceptanceRadius,
	const bool bAllowPartialPath)
{
	if (BehaviorState == EVHVNPCBehaviorState::Engaging || BehaviorState == EVHVNPCBehaviorState::Talking || BehaviorState == EVHVNPCBehaviorState::Unavailable)
	{
		return false;
	}

	ClearActiveOperations(true);
	ActiveOperation = EVHVNPCBehaviorOperation::MoveTo;
	AVHVNPCAIController* AIController = ResolveAIController();
	if (!AIController || !IsValid(Target))
	{
		UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Movement failed for '%s': invalid controller or target."), *GetNameSafe(GetOwner()));
		CompleteActiveOperation(false);
		return false;
	}

	SetBehaviorState(EVHVNPCBehaviorState::Moving);
	UE_LOG(LogVHV, Log, TEXT("[VHVNPC] Movement started for '%s' toward '%s'."), *GetNameSafe(GetOwner()), *GetNameSafe(Target));
	return HandleMoveRequestResult(
		AIController->RequestMoveToActor(Target, FMath::Max(0.0f, AcceptanceRadius), bAllowPartialPath),
		TEXT("actor"));
}

bool UVHVNPCBehaviorComponent::StartWait(const float Duration)
{
	if (BehaviorState == EVHVNPCBehaviorState::Engaging || BehaviorState == EVHVNPCBehaviorState::Talking || BehaviorState == EVHVNPCBehaviorState::Unavailable || Duration <= 0.0f || !GetWorld())
	{
		return false;
	}

	ClearActiveOperations(true);
	ActiveOperation = EVHVNPCBehaviorOperation::Wait;
	SetBehaviorState(EVHVNPCBehaviorState::Waiting);
	GetWorld()->GetTimerManager().SetTimer(
		WaitTimerHandle,
		this,
		&UVHVNPCBehaviorComponent::HandleWaitCompleted,
		Duration,
		false);
	UE_LOG(LogVHV, Log, TEXT("[VHVNPC] Wait started for '%s' (%.2fs)."), *GetNameSafe(GetOwner()), Duration);
	return true;
}

bool UVHVNPCBehaviorComponent::ReturnToPost(const float AcceptanceRadius)
{
	if (BehaviorState == EVHVNPCBehaviorState::Engaging || BehaviorState == EVHVNPCBehaviorState::Talking || BehaviorState == EVHVNPCBehaviorState::Unavailable)
	{
		return false;
	}

	ClearActiveOperations(true);
	ActiveOperation = EVHVNPCBehaviorOperation::ReturnToPost;
	AVHVNPCAIController* AIController = ResolveAIController();
	if (!AIController)
	{
		UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Return-to-post failed for '%s': no VHV NPC AI controller."), *GetNameSafe(GetOwner()));
		CompleteActiveOperation(false);
		return false;
	}

	SetBehaviorState(EVHVNPCBehaviorState::Moving);
	UE_LOG(LogVHV, Log, TEXT("[VHVNPC] Return-to-post started for '%s'."), *GetNameSafe(GetOwner()));
	return HandleMoveRequestResult(
		AIController->ReturnToPost(FMath::Max(0.0f, AcceptanceRadius)),
		TEXT("post"));
}

void UVHVNPCBehaviorComponent::CancelCurrentBehavior()
{
	ClearActiveOperations(true);
	SetBehaviorState(EVHVNPCBehaviorState::Idle);
}

void UVHVNPCBehaviorComponent::SetBehaviorState(const EVHVNPCBehaviorState NewState)
{
	if (BehaviorState == NewState)
	{
		return;
	}

	if (NewState == EVHVNPCBehaviorState::Engaging || NewState == EVHVNPCBehaviorState::Talking)
	{
		ClearActiveOperations(true);
	}

	const EVHVNPCBehaviorState PreviousState = BehaviorState;
	BehaviorState = NewState;
	OnBehaviorStateChanged.Broadcast(PreviousState, BehaviorState);
}

EVHVNPCBehaviorState UVHVNPCBehaviorComponent::GetBehaviorState() const
{
	return BehaviorState;
}

bool UVHVNPCBehaviorComponent::IsAvailableForInteraction() const
{
	return BehaviorState != EVHVNPCBehaviorState::Unavailable;
}

AVHVNPCAIController* UVHVNPCBehaviorComponent::ResolveAIController()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AVHVNPCAIController* AIController = OwnerPawn
		? Cast<AVHVNPCAIController>(OwnerPawn->GetController())
		: nullptr;
	if (AIController != BoundAIController)
	{
		BindToAIController();
	}
	return BoundAIController;
}

void UVHVNPCBehaviorComponent::BindToAIController()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AVHVNPCAIController* AIController = OwnerPawn
		? Cast<AVHVNPCAIController>(OwnerPawn->GetController())
		: nullptr;
	if (AIController == BoundAIController)
	{
		return;
	}

	if (BoundAIController)
	{
		BoundAIController->OnBehaviorMoveCompleted.RemoveAll(this);
	}
	BoundAIController = AIController;
	if (BoundAIController)
	{
		BoundAIController->OnBehaviorMoveCompleted.AddUObject(this, &UVHVNPCBehaviorComponent::HandleMovementCompleted);
	}
}

void UVHVNPCBehaviorComponent::ClearActiveOperations(const bool bStopMovement)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(WaitTimerHandle);
	}

	const bool bHadActiveMovement = bMovementActive;
	bMovementActive = false;
	ActiveOperation = EVHVNPCBehaviorOperation::None;
	if (bStopMovement && bHadActiveMovement)
	{
		if (AVHVNPCAIController* AIController = ResolveAIController())
		{
			AIController->StopMovementForBehavior();
		}
	}
}

void UVHVNPCBehaviorComponent::CompleteActiveOperation(const bool bSuccess)
{
	const EVHVNPCBehaviorOperation CompletedOperation = ActiveOperation;
	if (CompletedOperation == EVHVNPCBehaviorOperation::None)
	{
		return;
	}

	bMovementActive = false;
	ActiveOperation = EVHVNPCBehaviorOperation::None;
	SetBehaviorState(EVHVNPCBehaviorState::Idle);
	OnBehaviorCompleted.Broadcast(CompletedOperation, bSuccess);
}

bool UVHVNPCBehaviorComponent::HandleMoveRequestResult(
	const EPathFollowingRequestResult::Type RequestResult,
	const FString& Description)
{
	if (RequestResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		bMovementActive = true;
		return true;
	}

	if (RequestResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		UE_LOG(LogVHV, Log, TEXT("[VHVNPC] Movement succeeded for '%s' (%s; already at goal)."), *GetNameSafe(GetOwner()), *Description);
		CompleteActiveOperation(true);
		return true;
	}

	UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Movement failed for '%s' (%s request rejected)."), *GetNameSafe(GetOwner()), *Description);
	CompleteActiveOperation(false);
	return false;
}

void UVHVNPCBehaviorComponent::HandleMovementCompleted(const FPathFollowingResult& Result)
{
	if (!bMovementActive)
	{
		return;
	}

	const bool bWasReturningToPost = ActiveOperation == EVHVNPCBehaviorOperation::ReturnToPost;
	const bool bSuccess = Result.IsSuccess();

	if (bSuccess)
	{
		UE_LOG(LogVHV, Log, TEXT("[VHVNPC] Movement succeeded for '%s'%s."),
			*GetNameSafe(GetOwner()),
			bWasReturningToPost ? TEXT(" (returned to post)") : TEXT(""));
	}
	else
	{
		UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Movement failed for '%s'."), *GetNameSafe(GetOwner()));
	}

	CompleteActiveOperation(bSuccess);
}

void UVHVNPCBehaviorComponent::HandleWaitCompleted()
{
	UE_LOG(LogVHV, Log, TEXT("[VHVNPC] Wait finished for '%s'."), *GetNameSafe(GetOwner()));
	CompleteActiveOperation(true);
}
