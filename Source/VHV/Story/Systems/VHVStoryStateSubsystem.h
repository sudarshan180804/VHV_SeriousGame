#pragma once

#include "CoreMinimal.h"
#include "Story/Types/VHVStoryStateTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VHVStoryStateSubsystem.generated.h"

struct FVHVStoryStateSaveState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVHVStoryFlagChanged, FName, FlagID, bool, bValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVHVStoryCounterChanged, FName, CounterID, int32, NewValue);

UCLASS()
class VHV_API UVHVStoryStateSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "VHV|Story State")
    bool HasFlag(FName FlagID) const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Story State")
    void SetFlag(FName FlagID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Story State")
    void ClearFlag(FName FlagID);

    UFUNCTION(BlueprintPure, Category = "VHV|Story State")
    int32 GetCounter(FName CounterID) const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Story State")
    void SetCounter(FName CounterID, int32 Value);

    UFUNCTION(BlueprintCallable, Category = "VHV|Story State")
    int32 AddCounter(FName CounterID, int32 Delta);

    UFUNCTION(BlueprintCallable, Category = "VHV|Story State")
    void ResetStoryState();

    UFUNCTION(BlueprintPure, Category = "VHV|Story State|Conditions")
    bool EvaluateCondition(const FVHVStoryCondition& Condition) const;

    UFUNCTION(BlueprintPure, Category = "VHV|Story State|Conditions")
    bool EvaluateConditionSet(const FVHVStoryConditionSet& ConditionSet) const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Story State|Effects")
    bool ApplyEffect(const FVHVStoryEffect& Effect);

    UFUNCTION(BlueprintCallable, Category = "VHV|Story State|Effects")
    bool ApplyEffects(const TArray<FVHVStoryEffect>& Effects);

    UFUNCTION(BlueprintPure, Category = "VHV|Story State|Debug")
    TArray<FName> GetSetFlagIDs() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Story State|Debug")
    TMap<FName, int32> GetCounterValues() const;

    void ExportSaveState(FVHVStoryStateSaveState& OutSaveState) const;
    bool ValidateSaveState(const FVHVStoryStateSaveState& SaveState) const;
    bool ImportSaveState(const FVHVStoryStateSaveState& SaveState, bool bBroadcastChanges = false);

    UPROPERTY(BlueprintAssignable, Category = "VHV|Story State|Events")
    FOnVHVStoryFlagChanged OnStoryFlagChanged;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Story State|Events")
    FOnVHVStoryCounterChanged OnStoryCounterChanged;

private:
    UPROPERTY(Transient)
    TSet<FName> SetFlags;

    UPROPERTY(Transient)
    TMap<FName, int32> Counters;
};
