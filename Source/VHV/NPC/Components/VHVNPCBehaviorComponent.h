#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NPC/Types/VHVNPCBehaviorTypes.h"
#include "VHVNPCBehaviorComponent.generated.h"

class AVHVNPCAIController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnVHVNPCBehaviorStateChanged,
	EVHVNPCBehaviorState,
	PreviousState,
	EVHVNPCBehaviorState,
	NewState
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnVHVNPCBehaviorCompleted,
	EVHVNPCBehaviorOperation,
	CompletedOperation,
	bool,
	bSuccess
);

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVNPCBehaviorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVHVNPCBehaviorComponent();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Behavior", meta = (DisplayName = "Move To Location"))
	bool StartMoveToLocation(FVector Destination, float AcceptanceRadius = 25.0f, bool bAllowPartialPath = true);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Behavior", meta = (DisplayName = "Move To Actor"))
	bool StartMoveToActor(AActor* Target, float AcceptanceRadius = 25.0f, bool bAllowPartialPath = true);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Behavior", meta = (DisplayName = "Wait"))
	bool StartWait(float Duration);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Behavior")
	bool ReturnToPost(float AcceptanceRadius = 25.0f);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Behavior", meta = (DisplayName = "Cancel Behavior"))
	void CancelCurrentBehavior();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Behavior")
	void SetBehaviorState(EVHVNPCBehaviorState NewState);

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Behavior")
	EVHVNPCBehaviorState GetBehaviorState() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Behavior")
	bool IsAvailableForInteraction() const;

	UPROPERTY(BlueprintAssignable, Category = "VHV|NPC|Behavior")
	FOnVHVNPCBehaviorStateChanged OnBehaviorStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "VHV|NPC|Behavior")
	FOnVHVNPCBehaviorCompleted OnBehaviorCompleted;

private:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Behavior", meta = (AllowPrivateAccess = "true"))
	EVHVNPCBehaviorState BehaviorState = EVHVNPCBehaviorState::Idle;

	UPROPERTY(Transient)
	TObjectPtr<AVHVNPCAIController> BoundAIController;

	FTimerHandle WaitTimerHandle;
	bool bMovementActive = false;
	EVHVNPCBehaviorOperation ActiveOperation = EVHVNPCBehaviorOperation::None;

	AVHVNPCAIController* ResolveAIController();
	void BindToAIController();
	void ClearActiveOperations(bool bStopMovement);
	void CompleteActiveOperation(bool bSuccess);
	bool HandleMoveRequestResult(EPathFollowingRequestResult::Type RequestResult, const FString& Description);
	void HandleMovementCompleted(const FPathFollowingResult& Result);
	void HandleWaitCompleted();
};
