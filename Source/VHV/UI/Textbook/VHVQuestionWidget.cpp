#include "UI/Textbook/VHVQuestionWidget.h"
#include "UI/VHVUIManagerComponent.h"

UVHVQuestionWidget::UVHVQuestionWidget()
{
    SetIsFocusable(true);
}

void UVHVQuestionWidget::SetOwningUIManager(UVHVUIManagerComponent* InUIManager)
{
    OwningUIManager = InUIManager;
}

void UVHVQuestionOptionButtonProxy::OnOptionClicked()
{
    if (OwnerWidget)
    {
        OwnerWidget->HandleOptionSelected(OptionIndex);
    }
}

void UVHVQuestionWidget::SetQuestionData(const FQuestionData& InQuestion)
{
    CurrentQuestion = InQuestion;
    SelectedOptionIndex = CurrentQuestion.Options.Num() > 0 ? 0 : INDEX_NONE;
    SelectedOptionIndices.Empty();

    if (QuestionText)
    {
        QuestionText->SetText(FText::FromString(CurrentQuestion.QuestionText));
    }

    ClearOptionButtons();

    if (!OptionsContainer || CurrentQuestion.Options.Num() == 0)
    {
        return;
    }

    for (int32 Index = 0; Index < CurrentQuestion.Options.Num(); ++Index)
    {
        const FQuestionOption& Option = CurrentQuestion.Options[Index];

        UButton* OptionButton = NewObject<UButton>(this);
        UTextBlock* OptionText = NewObject<UTextBlock>(this);
        OptionText->SetText(FText::FromString(Option.OptionText));
        OptionText->SetAutoWrapText(true);
        OptionButton->SetIsEnabled(true);
        OptionButton->AddChild(OptionText);
        OptionButtons.Add(OptionButton);
        OptionsContainer->AddChild(OptionButton);

        UVHVQuestionOptionButtonProxy* OptionProxy = NewObject<UVHVQuestionOptionButtonProxy>(this);
        OptionProxy->OptionIndex = Index;
        OptionProxy->OwnerWidget = this;
        OptionProxies.Add(OptionProxy);
        OptionButton->OnClicked.AddDynamic(OptionProxy, &UVHVQuestionOptionButtonProxy::OnOptionClicked);
    }

    SetIsFocusable(true);
    UpdateSelectionVisuals();
}

void UVHVQuestionWidget::SetMultiChoiceEnabled(bool bInMultiChoiceEnabled)
{
    bMultiChoiceEnabled = bInMultiChoiceEnabled;
    SelectedOptionIndices.Empty();
    UpdateSelectionVisuals();
}

