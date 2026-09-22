#include "UI/Ambient/VHVAmbientSpeechWidget.h"

#include "Components/Widget.h"
#include "UI/Ambient/VHVAmbientSpeechStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

UVHVAmbientSpeechWidget::UVHVAmbientSpeechWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
    , ShadowBrush(FSlateRoundedBoxBrush(VHVAmbientSpeechStyle::Shadow(), VHVAmbientSpeechStyle::CornerRadius + 2.0f))
    , PanelBrush(FSlateRoundedBoxBrush(VHVAmbientSpeechStyle::SpeechBackground(), VHVAmbientSpeechStyle::CornerRadius,
        VHVAmbientSpeechStyle::Border(), 1.0f))
{
    SetIsFocusable(false);
}

TSharedRef<SWidget> UVHVAmbientSpeechWidget::RebuildWidget()
{
    RefreshBrushes();
    TSharedPtr<SVerticalBox> Content;
    TSharedRef<SWidget> Root =
        SAssignNew(AnimatedRoot, SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SOverlay)
            + SOverlay::Slot().Padding(FMargin(4.0f, 5.0f, -4.0f, -5.0f))
            [
                SNew(SBorder).BorderImage(&ShadowBrush)
            ]
            + SOverlay::Slot()
            [
                SNew(SBox)
                .MinDesiredWidth(VHVAmbientSpeechStyle::MinWidth)
                .MaxDesiredWidth(VHVAmbientSpeechStyle::MaxWidth)
                [
                    SNew(SBorder)
                    .BorderImage(&PanelBrush)
                    .Padding(FMargin(VHVAmbientSpeechStyle::PaddingHorizontal, VHVAmbientSpeechStyle::PaddingVertical))
                    [
                        SAssignNew(Content, SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 5.0f))
                        [
                            SAssignNew(SpeakerRow, SBox)
                            [
                                SAssignNew(SpeakerText, STextBlock)
                                .Font(VHVAmbientSpeechStyle::MediumFont(VHVAmbientSpeechStyle::SpeakerFontSize))
                                .ColorAndOpacity(VHVAmbientSpeechStyle::Speaker())
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SAssignNew(BodyText, STextBlock)
                            .Font(VHVAmbientSpeechStyle::RegularFont(VHVAmbientSpeechStyle::BodyFontSize))
                            .ColorAndOpacity(VHVAmbientSpeechStyle::Text())
                            .AutoWrapText(true)
                            .WrapTextAt(VHVAmbientSpeechStyle::MaxWidth - 2.0f * VHVAmbientSpeechStyle::PaddingHorizontal)
                            .LineHeightPercentage(1.12f)
                        ]
                    ]
                ]
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, -7.0f, 0.0f, 0.0f))
        [
            SAssignNew(TailText, STextBlock)
            .Font(VHVAmbientSpeechStyle::MediumFont(20))
            .ColorAndOpacity(VHVAmbientSpeechStyle::SpeechBackground())
            .Text(FText::FromString(TEXT("▼")))
        ];

    AnimatedRoot->SetVisibility(EVisibility::Collapsed);
    return Root;
}

void UVHVAmbientSpeechWidget::ShowBubble(const FText& InSpeakerName, const FText& InText,
    const EVHVAmbientSpeechType SpeechType, const bool bShowSpeakerName)
{
    CurrentType = SpeechType;
    RefreshBrushes();
    if (SpeakerText) SpeakerText->SetText(InSpeakerName.ToUpper());
    if (BodyText) BodyText->SetText(InText);
    if (SpeakerRow) SpeakerRow->SetVisibility(bShowSpeakerName && !InSpeakerName.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed);
    if (TailText)
    {
        TailText->SetText(FText::FromString(SpeechType == EVHVAmbientSpeechType::Thought ? TEXT("  ○\n○") : TEXT("▼")));
        TailText->SetColorAndOpacity(SpeechType == EVHVAmbientSpeechType::Thought
            ? VHVAmbientSpeechStyle::ThoughtBackground() : VHVAmbientSpeechStyle::SpeechBackground());
    }
    if (AnimatedRoot) AnimatedRoot->SetVisibility(EVisibility::Visible);
    AnimationElapsed = 0.0f;
    bAppearing = true;
    bDisappearing = false;
    bBubbleVisible = true;
    ApplyAnimatedState(0.0f, VHVAmbientSpeechStyle::EntranceOffset, VHVAmbientSpeechStyle::EntranceScale);
}

