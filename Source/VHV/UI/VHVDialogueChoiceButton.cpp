#include "UI/VHVDialogueChoiceButton.h"

#include "UI/VHVDialogueWidget.h"

void UVHVDialogueChoiceButton::InitializeChoice(UVHVDialogueWidget* InOwningDialogueWidget, int32 InChoiceIndex)
{
    OwningDialogueWidget = InOwningDialogueWidget;
    ChoiceIndex = InChoiceIndex;
    OnClicked.AddDynamic(this, &UVHVDialogueChoiceButton::HandleClicked);
}

void UVHVDialogueChoiceButton::HandleClicked()
{
    if (OwningDialogueWidget && ChoiceIndex != INDEX_NONE)
    {
        OwningDialogueWidget->SelectChoiceAndConfirm(ChoiceIndex);
    }
}
