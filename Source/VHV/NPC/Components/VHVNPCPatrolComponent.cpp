#include "NPC/Components/VHVNPCPatrolComponent.h"

#include "NPC/Components/VHVNPCBehaviorComponent.h"
#include "NPC/Patrol/VHVNPCPatrolRoute.h"
#include "TimerManager.h"
#include "VHV.h"

UVHVNPCPatrolComponent::UVHVNPCPatrolComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVHVNPCPatrolComponent::BeginPlay()
{
	Super::BeginPlay();
	BindBehaviorComponent();

	if (bAutoStartPatrol)
	{
		StartPatrol();
	}
}

void UVHVNPCPatrolComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PatrolStepTimerHandle);
	}
	if (BehaviorComponent)
	{
		BehaviorComponent->OnBehaviorCompleted.RemoveDynamic(this, &UVHVNPCPatrolComponent::HandleBehaviorCompleted);
		BehaviorComponent->OnBehaviorStateChanged.RemoveDynamic(this, &UVHVNPCPatrolComponent::HandleBehaviorStateChanged);
	}

	Super::EndPlay(EndPlayReason);
}

bool UVHVNPCPatrolComponent::StartPatrol()
{
	BindBehaviorComponent();
	FString ValidationError;
	if (!BehaviorComponent || !IsValid(PatrolRoute) || !PatrolRoute->ValidateRoute(ValidationError))
	{
		if (ValidationError.IsEmpty())
		{
			ValidationError = BehaviorComponent ? TEXT("no patrol route assigned") : TEXT("no behavior component available");
		}
		UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Patrol could not start for '%s': %s."), *GetNameSafe(GetOwner()), *ValidationError);
		return false;
	}

	StopPatrol();
	CurrentWaypointIndex = 0;
	bPatrolling = true;
	bPaused = false;
	bPausedForTalking = false;
	StartCurrentWaypoint();
	return bPatrolling;
}

void UVHVNPCPatrolComponent::StopPatrol()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PatrolStepTimerHandle);
	}

	const bool bShouldCancelBehavior = bPatrolOperationActive
		&& BehaviorComponent
		&& BehaviorComponent->GetBehaviorState() != EVHVNPCBehaviorState::Engaging
		&& BehaviorComponent->GetBehaviorState() != EVHVNPCBehaviorState::Talking;
	bPatrolOperationActive = false;
	ExpectedOperation = EVHVNPCBehaviorOperation::None;
	bPatrolling = false;
	bPaused = false;
	bPausedForTalking = false;
	if (bShouldCancelBehavior)
	{
		BehaviorComponent->CancelCurrentBehavior();
	}
}

void UVHVNPCPatrolComponent::PausePatrol()
{
	if (!bPatrolling || bPaused)
	{
		return;
	}

	bPaused = true;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PatrolStepTimerHandle);
	}

	const bool bShouldCancelBehavior = bPatrolOperationActive
		&& BehaviorComponent
		&& BehaviorComponent->GetBehaviorState() != EVHVNPCBehaviorState::Engaging
		&& BehaviorComponent->GetBehaviorState() != EVHVNPCBehaviorState::Talking;
	bPatrolOperationActive = false;
	ExpectedOperation = EVHVNPCBehaviorOperation::None;
	if (bShouldCancelBehavior)
	{
		BehaviorComponent->CancelCurrentBehavior();
	}
}

bool UVHVNPCPatrolComponent::ResumePatrol()
{
	if (!bPatrolling || !bPaused || !BehaviorComponent
		|| BehaviorComponent->GetBehaviorState() == EVHVNPCBehaviorState::Engaging
		|| BehaviorComponent->GetBehaviorState() == EVHVNPCBehaviorState::Talking)
	{
		return false;
	}

	bPaused = false;
	ScheduleCurrentWaypoint();
	return true;
}

bool UVHVNPCPatrolComponent::IsPatrolling() const
{
	return bPatrolling;
}

int32 UVHVNPCPatrolComponent::GetCurrentWaypointIndex() const
{
	return CurrentWaypointIndex;
}

void UVHVNPCPatrolComponent::BindBehaviorComponent()
{
	UVHVNPCBehaviorComponent* ResolvedBehavior = GetOwner()
		? GetOwner()->FindComponentByClass<UVHVNPCBehaviorComponent>()
		: nullptr;
	if (ResolvedBehavior == BehaviorComponent)
	{
		return;
	}

	if (BehaviorComponent)
	{
		BehaviorComponent->OnBehaviorCompleted.RemoveDynamic(this, &UVHVNPCPatrolComponent::HandleBehaviorCompleted);
		BehaviorComponent->OnBehaviorStateChanged.RemoveDynamic(this, &UVHVNPCPatrolComponent::HandleBehaviorStateChanged);
	}
	BehaviorComponent = ResolvedBehavior;
	if (BehaviorComponent)
	{
		BehaviorComponent->OnBehaviorCompleted.AddUniqueDynamic(this, &UVHVNPCPatrolComponent::HandleBehaviorCompleted);
		BehaviorComponent->OnBehaviorStateChanged.AddUniqueDynamic(this, &UVHVNPCPatrolComponent::HandleBehaviorStateChanged);
	}
}

