#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVMatchingWidget.generated.h"

class UTextBlock;
class UButton;
class UVerticalBox;
class SConstraintCanvas;
class STextBlock;
class SVerticalBox;
class UTexture2D;
class UVHVMatchingCardWidget;
class UVHVUIManagerComponent;

UCLASS()
class VHV_API UVHVMatchingWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVMatchingWidget();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetMatchingPairs(const TArray<FMatchingPair>& InPairs);

    /** Authored activity prompt shown on the paper board. */
    void SetPromptText(const FString& InPromptText);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetOwningUIManager(UVHVUIManagerComponent* InUIManager);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    TArray<FMatchingPair> GetCurrentMatches() const;

    void HandleCardSelected(bool bIsLeftColumn, int32 ItemIndex);
    bool ActivateFocusedSelection();
    void BeginConnectionDrag(int32 LeftIndex, const FVector2D& ScreenPosition);
    void UpdateConnectionDrag(const FVector2D& ScreenPosition, int32 HoveredRightIndex = INDEX_NONE);
    void CommitConnectionDrag(int32 RightDisplayIndex);
    void EndConnectionDrag();
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual void NativeConstruct() override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHV|Matching")
    TSubclassOf<UVHVMatchingCardWidget> CardWidgetClass;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    void RefreshDisplay();
    void ShuffleRightItems();
    void MoveFocus(int32 Direction);
    void SetFocusedColumn(bool bInLeftColumn);
    void AssignFocusedRightToFocusedLeft();
    void RemoveFocusedLeftMatch();
    void SubmitCurrentMatches();
    void UpdateCardVisuals();
    void UpdateSubmitHint();
    FVector2D GetConnectorCenterInPaintSpace(const UVHVMatchingCardWidget* CardWidget, const FVector2D& WindowToDesktop) const;

    UFUNCTION()
    void HandleSubmitButtonClicked();

    FReply HandleConfirmClicked();

    TArray<FMatchingPair> MatchingPairs;
    TArray<int32> RightDisplayOrder;
    TArray<int32> LeftToRight;
    TArray<TObjectPtr<UVHVMatchingCardWidget>> LeftCardWidgets;
    TArray<TObjectPtr<UVHVMatchingCardWidget>> RightCardWidgets;
    int32 FocusedLeftIndex = INDEX_NONE;
    int32 FocusedRightDisplayIndex = INDEX_NONE;
    int32 DraggedLeftIndex = INDEX_NONE;
    int32 HoveredRightDisplayIndex = INDEX_NONE;
    int32 PendingLeftIndex = INDEX_NONE;
    FVector2D DragScreenPosition = FVector2D::ZeroVector;
    bool bLeftColumnFocused = true;
    bool bConnectionDragActive = false;
    FString AuthoredPromptText;
    float EntranceElapsed = 0.0f;
    float ConnectionRevealElapsed = 1.0f;

    TSharedPtr<SConstraintCanvas> ActivityContentSlate;
    TSharedPtr<SVerticalBox> LeftItemsSlateContainer;
    TSharedPtr<SVerticalBox> RightItemsSlateContainer;
    TSharedPtr<STextBlock> PromptTextSlate;
    TSharedPtr<STextBlock> SubmitHintSlate;
    FSlateBrush BackgroundBrush;
    FSlateBrush HeaderDividerBrush;
    FSlateBrush KeycapBrush;
    FButtonStyle ConfirmButtonStyle;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> MatchingBackgroundTexture;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> LeftItemsContainer;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> RightItemsContainer;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SubmitHint;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UButton> SubmitButton;

    UPROPERTY()
    TObjectPtr<UVHVUIManagerComponent> OwningUIManager;
};
