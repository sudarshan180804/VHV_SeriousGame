#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VHVNPCPatrolRoute.generated.h"

UENUM(BlueprintType)
enum class EVHVNPCPatrolMode : uint8
{
	Once UMETA(DisplayName = "Once"),
	Loop UMETA(DisplayName = "Loop")
};

USTRUCT(BlueprintType)
struct VHV_API FVHVNPCPatrolWaypoint
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|NPC|Patrol")
	TObjectPtr<AActor> TargetActor;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|NPC|Patrol", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float WaitDuration = 0.0f;
};

UCLASS(Blueprintable)
class VHV_API AVHVNPCPatrolRoute : public AActor
{
	GENERATED_BODY()

public:
	AVHVNPCPatrolRoute();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|NPC|Patrol", meta = (TitleProperty = "TargetActor"))
	TArray<FVHVNPCPatrolWaypoint> Waypoints;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|NPC|Patrol")
	EVHVNPCPatrolMode PatrolMode = EVHVNPCPatrolMode::Loop;

	bool ValidateRoute(FString& OutError) const;
};
