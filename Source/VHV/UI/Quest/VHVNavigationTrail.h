#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VHVNavigationTrail.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UVHVObjectiveMarkerWidget;

/**
 * Glowing arrows on the ground leading from the player to the current objective, like a GPS route.
 *
 * Follows the objective marker: same target, shown only while the marker is shown (so it hides
 * during dialogue and learning UI, for auto-starting lessons, and once the player has arrived).
 * The route is a navmesh path, so it bends around houses and fences the way the player must walk.
 * Arrows are spaced back from the target so they stay fixed in the world as the player walks. Each
 * stores its negated distance to the target as per-instance custom data 0; M_NavTrail uses it to
 * run a glow towards the target.
 */
UCLASS(NotPlaceable)
class VHV_API AVHVNavigationTrail : public AActor
{
    GENERATED_BODY()

public:
    AVHVNavigationTrail();

    void SetObjectiveMarker(UVHVObjectiveMarkerWidget* InMarker);

    UFUNCTION(BlueprintPure, Category = "VHV|Navigation Trail")
    int32 GetArrowCount() const;

    /** Length (cm) of the current route (arrows may be zero when it is very short), or 0 when there is none. */
    UFUNCTION(BlueprintPure, Category = "VHV|Navigation Trail")
    float GetTrailLength() const { return TrailLength; }

    virtual void Tick(float DeltaSeconds) override;

protected:
    UPROPERTY(VisibleAnywhere, Category = "VHV|Navigation Trail")
    TObjectPtr<UInstancedStaticMeshComponent> Arrows;

    UPROPERTY(EditAnywhere, Category = "VHV|Navigation Trail")
    TObjectPtr<UStaticMesh> ArrowMesh;

    UPROPERTY(EditAnywhere, Category = "VHV|Navigation Trail")
    TObjectPtr<UMaterialInterface> ArrowMaterial;

    /** Distance (cm) between arrows along the route. */
    UPROPERTY(EditAnywhere, Category = "VHV|Navigation Trail")
    float ArrowSpacing = 160.0f;

    /** Gap (cm) left in front of the player and before the target. */
    UPROPERTY(EditAnywhere, Category = "VHV|Navigation Trail")
    float StartGap = 180.0f;

    UPROPERTY(EditAnywhere, Category = "VHV|Navigation Trail")
    float EndGap = 170.0f;

    /** Height (cm) of the arrows above the ground. */
    UPROPERTY(EditAnywhere, Category = "VHV|Navigation Trail")
    float HoverHeight = 6.0f;

    /** Seconds between route updates while the player or target moves. */
    UPROPERTY(EditAnywhere, Category = "VHV|Navigation Trail")
    float RefreshInterval = 0.25f;

    UPROPERTY(EditAnywhere, Category = "VHV|Navigation Trail")
    int32 MaxArrows = 160;

    /** Seconds between retries while there is no route (e.g. the navmesh is still building), even if nobody moves. */
    UPROPERTY(EditAnywhere, Category = "VHV|Navigation Trail")
    float RetryInterval = 1.0f;

    /** How far (cm) from the target the route may end, for targets standing just off the navmesh. */
    UPROPERTY(EditAnywhere, Category = "VHV|Navigation Trail")
    float GoalSearchRadius = 300.0f;

private:
    void ClearTrail();
    void RebuildTrail(const TArray<FVector>& PathPoints, const AActor* IgnoredTarget);

    TWeakObjectPtr<UVHVObjectiveMarkerWidget> Marker;
    TWeakObjectPtr<AActor> LastTarget;
    FVector LastStart = FVector::ZeroVector;
    FVector LastTargetLocation = FVector::ZeroVector;
    float TimeSinceRefresh = 0.0f;
    float TrailLength = 0.0f;
    bool bRouteFailed = false;
};
