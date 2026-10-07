#include "UI/VHVUIManagerComponent.h"
#include "UI/VHVMainHUD.h"
#include "UI/VHVUserWidgetBase.h"
#include "UI/VHVDialogueWidget.h"
#include "UI/Textbook/VHVQuestionWidget.h"
#include "UI/Textbook/VHVFeedbackWidget.h"
#include "UI/Textbook/VHVTeachWidget.h"
#include "UI/Textbook/VHVObservationWidget.h"
#include "UI/VHVMajorQuestStingerWidget.h"
#include "UI/VHVOrderingWidget.h"
#include "UI/VHVMatchingWidget.h"
#include "UI/SocialSupport/VHVSocialSupportHUDWidget.h"
#include "UI/SocialSupport/VHVSupportNetworkWidget.h"
#include "UI/SocialSupport/VHVSupportTypeOverlayWidget.h"
#include "UI/Quest/VHVObjectiveDirectionWidget.h"
#include "World/Tracking/VHVObjectiveTrackingMarker.h"
#include "Core/VHVDialogueTypes.h"
#include "VHV.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "World/Location/VHVQuestLocationVolume.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/BoxComponent.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/GridSlot.h"
#include "Blueprint/UserWidget.h"
#include "Engine/AssetManager.h"
#include "TimerManager.h"
#include "Textbook/Systems/VHVTextbookSubsystem.h"
#include "Player/Components/VHVPlayerInteractionComponent.h"
#include "Player/Components/VHVInteractionComponent.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "Save/VHVSaveTypes.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "UI/Quest/VHVQuestTrackerWidget.h"

namespace
{
    bool IsContinuousTeachingPage(const FTextbookActivityData& Activity)
    {
        const FTeachingContent& Teaching = Activity.Teaching;
        const FString TeachingTitle = Teaching.Title.TrimStartAndEnd();
        const bool bTechniqueIntroduction = Teaching.Category == ETextbookTeachingCategory::Technique
            || TeachingTitle.StartsWith(TEXT("TECHNIQUE "), ESearchCase::IgnoreCase);
        return Activity.ActivityType == ETextbookActivityType::Observation
            && !Activity.EvidenceTagging.bUseEvidenceTagging
            && !bTechniqueIntroduction
            && Teaching.HasMeaningfulContent();
    }

    void ApplyPanelSlotLayout(UWidget* ChildWidget, UPanelWidget* ParentPanel)
    {
        if (!ChildWidget || !ParentPanel)
        {
            return;
        }

        UPanelSlot* Slot = ParentPanel->AddChild(ChildWidget);
        if (!Slot)
        {
            return;
        }

        if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot))
        {
            CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            CanvasSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
            CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
            return;
        }

        if (USizeBoxSlot* SizeBoxSlot = Cast<USizeBoxSlot>(Slot))
        {
            SizeBoxSlot->SetHorizontalAlignment(HAlign_Fill);
            SizeBoxSlot->SetVerticalAlignment(VAlign_Fill);
            SizeBoxSlot->SetPadding(FMargin(0.0f));
            return;
        }

        if (UHorizontalBoxSlot* HorizontalBoxSlot = Cast<UHorizontalBoxSlot>(Slot))
        {
            HorizontalBoxSlot->SetHorizontalAlignment(HAlign_Fill);
            HorizontalBoxSlot->SetVerticalAlignment(VAlign_Fill);
            return;
        }

        if (UVerticalBoxSlot* VerticalBoxSlot = Cast<UVerticalBoxSlot>(Slot))
        {
            VerticalBoxSlot->SetHorizontalAlignment(HAlign_Fill);
            VerticalBoxSlot->SetVerticalAlignment(VAlign_Fill);
            return;
        }

        if (UGridSlot* GridSlot = Cast<UGridSlot>(Slot))
        {
            GridSlot->SetRow(0);
            GridSlot->SetColumn(0);
            GridSlot->SetHorizontalAlignment(HAlign_Fill);
            GridSlot->SetVerticalAlignment(VAlign_Fill);
            return;
        }
    }
}

UVHVUIManagerComponent::UVHVUIManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVHVUIManagerComponent::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC)
    {
        return;
    }

    ObjectiveDirectionWidget = CreateWidget<UVHVObjectiveDirectionWidget>(
        PC, UVHVObjectiveDirectionWidget::StaticClass());
    if (ObjectiveDirectionWidget)
    {
        ObjectiveDirectionWidget->AddToPlayerScreen(40);
        ObjectiveDirectionWidget->HideDirection();
    }

    if (MainHUDClass)
    {
        MainHUD = CreateWidget<UVHVMainHUD>(PC, MainHUDClass);
        if (MainHUD)
        {
            MainHUD->AddToViewport();
            MainHUD->SetInteractionPromptVisible(false);

            if (DialogueWidgetClass && !DialogueWidget)
            {
                DialogueWidget = CreateWidget<UVHVDialogueWidget>(PC, DialogueWidgetClass);
                if (DialogueWidget && MainHUD->DialogueLayer)
                {
                    DialogueWidget->SetOwningUIManager(this);
                    if (!DialogueWidget->GetParent())
                    {
                        ApplyPanelSlotLayout(DialogueWidget, MainHUD->DialogueLayer);
                    }
                    DialogueWidget->HideDialogue();
                }
            }

            if (QuestionWidgetClass && !QuestionWidget)
            {
                QuestionWidget = CreateWidget<UVHVQuestionWidget>(PC, QuestionWidgetClass);
                if (QuestionWidget && MainHUD->TextbookLayer)
                {
                    QuestionWidget->SetOwningUIManager(this);
                    if (!QuestionWidget->GetParent())
                    {
                        ApplyPanelSlotLayout(QuestionWidget, MainHUD->TextbookLayer);
                    }
                    QuestionWidget->SetVisibility(ESlateVisibility::Collapsed);
                }
            }

            if (FeedbackWidgetClass && !FeedbackWidget)
            {
                FeedbackWidget = CreateWidget<UVHVFeedbackWidget>(PC, FeedbackWidgetClass);
                if (FeedbackWidget && MainHUD->TextbookLayer)
                {
                    FeedbackWidget->SetOwningUIManager(this);
                    if (!FeedbackWidget->GetParent())
                    {
                        ApplyPanelSlotLayout(FeedbackWidget, MainHUD->TextbookLayer);
                    }
                    FeedbackWidget->SetVisibility(ESlateVisibility::Collapsed);
                }
            }

            if (TeachWidgetClass && !TeachWidget)
            {
                TeachWidget = CreateWidget<UVHVTeachWidget>(PC, TeachWidgetClass);
                if (TeachWidget && MainHUD->TextbookLayer)
                {
                    TeachWidget->SetOwningUIManager(this);
                    if (!TeachWidget->GetParent())
                    {
                        ApplyPanelSlotLayout(TeachWidget, MainHUD->TextbookLayer);
                    }
                    TeachWidget->SetVisibility(ESlateVisibility::Collapsed);
                }
            }

            EnsureObservationWidget();
            EnsureSocialSupportWidgets();

            if (OrderingWidgetClass && !OrderingWidget)
            {
                OrderingWidget = CreateWidget<UVHVOrderingWidget>(PC, OrderingWidgetClass);
                if (OrderingWidget && MainHUD->TextbookLayer)
                {
                    OrderingWidget->SetOwningUIManager(this);
                    if (!OrderingWidget->GetParent())
                    {
                        ApplyPanelSlotLayout(OrderingWidget, MainHUD->TextbookLayer);
                    }
                    OrderingWidget->SetVisibility(ESlateVisibility::Collapsed);
                }
            }

            if (MatchingWidgetClass && !MatchingWidget)
            {
                MatchingWidget = CreateWidget<UVHVMatchingWidget>(PC, MatchingWidgetClass);
                if (MatchingWidget && MainHUD->TextbookLayer)
                {
                    MatchingWidget->SetOwningUIManager(this);
                    if (!MatchingWidget->GetParent())
                    {
                        ApplyPanelSlotLayout(MatchingWidget, MainHUD->TextbookLayer);
                    }
                    MatchingWidget->SetVisibility(ESlateVisibility::Collapsed);
                }
            }

            if (QuestTrackerWidgetClass && MainHUD->TopLayer)
            {
                QuestTrackerWidget = CreateWidget<UVHVQuestTrackerWidget>(PC, QuestTrackerWidgetClass);
                if (QuestTrackerWidget)
                {
                    ApplyPanelSlotLayout(QuestTrackerWidget, MainHUD->TopLayer);
                    QuestTrackerWidget->SetVisibility(ESlateVisibility::Collapsed);
                }
            }
        }
    }

    UWorld* World = GetWorld();
    if (World && World->GetGameInstance())
    {
        TextbookSubsystem = World->GetGameInstance()->GetSubsystem<UVHVTextbookSubsystem>();
        if (TextbookSubsystem)
        {
            TextbookSubsystem->OnLearningPhaseChanged.AddDynamic(this, &UVHVUIManagerComponent::HandleLearningPhaseChanged);
            TextbookSubsystem->OnActivityCompleted.AddDynamic(this, &UVHVUIManagerComponent::HandleTextbookActivityCompleted);
        }

        QuestSubsystem = World->GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>();
        if (QuestSubsystem)
        {
            QuestSubsystem->OnObjectiveActivationRequested.AddDynamic(this, &UVHVUIManagerComponent::HandleQuestObjectiveActivationRequested);
            QuestSubsystem->OnObjectiveReady.AddDynamic(this, &UVHVUIManagerComponent::HandleQuestObjectiveReady);
            QuestSubsystem->OnObjectiveChanged.AddDynamic(this, &UVHVUIManagerComponent::HandleQuestObjectiveChanged);
            QuestSubsystem->OnObjectiveTrackingTargetChanged.AddDynamic(
                this, &UVHVUIManagerComponent::HandleQuestObjectiveTrackingTargetChanged);
            QuestSubsystem->OnQuestStarted.AddDynamic(this, &UVHVUIManagerComponent::HandleQuestStarted);
            QuestSubsystem->OnQuestCompleted.AddDynamic(this, &UVHVUIManagerComponent::HandleQuestCompleted);
            if (QuestTrackerWidget)
            {
                QuestTrackerWidget->SetQuestSubsystem(QuestSubsystem);
            }
        }

        StoryStateSubsystem = World->GetGameInstance()->GetSubsystem<UVHVStoryStateSubsystem>();
    }

    APawn* Pawn = PC->GetPawn();
    if (Pawn)
    {
        PlayerInteractionComponent = Pawn->FindComponentByClass<UVHVPlayerInteractionComponent>();
        if (PlayerInteractionComponent)
        {
            PlayerInteractionComponent->OnInteractionTargetChanged.AddDynamic(this, &UVHVUIManagerComponent::HandleInteractionTargetChanged);
            CurrentInteractionTarget = PlayerInteractionComponent->GetCurrentTarget();
        }
    }

    ApplyUIState(EVHVUIState::Gameplay);
}

void UVHVUIManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearObjectiveTracking();
    if (ObjectiveTrackingMarker)
    {
        ObjectiveTrackingMarker->Destroy();
        ObjectiveTrackingMarker = nullptr;
    }

    if (MajorQuestStingerWidget)
    {
        MajorQuestStingerWidget->OnStingerFinished.RemoveAll(this);
    }

    if (PlayerInteractionComponent)
    {
        PlayerInteractionComponent->OnInteractionTargetChanged.RemoveDynamic(this, &UVHVUIManagerComponent::HandleInteractionTargetChanged);
    }

    if (TextbookSubsystem)
    {
        TextbookSubsystem->OnLearningPhaseChanged.RemoveDynamic(this, &UVHVUIManagerComponent::HandleLearningPhaseChanged);
        TextbookSubsystem->OnActivityCompleted.RemoveDynamic(this, &UVHVUIManagerComponent::HandleTextbookActivityCompleted);
    }

    if (QuestSubsystem)
    {
        QuestSubsystem->OnObjectiveActivationRequested.RemoveDynamic(this, &UVHVUIManagerComponent::HandleQuestObjectiveActivationRequested);
        QuestSubsystem->OnObjectiveReady.RemoveDynamic(this, &UVHVUIManagerComponent::HandleQuestObjectiveReady);
        QuestSubsystem->OnObjectiveChanged.RemoveDynamic(this, &UVHVUIManagerComponent::HandleQuestObjectiveChanged);
        QuestSubsystem->OnObjectiveTrackingTargetChanged.RemoveDynamic(
            this, &UVHVUIManagerComponent::HandleQuestObjectiveTrackingTargetChanged);
        QuestSubsystem->OnQuestStarted.RemoveDynamic(this, &UVHVUIManagerComponent::HandleQuestStarted);
        QuestSubsystem->OnQuestCompleted.RemoveDynamic(this, &UVHVUIManagerComponent::HandleQuestCompleted);
    }

    Super::EndPlay(EndPlayReason);
}

void UVHVUIManagerComponent::HandleInteractionTargetChanged(UVHVInteractionComponent* NewTarget)
{
    CurrentInteractionTarget = NewTarget;
    RefreshInteractionPrompt();
    if (ObjectiveTrackingMode != EVHVObjectiveTrackingMode::Off)
    {
        UpdateObjectiveTrackingProximity();
    }
}

