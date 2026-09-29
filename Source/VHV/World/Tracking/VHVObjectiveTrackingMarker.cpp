#include "World/Tracking/VHVObjectiveTrackingMarker.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AVHVObjectiveTrackingMarker::AVHVObjectiveTrackingMarker()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(false);
    SetActorEnableCollision(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMesh"));
    BeamMesh->SetupAttachment(SceneRoot);
    BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BeamMesh->SetGenerateOverlapEvents(false);
    BeamMesh->SetCanEverAffectNavigation(false);
    BeamMesh->SetCastShadow(false);
    BeamMesh->bCastDynamicShadow = false;
    BeamMesh->bCastStaticShadow = false;
    BeamMesh->bReceivesDecals = false;
    BeamMesh->PrimaryComponentTick.bCanEverTick = false;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        BeamMesh->SetStaticMesh(CylinderMesh.Object);
        // The engine cylinder is 100 cm tall with a 50 cm radius.
        BeamMesh->SetRelativeScale3D(FVector(
            BeamRadius / 50.0f,
            BeamRadius / 50.0f,
            BeamHeight / 100.0f));
    }

    SetActorHiddenInGame(true);
}

void AVHVObjectiveTrackingMarker::BeginPlay()
{
    Super::BeginPlay();
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(
        nullptr,
        TEXT("/Game/VHV_Stuff/UI/Tracking/M_VHV_ObjectiveTrackingBeam.M_VHV_ObjectiveTrackingBeam"));
    if (!Material)
    {
        Material = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }
    if (BeamMesh && Material)
    {
        BeamMesh->SetMaterial(0, Material);
    }
}

void AVHVObjectiveTrackingMarker::SetTrackingLocation(const FVector& GroundLocation)
{
    // Keep the cylinder base at the authored semantic location instead of
    // centering half of the visual below the ground.
    SetActorLocation(GroundLocation + FVector(0.0f, 0.0f, BeamHeight * 0.5f),
        false, nullptr, ETeleportType::TeleportPhysics);
}

void AVHVObjectiveTrackingMarker::SetTrackingVisible(const bool bVisible)
{
    SetActorHiddenInGame(!bVisible);
    if (BeamMesh)
    {
        BeamMesh->SetVisibility(bVisible, true);
    }
}
