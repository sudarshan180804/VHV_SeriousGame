// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/Textbook/VHVTeachWidget.h"
#include "UI/VHVUIManagerComponent.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UVHVTeachWidget::SetOwningUIManager(UVHVUIManagerComponent* InUIManager)
{
    OwningUIManager = InUIManager;
}

void UVHVTeachWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (bHasTeachingContent)
    {
        PopulateTeachingContent();
    }
}

void UVHVTeachWidget::SetTeachingContent(const FTeachingContent& InTeaching)
{
    TeachingContent = InTeaching;
    bHasTeachingContent = true;

    // A widget can receive its data before its Blueprint widget tree has been
    // constructed. NativeConstruct will populate the cached value in that case.
    if (TitleText || ContentText || KeyTakeawaysContainer || MediaContainer)
    {
        PopulateTeachingContent();
    }
}

void UVHVTeachWidget::PopulateTeachingContent()
{
    if (TitleText)
    {
        TitleText->SetText(FText::FromString(TeachingContent.Title));
    }

    if (ContentText)
    {
        ContentText->SetText(FText::FromString(TeachingContent.Content));
    }

    if (KeyTakeawaysContainer)
    {
        KeyTakeawaysContainer->ClearChildren();
        for (const FString& Takeaway : TeachingContent.KeyTakeaways)
        {
            UTextBlock* TakeawayText = NewObject<UTextBlock>(this);
            if (!TakeawayText)
            {
                continue;
            }

            TakeawayText->SetText(FText::FromString(Takeaway));
            TakeawayText->SetAutoWrapText(true);
            KeyTakeawaysContainer->AddChild(TakeawayText);
        }
    }

    if (MediaContainer)
    {
        MediaContainer->ClearChildren();
        for (const FTextbookMediaReference& Media : TeachingContent.Media)
        {
            if ((Media.MediaType != ETextbookMediaType::Image && Media.MediaType != ETextbookMediaType::Diagram) || !Media.Asset.IsValid())
            {
                continue;
            }

            TSoftObjectPtr<UTexture2D> SoftTexture(Media.Asset);
            UTexture2D* Texture = SoftTexture.LoadSynchronous();
            if (!Texture)
            {
                continue;
            }

            UImage* MediaImage = NewObject<UImage>(this);
            if (!MediaImage)
            {
                continue;
            }

            MediaImage->SetBrushFromTexture(Texture, true);
            MediaImage->SetVisibility(ESlateVisibility::Visible);
            MediaImage->SetColorAndOpacity(FLinearColor::White);
            MediaContainer->AddChild(MediaImage);
        }
    }
}

void UVHVTeachWidget::SetTeachingData(const FTeachingContent& InTeaching)
{
    SetTeachingContent(InTeaching);
}

