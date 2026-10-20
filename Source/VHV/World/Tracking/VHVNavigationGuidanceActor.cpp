#include "World/Tracking/VHVNavigationGuidanceActor.h"

#include "VHV.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float ArrowGroundLift = 4.0f;
    constexpr float ArrowThickness = 1.6f;
    constexpr float MinArrowDistanceFromDestination = 180.0f;

    void ConfigureGuidancePrimitive(UPrimitiveComponent* Component)
    {
        if (!Component)
        {
            return;
        }

        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
        Component->SetCastShadow(false);
        Component->bCastDynamicShadow = false;
        Component->bCastStaticShadow = false;
        Component->bReceivesDecals = false;
        Component->PrimaryComponentTick.bCanEverTick = false;
    }
}

AVHVNavigationGuidanceActor::AVHVNavigationGuidanceActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetReplicates(false);
    SetActorEnableCollision(false);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(
        TEXT("/Engine/BasicShapes/Plane.Plane"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(
        TEXT("/Engine/BasicShapes/Cube.Cube"));

    const auto CreateBeam = [this](const TCHAR* Name)
    {
        UStaticMeshComponent* Beam = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Beam->SetupAttachment(SceneRoot);
        if (PlaneMesh.Succeeded())
        {
            Beam->SetStaticMesh(PlaneMesh.Object);
        }
        ConfigureGuidancePrimitive(Beam);
        Beam->SetTranslucentSortPriority(2);
        return Beam;
    };

    OuterBeamA = CreateBeam(TEXT("OuterBeamA"));
    OuterBeamB = CreateBeam(TEXT("OuterBeamB"));
    InnerBeamA = CreateBeam(TEXT("InnerBeamA"));
    InnerBeamB = CreateBeam(TEXT("InnerBeamB"));

    GroundGlyphInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
        TEXT("GroundGlyphInstances"));
    GroundGlyphInstances->SetupAttachment(SceneRoot);
    ArrowInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
        TEXT("ArrowInstances"));
    ArrowInstances->SetupAttachment(SceneRoot);

    for (UHierarchicalInstancedStaticMeshComponent* Instances
        : {GroundGlyphInstances.Get(), ArrowInstances.Get()})
    {
        if (CubeMesh.Succeeded())
        {
            Instances->SetStaticMesh(CubeMesh.Object);
        }
        ConfigureGuidancePrimitive(Instances);
        Instances->SetCullDistances(0, 60000);
    }

    SetActorHiddenInGame(true);
}

void AVHVNavigationGuidanceActor::BeginPlay()
{
    Super::BeginPlay();

    UMaterialInterface* BeamMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Game/VHV_Stuff/UI/Tracking/M_VHV_GuidanceBeam.M_VHV_GuidanceBeam"));
    UMaterialInterface* CoreMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Game/VHV_Stuff/UI/Tracking/M_VHV_GuidanceCore.M_VHV_GuidanceCore"));
    UMaterialInterface* GoldMaterial = LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Game/VHV_Stuff/UI/Tracking/M_VHV_GuidanceGold.M_VHV_GuidanceGold"));
    if (!BeamMaterial)
    {
        BeamMaterial = LoadObject<UMaterialInterface>(
            nullptr, TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    }
    if (!GoldMaterial)
    {
        GoldMaterial = BeamMaterial;
    }
    if (!CoreMaterial)
    {
        CoreMaterial = GoldMaterial;
    }

    const auto ApplyBeamMaterial = [this](
        UStaticMeshComponent* Beam,
        UMaterialInterface* Material,
        const float Opacity,
        const float Strength)
    {
        if (!Beam || !Material)
        {
            return;
        }

        UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(
            Material, this);
        DynamicMaterial->SetVectorParameterValue(TEXT("GuidanceColor"), GuidanceColor);
        DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), Opacity);
        DynamicMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), Strength);
        Beam->SetMaterial(0, DynamicMaterial);
        BeamMaterials.Add(DynamicMaterial);
    };

    ApplyBeamMaterial(
        OuterBeamA, BeamMaterial, OuterBeamOpacity, BeamEmissiveStrength * 0.65f);
    ApplyBeamMaterial(
        OuterBeamB, BeamMaterial, OuterBeamOpacity, BeamEmissiveStrength * 0.65f);
    ApplyBeamMaterial(InnerBeamA, CoreMaterial, 1.0f, BeamEmissiveStrength);
    ApplyBeamMaterial(InnerBeamB, CoreMaterial, 1.0f, BeamEmissiveStrength);

    if (GoldMaterial)
    {
        GroundMaterial = UMaterialInstanceDynamic::Create(GoldMaterial, this);
        GroundMaterial->SetVectorParameterValue(TEXT("GuidanceColor"), GuidanceColor);
        GroundMaterial->SetScalarParameterValue(
            TEXT("EmissiveStrength"), GroundEmissiveStrength);
        GroundGlyphInstances->SetMaterial(0, GroundMaterial);
        ArrowInstances->SetMaterial(0, GroundMaterial);
    }

    ConfigureBeam(OuterBeamA, OuterBeamWidth, 0.0f);
    ConfigureBeam(OuterBeamB, OuterBeamWidth, 90.0f);
    ConfigureBeam(InnerBeamA, BeamWidth, 45.0f);
    ConfigureBeam(InnerBeamB, BeamWidth, 135.0f);
    BuildDestinationGlyph();
}

