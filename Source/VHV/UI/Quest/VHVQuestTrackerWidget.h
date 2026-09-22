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
    void BeginMajorStingerSuppression();
    void EndMajorStingerSuppressionAndReveal();
    bool IsMajorStingerSuppressed() const { return bUpdatesSuppressed; }

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

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

    bool bUpdatesSuppressed = false;
    bool bRevealAnimating = false;
    float RevealElapsed = 0.0f;
};
