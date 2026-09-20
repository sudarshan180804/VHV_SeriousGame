#include "UI/VHVDialogueWidget.h"
#include "UI/VHVDialogueChoiceButton.h"
#include "UI/VHVUIManagerComponent.h"
#include "UI/Textbook/SVHVChoiceCard.h"
#include "UI/Textbook/VHVActivityUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

UVHVDialogueWidget::UVHVDialogueWidget()
{
    SetIsFocusable(true);
}

TSharedRef<SWidget> UVHVDialogueWidget::RebuildWidget()
{
    TSharedRef<SWidget> ExistingDialoguePresentation = Super::RebuildWidget();

    ActivityQuestionPanelBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassMain(), VHVActivityUIStyle::QuestionPanelRadius,
        VHVActivityUIStyle::PanelBorder(), VHVActivityUIStyle::BorderNormalWidth);
    ActivityQuestionShadowBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::ShadowPanel(), VHVActivityUIStyle::QuestionPanelRadius + 3.0f);
    ActivityKeycapBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(29, 34, 38, 145), 6.0f,
        VHVActivityUIStyle::BorderNeutral(), VHVActivityUIStyle::BorderNormalWidth);
    ActivityEmblemBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(28, 26, 20, 128), 10.0f,
        VHVActivityUIStyle::HeaderGold(), VHVActivityUIStyle::BorderNormalWidth);
    ActivityDividerBrush = VHVActivityUIStyle::RoundedBrush(VHVActivityUIStyle::DividerGold(), 1.0f);

    TSharedRef<SOverlay> Root = SNew(SOverlay)
        + SOverlay::Slot()
        [
            SAssignNew(NormalDialoguePresentation, SBox)
            [
                ExistingDialoguePresentation
            ]
        ]
        + SOverlay::Slot()
        [
            SAssignNew(ActivityRootCanvas, SConstraintCanvas)
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.1275f, 0.655f, 0.5625f, 0.865f))
            .Offset(FMargin(0.0f))
            [
                SAssignNew(ActivityQuestionPanel, SOverlay)
                + SOverlay::Slot()
                .Padding(FMargin(5.0f, 6.0f, -5.0f, -6.0f))
                [
                    SNew(SBorder)
                    .BorderImage(&ActivityQuestionShadowBrush)
                ]
                + SOverlay::Slot()
                [
                    SNew(SBorder)
                    .BorderImage(&ActivityQuestionPanelBrush)
                    .Padding(VHVActivityUIStyle::PanelPadding)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
                            [
                                SNew(SBox)
                                .WidthOverride(VHVActivityUIStyle::HeaderIconDiameter)
                                .HeightOverride(VHVActivityUIStyle::HeaderIconDiameter)
                                [
                                    SNew(SBorder)
                                    .BorderImage(&ActivityEmblemBrush)
                                    .HAlign(HAlign_Center)
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(STextBlock)
                                        .Font(VHVActivityUIStyle::MediumFont(12))
                                        .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
                                        .Text(FText::FromString(TEXT("\u2026")))
                                    ]
                                ]
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            [
                                SAssignNew(ActivitySpeakerText, STextBlock)
                                .Font(VHVActivityUIStyle::MediumFont(VHVActivityUIStyle::HeaderFontSize))
                                .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
                                .AutoWrapText(true)
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .HAlign(HAlign_Left)
                        .Padding(FMargin(32.0f, 7.0f, 0.0f, 14.0f))
                        [
                            SNew(SBox)
                            .WidthOverride(VHVActivityUIStyle::HeaderDividerWidth)
                            .HeightOverride(VHVActivityUIStyle::HeaderDividerHeight)
                            [
                                SNew(SBorder)
                                .BorderImage(&ActivityDividerBrush)
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        .VAlign(VAlign_Top)
                        [
                            SAssignNew(ActivityPromptText, STextBlock)
                            .Font(VHVActivityUIStyle::RegularFont(VHVActivityUIStyle::QuestionFontSize))
                            .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                            .AutoWrapText(true)
                            .LineHeightPercentage(1.0f)
                        ]
                    ]
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.6625f, 0.51f, 0.975f, 0.94f))
            .Offset(FMargin(0.0f))
            [
                SAssignNew(ActivityAnswerPanel, SVerticalBox)
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .VAlign(VAlign_Center)
                [
                    SAssignNew(ActivityChoiceContainer, SVerticalBox)
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(FMargin(0.0f, VHVActivityUIStyle::LegendGap, 8.0f, 0.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SBorder)
                        .BorderImage(&ActivityKeycapBrush)
                        .Padding(FMargin(8.0f, 3.0f))
                        [
                            SNew(STextBlock)
                            .Font(VHVActivityUIStyle::MediumFont(13))
                            .ColorAndOpacity(VHVActivityUIStyle::PrimaryText())
                            .Text(FText::FromString(TEXT("W/S")))
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(7.0f, 0.0f, 20.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(14))
                        .ColorAndOpacity(VHVActivityUIStyle::SecondaryText())
                        .Text(FText::FromString(TEXT("Navigate")))
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    [
                        SNew(SBorder)
                        .BorderImage(&ActivityKeycapBrush)
                        .Padding(FMargin(8.0f, 3.0f))
                        [
                            SNew(STextBlock)
                            .Font(VHVActivityUIStyle::MediumFont(13))
                            .ColorAndOpacity(VHVActivityUIStyle::PrimaryText())
                            .Text(FText::FromString(TEXT("ENTER")))
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(7.0f, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(14))
                        .ColorAndOpacity(VHVActivityUIStyle::SecondaryText())
                        .Text(FText::FromString(TEXT("Choose")))
                    ]
                ]
            ]
        ];

    SetActivityChoicePresentation(bActivityChoicePresentation);
    RebuildActivityChoiceCards();
    UpdateActivityChoicePresentation();
    return Root;
}

void UVHVDialogueWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    ActivityChoiceCards.Empty();
    NormalDialoguePresentation.Reset();
    ActivityRootCanvas.Reset();
    ActivitySpeakerText.Reset();
    ActivityPromptText.Reset();
    ActivityChoiceContainer.Reset();
    ActivityQuestionPanel.Reset();
    ActivityAnswerPanel.Reset();
}

void UVHVDialogueWidget::NativeConstruct()
{
    Super::NativeConstruct();
    ActivityEntranceElapsed = 0.0f;
}

void UVHVDialogueWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bActivityChoicePresentation)
    {
        return;
    }

    ActivityEntranceElapsed += InDeltaTime;
    const float Alpha = FMath::Clamp(
        ActivityEntranceElapsed / VHVActivityUIStyle::AnimationStandard, 0.0f, 1.0f);
    const float Smoothed = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 3.0f);

    if (ActivityQuestionPanel)
    {
        ActivityQuestionPanel->SetRenderOpacity(Smoothed);
        ActivityQuestionPanel->SetRenderTransform(FSlateRenderTransform(
            FVector2D(0.0f, FMath::Lerp(12.0f, 0.0f, Smoothed))));
    }
    if (ActivityAnswerPanel)
    {
        ActivityAnswerPanel->SetRenderOpacity(Smoothed);
    }
}

