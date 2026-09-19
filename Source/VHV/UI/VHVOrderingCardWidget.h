#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVOrderingCardWidget.generated.h"

class SBorder;
class SBox;
class SImage;
class STextBlock;
class UTexture2D;
class UVHVOrderingWidget;

UCLASS()
class VHV_API UVHVOrderingCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UVHVOrderingCardWidget(const FObjectInitializer& ObjectInitializer);

    void SetOrderingItem(const FOrderingItem& Item);
    FString GetItemID() const;
    FOrderingItem GetOrderingItem() const;
    void SetOwningOrderingWidget(UVHVOrderingWidget* InOrderingWidget);
    void SetSelected(bool bSelected);
    void SetGrabbed(bool bGrabbed);
    void SetVisualState(bool bIsFocused, bool bIsGrabbed);
    void SetDropHighlight(bool bActive);
    void SetMouseDropTarget(bool bActive);
    void SetMediaTexture(UTexture2D* Texture);
    void SetDescription(const FText& Description);

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    void SetCardIndex(int32 InCardIndex);
    void UpdatePositionText();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    void RefreshVisualState();

    // Legacy WBP bindings remain optional for asset load compatibility.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> CardText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PositionText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> CardBorder;

    UPROPERTY()
    TObjectPtr<UVHVOrderingWidget> OwningOrderingWidget;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> MediaTexture;

    FOrderingItem CurrentItem;
    FString BaseDisplayText;
    int32 CardIndex = INDEX_NONE;
    bool bFocused = false;
    bool bGrabbed = false;
    bool bMouseDropTarget = false;
    float CurrentLift = 0.0f;
    float CurrentScale = 1.0f;

    TSharedPtr<SBorder> CardBorderSlate;
    TSharedPtr<SBox> MediaAreaSlate;
    TSharedPtr<SImage> MediaImageSlate;
    TSharedPtr<STextBlock> CardTextSlate;
    TSharedPtr<STextBlock> DescriptionTextSlate;
    TSharedPtr<STextBlock> PositionTextSlate;
    FSlateBrush CardBrush;
    FSlateBrush ShadowBrush;
    FSlateBrush BadgeBrush;
    FSlateBrush MediaBrush;
    FSlateBrush MediaSurfaceBrush;
};
