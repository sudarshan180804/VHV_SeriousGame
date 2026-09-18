#include "NPC/Presentation/VHVNPCActionSet.h"

#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UVHVNPCActionSet::GetAction(const FName ActionID, FVHVNPCPresentationAction& OutAction) const
{
    const FVHVNPCPresentationAction* Action = Actions.FindByPredicate([ActionID](const FVHVNPCPresentationAction& Candidate)
    {
        return Candidate.ActionID == ActionID;
    });
    if (!Action || ActionID.IsNone())
    {
        OutAction = FVHVNPCPresentationAction();
        return false;
    }

    OutAction = *Action;
    return true;
}

#if WITH_EDITOR
EDataValidationResult UVHVNPCActionSet::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    TSet<FName> ActionIDs;
    for (int32 Index = 0; Index < Actions.Num(); ++Index)
    {
        const FVHVNPCPresentationAction& Action = Actions[Index];
        if (Action.ActionID.IsNone())
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("NPC Action Set '%s' has an invalid ActionID at index %d."), *GetName(), Index)));
            Result = EDataValidationResult::Invalid;
        }
        else if (ActionIDs.Contains(Action.ActionID))
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("NPC Action Set '%s' has duplicate ActionID '%s'."), *GetName(), *Action.ActionID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        ActionIDs.Add(Action.ActionID);

        if (!Action.Montage)
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("NPC Action Set '%s' action '%s' has no Montage."), *GetName(), *Action.ActionID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        if (Action.PlayRate <= 0.0f)
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("NPC Action Set '%s' action '%s' must have a positive PlayRate."), *GetName(), *Action.ActionID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
    }
    return Result;
}
#endif
