#include "AmbientRunners/VHVAmbientRunnerRoute.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NPC/Animation/VHVMixamoNPCAnimInstance.h"
#include "NPC/Animation/VHVNPCAnimationProfile.h"
#include "TimerManager.h"

#if WITH_EDITORONLY_DATA
#include "UObject/ConstructorHelpers.h"
#endif

bool UVHVAmbientRunnerCharacterData::IsUsable() const
{
	return SkeletalMesh && AnimationClass && AnimationProfile
		&& AnimationProfile->RunningAnimation
		&& AnimationProfile->IsCompatibleWithSkeleton(SkeletalMesh->GetSkeleton());
}

AVHVAmbientRunnerCharacter::AVHVAmbientRunnerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	Movement->bUseControllerDesiredRotation = false;
	Movement->MaxWalkSpeed = 450.0f;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AVHVAmbientRunnerCharacter::Configure(
	const UVHVAmbientRunnerCharacterData* Data,
	const float MovementSpeed)
{
	if (!Data)
	{
		return;
	}

	RuntimeAnimationProfile = Data->AnimationProfile;
	GetMesh()->SetSkeletalMeshAsset(Data->SkeletalMesh);
	GetMesh()->SetRelativeLocation(Data->MeshRelativeLocation);
	GetMesh()->SetRelativeRotation(Data->MeshRelativeRotation);
	GetMesh()->SetRelativeScale3D(Data->MeshRelativeScale);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetAnimInstanceClass(Data->AnimationClass);
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(1.0f, MovementSpeed);
}

void AVHVAmbientRunnerCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (UVHVMixamoNPCAnimInstance* AnimInstance =
		Cast<UVHVMixamoNPCAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->SetAnimationProfile(RuntimeAnimationProfile);
		AnimInstance->SetRequestedPosture(EVHVNPCPosture::Standing);
		AnimInstance->SetSpecialActionActive(false);
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}
}

AVHVAmbientRunnerRoute::AVHVAmbientRunnerRoute()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RouteRoot"));
	SetRootComponent(SceneRoot);

	Start = CreateDefaultSubobject<UBillboardComponent>(TEXT("Start"));
	Start->SetupAttachment(SceneRoot);
	Start->SetRelativeLocation(FVector::ZeroVector);
	Start->SetRelativeScale3D(FVector(1.5f));
	Start->SetHiddenInGame(true);
	Start->bSelectable = true;

	Mid = CreateDefaultSubobject<UBillboardComponent>(TEXT("Mid"));
	Mid->SetupAttachment(SceneRoot);
	Mid->SetRelativeLocation(FVector(500.0f, 0.0f, 0.0f));
	Mid->SetRelativeScale3D(FVector(1.5f));
	Mid->SetHiddenInGame(true);
	Mid->bSelectable = true;

	End = CreateDefaultSubobject<UBillboardComponent>(TEXT("End"));
	End->SetupAttachment(SceneRoot);
	End->SetRelativeLocation(FVector(1000.0f, 0.0f, 0.0f));
	End->SetRelativeScale3D(FVector(1.5f));
	End->SetHiddenInGame(true);
	End->bSelectable = true;

	RoutePreview = CreateDefaultSubobject<USplineComponent>(TEXT("RoutePreview"));
	RoutePreview->SetupAttachment(SceneRoot);
	RoutePreview->SetHiddenInGame(true);
	RoutePreview->SetClosedLoop(true);
	RoutePreview->SetIsVisualizationComponent(true);

#if WITH_EDITORONLY_DATA
	static ConstructorHelpers::FObjectFinder<UTexture2D> MarkerTexture(
		TEXT("/Engine/EditorResources/S_Note.S_Note"));
	if (MarkerTexture.Succeeded())
	{
		Start->SetSprite(MarkerTexture.Object);
		Mid->SetSprite(MarkerTexture.Object);
		End->SetSprite(MarkerTexture.Object);
	}
#endif
}

void AVHVAmbientRunnerRoute::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RoutePreview->ClearSplinePoints(false);
	RoutePreview->AddSplinePoint(Start->GetRelativeLocation(), ESplineCoordinateSpace::Local, false);
	RoutePreview->AddSplinePoint(Mid->GetRelativeLocation(), ESplineCoordinateSpace::Local, false);
	RoutePreview->AddSplinePoint(End->GetRelativeLocation(), ESplineCoordinateSpace::Local, false);
	RoutePreview->SetClosedLoop(true, true);
}

