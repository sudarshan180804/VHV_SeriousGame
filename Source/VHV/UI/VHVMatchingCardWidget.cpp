#include "UI/VHVMatchingCardWidget.h"

#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UI/VHVMatchingWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

UVHVMatchingCardWidget::UVHVMatchingCardWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
}

TSharedRef<SWidget> UVHVMatchingCardWidget::RebuildWidget()
{
    CardBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::MatchingPaperCard(), VHVActivityUIStyle::MatchingCardRadius,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);
    ShadowBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::MatchingShadow(), VHVActivityUIStyle::MatchingCardRadius + 2.0f);
    ConnectorBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::MatchingConnectorFill(), VHVActivityUIStyle::MatchingConnectorDiameter * 0.5f,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);
    ConnectorCoreBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::MatchingPaperBorder(), 3.0f);
    MediaSurfaceBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(224, 216, 197, 150), 10.0f,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);
    MediaBrush.DrawAs = ESlateBrushDrawType::Image;
    MediaBrush.SetResourceObject(MediaTexture);

    const float ConnectorHalf = VHVActivityUIStyle::MatchingConnectorDiameter * 0.5f;
    const FMargin BodyInset = bIsLeftColumn
        ? FMargin(0.0f, 0.0f, ConnectorHalf, 0.0f)
        : FMargin(ConnectorHalf, 0.0f, 0.0f, 0.0f);
    const FMargin ContentPadding = bIsLeftColumn
        ? FMargin(VHVActivityUIStyle::MatchingCardPadding, 14.0f,
            VHVActivityUIStyle::MatchingCardPadding + ConnectorHalf, 14.0f)
        : FMargin(VHVActivityUIStyle::MatchingCardPadding + ConnectorHalf, 14.0f,
            VHVActivityUIStyle::MatchingCardPadding, 14.0f);

    TSharedRef<SWidget> CardContent =
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(0.0f, 0.0f, 14.0f, 0.0f))
        [
            SAssignNew(MediaAreaSlate, SBox)
            .WidthOverride(VHVActivityUIStyle::MatchingMediaWidth)
            .HeightOverride(VHVActivityUIStyle::MatchingMediaHeight)
            .Visibility(MediaTexture ? EVisibility::Visible : EVisibility::Collapsed)
            [
                SNew(SBorder)
                .BorderImage(&MediaSurfaceBrush)
                .Padding(2.0f)
                [
                    SAssignNew(MediaImageSlate, SImage)
                    .Image(&MediaBrush)
                ]
            ]
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .VAlign(VAlign_Center)
            [
                SAssignNew(CardTextSlate, STextBlock)
                .Font(VHVActivityUIStyle::MediumFont(VHVActivityUIStyle::MatchingCardFontSize))
                .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                .AutoWrapText(true)
                .LineHeightPercentage(1.05f)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
            [
                SAssignNew(DescriptionTextSlate, STextBlock)
                .Font(VHVActivityUIStyle::RegularFont(14))
                .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                .AutoWrapText(true)
                .Visibility(Description.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible)
            ]
        ];

    TSharedRef<SWidget> Result =
        SNew(SBox)
        .HeightOverride(VHVActivityUIStyle::MatchingCardHeight)
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            .Padding(BodyInset + FMargin(2.0f, 4.0f, -2.0f, -5.0f))
            [
                SNew(SBorder)
                .BorderImage(&ShadowBrush)
            ]
            + SOverlay::Slot()
            .Padding(BodyInset)
            [
                SAssignNew(CardBorderSlate, SBorder)
                .BorderImage(&CardBrush)
                .Padding(ContentPadding)
                [
                    CardContent
                ]
            ]
            + SOverlay::Slot()
            .HAlign(bIsLeftColumn ? HAlign_Right : HAlign_Left)
            .VAlign(VAlign_Center)
            [
                SNew(SBox)
                .WidthOverride(VHVActivityUIStyle::MatchingConnectorDiameter)
                .HeightOverride(VHVActivityUIStyle::MatchingConnectorDiameter)
                [
                    SAssignNew(ConnectorSlate, SBorder)
                    .BorderImage(&ConnectorBrush)
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SBox)
                        .WidthOverride(6.0f)
                        .HeightOverride(6.0f)
                        [
                            SAssignNew(ConnectorCoreSlate, SBorder)
                            .BorderImage(&ConnectorCoreBrush)
                        ]
                    ]
                ]
            ]
        ];

    if (CardTextSlate)
    {
        CardTextSlate->SetText(FText::FromString(BaseDisplayText));
    }
    if (DescriptionTextSlate)
    {
        DescriptionTextSlate->SetText(Description);
    }
    RefreshVisualState();
    return Result;
}

void UVHVMatchingCardWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    CardBorderSlate.Reset();
    ConnectorSlate.Reset();
    ConnectorCoreSlate.Reset();
    MediaAreaSlate.Reset();
    MediaImageSlate.Reset();
    CardTextSlate.Reset();
    DescriptionTextSlate.Reset();
}

void UVHVMatchingCardWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (CardText)
    {
        CardText->SetAutoWrapText(true);
    }
}

void UVHVMatchingCardWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    EntranceElapsed += InDeltaTime;
    const float RawEntrance = FMath::Clamp(
        (EntranceElapsed - EntranceDelay) / VHVActivityUIStyle::AnimationStandard, 0.0f, 1.0f);
    const float EntranceAlpha = 1.0f - FMath::Pow(1.0f - RawEntrance, 3.0f);
    SetRenderOpacity(EntranceAlpha);
    const float TargetLift = bLastDragging ? 5.0f : (bLastFocused ? 2.0f : 0.0f);
    CurrentLift = FMath::FInterpTo(
        CurrentLift, TargetLift, InDeltaTime, 1.0f / VHVActivityUIStyle::AnimationFast);
    SetRenderTranslation(FVector2D(0.0f, 7.0f * (1.0f - EntranceAlpha) - CurrentLift));
}

void UVHVMatchingCardWidget::SetOwningMatchingWidget(UVHVMatchingWidget* InMatchingWidget)
{
    OwningMatchingWidget = InMatchingWidget;
}

void UVHVMatchingCardWidget::SetCardData(
    const bool bInLeftColumn, const int32 InItemIndex, const FString& InDisplayText)
{
    bIsLeftColumn = bInLeftColumn;
    ItemIndex = InItemIndex;
    BaseDisplayText = InDisplayText;
    if (CardText)
    {
        CardText->SetText(FText::FromString(BaseDisplayText));
        CardText->SetAutoWrapText(true);
    }
    if (CardTextSlate)
    {
        CardTextSlate->SetText(FText::FromString(BaseDisplayText));
    }
}

void UVHVMatchingCardWidget::SetEntranceDelay(const float InDelay)
{
    EntranceDelay = FMath::Max(0.0f, InDelay);
    EntranceElapsed = 0.0f;
    SetRenderOpacity(0.0f);
}

void UVHVMatchingCardWidget::SetMediaTexture(UTexture2D* InTexture)
{
    MediaTexture = InTexture;
    MediaBrush.SetResourceObject(MediaTexture);
    if (MediaTexture)
    {
        MediaBrush.SetImageSize(FVector2D(MediaTexture->GetSizeX(), MediaTexture->GetSizeY()));
    }
    if (MediaImageSlate)
    {
        MediaImageSlate->SetImage(&MediaBrush);
    }
    if (MediaAreaSlate)
    {
        MediaAreaSlate->SetVisibility(MediaTexture ? EVisibility::Visible : EVisibility::Collapsed);
    }
}

