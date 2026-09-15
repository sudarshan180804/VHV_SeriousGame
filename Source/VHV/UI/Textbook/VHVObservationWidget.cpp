#include "UI/Textbook/VHVObservationWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "UI/VHVUIManagerComponent.h"

UVHVObservationWidget::UVHVObservationWidget()
{
    SetIsFocusable(true);
}

void UVHVObservationWidget::NativeConstruct()
{
    Super::NativeConstruct();

    BuildNativeLayout();
    if (ContinueButton)
    {
        ContinueButton->OnClicked.AddUniqueDynamic(this, &UVHVObservationWidget::SubmitObservation);
    }
    PopulateObservation();
}

FReply UVHVObservationWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        SubmitObservation();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UVHVObservationWidget::SetObservationData(const FTextbookActivityData& InActivity)
{
    CurrentActivity = InActivity;
    bHasObservationData = true;
    PopulateObservation();
}

void UVHVObservationWidget::SetOwningUIManager(UVHVUIManagerComponent* InUIManager)
{
    OwningUIManager = InUIManager;
}

void UVHVObservationWidget::SubmitObservation()
{
    if (OwningUIManager)
    {
        OwningUIManager->SubmitObservation();
    }
}

void UVHVObservationWidget::BuildNativeLayout()
{
    if (!ContentRoot && WidgetTree)
    {
        ContentRoot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ObservationContentRoot"));
        WidgetTree->RootWidget = ContentRoot;
    }

    if (!ContentRoot || ActivityTitle)
    {
        return;
    }

    ActivityTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ActivityTitle"));
    NarrativeContext = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NarrativeContext"));
    PromptText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PromptText"));
    MediaContainer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MediaContainer"));
    ContinueButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ContinueButton"));
    UTextBlock* ConfirmText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ObservationConfirmText"));

    NarrativeContext->SetAutoWrapText(true);
    PromptText->SetAutoWrapText(true);
    ConfirmText->SetText(FText::FromString(TEXT("Continue")));
    ContinueButton->AddChild(ConfirmText);

    ContentRoot->AddChild(ActivityTitle);
    ContentRoot->AddChild(NarrativeContext);
    ContentRoot->AddChild(PromptText);
    ContentRoot->AddChild(MediaContainer);
    ContentRoot->AddChild(ContinueButton);
}

void UVHVObservationWidget::PopulateObservation()
{
    if (!bHasObservationData || !ContentRoot)
    {
        return;
    }

    if (ActivityTitle)
    {
        ActivityTitle->SetText(FText::FromString(CurrentActivity.ActivityTitle));
    }
    if (NarrativeContext)
    {
        NarrativeContext->SetText(FText::FromString(CurrentActivity.NarrativeContext));
    }
    if (PromptText)
    {
        PromptText->SetText(FText::FromString(CurrentActivity.PromptText));
    }
    if (MediaContainer)
    {
        MediaContainer->ClearChildren();
        for (const FTextbookMediaReference& Media : CurrentActivity.Media)
        {
            UTextBlock* MediaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            MediaText->SetAutoWrapText(true);
            MediaText->SetText(FText::FromString(Media.Description));
            MediaContainer->AddChild(MediaText);
        }
    }
}
