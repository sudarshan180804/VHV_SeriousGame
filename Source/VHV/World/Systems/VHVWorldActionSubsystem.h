#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/Types/VHVWorldActionTypes.h"
#include "VHVWorldActionSubsystem.generated.h"

class UVHVWorldActionReceiverComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FOnVHVWorldActionCompleted,
    FGuid, RequestID,
    FName, ReceiverID,
    FName, ActionID,
    bool, bSuccess);

UCLASS()
class VHV_API UVHVWorldActionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "VHV|World Action")
    EVHVWorldActionExecutionResult RequestWorldAction(FName ReceiverID, FName ActionID, FGuid& OutRequestID);

    UFUNCTION(BlueprintCallable, Category = "VHV|World Action")
    bool CompleteWorldAction(FGuid RequestID, bool bSuccess);

    UPROPERTY(BlueprintAssignable, Category = "VHV|World Action|Events")
    FOnVHVWorldActionCompleted OnWorldActionCompleted;

    bool RegisterReceiver(UVHVWorldActionReceiverComponent* Receiver);
    void UnregisterReceiver(UVHVWorldActionReceiverComponent* Receiver);
    bool CompleteWorldActionForReceiver(FGuid RequestID, bool bSuccess, const UVHVWorldActionReceiverComponent* Receiver);

    /** The receiver that world actions for this ID are sent to (the first one registered), or null. */
    const UVHVWorldActionReceiverComponent* GetRegisteredReceiver(FName ReceiverID) const
    {
        const TWeakObjectPtr<UVHVWorldActionReceiverComponent>* Receiver = Receivers.Find(ReceiverID);
        return Receiver ? Receiver->Get() : nullptr;
    }

private:
    struct FPendingWorldAction
    {
        FVHVWorldActionRequest Request;
        TWeakObjectPtr<UVHVWorldActionReceiverComponent> Receiver;
    };

    TMap<FName, TWeakObjectPtr<UVHVWorldActionReceiverComponent>> Receivers;
    TMap<FGuid, FPendingWorldAction> PendingRequests;

    UVHVWorldActionReceiverComponent* FindReceiver(FName ReceiverID);
    void FailRequestsForReceiver(const UVHVWorldActionReceiverComponent* Receiver);
};
