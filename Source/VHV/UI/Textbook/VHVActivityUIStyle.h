#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"

/** Canonical visual tokens shared by the VHV activity and narrative HUD. */
namespace VHVActivityUIStyle
{
    inline constexpr float QuestionPanelAlpha = 0.61f;
    inline constexpr float ChoiceCardAlpha = 0.57f;
    inline constexpr float QuestionPanelRadius = 26.0f;
    inline constexpr float ChoiceCardRadius = 41.0f;
    inline constexpr float ChoiceIndicatorDiameter = 66.0f;
    inline constexpr float IndicatorRadius = ChoiceIndicatorDiameter * 0.5f;
    inline constexpr float ChoiceCardHeight = 82.0f;
    inline constexpr float ChoiceContentLeftPadding = 8.0f;
    inline constexpr float ChoiceContentVerticalPadding = 8.0f;
    inline constexpr float IndicatorTextGap = 30.0f;
    inline constexpr float BorderNormalWidth = 1.0f;
    inline constexpr float BorderSelectedWidth = 1.75f;
    inline constexpr float PanelPadding = 32.0f;
    inline constexpr float ChoiceGap = 17.0f;
    inline constexpr float LegendGap = 14.0f;
    inline constexpr float HeaderIconDiameter = 19.0f;
    inline constexpr float HeaderDividerWidth = 205.0f;
    inline constexpr float HeaderDividerHeight = 1.0f;
    inline constexpr float AnimationFast = 0.12f;
    inline constexpr float AnimationStandard = 0.21f;
    inline constexpr float AnimationStagger = 0.03f;
    inline constexpr float OrderingCardWidth = 320.0f;
    inline constexpr float OrderingCardHeight = 380.0f;
    inline constexpr float OrderingCardRadius = 26.0f;
    inline constexpr float OrderingCardGap = 24.0f;
    inline constexpr float OrderingCardPadding = 24.0f;
    inline constexpr float OrderingBadgeDiameter = 44.0f;
    inline constexpr float OrderingMediaHeight = 176.0f;
    inline constexpr float OrderingGrabLift = 8.0f;
    inline constexpr float OrderingConfirmWidth = 230.0f;
    inline constexpr float OrderingConfirmHeight = 56.0f;
    inline constexpr float MatchingCardHeight = 92.0f;
    inline constexpr float MatchingCardWidth = 448.0f;
    inline constexpr float MatchingCardRadius = 18.0f;
    inline constexpr float MatchingCardGap = 16.0f;
    inline constexpr float MatchingCardPadding = 22.0f;
    inline constexpr float MatchingConnectorDiameter = 24.0f;
    inline constexpr float MatchingMediaWidth = 64.0f;
    inline constexpr float MatchingMediaHeight = 56.0f;
    inline constexpr float MatchingConfirmWidth = 196.0f;
    inline constexpr float MatchingConfirmHeight = 50.0f;
    inline constexpr int32 QuestionFontSize = 30;
    inline constexpr int32 OrderingTitleFontSize = 36;
    inline constexpr int32 MatchingTitleFontSize = 34;
    inline constexpr int32 MatchingCardFontSize = 20;
    inline constexpr int32 AnswerFontSize = 24;
    inline constexpr int32 InstructionFontSize = 17;
    inline constexpr int32 HeaderFontSize = 18;

    inline FLinearColor FromSRGB(const uint8 R, const uint8 G, const uint8 B, const uint8 A = 255)
    {
        FLinearColor Color = FLinearColor::FromSRGBColor(FColor(R, G, B, A));
        Color.A = static_cast<float>(A) / 255.0f;
        return Color;
    }

    inline FLinearColor GlassMain()
    {
        return FLinearColor(0.045f, 0.052f, 0.060f, QuestionPanelAlpha);
    }

