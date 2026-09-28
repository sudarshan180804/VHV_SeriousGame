#pragma once

#include "Blueprint/UserWidget.h"
#include "VHVNPCNameplateWidget.generated.h"

class STextBlock;

/** Minimal transparent screen-space identity label used above named NPCs. */
UCLASS()
class VHV_API UVHVNPCNameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UVHVNPCNameplateWidget(const FObjectInitializer& ObjectInitializer);

	void SetDisplayName(const FText& InDisplayName);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	FText DisplayName;
	TSharedPtr<STextBlock> NameText;
};
