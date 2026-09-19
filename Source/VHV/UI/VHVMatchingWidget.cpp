#include "UI/VHVMatchingWidget.h"

#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UI/VHVMatchingCardWidget.h"
#include "UI/VHVUIManagerComponent.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

UVHVMatchingWidget::UVHVMatchingWidget()
{
    SetIsFocusable(true);
    static ConstructorHelpers::FObjectFinder<UTexture2D> BackgroundFinder(
        TEXT("/Game/VHV_Stuff/UI/Backgrounds/T_BG_MatchingBoard.T_BG_MatchingBoard"));
    if (BackgroundFinder.Succeeded())
    {
        MatchingBackgroundTexture = BackgroundFinder.Object;
    }
}

TSharedRef<SWidget> UVHVMatchingWidget::RebuildWidget()
{
    BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
    BackgroundBrush.SetResourceObject(MatchingBackgroundTexture);
    if (MatchingBackgroundTexture)
    {
        BackgroundBrush.SetImageSize(FVector2D(
            MatchingBackgroundTexture->GetSizeX(), MatchingBackgroundTexture->GetSizeY()));
    }
    HeaderDividerBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::DividerGold(), 1.0f);
    KeycapBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(248, 244, 234, 225), 5.0f,
        VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth);

    const FLinearColor ButtonGold = VHVActivityUIStyle::GoldPrimary().CopyWithNewOpacity(0.92f);
    const FLinearColor ButtonHoverGold = VHVActivityUIStyle::GoldSelected();
    ConfirmButtonStyle = FButtonStyle()
        .SetNormal(VHVActivityUIStyle::RoundedBrush(
            ButtonGold, 25.0f,
            VHVActivityUIStyle::GoldSelected(), VHVActivityUIStyle::BorderNormalWidth))
        .SetHovered(VHVActivityUIStyle::RoundedBrush(
            ButtonHoverGold, 25.0f,
            VHVActivityUIStyle::MatchingInk().CopyWithNewOpacity(0.32f),
            VHVActivityUIStyle::BorderNormalWidth))
        .SetPressed(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::FromSRGB(202, 161, 79), 25.0f,
            VHVActivityUIStyle::MatchingInk().CopyWithNewOpacity(0.42f),
            VHVActivityUIStyle::BorderNormalWidth))
        .SetDisabled(VHVActivityUIStyle::RoundedBrush(
            VHVActivityUIStyle::FromSRGB(181, 166, 135, 170), 25.0f,
            VHVActivityUIStyle::MatchingPaperBorder(), VHVActivityUIStyle::BorderNormalWidth))
        .SetNormalPadding(FMargin(0.0f))
        .SetPressedPadding(FMargin(1.0f, 2.0f, 0.0f, 0.0f));

    const auto MakeKeycap = [this](const TCHAR* KeyText) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderImage(&KeycapBrush)
            .Padding(FMargin(7.0f, 3.0f))
            [
                SNew(STextBlock)
                .Font(VHVActivityUIStyle::MediumFont(12))
                .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                .Text(FText::FromString(KeyText))
            ];
    };

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
            // Baked paper workspace: (0.205, 0.105) to (0.825, 0.805).
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.265f, 0.125f, 0.765f, 0.285f))
            .Offset(FMargin(0.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Left)
                [
                    SNew(STextBlock)
                    .Font(VHVActivityUIStyle::MediumFont(VHVActivityUIStyle::HeaderFontSize))
                    .ColorAndOpacity(VHVActivityUIStyle::GoldPrimary())
                    .Text(FText::FromString(TEXT("MATCHING")))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Left)
                .Padding(FMargin(0.0f, 7.0f, 0.0f, 12.0f))
                [
                    SNew(SBox)
                    .WidthOverride(VHVActivityUIStyle::HeaderDividerWidth)
                    .HeightOverride(VHVActivityUIStyle::HeaderDividerHeight)
                    [
                        SNew(SBorder)
                        .BorderImage(&HeaderDividerBrush)
                    ]
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Left)
                [
                    SAssignNew(PromptTextSlate, STextBlock)
                    .Font(VHVActivityUIStyle::MediumFont(VHVActivityUIStyle::MatchingTitleFontSize))
                    .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                    .AutoWrapText(true)
                    .Text(FText::FromString(TEXT("Match each item with its pair.")))
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Left)
                .Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
                [
                    SNew(STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(VHVActivityUIStyle::InstructionFontSize))
                    .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                    .Text(FText::FromString(TEXT("Connect each item on the left to the correct item on the right.")))
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.235f, 0.315f, 0.468f, 0.735f))
            .Offset(FMargin(0.0f))
            [
                SNew(SScaleBox)
                .Stretch(EStretch::ScaleToFit)
                .StretchDirection(EStretchDirection::DownOnly)
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Top)
                [
                    SAssignNew(LeftItemsSlateContainer, SVerticalBox)
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.545f, 0.315f, 0.778f, 0.735f))
            .Offset(FMargin(0.0f))
            [
                SNew(SScaleBox)
                .Stretch(EStretch::ScaleToFit)
                .StretchDirection(EStretchDirection::DownOnly)
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Top)
                [
                    SAssignNew(RightItemsSlateContainer, SVerticalBox)
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.255f, 0.755f, 0.670f, 0.805f))
            .Offset(FMargin(0.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Left)
                [
                    SAssignNew(SubmitHintSlate, STextBlock)
                    .Font(VHVActivityUIStyle::RegularFont(14))
                    .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                ]
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(FMargin(0.0f, 7.0f, 0.0f, 0.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        MakeKeycap(TEXT("E"))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    .Padding(FMargin(6.0f, 0.0f, 14.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(13))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                        .Text(FText::FromString(TEXT("Connect")))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        MakeKeycap(TEXT("A / D"))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    .Padding(FMargin(6.0f, 0.0f, 14.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(13))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                        .Text(FText::FromString(TEXT("Column")))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        MakeKeycap(TEXT("DEL"))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    .Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::RegularFont(13))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInkMuted())
                        .Text(FText::FromString(TEXT("Remove")))
                    ]
                ]
            ]
            + SConstraintCanvas::Slot()
            .Anchors(FAnchors(0.805f, 0.780f))
            .Alignment(FVector2D(1.0f, 0.5f))
            .Offset(FMargin(0.0f, 0.0f,
                VHVActivityUIStyle::MatchingConfirmWidth,
                VHVActivityUIStyle::MatchingConfirmHeight))
            [
                SNew(SBox)
                .WidthOverride(VHVActivityUIStyle::MatchingConfirmWidth)
                .HeightOverride(VHVActivityUIStyle::MatchingConfirmHeight)
                [
                    SNew(SButton)
                    .ButtonStyle(&ConfirmButtonStyle)
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    .ContentPadding(FMargin(20.0f, 9.0f))
                    .OnClicked(FOnClicked::CreateUObject(this, &UVHVMatchingWidget::HandleConfirmClicked))
                    [
                        SNew(STextBlock)
                        .Font(VHVActivityUIStyle::MediumFont(15))
                        .ColorAndOpacity(VHVActivityUIStyle::MatchingInk())
                        .Text(FText::FromString(TEXT("Check Answers")))
                    ]
                ]
            ]
        ];

    SetPromptText(AuthoredPromptText);
    RefreshDisplay();
    return Result;
}

void UVHVMatchingWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    ActivityContentSlate.Reset();
    LeftItemsSlateContainer.Reset();
    RightItemsSlateContainer.Reset();
    PromptTextSlate.Reset();
    SubmitHintSlate.Reset();
    LeftCardWidgets.Reset();
    RightCardWidgets.Reset();
}

void UVHVMatchingWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (SubmitButton)
    {
        SubmitButton->OnClicked.AddUniqueDynamic(this, &UVHVMatchingWidget::HandleSubmitButtonClicked);
    }
}

void UVHVMatchingWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    EntranceElapsed = FMath::Min(EntranceElapsed + InDeltaTime, VHVActivityUIStyle::AnimationStandard);
    ConnectionRevealElapsed = FMath::Min(ConnectionRevealElapsed + InDeltaTime, 0.16f);
    const float Raw = FMath::Clamp(
        EntranceElapsed / VHVActivityUIStyle::AnimationStandard, 0.0f, 1.0f);
    const float Alpha = 1.0f - FMath::Pow(1.0f - Raw, 3.0f);
    if (ActivityContentSlate)
    {
        ActivityContentSlate->SetRenderOpacity(Alpha);
    }
    if (EntranceElapsed < VHVActivityUIStyle::AnimationStandard || ConnectionRevealElapsed < 0.16f)
    {
        InvalidateLayoutAndVolatility();
    }
}

