#include "NPC/VHVNPCAIController.h"

#include "Navigation/PathFollowingComponent.h"
#include "NPC/VHVNPCCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

EPathFollowingRequestResult::Type AVHVNPCAIController::RequestMoveToLocation(
	const FVector& Destination,
	const float AcceptanceRadius)
{
	return Super::MoveToLocation(Destination, AcceptanceRadius, true, true, true, true, nullptr, true);
}

EPathFollowingRequestResult::Type AVHVNPCAIController::RequestMoveToActor(
	AActor* Target,
	const float AcceptanceRadius)
{
	if (!IsValid(Target))
	{
		return EPathFollowingRequestResult::Failed;
	}

	return Super::MoveToActor(Target, AcceptanceRadius, true, true, true, nullptr, true);
}

EPathFollowingRequestResult::Type AVHVNPCAIController::ReturnToPost(const float AcceptanceRadius)
{
	const AVHVNPCCharacter* NPCCharacter = Cast<AVHVNPCCharacter>(GetPawn());
	return NPCCharacter
		? RequestMoveToLocation(NPCCharacter->GetHomeTransform().GetLocation(), AcceptanceRadius)
		: EPathFollowingRequestResult::Failed;
}

void AVHVNPCAIController::StopMovementForBehavior()
{
	StopMovement();
}

void AVHVNPCAIController::StopForInteraction()
{
	StopMovement();
}

bool AVHVNPCAIController::BeginFaceActor(
	AActor* Target,
	const float ToleranceDegrees,
	const float TimeoutSeconds)
{
	CancelFacing();

	ACharacter* NPCCharacter = Cast<ACharacter>(GetPawn());
	UCharacterMovementComponent* MovementComponent = NPCCharacter ? NPCCharacter->GetCharacterMovement() : nullptr;
	UWorld* World = GetWorld();
	if (!IsValid(Target) || !NPCCharacter || !MovementComponent || !World)
	{
		return false;
	}

	StopMovement();
	bPreviousUseControllerRotationYaw = NPCCharacter->bUseControllerRotationYaw;
	bPreviousOrientRotationToMovement = MovementComponent->bOrientRotationToMovement;
	bPreviousUseControllerDesiredRotation = MovementComponent->bUseControllerDesiredRotation;
	NPCCharacter->bUseControllerRotationYaw = false;
	MovementComponent->bOrientRotationToMovement = false;
	MovementComponent->bUseControllerDesiredRotation = true;
	bFacingRotationOverridden = true;

	FacingTarget = Target;
	ActiveFacingToleranceDegrees = FMath::Clamp(ToleranceDegrees, 0.1f, 180.0f);
	FacingDeadline = World->GetTimeSeconds() + FMath::Max(0.1f, TimeoutSeconds);
	bFacingActive = true;
	SetFocus(Target, EAIFocusPriority::Gameplay);
	World->GetTimerManager().SetTimer(
		FacingCheckTimerHandle,
		this,
		&AVHVNPCAIController::CheckFacing,
		0.05f,
		true);
	CheckFacing();
	return true;
}

void AVHVNPCAIController::CancelFacing()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FacingCheckTimerHandle);
	}
	bFacingActive = false;
	FacingTarget = nullptr;
	ClearFocus(EAIFocusPriority::Gameplay);
	RestoreFacingRotationSettings();
}

void AVHVNPCAIController::FaceActor(AActor* Target)
{
	if (IsValid(Target))
	{
		SetFocus(Target, EAIFocusPriority::Gameplay);
	}
	else
	{
		ClearInteractionFocus();
	}
}

void AVHVNPCAIController::ClearInteractionFocus()
{
	CancelFacing();
}

void AVHVNPCAIController::ResumeBehavior()
{
	ClearInteractionFocus();
}

void AVHVNPCAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);
	OnBehaviorMoveCompleted.Broadcast(Result);
}

void AVHVNPCAIController::OnUnPossess()
{
	CancelFacing();
	Super::OnUnPossess();
}

void AVHVNPCAIController::CheckFacing()
{
	if (!bFacingActive)
	{
		return;
	}

	const APawn* ControlledPawn = GetPawn();
	const UWorld* World = GetWorld();
	if (!ControlledPawn || !IsValid(FacingTarget) || !World)
	{
		FinishFacing(false);
		return;
	}

	const FVector DirectionToTarget = FacingTarget->GetActorLocation() - ControlledPawn->GetActorLocation();
	if (DirectionToTarget.IsNearlyZero())
	{
		FinishFacing(true);
		return;
	}

	const float DesiredYaw = DirectionToTarget.Rotation().Yaw;
	const float YawDifference = FMath::Abs(FMath::FindDeltaAngleDegrees(ControlledPawn->GetActorRotation().Yaw, DesiredYaw));
	if (YawDifference <= ActiveFacingToleranceDegrees)
	{
		FinishFacing(true);
		return;
	}

	if (World->GetTimeSeconds() >= FacingDeadline)
	{
		FinishFacing(false);
	}
}

void AVHVNPCAIController::FinishFacing(const bool bSuccess)
{
	if (!bFacingActive)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FacingCheckTimerHandle);
	}
	bFacingActive = false;
	OnFacingCompleted.Broadcast(bSuccess);
}

void AVHVNPCAIController::RestoreFacingRotationSettings()
{
	if (!bFacingRotationOverridden)
	{
		return;
	}

	if (ACharacter* NPCCharacter = Cast<ACharacter>(GetPawn()))
	{
		NPCCharacter->bUseControllerRotationYaw = bPreviousUseControllerRotationYaw;
		if (UCharacterMovementComponent* MovementComponent = NPCCharacter->GetCharacterMovement())
		{
			MovementComponent->bOrientRotationToMovement = bPreviousOrientRotationToMovement;
			MovementComponent->bUseControllerDesiredRotation = bPreviousUseControllerDesiredRotation;
		}
	}
	bFacingRotationOverridden = false;
}
