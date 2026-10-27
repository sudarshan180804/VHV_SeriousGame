#include "UI/Menu/VHVMainMenuWidget.h"

#include "Audio/VHVAudioSubsystem.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Quest/Data/VHVQuestArcData.h"
#include "Save/VHVSaveSubsystem.h"
#include "Styling/CoreStyle.h"
#include "UI/Menu/VHVFrontEndSubsystem.h"
#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace
{
    constexpr float FadeDuration = 0.4f;
    constexpr float SelectionSpeed = 7.0f;
    const FText MainLabels[] = {
        NSLOCTEXT("VHVMainMenu", "NewGame", "NEW GAME"),
        NSLOCTEXT("VHVMainMenu", "Continue", "CONTINUE GAME"),
        NSLOCTEXT("VHVMainMenu", "Techniques", "TECHNIQUES"),
        NSLOCTEXT("VHVMainMenu", "Tutorial", "TUTORIAL"),
        NSLOCTEXT("VHVMainMenu", "Settings", "SETTINGS"),
        NSLOCTEXT("VHVMainMenu", "Quit", "QUIT GAME")
    };
}

UVHVMainMenuWidget::UVHVMainMenuWidget()
{
    SetIsFocusable(true);
    SelectionEmphasis.Init(0.0f, static_cast<int32>(EMenuOption::Count));
    ConfirmationEmphasis.Init(0.0f, 2);
    static ConstructorHelpers::FObjectFinder<UTexture2D> Finder(
        TEXT("/Game/VHV_Stuff/UI/Backgrounds/T_BG_MainMenu_BetterChoices"));
    BackgroundTexture = Finder.Object;
    NavigationTutorialImage = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
        TEXT("/Game/VHV_Stuff/UI/Tutorial/T_Tutorial_Navigation.T_Tutorial_Navigation")));
    InteractionTutorialImage = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
        TEXT("/Game/VHV_Stuff/UI/Tutorial/T_Tutorial_Interaction.T_Tutorial_Interaction")));
}

void UVHVMainMenuWidget::InitializeMenu(const FName InGameplayMapName)
{
    if (!InGameplayMapName.IsNone()) GameplayMapName = InGameplayMapName;
}

