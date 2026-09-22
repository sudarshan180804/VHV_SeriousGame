#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"

namespace VHVAmbientSpeechStyle
{
    inline constexpr float MinWidth = 180.0f;
    inline constexpr float MaxWidth = 400.0f;
    inline constexpr float CornerRadius = 20.0f;
    inline constexpr float PaddingHorizontal = 18.0f;
    inline constexpr float PaddingVertical = 14.0f;
    inline constexpr float AppearDuration = 0.18f;
    inline constexpr float ThoughtAppearDuration = 0.22f;
    inline constexpr float DisappearDuration = 0.14f;
    inline constexpr float EntranceOffset = 8.0f;
    inline constexpr float EntranceScale = 0.97f;
    inline constexpr float DefaultFallbackAnchorHeight = 135.0f;
    inline constexpr float DefaultHeadSocketLift = 18.0f;
    inline constexpr float FadeStartDistance = 1400.0f;
    inline constexpr float MaxVisibleDistance = 2300.0f;
    inline constexpr float AutoDurationPerCharacter = 0.052f;
    inline constexpr float AutoDurationMin = 2.0f;
    inline constexpr float AutoDurationMax = 7.0f;
    inline constexpr int32 BodyFontSize = 19;
    inline constexpr int32 SpeakerFontSize = 14;

    inline FLinearColor FromSRGB(uint8 R, uint8 G, uint8 B, uint8 A = 255)
    {
        FLinearColor Color = FLinearColor::FromSRGBColor(FColor(R, G, B, A));
        Color.A = static_cast<float>(A) / 255.0f;
        return Color;
    }

    inline FLinearColor SpeechBackground() { return FromSRGB(241, 233, 218, 240); }
    inline FLinearColor ThoughtBackground() { return FromSRGB(238, 232, 221, 232); }
    inline FLinearColor Text() { return FromSRGB(36, 40, 43); }
    inline FLinearColor Speaker() { return FromSRGB(161, 126, 70); }
    inline FLinearColor Border() { return FromSRGB(188, 166, 107, 89); }
    inline FLinearColor Shadow() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.16f); }
    inline FSlateFontInfo RegularFont(int32 Size) { return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size); }
    inline FSlateFontInfo MediumFont(int32 Size) { return FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size); }
}