FReply UVHVQuestionWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (!CurrentQuestion.Options.Num())
    {
        return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
    }

    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
    {
        if (SelectedOptionIndex == INDEX_NONE)
        {
            SetSelectedOptionIndex(CurrentQuestion.Options.Num() - 1);
        }
        else
        {
            const int32 NextIndex = SelectedOptionIndex - 1;
            SetSelectedOptionIndex(NextIndex < 0 ? CurrentQuestion.Options.Num() - 1 : NextIndex);
        }
        return FReply::Handled();
    }

    if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
    {
        if (SelectedOptionIndex == INDEX_NONE)
        {
            SetSelectedOptionIndex(0);
        }
        else
        {
            const int32 NextIndex = SelectedOptionIndex + 1;
            SetSelectedOptionIndex(NextIndex >= CurrentQuestion.Options.Num() ? 0 : NextIndex);
        }
        return FReply::Handled();
    }

    if (bMultiChoiceEnabled && (Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Left))
    {
        ToggleSelectedOption(SelectedOptionIndex);
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

void UVHVQuestionWidget::SetSelectedOptionIndex(int32 Index)
{
    if (!CurrentQuestion.Options.IsValidIndex(Index))
    {
        SelectedOptionIndex = INDEX_NONE;
        UpdateSelectionVisuals();
        return;
    }

    SelectedOptionIndex = Index;
    ClearInputFeedback();
    UpdateSelectionVisuals();
}

bool UVHVQuestionWidget::HasSelection() const
{
    if (bMultiChoiceEnabled)
    {
        return SelectedOptionIndices.Num() > 0;
    }

    return CurrentQuestion.Options.IsValidIndex(SelectedOptionIndex);
}

TArray<int32> UVHVQuestionWidget::GetSelectedOptionIndices() const
{
    TArray<int32> Result = SelectedOptionIndices.Array();
    Result.Sort();
    return Result;
}

bool UVHVQuestionWidget::ActivateFocusedOption()
{
    if (!CurrentQuestion.Options.IsValidIndex(SelectedOptionIndex))
    {
        return false;
    }

    HandleOptionSelected(SelectedOptionIndex);
    return true;
}

void UVHVQuestionWidget::HandleOptionSelected(int32 OptionIndex)
{
    if (!CurrentQuestion.Options.IsValidIndex(OptionIndex))
    {
        return;
    }

    if (bMultiChoiceEnabled)
    {
        ToggleSelectedOption(OptionIndex);
    }
    else
    {
        // Mouse and keyboard use the same two-step select/confirm path.
        SetSelectedOptionIndex(OptionIndex);
    }

    SetKeyboardFocus();
}

void UVHVQuestionWidget::ToggleSelectedOption(int32 OptionIndex)
{
    if (!CurrentQuestion.Options.IsValidIndex(OptionIndex))
    {
        return;
    }

    SelectedOptionIndex = OptionIndex;
    ClearInputFeedback();
    if (SelectedOptionIndices.Contains(OptionIndex))
    {
        SelectedOptionIndices.Remove(OptionIndex);
    }
    else
    {
        SelectedOptionIndices.Add(OptionIndex);
    }

    UE_LOG(LogTemp, Log, TEXT("[VHVTextbook] MultiChoice selected OptionIndex=%d Selected=%s"), OptionIndex, SelectedOptionIndices.Contains(OptionIndex) ? TEXT("true") : TEXT("false"));
    UpdateSelectionVisuals();
}

void UVHVQuestionWidget::ShowSelectionRequiredFeedback()
{
    if (QuestionText)
    {
        QuestionText->SetText(FText::FromString(FString::Printf(
            TEXT("%s\nSelect at least one option before submitting."),
            *CurrentQuestion.QuestionText)));
    }
}

void UVHVQuestionWidget::ClearInputFeedback()
{
    if (QuestionText)
    {
        QuestionText->SetText(FText::FromString(CurrentQuestion.QuestionText));
    }
}

void UVHVQuestionWidget::UpdateSelectionVisuals()
{
    if (!OptionsContainer)
    {
        return;
    }

    const FLinearColor SelectedBackground = FLinearColor(0.18f, 0.45f, 0.75f, 1.0f);
    const FLinearColor UnselectedBackground = FLinearColor(0.12f, 0.12f, 0.12f, 1.0f);
    const FLinearColor SelectedText = FLinearColor::White;
    const FLinearColor UnselectedText = FLinearColor(0.82f, 0.82f, 0.82f, 1.0f);

    for (int32 Index = 0; Index < OptionButtons.Num(); ++Index)
    {
        UButton* OptionButton = OptionButtons[Index];
        if (!OptionButton)
        {
            continue;
        }

        const bool bIsFocused = Index == SelectedOptionIndex;
        const bool bIsSelected = bMultiChoiceEnabled ? SelectedOptionIndices.Contains(Index) : bIsFocused;
        OptionButton->SetBackgroundColor(bIsSelected ? SelectedBackground : UnselectedBackground);

        UTextBlock* OptionText = Cast<UTextBlock>(OptionButton->GetChildAt(0));
        if (OptionText)
        {
            FString DisplayText = CurrentQuestion.Options.IsValidIndex(Index) ? CurrentQuestion.Options[Index].OptionText : FString();
            if (bMultiChoiceEnabled)
            {
                DisplayText = FString::Printf(TEXT("%s %s%s"), bIsSelected ? TEXT("[x]") : TEXT("[ ]"), bIsFocused ? TEXT("< ") : TEXT(""), *DisplayText);
                if (bIsFocused)
                {
                    DisplayText += TEXT(" >");
                }
            }
            else if (bIsSelected)
            {
                DisplayText = FString::Printf(TEXT("< %s >"), *DisplayText);
            }
            OptionText->SetText(FText::FromString(DisplayText));
            OptionText->SetColorAndOpacity(bIsSelected || bIsFocused ? SelectedText : UnselectedText);
        }
    }
}

void UVHVQuestionWidget::ClearOptionButtons()
{
    OptionButtons.Empty();
    OptionProxies.Empty();
    if (OptionsContainer)
    {
        OptionsContainer->ClearChildren();
    }
}