void UVHVMatchingCardWidget::SetDescription(const FText& InDescription)
{
    Description = InDescription;
    if (DescriptionTextSlate)
    {
        DescriptionTextSlate->SetText(Description);
        DescriptionTextSlate->SetVisibility(Description.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
    }
}

FGeometry UVHVMatchingCardWidget::GetConnectorGeometry() const
{
    return ConnectorSlate ? ConnectorSlate->GetCachedGeometry() : GetCachedGeometry();
}

void UVHVMatchingCardWidget::SetVisualState(
    const bool bIsFocused, const bool bIsSelected, const bool bIsMapped,
    const bool bIsHovered, const bool bIsDragging)
{
    bLastFocused = bIsFocused;
    bLastSelected = bIsSelected;
    bLastMapped = bIsMapped;
    bLastDragTarget = bIsHovered;
    bLastDragging = bIsDragging;
    RefreshVisualState();
}

void UVHVMatchingCardWidget::RefreshVisualState()
{
    const bool bEffectiveHover = bLastDragTarget || bPointerHovered;
    FLinearColor Fill = VHVActivityUIStyle::MatchingPaperCard();
    FLinearColor Outline = VHVActivityUIStyle::MatchingPaperBorder();
    FLinearColor ConnectorFill = VHVActivityUIStyle::MatchingConnectorFill();
    FLinearColor ConnectorOutline = VHVActivityUIStyle::MatchingPaperBorder();
    FLinearColor CoreFill = VHVActivityUIStyle::MatchingPaperBorder();
    float OutlineWidth = VHVActivityUIStyle::BorderNormalWidth;
    if (bEffectiveHover)
    {
        Fill = VHVActivityUIStyle::MatchingPaperHover();
        Outline = VHVActivityUIStyle::BorderFocused();
    }
    if (bLastMapped)
    {
        Outline = VHVActivityUIStyle::PositiveMuted().CopyWithNewOpacity(0.62f);
        ConnectorOutline = VHVActivityUIStyle::PositiveMuted().CopyWithNewOpacity(0.88f);
        CoreFill = VHVActivityUIStyle::PositiveMuted();
    }
    if (bLastFocused)
    {
        Fill = VHVActivityUIStyle::MatchingPaperFocused();
        Outline = VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.68f);
        ConnectorOutline = VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.78f);
        OutlineWidth = VHVActivityUIStyle::BorderSelectedWidth;
    }
    if (bLastSelected || bLastDragging)
    {
        Fill = VHVActivityUIStyle::MatchingPaperSelected();
        Outline = VHVActivityUIStyle::GoldSelected().CopyWithNewOpacity(0.88f);
        ConnectorFill = VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.23f);
        ConnectorOutline = VHVActivityUIStyle::GoldSelected();
        CoreFill = VHVActivityUIStyle::GoldSelected();
        OutlineWidth = VHVActivityUIStyle::BorderSelectedWidth;
    }
    CardBrush = VHVActivityUIStyle::RoundedBrush(
        Fill, VHVActivityUIStyle::MatchingCardRadius, Outline, OutlineWidth);
    ConnectorBrush = VHVActivityUIStyle::RoundedBrush(
        ConnectorFill, VHVActivityUIStyle::MatchingConnectorDiameter * 0.5f,
        ConnectorOutline, OutlineWidth);
    ConnectorCoreBrush = VHVActivityUIStyle::RoundedBrush(CoreFill, 3.0f);
    if (CardBorderSlate)
    {
        CardBorderSlate->SetBorderImage(&CardBrush);
        CardBorderSlate->Invalidate(EInvalidateWidgetReason::Paint);
    }
    if (ConnectorSlate)
    {
        ConnectorSlate->SetBorderImage(&ConnectorBrush);
        ConnectorSlate->Invalidate(EInvalidateWidgetReason::Paint);
    }
    if (ConnectorCoreSlate)
    {
        ConnectorCoreSlate->SetBorderImage(&ConnectorCoreBrush);
        ConnectorCoreSlate->SetVisibility((bLastMapped || bLastSelected || bLastDragging) ? EVisibility::Visible : EVisibility::Collapsed);
        ConnectorCoreSlate->Invalidate(EInvalidateWidgetReason::Paint);
    }
    if (CardTextSlate)
    {
        CardTextSlate->SetColorAndOpacity(VHVActivityUIStyle::MatchingInk());
    }
    if (CardText)
    {
        CardText->SetText(FText::FromString(BaseDisplayText));
        CardText->SetColorAndOpacity(VHVActivityUIStyle::MatchingInk());
    }
}

FReply UVHVMatchingCardWidget::NativeOnMouseButtonDown(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && OwningMatchingWidget)
    {
        return bIsLeftColumn
            ? FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton)
            : FReply::Handled();
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UVHVMatchingCardWidget::NativeOnMouseButtonUp(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && OwningMatchingWidget)
    {
        OwningMatchingWidget->HandleCardSelected(bIsLeftColumn, ItemIndex);
        OwningMatchingWidget->SetKeyboardFocus();
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UVHVMatchingCardWidget::NativeOnDragDetected(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    if (!bIsLeftColumn || !OwningMatchingWidget)
    {
        return;
    }
    OutOperation = NewObject<UDragDropOperation>();
    if (OutOperation)
    {
        OutOperation->Payload = this;
        OutOperation->Pivot = EDragPivot::MouseDown;
        OwningMatchingWidget->BeginConnectionDrag(ItemIndex, InMouseEvent.GetScreenSpacePosition());
    }
}

bool UVHVMatchingCardWidget::NativeOnDragOver(
    const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (!bIsLeftColumn && OwningMatchingWidget
        && Cast<UVHVMatchingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        OwningMatchingWidget->UpdateConnectionDrag(InDragDropEvent.GetScreenSpacePosition(), ItemIndex);
        return true;
    }
    return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

bool UVHVMatchingCardWidget::NativeOnDrop(
    const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (!bIsLeftColumn && OwningMatchingWidget
        && Cast<UVHVMatchingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        OwningMatchingWidget->CommitConnectionDrag(ItemIndex);
        return true;
    }
    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UVHVMatchingCardWidget::NativeOnDragCancelled(
    const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (OwningMatchingWidget
        && Cast<UVHVMatchingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        OwningMatchingWidget->EndConnectionDrag();
    }
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

void UVHVMatchingCardWidget::NativeOnMouseEnter(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    bPointerHovered = true;
    RefreshVisualState();
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
}

void UVHVMatchingCardWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    bPointerHovered = false;
    RefreshVisualState();
    Super::NativeOnMouseLeave(InMouseEvent);
}
