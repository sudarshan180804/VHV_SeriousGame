#include "NPC/Components/VHVNPCBehaviorComponent.h"

UVHVNPCBehaviorComponent::UVHVNPCBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UVHVNPCBehaviorComponent::SetBehaviorState(const EVHVNPCBehaviorState NewState)
{
	if (BehaviorState == NewState)
	{
		return;
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
