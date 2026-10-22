#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "NPC/Animation/VHVNPCAnimationProfile.h"
#include "VHVMixamoNPCAnimInstance.generated.h"

struct FVHVMixamoNPCAnimInstanceProxy;

UENUM(BlueprintType)
enum class EVHVMixamoAnimationState : uint8
{
	StandingIdle UMETA(DisplayName = "Standing Idle"),
	Walking UMETA(DisplayName = "Walking"),
	SittingIdle UMETA(DisplayName = "Sitting Idle")
};

/**
 * Skeleton-agnostic native logic used by skeleton-specific Mixamo Animation Blueprints.
 * It evaluates profile-selected sequences without Control Rigs, IK, or root motion.
 */
UCLASS(Transient, Blueprintable)
class VHV_API UVHVMixamoNPCAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Animation")
	void SetAnimationProfile(UVHVNPCAnimationProfile* NewProfile);

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Animation")
	UVHVNPCAnimationProfile* GetAnimationProfile() const;

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Animation")
	void SetRequestedPosture(EVHVNPCPosture NewPosture);

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Animation")
	EVHVNPCPosture GetRequestedPosture() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Animation")
	EVHVMixamoAnimationState GetCurrentAnimationState() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Animation")
	float GetGroundSpeed() const;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;

private:
	void RefreshFromOwningNPC();
	bool ValidateProfileForCurrentMesh() const;

	UPROPERTY(Transient)
	TObjectPtr<UVHVNPCAnimationProfile> AnimationProfile;

	UPROPERTY(Transient)
	EVHVNPCPosture RequestedPosture = EVHVNPCPosture::Standing;

	UPROPERTY(Transient)
	EVHVMixamoAnimationState CurrentAnimationState = EVHVMixamoAnimationState::StandingIdle;

	UPROPERTY(Transient)
	float GroundSpeed = 0.0f;

	bool bProfileWarningIssued = false;

	friend struct FVHVMixamoNPCAnimInstanceProxy;
};