void UVHVMatchingWidget::SetPromptText(const FString& InPromptText)
{
    AuthoredPromptText = InPromptText;
    if (PromptTextSlate)
    {
        PromptTextSlate->SetText(FText::FromString(
            AuthoredPromptText.IsEmpty()
                ? TEXT("Match each item with its pair.")
                : AuthoredPromptText));
    }
}

void UVHVMatchingWidget::SetMatchingPairs(const TArray<FMatchingPair>& InPairs)
{
    MatchingPairs = InPairs;
    RightDisplayOrder.Reset();
    LeftToRight.Init(INDEX_NONE, MatchingPairs.Num());
    for (int32 Index = 0; Index < MatchingPairs.Num(); ++Index)
    {
        RightDisplayOrder.Add(Index);
    }
    ShuffleRightItems();
    FocusedLeftIndex = MatchingPairs.Num() > 0 ? 0 : INDEX_NONE;
    FocusedRightDisplayIndex = RightDisplayOrder.Num() > 0 ? 0 : INDEX_NONE;
    PendingLeftIndex = INDEX_NONE;
    bLeftColumnFocused = true;
    DraggedLeftIndex = INDEX_NONE;
    HoveredRightDisplayIndex = INDEX_NONE;
    bConnectionDragActive = false;
    EntranceElapsed = 0.0f;
    ConnectionRevealElapsed = 1.0f;
    RefreshDisplay();
}

void UVHVMatchingWidget::SetOwningUIManager(UVHVUIManagerComponent* InUIManager)
{
    OwningUIManager = InUIManager;
}

TArray<FMatchingPair> UVHVMatchingWidget::GetCurrentMatches() const
{
    TArray<FMatchingPair> Matches;
    for (int32 LeftIndex = 0; LeftIndex < LeftToRight.Num(); ++LeftIndex)
    {
        const int32 RightIndex = LeftToRight[LeftIndex];
        if (MatchingPairs.IsValidIndex(LeftIndex) && MatchingPairs.IsValidIndex(RightIndex))
        {
            FMatchingPair Match;
            Match.LeftText = MatchingPairs[LeftIndex].LeftText;
            Match.RightText = MatchingPairs[RightIndex].RightText;
            Matches.Add(Match);
        }
    }
    return Matches;
}

void UVHVMatchingWidget::HandleCardSelected(const bool bIsLeftColumn, const int32 ItemIndex)
{
    if (bIsLeftColumn)
    {
        if (!MatchingPairs.IsValidIndex(ItemIndex))
        {
            return;
        }
        FocusedLeftIndex = ItemIndex;
        PendingLeftIndex = ItemIndex;
        bLeftColumnFocused = true;
    }
    else
    {
        if (!RightDisplayOrder.IsValidIndex(ItemIndex))
        {
            return;
        }
        FocusedRightDisplayIndex = ItemIndex;
        bLeftColumnFocused = false;
        if (MatchingPairs.IsValidIndex(PendingLeftIndex))
        {
            FocusedLeftIndex = PendingLeftIndex;
            AssignFocusedRightToFocusedLeft();
        }
    }
    UpdateCardVisuals();
}

void UVHVMatchingWidget::BeginConnectionDrag(const int32 LeftIndex, const FVector2D& ScreenPosition)
{
    if (!MatchingPairs.IsValidIndex(LeftIndex))
    {
        return;
    }
    DraggedLeftIndex = LeftIndex;
    PendingLeftIndex = LeftIndex;
    HoveredRightDisplayIndex = INDEX_NONE;
    DragScreenPosition = ScreenPosition;
    bConnectionDragActive = true;
    FocusedLeftIndex = LeftIndex;
    bLeftColumnFocused = true;
    UpdateCardVisuals();
    InvalidateLayoutAndVolatility();
}