void UVHVUIManagerComponent::HandleLearningPhaseChanged(ELearningPhase NewPhase)
{
    if (bStartingMajorStingerActivity && NewPhase == ELearningPhase::Ask)
    {
        HideQuestionUI();
        HideObservationUI();
        HideFeedbackUI();
        HideTeachUI();
        return;
    }

    if (NewPhase == ELearningPhase::Ask)
    {
        if (TextbookSubsystem && TextbookSubsystem->IsActivityActive())
        {
            const FTextbookActivityData CurrentActivity = TextbookSubsystem->GetCurrentActivity();
            const bool bDirectMediaLesson = CurrentActivity.ActivityType == ETextbookActivityType::Observation
                && !CurrentActivity.EvidenceTagging.bUseEvidenceTagging
                && CurrentActivity.Teaching.Category != ETextbookTeachingCategory::Technique
                && !CurrentActivity.Teaching.MediaTexture.IsNull();
            if (bDirectMediaLesson
                || (bAwaitingContinuousTeachingResult && IsContinuousTeachingPage(CurrentActivity)))
            {
                // Passive lesson entries use Observation as their launch shell.
                // A primary media image is already the complete teaching page,
                // so bypass the otherwise empty Observation prompt page.
                TextbookSubsystem->EnterTeachPhase();
                return;
            }
        }

        HideFeedbackUI();
        HideTeachUI();
        RefreshCurrentAskQuestionUI();

        if (TextbookSubsystem && TextbookSubsystem->IsActivityActive()
            && TextbookSubsystem->GetCurrentActivity().ActivityType == ETextbookActivityType::DialogueChoice)
        {
            return;
        }

        ApplyUIState(EVHVUIState::LearningAsk);
        return;
    }

    if (NewPhase == ELearningPhase::Feedback)
    {
        EnsureFeedbackWidget();
        HideQuestionUI();
        HideObservationUI();
        HideTeachUI();
        RefreshCurrentFeedbackUI();
        ApplyUIState(EVHVUIState::LearningFeedback);
        return;
    }

    if (NewPhase == ELearningPhase::Hint)
    {
        EnsureFeedbackWidget();
        // Keep the assessed question visible behind the first-attempt hint.
        // Input remains owned by the hint until it is acknowledged; Ask then
        // rebuilds the options for a clean second attempt.
        HideObservationUI();
        HideOrderingUI();
        HideMatchingUI();
        HideTeachUI();
        RefreshCurrentHintUI();
        ApplyUIState(EVHVUIState::LearningHint);
        return;
    }

    if (NewPhase == ELearningPhase::Teach)
    {
        if (TextbookSubsystem && TextbookSubsystem->IsActivityActive()
            && !TextbookSubsystem->GetCurrentActivity().Teaching.HasMeaningfulContent())
        {
            TextbookSubsystem->AdvanceToNextActivity();
            return;
        }
        HideQuestionUI();
        HideObservationUI();
        HideFeedbackUI();
        RefreshCurrentTeachUI();
        ApplyUIState(EVHVUIState::LearningTeach);
        return;
    }

    HideQuestionUI();
    HideObservationUI();
    HideFeedbackUI();
    HideTeachUI();
    ApplyUIState(EVHVUIState::Gameplay);
}

void UVHVUIManagerComponent::HandleQuestObjectiveActivationRequested(FName QuestID, FName ObjectiveID)
{
    if (!QuestSubsystem)
    {
        return;
    }

    FVHVQuestObjectiveDefinition Objective;
    if (!QuestSubsystem->GetCurrentObjective(Objective) || Objective.ObjectiveID != ObjectiveID)
    {
        return;
    }

    if (Objective.ObjectiveType == EVHVQuestObjectiveType::LearningActivity)
    {
        if (TextbookSubsystem)
        {
            const FString ActivityID = Objective.GetEffectiveActivityID().ToString();
            TextbookSubsystem->SetProgressionMode(EVHVTextbookProgressionMode::QuestManaged);
            bQuestManagedLearningActivityActive = TextbookSubsystem->StartActivityByID(ActivityID);
            if (!bQuestManagedLearningActivityActive)
            {
                UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Could not start activity '%s' for objective '%s'."), *ActivityID, *ObjectiveID.ToString());
            }
        }
        return;
    }

    if (Objective.ObjectiveType != EVHVQuestObjectiveType::Talk && Objective.ObjectiveType != EVHVQuestObjectiveType::Conversation)
    {
        return;
    }

    UVHVConversationDataAsset* ConversationAsset = Objective.Conversation.LoadSynchronous();
    if (!ConversationAsset)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Could not load conversation for objective '%s'."), *ObjectiveID.ToString());
        return;
    }

    const FString ConversationID = ConversationAsset->Conversation.ConversationID;
    const FConversationRuntimeState* ExistingState = ConversationStates.Find(ConversationID);
    const bool bHasResumeState = ExistingState && (!ExistingState->CurrentNodeID.IsEmpty() || !ExistingState->LatestCheckpointID.IsEmpty());
    if (!Objective.EntryNodeID.IsNone() && !bHasResumeState)
    {
        StartConversationAtNode(ConversationAsset->Conversation, Objective.EntryNodeID.ToString());
    }
    else
    {
        StartConversationFromAsset(ConversationAsset);
    }
}

void UVHVUIManagerComponent::HandleQuestObjectiveReady(
    const FName QuestID,
    const FName ObjectiveID)
{
    FVHVQuestDefinition Quest;
    if (!QuestSubsystem || !QuestSubsystem->GetQuestDefinition(QuestID, Quest))
    {
        return;
    }
    const FVHVQuestObjectiveDefinition* Objective = Quest.Objectives.FindByPredicate(
        [ObjectiveID](const FVHVQuestObjectiveDefinition& Candidate)
        {
            return Candidate.ObjectiveID == ObjectiveID;
        });
    if (Objective && Objective->ActivationStinger.IsConfigured())
    {
        QueueMajorQuestStinger(Objective->ActivationStinger);
    }
}

void UVHVUIManagerComponent::HandleQuestObjectiveChanged(
    const FName QuestID,
    const FName ObjectiveID)
{
    ClearObjectiveTracking(true);
    if (QuestID == FName(TEXT("Q_HBCT_05_SELF_MONITORING")))
    {
        StageSelfMonitoringParticipants(ObjectiveID);
    }
}

void UVHVUIManagerComponent::HandleQuestObjectiveTrackingTargetChanged(
    const FName QuestID,
    const FName ObjectiveID)
{
    if (ObjectiveTrackingMode != EVHVObjectiveTrackingMode::Off)
    {
        RefreshActiveObjectiveTrackingTarget(QuestID, ObjectiveID);
    }
}

bool UVHVUIManagerComponent::StageQuestParticipantAtLocation(
    const FName ParticipantID,
    const FName LocationID)
{
    UWorld* World = GetWorld();
    if (!World || !QuestSubsystem) return false;

    AActor* ParticipantActor = nullptr;
    AActor* DestinationActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (const UVHVQuestParticipantComponent* Participant =
            It->FindComponentByClass<UVHVQuestParticipantComponent>())
        {
            if (Participant->GetEffectiveParticipantID() == ParticipantID)
            {
                if (ParticipantActor)
                {
                    UE_LOG(LogVHV, Warning,
                        TEXT("[VHVQuest] Quest 5 staging rejected duplicate participant '%s'."),
                        *ParticipantID.ToString());
                    return false;
                }
                ParticipantActor = *It;
            }
        }
        if (const AVHVQuestLocationVolume* Location = Cast<AVHVQuestLocationVolume>(*It))
        {
            if (Location->GetEffectiveLocationID() == LocationID)
            {
                DestinationActor = *It;
            }
        }
    }

    if (!ParticipantActor || !DestinationActor)
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHVQuest] Quest 5 staging could not resolve participant '%s' or location '%s'."),
            *ParticipantID.ToString(), *LocationID.ToString());
        return false;
    }

    QuestSubsystem->CancelNPCQuestCommand(ParticipantID);
    return ParticipantActor->SetActorLocationAndRotation(
        DestinationActor->GetActorLocation(), DestinationActor->GetActorRotation(),
        false, nullptr, ETeleportType::TeleportPhysics);
}

void UVHVUIManagerComponent::StageSelfMonitoringParticipants(const FName ObjectiveID)
{
    const auto Stage = [this](const TCHAR* Participant, const TCHAR* Location)
    {
        if (!StageQuestParticipantAtLocation(FName(Participant), FName(Location)))
        {
            UE_LOG(LogVHV, Warning,
                TEXT("[VHVQuest] Quest 5 objective staging failed: participant='%s' location='%s'."),
                Participant, Location);
        }
    };

    if (ObjectiveID == FName(TEXT("O01_CheckInWithChai"))) Stage(TEXT("UncleChai"), TEXT("SMChaiHome"));
    else if (ObjectiveID == FName(TEXT("O02_LearnWhyRecordsMatter")))
    {
        Stage(TEXT("Instructor"), TEXT("SMInstructorLesson"));
        Stage(TEXT("UncleChai"), TEXT("SMInstructorLessonChai"));
    }
    else if (ObjectiveID == FName(TEXT("O03_IdentifyTargetBehavior"))) Stage(TEXT("UncleChai"), TEXT("SMTargetBehaviorArea"));
    else if (ObjectiveID == FName(TEXT("O04_BuildMonitoringProcess"))) Stage(TEXT("Instructor"), TEXT("SMMonitoringProcess"));
    else if (ObjectiveID == FName(TEXT("O05_ChooseMonitoringMethod"))) Stage(TEXT("UncleChai"), TEXT("SMMethodChai"));
    else if (ObjectiveID == FName(TEXT("O06_PrepareChaiTracker"))) Stage(TEXT("UncleChai"), TEXT("SMTrackerChai"));
    else if (ObjectiveID == FName(TEXT("O07_RecordBreakfast"))) Stage(TEXT("UncleChai"), TEXT("SMBreakfast"));
    else if (ObjectiveID == FName(TEXT("O08_RecordAfternoon")))
    {
        Stage(TEXT("UncleChai"), TEXT("SMAfternoon"));
        Stage(TEXT("SelfMonitoringFriend"), TEXT("SMAfternoonFriend"));
    }
    else if (ObjectiveID == FName(TEXT("O09_RecordEveningWalk"))) Stage(TEXT("UncleChai"), TEXT("SMChaiWalkStart"));
    else if (ObjectiveID == FName(TEXT("O10_ReviewFirstDay")))
    {
        Stage(TEXT("UncleChai"), TEXT("SMFirstDayReview"));
        Stage(TEXT("Instructor"), TEXT("SMFirstDayInstructor"));
    }
    else if (ObjectiveID == FName(TEXT("O11_OneWeekLater")))
    {
        Stage(TEXT("UncleChai"), TEXT("SMOneWeekLater"));
        Stage(TEXT("Instructor"), TEXT("SMOneWeekInstructor"));
    }
    else if (ObjectiveID == FName(TEXT("O13_AdjustThePlan"))) Stage(TEXT("Instructor"), TEXT("SMAdjustment"));
    else if (ObjectiveID == FName(TEXT("O14_PracticeAdjustedRoutine"))) Stage(TEXT("UncleChai"), TEXT("SMAdjustedWalkStart"));
    else if (ObjectiveID == FName(TEXT("O15_BuildProgressGraph")))
    {
        Stage(TEXT("UncleChai"), TEXT("SMProgressGraphChai"));
        Stage(TEXT("Instructor"), TEXT("SMProgressGraphInstructor"));
    }
    else if (ObjectiveID == FName(TEXT("O16_SetSelfReminder"))) Stage(TEXT("UncleChai"), TEXT("SMSelfReminder"));
    else if (ObjectiveID == FName(TEXT("O17_WatchChaiSelfMonitor")))
    {
        Stage(TEXT("UncleChai"), TEXT("SMSelfMonitorPayoff"));
        Stage(TEXT("Instructor"), TEXT("SMSelfMonitorInstructor"));
    }
    else if (ObjectiveID == FName(TEXT("O18_FinalDebrief")))
    {
        Stage(TEXT("Instructor"), TEXT("SMFinalDebrief"));
        Stage(TEXT("UncleChai"), TEXT("SMFinalChai"));
    }
}

