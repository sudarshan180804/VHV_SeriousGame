#include "NPC/Animation/VHVNPCAnimationProfile.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"

bool UVHVNPCAnimationProfile::IsCompatibleWithSkeleton(const USkeleton* Skeleton) const
{
	if (!Skeleton || !HasAllRequiredAnimations())
	{
		return false;
	}

	return StandingIdleAnimation->GetSkeleton() == Skeleton
		&& WalkingAnimation->GetSkeleton() == Skeleton
		&& SittingIdleAnimation->GetSkeleton() == Skeleton;
}

bool UVHVNPCAnimationProfile::HasAllRequiredAnimations() const
{
	return StandingIdleAnimation != nullptr
		&& WalkingAnimation != nullptr
		&& SittingIdleAnimation != nullptr;
}
