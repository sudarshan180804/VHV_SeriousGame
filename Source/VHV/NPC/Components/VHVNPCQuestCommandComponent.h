#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPC/Types/VHVNPCBehaviorTypes.h"
#include "VHVNPCQuestCommandComponent.generated.h"

class UVHVNPCBehaviorComponent;
class UVHVNPCPatrolComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnVHVNPCQuestCommandCompleted,
	EVHVNPCQuestCommandType,
	Command,
	bool,
	bSuccess
);

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVNPCQuestCommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVHVNPCQuestCommandComponent();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	bool MoveToTarget(FName TargetID);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	bool Wait(float Duration);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	bool ReturnToPost();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	bool ReleaseToPatrol();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	void CancelQuestCommand();

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Quest Commands")
	bool HasQuestCommandOwnership() const;

	UPROPERTY(BlueprintAssignable, Category = "VHV|NPC|Quest Commands")
	FOnVHVNPCQuestCommandCompleted OnQuestCommandCompleted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVHVNPCBehaviorComponent> BehaviorComponent;

	UPROPERTY(Transient)
	TObjectPtr<UVHVNPCPatrolComponent> PatrolComponent;

	EVHVNPCQuestCommandType ActiveCommand = EVHVNPCQuestCommandType::None;
	FName ActiveTargetID;
	float ActiveWaitDuration = 0.0f;
	bool bHasQuestOwnership = false;
	bool bPatrolWasActive = false;
	bool bBehaviorOperationActive = false;
	bool bCommandPending = false;
	bool bResumePatrolWhenIdle = false;

	void BeginQuestOwnership();
	bool QueueOrExecuteActiveCommand();
	bool ExecuteActiveCommand();
	void CompleteActiveCommand(bool bSuccess);
	void AbortActiveCommand(bool bBroadcastFailure);
	void RegisterWithQuestSubsystem();
	void UnregisterFromQuestSubsystem();
	EVHVNPCBehaviorOperation GetExpectedBehaviorOperation() const;

	UFUNCTION()
	void HandleBehaviorCompleted(EVHVNPCBehaviorOperation CompletedOperation, bool bSuccess);

	UFUNCTION()
	void HandleBehaviorStateChanged(EVHVNPCBehaviorState PreviousState, EVHVNPCBehaviorState NewState);
};
