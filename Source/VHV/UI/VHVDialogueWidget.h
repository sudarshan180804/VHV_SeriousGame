#pragma once

#include "CoreMinimal.h"
#include "Core/VHVDialogueTypes.h"
#include "UI/VHVUserWidgetBase.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "VHVDialogueWidget.generated.h"

class UVHVUIManagerComponent;
class SBox;
class SConstraintCanvas;
class STextBlock;
class SVerticalBox;
class SVHVChoiceCard;

UCLASS()
class VHV_API UVHVDialogueWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVDialogueWidget();

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void ShowDialogue(const FDialogueData& InDialogue);

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void ShowConversation(const FDialogueConversation& InConversation, const FString& InStartNodeID = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void HideDialogue();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void AdvanceDialogue();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void SelectNextChoice();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void SelectPreviousChoice();

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    bool ConfirmChoice();

    /** Selects a rendered choice and asks the conversation owner to confirm it. */
    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void SelectChoiceAndConfirm(int32 ChoiceIndex);

    UFUNCTION(BlueprintCallable, Category = "VHV|UI")
    void SetOwningUIManager(UVHVUIManagerComponent* InUIManager);

    const FDialogueConversation& GetCurrentConversation() const { return CurrentConversation; }
    const FString& GetCurrentNodeID() const { return CurrentNodeID; }
    int32 GetSelectedChoiceIndex() const { return SelectedChoiceIndex; }

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    void UpdateDialogueLine();
    void UpdateFromNode();
    void UpdateChoiceSelectionUI();
    void EnsureChoiceContainer();
    void ClearChoices();
    void SetActivityChoicePresentation(bool bEnabled);
    void SetPlayerThoughtPresentation(bool bEnabled, const FText& Text = FText::GetEmpty());
    void RefreshNormalPresentationVisibility();
    void UpdateNormalDialogueText(const FText& SpeakerName, const FText& Text);
    void RebuildActivityChoiceCards();
    void UpdateActivityChoicePresentation();
    void HandleActivityChoiceCardChosen(int32 ChoiceIndex);
    const FDialogueNode* FindNodeByID(const FString& NodeID) const;
    void SetCurrentNode(const FString& NodeID);

    FDialogueConversation CurrentConversation;
    FString CurrentNodeID;
    FDialogueData CurrentDialogue;
    int32 CurrentLineIndex = 0;
    int32 SelectedChoiceIndex = INDEX_NONE;
    TArray<int32> AvailableChoiceIndices;
    TArray<TSharedPtr<SVHVChoiceCard>> ActivityChoiceCards;
    bool bActivityChoicePresentation = false;
    bool bPlayerThoughtPresentation = false;
    float ActivityEntranceElapsed = 0.0f;

    TSharedPtr<SBox> NormalDialoguePresentation;
    TSharedPtr<STextBlock> NormalSpeakerText;
    TSharedPtr<STextBlock> NormalDialogueText;
    TSharedPtr<SWidget> NormalSpeakerRow;
    TSharedPtr<SConstraintCanvas> ActivityRootCanvas;
    TSharedPtr<STextBlock> ActivitySpeakerText;
    TSharedPtr<STextBlock> ActivityPromptText;
    TSharedPtr<SVerticalBox> ActivityChoiceContainer;
    TSharedPtr<SWidget> ActivityQuestionPanel;
    TSharedPtr<SWidget> ActivityAnswerPanel;
    FSlateBrush ActivityQuestionPanelBrush;
    FSlateBrush ActivityQuestionShadowBrush;
    FSlateBrush ActivityKeycapBrush;
    FSlateBrush ActivityEmblemBrush;
    FSlateBrush ActivityDividerBrush;
    FSlateBrush DialogueCardBrush;
    FSlateBrush DialogueShadowBrush;
    FSlateBrush DialogueKeycapBrush;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> SpeakerNameText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget))
    TObjectPtr<UTextBlock> DialogueText;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> ChoiceContainer;

    UPROPERTY()
    TObjectPtr<UVHVUIManagerComponent> OwningUIManager;
};
