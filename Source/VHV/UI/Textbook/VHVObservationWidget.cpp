#include "UI/Textbook/VHVObservationWidget.h"

#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UI/VHVUIManagerComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
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
    constexpr float BoardLeft = 0.190f;
    constexpr float BoardRight = 0.820f;
    constexpr float HeaderLeft = 0.295f;
    constexpr float HeaderTop = 0.145f;
    constexpr float HeaderRight = 0.745f;
    constexpr float HeaderBottom = 0.355f;
    constexpr float ItemsLeft = 0.205f;
    constexpr float ItemsTop = 0.370f;
    constexpr float ItemsRight = 0.815f;
    constexpr float ItemsBottom = 0.680f;
    constexpr float BoardActionY = 0.735f;
    constexpr float ContinueWidth = 190.0f;
    constexpr float ContinueHeight = 52.0f;
    constexpr float ContinueClusterWidth = 314.0f;
    constexpr float CardMinHeight = 152.0f;

    FString CleanText(const FString& Text)
    {
        return Text.TrimStartAndEnd();
    }

    bool TextMatches(const FString& A, const FString& B)
    {
        const FString CleanA = CleanText(A);
        const FString CleanB = CleanText(B);
        return !CleanA.IsEmpty() && CleanA.Equals(CleanB, ESearchCase::IgnoreCase);
    }

    bool ContainsEquivalent(const TArray<FString>& Texts, const FString& Candidate)
    {
        return Texts.ContainsByPredicate(
            [&Candidate](const FString& Existing)
            {
                return TextMatches(Existing, Candidate);
            });
    }
}

UVHVObservationWidget::UVHVObservationWidget()
{
    SetIsFocusable(true);

    static ConstructorHelpers::FObjectFinder<UTexture2D> BackgroundFinder(
        TEXT("/Game/VHV_Stuff/UI/Backgrounds/T_BG_ObservationBoard.T_BG_ObservationBoard"));
    if (BackgroundFinder.Succeeded())
    {
        ObservationBackgroundTexture = BackgroundFinder.Object;
    }
}

TSharedRef<SWidget> UVHVObservationWidget::RebuildWidget()
{
    BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
    BackgroundBrush.SetResourceObject(ObservationBackgroundTexture);
    if (ObservationBackgroundTexture)
    {
        BackgroundBrush.SetImageSize(FVector2D(
            ObservationBackgroundTexture->GetSizeX(), ObservationBackgroundTexture->GetSizeY()));
    }

    HeaderDividerBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::DividerGold(), 1.0f);
    CardSurfaceBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::MatchingPaperCard(), 18.0f,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);
    CardAccentBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::PositiveMuted().CopyWithNewOpacity(0.72f), 2.0f);
    CardShadowBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::MatchingShadow().CopyWithNewOpacity(0.12f), 20.0f);
    KeycapBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(248, 244, 234, 225), 5.0f,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);

    ContinueButtonStyle = FButtonStyle()
        .SetNormal(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.94f),
            ContinueHeight * 0.5f,
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
            .Anchors(FAnchors(HeaderLeft, HeaderTop, HeaderRight, HeaderBottom))
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
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(16))
                        .ColorAndOpacity(VHVActivityUIStyle::HeaderGold())
                        .Text(FText::FromString(TEXT("OBSERVATION")))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Fill)
                    .Padding(FMargin(0.0f, 5.0f, 0.0f, 0.0f))
                    [
                        SAssignNew(PrimaryTitleSlate, STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(37))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                        .AutoWrapText(true)
                        .LineHeightPercentage(1.05f)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Left)
                    .Padding(FMargin(0.0f, 13.0f, 0.0f, 17.0f))
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
                        SAssignNew(SecondaryTextSlate, SVerticalBox)
                    ]
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(ItemsLeft, ItemsTop, ItemsRight, ItemsBottom))
            .Offset(FMargin(0.0f))
            [
                SNew(SScrollBox)
                .Orientation(Orient_Vertical)
                .ScrollBarThickness(FVector2D(3.0f, 3.0f))
                + SScrollBox::Slot()
                .Padding(FMargin(0.0f, 6.0f, 0.0f, 8.0f))
                [
                    SAssignNew(ObservationItemsSlate, SVerticalBox)
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(BoardLeft, BoardActionY))
            .Alignment(FVector2D(0.0f, 0.5f))
            .Offset(FMargin(0.0f, 0.0f, 330.0f, 34.0f))
            [
                SNew(STextBlock)
                .Font(VHVActivityUIStyle::RegularFont(13))
                .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted().CopyWithNewOpacity(0.72f))
                .Text(FText::FromString(TEXT("Take a moment to review the information.")))
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
                        .Font(VHVActivityUIStyle::MediumFont(11))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                        .Text(FText::FromString(TEXT("ENTER / SPACE")))
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
                        .OnClicked(FOnClicked::CreateUObject(
                            this, &UVHVObservationWidget::HandleContinueClicked))
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

void UVHVObservationWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    ActivityContentSlate.Reset();
    PrimaryTitleSlate.Reset();
    SecondaryTextSlate.Reset();
    ObservationItemsSlate.Reset();
    ObservationCardsSlate.Reset();
    MediaBrushes.Reset();
    LoadedMediaTextures.Reset();
}

