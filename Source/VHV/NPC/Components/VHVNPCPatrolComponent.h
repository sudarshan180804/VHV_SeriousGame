#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPC/Types/VHVNPCBehaviorTypes.h"
#include "VHVNPCPatrolComponent.generated.h"

class AVHVNPCPatrolRoute;
class UVHVNPCBehaviorComponent;

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVNPCPatrolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVHVNPCPatrolComponent();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Patrol")
	bool StartPatrol();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Patrol")
	void StopPatrol();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Patrol")
	void PausePatrol();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Patrol")
	bool ResumePatrol();

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Patrol")
	bool IsPatrolling() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Patrol")
	int32 GetCurrentWaypointIndex() const;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|NPC|Patrol")
	TObjectPtr<AVHVNPCPatrolRoute> PatrolRoute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Patrol")
	bool bAutoStartPatrol = false;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVHVNPCBehaviorComponent> BehaviorComponent;

	FTimerHandle PatrolStepTimerHandle;
	int32 CurrentWaypointIndex = INDEX_NONE;
	bool bPatrolling = false;
	bool bPaused = false;
	bool bPausedForTalking = false;
	bool bPatrolOperationActive = false;
	EVHVNPCBehaviorOperation ExpectedOperation = EVHVNPCBehaviorOperation::None;

	void BindBehaviorComponent();
	void StartCurrentWaypoint();
	void ScheduleCurrentWaypoint();
	void ScheduleAdvanceWaypoint();
	void AdvanceWaypoint();
	void StopPatrolForFailure(const TCHAR* Reason);

	UFUNCTION()
	void HandleBehaviorCompleted(EVHVNPCBehaviorOperation CompletedOperation, bool bSuccess);

	UFUNCTION()
	void HandleBehaviorStateChanged(EVHVNPCBehaviorState PreviousState, EVHVNPCBehaviorState NewState);
};