void UVHVNPCPatrolComponent::StartCurrentWaypoint()
{
	if (!bPatrolling || bPaused || !BehaviorComponent || !IsValid(PatrolRoute)
		|| !PatrolRoute->Waypoints.IsValidIndex(CurrentWaypointIndex))
	{
		return;
	}

	const FVHVNPCPatrolWaypoint& Waypoint = PatrolRoute->Waypoints[CurrentWaypointIndex];
	ExpectedOperation = EVHVNPCBehaviorOperation::MoveTo;
	bPatrolOperationActive = true;
	const bool bStarted = BehaviorComponent->StartMoveToActor(Waypoint.TargetActor);
	if (!bStarted && bPatrolling)
	{
		StopPatrolForFailure(TEXT("waypoint movement could not start"));
	}
}

void UVHVNPCPatrolComponent::ScheduleCurrentWaypoint()
{
	if (!GetWorld())
	{
		StartCurrentWaypoint();
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(PatrolStepTimerHandle);
	PatrolStepTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UVHVNPCPatrolComponent::StartCurrentWaypoint));
}

void UVHVNPCPatrolComponent::ScheduleAdvanceWaypoint()
{
	if (!GetWorld())
	{
		AdvanceWaypoint();
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(PatrolStepTimerHandle);
	PatrolStepTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UVHVNPCPatrolComponent::AdvanceWaypoint));
}

void UVHVNPCPatrolComponent::AdvanceWaypoint()
{
	if (!bPatrolling || bPaused || !IsValid(PatrolRoute))
	{
		return;
	}

	const int32 NextWaypointIndex = CurrentWaypointIndex + 1;
	if (PatrolRoute->Waypoints.IsValidIndex(NextWaypointIndex))
	{
		CurrentWaypointIndex = NextWaypointIndex;
		StartCurrentWaypoint();
		return;
	}

	if (PatrolRoute->PatrolMode == EVHVNPCPatrolMode::Loop)
	{
		CurrentWaypointIndex = 0;
		StartCurrentWaypoint();
		return;
	}

	StopPatrol();
}

void UVHVNPCPatrolComponent::StopPatrolForFailure(const TCHAR* Reason)
{
	UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Patrol stopped for '%s': %s."), *GetNameSafe(GetOwner()), Reason);
	bPatrolOperationActive = false;
	StopPatrol();
}

void UVHVNPCPatrolComponent::HandleBehaviorCompleted(
	const EVHVNPCBehaviorOperation CompletedOperation,
	const bool bSuccess)
{
	if (!bPatrolling || bPaused || !bPatrolOperationActive || CompletedOperation != ExpectedOperation)
	{
		return;
	}

	bPatrolOperationActive = false;
	ExpectedOperation = EVHVNPCBehaviorOperation::None;
	if (!bSuccess)
	{
		StopPatrolForFailure(TEXT("behavior operation failed"));
		return;
	}

	if (CompletedOperation == EVHVNPCBehaviorOperation::MoveTo)
	{
		const FVHVNPCPatrolWaypoint& Waypoint = PatrolRoute->Waypoints[CurrentWaypointIndex];
		if (Waypoint.WaitDuration > 0.0f)
		{
			ExpectedOperation = EVHVNPCBehaviorOperation::Wait;
			bPatrolOperationActive = true;
			const bool bWaitStarted = BehaviorComponent->StartWait(Waypoint.WaitDuration);
			if (!bWaitStarted && bPatrolling)
			{
				StopPatrolForFailure(TEXT("waypoint wait could not start"));
			}
			return;
		}
	}

	ScheduleAdvanceWaypoint();
}

void UVHVNPCPatrolComponent::HandleBehaviorStateChanged(
	const EVHVNPCBehaviorState PreviousState,
	const EVHVNPCBehaviorState NewState)
{
	if (!bPatrolling)
	{
		return;
	}

	if (NewState == EVHVNPCBehaviorState::Engaging || NewState == EVHVNPCBehaviorState::Talking)
	{
		if (!bPaused)
		{
			bPausedForTalking = true;
			PausePatrol();
		}
		return;
	}

	if (NewState == EVHVNPCBehaviorState::Idle && bPausedForTalking)
	{
		bPausedForTalking = false;
		ResumePatrol();
	}
}
