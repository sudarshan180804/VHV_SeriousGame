#include "UI/VHVOrderingWidget.h"

#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UI/VHVOrderingCardWidget.h"
#include "UI/VHVUIManagerComponent.h"
#include "Blueprint/DragDropOperation.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

UVHVOrderingWidget::UVHVOrderingWidget()
{
    SetIsFocusable(true);

    static ConstructorHelpers::FObjectFinder<UTexture2D> BackgroundFinder(
        TEXT("/Game/VHV_Stuff/UI/Backgrounds/T_BG_OrderingVillage.T_BG_OrderingVillage"));
    if (BackgroundFinder.Succeeded())
    {
        OrderingBackgroundTexture = BackgroundFinder.Object;
    }
}

TSharedRef<SWidget> UVHVOrderingWidget::RebuildWidget()
{
    BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
    BackgroundBrush.SetResourceObject(OrderingBackgroundTexture);
    if (OrderingBackgroundTexture)
    {
        BackgroundBrush.SetImageSize(FVector2D(
            OrderingBackgroundTexture->GetSizeX(), OrderingBackgroundTexture->GetSizeY()));
    }
    BackgroundShadeBrush = VHVActivityUIStyle::RoundedBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.12f), 0.0f);
    HeaderDividerBrush = VHVActivityUIStyle::RoundedBrush(VHVActivityUIStyle::DividerGold(), 1.0f);
    KeycapBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(29, 34, 38, 145), 6.0f,
        VHVActivityUIStyle::BorderNeutral(), VHVActivityUIStyle::BorderNormalWidth);

    ConfirmButtonStyle = FButtonStyle()
        .SetNormal(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::GlassSelected(), 28.0f,
            VHVActivityUIStyle::BorderGold(), VHVActivityUIStyle::BorderSelectedWidth))
        .SetHovered(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::FromSRGB(52, 44, 31, 184), 28.0f,
            VHVActivityUIStyle::GoldSelected(), VHVActivityUIStyle::BorderSelectedWidth))
        .SetPressed(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::GlassFocused(), 28.0f,
            VHVActivityUIStyle::BorderGold(), VHVActivityUIStyle::BorderSelectedWidth))
        .SetDisabled(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::GlassCard(), 28.0f,
            VHVActivityUIStyle::BorderNeutral(), VHVActivityUIStyle::BorderNormalWidth))
        .SetNormalPadding(FMargin(0.0f))
        .SetPressedPadding(FMargin(1.0f, 2.0f, 0.0f, 0.0f));

    const auto MakeKeycap = [this](const TCHAR* KeyText) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(&KeycapBrush)
            .Padding(FMargin(8.0f, 3.0f))
            [
                SNew(STextBlock)
                .Font(VHVActivityUIStyle::MediumFont(13))
                .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                .Text(FText::FromString(KeyText))
            ];
    };

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
            SNew(SBorder)
            .BorderImage(&BackgroundShadeBrush)
        ]
        + SOverlay::Slot()
        [
            SNew(SConstraintCanvas)
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.18f, 0.085f, 0.82f, 0.285f))
            .Offset(FMargin(0.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                [
                    SNew(STextBlock)
                    .Font(VHVActivityUIStyle::MediumFont(VHVActivityUIStyle::HeaderFontSize))
                    .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
                    .ShadowOffset(FVector2D(1.0f, 2.0f))
                    .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
                    .Text(FText::FromString(TEXT("ORDERING")))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                .Padding(FMargin(0.0f, 9.0f, 0.0f, 18.0f))
                [
                    SNew(SBox)
                    .WidthOverride(VHVActivityUIStyle::HeaderDividerWidth)
                    .HeightOverride(VHVActivityUIStyle::HeaderDividerHeight)
                    [
                        SNew(SBorder)
                        .BorderImage(&HeaderDividerBrush)
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                [
                    SNew(STextBlock)
                    .Font(VHVActivityUIStyle::MediumFont(VHVActivityUIStyle::OrderingTitleFontSize))
                    .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                    .Justification(ETextJustify::Center)
                    .ShadowOffset(FVector2D(1.0f, 2.0f))
                    .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.62f))
                    .Text(FText::FromString(TEXT("Put the steps in the correct order")))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Center)
                .Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(VHVActivityUIStyle::InstructionFontSize))
                    .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                    .Justification(ETextJustify::Center)
                    .ShadowOffset(FVector2D(1.0f, 1.0f))
                    .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f))
                    .Text(FText::FromString(TEXT("Drag the cards left or right to arrange them.")))
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.05f, 0.315f, 0.95f, 0.755f))
            .Offset(FMargin(0.0f))
            [
                SNew(SScaleBox)
                .Stretch(EStretch::ScaleToFit)
                .StretchDirection(EStretchDirection::DownOnly)
                [
                    SAssignNew(CardsSlateContainer, SHorizontalBox)
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.32f, 0.775f, 0.68f, 0.82f))
            .Offset(FMargin(0.0f))
            [
                SAssignNew(SubmitHintSlate, STextBlock)
                .Font(VHVActivityUIStyle::RegularFont(15))
                .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                .Justification(ETextJustify::Center)
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.44f, 0.855f, 0.79f, 0.915f))
            .Offset(FMargin(0.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    MakeKeycap(TEXT("E"))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(7.0f, 0.0f, 20.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(14))
                    .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                    .Text(FText::FromString(TEXT("Pick Up / Drop")))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    MakeKeycap(TEXT("A / D"))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(7.0f, 0.0f, 20.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(14))
                    .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                    .Text(FText::FromString(TEXT("Move")))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    MakeKeycap(TEXT("ENTER"))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(7.0f, 0.0f, 0.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(14))
                    .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
                    .Text(FText::FromString(TEXT("Confirm")))
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.945f, 0.875f))
            .Alignment(FVector2D(1.0f, 0.5f))
            .Offset(FMargin(
                0.0f,
                0.0f,
                VHVActivityUIStyle::OrderingConfirmWidth,
                VHVActivityUIStyle::OrderingConfirmHeight))
            [
                SNew(SBox)
                .WidthOverride(VHVActivityUIStyle::OrderingConfirmWidth)
                .HeightOverride(VHVActivityUIStyle::OrderingConfirmHeight)
                [
                    SNew(SButton)
                    .ButtonStyle(&ConfirmButtonStyle)
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    .ContentPadding(FMargin(22.0f, 10.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UVHVOrderingWidget::HandleConfirmClicked))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(16))
                        .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                        .Text(FText::FromString(TEXT("Confirm Order")))
                    ]
                ]
            ]
        ];

    RefreshDisplay();
    UpdateSubmitHint();
    return Result;
}

void UVHVOrderingWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    CardsSlateContainer.Reset();
    SubmitHintSlate.Reset();
    CardWidgets.Reset();
}

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
        UpdateSubmitHint();
        RefreshDisplay();
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

    if (Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Left)
    {
        ToggleFocusedCardGrab();
        return FReply::Handled();
    }

    if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
    {
        MoveFocusedCard(-1);
        return FReply::Handled();
    }

    if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
    {
        MoveFocusedCard(1);
        return FReply::Handled();
    }

    if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        return HandleConfirmClicked();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UVHVOrderingWidget::HandleConfirmClicked()
{
    if (OwningUIManager && CurrentOrder.Num() >= 2)
    {
        OwningUIManager->SubmitOrderingAnswer(GetCurrentOrder());
    }
    return FReply::Handled();
}

bool UVHVOrderingWidget::ToggleFocusedCardGrab()
{
    if (CurrentOrder.Num() == 0 || FocusedIndex == INDEX_NONE)
    {
        return false;
    }

    if (!bKeyboardReorderMode)
    {
        KeyboardReorderIndex = FocusedIndex;
    }
    SetKeyboardReorderMode(!bKeyboardReorderMode);
    return true;
}

bool UVHVOrderingWidget::NativeOnDragOver(
    const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    if (Cast<UVHVOrderingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        UpdateMouseDrag(InDragDropEvent);
        return true;
    }
    return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

bool UVHVOrderingWidget::NativeOnDrop(
    const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
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
    MouseInsertionIndex = FMath::Clamp(SourceIndex, 0, CurrentOrder.Num() - 1);
    MouseDropTargetIndex = INDEX_NONE;
    FocusedItemID = ItemID;
    FocusedIndex = SourceIndex;
    UpdateMouseDropTarget();
    UpdateSelectionVisuals();
    UpdateSubmitHint();
}

void UVHVOrderingWidget::UpdateMouseDrag(const FDragDropEvent& InDragDropEvent)
{
    if (!bMouseDragging || !CardsSlateContainer)
    {
        return;
    }

    const FVector2D CursorLocal = CardsSlateContainer->GetCachedGeometry().AbsoluteToLocal(
        InDragDropEvent.GetScreenSpacePosition());
    const int32 ProposedSlot = CalculateMouseInsertionSlot(CursorLocal);
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

    FocusedItemID = MouseDragItemID;
    FocusedIndex = CurrentOrder.Find(MouseDragItemID);
    SynchronizeCardWidgets();
    EndMouseDrag();
}

void UVHVOrderingWidget::EndMouseDrag()
{
    ClearDragState();
    UpdateSelectionVisuals();
    UpdateSubmitHint();
    SetKeyboardFocus();
}

void UVHVOrderingWidget::RefreshDisplay()
{
    if (!CardsSlateContainer)
    {
        return;
    }

    CardsSlateContainer->ClearChildren();
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

    for (int32 Index = 0; Index < CurrentOrder.Num(); ++Index)
    {
        UVHVOrderingCardWidget* CardWidget = CreateWidget<UVHVOrderingCardWidget>(this, WidgetClass);
        if (!CardWidget)
        {
            continue;
        }

        const FString ItemID = CurrentOrder[Index];
        CardWidget->SetOwningOrderingWidget(this);
        CardWidget->SetCardIndex(Index);
        CardWidget->SetOrderingItem(ResolveItemByID(ItemID));
        CardWidget->SetVisualState(FocusedItemID == ItemID, false);

        const float HalfGap = VHVActivityUIStyle::OrderingCardGap * 0.5f;
        CardsSlateContainer->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(Index == 0 ? 0.0f : HalfGap, 0.0f,
            Index + 1 == CurrentOrder.Num() ? 0.0f : HalfGap, 0.0f))
        [
            CardWidget->TakeWidget()
        ];
        CardWidgets.Add(CardWidget);
    }

    UpdatePositionLabels();
    UpdateSelectionVisuals();
    UpdateSubmitHint();
}

