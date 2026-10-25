// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "TimerManager.h"
#include "VHVCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UVHVPlayerInteractionComponent;
class UVHVAmbientSpeechComponent;
class UVHVNPCAnimationProfile;

struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 * A simple player-controllable third person character
 * Implements a controllable orbiting camera
 */
UCLASS(abstract)
class VHV_API AVHVCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Player interaction component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVHVPlayerInteractionComponent> InteractionComponent;

	/** Reuses the ambient world-bubble presenter for explicitly authored player thoughts. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVHVAmbientSpeechComponent> ThoughtSpeechComponent;

protected:
	virtual void BeginPlay() override;
	virtual bool CanJumpInternal_Implementation() const override;

	/** Character-specific Mixamo clips used by the production player visual. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VHV|Player|Animation")
	TObjectPtr<UVHVNPCAnimationProfile> PlayerAnimationProfile;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* MouseLookAction;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* InteractAction;

public:

	/** Constructor */
	AVHVCharacter();

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Called for movement input */
	void Move(const FInputActionValue& Value);
	void PlayMovementFootstep();
	FTimerHandle FootstepTimer;

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Intentionally disabled; retained as a no-op for Blueprint compatibility. */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoJumpStart();

	/** Intentionally disabled; retained as a no-op for Blueprint compatibility. */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void DoJumpEnd();

	/** Handles interaction input */
	UFUNCTION(BlueprintCallable, Category = "VHV|Interaction")
	void Interact();

	/** Event-driven dialogue pose. It never changes movement or input ownership. */
	UFUNCTION(BlueprintCallable, Category = "VHV|Player|Animation")
	void SetConversationTalking(bool bTalking);

	UFUNCTION(BlueprintPure, Category = "VHV|Player|Animation")
	bool IsConversationTalking() const { return bConversationTalking; }

public:

	/** Returns CameraBoom subobject */
	FORCEINLINE USpringArmComponent* GetCameraBoom() const
	{
		return CameraBoom;
	}

	/** Returns FollowCamera subobject */
	FORCEINLINE UCameraComponent* GetFollowCamera() const
	{
		return FollowCamera;
	}

	/** Returns player interaction component */
	FORCEINLINE UVHVPlayerInteractionComponent* GetInteractionComponent() const
	{
		return InteractionComponent;
	}

	FORCEINLINE UVHVAmbientSpeechComponent* GetThoughtSpeechComponent() const
	{
		return ThoughtSpeechComponent;
	}

private:
	UPROPERTY(Transient)
	bool bConversationTalking = false;
};
