#include "UI/VHVOrderingCardWidget.h"

#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UI/VHVOrderingWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

UVHVOrderingCardWidget::UVHVOrderingCardWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
}

TSharedRef<SWidget> UVHVOrderingCardWidget::RebuildWidget()
{
    CardBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassCard(), VHVActivityUIStyle::OrderingCardRadius,
        VHVActivityUIStyle::BorderNeutral(), VHVActivityUIStyle::BorderNormalWidth);
    ShadowBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::ShadowCard(), VHVActivityUIStyle::OrderingCardRadius + 2.0f);
    BadgeBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::IndicatorNormal(), VHVActivityUIStyle::OrderingBadgeDiameter * 0.5f,
        VHVActivityUIStyle::BorderNeutral(), VHVActivityUIStyle::BorderNormalWidth);
    MediaSurfaceBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassMain(), 18.0f,
        VHVActivityUIStyle::PanelBorder(), VHVActivityUIStyle::BorderNormalWidth);
    MediaBrush.DrawAs = ESlateBrushDrawType::Image;
    MediaBrush.SetResourceObject(MediaTexture);

    TSharedRef<SWidget> Result =
        SNew(SBox)
        .WidthOverride(VHVActivityUIStyle::OrderingCardWidth)
        .HeightOverride(VHVActivityUIStyle::OrderingCardHeight)
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            .Padding(FMargin(4.0f, 6.0f, -4.0f, -7.0f))
            [
                SNew(SBorder)
                .BorderImage(&ShadowBrush)
            ]
            + SOverlay::Slot()
            [
                SAssignNew(CardBorderSlate, SBorder)
                .BorderImage(&CardBrush)
                .Padding(VHVActivityUIStyle::OrderingCardPadding)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox)
                            .WidthOverride(VHVActivityUIStyle::OrderingBadgeDiameter)
                            .HeightOverride(VHVActivityUIStyle::OrderingBadgeDiameter)
                            [
                                SNew(SBorder)
                                .BorderImage(&BadgeBrush)
                                .HAlign(HAlign_Center)
                                .VAlign(VAlign_Center)
                                [
                                    SAssignNew(PositionTextSlate, STextBlock)
                                    .Font(VHVActivityUIStyle::MediumFont(20))
                                    .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                                ]
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            SNew(SSpacer)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Font(VHVActivityUIStyle::MediumFont(16))
                            .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                            .Text(FText::FromString(TEXT("|||")))
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 18.0f, 0.0f, 20.0f))
                    [
                        SAssignNew(MediaAreaSlate, SBox)
                        .HeightOverride(VHVActivityUIStyle::OrderingMediaHeight)
                        .Visibility(MediaTexture ? EVisibility::Visible : EVisibility::Collapsed)
                        [
                            SNew(SBorder)
                            .BorderImage(&MediaSurfaceBrush)
                            .Padding(3.0f)
                            [
                                SNew(SScaleBox)
                                .Stretch(EStretch::ScaleToFill)
                                [
                                    SAssignNew(MediaImageSlate, SImage)
                                    .Image(&MediaBrush)
                                ]
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    [
                        SAssignNew(CardTextSlate, STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(27))
                        .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                        .Justification(ETextJustify::Center)
                        .AutoWrapText(true)
                        .LineHeightPercentage(1.08f)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))
                    [
                        SAssignNew(DescriptionTextSlate, STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(16))
                        .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                        .Justification(ETextJustify::Center)
                        .AutoWrapText(true)
                        .Visibility(EVisibility::Collapsed)
                    ]
                ]
            ]
        ];

    if (CardTextSlate)
    {
        CardTextSlate->SetText(FText::FromString(BaseDisplayText));
    }
    UpdatePositionText();
    RefreshVisualState();
    return Result;
}

void UVHVOrderingCardWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    CardBorderSlate.Reset();
    MediaAreaSlate.Reset();
    MediaImageSlate.Reset();
    CardTextSlate.Reset();
    DescriptionTextSlate.Reset();
    PositionTextSlate.Reset();
}

void UVHVOrderingCardWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    const float TargetLift = bGrabbed ? VHVActivityUIStyle::OrderingGrabLift : 0.0f;
    const float TargetScale = bGrabbed ? 1.025f : 1.0f;
    const float InterpSpeed = 1.0f / VHVActivityUIStyle::AnimationFast;
    CurrentLift = FMath::FInterpTo(CurrentLift, TargetLift, InDeltaTime, InterpSpeed);
    CurrentScale = FMath::FInterpTo(CurrentScale, TargetScale, InDeltaTime, InterpSpeed);
    SetRenderTranslation(FVector2D(0.0f, -CurrentLift));
    SetRenderScale(FVector2D(CurrentScale));
}

void UVHVOrderingCardWidget::SetOrderingItem(const FOrderingItem& Item)
{
    CurrentItem = Item;
    BaseDisplayText = Item.ItemText;
    if (CardText)
    {
        CardText->SetText(FText::FromString(BaseDisplayText));
    }
    if (CardTextSlate)
    {
        CardTextSlate->SetText(FText::FromString(BaseDisplayText));
    }
}

FString UVHVOrderingCardWidget::GetItemID() const
{
    return CurrentItem.ItemID;
}

FOrderingItem UVHVOrderingCardWidget::GetOrderingItem() const
{
    return CurrentItem;
}

void UVHVOrderingCardWidget::SetOwningOrderingWidget(UVHVOrderingWidget* InOrderingWidget)
{
    OwningOrderingWidget = InOrderingWidget;
}

void UVHVOrderingCardWidget::SetSelected(const bool bSelected)
{
    bFocused = bSelected;
    RefreshVisualState();
}

void UVHVOrderingCardWidget::SetGrabbed(const bool bInGrabbed)
{
    bGrabbed = bInGrabbed;
    RefreshVisualState();
}

void UVHVOrderingCardWidget::SetVisualState(const bool bIsFocused, const bool bIsGrabbed)
{
    bFocused = bIsFocused;
    bGrabbed = bIsGrabbed;
    RefreshVisualState();
}

void UVHVOrderingCardWidget::SetDropHighlight(const bool bActive)
{
    SetMouseDropTarget(bActive);
}

void UVHVOrderingCardWidget::SetMouseDropTarget(const bool bActive)
{
    bMouseDropTarget = bActive;
    RefreshVisualState();
}

void UVHVOrderingCardWidget::SetMediaTexture(UTexture2D* Texture)
{
    MediaTexture = Texture;
    MediaBrush.SetResourceObject(MediaTexture);
    if (MediaTexture)
    {
        MediaBrush.SetImageSize(FVector2D(MediaTexture->GetSizeX(), MediaTexture->GetSizeY()));
    }
    if (MediaAreaSlate)
    {
        MediaAreaSlate->SetVisibility(MediaTexture ? EVisibility::Visible : EVisibility::Collapsed);
    }
    if (MediaImageSlate)
    {
        MediaImageSlate->Invalidate(EInvalidateWidgetReason::Paint);
    }
}

