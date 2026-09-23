#include "UI/Textbook/VHVObservationWidget.h"

#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UI/VHVUIManagerComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    constexpr float BoardLeft = 0.190f;
    constexpr float BoardRight = 0.820f;
    constexpr float HeaderLeft = 0.295f;
    constexpr float HeaderTop = 0.145f;
    constexpr float HeaderRight = 0.745f;
    constexpr float HeaderBottom = 0.355f;
    constexpr float ItemsLeft = 0.205f;
    constexpr float ItemsTop = 0.370f;
    constexpr float ItemsRight = 0.815f;
    constexpr float ItemsBottom = 0.680f;
    constexpr float BoardActionY = 0.735f;
    constexpr float ContinueWidth = 190.0f;
    constexpr float ContinueHeight = 52.0f;
    constexpr float ContinueClusterWidth = 314.0f;
    constexpr float CardMinHeight = 152.0f;
    constexpr float EvidenceRevealStagger = 0.12f;

    FString CleanText(const FString& Text)
    {
        return Text.TrimStartAndEnd();
    }

    bool TextMatches(const FString& A, const FString& B)
    {
        const FString CleanA = CleanText(A);
        const FString CleanB = CleanText(B);
        return !CleanA.IsEmpty() && CleanA.Equals(CleanB, ESearchCase::IgnoreCase);
    }

    bool ContainsEquivalent(const TArray<FString>& Texts, const FString& Candidate)
    {
        return Texts.ContainsByPredicate(
            [&Candidate](const FString& Existing)
            {
                return TextMatches(Existing, Candidate);
            });
    }
}

DECLARE_DELEGATE_OneParam(FOnVHVEvidenceCardChosen, int32);

