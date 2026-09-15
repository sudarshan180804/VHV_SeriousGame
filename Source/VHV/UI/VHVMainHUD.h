#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "VHVMainHUD.generated.h"

UCLASS()
class VHV_API UVHVMainHUD : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void SetInteractionPrompt(const FText& InPrompt);

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void SetInteractionPromptVisible(bool bVisible);

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UPanelWidget> DialogueLayer;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UPanelWidget> TextbookLayer;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> TopLayer;

protected:
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> PromptText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<USizeBox> InteractionPromptContainer;
};
