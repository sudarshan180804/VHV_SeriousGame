#include "UI/Textbook/VHVTeachWidget.h"

#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UI/VHVUIManagerComponent.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    constexpr float BoardLeft = 0.225f;
    constexpr float BoardTop = 0.120f;
    constexpr float BoardRight = 0.820f;
    constexpr float BoardContentBottom = 0.690f;
    constexpr float BoardActionY = 0.728f;
    constexpr float ContinueWidth = 190.0f;
    constexpr float ContinueClusterWidth = 262.0f;
    constexpr float ContinueHeight = 52.0f;
    constexpr float TakeawayCardMinHeight = 116.0f;

    FText TeachingCategoryText(const ETextbookTeachingCategory Category)
    {
        switch (Category)
        {
        case ETextbookTeachingCategory::KeyIdea:
            return FText::FromString(TEXT("KEY IDEA"));
        case ETextbookTeachingCategory::Technique:
            return FText::FromString(TEXT("TECHNIQUE"));
        case ETextbookTeachingCategory::Reflection:
            return FText::FromString(TEXT("REFLECTION"));
        case ETextbookTeachingCategory::Lesson:
        default:
            return FText::FromString(TEXT("LESSON"));
        }
    }
}

UVHVTeachWidget::UVHVTeachWidget()
{
    SetIsFocusable(true);

    static ConstructorHelpers::FObjectFinder<UTexture2D> BackgroundFinder(
        TEXT("/Game/VHV_Stuff/UI/Backgrounds/T_BG_LessonBoard.T_BG_LessonBoard"));
    if (BackgroundFinder.Succeeded())
    {
        LessonBackgroundTexture = BackgroundFinder.Object;
    }
}

TSharedRef<SWidget> UVHVTeachWidget::RebuildWidget()
{
    BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
    BackgroundBrush.SetResourceObject(LessonBackgroundTexture);
    if (LessonBackgroundTexture)
    {
        BackgroundBrush.SetImageSize(FVector2D(
            LessonBackgroundTexture->GetSizeX(), LessonBackgroundTexture->GetSizeY()));
    }

    HeaderDividerBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::DividerGold(), 1.0f);
    TakeawayGreenBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(222, 232, 215, 238), 16.0f,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);
    TakeawayGoldBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(244, 231, 199, 238), 16.0f,
        VHVActivityUIStyle::DividerGold().CopyWithNewOpacity(0.40f),
        VHVActivityUIStyle::BorderNormalWidth);
    TakeawayTealBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(216, 231, 228, 238), 16.0f,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);
    TakeawayShadowBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(47, 39, 26, 28), 17.0f);
    MediaSurfaceBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(250, 246, 236, 238), 15.0f,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);
    KeycapBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(248, 244, 234, 225), 5.0f,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);

    const FLinearColor ButtonGold = VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.94f);
    ContinueButtonStyle = FButtonStyle()
        .SetNormal(VHVActivityUIStyle::RoundedBrush(
            ButtonGold, ContinueHeight * 0.5f,
            VHVActivityUIStyle::GoldSelected(), VHVActivityUIStyle::BorderNormalWidth))
        .SetHovered(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::GoldSelected(), ContinueHeight * 0.5f,
            VHVActivityUIStyle::MatchingInk().CopyWithNewOpacity(0.30f),
            VHVActivityUIStyle::BorderNormalWidth))
        .SetPressed(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::FromSRGB(202, 161, 79), ContinueHeight * 0.5f,
            VHVActivityUIStyle::MatchingInk().CopyWithNewOpacity(0.42f),
            VHVActivityUIStyle::BorderNormalWidth))
        .SetNormalPadding(FMargin(0.0f))
        .SetPressedPadding(FMargin(1.0f, 2.0f, 0.0f, 0.0f));

    TSharedRef<SWidget> Result =
        SNew(SOverlay)
        + SOverlay::Slot()
        [
            // ScaleToFill preserves the authored 16:9 image aspect and crops
            // excess at non-matching viewport ratios; it never stretches it.
            SNew(SScaleBox)
            .Stretch(EStretch::ScaleToFill)
            .Clipping(EWidgetClipping::ClipToBounds)
            [
                SNew(SImage)
                .Image(&BackgroundBrush)
            ]
        ]
        + SOverlay::Slot()
        [
            SAssignNew(ActivityContentSlate, SConstraintCanvas)
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(BoardLeft, BoardTop, BoardRight, BoardContentBottom))
            .Offset(FMargin(0.0f))
            [
                SNew(SScrollBox)
                .Orientation(Orient_Vertical)
                .ScrollBarThickness(FVector2D(3.0f, 3.0f))
                + SScrollBox::Slot()
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Left)
                    [
                        SAssignNew(CategoryTextSlate, STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(16))
                        .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
                        .Text(TeachingCategoryText(TeachingContent.Category))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Fill)
                    .Padding(FMargin(0.0f, 5.0f, 0.0f, 0.0f))
                    [
                        SAssignNew(TitleTextSlate, STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(38))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                        .AutoWrapText(true)
                        .LineHeightPercentage(1.04f)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Left)
                    .Padding(FMargin(0.0f, 13.0f, 0.0f, 18.0f))
                    [
                        SNew(SBox)
                        .WidthOverride(280.0f)
                        .HeightOverride(VHVActivityUIStyle::HeaderDividerHeight)
                        [
                            SNew(SBorder)
                            .BorderImage(&HeaderDividerBrush)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(ReadingAreaSlate, SVerticalBox)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(FMargin(0.0f, 24.0f, 0.0f, 8.0f))
                    [
                        SAssignNew(TakeawaysSectionSlate, SVerticalBox)
                    ]
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(BoardRight, BoardActionY))
            .Alignment(FVector2D(1.0f, 0.5f))
            .Offset(FMargin(0.0f, 0.0f, ContinueClusterWidth, ContinueHeight))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
                [
                    SNew(SBorder)
                    .BorderImage(&KeycapBrush)
                    .Padding(FMargin(7.0f, 3.0f))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(12))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                        .Text(FText::FromString(TEXT("ENTER")))
                    ]
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBox)
                    .WidthOverride(ContinueWidth)
                    .HeightOverride(ContinueHeight)
                    [
                        SNew(SButton)
                        .ButtonStyle(&ContinueButtonStyle)
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .ContentPadding(FMargin(22.0f, 10.0f))
                        .OnClicked(FOnClicked::CreateUObject(this, &UVHVTeachWidget::HandleContinueClicked))
                        [
                            SNew(STextBlock)
                            .Font(VHVActivityUIStyle::MediumFont(16))
                            .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                            .Text(FText::FromString(TEXT("Continue  \u2192")))
                        ]
                    ]
                ]
            ]
        ];

    RefreshSlateContent();
    return Result;
}

void UVHVTeachWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    ActivityContentSlate.Reset();
    CategoryTextSlate.Reset();
    TitleTextSlate.Reset();
    ContentTextSlate.Reset();
    ReadingAreaSlate.Reset();
    TakeawaysSectionSlate.Reset();
    TakeawayCardsSlate.Reset();
    MediaBrushes.Reset();
    LoadedMediaTextures.Reset();
}

void UVHVTeachWidget::SetOwningUIManager(UVHVUIManagerComponent* InUIManager)
{
    OwningUIManager = InUIManager;
}

void UVHVTeachWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EntranceElapsed = 0.0f;
    if (ActivityContentSlate)
    {
        ActivityContentSlate->SetRenderOpacity(0.0f);
    }

    if (bHasTeachingContent)
    {
        PopulateTeachingContent();
    }
}

void UVHVTeachWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    EntranceElapsed = FMath::Min(
        EntranceElapsed + InDeltaTime,
        VHVActivityUIStyle::AnimationStandard
            + TakeawayCardsSlate.Num() * VHVActivityUIStyle::AnimationStagger);
    const float Raw = FMath::Clamp(
        EntranceElapsed / VHVActivityUIStyle::AnimationStandard, 0.0f, 1.0f);
    const float Smoothed = FMath::InterpEaseOut(0.0f, 1.0f, Raw, 3.0f);
    if (ActivityContentSlate)
    {
        ActivityContentSlate->SetRenderOpacity(Smoothed);
        ActivityContentSlate->SetRenderTransform(FSlateRenderTransform(
            FVector2D(0.0f, FMath::Lerp(10.0f, 0.0f, Smoothed))));
    }

    for (int32 Index = 0; Index < TakeawayCardsSlate.Num(); ++Index)
    {
        if (!TakeawayCardsSlate[Index])
        {
            continue;
        }
        const float CardRaw = FMath::Clamp(
            (EntranceElapsed - Index * VHVActivityUIStyle::AnimationStagger)
                / VHVActivityUIStyle::AnimationStandard,
            0.0f, 1.0f);
        const float CardAlpha = FMath::InterpEaseOut(0.0f, 1.0f, CardRaw, 3.0f);
        TakeawayCardsSlate[Index]->SetRenderOpacity(CardAlpha);
        TakeawayCardsSlate[Index]->SetRenderTransform(FSlateRenderTransform(
            FVector2D(0.0f, FMath::Lerp(7.0f, 0.0f, CardAlpha))));
    }
}