/** Paper-surface multi-select card used only by Observation evidence tagging. */
class SVHVEvidenceTagCard : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SVHVEvidenceTagCard)
        : _CardIndex(INDEX_NONE), _Text(), _Correct(false)
    {}
        SLATE_ARGUMENT(int32, CardIndex)
        SLATE_ARGUMENT(FText, Text)
        SLATE_ARGUMENT(bool, Correct)
        SLATE_EVENT(FOnVHVEvidenceCardChosen, OnChosen)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        CardIndex = InArgs._CardIndex;
        bCorrect = InArgs._Correct;
        OnChosen = InArgs._OnChosen;
        RefreshBrushes();

        ChildSlot
        [
            SNew(SBox)
            .MinDesiredHeight(78.0f)
            [
                SAssignNew(CardBorder, SBorder)
                .BorderImage(this, &SVHVEvidenceTagCard::GetCardBrush)
                .Padding(FMargin(17.0f, 14.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(0.0f, 0.0f, 13.0f, 0.0f))
                    [
                        SNew(SBox)
                        .WidthOverride(28.0f)
                        .HeightOverride(28.0f)
                        [
                            SNew(SBorder)
                            .BorderImage(this, &SVHVEvidenceTagCard::GetMarkerBrush)
                            .HAlign(HAlign_Center)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                .Font(VHVActivityUIStyle::MediumFont(14))
                                .ColorAndOpacity(this, &SVHVEvidenceTagCard::GetMarkerColor)
                                .Text(this, &SVHVEvidenceTagCard::GetMarkerText)
                            ]
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(17))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                        .AutoWrapText(true)
                        .LineHeightPercentage(1.16f)
                        .Text(InArgs._Text)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(14.0f, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(12))
                        .ColorAndOpacity(this, &SVHVEvidenceTagCard::GetResultColor)
                        .Visibility(this, &SVHVEvidenceTagCard::GetResultVisibility)
                        .Text(this, &SVHVEvidenceTagCard::GetResultText)
                    ]
                ]
            ]
        ];
    }

    void SetPresentationState(
        const bool bInFocused,
        const bool bInSelected,
        const bool bInReveal,
        const bool bInLocked)
    {
        bFocused = bInFocused;
        bSelected = bInSelected;
        bReveal = bInReveal;
        SetEnabled(!bInLocked);
        RefreshBrushes();
    }

    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
    {
        if (IsEnabled() && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
        {
            bPressed = true;
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }
        return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
    }

    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
    {
        if (bPressed && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
        {
            bPressed = false;
            if (MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
            {
                OnChosen.ExecuteIfBound(CardIndex);
            }
            return FReply::Handled().ReleaseMouseCapture();
        }
        return SCompoundWidget::OnMouseButtonUp(MyGeometry, MouseEvent);
    }

    virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
    {
        bHovered = true;
        RefreshBrushes();
        SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
    }

    virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
    {
        bHovered = false;
        bPressed = false;
        RefreshBrushes();
        SCompoundWidget::OnMouseLeave(MouseEvent);
    }

private:
    const FSlateBrush* GetCardBrush() const { return &CardBrush; }
    const FSlateBrush* GetMarkerBrush() const { return &MarkerBrush; }
    FSlateColor GetMarkerColor() const { return MarkerColor; }
    FSlateColor GetResultColor() const { return ResultColor; }
    EVisibility GetResultVisibility() const
    {
        return bReveal && (bSelected || bCorrect) ? EVisibility::Visible : EVisibility::Collapsed;
    }
    FText GetResultText() const
    {
        if (!bReveal) return FText::GetEmpty();
        if (bSelected && bCorrect) return FText::FromString(TEXT("CORRECT"));
        if (bSelected) return FText::FromString(TEXT("INCORRECT"));
        if (bCorrect) return FText::FromString(TEXT("MISSED"));
        return FText::GetEmpty();
    }
    FText GetMarkerText() const
    {
        if (bReveal)
        {
            if (bSelected && bCorrect) return FText::FromString(TEXT("\u2713"));
            if (bSelected) return FText::FromString(TEXT("\u00D7"));
            if (bCorrect) return FText::FromString(TEXT("!"));
            return FText::GetEmpty();
        }
        return bSelected ? FText::FromString(TEXT("\u2713")) : FText::GetEmpty();
    }

    void RefreshBrushes()
    {
        FLinearColor Fill = VHVActivityUIStyle::MatchingPaperCard();
        FLinearColor Outline = VHVActivityUIStyle::MatchingPaperBorder();
        float OutlineWidth = VHVActivityUIStyle::BorderNormalWidth;
        FLinearColor MarkerFill = VHVActivityUIStyle::MatchingConnectorFill();
        FLinearColor MarkerOutline = VHVActivityUIStyle::MatchingPaperBorder();
        MarkerColor = VHVActivityUIStyle::MatchingInkMuted();
        ResultColor = VHVActivityUIStyle::MatchingInkMuted();

        if (bReveal && bSelected && bCorrect)
        {
            Fill = VHVActivityUIStyle::FromSRGB(222, 232, 215, 246);
            Outline = VHVActivityUIStyle::PositiveMuted().CopyWithNewOpacity(0.78f);
            MarkerFill = VHVActivityUIStyle::PositiveMuted().CopyWithNewOpacity(0.18f);
            MarkerOutline = VHVActivityUIStyle::PositiveMuted();
            MarkerColor = VHVActivityUIStyle::PositiveMuted();
            ResultColor = VHVActivityUIStyle::PositiveMuted();
            OutlineWidth = 1.6f;
        }
        else if (bReveal && bSelected)
        {
            Fill = VHVActivityUIStyle::FromSRGB(242, 224, 218, 246);
            Outline = VHVActivityUIStyle::NegativeMuted().CopyWithNewOpacity(0.78f);
            MarkerFill = VHVActivityUIStyle::NegativeMuted().CopyWithNewOpacity(0.14f);
            MarkerOutline = VHVActivityUIStyle::NegativeMuted();
            MarkerColor = VHVActivityUIStyle::NegativeMuted();
            ResultColor = VHVActivityUIStyle::NegativeMuted();
            OutlineWidth = 1.6f;
        }
        else if (bReveal && bCorrect)
        {
            Fill = VHVActivityUIStyle::FromSRGB(247, 235, 203, 246);
            Outline = VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.88f);
            MarkerFill = VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.18f);
            MarkerOutline = VHVActivityUIStyle::GoldPrimary();
            MarkerColor = VHVActivityUIStyle::GoldPrimary();
            ResultColor = VHVActivityUIStyle::GoldPrimary();
            OutlineWidth = 1.6f;
        }
        else if (bReveal)
        {
            Fill = VHVActivityUIStyle::MatchingPaperCard().CopyWithNewOpacity(0.62f);
            MarkerFill = VHVActivityUIStyle::MatchingConnectorFill().CopyWithNewOpacity(0.55f);
            MarkerColor = VHVActivityUIStyle::MatchingInkMuted().CopyWithNewOpacity(0.55f);
        }
        else if (bSelected)
        {
            Fill = VHVActivityUIStyle::MatchingPaperSelected();
            Outline = VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.82f);
            MarkerFill = VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.16f);
            MarkerOutline = VHVActivityUIStyle::GoldPrimary();
            MarkerColor = VHVActivityUIStyle::GoldPrimary();
            OutlineWidth = 1.6f;
        }
        else if (bFocused || bHovered)
        {
            Fill = bFocused ? VHVActivityUIStyle::MatchingPaperFocused() : VHVActivityUIStyle::MatchingPaperHover();
            Outline = VHVActivityUIStyle::BorderFocused();
            MarkerOutline = VHVActivityUIStyle::BorderFocused();
        }

        CardBrush = VHVActivityUIStyle::RoundedBrush(Fill, 16.0f, Outline, OutlineWidth);
        MarkerBrush = VHVActivityUIStyle::RoundedBrush(MarkerFill, 14.0f, MarkerOutline, OutlineWidth);
        if (CardBorder) CardBorder->Invalidate(EInvalidateWidgetReason::Paint);
        Invalidate(EInvalidateWidgetReason::Paint);
    }

    int32 CardIndex = INDEX_NONE;
    bool bCorrect = false;
    bool bFocused = false;
    bool bSelected = false;
    bool bReveal = false;
    bool bHovered = false;
    bool bPressed = false;
    FOnVHVEvidenceCardChosen OnChosen;
    TSharedPtr<SBorder> CardBorder;
    FSlateBrush CardBrush;
    FSlateBrush MarkerBrush;
    FSlateColor MarkerColor;
    FSlateColor ResultColor;
};

UVHVObservationWidget::UVHVObservationWidget()
{
    SetIsFocusable(true);

    static ConstructorHelpers::FObjectFinder<UTexture2D> BackgroundFinder(
        TEXT("/Game/VHV_Stuff/UI/Backgrounds/T_BG_ObservationBoard.T_BG_ObservationBoard"));
    if (BackgroundFinder.Succeeded())
    {
        ObservationBackgroundTexture = BackgroundFinder.Object;
    }
}

