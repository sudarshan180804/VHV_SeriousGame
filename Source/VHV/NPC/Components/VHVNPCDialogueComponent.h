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

	/** Legacy convenience path; authoritative NPC interactions use the resolved methods below. */
	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Dialogue")
	bool StartDialogue(AActor* InteractingActor);

	bool StartQuestInteraction(AActor* InteractingActor, FName ExpectedQuestID = NAME_None, FName ExpectedObjectiveID = NAME_None);
	bool StartDefaultDialogue(AActor* InteractingActor, UVHVConversationDataAsset* ExpectedConversation = nullptr);
	bool CanStartDefaultDialogue(AActor* InteractingActor = nullptr) const;
	UVHVConversationDataAsset* GetAvailableDefaultConversation(AActor* InteractingActor = nullptr) const;

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Dialogue")
	void EndDialogue();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Dialogue")
	TSoftObjectPtr<UVHVConversationDataAsset> DefaultConversation;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVHVUIManagerComponent> ActiveUIManager;

	UPROPERTY(Transient)
	TObjectPtr<UVHVConversationDataAsset> CachedDefaultConversation;

	UFUNCTION()
	void HandleConversationSessionEnded();

	UVHVUIManagerComponent* ResolveUIManager(AActor* InteractingActor) const;
	bool BeginDialogueSession(UVHVUIManagerComponent* UIManager);
	void CancelDialogueSessionBinding();
	void RestoreNPCState();
};