FReply UVHVDialogueWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
    {
        SelectPreviousChoice();
        return FReply::Handled();
    }
    if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
    {
        SelectNextChoice();
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (OwningUIManager)
        {
            OwningUIManager->AdvanceConversation();
        }
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UVHVDialogueWidget::SetOwningUIManager(UVHVUIManagerComponent* InUIManager)
{
    OwningUIManager = InUIManager;
}

FReply UVHVDialogueWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (CurrentConversation.Nodes.Num() == 0 && CurrentDialogue.Lines.Num() > 0)
    {
        if (OwningUIManager)
        {
            OwningUIManager->AdvanceConversation();
        }
        return FReply::Handled();
    }

    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice)
    {
        if (OwningUIManager)
        {
            OwningUIManager->AdvanceConversation();
        }
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UVHVDialogueWidget::ShowDialogue(const FDialogueData& InDialogue)
{
    SetActivityChoicePresentation(false);
    CurrentConversation = FDialogueConversation();
    CurrentNodeID = FString();
    CurrentDialogue = InDialogue;
    CurrentLineIndex = 0;
    SelectedChoiceIndex = INDEX_NONE;
    AvailableChoiceIndices.Reset();

    if (CurrentDialogue.Lines.Num() == 0)
    {
        HideDialogue();
        return;
    }

    UpdateDialogueLine();
    SetVisibility(ESlateVisibility::Visible);
}

void UVHVDialogueWidget::ShowConversation(const FDialogueConversation& InConversation, const FString& InStartNodeID)
{
    CurrentDialogue = FDialogueData();
    CurrentConversation = InConversation;
    CurrentLineIndex = 0;
    SelectedChoiceIndex = INDEX_NONE;
    AvailableChoiceIndices.Reset();

    if (CurrentConversation.Nodes.Num() == 0)
    {
        HideDialogue();
        return;
    }

    FString StartNodeID = InStartNodeID;
    if (StartNodeID.IsEmpty())
    {
        StartNodeID = InConversation.StartNodeID;
    }
    if (StartNodeID.IsEmpty())
    {
        StartNodeID = InConversation.Nodes[0].NodeID;
    }

    SetCurrentNode(StartNodeID);
    SetVisibility(ESlateVisibility::Visible);
}

void UVHVDialogueWidget::HideDialogue()
{
    SetActivityChoicePresentation(false);
    CurrentConversation = FDialogueConversation();
    CurrentNodeID = FString();
    CurrentDialogue = FDialogueData();
    CurrentLineIndex = 0;
    SelectedChoiceIndex = INDEX_NONE;
    AvailableChoiceIndices.Reset();
    ClearChoices();
    SetVisibility(ESlateVisibility::Collapsed);
}

void UVHVDialogueWidget::AdvanceDialogue()
{
    if (!CurrentNodeID.IsEmpty())
    {
        if (OwningUIManager)
        {
            OwningUIManager->AdvanceConversation();
        }
        return;
    }

    if (CurrentDialogue.Lines.Num() == 0)
    {
        HideDialogue();
        return;
    }

    if (CurrentLineIndex < CurrentDialogue.Lines.Num() - 1)
    {
        ++CurrentLineIndex;
        UpdateDialogueLine();
        return;
    }

    HideDialogue();
}

void UVHVDialogueWidget::SelectNextChoice()
{
    if (CurrentConversation.Nodes.Num() == 0)
    {
        return;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice || AvailableChoiceIndices.IsEmpty())
    {
        return;
    }

    int32 AvailableIndex = AvailableChoiceIndices.IndexOfByKey(SelectedChoiceIndex);
    AvailableIndex = (AvailableIndex + 1) % AvailableChoiceIndices.Num();
    SelectedChoiceIndex = AvailableChoiceIndices[AvailableIndex];
    UpdateChoiceSelectionUI();
}

void UVHVDialogueWidget::SelectPreviousChoice()
{
    if (CurrentConversation.Nodes.Num() == 0)
    {
        return;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice || AvailableChoiceIndices.IsEmpty())
    {
        return;
    }

    int32 AvailableIndex = AvailableChoiceIndices.IndexOfByKey(SelectedChoiceIndex);
    AvailableIndex = AvailableIndex == INDEX_NONE
        ? AvailableChoiceIndices.Num() - 1
        : (AvailableIndex - 1 + AvailableChoiceIndices.Num()) % AvailableChoiceIndices.Num();
    SelectedChoiceIndex = AvailableChoiceIndices[AvailableIndex];
    UpdateChoiceSelectionUI();
}

bool UVHVDialogueWidget::ConfirmChoice()
{
    if (CurrentConversation.Nodes.Num() == 0)
    {
        return false;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice || !AvailableChoiceIndices.Contains(SelectedChoiceIndex))
    {
        return false;
    }

    return OwningUIManager ? OwningUIManager->ConfirmChoice() : false;
}

void UVHVDialogueWidget::SelectChoiceAndConfirm(int32 ChoiceIndex)
{
    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice || !AvailableChoiceIndices.Contains(ChoiceIndex))
    {
        return;
    }

    SelectedChoiceIndex = ChoiceIndex;
    UpdateChoiceSelectionUI();

    if (OwningUIManager)
    {
        OwningUIManager->ConfirmChoice();
    }
}

