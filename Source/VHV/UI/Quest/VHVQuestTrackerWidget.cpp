#include "UI/Quest/VHVQuestTrackerWidget.h"

#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "UI/Textbook/VHVActivityUIStyle.h"
#include "VHV.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    constexpr float TrackerRevealDuration = 0.22f;
    constexpr float TrackerRevealOffsetX = -10.0f;
}

TSharedRef<SWidget> UVHVQuestTrackerWidget::RebuildWidget()
{
    // Keep the authored UMG tree available for compatibility, but present the tracker
    // natively so every quest uses one responsive, data-driven visual treatment.
    Super::RebuildWidget();

    AccentLineBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(199, 170, 99, 190), 1.0f);
    TrackerPanelBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(10, 17, 22, 86), 12.0f,
        VHVActivityUIStyle::FromSRGB(199, 170, 99, 78), 1.0f);

    return SNew(SOverlay)
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Top)
        .Padding(FMargin(46.0f, 42.0f, 0.0f, 0.0f))
        [
            SNew(SBorder)
            .BorderImage(&TrackerPanelBrush)
            .Padding(FMargin(15.0f, 13.0f, 18.0f, 14.0f))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(FMargin(0.0f, 1.0f, 13.0f, 0.0f))
                [
                    SNew(SBox).WidthOverride(2.0f)
                    [
                        SNew(SBorder).BorderImage(&AccentLineBrush)
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBox).WidthOverride(410.0f)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
                            .Padding(FMargin(0.0f, 3.0f, 9.0f, 0.0f))
                            [
                                SNew(STextBlock)
                                .Font(VHVActivityUIStyle::MediumFont(16))
                                .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
                                .ShadowOffset(FVector2D(1.0f, 1.0f))
                                .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
                                .Text(FText::FromString(TEXT("\u25c7")))
                            ]
                            + SHorizontalBox::Slot().FillWidth(1.0f)
                            [
                                SAssignNew(NativeQuestTitleText, STextBlock)
                                .Font(VHVActivityUIStyle::MediumFont(23))
                                .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                                .ShadowOffset(FVector2D(1.0f, 1.0f))
                                .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f))
                                .AutoWrapText(true)
                                .WrapTextAt(375.0f)
                                .LineHeightPercentage(1.06f)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        .Padding(FMargin(25.0f, 9.0f, 0.0f, 10.0f))
                        [
                            SNew(SBox).WidthOverride(285.0f).HeightOverride(1.0f)
                            [
                                SNew(SBorder).BorderImage(&AccentLineBrush)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Top)
                            .Padding(FMargin(1.0f, 3.0f, 9.0f, 0.0f))
                            [
                                SNew(STextBlock)
                                .Font(VHVActivityUIStyle::MediumFont(11))
                                .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
                                .ShadowOffset(FVector2D(1.0f, 1.0f))
                                .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
                                .Text(FText::FromString(TEXT("\u25cf")))
                            ]
                            + SHorizontalBox::Slot().FillWidth(1.0f)
                            [
                                SAssignNew(NativeObjectiveText, STextBlock)
                                .Font(VHVActivityUIStyle::RegularFont(17))
                                .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                                .ShadowOffset(FVector2D(1.0f, 1.0f))
                                .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f))
                                .AutoWrapText(true)
                                .WrapTextAt(375.0f)
                                .LineHeightPercentage(1.12f)
                            ]
                        ]
                    ]
                ]
            ]
        ];
}

void UVHVQuestTrackerWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    NativeQuestTitleText.Reset();
    NativeObjectiveText.Reset();
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
    if (NativeQuestTitleText)
    {
        NativeQuestTitleText->SetText(Quest.QuestTitle);
    }
    if (NativeObjectiveText)
    {
        NativeObjectiveText->SetText(Quest.CurrentObjectiveText);
    }
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