void AVHVNavigationGuidanceActor::ConfigureBeam(
    UStaticMeshComponent* Beam,
    const float Width,
    const float YawDegrees) const
{
    if (!Beam)
    {
        return;
    }

    // Engine Plane is 100 cm in local X/Y. Roll turns local Y into world Z,
    // while yaw gives the crossed cards view-independent coverage.
    Beam->SetRelativeLocation(FVector(0.0f, 0.0f, BeamHeight * 0.5f));
    Beam->SetRelativeRotation(FRotator(0.0f, YawDegrees, 90.0f));
    Beam->SetRelativeScale3D(FVector(Width / 100.0f, BeamHeight / 100.0f, 1.0f));
}

void AVHVNavigationGuidanceActor::BuildDestinationGlyph()
{
    if (!GroundGlyphInstances)
    {
        return;
    }

    GroundGlyphInstances->ClearInstances();

    const auto AddRing = [this](
        const float Radius,
        const int32 SegmentCount,
        const float StrokeWidth,
        const float Height,
        const bool bBreakAtCardinals)
    {
        const float SegmentLength = (2.0f * PI * Radius / SegmentCount) * 0.82f;
        for (int32 Index = 0; Index < SegmentCount; ++Index)
        {
            // Deliberate cardinal gaps on the outer ring read as ceremonial
            // rather than technical; the inner halo remains continuous.
            if (bBreakAtCardinals && Index % (SegmentCount / 4) == 0)
            {
                continue;
            }

            const float AngleDegrees = 360.0f * Index / SegmentCount;
            const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
            const FVector Center(
                FMath::Cos(AngleRadians) * Radius,
                FMath::Sin(AngleRadians) * Radius,
                Height);
            GroundGlyphInstances->AddInstance(FTransform(
                FRotator(0.0f, AngleDegrees + 90.0f, 0.0f),
                Center,
                FVector(SegmentLength / 100.0f, StrokeWidth / 100.0f,
                    ArrowThickness / 100.0f)), false);
        }
    };

    AddRing(GroundGlyphRadius, 24, 10.0f, ArrowGroundLift, true);
    AddRing(GroundGlyphRadius * 0.62f, 16, 5.0f, ArrowGroundLift + 0.8f, false);
}

void AVHVNavigationGuidanceActor::SetDestination(const FVector& InDestination)
{
    Destination = InDestination;
    bHasDestination = true;
    LastPathDestination = FVector(BIG_NUMBER);
    LastArrowPresentationOrigin = FVector(BIG_NUMBER);
    LastPathRefreshWorldTime = -BIG_NUMBER;
    ClearRoute();
    SetActorLocation(Destination, false, nullptr, ETeleportType::TeleportPhysics);
}

