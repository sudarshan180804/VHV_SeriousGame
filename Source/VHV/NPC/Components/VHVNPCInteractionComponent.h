#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VHVNPCInteractionComponent.generated.h"

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVNPCInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVHVNPCInteractionComponent();

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Interaction")
	bool CanInteract() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Interaction")
	FText GetInteractionPrompt() const;

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Interaction")
	void Interact(AActor* InteractingActor);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Interaction")
	FText DefaultInteractionPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Interaction")
	bool bInteractionEnabled = true;
};
