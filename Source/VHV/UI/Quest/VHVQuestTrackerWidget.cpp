#include "UI/Quest/VHVQuestTrackerWidget.h"

#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "VHV.h"

namespace
{
    constexpr float TrackerRevealDuration = 0.22f;
    constexpr float TrackerRevealOffsetX = -10.0f;
}

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

void UVHVQuestTrackerWidget::NativeTick(
    const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bRevealAnimating)
    {
        return;
    }

    RevealElapsed = FMath::Min(RevealElapsed + InDeltaTime, TrackerRevealDuration);
    const float RawAlpha = FMath::Clamp(RevealElapsed / TrackerRevealDuration, 0.0f, 1.0f);
    const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, RawAlpha, 3.0f);
    SetRenderOpacity(Alpha);
    SetRenderTranslation(FVector2D(FMath::Lerp(TrackerRevealOffsetX, 0.0f, Alpha), 0.0f));

    if (RawAlpha >= 1.0f)
    {
        bRevealAnimating = false;
    }
}

void UVHVQuestTrackerWidget::BeginMajorStingerSuppression()
{
    if (!bUpdatesSuppressed)
    {
        UE_LOG(LogVHV, Log, TEXT("[VHVQuestTracker] Suppressed for major stinger sequence."));
    }
    bUpdatesSuppressed = true;
    bRevealAnimating = false;
    SetRenderOpacity(1.0f);
    SetRenderTranslation(FVector2D::ZeroVector);
    SetVisibility(ESlateVisibility::Collapsed);
}

void UVHVQuestTrackerWidget::EndMajorStingerSuppressionAndReveal()
{
    if (!bUpdatesSuppressed)
    {
        return;
    }

    bUpdatesSuppressed = false;
    RefreshTracker();
    UE_LOG(LogVHV, Log, TEXT("[VHVQuestTracker] Refreshed once after major stinger sequence."));
    if (GetVisibility() != ESlateVisibility::Collapsed)
    {
        RevealElapsed = 0.0f;
        bRevealAnimating = true;
        SetRenderOpacity(0.0f);
        SetRenderTranslation(FVector2D(TrackerRevealOffsetX, 0.0f));
    }
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
    if (bUpdatesSuppressed)
    {
        return;
    }

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
