#include "NPC/Presentation/VHVNPCActionSet.h"

#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UVHVNPCActionSet::GetAction(const FName ActionID, FVHVNPCPresentationAction& OutAction) const
{
    const FVHVNPCPresentationAction* Action = Actions.FindByPredicate([ActionID](const FVHVNPCPresentationAction& Candidate)
    {
        return Candidate.GetEffectiveActionID() == ActionID;
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
        const FName EffectiveActionID = Action.GetEffectiveActionID();
        if (EffectiveActionID.IsNone())
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("NPC Action Set '%s' has an invalid ActionID at index %d."), *GetName(), Index)));
            Result = EDataValidationResult::Invalid;
        }
        else if (ActionIDs.Contains(EffectiveActionID))
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("NPC Action Set '%s' has duplicate ActionID '%s'."), *GetName(), *EffectiveActionID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        ActionIDs.Add(EffectiveActionID);

        if (!VHVAuthoringReferences::IsValidReferenceTag(Action.ActionTag, TEXT("VHV.NPCAction")))
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("NPC Action Set '%s' action at index %d uses tag '%s', which must be a concrete tag beneath VHV.NPCAction."), *GetName(), Index, *Action.ActionTag.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        if (VHVAuthoringReferences::HasConflict(Action.ActionTag, Action.ActionID, TEXT("VHV.NPCAction")))
        {
            Context.AddWarning(FText::FromString(FString::Printf(TEXT("NPC Action Set '%s' action at index %d has tag '%s', which resolves to '%s', while legacy ActionID is '%s'; the tag wins."), *GetName(), Index, *Action.ActionTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Action.ActionTag).ToString(), *Action.ActionID.ToString())));
        }

        if (!Action.Montage)
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("NPC Action Set '%s' action '%s' has no Montage."), *GetName(), *EffectiveActionID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        if (Action.PlayRate <= 0.0f)
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("NPC Action Set '%s' action '%s' must have a positive PlayRate."), *GetName(), *EffectiveActionID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
    }
    return Result;
}
#endif
