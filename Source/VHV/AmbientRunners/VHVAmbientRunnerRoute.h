#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Navigation/PathFollowingComponent.h"
#include "VHVAmbientRunnerRoute.generated.h"

class AAIController;
class UAnimInstance;
class UBillboardComponent;
class USkeletalMesh;
class USplineComponent;
class UVHVNPCAnimationProfile;

UENUM(BlueprintType)
enum class EVHVAmbientRunnerCharacter : uint8
{
	Kid UMETA(DisplayName = "Kid"),
	Athlete UMETA(DisplayName = "Athlete"),
	ChaiDaughter UMETA(DisplayName = "Chai Daughter")
};

/** Skeleton-safe visual and animation configuration for one ambient runner. */
UCLASS(BlueprintType)
class VHV_API UVHVAmbientRunnerCharacterData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	TSubclassOf<UAnimInstance> AnimationClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	TObjectPtr<UVHVNPCAnimationProfile> AnimationProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	FVector MeshRelativeLocation = FVector(0.0f, 0.0f, -96.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	FRotator MeshRelativeRotation = FRotator(0.0f, -90.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	FVector MeshRelativeScale = FVector::OneVector;

	UFUNCTION(BlueprintPure, Category = "VHV|Ambient Runner")
	bool IsUsable() const;
};

/** Runtime-only in-place animated character owned by a route actor. */
UCLASS(NotPlaceable, Transient)
class VHV_API AVHVAmbientRunnerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AVHVAmbientRunnerCharacter();
	void Configure(const UVHVAmbientRunnerCharacterData* Data, float MovementSpeed);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVHVNPCAnimationProfile> RuntimeAnimationProfile;
};

/**
 * Placeable three-point background-running route. Movement is driven by one
 * path request at a time and completion callbacks; it performs no gameplay Tick.
 */
UCLASS(Blueprintable)
class VHV_API AVHVAmbientRunnerRoute : public AActor
{
	GENERATED_BODY()

public:
	AVHVAmbientRunnerRoute();
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	TObjectPtr<UBillboardComponent> Start;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	TObjectPtr<UBillboardComponent> Mid;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Route")
	TObjectPtr<UBillboardComponent> End;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	EVHVAmbientRunnerCharacter Character = EVHVAmbientRunnerCharacter::Kid;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner")
	bool bLoopIndefinitely = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "1.0", Units = "cm/s"))
	float RunningSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runner", meta = (ClampMin = "0.0", Units = "s"))
	float WaitTimeAtPoints = 0.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runner|Character Data")
	TObjectPtr<UVHVAmbientRunnerCharacterData> KidData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runner|Character Data")
	TObjectPtr<UVHVAmbientRunnerCharacterData> AthleteData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Runner|Character Data")
	TObjectPtr<UVHVAmbientRunnerCharacterData> ChaiDaughterData;

private:
	UPROPERTY(VisibleAnywhere, Category = "Route")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Route")
	TObjectPtr<USplineComponent> RoutePreview;

	UPROPERTY(Transient)
	TObjectPtr<AVHVAmbientRunnerCharacter> SpawnedRunner;

	UPROPERTY(Transient)
	TObjectPtr<AAIController> RunnerController;

	FTimerHandle RouteTimer;
	int32 CurrentPointIndex = 0;
	int32 RequestedPointIndex = INDEX_NONE;
	bool bRouteStopped = false;

	const UVHVAmbientRunnerCharacterData* GetSelectedCharacterData() const;
	int32 GetRoutePointCount() const;
	FVector GetPointLocation(int32 Index) const;
	void BeginRouteMovement();
	void MoveToNextPoint();
	void HandleMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result);
	void StopRoute(const TCHAR* Reason);
};
