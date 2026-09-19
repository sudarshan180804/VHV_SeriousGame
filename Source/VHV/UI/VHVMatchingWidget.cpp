#include "UI/VHVMatchingWidget.h"

#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/DragDropOperation.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "UI/VHVMatchingCardWidget.h"
#include "UI/VHVUIManagerComponent.h"

UVHVMatchingWidget::UVHVMatchingWidget()
{
    SetIsFocusable(true);
}

void UVHVMatchingWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (SubmitButton)
    {
        SubmitButton->OnClicked.AddDynamic(this, &UVHVMatchingWidget::HandleSubmitButtonClicked);
    }
}

void UVHVMatchingWidget::SetMatchingPairs(const TArray<FMatchingPair>& InPairs)
{
    MatchingPairs = InPairs;
    RightDisplayOrder.Reset();
    LeftToRight.Init(INDEX_NONE, MatchingPairs.Num());
    for (int32 Index = 0; Index < MatchingPairs.Num(); ++Index)
    {
        RightDisplayOrder.Add(Index);
    }
    ShuffleRightItems();
    FocusedLeftIndex = MatchingPairs.Num() > 0 ? 0 : INDEX_NONE;
    FocusedRightDisplayIndex = RightDisplayOrder.Num() > 0 ? 0 : INDEX_NONE;
    bLeftColumnFocused = true;
    DraggedLeftIndex = INDEX_NONE;
    HoveredRightDisplayIndex = INDEX_NONE;
    bConnectionDragActive = false;
    RefreshDisplay();
}

void UVHVMatchingWidget::SetOwningUIManager(UVHVUIManagerComponent* InUIManager)
{
    OwningUIManager = InUIManager;
}

TArray<FMatchingPair> UVHVMatchingWidget::GetCurrentMatches() const
{
    TArray<FMatchingPair> Matches;
    for (int32 LeftIndex = 0; LeftIndex < LeftToRight.Num(); ++LeftIndex)
    {
        const int32 RightIndex = LeftToRight[LeftIndex];
        if (MatchingPairs.IsValidIndex(LeftIndex) && MatchingPairs.IsValidIndex(RightIndex))
        {
            FMatchingPair Match;
            Match.LeftText = MatchingPairs[LeftIndex].LeftText;
            Match.RightText = MatchingPairs[RightIndex].RightText;
            Matches.Add(Match);
        }
    }
    return Matches;
}

void UVHVMatchingWidget::HandleCardSelected(const bool bIsLeftColumn, const int32 ItemIndex)
{
    if (bIsLeftColumn)
    {
        if (!MatchingPairs.IsValidIndex(ItemIndex))
        {
            return;
        }
        FocusedLeftIndex = ItemIndex;
        bLeftColumnFocused = true;
    }
    else
    {
        if (!RightDisplayOrder.IsValidIndex(ItemIndex))
        {
            return;
        }
        FocusedRightDisplayIndex = ItemIndex;
        bLeftColumnFocused = false;
        AssignFocusedRightToFocusedLeft();
    }
    UpdateCardVisuals();
}

void UVHVMatchingWidget::BeginConnectionDrag(const int32 LeftIndex, const FVector2D& ScreenPosition)
{
    if (!MatchingPairs.IsValidIndex(LeftIndex))
    {
        return;
    }
    DraggedLeftIndex = LeftIndex;
    HoveredRightDisplayIndex = INDEX_NONE;
    DragScreenPosition = ScreenPosition;
    bConnectionDragActive = true;
    FocusedLeftIndex = LeftIndex;
    bLeftColumnFocused = true;
    UpdateCardVisuals();
    InvalidateLayoutAndVolatility();
}

