#pragma once

#include "Ambient/VHVAmbientSpeechTypes.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "VHVAmbientSpeechWidget.generated.h"

class STextBlock;

UCLASS()
class VHV_API UVHVAmbientSpeechWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UVHVAmbientSpeechWidget(const FObjectInitializer& ObjectInitializer);

    void ShowBubble(const FText& SpeakerName, const FText& Text, EVHVAmbientSpeechType SpeechType, bool bShowSpeakerName);
    void HideBubble(bool bImmediate = false);
    void SetDistanceOpacity(float Opacity);
    void SetSeparationOffset(float OffsetY);
    void TickPresentation(float DeltaTime);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    TSharedPtr<SWidget> AnimatedRoot;
    TSharedPtr<STextBlock> SpeakerText;
    TSharedPtr<STextBlock> BodyText;
    TSharedPtr<STextBlock> TailText;
    TSharedPtr<SWidget> SpeakerRow;

    FSlateBrush ShadowBrush;
    FSlateBrush PanelBrush;
    EVHVAmbientSpeechType CurrentType = EVHVAmbientSpeechType::Speech;
    float AnimationElapsed = 0.0f;
    float DistanceOpacity = 1.0f;
    float SeparationOffsetY = 0.0f;
    float LastAnimationOpacity = 1.0f;
    float LastAnimationOffsetY = 0.0f;
    float LastAnimationScale = 1.0f;
    bool bAppearing = false;
    bool bDisappearing = false;
    bool bBubbleVisible = false;

    void RefreshBrushes();
    void ApplyAnimatedState(float Opacity, float OffsetY, float Scale);
};
