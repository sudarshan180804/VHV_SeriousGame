// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "VHV/Textbook/Types/VHVTextbookTypes.h"
#include "VHVTeachWidget.generated.h"

class UVHVUIManagerComponent;

UCLASS()
class VHV_API UVHVTeachWidget : public UVHVUserWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
	void SetTeachingContent(const FTeachingContent& InTeaching);

	UFUNCTION(BlueprintCallable, Category = "VHV|Textbook")
	void SetTeachingData(const FTeachingContent& InTeaching);

	UFUNCTION()
	void SetOwningUIManager(UVHVUIManagerComponent* InUIManager);

protected:
    virtual void NativeConstruct() override;

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

    // Keep the runtime payload until the UMG widget tree is constructed and its
    // BindWidget members are available.
    FTeachingContent TeachingContent;
    bool bHasTeachingContent = false;
};
