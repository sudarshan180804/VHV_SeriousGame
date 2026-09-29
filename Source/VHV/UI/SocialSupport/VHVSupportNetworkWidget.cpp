#include "UI/SocialSupport/VHVSupportNetworkWidget.h"

#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UI/VHVUIManagerComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const TCHAR* SupporterNames[] = {
        TEXT("Chai's Wife"), TEXT("Chai's Daughter"), TEXT("Walking Neighbor"), TEXT("Clinic Nurse / VHV")};
    const TCHAR* RoleNames[] = {
        TEXT("Prepare healthier meals"), TEXT("Encourage / remind progress"),
        TEXT("Join evening walking"), TEXT("Provide health information")};
    const TCHAR* PlanningDetails[] = {
        TEXT("Healthy Meals\nHome • Dinner"),
        TEXT("Encouragement\nPhone/check-in • Evening"),
        TEXT("Walking Partner\nVillage route • Evening"),
        TEXT("Health Guidance\nClinic • Weekly")};
    const TCHAR* PayoffDetails[] = {
        TEXT("Healthy meals\n+ backup short walk"),
        TEXT("Regular encouragement\n+ check-ins"),
        TEXT("Evening walking partner"),
        TEXT("Health guidance")};
}

UVHVSupportNetworkWidget::UVHVSupportNetworkWidget()
{
    SetIsFocusable(true);
    Assignments.Init(INDEX_NONE, 4);
    IncorrectAssignments.Init(false, 4);
}

void UVHVSupportNetworkWidget::SetOwningUIManager(UVHVUIManagerComponent* InManager)
{
    OwningUIManager = InManager;
}