TSharedRef<SWidget> UVHVObservationWidget::RebuildWidget()
{
    BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
    BackgroundBrush.SetResourceObject(ObservationBackgroundTexture);
    if (ObservationBackgroundTexture)
    {
        BackgroundBrush.SetImageSize(FVector2D(
            ObservationBackgroundTexture->GetSizeX(), ObservationBackgroundTexture->GetSizeY()));
    }

    HeaderDividerBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::DividerGold(), 1.0f);
    CardSurfaceBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::MatchingPaperCard(), 18.0f,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);
    CardAccentBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::PositiveMuted().CopyWithNewOpacity(0.72f), 2.0f);
    CardShadowBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::MatchingShadow().CopyWithNewOpacity(0.12f), 20.0f);
    KeycapBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(248, 244, 234, 225), 5.0f,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);

    ContinueButtonStyle = FButtonStyle()
        .SetNormal(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.94f),
            ContinueHeight * 0.5f,
            VHVActivityUIStyle::GoldSelected(), VHVActivityUIStyle::BorderNormalWidth))
        .SetHovered(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::GoldSelected(), ContinueHeight * 0.5f,
            VHVActivityUIStyle::MatchingInk().CopyWithNewOpacity(0.30f),
            VHVActivityUIStyle::BorderNormalWidth))
        .SetPressed(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::FromSRGB(202, 161, 79), ContinueHeight * 0.5f,
            VHVActivityUIStyle::MatchingInk().CopyWithNewOpacity(0.42f),
            VHVActivityUIStyle::BorderNormalWidth))
        .SetNormalPadding(FMargin(0.0f))
        .SetPressedPadding(FMargin(1.0f, 2.0f, 0.0f, 0.0f));

    TSharedRef<SWidget> Result =
        SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SScaleBox)
            .Stretch(EStretch::ScaleToFill)
            .Clipping(EWidgetClipping::ClipToBounds)
            [
                SNew(SImage)
                .Image(&BackgroundBrush)
            ]
        ]
        + SOverlay::Slot()
        [
            SAssignNew(ActivityContentSlate, SConstraintCanvas)
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(HeaderLeft, HeaderTop, HeaderRight, HeaderBottom))
            .Offset(FMargin(0.0f))
            [
                SNew(SScrollBox)
                .Orientation(Orient_Vertical)
                .ScrollBarThickness(FVector2D(3.0f, 3.0f))
                + SScrollBox::Slot()
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Left)
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(16))
                        .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
                        .Text(FText::FromString(TEXT("OBSERVATION")))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Fill)
                    .Padding(FMargin(0.0f, 5.0f, 0.0f, 0.0f))
                    [
                        SAssignNew(PrimaryTitleSlate, STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(37))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                        .AutoWrapText(true)
                        .LineHeightPercentage(1.05f)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Left)
                    .Padding(FMargin(0.0f, 13.0f, 0.0f, 17.0f))
                    [
                        SNew(SBox)
                        .WidthOverride(280.0f)
                        .HeightOverride(VHVActivityUIStyle::HeaderDividerHeight)
                        [
                            SNew(SBorder)
                            .BorderImage(&HeaderDividerBrush)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(SecondaryTextSlate, SVerticalBox)
                    ]
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(ItemsLeft, ItemsTop, ItemsRight, ItemsBottom))
            .Offset(FMargin(0.0f))
            [
                SNew(SScrollBox)
                .Orientation(Orient_Vertical)
                .ScrollBarThickness(FVector2D(3.0f, 3.0f))
                + SScrollBox::Slot()
                .Padding(FMargin(0.0f, 6.0f, 0.0f, 8.0f))
                [
                    SAssignNew(ObservationItemsSlate, SVerticalBox)
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(BoardLeft, BoardActionY))
            .Alignment(FVector2D(0.0f, 0.5f))
            .Offset(FMargin(0.0f, 0.0f, 760.0f, 40.0f))
            [
                SAssignNew(BoardInstructionSlate, STextBlock)
                .Font(VHVActivityUIStyle::RegularFont(13))
                .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted().CopyWithNewOpacity(0.72f))
                .AutoWrapText(true)
                .Text(FText::FromString(TEXT("Take a moment to review the information.")))
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(BoardRight, BoardActionY))
            .Alignment(FVector2D(1.0f, 0.5f))
            .Offset(FMargin(0.0f, 0.0f, ContinueClusterWidth, ContinueHeight))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
                [
                    SNew(SBorder)
                    .BorderImage(&KeycapBrush)
                    .Padding(FMargin(7.0f, 3.0f))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(11))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                        .Text(FText::FromString(TEXT("ENTER / SPACE")))
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBox)
                    .WidthOverride(ContinueWidth)
                    .HeightOverride(ContinueHeight)
                    [
                        SAssignNew(ActionButtonSlate, SButton)
                        .ButtonStyle(&ContinueButtonStyle)
                        .IsEnabled(TAttribute<bool>::Create(
                            TAttribute<bool>::FGetter::CreateUObject(
                                this, &UVHVObservationWidget::CanAdvanceObservation)))
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .ContentPadding(FMargin(22.0f, 10.0f))
                        .OnClicked(FOnClicked::CreateUObject(
                            this, &UVHVObservationWidget::HandleContinueClicked))
                        [
                            SAssignNew(ActionButtonTextSlate, STextBlock)
                            .Font(VHVActivityUIStyle::MediumFont(16))
                            .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                            .Text(FText::FromString(TEXT("Continue  \u2192")))
                        ]
                    ]
                ]
            ]
        ];

    RefreshSlateContent();
    return Result;
}

void UVHVObservationWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    ActivityContentSlate.Reset();
    PrimaryTitleSlate.Reset();
    SecondaryTextSlate.Reset();
    ObservationItemsSlate.Reset();
    BoardInstructionSlate.Reset();
    ActionButtonTextSlate.Reset();
    ActionButtonSlate.Reset();
    ObservationCardsSlate.Reset();
    EvidenceCardsSlate.Reset();
    MediaBrushes.Reset();
    LoadedMediaTextures.Reset();
}

void UVHVObservationWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EntranceElapsed = 0.0f;
    if (ActivityContentSlate)
    {
        ActivityContentSlate->SetRenderOpacity(0.0f);
        ActivityContentSlate->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, 10.0f)));
    }
    if (ContinueButton)
    {
        ContinueButton->OnClicked.AddUniqueDynamic(this, &UVHVObservationWidget::SubmitObservation);
    }
    PopulateObservation();
}

void UVHVObservationWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    EntranceElapsed = FMath::Min(
        EntranceElapsed + InDeltaTime,
        VHVActivityUIStyle::AnimationStandard
            + ObservationCardsSlate.Num() * VHVActivityUIStyle::AnimationStagger);
    const float Raw = FMath::Clamp(
        EntranceElapsed / VHVActivityUIStyle::AnimationStandard, 0.0f, 1.0f);
    const float Smoothed = FMath::InterpEaseOut(0.0f, 1.0f, Raw, 3.0f);
    if (ActivityContentSlate)
    {
        ActivityContentSlate->SetRenderOpacity(Smoothed);
        ActivityContentSlate->SetRenderTransform(FSlateRenderTransform(
            FVector2D(0.0f, FMath::Lerp(10.0f, 0.0f, Smoothed))));
    }

    for (int32 Index = 0; Index < ObservationCardsSlate.Num(); ++Index)
    {
        if (!ObservationCardsSlate[Index])
        {
            continue;
        }
        const float CardRaw = FMath::Clamp(
            (EntranceElapsed - Index * VHVActivityUIStyle::AnimationStagger)
                / VHVActivityUIStyle::AnimationStandard,
            0.0f, 1.0f);
        const float CardAlpha = FMath::InterpEaseOut(0.0f, 1.0f, CardRaw, 3.0f);
        ObservationCardsSlate[Index]->SetRenderOpacity(CardAlpha);
        ObservationCardsSlate[Index]->SetRenderTransform(FSlateRenderTransform(
            FVector2D(0.0f, FMath::Lerp(7.0f, 0.0f, CardAlpha))));
    }

    if (CurrentActivity.EvidenceTagging.bUseEvidenceTagging
        && (EvidenceStage == EEvidenceTaggingStage::Stage1Review
            || EvidenceStage == EEvidenceTaggingStage::Stage2Review)
        && !bReviewReady)
    {
        ReviewRevealElapsed += InDeltaTime;
        const int32 CardCount = GetCurrentEvidenceCards().Num();
        const int32 NewRevealCount = FMath::Clamp(
            FMath::FloorToInt(ReviewRevealElapsed / EvidenceRevealStagger) + 1,
            0,
            CardCount);
        if (NewRevealCount != RevealedEvidenceCardCount)
        {
            RevealedEvidenceCardCount = NewRevealCount;
            UpdateEvidenceCardStates();
        }
        if (RevealedEvidenceCardCount >= CardCount)
        {
            bReviewReady = true;
            if (BoardInstructionSlate)
            {
                BoardInstructionSlate->SetText(FText::FromString(
                    TEXT("Review complete. Press Enter or select Continue when ready.")));
            }
            if (ActionButtonTextSlate)
            {
                ActionButtonTextSlate->SetText(FText::FromString(TEXT("CONTINUE")));
            }
        }
    }
}

FReply UVHVObservationWidget::NativeOnKeyDown(
    const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (CurrentActivity.EvidenceTagging.bUseEvidenceTagging
        && (EvidenceStage == EEvidenceTaggingStage::Stage1
            || EvidenceStage == EEvidenceTaggingStage::Stage2))
    {
        if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left)
        {
            MoveEvidenceFocus(-1, 0);
            return FReply::Handled();
        }
        if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right)
        {
            MoveEvidenceFocus(1, 0);
            return FReply::Handled();
        }
        if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up)
        {
            MoveEvidenceFocus(0, -1);
            return FReply::Handled();
        }
        if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down)
        {
            MoveEvidenceFocus(0, 1);
            return FReply::Handled();
        }
    }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar
        || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        AdvanceObservation();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UVHVObservationWidget::SetObservationData(const FTextbookActivityData& InActivity)
{
    CurrentActivity = InActivity;
    bHasObservationData = true;
    bCompletionRequested = false;
    EvidenceStage = EEvidenceTaggingStage::Stage1;
    Stage1Selections.Reset();
    Stage2Selections.Reset();
    FocusedEvidenceCard = INDEX_NONE;
    ReviewRevealElapsed = 0.0f;
    RevealedEvidenceCardCount = 0;
    bReviewReady = false;
    EntranceElapsed = 0.0f;
    if (ActivityContentSlate)
    {
        ActivityContentSlate->SetRenderOpacity(0.0f);
        ActivityContentSlate->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, 10.0f)));
    }
    PopulateObservation();
}

void UVHVObservationWidget::SetOwningUIManager(UVHVUIManagerComponent* InUIManager)
{
    OwningUIManager = InUIManager;
}

void UVHVObservationWidget::SubmitObservation()
{
    if (!bCompletionRequested && OwningUIManager && OwningUIManager->SubmitObservation())
    {
        bCompletionRequested = true;
    }
}

FReply UVHVObservationWidget::HandleContinueClicked()
{
    AdvanceObservation();
    SetKeyboardFocus();
    return FReply::Handled();
}

