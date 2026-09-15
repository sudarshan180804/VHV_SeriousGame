#include "UI/VHVOrderingWidget.h"

#include "Layout/Geometry.h"
#include "UI/VHVOrderingCardWidget.h"
#include "UI/VHVUIManagerComponent.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

void UVHVOrderingWidget::SetOrderingItems(const TArray<FOrderingItem>& Items)
{
    SourceItems = Items;
    if (SourceItems.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Invalid ordering state. Items=%d"), SourceItems.Num());
        CurrentOrder.Reset();
        CurrentOrderingItems.Reset();
        FocusedIndex = INDEX_NONE;
        FocusedItemID.Reset();
        KeyboardReorderIndex = INDEX_NONE;
        bKeyboardReorderMode = false;
        ClearDragState();
        if (SubmitHint)
        {
            SubmitHint->SetText(FText::FromString(TEXT("Invalid ordering setup")));
        }
        return;
    }

    ApplyRandomizedSourceOrder();
    FocusedIndex = CurrentOrder.Num() > 0 ? 0 : INDEX_NONE;
    FocusedItemID = CurrentOrder.IsValidIndex(FocusedIndex) ? CurrentOrder[FocusedIndex] : FString();
    KeyboardReorderIndex = INDEX_NONE;
    bKeyboardReorderMode = false;
    ClearDragState();
    RefreshDisplay();
    UpdateSelectionVisuals();
    UpdateSubmitHint();
}

void UVHVOrderingWidget::SetOwningUIManager(UVHVUIManagerComponent* InUIManager)
{
    OwningUIManager = InUIManager;
}

TArray<FString> UVHVOrderingWidget::GetCurrentOrder() const
{
    return CurrentOrder;
}

void UVHVOrderingWidget::ResetOrder()
{
    if (SourceItems.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Invalid ordering state. Cannot reset."));
        return;
    }

    ApplyRandomizedSourceOrder();
    FocusedIndex = CurrentOrder.Num() > 0 ? 0 : INDEX_NONE;
    FocusedItemID = CurrentOrder.IsValidIndex(FocusedIndex) ? CurrentOrder[FocusedIndex] : FString();
    KeyboardReorderIndex = INDEX_NONE;
    bKeyboardReorderMode = false;
    ClearDragState();
    RefreshDisplay();
    UpdateSelectionVisuals();
    UpdateSubmitHint();
}