TSharedRef<SWidget> UVHVSupportNetworkWidget::RebuildWidget()
{
    BackgroundBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassMain().CopyWithNewOpacity(0.94f), 26.0f,
        VHVActivityUIStyle::PanelBorder(), VHVActivityUIStyle::BorderNormalWidth);
    NodeBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassCard(), 18.0f,
        VHVActivityUIStyle::BorderNeutral(), VHVActivityUIStyle::BorderNormalWidth);
    SelectedNodeBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassSelected(), 18.0f,
        VHVActivityUIStyle::BorderGold(), VHVActivityUIStyle::BorderSelectedWidth);
    CenterBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::FromSRGB(57, 47, 30, 238), 54.0f,
        VHVActivityUIStyle::BorderGold(), 2.0f);
    RoleBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassCard(), 14.0f,
        VHVActivityUIStyle::BorderNeutral(), VHVActivityUIStyle::BorderNormalWidth);
    SelectedRoleBrush = VHVActivityUIStyle::RoundedBrush(
        VHVActivityUIStyle::GlassSelected(), 14.0f,
        VHVActivityUIStyle::BorderGold(), VHVActivityUIStyle::BorderSelectedWidth);

    NodeButtonStyle = FButtonStyle()
        .SetNormal(NodeBrush).SetHovered(SelectedNodeBrush).SetPressed(SelectedNodeBrush)
        .SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f));
    RoleButtonStyle = FButtonStyle()
        .SetNormal(RoleBrush).SetHovered(SelectedRoleBrush).SetPressed(SelectedRoleBrush)
        .SetNormalPadding(FMargin(0.0f)).SetPressedPadding(FMargin(0.0f));

    SupporterTexts.Reset();
    RoleTexts.Reset();

    TSharedRef<SConstraintCanvas> Canvas = SNew(SConstraintCanvas);
    Canvas->AddSlot().Anchors(FAnchors(0.07f, 0.05f, 0.93f, 0.95f)).Offset(FMargin(0.0f))
    [
        SNew(SBorder).BorderImage(&BackgroundBrush).Padding(FMargin(0.0f))
    ];
    Canvas->AddSlot().Anchors(FAnchors(0.18f, 0.09f, 0.82f, 0.19f)).Offset(FMargin(0.0f))
    [
        SNew(STextBlock)
        .Font(VHVActivityUIStyle::MediumFont(30))
        .ColorAndOpacity(VHVActivityUIStyle::GoldPrimary())
        .Justification(ETextJustify::Center)
        .Text(FText::FromString(TEXT("BUILD UNCLE CHAI'S SUPPORT NETWORK")))
    ];

    const FAnchors SupporterAnchors[] = {
        FAnchors(0.12f, 0.22f, 0.37f, 0.40f), FAnchors(0.12f, 0.52f, 0.37f, 0.70f),
        FAnchors(0.63f, 0.22f, 0.88f, 0.40f), FAnchors(0.63f, 0.52f, 0.88f, 0.70f)};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        TSharedPtr<STextBlock> TextWidget;
        Canvas->AddSlot().Anchors(SupporterAnchors[Index]).Offset(FMargin(0.0f))
        [
            SNew(SButton)
            .ButtonStyle(&NodeButtonStyle)
            .ContentPadding(FMargin(14.0f, 10.0f))
            .OnClicked_UObject(this, &UVHVSupportNetworkWidget::HandleSupporterClicked, Index)
            [
                SAssignNew(TextWidget, STextBlock)
                .Font(VHVActivityUIStyle::MediumFont(18))
                .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                .Justification(ETextJustify::Center)
                .AutoWrapText(true)
            ]
        ];
        SupporterTexts.Add(TextWidget);
    }

    Canvas->AddSlot().Anchors(FAnchors(0.405f, 0.33f, 0.595f, 0.57f)).Offset(FMargin(0.0f))
    [
        SNew(SBorder).BorderImage(&CenterBrush).Padding(FMargin(18.0f))
        [
            SNew(STextBlock)
            .Font(VHVActivityUIStyle::MediumFont(27))
            .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
            .Justification(ETextJustify::Center)
            .Text(FText::FromString(TEXT("UNCLE\nCHAI")))
        ]
    ];

    TSharedRef<SHorizontalBox> Roles = SNew(SHorizontalBox);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        TSharedPtr<STextBlock> TextWidget;
        Roles->AddSlot().FillWidth(1.0f).Padding(5.0f, 0.0f)
        [
            SNew(SButton)
            .ButtonStyle(&RoleButtonStyle)
            .ContentPadding(FMargin(8.0f, 10.0f))
            .OnClicked_UObject(this, &UVHVSupportNetworkWidget::HandleRoleClicked, Index)
            [
                SAssignNew(TextWidget, STextBlock)
                .Font(VHVActivityUIStyle::RegularFont(15))
                .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                .Justification(ETextJustify::Center)
                .AutoWrapText(true)
                .Text(FText::FromString(RoleNames[Index]))
            ]
        ];
        RoleTexts.Add(TextWidget);
    }
    Canvas->AddSlot().Anchors(FAnchors(0.12f, 0.76f, 0.88f, 0.85f)).Offset(FMargin(0.0f))[Roles];
    Canvas->AddSlot().Anchors(FAnchors(0.17f, 0.87f, 0.83f, 0.93f)).Offset(FMargin(0.0f))
    [
        SAssignNew(StatusText, STextBlock)
        .Font(VHVActivityUIStyle::MediumFont(16))
        .ColorAndOpacity(VHVActivityUIStyle::TextSecondary())
        .Justification(ETextJustify::Center)
        .AutoWrapText(true)
    ];
    RefreshVisuals();
    return Canvas;
}

void UVHVSupportNetworkWidget::Configure(const FTextbookActivityData& Activity)
{
    ActivityData = Activity;
    bReadOnly = Activity.PresentationStyle == EVHVActivityPresentationStyle::SocialSupportNetworkReadOnly;
    bSubmissionLocked = false;
    SelectedSupporter = 0;
    SelectedRole = 0;
    IncorrectAssignments.Init(false, 4);
    if (bReadOnly)
    {
        Assignments = {0, 1, 2, 3};
        Status = TEXT("Appraisal Support  •  Player/VHV recognition of progress\nGood support is the right help, from the right person, at the right time.\nPress Enter to continue.");
    }
    else
    {
        Assignments.Init(INDEX_NONE, 4);
        Status = TEXT("W/S: supporter   Left/Right: role   E: connect/remove   Enter: submit");
    }
    RefreshVisuals();
}