void UVHVMatchingWidget::UpdateConnectionDrag(const FVector2D& ScreenPosition, const int32 HoveredRightIndex)
{
    if (!bConnectionDragActive)
    {
        return;
    }
    DragScreenPosition = ScreenPosition;
    HoveredRightDisplayIndex = RightDisplayOrder.IsValidIndex(HoveredRightIndex) ? HoveredRightIndex : INDEX_NONE;
    UpdateCardVisuals();
    InvalidateLayoutAndVolatility();
}

void UVHVMatchingWidget::CommitConnectionDrag(const int32 RightDisplayIndex)
{
    if (!bConnectionDragActive || !MatchingPairs.IsValidIndex(DraggedLeftIndex) || !RightDisplayOrder.IsValidIndex(RightDisplayIndex))
    {
        EndConnectionDrag();
        return;
    }
    FocusedLeftIndex = DraggedLeftIndex;
    FocusedRightDisplayIndex = RightDisplayIndex;
    AssignFocusedRightToFocusedLeft();
    EndConnectionDrag();
}

void UVHVMatchingWidget::EndConnectionDrag()
{
    bConnectionDragActive = false;
    DraggedLeftIndex = INDEX_NONE;
    HoveredRightDisplayIndex = INDEX_NONE;
    UpdateCardVisuals();
    InvalidateLayoutAndVolatility();
}