void UVHVObservationWidget::NativeConstruct()
{
    Super::NativeConstruct();
    EntranceElapsed = 0.0f;
    if (ActivityContentSlate)
    {
        ActivityContentSlate->SetRenderOpacity(0.0f);
        ActivityContentSlate->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, 10.0f)));
    }
    if (ContinueButton)
    {
        ContinueButton->OnClicked.AddUniqueDynamic(this, &UVHVObservationWidget::SubmitObservation);
    }
    PopulateObservation();
}

void UVHVObservationWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    EntranceElapsed = FMath::Min(
        EntranceElapsed + InDeltaTime,
        VHVActivityUIStyle::AnimationStandard
            + ObservationCardsSlate.Num() * VHVActivityUIStyle::AnimationStagger);
    const float Raw = FMath::Clamp(
        EntranceElapsed / VHVActivityUIStyle::AnimationStandard, 0.0f, 1.0f);
    const float Smoothed = FMath::InterpEaseOut(0.0f, 1.0f, Raw, 3.0f);
    if (ActivityContentSlate)
    {
        ActivityContentSlate->SetRenderOpacity(Smoothed);
        ActivityContentSlate->SetRenderTransform(FSlateRenderTransform(
            FVector2D(0.0f, FMath::Lerp(10.0f, 0.0f, Smoothed))));
    }

    for (int32 Index = 0; Index < ObservationCardsSlate.Num(); ++Index)
    {
        if (!ObservationCardsSlate[Index])
        {
            continue;
        }
        const float CardRaw = FMath::Clamp(
            (EntranceElapsed - Index * VHVActivityUIStyle::AnimationStagger)
                / VHVActivityUIStyle::AnimationStandard,
            0.0f, 1.0f);
        const float CardAlpha = FMath::InterpEaseOut(0.0f, 1.0f, CardRaw, 3.0f);
        ObservationCardsSlate[Index]->SetRenderOpacity(CardAlpha);
        ObservationCardsSlate[Index]->SetRenderTransform(FSlateRenderTransform(
            FVector2D(0.0f, FMath::Lerp(7.0f, 0.0f, CardAlpha))));
    }
}

FReply UVHVObservationWidget::NativeOnKeyDown(
    const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar
        || Key == EKeys::Gamepad_FaceButton_Bottom)
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
    EntranceElapsed = 0.0f;
    if (ActivityContentSlate)
    {
        ActivityContentSlate->SetRenderOpacity(0.0f);
        ActivityContentSlate->SetRenderTransform(FSlateRenderTransform(FVector2D(0.0f, 10.0f)));
    }
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

FReply UVHVObservationWidget::HandleContinueClicked()
{
    SubmitObservation();
    SetKeyboardFocus();
    return FReply::Handled();
}

void UVHVObservationWidget::PopulateObservation()
{
    if (!bHasObservationData)
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
    }

    RefreshSlateContent();
}

