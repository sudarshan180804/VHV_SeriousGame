#include "UI/Quest/VHVObjectiveMarkerWidget.h"

#include "Ambient/VHVAmbientConversationActor.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "Quest/Types/VHVQuestTypes.h"
#include "Textbook/Systems/VHVTextbookSubsystem.h"
#include "UI/Ambient/VHVAmbientSpeechStyle.h"
#include "UI/Textbook/VHVActivityUIStyle.h"
#include "UI/VHVUIManagerComponent.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "World/Components/VHVWorldActionReceiverComponent.h"
#include "World/Location/VHVQuestLocationVolume.h"
#include "World/Systems/VHVWorldActionSubsystem.h"

namespace VHVObjectiveMarker
{
    constexpr float RingSize = 46.0f;
    constexpr float DotSize = 14.0f;
    constexpr float PinWidth = 120.0f;
    constexpr float PinHeight = 92.0f;
    constexpr float ArrowSize = 52.0f;
    constexpr int32 ArrowFontSize = 34;
    constexpr float EdgePinInset = 52.0f;
    constexpr float FadeSpeed = 4.0f;
    constexpr float BobSpeed = 2.4f;
    constexpr float BobHeight = 5.0f;
}

UVHVObjectiveMarkerWidget::UVHVObjectiveMarkerWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(false);
}

void UVHVObjectiveMarkerWidget::SetUIManager(UVHVUIManagerComponent* InUIManager)
{
    UIManager = InUIManager;
}

TSharedRef<SWidget> UVHVObjectiveMarkerWidget::RebuildWidget()
{
    using namespace VHVObjectiveMarker;
    RingBrush = FSlateRoundedBoxBrush(VHVActivityUIStyle::GlassMain(), RingSize * 0.5f, VHVActivityUIStyle::GoldPrimary(), 2.5f);
    DotBrush = FSlateRoundedBoxBrush(VHVActivityUIStyle::GoldPrimary(), DotSize * 0.5f);
    const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.75f);

    TSharedRef<SWidget> Root =
        SNew(SOverlay)
        .Visibility(EVisibility::HitTestInvisible)
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
        [
            SAssignNew(PinRoot, SBox)
            .WidthOverride(PinWidth)
            .HeightOverride(PinHeight)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                [
                    SNew(SBox).WidthOverride(RingSize).HeightOverride(RingSize)
                    [
                        SNew(SBorder)
                        .BorderImage(&RingBrush)
                        .Padding(0.0f)
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SBox).WidthOverride(DotSize).HeightOverride(DotSize)
                            [
                                SNew(SBorder).BorderImage(&DotBrush)
                            ]
                        ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, 3.0f, 0.0f, 0.0f))
                [
                    SAssignNew(DistanceText, STextBlock)
                    .Font(VHVAmbientSpeechStyle::MediumFont(15))
                    .ColorAndOpacity(VHVActivityUIStyle::TextPrimary())
                    .ShadowOffset(FVector2D(1.0f, 1.0f))
                    .ShadowColorAndOpacity(Shadow)
                ]
                + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(0.0f, -4.0f, 0.0f, 0.0f))
                [
                    SAssignNew(PointerText, STextBlock)
                    .Font(VHVAmbientSpeechStyle::MediumFont(18))
                    .ColorAndOpacity(VHVActivityUIStyle::GoldPrimary())
                    .ShadowOffset(FVector2D(1.0f, 1.0f))
                    .ShadowColorAndOpacity(Shadow)
                    .Text(FText::FromString(TEXT("▼")))
                ]
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
        [
            SAssignNew(EdgeArrow, SBox)
            .WidthOverride(ArrowSize)
            .HeightOverride(ArrowSize)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font(VHVAmbientSpeechStyle::MediumFont(ArrowFontSize))
                .ColorAndOpacity(VHVActivityUIStyle::GoldPrimary())
                .ShadowOffset(FVector2D(1.0f, 1.0f))
                .ShadowColorAndOpacity(Shadow)
                .Text(FText::FromString(TEXT("▲")))
            ]
        ];

    PinRoot->SetRenderOpacity(0.0f);
    EdgeArrow->SetRenderOpacity(0.0f);
    EdgeArrow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    return Root;
}

void UVHVObjectiveMarkerWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    Elapsed += InDeltaTime;

    ObjectiveCheckTimer -= InDeltaTime;
    if (ObjectiveCheckTimer <= 0.0f)
    {
        ObjectiveCheckTimer = ObjectiveCheckInterval;
        RefreshObjectiveTarget();
    }

    bool bShow = false;
    APlayerController* PlayerController = GetOwningPlayer();
    const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    const AActor* Target = TargetActor.Get();
    if (Pawn && Target && !IsBlockingUIActive())
    {
        const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), Target->GetActorLocation());
        // Reaching a place completes on the volume overlap itself, so keep guiding until the objective
        // moves on; for people and scenes, hand over to the interaction prompt or the scene up close.
        // A scene that is already playing is being watched: do not lead the player into the crowd.
        const AVHVAmbientConversationActor* Scene = Cast<AVHVAmbientConversationActor>(Target);
        const bool bArrived = (Scene && Scene->IsPlaying())
            || (!Target->IsA<AVHVQuestLocationVolume>() && Distance <= HideWithinDistance);
        if (!bArrived)
        {
            bShow = UpdateScreenPlacement(PlayerController, GetMarkerWorldLocation(Target));
            const int32 Meters = FMath::Max(1, FMath::RoundToInt(Distance / 100.0f));
            if (DistanceText && Meters != ShownMeters)
            {
                ShownMeters = Meters;
                DistanceText->SetText(FText::Format(NSLOCTEXT("VHVQuestMarker", "DistanceMeters", "{0} m"),
                    FText::AsNumber(Meters)));
            }
        }
    }

    bMarkerShown = bShow;
    ApplyPresentation(InDeltaTime);
}

void UVHVObjectiveMarkerWidget::RefreshObjectiveTarget()
{
    const UGameInstance* GameInstance = GetGameInstance();
    const UVHVQuestSubsystem* QuestSubsystem = GameInstance ? GameInstance->GetSubsystem<UVHVQuestSubsystem>() : nullptr;
    FVHVQuestObjectiveDefinition Objective;
    if (!QuestSubsystem || !QuestSubsystem->IsQuestFlowActive() || !QuestSubsystem->GetCurrentObjective(Objective))
    {
        bHasObjective = false;
        TargetActor.Reset();
        return;
    }

    // Objective IDs are freeform and may repeat across quests, so compare everything that picks the target.
    const bool bSameObjective = bHasObjective
        && Objective.ObjectiveID == LastObjectiveID
        && static_cast<uint8>(Objective.ObjectiveType) == LastObjectiveType
        && Objective.GetEffectiveTargetID() == LastTargetID
        && Objective.GetEffectiveWorldActionReceiverID() == LastReceiverID
        && Objective.GetEffectiveNPCParticipantID() == LastNPCParticipantID;

    // Resolve for a new objective, and keep retrying while the target is missing (it may register
    // later, e.g. from a streamed level) or has been destroyed.
    if (!bSameObjective || !TargetActor.IsValid())
    {
        bHasObjective = true;
        LastObjectiveID = Objective.ObjectiveID;
        LastObjectiveType = static_cast<uint8>(Objective.ObjectiveType);
        LastTargetID = Objective.GetEffectiveTargetID();
        LastReceiverID = Objective.GetEffectiveWorldActionReceiverID();
        LastNPCParticipantID = Objective.GetEffectiveNPCParticipantID();
        TargetActor = ResolveTarget(Objective);
    }
}

