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

    /**
     * Gives a modal widget a chance to establish its logical navigation target
     * before the UI manager assigns Slate keyboard focus to the widget root.
     */
    virtual void PrepareForModalFocus() {}

protected:
    /** Routes unhandled left clicks to the UI manager's semantic modal policy. */
    virtual FReply NativeOnMouseButtonDown(
        const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
};