TSharedRef<SWidget> UVHVMainMenuWidget::RebuildWidget()
{
    BackgroundBrush = FSlateBrush();
    BackgroundBrush.SetResourceObject(BackgroundTexture);
    BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
    if (BackgroundTexture)
    {
        BackgroundBrush.ImageSize = FVector2D(BackgroundTexture->GetSizeX(), BackgroundTexture->GetSizeY());
    }
    AccentBrush = VHVActivityUIStyle::RoundedBrush(VHVActivityUIStyle::GoldPrimary(), 1.5f);
    PanelBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassMain().CopyWithNewOpacity(0.88f), 18.0f,
        VHVActivityUIStyle::PanelBorder(), 1.0f);
    PlaceholderBrush = VHVActivityUIStyle::RoundedBrush(
        FLinearColor(0.035f, 0.045f, 0.06f, 0.78f), 12.0f,
        VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.38f), 1.0f);
    const FButtonStyle& NoBorder = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("NoBorder"));

    TSharedPtr<SVerticalBox> MainStack;
    SAssignNew(MainStack, SVerticalBox);
    MenuAccents.Empty(); MenuLabels.Empty(); MenuRows.Empty();
    for (int32 Index = 0; Index < static_cast<int32>(EMenuOption::Count); ++Index)
    {
        TSharedPtr<SBorder> Accent;
        TSharedPtr<STextBlock> Label;
        TSharedPtr<SWidget> Row;
        MainStack->AddSlot().AutoHeight().Padding(0.0f, 5.0f)
        [
            SAssignNew(Row, SBox).HeightOverride(42.0f)
            [
                SNew(SButton).ButtonStyle(&NoBorder).ContentPadding(0.0f)
                .OnHovered(FSimpleDelegate::CreateUObject(this, &UVHVMainMenuWidget::HandleMenuHovered, Index))
                .OnClicked(FOnClicked::CreateUObject(this, &UVHVMainMenuWidget::HandleMenuClicked, Index))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 18.0f, 0.0f)
                    [ SNew(SBox).WidthOverride(3.0f)[SAssignNew(Accent, SBorder).BorderImage(&AccentBrush)] ]
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                    [ SAssignNew(Label, STextBlock).Font(VHVActivityUIStyle::MediumFont(25)).Text(MainLabels[Index]) ]
                ]
            ]
        ];
        MenuAccents.Add(Accent); MenuLabels.Add(Label); MenuRows.Add(Row);
    }

    TSharedPtr<SHorizontalBox> ConfirmActions;
    SAssignNew(ConfirmActions, SHorizontalBox);
    ConfirmationLabels.Empty();
    const FText ConfirmText[] = {
        NSLOCTEXT("VHVMainMenu", "ConfirmNew", "START NEW GAME"),
        NSLOCTEXT("VHVMainMenu", "Cancel", "CANCEL") };
    for (int32 Index = 0; Index < 2; ++Index)
    {
        TSharedPtr<STextBlock> Label;
        ConfirmActions->AddSlot().AutoWidth().Padding(0.0f, 0.0f, Index == 0 ? 28.0f : 0.0f, 0.0f)
        [
            SNew(SButton).ButtonStyle(&NoBorder)
            .OnHovered(FSimpleDelegate::CreateUObject(this, &UVHVMainMenuWidget::HandleConfirmationHovered, Index))
            .OnClicked(FOnClicked::CreateUObject(this, &UVHVMainMenuWidget::HandleConfirmationClicked, Index))
            [ SAssignNew(Label, STextBlock).Font(VHVActivityUIStyle::MediumFont(16)).Text(ConfirmText[Index]) ]
        ];
        ConfirmationLabels.Add(Label);
    }

    TSharedPtr<SVerticalBox> TechniqueStack;
    SAssignNew(TechniqueStack, SVerticalBox);
    TechniqueQuestIDs.Empty(); TechniqueLabels.Empty();
    if (UVHVQuestArcData* Arc = LoadObject<UVHVQuestArcData>(nullptr,
        TEXT("/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT.DA_QuestArc_HBCT")))
    {
        for (int32 QuestIndex = 1; QuestIndex < Arc->Quests.Num(); ++QuestIndex)
        {
            const int32 TechniqueIndex = TechniqueQuestIDs.Num();
            const FVHVQuestDefinition& Quest = Arc->Quests[QuestIndex];
            TechniqueQuestIDs.Add(Quest.QuestID);
            TSharedPtr<STextBlock> Label;
            TechniqueStack->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
            [
                SNew(SButton).ButtonStyle(&NoBorder).ContentPadding(FMargin(12.0f, 8.0f))
                .OnHovered(FSimpleDelegate::CreateUObject(this, &UVHVMainMenuWidget::SetTechniqueSelection, TechniqueIndex))
                .OnClicked(FOnClicked::CreateUObject(this, &UVHVMainMenuWidget::HandleTechniqueClicked, TechniqueIndex))
                [
                    SAssignNew(Label, STextBlock).AutoWrapText(true)
                    .Font(VHVActivityUIStyle::MediumFont(17))
                    .Text(Quest.QuestTitle)
                ]
            ];
            TechniqueLabels.Add(Label);
        }
    }
    TechniqueStack->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
    [
        SNew(SButton).ButtonStyle(&NoBorder)
        .OnClicked(FOnClicked::CreateUObject(this, &UVHVMainMenuWidget::HandleBackClicked))
        [ SNew(STextBlock).Font(VHVActivityUIStyle::MediumFont(15)).ColorAndOpacity(VHVActivityUIStyle::GoldPrimary()).Text(NSLOCTEXT("VHVMainMenu", "Back", "← BACK")) ]
    ];

    SAssignNew(TutorialPanel, SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 24.0f))
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [ SAssignNew(TutorialTitle, STextBlock).Font(VHVActivityUIStyle::MediumFont(25)).ColorAndOpacity(VHVActivityUIStyle::GoldSelected()) ]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 16.0f, 0.0f, 14.0f)
        [
            SNew(SBorder).BorderImage(&PlaceholderBrush).HAlign(HAlign_Fill).VAlign(VAlign_Fill)
            [
                SNew(SOverlay)
                + SOverlay::Slot()[SAssignNew(TutorialImage, SImage).Image(&TutorialImageBrush)]
                + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
                [ SAssignNew(TutorialImageLabel, STextBlock).Font(VHVActivityUIStyle::RegularFont(17)).ColorAndOpacity(VHVActivityUIStyle::TextSecondary()).Justification(ETextJustify::Center) ]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 16.0f)
        [ SAssignNew(TutorialBody, STextBlock).AutoWrapText(true).Font(VHVActivityUIStyle::RegularFont(16)).ColorAndOpacity(VHVActivityUIStyle::TextPrimary()) ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [ SNew(SButton).ButtonStyle(&NoBorder).OnClicked(FOnClicked::CreateUObject(this, &UVHVMainMenuWidget::HandleTutorialPrevious))[SNew(STextBlock).Font(VHVActivityUIStyle::MediumFont(15)).Text(NSLOCTEXT("VHVMainMenu", "Previous", "PREVIOUS"))] ]
            + SHorizontalBox::Slot().FillWidth(1.0f).HAlign(HAlign_Center)
            [ SNew(SButton).ButtonStyle(&NoBorder).OnClicked(FOnClicked::CreateUObject(this, &UVHVMainMenuWidget::HandleTutorialSkip))[SNew(STextBlock).Font(VHVActivityUIStyle::RegularFont(14)).ColorAndOpacity(VHVActivityUIStyle::TextSecondary()).Text(NSLOCTEXT("VHVMainMenu", "Skip", "SKIP / CLOSE"))] ]
            + SHorizontalBox::Slot().AutoWidth()
            [ SNew(SButton).ButtonStyle(&NoBorder).OnClicked(FOnClicked::CreateUObject(this, &UVHVMainMenuWidget::HandleTutorialNext))[SAssignNew(TutorialNextLabel, STextBlock).Font(VHVActivityUIStyle::MediumFont(15)).ColorAndOpacity(VHVActivityUIStyle::GoldSelected())] ]
        ]
    ];

    SAssignNew(SettingsPanel, SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 24.0f))
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [ SNew(STextBlock).Font(VHVActivityUIStyle::MediumFont(25)).ColorAndOpacity(VHVActivityUIStyle::GoldSelected()).Text(NSLOCTEXT("VHVMainMenu", "SettingsTitle", "SETTINGS")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 22.0f, 0.0f, 0.0f)
        [ SNew(STextBlock).Font(VHVActivityUIStyle::RegularFont(14)).ColorAndOpacity(VHVActivityUIStyle::TextSecondary()).Text(NSLOCTEXT("VHVMainMenu", "Music", "BACKGROUND MUSIC VOLUME")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 18.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [ SAssignNew(MusicSlider, SSlider).OnValueChanged(FOnFloatValueChanged::CreateUObject(this, &UVHVMainMenuWidget::HandleMusicVolumeChanged)) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(16.0f, 0.0f)
            [ SAssignNew(MusicVolumeValue, STextBlock).Font(VHVActivityUIStyle::MediumFont(15)).ColorAndOpacity(VHVActivityUIStyle::TextPrimary()) ]
        ]
        + SVerticalBox::Slot().AutoHeight()
        [ SNew(STextBlock).Font(VHVActivityUIStyle::RegularFont(14)).ColorAndOpacity(VHVActivityUIStyle::TextSecondary()).Text(NSLOCTEXT("VHVMainMenu", "ShowTutorial", "SHOW TUTORIAL ON NEW GAME")) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 20.0f)
        [ SNew(SButton).ButtonStyle(&NoBorder).OnClicked(FOnClicked::CreateUObject(this, &UVHVMainMenuWidget::HandleTutorialPreferenceClicked))[SAssignNew(TutorialPreferenceValue, STextBlock).Font(VHVActivityUIStyle::MediumFont(16)).ColorAndOpacity(VHVActivityUIStyle::GoldSelected())] ]
        + SVerticalBox::Slot().AutoHeight()
        [ SNew(SButton).ButtonStyle(&NoBorder).OnClicked(FOnClicked::CreateUObject(this, &UVHVMainMenuWidget::HandleBackClicked))[SNew(STextBlock).Font(VHVActivityUIStyle::MediumFont(15)).ColorAndOpacity(VHVActivityUIStyle::GoldPrimary()).Text(NSLOCTEXT("VHVMainMenu", "SettingsBack", "BACK"))] ]
    ];

    TSharedRef<SOverlay> Root = SNew(SOverlay)
        + SOverlay::Slot()[SNew(SBorder).BorderBackgroundColor(FLinearColor::Black)]
        + SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFill).StretchDirection(EStretchDirection::Both).Clipping(EWidgetClipping::ClipToBounds)[SAssignNew(BackgroundImage, SImage).Image(&BackgroundBrush)]]
        + SOverlay::Slot()
        [
            SNew(SConstraintCanvas)
            + SConstraintCanvas::Slot().Anchors(FAnchors(0.12f, 0.27f, 0.43f, 0.88f)).Offset(FMargin(0.0f))
            [ SAssignNew(MenuPanel, SBox).HAlign(HAlign_Fill).VAlign(VAlign_Center)[MainStack.ToSharedRef()] ]
            + SConstraintCanvas::Slot().Anchors(FAnchors(0.135f, 0.42f, 0.49f, 0.72f)).Offset(FMargin(0.0f))
            [
                SAssignNew(ConfirmationPanel, SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 25.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(VHVActivityUIStyle::MediumFont(23)).ColorAndOpacity(VHVActivityUIStyle::TextPrimary()).Text(NSLOCTEXT("VHVMainMenu", "Prompt", "Start a new game?"))]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 24.0f)[SNew(STextBlock).AutoWrapText(true).Font(VHVActivityUIStyle::RegularFont(15)).ColorAndOpacity(VHVActivityUIStyle::TextSecondary()).Text(NSLOCTEXT("VHVMainMenu", "Warning", "Your existing campaign remains available until the new session is saved."))]
                    + SVerticalBox::Slot().AutoHeight()[ConfirmActions.ToSharedRef()]
                ]
            ]
            + SConstraintCanvas::Slot().Anchors(FAnchors(0.18f, 0.16f, 0.72f, 0.86f)).Offset(FMargin(0.0f))
            [
                SAssignNew(TechniquesPanel, SBorder).BorderImage(&PanelBrush).Padding(FMargin(30.0f, 24.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[SNew(STextBlock).Font(VHVActivityUIStyle::MediumFont(25)).ColorAndOpacity(VHVActivityUIStyle::GoldSelected()).Text(NSLOCTEXT("VHVMainMenu", "Practice", "PRACTICE A TECHNIQUE"))]
                    + SVerticalBox::Slot().FillHeight(1.0f)[SNew(SScrollBox)+SScrollBox::Slot()[TechniqueStack.ToSharedRef()]]
                ]
            ]
            + SConstraintCanvas::Slot().Anchors(FAnchors(0.18f, 0.10f, 0.82f, 0.90f)).Offset(FMargin(0.0f))[TutorialPanel.ToSharedRef()]
            + SConstraintCanvas::Slot().Anchors(FAnchors(0.20f, 0.18f, 0.76f, 0.84f)).Offset(FMargin(0.0f))[SettingsPanel.ToSharedRef()]
        ];

    ShowPage(CurrentPage);
    if (ConfirmationPanel) ConfirmationPanel->SetVisibility(bConfirmationVisible ? EVisibility::Visible : EVisibility::Collapsed);
    SetTutorialPage(TutorialPageIndex);
    RefreshSettingsLabels();
    return Root;
}

void UVHVMainMenuWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    MenuAccents.Empty(); MenuLabels.Empty(); MenuRows.Empty(); ConfirmationLabels.Empty(); TechniqueLabels.Empty();
    BackgroundImage.Reset(); MenuPanel.Reset(); ConfirmationPanel.Reset(); TechniquesPanel.Reset(); TutorialPanel.Reset(); SettingsPanel.Reset();
    TutorialTitle.Reset(); TutorialBody.Reset(); TutorialImageLabel.Reset(); TutorialNextLabel.Reset();
    TutorialImage.Reset(); ActiveTutorialTexture = nullptr;
    MusicVolumeValue.Reset(); TutorialPreferenceValue.Reset(); MusicSlider.Reset();
}

void UVHVMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EntranceElapsed = 0.0f;
    RefreshSaveAvailability();
    SetSelectedOption(0, false);
    ShowPage(EPage::Main);
    if (BackgroundImage) BackgroundImage->SetRenderOpacity(0.0f);
    if (MenuPanel)
    {
        MenuPanel->SetRenderOpacity(0.0f);
        MenuPanel->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, 14.0f)));
    }
}

void UVHVMainMenuWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    EntranceElapsed += InDeltaTime;
    const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(EntranceElapsed / FadeDuration, 0.0f, 1.0f), 3.0f);
    if (BackgroundImage) BackgroundImage->SetRenderOpacity(Alpha);
    if (MenuPanel)
    {
        MenuPanel->SetRenderOpacity(Alpha);
        MenuPanel->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, FMath::Lerp(14.0f, 0.0f, Alpha))));
    }
    for (int32 Index = 0; Index < SelectionEmphasis.Num(); ++Index)
    {
        const bool bEnabled = Index != static_cast<int32>(EMenuOption::Continue) || bContinueEnabled;
        const float Target = CurrentPage == EPage::Main && !bConfirmationVisible && bEnabled && Index == SelectedOption ? 1.0f : 0.0f;
        SelectionEmphasis[Index] = FMath::FInterpTo(SelectionEmphasis[Index], Target, InDeltaTime, SelectionSpeed);
        if (MenuAccents.IsValidIndex(Index) && MenuAccents[Index]) MenuAccents[Index]->SetRenderOpacity(SelectionEmphasis[Index]);
        if (MenuLabels.IsValidIndex(Index) && MenuLabels[Index])
        {
            const FLinearColor Inactive = VHVActivityUIStyle::FromSRGB(166, 174, 187, 210);
            MenuLabels[Index]->SetColorAndOpacity(bEnabled ? FMath::Lerp(Inactive, VHVActivityUIStyle::TextPrimary(), SelectionEmphasis[Index]) : VHVActivityUIStyle::DisabledText().CopyWithNewOpacity(0.46f));
        }
    }
    for (int32 Index = 0; Index < ConfirmationEmphasis.Num(); ++Index)
    {
        ConfirmationEmphasis[Index] = FMath::FInterpTo(ConfirmationEmphasis[Index], bConfirmationVisible && Index == ConfirmationSelection ? 1.0f : 0.0f, InDeltaTime, SelectionSpeed);
        if (ConfirmationLabels.IsValidIndex(Index) && ConfirmationLabels[Index]) ConfirmationLabels[Index]->SetColorAndOpacity(FMath::Lerp(VHVActivityUIStyle::TextSecondary(), VHVActivityUIStyle::GoldSelected(), ConfirmationEmphasis[Index]));
    }
}