namespace VHVObjectiveMarker
{
    /** Owner of the first registered component of this class in World that matches. Iterates only that class. */
    template <typename ComponentType, typename PredicateType>
    AActor* FindComponentOwner(const UWorld* World, PredicateType&& Matches)
    {
        for (TObjectIterator<ComponentType> It; It; ++It)
        {
            const ComponentType* Component = *It;
            if (Component && !Component->IsTemplate() && Component->GetWorld() == World && Matches(*Component))
            {
                if (AActor* Owner = Component->GetOwner())
                {
                    return Owner;
                }
            }
        }
        return nullptr;
    }
}

AActor* UVHVObjectiveMarkerWidget::ResolveTarget(const FVHVQuestObjectiveDefinition& Objective) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FName ParticipantID = NAME_None;
    switch (Objective.ObjectiveType)
    {
    case EVHVQuestObjectiveType::Talk:
    case EVHVQuestObjectiveType::Interact:
        ParticipantID = Objective.GetEffectiveTargetID();
        break;
    case EVHVQuestObjectiveType::Conversation:
    case EVHVQuestObjectiveType::LearningActivity:
        // Auto-starting lessons and activities open by themselves wherever the player is.
        if (Objective.bAutoStart)
        {
            return nullptr;
        }
        ParticipantID = Objective.GetEffectiveTargetID();
        break;
    case EVHVQuestObjectiveType::NPCAction:
        ParticipantID = Objective.GetEffectiveNPCParticipantID();
        break;
    case EVHVQuestObjectiveType::ReachLocation:
    {
        const FName LocationID = Objective.GetEffectiveTargetID();
        for (TActorIterator<AVHVQuestLocationVolume> It(const_cast<UWorld*>(World)); It; ++It)
        {
            if (It->bEnabled && It->GetEffectiveLocationID() == LocationID)
            {
                return *It;
            }
        }
        return nullptr;
    }
    case EVHVQuestObjectiveType::WorldAction:
    {
        // Point at the receiver the world action is actually sent to, not just any with the same ID.
        const UVHVWorldActionSubsystem* WorldActions = World->GetSubsystem<UVHVWorldActionSubsystem>();
        const UVHVWorldActionReceiverComponent* Receiver = WorldActions
            ? WorldActions->GetRegisteredReceiver(Objective.GetEffectiveWorldActionReceiverID())
            : nullptr;
        return Receiver ? Receiver->GetOwner() : nullptr;
    }
    default:
        return nullptr;
    }

    if (ParticipantID.IsNone())
    {
        return nullptr;
    }
    return VHVObjectiveMarker::FindComponentOwner<UVHVQuestParticipantComponent>(World,
        [ParticipantID](const UVHVQuestParticipantComponent& Participant)
        {
            return Participant.IsQuestParticipationEnabled() && Participant.GetEffectiveParticipantID() == ParticipantID;
        });
}

FVector UVHVObjectiveMarkerWidget::GetMarkerWorldLocation(const AActor* Target) const
{
    if (const ACharacter* Character = Cast<ACharacter>(Target))
    {
        const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
        const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f;
        return Target->GetActorLocation() + FVector(0.0f, 0.0f, HalfHeight + HeightAboveTarget);
    }
    return Target->GetActorLocation() + FVector(0.0f, 0.0f, HeightAboveTarget + 110.0f);
}

bool UVHVObjectiveMarkerWidget::IsBlockingUIActive() const
{
    const UVHVUIManagerComponent* Manager = UIManager.Get();
    if (Manager && (Manager->CurrentUIState != EVHVUIState::Gameplay || Manager->IsConversationActive()))
    {
        return true;
    }
    const UGameInstance* GameInstance = GetGameInstance();
    const UVHVTextbookSubsystem* Textbook = GameInstance ? GameInstance->GetSubsystem<UVHVTextbookSubsystem>() : nullptr;
    return Textbook && Textbook->IsActivityActive();
}

