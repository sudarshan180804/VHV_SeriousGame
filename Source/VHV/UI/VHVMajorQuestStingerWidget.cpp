#include "UI/VHVMajorQuestStingerWidget.h"

#include "Kismet/GameplayStatics.h"
#include "UI/Textbook/VHVActivityUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    constexpr float DimFadeInDuration = 0.35f;
    constexpr float LabelStartTime = 0.25f;
    constexpr float LabelFadeDuration = 0.35f;
    constexpr float TitleStartTime = 0.65f;
    constexpr float TitleFadeDuration = 0.65f;
    constexpr float MinimumHoldDuration = 2.10f;
    constexpr float FadeOutDuration = 0.60f;
    constexpr float TitleSettleOffset = 8.0f;
}

UVHVMajorQuestStingerWidget::UVHVMajorQuestStingerWidget()
{
    SetIsFocusable(false);
}

TSharedRef<SWidget> UVHVMajorQuestStingerWidget::RebuildWidget()
{
    DimBrush = VHVActivityUIStyle::RoundedBrush(FLinearColor(0.006f, 0.008f, 0.010f, 0.64f), 0.0f);
    OrnamentBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::DividerGold().CopyWithNewOpacity(0.78f), 1.0f);

    TSharedRef<SWidget> Result =
        SNew(SOverlay)
        + SOverlay::Slot()
        [
            SAssignNew(DimLayer, SBorder)
            .BorderImage(&DimBrush)
        ]
        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SAssignNew(LabelText, STextBlock)
                .Font(VHVActivityUIStyle::MediumFont(20))
                .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
                .Justification(ETextJustify::Center)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(FMargin(0.0f, 16.0f, 0.0f, 0.0f))
            [
                SAssignNew(TitleGroup, SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                [
                    SNew(SBox)
                    .WidthOverride(420.0f)
                    .HeightOverride(1.0f)
                    [
                        SNew(SBorder)
                        .BorderImage(&OrnamentBrush)
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                .Padding(FMargin(40.0f, 22.0f, 40.0f, 0.0f))
                [
                    SAssignNew(TitleText, STextBlock)
                    .Font(VHVActivityUIStyle::MediumFont(58))
                    .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                    .Justification(ETextJustify::Center)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                .Padding(FMargin(24.0f, 15.0f, 24.0f, 0.0f))
                [
                    SAssignNew(SubtitleText, STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(16))
                    .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                    .Justification(ETextJustify::Center)
                ]
            ]
        ];

    SetVisibility(ESlateVisibility::Collapsed);
    return Result;
}

void UVHVMajorQuestStingerWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    DimLayer.Reset();
    LabelText.Reset();
    TitleGroup.Reset();
    TitleText.Reset();
    SubtitleText.Reset();
}

void UVHVMajorQuestStingerWidget::ShowStinger(const FVHVMajorQuestStingerData& InData)
{
    if (!InData.IsConfigured())
    {
        return;
    }

    StingerData = InData;
    ElapsedTime = 0.0f;
    bPlaying = true;

    if (LabelText)
    {
        LabelText->SetText(FText::FromString(StingerData.Label));
        LabelText->SetVisibility(StingerData.Label.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
        LabelText->SetRenderOpacity(0.0f);
    }
    if (TitleText)
    {
        TitleText->SetText(FText::FromString(StingerData.Title));
    }
    if (SubtitleText)
    {
        SubtitleText->SetText(FText::FromString(StingerData.Subtitle));
        SubtitleText->SetVisibility(StingerData.Subtitle.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
    }
    if (DimLayer)
    {
        DimLayer->SetRenderOpacity(0.0f);
    }
    if (TitleGroup)
    {
        TitleGroup->SetRenderOpacity(0.0f);
        TitleGroup->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
        TitleGroup->SetRenderTransform(FSlateRenderTransform(
            0.97f, FVector2D(0.0f, TitleSettleOffset)));
    }

    SetVisibility(ESlateVisibility::HitTestInvisible);

    if (USoundBase* Sound = StingerData.IntroSound.LoadSynchronous())
    {
        UGameplayStatics::PlaySound2D(this, Sound);
    }
}

void UVHVMajorQuestStingerWidget::NativeTick(
    const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bPlaying)
    {
        return;
    }

    ElapsedTime += InDeltaTime;
    const float HoldEndTime = TitleStartTime + TitleFadeDuration
        + FMath::Max(MinimumHoldDuration, StingerData.HoldDuration);
    const float EndTime = HoldEndTime + FadeOutDuration;
    const float FadeOutAlpha = FMath::Clamp(
        (ElapsedTime - HoldEndTime) / FadeOutDuration, 0.0f, 1.0f);

    if (DimLayer)
    {
        const float FadeInAlpha = FMath::Clamp(ElapsedTime / DimFadeInDuration, 0.0f, 1.0f);
        DimLayer->SetRenderOpacity(FadeInAlpha * (1.0f - FadeOutAlpha));
    }
    if (LabelText)
    {
        const float LabelAlpha = FMath::Clamp(
            (ElapsedTime - LabelStartTime) / LabelFadeDuration, 0.0f, 1.0f);
        LabelText->SetRenderOpacity(LabelAlpha * (1.0f - FadeOutAlpha));
    }
    if (TitleGroup)
    {
        const float RawTitleAlpha = FMath::Clamp(
            (ElapsedTime - TitleStartTime) / TitleFadeDuration, 0.0f, 1.0f);
        const float TitleAlpha = FMath::InterpEaseOut(0.0f, 1.0f, RawTitleAlpha, 3.0f);
        TitleGroup->SetRenderOpacity(TitleAlpha * (1.0f - FadeOutAlpha));
        const float Scale = FMath::Lerp(0.97f, 1.0f, TitleAlpha);
        TitleGroup->SetRenderTransform(FSlateRenderTransform(
            Scale, FVector2D(0.0f, FMath::Lerp(TitleSettleOffset, 0.0f, TitleAlpha))));
    }

    if (ElapsedTime >= EndTime)
    {
        FinishStinger();
    }
}

void UVHVMajorQuestStingerWidget::FinishStinger()
{
    if (!bPlaying)
    {
        return;
    }

    bPlaying = false;
    SetVisibility(ESlateVisibility::Collapsed);
    OnStingerFinished.Broadcast();
}
