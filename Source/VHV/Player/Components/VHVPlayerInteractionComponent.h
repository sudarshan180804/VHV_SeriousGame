#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VHVPlayerInteractionComponent.generated.h"

class UVHVInteractionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnInteractionTargetChanged,
	UVHVInteractionComponent*,
	NewTarget
);

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVPlayerInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UVHVPlayerInteractionComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;

	// ========================================================
	// INTERACTION
	// ========================================================

	UFUNCTION(BlueprintCallable, Category = "VHV|Interaction")
	void TryInteract();

	UFUNCTION(BlueprintPure, Category = "VHV|Interaction")
	bool HasInteractionTarget() const;

	UFUNCTION(BlueprintPure, Category = "VHV|Interaction")
	UVHVInteractionComponent* GetCurrentTarget() const;

	// ========================================================
	// SETTINGS
	// ========================================================

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Interaction")
	float InteractionDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Interaction")
	float TraceRadius = 100.0f;

	// ========================================================
	// EVENTS
	// ========================================================

	UPROPERTY(BlueprintAssignable, Category = "VHV|Interaction")
	FOnInteractionTargetChanged OnInteractionTargetChanged;

private:

	UPROPERTY()
	TObjectPtr<UVHVInteractionComponent> CurrentTarget;

	void FindInteractionTarget();
};