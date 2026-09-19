#pragma once

#include "CoreMinimal.h"
#include "NPC/Types/VHVNPCBehaviorTypes.h"
#include "Story/Types/VHVStoryStateTypes.h"
#include "VHVQuestTypes.generated.h"

class UVHVConversationDataAsset;

UENUM(BlueprintType)
enum class EVHVQuestStatus : uint8
{
    Locked,
    NotStarted,
    Active,
    Completed,
    Failed
};

UENUM(BlueprintType)
enum class EVHVQuestCategory : uint8
{
    MainStory,
    Side
};

UENUM(BlueprintType)
enum class EVHVQuestObjectiveType : uint8
{
    Talk,
    Conversation,
    LearningActivity,
    Interact,
    ReachLocation,
    CustomEvent,
    NPCAction,
    WorldAction
};

USTRUCT(BlueprintType)
struct VHV_API FVHVQuestObjectiveDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    FName ObjectiveID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    FText ObjectiveText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
    EVHVQuestObjectiveType ObjectiveType = EVHVQuestObjectiveType::Interact;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|References", meta = (Categories = "VHV.Participant", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::Talk || ObjectiveType == EVHVQuestObjectiveType::Conversation || ObjectiveType == EVHVQuestObjectiveType::LearningActivity || ObjectiveType == EVHVQuestObjectiveType::Interact", EditConditionHides))
    FGameplayTag ParticipantTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|References", meta = (Categories = "VHV.Location", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::ReachLocation", EditConditionHides))
    FGameplayTag LocationTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|References", meta = (Categories = "VHV.CustomEvent", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::CustomEvent", EditConditionHides))
    FGameplayTag CustomEventTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|References", meta = (AdvancedDisplay, DisplayName = "Target ID (Legacy)", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::Talk || ObjectiveType == EVHVQuestObjectiveType::Conversation || ObjectiveType == EVHVQuestObjectiveType::LearningActivity || ObjectiveType == EVHVQuestObjectiveType::Interact || ObjectiveType == EVHVQuestObjectiveType::ReachLocation || ObjectiveType == EVHVQuestObjectiveType::CustomEvent", EditConditionHides))
    FName TargetID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|References", meta = (Categories = "VHV.Activity", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::LearningActivity", EditConditionHides))
    FGameplayTag ActivityTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|References", meta = (AdvancedDisplay, DisplayName = "Activity ID (Legacy)", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::LearningActivity", EditConditionHides))
    FName ActivityID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "ObjectiveType == EVHVQuestObjectiveType::Talk || ObjectiveType == EVHVQuestObjectiveType::Conversation", EditConditionHides))
    TSoftObjectPtr<UVHVConversationDataAsset> Conversation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "ObjectiveType == EVHVQuestObjectiveType::Talk || ObjectiveType == EVHVQuestObjectiveType::Conversation", EditConditionHides))
    FName EntryNodeID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "ObjectiveType == EVHVQuestObjectiveType::Conversation || ObjectiveType == EVHVQuestObjectiveType::LearningActivity", EditConditionHides))
    bool bAutoStart = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (Categories = "VHV.Participant", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction", EditConditionHides))
    FGameplayTag NPCParticipantTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (AdvancedDisplay, DisplayName = "NPC Participant ID (Legacy)", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction", EditConditionHides))
    FName NPCParticipantID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction", EditConditionHides))
    EVHVNPCQuestCommandType NPCCommandType = EVHVNPCQuestCommandType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (Categories = "VHV.BehaviorTarget", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction && NPCCommandType == EVHVNPCQuestCommandType::MoveToTarget", EditConditionHides))
    FGameplayTag NPCBehaviorTargetTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (AdvancedDisplay, DisplayName = "NPC Target ID (Legacy)", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction && NPCCommandType == EVHVNPCQuestCommandType::MoveToTarget", EditConditionHides))
    FName NPCTargetID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction && NPCCommandType == EVHVNPCQuestCommandType::Wait", EditConditionHides, ClampMin = "0.0"))
    float NPCWaitDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (Categories = "VHV.NPCAction", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction && NPCCommandType == EVHVNPCQuestCommandType::PlayAction", EditConditionHides))
    FGameplayTag NPCActionTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (AdvancedDisplay, DisplayName = "NPC Action ID (Legacy)", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction && NPCCommandType == EVHVNPCQuestCommandType::PlayAction", EditConditionHides))
    FName NPCActionID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|World Action", meta = (Categories = "VHV.WorldReceiver", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::WorldAction", EditConditionHides))
    FGameplayTag WorldActionReceiverTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|World Action", meta = (AdvancedDisplay, DisplayName = "World Action Receiver ID (Legacy)", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::WorldAction", EditConditionHides))
    FName WorldActionReceiverID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|World Action", meta = (Categories = "VHV.WorldAction", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::WorldAction", EditConditionHides))
    FGameplayTag WorldActionTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|World Action", meta = (AdvancedDisplay, DisplayName = "World Action ID (Legacy)", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::WorldAction", EditConditionHides))
    FName WorldActionID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Story State")
    FVHVStoryConditionSet ActivationConditions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Story State")
    TArray<FVHVStoryEffect> CompletionEffects;

    FName GetEffectiveTargetID() const
    {
        if (ObjectiveType == EVHVQuestObjectiveType::ReachLocation)
        {
            return VHVAuthoringReferences::ResolveID(LocationTag, TargetID, TEXT("VHV.Location"));
        }
        if (ObjectiveType == EVHVQuestObjectiveType::CustomEvent)
        {
            return VHVAuthoringReferences::ResolveID(CustomEventTag, TargetID, TEXT("VHV.CustomEvent"));
        }
        return VHVAuthoringReferences::ResolveID(ParticipantTag, TargetID, TEXT("VHV.Participant"));
    }

    FName GetEffectiveActivityID() const { return VHVAuthoringReferences::ResolveID(ActivityTag, ActivityID, TEXT("VHV.Activity")); }
    FName GetEffectiveNPCParticipantID() const { return VHVAuthoringReferences::ResolveID(NPCParticipantTag, NPCParticipantID, TEXT("VHV.Participant")); }
    FName GetEffectiveNPCTargetID() const { return VHVAuthoringReferences::ResolveID(NPCBehaviorTargetTag, NPCTargetID, TEXT("VHV.BehaviorTarget")); }
    FName GetEffectiveNPCActionID() const { return VHVAuthoringReferences::ResolveID(NPCActionTag, NPCActionID, TEXT("VHV.NPCAction")); }
    FName GetEffectiveWorldActionReceiverID() const { return VHVAuthoringReferences::ResolveID(WorldActionReceiverTag, WorldActionReceiverID, TEXT("VHV.WorldReceiver")); }
    FName GetEffectiveWorldActionID() const { return VHVAuthoringReferences::ResolveID(WorldActionTag, WorldActionID, TEXT("VHV.WorldAction")); }
};

