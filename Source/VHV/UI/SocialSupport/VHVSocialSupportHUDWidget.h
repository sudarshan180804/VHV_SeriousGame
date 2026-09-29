#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHVSocialSupportHUDWidget.generated.h"

class STextBlock;

UCLASS()
class VHV_API UVHVSocialSupportHUDWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVSocialSupportHUDWidget();
    void ShowObservationProgress(int32 CompletedCount);
    void ShowSupporterProgress(int32 CompletedCount);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    void Refresh();
    FString Title;
    FString Progress;
    TSharedPtr<STextBlock> TitleText;
    TSharedPtr<STextBlock> ProgressText;
    FSlateBrush PanelBrush;
};
