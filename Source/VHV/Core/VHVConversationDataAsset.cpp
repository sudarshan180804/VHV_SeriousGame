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
        if (!VHVAuthoringReferences::IsValidReferenceTag(Node.SpeakerTag, TEXT("VHV.Participant")))
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s' uses speaker tag '%s', which must be a concrete tag beneath VHV.Participant."), *ConversationLabel, *Node.NodeID, *Node.SpeakerTag.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        if (VHVAuthoringReferences::HasConflict(Node.SpeakerTag, Node.SpeakerID, TEXT("VHV.Participant")))
        {
            Context.AddWarning(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s' has speaker tag '%s', which resolves to '%s', while legacy SpeakerID is '%s'; the tag wins."), *ConversationLabel, *Node.NodeID, *Node.SpeakerTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Node.SpeakerTag).ToString(), *Node.SpeakerID)));
        }
        if (Node.bIsCheckpoint && !VHVAuthoringReferences::IsValidReferenceTag(Node.CheckpointTag, TEXT("VHV.Checkpoint")))
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s' uses checkpoint tag '%s', which must be a concrete tag beneath VHV.Checkpoint."), *ConversationLabel, *Node.NodeID, *Node.CheckpointTag.ToString())));
            Result = EDataValidationResult::Invalid;
        }
        if (Node.bIsCheckpoint && VHVAuthoringReferences::HasConflict(Node.CheckpointTag, Node.CheckpointID, TEXT("VHV.Checkpoint")))
        {
            Context.AddWarning(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s' has checkpoint tag '%s', which resolves to '%s', while legacy CheckpointID is '%s'; the tag wins."), *ConversationLabel, *Node.NodeID, *Node.CheckpointTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Node.CheckpointTag).ToString(), *Node.CheckpointID)));
        }
        if (Node.bIsCheckpoint && Node.GetEffectiveCheckpointID().IsEmpty())
        {
            Context.AddError(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s' is a checkpoint but has no effective Checkpoint ID."), *ConversationLabel, *Node.NodeID)));
            Result = EDataValidationResult::Invalid;
        }
        if (Node.NodeType == EVHVDialogueNodeType::LearningActivity)
        {
            if (!VHVAuthoringReferences::IsValidReferenceTag(Node.LinkedActivity.ActivityTag, TEXT("VHV.Activity")))
            {
                Context.AddError(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s' uses linked activity tag '%s', which must be a concrete tag beneath VHV.Activity."), *ConversationLabel, *Node.NodeID, *Node.LinkedActivity.ActivityTag.ToString())));
                Result = EDataValidationResult::Invalid;
            }
            if (VHVAuthoringReferences::HasConflict(Node.LinkedActivity.ActivityTag, Node.LinkedActivity.ActivityID, TEXT("VHV.Activity")))
            {
                Context.AddWarning(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s' has linked activity tag '%s', which resolves to '%s', while legacy ActivityID is '%s'; the tag wins."), *ConversationLabel, *Node.NodeID, *Node.LinkedActivity.ActivityTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Node.LinkedActivity.ActivityTag).ToString(), *Node.LinkedActivity.ActivityID)));
            }
            if (Node.LinkedActivity.GetEffectiveActivityID().IsEmpty())
            {
                Context.AddError(FText::FromString(FString::Printf(TEXT("Conversation '%s', learning node '%s' has no effective linked Activity ID."), *ConversationLabel, *Node.NodeID)));
                Result = EDataValidationResult::Invalid;
            }
        }
        for (int32 ConditionIndex = 0; ConditionIndex < Node.ActivationConditions.Conditions.Num(); ++ConditionIndex)
        {
            const FVHVStoryCondition& Condition = Node.ActivationConditions.Conditions[ConditionIndex];
            if (Condition.GetEffectiveStateID().IsNone())
            {
                Context.AddError(FText::FromString(FString::Printf(
                    TEXT("Conversation '%s', node '%s' has an invalid ActivationConditions condition at index %d: StateID must not be NAME_None."),
                    *ConversationLabel, *Node.NodeID, ConditionIndex)));
                Result = EDataValidationResult::Invalid;
            }
            const bool bFlag = Condition.ConditionType == EVHVStoryConditionType::FlagSet || Condition.ConditionType == EVHVStoryConditionType::FlagNotSet;
            const TCHAR* ExpectedCategory = bFlag ? TEXT("VHV.Story.Flag") : TEXT("VHV.Story.Counter");
            if (!VHVAuthoringReferences::IsValidReferenceTag(Condition.StateTag, ExpectedCategory))
            {
                Context.AddError(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s' ActivationConditions condition at index %d uses tag '%s', which must be a concrete tag beneath %s."), *ConversationLabel, *Node.NodeID, ConditionIndex, *Condition.StateTag.ToString(), ExpectedCategory)));
                Result = EDataValidationResult::Invalid;
            }
            if (VHVAuthoringReferences::HasConflict(Condition.StateTag, Condition.StateID, ExpectedCategory))
            {
                Context.AddWarning(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s' ActivationConditions condition at index %d has tag '%s', which resolves to '%s', while legacy StateID is '%s'; the tag wins."), *ConversationLabel, *Node.NodeID, ConditionIndex, *Condition.StateTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Condition.StateTag).ToString(), *Condition.StateID.ToString())));
            }
        }

        for (int32 EffectIndex = 0; EffectIndex < Node.CompletionEffects.Num(); ++EffectIndex)
        {
            const FVHVStoryEffect& Effect = Node.CompletionEffects[EffectIndex];
            if (Effect.GetEffectiveStateID().IsNone())
            {
                Context.AddError(FText::FromString(FString::Printf(
                    TEXT("Conversation '%s', node '%s' has an invalid CompletionEffects effect at index %d: StateID must not be NAME_None."),
                    *ConversationLabel, *Node.NodeID, EffectIndex)));
                Result = EDataValidationResult::Invalid;
            }
            const bool bFlag = Effect.EffectType == EVHVStoryEffectType::SetFlag || Effect.EffectType == EVHVStoryEffectType::ClearFlag;
            const TCHAR* ExpectedCategory = bFlag ? TEXT("VHV.Story.Flag") : TEXT("VHV.Story.Counter");
            if (!VHVAuthoringReferences::IsValidReferenceTag(Effect.StateTag, ExpectedCategory))
            {
                Context.AddError(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s' CompletionEffects effect at index %d uses tag '%s', which must be a concrete tag beneath %s."), *ConversationLabel, *Node.NodeID, EffectIndex, *Effect.StateTag.ToString(), ExpectedCategory)));
                Result = EDataValidationResult::Invalid;
            }
            if (VHVAuthoringReferences::HasConflict(Effect.StateTag, Effect.StateID, ExpectedCategory))
            {
                Context.AddWarning(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s' CompletionEffects effect at index %d has tag '%s', which resolves to '%s', while legacy StateID is '%s'; the tag wins."), *ConversationLabel, *Node.NodeID, EffectIndex, *Effect.StateTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Effect.StateTag).ToString(), *Effect.StateID.ToString())));
            }
        }

        for (const FDialogueChoiceOption& Choice : Node.Choices)
        {
            for (int32 ConditionIndex = 0; ConditionIndex < Choice.AvailabilityConditions.Conditions.Num(); ++ConditionIndex)
            {
                const FVHVStoryCondition& Condition = Choice.AvailabilityConditions.Conditions[ConditionIndex];
                if (Condition.GetEffectiveStateID().IsNone())
                {
                    Context.AddError(FText::FromString(FString::Printf(
                        TEXT("Conversation '%s', node '%s', choice '%s' has an invalid AvailabilityConditions condition at index %d: StateID must not be NAME_None."),
                        *ConversationLabel, *Node.NodeID, *Choice.OptionID, ConditionIndex)));
                    Result = EDataValidationResult::Invalid;
                }
                const bool bFlag = Condition.ConditionType == EVHVStoryConditionType::FlagSet || Condition.ConditionType == EVHVStoryConditionType::FlagNotSet;
                const TCHAR* ExpectedCategory = bFlag ? TEXT("VHV.Story.Flag") : TEXT("VHV.Story.Counter");
                if (!VHVAuthoringReferences::IsValidReferenceTag(Condition.StateTag, ExpectedCategory))
                {
                    Context.AddError(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s', choice '%s' AvailabilityConditions condition at index %d uses tag '%s', which must be a concrete tag beneath %s."), *ConversationLabel, *Node.NodeID, *Choice.OptionID, ConditionIndex, *Condition.StateTag.ToString(), ExpectedCategory)));
                    Result = EDataValidationResult::Invalid;
                }
                if (VHVAuthoringReferences::HasConflict(Condition.StateTag, Condition.StateID, ExpectedCategory))
                {
                    Context.AddWarning(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s', choice '%s' AvailabilityConditions condition at index %d has tag '%s', which resolves to '%s', while legacy StateID is '%s'; the tag wins."), *ConversationLabel, *Node.NodeID, *Choice.OptionID, ConditionIndex, *Condition.StateTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Condition.StateTag).ToString(), *Condition.StateID.ToString())));
                }
            }

            for (int32 EffectIndex = 0; EffectIndex < Choice.SelectionEffects.Num(); ++EffectIndex)
            {
                const FVHVStoryEffect& Effect = Choice.SelectionEffects[EffectIndex];
                if (Effect.GetEffectiveStateID().IsNone())
                {
                    Context.AddError(FText::FromString(FString::Printf(
                        TEXT("Conversation '%s', node '%s', choice '%s' has an invalid SelectionEffects effect at index %d: StateID must not be NAME_None."),
                        *ConversationLabel, *Node.NodeID, *Choice.OptionID, EffectIndex)));
                    Result = EDataValidationResult::Invalid;
                }
                const bool bFlag = Effect.EffectType == EVHVStoryEffectType::SetFlag || Effect.EffectType == EVHVStoryEffectType::ClearFlag;
                const TCHAR* ExpectedCategory = bFlag ? TEXT("VHV.Story.Flag") : TEXT("VHV.Story.Counter");
                if (!VHVAuthoringReferences::IsValidReferenceTag(Effect.StateTag, ExpectedCategory))
                {
                    Context.AddError(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s', choice '%s' SelectionEffects effect at index %d uses tag '%s', which must be a concrete tag beneath %s."), *ConversationLabel, *Node.NodeID, *Choice.OptionID, EffectIndex, *Effect.StateTag.ToString(), ExpectedCategory)));
                    Result = EDataValidationResult::Invalid;
                }
                if (VHVAuthoringReferences::HasConflict(Effect.StateTag, Effect.StateID, ExpectedCategory))
                {
                    Context.AddWarning(FText::FromString(FString::Printf(TEXT("Conversation '%s', node '%s', choice '%s' SelectionEffects effect at index %d has tag '%s', which resolves to '%s', while legacy StateID is '%s'; the tag wins."), *ConversationLabel, *Node.NodeID, *Choice.OptionID, EffectIndex, *Effect.StateTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Effect.StateTag).ToString(), *Effect.StateID.ToString())));
                }
            }
        }
    }

    return Result;
}
#endif