bool UVHVObjectiveMarkerWidget::UpdateScreenPlacement(const APlayerController* PlayerController, const FVector& WorldLocation)
{
    int32 ViewportX = 0;
    int32 ViewportY = 0;
    PlayerController->GetViewportSize(ViewportX, ViewportY);
    if (ViewportX <= 0 || ViewportY <= 0)
    {
        return false;
    }

    const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
    const FVector2D Size(ViewportX, ViewportY);
    const FVector2D Margin(ScreenEdgeMargin * ViewportScale, ScreenEdgeMargin * ViewportScale);

    FVector CameraLocation;
    FRotator CameraRotation;
    PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
    const FVector Local = CameraRotation.UnrotateVector(WorldLocation - CameraLocation);

    FVector2D ScreenPosition = FVector2D::ZeroVector;
    const bool bProjected = Local.X > 0.0f
        && PlayerController->ProjectWorldLocationToScreen(WorldLocation, ScreenPosition, true);
    const bool bInside = bProjected
        && ScreenPosition.X >= Margin.X && ScreenPosition.X <= Size.X - Margin.X
        && ScreenPosition.Y >= Margin.Y && ScreenPosition.Y <= Size.Y - Margin.Y;

    if (bInside)
    {
        bOnScreenEdge = false;
        MarkerScreenPosition = ScreenPosition;
        return true;
    }

    // Off-screen or behind the camera: pin to the screen edge in the target's direction.
    FVector2D Direction(Local.Y, -Local.Z);
    if (Direction.IsNearlyZero())
    {
        Direction = FVector2D(0.0f, 1.0f);
    }
    Direction.Normalize();
    const FVector2D Center = Size * 0.5f;
    const FVector2D HalfExtent = Center - Margin;
    const float Reach = FMath::Min(
        HalfExtent.X / FMath::Max(FMath::Abs(Direction.X), KINDA_SMALL_NUMBER),
        HalfExtent.Y / FMath::Max(FMath::Abs(Direction.Y), KINDA_SMALL_NUMBER));
    bOnScreenEdge = true;
    EdgeDirection = Direction;
    EdgeArrowAngleDegrees = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
    MarkerScreenPosition = Center + Direction * Reach;
    return true;
}

void UVHVObjectiveMarkerWidget::ApplyPresentation(const float DeltaTime)
{
    using namespace VHVObjectiveMarker;
    Opacity = FMath::FInterpConstantTo(Opacity, bMarkerShown ? 1.0f : 0.0f, DeltaTime, FadeSpeed);
    if (!PinRoot || !EdgeArrow)
    {
        return;
    }

    const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
    const FVector2D Anchor = MarkerScreenPosition / ViewportScale;
    FVector2D PinOffset;
    if (bOnScreenEdge)
    {
        const FVector2D PinCenter = Anchor - EdgeDirection * EdgePinInset;
        PinOffset = PinCenter - FVector2D(PinWidth * 0.5f, RingSize * 0.5f);
    }
    else
    {
        PinOffset = Anchor - FVector2D(PinWidth * 0.5f, PinHeight)
            + FVector2D(0.0f, FMath::Sin(Elapsed * BobSpeed) * BobHeight);
    }

    PinRoot->SetRenderOpacity(Opacity);
    PinRoot->SetRenderTransform(FSlateRenderTransform(FVector2f(PinOffset)));
    if (PointerText)
    {
        PointerText->SetVisibility(bOnScreenEdge ? EVisibility::Collapsed : EVisibility::HitTestInvisible);
    }

    // The arrow glyph points up; rotate it to face the off-screen target.
    EdgeArrow->SetRenderOpacity(bOnScreenEdge ? Opacity : 0.0f);
    EdgeArrow->SetRenderTransform(FSlateRenderTransform(
        FQuat2f(FMath::DegreesToRadians(EdgeArrowAngleDegrees + 90.0f)),
        FVector2f(Anchor - FVector2D(ArrowSize * 0.5f, ArrowSize * 0.5f))));
}
