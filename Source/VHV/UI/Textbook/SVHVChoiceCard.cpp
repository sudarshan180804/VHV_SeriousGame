#include "UI/Textbook/SVHVChoiceCard.h"

#include "UI/Textbook/VHVActivityUIStyle.h"
#include "InputCoreTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SVHVChoiceCard::Construct(const FArguments& InArgs)
{
    OptionIndex = InArgs._OptionIndex;
    OnChosen = InArgs._OnChosen;
    EntranceDelay = InArgs._EntranceDelay;
    CardBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassCard(), VHVActivityUIStyle::ChoiceCardRadius,
        VHVActivityUIStyle::BorderNeutral(), VHVActivityUIStyle::BorderNormalWidth);
    ShadowBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::ShadowCard(), VHVActivityUIStyle::ChoiceCardRadius + 2.0f);
    IndicatorShadowBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::ShadowCard(), VHVActivityUIStyle::IndicatorRadius);
    IndicatorBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::IndicatorNormal(), VHVActivityUIStyle::IndicatorRadius,
        VHVActivityUIStyle::BorderNeutral(), VHVActivityUIStyle::BorderNormalWidth);

    TSharedRef<SOverlay> CardContent =
        SNew(SOverlay)
            + SOverlay::Slot()
            .Padding(FMargin(3.0f, 3.0f, -3.0f, -4.0f))
            [
                SNew(SBorder)
                .BorderImage(this, &SVHVChoiceCard::GetShadowBrush)
            ]
            + SOverlay::Slot()
            [
                SAssignNew(CardBorder, SBorder)
                .BorderImage(this, &SVHVChoiceCard::GetCardBrush)
                .VAlign(VAlign_Center)
                .Padding(FMargin(
                    VHVActivityUIStyle::ChoiceContentLeftPadding,
                    VHVActivityUIStyle::ChoiceContentVerticalPadding,
                    30.0f,
                    VHVActivityUIStyle::ChoiceContentVerticalPadding))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, VHVActivityUIStyle::IndicatorTextGap, 0.0f))
                    [
                        SNew(SBox)
                        .WidthOverride(VHVActivityUIStyle::ChoiceIndicatorDiameter)
                        .HeightOverride(VHVActivityUIStyle::ChoiceIndicatorDiameter)
                        [
                            SNew(SOverlay)
                            + SOverlay::Slot()
                            [
                                SNew(SBorder)
                                .BorderImage(this, &SVHVChoiceCard::GetIndicatorShadowBrush)
                            ]
                            + SOverlay::Slot()
                            [
                                SNew(SBorder)
                                .BorderImage(this, &SVHVChoiceCard::GetIndicatorBrush)
                                .HAlign(HAlign_Center)
                                .VAlign(VAlign_Center)
                                [
                                    SAssignNew(IndicatorText, STextBlock)
                                    .Font(VHVActivityUIStyle::MediumFont(20))
                                    .ColorAndOpacity(this, &SVHVChoiceCard::GetIndicatorColor)
                                    .Text(this, &SVHVChoiceCard::GetIndicatorText)
                                ]
                            ]
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SAssignNew(AnswerText, STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(VHVActivityUIStyle::AnswerFontSize))
                        .ColorAndOpacity(this, &SVHVChoiceCard::GetAnswerColor)
                        .AutoWrapText(true)
                        .LineHeightPercentage(1.08f)
                        .Text(InArgs._AnswerText)
                    ]
                ]
            ];

    if (InArgs._AllowVariableHeight)
    {
        ChildSlot
        [
            SNew(SBox)
            .MinDesiredHeight(VHVActivityUIStyle::ChoiceCardHeight)
            [
                CardContent
            ]
        ];
    }
    else
    {
        ChildSlot
        [
            SNew(SBox)
            .HeightOverride(VHVActivityUIStyle::ChoiceCardHeight)
            [
                CardContent
            ]
        ];
    }

    SetRenderOpacity(0.0f);
    SetRenderTransform(FSlateRenderTransform(FVector2D(16.0f, 0.0f)));
}

void SVHVChoiceCard::SetPresentationState(
    const bool bInFocused,
    const bool bInSelected,
    const bool bInEnabled,
    const bool bInMultiChoice)
{
    bFocused = bInFocused;
    bSelected = bInSelected;
    bEnabled = bInEnabled;
    bMultiChoice = bInMultiChoice;
    SetEnabled(bEnabled);
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SVHVChoiceCard::Tick(
    const FGeometry& AllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

    EntranceElapsed += InDeltaTime;
    const float EntranceAlpha = FMath::Clamp(
        (EntranceElapsed - EntranceDelay) / VHVActivityUIStyle::AnimationStandard, 0.0f, 1.0f);
    const float SmoothedEntrance = FMath::InterpEaseOut(0.0f, 1.0f, EntranceAlpha, 3.0f);
    SetRenderOpacity(SmoothedEntrance);

    RefreshBrushes(InDeltaTime);
    const float FocusOffset = bFocused ? 3.0f : 0.0f;
    SetRenderTransform(FSlateRenderTransform(FVector2D(
        FMath::Lerp(16.0f, FocusOffset, SmoothedEntrance), 0.0f)));
}

FReply SVHVChoiceCard::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    if (bEnabled && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bPressed = true;
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }
    return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SVHVChoiceCard::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    if (bPressed && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bPressed = false;
        if (MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
        {
            OnChosen.ExecuteIfBound(OptionIndex);
        }
        return FReply::Handled().ReleaseMouseCapture();
    }
    return SCompoundWidget::OnMouseButtonUp(MyGeometry, MouseEvent);
}

void SVHVChoiceCard::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    bHovered = true;
    SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
}

