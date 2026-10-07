#pragma once

#include "CoreMinimal.h"
#include "NPC/Types/VHVNPCBehaviorTypes.h"
#include "Story/Types/VHVStoryStateTypes.h"
#include "UI/VHVMajorQuestStingerTypes.h"
#include "VHVQuestTypes.generated.h"

class UVHVConversationDataAsset;
class AVHVNPCCharacter;

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

UENUM(BlueprintType)
enum class EVHVWorldActionStartPolicy : uint8
{
    /** Dispatch as soon as the objective activates. */
    Immediate,

    /** Wait for a matching authored world trigger to request dispatch. */
    ExplicitTrigger UMETA(DisplayName = "Explicit Trigger")
};

/** Controls player/UI presentation while an objective's staged NPC travel is running. */
UENUM(BlueprintType)
enum class EVHVQuestNPCTravelMode : uint8
{
    Standard UMETA(DisplayName = "Standard / Modal"),
    FreeRoam UMETA(DisplayName = "Free Roam / Walk Along")
};

/** A story-authored request that moves a persistent quest NPC to a semantic world location. */
USTRUCT(BlueprintType)
struct VHV_API FVHVQuestNPCMoveRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Move", meta = (DisplayName = "Participant", Categories = "VHV.Participant"))
    FGameplayTag ParticipantTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Move", meta = (DisplayName = "Destination", Categories = "VHV.Location"))
    FGameplayTag DestinationLocationTag;

    /** Apply the destination actor's authored yaw after the path-following move succeeds. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Move")
    bool bFaceDestinationRotation = false;

    /** Optional speed for this move. Values at or below zero preserve the NPC's authored walk speed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Move", meta = (ClampMin = "0.0", Units = "CentimetersPerSecond"))
    float MoveSpeedOverride = 0.0f;

    FName GetEffectiveParticipantID() const
    {
        return VHVAuthoringReferences::ResolveID(ParticipantTag, NAME_None, TEXT("VHV.Participant"));
    }

    FName GetEffectiveDestinationID() const
    {
        return VHVAuthoringReferences::ResolveID(DestinationLocationTag, NAME_None, TEXT("VHV.Location"));
    }
};

/** A group of NPC moves that must all finish before the next authored stage begins. */
USTRUCT(BlueprintType)
struct VHV_API FVHVQuestNPCMoveStage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Move")
    TArray<FVHVQuestNPCMoveRequest> Moves;

    /** Optional ambient-conversation receiver started non-modally after this stage's move requests are dispatched. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC Move", meta = (DisplayName = "Ambient Conversation Receiver", Categories = "VHV.WorldReceiver"))
    FGameplayTag AmbientConversationReceiverTag;
};

/** An independently available participant conversation inside a non-linear objective. */
USTRUCT(BlueprintType)
struct VHV_API FVHVQuestContextualConversation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contextual Conversation", meta = (Categories = "VHV.Participant"))
    FGameplayTag ParticipantTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contextual Conversation")
    TSoftObjectPtr<UVHVConversationDataAsset> Conversation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contextual Conversation")
    FName EntryNodeID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Contextual Conversation|Story State")
    FVHVStoryConditionSet AvailabilityConditions;

    FName GetEffectiveParticipantID() const
    {
        return VHVAuthoringReferences::ResolveID(ParticipantTag, NAME_None, TEXT("VHV.Participant"));
    }
};

