// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "Components/TextBlock.h"
#include "VHVFeedbackWidget.generated.h"

UCLASS()
class VHV_API UVHVFeedbackWidget : public UVHVUserWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
	void SetFeedbackText(const FText& InText);

protected:
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
	TObjectPtr<UTextBlock> FeedbackText;
};