void UVHVUIManagerComponent::HandleTextbookActivityCompleted()
{
    if (!TextbookSubsystem)
    {
        return;
    }

    if (!bConversationActive)
    {
        if (bQuestManagedLearningActivityActive && !bAwaitingContinuousTeachingResult)
        {
            bQuestManagedLearningActivityActive = false;
            RestoreGameplayAfterQuestModalIfNeeded();
        }
        return;
    }

    if (TextbookSubsystem->GetProgressionMode() != EVHVTextbookProgressionMode::QuestManaged)
    {
        return;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(ActiveConversation.ConversationID, ActiveConversationState.CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::LearningActivity)
    {
        return;
    }

    const FTextbookActivityData CompletedActivity = TextbookSubsystem->GetCurrentActivity();
    if (CurrentNode->LinkedActivity.GetEffectiveActivityID() != CompletedActivity.GetEffectiveActivityID())
    {
        return;
    }

    if (CurrentNode->NextNodeID.IsEmpty())
    {
        ApplyCurrentDialogueNodeCompletionEffects();
        CompleteConversation();
    }
    else
    {
        ApplyCurrentDialogueNodeCompletionEffects();
        TraverseToNode(CurrentNode->NextNodeID);
    }
}

void UVHVUIManagerComponent::EnsureFeedbackWidget()
{
    if (FeedbackWidget)
    {
        return;
    }

    if (!MainHUD || !MainHUD->TextbookLayer || !FeedbackWidgetClass)
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC)
    {
        return;
    }

    FeedbackWidget = CreateWidget<UVHVFeedbackWidget>(PC, FeedbackWidgetClass);
    if (!FeedbackWidget)
    {
        return;
    }

    FeedbackWidget->SetOwningUIManager(this);

    if (!FeedbackWidget->GetParent())
    {
        ApplyPanelSlotLayout(FeedbackWidget, MainHUD->TextbookLayer);
    }

    FeedbackWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UVHVUIManagerComponent::EnsureObservationWidget()
{
    if (ObservationWidget || !MainHUD || !MainHUD->TextbookLayer)
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC)
    {
        return;
    }

    TSubclassOf<UVHVObservationWidget> WidgetClass = ObservationWidgetClass;
    if (!WidgetClass)
    {
        WidgetClass = UVHVObservationWidget::StaticClass();
    }
    ObservationWidget = CreateWidget<UVHVObservationWidget>(PC, WidgetClass);
    if (!ObservationWidget)
    {
        return;
    }

    ObservationWidget->SetOwningUIManager(this);
    if (!ObservationWidget->GetParent())
    {
        ApplyPanelSlotLayout(ObservationWidget, MainHUD->TextbookLayer);
    }
    ObservationWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UVHVUIManagerComponent::EnsureOrderingWidget()
{
    if (OrderingWidget)
    {
        return;
    }

    if (!MainHUD || !MainHUD->TextbookLayer || !OrderingWidgetClass)
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC)
    {
        return;
    }

    OrderingWidget = CreateWidget<UVHVOrderingWidget>(PC, OrderingWidgetClass);
    if (!OrderingWidget)
    {
        return;
    }

    OrderingWidget->SetOwningUIManager(this);
    if (!OrderingWidget->GetParent())
    {
        ApplyPanelSlotLayout(OrderingWidget, MainHUD->TextbookLayer);
    }

    OrderingWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UVHVUIManagerComponent::EnsureMatchingWidget()
{
    if (MatchingWidget || !MainHUD || !MainHUD->TextbookLayer || !MatchingWidgetClass)
    {
        return;
    }
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC)
    {
        return;
    }
    MatchingWidget = CreateWidget<UVHVMatchingWidget>(PC, MatchingWidgetClass);
    if (MatchingWidget)
    {
        MatchingWidget->SetOwningUIManager(this);
        ApplyPanelSlotLayout(MatchingWidget, MainHUD->TextbookLayer);
        MatchingWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UVHVUIManagerComponent::EnsureSocialSupportWidgets()
{
    if (!MainHUD)
    {
        return;
    }
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    UPanelWidget* OverlayLayer = MainHUD->TopLayer
        ? MainHUD->TopLayer.Get() : MainHUD->TextbookLayer.Get();
    if (!PC || !OverlayLayer)
    {
        return;
    }

    if (!SupportTypeOverlayWidget)
    {
        SupportTypeOverlayWidget = CreateWidget<UVHVSupportTypeOverlayWidget>(
            PC, UVHVSupportTypeOverlayWidget::StaticClass());
        if (SupportTypeOverlayWidget)
        {
            ApplyPanelSlotLayout(SupportTypeOverlayWidget, OverlayLayer);
            SupportTypeOverlayWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    if (!SocialSupportHUDWidget)
    {
        SocialSupportHUDWidget = CreateWidget<UVHVSocialSupportHUDWidget>(
            PC, UVHVSocialSupportHUDWidget::StaticClass());
        if (SocialSupportHUDWidget)
        {
            ApplyPanelSlotLayout(SocialSupportHUDWidget, OverlayLayer);
            SocialSupportHUDWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    if (!SupportNetworkWidget && MainHUD->TextbookLayer)
    {
        SupportNetworkWidget = CreateWidget<UVHVSupportNetworkWidget>(
            PC, UVHVSupportNetworkWidget::StaticClass());
        if (SupportNetworkWidget)
        {
            SupportNetworkWidget->SetOwningUIManager(this);
            ApplyPanelSlotLayout(SupportNetworkWidget, MainHUD->TextbookLayer);
            SupportNetworkWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

UUserWidget* UVHVUIManagerComponent::ResolveModalFocusTarget(const EVHVUIState State) const
{
    if (State == EVHVUIState::Dialogue)
    {
        return DialogueWidget;
    }

    if (State == EVHVUIState::LearningFeedback || State == EVHVUIState::LearningHint)
    {
        return FeedbackWidget;
    }

    if (State == EVHVUIState::LearningTeach)
    {
        return TeachWidget;
    }

    if (State != EVHVUIState::LearningAsk || !TextbookSubsystem || !TextbookSubsystem->IsActivityActive())
    {
        return QuestionWidget;
    }

    const EVHVActivityPresentationStyle PresentationStyle =
        TextbookSubsystem->GetCurrentActivity().PresentationStyle;
    if (PresentationStyle == EVHVActivityPresentationStyle::SocialSupportNetwork
        || PresentationStyle == EVHVActivityPresentationStyle::SocialSupportNetworkReadOnly)
    {
        return SupportNetworkWidget;
    }

    switch (TextbookSubsystem->GetCurrentActivity().ActivityType)
    {
    case ETextbookActivityType::Ordering:
        return OrderingWidget;
    case ETextbookActivityType::Matching:
        return MatchingWidget;
    case ETextbookActivityType::Observation:
        return ObservationWidget;
    case ETextbookActivityType::DialogueChoice:
        return DialogueWidget;
    default:
        return QuestionWidget;
    }
}

void UVHVUIManagerComponent::FocusModalWidget(const EVHVUIState ExpectedState)
{
    if (CurrentUIState != ExpectedState)
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(GetOwner());
    UUserWidget* FocusTarget = ResolveModalFocusTarget(ExpectedState);
    if (!PC || !FocusTarget || FocusTarget->GetVisibility() == ESlateVisibility::Collapsed)
    {
        return;
    }

    if (UVHVUserWidgetBase* ModalWidget = Cast<UVHVUserWidgetBase>(FocusTarget))
    {
        ModalWidget->PrepareForModalFocus();
    }
    FocusTarget->SetIsFocusable(true);
    FocusTarget->SetUserFocus(PC);
    FocusTarget->SetKeyboardFocus();
}

void UVHVUIManagerComponent::ApplyModalInputAndFocus(const EVHVUIState State)
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC)
    {
        return;
    }

    UUserWidget* FocusTarget = ResolveModalFocusTarget(State);
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    if (FocusTarget)
    {
        FocusTarget->SetIsFocusable(true);
        InputMode.SetWidgetToFocus(FocusTarget->TakeWidget());
    }
    PC->SetInputMode(InputMode);
    PC->bShowMouseCursor = true;
    SetMovementLocked(true);
    FocusModalWidget(State);

    // Newly shown or repopulated widgets finish their Slate focus path on the
    // next tick. Reassert focus then so consecutive quest activities cannot
    // retain a stale path to the previous modal.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateWeakLambda(this, [this, State]()
            {
                FocusModalWidget(State);
            }));
    }
}

void UVHVUIManagerComponent::ApplyUIState(EVHVUIState NewState)
{
    CurrentUIState = NewState;

    if (DialogueWidget)
    {
        DialogueWidget->SetVisibility(NewState == EVHVUIState::Dialogue ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (QuestionWidget)
    {
        const bool bHasChoiceActivity = TextbookSubsystem && TextbookSubsystem->IsActivityActive()
            && (TextbookSubsystem->GetCurrentActivity().ActivityType == ETextbookActivityType::SingleChoice
                || TextbookSubsystem->GetCurrentActivity().ActivityType == ETextbookActivityType::MultiChoice);
        const bool bRetryHint = NewState == EVHVUIState::LearningHint
            && bHasChoiceActivity
            && TextbookSubsystem->GetCurrentActivity().AttemptPolicy.bEnabled;
        const bool bShouldShowQuestion = bRetryHint
            || (NewState == EVHVUIState::LearningAsk
                && (!TextbookSubsystem || !TextbookSubsystem->IsActivityActive()
                    || (TextbookSubsystem->GetCurrentActivity().ActivityType != ETextbookActivityType::Ordering
                        && TextbookSubsystem->GetCurrentActivity().ActivityType != ETextbookActivityType::Matching
                        && TextbookSubsystem->GetCurrentActivity().ActivityType != ETextbookActivityType::Observation)));
        QuestionWidget->SetVisibility(bShouldShowQuestion ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (FeedbackWidget)
    {
        FeedbackWidget->SetVisibility(NewState == EVHVUIState::LearningFeedback || NewState == EVHVUIState::LearningHint ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (TeachWidget)
    {
        TeachWidget->SetVisibility(NewState == EVHVUIState::LearningTeach ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (ObservationWidget)
    {
        const bool bShouldShowObservation = NewState == EVHVUIState::LearningAsk
            && TextbookSubsystem && TextbookSubsystem->IsActivityActive()
            && TextbookSubsystem->GetCurrentActivity().ActivityType == ETextbookActivityType::Observation
            && TextbookSubsystem->GetCurrentActivity().PresentationStyle
                == EVHVActivityPresentationStyle::Default;
        ObservationWidget->SetVisibility(bShouldShowObservation ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (OrderingWidget)
    {
        const bool bShouldShowOrdering = NewState == EVHVUIState::LearningAsk && TextbookSubsystem && TextbookSubsystem->IsActivityActive() && TextbookSubsystem->GetCurrentActivity().ActivityType == ETextbookActivityType::Ordering;
        OrderingWidget->SetVisibility(bShouldShowOrdering ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (MatchingWidget)
    {
        const bool bShouldShowMatching = NewState == EVHVUIState::LearningAsk
            && TextbookSubsystem && TextbookSubsystem->IsActivityActive()
            && TextbookSubsystem->GetCurrentActivity().ActivityType == ETextbookActivityType::Matching
            && TextbookSubsystem->GetCurrentActivity().PresentationStyle
                == EVHVActivityPresentationStyle::Default;
        MatchingWidget->SetVisibility(bShouldShowMatching ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (SupportNetworkWidget)
    {
        const bool bShouldShowNetwork = NewState == EVHVUIState::LearningAsk
            && TextbookSubsystem && TextbookSubsystem->IsActivityActive()
            && (TextbookSubsystem->GetCurrentActivity().PresentationStyle
                    == EVHVActivityPresentationStyle::SocialSupportNetwork
                || TextbookSubsystem->GetCurrentActivity().PresentationStyle
                    == EVHVActivityPresentationStyle::SocialSupportNetworkReadOnly);
        SupportNetworkWidget->SetVisibility(bShouldShowNetwork
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC)
    {
        return;
    }

    switch (NewState)
    {
    case EVHVUIState::Gameplay:
    {
        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = false;
        SetMovementLocked(false);
        RefreshInteractionPrompt();

        const ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
        const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
        UE_LOG(LogVHV, Log, TEXT("[VHVUI] State=%s MoveIgnored=%s LookIgnored=%s MovementMode=%s Cursor=%s"),
            *UEnum::GetValueAsString(CurrentUIState),
            PC->IsMoveInputIgnored() ? TEXT("true") : TEXT("false"),
            PC->IsLookInputIgnored() ? TEXT("true") : TEXT("false"),
            Movement ? *UEnum::GetValueAsString(Movement->MovementMode) : TEXT("None"),
            PC->bShowMouseCursor ? TEXT("true") : TEXT("false"));
        break;
    }
    case EVHVUIState::Dialogue:
    case EVHVUIState::LearningAsk:
    case EVHVUIState::LearningHint:
    case EVHVUIState::LearningFeedback:
    case EVHVUIState::LearningTeach:
    {
        ApplyModalInputAndFocus(NewState);
        if (MainHUD)
        {
            MainHUD->SetInteractionPromptVisible(false);
        }
        break;
    }
    case EVHVUIState::MajorStinger:
    {
        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = false;
        SetMovementLocked(true);
        if (MainHUD)
        {
            MainHUD->SetInteractionPromptVisible(false);
        }
        break;
    }
    default:
        break;
    }
}

void UVHVUIManagerComponent::RefreshInteractionPrompt()
{
    if (!MainHUD)
    {
        return;
    }

    const bool bGameplayActive = CurrentUIState == EVHVUIState::Gameplay && !bConversationActive && (!TextbookSubsystem || !TextbookSubsystem->IsActivityActive());
    if (CurrentInteractionTarget && CurrentInteractionTarget->CanInteract() && bGameplayActive)
    {
        MainHUD->SetInteractionPrompt(CurrentInteractionTarget->GetInteractionPrompt());
        MainHUD->SetInteractionPromptVisible(true);
    }
    else
    {
        MainHUD->SetInteractionPromptVisible(false);
    }
}

void UVHVUIManagerComponent::RefreshCurrentAskQuestionUI()
{
    if (!MainHUD || !MainHUD->TextbookLayer || !TextbookSubsystem)
    {
        return;
    }

    if (!TextbookSubsystem->IsActivityActive())
    {
        HideQuestionUI();
        HideObservationUI();
        HideOrderingUI();
        HideMatchingUI();
        HideSupportNetworkUI();
        return;
    }

    const FTextbookActivityData CurrentActivity = TextbookSubsystem->GetCurrentActivity();
    if (CurrentActivity.PresentationStyle == EVHVActivityPresentationStyle::SocialSupportNetwork
        || CurrentActivity.PresentationStyle == EVHVActivityPresentationStyle::SocialSupportNetworkReadOnly)
    {
        EnsureSocialSupportWidgets();
        if (SupportNetworkWidget)
        {
            SupportNetworkWidget->SetOwningUIManager(this);
            SupportNetworkWidget->Configure(CurrentActivity);
            SupportNetworkWidget->SetVisibility(ESlateVisibility::Visible);
        }
        HideQuestionUI();
        HideObservationUI();
        HideOrderingUI();
        HideMatchingUI();
        HideFeedbackUI();
        HideTeachUI();
        if (DialogueWidget) DialogueWidget->SetVisibility(ESlateVisibility::Collapsed);
        MainHUD->SetInteractionPromptVisible(false);
        return;
    }
    if (CurrentActivity.ActivityType == ETextbookActivityType::DialogueChoice)
    {
        HideQuestionUI();
        HideOrderingUI();
        StartDialogueChoiceActivity(CurrentActivity.DialogueChoice);
        return;
    }

    if (CurrentActivity.ActivityType == ETextbookActivityType::Observation)
    {
        EnsureObservationWidget();
        if (ObservationWidget)
        {
            ObservationWidget->SetOwningUIManager(this);
            ObservationWidget->SetObservationData(CurrentActivity);
            if (!ObservationWidget->GetParent())
            {
                ApplyPanelSlotLayout(ObservationWidget, MainHUD->TextbookLayer);
            }
            ObservationWidget->SetVisibility(ESlateVisibility::Visible);
            ObservationWidget->SetIsFocusable(true);
        }

        HideQuestionUI();
        HideOrderingUI();
        HideMatchingUI();
        HideFeedbackUI();
        HideTeachUI();
        if (DialogueWidget)
        {
            DialogueWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
        MainHUD->SetInteractionPromptVisible(false);
        return;
    }

    if (CurrentActivity.ActivityType == ETextbookActivityType::Ordering)
    {
        EnsureOrderingWidget();
        if (OrderingWidget)
        {
            OrderingWidget->SetOwningUIManager(this);
            OrderingWidget->SetOrderingItems(CurrentActivity.OrderingItems);
            if (!OrderingWidget->GetParent())
            {
                ApplyPanelSlotLayout(OrderingWidget, MainHUD->TextbookLayer);
            }
            OrderingWidget->SetVisibility(ESlateVisibility::Visible);
            OrderingWidget->SetIsFocusable(true);
        }

        if (QuestionWidget)
        {
            QuestionWidget->SetVisibility(ESlateVisibility::Collapsed);
        }

        if (FeedbackWidget)
        {
            FeedbackWidget->SetVisibility(ESlateVisibility::Collapsed);
        }

        if (TeachWidget)
        {
            TeachWidget->SetVisibility(ESlateVisibility::Collapsed);
        }

        if (DialogueWidget)
        {
            DialogueWidget->SetVisibility(ESlateVisibility::Collapsed);
        }

        MainHUD->SetInteractionPromptVisible(false);
        return;
    }

    if (CurrentActivity.ActivityType == ETextbookActivityType::Matching)
    {
        EnsureMatchingWidget();
        if (MatchingWidget)
        {
            MatchingWidget->SetOwningUIManager(this);
            MatchingWidget->SetPromptText(CurrentActivity.PromptText);
            MatchingWidget->SetMatchingPairs(CurrentActivity.MatchingPairs);
            if (!MatchingWidget->GetParent())
            {
                ApplyPanelSlotLayout(MatchingWidget, MainHUD->TextbookLayer);
            }
            MatchingWidget->SetVisibility(ESlateVisibility::Visible);
            MatchingWidget->SetIsFocusable(true);
        }
        HideQuestionUI();
        HideOrderingUI();
        HideFeedbackUI();
        HideTeachUI();
        if (DialogueWidget)
        {
            DialogueWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
        MainHUD->SetInteractionPromptVisible(false);
        return;
    }

    if (!QuestionWidget)
    {
        return;
    }

    if (CurrentActivity.GetEffectiveActivityID().IsEmpty() || CurrentActivity.Question.QuestionText.IsEmpty())
    {
        HideQuestionUI();
        return;
    }

    if (!QuestionWidget->GetParent())
    {
        ApplyPanelSlotLayout(QuestionWidget, MainHUD->TextbookLayer);
    }

    QuestionWidget->SetMultiChoiceEnabled(CurrentActivity.ActivityType == ETextbookActivityType::MultiChoice);
    QuestionWidget->SetQuestionData(CurrentActivity.Question);
    QuestionWidget->SetOwningUIManager(this);
    QuestionWidget->SetVisibility(ESlateVisibility::Visible);
    QuestionWidget->SetIsFocusable(true);

    if (DialogueWidget)
    {
        DialogueWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (OrderingWidget)
    {
        OrderingWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (MatchingWidget)
    {
        MatchingWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (FeedbackWidget)
    {
        FeedbackWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (TeachWidget)
    {
        TeachWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    HideObservationUI();

    MainHUD->SetInteractionPromptVisible(false);
}

void UVHVUIManagerComponent::RefreshCurrentHintUI()
{
    EnsureFeedbackWidget();

    if (!MainHUD || !MainHUD->TextbookLayer || !TextbookSubsystem || !FeedbackWidget || !TextbookSubsystem->IsActivityActive())
    {
        return;
    }

    const FTextbookActivityData CurrentActivity = TextbookSubsystem->GetCurrentActivity();
    const FTextbookRuntimeState RuntimeState = TextbookSubsystem->GetRuntimeState();
    FString HintText;
    if (CurrentActivity.AttemptPolicy.bEnabled
        && !CurrentActivity.AttemptPolicy.FirstIncorrectHint.IsEmpty())
    {
        HintText = CurrentActivity.AttemptPolicy.FirstIncorrectHint;
    }
    else if (CurrentActivity.Hints.Num() > 0)
    {
        const int32 HintIndex = FMath::Clamp(RuntimeState.HintLevel - 1, 0, CurrentActivity.Hints.Num() - 1);
        HintText = CurrentActivity.Hints[HintIndex];
    }

    if (!FeedbackWidget->GetParent())
    {
        ApplyPanelSlotLayout(FeedbackWidget, MainHUD->TextbookLayer);
    }

    FeedbackWidget->SetFeedbackPresentation(
        FText::FromString(HintText), UVHVFeedbackWidget::ETone::Neutral, true);
    FeedbackWidget->SetVisibility(ESlateVisibility::Visible);
    MainHUD->SetInteractionPromptVisible(false);
}

void UVHVUIManagerComponent::RefreshCurrentFeedbackUI()
{
    EnsureFeedbackWidget();

    if (!MainHUD || !MainHUD->TextbookLayer || !TextbookSubsystem || !FeedbackWidget)
    {
        return;
    }

    if (!TextbookSubsystem->IsActivityActive())
    {
        HideFeedbackUI();
        return;
    }

    const FTextbookActivityData CurrentActivity = TextbookSubsystem->GetCurrentActivity();
    if (CurrentActivity.GetEffectiveActivityID().IsEmpty())
    {
        HideFeedbackUI();
        return;
    }

    if (!FeedbackWidget->GetParent())
    {
        ApplyPanelSlotLayout(FeedbackWidget, MainHUD->TextbookLayer);
    }

    const FTextbookRuntimeState RuntimeState = TextbookSubsystem->GetRuntimeState();
    const FString FeedbackText = CurrentActivity.GetFeedbackTextForResult(
        RuntimeState.bAnswerCorrect, RuntimeState.bAnswerPartial);
    if (FeedbackText.TrimStartAndEnd().IsEmpty())
    {
        // Imported or legacy runtime state may still point at Feedback. Collapse
        // it through the same safe Teach/completion path instead of showing an
        // empty result panel.
        HideFeedbackUI();
        TextbookSubsystem->EnterTeachPhase();
        return;
    }

    FeedbackWidget->SetFeedbackPresentation(
        FText::FromString(FeedbackText),
        RuntimeState.bAnswerCorrect ? UVHVFeedbackWidget::ETone::Positive : UVHVFeedbackWidget::ETone::Caution);
    FeedbackWidget->SetVisibility(ESlateVisibility::Visible);
    MainHUD->SetInteractionPromptVisible(false);
}

void UVHVUIManagerComponent::HideQuestionUI()
{
    if (QuestionWidget)
    {
        QuestionWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UVHVUIManagerComponent::HideFeedbackUI()
{
    if (FeedbackWidget)
    {
        FeedbackWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UVHVUIManagerComponent::HideTeachUI()
{
    if (TeachWidget)
    {
        TeachWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UVHVUIManagerComponent::HideObservationUI()
{
    if (ObservationWidget)
    {
        ObservationWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UVHVUIManagerComponent::HideOrderingUI()
{
    if (OrderingWidget)
    {
        OrderingWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UVHVUIManagerComponent::HideMatchingUI()
{
    if (MatchingWidget)
    {
        MatchingWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UVHVUIManagerComponent::SubmitOrderingAnswer(const TArray<FString>& OrderedItemIDs)
{
    if (!TextbookSubsystem)
    {
        return;
    }

    TextbookSubsystem->SubmitOrdering(OrderedItemIDs);
}

void UVHVUIManagerComponent::SubmitMatchingAnswer(const TArray<FMatchingPair>& SubmittedMatches)
{
    if (TextbookSubsystem)
    {
        TextbookSubsystem->SubmitMatching(SubmittedMatches);
    }
}

bool UVHVUIManagerComponent::SubmitObservation()
{
    return TextbookSubsystem && TextbookSubsystem->SubmitObservation();
}

void UVHVUIManagerComponent::HideSupportNetworkUI()
{
    if (SupportNetworkWidget)
    {
        SupportNetworkWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

bool UVHVUIManagerComponent::ShowMajorQuestStinger(
    const FVHVMajorQuestStingerData& StingerData)
{
    return QueueMajorQuestStinger(StingerData);
}

bool UVHVUIManagerComponent::IsQuestTrackerSuppressedForMajorStinger() const
{
    return QuestTrackerWidget && QuestTrackerWidget->IsMajorStingerSuppressed();
}

bool UVHVUIManagerComponent::QueueMajorQuestStinger(
    const FVHVMajorQuestStingerData& StingerData,
    const bool bCompletesLearningActivity)
{
    if (!StingerData.IsConfigured() || !MainHUD)
    {
        return false;
    }

    FQueuedMajorStinger& Queued = PendingMajorStingers.AddDefaulted_GetRef();
    Queued.Data = StingerData;
    Queued.bCompletesLearningActivity = bCompletesLearningActivity;

    if (bMajorStingerSequenceActive)
    {
        UE_LOG(LogVHV, Log, TEXT("[VHVUI] Queued major stinger '%s' behind the active presentation."),
            *StingerData.Title);
        return true;
    }

    bMajorStingerSequenceActive = true;
    if (QuestTrackerWidget)
    {
        QuestTrackerWidget->BeginMajorStingerSuppression();
    }
    ApplyUIState(EVHVUIState::MajorStinger);
    return PlayNextMajorQuestStinger();
}

bool UVHVUIManagerComponent::PlayNextMajorQuestStinger()
{
    if (!bMajorStingerSequenceActive || PendingMajorStingers.IsEmpty())
    {
        FinishMajorQuestStingerSequence();
        return false;
    }

    if (!MajorQuestStingerWidget)
    {
        APlayerController* PC = Cast<APlayerController>(GetOwner());
        UPanelWidget* StingerLayer = MainHUD->TopLayer
            ? MainHUD->TopLayer.Get()
            : MainHUD->TextbookLayer.Get();
        if (!PC || !StingerLayer)
        {
            PendingMajorStingers.Reset();
            FinishMajorQuestStingerSequence();
            return false;
        }

        MajorQuestStingerWidget = CreateWidget<UVHVMajorQuestStingerWidget>(
            PC, UVHVMajorQuestStingerWidget::StaticClass());
        if (!MajorQuestStingerWidget)
        {
            PendingMajorStingers.Reset();
            FinishMajorQuestStingerSequence();
            return false;
        }

        ApplyPanelSlotLayout(MajorQuestStingerWidget, StingerLayer);
        MajorQuestStingerWidget->OnStingerFinished.AddUObject(
            this, &UVHVUIManagerComponent::HandleMajorQuestStingerFinished);
        MajorQuestStingerWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (MajorQuestStingerWidget->IsPlaying())
    {
        return false;
    }

    FQueuedMajorStinger Queued = MoveTemp(PendingMajorStingers[0]);
    PendingMajorStingers.RemoveAt(0);
    bCurrentMajorStingerCompletesActivity = Queued.bCompletesLearningActivity;
    UE_LOG(LogVHV, Log, TEXT("[VHVUI] Major stinger begin: '%s'."), *Queued.Data.Title);
    MajorQuestStingerWidget->ShowStinger(Queued.Data);
    if (!MajorQuestStingerWidget->IsPlaying())
    {
        bCurrentMajorStingerCompletesActivity = false;
        PendingMajorStingers.Reset();
        FinishMajorQuestStingerSequence();
        return false;
    }
    return true;
}

void UVHVUIManagerComponent::HandleMajorQuestStingerFinished()
{
    UE_LOG(LogVHV, Log, TEXT("[VHVUI] Major stinger complete."));
    const bool bCompleteActivity = bCurrentMajorStingerCompletesActivity;
    bCurrentMajorStingerCompletesActivity = false;

    if (bCompleteActivity && TextbookSubsystem && TextbookSubsystem->IsActivityActive())
    {
        TextbookSubsystem->AdvanceToNextActivity();
    }

    if (!PendingMajorStingers.IsEmpty())
    {
        PlayNextMajorQuestStinger();
        return;
    }

    FinishMajorQuestStingerSequence();
}

void UVHVUIManagerComponent::FinishMajorQuestStingerSequence()
{
    if (!bMajorStingerSequenceActive)
    {
        return;
    }

    bMajorStingerSequenceActive = false;
    bCurrentMajorStingerCompletesActivity = false;
    PendingMajorStingers.Reset();

    if (QuestTrackerWidget)
    {
        QuestTrackerWidget->EndMajorStingerSuppressionAndReveal();
    }

    ReconcileUIStateAfterMajorStinger();
    UE_LOG(LogVHV, Log, TEXT("[VHVUI] Major stinger sequence complete; tracker and input ownership reconciled."));
}

void UVHVUIManagerComponent::ReconcileUIStateAfterMajorStinger()
{
    if (bConversationActive)
    {
        ApplyUIState(EVHVUIState::Dialogue);
        return;
    }

    if (TextbookSubsystem && TextbookSubsystem->IsActivityActive())
    {
        switch (TextbookSubsystem->GetCurrentPhase())
        {
        case ELearningPhase::Ask:
            ApplyUIState(EVHVUIState::LearningAsk);
            return;
        case ELearningPhase::Hint:
            ApplyUIState(EVHVUIState::LearningHint);
            return;
        case ELearningPhase::Feedback:
            ApplyUIState(EVHVUIState::LearningFeedback);
            return;
        case ELearningPhase::Teach:
            ApplyUIState(EVHVUIState::LearningTeach);
            return;
        default:
            break;
        }
    }

    ApplyUIState(EVHVUIState::Gameplay);
    EndConversationSession();
}

void UVHVUIManagerComponent::HandleQuestStarted(const FName QuestID)
{
    ClearObjectiveTracking(true);
    FVHVQuestDefinition Quest;
    if (QuestSubsystem && QuestSubsystem->GetQuestDefinition(QuestID, Quest)
        && Quest.StartStinger.IsConfigured())
    {
        QueueMajorQuestStinger(Quest.StartStinger);
    }
}

void UVHVUIManagerComponent::HandleQuestCompleted(const FName QuestID)
{
    ClearObjectiveTracking(true);
    FVHVQuestDefinition Quest;
    if (QuestSubsystem && QuestSubsystem->GetQuestDefinition(QuestID, Quest)
        && Quest.CompletionStinger.IsConfigured())
    {
        QueueMajorQuestStinger(Quest.CompletionStinger);
    }
}

void UVHVUIManagerComponent::RefreshCurrentTeachUI()
{
    if (!MainHUD || !MainHUD->TextbookLayer || !TextbookSubsystem || !TeachWidget)
    {
        return;
    }

    if (!TextbookSubsystem->IsActivityActive())
    {
        HideTeachUI();
        return;
    }

    const FTextbookActivityData CurrentActivity = TextbookSubsystem->GetCurrentActivity();

    if (!TeachWidget->GetParent())
    {
        ApplyPanelSlotLayout(TeachWidget, MainHUD->TextbookLayer);
    }

    TeachWidget->SetOwningUIManager(this);
    TeachWidget->SetTeachingContent(CurrentActivity.Teaching);
    TeachWidget->SetVisibility(ESlateVisibility::Visible);
    MainHUD->SetInteractionPromptVisible(false);
}

void UVHVUIManagerComponent::AdvanceFeedback()
{
    if (!TextbookSubsystem || !TextbookSubsystem->IsActivityActive())
    {
        return;
    }

    const FTextbookRuntimeState RuntimeState = TextbookSubsystem->GetRuntimeState();
    if (!RuntimeState.bAnswerSubmitted)
    {
        return;
    }

    const FTextbookActivityData CurrentActivity = TextbookSubsystem->GetCurrentActivity();
    if (!CurrentActivity.AttemptPolicy.bEnabled
        && CurrentActivity.bRequireCorrectAnswerToAdvance
        && !RuntimeState.bAnswerCorrect
        && RuntimeState.AttemptCount < 3)
    {
        TextbookSubsystem->EnterAskPhase();
        HideFeedbackUI();
        return;
    }

    TextbookSubsystem->EnterTeachPhase();
    HideFeedbackUI();
    RefreshInteractionPrompt();
}

void UVHVUIManagerComponent::AdvanceHint()
{
    if (!TextbookSubsystem || !TextbookSubsystem->IsActivityActive() || TextbookSubsystem->GetCurrentPhase() != ELearningPhase::Hint)
    {
        return;
    }

    TextbookSubsystem->RetryCurrentActivityAfterHint();
    HideFeedbackUI();
}

void UVHVUIManagerComponent::AdvanceTeach()
{
    if (!TextbookSubsystem || !TextbookSubsystem->IsActivityActive())
    {
        return;
    }

    const FTextbookRuntimeState RuntimeState = TextbookSubsystem->GetRuntimeState();
    if (RuntimeState.CurrentPhase != ELearningPhase::Teach)
    {
        return;
    }

    const FTextbookActivityData CurrentActivity = TextbookSubsystem->GetCurrentActivity();
    if (TeachWidget && IsContinuousTeachingPage(CurrentActivity))
    {
        if (TeachWidget->IsContentTransitionActive())
        {
            return;
        }
        if (TeachWidget->BeginContentTransition(
            FSimpleDelegate::CreateUObject(this, &UVHVUIManagerComponent::CompleteTeachingAdvanceAfterFade)))
        {
            return;
        }
    }

    const bool bQuestManaged = TextbookSubsystem->GetProgressionMode() == EVHVTextbookProgressionMode::QuestManaged;
    TextbookSubsystem->AdvanceToNextActivity();
    HideTeachUI();
    if (bQuestManaged && CurrentUIState == EVHVUIState::LearningTeach && !TextbookSubsystem->IsActivityActive())
    {
        RestoreGameplayAfterQuestModalIfNeeded();
    }
    RefreshInteractionPrompt();
}

void UVHVUIManagerComponent::CompleteTeachingAdvanceAfterFade()
{
    if (!TextbookSubsystem || !TextbookSubsystem->IsActivityActive())
    {
        if (TeachWidget)
        {
            TeachWidget->CancelContentTransition();
        }
        HideTeachUI();
        return;
    }

    const bool bQuestManaged = TextbookSubsystem->GetProgressionMode() == EVHVTextbookProgressionMode::QuestManaged;
    bAwaitingContinuousTeachingResult = true;
    TextbookSubsystem->AdvanceToNextActivity();

    const bool bContinues = TextbookSubsystem->IsActivityActive()
        && TextbookSubsystem->GetCurrentPhase() == ELearningPhase::Teach
        && IsContinuousTeachingPage(TextbookSubsystem->GetCurrentActivity());
    bAwaitingContinuousTeachingResult = false;

    if (!bContinues)
    {
        if (!bConversationActive)
        {
            bQuestManagedLearningActivityActive = false;
        }
        if (TeachWidget)
        {
            TeachWidget->CancelContentTransition();
        }
        HideTeachUI();
        if (bQuestManaged && CurrentUIState == EVHVUIState::LearningTeach
            && !TextbookSubsystem->IsActivityActive())
        {
            RestoreGameplayAfterQuestModalIfNeeded();
        }
    }

    RefreshInteractionPrompt();
}

void UVHVUIManagerComponent::SetMovementLocked(bool bLocked)
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC)
    {
        return;
    }

    // Ignore input uses stacked counters. Reset first so every UI transition
    // establishes one absolute lock state and Gameplay is always idempotent.
    PC->ResetIgnoreMoveInput();
    PC->ResetIgnoreLookInput();
    if (bLocked)
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
    }

    APawn* Pawn = PC->GetPawn();
    if (!Pawn)
    {
        return;
    }

    ACharacter* Character = Cast<ACharacter>(Pawn);
    if (!Character)
    {
        return;
    }

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (!Movement)
    {
        return;
    }

    if (bLocked)
    {
        Movement->DisableMovement();
    }
    else
    {
        Movement->SetMovementMode(MOVE_Walking);
    }
}

void UVHVUIManagerComponent::CommitCurrentConversationState()
{
    if (!bConversationActive || ActiveConversation.ConversationID.IsEmpty() || !DialogueWidget)
    {
        return;
    }

    ActiveConversationState.ConversationID = ActiveConversation.ConversationID;
    ActiveConversationState.CurrentNodeID = DialogueWidget->GetCurrentNodeID();
    ActiveConversationState.bCompleted = ActiveConversationState.CurrentNodeID.IsEmpty();

    const FDialogueNode* CurrentNode = FindNodeByID(ActiveConversation.ConversationID, ActiveConversationState.CurrentNodeID);
    if (CurrentNode && CurrentNode->bIsCheckpoint && !CurrentNode->GetEffectiveCheckpointID().IsEmpty())
    {
        ActiveConversationState.LatestCheckpointID = CurrentNode->GetEffectiveCheckpointID();
    }

    ConversationStates.Add(ActiveConversation.ConversationID, ActiveConversationState);
}

const FDialogueNode* UVHVUIManagerComponent::FindNodeByID(const FString& ConversationID, const FString& NodeID) const
{
    for (const FDialogueNode& Node : ActiveConversation.Nodes)
    {
        if (Node.NodeID == NodeID)
        {
            return &Node;
        }
    }

    return nullptr;
}

FDialogueNode* UVHVUIManagerComponent::FindNodeByIDMutable(const FString& ConversationID, const FString& NodeID)
{
    for (FDialogueNode& Node : ActiveConversation.Nodes)
    {
        if (Node.NodeID == NodeID)
        {
            return &Node;
        }
    }

    return nullptr;
}

bool UVHVUIManagerComponent::StartConversation(const FDialogueConversation& InConversation)
{
    return StartConversationInternal(InConversation, FString());
}

bool UVHVUIManagerComponent::StartConversationAtNode(const FDialogueConversation& InConversation, const FString& NodeID)
{
    if (NodeID.IsEmpty())
    {
        return false;
    }

    return StartConversationInternal(InConversation, NodeID);
}

bool UVHVUIManagerComponent::StartConversationInternal(const FDialogueConversation& InConversation, const FString& ExplicitStartNodeID)
{
    if (!MainHUD || !DialogueWidget || !MainHUD->DialogueLayer)
    {
        return false;
    }

    FString StartingNodeID;
    if (!ResolveAvailableConversationStartNode(InConversation, ExplicitStartNodeID, StartingNodeID))
    {
        return false;
    }

    ActiveConversation = InConversation;
    ActiveConversationState = ConversationStates.Contains(InConversation.ConversationID) ? ConversationStates[InConversation.ConversationID] : FConversationRuntimeState();
    ActiveConversationState.ConversationID = InConversation.ConversationID;

    DialogueWidget->SetOwningUIManager(this);

    if (!DialogueWidget->GetParent())
    {
        MainHUD->DialogueLayer->AddChild(DialogueWidget);

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DialogueWidget->Slot);
        if (CanvasSlot)
        {
            CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            CanvasSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
            CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
        }
    }

    bConversationSessionActive = true;
    bConversationActive = true;
    return TraverseToNode(StartingNodeID);
}

bool UVHVUIManagerComponent::StartConversationFromAsset(UVHVConversationDataAsset* ConversationAsset)
{
    if (!ConversationAsset)
    {
        return false;
    }

    return StartConversation(ConversationAsset->Conversation);
}

bool UVHVUIManagerComponent::CanStartConversationFromAsset(const UVHVConversationDataAsset* ConversationAsset) const
{
    if (!ConversationAsset || bConversationSessionActive || !MainHUD || !DialogueWidget || !MainHUD->DialogueLayer)
    {
        return false;
    }

    FString StartingNodeID;
    return ResolveAvailableConversationStartNode(ConversationAsset->Conversation, FString(), StartingNodeID);
}

bool UVHVUIManagerComponent::ResolveAvailableConversationStartNode(
    const FDialogueConversation& InConversation,
    const FString& ExplicitStartNodeID,
    FString& OutStartingNodeID) const
{
    OutStartingNodeID = ExplicitStartNodeID;
    const FConversationRuntimeState* ExistingState = ConversationStates.Find(InConversation.ConversationID);
    if (OutStartingNodeID.IsEmpty())
    {
        OutStartingNodeID = InConversation.StartNodeID;
        if (ExistingState && !ExistingState->LatestCheckpointID.IsEmpty())
        {
            for (const FDialogueNode& Node : InConversation.Nodes)
            {
                if (Node.bIsCheckpoint && Node.GetEffectiveCheckpointID() == ExistingState->LatestCheckpointID)
                {
                    OutStartingNodeID = Node.NodeID;
                    break;
                }
            }
        }
        else if (ExistingState && !ExistingState->CurrentNodeID.IsEmpty())
        {
            OutStartingNodeID = ExistingState->CurrentNodeID;
        }
    }

    if (OutStartingNodeID.IsEmpty() && !InConversation.Nodes.IsEmpty())
    {
        OutStartingNodeID = InConversation.Nodes[0].NodeID;
    }

    TSet<FString> VisitedNodeIDs;
    while (!OutStartingNodeID.IsEmpty() && !VisitedNodeIDs.Contains(OutStartingNodeID))
    {
        VisitedNodeIDs.Add(OutStartingNodeID);
        const FDialogueNode* Node = InConversation.Nodes.FindByPredicate(
            [&OutStartingNodeID](const FDialogueNode& Candidate)
            {
                return Candidate.NodeID == OutStartingNodeID;
            });
        if (!Node)
        {
            return false;
        }

        const bool bConditionsPass = Node->ActivationConditions.Conditions.IsEmpty()
            || (StoryStateSubsystem && StoryStateSubsystem->EvaluateConditionSet(Node->ActivationConditions));
        if (bConditionsPass)
        {
            return true;
        }

        OutStartingNodeID = Node->NextNodeID;
    }

    OutStartingNodeID.Reset();
    return false;
}

void UVHVUIManagerComponent::AdvanceConversation()
{
    if (!bConversationActive || !DialogueWidget)
    {
        return;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(ActiveConversation.ConversationID, ActiveConversationState.CurrentNodeID);
    if (!CurrentNode)
    {
        CompleteConversation();
        return;
    }

    if (CurrentNode->NodeType == EVHVDialogueNodeType::Choice)
    {
        ConfirmChoice();
        return;
    }

    if (CurrentNode->NodeType == EVHVDialogueNodeType::LearningActivity)
    {
        return;
    }

    if (CurrentNode->NextNodeID.IsEmpty())
    {
        ApplyCurrentDialogueNodeCompletionEffects();
        CompleteConversation();
        return;
    }

    ApplyCurrentDialogueNodeCompletionEffects();
    TraverseToNode(CurrentNode->NextNodeID);
}

bool UVHVUIManagerComponent::StartLinkedLearningActivity(const FTextbookActivityReference& Reference)
{
    const FString ActivityID = Reference.GetEffectiveActivityID();
    if (ActivityID.IsEmpty())
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (!World || !World->GetGameInstance())
    {
        return false;
    }

    UVHVTextbookSubsystem* TextbookSubsystemInstance = World->GetGameInstance()->GetSubsystem<UVHVTextbookSubsystem>();
    if (!TextbookSubsystemInstance)
    {
        return false;
    }

    TextbookSubsystem = TextbookSubsystemInstance;
    FTextbookActivityData LinkedActivity;
    const bool bUsesMajorStinger = TextbookSubsystem->TryGetActivityByID(ActivityID, LinkedActivity)
        && LinkedActivity.MajorStinger.IsConfigured();
    bStartingMajorStingerActivity = bUsesMajorStinger;
    const bool bStarted = TextbookSubsystem->StartActivityByID(ActivityID);
    bStartingMajorStingerActivity = false;
    if (bStarted && bUsesMajorStinger)
    {
        if (!QueueMajorQuestStinger(LinkedActivity.MajorStinger, true))
        {
            HandleLearningPhaseChanged(TextbookSubsystem->GetCurrentPhase());
        }
        return bStarted;
    }
    if (bStarted && TextbookSubsystem->GetCurrentPhase() == ELearningPhase::Ask)
    {
        RefreshCurrentAskQuestionUI();
    }
    return bStarted;
}

bool UVHVUIManagerComponent::StartQuestManagedLearningActivity(
    const FTextbookActivityReference& Reference)
{
    if (!TextbookSubsystem || bConversationActive || TextbookSubsystem->IsActivityActive())
    {
        return false;
    }

    TextbookSubsystem->SetProgressionMode(EVHVTextbookProgressionMode::QuestManaged);
    bQuestManagedLearningActivityActive = StartLinkedLearningActivity(Reference);
    return bQuestManagedLearningActivityActive;
}

bool UVHVUIManagerComponent::TrySubmitCurrentQuestionAnswer()
{
    if (!TextbookSubsystem || TextbookSubsystem->GetCurrentPhase() != ELearningPhase::Ask || !QuestionWidget)
    {
        return false;
    }

    const FTextbookActivityData CurrentActivity = TextbookSubsystem->GetCurrentActivity();
    if ((CurrentActivity.ActivityType == ETextbookActivityType::SingleChoice ||
         CurrentActivity.ActivityType == ETextbookActivityType::MultiChoice) &&
        !QuestionWidget->HasSelection())
    {
        QuestionWidget->ShowSelectionRequiredFeedback();
        FocusModalWidget(EVHVUIState::LearningAsk);
        return false;
    }

    if (CurrentActivity.ActivityType == ETextbookActivityType::MultiChoice)
    {
        return TextbookSubsystem->SubmitMultiChoice(QuestionWidget->GetSelectedOptionIndices());
    }

    const int32 SelectedIndex = QuestionWidget->GetSelectedOptionIndex();
    if (!CurrentActivity.Question.Options.IsValidIndex(SelectedIndex))
    {
        return false;
    }

    const bool bWasCorrect = CurrentActivity.Question.Options[SelectedIndex].bIsCorrect;
    TextbookSubsystem->SubmitAnswer(bWasCorrect);
    return true;
}

bool UVHVUIManagerComponent::HandleActivityInteractionInput()
{
    if (CurrentUIState == EVHVUIState::MajorStinger)
    {
        return true;
    }

    if (CurrentUIState == EVHVUIState::Dialogue)
    {
        ConfirmChoiceInput();
        return true;
    }

    if (CurrentUIState != EVHVUIState::LearningAsk || !TextbookSubsystem ||
        TextbookSubsystem->GetCurrentPhase() != ELearningPhase::Ask || !TextbookSubsystem->IsActivityActive())
    {
        // Every modal owns the interaction press even when E has no action on
        // that presentation, so it can never leak through to a world target.
        return CurrentUIState != EVHVUIState::Gameplay;
    }

    switch (TextbookSubsystem->GetCurrentActivity().ActivityType)
    {
    case ETextbookActivityType::SingleChoice:
    case ETextbookActivityType::MultiChoice:
        if (QuestionWidget)
        {
            QuestionWidget->ActivateFocusedOption();
        }
        break;
    case ETextbookActivityType::Ordering:
        if (OrderingWidget)
        {
            OrderingWidget->ToggleFocusedCardGrab();
        }
        break;
    case ETextbookActivityType::Matching:
        if (TextbookSubsystem->GetCurrentActivity().PresentationStyle
                == EVHVActivityPresentationStyle::SocialSupportNetwork
            && SupportNetworkWidget)
        {
            SupportNetworkWidget->ActivateSelectedConnection();
        }
        else if (MatchingWidget)
        {
            MatchingWidget->ActivateFocusedSelection();
        }
        break;
    case ETextbookActivityType::Observation:
        if (ObservationWidget)
        {
            ObservationWidget->ActivateFocusedEvidenceCard();
        }
        break;
    default:
        break;
    }

    // LearningAsk owns interaction while modal, including activities without a
    // select operation, so this press cannot also activate a world target.
    return true;
}

void UVHVUIManagerComponent::ShowSocialSupportObservationProgress(const int32 CompletedCount)
{
    EnsureSocialSupportWidgets();
    if (SocialSupportHUDWidget) SocialSupportHUDWidget->ShowObservationProgress(CompletedCount);
}

void UVHVUIManagerComponent::ShowSocialSupportSupporterProgress(const int32 CompletedCount)
{
    EnsureSocialSupportWidgets();
    if (SocialSupportHUDWidget) SocialSupportHUDWidget->ShowSupporterProgress(CompletedCount);
}

void UVHVUIManagerComponent::HideSocialSupportProgress()
{
    if (SocialSupportHUDWidget) SocialSupportHUDWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void UVHVUIManagerComponent::ShowSupportTypeReveal()
{
    EnsureSocialSupportWidgets();
    if (!SupportTypeOverlayWidget) return;
    SupportTypeOverlayWidget->ShowTypeReveal();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            SupportTypeOverlayTimer, this,
            &UVHVUIManagerComponent::HideSupportTypeOverlay, 5.0f, false);
    }
}

void UVHVUIManagerComponent::HideSupportTypeOverlay()
{
    if (SupportTypeOverlayWidget)
    {
        SupportTypeOverlayWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

bool UVHVUIManagerComponent::IsSupportNetworkSubmissionCorrect(
    const TArray<FMatchingPair>& Matches) const
{
    return TextbookSubsystem && TextbookSubsystem->IsMatchingSubmissionCorrect(Matches);
}

void UVHVUIManagerComponent::CompleteSupportNetworkPlanning(
    const TArray<FMatchingPair>& Matches)
{
    if (TextbookSubsystem && TextbookSubsystem->IsMatchingSubmissionCorrect(Matches))
    {
        TextbookSubsystem->SubmitMatching(Matches);
    }
}

void UVHVUIManagerComponent::CompleteSocialSupportNetworkPayoff()
{
    if (TextbookSubsystem && TextbookSubsystem->IsActivityActive()
        && TextbookSubsystem->GetCurrentActivity().PresentationStyle
            == EVHVActivityPresentationStyle::SocialSupportNetworkReadOnly)
    {
        TextbookSubsystem->CompleteCurrentActivity();
    }
}

bool UVHVUIManagerComponent::CanAdvanceFromBackgroundClick() const
{
    switch (CurrentUIState)
    {
    case EVHVUIState::Dialogue:
    {
        if (!bConversationActive)
        {
            return false;
        }
        const FDialogueNode* CurrentNode = FindNodeByID(
            ActiveConversation.ConversationID, ActiveConversationState.CurrentNodeID);
        return !CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice;
    }
    case EVHVUIState::LearningTeach:
    case EVHVUIState::LearningFeedback:
    case EVHVUIState::LearningHint:
        return true;
    case EVHVUIState::LearningAsk:
        if (!TextbookSubsystem || !TextbookSubsystem->IsActivityActive()
            || TextbookSubsystem->GetCurrentPhase() != ELearningPhase::Ask)
        {
            return false;
        }
        if (TextbookSubsystem->GetCurrentActivity().ActivityType != ETextbookActivityType::Observation)
        {
            return false;
        }
        return !TextbookSubsystem->GetCurrentActivity().EvidenceTagging.bUseEvidenceTagging;
    case EVHVUIState::Gameplay:
    case EVHVUIState::MajorStinger:
    default:
        return false;
    }
}

bool UVHVUIManagerComponent::HandleModalBackgroundClick()
{
    if (CurrentUIState == EVHVUIState::Gameplay)
    {
        return false;
    }

    if (CanAdvanceFromBackgroundClick())
    {
        ConfirmChoiceInput();
    }

    // Interactive activities and stingers still consume the background click.
    // Their authored controls are the only allowed route to a semantic action.
    return true;
}

bool UVHVUIManagerComponent::PrepareForDeveloperObjectiveSkip()
{
#if !UE_BUILD_SHIPPING
    // Do not tear down a queued stinger sequence mid-callback. The shortcut can
    // be pressed again after the current cinematic transition completes.
    if (bMajorStingerSequenceActive)
    {
        return false;
    }

    ClearObjectiveTracking();
    ExitConversation();
    return true;
#else
    return false;
#endif
}

bool UVHVUIManagerComponent::EnsureObjectiveTrackingPresentation()
{
    APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
    UWorld* World = GetWorld();
    if (!PlayerController || !World)
    {
        return false;
    }

    if (!ObjectiveDirectionWidget)
    {
        ObjectiveDirectionWidget = CreateWidget<UVHVObjectiveDirectionWidget>(
            PlayerController, UVHVObjectiveDirectionWidget::StaticClass());
        if (ObjectiveDirectionWidget)
        {
            ObjectiveDirectionWidget->AddToPlayerScreen(40);
            ObjectiveDirectionWidget->HideDirection();
        }
    }

    if (!ObjectiveTrackingMarker)
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Name = TEXT("VHV_ObjectiveTrackingMarker");
        SpawnParameters.ObjectFlags |= RF_Transient;
        SpawnParameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ObjectiveTrackingMarker = World->SpawnActor<AVHVObjectiveTrackingMarker>(
            AVHVObjectiveTrackingMarker::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    }

    return ObjectiveTrackingMarker && ObjectiveDirectionWidget;
}

bool UVHVUIManagerComponent::RefreshActiveObjectiveTrackingTarget(
    const FName QuestID,
    const FName ObjectiveID)
{
    FName ResolvedQuestID;
    FName ResolvedObjectiveID;
    FName LocationID;
    FVector WorldLocation;
    AActor* TargetActor = nullptr;
    if (!QuestSubsystem || !QuestSubsystem->ResolveCurrentObjectiveTrackingTarget(
            GetWorld(), ResolvedQuestID, ResolvedObjectiveID, LocationID, WorldLocation, TargetActor)
        || ResolvedQuestID != QuestID || ResolvedObjectiveID != ObjectiveID)
    {
        ClearObjectiveTracking();
        return false;
    }

    TrackedLocationQuestID = ResolvedQuestID;
    TrackedLocationObjectiveID = ResolvedObjectiveID;
    TrackedLocationID = LocationID;
    TrackedLocation = WorldLocation;
    TrackedLocationActor = TargetActor;
    TrackedLocationVolume = Cast<AVHVQuestLocationVolume>(TargetActor);
    FVHVQuestObjectiveDefinition Objective;
    TrackedParticipantID = QuestSubsystem->GetCurrentObjective(Objective)
        ? Objective.GetEffectiveParticipantID()
        : NAME_None;
    if (ObjectiveTrackingMarker)
    {
        ObjectiveTrackingMarker->SetTrackingLocation(TrackedLocation);
    }

    bObjectiveTrackingProximitySuppressed = false;
    bObjectiveTrackingTemporaryReveal = false;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ObjectiveTrackingTemporaryRevealTimer);
    }
    UpdateObjectiveTrackingProximity();
    ApplyObjectiveTrackingVisualState();
    UE_LOG(LogVHV, Log,
        TEXT("[VHVTracking] Objective '%s' advanced to semantic destination '%s'."),
        *ObjectiveID.ToString(), *LocationID.ToString());
    return true;
}

void UVHVUIManagerComponent::ToggleObjectiveTracking()
{
    if (CurrentUIState != EVHVUIState::Gameplay)
    {
        return;
    }

    if (ObjectiveTrackingMode == EVHVObjectiveTrackingMode::Off)
    {
        FName QuestID;
        FName ObjectiveID;
        FName LocationID;
        FVector WorldLocation;
        AActor* TargetActor = nullptr;
        if (!QuestSubsystem || !QuestSubsystem->ResolveCurrentObjectiveTrackingTarget(
            GetWorld(), QuestID, ObjectiveID, LocationID, WorldLocation, TargetActor))
        {
            UE_LOG(LogVHV, Log,
                TEXT("[VHVTracking] Current objective has no trackable location."));
            return;
        }
        if (!EnsureObjectiveTrackingPresentation())
        {
            UE_LOG(LogVHV, Warning,
                TEXT("[VHVTracking] Could not create objective tracking presentation."));
            return;
        }

        TrackedLocationQuestID = QuestID;
        TrackedLocationObjectiveID = ObjectiveID;
        TrackedLocationID = LocationID;
        TrackedLocation = WorldLocation;
        TrackedLocationActor = TargetActor;
        TrackedLocationVolume = Cast<AVHVQuestLocationVolume>(TargetActor);
        FVHVQuestObjectiveDefinition Objective;
        TrackedParticipantID = QuestSubsystem->GetCurrentObjective(Objective)
            ? Objective.GetEffectiveParticipantID()
            : NAME_None;
        ObjectiveTrackingMarker->SetTrackingLocation(TrackedLocation);
        ObjectiveTrackingMode = EVHVObjectiveTrackingMode::WorldMarker;
        bObjectiveTrackingProximitySuppressed = false;
        bObjectiveTrackingTemporaryReveal = false;
        GetWorld()->GetTimerManager().SetTimer(
            ObjectiveTrackingProximityTimer,
            this,
            &UVHVUIManagerComponent::UpdateObjectiveTrackingProximity,
            ObjectiveTrackingProximityCheckInterval,
            true);
        UpdateObjectiveTrackingProximity();
        ApplyObjectiveTrackingVisualState();
        UE_LOG(LogVHV, Log,
            TEXT("[VHVTracking] Tracking objective '%s' at '%s'; mode=WorldMarker."),
            *TrackedLocationObjectiveID.ToString(), *TrackedLocationID.ToString());
        return;
    }

    if (!bObjectiveTrackingProximitySuppressed && IsPlayerAtObjectiveTrackingTarget())
    {
        bObjectiveTrackingProximitySuppressed = true;
        ApplyObjectiveTrackingVisualState();
    }
    if (bObjectiveTrackingProximitySuppressed)
    {
        BeginObjectiveTrackingTemporaryReveal();
        return;
    }

    if (ObjectiveTrackingMode == EVHVObjectiveTrackingMode::WorldMarker)
    {
        ObjectiveTrackingMode = EVHVObjectiveTrackingMode::WorldMarkerAndDirection;
        ApplyObjectiveTrackingVisualState();
        UE_LOG(LogVHV, Log,
            TEXT("[VHVTracking] mode=WorldMarkerAndDirection."));
        return;
    }

    ClearObjectiveTracking();
    UE_LOG(LogVHV, Log, TEXT("[VHVTracking] Tracking disabled."));
}

bool UVHVUIManagerComponent::IsPlayerAtObjectiveTrackingTarget() const
{
    const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
    const APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    if (!PlayerPawn)
    {
        return false;
    }

    if (const AVHVQuestLocationVolume* Volume = TrackedLocationVolume.Get())
    {
        return Volume->BoxComponent
            && Volume->BoxComponent->Bounds.GetBox().IsInsideOrOn(PlayerPawn->GetActorLocation());
    }

    if (!TrackedParticipantID.IsNone() && CurrentInteractionTarget
        && CurrentInteractionTarget->CanInteract())
    {
        const AActor* InteractionOwner = CurrentInteractionTarget->GetOwner();
        const UVHVQuestParticipantComponent* Participant = InteractionOwner
            ? InteractionOwner->FindComponentByClass<UVHVQuestParticipantComponent>()
            : nullptr;
        if (Participant && Participant->GetEffectiveParticipantID() == TrackedParticipantID)
        {
            return true;
        }
    }

    return FVector::DistSquared(PlayerPawn->GetActorLocation(), TrackedLocation)
        <= FMath::Square(ObjectiveTrackingFallbackArrivalDistance);
}

void UVHVUIManagerComponent::UpdateObjectiveTrackingProximity()
{
    if (ObjectiveTrackingMode == EVHVObjectiveTrackingMode::Off)
    {
        return;
    }

    const bool bArrived = IsPlayerAtObjectiveTrackingTarget();
    if (bObjectiveTrackingProximitySuppressed == bArrived)
    {
        return;
    }

    bObjectiveTrackingProximitySuppressed = bArrived;
    if (!bArrived)
    {
        bObjectiveTrackingTemporaryReveal = false;
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(ObjectiveTrackingTemporaryRevealTimer);
        }
    }
    ApplyObjectiveTrackingVisualState();
}

void UVHVUIManagerComponent::ApplyObjectiveTrackingVisualState()
{
    const bool bTracking = ObjectiveTrackingMode != EVHVObjectiveTrackingMode::Off;
    const bool bSuppressed = bObjectiveTrackingProximitySuppressed
        && !bObjectiveTrackingTemporaryReveal;
    const bool bShowMarker = bTracking && !bSuppressed;
    const bool bShowDirection = bShowMarker
        && ObjectiveTrackingMode == EVHVObjectiveTrackingMode::WorldMarkerAndDirection;

    if (ObjectiveTrackingMarker)
    {
        ObjectiveTrackingMarker->SetTrackingVisible(bShowMarker);
    }
    if (ObjectiveDirectionWidget)
    {
        if (bShowDirection)
        {
            ObjectiveDirectionWidget->ShowDirectionTo(TrackedLocation);
        }
        else
        {
            ObjectiveDirectionWidget->HideDirection();
        }
    }
}

void UVHVUIManagerComponent::BeginObjectiveTrackingTemporaryReveal()
{
    UWorld* World = GetWorld();
    if (!World || ObjectiveTrackingMode == EVHVObjectiveTrackingMode::Off
        || !bObjectiveTrackingProximitySuppressed)
    {
        return;
    }

    bObjectiveTrackingTemporaryReveal = true;
    ApplyObjectiveTrackingVisualState();
    World->GetTimerManager().SetTimer(
        ObjectiveTrackingTemporaryRevealTimer,
        this,
        &UVHVUIManagerComponent::EndObjectiveTrackingTemporaryReveal,
        ObjectiveTrackingTemporaryRevealDuration,
        false);
    UE_LOG(LogVHV, Log,
        TEXT("[VHVTracking] Temporarily revealing arrived target for %.1f seconds."),
        ObjectiveTrackingTemporaryRevealDuration);
}

void UVHVUIManagerComponent::EndObjectiveTrackingTemporaryReveal()
{
    bObjectiveTrackingTemporaryReveal = false;
    UpdateObjectiveTrackingProximity();
    ApplyObjectiveTrackingVisualState();
}

void UVHVUIManagerComponent::ClearObjectiveTracking(const bool bObjectiveChanged)
{
    const bool bWasTracking = ObjectiveTrackingMode != EVHVObjectiveTrackingMode::Off;
    ObjectiveTrackingMode = EVHVObjectiveTrackingMode::Off;
    TrackedLocationQuestID = NAME_None;
    TrackedLocationObjectiveID = NAME_None;
    TrackedLocationID = NAME_None;
    TrackedParticipantID = NAME_None;
    TrackedLocation = FVector::ZeroVector;
    TrackedLocationActor.Reset();
    TrackedLocationVolume.Reset();
    bObjectiveTrackingProximitySuppressed = false;
    bObjectiveTrackingTemporaryReveal = false;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ObjectiveTrackingProximityTimer);
        World->GetTimerManager().ClearTimer(ObjectiveTrackingTemporaryRevealTimer);
    }
    ApplyObjectiveTrackingVisualState();
    if (bObjectiveChanged && bWasTracking)
    {
        UE_LOG(LogVHV, Log,
            TEXT("[VHVTracking] Objective changed; clearing previous tracking target."));
    }
}

void UVHVUIManagerComponent::SelectNextChoice()
{
    if (DialogueWidget)
    {
        DialogueWidget->SelectNextChoice();
    }
}

void UVHVUIManagerComponent::SelectPreviousChoice()
{
    if (DialogueWidget)
    {
        DialogueWidget->SelectPreviousChoice();
    }
}

bool UVHVUIManagerComponent::ConfirmChoice()
{
    if (!bConversationActive || !DialogueWidget)
    {
        return false;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(ActiveConversation.ConversationID, ActiveConversationState.CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice)
    {
        return false;
    }

    const int32 SelectedChoiceIndex = DialogueWidget->GetSelectedChoiceIndex();
    if (!CurrentNode->Choices.IsValidIndex(SelectedChoiceIndex))
    {
        return false;
    }

    const FDialogueChoiceOption& SelectedOption = CurrentNode->Choices[SelectedChoiceIndex];
    if (!IsDialogueChoiceAvailable(SelectedOption))
    {
        return false;
    }

    UE_LOG(LogTemp, Log, TEXT("[VHVDialogue] Choice selected OptionID=%s NextNodeID=%s"), *SelectedOption.OptionID, *SelectedOption.NextNodeID);

    ApplyCurrentDialogueNodeCompletionEffects(&SelectedOption.SelectionEffects);

    if (TextbookSubsystem && TextbookSubsystem->IsActivityActive())
    {
        const FTextbookActivityData CurrentActivity = TextbookSubsystem->GetCurrentActivity();
        if (CurrentActivity.ActivityType == ETextbookActivityType::DialogueChoice
            && CurrentActivity.DialogueChoice.ConversationID == ActiveConversation.ConversationID
            && CurrentActivity.DialogueChoice.ChoiceNodeID == CurrentNode->NodeID)
        {
            TextbookSubsystem->CompleteCurrentActivity();
        }
    }

    if (SelectedOption.NextNodeID.IsEmpty())
    {
        CompleteConversation();
        return true;
    }

    return TraverseToNode(SelectedOption.NextNodeID, true);
}

bool UVHVUIManagerComponent::IsDialogueChoiceAvailable(const FDialogueChoiceOption& Choice) const
{
    if (Choice.AvailabilityConditions.Conditions.IsEmpty())
    {
        return true;
    }

    return StoryStateSubsystem && StoryStateSubsystem->EvaluateConditionSet(Choice.AvailabilityConditions);
}

bool UVHVUIManagerComponent::IsDialogueChoiceLearningActivity(
    const FString& ConversationID,
    const FString& ChoiceNodeID) const
{
    if (!TextbookSubsystem || !TextbookSubsystem->IsActivityActive()
        || TextbookSubsystem->GetCurrentPhase() != ELearningPhase::Ask)
    {
        return false;
    }

    const FTextbookActivityData CurrentActivity = TextbookSubsystem->GetCurrentActivity();
    return CurrentActivity.ActivityType == ETextbookActivityType::DialogueChoice
        && CurrentActivity.DialogueChoice.ConversationID == ConversationID
        && CurrentActivity.DialogueChoice.ChoiceNodeID == ChoiceNodeID;
}

bool UVHVUIManagerComponent::StartDialogueChoiceActivity(const FDialogueChoiceActivityReference& Reference)
{
    if (Reference.ConversationID.IsEmpty() || Reference.ChoiceNodeID.IsEmpty())
    {
        return false;
    }

    if (bConversationActive && ActiveConversation.ConversationID == Reference.ConversationID)
    {
        const FDialogueNode* ChoiceNode = FindNodeByID(Reference.ConversationID, Reference.ChoiceNodeID);
        return ChoiceNode && ChoiceNode->NodeType == EVHVDialogueNodeType::Choice && TraverseToNode(Reference.ChoiceNodeID);
    }

    const FPrimaryAssetId ConversationAssetID(FPrimaryAssetType(TEXT("VHVConversation")), FName(*Reference.ConversationID));
    const FSoftObjectPath ConversationAssetPath = UAssetManager::Get().GetPrimaryAssetPath(ConversationAssetID);
    UVHVConversationDataAsset* ConversationAsset = Cast<UVHVConversationDataAsset>(ConversationAssetPath.TryLoad());
    if (!ConversationAsset)
    {
        return false;
    }

    const FDialogueNode* ChoiceNode = ConversationAsset->Conversation.Nodes.FindByPredicate(
        [&Reference](const FDialogueNode& Node)
        {
            return Node.NodeID == Reference.ChoiceNodeID && Node.NodeType == EVHVDialogueNodeType::Choice;
        });
    return ChoiceNode && StartConversationAtNode(ConversationAsset->Conversation, Reference.ChoiceNodeID);
}

bool UVHVUIManagerComponent::TraverseToNode(const FString& NodeID, bool bLogBranchEntry)
{
    if (!bConversationActive || !DialogueWidget || NodeID.IsEmpty())
    {
        return false;
    }

    FString TargetNodeID = NodeID;
    const FDialogueNode* TargetNode = nullptr;
    TSet<FString> VisitedNodeIDs;

    while (!TargetNodeID.IsEmpty())
    {
        if (VisitedNodeIDs.Contains(TargetNodeID))
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVDialogue] Conversation '%s' stopped while skipping nodes: cycle detected at node '%s'."),
                *ActiveConversation.ConversationID, *TargetNodeID);
            return false;
        }
        VisitedNodeIDs.Add(TargetNodeID);

        TargetNode = FindNodeByID(ActiveConversation.ConversationID, TargetNodeID);
        if (!TargetNode)
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVDialogue] Conversation '%s' cannot traverse to missing node '%s'."),
                *ActiveConversation.ConversationID, *TargetNodeID);
            return false;
        }

        const bool bConditionsPass = TargetNode->ActivationConditions.Conditions.IsEmpty()
            || (StoryStateSubsystem && StoryStateSubsystem->EvaluateConditionSet(TargetNode->ActivationConditions));
        if (bConditionsPass)
        {
            break;
        }

        UE_LOG(LogVHV, Log, TEXT("[VHVDialogue] Conversation '%s' skipped node '%s' because its activation conditions failed."),
            *ActiveConversation.ConversationID, *TargetNodeID);
        TargetNodeID = TargetNode->NextNodeID;
        TargetNode = nullptr;
    }

    if (!TargetNode)
    {
        UE_LOG(LogVHV, Log, TEXT("[VHVDialogue] Conversation '%s' completed after conditional node skipping reached the end of the traversal."),
            *ActiveConversation.ConversationID);
        CompleteConversation();
        return true;
    }

    ApplyUIState(EVHVUIState::Dialogue);
    DialogueWidget->ShowConversation(ActiveConversation, TargetNodeID);
    ActiveConversationState.CurrentNodeID = TargetNodeID;
    bCurrentDialogueNodeCompleted = false;
    CommitCurrentConversationState();

    if (bLogBranchEntry)
    {
        UE_LOG(LogTemp, Log, TEXT("[VHVDialogue] Branch entered NodeID=%s"), *TargetNodeID);
    }

    if (TargetNode->NodeType == EVHVDialogueNodeType::LearningActivity)
    {
        StartLinkedLearningActivity(TargetNode->LinkedActivity);
    }

    return true;
}

void UVHVUIManagerComponent::ApplyCurrentDialogueNodeCompletionEffects(const TArray<FVHVStoryEffect>* SelectionEffects)
{
    if (bCurrentDialogueNodeCompleted)
    {
        return;
    }

    bCurrentDialogueNodeCompleted = true;
    if (!StoryStateSubsystem)
    {
        return;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(ActiveConversation.ConversationID, ActiveConversationState.CurrentNodeID);
    if (!CurrentNode)
    {
        return;
    }

    StoryStateSubsystem->ApplyEffects(CurrentNode->CompletionEffects);
    if (SelectionEffects)
    {
        StoryStateSubsystem->ApplyEffects(*SelectionEffects);
    }
}

void UVHVUIManagerComponent::CompleteConversation()
{
    const FName CompletedConversationID(*ActiveConversation.ConversationID);
    if (DialogueWidget)
    {
        DialogueWidget->HideDialogue();
    }

    CommitCurrentConversationState();
    bConversationActive = false;
    if (TextbookSubsystem)
    {
        TextbookSubsystem->ExitCurrentLearningSession();
    }
    bQuestManagedLearningActivityActive = false;
    HideQuestionUI();
    HideFeedbackUI();
    HideTeachUI();

    if (QuestSubsystem && !CompletedConversationID.IsNone())
    {
        QuestSubsystem->NotifyConversationCompleted(CompletedConversationID);
    }
    RestoreGameplayAfterQuestModalIfNeeded();
}

void UVHVUIManagerComponent::RestoreGameplayAfterQuestModalIfNeeded()
{
    // Quest/activity callbacks can complete synchronously beneath a stinger.
    // The active sequence remains the sole modal owner until its last callback.
    if (bMajorStingerSequenceActive)
    {
        return;
    }

    FVHVQuestObjectiveDefinition Objective;
    if (QuestSubsystem && QuestSubsystem->GetCurrentObjective(Objective))
    {
        const bool bAutoStartsModal = Objective.bAutoStart
            && (Objective.ObjectiveType == EVHVQuestObjectiveType::Conversation
                || Objective.ObjectiveType == EVHVQuestObjectiveType::LearningActivity);
        const bool bFreeRoamTravelActive =
            QuestSubsystem->IsCurrentObjectiveFreeRoamNPCTravelActive();
        if (bAutoStartsModal
            && !bFreeRoamTravelActive
            && !QuestSubsystem->IsCurrentObjectiveWaitingForActivation())
        {
            return;
        }
    }

    if (CurrentUIState != EVHVUIState::Gameplay)
    {
        UE_LOG(LogVHV, Log, TEXT("[VHVQuest] Quest modal complete; returning UI to Gameplay."));
        ApplyUIState(EVHVUIState::Gameplay);
    }

    EndConversationSession();
}

void UVHVUIManagerComponent::EndConversationSession()
{
    if (!bConversationSessionActive)
    {
        return;
    }

    bConversationSessionActive = false;
    OnConversationSessionEnded.Broadcast();
}

void UVHVUIManagerComponent::ConfirmChoiceInput()
{
    if (CurrentUIState == EVHVUIState::MajorStinger)
    {
        return;
    }

    if (CurrentUIState == EVHVUIState::Dialogue)
    {
        AdvanceConversation();
        return;
    }

    if (CurrentUIState == EVHVUIState::LearningAsk)
    {
        if (TextbookSubsystem && TextbookSubsystem->GetCurrentPhase() == ELearningPhase::Ask)
        {
            if (TextbookSubsystem->GetCurrentActivity().ActivityType == ETextbookActivityType::Observation
                && ObservationWidget)
            {
                ObservationWidget->AdvanceObservation();
                return;
            }
            TrySubmitCurrentQuestionAnswer();
            return;
        }
        return;
    }

    if (CurrentUIState == EVHVUIState::LearningFeedback)
    {
        AdvanceFeedback();
        return;
    }

    if (CurrentUIState == EVHVUIState::LearningHint)
    {
        AdvanceHint();
        return;
    }

    if (CurrentUIState == EVHVUIState::LearningTeach)
    {
        AdvanceTeach();
        return;
    }

    if (!bConversationActive || !DialogueWidget)
    {
        return;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(ActiveConversation.ConversationID, DialogueWidget->GetCurrentNodeID());
    if (CurrentNode && CurrentNode->NodeType == EVHVDialogueNodeType::Choice)
    {
        ConfirmChoice();
        return;
    }

    if (TextbookSubsystem && TextbookSubsystem->GetCurrentPhase() == ELearningPhase::Ask)
    {
        TrySubmitCurrentQuestionAnswer();
        return;
    }

    if (TextbookSubsystem && TextbookSubsystem->GetCurrentPhase() == ELearningPhase::Feedback)
    {
        AdvanceFeedback();
        return;
    }

    if (TextbookSubsystem && TextbookSubsystem->GetCurrentPhase() == ELearningPhase::Hint)
    {
        AdvanceHint();
        return;
    }

    if (TextbookSubsystem && TextbookSubsystem->GetCurrentPhase() == ELearningPhase::Teach)
    {
        AdvanceTeach();
        return;
    }
}

void UVHVUIManagerComponent::ExitConversation()
{
    if (CurrentUIState == EVHVUIState::Gameplay && !bConversationSessionActive)
    {
        return;
    }

    if (bConversationActive)
    {
        CommitCurrentConversationState();
    }
    bConversationActive = false;

    if (TextbookSubsystem)
    {
        TextbookSubsystem->ExitCurrentLearningSession();
    }
    bQuestManagedLearningActivityActive = false;

    if (DialogueWidget)
    {
        DialogueWidget->HideDialogue();
    }

    HideQuestionUI();
    HideFeedbackUI();
    HideTeachUI();
    ApplyUIState(EVHVUIState::Gameplay);
    EndConversationSession();
}

bool UVHVUIManagerComponent::IsConversationActive() const
{
    return bConversationActive;
}

bool UVHVUIManagerComponent::IsConversationSessionActive() const
{
    return bConversationSessionActive;
}

FConversationRuntimeState UVHVUIManagerComponent::GetConversationState() const
{
    return ActiveConversationState;
}

void UVHVUIManagerComponent::ExportDialogueCheckpointSaveState(TArray<FVHVDialogueCheckpointSaveState>& OutSaveStates) const
{
    OutSaveStates.Reset();
    for (const TPair<FString, FConversationRuntimeState>& Pair : ConversationStates)
    {
        const FConversationRuntimeState& State = Pair.Value;
        if (State.ConversationID.IsEmpty())
        {
            continue;
        }

        FVHVDialogueCheckpointSaveState SavedState;
        SavedState.ConversationID = State.ConversationID;
        SavedState.LatestCheckpointID = State.LatestCheckpointID;
        SavedState.CurrentNodeID = State.CurrentNodeID;
        SavedState.bCompleted = State.bCompleted;
        OutSaveStates.Add(MoveTemp(SavedState));
    }
    OutSaveStates.Sort([](const FVHVDialogueCheckpointSaveState& A, const FVHVDialogueCheckpointSaveState& B)
    {
        return A.ConversationID < B.ConversationID;
    });
}

bool UVHVUIManagerComponent::ValidateDialogueCheckpointSaveState(const TArray<FVHVDialogueCheckpointSaveState>& SaveStates) const
{
    TSet<FString> ConversationIDs;
    for (const FVHVDialogueCheckpointSaveState& SavedState : SaveStates)
    {
        if (SavedState.ConversationID.IsEmpty() || ConversationIDs.Contains(SavedState.ConversationID))
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVDialogue] Saved dialogue checkpoint has an empty or duplicate Conversation ID '%s'."), *SavedState.ConversationID);
            return false;
        }
        ConversationIDs.Add(SavedState.ConversationID);
    }
    return true;
}

bool UVHVUIManagerComponent::ImportDialogueCheckpointSaveState(const TArray<FVHVDialogueCheckpointSaveState>& SaveStates)
{
    if (!ValidateDialogueCheckpointSaveState(SaveStates) || bConversationSessionActive)
    {
        return false;
    }

    ConversationStates.Reset();
    for (const FVHVDialogueCheckpointSaveState& SavedState : SaveStates)
    {
        FConversationRuntimeState State;
        State.ConversationID = SavedState.ConversationID;
        State.LatestCheckpointID = SavedState.LatestCheckpointID;
        State.CurrentNodeID = SavedState.CurrentNodeID;
        State.bCompleted = SavedState.bCompleted;
        ConversationStates.Add(State.ConversationID, MoveTemp(State));
    }
    return true;
}

void UVHVUIManagerComponent::ShowDialogue(const FDialogueData& InDialogue)
{
    if (!MainHUD || !DialogueWidget || !MainHUD->DialogueLayer)
    {
        return;
    }

    if (!DialogueWidget->GetParent())
    {
        MainHUD->DialogueLayer->AddChild(DialogueWidget);

        UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(DialogueWidget->Slot);
        if (CanvasSlot)
        {
            CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
            CanvasSlot->SetOffsets(FMargin(0.0f, 0.0f, 0.0f, 0.0f));
            CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
        }
    }

    bConversationSessionActive = true;
    bConversationActive = true;
    ApplyUIState(EVHVUIState::Dialogue);
    DialogueWidget->ShowDialogue(InDialogue);
}

void UVHVUIManagerComponent::HideDialogue()
{
    if (DialogueWidget)
    {
        DialogueWidget->HideDialogue();
    }

    bConversationActive = false;
    ApplyUIState(EVHVUIState::Gameplay);
    EndConversationSession();
}

void UVHVUIManagerComponent::AdvanceDialogue()
{
    if (DialogueWidget)
    {
        DialogueWidget->AdvanceDialogue();
        if (!DialogueWidget->IsVisibleInViewport())
        {
            HideDialogue();
        }
    }
}
