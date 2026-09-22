#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVObservationWidget.generated.h"

class SConstraintCanvas;
class SButton;
class STextBlock;
class SVerticalBox;
class SVHVEvidenceTagCard;
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

    /** Semantic E action routed by the UI manager while evidence tagging owns focus. */
    bool ActivateFocusedEvidenceCard();

    /** Enter/button action for passive review or the active evidence-tagging stage. */
    bool AdvanceObservation();

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

    UFUNCTION()
    void SubmitObservation();

private:
    void PopulateObservation();
    void RefreshSlateContent();
    void RefreshPassiveContent();
    void RefreshEvidenceTaggingContent();
    void RebuildEvidenceCards();
    void UpdateEvidenceCardStates();
    void MoveEvidenceFocus(int32 ColumnDelta, int32 RowDelta);
    void ToggleEvidenceCard(int32 CardIndex);
    void HandleEvidenceCardChosen(int32 CardIndex);
    bool CanAdvanceObservation() const;
    const TArray<FObservationEvidenceCard>& GetCurrentEvidenceCards() const;
    const TSet<int32>& GetCurrentEvidenceSelection() const;
    TSet<int32>& GetCurrentEvidenceSelection();
    FReply HandleContinueClicked();

    enum class EEvidenceTaggingStage : uint8
    {
        Stage1,
        Stage2,
        Reveal,
        Result
    };

    FTextbookActivityData CurrentActivity;
    bool bHasObservationData = false;
    bool bCompletionRequested = false;
    float EntranceElapsed = 0.0f;
    float RevealElapsed = 0.0f;
    int32 RevealPage = 0;
    int32 FocusedEvidenceCard = INDEX_NONE;
    int32 EvidenceColumnCount = 3;
    EEvidenceTaggingStage EvidenceStage = EEvidenceTaggingStage::Stage1;
    TSet<int32> Stage1Selections;
    TSet<int32> Stage2Selections;

    TSharedPtr<SConstraintCanvas> ActivityContentSlate;
    TSharedPtr<STextBlock> PrimaryTitleSlate;
    TSharedPtr<SVerticalBox> SecondaryTextSlate;
    TSharedPtr<SVerticalBox> ObservationItemsSlate;
    TSharedPtr<STextBlock> BoardInstructionSlate;
    TSharedPtr<STextBlock> ActionButtonTextSlate;
    TSharedPtr<SButton> ActionButtonSlate;
    TArray<TSharedPtr<SVHVEvidenceTagCard>> EvidenceCardsSlate;
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
