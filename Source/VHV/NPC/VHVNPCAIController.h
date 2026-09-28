#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "VHVNPCAIController.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnVHVNPCMoveCompleted, const FPathFollowingResult&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnVHVNPCFacingCompleted, bool);

UCLASS()
class VHV_API AVHVNPCAIController : public AAIController
{
	GENERATED_BODY()

public:
	EPathFollowingRequestResult::Type RequestMoveToLocation(const FVector& Destination, float AcceptanceRadius, bool bAllowPartialPath = true);
	EPathFollowingRequestResult::Type RequestMoveToActor(AActor* Target, float AcceptanceRadius, bool bAllowPartialPath = true);
	EPathFollowingRequestResult::Type ReturnToPost(float AcceptanceRadius);
	void StopMovementForBehavior();

	FOnVHVNPCMoveCompleted OnBehaviorMoveCompleted;
	FOnVHVNPCFacingCompleted OnFacingCompleted;

	bool BeginFaceActor(AActor* Target, float ToleranceDegrees = 6.0f, float TimeoutSeconds = 1.75f);
	void CancelFacing();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|AI")
	void StopForInteraction();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|AI")
	void FaceActor(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|AI")
	void ClearInteractionFocus();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|AI")
	void ResumeBehavior();

protected:
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	virtual void OnUnPossess() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> FacingTarget;

	FTimerHandle FacingCheckTimerHandle;
	float ActiveFacingToleranceDegrees = 6.0f;
	float FacingDeadline = 0.0f;
	bool bFacingActive = false;
	bool bFacingRotationOverridden = false;
	bool bPreviousUseControllerRotationYaw = false;
	bool bPreviousOrientRotationToMovement = false;
	bool bPreviousUseControllerDesiredRotation = false;

	void CheckFacing();
	void FinishFacing(bool bSuccess);
	void RestoreFacingRotationSettings();
};
