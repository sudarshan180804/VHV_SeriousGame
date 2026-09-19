// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVQuestionWidget.generated.h"

class UVHVUIManagerComponent;

UCLASS()
class VHV_API UVHVQuestionOptionButtonProxy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 OptionIndex = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<class UVHVQuestionWidget> OwnerWidget;

	UFUNCTION()
	void OnOptionClicked();
};

UCLASS()
class VHV_API UVHVQuestionWidget : public UVHVUserWidgetBase
{
	GENERATED_BODY()

public:
	UVHVQuestionWidget();

	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

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

	/** Applies the activity interaction action to the currently focused option. */
	bool ActivateFocusedOption();

	/** Uses the existing question text area for a non-destructive invalid-submit response. */
	void ShowSelectionRequiredFeedback();

	UFUNCTION()
	void SetOwningUIManager(UVHVUIManagerComponent* InUIManager);

	UFUNCTION()
	void HandleOptionSelected(int32 OptionIndex);

protected:

	void UpdateSelectionVisuals();
	void ClearOptionButtons();
	void ToggleSelectedOption(int32 OptionIndex);
	void ClearInputFeedback();

	FQuestionData CurrentQuestion;
	int32 SelectedOptionIndex = INDEX_NONE;
	bool bMultiChoiceEnabled = false;
	TSet<int32> SelectedOptionIndices;
	TArray<TObjectPtr<UButton>> OptionButtons;
	TArray<TObjectPtr<UVHVQuestionOptionButtonProxy>> OptionProxies;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> QuestionText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> OptionsContainer;

	UPROPERTY()
	TObjectPtr<UVHVUIManagerComponent> OwningUIManager;
};