void UVHVDialogueWidget::UpdateDialogueLine()
{
    if (CurrentDialogue.Lines.IsValidIndex(CurrentLineIndex))
    {
        const FDialogueLine& Line = CurrentDialogue.Lines[CurrentLineIndex];

        if (SpeakerNameText)
        {
            SpeakerNameText->SetText(Line.SpeakerName);
        }

        if (DialogueText)
        {
            DialogueText->SetText(Line.Text);
        }
    }
}

void UVHVDialogueWidget::UpdateFromNode()
{
    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode)
    {
        HideDialogue();
        return;
    }

    SetActivityChoicePresentation(
        CurrentNode->NodeType == EVHVDialogueNodeType::Choice
        && OwningUIManager
        && OwningUIManager->IsDialogueChoiceLearningActivity(CurrentConversation.ConversationID, CurrentNodeID));

    if (SpeakerNameText)
    {
        if (CurrentNode->SpeakerName.IsEmpty())
        {
            SpeakerNameText->SetText(FText::FromString(TEXT("Instructor")));
        }
        else
        {
            SpeakerNameText->SetText(CurrentNode->SpeakerName);
        }
    }

    if (DialogueText)
    {
        DialogueText->SetText(CurrentNode->Text);
    }

    ClearChoices();
    AvailableChoiceIndices.Reset();

    if (CurrentNode->NodeType == EVHVDialogueNodeType::Choice)
    {
        for (int32 Index = 0; Index < CurrentNode->Choices.Num(); ++Index)
        {
            if (!OwningUIManager || OwningUIManager->IsDialogueChoiceAvailable(CurrentNode->Choices[Index]))
            {
                AvailableChoiceIndices.Add(Index);
            }
        }

        SelectedChoiceIndex = AvailableChoiceIndices.IsEmpty() ? INDEX_NONE : AvailableChoiceIndices[0];
        for (const int32 ChoiceIndex : AvailableChoiceIndices)
        {
            if (CurrentNode->Choices[ChoiceIndex].bIsDefault)
            {
                SelectedChoiceIndex = ChoiceIndex;
                break;
            }
        }

        if (bActivityChoicePresentation)
        {
            RebuildActivityChoiceCards();
        }
        else
        {
            EnsureChoiceContainer();

            if (ChoiceContainer)
            {
                for (const int32 ChoiceIndex : AvailableChoiceIndices)
                {
                    const FDialogueChoiceOption& Choice = CurrentNode->Choices[ChoiceIndex];
                    UVHVDialogueChoiceButton* ChoiceButton = NewObject<UVHVDialogueChoiceButton>(this);
                    ChoiceButton->InitializeChoice(this, ChoiceIndex);

                    UTextBlock* ChoiceText = NewObject<UTextBlock>(ChoiceButton);
                    ChoiceText->SetText(Choice.OptionText);
                    ChoiceText->SetColorAndOpacity(ChoiceIndex == SelectedChoiceIndex ? FLinearColor::White : FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
                    ChoiceButton->SetContent(ChoiceText);
                    ChoiceContainer->AddChild(ChoiceButton);
                }
            }
        }
    }

    UpdateActivityChoicePresentation();
}

