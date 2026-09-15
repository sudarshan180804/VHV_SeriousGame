#include "Quest/Components/VHVQuestParticipantComponent.h"

#include "Engine/GameInstance.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "VHV.h"

UVHVQuestParticipantComponent::UVHVQuestParticipantComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UVHVQuestParticipantComponent::NotifyInteracted()
{
    if (ParticipantID.IsNone() || !GetWorld() || !GetWorld()->GetGameInstance())
    {
        if (ParticipantID.IsNone())
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVQuest] Quest participant on '%s' has an empty Participant ID."), *GetNameSafe(GetOwner()));
        }
        return false;
    }

    UVHVQuestSubsystem* QuestSubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UVHVQuestSubsystem>();
    return QuestSubsystem && QuestSubsystem->NotifyParticipantInteracted(ParticipantID);
}
