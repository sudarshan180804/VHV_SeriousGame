#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Quest/Types/VHVQuestTypes.h"
#include "VHVQuestSubsystem.generated.h"

class UVHVQuestArcData;
class UVHVTextbookSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVHVQuestEvent, FName, QuestID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVHVQuestObjectiveEvent, FName, QuestID, FName, ObjectiveID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVHVTrackedQuestChanged, FName, TrackedQuestID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVHVQuestArcEvent, FName, QuestArcID);

UCLASS()
class VHV_API UVHVQuestSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool RegisterQuestArc(UVHVQuestArcData* QuestArc);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool StartQuestArc(UVHVQuestArcData* QuestArc);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool StartQuest(FName QuestID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool SetTrackedQuest(FName QuestID);

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool GetTrackedQuest(FVHVQuestJournalEntry& OutQuest) const;

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool GetCurrentObjective(FVHVQuestObjectiveDefinition& OutObjective) const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool CompleteCurrentObjective();

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool GetQuestState(FName QuestID, FVHVQuestRuntimeState& OutState) const;

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    FVHVQuestArcRuntimeState GetRuntimeState() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    TArray<FVHVQuestJournalEntry> GetQuestJournalEntries() const;

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    bool IsQuestFlowActive() const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool RequestCurrentObjectiveActivation();

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool NotifyParticipantInteracted(FName ParticipantID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyConversationCompleted(FName ConversationID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyActivityCompleted(FName ActivityID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyLocationReached(FName LocationID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    void NotifyCustomEvent(FName EventID);

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestEvent OnQuestStarted;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestEvent OnQuestUpdated;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestEvent OnQuestCompleted;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestObjectiveEvent OnObjectiveChanged;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestObjectiveEvent OnObjectiveCompleted;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestObjectiveEvent OnObjectiveActivationRequested;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVTrackedQuestChanged OnTrackedQuestChanged;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Quest|Events")
    FOnVHVQuestArcEvent OnQuestArcCompleted;

private:
    UFUNCTION()
    void HandleTextbookActivityCompleted();

    bool ValidateQuestArc(const UVHVQuestArcData* QuestArc) const;
    void CompleteCurrentQuest();
    void ActivateCurrentObjective();
    bool BuildJournalEntry(const FVHVQuestDefinition& Definition, const FVHVQuestRuntimeState& State, FVHVQuestJournalEntry& OutEntry) const;
    FName GetRequiredConversationID(const FVHVQuestObjectiveDefinition& Objective) const;
    const FVHVQuestDefinition* FindQuestDefinition(FName QuestID) const;
    FVHVQuestRuntimeState* FindQuestState(FName QuestID);
    const FVHVQuestRuntimeState* FindQuestState(FName QuestID) const;
    const FVHVQuestObjectiveDefinition* GetActiveObjective() const;

    UPROPERTY()
    TObjectPtr<UVHVQuestArcData> ActiveQuestArc;

    UPROPERTY()
    TObjectPtr<UVHVTextbookSubsystem> TextbookSubsystem;

    UPROPERTY()
    FVHVQuestArcRuntimeState RuntimeState;
};
