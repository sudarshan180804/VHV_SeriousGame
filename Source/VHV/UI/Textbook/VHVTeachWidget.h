#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/VHVUserWidgetBase.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVTeachWidget.generated.h"

class SConstraintCanvas;
class STextBlock;
class SVerticalBox;
class UTexture2D;
class UVHVUIManagerComponent;

/** Full-screen teaching page presented on the authored village lesson board. */
UCLASS()
class VHV_API UVHVTeachWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVTeachWidget();

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetTeachingContent(const FTeachingContent& InTeaching);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetTeachingData(const FTeachingContent& InTeaching);

    UFUNCTION()
    void SetOwningUIManager(UVHVUIManagerComponent* InUIManager);

    bool BeginContentTransition(const FSimpleDelegate& OnFadeOutComplete);
    void CancelContentTransition();
    bool IsContentTransitionActive() const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // Retained for compatibility with the existing WBP_Teach widget tree. The
    // runtime presentation is built by RebuildWidget so configured instances
    // receive the new layout without replacing the assigned widget Blueprint.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> TitleText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> ContentText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> KeyTakeawaysContainer;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> MediaContainer;

    UPROPERTY()
    TObjectPtr<UVHVUIManagerComponent> OwningUIManager;

private:
    void PopulateTeachingContent();
    void RefreshSlateContent();
    FReply HandleContinueClicked();

    FTeachingContent TeachingContent;
    bool bHasTeachingContent = false;
    float EntranceElapsed = 0.0f;

    enum class EContentTransitionPhase : uint8
    {
        None,
        FadingOut,
        FadingIn
    };

    EContentTransitionPhase ContentTransitionPhase = EContentTransitionPhase::None;
    float ContentTransitionElapsed = 0.0f;
    FSimpleDelegate PendingContentSwap;

    TSharedPtr<SConstraintCanvas> ActivityContentSlate;
    TSharedPtr<STextBlock> CategoryTextSlate;
    TSharedPtr<STextBlock> TitleTextSlate;
    TSharedPtr<STextBlock> ContentTextSlate;
    TSharedPtr<SVerticalBox> ReadingAreaSlate;
    TSharedPtr<SVerticalBox> TakeawaysSectionSlate;
    TArray<TSharedPtr<SWidget>> TakeawayCardsSlate;

    FSlateBrush BackgroundBrush;
    FSlateBrush HeaderDividerBrush;
    FSlateBrush TakeawayGreenBrush;
    FSlateBrush TakeawayGoldBrush;
    FSlateBrush TakeawayTealBrush;
    FSlateBrush TakeawayShadowBrush;
    FSlateBrush MediaSurfaceBrush;
    FSlateBrush LargeMediaBrush;
    FSlateBrush KeycapBrush;
    FButtonStyle ContinueButtonStyle;
    TArray<FSlateBrush> MediaBrushes;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> LessonBackgroundTexture;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> LoadedMediaTextures;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> LoadedLargeMediaTexture;
};
