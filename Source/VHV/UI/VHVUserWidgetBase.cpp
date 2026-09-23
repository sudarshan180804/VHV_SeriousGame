#include "UI/VHVUserWidgetBase.h"

#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "UI/VHVUIManagerComponent.h"

void UVHVUserWidgetBase::Show()
{
    if (!IsInViewport())
    {
        AddToViewport();
    }

    SetVisibility(ESlateVisibility::Visible);
}

void UVHVUserWidgetBase::Hide()
{
    if (IsInViewport())
    {
        RemoveFromParent();
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

bool UVHVUserWidgetBase::IsVisibleInViewport() const
{
    return IsInViewport() && GetVisibility() == ESlateVisibility::Visible;
}

FReply UVHVUserWidgetBase::NativeOnMouseButtonDown(
    const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (APlayerController* PlayerController = GetOwningPlayer())
        {
            if (UVHVUIManagerComponent* UIManager =
                PlayerController->FindComponentByClass<UVHVUIManagerComponent>())
            {
                if (UIManager->HandleModalBackgroundClick())
                {
                    return FReply::Handled();
                }
            }
        }
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}
