#include "UI/VHVOrderingCardWidget.h"

#include "UI/VHVOrderingWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"

void UVHVOrderingCardWidget::SetOrderingItem(const FOrderingItem& Item)
{
    CurrentItem = Item;
    BaseDisplayText = Item.ItemText;
    if (CardText)
    {
        CardText->SetText(FText::FromString(BaseDisplayText));
    }
    UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Created item ItemID=%s"), *Item.ItemID);
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

void UVHVOrderingCardWidget::SetSelected(bool bSelected)
{
    if (!CardText)
    {
        return;
    }

    const FLinearColor TextColor = bSelected ? FLinearColor(0.95f, 1.0f, 0.98f, 1.0f) : FLinearColor::White;
    CardText->SetColorAndOpacity(TextColor);
    if (PositionText)
    {
        PositionText->SetColorAndOpacity(TextColor);
    }
}

void UVHVOrderingCardWidget::SetGrabbed(bool bGrabbed)
{
    if (!CardBorder)
    {
        return;
    }

    if (bGrabbed)
    {
        CardBorder->SetPadding(FMargin(8.0f));
        CardBorder->SetRenderScale(FVector2D(1.03f, 1.03f));
        CardBorder->SetBrushColor(FLinearColor(0.18f, 0.52f, 0.72f, 1.0f));
    }
    else if (CardText)
    {
        CardBorder->SetPadding(FMargin(4.0f));
        CardBorder->SetRenderScale(FVector2D(1.0f, 1.0f));
        ApplyCardColor();
    }
}

void UVHVOrderingCardWidget::SetVisualState(bool bIsFocused, bool bIsGrabbed)
{
    FString DisplayText = BaseDisplayText;
    if (bIsGrabbed)
    {
        DisplayText = FString::Printf(TEXT("< [G] %s >"), *BaseDisplayText);
        SetGrabbed(true);
        SetSelected(true);
    }
    else if (bIsFocused)
    {
        DisplayText = FString::Printf(TEXT("< %s >"), *BaseDisplayText);
        SetGrabbed(false);
        SetSelected(true);
    }
    else
    {
        SetGrabbed(false);
        SetSelected(false);
        if (CardBorder)
        {
            CardBorder->SetRenderScale(FVector2D(1.0f, 1.0f));
        }
    }

    if (CardText)
    {
        CardText->SetText(FText::FromString(DisplayText));
    }
}

void UVHVOrderingCardWidget::SetDropHighlight(bool bActive)
{
    if (!CardText)
    {
        return;
    }

    const FLinearColor TextColor = bActive ? FLinearColor(0.96f, 1.0f, 0.98f, 1.0f) : FLinearColor::White;
    CardText->SetColorAndOpacity(TextColor);
    if (PositionText)
    {
        PositionText->SetColorAndOpacity(TextColor);
    }
}

void UVHVOrderingCardWidget::SetMouseDropTarget(bool bActive)
{
    bMouseDropTarget = bActive;
    if (!CardBorder)
    {
        return;
    }

    if (bActive)
    {
        CardBorder->SetBrushColor(FLinearColor(0.75f, 0.05f, 0.05f, 1.0f));
        return;
    }

    ApplyCardColor();
}

FReply UVHVOrderingCardWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
    {
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UVHVOrderingCardWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    OutOperation = NewObject<UDragDropOperation>();
    if (!OutOperation)
    {
        return;
    }

    OutOperation->Payload = this;
    // The drag visual is a detached widget. The source card remains the single
    // real card in CardsContainer throughout the drag.
    UVHVOrderingCardWidget* DragVisual = CreateWidget<UVHVOrderingCardWidget>(GetOwningPlayer(), GetClass());
    if (DragVisual)
    {
        DragVisual->SetOrderingItem(CurrentItem);
        DragVisual->SetCardIndex(CardIndex);
        DragVisual->SetVisualState(false, false);
        OutOperation->DefaultDragVisual = DragVisual;
    }
    OutOperation->Pivot = EDragPivot::MouseDown;

    if (OwningOrderingWidget)
    {
        OwningOrderingWidget->BeginMouseDrag(CurrentItem.ItemID);
    }
}

bool UVHVOrderingCardWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (!OwningOrderingWidget)
    {
        return false;
    }

    if (!Cast<UVHVOrderingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        return false;
    }

    OwningOrderingWidget->UpdateMouseDrag(InDragDropEvent);
    OwningOrderingWidget->CommitMouseReorder();
    return true;
}

bool UVHVOrderingCardWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (OwningOrderingWidget && Cast<UVHVOrderingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        OwningOrderingWidget->UpdateMouseDrag(InDragDropEvent);
        return true;
    }

    return false;
}

void UVHVOrderingCardWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (OwningOrderingWidget && Cast<UVHVOrderingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        OwningOrderingWidget->EndMouseDrag();
    }

    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

void UVHVOrderingCardWidget::SetCardIndex(int32 InCardIndex)
{
    CardIndex = InCardIndex;
    if (CardColorIndex == INDEX_NONE)
    {
        CardColorIndex = InCardIndex % 3;
    }
    UpdatePositionText();
    ApplyCardColor();
}

void UVHVOrderingCardWidget::UpdatePositionText()
{
    if (!PositionText)
    {
        return;
    }

    PositionText->SetText(FText::AsNumber(CardIndex + 1));
}

void UVHVOrderingCardWidget::ApplyCardColor()
{
    if (!CardBorder)
    {
        return;
    }

    FLinearColor BorderColor;
    switch (CardColorIndex)
    {
    case 0:
        BorderColor = FLinearColor(0.15f, 0.45f, 0.72f, 1.0f);
        break;
    case 1:
        BorderColor = FLinearColor(0.20f, 0.58f, 0.42f, 1.0f);
        break;
    default:
        BorderColor = FLinearColor(0.74f, 0.54f, 0.18f, 1.0f);
        break;
    }

    CardBorder->SetBrushColor(BorderColor);
}
