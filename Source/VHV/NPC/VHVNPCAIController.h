#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "VHVNPCAIController.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnVHVNPCMoveCompleted, const FPathFollowingResult&);

UCLASS()
class VHV_API AVHVNPCAIController : public AAIController
{
	GENERATED_BODY()

public:
	EPathFollowingRequestResult::Type RequestMoveToLocation(const FVector& Destination, float AcceptanceRadius);
	EPathFollowingRequestResult::Type RequestMoveToActor(AActor* Target, float AcceptanceRadius);
	EPathFollowingRequestResult::Type ReturnToPost(float AcceptanceRadius);
	void StopMovementForBehavior();

	FOnVHVNPCMoveCompleted OnBehaviorMoveCompleted;

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
};
