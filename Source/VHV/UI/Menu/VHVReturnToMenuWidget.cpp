#include "UI/Menu/VHVReturnToMenuWidget.h"

#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "UI/Textbook/VHVActivityUIStyle.h"
#include "VHVPlayerController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

UVHVReturnToMenuWidget::UVHVReturnToMenuWidget()
{
    SetIsFocusable(true);
}

TSharedRef<SWidget> UVHVReturnToMenuWidget::RebuildWidget()
{
    PanelBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassMain().CopyWithNewOpacity(0.92f), 18.0f,
        VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.62f), 1.0f);
    const FButtonStyle& ButtonStyle = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("NoBorder"));
    const FText Choices[] = {
        NSLOCTEXT("VHVReturnMenu", "Save", "SAVE & MAIN MENU"),
        NSLOCTEXT("VHVReturnMenu", "NoSave", "MAIN MENU WITHOUT SAVING"),
        NSLOCTEXT("VHVReturnMenu", "Cancel", "CANCEL / RESUME")
    };

    TSharedPtr<SVerticalBox> Actions;
    SAssignNew(Actions, SVerticalBox);
    Labels.Empty();
    for (int32 Index = 0; Index < 3; ++Index)
    {
        TSharedPtr<STextBlock> Label;
        Actions->AddSlot().AutoHeight().Padding(0.0f, 5.0f)
        [
            SNew(SButton)
            .ButtonStyle(&ButtonStyle)
            .ContentPadding(FMargin(14.0f, 9.0f))
            .OnHovered(FSimpleDelegate::CreateUObject(this, &UVHVReturnToMenuWidget::HandleHovered, Index))
            .OnClicked(FOnClicked::CreateUObject(this, &UVHVReturnToMenuWidget::HandleClicked, Index))
            [
                SAssignNew(Label, STextBlock)
                .Justification(ETextJustify::Center)
                .Font(VHVActivityUIStyle::MediumFont(17))
                .Text(Choices[Index])
            ]
        ];
        Labels.Add(Label);
    }

    TSharedRef<SWidget> Root =
        SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder).BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.48f))
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(490.0f)
            [
                SNew(SBorder).BorderImage(&PanelBrush).Padding(FMargin(34.0f, 28.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(25))
                        .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                        .Text(NSLOCTEXT("VHVReturnMenu", "Title", "Return to Main Menu?"))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 18.0f)
                    [
                        SNew(STextBlock)
                        .Justification(ETextJustify::Center)
                        .AutoWrapText(true)
                        .Font(VHVActivityUIStyle::RegularFont(14))
                        .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                        .Text(NSLOCTEXT("VHVReturnMenu", "Warning", "Unsaved campaign progress is lost if you leave without saving."))
                    ]
                    + SVerticalBox::Slot().AutoHeight()[Actions.ToSharedRef()]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
                    [
                        SAssignNew(StatusText, STextBlock)
                        .Justification(ETextJustify::Center)
                        .AutoWrapText(true)
                        .Font(VHVActivityUIStyle::RegularFont(13))
                        .ColorAndOpacity(FLinearColor(1.0f, 0.48f, 0.35f, 1.0f))
                        .Visibility(EVisibility::Collapsed)
                    ]
                ]
            ]
        ];
    SetSelection(0);
    return Root;
}

FReply UVHVReturnToMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up)
    {
        SetSelection((SelectedIndex + 2) % 3);
        return FReply::Handled();
    }
    if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down)
    {
        SetSelection((SelectedIndex + 1) % 3);
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        ActivateSelection();
        return FReply::Handled();
    }
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right)
    {
        if (AVHVPlayerController* Controller = Cast<AVHVPlayerController>(GetOwningPlayer()))
        {
            Controller->CloseReturnToMenuOverlay();
        }
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UVHVReturnToMenuWidget::SetSelection(const int32 Index)
{
    SelectedIndex = FMath::Clamp(Index, 0, 2);
    for (int32 LabelIndex = 0; LabelIndex < Labels.Num(); ++LabelIndex)
    {
        if (Labels[LabelIndex])
        {
            Labels[LabelIndex]->SetColorAndOpacity(LabelIndex == SelectedIndex
                ? VHVActivityUIStyle::GoldSelected() : VHVActivityUIStyle::TextSecondary());
        }
    }
}

void UVHVReturnToMenuWidget::ActivateSelection()
{
    if (AVHVPlayerController* Controller = Cast<AVHVPlayerController>(GetOwningPlayer()))
    {
        if (SelectedIndex == 0) Controller->ReturnToMainMenu(true);
        else if (SelectedIndex == 1) Controller->ReturnToMainMenu(false);
        else Controller->CloseReturnToMenuOverlay();
    }
}

FReply UVHVReturnToMenuWidget::HandleClicked(const int32 Index)
{
    SetSelection(Index);
    ActivateSelection();
    return FReply::Handled();
}

void UVHVReturnToMenuWidget::HandleHovered(const int32 Index)
{
    SetSelection(Index);
}

void UVHVReturnToMenuWidget::ShowSaveFailure()
{
    if (StatusText)
    {
        StatusText->SetText(NSLOCTEXT("VHVReturnMenu", "SaveFailed",
            "Save failed or the game is not at a safe checkpoint. Resume and try again."));
        StatusText->SetVisibility(EVisibility::Visible);
    }
}