USTRUCT(BlueprintType)
struct VHV_API FVHVQuestDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
    FName QuestID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
    FText QuestTitle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
    FText QuestDescription;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
    EVHVQuestCategory Category = EVHVQuestCategory::MainStory;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
    TArray<FVHVQuestObjectiveDefinition> Objectives;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
    bool bAutoTrack = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
    bool bAutoStartNextQuest = true;
};

USTRUCT(BlueprintType)
struct VHV_API FVHVQuestRuntimeState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Runtime")
    FName QuestID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Runtime")
    EVHVQuestStatus Status = EVHVQuestStatus::Locked;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Runtime")
    int32 CurrentObjectiveIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Runtime")
    TSet<FName> CompletedObjectiveIDs;
};

USTRUCT(BlueprintType)
struct VHV_API FVHVQuestArcRuntimeState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Runtime")
    FName QuestArcID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Runtime")
    FName ActiveQuestID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Runtime")
    FName TrackedQuestID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Runtime")
    TArray<FVHVQuestRuntimeState> QuestStates;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Runtime")
    TSet<FName> CompletedQuestIDs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Runtime")
    bool bArcActive = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Runtime")
    bool bArcCompleted = false;
};

USTRUCT(BlueprintType)
struct VHV_API FVHVQuestJournalEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Quest Journal")
    FName QuestID;

    UPROPERTY(BlueprintReadOnly, Category = "Quest Journal")
    FText QuestTitle;

    UPROPERTY(BlueprintReadOnly, Category = "Quest Journal")
    FText QuestDescription;

    UPROPERTY(BlueprintReadOnly, Category = "Quest Journal")
    EVHVQuestStatus Status = EVHVQuestStatus::Locked;

    UPROPERTY(BlueprintReadOnly, Category = "Quest Journal")
    EVHVQuestCategory Category = EVHVQuestCategory::MainStory;

    UPROPERTY(BlueprintReadOnly, Category = "Quest Journal")
    FText CurrentObjectiveText;

    UPROPERTY(BlueprintReadOnly, Category = "Quest Journal")
    int32 CurrentObjectiveIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Quest Journal")
    int32 TotalObjectiveCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Quest Journal")
    bool bTracked = false;
};