void UVHVDialogueWidget::EnsureChoiceContainer()
{
    if (ChoiceContainer || !WidgetTree)
    {
        return;
    }

    UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
    if (!RootCanvas)
    {
        return;
    }

    ChoiceContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RuntimeChoiceContainer"));
    UCanvasPanelSlot* ChoiceSlot = RootCanvas->AddChildToCanvas(ChoiceContainer);
    ChoiceSlot->SetAnchors(FAnchors(0.15f, 0.58f, 0.85f, 0.92f));
    ChoiceSlot->SetOffsets(FMargin(0.0f));
}

void UVHVDialogueWidget::UpdateChoiceSelectionUI()
{
    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice)
    {
        return;
    }

    if (ChoiceContainer)
    {
        for (int32 RenderedIndex = 0; RenderedIndex < ChoiceContainer->GetChildrenCount(); ++RenderedIndex)
        {
            if (UVHVDialogueChoiceButton* ChoiceButton = Cast<UVHVDialogueChoiceButton>(ChoiceContainer->GetChildAt(RenderedIndex)))
            {
                if (UTextBlock* ChoiceText = Cast<UTextBlock>(ChoiceButton->GetContent()))
                {
                    const int32 ChoiceIndex = AvailableChoiceIndices.IsValidIndex(RenderedIndex) ? AvailableChoiceIndices[RenderedIndex] : INDEX_NONE;
                    ChoiceText->SetColorAndOpacity(ChoiceIndex == SelectedChoiceIndex ? FLinearColor::White : FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
                }
            }
        }
    }

    UpdateActivityChoicePresentation();
}

void UVHVDialogueWidget::ClearChoices()
{
    if (ChoiceContainer)
    {
        ChoiceContainer->ClearChildren();
    }
    if (ActivityChoiceContainer)
    {
        ActivityChoiceContainer->ClearChildren();
    }
    ActivityChoiceCards.Empty();
}

