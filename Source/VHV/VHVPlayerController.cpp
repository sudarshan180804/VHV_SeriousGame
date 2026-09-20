// Copyright Epic Games, Inc. All Rights Reserved.


#include "VHVPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "VHV.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UI/VHVUIManagerComponent.h"
#include "Textbook/Systems/VHVTextbookSubsystem.h"
#include "Textbook/Data/VHVLevelData.h"
#include "Quest/Data/VHVQuestArcData.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "Save/VHVSaveSubsystem.h"

AVHVPlayerController::AVHVPlayerController()
{
    UIManagerComponent = CreateDefaultSubobject<UVHVUIManagerComponent>(TEXT("UIManagerComponent"));
}

void AVHVPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	const UVHVSaveSubsystem* SaveSubsystem = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UVHVSaveSubsystem>()
		: nullptr;
	if (SaveSubsystem && SaveSubsystem->IsLoadTravelPending())
	{
		return;
	}

	bool bQuestFlowStarted = false;
	if (bUseQuestFlow && !StartingQuestArc.IsNull() && World && World->GetGameInstance())
	{
		if (UVHVQuestArcData* QuestArc = StartingQuestArc.LoadSynchronous())
		{
			if (UVHVQuestSubsystem* QuestSubsystem = World->GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>())
			{
				bQuestFlowStarted = QuestSubsystem->StartQuestArc(QuestArc);
			}
		}
	}

	if (!bQuestFlowStarted)
	{
		// Preserve the existing standalone textbook startup path when quest flow is not configured.
		if (!StartingLevelData.IsValid())
		{
			StartingLevelData = TSoftObjectPtr<UVHVLevelData>(FSoftObjectPath(TEXT("/Game/VHV_Stuff/Textbook/Levels/Level1/DA_Level_01_Unit_01")));
		}

		UVHVLevelData* ResolvedLevelData = StartingLevelData.LoadSynchronous();
		if (ResolvedLevelData && ResolvedLevelData->GetName() != TEXT("DA_Level_01_Unit_01"))
		{
			StartingLevelData = TSoftObjectPtr<UVHVLevelData>(FSoftObjectPath(TEXT("/Game/VHV_Stuff/Textbook/Levels/Level1/DA_Level_01_Unit_01")));
			ResolvedLevelData = StartingLevelData.LoadSynchronous();
		}

		if (ResolvedLevelData && World && World->GetGameInstance())
		{
			if (UVHVTextbookSubsystem* TextbookSubsystem = World->GetGameInstance()->GetSubsystem<UVHVTextbookSubsystem>())
			{
				TextbookSubsystem->StartJourney(ResolvedLevelData);
			}
		}
	}

	// only spawn touch controls on local player controllers
	if (SVirtualJoystick::ShouldDisplayTouchInterface() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogVHV, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AVHVPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!SVirtualJoystick::ShouldDisplayTouchInterface())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}

			if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
			{
				if (UIManagerComponent)
				{
					if (DialogueConfirmAction)
					{
						EnhancedInputComponent->BindAction(DialogueConfirmAction.Get(), ETriggerEvent::Started, UIManagerComponent.Get(), &UVHVUIManagerComponent::ConfirmChoiceInput);
					}
					if (DialogueChoiceUpAction)
					{
						EnhancedInputComponent->BindAction(DialogueChoiceUpAction.Get(), ETriggerEvent::Triggered, UIManagerComponent.Get(), &UVHVUIManagerComponent::SelectPreviousChoice);
					}
					if (DialogueChoiceDownAction)
					{
						EnhancedInputComponent->BindAction(DialogueChoiceDownAction.Get(), ETriggerEvent::Triggered, UIManagerComponent.Get(), &UVHVUIManagerComponent::SelectNextChoice);
					}
					if (DialogueExitAction)
					{
						EnhancedInputComponent->BindAction(DialogueExitAction.Get(), ETriggerEvent::Triggered, UIManagerComponent.Get(), &UVHVUIManagerComponent::ExitConversation);
					}
				}
			}
		}
	}
}
