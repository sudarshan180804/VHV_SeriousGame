#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "VHVObjectiveMarkerWidget.generated.h"

class SBox;
class STextBlock;
class UVHVUIManagerComponent;
struct FVHVQuestObjectiveDefinition;

/**
 * Floating guidance marker over the person or place the current quest objective points at.
 *
 * The game has no other navigation aid, so this keeps first-time players from getting lost:
 * it shows the distance, stays visible through buildings, moves to the screen edge with an
 * arrow when the target is off-screen or behind the camera, and hides while dialogue or
 * learning UI is open, for auto-starting lesson objectives, and once the player is close
 * enough to a person or scene for the interaction prompt or the scene to take over (places keep
 * their marker until the player is inside the volume and the objective completes).
 */
UCLASS()
class VHV_API UVHVObjectiveMarkerWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UVHVObjectiveMarkerWidget(const FObjectInitializer& ObjectInitializer);

    void SetUIManager(UVHVUIManagerComponent* InUIManager);

    /** Actor the current objective points at, or null when the objective has no world target. */
    UFUNCTION(BlueprintPure, Category = "VHV|Quest Marker")
    AActor* GetMarkerTarget() const { return TargetActor.Get(); }

    /** True while the marker is being shown (it may still be fading in). */
    UFUNCTION(BlueprintPure, Category = "VHV|Quest Marker")
    bool IsMarkerShown() const { return bMarkerShown; }

    /** True when the target is off-screen and the marker is pinned to the screen edge. */
    UFUNCTION(BlueprintPure, Category = "VHV|Quest Marker")
    bool IsMarkerOnScreenEdge() const { return bOnScreenEdge; }

    /** Marker anchor in viewport pixels (valid while shown). */
    UFUNCTION(BlueprintPure, Category = "VHV|Quest Marker")
    FVector2D GetMarkerScreenPosition() const { return MarkerScreenPosition; }

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    /** Within this distance (cm) of a person or scene the marker hides; the interaction prompt or scene takes over. Not used for places. */
    UPROPERTY(EditAnywhere, Category = "VHV|Quest Marker")
    float HideWithinDistance = 350.0f;

    /** Height (cm) of the marker above a character's head or above other targets' origin. */
    UPROPERTY(EditAnywhere, Category = "VHV|Quest Marker")
    float HeightAboveTarget = 70.0f;

    /** Distance (Slate units) kept between an off-screen marker and the screen edge. */
    UPROPERTY(EditAnywhere, Category = "VHV|Quest Marker")
    float ScreenEdgeMargin = 72.0f;

private:
    /** Reads the current objective (a few times a second) and resolves its target when it changes or is missing. */
    void RefreshObjectiveTarget();
    AActor* ResolveTarget(const FVHVQuestObjectiveDefinition& Objective) const;
    FVector GetMarkerWorldLocation(const AActor* Target) const;
    /** True while dialogue or learning UI is open, when the marker would only be in the way. */
    bool IsBlockingUIActive() const;
    /** Places the marker over the target, or on the screen edge when it is off-screen. */
    bool UpdateScreenPlacement(const APlayerController* PlayerController, const FVector& WorldLocation);
    void ApplyPresentation(float DeltaTime);

    static constexpr float ObjectiveCheckInterval = 0.2f;

    TWeakObjectPtr<UVHVUIManagerComponent> UIManager;
    TWeakObjectPtr<AActor> TargetActor;
    float ObjectiveCheckTimer = 0.0f;
    bool bHasObjective = false;
    FName LastObjectiveID;
    uint8 LastObjectiveType = MAX_uint8;
    FName LastTargetID;
    FName LastReceiverID;
    FName LastNPCParticipantID;
    int32 ShownMeters = -1;

    bool bMarkerShown = false;
    bool bOnScreenEdge = false;
    float Opacity = 0.0f;
    float Elapsed = 0.0f;
    float EdgeArrowAngleDegrees = 0.0f;
    FVector2D MarkerScreenPosition = FVector2D::ZeroVector;
    FVector2D EdgeDirection = FVector2D(0.0f, 1.0f);

    TSharedPtr<SBox> PinRoot;
    TSharedPtr<STextBlock> DistanceText;
    TSharedPtr<STextBlock> PointerText;
    TSharedPtr<SBox> EdgeArrow;

    FSlateBrush RingBrush;
    FSlateBrush DotBrush;
};