void SVHVChoiceCard::OnMouseLeave(const FPointerEvent& MouseEvent)
{
    bHovered = false;
    bPressed = false;
    SCompoundWidget::OnMouseLeave(MouseEvent);
}

void SVHVChoiceCard::RefreshBrushes(const float DeltaTime)
{
    const float TargetEmphasis = bSelected ? 1.0f : (bFocused ? 0.72f : (bHovered ? 0.38f : 0.0f));
    Emphasis = FMath::FInterpTo(
        Emphasis, TargetEmphasis, DeltaTime, 1.0f / VHVActivityUIStyle::AnimationFast);

    FLinearColor TargetFill = bSelected ? VHVActivityUIStyle::GlassSelected()
        : (bFocused ? VHVActivityUIStyle::GlassFocused()
        : (bHovered ? VHVActivityUIStyle::GlassHover() : VHVActivityUIStyle::GlassCard()));
    FLinearColor TargetOutline = bSelected
        ? VHVActivityUIStyle::BorderGold()
        : (bFocused ? VHVActivityUIStyle::BorderFocused() : VHVActivityUIStyle::BorderNeutral());

    if (!bEnabled)
    {
        TargetFill = VHVActivityUIStyle::FromSRGB(25, 29, 32, 118);
        TargetOutline = VHVActivityUIStyle::FromSRGB(123, 124, 121, 58);
    }

    const float TargetOutlineWidth = bSelected
        ? VHVActivityUIStyle::BorderSelectedWidth
        : (bFocused ? 1.25f : VHVActivityUIStyle::BorderNormalWidth);
    const float OutlineWidth = FMath::Lerp(
        VHVActivityUIStyle::BorderNormalWidth, TargetOutlineWidth, Emphasis);
    CardBrush = VHVActivityUIStyle::RoundedBrush(
        TargetFill, VHVActivityUIStyle::ChoiceCardRadius, TargetOutline, OutlineWidth);
    ShadowBrush = VHVActivityUIStyle::RoundedBrush(
        bSelected ? VHVActivityUIStyle::SelectedGlow() : VHVActivityUIStyle::ShadowCard(),
        VHVActivityUIStyle::ChoiceCardRadius + 2.0f);
    IndicatorShadowBrush = VHVActivityUIStyle::RoundedBrush(
        bSelected ? VHVActivityUIStyle::SelectedGlow() : VHVActivityUIStyle::ShadowCard(),
        VHVActivityUIStyle::IndicatorRadius);

    const FLinearColor IndicatorFill = bSelected
        ? VHVActivityUIStyle::IndicatorSelected()
        : VHVActivityUIStyle::IndicatorNormal();
    IndicatorBrush = VHVActivityUIStyle::RoundedBrush(
        IndicatorFill,
        VHVActivityUIStyle::IndicatorRadius,
        bSelected ? VHVActivityUIStyle::BorderGold()
            : (bFocused ? VHVActivityUIStyle::BorderFocused() : VHVActivityUIStyle::BorderNeutral()),
        bSelected ? VHVActivityUIStyle::BorderSelectedWidth
            : (bFocused ? 1.25f : VHVActivityUIStyle::BorderNormalWidth));

    if (CardBorder)
    {
        CardBorder->Invalidate(EInvalidateWidgetReason::Paint);
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

FSlateColor SVHVChoiceCard::GetAnswerColor() const
{
    return bEnabled
        ? FSlateColor(bSelected || bFocused || bHovered
            ? VHVActivityUIStyle::PrimaryText()
            : VHVActivityUIStyle::SecondaryText())
        : FSlateColor(VHVActivityUIStyle::DisabledText());
}

FSlateColor SVHVChoiceCard::GetIndicatorColor() const
{
    return FSlateColor(bSelected
        ? VHVActivityUIStyle::GoldActive()
        : VHVActivityUIStyle::TextPrimary());
}

FText SVHVChoiceCard::GetIndicatorText() const
{
    if (bMultiChoice)
    {
        return bSelected ? FText::FromString(TEXT("\u2713")) : FText::GetEmpty();
    }

    const TCHAR Letter = static_cast<TCHAR>(TEXT('A') + FMath::Clamp(OptionIndex, 0, 25));
    return FText::FromString(FString::Chr(Letter));
}
