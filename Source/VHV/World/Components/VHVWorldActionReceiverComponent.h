#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "World/Types/VHVWorldActionTypes.h"
#include "VHVWorldActionReceiverComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVHVWorldActionRequested, FGuid, RequestID, FName, ActionID);

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVWorldActionReceiverComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVHVWorldActionReceiverComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|World Action")
    FName ReceiverID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHV|World Action")
    bool bWorldActionsEnabled = true;

    UPROPERTY(BlueprintAssignable, Category = "VHV|World Action|Events")
    FOnVHVWorldActionRequested OnWorldActionRequested;

    UFUNCTION(BlueprintNativeEvent, Category = "VHV|World Action", meta = (DisplayName = "Handle World Action"))
    EVHVWorldActionExecutionResult HandleWorldAction(FGuid RequestID, FName ActionID);

    UFUNCTION(BlueprintCallable, Category = "VHV|World Action")
    bool FinishWorldAction(FGuid RequestID, bool bSuccess);

    UFUNCTION(BlueprintCallable, Category = "VHV|World Action")
    bool SetWorldActionExecutionResult(FGuid RequestID, EVHVWorldActionExecutionResult Result);

    EVHVWorldActionExecutionResult ExecuteWorldAction(FGuid RequestID, FName ActionID);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual EVHVWorldActionExecutionResult HandleWorldAction_Implementation(FGuid RequestID, FName ActionID);

private:
    FGuid DispatchingRequestID;
    EVHVWorldActionExecutionResult DispatchingResult = EVHVWorldActionExecutionResult::Rejected;
    bool bDispatchingWorldAction = false;
    bool bDispatchResponseProvided = false;

    void RegisterWithWorldActionSubsystem();
    void UnregisterFromWorldActionSubsystem();
};