void UVHVOrderingCardWidget::SetDescription(const FText& Description)
{
    if (!DescriptionTextSlate)
    {
        return;
    }
    DescriptionTextSlate->SetText(Description);
    DescriptionTextSlate->SetVisibility(Description.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
}

void UVHVOrderingCardWidget::RefreshVisualState()
{
    const bool bEmphasized = bGrabbed || bMouseDropTarget;
    const FLinearColor Fill = bGrabbed
        ? VHVActivityUIStyle::GlassSelected()
        : (bMouseDropTarget || bFocused ? VHVActivityUIStyle::GlassFocused() : VHVActivityUIStyle::GlassCard());
    const FLinearColor Outline = bEmphasized
        ? VHVActivityUIStyle::BorderGold()
        : (bFocused ? VHVActivityUIStyle::BorderFocused() : VHVActivityUIStyle::BorderNeutral());
    const float OutlineWidth = bEmphasized
        ? VHVActivityUIStyle::BorderSelectedWidth
        : (bFocused ? 1.25f : VHVActivityUIStyle::BorderNormalWidth);

    CardBrush = VHVActivityUIStyle::RoundedBrush(
        Fill, VHVActivityUIStyle::OrderingCardRadius, Outline, OutlineWidth);
    ShadowBrush = VHVActivityUIStyle::RoundedBrush(
        bGrabbed ? VHVActivityUIStyle::SelectedGlow() : VHVActivityUIStyle::ShadowCard(),
        VHVActivityUIStyle::OrderingCardRadius + 2.0f);
    BadgeBrush = VHVActivityUIStyle::RoundedBrush(
        bGrabbed ? VHVActivityUIStyle::IndicatorSelected() : VHVActivityUIStyle::IndicatorNormal(),
        VHVActivityUIStyle::OrderingBadgeDiameter * 0.5f,
        Outline,
        OutlineWidth);

    const FLinearColor PrimaryColor = bFocused || bEmphasized
        ? VHVActivityUIStyle::TextPrimary()
        : VHVActivityUIStyle::TextSecondary();
    const FLinearColor BadgeTextColor = bGrabbed
        ? VHVActivityUIStyle::GoldSelected()
        : VHVActivityUIStyle::TextPrimary();

    if (CardTextSlate)
    {
        CardTextSlate->SetColorAndOpacity(PrimaryColor);
    }
    if (PositionTextSlate)
    {
        PositionTextSlate->SetColorAndOpacity(BadgeTextColor);
    }
    if (CardText)
    {
        CardText->SetColorAndOpacity(PrimaryColor);
    }
    if (PositionText)
    {
        PositionText->SetColorAndOpacity(BadgeTextColor);
    }
    if (CardBorderSlate)
    {
        CardBorderSlate->Invalidate(EInvalidateWidgetReason::Paint);
    }
    InvalidateLayoutAndVolatility();
}

FReply UVHVOrderingCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UVHVOrderingCardWidget::NativeOnDragDetected(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent,
    UDragDropOperation*& OutOperation)
{
    OutOperation = NewObject<UDragDropOperation>();
    if (!OutOperation)
    {
        return;
    }

    OutOperation->Payload = this;
    UVHVOrderingCardWidget* DragVisual = CreateWidget<UVHVOrderingCardWidget>(GetOwningPlayer(), GetClass());
    if (DragVisual)
    {
        DragVisual->SetOrderingItem(CurrentItem);
        DragVisual->SetCardIndex(CardIndex);
        DragVisual->SetMediaTexture(MediaTexture);
        DragVisual->SetVisualState(true, true);
        OutOperation->DefaultDragVisual = DragVisual;
    }
    OutOperation->Pivot = EDragPivot::MouseDown;

    if (OwningOrderingWidget)
    {
        OwningOrderingWidget->BeginMouseDrag(CurrentItem.ItemID);
    }
}

bool UVHVOrderingCardWidget::NativeOnDrop(
    const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    if (!OwningOrderingWidget || !Cast<UVHVOrderingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        return false;
    }

    OwningOrderingWidget->UpdateMouseDrag(InDragDropEvent);
    OwningOrderingWidget->CommitMouseReorder();
    return true;
}

bool UVHVOrderingCardWidget::NativeOnDragOver(
    const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    if (OwningOrderingWidget && Cast<UVHVOrderingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        OwningOrderingWidget->UpdateMouseDrag(InDragDropEvent);
        return true;
    }
    return false;
}

void UVHVOrderingCardWidget::NativeOnDragCancelled(
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    if (OwningOrderingWidget && Cast<UVHVOrderingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        OwningOrderingWidget->EndMouseDrag();
    }
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

void UVHVOrderingCardWidget::SetCardIndex(const int32 InCardIndex)
{
    CardIndex = InCardIndex;
    UpdatePositionText();
}

void UVHVOrderingCardWidget::UpdatePositionText()
{
    const FText Position = FText::AsNumber(CardIndex + 1);
    if (PositionText)
    {
        PositionText->SetText(Position);
    }
    if (PositionTextSlate)
    {
        PositionTextSlate->SetText(Position);
    }
}