void AVHVNavigationGuidanceActor::SetGuidanceVisible(const bool bVisible)
{
    SetActorHiddenInGame(!bVisible);
    SetActorTickEnabled(false);
}

int32 AVHVNavigationGuidanceActor::GetArrowPartInstanceCount() const
{
    return ArrowInstances ? ArrowInstances->GetInstanceCount() : 0;
}

bool AVHVNavigationGuidanceActor::ShouldRefreshPath(
    const FVector& PlayerLocation,
    const bool bForceRefresh) const
{
    if (bForceRefresh)
    {
        return true;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const float Elapsed = World->GetTimeSeconds() - LastPathRefreshWorldTime;
    if (!bHasValidPath || CachedPathPoints.Num() < 2)
    {
        // A missing route is retried at the same low rate as an off-route
        // correction, never on every guidance service tick.
        return Elapsed >= PathRefreshInterval;
    }

    const bool bDestinationMoved = FVector::DistSquared2D(Destination, LastPathDestination)
        >= FMath::Square(50.0f);
    const bool bPlayerMovedFar = FVector::DistSquared2D(PlayerLocation, LastPathOrigin)
        >= FMath::Square(PlayerMoveRefreshDistance);
    return bDestinationMoved
        || (Elapsed >= PathRefreshInterval && bPlayerMovedFar
            && IsPlayerOffCachedRoute(PlayerLocation));
}

bool AVHVNavigationGuidanceActor::ShouldRefreshArrowPresentation(
    const FVector& PlayerLocation) const
{
    return FVector::DistSquared2D(PlayerLocation, LastArrowPresentationOrigin)
        >= FMath::Square(ArrowVisualRefreshDistance);
}

bool AVHVNavigationGuidanceActor::IsPlayerOffCachedRoute(
    const FVector& PlayerLocation) const
{
    if (CachedPathPoints.Num() < 2)
    {
        return true;
    }

    const float ClosestPathDistance = FindClosestPathDistance2D(
        CachedPathPoints, PlayerLocation);
    FVector ClosestPosition;
    FVector UnusedTangent;
    return !SamplePathAtDistance(
        CachedPathPoints, ClosestPathDistance, ClosestPosition, UnusedTangent)
        || FVector::DistSquared2D(PlayerLocation, ClosestPosition)
            >= FMath::Square(RouteDeviationRepathDistance);
}

bool AVHVNavigationGuidanceActor::RefreshPathFrom(
    const FVector& PlayerLocation,
    AActor* NavigationAgent,
    const bool bForceRefresh)
{
    if (!bHasDestination)
    {
        return bHasValidPath;
    }

    if (!ShouldRefreshPath(PlayerLocation, bForceRefresh))
    {
        if (ShouldRefreshArrowPresentation(PlayerLocation))
        {
            RebuildArrowInstances(PlayerLocation, NavigationAgent);
        }
        return bHasValidPath;
    }

    UWorld* World = GetWorld();
    UNavigationSystemV1* NavigationSystem = World
        ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)
        : nullptr;
    LastPathOrigin = PlayerLocation;
    LastPathDestination = Destination;
    LastPathRefreshWorldTime = World ? World->GetTimeSeconds() : 0.0f;

    FNavLocation ProjectedStart;
    FNavLocation ProjectedDestination;
    const bool bProjected = NavigationSystem
        && NavigationSystem->ProjectPointToNavigation(
            PlayerLocation, ProjectedStart, NavigationProjectionExtent)
        && NavigationSystem->ProjectPointToNavigation(
            Destination, ProjectedDestination, NavigationProjectionExtent);
    if (bProjected)
    {
        // Semantic actors and volumes are often centred above the floor. Keep
        // the blessing/rune at the walkable endpoint while retaining the real
        // semantic location for arrival checks.
        SetActorLocation(
            ProjectedDestination.Location,
            false,
            nullptr,
            ETeleportType::TeleportPhysics);
    }
    UNavigationPath* Path = bProjected
        ? UNavigationSystemV1::FindPathToLocationSynchronously(
            World,
            ProjectedStart.Location,
            ProjectedDestination.Location,
            NavigationAgent)
        : nullptr;

    if (!Path || !Path->IsValid() || Path->IsPartial() || Path->PathPoints.Num() < 2)
    {
        ClearRoute();
        if (!bLoggedPathFailure)
        {
            UE_LOG(LogVHV, Warning,
                TEXT("[VHVGuidance] No complete NavMesh path to destination; keeping only the destination blessing."));
            bLoggedPathFailure = true;
        }
        return false;
    }

    bLoggedPathFailure = false;
    bHasValidPath = true;
    CachedPathPoints.Reset(Path->PathPoints.Num());
    CachedPathPoints.Append(Path->PathPoints);
    CachedPathLength = 0.0f;
    for (int32 Index = 1; Index < CachedPathPoints.Num(); ++Index)
    {
        CachedPathLength += FVector::Distance(
            CachedPathPoints[Index - 1], CachedPathPoints[Index]);
    }
    RebuildArrowInstances(PlayerLocation, NavigationAgent);
    return true;
}

