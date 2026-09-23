#include "UI/Textbook/VHVQuestionWidget.h"

#include "UI/Textbook/SVHVChoiceCard.h"
#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UI/VHVUIManagerComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "InputCoreTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

UVHVQuestionWidget::UVHVQuestionWidget()
{
    SetIsFocusable(true);
}

TSharedRef<SWidget> UVHVQuestionWidget::RebuildWidget()
{
    QuestionPanelBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassMain(), VHVActivityUIStyle::QuestionPanelRadius,
        VHVActivityUIStyle::PanelBorder(), VHVActivityUIStyle::BorderNormalWidth);
    QuestionShadowBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::ShadowPanel(), VHVActivityUIStyle::QuestionPanelRadius + 3.0f);
    KeycapBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(29, 34, 38, 145), 6.0f,
        VHVActivityUIStyle::BorderNeutral(), VHVActivityUIStyle::BorderNormalWidth);
    ActivityEmblemBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(28, 26, 20, 128), 10.0f,
        VHVActivityUIStyle::HeaderGold(), VHVActivityUIStyle::BorderNormalWidth);
    DividerBrush = VHVActivityUIStyle::RoundedBrush(VHVActivityUIStyle::DividerGold(), 1.0f);
    SubmitButtonStyle = FButtonStyle()
        .SetNormal(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.92f), 22.0f,
            VHVActivityUIStyle::GoldSelected(), VHVActivityUIStyle::BorderNormalWidth))
        .SetHovered(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::GoldSelected(), 22.0f,
            VHVActivityUIStyle::TextPrimary().CopyWithNewOpacity(0.28f),
            VHVActivityUIStyle::BorderNormalWidth))
        .SetPressed(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::FromSRGB(202, 161, 79), 22.0f,
            VHVActivityUIStyle::TextPrimary().CopyWithNewOpacity(0.36f),
            VHVActivityUIStyle::BorderNormalWidth))
        .SetNormalPadding(FMargin(0.0f))
        .SetPressedPadding(FMargin(1.0f, 2.0f, 0.0f, 0.0f));

    TSharedRef<SWidget> Result =
        SAssignNew(RootCanvas, SConstraintCanvas)
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.1275f, 0.655f, 0.5625f, 0.865f))
        .Offset(FMargin(0.0f))
        [
            SAssignNew(QuestionPanel, SOverlay)
            + SOverlay::Slot()
            .Padding(FMargin(5.0f, 6.0f, -5.0f, -6.0f))
            [
                SNew(SBorder)
                .BorderImage(&QuestionShadowBrush)
            ]
            + SOverlay::Slot()
            [
                SNew(SBorder)
                .BorderImage(&QuestionPanelBrush)
                .Padding(VHVActivityUIStyle::PanelPadding)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
                        [
                            SNew(SBox)
                            .WidthOverride(VHVActivityUIStyle::HeaderIconDiameter)
                            .HeightOverride(VHVActivityUIStyle::HeaderIconDiameter)
                            [
                                SNew(SBorder)
                                .BorderImage(&ActivityEmblemBrush)
                                .HAlign(HAlign_Center)
                                .VAlign(VAlign_Center)
                                [
                                    SNew(STextBlock)
                                    .Font(VHVActivityUIStyle::MediumFont(12))
                                    .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
                                    .Text(FText::FromString(TEXT("?")))
                                ]
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SAssignNew(ActivityTypeText, STextBlock)
                            .Font(VHVActivityUIStyle::MediumFont(VHVActivityUIStyle::HeaderFontSize))
                            .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Left)
                    .Padding(FMargin(32.0f, 9.0f, 0.0f, 22.0f))
                    [
                        SNew(SBox)
                        .WidthOverride(VHVActivityUIStyle::HeaderDividerWidth)
                        .HeightOverride(VHVActivityUIStyle::HeaderDividerHeight)
                        [
                            SNew(SBorder)
                            .BorderImage(&DividerBrush)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(QuestionTextSlate, STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(VHVActivityUIStyle::QuestionFontSize))
                        .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                        .AutoWrapText(true)
                        .LineHeightPercentage(1.12f)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 24.0f, 0.0f, 0.0f))
                    [
                        SAssignNew(InstructionTextSlate, STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(VHVActivityUIStyle::InstructionFontSize))
                        .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                        .AutoWrapText(true)
                    ]
                ]
            ]
        ]
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.6625f, 0.38f, 0.975f, 0.86f))
        .Offset(FMargin(0.0f))
        [
            SAssignNew(AnswerPanel, SBox)
            .VAlign(VAlign_Center)
            [
                SAssignNew(OptionsSlateContainer, SVerticalBox)
            ]
        ]
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.87f, 0.93f))
        .Alignment(FVector2D(0.5f, 0.5f))
        .Offset(FMargin(0.0f, 0.0f, 320.0f, 44.0f))
        [
            SNew(SBox)
            .HAlign(HAlign_Right)
            .VAlign(VAlign_Center)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBorder)
                    .BorderImage(&KeycapBrush)
                    .Padding(FMargin(8.0f, 3.0f))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(13))
                        .ColorAndOpacity(VHVActivityUIStyle::PrimaryText())
                        .Text(FText::FromString(TEXT("E")))
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(7.0f, 0.0f, 16.0f, 0.0f))
                [
                    SAssignNew(InteractionLegendText, STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(14))
                    .ColorAndOpacity(VHVActivityUIStyle::SecondaryText())
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(126.0f)
                    .HeightOverride(44.0f)
                    [
                        SNew(SButton)
                        .ButtonStyle(&SubmitButtonStyle)
                        .IsEnabled(TAttribute<bool>::Create(
                            TAttribute<bool>::FGetter::CreateUObject(
                                this, &UVHVQuestionWidget::HasSelection)))
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .ContentPadding(FMargin(18.0f, 8.0f))
                        .OnClicked(FOnClicked::CreateUObject(
                            this, &UVHVQuestionWidget::HandleSubmitClicked))
                        [
                            SNew(STextBlock)
                            .Font(VHVActivityUIStyle::MediumFont(14))
                            .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                            .Text(FText::FromString(TEXT("SUBMIT")))
                        ]
                    ]
                ]
            ]
        ];

    RebuildChoiceCards();
    UpdatePresentation();
    return Result;
}

void UVHVQuestionWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    ChoiceCards.Empty();
    RootCanvas.Reset();
    OptionsSlateContainer.Reset();
    ActivityTypeText.Reset();
    QuestionTextSlate.Reset();
    InstructionTextSlate.Reset();
    InteractionLegendText.Reset();
    QuestionPanel.Reset();
    AnswerPanel.Reset();
}

void UVHVQuestionWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EntranceElapsed = 0.0f;
    if (QuestionPanel)
    {
        QuestionPanel->SetRenderOpacity(0.0f);
    }
    if (AnswerPanel)
    {
        AnswerPanel->SetRenderOpacity(0.0f);
    }
}

void UVHVQuestionWidget::PrepareForModalFocus()
{
    if (!CurrentQuestion.Options.IsValidIndex(FocusedOptionIndex)
        && !CurrentQuestion.Options.IsEmpty())
    {
        FocusedOptionIndex = 0;
    }
    UpdatePresentation();
}

void UVHVQuestionWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    EntranceElapsed += InDeltaTime;
    const float Alpha = FMath::Clamp(
        EntranceElapsed / VHVActivityUIStyle::AnimationStandard, 0.0f, 1.0f);
    const float Smoothed = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f);

    if (QuestionPanel)
    {
        QuestionPanel->SetRenderOpacity(Smoothed);
        QuestionPanel->SetRenderTransform(FSlateRenderTransform(
            FVector2D(0.0f, FMath::Lerp(12.0f, 0.0f, Smoothed))));
    }
    if (AnswerPanel)
    {
        AnswerPanel->SetRenderOpacity(Smoothed);
    }
}

void UVHVQuestionWidget::SetOwningUIManager(UVHVUIManagerComponent* InUIManager)
{
    OwningUIManager = InUIManager;
}

void UVHVQuestionWidget::SetQuestionData(const FQuestionData& InQuestion)
{
    CurrentQuestion = InQuestion;
    FocusedOptionIndex = CurrentQuestion.Options.Num() > 0 ? 0 : INDEX_NONE;
    SelectedOptionIndex = INDEX_NONE;
    SelectedOptionIndices.Empty();
    bShowingSelectionFeedback = false;
    EntranceElapsed = 0.0f;
    RebuildChoiceCards();
    UpdatePresentation();
}

