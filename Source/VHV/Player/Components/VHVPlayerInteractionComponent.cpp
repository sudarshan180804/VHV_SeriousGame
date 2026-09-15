#include "VHVPlayerInteractionComponent.h"
#include "Engine/CollisionProfile.h"
#include "VHVInteractionComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "VHV.h"
#include "VHV/Core/VHVCollisionChannels.h"

UVHVPlayerInteractionComponent::UVHVPlayerInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    CurrentTarget = nullptr;
}

void UVHVPlayerInteractionComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    FindInteractionTarget();
}

void UVHVPlayerInteractionComponent::FindInteractionTarget()
{
    AActor* OwnerActor = GetOwner();

    if (!OwnerActor || !GetWorld())
    {
        return;
    }

    APawn* PawnOwner = Cast<APawn>(OwnerActor);

    if (!PawnOwner)
    {
        return;
    }

    AController* Controller = PawnOwner->GetController();

    if (!Controller)
    {
        return;
    }

    APlayerController* PlayerController = Cast<APlayerController>(Controller);

    if (!PlayerController || !PlayerController->PlayerCameraManager)
    {
        return;
    }

    const FVector CameraLocation = PlayerController->PlayerCameraManager->GetCameraLocation();
    const FVector CameraForward = PlayerController->PlayerCameraManager->GetCameraRotation().Vector();
    const FVector TraceEnd = CameraLocation + (CameraForward * InteractionDistance);

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerActor);
    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = false;

    FCollisionObjectQueryParams ObjectQueryParams;
    ObjectQueryParams.AddObjectTypesToQuery(VHV_INTERACTABLE_CHANNEL);

    TArray<FHitResult> HitResults;
    const bool bHit = GetWorld()->LineTraceMultiByObjectType(
        HitResults,
        CameraLocation,
        TraceEnd,
        ObjectQueryParams,
        QueryParams
    );

    UVHVInteractionComponent* BestTarget = nullptr;
    float BestScore = -1.0f;

    if (bHit)
    {
        for (const FHitResult& HitResult : HitResults)
        {
            AActor* HitActor = HitResult.GetActor();
            if (!HitActor)
            {
                continue;
            }

            const float Distance = (HitResult.ImpactPoint - CameraLocation).Size();
            if (Distance > InteractionDistance)
            {
                continue;
            }

            UVHVInteractionComponent* InteractionComponent =
                HitActor->FindComponentByClass<UVHVInteractionComponent>();

            if (!InteractionComponent || !InteractionComponent->CanInteract())
            {
                continue;
            }

            const FVector ToHit = (HitResult.ImpactPoint - CameraLocation).GetSafeNormal();
            const float ViewScore = FVector::DotProduct(ToHit, CameraForward);

            if (ViewScore > BestScore)
            {
                BestTarget = InteractionComponent;
                BestScore = ViewScore;
            }
        }
    }

    if (BestTarget != CurrentTarget)
    {
        CurrentTarget = BestTarget;

        if (CurrentTarget)
        {
            UE_LOG(LogVHV, Warning, TEXT("VHV Interaction: Target = %s"), *GetNameSafe(CurrentTarget->GetOwner()));
        }
        else
        {
            UE_LOG(LogVHV, Warning, TEXT("VHV Interaction: No target"));
        }

        OnInteractionTargetChanged.Broadcast(CurrentTarget);
    }
}

void UVHVPlayerInteractionComponent::TryInteract()
{
    if (!CurrentTarget)
    {
        UE_LOG(LogVHV, Warning, TEXT("VHV Interaction: Interact pressed but no target"));
        return;
    }

    if (!CurrentTarget->CanInteract())
    {
        UE_LOG(LogVHV, Warning, TEXT("VHV Interaction: Interact pressed but no target"));
        return;
    }

    UE_LOG(LogVHV, Warning, TEXT("VHV Interaction: Interacting with %s"), *GetNameSafe(CurrentTarget->GetOwner()));
    CurrentTarget->Interact();
}

bool UVHVPlayerInteractionComponent::HasInteractionTarget() const
{
    return CurrentTarget != nullptr;
}

UVHVInteractionComponent*
UVHVPlayerInteractionComponent::GetCurrentTarget() const
{
    return CurrentTarget;
}