FReply UVHVOrderingWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();

    if (Key == EKeys::E)
    {
        if (CurrentOrder.Num() == 0 || FocusedIndex == INDEX_NONE)
        {
            return FReply::Handled();
        }

        if (bKeyboardReorderMode)
        {
            bKeyboardReorderMode = false;
            UpdateSelectionVisuals();
            UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Release Grab ItemID=%s"), *FocusedItemID);
            return FReply::Handled();
        }

        bKeyboardReorderMode = true;
        KeyboardReorderIndex = FocusedIndex;
        UpdateSelectionVisuals();
        UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Grab ItemID=%s"), *FocusedItemID);
        return FReply::Handled();
    }

    if (Key == EKeys::Up)
    {
        if (CurrentOrder.Num() == 0)
        {
            return FReply::Handled();
        }

        if (bKeyboardReorderMode)
        {
            if (FocusedIndex > 0)
            {
                const int32 FromIndex = FocusedIndex;
                const int32 ToIndex = FocusedIndex - 1;
                if (MoveItem(FromIndex, ToIndex))
                {
                    UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Move Grabbed ItemID=%s NewIndex=%d"), *FocusedItemID, FocusedIndex);
                }
            }
            return FReply::Handled();
        }

        const int32 NextIndex = FMath::Max(0, FocusedIndex - 1);
        if (FocusedIndex != NextIndex)
        {
            SetFocusedIndex(NextIndex);
        }
        return FReply::Handled();
    }

    if (Key == EKeys::Down)
    {
        if (CurrentOrder.Num() == 0)
        {
            return FReply::Handled();
        }

        if (bKeyboardReorderMode)
        {
            if (FocusedIndex < CurrentOrder.Num() - 1)
            {
                const int32 FromIndex = FocusedIndex;
                const int32 ToIndex = FocusedIndex + 1;
                if (MoveItem(FromIndex, ToIndex))
                {
                    UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Move Grabbed ItemID=%s NewIndex=%d"), *FocusedItemID, FocusedIndex);
                }
            }
            return FReply::Handled();
        }

        const int32 NextIndex = FMath::Min(CurrentOrder.Num() - 1, FocusedIndex + 1);
        if (FocusedIndex != NextIndex)
        {
            SetFocusedIndex(NextIndex);
        }
        return FReply::Handled();
    }

    if (Key == EKeys::Enter)
    {
        if (OwningUIManager)
        {
            UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Submitting order"));
            OwningUIManager->SubmitOrderingAnswer(GetCurrentOrder());
        }
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UVHVOrderingWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (Cast<UVHVOrderingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        UpdateMouseDrag(InDragDropEvent);
        return true;
    }

    return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

bool UVHVOrderingWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (Cast<UVHVOrderingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        UpdateMouseDrag(InDragDropEvent);
        CommitMouseReorder();
        return true;
    }

    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UVHVOrderingWidget::BeginMouseDrag(const FString& ItemID)
{
    const int32 SourceIndex = CurrentOrder.Find(ItemID);
    if (SourceIndex == INDEX_NONE)
    {
        return;
    }

    bMouseDragging = true;
    MouseDragItemID = ItemID;
    MouseDragSourceIndex = SourceIndex;
    // Slots are always measured against the order with the dragged item removed.
    MouseInsertionIndex = FMath::Clamp(SourceIndex, 0, CurrentOrder.Num() - 1);
    MouseDropTargetIndex = INDEX_NONE;
    UpdateMouseDropTarget();
    UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Mouse drag started ItemID=%s"), *ItemID);
}

void UVHVOrderingWidget::UpdateMouseDrag(const FDragDropEvent& InDragDropEvent)
{
    if (!bMouseDragging || !CardsContainer)
    {
        return;
    }

    const FVector2D CursorLocalToContainer = CardsContainer->GetCachedGeometry().AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
    const int32 ProposedSlot = CalculateMouseInsertionSlot(CursorLocalToContainer);
    if (MouseInsertionIndex != ProposedSlot)
    {
        MouseInsertionIndex = ProposedSlot;
        UpdateMouseDropTarget();
    }
}

void UVHVOrderingWidget::CommitMouseReorder()
{
    if (!bMouseDragging || MouseDragItemID.IsEmpty())
    {
        return;
    }

    const int32 SourceIndex = CurrentOrder.Find(MouseDragItemID);
    if (SourceIndex == INDEX_NONE)
    {
        EndMouseDrag();
        return;
    }

    TArray<FString> UpdatedOrder = CurrentOrder;
    UpdatedOrder.RemoveAt(SourceIndex);
    const int32 FinalSlot = FMath::Clamp(MouseInsertionIndex, 0, UpdatedOrder.Num());
    UpdatedOrder.Insert(MouseDragItemID, FinalSlot);

    CurrentOrder = UpdatedOrder;
    CurrentOrderingItems.Reset();
    for (const FString& ItemID : CurrentOrder)
    {
        CurrentOrderingItems.Add(ResolveItemByID(ItemID));
    }

    SynchronizeCardWidgets();
    FocusedItemID = MouseDragItemID;
    FocusedIndex = CurrentOrder.Find(MouseDragItemID);
    UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Mouse reorder ItemID=%s NewIndex=%d"), *MouseDragItemID, FocusedIndex);
    EndMouseDrag();
    UpdateSelectionVisuals();
}

void UVHVOrderingWidget::EndMouseDrag()
{
    ClearDragState();
    UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Mouse drag ended"));
}

void UVHVOrderingWidget::RefreshDisplay()
{
    if (!CardsContainer)
    {
        return;
    }

    CardsContainer->ClearChildren();
    CardWidgets.Reset();

    TSubclassOf<UVHVOrderingCardWidget> WidgetClass = UVHVOrderingCardWidget::StaticClass();
    if (CardWidgetClass)
    {
        WidgetClass = CardWidgetClass;
    }
    else if (OwningUIManager && OwningUIManager->OrderingCardWidgetClass)
    {
        WidgetClass = OwningUIManager->OrderingCardWidgetClass;
    }

    UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Card class valid=%s"), WidgetClass ? TEXT("true") : TEXT("false"));

    for (int32 Index = 0; Index < CurrentOrder.Num(); ++Index)
    {
        const FString ItemID = CurrentOrder[Index];
        const FOrderingItem Item = ResolveItemByID(ItemID);
        UVHVOrderingCardWidget* CardWidget = CreateWidget<UVHVOrderingCardWidget>(this, WidgetClass);
        if (!CardWidget)
        {
            continue;
        }

        UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Creating card ItemID=%s"), *ItemID);
        CardWidget->SetOwningOrderingWidget(this);
        CardWidget->SetCardIndex(Index);
        CardWidget->SetOrderingItem(Item);
        CardWidget->SetVisualState(FocusedItemID == ItemID, false);
        CardsContainer->AddChild(CardWidget);

        if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(CardWidget->Slot))
        {
            CardSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
            CardSlot->SetHorizontalAlignment(HAlign_Fill);
        }

        CardWidgets.Add(CardWidget);
        UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Card added successfully=%s"), CardWidget->GetParent() ? TEXT("true") : TEXT("false"));
    }

    UpdatePositionLabels();
    UpdateSelectionVisuals();
    UpdateSubmitHint();
}