void AVHVAmbientRunnerRoute::BeginPlay()
{
	Super::BeginPlay();
	if (!bEnabled)
	{
		return;
	}

	const UVHVAmbientRunnerCharacterData* Data = GetSelectedCharacterData();
	if (!Data || !Data->IsUsable())
	{
		StopRoute(TEXT("selected character has no skeleton-compatible running configuration"));
		return;
	}

	FVector SpawnLocation = Start->GetComponentLocation();
	if (const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation ProjectedLocation;
		if (Navigation->ProjectPointToNavigation(
			SpawnLocation, ProjectedLocation, FVector(150.0f, 150.0f, 500.0f)))
		{
			SpawnLocation = ProjectedLocation.Location + FVector(0.0f, 0.0f, 96.0f);
		}
	}

	const FTransform SpawnTransform(GetActorRotation(), SpawnLocation);
	SpawnedRunner = GetWorld()->SpawnActorDeferred<AVHVAmbientRunnerCharacter>(
		AVHVAmbientRunnerCharacter::StaticClass(), SpawnTransform, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!SpawnedRunner)
	{
		StopRoute(TEXT("runner spawn failed"));
		return;
	}

	SpawnedRunner->Configure(Data, RunningSpeed);
	UGameplayStatics::FinishSpawningActor(SpawnedRunner, SpawnTransform);
	SpawnedRunner->SpawnDefaultController();
	RunnerController = Cast<AAIController>(SpawnedRunner->GetController());
	if (!RunnerController || !RunnerController->GetPathFollowingComponent())
	{
		StopRoute(TEXT("runner AI controller was unavailable"));
		return;
	}

	RunnerController->GetPathFollowingComponent()->OnRequestFinished.AddUObject(
		this, &AVHVAmbientRunnerRoute::HandleMoveCompleted);
	CurrentPointIndex = 0;
	GetWorldTimerManager().SetTimer(
		RouteTimer, this, &AVHVAmbientRunnerRoute::BeginRouteMovement, 0.05f, false);
}

void AVHVAmbientRunnerRoute::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RouteTimer);
	if (RunnerController && RunnerController->GetPathFollowingComponent())
	{
		RunnerController->GetPathFollowingComponent()->OnRequestFinished.RemoveAll(this);
	}
	if (SpawnedRunner && !SpawnedRunner->IsActorBeingDestroyed())
	{
		SpawnedRunner->Destroy();
	}
	Super::EndPlay(EndPlayReason);
}

const UVHVAmbientRunnerCharacterData* AVHVAmbientRunnerRoute::GetSelectedCharacterData() const
{
	switch (Character)
	{
	case EVHVAmbientRunnerCharacter::Athlete:
		return AthleteData;
	case EVHVAmbientRunnerCharacter::ChaiDaughter:
		return ChaiDaughterData;
	case EVHVAmbientRunnerCharacter::Kid:
	default:
		return KidData;
	}
}

FVector AVHVAmbientRunnerRoute::GetPointLocation(const int32 Index) const
{
	switch (Index)
	{
	case 1:
		return Mid->GetComponentLocation();
	case 2:
		return End->GetComponentLocation();
	case 0:
	default:
		return Start->GetComponentLocation();
	}
}

void AVHVAmbientRunnerRoute::BeginRouteMovement()
{
	if (!bRouteStopped)
	{
		MoveToNextPoint();
	}
}

void AVHVAmbientRunnerRoute::MoveToNextPoint()
{
	if (bRouteStopped || !RunnerController || !SpawnedRunner)
	{
		return;
	}

	if (!bLoopIndefinitely && CurrentPointIndex == 2)
	{
		return;
	}

	RequestedPointIndex = (CurrentPointIndex + 1) % 3;
	const EPathFollowingRequestResult::Type RequestResult = RunnerController->MoveToLocation(
		GetPointLocation(RequestedPointIndex), 35.0f, true, true, true, false, nullptr, true);
	if (RequestResult == EPathFollowingRequestResult::Failed)
	{
		StopRoute(TEXT("NavMesh path request failed"));
	}
	else if (RequestResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		CurrentPointIndex = RequestedPointIndex;
		RequestedPointIndex = INDEX_NONE;
		GetWorldTimerManager().SetTimer(
			RouteTimer, this, &AVHVAmbientRunnerRoute::MoveToNextPoint,
			FMath::Max(WaitTimeAtPoints, 0.01f), false);
	}
}

void AVHVAmbientRunnerRoute::HandleMoveCompleted(
	const FAIRequestID RequestID,
	const FPathFollowingResult& Result)
{
	if (bRouteStopped || RequestedPointIndex == INDEX_NONE)
	{
		return;
	}
	if (!Result.IsSuccess())
	{
		StopRoute(TEXT("NavMesh movement did not complete"));
		return;
	}

	CurrentPointIndex = RequestedPointIndex;
	RequestedPointIndex = INDEX_NONE;
	if (!bLoopIndefinitely && CurrentPointIndex == 2)
	{
		return;
	}
	if (WaitTimeAtPoints <= KINDA_SMALL_NUMBER)
	{
		MoveToNextPoint();
	}
	else
	{
		GetWorldTimerManager().SetTimer(
			RouteTimer, this, &AVHVAmbientRunnerRoute::MoveToNextPoint,
			WaitTimeAtPoints, false);
	}
}

void AVHVAmbientRunnerRoute::StopRoute(const TCHAR* Reason)
{
	if (bRouteStopped)
	{
		return;
	}
	bRouteStopped = true;
	UE_LOG(LogTemp, Warning, TEXT("[VHVAmbientRunner] Route '%s' stopped: %s."), *GetName(), Reason);
	if (RunnerController)
	{
		RunnerController->StopMovement();
	}
}
