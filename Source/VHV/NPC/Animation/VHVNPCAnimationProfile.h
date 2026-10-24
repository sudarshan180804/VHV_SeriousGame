#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VHVNPCAnimationProfile.generated.h"

class UAnimSequence;
class USkeleton;

UENUM(BlueprintType)
enum class EVHVNPCPosture : uint8
{
	Standing UMETA(DisplayName = "Standing"),
	Sitting UMETA(DisplayName = "Sitting")
};

/**
 * Character-specific clips and tuning for the lightweight Mixamo NPC player.
 * All sequences must use the same skeleton as the mesh/Animation Blueprint.
 */
UCLASS(BlueprintType)
class VHV_API UVHVNPCAnimationProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	TObjectPtr<UAnimSequence> StandingIdleAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	TObjectPtr<UAnimSequence> WalkingAnimation;

	/** Optional faster locomotion clip. Production NPC profiles normally leave this unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	TObjectPtr<UAnimSequence> RunningAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	TObjectPtr<UAnimSequence> SittingIdleAnimation;

	/** Optional non-locomotion clip such as talking, a workout, or a character-specific gesture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	TObjectPtr<UAnimSequence> SpecialActionAnimation;

	/** Use the optional action only while this NPC is the active speaker in authored dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	bool bUseSpecialActionDuringDialogue = false;

	/** Keep the optional action active while the NPC is outside an interaction/dialogue session. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	bool bUseSpecialActionOutsideDialogue = false;

	/** Workout clips normally play once; conversational clips may loop for the duration of a line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	bool bLoopSpecialAction = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations", meta = (ClampMin = "0.01"))
	float SpecialActionPlaybackSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float IdleWalkThreshold = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.01"))
	float WalkPlaybackSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float RunSpeedThreshold = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.01"))
	float RunPlaybackSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transitions", meta = (ClampMin = "0.0", Units = "s"))
	float TransitionBlendTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
	EVHVNPCPosture InitialPosture = EVHVNPCPosture::Standing;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Animation")
	bool IsCompatibleWithSkeleton(const USkeleton* Skeleton) const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Animation")
	bool HasAllRequiredAnimations() const;
};
