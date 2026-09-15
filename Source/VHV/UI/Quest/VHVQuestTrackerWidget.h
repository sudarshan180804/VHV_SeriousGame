#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHVQuestTrackerWidget.generated.h"

class UTextBlock;
class UVHVQuestSubsystem;

UCLASS()
class VHV_API UVHVQuestTrackerWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    void SetQuestSubsystem(UVHVQuestSubsystem* InQuestSubsystem);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> QuestTitleText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> ObjectiveText;

private:
    UFUNCTION()
    void HandleQuestUpdated(FName QuestID);

    UFUNCTION()
    void HandleTrackedQuestChanged(FName QuestID);

    void BindToQuestSubsystem();
    void UnbindFromQuestSubsystem();
    void RefreshTracker();

    UPROPERTY()
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;
};
