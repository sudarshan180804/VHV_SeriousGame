#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPC/Types/VHVNPCBehaviorTypes.h"
#include "VHVNPCQuestCommandComponent.generated.h"

class UVHVNPCBehaviorComponent;
class UVHVNPCPatrolComponent;
class UVHVNPCPresentationComponent;
class UVHVNPCQuestCommandComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnVHVNPCQuestCommandCompleted,
	EVHVNPCQuestCommandType,
	Command,
	bool,
	bSuccess
);

DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FOnVHVNPCQuestCommandCompletedNative,
	UVHVNPCQuestCommandComponent*,
	EVHVNPCQuestCommandType,
	bool
);

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVNPCQuestCommandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVHVNPCQuestCommandComponent();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	bool MoveToTarget(FName TargetID, bool bFaceDestinationRotation = false, float MoveSpeedOverride = 0.0f);

	/** Use the normal queued quest-movement pipeline for a validated runtime location. */
	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	bool MoveToLocation(
		FVector Destination,
		float AcceptanceRadius = 90.0f,
		bool bFaceDestinationRotation = false,
		FRotator DestinationRotation = FRotator::ZeroRotator,
		float MoveSpeedOverride = 0.0f);

	/** Short, prevalidated quest-presence walk with posture restore and bounded fallback. */
	bool MoveToLocationForPresence(
		FVector Destination,
		FRotator DestinationRotation,
		bool bSitAtDestination,
		float AcceptanceRadius = 45.0f,
		float MoveSpeedOverride = 260.0f,
		float TimeoutSeconds = 4.5f);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	bool Wait(float Duration);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	bool ReturnToPost();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	bool PlayAction(FName ActionID);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	bool ReleaseToPatrol();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Quest Commands")
	void CancelQuestCommand();

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Quest Commands")
	bool HasQuestCommandOwnership() const;

	/** True after this persistent NPC has successfully completed a move to the semantic target. */
	bool HasReachedTarget(FName TargetID) const { return !TargetID.IsNone() && LastSuccessfulMoveTargetID == TargetID; }

	UPROPERTY(BlueprintAssignable, Category = "VHV|NPC|Quest Commands")
	FOnVHVNPCQuestCommandCompleted OnQuestCommandCompleted;

	/** Native completion signal includes the command component so paired quest moves can track each participant. */
	FOnVHVNPCQuestCommandCompletedNative OnQuestCommandCompletedNative;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVHVNPCBehaviorComponent> BehaviorComponent;

	UPROPERTY(Transient)
	TObjectPtr<UVHVNPCPatrolComponent> PatrolComponent;

	UPROPERTY(Transient)
	TObjectPtr<UVHVNPCPresentationComponent> PresentationComponent;

	EVHVNPCQuestCommandType ActiveCommand = EVHVNPCQuestCommandType::None;
	FName ActiveTargetID;
	FName LastSuccessfulMoveTargetID;
	FName ActiveActionID;
	float ActiveWaitDuration = 0.0f;
	float ActiveMoveSpeedOverride = 0.0f;
	float ActiveMoveAcceptanceRadius = 25.0f;
	float PreviousMaxWalkSpeed = 0.0f;
	bool bFaceActiveDestinationRotation = false;
	bool bUseDirectMoveDestination = false;
	bool bMoveSpeedOverridden = false;
	FVector ActiveMoveDestination = FVector::ZeroVector;
	FRotator ActiveMoveDestinationRotation = FRotator::ZeroRotator;
	TWeakObjectPtr<AActor> ActiveMoveTarget;
	bool bHasQuestOwnership = false;
	bool bPatrolWasActive = false;
	bool bBehaviorOperationActive = false;
	bool bCommandPending = false;
	bool bResumePatrolWhenIdle = false;
	bool bPresenceTransition = false;
	bool bPresencePreviousSitting = false;
	bool bPresenceDestinationSitting = false;
	bool bPresenceTeleportOnTimeout = false;
	float PresenceMoveTimeoutSeconds = 0.0f;
	FTimerHandle PresenceMoveTimeoutHandle;

	void BeginQuestOwnership();
	bool QueueOrExecuteActiveCommand();
	bool ExecuteActiveCommand();
	void ApplyActiveMoveSpeedOverride();
	void RestoreMoveSpeedOverride();
	void RestorePresencePosture(bool bReachedDestination);
	void HandlePresenceMoveTimeout();
	void CompleteActiveCommand(bool bSuccess);
	void AbortActiveCommand(bool bBroadcastFailure);
	void RegisterWithQuestSubsystem();
	void UnregisterFromQuestSubsystem();
	EVHVNPCBehaviorOperation GetExpectedBehaviorOperation() const;

	UFUNCTION()
	void HandleBehaviorCompleted(EVHVNPCBehaviorOperation CompletedOperation, bool bSuccess);

	UFUNCTION()
	void HandleBehaviorStateChanged(EVHVNPCBehaviorState PreviousState, EVHVNPCBehaviorState NewState);

	UFUNCTION()
	void HandlePresentationActionCompleted(FName ActionID, bool bSuccess);
};