bool UVHVObservationWidget::ActivateFocusedEvidenceCard()
{
    if (!CurrentActivity.EvidenceTagging.bUseEvidenceTagging
        || (EvidenceStage != EEvidenceTaggingStage::Stage1
            && EvidenceStage != EEvidenceTaggingStage::Stage2))
    {
        return false;
    }
    if (!GetCurrentEvidenceCards().IsValidIndex(FocusedEvidenceCard))
    {
        return false;
    }
    ToggleEvidenceCard(FocusedEvidenceCard);
    return true;
}

bool UVHVObservationWidget::AdvanceObservation()
{
    if (bCompletionRequested)
    {
        return false;
    }
    if (!CurrentActivity.EvidenceTagging.bUseEvidenceTagging)
    {
        SubmitObservation();
        return true;
    }

    if (!CanAdvanceObservation())
    {
        if (BoardInstructionSlate
            && (EvidenceStage == EEvidenceTaggingStage::Stage1
                || EvidenceStage == EEvidenceTaggingStage::Stage2))
        {
            BoardInstructionSlate->SetText(FText::FromString(TEXT("Select at least one card before continuing.")));
            BoardInstructionSlate->SetColorAndOpacity(VHVActivityUIStyle::WarningText());
        }
        return false;
    }

    if (EvidenceStage == EEvidenceTaggingStage::Stage1)
    {
        EvidenceStage = EEvidenceTaggingStage::Stage1Review;
        ReviewRevealElapsed = 0.0f;
        RevealedEvidenceCardCount = 0;
        bReviewReady = false;
        FocusedEvidenceCard = INDEX_NONE;
        RefreshEvidenceTaggingContent();
        return true;
    }
    if (EvidenceStage == EEvidenceTaggingStage::Stage1Review)
    {
        EvidenceStage = EEvidenceTaggingStage::Stage2;
        FocusedEvidenceCard = 0;
        RefreshEvidenceTaggingContent();
        return true;
    }
    if (EvidenceStage == EEvidenceTaggingStage::Stage2)
    {
        EvidenceStage = EEvidenceTaggingStage::Stage2Review;
        ReviewRevealElapsed = 0.0f;
        RevealedEvidenceCardCount = 0;
        bReviewReady = false;
        FocusedEvidenceCard = INDEX_NONE;
        RefreshEvidenceTaggingContent();
        return true;
    }
    if (EvidenceStage == EEvidenceTaggingStage::Stage2Review)
    {
        EvidenceStage = EEvidenceTaggingStage::Result;
        RefreshEvidenceTaggingContent();
        return true;
    }
    if (EvidenceStage == EEvidenceTaggingStage::Result)
    {
        SubmitObservation();
        return true;
    }
    return false;
}

void UVHVObservationWidget::PopulateObservation()
{
    if (!bHasObservationData)
    {
        return;
    }

    if (ActivityTitle)
    {
        ActivityTitle->SetText(FText::FromString(CurrentActivity.ActivityTitle));
    }
    if (NarrativeContext)
    {
        NarrativeContext->SetText(FText::FromString(CurrentActivity.NarrativeContext));
    }
    if (PromptText)
    {
        PromptText->SetText(FText::FromString(CurrentActivity.PromptText));
    }
    if (MediaContainer)
    {
        MediaContainer->ClearChildren();
    }

    RefreshSlateContent();
}

void UVHVObservationWidget::RefreshSlateContent()
{
    if (CurrentActivity.EvidenceTagging.bUseEvidenceTagging)
    {
        RefreshEvidenceTaggingContent();
    }
    else
    {
        RefreshPassiveContent();
    }
}

