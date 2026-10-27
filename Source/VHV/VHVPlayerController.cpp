// Copyright Epic Games, Inc. All Rights Reserved.


#include "VHVPlayerController.h"

#include "Debug/VHVDeveloperSubsystem.h"
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
#include "Audio/VHVAudioSubsystem.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "UI/Menu/VHVFrontEndSubsystem.h"
#include "UI/Menu/VHVReturnToMenuWidget.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

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
	const bool bLoadTravelPending = SaveSubsystem && SaveSubsystem->IsLoadTravelPending();

	if (UVHVAudioSubsystem* Audio = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UVHVAudioSubsystem>() : nullptr)
	{
		Audio->PlayGameplayMusic();
	}

	bool bQuestFlowStarted = false;
	if (!bLoadTravelPending && bUseQuestFlow && !StartingQuestArc.IsNull() && World && World->GetGameInstance())
	{
		if (UVHVQuestArcData* QuestArc = StartingQuestArc.LoadSynchronous())
		{
			if (UVHVQuestSubsystem* QuestSubsystem = World->GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>())
			{
				FName TechniqueQuestID;
				UVHVFrontEndSubsystem* FrontEnd = World->GetGameInstance()->GetSubsystem<UVHVFrontEndSubsystem>();
				const EVHVFrontEndLaunchMode LaunchMode = FrontEnd
					? FrontEnd->ConsumeLaunchRequest(TechniqueQuestID)
					: EVHVFrontEndLaunchMode::None;
				if (LaunchMode == EVHVFrontEndLaunchMode::TechniquePractice)
				{
					bQuestFlowStarted = QuestSubsystem->StartTechniquePractice(QuestArc, TechniqueQuestID);
					if (bQuestFlowStarted)
					{
						GetWorldTimerManager().SetTimerForNextTick(
							FTimerDelegate::CreateUObject(this, &AVHVPlayerController::PositionPlayerForTechniqueStart));
					}
				}
				else
				{
					if (UVHVStoryStateSubsystem* StoryState = World->GetGameInstance()->GetSubsystem<UVHVStoryStateSubsystem>())
					{
						StoryState->ResetStoryState();
					}
					if (FrontEnd)
					{
						FrontEnd->SetTechniquePracticeActive(false);
					}
					bQuestFlowStarted = QuestSubsystem->StartQuestArc(QuestArc);
				}
			}
		}
	}

	if (!bLoadTravelPending && !bQuestFlowStarted)
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
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this,
		&AVHVPlayerController::ToggleReturnToMenuOverlay);

#if !UE_BUILD_SHIPPING
	// Developer-only shortcut ownership lives in UVHVDeveloperSubsystem.
	if (UVHVDeveloperSubsystem* DeveloperSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UVHVDeveloperSubsystem>()
		: nullptr)
	{
		DeveloperSubsystem->BindDeveloperInput(InputComponent);
	}
#endif

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
					if (LocateObjectiveAction)
					{
						EnhancedInputComponent->BindAction(
							LocateObjectiveAction.Get(), ETriggerEvent::Started,
							UIManagerComponent.Get(), &UVHVUIManagerComponent::ToggleObjectiveTracking);
					}
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

void AVHVPlayerController::ToggleReturnToMenuOverlay()
{
    if (ReturnToMenuWidget)
    {
        CloseReturnToMenuOverlay();
        return;
    }
    if (UIManagerComponent && UIManagerComponent->IsConversationSessionActive())
    {
        // Dialogue owns Escape until it reaches a stable checkpoint.
        return;
    }

    ReturnToMenuWidget = CreateWidget<UVHVReturnToMenuWidget>(this, UVHVReturnToMenuWidget::StaticClass());
    if (!ReturnToMenuWidget)
    {
        return;
    }
    ReturnToMenuWidget->AddToViewport(500);
    SetPause(true);
    bShowMouseCursor = true;
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetWidgetToFocus(ReturnToMenuWidget->TakeWidget());
    SetInputMode(InputMode);
    ReturnToMenuWidget->SetKeyboardFocus();
}

void AVHVPlayerController::CloseReturnToMenuOverlay()
{
    if (ReturnToMenuWidget)
    {
        ReturnToMenuWidget->RemoveFromParent();
        ReturnToMenuWidget = nullptr;
    }
    SetPause(false);
    bShowMouseCursor = false;
    SetInputMode(FInputModeGameOnly());
}

void AVHVPlayerController::ReturnToMainMenu(const bool bSaveFirst)
{
    if (bSaveFirst)
    {
        UVHVSaveSubsystem* SaveSubsystem = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UVHVSaveSubsystem>() : nullptr;
        if (!SaveSubsystem || !SaveSubsystem->SaveProgress())
        {
            if (ReturnToMenuWidget)
            {
                ReturnToMenuWidget->ShowSaveFailure();
            }
            return;
        }
    }
    if (UVHVAudioSubsystem* Audio = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UVHVAudioSubsystem>() : nullptr)
    {
        Audio->StopMusic(0.8f);
    }
    SetPause(false);
    UGameplayStatics::OpenLevel(this, TEXT("/Game/VHV_Stuff/UI/MainMenu/Maps/Lvl_MainMenu"));
}

void AVHVPlayerController::PositionPlayerForTechniqueStart()
{
    UVHVQuestSubsystem* QuestSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>() : nullptr;
    APawn* PlayerPawn = GetPawn();
    if (!QuestSubsystem || !PlayerPawn || !GetWorld())
    {
        return;
    }

    FName QuestID;
    FName ObjectiveID;
    FName LocationID;
    FVector TargetLocation;
    AActor* TargetActor = nullptr;
    if (!QuestSubsystem->ResolveCurrentObjectiveTrackingTarget(
        GetWorld(), QuestID, ObjectiveID, LocationID, TargetLocation, TargetActor))
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVFrontEnd] Technique '%s' has no resolvable starting destination; using PlayerStart."),
            *QuestID.ToString());
        return;
    }

    FVector Away = PlayerPawn->GetActorLocation() - TargetLocation;
    Away.Z = 0.0f;
    if (!Away.Normalize())
    {
        Away = FVector::ForwardVector;
    }
    FVector Destination = TargetLocation + Away * 500.0f;
    FNavLocation Projected;
    if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        Nav && Nav->ProjectPointToNavigation(Destination, Projected, FVector(500.0f, 500.0f, 400.0f)))
    {
        Destination = Projected.Location;
    }
    const FRotator Facing = (TargetLocation - Destination).Rotation();
    PlayerPawn->TeleportTo(Destination, FRotator(0.0f, Facing.Yaw, 0.0f), false, true);
    SetControlRotation(FRotator(Facing.Pitch, Facing.Yaw, 0.0f));
}
