#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVMatchingWidget.generated.h"

class UTextBlock;
class UButton;
class UVerticalBox;
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

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetOwningUIManager(UVHVUIManagerComponent* InUIManager);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    TArray<FMatchingPair> GetCurrentMatches() const;

    void HandleCardSelected(bool bIsLeftColumn, int32 ItemIndex);
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
    void RefreshDisplay();
    void ShuffleRightItems();
    void MoveFocus(int32 Direction);
    void SetFocusedColumn(bool bInLeftColumn);
    void AssignFocusedRightToFocusedLeft();
    void RemoveFocusedLeftMatch();
    void SubmitCurrentMatches();
    void UpdateCardVisuals();
    void UpdateSubmitHint();
    FVector2D GetCardConnectionPoint(const UVHVMatchingCardWidget* CardWidget, bool bLeftSide, const FGeometry& AllottedGeometry) const;

    UFUNCTION()
    void HandleSubmitButtonClicked();

    TArray<FMatchingPair> MatchingPairs;
    TArray<int32> RightDisplayOrder;
    TArray<int32> LeftToRight;
    TArray<TObjectPtr<UVHVMatchingCardWidget>> LeftCardWidgets;
    TArray<TObjectPtr<UVHVMatchingCardWidget>> RightCardWidgets;
    int32 FocusedLeftIndex = INDEX_NONE;
    int32 FocusedRightDisplayIndex = INDEX_NONE;
    int32 DraggedLeftIndex = INDEX_NONE;
    int32 HoveredRightDisplayIndex = INDEX_NONE;
    FVector2D DragScreenPosition = FVector2D::ZeroVector;
    bool bLeftColumnFocused = true;
    bool bConnectionDragActive = false;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UVerticalBox> LeftItemsContainer;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UVerticalBox> RightItemsContainer;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> SubmitHint;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UButton> SubmitButton;

    UPROPERTY()
    TObjectPtr<UVHVUIManagerComponent> OwningUIManager;
};