void UVHVMatchingWidget::UpdateConnectionDrag(
    const FVector2D& ScreenPosition, const int32 HoveredRightIndex)
{
    if (!bConnectionDragActive)
    {
        return;
    }
    DragScreenPosition = ScreenPosition;
    HoveredRightDisplayIndex = RightDisplayOrder.IsValidIndex(HoveredRightIndex)
        ? HoveredRightIndex : INDEX_NONE;
    UpdateCardVisuals();
    InvalidateLayoutAndVolatility();
}

void UVHVMatchingWidget::CommitConnectionDrag(const int32 RightDisplayIndex)
{
    if (!bConnectionDragActive || !MatchingPairs.IsValidIndex(DraggedLeftIndex)
        || !RightDisplayOrder.IsValidIndex(RightDisplayIndex))
    {
        EndConnectionDrag();
        return;
    }
    FocusedLeftIndex = DraggedLeftIndex;
    FocusedRightDisplayIndex = RightDisplayIndex;
    AssignFocusedRightToFocusedLeft();
    EndConnectionDrag();
}

void UVHVMatchingWidget::EndConnectionDrag()
{
    bConnectionDragActive = false;
    DraggedLeftIndex = INDEX_NONE;
    HoveredRightDisplayIndex = INDEX_NONE;
    UpdateCardVisuals();
    InvalidateLayoutAndVolatility();
}

