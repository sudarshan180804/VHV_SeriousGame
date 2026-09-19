#include "UI/VHVMatchingCardWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetTree.h"
#include "InputCoreTypes.h"
#include "UI/VHVMatchingWidget.h"

void UVHVMatchingCardWidget::SetOwningMatchingWidget(UVHVMatchingWidget* InMatchingWidget)
{
    OwningMatchingWidget = InMatchingWidget;
}

void UVHVMatchingCardWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // The existing card Blueprint may contain only CardText as its root. Wrap
    // that root in a Border so the visible presentation fills this generated
    // widget's column slot, which is also the geometry used by connection lines.
    if (!CardBorder && CardText && WidgetTree && WidgetTree->RootWidget == CardText)
    {
        CardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RuntimeCardBorder"));
        if (CardBorder)
        {
            CardBorder->SetPadding(FMargin(16.0f, 12.0f));
            CardBorder->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.12f, 1.0f));
            CardBorder->SetContent(CardText);
            WidgetTree->RootWidget = CardBorder;
        }
    }

    if (CardText)
    {
        CardText->SetAutoWrapText(true);
    }
}

void UVHVMatchingCardWidget::SetCardData(const bool bInLeftColumn, const int32 InItemIndex, const FString& InDisplayText)
{
    bIsLeftColumn = bInLeftColumn;
    ItemIndex = InItemIndex;
    BaseDisplayText = InDisplayText;
    if (CardText)
    {
        CardText->SetText(FText::FromString(BaseDisplayText));
        CardText->SetAutoWrapText(true);
    }
}

FGeometry UVHVMatchingCardWidget::GetVisibleContentGeometry() const
{
    return CardText ? CardText->GetCachedGeometry() : GetCachedGeometry();
}

void UVHVMatchingCardWidget::SetVisualState(const bool bIsFocused, const bool bIsSelected, const bool bIsMapped, const bool bIsHovered, const bool bIsDragging)
{
    bLastFocused = bIsFocused;
    bLastSelected = bIsSelected;
    bLastMapped = bIsMapped;
    bLastDragTarget = bIsHovered;
    bLastDragging = bIsDragging;
    const bool bEffectiveHover = bIsHovered || bPointerHovered;
    if (CardText)
    {
        FString DisplayText = BaseDisplayText;
        if (bIsFocused)
        {
            DisplayText = FString::Printf(TEXT("< %s >"), *DisplayText);
        }
        CardText->SetText(FText::FromString(DisplayText));
        CardText->SetColorAndOpacity(bIsFocused || bIsSelected || bEffectiveHover || bIsDragging ? FLinearColor::White : FLinearColor(0.82f, 0.82f, 0.82f, 1.0f));
    }

    if (CardBorder)
    {
        CardBorder->SetPadding(FMargin(12.0f, 8.0f));
        FLinearColor BorderColor = FLinearColor(0.12f, 0.12f, 0.12f, 1.0f);
        if (bIsMapped)
        {
            BorderColor = FLinearColor(0.32f, 0.42f, 0.55f, 1.0f);
        }
        if (bIsSelected)
        {
            BorderColor = FLinearColor(0.20f, 0.58f, 0.42f, 1.0f);
        }
        if (bIsFocused)
        {
            BorderColor = FLinearColor(0.18f, 0.52f, 0.72f, 1.0f);
        }
        if (bEffectiveHover)
        {
            BorderColor = FLinearColor(0.32f, 0.65f, 0.88f, 1.0f);
        }
        if (bIsDragging)
        {
            BorderColor = FLinearColor(0.95f, 0.58f, 0.12f, 1.0f);
        }
        CardBorder->SetBrushColor(BorderColor);
    }
}

FReply UVHVMatchingCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && OwningMatchingWidget)
    {
        return bIsLeftColumn
            ? FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton)
            : FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UVHVMatchingCardWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && OwningMatchingWidget)
    {
        OwningMatchingWidget->HandleCardSelected(bIsLeftColumn, ItemIndex);
        OwningMatchingWidget->SetKeyboardFocus();
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UVHVMatchingCardWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
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

bool UVHVMatchingCardWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (!bIsLeftColumn && OwningMatchingWidget && Cast<UVHVMatchingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        OwningMatchingWidget->UpdateConnectionDrag(InDragDropEvent.GetScreenSpacePosition(), ItemIndex);
        return true;
    }
    return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

bool UVHVMatchingCardWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (!bIsLeftColumn && OwningMatchingWidget && Cast<UVHVMatchingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        OwningMatchingWidget->CommitConnectionDrag(ItemIndex);
        return true;
    }
    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UVHVMatchingCardWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (OwningMatchingWidget && Cast<UVHVMatchingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        OwningMatchingWidget->EndConnectionDrag();
    }
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

void UVHVMatchingCardWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    bPointerHovered = true;
    SetVisualState(bLastFocused, bLastSelected, bLastMapped, bLastDragTarget, bLastDragging);
    Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
}

void UVHVMatchingCardWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
    bPointerHovered = false;
    SetVisualState(bLastFocused, bLastSelected, bLastMapped, bLastDragTarget, bLastDragging);
    Super::NativeOnMouseLeave(InMouseEvent);
}
