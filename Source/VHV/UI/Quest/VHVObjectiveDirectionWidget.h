#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHVObjectiveDirectionWidget.generated.h"

class STextBlock;
class SWidget;
class SBox;

/** Non-interactive HUD arrow that rotates toward one cached world position. */
UCLASS(NotBlueprintable)
class VHV_API UVHVObjectiveDirectionWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVObjectiveDirectionWidget();

    void ShowDirectionTo(const FVector& InTargetLocation);
    void HideDirection();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    FVector TargetLocation = FVector::ZeroVector;
    bool bHasTarget = false;
    TSharedPtr<SBox> ArrowVisual;
    TSharedPtr<STextBlock> DistanceText;
};