FReply UVHVMainMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    const bool bAccept = Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom;
    if (bConfirmationVisible)
    {
        if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_DPad_Up) SetConfirmationSelection(0, true);
        else if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_DPad_Down) SetConfirmationSelection(1, true);
        else if (bAccept) ConfirmationSelection == 0 ? StartNewGame() : HideNewGameConfirmation();
        else if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right) HideNewGameConfirmation();
        return FReply::Handled();
    }
    if (CurrentPage == EPage::Main)
    {
        if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up) MoveSelection(-1);
        else if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down) MoveSelection(1);
        else if (bAccept) ActivateSelectedOption();
        return FReply::Handled();
    }
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
    {
        if (CurrentPage == EPage::Tutorial && bTutorialLaunchingNewGame) CloseTutorial(false); else ShowPage(EPage::Main);
        return FReply::Handled();
    }
    if (CurrentPage == EPage::Techniques)
    {
        if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up) SetTechniqueSelection(TechniqueSelection - 1);
        else if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down) SetTechniqueSelection(TechniqueSelection + 1);
        else if (bAccept) StartSelectedTechnique();
        return FReply::Handled();
    }
    if (CurrentPage == EPage::Tutorial)
    {
        if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left) SetTutorialPage(TutorialPageIndex - 1);
        else if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || bAccept) TutorialPageIndex == 0 ? SetTutorialPage(1) : CloseTutorial(bTutorialLaunchingNewGame);
        return FReply::Handled();
    }
    if (CurrentPage == EPage::Settings)
    {
        const bool bUp = Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up;
        const bool bDown = Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down;
        const bool bLeft = Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left;
        const bool bRight = Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right;
        if (bUp) SettingsSelection = (SettingsSelection + 2) % 3;
        else if (bDown) SettingsSelection = (SettingsSelection + 1) % 3;
        else if ((bLeft || bRight) && SettingsSelection == 0)
        {
            UVHVAudioSubsystem* Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVAudioSubsystem>() : nullptr;
            if (Audio) HandleMusicVolumeChanged(FMath::Clamp(Audio->GetMusicVolume() + (bRight ? 0.05f : -0.05f), 0.0f, 1.0f));
        }
        else if (bAccept && SettingsSelection == 1) ToggleTutorialPreference();
        else if (bAccept && SettingsSelection == 2) ShowPage(EPage::Main);
        return FReply::Handled();
    }
    return FReply::Handled();
}

