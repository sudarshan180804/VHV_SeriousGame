#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VHVObjectiveTrackingMarker.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

/** Cheap, reusable world-space marker for the current objective destination. */
UCLASS(NotBlueprintable, Transient)
class VHV_API AVHVObjectiveTrackingMarker : public AActor
{
    GENERATED_BODY()

public:
    AVHVObjectiveTrackingMarker();

    void SetTrackingLocation(const FVector& GroundLocation);
    void SetTrackingVisible(bool bVisible);

    static constexpr float BeamHeight = 20000.0f;
    static constexpr float BeamRadius = 75.0f;

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "VHV|Tracking")
    TObjectPtr<UStaticMeshComponent> BeamMesh;
};
