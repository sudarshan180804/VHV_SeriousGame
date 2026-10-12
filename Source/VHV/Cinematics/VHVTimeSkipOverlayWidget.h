#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VHVTimeSkipOverlayWidget.generated.h"

class STextBlock;

DECLARE_MULTICAST_DELEGATE(FOnVHVTimeSkipOverlayEvent);

/** Lightweight full-screen presentation used by data-driven time-skip sequences. */
UCLASS(Blueprintable)
class VHV_API UVHVTimeSkipOverlayWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void PlayTransition(const FText& InText, float InFadeInDuration, float InHoldDuration, float InFadeOutDuration);
    void CancelTransition();
    bool IsTransitionPlaying() const { return bPlaying; }

    FOnVHVTimeSkipOverlayEvent OnFullyObscured;
    FOnVHVTimeSkipOverlayEvent OnTransitionFinished;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    TSharedPtr<STextBlock> TransitionTextBlock;
    float Elapsed = 0.0f;
    float FadeInDuration = 0.25f;
    float HoldDuration = 0.60f;
    float FadeOutDuration = 0.28f;
    bool bPlaying = false;
    bool bObscuredEventSent = false;
};