void UVHVOrderingWidget::ApplyRandomizedSourceOrder()
{
    CurrentOrder.Reset();
    CurrentOrder.Reserve(SourceItems.Num());
    for (const FOrderingItem& Item : SourceItems)
    {
        CurrentOrder.Add(Item.ItemID);
    }

    for (int32 Index = CurrentOrder.Num() - 1; Index > 0; --Index)
    {
        CurrentOrder.Swap(Index, FMath::RandRange(0, Index));
    }

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
        Items.Swap(Index, FMath::RandRange(0, Index));
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
    if (SourceIndex == INDEX_NONE || !CardsSlateContainer)
    {
        return 0;
    }

    TArray<int32> ReducedIndices;
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

    const FGeometry& ContainerGeometry = CardsSlateContainer->GetCachedGeometry();
    const auto GetCardCenterX = [this, &ContainerGeometry](const int32 Index) -> float
    {
        if (!CardWidgets.IsValidIndex(Index) || !CardWidgets[Index])
        {
            return 0.0f;
        }
        const FGeometry& CardGeometry = CardWidgets[Index]->GetCachedGeometry();
        return ContainerGeometry.AbsoluteToLocal(CardGeometry.GetAbsolutePosition()).X
            + CardGeometry.GetLocalSize().X * 0.5f;
    };

    const float FirstCenter = GetCardCenterX(ReducedIndices[0]);
    const float LastCenter = GetCardCenterX(ReducedIndices.Last());
    if (CursorLocalToContainer.X <= FirstCenter)
    {
        return 0;
    }
    if (CursorLocalToContainer.X >= LastCenter)
    {
        return ReducedIndices.Num();
    }

    for (int32 ReducedIndex = 0; ReducedIndex < ReducedIndices.Num() - 1; ++ReducedIndex)
    {
        const float CurrentCenter = GetCardCenterX(ReducedIndices[ReducedIndex]);
        const float NextCenter = GetCardCenterX(ReducedIndices[ReducedIndex + 1]);
        if (CursorLocalToContainer.X < (CurrentCenter + NextCenter) * 0.5f)
        {
            return ReducedIndex + 1;
        }
    }
    return ReducedIndices.Num();
}

bool UVHVOrderingWidget::MoveItem(const int32 FromIndex, const int32 ToIndex)
{
    if (!CurrentOrder.IsValidIndex(FromIndex))
    {
        return false;
    }

    const int32 InsertIndex = FMath::Clamp(ToIndex, 0, CurrentOrder.Num() - 1);
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

    FocusedItemID = MovedItemID;
    FocusedIndex = CurrentOrder.Find(MovedItemID);
    KeyboardReorderIndex = FocusedIndex;
    SynchronizeCardWidgets();
    return true;
}