void UVHVMainMenuWidget::RefreshSaveAvailability()
{
    const UVHVSaveSubsystem* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVSaveSubsystem>() : nullptr;
    bSaveSlotExists = Save && Save->DoesSaveExist();
    bContinueEnabled = Save && Save->CanLoadProgress();
    const int32 ContinueIndex = static_cast<int32>(EMenuOption::Continue);
    if (MenuLabels.IsValidIndex(ContinueIndex) && MenuLabels[ContinueIndex])
    {
        MenuLabels[ContinueIndex]->SetText(bContinueEnabled
            ? MainLabels[ContinueIndex]
            : NSLOCTEXT("VHVMainMenu", "ContinueUnavailable", "CONTINUE GAME  —  NO VALID SAVE"));
    }
    if (!bContinueEnabled && SelectedOption == static_cast<int32>(EMenuOption::Continue)) SelectedOption = 0;
}

void UVHVMainMenuWidget::ShowPage(const EPage Page)
{
    CurrentPage = Page;
    if (MenuPanel) MenuPanel->SetVisibility(Page == EPage::Main && !bConfirmationVisible ? EVisibility::Visible : EVisibility::Collapsed);
    if (TechniquesPanel) TechniquesPanel->SetVisibility(Page == EPage::Techniques ? EVisibility::Visible : EVisibility::Collapsed);
    if (TutorialPanel) TutorialPanel->SetVisibility(Page == EPage::Tutorial ? EVisibility::Visible : EVisibility::Collapsed);
    if (SettingsPanel) SettingsPanel->SetVisibility(Page == EPage::Settings ? EVisibility::Visible : EVisibility::Collapsed);
    if (Page == EPage::Techniques) SetTechniqueSelection(TechniqueSelection);
    if (Page == EPage::Settings) RefreshSettingsLabels();
}

