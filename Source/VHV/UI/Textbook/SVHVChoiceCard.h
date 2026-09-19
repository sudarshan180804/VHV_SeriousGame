#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SBorder;
class STextBlock;

DECLARE_DELEGATE_OneParam(FOnVHVChoiceCardChosen, int32);

/** Reusable visual choice row shared by single-choice and multiple-choice activities. */
class SVHVChoiceCard : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SVHVChoiceCard)
        : _OptionIndex(INDEX_NONE)
        , _AnswerText()
        , _EntranceDelay(0.0f)
    {}
        SLATE_ARGUMENT(int32, OptionIndex)
        SLATE_ARGUMENT(FText, AnswerText)
        SLATE_ARGUMENT(float, EntranceDelay)
        SLATE_EVENT(FOnVHVChoiceCardChosen, OnChosen)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetPresentationState(bool bInFocused, bool bInSelected, bool bInEnabled, bool bInMultiChoice);

    virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

private:
    const FSlateBrush* GetCardBrush() const { return &CardBrush; }
    const FSlateBrush* GetShadowBrush() const { return &ShadowBrush; }
    const FSlateBrush* GetIndicatorShadowBrush() const { return &IndicatorShadowBrush; }
    const FSlateBrush* GetIndicatorBrush() const { return &IndicatorBrush; }
    FSlateColor GetAnswerColor() const;
    FSlateColor GetIndicatorColor() const;
    FText GetIndicatorText() const;
    void RefreshBrushes(float DeltaTime);

    int32 OptionIndex = INDEX_NONE;
    FOnVHVChoiceCardChosen OnChosen;
    TSharedPtr<SBorder> CardBorder;
    TSharedPtr<STextBlock> IndicatorText;
    TSharedPtr<STextBlock> AnswerText;
    FSlateBrush CardBrush;
    FSlateBrush ShadowBrush;
    FSlateBrush IndicatorShadowBrush;
    FSlateBrush IndicatorBrush;
    bool bFocused = false;
    bool bSelected = false;
    bool bEnabled = true;
    bool bMultiChoice = false;
    bool bHovered = false;
    bool bPressed = false;
    float Emphasis = 0.0f;
    float EntranceDelay = 0.0f;
    float EntranceElapsed = 0.0f;
};
