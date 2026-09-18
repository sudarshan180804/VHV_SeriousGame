#include "Core/VHVConversationDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UVHVConversationDataAsset::UVHVConversationDataAsset()
{
}

FPrimaryAssetId UVHVConversationDataAsset::GetPrimaryAssetId() const
{
    const FPrimaryAssetType PrimaryAssetType = TEXT("VHVConversation");

    if (!Conversation.ConversationID.IsEmpty())
    {
        return FPrimaryAssetId(PrimaryAssetType, FName(*Conversation.ConversationID));
    }

    return FPrimaryAssetId(PrimaryAssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UVHVConversationDataAsset::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    const FString ConversationLabel = Conversation.ConversationID.IsEmpty() ? GetName() : Conversation.ConversationID;

    for (const FDialogueNode& Node : Conversation.Nodes)
    {
        for (int32 ConditionIndex = 0; ConditionIndex < Node.ActivationConditions.Conditions.Num(); ++ConditionIndex)
        {
            if (Node.ActivationConditions.Conditions[ConditionIndex].StateID.IsNone())
            {
                Context.AddError(FText::FromString(FString::Printf(
                    TEXT("Conversation '%s', node '%s' has an invalid ActivationConditions condition at index %d: StateID must not be NAME_None."),
                    *ConversationLabel, *Node.NodeID, ConditionIndex)));
                Result = EDataValidationResult::Invalid;
            }
        }

        for (int32 EffectIndex = 0; EffectIndex < Node.CompletionEffects.Num(); ++EffectIndex)
        {
            if (Node.CompletionEffects[EffectIndex].StateID.IsNone())
            {
                Context.AddError(FText::FromString(FString::Printf(
                    TEXT("Conversation '%s', node '%s' has an invalid CompletionEffects effect at index %d: StateID must not be NAME_None."),
                    *ConversationLabel, *Node.NodeID, EffectIndex)));
                Result = EDataValidationResult::Invalid;
            }
        }

        for (const FDialogueChoiceOption& Choice : Node.Choices)
        {
            for (int32 ConditionIndex = 0; ConditionIndex < Choice.AvailabilityConditions.Conditions.Num(); ++ConditionIndex)
            {
                if (Choice.AvailabilityConditions.Conditions[ConditionIndex].StateID.IsNone())
                {
                    Context.AddError(FText::FromString(FString::Printf(
                        TEXT("Conversation '%s', node '%s', choice '%s' has an invalid AvailabilityConditions condition at index %d: StateID must not be NAME_None."),
                        *ConversationLabel, *Node.NodeID, *Choice.OptionID, ConditionIndex)));
                    Result = EDataValidationResult::Invalid;
                }
            }

            for (int32 EffectIndex = 0; EffectIndex < Choice.SelectionEffects.Num(); ++EffectIndex)
            {
                if (Choice.SelectionEffects[EffectIndex].StateID.IsNone())
                {
                    Context.AddError(FText::FromString(FString::Printf(
                        TEXT("Conversation '%s', node '%s', choice '%s' has an invalid SelectionEffects effect at index %d: StateID must not be NAME_None."),
                        *ConversationLabel, *Node.NodeID, *Choice.OptionID, EffectIndex)));
                    Result = EDataValidationResult::Invalid;
                }
            }
        }
    }

    return Result;
}
#endif