void AVHVNavigationGuidanceActor::RebuildArrowInstances(
    const FVector& PlayerLocation,
    AActor* NavigationAgent)
{
    ArrowInstances->ClearInstances();
    LastArrowPresentationOrigin = PlayerLocation;

    if (CachedPathPoints.Num() < 2 || CachedPathLength <= 0.0f)
    {
        return;
    }

    const float PlayerPathDistance = FindClosestPathDistance2D(
        CachedPathPoints, PlayerLocation);
    const float FurthestArrowDistance = CachedPathLength - MinArrowDistanceFromDestination;
    const float ArrowOffsets[] = {
        FirstArrowDistance,
        SecondArrowDistance};
    const int32 ArrowLimit = FMath::Clamp(MaxArrowCount, 1, UE_ARRAY_COUNT(ArrowOffsets));
    for (int32 Index = 0; Index < ArrowLimit; ++Index)
    {
        const float Distance = PlayerPathDistance + ArrowOffsets[Index];
        if (Distance >= FurthestArrowDistance)
        {
            continue;
        }

        FVector Position;
        FVector Tangent;
        if (SamplePathAtDistance(CachedPathPoints, Distance, Position, Tangent))
        {
            AddArrowAt(Position, Tangent, NavigationAgent);
        }
    }
}

