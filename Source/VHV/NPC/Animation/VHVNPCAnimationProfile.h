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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animations")
	TObjectPtr<UAnimSequence> SittingIdleAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float IdleWalkThreshold = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.01"))
	float WalkPlaybackSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transitions", meta = (ClampMin = "0.0", Units = "s"))
	float TransitionBlendTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Posture")
	EVHVNPCPosture InitialPosture = EVHVNPCPosture::Standing;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Animation")
	bool IsCompatibleWithSkeleton(const USkeleton* Skeleton) const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Animation")
	bool HasAllRequiredAnimations() const;
};
