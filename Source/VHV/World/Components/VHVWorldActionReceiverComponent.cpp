#include "World/Components/VHVWorldActionReceiverComponent.h"

#include "World/Systems/VHVWorldActionSubsystem.h"

UVHVWorldActionReceiverComponent::UVHVWorldActionReceiverComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVHVWorldActionReceiverComponent::BeginPlay()
{
    Super::BeginPlay();
    RegisterWithWorldActionSubsystem();
}

void UVHVWorldActionReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnregisterFromWorldActionSubsystem();
    Super::EndPlay(EndPlayReason);
}

EVHVWorldActionExecutionResult UVHVWorldActionReceiverComponent::HandleWorldAction_Implementation(
    const FGuid RequestID,
    const FName ActionID)
{
    DispatchingRequestID = RequestID;
    DispatchingResult = EVHVWorldActionExecutionResult::Rejected;
    bDispatchingWorldAction = true;
    bDispatchResponseProvided = false;
    OnWorldActionRequested.Broadcast(RequestID, ActionID);
    bDispatchingWorldAction = false;
    DispatchingRequestID.Invalidate();
    return DispatchingResult;
}

EVHVWorldActionExecutionResult UVHVWorldActionReceiverComponent::ExecuteWorldAction(
    const FGuid RequestID,
    const FName ActionID)
{
    if (!bWorldActionsEnabled)
    {
        return EVHVWorldActionExecutionResult::Rejected;
    }
    return HandleWorldAction(RequestID, ActionID);
}

bool UVHVWorldActionReceiverComponent::FinishWorldAction(const FGuid RequestID, const bool bSuccess)
{
    UWorld* World = GetWorld();
    UVHVWorldActionSubsystem* Subsystem = World ? World->GetSubsystem<UVHVWorldActionSubsystem>() : nullptr;
    return Subsystem && Subsystem->CompleteWorldActionForReceiver(RequestID, bSuccess, this);
}

bool UVHVWorldActionReceiverComponent::SetWorldActionExecutionResult(
    const FGuid RequestID,
    const EVHVWorldActionExecutionResult Result)
{
    if (!bDispatchingWorldAction || bDispatchResponseProvided || RequestID != DispatchingRequestID)
    {
        return false;
    }
    DispatchingResult = Result;
    bDispatchResponseProvided = true;
    return true;
}

void UVHVWorldActionReceiverComponent::RegisterWithWorldActionSubsystem()
{
    if (UWorld* World = GetWorld())
    {
        if (UVHVWorldActionSubsystem* Subsystem = World->GetSubsystem<UVHVWorldActionSubsystem>())
        {
            Subsystem->RegisterReceiver(this);
        }
    }
}

void UVHVWorldActionReceiverComponent::UnregisterFromWorldActionSubsystem()
{
    if (UWorld* World = GetWorld())
    {
        if (UVHVWorldActionSubsystem* Subsystem = World->GetSubsystem<UVHVWorldActionSubsystem>())
        {
            Subsystem->UnregisterReceiver(this);
        }
    }
}
