#include "UI/VHVUserWidgetBase.h"

#include "Components/Widget.h"

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
