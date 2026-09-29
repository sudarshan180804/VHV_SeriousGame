#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Textbook/Types/VHVTextbookTypes.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHVSupportNetworkWidget.generated.h"

class SButton;
class STextBlock;
class UVHVUIManagerComponent;

UCLASS()
class VHV_API UVHVSupportNetworkWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVSupportNetworkWidget();

    void Configure(const FTextbookActivityData& Activity);
    void SetOwningUIManager(UVHVUIManagerComponent* InManager);
    TArray<FMatchingPair> GetCurrentMatches() const;
    void ActivateSelectedConnection() { ConnectSelected(); }

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    FReply HandleSupporterClicked(int32 Index);
    FReply HandleRoleClicked(int32 Index);
    void MoveSupporterFocus(int32 Delta);
    void MoveRoleFocus(int32 Delta);
    void ConnectSelected();
    void RemoveSelected();
    void SubmitOrClose();
    void FinishCorrectSubmission();
    void RefreshVisuals();
    FString SupporterText(int32 Index) const;

    UPROPERTY(Transient)
    TObjectPtr<UVHVUIManagerComponent> OwningUIManager;

    FTextbookActivityData ActivityData;
    TArray<int32> Assignments;
    TArray<bool> IncorrectAssignments;
    int32 SelectedSupporter = 0;
    int32 SelectedRole = 0;
    bool bReadOnly = false;
    bool bSubmissionLocked = false;
    FString Status;
    FTimerHandle SubmitTimer;

    TArray<TSharedPtr<STextBlock>> SupporterTexts;
    TArray<TSharedPtr<STextBlock>> RoleTexts;
    TSharedPtr<STextBlock> StatusText;
    FSlateBrush BackgroundBrush;
    FSlateBrush NodeBrush;
    FSlateBrush SelectedNodeBrush;
    FSlateBrush CenterBrush;
    FSlateBrush RoleBrush;
    FSlateBrush SelectedRoleBrush;
    FButtonStyle NodeButtonStyle;
    FButtonStyle RoleButtonStyle;
};
