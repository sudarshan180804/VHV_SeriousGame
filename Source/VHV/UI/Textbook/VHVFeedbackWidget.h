// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/VHVUserWidgetBase.h"
#include "Components/TextBlock.h"
#include "VHVFeedbackWidget.generated.h"

class UVHVUIManagerComponent;

UCLASS()
class VHV_API UVHVFeedbackWidget : public UVHVUserWidgetBase
{
	GENERATED_BODY()

public:
	enum class ETone : uint8
	{
		Neutral,
		Positive,
		Caution
	};

	UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
	void SetFeedbackText(const FText& InText);

	void SetFeedbackPresentation(const FText& InText, ETone InTone, bool bIsHint = false);
	void SetOwningUIManager(UVHVUIManagerComponent* InUIManager);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> FeedbackText;

private:
	void RefreshPresentation();
	FReply HandleContinueClicked();

	FText CurrentText;
	ETone CurrentTone = ETone::Neutral;
	bool bCurrentIsHint = false;
	bool bContinueActivated = false;
	float EntranceElapsed = 0.0f;
	TSharedPtr<class STextBlock> TitleTextSlate;
	TSharedPtr<class STextBlock> FeedbackTextSlate;
	TSharedPtr<SWidget> FeedbackPanel;
	FSlateBrush PanelBrush;
	FSlateBrush ShadowBrush;
	FButtonStyle ContinueButtonStyle;

	UPROPERTY()
	TObjectPtr<UVHVUIManagerComponent> OwningUIManager;
};
