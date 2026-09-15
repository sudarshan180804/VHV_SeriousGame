#include "UI/VHVDialogueWidget.h"
#include "UI/VHVDialogueChoiceButton.h"
#include "UI/VHVUIManagerComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"

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
    CurrentConversation = FDialogueConversation();
    CurrentNodeID = FString();
    CurrentDialogue = InDialogue;
    CurrentLineIndex = 0;
    SelectedChoiceIndex = 0;

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
    SelectedChoiceIndex = 0;

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
    CurrentConversation = FDialogueConversation();
    CurrentNodeID = FString();
    CurrentDialogue = FDialogueData();
    CurrentLineIndex = 0;
    SelectedChoiceIndex = 0;
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
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice || CurrentNode->Choices.Num() == 0)
    {
        return;
    }

    SelectedChoiceIndex = (SelectedChoiceIndex + 1) % CurrentNode->Choices.Num();
    UpdateChoiceSelectionUI();
}

void UVHVDialogueWidget::SelectPreviousChoice()
{
    if (CurrentConversation.Nodes.Num() == 0)
    {
        return;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice || CurrentNode->Choices.Num() == 0)
    {
        return;
    }

    SelectedChoiceIndex = (SelectedChoiceIndex - 1 + CurrentNode->Choices.Num()) % CurrentNode->Choices.Num();
    UpdateChoiceSelectionUI();
}

bool UVHVDialogueWidget::ConfirmChoice()
{
    if (CurrentConversation.Nodes.Num() == 0)
    {
        return false;
    }

    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice || CurrentNode->Choices.Num() == 0)
    {
        return false;
    }

    return OwningUIManager ? OwningUIManager->ConfirmChoice() : false;
}

void UVHVDialogueWidget::SelectChoiceAndConfirm(int32 ChoiceIndex)
{
    const FDialogueNode* CurrentNode = FindNodeByID(CurrentNodeID);
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice || !CurrentNode->Choices.IsValidIndex(ChoiceIndex))
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

    if (CurrentNode->NodeType == EVHVDialogueNodeType::Choice)
    {
        if (SelectedChoiceIndex >= CurrentNode->Choices.Num())
        {
            SelectedChoiceIndex = 0;
        }

        EnsureChoiceContainer();

        if (ChoiceContainer)
        {
            for (int32 Index = 0; Index < CurrentNode->Choices.Num(); ++Index)
            {
                const FDialogueChoiceOption& Choice = CurrentNode->Choices[Index];
                UVHVDialogueChoiceButton* ChoiceButton = NewObject<UVHVDialogueChoiceButton>(this);
                ChoiceButton->InitializeChoice(this, Index);

                UTextBlock* ChoiceText = NewObject<UTextBlock>(ChoiceButton);
                ChoiceText->SetText(Choice.OptionText);
                ChoiceText->SetColorAndOpacity(Index == SelectedChoiceIndex ? FLinearColor::White : FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
                ChoiceButton->SetContent(ChoiceText);
                ChoiceContainer->AddChild(ChoiceButton);
            }
        }
    }
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
    if (!CurrentNode || CurrentNode->NodeType != EVHVDialogueNodeType::Choice || !ChoiceContainer)
    {
        return;
    }

    for (int32 Index = 0; Index < ChoiceContainer->GetChildrenCount(); ++Index)
    {
        if (UVHVDialogueChoiceButton* ChoiceButton = Cast<UVHVDialogueChoiceButton>(ChoiceContainer->GetChildAt(Index)))
        {
            if (UTextBlock* ChoiceText = Cast<UTextBlock>(ChoiceButton->GetContent()))
            {
                ChoiceText->SetColorAndOpacity(Index == SelectedChoiceIndex ? FLinearColor::White : FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
            }
        }
    }
}

void UVHVDialogueWidget::ClearChoices()
{
    if (ChoiceContainer)
    {
        ChoiceContainer->ClearChildren();
    }
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
    SelectedChoiceIndex = 0;
    const FDialogueNode* Node = FindNodeByID(NodeID);
    if (!Node)
    {
        HideDialogue();
        return;
    }

    if (Node->NodeType == EVHVDialogueNodeType::Choice && Node->Choices.Num() > 0)
    {
        for (int32 Index = 0; Index < Node->Choices.Num(); ++Index)
        {
            if (Node->Choices[Index].bIsDefault)
            {
                SelectedChoiceIndex = Index;
                break;
            }
        }
    }

    UpdateFromNode();
}

