#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VHVNPCPresentationComponent.generated.h"

class UAnimMontage;
class UVHVNPCActionSet;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVHVNPCPresentationActionCompleted, FName, ActionID, bool, bSuccess);

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVNPCPresentationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVHVNPCPresentationComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Presentation")
    TObjectPtr<UVHVNPCActionSet> ActionSet;

    UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Presentation")
    bool PlayAction(FName ActionID);

    UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Presentation")
    void StopCurrentAction();

    UFUNCTION(BlueprintPure, Category = "VHV|NPC|Presentation")
    bool IsActionPlaying() const;

    UFUNCTION(BlueprintPure, Category = "VHV|NPC|Presentation")
    FName GetCurrentActionID() const;

    void StopCurrentActionSilently();

    UPROPERTY(BlueprintAssignable, Category = "VHV|NPC|Presentation")
    FOnVHVNPCPresentationActionCompleted OnPresentationActionCompleted;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UAnimMontage> CurrentMontage;

    FName CurrentActionID;
    uint32 PlaybackSerial = 0;
    uint32 ActivePlaybackSerial = 0;

    void StopCurrentActionInternal(bool bBroadcastCompletion);
    void FinishCurrentAction(bool bSuccess, bool bBroadcastCompletion);
    void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint32 CompletedPlaybackSerial);
};