/** An ordered semantic tracking destination that remains active until its story conditions pass. */
USTRUCT(BlueprintType)
struct VHV_API FVHVQuestTrackingDestination
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Location Tracking", meta = (Categories = "VHV.Location"))
    FGameplayTag DestinationLocationTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Location Tracking")
    FVHVStoryConditionSet CompletionConditions;

    FName GetEffectiveLocationID() const
    {
        return VHVAuthoringReferences::ResolveID(
            DestinationLocationTag, NAME_None, TEXT("VHV.Location"));
    }
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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|References", meta = (DisplayName = "Participant", Categories = "VHV.Participant", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::Talk || ObjectiveType == EVHVQuestObjectiveType::Conversation || ObjectiveType == EVHVQuestObjectiveType::LearningActivity || ObjectiveType == EVHVQuestObjectiveType::Interact || bEnsureParticipantPresentAtLocation", EditConditionHides))
    FGameplayTag ParticipantTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|References", meta = (DisplayName = "Location", Categories = "VHV.Location", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::ReachLocation || bEnsureParticipantPresentAtLocation", EditConditionHides))
    FGameplayTag LocationTag;

    /**
     * Before exposing this objective, ensure its required participant is physically
     * inside the authored semantic location volume. When ActivationNPCReadiness is
     * populated, every participant/location pair in that array is guaranteed instead.
     * Keep disabled for visible travel and movement-stage objectives.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Presence")
    bool bEnsureParticipantPresentAtLocation = false;

    /** Optional last-resort class used only after registry and one-time world recovery both fail. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Presence", meta = (EditCondition = "bEnsureParticipantPresentAtLocation", EditConditionHides))
    TSoftClassPtr<AVHVNPCCharacter> RequiredParticipantSpawnClass;

    /** Allows the player to locate this objective's stable world destination. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Location Tracking")
    bool bEnableLocationTracking = false;

    /** Optional destination override. LocationTag remains the default when this is unset. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Location Tracking", meta = (Categories = "VHV.Location", EditCondition = "bEnableLocationTracking", EditConditionHides))
    FGameplayTag TrackingLocationTag;

    /** Ordered tracking preference for objectives with independently completable world destinations. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Location Tracking", meta = (EditCondition = "bEnableLocationTracking", EditConditionHides))
    TArray<FVHVQuestTrackingDestination> TrackingDestinations;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|References", meta = (DisplayName = "Custom Event", Categories = "VHV.CustomEvent", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::CustomEvent", EditConditionHides))
    FGameplayTag CustomEventTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "Objective|References", meta = (DisplayName = "Target ID (Legacy)"))
    FName TargetID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|References", meta = (DisplayName = "Activity", Categories = "VHV.Activity", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::LearningActivity", EditConditionHides))
    FGameplayTag ActivityTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "Objective|References", meta = (DisplayName = "Activity ID (Legacy)"))
    FName ActivityID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "ObjectiveType == EVHVQuestObjectiveType::Talk || ObjectiveType == EVHVQuestObjectiveType::Conversation", EditConditionHides))
    TSoftObjectPtr<UVHVConversationDataAsset> Conversation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "ObjectiveType == EVHVQuestObjectiveType::Talk || ObjectiveType == EVHVQuestObjectiveType::Conversation", EditConditionHides))
    FName EntryNodeID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "ObjectiveType == EVHVQuestObjectiveType::Conversation || ObjectiveType == EVHVQuestObjectiveType::LearningActivity", EditConditionHides))
    bool bAutoStart = true;

    /** Optional participant-specific conversations used by non-linear objectives. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Contextual Conversations")
    TArray<FVHVQuestContextualConversation> ContextualConversations;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (DisplayName = "Participant", Categories = "VHV.Participant", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction", EditConditionHides))
    FGameplayTag NPCParticipantTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "Objective|NPC Action", meta = (DisplayName = "NPC Participant ID (Legacy)"))
    FName NPCParticipantID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction", EditConditionHides))
    EVHVNPCQuestCommandType NPCCommandType = EVHVNPCQuestCommandType::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (DisplayName = "Behavior Target", Categories = "VHV.BehaviorTarget", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction && NPCCommandType == EVHVNPCQuestCommandType::MoveToTarget", EditConditionHides))
    FGameplayTag NPCBehaviorTargetTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "Objective|NPC Action", meta = (DisplayName = "NPC Target ID (Legacy)"))
    FName NPCTargetID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction && NPCCommandType == EVHVNPCQuestCommandType::Wait", EditConditionHides, ClampMin = "0.0"))
    float NPCWaitDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Action", meta = (DisplayName = "NPC Action", Categories = "VHV.NPCAction", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::NPCAction && NPCCommandType == EVHVNPCQuestCommandType::PlayAction", EditConditionHides))
    FGameplayTag NPCActionTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "Objective|NPC Action", meta = (DisplayName = "NPC Action ID (Legacy)"))
    FName NPCActionID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|World Action", meta = (DisplayName = "World Receiver", Categories = "VHV.WorldReceiver", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::WorldAction", EditConditionHides))
    FGameplayTag WorldActionReceiverTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "Objective|World Action", meta = (DisplayName = "World Action Receiver ID (Legacy)"))
    FName WorldActionReceiverID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|World Action", meta = (DisplayName = "World Action", Categories = "VHV.WorldAction", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::WorldAction", EditConditionHides))
    FGameplayTag WorldActionTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|World Action", meta = (DisplayName = "Start Policy", EditCondition = "ObjectiveType == EVHVQuestObjectiveType::WorldAction", EditConditionHides))
    EVHVWorldActionStartPolicy WorldActionStartPolicy = EVHVWorldActionStartPolicy::Immediate;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "Objective|World Action", meta = (DisplayName = "World Action ID (Legacy)"))
    FName WorldActionID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Story State")
    FVHVStoryConditionSet ActivationConditions;

    /** Optional conditions that complete an active objective when Story State changes. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Story State")
    FVHVStoryConditionSet CompletionConditions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Story State")
    TArray<FVHVStoryEffect> CompletionEffects;

    /** Lightweight presentation shown when readiness gates have passed and the objective becomes playable. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|Presentation")
    FVHVMajorQuestStingerData ActivationStinger;

    /** Fire-and-forget NPC movement started when this objective completes. It does not become quest UI or block progression. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Movement")
    TArray<FVHVQuestNPCMoveRequest> CompletionNPCMoves;

    /**
     * Ordered, blocking movement stages started when this objective activates.
     * Every participant in a stage must arrive before the next stage begins, and
     * the objective's normal interaction/auto-start remains locked until all stages finish.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Movement")
    TArray<FVHVQuestNPCMoveStage> NPCMoveStages;

    /** Existing moves that must already have reached their targets before this objective becomes playable. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Movement")
    TArray<FVHVQuestNPCMoveRequest> ActivationNPCReadiness;

    /** When set, staged movement starts only after these story-state conditions become true. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Movement")
    FVHVStoryConditionSet NPCMoveStageActivationConditions;

    /** Complete the objective directly after every authored movement stage succeeds. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Movement")
    bool bCompleteAfterNPCMoveStages = false;

    /** Free-roam travel blocks quest progression on arrivals while leaving gameplay input and camera control active. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective|NPC Movement")
    EVHVQuestNPCTravelMode NPCTravelMode = EVHVQuestNPCTravelMode::Standard;

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

    FName GetEffectiveParticipantID() const { return VHVAuthoringReferences::ResolveID(ParticipantTag, NAME_None, TEXT("VHV.Participant")); }
    FName GetEffectiveActivityID() const { return VHVAuthoringReferences::ResolveID(ActivityTag, ActivityID, TEXT("VHV.Activity")); }
    FName GetEffectiveLocationID() const { return VHVAuthoringReferences::ResolveID(LocationTag, NAME_None, TEXT("VHV.Location")); }
    FName GetEffectiveTrackingLocationID() const
    {
        if (TrackingLocationTag.IsValid())
        {
            return VHVAuthoringReferences::ResolveID(TrackingLocationTag, NAME_None, TEXT("VHV.Location"));
        }
        const FName ObjectiveLocationID = GetEffectiveLocationID();
        if (!ObjectiveLocationID.IsNone())
        {
            return ObjectiveLocationID;
        }
        return ActivationNPCReadiness.Num() == 1
            ? ActivationNPCReadiness[0].GetEffectiveDestinationID()
            : NAME_None;
    }
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

    /** Optional non-interactive presentation shown when this quest becomes active. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Presentation")
    FVHVMajorQuestStingerData StartStinger;

    /** Optional non-interactive presentation shown when this quest completes. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest|Presentation")
    FVHVMajorQuestStingerData CompletionStinger;
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
