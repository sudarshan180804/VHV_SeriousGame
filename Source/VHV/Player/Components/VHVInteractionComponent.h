#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/VHVDialogueTypes.h"
#include "Core/VHVConversationDataAsset.h"
#include "UI/VHVUIManagerComponent.h"
#include "GameFramework/PlayerController.h"
#include "VHVInteractionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVHVInteraction);

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UVHVInteractionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractionPrompt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bCanInteract = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Test")
	bool bEnableTemporaryDialogueTest = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Test", meta = (AllowedClasses = "/Script/VHV.UVHVConversationDataAsset"))
	TSoftObjectPtr<UVHVConversationDataAsset> TemporaryDialogueConversation;

	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnVHVInteraction OnInteracted;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Interact();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void InteractWithActor(AActor* InteractingActor);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	FText GetInteractionPrompt() const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool CanInteract() const;

protected:
	UFUNCTION()
	void TriggerTemporaryDialogueTest();
};
