#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VHVMatchingCardWidget.generated.h"

class UBorder;
class UTextBlock;
class UVHVMatchingWidget;

UCLASS()
class VHV_API UVHVMatchingCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetOwningMatchingWidget(UVHVMatchingWidget* InMatchingWidget);
    void SetCardData(bool bInLeftColumn, int32 InItemIndex, const FString& InDisplayText);
    void SetVisualState(bool bIsFocused, bool bIsSelected, bool bIsMapped, bool bIsHovered, bool bIsDragging);
    FGeometry GetVisibleContentGeometry() const;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeConstruct() override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

protected:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> CardText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> CardBorder;

    UPROPERTY()
    TObjectPtr<UVHVMatchingWidget> OwningMatchingWidget;

    bool bIsLeftColumn = true;
    int32 ItemIndex = INDEX_NONE;
    FString BaseDisplayText;
    bool bPointerHovered = false;
    bool bLastFocused = false;
    bool bLastSelected = false;
    bool bLastMapped = false;
    bool bLastDragTarget = false;
    bool bLastDragging = false;
};