void UVHVMainMenuWidget::MoveSelection(const int32 Direction)
{
    int32 Candidate = SelectedOption;
    const int32 Count = static_cast<int32>(EMenuOption::Count);
    do Candidate = (Candidate + Direction + Count) % Count;
    while (Candidate == static_cast<int32>(EMenuOption::Continue) && !bContinueEnabled);
    SetSelectedOption(Candidate, true);
}

void UVHVMainMenuWidget::SetSelectedOption(const int32 Index, const bool bPlaySound)
{
    if (Index < 0 || Index >= static_cast<int32>(EMenuOption::Count) || (Index == static_cast<int32>(EMenuOption::Continue) && !bContinueEnabled)) return;
    if (SelectedOption != Index) { SelectedOption = Index; if (bPlaySound) PlayFocusSound(); }
}

void UVHVMainMenuWidget::ActivateSelectedOption() { ActivateOption(SelectedOption); }

void UVHVMainMenuWidget::ActivateOption(const int32 Index)
{
    if (Index == static_cast<int32>(EMenuOption::Continue) && !bContinueEnabled) return;
    PlayConfirmSound();
    switch (static_cast<EMenuOption>(Index))
    {
    case EMenuOption::NewGame: bSaveSlotExists ? ShowNewGameConfirmation() : StartNewGame(); break;
    case EMenuOption::Continue: ContinueGame(); break;
    case EMenuOption::Techniques: ShowPage(EPage::Techniques); break;
    case EMenuOption::Tutorial: bTutorialLaunchingNewGame = false; SetTutorialPage(0); ShowPage(EPage::Tutorial); break;
    case EMenuOption::Settings: ShowPage(EPage::Settings); break;
    case EMenuOption::Quit: QuitGame(); break;
    default: break;
    }
}

void UVHVMainMenuWidget::ShowNewGameConfirmation()
{
    bConfirmationVisible = true; SetConfirmationSelection(0, false);
    if (ConfirmationPanel) ConfirmationPanel->SetVisibility(EVisibility::Visible);
    if (MenuPanel) MenuPanel->SetVisibility(EVisibility::Collapsed);
}

void UVHVMainMenuWidget::HideNewGameConfirmation()
{
    bConfirmationVisible = false;
    if (ConfirmationPanel) ConfirmationPanel->SetVisibility(EVisibility::Collapsed);
    ShowPage(EPage::Main);
    if (UVHVAudioSubsystem* Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVAudioSubsystem>() : nullptr) Audio->PlayEvent(TEXT("UI_BackCancel"));
}

void UVHVMainMenuWidget::SetConfirmationSelection(const int32 Index, const bool bPlaySound)
{
    if (Index < 0 || Index > 1) return;
    if (ConfirmationSelection != Index) { ConfirmationSelection = Index; if (bPlaySound) PlayFocusSound(); }
}

void UVHVMainMenuWidget::StartNewGame()
{
    bConfirmationVisible = false;
    if (ConfirmationPanel) ConfirmationPanel->SetVisibility(EVisibility::Collapsed);
    const UVHVFrontEndSubsystem* FrontEnd = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVFrontEndSubsystem>() : nullptr;
    if (FrontEnd && FrontEnd->ShouldShowTutorialOnNewGame())
    {
        bTutorialLaunchingNewGame = true; SetTutorialPage(0); ShowPage(EPage::Tutorial); return;
    }
    BeginNewGameTravel();
}

void UVHVMainMenuWidget::BeginNewGameTravel()
{
    if (UVHVFrontEndSubsystem* FrontEnd = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVFrontEndSubsystem>() : nullptr) FrontEnd->PrepareNewGame();
    PrepareForTravel();
    UGameplayStatics::OpenLevel(this, GameplayMapName);
}