FReply UVHVMatchingWidget::NativeOnKeyDown(
    const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Up || Key == EKeys::W
        || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
    {
        MoveFocus(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::Down || Key == EKeys::S
        || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
    {
        MoveFocus(1);
        return FReply::Handled();
    }
    if (Key == EKeys::Left || Key == EKeys::A
        || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
    {
        SetFocusedColumn(true);
        return FReply::Handled();
    }
    if (Key == EKeys::Right || Key == EKeys::D
        || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
    {
        SetFocusedColumn(false);
        return FReply::Handled();
    }
    if (Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Left)
    {
        ActivateFocusedSelection();
        return FReply::Handled();
    }
    if (Key == EKeys::BackSpace || Key == EKeys::Delete || Key == EKeys::Gamepad_FaceButton_Right)
    {
        RemoveFocusedLeftMatch();
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        SubmitCurrentMatches();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UVHVMatchingWidget::ActivateFocusedSelection()
{
    if (bLeftColumnFocused)
    {
        if (!MatchingPairs.IsValidIndex(FocusedLeftIndex))
        {
            return false;
        }
        PendingLeftIndex = FocusedLeftIndex;
        UpdateCardVisuals();
        return true;
    }
    if (!MatchingPairs.IsValidIndex(PendingLeftIndex)
        || !RightDisplayOrder.IsValidIndex(FocusedRightDisplayIndex))
    {
        return false;
    }
    FocusedLeftIndex = PendingLeftIndex;
    AssignFocusedRightToFocusedLeft();
    return true;
}

bool UVHVMatchingWidget::NativeOnDragOver(
    const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    if (bConnectionDragActive
        && Cast<UVHVMatchingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        UpdateConnectionDrag(InDragDropEvent.GetScreenSpacePosition());
        return true;
    }
    return Super::NativeOnDragOver(InGeometry, InDragDropEvent, InOperation);
}

bool UVHVMatchingWidget::NativeOnDrop(
    const FGeometry& InGeometry,
    const FDragDropEvent& InDragDropEvent,
    UDragDropOperation* InOperation)
{
    if (bConnectionDragActive
        && Cast<UVHVMatchingCardWidget>(InOperation ? InOperation->Payload : nullptr))
    {
        EndConnectionDrag();
        return true;
    }
    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

int32 UVHVMatchingWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    const int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    const bool bParentEnabled) const
{
    const int32 PaintLayer = Super::NativePaint(
        Args, AllottedGeometry, MyCullingRect, OutDrawElements,
        LayerId, InWidgetStyle, bParentEnabled);
    int32 ConnectionLayer = PaintLayer + 1;
    const float Reveal = FMath::Clamp(ConnectionRevealElapsed / 0.16f, 0.0f, 1.0f);
    const FVector2D WindowToDesktop = Args.GetWindowToDesktopTransform();
    for (int32 LeftIndex = 0; LeftIndex < LeftToRight.Num(); ++LeftIndex)
    {
        const int32 RightIndex = LeftToRight[LeftIndex];
        const int32 RightDisplayIndex = RightDisplayOrder.Find(RightIndex);
        if (!LeftCardWidgets.IsValidIndex(LeftIndex)
            || !RightCardWidgets.IsValidIndex(RightDisplayIndex)
            || !LeftCardWidgets[LeftIndex] || !RightCardWidgets[RightDisplayIndex])
        {
            continue;
        }
        TArray<FVector2D> Points;
        Points.Add(GetConnectorCenterInPaintSpace(LeftCardWidgets[LeftIndex], WindowToDesktop));
        Points.Add(GetConnectorCenterInPaintSpace(RightCardWidgets[RightDisplayIndex], WindowToDesktop));
        FLinearColor LineColor = VHVActivityUIStyle::PositiveMuted();
        LineColor.A = 0.78f * Reveal;
        FSlateDrawElement::MakeLines(
            OutDrawElements, ConnectionLayer, FPaintGeometry(),
            Points, ESlateDrawEffect::None, LineColor, true, 2.0f);
    }
    if (bConnectionDragActive
        && LeftCardWidgets.IsValidIndex(DraggedLeftIndex)
        && LeftCardWidgets[DraggedLeftIndex])
    {
        TArray<FVector2D> DragPoints;
        DragPoints.Add(GetConnectorCenterInPaintSpace(LeftCardWidgets[DraggedLeftIndex], WindowToDesktop));
        if (RightCardWidgets.IsValidIndex(HoveredRightDisplayIndex)
            && RightCardWidgets[HoveredRightDisplayIndex])
        {
            DragPoints.Add(GetConnectorCenterInPaintSpace(
                RightCardWidgets[HoveredRightDisplayIndex], WindowToDesktop));
        }
        else
        {
            DragPoints.Add(DragScreenPosition - WindowToDesktop);
        }
        FLinearColor PreviewColor = VHVActivityUIStyle::GoldSelected();
        PreviewColor.A = 0.86f;
        FSlateDrawElement::MakeLines(
            OutDrawElements, ConnectionLayer + 1, FPaintGeometry(),
            DragPoints, ESlateDrawEffect::None, PreviewColor, true, 2.0f);
        ConnectionLayer++;
    }
    return ConnectionLayer;
}

void UVHVMatchingWidget::RefreshDisplay()
{
    if ((!LeftItemsSlateContainer || !RightItemsSlateContainer)
        && (!LeftItemsContainer || !RightItemsContainer))
    {
        return;
    }
    if (LeftItemsSlateContainer)
    {
        LeftItemsSlateContainer->ClearChildren();
    }
    if (RightItemsSlateContainer)
    {
        RightItemsSlateContainer->ClearChildren();
    }
    if (LeftItemsContainer)
    {
        LeftItemsContainer->ClearChildren();
    }
    if (RightItemsContainer)
    {
        RightItemsContainer->ClearChildren();
    }
    LeftCardWidgets.Reset();
    RightCardWidgets.Reset();

    TSubclassOf<UVHVMatchingCardWidget> WidgetClass = UVHVMatchingCardWidget::StaticClass();
    if (CardWidgetClass)
    {
        WidgetClass = CardWidgetClass;
    }
    else if (OwningUIManager && OwningUIManager->MatchingCardWidgetClass)
    {
        WidgetClass = OwningUIManager->MatchingCardWidgetClass;
    }

    for (int32 Index = 0; Index < MatchingPairs.Num(); ++Index)
    {
        UVHVMatchingCardWidget* LeftCard = CreateWidget<UVHVMatchingCardWidget>(this, WidgetClass);
        if (!LeftCard)
        {
            continue;
        }
        LeftCard->SetOwningMatchingWidget(this);
        LeftCard->SetCardData(true, Index, MatchingPairs[Index].LeftText);
        LeftCard->SetEntranceDelay(Index * VHVActivityUIStyle::AnimationStagger);
        if (LeftItemsSlateContainer)
        {
            LeftItemsSlateContainer->AddSlot()
            .AutoHeight()
            .HAlign(HAlign_Fill)
            .Padding(FMargin(0.0f, 0.0f, 0.0f,
                Index + 1 == MatchingPairs.Num() ? 0.0f : VHVActivityUIStyle::MatchingCardGap))
            [
                LeftCard->TakeWidget()
            ];
        }
        else if (LeftItemsContainer)
        {
            LeftItemsContainer->AddChild(LeftCard);
            if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(LeftCard->Slot))
            {
                CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, VHVActivityUIStyle::MatchingCardGap));
                CardSlot->SetHorizontalAlignment(HAlign_Fill);
            }
        }
        LeftCardWidgets.Add(LeftCard);
    }

    for (int32 DisplayIndex = 0; DisplayIndex < RightDisplayOrder.Num(); ++DisplayIndex)
    {
        const int32 PairIndex = RightDisplayOrder[DisplayIndex];
        if (!MatchingPairs.IsValidIndex(PairIndex))
        {
            continue;
        }
        UVHVMatchingCardWidget* RightCard = CreateWidget<UVHVMatchingCardWidget>(this, WidgetClass);
        if (!RightCard)
        {
            continue;
        }
        RightCard->SetOwningMatchingWidget(this);
        RightCard->SetCardData(false, DisplayIndex, MatchingPairs[PairIndex].RightText);
        RightCard->SetEntranceDelay(
            (DisplayIndex + 1) * VHVActivityUIStyle::AnimationStagger);
        if (RightItemsSlateContainer)
        {
            RightItemsSlateContainer->AddSlot()
            .AutoHeight()
            .HAlign(HAlign_Fill)
            .Padding(FMargin(0.0f, 0.0f, 0.0f,
                DisplayIndex + 1 == RightDisplayOrder.Num()
                    ? 0.0f : VHVActivityUIStyle::MatchingCardGap))
            [
                RightCard->TakeWidget()
            ];
        }
        else if (RightItemsContainer)
        {
            RightItemsContainer->AddChild(RightCard);
            if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(RightCard->Slot))
            {
                CardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, VHVActivityUIStyle::MatchingCardGap));
                CardSlot->SetHorizontalAlignment(HAlign_Fill);
            }
        }
        RightCardWidgets.Add(RightCard);
    }
    UpdateCardVisuals();
    UpdateSubmitHint();
}