bool AVHVNavigationGuidanceActor::SamplePathAtDistance(
    const TArray<FVector>& PathPoints,
    float DistanceAlongPath,
    FVector& OutPosition,
    FVector& OutTangent)
{
    for (int32 Index = 1; Index < PathPoints.Num(); ++Index)
    {
        const FVector Segment = PathPoints[Index] - PathPoints[Index - 1];
        const float SegmentLength = Segment.Size();
        if (SegmentLength <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        if (DistanceAlongPath <= SegmentLength)
        {
            OutTangent = Segment / SegmentLength;
            OutPosition = PathPoints[Index - 1] + OutTangent * DistanceAlongPath;
            return true;
        }
        DistanceAlongPath -= SegmentLength;
    }

    if (PathPoints.Num() >= 2)
    {
        OutPosition = PathPoints.Last();
        OutTangent = (PathPoints.Last() - PathPoints[PathPoints.Num() - 2]).GetSafeNormal();
        return !OutTangent.IsNearlyZero();
    }
    return false;
}

float AVHVNavigationGuidanceActor::FindClosestPathDistance2D(
    const TArray<FVector>& PathPoints,
    const FVector& Location)
{
    float ClosestDistanceSquared = TNumericLimits<float>::Max();
    float DistanceAlongPath = 0.0f;
    float ClosestPathDistance = 0.0f;

    for (int32 Index = 1; Index < PathPoints.Num(); ++Index)
    {
        const FVector Segment = PathPoints[Index] - PathPoints[Index - 1];
        const float SegmentLength = Segment.Size();
        if (SegmentLength <= KINDA_SMALL_NUMBER)
        {
            continue;
        }

        const FVector Segment2D(Segment.X, Segment.Y, 0.0f);
        const float SegmentLengthSquared2D = Segment2D.SizeSquared();
        const FVector ToPlayer2D = FVector(
            Location.X - PathPoints[Index - 1].X,
            Location.Y - PathPoints[Index - 1].Y,
            0.0f);
        const float Alpha = SegmentLengthSquared2D > KINDA_SMALL_NUMBER
            ? FMath::Clamp(FVector::DotProduct(ToPlayer2D, Segment2D)
                / SegmentLengthSquared2D, 0.0f, 1.0f)
            : 0.0f;
        const FVector ClosestPoint = PathPoints[Index - 1] + Segment * Alpha;
        const float DistanceSquared = FVector::DistSquared2D(Location, ClosestPoint);
        if (DistanceSquared < ClosestDistanceSquared)
        {
            ClosestDistanceSquared = DistanceSquared;
            ClosestPathDistance = DistanceAlongPath + SegmentLength * Alpha;
        }
        DistanceAlongPath += SegmentLength;
    }

    return ClosestPathDistance;
}

void AVHVNavigationGuidanceActor::AddArrowAt(
    const FVector& Position,
    const FVector& Tangent,
    AActor* NavigationAgent)
{
    FVector SurfacePosition = Position;
    FVector SurfaceNormal = FVector::UpVector;

    FHitResult Hit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VHVGuidanceGroundTrace), false, this);
    QueryParams.AddIgnoredActor(this);
    if (NavigationAgent)
    {
        QueryParams.AddIgnoredActor(NavigationAgent);
    }

    if (GetWorld()->LineTraceSingleByChannel(
        Hit,
        Position + FVector::UpVector * 250.0f,
        Position - FVector::UpVector * 500.0f,
        ECC_Visibility,
        QueryParams))
    {
        SurfacePosition = Hit.ImpactPoint;
        SurfaceNormal = Hit.ImpactNormal.GetSafeNormal();
    }

    FVector Forward = FVector::VectorPlaneProject(Tangent, SurfaceNormal).GetSafeNormal();
    if (Forward.IsNearlyZero())
    {
        Forward = FVector::ForwardVector;
    }
    const FVector Right = FVector::CrossProduct(SurfaceNormal, Forward).GetSafeNormal();
    const FVector Center = SurfacePosition + SurfaceNormal * ArrowGroundLift;

    const FVector Tip = Center + Forward * (ArrowLength * 0.48f);
    const FVector ShoulderCenter = Center - Forward * (ArrowLength * 0.32f);
    const FVector LeftShoulder = ShoulderCenter + Right * (ArrowWidth * 0.5f);
    const FVector RightShoulder = ShoulderCenter - Right * (ArrowWidth * 0.5f);

    // Two short chevron arms only: no stem, interior stripe, or debug-like tail.
    AddStripInstance(LeftShoulder, Tip, ArrowStrokeWidth, SurfaceNormal);
    AddStripInstance(RightShoulder, Tip, ArrowStrokeWidth, SurfaceNormal);
}

void AVHVNavigationGuidanceActor::AddStripInstance(
    const FVector& Start,
    const FVector& End,
    const float Width,
    const FVector& SurfaceNormal)
{
    const FVector Delta = End - Start;
    const float Length = Delta.Size();
    if (Length <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const FTransform Transform(
        FRotationMatrix::MakeFromXZ(Delta / Length, SurfaceNormal).Rotator(),
        (Start + End) * 0.5f,
        FVector(Length / 100.0f, Width / 100.0f, ArrowThickness / 100.0f));
    ArrowInstances->AddInstance(Transform, true);
}

void AVHVNavigationGuidanceActor::ClearRoute()
{
    bHasValidPath = false;
    CachedPathPoints.Reset();
    CachedPathLength = 0.0f;
    LastArrowPresentationOrigin = FVector(BIG_NUMBER);
    if (ArrowInstances)
    {
        ArrowInstances->ClearInstances();
    }
}
