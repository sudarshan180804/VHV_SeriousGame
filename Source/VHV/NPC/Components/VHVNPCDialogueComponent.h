#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VHVNPCDialogueComponent.generated.h"

class UVHVConversationDataAsset;
class UVHVUIManagerComponent;

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVNPCDialogueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVHVNPCDialogueComponent();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Dialogue")
	bool StartDialogue(AActor* InteractingActor);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Dialogue")
	void EndDialogue();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Dialogue")
	TSoftObjectPtr<UVHVConversationDataAsset> DefaultConversation;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVHVUIManagerComponent> ActiveUIManager;

	UFUNCTION()
	void HandleConversationEnded();

	UVHVUIManagerComponent* ResolveUIManager(AActor* InteractingActor) const;
	void RestoreNPCState();
};
