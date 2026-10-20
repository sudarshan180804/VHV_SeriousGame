#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VHVNavigationGuidanceActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;

/**
 * Lightweight world presentation for objective navigation.
 *
 * The destination uses crossed, soft beam cards and a refined double-ring aura.
 * Route arrows are batched into one HISM component and rebuilt only after a
 * throttled NavMesh query, so there are no per-arrow actors or per-frame work.
 */
UCLASS(NotBlueprintable, Config=Game)
class VHV_API AVHVNavigationGuidanceActor : public AActor
{
    GENERATED_BODY()

public:
    AVHVNavigationGuidanceActor();

    UFUNCTION(BlueprintCallable, Category = "VHV|Navigation Guidance")
    void SetDestination(const FVector& InDestination);

    UFUNCTION(BlueprintCallable, Category = "VHV|Navigation Guidance")
    void SetGuidanceVisible(bool bVisible);

    /** Rebuilds the NavMesh route when movement/time thresholds require it. */
    UFUNCTION(BlueprintCallable, Category = "VHV|Navigation Guidance")
    bool RefreshPathFrom(
        const FVector& PlayerLocation,
        AActor* NavigationAgent,
        bool bForceRefresh = false);

    UFUNCTION(BlueprintPure, Category = "VHV|Navigation Guidance|Diagnostics")
    int32 GetArrowPartInstanceCount() const;

protected:
    virtual void BeginPlay() override;

    /** Route distance of the immediate guidance arrow from the player. */
    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Route",
        meta = (ClampMin = "100.0", UIMin = "150.0", UIMax = "500.0"))
    float FirstArrowDistance = 250.0f;

    /** Route distance of the advance/turn guidance arrow from the player. */
    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Route",
        meta = (ClampMin = "200.0", UIMin = "350.0", UIMax = "1000.0"))
    float SecondArrowDistance = 400.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Route",
        meta = (ClampMin = "60.0", UIMin = "100.0", UIMax = "260.0"))
    float ArrowLength = 170.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Route",
        meta = (ClampMin = "40.0", UIMin = "70.0", UIMax = "200.0"))
    float ArrowWidth = 125.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Route",
        meta = (ClampMin = "3.0", UIMin = "6.0", UIMax = "24.0"))
    float ArrowStrokeWidth = 16.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Route",
        meta = (ClampMin = "1", ClampMax = "2"))
    int32 MaxArrowCount = 2;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Route",
        meta = (ClampMin = "0.5", UIMin = "1.0", UIMax = "8.0"))
    float PathRefreshInterval = 3.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Route",
        meta = (ClampMin = "100.0", UIMin = "250.0", UIMax = "1000.0"))
    float PlayerMoveRefreshDistance = 500.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Route",
        meta = (ClampMin = "25.0", UIMin = "75.0", UIMax = "300.0"))
    float ArrowVisualRefreshDistance = 125.0f;

    /** Distance away from the cached route that permits a new NavMesh query. */
    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Route",
        meta = (ClampMin = "100.0", UIMin = "200.0", UIMax = "1000.0"))
    float RouteDeviationRepathDistance = 400.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Route")
    FVector NavigationProjectionExtent = FVector(350.0f, 350.0f, 600.0f);

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Destination",
        meta = (ClampMin = "1000.0", UIMin = "3000.0", UIMax = "15000.0"))
    float BeamHeight = 10500.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Destination",
        meta = (ClampMin = "40.0", UIMin = "60.0", UIMax = "300.0"))
    float BeamWidth = 180.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Destination",
        meta = (ClampMin = "80.0", UIMin = "120.0", UIMax = "500.0"))
    float OuterBeamWidth = 360.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Destination",
        meta = (ClampMin = "60.0", UIMin = "100.0", UIMax = "350.0"))
    float GroundGlyphRadius = 175.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Style")
    FLinearColor GuidanceColor = FLinearColor(1.0f, 0.28f, 0.015f, 1.0f);

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Style",
        meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "6.0"))
    float GroundEmissiveStrength = 4.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Style",
        meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "8.0"))
    float BeamEmissiveStrength = 5.0f;

    UPROPERTY(EditAnywhere, Config, Category = "VHV|Navigation Guidance|Style",
        meta = (ClampMin = "0.05", UIMin = "0.15", UIMax = "0.65"))
    float OuterBeamOpacity = 0.32f;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "VHV|Navigation Guidance")
    TObjectPtr<UStaticMeshComponent> OuterBeamA;

    UPROPERTY(VisibleAnywhere, Category = "VHV|Navigation Guidance")
    TObjectPtr<UStaticMeshComponent> OuterBeamB;

    UPROPERTY(VisibleAnywhere, Category = "VHV|Navigation Guidance")
    TObjectPtr<UStaticMeshComponent> InnerBeamA;

    UPROPERTY(VisibleAnywhere, Category = "VHV|Navigation Guidance")
    TObjectPtr<UStaticMeshComponent> InnerBeamB;

    UPROPERTY(VisibleAnywhere, Category = "VHV|Navigation Guidance")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> GroundGlyphInstances;

    UPROPERTY(VisibleAnywhere, Category = "VHV|Navigation Guidance")
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ArrowInstances;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> BeamMaterials;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> GroundMaterial;

    FVector Destination = FVector::ZeroVector;
    FVector LastPathOrigin = FVector::ZeroVector;
    FVector LastPathDestination = FVector::ZeroVector;
    FVector LastArrowPresentationOrigin = FVector(BIG_NUMBER);
    TArray<FVector> CachedPathPoints;
    float CachedPathLength = 0.0f;
    float LastPathRefreshWorldTime = -BIG_NUMBER;
    bool bHasDestination = false;
    bool bHasValidPath = false;
    bool bLoggedPathFailure = false;

    void ConfigureBeam(UStaticMeshComponent* Beam, float Width, float YawDegrees) const;
    void BuildDestinationGlyph();
    void RebuildArrowInstances(const FVector& PlayerLocation, AActor* NavigationAgent);
    void AddArrowAt(const FVector& Position, const FVector& Tangent, AActor* NavigationAgent);
    void AddStripInstance(
        const FVector& Start,
        const FVector& End,
        float Width,
        const FVector& SurfaceNormal);
    void ClearRoute();
    bool ShouldRefreshPath(const FVector& PlayerLocation, bool bForceRefresh) const;
    bool ShouldRefreshArrowPresentation(const FVector& PlayerLocation) const;
    bool IsPlayerOffCachedRoute(const FVector& PlayerLocation) const;
    static float FindClosestPathDistance2D(
        const TArray<FVector>& PathPoints,
        const FVector& Location);

    static bool SamplePathAtDistance(
        const TArray<FVector>& PathPoints,
        float DistanceAlongPath,
        FVector& OutPosition,
        FVector& OutTangent);
};