FReply UVHVMatchingWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
    {
        MoveFocus(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
    {
        MoveFocus(1);
        return FReply::Handled();
    }
    if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
    {
        SetFocusedColumn(true);
        return FReply::Handled();
    }
    if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
    {
        SetFocusedColumn(false);
        return FReply::Handled();
    }
    if (Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Left)
    {
        ActivateFocusedSelection();
        return FReply::Handled();
    }
    if (Key == EKeys::BackSpace || Key == EKeys::Delete || Key == EKeys::Gamepad_FaceButton_Right)
    {
        RemoveFocusedLeftMatch();
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        SubmitCurrentMatches();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UVHVMatchingWidget::ActivateFocusedSelection()
{
    if (!MatchingPairs.IsValidIndex(FocusedLeftIndex) || !RightDisplayOrder.IsValidIndex(FocusedRightDisplayIndex))
    {
        return false;
    }

    AssignFocusedRightToFocusedLeft();
    return true;
}

bool UVHVMatchingWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (bConnectionDragActive && Cast<UVHVMatchingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        UpdateConnectionDrag(InDragDropEvent.GetScreenSpacePosition());
        return true;
    }
    return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

bool UVHVMatchingWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (bConnectionDragActive && Cast<UVHVMatchingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        EndConnectionDrag();
        return true;
    }
    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

int32 UVHVMatchingWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const
{
    const int32 PaintLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    int32 ConnectionLayer = PaintLayer + 1;
    for (int32 LeftIndex = 0; LeftIndex < LeftToRight.Num(); ++LeftIndex)
    {
        const int32 RightIndex = LeftToRight[LeftIndex];
        const int32 RightDisplayIndex = RightDisplayOrder.Find(RightIndex);
        if (!LeftCardWidgets.IsValidIndex(LeftIndex) || !RightCardWidgets.IsValidIndex(RightDisplayIndex) || !LeftCardWidgets[LeftIndex] || !RightCardWidgets[RightDisplayIndex])
        {
            continue;
        }
        TArray<FVector2D> Points;
        Points.Add(GetCardConnectionPoint(LeftCardWidgets[LeftIndex], false, AllottedGeometry));
        Points.Add(GetCardConnectionPoint(RightCardWidgets[RightDisplayIndex], true, AllottedGeometry));
        FSlateDrawElement::MakeLines(OutDrawElements, ConnectionLayer, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, FLinearColor(0.20f, 0.72f, 0.95f, 0.95f), true, 3.0f);
    }
    if (bConnectionDragActive && LeftCardWidgets.IsValidIndex(DraggedLeftIndex) && LeftCardWidgets[DraggedLeftIndex])
    {
        TArray<FVector2D> DragPoints;
        DragPoints.Add(GetCardConnectionPoint(LeftCardWidgets[DraggedLeftIndex], false, AllottedGeometry));
        DragPoints.Add(AllottedGeometry.AbsoluteToLocal(DragScreenPosition));
        FSlateDrawElement::MakeLines(OutDrawElements, ConnectionLayer + 1, AllottedGeometry.ToPaintGeometry(), DragPoints, ESlateDrawEffect::None, FLinearColor(1.0f, 0.82f, 0.20f, 0.95f), true, 3.0f);
        ConnectionLayer++;
    }
    return ConnectionLayer;
}

void UVHVMatchingWidget::RefreshDisplay()
{
    if (!LeftItemsContainer || !RightItemsContainer)
    {
        return;
    }

    LeftItemsContainer->ClearChildren();
    RightItemsContainer->ClearChildren();
    LeftCardWidgets.Reset();
    RightCardWidgets.Reset();
    TSubclassOf<UVHVMatchingCardWidget> WidgetClass = UVHVMatchingCardWidget::StaticClass();
    if (CardWidgetClass)
    {
        WidgetClass = CardWidgetClass;
    }
    else if (OwningUIManager && OwningUIManager->MatchingCardWidgetClass)
    {
        WidgetClass = OwningUIManager->MatchingCardWidgetClass;
    }

    for (int32 Index = 0; Index < MatchingPairs.Num(); ++Index)
    {
        UVHVMatchingCardWidget* LeftCard = CreateWidget<UVHVMatchingCardWidget>(this, WidgetClass);
        if (LeftCard)
        {
            LeftCard->SetOwningMatchingWidget(this);
            LeftItemsContainer->AddChild(LeftCard);
            if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(LeftCard->Slot))
            {
                CardSlot->SetPadding(FMargin(0.0f, 4.0f));
                CardSlot->SetHorizontalAlignment(HAlign_Fill);
            }
            LeftCardWidgets.Add(LeftCard);
        }
    }
    for (int32 DisplayIndex = 0; DisplayIndex < RightDisplayOrder.Num(); ++DisplayIndex)
    {
        UVHVMatchingCardWidget* RightCard = CreateWidget<UVHVMatchingCardWidget>(this, WidgetClass);
        if (RightCard)
        {
            RightCard->SetOwningMatchingWidget(this);
            RightItemsContainer->AddChild(RightCard);
            if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(RightCard->Slot))
            {
                CardSlot->SetPadding(FMargin(0.0f, 4.0f));
                CardSlot->SetHorizontalAlignment(HAlign_Fill);
            }
            RightCardWidgets.Add(RightCard);
        }
    }
    UpdateCardVisuals();
    UpdateSubmitHint();
}

void UVHVMatchingWidget::ShuffleRightItems()
{
    for (int32 Index = RightDisplayOrder.Num() - 1; Index > 0; --Index)
    {
        RightDisplayOrder.Swap(Index, FMath::RandRange(0, Index));
    }
}

void UVHVMatchingWidget::MoveFocus(const int32 Direction)
{
    int32& FocusedIndex = bLeftColumnFocused ? FocusedLeftIndex : FocusedRightDisplayIndex;
    const int32 ItemCount = bLeftColumnFocused ? MatchingPairs.Num() : RightDisplayOrder.Num();
    if (ItemCount == 0)
    {
        return;
    }
    FocusedIndex = FocusedIndex == INDEX_NONE ? 0 : (FocusedIndex + Direction + ItemCount) % ItemCount;
    UpdateCardVisuals();
}

void UVHVMatchingWidget::SetFocusedColumn(const bool bInLeftColumn)
{
    bLeftColumnFocused = bInLeftColumn;
    UpdateCardVisuals();
}

void UVHVMatchingWidget::AssignFocusedRightToFocusedLeft()
{
    if (!MatchingPairs.IsValidIndex(FocusedLeftIndex) || !RightDisplayOrder.IsValidIndex(FocusedRightDisplayIndex))
    {
        return;
    }
    const int32 RightIndex = RightDisplayOrder[FocusedRightDisplayIndex];
    for (int32 LeftIndex = 0; LeftIndex < LeftToRight.Num(); ++LeftIndex)
    {
        if (LeftIndex != FocusedLeftIndex && LeftToRight[LeftIndex] == RightIndex)
        {
            LeftToRight[LeftIndex] = INDEX_NONE;
        }
    }
    LeftToRight[FocusedLeftIndex] = RightIndex;
    UpdateCardVisuals();
}

void UVHVMatchingWidget::RemoveFocusedLeftMatch()
{
    if (LeftToRight.IsValidIndex(FocusedLeftIndex))
    {
        LeftToRight[FocusedLeftIndex] = INDEX_NONE;
        UpdateCardVisuals();
    }
}

void UVHVMatchingWidget::SubmitCurrentMatches()
{
    if (OwningUIManager)
    {
        OwningUIManager->SubmitMatchingAnswer(GetCurrentMatches());
    }
}

void UVHVMatchingWidget::UpdateCardVisuals()
{
    for (int32 LeftIndex = 0; LeftIndex < LeftCardWidgets.Num(); ++LeftIndex)
    {
        if (!LeftCardWidgets[LeftIndex] || !MatchingPairs.IsValidIndex(LeftIndex))
        {
            continue;
        }
        LeftCardWidgets[LeftIndex]->SetCardData(true, LeftIndex, MatchingPairs[LeftIndex].LeftText);
        LeftCardWidgets[LeftIndex]->SetVisualState(bLeftColumnFocused && LeftIndex == FocusedLeftIndex, LeftIndex == FocusedLeftIndex, LeftToRight.IsValidIndex(LeftIndex) && LeftToRight[LeftIndex] != INDEX_NONE, false, bConnectionDragActive && LeftIndex == DraggedLeftIndex);
    }
    for (int32 DisplayIndex = 0; DisplayIndex < RightCardWidgets.Num(); ++DisplayIndex)
    {
        if (!RightCardWidgets[DisplayIndex] || !RightDisplayOrder.IsValidIndex(DisplayIndex))
        {
            continue;
        }
        const int32 RightIndex = RightDisplayOrder[DisplayIndex];
        const bool bMapped = LeftToRight.Contains(RightIndex);
        RightCardWidgets[DisplayIndex]->SetCardData(false, DisplayIndex, MatchingPairs[RightIndex].RightText);
        RightCardWidgets[DisplayIndex]->SetVisualState(!bLeftColumnFocused && DisplayIndex == FocusedRightDisplayIndex, DisplayIndex == FocusedRightDisplayIndex, bMapped, bConnectionDragActive && DisplayIndex == HoveredRightDisplayIndex, false);
    }
}

void UVHVMatchingWidget::UpdateSubmitHint()
{
    if (SubmitHint)
    {
        SubmitHint->SetText(FText::FromString(TEXT("Drag from a left item to its match. Keyboard: arrows select, Space/E connects, Backspace removes, Enter submits.")));
    }
}

FVector2D UVHVMatchingWidget::GetCardConnectionPoint(const UVHVMatchingCardWidget* CardWidget, const bool bLeftSide, const FGeometry& AllottedGeometry) const
{
    const FGeometry CardGeometry = CardWidget->GetVisibleContentGeometry();
    const FVector2D CardPosition = AllottedGeometry.AbsoluteToLocal(CardGeometry.GetAbsolutePosition());
    const FVector2D CardSize = CardGeometry.GetLocalSize();
    return CardPosition + FVector2D(bLeftSide ? 0.0f : CardSize.X, CardSize.Y * 0.5f);
}

void UVHVMatchingWidget::HandleSubmitButtonClicked()
{
    SubmitCurrentMatches();
}
