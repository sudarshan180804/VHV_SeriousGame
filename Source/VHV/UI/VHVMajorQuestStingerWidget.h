#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "UI/VHVMajorQuestStingerTypes.h"
#include "VHVMajorQuestStingerWidget.generated.h"

class SBorder;
class STextBlock;
class SVerticalBox;

DECLARE_MULTICAST_DELEGATE(FOnVHVMajorQuestStingerFinished);

/** Non-interactive full-screen title treatment for major quest starts and completions. */
UCLASS()
class VHV_API UVHVMajorQuestStingerWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVMajorQuestStingerWidget();

    void ShowStinger(const FVHVMajorQuestStingerData& InData);
    bool IsPlaying() const { return bPlaying; }

    FOnVHVMajorQuestStingerFinished OnStingerFinished;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void FinishStinger();

    FVHVMajorQuestStingerData StingerData;
    float ElapsedTime = 0.0f;
    bool bPlaying = false;

    TSharedPtr<SBorder> DimLayer;
    TSharedPtr<STextBlock> LabelText;
    TSharedPtr<SVerticalBox> TitleGroup;
    TSharedPtr<STextBlock> TitleText;
    TSharedPtr<STextBlock> SubtitleText;

    FSlateBrush DimBrush;
    FSlateBrush OrnamentBrush;
};
