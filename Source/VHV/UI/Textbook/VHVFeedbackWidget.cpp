#include "UI/Textbook/VHVFeedbackWidget.h"

#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UI/VHVUIManagerComponent.h"
#include "Components/TextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UVHVFeedbackWidget::RebuildWidget()
{
    ShadowBrush = VHVActivityUIStyle::RoundedBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.42f), 17.0f);
    PanelBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::CharcoalGlass(), 16.0f, VHVActivityUIStyle::SoftGold(), 1.0f);
    ContinueButtonStyle = FButtonStyle()
        .SetNormal(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::GoldPrimary(), 22.0f))
        .SetHovered(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::GoldSelected(), 22.0f))
        .SetPressed(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::FromSRGB(202, 161, 79), 22.0f))
        .SetDisabled(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::FromSRGB(181, 166, 135, 170), 22.0f))
        .SetNormalPadding(FMargin(0.0f))
        .SetPressedPadding(FMargin(1.0f, 2.0f, 0.0f, 0.0f));

    TSharedRef<SWidget> Result =
        SNew(SConstraintCanvas)
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.045f, 0.69f, 0.49f, 0.925f))
        .Offset(FMargin(0.0f))
        [
            SAssignNew(FeedbackPanel, SOverlay)
            + SOverlay::Slot()
            .Padding(FMargin(7.0f, 9.0f, -7.0f, -9.0f))
            [
                SNew(SBorder)
                .BorderImage(&ShadowBrush)
            ]
            + SOverlay::Slot()
            [
                SNew(SBackgroundBlur)
                .BlurRadius(4)
                .BlurStrength(5.0f)
                .LowQualityFallbackBrush(&PanelBrush)
            ]
            + SOverlay::Slot()
            [
                SNew(SBorder)
                .BorderImage(&PanelBrush)
                .Padding(FMargin(30.0f, 23.0f, 32.0f, 24.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(TitleTextSlate, STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(16))
                    ]
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 14.0f, 0.0f, 12.0f))
                    [
                        SAssignNew(FeedbackTextSlate, STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(25))
                        .ColorAndOpacity(VHVActivityUIStyle::PrimaryText())
                        .AutoWrapText(true)
                        .LineHeightPercentage(1.12f)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Right)
                    [
                        SNew(SButton)
                        .ButtonStyle(&ContinueButtonStyle)
                        .IsEnabled(true)
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .ContentPadding(FMargin(22.0f, 10.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UVHVFeedbackWidget::HandleContinueClicked))
                        [
                            SNew(STextBlock)
                            .Font(VHVActivityUIStyle::MediumFont(15))
                            .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                            .Text(FText::FromString(TEXT("Continue  \u2192")))
                        ]
                    ]
                ]
            ]
        ];

    RefreshPresentation();
    return Result;
}

void UVHVFeedbackWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EntranceElapsed = 0.0f;
    if (FeedbackPanel)
    {
        FeedbackPanel->SetRenderOpacity(0.0f);
    }
}

void UVHVFeedbackWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    EntranceElapsed += InDeltaTime;
    const float Alpha = FMath::Clamp(EntranceElapsed / 0.20f, 0.0f, 1.0f);
    const float Smoothed = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f);
    if (FeedbackPanel)
    {
        FeedbackPanel->SetRenderOpacity(Smoothed);
        const float HintLift = bCurrentIsHint ? -300.0f : 0.0f;
        FeedbackPanel->SetRenderTransform(FSlateRenderTransform(
            FVector2D(0.0f, HintLift + FMath::Lerp(14.0f, 0.0f, Smoothed))));
    }
}

void UVHVFeedbackWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    TitleTextSlate.Reset();
    FeedbackTextSlate.Reset();
    FeedbackPanel.Reset();
}

void UVHVFeedbackWidget::SetFeedbackText(const FText& InText)
{
    SetFeedbackPresentation(InText, ETone::Neutral);
}

void UVHVFeedbackWidget::SetFeedbackPresentation(const FText& InText, const ETone InTone, const bool bIsHint)
{
    CurrentText = InText;
    CurrentTone = InTone;
    bCurrentIsHint = bIsHint;
    bContinueActivated = false;
    EntranceElapsed = 0.0f;
    RefreshPresentation();
}

void UVHVFeedbackWidget::SetOwningUIManager(UVHVUIManagerComponent* InUIManager)
{
    OwningUIManager = InUIManager;
}

FReply UVHVFeedbackWidget::HandleContinueClicked()
{
    if (!bContinueActivated && OwningUIManager)
    {
        bContinueActivated = true;
        OwningUIManager->ConfirmChoiceInput();
    }
    return FReply::Handled();
}

void UVHVFeedbackWidget::RefreshPresentation()
{
    FLinearColor Accent = VHVActivityUIStyle::Gold();
    FText Title = FText::FromString(TEXT("FEEDBACK"));
    if (bCurrentIsHint)
    {
        Title = FText::FromString(TEXT("HINT"));
    }
    else if (CurrentTone == ETone::Positive)
    {
        Accent = FLinearColor(0.43f, 0.76f, 0.54f, 1.0f);
        Title = FText::FromString(TEXT("CORRECT"));
    }
    else if (CurrentTone == ETone::Caution)
    {
        Accent = FLinearColor(0.88f, 0.43f, 0.30f, 1.0f);
        Title = FText::FromString(TEXT("REVIEW"));
    }

    PanelBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::CharcoalGlass(), 16.0f,
        FLinearColor(Accent.R, Accent.G, Accent.B, 0.72f), 1.2f);
    if (TitleTextSlate)
    {
        TitleTextSlate->SetText(Title);
        TitleTextSlate->SetColorAndOpacity(Accent);
    }
    if (FeedbackTextSlate)
    {
        FeedbackTextSlate->SetText(CurrentText);
    }
}