void UVHVObservationWidget::RefreshPassiveContent()
{
    if (!PrimaryTitleSlate || !SecondaryTextSlate || !ObservationItemsSlate)
    {
        return;
    }

    SecondaryTextSlate->ClearChildren();
    ObservationItemsSlate->ClearChildren();
    ObservationCardsSlate.Reset();
    MediaBrushes.Reset();
    LoadedMediaTextures.Reset();

    const FString ActivityTitleText = CleanText(CurrentActivity.ActivityTitle);
    const FString Prompt = CleanText(CurrentActivity.PromptText);
    const FString Narrative = CleanText(CurrentActivity.NarrativeContext);
    const bool bGenericActivityTitle = ActivityTitleText.IsEmpty()
        || ActivityTitleText.Equals(TEXT("Observation"), ESearchCase::IgnoreCase);

    FString PrimaryText;
    if (!bGenericActivityTitle)
    {
        PrimaryText = ActivityTitleText;
    }
    else if (!Prompt.IsEmpty())
    {
        PrimaryText = Prompt;
    }
    else if (!Narrative.IsEmpty())
    {
        PrimaryText = Narrative;
    }
    else
    {
        PrimaryText = ActivityTitleText;
    }
    PrimaryTitleSlate->SetText(FText::FromString(PrimaryText));

    TArray<FString> PresentedTexts;
    if (!PrimaryText.IsEmpty())
    {
        PresentedTexts.Add(PrimaryText);
    }

    TArray<FString> SecondaryTexts;
    const auto AddSecondaryIfUnique = [&PresentedTexts, &SecondaryTexts](const FString& Candidate)
    {
        const FString CleanCandidate = CleanText(Candidate);
        if (!CleanCandidate.IsEmpty() && !ContainsEquivalent(PresentedTexts, CleanCandidate))
        {
            SecondaryTexts.Add(CleanCandidate);
            PresentedTexts.Add(CleanCandidate);
        }
    };
    if (!bGenericActivityTitle)
    {
        AddSecondaryIfUnique(Prompt);
    }
    AddSecondaryIfUnique(Narrative);

    for (int32 Index = 0; Index < SecondaryTexts.Num(); ++Index)
    {
        SecondaryTextSlate->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, Index == 0 ? 0.0f : 8.0f, 0.0f, 0.0f))
            [
                SNew(STextBlock)
                .Font(VHVActivityUIStyle::RegularFont(18))
                .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                .AutoWrapText(true)
                .LineHeightPercentage(1.22f)
                .Text(FText::FromString(SecondaryTexts[Index]))
            ];
    }

    struct FObservationItem
    {
        FString Description;
        int32 BrushIndex = INDEX_NONE;
    };
    TArray<FObservationItem> Items;
    MediaBrushes.Reserve(CurrentActivity.Media.Num());
    LoadedMediaTextures.Reserve(CurrentActivity.Media.Num());

    for (const FTextbookMediaReference& Media : CurrentActivity.Media)
    {
        const FString Description = CleanText(Media.Description);
        int32 BrushIndex = INDEX_NONE;
        if ((Media.MediaType == ETextbookMediaType::Image
                || Media.MediaType == ETextbookMediaType::Diagram)
            && Media.Asset.IsValid())
        {
            TSoftObjectPtr<UTexture2D> SoftTexture(Media.Asset);
            if (UTexture2D* Texture = SoftTexture.LoadSynchronous())
            {
                LoadedMediaTextures.Add(Texture);
                FSlateBrush& Brush = MediaBrushes.AddDefaulted_GetRef();
                Brush.DrawAs = ESlateBrushDrawType::Image;
                Brush.SetResourceObject(Texture);
                Brush.SetImageSize(FVector2D(Texture->GetSizeX(), Texture->GetSizeY()));
                BrushIndex = MediaBrushes.Num() - 1;
            }
        }

        const bool bDescriptionIsNew = !Description.IsEmpty()
            && !ContainsEquivalent(PresentedTexts, Description);
        if (BrushIndex == INDEX_NONE && !bDescriptionIsNew)
        {
            continue;
        }

        FObservationItem& Item = Items.AddDefaulted_GetRef();
        Item.Description = bDescriptionIsNew ? Description : FString();
        Item.BrushIndex = BrushIndex;
        if (bDescriptionIsNew)
        {
            PresentedTexts.Add(Description);
        }
    }

    if (Items.IsEmpty())
    {
        return;
    }

    const int32 ColumnCount = Items.Num() == 4 ? 2 : FMath::Min(Items.Num(), 3);
    TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel)
        .SlotPadding(FMargin(7.0f));
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        TSharedRef<SVerticalBox> ItemContent = SNew(SVerticalBox);
        if (Items[Index].BrushIndex != INDEX_NONE)
        {
            ItemContent->AddSlot()
                .AutoHeight()
                [
                    SNew(SBox)
                    .HeightOverride(150.0f)
                    [
                        SNew(SScaleBox)
                        .Stretch(EStretch::ScaleToFit)
                        .StretchDirection(EStretchDirection::Both)
                        [
                            SNew(SImage)
                            .Image(&MediaBrushes[Items[Index].BrushIndex])
                        ]
                    ]
                ];
        }
        if (!Items[Index].Description.IsEmpty())
        {
            ItemContent->AddSlot()
                .AutoHeight()
                .Padding(FMargin(2.0f,
                    Items[Index].BrushIndex == INDEX_NONE ? 2.0f : 12.0f,
                    2.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(16))
                    .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                    .AutoWrapText(true)
                    .LineHeightPercentage(1.18f)
                    .Text(FText::FromString(Items[Index].Description))
                ];
        }

        TSharedPtr<SBox> CardBox;
        TSharedRef<SWidget> Card =
            SAssignNew(CardBox, SBox)
            .MinDesiredHeight(CardMinHeight)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                .Padding(FMargin(3.0f, 4.0f, -3.0f, -4.0f))
                [
                    SNew(SBorder)
                    .BorderImage(&CardShadowBrush)
                ]
                + SOverlay::Slot()
                [
                    SNew(SBorder)
                    .BorderImage(&CardSurfaceBrush)
                    .Padding(FMargin(18.0f, 16.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(FMargin(0.0f, 2.0f, 13.0f, 2.0f))
                        [
                            SNew(SBox)
                            .WidthOverride(3.0f)
                            [
                                SNew(SBorder)
                                .BorderImage(&CardAccentBrush)
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            ItemContent
                        ]
                    ]
                ]
            ];
        CardBox->SetRenderOpacity(0.0f);
        ObservationCardsSlate.Add(CardBox);

        Grid->AddSlot(Index % ColumnCount, Index / ColumnCount)
            [
                Card
            ];
    }

    ObservationItemsSlate->AddSlot()
        .AutoHeight()
        [
            Grid
        ];
}