void UVHVOrderingWidget::ApplyRandomizedSourceOrder()
{
    TArray<FString> OrderedIDs;
    OrderedIDs.Reserve(SourceItems.Num());
    for (const FOrderingItem& Item : SourceItems)
    {
        OrderedIDs.Add(Item.ItemID);
    }

    for (int32 Index = OrderedIDs.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = FMath::RandRange(0, Index);
        OrderedIDs.Swap(Index, SwapIndex);
    }

    CurrentOrder = OrderedIDs;
    CurrentOrderingItems.Reset();
    for (const FString& ItemID : CurrentOrder)
    {
        CurrentOrderingItems.Add(ResolveItemByID(ItemID));
    }
}

void UVHVOrderingWidget::ShuffleItems(TArray<FOrderingItem>& Items)
{
    for (int32 Index = Items.Num() - 1; Index > 0; --Index)
    {
        const int32 SwapIndex = FMath::RandRange(0, Index);
        Items.Swap(Index, SwapIndex);
    }
}

FOrderingItem UVHVOrderingWidget::ResolveItemByID(const FString& ItemID) const
{
    for (const FOrderingItem& Item : SourceItems)
    {
        if (Item.ItemID == ItemID)
        {
            return Item;
        }
    }

    FOrderingItem EmptyItem;
    EmptyItem.ItemID = ItemID;
    EmptyItem.ItemText = ItemID;
    return EmptyItem;
}

int32 UVHVOrderingWidget::CalculateMouseInsertionSlot(const FVector2D& CursorLocalToContainer) const
{
    const int32 SourceIndex = MouseDragItemID.IsEmpty() ? INDEX_NONE : CurrentOrder.Find(MouseDragItemID);
    if (SourceIndex == INDEX_NONE)
    {
        return 0;
    }

    TArray<int32> ReducedIndices;
    ReducedIndices.Reserve(CurrentOrder.Num() - 1);
    for (int32 Index = 0; Index < CurrentOrder.Num(); ++Index)
    {
        if (Index != SourceIndex)
        {
            ReducedIndices.Add(Index);
        }
    }

    if (ReducedIndices.Num() == 0)
    {
        return 0;
    }

    const int32 FirstVisibleCardIndex = ReducedIndices[0];
    const int32 LastVisibleCardIndex = ReducedIndices[ReducedIndices.Num() - 1];
    const FGeometry* FirstCardGeometry = nullptr;
    const FGeometry* LastCardGeometry = nullptr;
    if (CardWidgets.IsValidIndex(FirstVisibleCardIndex) && CardWidgets[FirstVisibleCardIndex])
    {
        FirstCardGeometry = &CardWidgets[FirstVisibleCardIndex]->GetCachedGeometry();
    }
    if (CardWidgets.IsValidIndex(LastVisibleCardIndex) && CardWidgets[LastVisibleCardIndex])
    {
        LastCardGeometry = &CardWidgets[LastVisibleCardIndex]->GetCachedGeometry();
    }
    if (!FirstCardGeometry || !LastCardGeometry)
    {
        return 0;
    }
    const FGeometry& ContainerGeometry = CardsContainer->GetCachedGeometry();
    const float FirstCardCenterY = ContainerGeometry.AbsoluteToLocal(FirstCardGeometry->GetAbsolutePosition()).Y + (FirstCardGeometry->GetLocalSize().Y * 0.5f);
    const float LastCardCenterY = ContainerGeometry.AbsoluteToLocal(LastCardGeometry->GetAbsolutePosition()).Y + (LastCardGeometry->GetLocalSize().Y * 0.5f);

    if (CursorLocalToContainer.Y <= FirstCardCenterY)
    {
        return 0;
    }

    if (CursorLocalToContainer.Y >= LastCardCenterY)
    {
        return ReducedIndices.Num();
    }

    for (int32 ReducedIndex = 0; ReducedIndex < ReducedIndices.Num() - 1; ++ReducedIndex)
    {
        const int32 CurrentCardIndex = ReducedIndices[ReducedIndex];
        const int32 NextCardIndex = ReducedIndices[ReducedIndex + 1];
        if (!CardWidgets.IsValidIndex(CurrentCardIndex) || !CardWidgets[CurrentCardIndex] || !CardWidgets.IsValidIndex(NextCardIndex) || !CardWidgets[NextCardIndex])
        {
            continue;
        }

        const FGeometry& CurrentCardGeometry = CardWidgets[CurrentCardIndex]->GetCachedGeometry();
        const FGeometry& NextCardGeometry = CardWidgets[NextCardIndex]->GetCachedGeometry();
        const float CurrentCenter = ContainerGeometry.AbsoluteToLocal(CurrentCardGeometry.GetAbsolutePosition()).Y + (CurrentCardGeometry.GetLocalSize().Y * 0.5f);
        const float NextCenter = ContainerGeometry.AbsoluteToLocal(NextCardGeometry.GetAbsolutePosition()).Y + (NextCardGeometry.GetLocalSize().Y * 0.5f);
        const float Midpoint = (CurrentCenter + NextCenter) * 0.5f;
        if (CursorLocalToContainer.Y < Midpoint)
        {
            return ReducedIndex + 1;
        }
    }

    return ReducedIndices.Num();
}

