#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVObservationWidget.generated.h"

class SConstraintCanvas;
class STextBlock;
class SVerticalBox;
class UButton;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UVHVUIManagerComponent;

/** Calm, full-screen review presentation for Observation activities. */
UCLASS()
class VHV_API UVHVObservationWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVObservationWidget();

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetObservationData(const FTextbookActivityData& InActivity);

    UFUNCTION()
    void SetOwningUIManager(UVHVUIManagerComponent* InUIManager);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

    UFUNCTION()
    void SubmitObservation();

private:
    void PopulateObservation();
    void RefreshSlateContent();
    FReply HandleContinueClicked();

    FTextbookActivityData CurrentActivity;
    bool bHasObservationData = false;
    float EntranceElapsed = 0.0f;

    TSharedPtr<SConstraintCanvas> ActivityContentSlate;
    TSharedPtr<STextBlock> PrimaryTitleSlate;
    TSharedPtr<SVerticalBox> SecondaryTextSlate;
    TSharedPtr<SVerticalBox> ObservationItemsSlate;
    TArray<TSharedPtr<SWidget>> ObservationCardsSlate;

    FSlateBrush BackgroundBrush;
    FSlateBrush HeaderDividerBrush;
    FSlateBrush CardSurfaceBrush;
    FSlateBrush CardAccentBrush;
    FSlateBrush CardShadowBrush;
    FSlateBrush KeycapBrush;
    FButtonStyle ContinueButtonStyle;
    TArray<FSlateBrush> MediaBrushes;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> ObservationBackgroundTexture;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> LoadedMediaTextures;

    // Legacy WBP bindings are retained so the configured WBP_Observation
    // remains compatible. RebuildWidget supplies its runtime presentation.
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> ContentRoot;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> ActivityTitle;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> NarrativeContext;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> PromptText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> MediaContainer;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ContinueButton;

    UPROPERTY()
    TObjectPtr<UVHVUIManagerComponent> OwningUIManager;
};
