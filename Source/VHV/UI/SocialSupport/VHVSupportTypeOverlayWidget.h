#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHVSupportTypeOverlayWidget.generated.h"

class STextBlock;

UCLASS()
class VHV_API UVHVSupportTypeOverlayWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVSupportTypeOverlayWidget();

    void ShowTypeReveal();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    void RefreshText();

    TSharedPtr<STextBlock> HeaderText;
    TSharedPtr<STextBlock> FeedbackText;
    FSlateBrush PanelBrush;
};