void UVHVDialogueWidget::SetActivityChoicePresentation(const bool bEnabled)
{
    const bool bStartingActivityPresentation = bEnabled && !bActivityChoicePresentation;
    bActivityChoicePresentation = bEnabled;

    if (NormalDialoguePresentation)
    {
        NormalDialoguePresentation->SetVisibility(
            bActivityChoicePresentation ? EVisibility::Collapsed : EVisibility::Visible);
    }
    if (ActivityRootCanvas)
    {
        ActivityRootCanvas->SetVisibility(
            bActivityChoicePresentation ? EVisibility::Visible : EVisibility::Collapsed);
    }

    if (bStartingActivityPresentation)
    {
        ActivityEntranceElapsed = 0.0f;
        if (ActivityQuestionPanel)
        {
            ActivityQuestionPanel->SetRenderOpacity(0.0f);
            ActivityQuestionPanel->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, 12.0f)));
        }
        if (ActivityAnswerPanel)
        {
            ActivityAnswerPanel->SetRenderOpacity(0.0f);
        }
    }
}

void UVHVDialogueWidget::RebuildActivityChoiceCards()
{
    ActivityChoiceCards.Empty();
    if (!ActivityChoiceContainer)
    {
        return;
    }

    ActivityChoiceContainer->ClearChildren();
    if (!bActivityChoicePresentation)
    {
        return;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice)
    {
        return;
    }

    for (int32 RenderedIndex = 0; RenderedIndex < AvailableChoiceIndices.Num(); ++RenderedIndex)
    {
        const int32 ChoiceIndex = AvailableChoiceIndices[RenderedIndex];
        if (!CurrentNode->Choices.IsValidIndex(ChoiceIndex))
        {
            continue;
        }

        TSharedPtr<SVHVChoiceCard> ChoiceCard;
        ActivityChoiceContainer->AddSlot()
        .AutoHeight()
        .Padding(FMargin(0.0f, 0.0f, 0.0f,
            RenderedIndex + 1 < AvailableChoiceIndices.Num() ? VHVActivityUIStyle::ChoiceGap : 0.0f))
        [
            SAssignNew(ChoiceCard, SVHVChoiceCard)
            .OptionIndex(ChoiceIndex)
            .AnswerText(CurrentNode->Choices[ChoiceIndex].OptionText)
            .EntranceDelay(RenderedIndex * VHVActivityUIStyle::AnimationStagger)
            .AllowVariableHeight(true)
            .OnChosen(FOnVHVChoiceCardChosen::CreateUObject(
                this, &UVHVDialogueWidget::HandleActivityChoiceCardChosen))
        ];
        ActivityChoiceCards.Add(ChoiceCard);
    }
}

void UVHVDialogueWidget::UpdateActivityChoicePresentation()
{
    if (!bActivityChoicePresentation)
    {
        return;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode)
    {
        return;
    }

    if (ActivitySpeakerText)
    {
        ActivitySpeakerText->SetText(CurrentNode->SpeakerName.IsEmpty()
            ? FText::FromString(TEXT("Instructor"))
            : CurrentNode->SpeakerName);
    }
    if (ActivityPromptText)
    {
        ActivityPromptText->SetText(CurrentNode->Text.IsEmpty()
            ? FText::FromString(TEXT("Choose a response."))
            : CurrentNode->Text);
    }

    for (int32 RenderedIndex = 0; RenderedIndex < ActivityChoiceCards.Num(); ++RenderedIndex)
    {
        if (ActivityChoiceCards[RenderedIndex])
        {
            const int32 ChoiceIndex = AvailableChoiceIndices.IsValidIndex(RenderedIndex)
                ? AvailableChoiceIndices[RenderedIndex]
                : INDEX_NONE;
            const bool bCurrentChoice = ChoiceIndex == SelectedChoiceIndex;
            ActivityChoiceCards[RenderedIndex]->SetPresentationState(
                bCurrentChoice, bCurrentChoice, true, false);
        }
    }
}

void UVHVDialogueWidget::HandleActivityChoiceCardChosen(const int32 ChoiceIndex)
{
    SelectChoiceAndConfirm(ChoiceIndex);
}

const FDialogueNode* UVHVDialogueWidget::FindNodeByID(const FString& NodeID) const
{
    if (NodeID.IsEmpty())
    {
        return nullptr;
    }

    for (const FDialogueNode& Node : CurrentConversation.Nodes)
    {
        if (Node.NodeID == NodeID)
        {
            return &Node;
        }
    }

    return nullptr;
}

void UVHVDialogueWidget::SetCurrentNode(const FString& NodeID)
{
    if (NodeID.IsEmpty())
    {
        HideDialogue();
        return;
    }

    CurrentNodeID = NodeID;
    SelectedChoiceIndex = INDEX_NONE;
    const FDialogueNode* Node = FindNodeByID(NodeID);
    if (!Node)
    {
        HideDialogue();
        return;
    }

    UpdateFromNode();
}