void UVHVQuestionWidget::SetMultiChoiceEnabled(const bool bInMultiChoiceEnabled)
{
    bMultiChoiceEnabled = bInMultiChoiceEnabled;
    SelectedOptionIndex = INDEX_NONE;
    SelectedOptionIndices.Empty();
    bShowingSelectionFeedback = false;
    UpdatePresentation();
}

FReply UVHVQuestionWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (CurrentQuestion.Options.Num() == 0)
    {
        return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
    }

    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
    {
        const int32 NextIndex = FocusedOptionIndex == INDEX_NONE
            ? CurrentQuestion.Options.Num() - 1
            : (FocusedOptionIndex - 1 + CurrentQuestion.Options.Num()) % CurrentQuestion.Options.Num();
        SetFocusedOptionIndex(NextIndex);
        return FReply::Handled();
    }

    if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
    {
        const int32 NextIndex = FocusedOptionIndex == INDEX_NONE
            ? 0
            : (FocusedOptionIndex + 1) % CurrentQuestion.Options.Num();
        SetFocusedOptionIndex(NextIndex);
        return FReply::Handled();
    }

    if (bMultiChoiceEnabled && (Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Left))
    {
        ToggleSelectedOption(FocusedOptionIndex);
        return FReply::Handled();
    }

    if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (OwningUIManager)
        {
            OwningUIManager->TrySubmitCurrentQuestionAnswer();
        }
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 UVHVQuestionWidget::GetSelectedOptionIndex() const
{
    return SelectedOptionIndex;
}

void UVHVQuestionWidget::SetSelectedOptionIndex(const int32 Index)
{
    if (!CurrentQuestion.Options.IsValidIndex(Index))
    {
        SelectedOptionIndex = INDEX_NONE;
        UpdatePresentation();
        return;
    }

    FocusedOptionIndex = Index;
    SelectedOptionIndex = Index;
    ClearInputFeedback();
    UpdatePresentation();
}

void UVHVQuestionWidget::SetFocusedOptionIndex(const int32 Index)
{
    if (!CurrentQuestion.Options.IsValidIndex(Index))
    {
        return;
    }
    FocusedOptionIndex = Index;
    UpdatePresentation();
}

bool UVHVQuestionWidget::HasSelection() const
{
    return bMultiChoiceEnabled
        ? SelectedOptionIndices.Num() > 0
        : CurrentQuestion.Options.IsValidIndex(SelectedOptionIndex);
}

TArray<int32> UVHVQuestionWidget::GetSelectedOptionIndices() const
{
    TArray<int32> Result = SelectedOptionIndices.Array();
    Result.Sort();
    return Result;
}

bool UVHVQuestionWidget::ActivateFocusedOption()
{
    if (!CurrentQuestion.Options.IsValidIndex(FocusedOptionIndex))
    {
        if (CurrentQuestion.Options.IsEmpty())
        {
            return false;
        }
        FocusedOptionIndex = 0;
        UpdatePresentation();
    }

    HandleOptionSelected(FocusedOptionIndex);
    return true;
}

FReply UVHVQuestionWidget::HandleSubmitClicked()
{
    if (OwningUIManager)
    {
        OwningUIManager->TrySubmitCurrentQuestionAnswer();
    }
    SetKeyboardFocus();
    return FReply::Handled();
}

void UVHVQuestionWidget::HandleOptionSelected(const int32 OptionIndex)
{
    if (!CurrentQuestion.Options.IsValidIndex(OptionIndex))
    {
        return;
    }

    FocusedOptionIndex = OptionIndex;
    if (bMultiChoiceEnabled)
    {
        ToggleSelectedOption(OptionIndex);
    }
    else
    {
        SetSelectedOptionIndex(OptionIndex);
    }

    SetKeyboardFocus();
}

void UVHVQuestionWidget::HandleChoiceCardChosen(const int32 OptionIndex)
{
    HandleOptionSelected(OptionIndex);
}