void UVHVObservationWidget::RefreshSlateContent()
{
    if (!PrimaryTitleSlate || !SecondaryTextSlate || !ObservationItemsSlate)
    {
        return;
    }

    SecondaryTextSlate->ClearChildren();
    ObservationItemsSlate->ClearChildren();
    ObservationCardsSlate.Reset();
    MediaBrushes.Reset();
    LoadedMediaTextures.Reset();

    const FString ActivityTitleText = CleanText(CurrentActivity.ActivityTitle);
    const FString Prompt = CleanText(CurrentActivity.PromptText);
    const FString Narrative = CleanText(CurrentActivity.NarrativeContext);
    const bool bGenericActivityTitle = ActivityTitleText.IsEmpty()
        || ActivityTitleText.Equals(TEXT("Observation"), ESearchCase::IgnoreCase);

    FString PrimaryText;
    if (!bGenericActivityTitle)
    {
        PrimaryText = ActivityTitleText;
    }
    else if (!Prompt.IsEmpty())
    {
        PrimaryText = Prompt;
    }
    else if (!Narrative.IsEmpty())
    {
        PrimaryText = Narrative;
    }
    else
    {
        PrimaryText = ActivityTitleText;
    }
    PrimaryTitleSlate->SetText(FText::FromString(PrimaryText));

    TArray<FString> PresentedTexts;
    if (!PrimaryText.IsEmpty())
    {
        PresentedTexts.Add(PrimaryText);
    }

    TArray<FString> SecondaryTexts;
    const auto AddSecondaryIfUnique = [&PresentedTexts, &SecondaryTexts](const FString& Candidate)
    {
        const FString CleanCandidate = CleanText(Candidate);
        if (!CleanCandidate.IsEmpty() && !ContainsEquivalent(PresentedTexts, CleanCandidate))
        {
            SecondaryTexts.Add(CleanCandidate);
            PresentedTexts.Add(CleanCandidate);
        }
    };
    if (!bGenericActivityTitle)
    {
        AddSecondaryIfUnique(Prompt);
    }
    AddSecondaryIfUnique(Narrative);

    for (int32 Index = 0; Index < SecondaryTexts.Num(); ++Index)
    {
        SecondaryTextSlate->AddSlot()
            .AutoHeight()
            .Padding(FMargin(0.0f, Index == 0 ? 0.0f : 8.0f, 0.0f, 0.0f))
            [
                SNew(STextBlock)
                .Font(VHVActivityUIStyle::RegularFont(18))
                .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                .AutoWrapText(true)
                .LineHeightPercentage(1.22f)
                .Text(FText::FromString(SecondaryTexts[Index]))
            ];
    }

    struct FObservationItem
    {
        FString Description;
        int32 BrushIndex = INDEX_NONE;
    };
    TArray<FObservationItem> Items;
    MediaBrushes.Reserve(CurrentActivity.Media.Num());
    LoadedMediaTextures.Reserve(CurrentActivity.Media.Num());

    for (const FTextbookMediaReference& Media : CurrentActivity.Media)
    {
        const FString Description = CleanText(Media.Description);
        int32 BrushIndex = INDEX_NONE;
        if ((Media.MediaType == ETextbookMediaType::Image
                || Media.MediaType == ETextbookMediaType::Diagram)
            && Media.Asset.IsValid())
        {
            TSoftObjectPtr<UTexture2D> SoftTexture(Media.Asset);
            if (UTexture2D* Texture = SoftTexture.LoadSynchronous())
            {
                LoadedMediaTextures.Add(Texture);
                FSlateBrush& Brush = MediaBrushes.AddDefaulted_GetRef();
                Brush.DrawAs = ESlateBrushDrawType::Image;
                Brush.SetResourceObject(Texture);
                Brush.SetImageSize(FVector2D(Texture->GetSizeX(), Texture->GetSizeY()));
                BrushIndex = MediaBrushes.Num() - 1;
            }
        }

        const bool bDescriptionIsNew = !Description.IsEmpty()
            && !ContainsEquivalent(PresentedTexts, Description);
        if (BrushIndex == INDEX_NONE && !bDescriptionIsNew)
        {
            continue;
        }

        FObservationItem& Item = Items.AddDefaulted_GetRef();
        Item.Description = bDescriptionIsNew ? Description : FString();
        Item.BrushIndex = BrushIndex;
        if (bDescriptionIsNew)
        {
            PresentedTexts.Add(Description);
        }
    }

    if (Items.IsEmpty())
    {
        return;
    }

    const int32 ColumnCount = Items.Num() == 4 ? 2 : FMath::Min(Items.Num(), 3);
    TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel)
        .SlotPadding(FMargin(7.0f));
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        TSharedRef<SVerticalBox> ItemContent = SNew(SVerticalBox);
        if (Items[Index].BrushIndex != INDEX_NONE)
        {
            ItemContent->AddSlot()
                .AutoHeight()
                [
                    SNew(SBox)
                    .HeightOverride(150.0f)
                    [
                        SNew(SScaleBox)
                        .Stretch(EStretch::ScaleToFit)
                        .StretchDirection(EStretchDirection::Both)
                        [
                            SNew(SImage)
                            .Image(&MediaBrushes[Items[Index].BrushIndex])
                        ]
                    ]
                ];
        }
        if (!Items[Index].Description.IsEmpty())
        {
            ItemContent->AddSlot()
                .AutoHeight()
                .Padding(FMargin(2.0f,
                    Items[Index].BrushIndex == INDEX_NONE ? 2.0f : 12.0f,
                    2.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(16))
                    .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                    .AutoWrapText(true)
                    .LineHeightPercentage(1.18f)
                    .Text(FText::FromString(Items[Index].Description))
                ];
        }

        TSharedPtr<SBox> CardBox;
        TSharedRef<SWidget> Card =
            SAssignNew(CardBox, SBox)
            .MinDesiredHeight(CardMinHeight)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                .Padding(FMargin(3.0f, 4.0f, -3.0f, -4.0f))
                [
                    SNew(SBorder)
                    .BorderImage(&CardShadowBrush)
                ]
                + SOverlay::Slot()
                [
                    SNew(SBorder)
                    .BorderImage(&CardSurfaceBrush)
                    .Padding(FMargin(18.0f, 16.0f))
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(FMargin(0.0f, 2.0f, 13.0f, 2.0f))
                        [
                            SNew(SBox)
                            .WidthOverride(3.0f)
                            [
                                SNew(SBorder)
                                .BorderImage(&CardAccentBrush)
                            ]
                        ]
                        + SHorizontalBox::Slot()
                        .FillWidth(1.0f)
                        [
                            ItemContent
                        ]
                    ]
                ]
            ];
        CardBox->SetRenderOpacity(0.0f);
        ObservationCardsSlate.Add(CardBox);

        Grid->AddSlot(Index % ColumnCount, Index / ColumnCount)
            [
                Card
            ];
    }

    ObservationItemsSlate->AddSlot()
        .AutoHeight()
        [
            Grid
        ];
}
