#include "UI/Menu/VHVMainMenuGameMode.h"

#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/Menu/VHVMainMenuWidget.h"

AVHVMainMenuGameMode::AVHVMainMenuGameMode()
{
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
    PlayerControllerClass = APlayerController::StaticClass();
    MainMenuWidgetClass = UVHVMainMenuWidget::StaticClass();
    bStartPlayersAsSpectators = true;
}

void AVHVMainMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PlayerController || !MainMenuWidgetClass)
    {
        return;
    }

    MainMenuWidget = CreateWidget<UVHVMainMenuWidget>(PlayerController, MainMenuWidgetClass);
    if (!MainMenuWidget)
    {
        return;
    }

    MainMenuWidget->InitializeMenu(GameplayMapName);
    MainMenuWidget->AddToViewport(100);

    PlayerController->bShowMouseCursor = true;
    PlayerController->SetIgnoreMoveInput(true);
    PlayerController->SetIgnoreLookInput(true);
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
    PlayerController->SetInputMode(InputMode);
    RestoreMenuFocus();

    GetWorldTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateUObject(this, &AVHVMainMenuGameMode::RestoreMenuFocus));
}

void AVHVMainMenuGameMode::RestoreMenuFocus()
{
    APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (PlayerController && MainMenuWidget && MainMenuWidget->GetVisibility() != ESlateVisibility::Collapsed)
    {
        MainMenuWidget->SetIsFocusable(true);
        MainMenuWidget->SetUserFocus(PlayerController);
        MainMenuWidget->SetKeyboardFocus();
    }
}