void UVHVObservationWidget::RefreshEvidenceTaggingContent()
{
    if (!PrimaryTitleSlate || !SecondaryTextSlate || !ObservationItemsSlate)
    {
        return;
    }

    SecondaryTextSlate->ClearChildren();
    ObservationItemsSlate->ClearChildren();
    ObservationCardsSlate.Reset();
    EvidenceCardsSlate.Reset();
    MediaBrushes.Reset();
    LoadedMediaTextures.Reset();

    PrimaryTitleSlate->SetText(FText::FromString(CurrentActivity.ActivityTitle.ToUpper()));

    FString Prompt;
    FString Instruction;
    FString ActionText;
    switch (EvidenceStage)
    {
    case EEvidenceTaggingStage::Stage1:
        Prompt = CurrentActivity.EvidenceTagging.Stage1Prompt;
        Instruction = TEXT("W/A/S/D or arrows Navigate    E Toggle    Enter Submit");
        ActionText = TEXT("SUBMIT");
        break;
    case EEvidenceTaggingStage::Stage1Review:
        Prompt = TEXT("Review the evidence you tagged.");
        Instruction = bReviewReady
            ? TEXT("Review complete. Press Enter or select Continue when ready.")
            : TEXT("Revealing your evidence results...");
        ActionText = bReviewReady ? TEXT("CONTINUE") : TEXT("REVEALING");
        break;
    case EEvidenceTaggingStage::Stage2:
        Prompt = CurrentActivity.EvidenceTagging.Stage2Prompt;
        Instruction = TEXT("W/A/S/D or arrows Navigate    E Toggle    Enter Submit");
        ActionText = TEXT("SUBMIT");
        break;
    case EEvidenceTaggingStage::Stage2Review:
        Prompt = TEXT("Review the obstacles you tagged.");
        Instruction = bReviewReady
            ? TEXT("Review complete. Press Enter or select Continue for the takeaways.")
            : TEXT("Revealing your obstacle results...");
        ActionText = bReviewReady ? TEXT("CONTINUE") : TEXT("REVEALING");
        break;
    case EEvidenceTaggingStage::Result:
        Prompt = TEXT("What the market evidence tells us");
        Instruction = TEXT("Review the takeaways, then continue.");
        ActionText = TEXT("CONTINUE");
        break;
    }

    SecondaryTextSlate->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Font(VHVActivityUIStyle::RegularFont(21))
            .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
            .AutoWrapText(true)
            .LineHeightPercentage(1.20f)
            .Text(FText::FromString(Prompt))
        ];

    const bool bShowingReview = EvidenceStage == EEvidenceTaggingStage::Stage1Review
        || EvidenceStage == EEvidenceTaggingStage::Stage2Review;
    if (bShowingReview)
    {
        const TArray<FObservationEvidenceCard>& Cards = GetCurrentEvidenceCards();
        const TSet<int32>& Selection = GetCurrentEvidenceSelection();
        int32 CorrectCount = 0;
        int32 IncorrectCount = 0;
        int32 MissedCount = 0;
        for (int32 Index = 0; Index < Cards.Num(); ++Index)
        {
            if (Selection.Contains(Index))
            {
                Cards[Index].bCorrect ? ++CorrectCount : ++IncorrectCount;
            }
            else if (Cards[Index].bCorrect)
            {
                ++MissedCount;
            }
        }

        SecondaryTextSlate->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
            [
                SNew(STextBlock)
                .Font(VHVActivityUIStyle::MediumFont(16))
                .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                .Text(FText::FromString(FString::Printf(
                    TEXT("%d correct    %d incorrect    %d missed"),
                    CorrectCount, IncorrectCount, MissedCount)))
            ];

        const int32 TakeawayIndex = EvidenceStage == EEvidenceTaggingStage::Stage1Review ? 0 : 1;
        const TArray<FObservationTakeawayCard>& Takeaways = CurrentActivity.EvidenceTagging.TakeawayCards;
        if (Takeaways.IsValidIndex(TakeawayIndex))
        {
            const FString Takeaway = Takeaways[TakeawayIndex].Title.IsEmpty()
                ? Takeaways[TakeawayIndex].Text
                : FString::Printf(TEXT("%s — %s"),
                    *Takeaways[TakeawayIndex].Title,
                    *Takeaways[TakeawayIndex].Text);
            SecondaryTextSlate->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 7.0f, 0.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(15))
                    .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                    .AutoWrapText(true)
                    .Text(FText::FromString(Takeaway))
                ];
        }
    }

    if (BoardInstructionSlate)
    {
        BoardInstructionSlate->SetText(FText::FromString(Instruction));
        BoardInstructionSlate->SetColorAndOpacity(
            VHVActivityUIStyle::MatchingInkMuted().CopyWithNewOpacity(0.78f));
    }
    if (ActionButtonTextSlate)
    {
        ActionButtonTextSlate->SetText(FText::FromString(ActionText));
    }

    if (EvidenceStage != EEvidenceTaggingStage::Result)
    {
        RebuildEvidenceCards();
        return;
    }

    const TArray<FObservationTakeawayCard>& Takeaways = CurrentActivity.EvidenceTagging.TakeawayCards;
    TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(8.0f));
    for (int32 Index = 0; Index < Takeaways.Num(); ++Index)
    {
        TSharedPtr<SBox> CardBox;
        TSharedRef<SWidget> Card =
            SAssignNew(CardBox, SBox)
            .MinDesiredHeight(156.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot().Padding(FMargin(3.0f, 4.0f, -3.0f, -4.0f))
                [
                    SNew(SBorder).BorderImage(&CardShadowBrush)
                ]
                + SOverlay::Slot()
                [
                    SNew(SBorder)
                    .BorderImage(&CardSurfaceBrush)
                    .Padding(FMargin(20.0f, 17.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
                        [
                            SNew(STextBlock)
                            .Font(VHVActivityUIStyle::MediumFont(14))
                            .ColorAndOpacity(VHVActivityUIStyle::GoldPrimary())
                            .AutoWrapText(true)
                            .Text(FText::FromString(Takeaways[Index].Title))
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Font(VHVActivityUIStyle::RegularFont(16))
                            .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                            .AutoWrapText(true)
                            .LineHeightPercentage(1.18f)
                            .Text(FText::FromString(Takeaways[Index].Text))
                        ]
                    ]
                ]
            ];
        CardBox->SetRenderOpacity(0.0f);
        ObservationCardsSlate.Add(CardBox);
        Grid->AddSlot(Index % 3, Index / 3)[Card];
    }
    ObservationItemsSlate->AddSlot().AutoHeight()[Grid];
}

