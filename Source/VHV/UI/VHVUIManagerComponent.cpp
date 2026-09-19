#include "UI/VHVUIManagerComponent.h"
#include "UI/VHVMainHUD.h"
#include "UI/VHVDialogueWidget.h"
#include "UI/Textbook/VHVQuestionWidget.h"
#include "UI/Textbook/VHVFeedbackWidget.h"
#include "UI/Textbook/VHVTeachWidget.h"
#include "UI/Textbook/VHVObservationWidget.h"
#include "UI/VHVOrderingWidget.h"
#include "UI/VHVMatchingWidget.h"
#include "Core/VHVDialogueTypes.h"
#include "VHV.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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
    }

    Super::EndPlay(EndPlayReason);
}

void UVHVUIManagerComponent::HandleInteractionTargetChanged(UVHVInteractionComponent* NewTarget)
{
    CurrentInteractionTarget = NewTarget;
    RefreshInteractionPrompt();
}

void UVHVUIManagerComponent::HandleLearningPhaseChanged(ELearningPhase NewPhase)
{
    if (NewPhase == ELearningPhase::Ask)
    {
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
        HideQuestionUI();
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

void UVHVUIManagerComponent::HandleTextbookActivityCompleted()
{
    if (!TextbookSubsystem)
    {
        return;
    }

    if (!bConversationActive)
    {
        if (bQuestManagedLearningActivityActive)
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
        const bool bShouldShowQuestion = NewState == EVHVUIState::LearningAsk && (!TextbookSubsystem || !TextbookSubsystem->IsActivityActive() || (TextbookSubsystem->GetCurrentActivity().ActivityType != ETextbookActivityType::Ordering && TextbookSubsystem->GetCurrentActivity().ActivityType != ETextbookActivityType::Matching && TextbookSubsystem->GetCurrentActivity().ActivityType != ETextbookActivityType::Observation));
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
        const bool bShouldShowObservation = NewState == EVHVUIState::LearningAsk && TextbookSubsystem && TextbookSubsystem->IsActivityActive() && TextbookSubsystem->GetCurrentActivity().ActivityType == ETextbookActivityType::Observation;
        ObservationWidget->SetVisibility(bShouldShowObservation ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (OrderingWidget)
    {
        const bool bShouldShowOrdering = NewState == EVHVUIState::LearningAsk && TextbookSubsystem && TextbookSubsystem->IsActivityActive() && TextbookSubsystem->GetCurrentActivity().ActivityType == ETextbookActivityType::Ordering;
        OrderingWidget->SetVisibility(bShouldShowOrdering ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    if (MatchingWidget)
    {
        const bool bShouldShowMatching = NewState == EVHVUIState::LearningAsk && TextbookSubsystem && TextbookSubsystem->IsActivityActive() && TextbookSubsystem->GetCurrentActivity().ActivityType == ETextbookActivityType::Matching;
        MatchingWidget->SetVisibility(bShouldShowMatching ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
        return;
    }

    const FTextbookActivityData CurrentActivity = TextbookSubsystem->GetCurrentActivity();
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
    if (CurrentActivity.Hints.Num() > 0)
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
    const FString FeedbackText = RuntimeState.bAnswerCorrect
        ? CurrentActivity.CorrectFeedback
        : (RuntimeState.bAnswerPartial ? CurrentActivity.PartialFeedback : CurrentActivity.IncorrectFeedback);

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
    if (CurrentActivity.bRequireCorrectAnswerToAdvance && !RuntimeState.bAnswerCorrect && RuntimeState.AttemptCount < 3)
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

    const bool bQuestManaged = TextbookSubsystem->GetProgressionMode() == EVHVTextbookProgressionMode::QuestManaged;
    TextbookSubsystem->AdvanceToNextActivity();
    HideTeachUI();
    if (bQuestManaged && CurrentUIState == EVHVUIState::LearningTeach && !TextbookSubsystem->IsActivityActive())
    {
        RestoreGameplayAfterQuestModalIfNeeded();
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

    ActiveConversation = InConversation;
    ActiveConversationState = ConversationStates.Contains(InConversation.ConversationID) ? ConversationStates[InConversation.ConversationID] : FConversationRuntimeState();
    ActiveConversationState.ConversationID = InConversation.ConversationID;

    FString StartingNodeID = ExplicitStartNodeID;
    if (StartingNodeID.IsEmpty())
    {
        StartingNodeID = InConversation.StartNodeID;
        if (!ActiveConversationState.LatestCheckpointID.IsEmpty())
        {
            for (const FDialogueNode& Node : InConversation.Nodes)
            {
                if (Node.bIsCheckpoint && Node.GetEffectiveCheckpointID() == ActiveConversationState.LatestCheckpointID)
                {
                    StartingNodeID = Node.NodeID;
                    break;
                }
            }
        }
        else if (!ActiveConversationState.CurrentNodeID.IsEmpty())
        {
            StartingNodeID = ActiveConversationState.CurrentNodeID;
        }
    }

    if (StartingNodeID.IsEmpty() && InConversation.Nodes.Num() > 0)
    {
        StartingNodeID = InConversation.Nodes[0].NodeID;
    }

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
    const bool bStarted = TextbookSubsystem->StartActivityByID(ActivityID);
    if (bStarted)
    {
        RefreshCurrentAskQuestionUI();
    }
    return bStarted;
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
    if (CurrentUIState != EVHVUIState::LearningAsk || !TextbookSubsystem ||
        TextbookSubsystem->GetCurrentPhase() != ELearningPhase::Ask || !TextbookSubsystem->IsActivityActive())
    {
        return false;
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
        if (MatchingWidget)
        {
            MatchingWidget->ActivateFocusedSelection();
        }
        break;
    default:
        break;
    }

    // LearningAsk owns interaction while modal, including activities without a
    // select operation, so this press cannot also activate a world target.
    return true;
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
    FVHVQuestObjectiveDefinition Objective;
    if (QuestSubsystem && QuestSubsystem->GetCurrentObjective(Objective))
    {
        const bool bAutoStartsModal = Objective.bAutoStart
            && (Objective.ObjectiveType == EVHVQuestObjectiveType::Conversation
                || Objective.ObjectiveType == EVHVQuestObjectiveType::LearningActivity);
        if (bAutoStartsModal && !QuestSubsystem->IsCurrentObjectiveWaitingForActivation())
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
    if (CurrentUIState == EVHVUIState::Dialogue)
    {
        AdvanceConversation();
        return;
    }

    if (CurrentUIState == EVHVUIState::LearningAsk)
    {
        if (TextbookSubsystem && TextbookSubsystem->GetCurrentPhase() == ELearningPhase::Ask)
        {
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
