#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "NPC/Animation/VHVNPCAnimationProfile.h"
#include "VHVNPCCharacter.generated.h"

class UVHVInteractionComponent;
class UVHVAmbientSpeechComponent;
class UVHVNPCBehaviorComponent;
class UVHVNPCDialogueComponent;
class UVHVNPCInteractionComponent;
class UVHVNPCNameplateComponent;
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
	virtual void OnConstruction(const FTransform& Transform) override;

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

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Components")
	UVHVAmbientSpeechComponent* GetAmbientSpeechComponent() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Components")
	UVHVNPCNameplateComponent* GetNameplateComponent() const;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Animation")
	UVHVNPCAnimationProfile* GetAnimationProfile() const;

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Animation")
	void SetAnimationProfile(UVHVNPCAnimationProfile* NewAnimationProfile);

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Animation")
	EVHVNPCPosture GetNPCPosture() const;

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Animation")
	void SetNPCPosture(EVHVNPCPosture NewPosture);

	UFUNCTION(BlueprintCallable, Category = "VHV|Animation")
	void SetIsSitting(bool bNewIsSitting);

	UFUNCTION(BlueprintPure, Category = "VHV|Animation")
	bool IsSitting() const { return bIsSitting; }

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Animation")
	void SetSpecialAnimationActionActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Animation")
	bool IsSpecialAnimationActionActive() const;

private:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void ApplyRequestedAnimationState(bool bPreviewInEditor = false);

	UPROPERTY(Transient)
	FTransform HomeTransform;

	/** Optional. When unset, the NPC keeps its existing animation behavior unchanged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVHVNPCAnimationProfile> AnimationProfile;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "VHV|NPC|Animation", meta = (AllowPrivateAccess = "true"))
	EVHVNPCPosture CurrentPosture = EVHVNPCPosture::Standing;

	/** Uses the profile's Sitting Idle whenever this placed NPC is stationary. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|Animation",
		meta = (AllowPrivateAccess = "true", DisplayName = "Is Sitting"))
	bool bIsSitting = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "VHV|NPC|Animation", meta = (AllowPrivateAccess = "true"))
	bool bSpecialAnimationActionActive = false;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Components", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UVHVAmbientSpeechComponent> AmbientSpeechComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Components", meta = (AllowPrivateAccess = "true", NoEditInline))
	TObjectPtr<UVHVNPCNameplateComponent> NameplateComponent;
};
