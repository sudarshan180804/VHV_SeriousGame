// Copyright Epic Games, Inc. All Rights Reserved.

#include "VHVCharacter.h"

#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "VHV.h"
#include "VHV/Player/Components/VHVPlayerInteractionComponent.h"
#include "VHV/NPC/Components/VHVAmbientSpeechComponent.h"
#include "VHV/UI/VHVUIManagerComponent.h"

AVHVCharacter::AVHVCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates.
	// Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Character movement settings
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create camera boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(
		TEXT("CameraBoom")
	);

	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(
		TEXT("FollowCamera")
	);

	FollowCamera->SetupAttachment(
		CameraBoom,
		USpringArmComponent::SocketName
	);

	FollowCamera->bUsePawnControlRotation = false;

	// Create VHV player interaction component
	InteractionComponent =
		CreateDefaultSubobject<UVHVPlayerInteractionComponent>(
			TEXT("InteractionComponent")
			);

	ThoughtSpeechComponent = CreateDefaultSubobject<UVHVAmbientSpeechComponent>(
		TEXT("ThoughtSpeechComponent"));

	// The skeletal mesh and animation blueprint references
	// remain configured in the ThirdPersonCharacter Blueprint.
}

void AVHVCharacter::SetupPlayerInputComponent(
	UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent =
		Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(
			JumpAction,
			ETriggerEvent::Started,
			this,
			&ACharacter::Jump
		);

		EnhancedInputComponent->BindAction(
			JumpAction,
			ETriggerEvent::Completed,
			this,
			&ACharacter::StopJumping
		);

		// Moving
		EnhancedInputComponent->BindAction(
			MoveAction,
			ETriggerEvent::Triggered,
			this,
			&AVHVCharacter::Move
		);

		// Looking
		EnhancedInputComponent->BindAction(
			MouseLookAction,
			ETriggerEvent::Triggered,
			this,
			&AVHVCharacter::Look
		);

		EnhancedInputComponent->BindAction(
			LookAction,
			ETriggerEvent::Triggered,
			this,
			&AVHVCharacter::Look
		);

		// Interaction
		EnhancedInputComponent->BindAction(
			InteractAction,
			ETriggerEvent::Started,
			this,
			&AVHVCharacter::Interact
		);
	}
	else
	{
		UE_LOG(
			LogVHV,
			Error,
			TEXT("'%s' Failed to find an Enhanced Input component! "
			     "This template is built to use the Enhanced Input system. "
			     "If you intend to use the legacy system, then you will "
			     "need to update this C++ file."),
			*GetNameSafe(this)
		);
	}
}

void AVHVCharacter::Move(const FInputActionValue& Value)
{
	// Input is a Vector2D
	const FVector2D MovementVector = Value.Get<FVector2D>();

	// Route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AVHVCharacter::Look(const FInputActionValue& Value)
{
	// Input is a Vector2D
	const FVector2D LookAxisVector = Value.Get<FVector2D>();

	// Route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AVHVCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// Find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// Get forward vector
		const FVector ForwardDirection =
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// Get right vector
		const FVector RightDirection =
			FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// Add movement
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AVHVCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// Add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AVHVCharacter::DoJumpStart()
{
	// Signal the character to jump
	Jump();
}

void AVHVCharacter::DoJumpEnd()
{
	// Signal the character to stop jumping
	StopJumping();
}

void AVHVCharacter::Interact()
{
	if (const APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (UVHVUIManagerComponent* UIManager = PlayerController->FindComponentByClass<UVHVUIManagerComponent>())
		{
			if (UIManager->HandleActivityInteractionInput())
			{
				return;
			}
		}
	}

	UE_LOG(
		LogVHV,
		Warning,
		TEXT("INTERACT INPUT RECEIVED")
	);

	if (InteractionComponent)
	{
		UE_LOG(
			LogVHV,
			Warning,
			TEXT("Player Interaction Component exists")
		);

		InteractionComponent->TryInteract();
	}
	else
	{
		UE_LOG(
			LogVHV,
			Error,
			TEXT("Player Interaction Component is NULL")
		);
	}
}
