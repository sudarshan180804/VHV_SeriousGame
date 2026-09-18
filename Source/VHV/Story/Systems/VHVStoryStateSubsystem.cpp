#include "Story/Systems/VHVStoryStateSubsystem.h"

#include "Save/VHVSaveTypes.h"
#include "VHV.h"

bool UVHVStoryStateSubsystem::HasFlag(const FName FlagID) const
{
    if (FlagID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVStoryState] HasFlag rejected an empty Flag ID."));
        return false;
    }
    return SetFlags.Contains(FlagID);
}

void UVHVStoryStateSubsystem::SetFlag(const FName FlagID)
{
    if (FlagID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVStoryState] SetFlag rejected an empty Flag ID."));
        return;
    }
    if (!SetFlags.Contains(FlagID))
    {
        SetFlags.Add(FlagID);
        OnStoryFlagChanged.Broadcast(FlagID, true);
    }
}

void UVHVStoryStateSubsystem::ClearFlag(const FName FlagID)
{
    if (FlagID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVStoryState] ClearFlag rejected an empty Flag ID."));
        return;
    }
    if (SetFlags.Remove(FlagID) > 0)
    {
        OnStoryFlagChanged.Broadcast(FlagID, false);
    }
}

int32 UVHVStoryStateSubsystem::GetCounter(const FName CounterID) const
{
    if (CounterID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVStoryState] GetCounter rejected an empty Counter ID."));
        return 0;
    }
    return Counters.FindRef(CounterID);
}

void UVHVStoryStateSubsystem::SetCounter(const FName CounterID, const int32 Value)
{
    if (CounterID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVStoryState] SetCounter rejected an empty Counter ID."));
        return;
    }

    const int32 PreviousValue = Counters.FindRef(CounterID);
    if (PreviousValue == Value)
    {
        return;
    }
    if (Value == 0)
    {
        Counters.Remove(CounterID);
    }
    else
    {
        Counters.Add(CounterID, Value);
    }
    OnStoryCounterChanged.Broadcast(CounterID, Value);
}

int32 UVHVStoryStateSubsystem::AddCounter(const FName CounterID, const int32 Delta)
{
    if (CounterID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVStoryState] AddCounter rejected an empty Counter ID."));
        return 0;
    }

    const int64 Sum = static_cast<int64>(Counters.FindRef(CounterID)) + static_cast<int64>(Delta);
    const int32 NewValue = static_cast<int32>(FMath::Clamp<int64>(Sum, MIN_int32, MAX_int32));
    SetCounter(CounterID, NewValue);
    return NewValue;
}

void UVHVStoryStateSubsystem::ResetStoryState()
{
    TArray<FName> FlagsToClear = SetFlags.Array();
    TMap<FName, int32> CountersToClear = Counters;
    SetFlags.Empty();
    Counters.Empty();

    for (const FName FlagID : FlagsToClear)
    {
        OnStoryFlagChanged.Broadcast(FlagID, false);
    }
    for (const TPair<FName, int32>& Counter : CountersToClear)
    {
        OnStoryCounterChanged.Broadcast(Counter.Key, 0);
    }
}

bool UVHVStoryStateSubsystem::EvaluateCondition(const FVHVStoryCondition& Condition) const
{
    if (Condition.StateID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVStoryState] Cannot evaluate condition '%s' with an empty State ID."),
            *UEnum::GetValueAsString(Condition.ConditionType));
        return false;
    }

    switch (Condition.ConditionType)
    {
    case EVHVStoryConditionType::FlagSet:
        return HasFlag(Condition.StateID);
    case EVHVStoryConditionType::FlagNotSet:
        return !HasFlag(Condition.StateID);
    case EVHVStoryConditionType::CounterEqual:
        return GetCounter(Condition.StateID) == Condition.CompareValue;
    case EVHVStoryConditionType::CounterGreaterOrEqual:
        return GetCounter(Condition.StateID) >= Condition.CompareValue;
    case EVHVStoryConditionType::CounterLessOrEqual:
        return GetCounter(Condition.StateID) <= Condition.CompareValue;
    default:
        UE_LOG(LogVHV, Warning, TEXT("[VHVStoryState] Cannot evaluate condition with unsupported type value %d."),
            static_cast<uint8>(Condition.ConditionType));
        return false;
    }
}

bool UVHVStoryStateSubsystem::EvaluateConditionSet(const FVHVStoryConditionSet& ConditionSet) const
{
    if (ConditionSet.Conditions.IsEmpty())
    {
        return true;
    }

    if (ConditionSet.MatchMode == EVHVStoryConditionMatch::All)
    {
        for (const FVHVStoryCondition& Condition : ConditionSet.Conditions)
        {
            if (!EvaluateCondition(Condition))
            {
                return false;
            }
        }
        return true;
    }

    if (ConditionSet.MatchMode == EVHVStoryConditionMatch::Any)
    {
        for (const FVHVStoryCondition& Condition : ConditionSet.Conditions)
        {
            if (EvaluateCondition(Condition))
            {
                return true;
            }
        }
        return false;
    }

    UE_LOG(LogVHV, Warning, TEXT("[VHVStoryState] Cannot evaluate condition set with unsupported match mode value %d."),
        static_cast<uint8>(ConditionSet.MatchMode));
    return false;
}

