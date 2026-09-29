#include "UI/SocialSupport/VHVSupportTypeOverlayWidget.h"

#include "UI/Textbook/VHVActivityUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

UVHVSupportTypeOverlayWidget::UVHVSupportTypeOverlayWidget()
{
    SetIsFocusable(false);
    SetVisibility(ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> UVHVSupportTypeOverlayWidget::RebuildWidget()
{
    PanelBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassMain(), 18.0f,
        VHVActivityUIStyle::PanelBorder(), VHVActivityUIStyle::BorderNormalWidth);

    return SNew(SConstraintCanvas)
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.16f, 0.72f, 0.84f, 0.96f))
        .Offset(FMargin(0.0f))
        [
            SNew(SBorder)
            .BorderImage(&PanelBrush)
            .Padding(FMargin(24.0f, 16.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SAssignNew(HeaderText, STextBlock)
                    .Font(VHVActivityUIStyle::MediumFont(22))
                    .ColorAndOpacity(VHVActivityUIStyle::GoldPrimary())
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f)
                [
                    SAssignNew(FeedbackText, STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(16))
                    .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                    .Justification(ETextJustify::Center)
                    .AutoWrapText(true)
                ]
            ]
        ];
}

void UVHVSupportTypeOverlayWidget::ShowTypeReveal()
{
    RefreshText();
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UVHVSupportTypeOverlayWidget::RefreshText()
{
    if (HeaderText)
    {
        HeaderText->SetText(FText::FromString(TEXT("FOUR FORMS OF SUPPORT")));
    }
    if (FeedbackText)
    {
        FeedbackText->SetText(FText::FromString(
            TEXT("EMOTIONAL  Understanding • encouragement • caring\n"
                 "INFORMATIONAL  Advice • guidance • useful knowledge\n"
                 "INSTRUMENTAL  Practical or physical help\n"
                 "APPRAISAL  Feedback about progress and results")));
    }
}
