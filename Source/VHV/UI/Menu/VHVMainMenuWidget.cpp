#include "UI/Menu/VHVMainMenuWidget.h"

#include "Components/AudioComponent.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Save/VHVSaveSubsystem.h"
#include "Styling/CoreStyle.h"
#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace
{
    constexpr float BackgroundFadeDuration = 0.45f;
    constexpr float MenuFadeDelay = 0.08f;
    constexpr float MenuFadeDuration = 0.32f;
    constexpr float MenuEntranceOffset = 14.0f;
    constexpr float SelectionDuration = 0.15f;
    constexpr float MenuRowHeight = 48.0f;
    constexpr float MenuRowGap = 36.0f;

    const FText MenuOptionLabels[] =
    {
        NSLOCTEXT("VHVMainMenu", "Play", "PLAY"),
        NSLOCTEXT("VHVMainMenu", "Continue", "CONTINUE"),
        NSLOCTEXT("VHVMainMenu", "Quit", "QUIT")
    };
}

UVHVMainMenuWidget::UVHVMainMenuWidget()
{
    SetIsFocusable(true);
    SelectionEmphasis.Init(0.0f, static_cast<int32>(EMenuOption::Count));
    ConfirmationEmphasis.Init(0.0f, 2);

    static ConstructorHelpers::FObjectFinder<UTexture2D> BackgroundFinder(
        TEXT("/Game/VHV_Stuff/UI/Backgrounds/T_BG_MainMenu_BetterChoices"));
    BackgroundTexture = BackgroundFinder.Object;
}

void UVHVMainMenuWidget::InitializeMenu(const FName InGameplayMapName)
{
    if (!InGameplayMapName.IsNone())
    {
        GameplayMapName = InGameplayMapName;
    }
}

TSharedRef<SWidget> UVHVMainMenuWidget::RebuildWidget()
{
    BackgroundBrush = FSlateBrush();
    BackgroundBrush.SetResourceObject(BackgroundTexture);
    BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
    if (BackgroundTexture)
    {
        BackgroundBrush.ImageSize = FVector2D(
            static_cast<float>(BackgroundTexture->GetSizeX()),
            static_cast<float>(BackgroundTexture->GetSizeY()));
    }

    AccentBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GoldPrimary(), 1.5f);
    ConfirmationPanelBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassMain().CopyWithNewOpacity(0.86f), 18.0f,
        VHVActivityUIStyle::PanelBorder(), VHVActivityUIStyle::BorderNormalWidth);

    const FButtonStyle& NoBorderButtonStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("NoBorder"));

    TSharedPtr<SVerticalBox> MenuStack;
    SAssignNew(MenuStack, SVerticalBox);
    MenuAccents.Empty();
    MenuLabels.Empty();
    MenuRows.Empty();

    for (int32 Index = 0; Index < static_cast<int32>(EMenuOption::Count); ++Index)
    {
        TSharedPtr<SBorder> Accent;
        TSharedPtr<STextBlock> Label;
        TSharedPtr<SWidget> Row;

        MenuStack->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f,
            Index + 1 < static_cast<int32>(EMenuOption::Count) ? MenuRowGap : 0.0f))
        [
            SAssignNew(Row, SBox)
            .HeightOverride(MenuRowHeight)
            [
                SNew(SButton)
                .ButtonStyle(&NoBorderButtonStyle)
                .ContentPadding(FMargin(0.0f))
                .ClickMethod(EButtonClickMethod::MouseDown)
                .OnHovered(FSimpleDelegate::CreateUObject(this, &UVHVMainMenuWidget::HandleMenuHovered, Index))
                .OnClicked(FOnClicked::CreateUObject(this, &UVHVMainMenuWidget::HandleMenuClicked, Index))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Fill)
                    .Padding(FMargin(0.0f, 0.0f, 25.0f, 0.0f))
                    [
                        SNew(SBox)
                        .WidthOverride(3.0f)
                        [
                            SAssignNew(Accent, SBorder)
                            .BorderImage(&AccentBrush)
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SAssignNew(Label, STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(30))
                        .Text(MenuOptionLabels[Index])
                    ]
                ]
            ]
        ];

        MenuAccents.Add(Accent);
        MenuLabels.Add(Label);
        MenuRows.Add(Row);
    }

    TSharedPtr<SHorizontalBox> ConfirmationActions;
    SAssignNew(ConfirmationActions, SHorizontalBox);
    ConfirmationLabels.Empty();
    const FText ConfirmationActionLabels[] =
    {
        NSLOCTEXT("VHVMainMenu", "StartNewGame", "START NEW GAME"),
        NSLOCTEXT("VHVMainMenu", "Cancel", "CANCEL")
    };

    for (int32 Index = 0; Index < 2; ++Index)
    {
        TSharedPtr<STextBlock> Label;
        ConfirmationActions->AddSlot()
        .AutoWidth()
        .Padding(FMargin(0.0f, 0.0f, Index == 0 ? 28.0f : 0.0f, 0.0f))
        [
            SNew(SButton)
            .ButtonStyle(&NoBorderButtonStyle)
            .ContentPadding(FMargin(0.0f))
            .ClickMethod(EButtonClickMethod::MouseDown)
            .OnHovered(FSimpleDelegate::CreateUObject(this, &UVHVMainMenuWidget::HandleConfirmationHovered, Index))
            .OnClicked(FOnClicked::CreateUObject(this, &UVHVMainMenuWidget::HandleConfirmationClicked, Index))
            [
                SAssignNew(Label, STextBlock)
                .Font(VHVActivityUIStyle::MediumFont(17))
                .Text(ConfirmationActionLabels[Index])
            ]
        ];
        ConfirmationLabels.Add(Label);
    }

    TSharedRef<SOverlay> Root =
        SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor::Black)
        ]
        + SOverlay::Slot()
        [
            SNew(SScaleBox)
            .Stretch(EStretch::ScaleToFill)
            .StretchDirection(EStretchDirection::Both)
            .Clipping(EWidgetClipping::ClipToBounds)
            [
                SAssignNew(BackgroundImage, SImage)
                .Image(&BackgroundBrush)
            ]
        ]
        + SOverlay::Slot()
        [
            SNew(SConstraintCanvas)
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.145f, 0.465f, 0.40f, 0.79f))
            .Offset(FMargin(0.0f))
            [
                SAssignNew(MenuPanel, SBox)
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Top)
                [
                    MenuStack.ToSharedRef()
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.135f, 0.47f, 0.46f, 0.72f))
            .Offset(FMargin(0.0f))
            [
                SAssignNew(ConfirmationPanel, SBorder)
                .BorderImage(&ConfirmationPanelBrush)
                .Padding(FMargin(30.0f, 25.0f, 32.0f, 26.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(23))
                        .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                        .Text(NSLOCTEXT("VHVMainMenu", "NewGamePrompt", "Start a new game?"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 10.0f, 0.0f, 24.0f))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(16))
                        .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                        .AutoWrapText(true)
                        .Text(NSLOCTEXT("VHVMainMenu", "NewGameWarning", "Existing progress will be replaced after you save."))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        ConfirmationActions.ToSharedRef()
                    ]
                ]
            ]
        ];

    if (ConfirmationPanel)
    {
        ConfirmationPanel->SetVisibility(bConfirmationVisible ? EVisibility::Visible : EVisibility::Collapsed);
    }
    return Root;
}

void UVHVMainMenuWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    MenuAccents.Empty();
    MenuLabels.Empty();
    MenuRows.Empty();
    ConfirmationLabels.Empty();
    BackgroundImage.Reset();
    MenuPanel.Reset();
    ConfirmationPanel.Reset();
}

void UVHVMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EntranceElapsed = 0.0f;
    RefreshSaveAvailability();
    SetSelectedOption(static_cast<int32>(EMenuOption::Play), false);
    if (BackgroundImage)
    {
        BackgroundImage->SetRenderOpacity(0.0f);
    }
    if (MenuPanel)
    {
        MenuPanel->SetRenderOpacity(0.0f);
        MenuPanel->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, MenuEntranceOffset)));
    }
    if (MenuAmbience)
    {
        MenuAmbienceComponent = UGameplayStatics::SpawnSound2D(this, MenuAmbience);
    }
}

void UVHVMainMenuWidget::NativeDestruct()
{
    if (MenuAmbienceComponent)
    {
        MenuAmbienceComponent->Stop();
        MenuAmbienceComponent = nullptr;
    }
    Super::NativeDestruct();
}

void UVHVMainMenuWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    EntranceElapsed += InDeltaTime;

    const float BackgroundAlpha = FMath::InterpEaseOut(0.0f, 1.0f,
        FMath::Clamp(EntranceElapsed / BackgroundFadeDuration, 0.0f, 1.0f), 3.0f);
    const float MenuAlpha = FMath::InterpEaseOut(0.0f, 1.0f,
        FMath::Clamp((EntranceElapsed - MenuFadeDelay) / MenuFadeDuration, 0.0f, 1.0f), 3.0f);
    if (BackgroundImage)
    {
        BackgroundImage->SetRenderOpacity(BackgroundAlpha);
    }
    if (MenuPanel)
    {
        MenuPanel->SetRenderOpacity(MenuAlpha);
        MenuPanel->SetRenderTransform(FSlateRenderTransform(
            FVector2D(0.0f, FMath::Lerp(MenuEntranceOffset, 0.0f, MenuAlpha))));
        MenuPanel->SetVisibility(bConfirmationVisible ? EVisibility::Collapsed : EVisibility::Visible);
    }

    const float InterpSpeed = 1.0f / SelectionDuration;
    for (int32 Index = 0; Index < SelectionEmphasis.Num(); ++Index)
    {
        const bool bEnabled = Index != static_cast<int32>(EMenuOption::Continue) || bContinueEnabled;
        const float Target = !bConfirmationVisible && bEnabled && Index == SelectedOption ? 1.0f : 0.0f;
        SelectionEmphasis[Index] = FMath::FInterpTo(SelectionEmphasis[Index], Target, InDeltaTime, InterpSpeed);
        const float Emphasis = SelectionEmphasis[Index];

        if (MenuAccents.IsValidIndex(Index) && MenuAccents[Index])
        {
            MenuAccents[Index]->SetRenderOpacity(Emphasis);
        }
        if (MenuLabels.IsValidIndex(Index) && MenuLabels[Index])
        {
            const FLinearColor Inactive = VHVActivityUIStyle::FromSRGB(166, 174, 187, 210);
            const FLinearColor Disabled = VHVActivityUIStyle::DisabledText().CopyWithNewOpacity(0.46f);
            MenuLabels[Index]->SetColorAndOpacity(bEnabled
                ? FMath::Lerp(Inactive, VHVActivityUIStyle::TextPrimary(), Emphasis)
                : Disabled);
        }
        if (MenuRows.IsValidIndex(Index) && MenuRows[Index])
        {
            MenuRows[Index]->SetRenderTransform(FSlateRenderTransform(
                FVector2D(FMath::Lerp(0.0f, 3.0f, Emphasis), 0.0f)));
        }
    }

    for (int32 Index = 0; Index < ConfirmationEmphasis.Num(); ++Index)
    {
        const float Target = bConfirmationVisible && Index == ConfirmationSelection ? 1.0f : 0.0f;
        ConfirmationEmphasis[Index] = FMath::FInterpTo(
            ConfirmationEmphasis[Index], Target, InDeltaTime, InterpSpeed);
        if (ConfirmationLabels.IsValidIndex(Index) && ConfirmationLabels[Index])
        {
            ConfirmationLabels[Index]->SetColorAndOpacity(FMath::Lerp(
                VHVActivityUIStyle::TextSecondary(), VHVActivityUIStyle::GoldSelected(),
                ConfirmationEmphasis[Index]));
        }
    }
}

