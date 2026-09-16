#include "NPC/VHVNPCAIController.h"

#include "Navigation/PathFollowingComponent.h"
#include "NPC/VHVNPCCharacter.h"

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
	ClearFocus(EAIFocusPriority::Gameplay);
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
