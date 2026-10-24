#include "NPC/Animation/VHVNPCAnimationProfile.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"

bool UVHVNPCAnimationProfile::IsCompatibleWithSkeleton(const USkeleton* Skeleton) const
{
	if (!Skeleton || !HasAllRequiredAnimations())
	{
		return false;
	}

	const auto UsesSkeleton = [Skeleton](const UAnimSequence* Sequence)
	{
		return !Sequence || Sequence->GetSkeleton() == Skeleton;
	};
	return UsesSkeleton(StandingIdleAnimation)
		&& UsesSkeleton(WalkingAnimation)
		&& UsesSkeleton(RunningAnimation)
		&& UsesSkeleton(SittingIdleAnimation)
		&& UsesSkeleton(SpecialActionAnimation);
}

bool UVHVNPCAnimationProfile::HasAllRequiredAnimations() const
{
	// Standing is the only universal state. Locomotion, sitting, and actions are
	// optional because several production Mixamo imports intentionally omit them.
	return StandingIdleAnimation != nullptr;
}
