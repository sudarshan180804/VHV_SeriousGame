#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/VHVUserWidgetBase.h"
#include "Components/CanvasPanel.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVOrderingWidget.generated.h"

class SHorizontalBox;
class STextBlock;
class UTexture2D;
class UVHVOrderingCardWidget;
class UVHVUIManagerComponent;

UCLASS()
class VHV_API UVHVOrderingWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVOrderingWidget();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetOrderingItems(const TArray<FOrderingItem>& Items);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetOwningUIManager(UVHVUIManagerComponent* InUIManager);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    TArray<FString> GetCurrentOrder() const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void ResetOrder();

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    void BeginMouseDrag(const FString& ItemID);
    void UpdateMouseDrag(const FDragDropEvent& InDragDropEvent);
    void CommitMouseReorder();
    void EndMouseDrag();
    bool MoveItem(int32 FromIndex, int32 ToIndex);
    void MoveFocusedCard(int32 Direction);
    void SetFocusedIndex(int32 NewIndex);
    void SetKeyboardReorderMode(bool bReorder);
    bool ToggleFocusedCardGrab();
    void ClearDragState();
    void SynchronizeCardWidgets();
    void UpdatePositionLabels();
    void UpdateSelectionVisuals();
    void UpdateCardPresentationStates();
    void ClearMouseDropTarget();

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHV|Ordering")
    TSubclassOf<UVHVOrderingCardWidget> CardWidgetClass;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

    void RefreshDisplay();
    void ApplyRandomizedSourceOrder();
    void ShuffleItems(TArray<FOrderingItem>& Items);
    FOrderingItem ResolveItemByID(const FString& ItemID) const;
    int32 CalculateMouseInsertionSlot(const FVector2D& CursorLocalToContainer) const;
    void UpdateMouseDropTarget();
    void UpdateSubmitHint();
    FReply HandleConfirmClicked();

    TArray<FOrderingItem> SourceItems;
    TArray<FOrderingItem> CurrentOrderingItems;
    TArray<FString> CurrentOrder;
    TArray<TObjectPtr<UVHVOrderingCardWidget>> CardWidgets;

    int32 FocusedIndex = INDEX_NONE;
    int32 KeyboardReorderIndex = INDEX_NONE;
    int32 MouseDragSourceIndex = INDEX_NONE;
    int32 MouseInsertionIndex = INDEX_NONE;
    bool bKeyboardReorderMode = false;
    bool bMouseDragging = false;
    int32 MouseDropTargetIndex = INDEX_NONE;
    FString FocusedItemID;
    FString MouseDragItemID;

    TSharedPtr<SHorizontalBox> CardsSlateContainer;
    TSharedPtr<STextBlock> SubmitHintSlate;
    FSlateBrush BackgroundBrush;
    FSlateBrush BackgroundShadeBrush;
    FSlateBrush HeaderDividerBrush;
    FSlateBrush KeycapBrush;
    FButtonStyle ConfirmButtonStyle;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> OrderingBackgroundTexture;

    // Legacy WBP bindings are retained for load compatibility. The approved
    // Ordering presentation is built by the native Slate tree.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UCanvasPanel> OrderingRoot;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> CardsContainer;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SubmitHint;

    UPROPERTY()
    TObjectPtr<UVHVUIManagerComponent> OwningUIManager;
};
