#include "UI/NPC/VHVNPCNameplateWidget.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

UVHVNPCNameplateWidget::UVHVNPCNameplateWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

void UVHVNPCNameplateWidget::SetDisplayName(const FText& InDisplayName)
{
	DisplayName = InDisplayName;
	if (NameText)
	{
		NameText->SetText(DisplayName);
	}
}

TSharedRef<SWidget> UVHVNPCNameplateWidget::RebuildWidget()
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SAssignNew(NameText, STextBlock)
			.Text(DisplayName)
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16))
			.ColorAndOpacity(FLinearColor::FromSRGBColor(FColor(244, 240, 230)))
			.ShadowOffset(FVector2D(1.0f, 1.0f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f))
			.Justification(ETextJustify::Center)
		];
}