FReply UVHVMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (bConfirmationVisible)
    {
        if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Up || Key == EKeys::W
            || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Up)
        {
            SetConfirmationSelection(0, true);
            return FReply::Handled();
        }
        if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Down || Key == EKeys::S
            || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_DPad_Down)
        {
            SetConfirmationSelection(1, true);
            return FReply::Handled();
        }
        if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
        {
            ConfirmationSelection == 0 ? StartNewGame() : HideNewGameConfirmation();
            return FReply::Handled();
        }
        if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
        {
            HideNewGameConfirmation();
            return FReply::Handled();
        }
        return FReply::Handled();
    }

    if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
    {
        MoveSelection(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
    {
        MoveSelection(1);
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        ActivateSelectedOption();
        return FReply::Handled();
    }
    if (Key == EKeys::Escape)
    {
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UVHVMainMenuWidget::RefreshSaveAvailability()
{
    const UGameInstance* GameInstance = GetGameInstance();
    const UVHVSaveSubsystem* SaveSubsystem = GameInstance
        ? GameInstance->GetSubsystem<UVHVSaveSubsystem>()
        : nullptr;
    bSaveSlotExists = SaveSubsystem && SaveSubsystem->DoesSaveExist();
    bContinueEnabled = SaveSubsystem && SaveSubsystem->CanLoadProgress();
    if (!bContinueEnabled && SelectedOption == static_cast<int32>(EMenuOption::Continue))
    {
        SelectedOption = static_cast<int32>(EMenuOption::Play);
    }
}

void UVHVMainMenuWidget::MoveSelection(const int32 Direction)
{
    int32 Candidate = SelectedOption;
    const int32 OptionCount = static_cast<int32>(EMenuOption::Count);
    do
    {
        Candidate = (Candidate + Direction + OptionCount) % OptionCount;
    }
    while (Candidate == static_cast<int32>(EMenuOption::Continue) && !bContinueEnabled);
    SetSelectedOption(Candidate, true);
}

void UVHVMainMenuWidget::SetSelectedOption(const int32 Index, const bool bPlaySound)
{
    if (Index < 0 || Index >= static_cast<int32>(EMenuOption::Count)
        || (Index == static_cast<int32>(EMenuOption::Continue) && !bContinueEnabled))
    {
        return;
    }
    if (SelectedOption != Index)
    {
        SelectedOption = Index;
        if (bPlaySound)
        {
            PlayFocusSound();
        }
    }
}

void UVHVMainMenuWidget::ActivateSelectedOption()
{
    ActivateOption(SelectedOption);
}

void UVHVMainMenuWidget::ActivateOption(const int32 Index)
{
    if (Index == static_cast<int32>(EMenuOption::Continue) && !bContinueEnabled)
    {
        return;
    }
    PlayConfirmSound();
    switch (static_cast<EMenuOption>(Index))
    {
    case EMenuOption::Play:
        if (bSaveSlotExists)
        {
            ShowNewGameConfirmation();
        }
        else
        {
            StartNewGame();
        }
        break;
    case EMenuOption::Continue:
        ContinueGame();
        break;
    case EMenuOption::Quit:
        QuitGame();
        break;
    default:
        break;
    }
}

void UVHVMainMenuWidget::ShowNewGameConfirmation()
{
    bConfirmationVisible = true;
    SetConfirmationSelection(0, false);
    if (ConfirmationPanel)
    {
        ConfirmationPanel->SetVisibility(EVisibility::Visible);
    }
}

void UVHVMainMenuWidget::HideNewGameConfirmation()
{
    bConfirmationVisible = false;
    if (ConfirmationPanel)
    {
        ConfirmationPanel->SetVisibility(EVisibility::Collapsed);
    }
}

void UVHVMainMenuWidget::SetConfirmationSelection(const int32 Index, const bool bPlaySound)
{
    if (Index < 0 || Index > 1)
    {
        return;
    }
    if (ConfirmationSelection != Index)
    {
        ConfirmationSelection = Index;
        if (bPlaySound)
        {
            PlayFocusSound();
        }
    }
}

void UVHVMainMenuWidget::StartNewGame()
{
    PrepareForTravel();
    UGameplayStatics::OpenLevel(this, GameplayMapName);
}

void UVHVMainMenuWidget::ContinueGame()
{
    UVHVSaveSubsystem* SaveSubsystem = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UVHVSaveSubsystem>()
        : nullptr;
    if (!SaveSubsystem || !SaveSubsystem->CanLoadProgress())
    {
        RefreshSaveAvailability();
        return;
    }
    PrepareForTravel();
    if (!SaveSubsystem->LoadProgressFromMainMenu())
    {
        RefreshSaveAvailability();
    }
}

void UVHVMainMenuWidget::QuitGame()
{
    UWorld* World = GetWorld();
#if WITH_EDITOR
    if (World && World->WorldType == EWorldType::PIE && GEditor)
    {
        GEditor->RequestEndPlayMap();
        return;
    }
#endif
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UVHVMainMenuWidget::PrepareForTravel()
{
    if (MenuAmbienceComponent)
    {
        MenuAmbienceComponent->Stop();
    }
    if (APlayerController* PlayerController = GetOwningPlayer())
    {
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = false;
        PlayerController->ResetIgnoreMoveInput();
        PlayerController->ResetIgnoreLookInput();
    }
    SetVisibility(ESlateVisibility::Collapsed);
}

void UVHVMainMenuWidget::PlayFocusSound() const
{
    if (FocusSound)
    {
        UGameplayStatics::PlaySound2D(this, FocusSound);
    }
}

void UVHVMainMenuWidget::PlayConfirmSound() const
{
    if (ConfirmSound)
    {
        UGameplayStatics::PlaySound2D(this, ConfirmSound);
    }
}

void UVHVMainMenuWidget::HandleMenuHovered(const int32 Index)
{
    SetSelectedOption(Index, true);
}

FReply UVHVMainMenuWidget::HandleMenuClicked(const int32 Index)
{
    SetSelectedOption(Index, false);
    ActivateOption(Index);
    return FReply::Handled();
}

void UVHVMainMenuWidget::HandleConfirmationHovered(const int32 Index)
{
    SetConfirmationSelection(Index, true);
}

FReply UVHVMainMenuWidget::HandleConfirmationClicked(const int32 Index)
{
    SetConfirmationSelection(Index, false);
    PlayConfirmSound();
    Index == 0 ? StartNewGame() : HideNewGameConfirmation();
    return FReply::Handled();
}