FReply UVHVSupportNetworkWidget::HandleSupporterClicked(const int32 Index)
{
    if (!bReadOnly && !bSubmissionLocked)
    {
        SelectedSupporter = FMath::Clamp(Index, 0, 3);
        RefreshVisuals();
    }
    return FReply::Handled();
}

FReply UVHVSupportNetworkWidget::HandleRoleClicked(const int32 Index)
{
    if (!bReadOnly && !bSubmissionLocked)
    {
        SelectedRole = FMath::Clamp(Index, 0, 3);
        ConnectSelected();
    }
    return FReply::Handled();
}

void UVHVSupportNetworkWidget::MoveSupporterFocus(const int32 Delta)
{
    SelectedSupporter = (SelectedSupporter + Delta + 4) % 4;
    RefreshVisuals();
}

void UVHVSupportNetworkWidget::MoveRoleFocus(const int32 Delta)
{
    SelectedRole = (SelectedRole + Delta + 4) % 4;
    RefreshVisuals();
}

void UVHVSupportNetworkWidget::ConnectSelected()
{
    if (bReadOnly || bSubmissionLocked) return;
    if (Assignments.IsValidIndex(SelectedSupporter) && Assignments[SelectedSupporter] == SelectedRole)
    {
        RemoveSelected();
        return;
    }
    for (int32& AssignedRole : Assignments)
    {
        if (AssignedRole == SelectedRole) AssignedRole = INDEX_NONE;
    }
    Assignments[SelectedSupporter] = SelectedRole;
    IncorrectAssignments.Init(false, 4);
    Status = TEXT("Connection added. Assign all four supporters, then press Enter.");
    RefreshVisuals();
}

void UVHVSupportNetworkWidget::RemoveSelected()
{
    if (bReadOnly || bSubmissionLocked || !Assignments.IsValidIndex(SelectedSupporter)) return;
    Assignments[SelectedSupporter] = INDEX_NONE;
    IncorrectAssignments.Init(false, 4);
    Status = TEXT("Connection removed.");
    RefreshVisuals();
}

TArray<FMatchingPair> UVHVSupportNetworkWidget::GetCurrentMatches() const
{
    TArray<FMatchingPair> Result;
    for (int32 Supporter = 0; Supporter < Assignments.Num(); ++Supporter)
    {
        const int32 Role = Assignments[Supporter];
        if (Role >= 0 && Role < 4)
        {
            FMatchingPair& Pair = Result.AddDefaulted_GetRef();
            Pair.LeftText = SupporterNames[Supporter];
            Pair.RightText = RoleNames[Role];
        }
    }
    return Result;
}

void UVHVSupportNetworkWidget::SubmitOrClose()
{
    if (bReadOnly)
    {
        if (OwningUIManager) OwningUIManager->CompleteSocialSupportNetworkPayoff();
        return;
    }
    if (bSubmissionLocked) return;
    if (Assignments.Contains(INDEX_NONE))
    {
        Status = TEXT("Connect all four supporters before submitting.");
        RefreshVisuals();
        return;
    }
    const TArray<FMatchingPair> Matches = GetCurrentMatches();
    if (!OwningUIManager || !OwningUIManager->IsSupportNetworkSubmissionCorrect(Matches))
    {
        for (int32 Index = 0; Index < 4; ++Index)
        {
            IncorrectAssignments[Index] = Assignments[Index] != Index;
        }
        Status = TEXT("Some connections do not fit what each person can realistically do. Repair the highlighted links.");
        RefreshVisuals();
        return;
    }

    bSubmissionLocked = true;
    IncorrectAssignments.Init(false, 4);
    Status = TEXT("SUPPORT NETWORK READY\nEveryone has a clear role: who, what, where and when.");
    RefreshVisuals();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            SubmitTimer, this, &UVHVSupportNetworkWidget::FinishCorrectSubmission, 1.15f, false);
    }
    else
    {
        FinishCorrectSubmission();
    }
}

void UVHVSupportNetworkWidget::FinishCorrectSubmission()
{
    if (OwningUIManager) OwningUIManager->CompleteSupportNetworkPlanning(GetCurrentMatches());
}

