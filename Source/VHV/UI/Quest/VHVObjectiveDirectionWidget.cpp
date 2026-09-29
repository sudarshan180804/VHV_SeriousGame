#include "UI/Quest/VHVObjectiveDirectionWidget.h"

#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "UI/Textbook/VHVActivityUIStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

UVHVObjectiveDirectionWidget::UVHVObjectiveDirectionWidget()
{
    SetIsFocusable(false);
    SetVisibility(ESlateVisibility::Collapsed);
}

TSharedRef<SWidget> UVHVObjectiveDirectionWidget::RebuildWidget()
{
    return SNew(SConstraintCanvas)
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.5f, 0.14f))
        .Alignment(FVector2D(0.5f, 0.5f))
        .AutoSize(true)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SAssignNew(ArrowVisual, SBox)
                .WidthOverride(40.0f)
                .HeightOverride(66.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("\u25B2")))
                        .Font(VHVActivityUIStyle::MediumFont(34))
                        .ColorAndOpacity(FLinearColor(0.05f, 0.9f, 1.0f, 1.0f))
                        .ShadowOffset(FVector2D(1.5f, 1.5f))
                        .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
                    ]
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    .HAlign(HAlign_Center)
                    .Padding(0.0f, -5.0f, 0.0f, 0.0f)
                    [
                        SNew(SBox)
                        .WidthOverride(9.0f)
                        .HeightOverride(31.0f)
                        [
                            SNew(SOverlay)
                            + SOverlay::Slot()
                            [
                                SNew(SBorder)
                                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f))
                                .Padding(0.0f)
                            ]
                            + SOverlay::Slot()
                            .HAlign(HAlign_Center)
                            .VAlign(VAlign_Center)
                            [
                                SNew(SBox)
                                .WidthOverride(5.0f)
                                .HeightOverride(27.0f)
                                [
                                    SNew(SBorder)
                                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                                    .BorderBackgroundColor(FLinearColor(0.05f, 0.9f, 1.0f, 1.0f))
                                    .Padding(0.0f)
                                ]
                            ]
                        ]
                    ]
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            .Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [
                SAssignNew(DistanceText, STextBlock)
                .Font(VHVActivityUIStyle::MediumFont(13))
                .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                .ShadowOffset(FVector2D(1.0f, 1.0f))
                .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
            ]
        ];
}

void UVHVObjectiveDirectionWidget::ShowDirectionTo(const FVector& InTargetLocation)
{
    TargetLocation = InTargetLocation;
    bHasTarget = true;
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UVHVObjectiveDirectionWidget::HideDirection()
{
    bHasTarget = false;
    SetVisibility(ESlateVisibility::Collapsed);
}

void UVHVObjectiveDirectionWidget::NativeTick(
    const FGeometry& MyGeometry,
    const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bHasTarget || GetVisibility() == ESlateVisibility::Collapsed)
    {
        return;
    }

    const APlayerController* PlayerController = GetOwningPlayer();
    if (!PlayerController || !ArrowVisual)
    {
        return;
    }

    FVector CameraLocation;
    FRotator CameraRotation;
    PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

    FVector ToTarget = TargetLocation - CameraLocation;
    ToTarget.Z = 0.0f;
    const float DistanceCentimeters = ToTarget.Size();
    if (!ToTarget.Normalize())
    {
        return;
    }

    FVector CameraForward = CameraRotation.Vector();
    CameraForward.Z = 0.0f;
    CameraForward.Normalize();
    FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);
    CameraRight.Z = 0.0f;
    CameraRight.Normalize();

    const float ForwardDot = FVector::DotProduct(ToTarget, CameraForward);
    const float RightDot = FVector::DotProduct(ToTarget, CameraRight);
    const float AngleRadians = FMath::Atan2(RightDot, ForwardDot);
    // The composite is authored pointing up, so the existing atan2 result maps
    // directly to screen up/right/down/left without an orientation offset.
    ArrowVisual->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    ArrowVisual->SetRenderTransform(FSlateRenderTransform(FQuat2D(AngleRadians)));

    if (DistanceText)
    {
        DistanceText->SetText(FText::FromString(FString::Printf(
            TEXT("%.0f m"), DistanceCentimeters / 100.0f)));
    }
}