void UVHVMatchingWidget::ShuffleRightItems()
{
    for (int32 Index = RightDisplayOrder.Num() - 1; Index > 0; --Index)
    {
        RightDisplayOrder.Swap(Index, FMath::RandRange(0, Index));
    }
}

void UVHVMatchingWidget::MoveFocus(const int32 Direction)
{
    int32& FocusedIndex = bLeftColumnFocused ? FocusedLeftIndex : FocusedRightDisplayIndex;
    const int32 ItemCount = bLeftColumnFocused ? MatchingPairs.Num() : RightDisplayOrder.Num();
    if (ItemCount == 0)
    {
        return;
    }
    FocusedIndex = FocusedIndex == INDEX_NONE
        ? 0 : (FocusedIndex + Direction + ItemCount) % ItemCount;
    UpdateCardVisuals();
}

void UVHVMatchingWidget::SetFocusedColumn(const bool bInLeftColumn)
{
    bLeftColumnFocused = bInLeftColumn;
    UpdateCardVisuals();
}

void UVHVMatchingWidget::AssignFocusedRightToFocusedLeft()
{
    if (!MatchingPairs.IsValidIndex(FocusedLeftIndex)
        || !RightDisplayOrder.IsValidIndex(FocusedRightDisplayIndex))
    {
        return;
    }
    const int32 RightIndex = RightDisplayOrder[FocusedRightDisplayIndex];
    for (int32 LeftIndex = 0; LeftIndex < LeftToRight.Num(); ++LeftIndex)
    {
        if (LeftIndex != FocusedLeftIndex && LeftToRight[LeftIndex] == RightIndex)
        {
            LeftToRight[LeftIndex] = INDEX_NONE;
        }
    }
    LeftToRight[FocusedLeftIndex] = RightIndex;
    PendingLeftIndex = INDEX_NONE;
    ConnectionRevealElapsed = 0.0f;
    UpdateCardVisuals();
    UpdateSubmitHint();
}

