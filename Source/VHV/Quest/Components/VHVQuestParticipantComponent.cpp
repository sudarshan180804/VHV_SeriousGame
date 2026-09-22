#include "Quest/Components/VHVQuestParticipantComponent.h"

#include "Engine/GameInstance.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "VHV.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UVHVQuestParticipantComponent::UVHVQuestParticipantComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UVHVQuestParticipantComponent::NotifyInteracted()
{
    if (!IsQuestParticipationEnabled())
    {
        return false;
    }

    const FName EffectiveParticipantID = GetEffectiveParticipantID();
    if (EffectiveParticipantID.IsNone() || !GetWorld() || !GetWorld()->GetGameInstance())
    {
        if (EffectiveParticipantID.IsNone())
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest participant on '%s' has an empty Participant ID."), *GetNameSafe(GetOwner()));
        }
        return false;
    }

    UVHVQuestSubsystem* QuestSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>();
    return QuestSubsystem && QuestSubsystem->NotifyParticipantInteracted(EffectiveParticipantID);
}

#if WITH_EDITOR
EDataValidationResult UVHVQuestParticipantComponent::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    if (!IsQuestParticipationEnabled())
    {
        return Result;
    }

    if (GetEffectiveParticipantID().IsNone())
    {
        Context.AddError(FText::FromString(FString::Printf(TEXT("Quest participant on '%s' has no effective Participant ID."), *GetNameSafe(GetOwner()))));
        Result = EDataValidationResult::Invalid;
    }
    if (!VHVAuthoringReferences::IsValidReferenceTag(ParticipantTag, TEXT("VHV.Participant")))
    {
        Context.AddError(FText::FromString(FString::Printf(TEXT("Quest participant on '%s' uses tag '%s', which must be a concrete tag beneath VHV.Participant."), *GetNameSafe(GetOwner()), *ParticipantTag.ToString())));
        Result = EDataValidationResult::Invalid;
    }
    if (VHVAuthoringReferences::HasConflict(ParticipantTag, ParticipantID, TEXT("VHV.Participant")))
    {
        Context.AddWarning(FText::FromString(FString::Printf(TEXT("Quest participant on '%s' has tag '%s', which resolves to '%s', while legacy ParticipantID is '%s'; the tag wins."), *GetNameSafe(GetOwner()), *ParticipantTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(ParticipantTag).ToString(), *ParticipantID.ToString())));
    }
    return Result;
}
#endif