bool UVHVOrderingWidget::MoveItem(int32 FromIndex, int32 ToIndex)
{
    if (!CurrentOrder.IsValidIndex(FromIndex))
    {
        return false;
    }

    const int32 InsertIndex = FMath::Clamp(ToIndex, 0, CurrentOrder.Num());
    if (FromIndex == InsertIndex)
    {
        return false;
    }

    const FString MovedItemID = CurrentOrder[FromIndex];
    CurrentOrder.RemoveAt(FromIndex);
    CurrentOrder.Insert(MovedItemID, InsertIndex);

    CurrentOrderingItems.Reset();
    for (const FString& ItemID : CurrentOrder)
    {
        CurrentOrderingItems.Add(ResolveItemByID(ItemID));
    }

    SynchronizeCardWidgets();
    FocusedItemID = MovedItemID;
    FocusedIndex = CurrentOrder.Find(MovedItemID);
    KeyboardReorderIndex = FocusedIndex;
    UpdateSelectionVisuals();
    UpdatePositionLabels();
    return true;
}

void UVHVOrderingWidget::MoveFocusedCard(int32 Direction)
{
    if (CurrentOrder.Num() == 0)
    {
        return;
    }

    if (bKeyboardReorderMode)
    {
        const int32 NewIndex = FocusedIndex + Direction;
        if (NewIndex < 0 || NewIndex >= CurrentOrder.Num())
        {
            return;
        }

        if (MoveItem(FocusedIndex, NewIndex))
        {
            UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Move Grabbed ItemID=%s NewIndex=%d"), *FocusedItemID, FocusedIndex);
        }
        return;
    }

    if (FocusedIndex == INDEX_NONE)
    {
        SetFocusedIndex(0);
        return;
    }

    const int32 NextIndex = FMath::Clamp(FocusedIndex + Direction, 0, CurrentOrder.Num() - 1);
    if (NextIndex == FocusedIndex)
    {
        return;
    }

    SetFocusedIndex(NextIndex);
}

void UVHVOrderingWidget::SetFocusedIndex(int32 NewIndex)
{
    if (CurrentOrder.Num() == 0)
    {
        FocusedIndex = INDEX_NONE;
        FocusedItemID.Reset();
        UpdateSelectionVisuals();
        return;
    }

    FocusedIndex = FMath::Clamp(NewIndex, 0, CurrentOrder.Num() - 1);
    FocusedItemID = CurrentOrder[FocusedIndex];
    UpdateSelectionVisuals();
    UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Focus ItemID=%s"), *FocusedItemID);
}

void UVHVOrderingWidget::SetKeyboardReorderMode(bool bReorder)
{
    bKeyboardReorderMode = bReorder;
    UpdateSelectionVisuals();
    if (bKeyboardReorderMode)
    {
        UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Grab ItemID=%s"), *FocusedItemID);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Release Grab ItemID=%s"), *FocusedItemID);
    }
}

void UVHVOrderingWidget::UpdateCardPresentationStates()
{
    UpdateSelectionVisuals();
}

void UVHVOrderingWidget::ClearDragState()
{
    bMouseDragging = false;
    MouseDragItemID.Reset();
    MouseDragSourceIndex = INDEX_NONE;
    MouseInsertionIndex = INDEX_NONE;
    ClearMouseDropTarget();
}

