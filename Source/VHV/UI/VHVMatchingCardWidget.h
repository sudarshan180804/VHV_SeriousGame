#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "VHVMatchingCardWidget.generated.h"

class UBorder;
class UTextBlock;
class SBorder;
class SBox;
class SImage;
class STextBlock;
class UTexture2D;
class UVHVMatchingWidget;

UCLASS()
class VHV_API UVHVMatchingCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UVHVMatchingCardWidget(const FObjectInitializer& ObjectInitializer);

    void SetOwningMatchingWidget(UVHVMatchingWidget* InMatchingWidget);
    void SetCardData(bool bInLeftColumn, int32 InItemIndex, const FString& InDisplayText);
    void SetVisualState(bool bIsFocused, bool bIsSelected, bool bIsMapped, bool bIsHovered, bool bIsDragging);
    void SetEntranceDelay(float InDelay);
    void SetMediaTexture(UTexture2D* InTexture);
    void SetDescription(const FText& InDescription);
    FGeometry GetConnectorGeometry() const;

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
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    void RefreshVisualState();

    // Legacy WBP bindings remain optional for asset load compatibility. The
    // professional Matching card is composed by the native Slate tree.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> CardText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UBorder> CardBorder;

    UPROPERTY()
    TObjectPtr<UVHVMatchingWidget> OwningMatchingWidget;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> MediaTexture;

    bool bIsLeftColumn = true;
    int32 ItemIndex = INDEX_NONE;
    FString BaseDisplayText;
    bool bPointerHovered = false;
    bool bLastFocused = false;
    bool bLastSelected = false;
    bool bLastMapped = false;
    bool bLastDragTarget = false;
    bool bLastDragging = false;
    float EntranceDelay = 0.0f;
    float EntranceElapsed = 0.0f;
    float CurrentLift = 0.0f;
    FText Description;

    TSharedPtr<SBorder> CardBorderSlate;
    TSharedPtr<SBorder> ConnectorSlate;
    TSharedPtr<SBorder> ConnectorCoreSlate;
    TSharedPtr<SBox> MediaAreaSlate;
    TSharedPtr<SImage> MediaImageSlate;
    TSharedPtr<STextBlock> CardTextSlate;
    TSharedPtr<STextBlock> DescriptionTextSlate;
    FSlateBrush CardBrush;
    FSlateBrush ShadowBrush;
    FSlateBrush ConnectorBrush;
    FSlateBrush ConnectorCoreBrush;
    FSlateBrush MediaSurfaceBrush;
    FSlateBrush MediaBrush;
};
