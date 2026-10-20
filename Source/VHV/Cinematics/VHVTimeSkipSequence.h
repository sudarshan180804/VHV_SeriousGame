#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VHVTimeSkipSequence.generated.h"

class AVHVAmbientConversationActor;
class AVHVQuestLocationVolume;
class USkeletalMeshComponent;
class UVHVQuestSubsystem;
class UVHVStoryStateSubsystem;
class UVHVTimeSkipOverlayWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FVHVTimeSkipStagePresentationActivated, FName, StageID, FName, ObjectiveID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVHVTimeSkipSequenceCompleted);

UENUM(BlueprintType)
enum class EVHVTimeSkipSequenceState : uint8
{
    Idle,
    PlayingStage,
    Transitioning,
    Completed
};

USTRUCT(BlueprintType)
struct VHV_API FVHVTimeSkipStage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip")
    FName StageID;

    /** Optional objective represented by this stage when objective-completion advancement is enabled. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Gating")
    FName ObjectiveID;

    /** Text displayed after this stage and before the next one. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip")
    FText TransitionText = FText::FromString(TEXT("Afterwards..."));

    /** Existing scene whose genuine completion advances this stage. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Time Skip")
    TObjectPtr<AVHVAmbientConversationActor> ObservationScene;

    /** Existing NPCs/props visible and collidable only while this stage is active. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Time Skip")
    TArray<TObjectPtr<AActor>> StageActors;

    /** Optional existing marker/TargetPoint used for runtime player arrival. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Time Skip")
    TObjectPtr<AActor> PlayerArrivalTarget;

    /** Optional actor used to calculate a conversational arrival behind its current runtime orientation. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Time Skip")
    TObjectPtr<AActor> PlayerArrivalRelativeActor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip", meta = (ClampMin = "150.0", ClampMax = "260.0", Units = "Centimeters"))
    float PlayerArrivalBehindDistance = 180.0f;

    /** Optional right-vector offset for separating conversational capsules and sight lines. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip", meta = (ClampMin = "-100.0", ClampMax = "100.0", Units = "Centimeters"))
    float PlayerArrivalLateralOffset = 0.0f;

    /** During blackout, rotate the relative actor toward the resolved player arrival without translating it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip")
    bool bFaceRelativeActorTowardPlayer = false;

    /** Optional physical support actor for elevated platforms. Null retains Landscape-only support. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Time Skip")
    TObjectPtr<AActor> PlayerArrivalSupportActor;

    /** Existing overlap route retained for the first stage and as a safe fallback. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Time Skip")
    TObjectPtr<AVHVQuestLocationVolume> LegacyTrigger;

    /** Auto-play ObservationScene after a successful blackout transition. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip")
    bool bAutoStartObservationAfterTransition = true;

    /** Optional breathing room after fade-out before an automatic observation begins. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip", meta = (ClampMin = "0.0"))
    float PostTransitionObservationDelay = 0.0f;

    /** A fade-only stage may omit scene and stage-actor ownership. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip")
    bool bPresentationOnly = false;
};

/**
 * Reusable, event-driven stage director for short cinematic time transitions.
 * It never owns quest progress: authored scene completion effects remain authoritative.
 */
UCLASS(Blueprintable)
class VHV_API AVHVTimeSkipSequence : public AActor
{
    GENERATED_BODY()

public:
    AVHVTimeSkipSequence();

    /** Notification only; does not alter sequence timing or quest progress. */
    UPROPERTY(BlueprintAssignable, Category = "Time Skip")
    FVHVTimeSkipStagePresentationActivated OnStagePresentationActivated;

    /** Fired once after the final required observation succeeds. */
    UPROPERTY(BlueprintAssignable, Category = "Time Skip")
    FVHVTimeSkipSequenceCompleted OnSequenceCompleted;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Gating")
    FName RequiredQuestID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Gating")
    FName RequiredObjectiveID;

    /** Optional saved-progress counter; completed count selects the resume stage. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Gating")
    FName ProgressCounterID;

    /** Advance from each stage's genuine quest-objective completion instead of conversation completion. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Gating")
    bool bAdvanceOnObjectiveCompletion = false;

    /**
     * Arm when RequiredObjectiveID completes, then play the first stage as an
     * entry transition when that stage's objective becomes active. This never
     * advances quest state and does not replay when loading directly into the
     * target objective.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Gating")
    bool bTransitionOnNextObjectiveActivation = false;

    /** Defer an armed entry transition until objective readiness has staged its runtime participants. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Gating")
    bool bWaitForObjectiveReadyForEntryTransition = false;

    /** Q3 retains Recast validation; cinematic platform sequences may opt out per instance. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Safety")
    bool bRequireNavigationProjection = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip")
    TArray<FVHVTimeSkipStage> Stages;

    /** Actors shared by every stage; hidden whenever this sequence is inactive. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Time Skip|Presentation")
    TArray<TObjectPtr<AActor>> SequenceActors;

    /** Opt-in hook for a staged lesson whose final observation unlocks its authored objective activity. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Gating")
    bool bRequestObjectiveActivationOnSequenceCompleted = false;

    /** Existing Q3/Q5 sequences preserve their final tableau by default. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Presentation")
    bool bKeepFinalStageAfterObjective = true;

    /** Stop only this sequence's ambient observations when its objective is skipped or replaced. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Safety")
    bool bStopObservationsWhenDeactivated = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Presentation")
    TSubclassOf<UVHVTimeSkipOverlayWidget> OverlayWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Presentation", meta = (ClampMin = "0.05"))
    float FadeInDuration = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Presentation", meta = (ClampMin = "0.0"))
    float TextHoldDuration = 0.60f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Presentation", meta = (ClampMin = "0.05"))
    float FadeOutDuration = 0.28f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time Skip|Safety", meta = (ClampMin = "1.0"))
    float TransitionFailSafeDuration = 3.0f;

    /** Blueprint/dialogue extension point for future non-ambient stages. */
    UFUNCTION(BlueprintCallable, Category = "VHV|Time Skip")
    bool NotifyCurrentStageCompleted(FName StageID, bool bSuccess = true);

