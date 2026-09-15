#include "UI/Textbook/VHVFeedbackWidget.h"

void UVHVFeedbackWidget::SetFeedbackText(const FText& InText)
{
    if (FeedbackText)
    {
        FeedbackText->SetText(InText);
    }
}