void UVHVMainMenuWidget::ContinueGame()
{
    UVHVSaveSubsystem* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVSaveSubsystem>() : nullptr;
    if (!Save || !Save->CanLoadProgress()) { RefreshSaveAvailability(); return; }
    if (UVHVFrontEndSubsystem* FrontEnd = GetGameInstance()->GetSubsystem<UVHVFrontEndSubsystem>()) FrontEnd->SetTechniquePracticeActive(false);
    PrepareForTravel();
    if (!Save->LoadProgressFromMainMenu()) { SetVisibility(ESlateVisibility::Visible); RefreshSaveAvailability(); }
}

void UVHVMainMenuWidget::QuitGame()
{
    UWorld* World = GetWorld();
#if WITH_EDITOR
    if (World && World->WorldType == EWorldType::PIE && GEditor) { GEditor->RequestEndPlayMap(); return; }
#endif
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UVHVMainMenuWidget::PrepareForTravel()
{
    if (UVHVAudioSubsystem* Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVAudioSubsystem>() : nullptr) Audio->StopMusic(0.8f);
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor = false;
        PC->ResetIgnoreMoveInput(); PC->ResetIgnoreLookInput();
    }
    SetVisibility(ESlateVisibility::Collapsed);
}

void UVHVMainMenuWidget::SetTechniqueSelection(const int32 Index)
{
    if (TechniqueQuestIDs.IsEmpty()) return;
    TechniqueSelection = FMath::Clamp(Index, 0, TechniqueQuestIDs.Num() - 1);
    for (int32 LabelIndex = 0; LabelIndex < TechniqueLabels.Num(); ++LabelIndex)
    {
        if (TechniqueLabels[LabelIndex]) TechniqueLabels[LabelIndex]->SetColorAndOpacity(LabelIndex == TechniqueSelection ? VHVActivityUIStyle::GoldSelected() : VHVActivityUIStyle::TextPrimary());
    }
}

void UVHVMainMenuWidget::StartSelectedTechnique()
{
    if (!TechniqueQuestIDs.IsValidIndex(TechniqueSelection)) return;
    if (UVHVFrontEndSubsystem* FrontEnd = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVFrontEndSubsystem>() : nullptr)
    {
        if (FrontEnd->PrepareTechniquePractice(TechniqueQuestIDs[TechniqueSelection]))
        {
            PrepareForTravel(); UGameplayStatics::OpenLevel(this, GameplayMapName);
        }
    }
}

void UVHVMainMenuWidget::SetTutorialPage(const int32 PageIndex)
{
    TutorialPageIndex = FMath::Clamp(PageIndex, 0, 1);
    ActiveTutorialTexture = TutorialPageIndex == 0
        ? NavigationTutorialImage.LoadSynchronous()
        : InteractionTutorialImage.LoadSynchronous();
    TutorialImageBrush = FSlateBrush();
    TutorialImageBrush.DrawAs = ESlateBrushDrawType::Image;
    TutorialImageBrush.SetResourceObject(ActiveTutorialTexture);
    if (ActiveTutorialTexture)
    {
        TutorialImageBrush.ImageSize = FVector2D(
            ActiveTutorialTexture->GetSizeX(), ActiveTutorialTexture->GetSizeY());
    }
    if (TutorialImage)
    {
        TutorialImage->SetImage(&TutorialImageBrush);
        TutorialImage->SetVisibility(ActiveTutorialTexture ? EVisibility::Visible : EVisibility::Collapsed);
    }
    if (TutorialImageLabel)
    {
        TutorialImageLabel->SetVisibility(ActiveTutorialTexture ? EVisibility::Collapsed : EVisibility::Visible);
    }
    if (TutorialPageIndex == 0)
    {
        if (TutorialTitle) TutorialTitle->SetText(NSLOCTEXT("VHVMainMenu", "NavTitle", "1 / 2  —  NAVIGATION"));
        if (TutorialImageLabel) TutorialImageLabel->SetText(NSLOCTEXT("VHVMainMenu", "NavPlaceholder", "NAVIGATION IMAGE PLACEHOLDER\nReplace with a Texture2D screenshot"));
        if (TutorialBody) TutorialBody->SetText(NSLOCTEXT("VHVMainMenu", "NavBody", "Move through the village with the existing movement controls. Press X to toggle guidance: a golden destination beacon and ground arrows show the route to your current objective."));
        if (TutorialNextLabel) TutorialNextLabel->SetText(NSLOCTEXT("VHVMainMenu", "Next", "NEXT →"));
    }
    else
    {
        if (TutorialTitle) TutorialTitle->SetText(NSLOCTEXT("VHVMainMenu", "InteractTitle", "2 / 2  —  INTERACTION"));
        if (TutorialImageLabel) TutorialImageLabel->SetText(NSLOCTEXT("VHVMainMenu", "InteractPlaceholder", "INTERACTION IMAGE PLACEHOLDER\nReplace with a Texture2D screenshot"));
        if (TutorialBody) TutorialBody->SetText(NSLOCTEXT("VHVMainMenu", "InteractBody", "Approach a highlighted NPC or object and press E (or the displayed controller interaction button). Read each conversation, choose responses with mouse, keyboard, or controller, and confirm to advance."));
        if (TutorialNextLabel) TutorialNextLabel->SetText(bTutorialLaunchingNewGame ? NSLOCTEXT("VHVMainMenu", "Start", "START GAME") : NSLOCTEXT("VHVMainMenu", "Close", "CLOSE"));
    }
}

