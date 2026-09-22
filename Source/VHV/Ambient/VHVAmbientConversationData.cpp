#include "Ambient/VHVAmbientConversationData.h"
#include "Core/VHVAuthoringReferences.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UVHVAmbientConversationData::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("VHVAmbientConversation"), ConversationID.IsNone() ? GetFName() : ConversationID);
}

const FVHVAmbientParticipantDefinition* UVHVAmbientConversationData::FindParticipant(const FName SlotID) const
{
    return Participants.FindByPredicate([SlotID](const FVHVAmbientParticipantDefinition& Participant)
    {
        return Participant.SlotID == SlotID;
    });
}

#if WITH_EDITOR
EDataValidationResult UVHVAmbientConversationData::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    TSet<FName> Slots;
    if (ConversationID.IsNone())
    {
        Context.AddError(FText::FromString(TEXT("Ambient conversation requires a Conversation ID.")));
        Result = EDataValidationResult::Invalid;
    }
    for (const FVHVAmbientParticipantDefinition& Participant : Participants)
    {
        if (Participant.SlotID.IsNone() || Slots.Contains(Participant.SlotID))
        {
            Context.AddError(FText::FromString(TEXT("Ambient participant slots must be non-empty and unique.")));
            Result = EDataValidationResult::Invalid;
        }
        Slots.Add(Participant.SlotID);
    }
    if (Participants.IsEmpty() || Lines.IsEmpty())
    {
        Context.AddError(FText::FromString(TEXT("Ambient conversation requires at least one participant and one line.")));
        Result = EDataValidationResult::Invalid;
    }
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    {
        const FVHVAmbientSpeechLine& Line = Lines[Index];
        if (!Slots.Contains(Line.SpeakerSlot) || Line.Text.IsEmpty())
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Ambient line %d requires text and a Speaker Slot declared in Participants."), Index)));
            Result = EDataValidationResult::Invalid;
        }
        if (Line.Text.ToString().Len() > 220)
        {
            Context.AddWarning(FText::FromString(FString::Printf(
                TEXT("Ambient line %d is longer than 220 characters and may exceed the intended 3-4 line bubble."), Index)));
        }
        if (Line.NPCActionTag.IsValid()
            && !VHVAuthoringReferences::IsConcreteTagInCategory(Line.NPCActionTag, TEXT("VHV.NPCAction")))
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Ambient line %d NPC Action must be beneath VHV.NPCAction."), Index)));
            Result = EDataValidationResult::Invalid;
        }
    }
    for (int32 Index = 0; Index < CompletionEffects.Num(); ++Index)
    {
        const FVHVStoryEffect& Effect = CompletionEffects[Index];
        const bool bFlag = Effect.EffectType == EVHVStoryEffectType::SetFlag
            || Effect.EffectType == EVHVStoryEffectType::ClearFlag;
        const TCHAR* Category = bFlag ? TEXT("VHV.Story.Flag") : TEXT("VHV.Story.Counter");
        if (!VHVAuthoringReferences::IsValidReferenceTag(Effect.StateTag, Category))
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Completion effect %d requires a concrete tag beneath %s."), Index, Category)));
            Result = EDataValidationResult::Invalid;
        }
    }
    return Result;
}
#endif