void UVHVObservationWidget::RebuildEvidenceCards()
{
    const TArray<FObservationEvidenceCard>& Cards = GetCurrentEvidenceCards();
    if (Cards.IsEmpty())
    {
        return;
    }

    EvidenceColumnCount = Cards.Num() <= 4 ? 2 : 3;
    if (EvidenceStage == EEvidenceTaggingStage::Stage1
        || EvidenceStage == EEvidenceTaggingStage::Stage2)
    {
        FocusedEvidenceCard = Cards.IsValidIndex(FocusedEvidenceCard) ? FocusedEvidenceCard : 0;
    }

    TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(8.0f));
    for (int32 Index = 0; Index < Cards.Num(); ++Index)
    {
        TSharedPtr<SVHVEvidenceTagCard> Card;
        SAssignNew(Card, SVHVEvidenceTagCard)
            .CardIndex(Index)
            .Text(FText::FromString(Cards[Index].Text))
            .Correct(Cards[Index].bCorrect)
            .OnChosen(FOnVHVEvidenceCardChosen::CreateUObject(
                this, &UVHVObservationWidget::HandleEvidenceCardChosen));
        EvidenceCardsSlate.Add(Card);
        ObservationCardsSlate.Add(Card);
        Grid->AddSlot(Index % EvidenceColumnCount, Index / EvidenceColumnCount)[Card.ToSharedRef()];
    }
    ObservationItemsSlate->AddSlot().AutoHeight()[Grid];
    UpdateEvidenceCardStates();
}

void UVHVObservationWidget::UpdateEvidenceCardStates()
{
    const TSet<int32>& Selection = GetCurrentEvidenceSelection();
    const bool bReview = EvidenceStage == EEvidenceTaggingStage::Stage1Review
        || EvidenceStage == EEvidenceTaggingStage::Stage2Review;
    for (int32 Index = 0; Index < EvidenceCardsSlate.Num(); ++Index)
    {
        if (EvidenceCardsSlate[Index])
        {
            EvidenceCardsSlate[Index]->SetPresentationState(
                !bReview && Index == FocusedEvidenceCard,
                Selection.Contains(Index),
                bReview && Index < RevealedEvidenceCardCount,
                bReview);
        }
    }
}

void UVHVObservationWidget::MoveEvidenceFocus(const int32 ColumnDelta, const int32 RowDelta)
{
    const int32 CardCount = GetCurrentEvidenceCards().Num();
    if (CardCount == 0)
    {
        return;
    }
    const int32 Current = FMath::Clamp(FocusedEvidenceCard, 0, CardCount - 1);
    const int32 Delta = ColumnDelta + RowDelta * EvidenceColumnCount;
    FocusedEvidenceCard = FMath::Clamp(Current + Delta, 0, CardCount - 1);
    UpdateEvidenceCardStates();
}

void UVHVObservationWidget::ToggleEvidenceCard(const int32 CardIndex)
{
    if (!GetCurrentEvidenceCards().IsValidIndex(CardIndex))
    {
        return;
    }
    TSet<int32>& Selection = GetCurrentEvidenceSelection();
    if (Selection.Contains(CardIndex)) Selection.Remove(CardIndex);
    else Selection.Add(CardIndex);

    if (BoardInstructionSlate)
    {
        BoardInstructionSlate->SetText(FText::FromString(
            TEXT("W/A/S/D or arrows Navigate    E Toggle    Enter Submit")));
        BoardInstructionSlate->SetColorAndOpacity(
            VHVActivityUIStyle::MatchingInkMuted().CopyWithNewOpacity(0.78f));
    }
    UpdateEvidenceCardStates();
}

void UVHVObservationWidget::HandleEvidenceCardChosen(const int32 CardIndex)
{
    FocusedEvidenceCard = CardIndex;
    ToggleEvidenceCard(CardIndex);
    SetKeyboardFocus();
}

bool UVHVObservationWidget::CanAdvanceObservation() const
{
    if (!CurrentActivity.EvidenceTagging.bUseEvidenceTagging)
    {
        return !bCompletionRequested;
    }
    switch (EvidenceStage)
    {
    case EEvidenceTaggingStage::Stage1:
        return !Stage1Selections.IsEmpty();
    case EEvidenceTaggingStage::Stage2:
        return !Stage2Selections.IsEmpty();
    case EEvidenceTaggingStage::Stage1Review:
    case EEvidenceTaggingStage::Stage2Review:
        return bReviewReady;
    case EEvidenceTaggingStage::Result:
        return !bCompletionRequested;
    default:
        return false;
    }
}

const TArray<FObservationEvidenceCard>& UVHVObservationWidget::GetCurrentEvidenceCards() const
{
    if (EvidenceStage == EEvidenceTaggingStage::Stage1
        || EvidenceStage == EEvidenceTaggingStage::Stage1Review)
    {
        return CurrentActivity.EvidenceTagging.EvidenceCards;
    }
    return CurrentActivity.EvidenceTagging.ObstacleCards;
}

const TSet<int32>& UVHVObservationWidget::GetCurrentEvidenceSelection() const
{
    if (EvidenceStage == EEvidenceTaggingStage::Stage1
        || EvidenceStage == EEvidenceTaggingStage::Stage1Review)
    {
        return Stage1Selections;
    }
    return Stage2Selections;
}

TSet<int32>& UVHVObservationWidget::GetCurrentEvidenceSelection()
{
    if (EvidenceStage == EEvidenceTaggingStage::Stage1
        || EvidenceStage == EEvidenceTaggingStage::Stage1Review)
    {
        return Stage1Selections;
    }
    return Stage2Selections;
}
