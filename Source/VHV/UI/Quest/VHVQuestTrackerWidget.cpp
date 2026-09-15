#include "UI/Quest/VHVQuestTrackerWidget.h"

#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Quest/Systems/VHVQuestSubsystem.h"

void UVHVQuestTrackerWidget::SetQuestSubsystem(UVHVQuestSubsystem* InQuestSubsystem)
{
    if (QuestSubsystem == InQuestSubsystem)
    {
        RefreshTracker();
        return;
    }
    UnbindFromQuestSubsystem();
    QuestSubsystem = InQuestSubsystem;
    BindToQuestSubsystem();
    RefreshTracker();
}

void UVHVQuestTrackerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (!QuestSubsystem && GetGameInstance())
    {
        QuestSubsystem = GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>();
    }
    BindToQuestSubsystem();
    RefreshTracker();
}

void UVHVQuestTrackerWidget::NativeDestruct()
{
    UnbindFromQuestSubsystem();
    Super::NativeDestruct();
}

void UVHVQuestTrackerWidget::HandleQuestUpdated(FName QuestID)
{
    RefreshTracker();
}

void UVHVQuestTrackerWidget::HandleTrackedQuestChanged(FName QuestID)
{
    RefreshTracker();
}

void UVHVQuestTrackerWidget::BindToQuestSubsystem()
{
    if (QuestSubsystem)
    {
        QuestSubsystem->OnQuestUpdated.AddUniqueDynamic(this, &UVHVQuestTrackerWidget::HandleQuestUpdated);
        QuestSubsystem->OnTrackedQuestChanged.AddUniqueDynamic(this, &UVHVQuestTrackerWidget::HandleTrackedQuestChanged);
    }
}

void UVHVQuestTrackerWidget::UnbindFromQuestSubsystem()
{
    if (QuestSubsystem)
    {
        QuestSubsystem->OnQuestUpdated.RemoveDynamic(this, &UVHVQuestTrackerWidget::HandleQuestUpdated);
        QuestSubsystem->OnTrackedQuestChanged.RemoveDynamic(this, &UVHVQuestTrackerWidget::HandleTrackedQuestChanged);
    }
}

void UVHVQuestTrackerWidget::RefreshTracker()
{
    FVHVQuestJournalEntry Quest;
    const bool bHasActiveTrackedQuest = QuestSubsystem && QuestSubsystem->GetTrackedQuest(Quest) && Quest.Status == EVHVQuestStatus::Active;
    if (!bHasActiveTrackedQuest)
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (QuestTitleText)
    {
        QuestTitleText->SetText(Quest.QuestTitle);
    }
    if (ObjectiveText)
    {
        ObjectiveText->SetText(Quest.CurrentObjectiveText);
    }
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
