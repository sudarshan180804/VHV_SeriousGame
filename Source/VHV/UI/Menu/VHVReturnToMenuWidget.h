#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHVReturnToMenuWidget.generated.h"

class SBorder;
class STextBlock;

/** Compact, modal Esc overlay owned by the gameplay player controller. */
UCLASS()
class VHV_API UVHVReturnToMenuWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVReturnToMenuWidget();
    void ShowSaveFailure();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    void SetSelection(int32 Index);
    void ActivateSelection();
    FReply HandleClicked(int32 Index);
    void HandleHovered(int32 Index);

    int32 SelectedIndex = 0;
    TArray<TSharedPtr<STextBlock>> Labels;
    TSharedPtr<STextBlock> StatusText;
    FSlateBrush PanelBrush;
};