void UVHVTeachWidget::SetTeachingContent(const FTeachingContent& InTeaching)
{
    TeachingContent = InTeaching;
    bHasTeachingContent = true;
    EntranceElapsed = 0.0f;
    if (ActivityContentSlate)
    {
        ActivityContentSlate->SetRenderOpacity(0.0f);
        ActivityContentSlate->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, 10.0f)));
    }

    // A widget can receive its data before its tree has been constructed.
    if (TitleText || ContentText || KeyTakeawaysContainer || MediaContainer
        || TitleTextSlate || ReadingAreaSlate)
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

    // Keep the legacy Blueprint containers synchronized for editor previews.
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
    }

    RefreshSlateContent();
}

void UVHVTeachWidget::RefreshSlateContent()
{
    if (CategoryTextSlate)
    {
        CategoryTextSlate->SetText(TeachingCategoryText(TeachingContent.Category));
    }
    if (TitleTextSlate)
    {
        TitleTextSlate->SetText(FText::FromString(TeachingContent.Title));
    }
    if (!ReadingAreaSlate || !TakeawaysSectionSlate)
    {
        return;
    }

    ReadingAreaSlate->ClearChildren();
    TakeawaysSectionSlate->ClearChildren();
    TakeawayCardsSlate.Reset();
    MediaBrushes.Reset();
    LoadedMediaTextures.Reset();

    ContentTextSlate = SNew(STextBlock)
        .Font(VHVActivityUIStyle::RegularFont(20))
        .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
        .AutoWrapText(true)
        .LineHeightPercentage(1.28f)
        .Text(FText::FromString(TeachingContent.Content));

    TArray<const FTextbookMediaReference*> ValidMedia;
    MediaBrushes.Reserve(TeachingContent.Media.Num());
    LoadedMediaTextures.Reserve(TeachingContent.Media.Num());
    for (const FTextbookMediaReference& Media : TeachingContent.Media)
    {
        if ((Media.MediaType != ETextbookMediaType::Image
                && Media.MediaType != ETextbookMediaType::Diagram)
            || !Media.Asset.IsValid())
        {
            continue;
        }

        TSoftObjectPtr<UTexture2D> SoftTexture(Media.Asset);
        UTexture2D* Texture = SoftTexture.LoadSynchronous();
        if (!Texture)
        {
            continue;
        }

        LoadedMediaTextures.Add(Texture);
        FSlateBrush& Brush = MediaBrushes.AddDefaulted_GetRef();
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.SetResourceObject(Texture);
        Brush.SetImageSize(FVector2D(Texture->GetSizeX(), Texture->GetSizeY()));
        ValidMedia.Add(&Media);
    }

    TSharedRef<SHorizontalBox> ReadingRow = SNew(SHorizontalBox);
    ReadingRow->AddSlot()
        .FillWidth(ValidMedia.IsEmpty() ? 1.0f : 0.62f)
        .VAlign(VAlign_Top)
        [
            ContentTextSlate.ToSharedRef()
        ];

    if (!ValidMedia.IsEmpty())
    {
        TSharedRef<SVerticalBox> MediaList = SNew(SVerticalBox);
        for (int32 Index = 0; Index < ValidMedia.Num(); ++Index)
        {
            TSharedRef<SVerticalBox> MediaContent = SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SBox)
                    .HeightOverride(176.0f)
                    [
                        SNew(SScaleBox)
                        .Stretch(EStretch::ScaleToFit)
                        .StretchDirection(EStretchDirection::Both)
                        [
                            SNew(SImage)
                            .Image(&MediaBrushes[Index])
                        ]
                    ]
                ];

            if (!ValidMedia[Index]->Description.IsEmpty())
            {
                MediaContent->AddSlot()
                    .AutoHeight()
                    .Padding(FMargin(2.0f, 8.0f, 2.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(13))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                        .AutoWrapText(true)
                        .Text(FText::FromString(ValidMedia[Index]->Description))
                    ];
            }

            MediaList->AddSlot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 0.0f, 0.0f,
                    Index + 1 == ValidMedia.Num() ? 0.0f : 12.0f))
                [
                    SNew(SBorder)
                    .BorderImage(&MediaSurfaceBrush)
                    .Padding(FMargin(10.0f))
                    [
                        MediaContent
                    ]
                ];
        }

        ReadingRow->AddSlot()
            .FillWidth(0.38f)
            .VAlign(VAlign_Top)
            .Padding(FMargin(28.0f, 0.0f, 5.0f, 0.0f))
            [
                MediaList
            ];
    }

    ReadingAreaSlate->AddSlot()
        .AutoHeight()
        [
            ReadingRow
        ];

    TArray<FString> NonEmptyTakeaways;
    for (const FString& Takeaway : TeachingContent.KeyTakeaways)
    {
        if (!Takeaway.TrimStartAndEnd().IsEmpty())
        {
            NonEmptyTakeaways.Add(Takeaway);
        }
    }
    if (NonEmptyTakeaways.IsEmpty())
    {
        return;
    }

    TakeawaysSectionSlate->AddSlot()
        .AutoHeight()
        .HAlign(HAlign_Left)
        .Padding(FMargin(0.0f, 0.0f, 0.0f, 11.0f))
        [
            SNew(STextBlock)
            .Font(VHVActivityUIStyle::MediumFont(15))
            .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
            .Text(FText::FromString(TEXT("KEY TAKEAWAYS")))
        ];

    TSharedRef<SUniformGridPanel> TakeawayGrid = SNew(SUniformGridPanel)
        .SlotPadding(FMargin(6.0f));
    FSlateBrush* CardBrushes[] =
    {
        &TakeawayGreenBrush,
        &TakeawayGoldBrush,
        &TakeawayTealBrush
    };
    for (int32 Index = 0; Index < NonEmptyTakeaways.Num(); ++Index)
    {
        TSharedPtr<SBox> CardBox;
        TSharedRef<SWidget> Card =
            SAssignNew(CardBox, SBox)
            .MinDesiredHeight(TakeawayCardMinHeight)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                .Padding(FMargin(2.0f, 3.0f, -2.0f, -3.0f))
                [
                    SNew(SBorder)
                    .BorderImage(&TakeawayShadowBrush)
                ]
                + SOverlay::Slot()
                [
                    SNew(SBorder)
                    .BorderImage(CardBrushes[Index % UE_ARRAY_COUNT(CardBrushes)])
                    .Padding(FMargin(18.0f, 15.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(FMargin(0.0f, 0.0f, 0.0f, 8.0f))
                        [
                            SNew(STextBlock)
                            .Font(VHVActivityUIStyle::MediumFont(12))
                            .ColorAndOpacity(VHVActivityUIStyle::GoldPrimary())
                            .Text(FText::AsNumber(Index + 1))
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                            .Font(VHVActivityUIStyle::RegularFont(16))
                            .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                            .AutoWrapText(true)
                            .LineHeightPercentage(1.18f)
                            .Text(FText::FromString(NonEmptyTakeaways[Index]))
                        ]
                    ]
                ]
            ];
        CardBox->SetRenderOpacity(0.0f);
        TakeawayCardsSlate.Add(CardBox);

        const int32 Column = Index % 3;
        const int32 Row = Index / 3;
        TakeawayGrid->AddSlot(Column, Row)
            [
                Card
            ];
    }

    TakeawaysSectionSlate->AddSlot()
        .AutoHeight()
        [
            TakeawayGrid
        ];
}

FReply UVHVTeachWidget::HandleContinueClicked()
{
    if (OwningUIManager)
    {
        // Use the same manager input route as the existing Enter binding. It
        // preserves phase checks, progression, and quest-managed behavior.
        OwningUIManager->ConfirmChoiceInput();
    }
    SetKeyboardFocus();
    return FReply::Handled();
}

void UVHVTeachWidget::SetTeachingData(const FTeachingContent& InTeaching)
{
    SetTeachingContent(InTeaching);
}
