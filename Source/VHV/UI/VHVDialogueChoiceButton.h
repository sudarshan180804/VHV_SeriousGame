#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "VHVDialogueChoiceButton.generated.h"

class UVHVDialogueWidget;

/** A runtime-created dialogue choice button that reports its index to the dialogue widget. */
UCLASS()
class VHV_API UVHVDialogueChoiceButton : public UButton
{
    GENERATED_BODY()

public:
    void InitializeChoice(UVHVDialogueWidget* InOwningDialogueWidget, int32 InChoiceIndex);

private:
    UFUNCTION()
    void HandleClicked();

    UPROPERTY()
    TObjectPtr<UVHVDialogueWidget> OwningDialogueWidget;

    int32 ChoiceIndex = INDEX_NONE;
};