void UVHVQuestionWidget::ToggleSelectedOption(const int32 OptionIndex)
{
    if (!CurrentQuestion.Options.IsValidIndex(OptionIndex))
    {
        return;
    }

    FocusedOptionIndex = OptionIndex;
    ClearInputFeedback();
    if (SelectedOptionIndices.Contains(OptionIndex))
    {
        SelectedOptionIndices.Remove(OptionIndex);
    }
    else
    {
        SelectedOptionIndices.Add(OptionIndex);
    }

    UE_LOG(LogTemp, Log, TEXT("[VHVTextbook] MultiChoice selected OptionIndex=%d Selected=%s"),
        OptionIndex, SelectedOptionIndices.Contains(OptionIndex) ? TEXT("true") : TEXT("false"));
    UpdatePresentation();
}

void UVHVQuestionWidget::ShowSelectionRequiredFeedback()
{
    bShowingSelectionFeedback = true;
    if (InstructionTextSlate)
    {
        InstructionTextSlate->SetText(FText::FromString(bMultiChoiceEnabled
            ? TEXT("Select at least one option before confirming.")
            : TEXT("Select an option before confirming.")));
        InstructionTextSlate->SetColorAndOpacity(VHVActivityUIStyle::WarningText());
    }
}

void UVHVQuestionWidget::ClearInputFeedback()
{
    bShowingSelectionFeedback = false;
    if (InstructionTextSlate)
    {
        InstructionTextSlate->SetText(GetDefaultInstruction());
        InstructionTextSlate->SetColorAndOpacity(VHVActivityUIStyle::SecondaryText());
    }
}

FText UVHVQuestionWidget::GetDefaultInstruction() const
{
    return FText::FromString(bMultiChoiceEnabled
        ? TEXT("Choose every answer that applies, then confirm.")
        : TEXT("Choose one answer, then confirm."));
}

void UVHVQuestionWidget::RebuildChoiceCards()
{
    ChoiceCards.Empty();
    if (!OptionsSlateContainer)
    {
        return;
    }

    OptionsSlateContainer->ClearChildren();
    for (int32 Index = 0; Index < CurrentQuestion.Options.Num(); ++Index)
    {
        TSharedPtr<SVHVChoiceCard> ChoiceCard;
        OptionsSlateContainer->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f,
            Index + 1 < CurrentQuestion.Options.Num() ? VHVActivityUIStyle::ChoiceGap : 0.0f))
        [
            SAssignNew(ChoiceCard, SVHVChoiceCard)
            .OptionIndex(Index)
            .AnswerText(FText::FromString(CurrentQuestion.Options[Index].OptionText))
            .EntranceDelay(Index * VHVActivityUIStyle::AnimationStagger)
            .AllowVariableHeight(true)
            .OnChosen(FOnVHVChoiceCardChosen::CreateUObject(this, &UVHVQuestionWidget::HandleChoiceCardChosen))
        ];
        ChoiceCards.Add(ChoiceCard);
    }
}

void UVHVQuestionWidget::UpdatePresentation()
{
    if (ActivityTypeText)
    {
        ActivityTypeText->SetText(FText::FromString(bMultiChoiceEnabled
            ? TEXT("MULTIPLE CHOICE")
            : TEXT("SINGLE CHOICE")));
    }
    if (QuestionTextSlate)
    {
        QuestionTextSlate->SetText(FText::FromString(CurrentQuestion.QuestionText));
    }
    if (InstructionTextSlate && !bShowingSelectionFeedback)
    {
        InstructionTextSlate->SetText(GetDefaultInstruction());
        InstructionTextSlate->SetColorAndOpacity(VHVActivityUIStyle::SecondaryText());
    }
    if (InteractionLegendText)
    {
        InteractionLegendText->SetText(FText::FromString(bMultiChoiceEnabled
            ? TEXT("Toggle")
            : TEXT("Select")));
    }

    for (int32 Index = 0; Index < ChoiceCards.Num(); ++Index)
    {
        if (ChoiceCards[Index])
        {
            const bool bIsSelected = bMultiChoiceEnabled
                ? SelectedOptionIndices.Contains(Index)
                : SelectedOptionIndex == Index;
            ChoiceCards[Index]->SetPresentationState(
                Index == FocusedOptionIndex,
                bIsSelected,
                true,
                bMultiChoiceEnabled);
        }
    }
}
