// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVQuestionWidget.generated.h"

class SConstraintCanvas;
class STextBlock;
class SVerticalBox;
class SVHVChoiceCard;
class UTextBlock;
class UVerticalBox;
class UVHVUIManagerComponent;

UCLASS()
class VHV_API UVHVQuestionWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVQuestionWidget();

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual void PrepareForModalFocus() override;

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetQuestionData(const FQuestionData& InQuestion);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetMultiChoiceEnabled(bool bInMultiChoiceEnabled);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    int32 GetSelectedOptionIndex() const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    void SetSelectedOptionIndex(int32 Index);

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    bool HasSelection() const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
    TArray<int32> GetSelectedOptionIndices() const;

    /** Applies the existing IA_Interact action to the currently focused option. */
    bool ActivateFocusedOption();

    /** Uses the instruction line for a non-destructive invalid-submit response. */
    void ShowSelectionRequiredFeedback();

    UFUNCTION()
    void SetOwningUIManager(UVHVUIManagerComponent* InUIManager);

    UFUNCTION()
    void HandleOptionSelected(int32 OptionIndex);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void RebuildChoiceCards();
    void UpdatePresentation();
    void SetFocusedOptionIndex(int32 Index);
    void ToggleSelectedOption(int32 OptionIndex);
    void ClearInputFeedback();
    void HandleChoiceCardChosen(int32 OptionIndex);
    FReply HandleSubmitClicked();
    FText GetDefaultInstruction() const;

    FQuestionData CurrentQuestion;
    int32 FocusedOptionIndex = INDEX_NONE;
    int32 SelectedOptionIndex = INDEX_NONE;
    bool bMultiChoiceEnabled = false;
    bool bShowingSelectionFeedback = false;
    float EntranceElapsed = 0.0f;
    TSet<int32> SelectedOptionIndices;
    TArray<TSharedPtr<SVHVChoiceCard>> ChoiceCards;

    TSharedPtr<SConstraintCanvas> RootCanvas;
    TSharedPtr<SVerticalBox> OptionsSlateContainer;
    TSharedPtr<STextBlock> ActivityTypeText;
    TSharedPtr<STextBlock> QuestionTextSlate;
    TSharedPtr<STextBlock> InstructionTextSlate;
    TSharedPtr<STextBlock> InteractionLegendText;
    TSharedPtr<SWidget> QuestionPanel;
    TSharedPtr<SWidget> AnswerPanel;
    FSlateBrush QuestionPanelBrush;
    FSlateBrush QuestionShadowBrush;
    FSlateBrush KeycapBrush;
    FSlateBrush ActivityEmblemBrush;
    FSlateBrush DividerBrush;
    FButtonStyle SubmitButtonStyle;

    // Kept so the existing WBP bindings remain load-compatible. The native
    // Slate layout is the runtime presentation for this widget.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget, AllowPrivateAccess = "true"))
    TObjectPtr<UTextBlock> QuestionText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UVerticalBox> OptionsContainer;

    UPROPERTY()
    TObjectPtr<UVHVUIManagerComponent> OwningUIManager;
};
