#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVObservationWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UVHVUIManagerComponent;

/**
 * Presents an observation prompt and records that the learner has reviewed it.
 * The widget builds a small native layout when no Blueprint presentation is supplied.
 */
UCLASS()
class VHV_API UVHVObservationWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVObservationWidget();

    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetObservationData(const FTextbookActivityData& InActivity);

    UFUNCTION()
    void SetOwningUIManager(UVHVUIManagerComponent* InUIManager);

protected:
    UFUNCTION()
    void SubmitObservation();

private:
    void BuildNativeLayout();
    void PopulateObservation();

    FTextbookActivityData CurrentActivity;
    bool bHasObservationData = false;

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
