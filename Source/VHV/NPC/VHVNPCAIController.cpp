#include "NPC/VHVNPCAIController.h"

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