bool UVHVStoryStateSubsystem::ApplyEffect(const FVHVStoryEffect& Effect)
{
    if (Effect.StateID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVStoryState] Cannot apply effect '%s' with an empty State ID."),
            *UEnum::GetValueAsString(Effect.EffectType));
        return false;
    }

    switch (Effect.EffectType)
    {
    case EVHVStoryEffectType::SetFlag:
        SetFlag(Effect.StateID);
        return true;
    case EVHVStoryEffectType::ClearFlag:
        ClearFlag(Effect.StateID);
        return true;
    case EVHVStoryEffectType::SetCounter:
        SetCounter(Effect.StateID, Effect.Value);
        return true;
    case EVHVStoryEffectType::AddCounter:
        AddCounter(Effect.StateID, Effect.Value);
        return true;
    default:
        UE_LOG(LogVHV, Warning, TEXT("[VHVStoryState] Cannot apply effect with unsupported type value %d."),
            static_cast<uint8>(Effect.EffectType));
        return false;
    }
}

bool UVHVStoryStateSubsystem::ApplyEffects(const TArray<FVHVStoryEffect>& Effects)
{
    bool bAllEffectsApplied = true;
    for (const FVHVStoryEffect& Effect : Effects)
    {
        if (!ApplyEffect(Effect))
        {
            bAllEffectsApplied = false;
        }
    }
    return bAllEffectsApplied;
}

TArray<FName> UVHVStoryStateSubsystem::GetSetFlagIDs() const
{
    TArray<FName> FlagIDs = SetFlags.Array();
    FlagIDs.Sort(FNameLexicalLess());
    return FlagIDs;
}

TMap<FName, int32> UVHVStoryStateSubsystem::GetCounterValues() const
{
    return Counters;
}

void UVHVStoryStateSubsystem::ExportSaveState(FVHVStoryStateSaveState& OutSaveState) const
{
    OutSaveState = FVHVStoryStateSaveState();
    OutSaveState.SetFlagIDs = SetFlags.Array();
    OutSaveState.SetFlagIDs.Sort(FNameLexicalLess());
    OutSaveState.CounterValues = Counters;
}

bool UVHVStoryStateSubsystem::ValidateSaveState(const FVHVStoryStateSaveState& SaveState) const
{
    for (const FName FlagID : SaveState.SetFlagIDs)
    {
        if (FlagID.IsNone())
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVStoryState] Saved Story State contains an empty Flag ID."));
            return false;
        }
    }
    for (const TPair<FName, int32>& Counter : SaveState.CounterValues)
    {
        if (Counter.Key.IsNone())
        {
            UE_LOG(LogVHV, Error, TEXT("[VHVStoryState] Saved Story State contains an empty Counter ID."));
            return false;
        }
    }
    return true;
}

bool UVHVStoryStateSubsystem::ImportSaveState(const FVHVStoryStateSaveState& SaveState, const bool bBroadcastChanges)
{
    if (!ValidateSaveState(SaveState))
    {
        return false;
    }

    const TSet<FName> PreviousFlags = SetFlags;
    const TMap<FName, int32> PreviousCounters = Counters;
    SetFlags.Reset();
    for (const FName FlagID : SaveState.SetFlagIDs)
    {
        SetFlags.Add(FlagID);
    }
    Counters = SaveState.CounterValues;

    for (auto It = Counters.CreateIterator(); It; ++It)
    {
        if (It.Value() == 0)
        {
            It.RemoveCurrent();
        }
    }

    if (bBroadcastChanges)
    {
        TSet<FName> AllFlagIDs = PreviousFlags;
        AllFlagIDs.Append(SetFlags);
        for (const FName FlagID : AllFlagIDs)
        {
            const bool bWasSet = PreviousFlags.Contains(FlagID);
            const bool bIsSet = SetFlags.Contains(FlagID);
            if (bWasSet != bIsSet)
            {
                OnStoryFlagChanged.Broadcast(FlagID, bIsSet);
            }
        }

        TSet<FName> AllCounterIDs;
        for (const TPair<FName, int32>& Counter : PreviousCounters)
        {
            AllCounterIDs.Add(Counter.Key);
        }
        for (const TPair<FName, int32>& Counter : Counters)
        {
            AllCounterIDs.Add(Counter.Key);
        }
        for (const FName CounterID : AllCounterIDs)
        {
            const int32 PreviousValue = PreviousCounters.FindRef(CounterID);
            const int32 NewValue = Counters.FindRef(CounterID);
            if (PreviousValue != NewValue)
            {
                OnStoryCounterChanged.Broadcast(CounterID, NewValue);
            }
        }
    }

    return true;
}
