#include "UI/SocialSupport/VHVSocialSupportHUDWidget.h"

#include "UI/Textbook/VHVActivityUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

UVHVSocialSupportHUDWidget::UVHVSocialSupportHUDWidget()
{
    SetIsFocusable(false);
    SetVisibility(ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> UVHVSocialSupportHUDWidget::RebuildWidget()
{
    PanelBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassMain(), 14.0f,
        VHVActivityUIStyle::PanelBorder(), VHVActivityUIStyle::BorderNormalWidth);
    return SNew(SConstraintCanvas)
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.73f, 0.08f, 0.96f, 0.20f))
        .Offset(FMargin(0.0f))
        [
            SNew(SBorder)
            .BorderImage(&PanelBrush)
            .Padding(FMargin(16.0f, 10.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SAssignNew(TitleText, STextBlock)
                    .Font(VHVActivityUIStyle::MediumFont(14))
                    .ColorAndOpacity(VHVActivityUIStyle::GoldPrimary())
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 5.0f, 0.0f, 0.0f)
                [
                    SAssignNew(ProgressText, STextBlock)
                    .Font(VHVActivityUIStyle::MediumFont(18))
                    .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                ]
            ]
        ];
}

void UVHVSocialSupportHUDWidget::ShowObservationProgress(const int32 CompletedCount)
{
    Title = TEXT("SUPPORT AROUND THE VILLAGE");
    Progress.Empty();
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (Index > 0) Progress += TEXT("  ");
        Progress += Index < CompletedCount ? TEXT("●") : TEXT("○");
    }
    Refresh();
}

void UVHVSocialSupportHUDWidget::ShowSupporterProgress(const int32 CompletedCount)
{
    Title = TEXT("UNCLE CHAI'S SUPPORT NETWORK");
    Progress = FString::Printf(TEXT("SUPPORTERS FOUND  %d / 4"), FMath::Clamp(CompletedCount, 0, 4));
    Refresh();
}

void UVHVSocialSupportHUDWidget::Refresh()
{
    if (TitleText) TitleText->SetText(FText::FromString(Title));
    if (ProgressText) ProgressText->SetText(FText::FromString(Progress));
    SetVisibility(ESlateVisibility::HitTestInvisible);
}
