#include "Cinematics/VHVTimeSkipOverlayWidget.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> UVHVTimeSkipOverlayWidget::RebuildWidget()
{
    return SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
            .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.94f))
        ]
        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SAssignNew(TransitionTextBlock, STextBlock)
            .ColorAndOpacity(FLinearColor::White)
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 36))
            .Justification(ETextJustify::Center)
        ];
}

void UVHVTimeSkipOverlayWidget::PlayTransition(
    const FText& InText,
    const float InFadeInDuration,
    const float InHoldDuration,
    const float InFadeOutDuration)
{
    FadeInDuration = FMath::Max(0.01f, InFadeInDuration);
    HoldDuration = FMath::Max(0.0f, InHoldDuration);
    FadeOutDuration = FMath::Max(0.01f, InFadeOutDuration);
    Elapsed = 0.0f;
    bPlaying = true;
    bObscuredEventSent = false;
    if (TransitionTextBlock)
    {
        TransitionTextBlock->SetText(InText.IsEmpty() ? FText::FromString(TEXT("Afterwards...")) : InText);
    }
    SetRenderOpacity(0.0f);
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UVHVTimeSkipOverlayWidget::CancelTransition()
{
    bPlaying = false;
    bObscuredEventSent = false;
    Elapsed = 0.0f;
    SetRenderOpacity(0.0f);
    SetVisibility(ESlateVisibility::Collapsed);
}

void UVHVTimeSkipOverlayWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bPlaying)
    {
        return;
    }

    Elapsed += FMath::Max(0.0f, InDeltaTime);
    const float FadeInEnd = FadeInDuration;
    const float HoldEnd = FadeInEnd + HoldDuration;
    const float TransitionEnd = HoldEnd + FadeOutDuration;
    float Opacity = 1.0f;

    if (Elapsed < FadeInEnd)
    {
        Opacity = FMath::Clamp(Elapsed / FadeInDuration, 0.0f, 1.0f);
    }
    else
    {
        if (!bObscuredEventSent)
        {
            bObscuredEventSent = true;
            OnFullyObscured.Broadcast();
        }
        if (Elapsed > HoldEnd)
        {
            Opacity = 1.0f - FMath::Clamp((Elapsed - HoldEnd) / FadeOutDuration, 0.0f, 1.0f);
        }
    }
    SetRenderOpacity(Opacity);

    if (Elapsed >= TransitionEnd)
    {
        bPlaying = false;
        SetRenderOpacity(0.0f);
        SetVisibility(ESlateVisibility::Collapsed);
        OnTransitionFinished.Broadcast();
    }
}
