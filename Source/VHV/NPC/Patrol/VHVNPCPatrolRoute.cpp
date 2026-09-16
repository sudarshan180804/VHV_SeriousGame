#include "NPC/Patrol/VHVNPCPatrolRoute.h"

AVHVNPCPatrolRoute::AVHVNPCPatrolRoute()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AVHVNPCPatrolRoute::ValidateRoute(FString& OutError) const
{
	if (Waypoints.IsEmpty())
	{
		OutError = TEXT("route has no waypoints");
		return false;
	}

	for (int32 Index = 0; Index < Waypoints.Num(); ++Index)
	{
		const FVHVNPCPatrolWaypoint& Waypoint = Waypoints[Index];
		if (!IsValid(Waypoint.TargetActor))
		{
			OutError = FString::Printf(TEXT("waypoint %d has no valid target actor"), Index);
			return false;
		}
		if (Waypoint.WaitDuration < 0.0f)
		{
			OutError = FString::Printf(TEXT("waypoint %d has a negative wait duration"), Index);
			return false;
		}
	}

	OutError.Reset();
	return true;
}
