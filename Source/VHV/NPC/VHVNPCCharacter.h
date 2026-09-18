#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VHVNPCCharacter.generated.h"

class UVHVInteractionComponent;
class UVHVNPCBehaviorComponent;
class UVHVNPCDialogueComponent;
class UVHVNPCInteractionComponent;
class UVHVNPCPatrolComponent;
class UVHVNPCPresentationComponent;
class UVHVNPCQuestCommandComponent;
class UVHVQuestParticipantComponent;
class USphereComponent;

UCLASS(Blueprintable)
class VHV_API AVHVNPCCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AVHVNPCCharacter();

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Behavior")
	FTransform GetHomeTransform() const;

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Behavior")
	void SetHomeTransform(const FTransform& NewHomeTransform);

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Components")
	UVHVInteractionComponent* GetInteractionComponent() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Components")
	UVHVQuestParticipantComponent* GetQuestParticipantComponent() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Components")
	UVHVNPCBehaviorComponent* GetBehaviorComponent() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Components")
	UVHVNPCInteractionComponent* GetNPCInteractionComponent() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Components")
	UVHVNPCDialogueComponent* GetDialogueComponent() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Components")
	USphereComponent* GetInteractionCollision() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Components")
	UVHVNPCPatrolComponent* GetPatrolComponent() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Components")
	UVHVNPCQuestCommandComponent* GetQuestCommandComponent() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Components")
	UVHVNPCPresentationComponent* GetPresentationComponent() const;

private:
	virtual void BeginPlay() override;

	UPROPERTY(Transient)
	FTransform HomeTransform;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVHVInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVHVQuestParticipantComponent> QuestParticipantComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Components", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UVHVNPCBehaviorComponent> BehaviorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Components", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UVHVNPCInteractionComponent> NPCInteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Components", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UVHVNPCDialogueComponent> DialogueComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Components", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<USphereComponent> InteractionCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Components", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UVHVNPCPatrolComponent> PatrolComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Components", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UVHVNPCQuestCommandComponent> QuestCommandComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Components", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UVHVNPCPresentationComponent> PresentationComponent;
};
