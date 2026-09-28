#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VHVNPCInteractionComponent.generated.h"

class AVHVNPCAIController;
class UVHVConversationDataAsset;

UENUM(BlueprintType)
enum class EVHVNPCAvailableInteraction : uint8
{
	None,
	Quest,
	DefaultDialogue
};

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVNPCInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVHVNPCInteractionComponent();

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Interaction")
	bool CanInteract() const;

	/** Authoritative interaction resolution used by both prompt targeting and execution. */
	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Interaction")
	EVHVNPCAvailableInteraction GetAvailableInteraction() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Interaction")
	FText GetInteractionPrompt() const;

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Interaction")
	void Interact(AActor* InteractingActor);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Interaction")
	FText DefaultInteractionPrompt;

	/** When disabled, keep the normal interaction verb instead of replacing it with tracker copy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Interaction")
	bool bUseQuestObjectiveTextAsPrompt = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Interaction")
	bool bInteractionEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Interaction|Facing")
	bool bFacePlayerBeforeDialogue = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Interaction|Facing", meta = (ClampMin = "0.1", ClampMax = "180.0", Units = "Degrees"))
	float FacingToleranceDegrees = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Interaction|Facing", meta = (ClampMin = "0.1", Units = "Seconds"))
	float FacingTimeoutSeconds = 1.75f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FResolvedInteraction
	{
		EVHVNPCAvailableInteraction Type = EVHVNPCAvailableInteraction::None;
		FName QuestID;
		FName ObjectiveID;
		UVHVConversationDataAsset* Conversation = nullptr;

		bool IsAvailable() const { return Type != EVHVNPCAvailableInteraction::None; }
		bool operator==(const FResolvedInteraction& Other) const
		{
			return Type == Other.Type
				&& QuestID == Other.QuestID
				&& ObjectiveID == Other.ObjectiveID
				&& Conversation == Other.Conversation;
		}
	};

	UPROPERTY(Transient)
	TObjectPtr<AVHVNPCAIController> FacingAIController;

	UPROPERTY(Transient)
	TObjectPtr<AActor> PendingInteractingActor;

	bool bInteractionPending = false;
	FResolvedInteraction PendingInteraction;

	FResolvedInteraction ResolveAvailableInteraction(bool bAllowPendingInteraction) const;
	void HandleFacingCompleted(bool bSuccess);
	void StartDialogueAfterFacing();
	void RestoreNPCStateWithoutDialogue();
};
