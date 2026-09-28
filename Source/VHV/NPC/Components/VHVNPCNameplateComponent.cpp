#include "NPC/Components/VHVNPCNameplateComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UI/NPC/VHVNPCNameplateWidget.h"
#include "VHV.h"

UVHVNPCNameplateComponent::UVHVNPCNameplateComponent()
{
	// Screen-space WidgetComponents use their inherited tick to register with and
	// update the player's screen layer. Disabling it prevents any screen rendering.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetWidgetClass(UVHVNPCNameplateWidget::StaticClass());
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
	SetPivot(FVector2D(0.5f, 0.5f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetWindowFocusable(false);
	SetVisibility(false);
}

void UVHVNPCNameplateComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveAnchor();
	UpdateAnchorLocation();
	RefreshNameplate();
}

void UVHVNPCNameplateComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	UpdateAnchorLocation();
	VisibilityRefreshElapsed += DeltaTime;
	if (VisibilityRefreshElapsed >= FMath::Max(0.05f, VisibilityRefreshInterval))
	{
		VisibilityRefreshElapsed = 0.0f;
		UpdateNameplateVisibility();
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UVHVNPCNameplateComponent::InitWidget()
{
	Super::InitWidget();
	NameplateWidget = Cast<UVHVNPCNameplateWidget>(GetUserWidgetObject());
	if (NameplateWidget)
	{
		NameplateWidget->TakeWidget();
		NameplateWidget->SetDisplayName(NameplateDisplayName);
	}
}

void UVHVNPCNameplateComponent::RefreshNameplate()
{
	const bool bShouldShow = bShowNameplate && !NameplateDisplayName.IsEmpty();
	if (!bShouldShow)
	{
		SetVisibility(false);
		SetComponentTickEnabled(false);
		return;
	}

	if (!EnsureNameplateWidget())
	{
		SetVisibility(false);
		return;
	}

	NameplateWidget->SetDisplayName(NameplateDisplayName);
	SetHiddenInGame(false);
	SetComponentTickEnabled(true);
	VisibilityRefreshElapsed = 0.0f;
	UpdateAnchorLocation();
	UpdateNameplateVisibility();
	UpdateWidget();
	UE_LOG(LogVHV, Verbose,
		TEXT("[VHVNameplate] Ready owner='%s' name='%s' widget=%s visible=%s hiddenInGame=%s screenSpace=%s desiredSize=%s tick=%s."),
		*GetNameSafe(GetOwner()), *NameplateDisplayName.ToString(), *GetNameSafe(GetUserWidgetObject()),
		IsVisible() ? TEXT("true") : TEXT("false"), bHiddenInGame ? TEXT("true") : TEXT("false"),
		GetWidgetSpace() == EWidgetSpace::Screen ? TEXT("true") : TEXT("false"),
		GetDrawAtDesiredSize() ? TEXT("true") : TEXT("false"), IsComponentTickEnabled() ? TEXT("true") : TEXT("false"));
}

void UVHVNPCNameplateComponent::SetNameplateDisplayName(const FText& InDisplayName)
{
	NameplateDisplayName = InDisplayName;
	RefreshNameplate();
}

bool UVHVNPCNameplateComponent::EnsureNameplateWidget()
{
	if (!GetWidgetClass())
	{
		SetWidgetClass(UVHVNPCNameplateWidget::StaticClass());
	}
	InitWidget();
	if (NameplateWidget)
	{
		NameplateWidget->TakeWidget();
	}
	return NameplateWidget != nullptr;
}

void UVHVNPCNameplateComponent::ResolveAnchor()
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		AnchorMesh = Character->GetMesh();
		ResolvedHeadSocket = ResolveHeadSocket(AnchorMesh);
	}
}

void UVHVNPCNameplateComponent::UpdateAnchorLocation()
{
	FVector HeadLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	if (AnchorMesh && !ResolvedHeadSocket.IsNone())
	{
		HeadLocation = AnchorMesh->GetSocketLocation(ResolvedHeadSocket);
	}
	else if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			HeadLocation = Capsule->GetComponentLocation()
				+ FVector::UpVector * Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	SetWorldLocation(HeadLocation + FVector::UpVector * NameplateHeadPadding);
}

void UVHVNPCNameplateComponent::UpdateNameplateVisibility()
{
	SetVisibility(ShouldShowNameplate());
}

bool UVHVNPCNameplateComponent::ShouldShowNameplate() const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!bShowNameplate || NameplateDisplayName.IsEmpty() || !Owner || Owner->IsHidden() || !World)
	{
		return false;
	}

	const APlayerController* PlayerController = World->GetFirstPlayerController();
	const APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager : nullptr;
	if (!PlayerController || !CameraManager)
	{
		// Local camera setup can lag component initialization. Keep the named widget
		// eligible and retry on the next periodic refresh instead of latching hidden.
		return true;
	}

	const FVector CameraLocation = CameraManager->GetCameraLocation();
	const FVector AnchorLocation = GetComponentLocation();
	if (NameplateMaxVisibleDistance > 0.0f
		&& FVector::DistSquared(CameraLocation, AnchorLocation)
			> FMath::Square(NameplateMaxVisibleDistance))
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VHVNPCNameplateOcclusion), false);
	QueryParams.AddIgnoredActor(Owner);
	if (const APawn* PlayerPawn = PlayerController->GetPawn())
	{
		QueryParams.AddIgnoredActor(PlayerPawn);
	}

	FHitResult Hit;
	return !World->LineTraceSingleByChannel(
		Hit, CameraLocation, AnchorLocation, ECC_Visibility, QueryParams);
}

FName UVHVNPCNameplateComponent::ResolveHeadSocket(const USkeletalMeshComponent* Mesh) const
{
	if (!Mesh)
	{
		return NAME_None;
	}
	if (!AnchorSocketName.IsNone() && Mesh->DoesSocketExist(AnchorSocketName))
	{
		return AnchorSocketName;
	}
	if (!bPreferHeadSocket)
	{
		return NAME_None;
	}

	static const FName HeadCandidates[] =
	{
		TEXT("head"),
		TEXT("HeadSocket"),
		TEXT("head_socket")
	};
	for (const FName Candidate : HeadCandidates)
	{
		if (Mesh->DoesSocketExist(Candidate))
		{
			return Candidate;
		}
	}
	return NAME_None;
}