void UVHVMatchingWidget::RemoveFocusedLeftMatch()
{
    const int32 LeftIndex = MatchingPairs.IsValidIndex(PendingLeftIndex)
        ? PendingLeftIndex : FocusedLeftIndex;
    if (LeftToRight.IsValidIndex(LeftIndex))
    {
        LeftToRight[LeftIndex] = INDEX_NONE;
        PendingLeftIndex = INDEX_NONE;
        UpdateCardVisuals();
        UpdateSubmitHint();
        InvalidateLayoutAndVolatility();
    }
}

void UVHVMatchingWidget::SubmitCurrentMatches()
{
    if (OwningUIManager)
    {
        OwningUIManager->SubmitMatchingAnswer(GetCurrentMatches());
    }
}

void UVHVMatchingWidget::UpdateCardVisuals()
{
    for (int32 LeftIndex = 0; LeftIndex < LeftCardWidgets.Num(); ++LeftIndex)
    {
        if (!LeftCardWidgets[LeftIndex] || !MatchingPairs.IsValidIndex(LeftIndex))
        {
            continue;
        }
        LeftCardWidgets[LeftIndex]->SetCardData(
            true, LeftIndex, MatchingPairs[LeftIndex].LeftText);
        LeftCardWidgets[LeftIndex]->SetVisualState(
            bLeftColumnFocused && LeftIndex == FocusedLeftIndex,
            LeftIndex == PendingLeftIndex,
            LeftToRight.IsValidIndex(LeftIndex) && LeftToRight[LeftIndex] != INDEX_NONE,
            false,
            bConnectionDragActive && LeftIndex == DraggedLeftIndex);
    }
    for (int32 DisplayIndex = 0; DisplayIndex < RightCardWidgets.Num(); ++DisplayIndex)
    {
        if (!RightCardWidgets[DisplayIndex] || !RightDisplayOrder.IsValidIndex(DisplayIndex))
        {
            continue;
        }
        const int32 RightIndex = RightDisplayOrder[DisplayIndex];
        const bool bMapped = LeftToRight.Contains(RightIndex);
        RightCardWidgets[DisplayIndex]->SetCardData(
            false, DisplayIndex, MatchingPairs[RightIndex].RightText);
        RightCardWidgets[DisplayIndex]->SetVisualState(
            !bLeftColumnFocused && DisplayIndex == FocusedRightDisplayIndex,
            false,
            bMapped,
            bConnectionDragActive && DisplayIndex == HoveredRightDisplayIndex,
            false);
    }
}

void UVHVMatchingWidget::UpdateSubmitHint()
{
    const FText Hint = FText::FromString(FString::Printf(
        TEXT("%d / %d pairs connected"), GetCurrentMatches().Num(), MatchingPairs.Num()));
    if (SubmitHintSlate)
    {
        SubmitHintSlate->SetText(Hint);
    }
    if (SubmitHint)
    {
        SubmitHint->SetText(Hint);
    }
}

FVector2D UVHVMatchingWidget::GetConnectorCenterInPaintSpace(
    const UVHVMatchingCardWidget* CardWidget,
    const FVector2D& WindowToDesktop) const
{
    if (!CardWidget)
    {
        return FVector2D::ZeroVector;
    }

    // Resolve this during paint, after Slate has arranged the connector.
    const FGeometry ConnectorGeometry = CardWidget->GetConnectorGeometry();
    const FVector2D ConnectorLocalCenter = ConnectorGeometry.GetLocalSize() * 0.5f;
    const FVector2D ConnectorAbsoluteCenter = ConnectorGeometry.LocalToAbsolute(
        ConnectorLocalCenter);
    // Cached widget geometry is in desktop space, while a draw element using
    // identity paint geometry consumes window-space points. Remove exactly the
    // window-to-desktop translation supplied by this paint pass; DPI and every
    // arranged-widget transform are already represented in the absolute center.
    return ConnectorAbsoluteCenter - WindowToDesktop;
}

void UVHVMatchingWidget::HandleSubmitButtonClicked()
{
    SubmitCurrentMatches();
}

FReply UVHVMatchingWidget::HandleConfirmClicked()
{
    SubmitCurrentMatches();
    SetKeyboardFocus();
    return FReply::Handled();
}