void UVHVAmbientSpeechWidget::HideBubble(const bool bImmediate)
{
    if (!bBubbleVisible) return;
    if (bImmediate)
    {
        bBubbleVisible = false;
        bAppearing = false;
        bDisappearing = false;
        if (AnimatedRoot) AnimatedRoot->SetVisibility(EVisibility::Collapsed);
        return;
    }
    AnimationElapsed = 0.0f;
    bAppearing = false;
    bDisappearing = true;
}

void UVHVAmbientSpeechWidget::SetDistanceOpacity(const float Opacity)
{
    DistanceOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
    ApplyAnimatedState(LastAnimationOpacity, LastAnimationOffsetY, LastAnimationScale);
}

void UVHVAmbientSpeechWidget::SetSeparationOffset(const float OffsetY)
{
    SeparationOffsetY = OffsetY;
    ApplyAnimatedState(LastAnimationOpacity, LastAnimationOffsetY, LastAnimationScale);
}

void UVHVAmbientSpeechWidget::TickPresentation(const float DeltaTime)
{
    if (!bBubbleVisible || (!bAppearing && !bDisappearing)) return;
    AnimationElapsed += DeltaTime;
    if (bAppearing)
    {
        const float Duration = CurrentType == EVHVAmbientSpeechType::Thought
            ? VHVAmbientSpeechStyle::ThoughtAppearDuration : VHVAmbientSpeechStyle::AppearDuration;
        const float Alpha = FMath::Clamp(AnimationElapsed / Duration, 0.0f, 1.0f);
        const float Eased = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f);
        ApplyAnimatedState(Eased, FMath::Lerp(VHVAmbientSpeechStyle::EntranceOffset, 0.0f, Eased),
            FMath::Lerp(VHVAmbientSpeechStyle::EntranceScale, 1.0f, Eased));
        if (Alpha >= 1.0f) bAppearing = false;
    }
    else
    {
        const float Alpha = FMath::Clamp(AnimationElapsed / VHVAmbientSpeechStyle::DisappearDuration, 0.0f, 1.0f);
        ApplyAnimatedState(1.0f - Alpha, 0.0f, 1.0f);
        if (Alpha >= 1.0f)
        {
            bDisappearing = false;
            bBubbleVisible = false;
            if (AnimatedRoot) AnimatedRoot->SetVisibility(EVisibility::Collapsed);
        }
    }
}

void UVHVAmbientSpeechWidget::RefreshBrushes()
{
    PanelBrush = FSlateRoundedBoxBrush(
        CurrentType == EVHVAmbientSpeechType::Thought ? VHVAmbientSpeechStyle::ThoughtBackground() : VHVAmbientSpeechStyle::SpeechBackground(),
        VHVAmbientSpeechStyle::CornerRadius, VHVAmbientSpeechStyle::Border(), 1.0f);
}

void UVHVAmbientSpeechWidget::ApplyAnimatedState(const float Opacity, const float OffsetY, const float Scale)
{
    if (!AnimatedRoot) return;
    LastAnimationOpacity = Opacity;
    LastAnimationOffsetY = OffsetY;
    LastAnimationScale = Scale;
    AnimatedRoot->SetRenderOpacity(Opacity * DistanceOpacity);
    AnimatedRoot->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
    AnimatedRoot->SetRenderTransform(FSlateRenderTransform(Scale, FVector2D(0.0f, OffsetY + SeparationOffsetY)));
}