void UVHVOrderingWidget::SynchronizeCardWidgets()
{
    if (!CardsContainer)
    {
        return;
    }

    TArray<TObjectPtr<UVHVOrderingCardWidget>> OrderedCards;
    OrderedCards.Reserve(CurrentOrder.Num());

    for (const FString& ItemID : CurrentOrder)
    {
        for (UVHVOrderingCardWidget* CardWidget : CardWidgets)
        {
            if (!CardWidget)
            {
                continue;
            }

            if (CardWidget->GetItemID() == ItemID)
            {
                OrderedCards.Add(CardWidget);
                break;
            }
        }
    }

    CardWidgets = OrderedCards;
    for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
    {
        if (!CardWidgets[Index])
        {
            continue;
        }

        CardsContainer->InsertChildAt(Index, CardWidgets[Index]);
        if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(CardWidgets[Index]->Slot))
        {
            CardSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 6.0f));
            CardSlot->SetHorizontalAlignment(HAlign_Fill);
        }
        CardWidgets[Index]->SetCardIndex(Index);
        CardWidgets[Index]->SetOrderingItem(ResolveItemByID(CardWidgets[Index]->GetItemID()));
    }

    UpdatePositionLabels();
    UpdateSelectionVisuals();
}

void UVHVOrderingWidget::UpdatePositionLabels()
{
    for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
    {
        if (CardWidgets[Index])
        {
            CardWidgets[Index]->SetCardIndex(Index);
            CardWidgets[Index]->UpdatePositionText();
        }
    }
}

void UVHVOrderingWidget::UpdateSelectionVisuals()
{
    for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
    {
        if (!CardWidgets[Index])
        {
            continue;
        }

        const FString ItemID = CardWidgets[Index]->GetItemID();
        const bool bIsFocused = FocusedItemID == ItemID;
        const bool bIsSelected = bKeyboardReorderMode && FocusedItemID == ItemID;
        CardWidgets[Index]->SetVisualState(bIsFocused, bIsSelected);
    }
}

void UVHVOrderingWidget::ClearMouseDropTarget()
{
    for (UVHVOrderingCardWidget* CardWidget : CardWidgets)
    {
        if (CardWidget)
        {
            CardWidget->SetMouseDropTarget(false);
        }
    }
    MouseDropTargetIndex = INDEX_NONE;
}

void UVHVOrderingWidget::UpdateMouseDropTarget()
{
    if (!bMouseDragging || MouseInsertionIndex == INDEX_NONE)
    {
        ClearMouseDropTarget();
        return;
    }
    const int32 SourceIndex = MouseDragItemID.IsEmpty() ? INDEX_NONE : CurrentOrder.Find(MouseDragItemID);
    if (SourceIndex == INDEX_NONE)
    {
        return;
    }

    TArray<FString> ReducedOrder;
    ReducedOrder.Reserve(CurrentOrder.Num() - 1);
    for (int32 Index = 0; Index < CurrentOrder.Num(); ++Index)
    {
        if (Index != SourceIndex)
        {
            ReducedOrder.Add(CurrentOrder[Index]);
        }
    }

    int32 NewTargetIndex = INDEX_NONE;
    if (ReducedOrder.Num() > 0)
    {
        const int32 ReducedTargetIndex = MouseInsertionIndex == 0 ? 0 : FMath::Min(MouseInsertionIndex - 1, ReducedOrder.Num() - 1);
        const FString TargetItemID = ReducedOrder[ReducedTargetIndex];
        NewTargetIndex = CurrentOrder.Find(TargetItemID);
    }

    if (MouseDropTargetIndex != NewTargetIndex)
    {
        if (CardWidgets.IsValidIndex(MouseDropTargetIndex) && CardWidgets[MouseDropTargetIndex])
        {
            CardWidgets[MouseDropTargetIndex]->SetMouseDropTarget(false);
        }
        if (CardWidgets.IsValidIndex(NewTargetIndex) && CardWidgets[NewTargetIndex])
        {
            CardWidgets[NewTargetIndex]->SetMouseDropTarget(true);
        }

        MouseDropTargetIndex = NewTargetIndex;
        if (NewTargetIndex != INDEX_NONE)
        {
            UE_LOG(LogTemp, Warning, TEXT("[VHVOrdering] Mouse drop target ItemID=%s InsertionSlot=%d"), *CurrentOrder[NewTargetIndex], MouseInsertionIndex);
        }
    }
}

void UVHVOrderingWidget::UpdateSubmitHint()
{
    if (!SubmitHint)
    {
        return;
    }

    if (CurrentOrder.Num() < 2)
    {
        SubmitHint->SetText(FText::FromString(TEXT("Invalid ordering")));
        return;
    }

    SubmitHint->SetText(FText::FromString(TEXT("Use ↑/↓ to focus, E to reorder, and Enter to submit")));
}