    UFUNCTION(BlueprintPure, Category = "VHV|Time Skip")
    EVHVTimeSkipSequenceState GetSequenceState() const { return SequenceState; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
    UPROPERTY(Transient)
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UVHVStoryStateSubsystem> StoryStateSubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UVHVTimeSkipOverlayWidget> OverlayWidget;

    EVHVTimeSkipSequenceState SequenceState = EVHVTimeSkipSequenceState::Idle;
    int32 CurrentStageIndex = INDEX_NONE;
    int32 PendingStageIndex = INDEX_NONE;
    bool bCurrentStageConversationStarted = false;
    bool bInputLocked = false;
    bool bChangedMoveIgnore = false;
    bool bChangedLookIgnore = false;
    bool bDisabledPawnInput = false;
    bool bPendingPlayerRelocation = false;
    bool bEntryTransitionArmed = false;
    FName DiagnosticSourceObjectiveID;
    FName DiagnosticDestinationObjectiveID;
    bool bDiagnosticSourceComplete = false;
    bool bDiagnosticDestinationActive = false;
    bool bDiagnosticTransitionRequested = false;
    bool bDiagnosticArrivalResolved = false;
    bool bDiagnosticCollisionValid = false;
    bool bDiagnosticOverlayStarted = false;
    bool bDiagnosticInputRestored = false;
    FString DiagnosticBlockReason;
    FTransform PendingPlayerTransform = FTransform::Identity;
    FTimerHandle TransitionFailSafeTimer;
    FTimerHandle PostTransitionObservationTimer;
    TSet<FName> CompletedStageIDs;
    TMap<TWeakObjectPtr<AActor>, bool> OriginalActorHiddenStates;
    TMap<TWeakObjectPtr<AActor>, bool> OriginalActorCollisionStates;
    TMap<TWeakObjectPtr<USkeletalMeshComponent>, bool> OriginalMeshVisibilityStates;
    TMap<TWeakObjectPtr<USkeletalMeshComponent>, bool> OriginalMeshHiddenStates;
    TMap<TWeakObjectPtr<AVHVQuestLocationVolume>, bool> OriginalTriggerEnabledStates;
    TMap<TWeakObjectPtr<AVHVQuestLocationVolume>, TEnumAsByte<ECollisionEnabled::Type>> OriginalTriggerCollisionStates;

    UFUNCTION()
    void HandleObjectiveChanged(FName QuestID, FName ObjectiveID);

    UFUNCTION()
    void HandleObjectiveReady(FName QuestID, FName ObjectiveID);

    UFUNCTION()
    void HandleObjectiveCompleted(FName QuestID, FName ObjectiveID);

    UFUNCTION()
    void HandleQuestStarted(FName QuestID);

    UFUNCTION()
    void HandleQuestCompleted(FName QuestID);

    UFUNCTION()
    void HandleConversationStarted();

    UFUNCTION()
    void HandleConversationFinished(bool bSuccess);

    void RefreshForCurrentObjective();
    void ActivateSequence();
    void ConfigureInactivePresentation(bool bKeepFinalStage);
    void ApplyStagePresentation(int32 StageIndex);
    void SetLegacyTriggerEnabled(int32 StageIndex, bool bEnabled);
    bool BeginTransition(int32 NextStageIndex);
    bool BeginEntryTransition(int32 StageIndex);
    bool EnsureOverlay();
    bool ResolvePlayerArrival(int32 StageIndex, FTransform& OutTransform);
    void HandleOverlayFullyObscured();
    void HandleOverlayFinished();
    void HandleTransitionFailSafe();
    void StartPendingAutoObservation();
    void LockPlayerInput();
    void RestorePlayerInput();
    void EnableFallbackForStage(int32 StageIndex, const TCHAR* Reason);
    void RestoreOriginalActorState();
    void BeginQ5Diagnostic(FName SourceObjectiveID, FName DestinationObjectiveID);
    void LogQ5Diagnostic(const TCHAR* Phase) const;
};