FString UVHVSupportNetworkWidget::SupporterText(const int32 Index) const
{
    FString Result = SupporterNames[Index];
    if (Assignments.IsValidIndex(Index) && Assignments[Index] != INDEX_NONE)
    {
        Result += TEXT("\n");
        Result += bReadOnly ? PayoffDetails[Index] : PlanningDetails[Index];
    }
    return Result;
}

void UVHVSupportNetworkWidget::RefreshVisuals()
{
    for (int32 Index = 0; Index < SupporterTexts.Num(); ++Index)
    {
        if (!SupporterTexts[Index]) continue;
        SupporterTexts[Index]->SetText(FText::FromString(SupporterText(Index)));
        SupporterTexts[Index]->SetColorAndOpacity(
            IncorrectAssignments.IsValidIndex(Index) && IncorrectAssignments[Index]
                ? VHVActivityUIStyle::NegativeMuted()
                : (Index == SelectedSupporter && !bReadOnly
                    ? VHVActivityUIStyle::GoldSelected()
                    : VHVActivityUIStyle::TextPrimary()));
    }
    for (int32 Index = 0; Index < RoleTexts.Num(); ++Index)
    {
        if (RoleTexts[Index])
        {
            RoleTexts[Index]->SetColorAndOpacity(Index == SelectedRole && !bReadOnly
                ? VHVActivityUIStyle::GoldSelected() : VHVActivityUIStyle::TextPrimary());
        }
    }
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Status));
        StatusText->SetColorAndOpacity(bSubmissionLocked
            ? VHVActivityUIStyle::PositiveMuted() : VHVActivityUIStyle::TextSecondary());
    }
    InvalidateLayoutAndVolatility();
}

FReply UVHVSupportNetworkWidget::NativeOnKeyDown(
    const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (bReadOnly && (Key == EKeys::Enter || Key == EKeys::SpaceBar))
    {
        SubmitOrClose();
        return FReply::Handled();
    }
    if (Key == EKeys::Up || Key == EKeys::W) { MoveSupporterFocus(-1); return FReply::Handled(); }
    if (Key == EKeys::Down || Key == EKeys::S) { MoveSupporterFocus(1); return FReply::Handled(); }
    if (Key == EKeys::Left || Key == EKeys::A) { MoveRoleFocus(-1); return FReply::Handled(); }
    if (Key == EKeys::Right || Key == EKeys::D) { MoveRoleFocus(1); return FReply::Handled(); }
    if (Key == EKeys::E || Key == EKeys::SpaceBar) { ConnectSelected(); return FReply::Handled(); }
    if (Key == EKeys::BackSpace || Key == EKeys::Delete) { RemoveSelected(); return FReply::Handled(); }
    if (Key == EKeys::Enter) { SubmitOrClose(); return FReply::Handled(); }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 UVHVSupportNetworkWidget::NativePaint(
    const FPaintArgs& Args,
    const FGeometry& AllottedGeometry,
    const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements,
    const int32 LayerId,
    const FWidgetStyle& InWidgetStyle,
    const bool bParentEnabled) const
{
    const int32 BaseLayer = Super::NativePaint(
        Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const FVector2D Center(Size.X * 0.5f, Size.Y * 0.45f);
    const FVector2D SupporterCenters[] = {
        FVector2D(Size.X * 0.245f, Size.Y * 0.31f), FVector2D(Size.X * 0.245f, Size.Y * 0.61f),
        FVector2D(Size.X * 0.755f, Size.Y * 0.31f), FVector2D(Size.X * 0.755f, Size.Y * 0.61f)};
    for (int32 Index = 0; Index < Assignments.Num(); ++Index)
    {
        if (Assignments[Index] == INDEX_NONE) continue;
        const FLinearColor Color = IncorrectAssignments.IsValidIndex(Index) && IncorrectAssignments[Index]
            ? VHVActivityUIStyle::NegativeMuted()
            : VHVActivityUIStyle::GoldPrimary();
        FSlateDrawElement::MakeLines(
            OutDrawElements, BaseLayer + 1, AllottedGeometry.ToPaintGeometry(),
            {SupporterCenters[Index], Center}, ESlateDrawEffect::None, Color, true, 3.0f);
    }
    return BaseLayer + 1;
}