void UVHVMainMenuWidget::CloseTutorial(const bool bStartGame)
{
    if (UVHVFrontEndSubsystem* FrontEnd = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVFrontEndSubsystem>() : nullptr) FrontEnd->MarkTutorialCompleted();
    if (bStartGame) BeginNewGameTravel();
    else { bTutorialLaunchingNewGame = false; ShowPage(EPage::Main); }
}

void UVHVMainMenuWidget::RefreshSettingsLabels()
{
    const UVHVAudioSubsystem* Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVAudioSubsystem>() : nullptr;
    const float Volume = Audio ? Audio->GetMusicVolume() : 0.70f;
    if (MusicSlider) MusicSlider->SetValue(Volume);
    if (MusicVolumeValue) MusicVolumeValue->SetText(FText::AsPercent(Volume));
    const UVHVFrontEndSubsystem* FrontEnd = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVFrontEndSubsystem>() : nullptr;
    if (TutorialPreferenceValue) TutorialPreferenceValue->SetText(FrontEnd && FrontEnd->ShouldShowTutorialOnNewGame() ? NSLOCTEXT("VHVMainMenu", "On", "ON") : NSLOCTEXT("VHVMainMenu", "Off", "OFF"));
}

void UVHVMainMenuWidget::HandleMusicVolumeChanged(const float Value)
{
    if (UVHVAudioSubsystem* Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVAudioSubsystem>() : nullptr) Audio->SetMusicVolume(Value);
    if (MusicVolumeValue) MusicVolumeValue->SetText(FText::AsPercent(Value));
}

void UVHVMainMenuWidget::ToggleTutorialPreference()
{
    if (UVHVFrontEndSubsystem* FrontEnd = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVFrontEndSubsystem>() : nullptr) FrontEnd->SetShowTutorialOnNewGame(!FrontEnd->ShouldShowTutorialOnNewGame());
    RefreshSettingsLabels();
}

void UVHVMainMenuWidget::PlayFocusSound() const {}
void UVHVMainMenuWidget::PlayConfirmSound() const { if (UVHVAudioSubsystem* Audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVHVAudioSubsystem>() : nullptr) Audio->PlayEvent(TEXT("UI_ButtonConfirm")); }
void UVHVMainMenuWidget::HandleMenuHovered(const int32 Index) { SetSelectedOption(Index, true); }
FReply UVHVMainMenuWidget::HandleMenuClicked(const int32 Index) { SetSelectedOption(Index, false); ActivateOption(Index); return FReply::Handled(); }
void UVHVMainMenuWidget::HandleConfirmationHovered(const int32 Index) { SetConfirmationSelection(Index, true); }
FReply UVHVMainMenuWidget::HandleConfirmationClicked(const int32 Index) { SetConfirmationSelection(Index, false); Index == 0 ? StartNewGame() : HideNewGameConfirmation(); return FReply::Handled(); }
FReply UVHVMainMenuWidget::HandleTechniqueClicked(const int32 Index) { SetTechniqueSelection(Index); StartSelectedTechnique(); return FReply::Handled(); }
FReply UVHVMainMenuWidget::HandleBackClicked() { ShowPage(EPage::Main); return FReply::Handled(); }
FReply UVHVMainMenuWidget::HandleTutorialPrevious() { SetTutorialPage(TutorialPageIndex - 1); return FReply::Handled(); }
FReply UVHVMainMenuWidget::HandleTutorialNext() { TutorialPageIndex == 0 ? SetTutorialPage(1) : CloseTutorial(bTutorialLaunchingNewGame); return FReply::Handled(); }
FReply UVHVMainMenuWidget::HandleTutorialSkip() { CloseTutorial(bTutorialLaunchingNewGame); return FReply::Handled(); }
FReply UVHVMainMenuWidget::HandleTutorialPreferenceClicked() { ToggleTutorialPreference(); return FReply::Handled(); }