void UVHVOrderingWidget::MoveFocusedCard(const int32 Direction)
{
    if (CurrentOrder.Num() == 0)
    {
        return;
    }

    if (bKeyboardReorderMode)
    {
        const int32 NewIndex = FocusedIndex + Direction;
        if (NewIndex >= 0 && NewIndex < CurrentOrder.Num())
        {
            MoveItem(FocusedIndex, NewIndex);
        }
        return;
    }

    if (FocusedIndex == INDEX_NONE)
    {
        SetFocusedIndex(0);
        return;
    }
    SetFocusedIndex(FMath::Clamp(FocusedIndex + Direction, 0, CurrentOrder.Num() - 1));
}

void UVHVOrderingWidget::SetFocusedIndex(const int32 NewIndex)
{
    if (CurrentOrder.Num() == 0)
    {
        FocusedIndex = INDEX_NONE;
        FocusedItemID.Reset();
    }
    else
    {
        FocusedIndex = FMath::Clamp(NewIndex, 0, CurrentOrder.Num() - 1);
        FocusedItemID = CurrentOrder[FocusedIndex];
    }
    UpdateSelectionVisuals();
}

void UVHVOrderingWidget::SetKeyboardReorderMode(const bool bReorder)
{
    bKeyboardReorderMode = bReorder;
    UpdateSelectionVisuals();
    UpdateSubmitHint();
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
    if (!CardsSlateContainer)
    {
        return;
    }

    TArray<TObjectPtr<UVHVOrderingCardWidget>> OrderedCards;
    OrderedCards.Reserve(CurrentOrder.Num());
    for (const FString& ItemID : CurrentOrder)
    {
        for (UVHVOrderingCardWidget* CardWidget : CardWidgets)
        {
            if (CardWidget && CardWidget->GetItemID() == ItemID)
            {
                OrderedCards.Add(CardWidget);
                break;
            }
        }
    }

    CardWidgets = OrderedCards;
    CardsSlateContainer->ClearChildren();
    for (int32 Index = 0; Index < CardWidgets.Num(); ++Index)
    {
        UVHVOrderingCardWidget* CardWidget = CardWidgets[Index];
        if (!CardWidget)
        {
            continue;
        }

        CardWidget->SetCardIndex(Index);
        const float HalfGap = VHVActivityUIStyle::OrderingCardGap * 0.5f;
        CardsSlateContainer->AddSlot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(Index == 0 ? 0.0f : HalfGap, 0.0f,
            Index + 1 == CardWidgets.Num() ? 0.0f : HalfGap, 0.0f))
        [
            CardWidget->TakeWidget()
        ];
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
        }
    }
}

void UVHVOrderingWidget::UpdateSelectionVisuals()
{
    for (UVHVOrderingCardWidget* CardWidget : CardWidgets)
    {
        if (!CardWidget)
        {
            continue;
        }

        const bool bIsFocused = FocusedItemID == CardWidget->GetItemID();
        const bool bIsGrabbed = bIsFocused && (bKeyboardReorderMode
            || (bMouseDragging && MouseDragItemID == CardWidget->GetItemID()));
        CardWidget->SetVisualState(bIsFocused, bIsGrabbed);
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
    ClearMouseDropTarget();
    if (!bMouseDragging || MouseInsertionIndex == INDEX_NONE)
    {
        return;
    }

    const int32 SourceIndex = CurrentOrder.Find(MouseDragItemID);
    if (SourceIndex == INDEX_NONE)
    {
        return;
    }

    TArray<FString> ReducedOrder = CurrentOrder;
    ReducedOrder.RemoveAt(SourceIndex);
    if (ReducedOrder.Num() == 0)
    {
        return;
    }

    const int32 ReducedTargetIndex = MouseInsertionIndex == 0
        ? 0
        : FMath::Min(MouseInsertionIndex - 1, ReducedOrder.Num() - 1);
    MouseDropTargetIndex = CurrentOrder.Find(ReducedOrder[ReducedTargetIndex]);
    if (CardWidgets.IsValidIndex(MouseDropTargetIndex) && CardWidgets[MouseDropTargetIndex])
    {
        CardWidgets[MouseDropTargetIndex]->SetMouseDropTarget(true);
    }
}

void UVHVOrderingWidget::UpdateSubmitHint()
{
    FText Hint;
    if (CurrentOrder.Num() < 2)
    {
        Hint = FText::FromString(TEXT("Invalid ordering setup"));
    }
    else if (bKeyboardReorderMode || bMouseDragging)
    {
        Hint = FText::FromString(TEXT("Card picked up — move left or right, then drop."));
    }

    if (SubmitHint)
    {
        SubmitHint->SetText(Hint);
    }
    if (SubmitHintSlate)
    {
        SubmitHintSlate->SetText(Hint);
        SubmitHintSlate->SetVisibility(Hint.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible);
    }
}
