#include "World/Systems/VHVWorldActionSubsystem.h"

#include "VHV.h"
#include "World/Components/VHVWorldActionReceiverComponent.h"

void UVHVWorldActionSubsystem::Deinitialize()
{
    TArray<FGuid> RequestIDs;
    PendingRequests.GetKeys(RequestIDs);
    for (const FGuid& RequestID : RequestIDs)
    {
        CompleteWorldAction(RequestID, false);
    }

    Receivers.Empty();
    Super::Deinitialize();
}

EVHVWorldActionExecutionResult UVHVWorldActionSubsystem::RequestWorldAction(
    const FName ReceiverID,
    const FName ActionID,
    FGuid& OutRequestID)
{
    OutRequestID.Invalidate();
    if (ReceiverID.IsNone() || ActionID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVWorldAction] Request rejected: Receiver ID and Action ID must both be set."));
        return EVHVWorldActionExecutionResult::Rejected;
    }

    UVHVWorldActionReceiverComponent* Receiver = FindReceiver(ReceiverID);
    if (!Receiver)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVWorldAction] Request for action '%s' failed: no receiver '%s' is registered in world '%s'."),
            *ActionID.ToString(), *ReceiverID.ToString(), *GetNameSafe(GetWorld()));
        return EVHVWorldActionExecutionResult::Rejected;
    }

    OutRequestID = FGuid::NewGuid();
    FPendingWorldAction Pending;
    Pending.Request.RequestID = OutRequestID;
    Pending.Request.ReceiverID = ReceiverID;
    Pending.Request.ActionID = ActionID;
    Pending.Receiver = Receiver;
    PendingRequests.Add(OutRequestID, Pending);

    const EVHVWorldActionExecutionResult Result = Receiver->ExecuteWorldAction(OutRequestID, ActionID);
    switch (Result)
    {
    case EVHVWorldActionExecutionResult::Rejected:
        CompleteWorldAction(OutRequestID, false);
        break;
    case EVHVWorldActionExecutionResult::Completed:
        CompleteWorldAction(OutRequestID, true);
        break;
    case EVHVWorldActionExecutionResult::StartedAsync:
        break;
    default:
        UE_LOG(LogVHV, Warning, TEXT("[VHVWorldAction] Receiver '%s' returned an unsupported result for action '%s'."),
            *ReceiverID.ToString(), *ActionID.ToString());
        CompleteWorldAction(OutRequestID, false);
        return EVHVWorldActionExecutionResult::Rejected;
    }

    return Result;
}

bool UVHVWorldActionSubsystem::CompleteWorldAction(const FGuid RequestID, const bool bSuccess)
{
    if (!RequestID.IsValid())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVWorldAction] Completion rejected: Request ID is invalid."));
        return false;
    }

    FPendingWorldAction Pending;
    if (!PendingRequests.RemoveAndCopyValue(RequestID, Pending))
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVWorldAction] Completion ignored: request '%s' is not pending."), *RequestID.ToString());
        return false;
    }

    OnWorldActionCompleted.Broadcast(
        Pending.Request.RequestID,
        Pending.Request.ReceiverID,
        Pending.Request.ActionID,
        bSuccess);
    return true;
}

bool UVHVWorldActionSubsystem::RegisterReceiver(UVHVWorldActionReceiverComponent* Receiver)
{
    if (!IsValid(Receiver) || Receiver->ReceiverID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVWorldAction] Receiver component on '%s' requires a non-empty Receiver ID."),
            *GetNameSafe(Receiver ? Receiver->GetOwner() : nullptr));
        return false;
    }

    if (TWeakObjectPtr<UVHVWorldActionReceiverComponent>* Existing = Receivers.Find(Receiver->ReceiverID))
    {
        if (Existing->IsValid() && Existing->Get() != Receiver)
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVWorldAction] Duplicate Receiver ID '%s' in world '%s': preserving '%s' and rejecting '%s'."),
                *Receiver->ReceiverID.ToString(), *GetNameSafe(GetWorld()),
                *GetNameSafe(Existing->Get()->GetOwner()), *GetNameSafe(Receiver->GetOwner()));
            return false;
        }
    }

    Receivers.Add(Receiver->ReceiverID, Receiver);
    return true;
}

void UVHVWorldActionSubsystem::UnregisterReceiver(UVHVWorldActionReceiverComponent* Receiver)
{
    if (!Receiver)
    {
        return;
    }

    const TWeakObjectPtr<UVHVWorldActionReceiverComponent>* Registered = Receivers.Find(Receiver->ReceiverID);
    if (!Registered || Registered->Get() != Receiver)
    {
        return;
    }

    Receivers.Remove(Receiver->ReceiverID);
    FailRequestsForReceiver(Receiver);
}

bool UVHVWorldActionSubsystem::CompleteWorldActionForReceiver(
    const FGuid RequestID,
    const bool bSuccess,
    const UVHVWorldActionReceiverComponent* Receiver)
{
    const FPendingWorldAction* Pending = PendingRequests.Find(RequestID);
    if (!Pending || Pending->Receiver.Get() != Receiver)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVWorldAction] Receiver '%s' cannot finish request '%s' because it is not pending for that receiver."),
            *GetNameSafe(Receiver ? Receiver->GetOwner() : nullptr), *RequestID.ToString());
        return false;
    }
    return CompleteWorldAction(RequestID, bSuccess);
}

UVHVWorldActionReceiverComponent* UVHVWorldActionSubsystem::FindReceiver(const FName ReceiverID)
{
    TWeakObjectPtr<UVHVWorldActionReceiverComponent>* Receiver = Receivers.Find(ReceiverID);
    if (!Receiver)
    {
        return nullptr;
    }
    if (!Receiver->IsValid())
    {
        Receivers.Remove(ReceiverID);
        return nullptr;
    }
    return Receiver->Get();
}

void UVHVWorldActionSubsystem::FailRequestsForReceiver(const UVHVWorldActionReceiverComponent* Receiver)
{
    TArray<FGuid> RequestIDs;
    for (const TPair<FGuid, FPendingWorldAction>& Pair : PendingRequests)
    {
        if (Pair.Value.Receiver.Get() == Receiver)
        {
            RequestIDs.Add(Pair.Key);
        }
    }
    for (const FGuid& RequestID : RequestIDs)
    {
        CompleteWorldAction(RequestID, false);
    }
}
