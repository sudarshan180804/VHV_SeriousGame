#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "VHVSocialSupportDirector.generated.h"

class AVHVAmbientConversationActor;
class UVHVQuestSubsystem;
class UVHVStoryStateSubsystem;
class UVHVUIManagerComponent;

UCLASS()
class VHV_API AVHVSocialSupportDirector : public AActor
{
    GENERATED_BODY()

public:
    AVHVSocialSupportDirector();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Observation")
    TArray<FGameplayTag> ObservationFlags;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Supporters")
    TArray<FGameplayTag> SupporterFlags;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Observation")
    TObjectPtr<AVHVAmbientConversationActor> WalkingObservationScene;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Observation", meta = (Categories = "VHV.Participant"))
    FGameplayTag ObservationWalkerTag;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Observation", meta = (Categories = "VHV.Participant"))
    FGameplayTag ObservationPartnerTag;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Observation", meta = (Categories = "VHV.Location"))
    FGameplayTag ObservationWalkerDestinationTag;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Observation", meta = (Categories = "VHV.Location"))
    FGameplayTag ObservationPartnerDestinationTag;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Rooms")
    TObjectPtr<AVHVAmbientConversationActor> EmotionalRoomScene;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Rooms")
    TObjectPtr<AVHVAmbientConversationActor> InformationalRoomScene;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Rooms")
    TObjectPtr<AVHVAmbientConversationActor> InstrumentalRoomScene;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Rooms")
    TObjectPtr<AVHVAmbientConversationActor> AppraisalRoomScene;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Rooms", meta = (Categories = "VHV.Story.Flag"))
    FGameplayTag EmotionalRoomFlag;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Rooms", meta = (Categories = "VHV.Story.Flag"))
    FGameplayTag InformationalRoomFlag;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Rooms", meta = (Categories = "VHV.Story.Flag"))
    FGameplayTag InstrumentalRoomFlag;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Rooms", meta = (Categories = "VHV.Story.Flag"))
    FGameplayTag AppraisalRoomFlag;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Rooms")
    TObjectPtr<AVHVAmbientConversationActor> RoomsCompleteScene;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Social Support|Network")
    TObjectPtr<AVHVAmbientConversationActor> NetworkReadyScene;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UVHVStoryStateSubsystem> StoryStateSubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UVHVUIManagerComponent> UIManager;

    void InitializeRuntimeBindings();
    void RefreshQuestPresentation();
    bool IsActiveObjective(FName ObjectiveID) const;
    int32 CountSetFlags(const TArray<FGameplayTag>& Flags) const;
    FName ResolveFlag(const FGameplayTag& Flag) const;
    void ShowRoomClassification(FName ActivityTagName, const FGameplayTag& CompletionFlag);
    bool StageUncleChaiForIntroduction();

    UFUNCTION()
    void HandleObjectiveChanged(FName QuestID, FName ObjectiveID);

    UFUNCTION()
    void HandleObjectiveCompleted(FName QuestID, FName ObjectiveID);

    UFUNCTION()
    void HandleStoryFlagChanged(FName FlagID, bool bValue);

    UFUNCTION()
    void HandleWalkingObservationStarted();

    UFUNCTION()
    void HandleEmotionalRoomFinished(bool bSuccess);

    UFUNCTION()
    void HandleInformationalRoomFinished(bool bSuccess);

    UFUNCTION()
    void HandleInstrumentalRoomFinished(bool bSuccess);

    UFUNCTION()
    void HandleAppraisalRoomFinished(bool bSuccess);
};