    inline FLinearColor GlassCard()
    {
        FLinearColor Color = FromSRGB(26, 31, 35);
        Color.A = ChoiceCardAlpha;
        return Color;
    }
    inline FLinearColor GlassHover() { return FromSRGB(29, 35, 39, 151); }
    inline FLinearColor GlassFocused() { return FromSRGB(35, 34, 30, 158); }
    inline FLinearColor GlassSelected() { return FromSRGB(44, 38, 29, 166); }
    inline FLinearColor TextPrimary() { return FromSRGB(240, 236, 228); }
    inline FLinearColor TextSecondary() { return FromSRGB(198, 192, 181, 217); }
    inline FLinearColor GoldPrimary() { return FromSRGB(224, 187, 105); }
    inline FLinearColor GoldSelected() { return FromSRGB(225, 187, 107); }
    inline FLinearColor BorderNeutral() { return FromSRGB(195, 190, 179, 82); }
    inline FLinearColor BorderFocused() { return FromSRGB(214, 190, 145, 133); }
    inline FLinearColor BorderGold() { return FromSRGB(225, 187, 107, 224); }
    inline FLinearColor PanelBorder() { return FromSRGB(190, 179, 158, 69); }
    inline FLinearColor DividerGold() { return FromSRGB(215, 179, 101, 184); }
    inline FLinearColor HeaderGold() { return FromSRGB(224, 187, 105, 217); }
    inline FLinearColor IndicatorNormal() { return FromSRGB(20, 24, 27, 145); }
    inline FLinearColor IndicatorSelected() { return FromSRGB(44, 38, 29, 176); }
    inline FLinearColor SelectedGlow() { return FromSRGB(161, 119, 54, 25); }
    inline FLinearColor PositiveMuted() { return FromSRGB(104, 153, 111); }
    inline FLinearColor NegativeMuted() { return FromSRGB(181, 105, 91); }
    inline FLinearColor DisabledText() { return FromSRGB(123, 124, 121, 190); }
    inline FLinearColor WarningText() { return FromSRGB(224, 163, 105); }
    inline FLinearColor ShadowPanel() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.20f); }
    inline FLinearColor ShadowCard() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.17f); }

    // Matching sits on the baked cream paper board, so it uses ink and paper
    // variants of the canonical VHV gold, green, border, and shadow language.
    inline FLinearColor MatchingPaperCard() { return FromSRGB(248, 244, 234, 242); }
    inline FLinearColor MatchingPaperHover() { return FromSRGB(255, 251, 240, 248); }
    inline FLinearColor MatchingPaperFocused() { return FromSRGB(255, 248, 229, 250); }
    inline FLinearColor MatchingPaperSelected() { return FromSRGB(250, 239, 211, 250); }
    inline FLinearColor MatchingInk() { return FromSRGB(28, 39, 42); }
    inline FLinearColor MatchingInkMuted() { return FromSRGB(88, 89, 82, 222); }
    inline FLinearColor MatchingPaperBorder() { return FromSRGB(132, 119, 96, 92); }
    inline FLinearColor MatchingConnectorFill() { return FromSRGB(239, 232, 215, 252); }
    inline FLinearColor MatchingShadow() { return FLinearColor(0.035f, 0.027f, 0.018f, 0.16f); }

    // Compatibility aliases used by the existing feedback presentation.
    inline constexpr float QuestionPanelOpacity = QuestionPanelAlpha;
    inline constexpr float ChoiceCardOpacity = ChoiceCardAlpha;
    inline constexpr float IndicatorDiameter = ChoiceIndicatorDiameter;
    inline constexpr float BorderThin = BorderNormalWidth;
    inline constexpr float BorderSelected = BorderSelectedWidth;
    inline constexpr int32 LabelFontSize = HeaderFontSize;
    inline FLinearColor CharcoalGlass() { return GlassMain(); }
    inline FLinearColor GlassSecondary() { return GlassCard(); }
    inline FLinearColor CardNormal() { return GlassCard(); }
    inline FLinearColor CardHover() { return GlassHover(); }
    inline FLinearColor CardFocused() { return GlassFocused(); }
    inline FLinearColor CardSelected() { return GlassSelected(); }
    inline FLinearColor Gold() { return GoldPrimary(); }
    inline FLinearColor GoldActive() { return GoldSelected(); }
    inline FLinearColor ActiveGold() { return GoldSelected(); }
    inline FLinearColor SoftGold() { return PanelBorder(); }
    inline FLinearColor PaleBorder() { return BorderNeutral(); }
    inline FLinearColor PrimaryText() { return TextPrimary(); }
    inline FLinearColor SecondaryText() { return TextSecondary(); }
    inline FLinearColor SoftShadow() { return ShadowPanel(); }

    inline FSlateFontInfo RegularFont(const int32 Size)
    {
        return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
    }

    inline FSlateFontInfo MediumFont(const int32 Size)
    {
        return FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
    }

    inline FSlateBrush RoundedBrush(
        const FLinearColor& Fill,
        const float Radius,
        const FLinearColor& Outline = FLinearColor::Transparent,
        const float OutlineWidth = 0.0f)
    {
        return FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth);
    }
}
