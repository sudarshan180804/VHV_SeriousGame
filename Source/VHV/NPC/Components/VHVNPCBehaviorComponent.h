#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NPC/Types/VHVNPCBehaviorTypes.h"
#include "VHVNPCBehaviorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnVHVNPCBehaviorStateChanged,
	EVHVNPCBehaviorState,
	PreviousState,
	EVHVNPCBehaviorState,
	NewState
);

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVNPCBehaviorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVHVNPCBehaviorComponent();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Behavior")
	void SetBehaviorState(EVHVNPCBehaviorState NewState);

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Behavior")
	EVHVNPCBehaviorState GetBehaviorState() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Behavior")
	bool IsAvailableForInteraction() const;

	UPROPERTY(BlueprintAssignable, Category = "VHV|NPC|Behavior")
	FOnVHVNPCBehaviorStateChanged OnBehaviorStateChanged;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Behavior", meta = (AllowPrivateAccess = "true"))
	EVHVNPCBehaviorState BehaviorState = EVHVNPCBehaviorState::Idle;
};
