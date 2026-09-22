#pragma once

#include "Ambient/VHVAmbientSpeechTypes.h"
#include "Components/WidgetComponent.h"
#include "UI/Ambient/VHVAmbientSpeechStyle.h"
#include "VHVAmbientSpeechComponent.generated.h"

class UVHVAmbientSpeechWidget;

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVAmbientSpeechComponent : public UWidgetComponent
{
    GENERATED_BODY()

public:
    UVHVAmbientSpeechComponent();

    UFUNCTION(BlueprintCallable, Category = "VHV|Ambient Speech")
    bool ShowBubble(FText SpeakerName, FText Text, EVHVAmbientSpeechType SpeechType, bool bShowSpeakerName);

    UFUNCTION(BlueprintCallable, Category = "VHV|Ambient Speech")
    void HideBubble(bool bImmediate = false);

    UFUNCTION(BlueprintPure, Category = "VHV|Ambient Speech")
    bool IsBubbleActive() const { return bBubbleActive; }

    void SetSeparationOffset(float OffsetY);
    bool GetAnchorScreenPosition(FVector2D& OutScreenPosition) const;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Speech|Anchor", meta = (Units = "Centimeters"))
    float BubbleHeightOffset = VHVAmbientSpeechStyle::DefaultFallbackAnchorHeight;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Speech|Anchor", meta = (Units = "Centimeters"))
    float BubbleHorizontalOffset = 0.0f;

    /** Prefer a head bone/socket when the NPC mesh supplies one. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Speech|Anchor")
    bool bPreferHeadSocket = true;

    /** Optional explicit skeletal-mesh socket. When unset, common head names are resolved automatically. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Speech|Anchor")
    FName AnchorSocketName;

    /** Small lift above the resolved head socket so the tail sits just clear of the head. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Speech|Anchor", meta = (Units = "Centimeters"))
    float HeadSocketVerticalOffset = VHVAmbientSpeechStyle::DefaultHeadSocketLift;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Speech|Visibility", meta = (ClampMin = "0.0", Units = "Centimeters"))
    float MinVisibleDistance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Speech|Visibility", meta = (ClampMin = "0.0", Units = "Centimeters"))
    float FadeStartDistance = 1400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Speech|Visibility", meta = (ClampMin = "0.0", Units = "Centimeters"))
    float MaxVisibleDistance = 2300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Speech|Visibility")
    bool bHideSignificantlyOffscreen = true;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UVHVAmbientSpeechWidget> SpeechWidget;

    FTimerHandle HideTimerHandle;
    bool bBubbleActive = false;

    bool EnsureSpeechWidget();
    void ApplyAnchor();
    FName ResolveHeadSocket(const USkeletalMeshComponent* Mesh) const;
    void FinishHide();
    float CalculateDistanceOpacity() const;
};
