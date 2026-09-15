#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVOrderingCardWidget.generated.h"

class UVHVOrderingWidget;

UCLASS()
class VHV_API UVHVOrderingCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetOrderingItem(const FOrderingItem& Item);
    FString GetItemID() const;
    FOrderingItem GetOrderingItem() const;
    void SetOwningOrderingWidget(UVHVOrderingWidget* InOrderingWidget);
    void SetSelected(bool bSelected);
    void SetGrabbed(bool bGrabbed);
    void SetVisualState(bool bIsFocused, bool bIsGrabbed);
    void SetDropHighlight(bool bActive);
    void SetMouseDropTarget(bool bActive);

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    void SetCardIndex(int32 InCardIndex);
    void UpdatePositionText();

protected:
    void ApplyCardColor();

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> CardText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> PositionText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> CardBorder;

    UPROPERTY()
    TObjectPtr<UVHVOrderingWidget> OwningOrderingWidget;

    FOrderingItem CurrentItem;
    FString BaseDisplayText;
    int32 CardIndex = INDEX_NONE;
    int32 CardColorIndex = INDEX_NONE;
    bool bMouseDropTarget = false;
};
