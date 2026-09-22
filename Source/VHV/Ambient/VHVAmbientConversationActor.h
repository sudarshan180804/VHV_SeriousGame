#pragma once

#include "Ambient/VHVAmbientSpeechTypes.h"
#include "GameFramework/Actor.h"
#include "NPC/Types/VHVNPCBehaviorTypes.h"
#include "VHVAmbientConversationActor.generated.h"

class AVHVNPCCharacter;
class UVHVAmbientConversationData;
class UVHVAmbientSpeechComponent;
class UVHVWorldActionReceiverComponent;

USTRUCT(BlueprintType)
struct VHV_API FVHVAmbientParticipantBinding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Participant")
    FName SlotID;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Participant")
    TObjectPtr<AVHVNPCCharacter> NPC;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVHVAmbientConversationStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVHVAmbientConversationFinished, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnVHVAmbientLineStarted, int32, LineIndex, AVHVNPCCharacter*, Speaker, FGameplayTag, PresentationTag);

UCLASS(Blueprintable)
class VHV_API AVHVAmbientConversationActor : public AActor
{
    GENERATED_BODY()

public:
    AVHVAmbientConversationActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Conversation")
    TObjectPtr<UVHVAmbientConversationData> ConversationData;

    /** Bind each slot declared by Conversation Data to a placed NPC. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|Ambient Conversation")
    TArray<FVHVAmbientParticipantBinding> Participants;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Conversation|Playback")
    bool bAutoStartOnBeginPlay = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Conversation|Playback")
    bool bPlayOnce = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Conversation|Playback")
    EVHVAmbientConversationLeavePolicy PlayerLeavePolicy = EVHVAmbientConversationLeavePolicy::Cancel;

    /** Zero disables player-distance leave handling. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Conversation|Playback", meta = (ClampMin = "0.0", Units = "Centimeters"))
    float ObservationRadius = 2600.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|Ambient Conversation")
    TObjectPtr<UVHVWorldActionReceiverComponent> WorldActionReceiver;

    UFUNCTION(BlueprintCallable, Category = "VHV|Ambient Conversation")
    bool StartConversation();

    UFUNCTION(BlueprintCallable, Category = "VHV|Ambient Conversation")
    void StopConversation();

    UFUNCTION(BlueprintCallable, Category = "VHV|Ambient Conversation")
    void ResetConversation();

    UFUNCTION(BlueprintPure, Category = "VHV|Ambient Conversation")
    bool IsPlaying() const { return bPlaying; }

    UFUNCTION(BlueprintPure, Category = "VHV|Ambient Conversation")
    int32 GetCurrentLineIndex() const { return CurrentLineIndex; }

    UFUNCTION(BlueprintPure, Category = "VHV|Ambient Conversation|Debug")
    AVHVNPCCharacter* GetCurrentSpeaker() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Ambient Conversation|Debug")
    bool AreParticipantsValid() const;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Ambient Conversation|Events")
    FOnVHVAmbientConversationStarted OnConversationStarted;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Ambient Conversation|Events")
    FOnVHVAmbientConversationFinished OnConversationFinished;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Ambient Conversation|Events")
    FOnVHVAmbientLineStarted OnLineStarted;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<AVHVNPCCharacter>> ResolvedParticipants;

    TMap<TWeakObjectPtr<AVHVNPCCharacter>, EVHVNPCBehaviorState> PreviousBehaviorStates;
    TMap<TWeakObjectPtr<AVHVNPCCharacter>, FName> StartedPresentationActions;
    FTimerHandle SequenceTimerHandle;
    FGuid ActiveWorldActionRequestID;
    int32 CurrentLineIndex = INDEX_NONE;
    bool bPlaying = false;
    bool bHasPlayed = false;
    bool bPausedForDistance = false;

    UFUNCTION()
    void HandleWorldActionRequested(FGuid RequestID, FName ActionID);

    UFUNCTION()
    void HandleWorldActionCompleted(FGuid RequestID, FName ReceiverID, FName ActionID, bool bSuccess);

    bool StartConversationInternal(FGuid WorldActionRequestID);
    bool ResolveAndValidateParticipants(FString& OutError);
    void PrepareParticipants();
    void RestoreParticipants();
    void AdvanceSequence();
    void ShowCurrentLine();
    void CompleteConversation(bool bSuccess);
    void HideAllBubbles(bool bImmediate);
    void ApplyLineFacing(AVHVNPCCharacter* Speaker, bool bEnabled);
    void ClearFacing();
    void UpdateBubbleSeparation();
    bool IsPlayerOutsideObservationRadius() const;
    AVHVNPCCharacter* FindParticipant(FName SlotID) const;
    FText FindSpeakerName(FName SlotID) const;
    float GetLineDuration(const FVHVAmbientSpeechLine& Line) const;
};
