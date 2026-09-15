#include "UI/VHVMainHUD.h"

void UVHVMainHUD::SetInteractionPrompt(const FText& InPrompt)
{
    if (PromptText)
    {
        PromptText->SetText(InPrompt);
    }
}

void UVHVMainHUD::SetInteractionPromptVisible(bool bVisible)
{
    if (InteractionPromptContainer)
    {
        InteractionPromptContainer->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}
