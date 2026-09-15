#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VHVUserWidgetBase.generated.h"

UCLASS(Abstract)
class VHV_API UVHVUserWidgetBase : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    virtual void Show();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    virtual void Hide();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    virtual bool IsVisibleInViewport() const;
};
