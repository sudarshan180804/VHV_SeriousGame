#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VHVDeveloperSubsystem.generated.h"

class UVHVQuestSubsystem;
class UVHVSaveSubsystem;
class UVHVStoryStateSubsystem;
class UInputComponent;

UCLASS()
class VHV_API UVHVDeveloperSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    void PrintStatus() const;

    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    bool CompleteCurrentObjective();

    /** Completes exactly one captured active quest without playing skipped objectives. */
    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    bool CompleteCurrentQuest();

    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    bool RestartCurrentQuest();

    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    bool ActivateCurrentObjective();

    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    bool SetStoryFlag(FName FlagID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    bool ClearStoryFlag(FName FlagID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    bool SetStoryCounter(FName CounterID, int32 Value);

    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    bool AddStoryCounter(FName CounterID, int32 Delta);

    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    bool TriggerWorldAction(FName ReceiverID, FName ActionID);

    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    bool SaveProgress();

    UFUNCTION(BlueprintCallable, Category = "VHV|Developer")
    bool LoadProgress();

    /** Registers development-only keyboard shortcuts on the local player input stack. */
    void BindDeveloperInput(UInputComponent* InputComponent);

private:
    /** Developer shortcut: N completes exactly one active quest objective. */
    void HandleCompleteCurrentObjectiveShortcut();

    /** Developer shortcut: M completes exactly one active quest and starts its successor. */
    void HandleCompleteCurrentQuestShortcut();

    UPROPERTY()
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    UPROPERTY()
    TObjectPtr<UVHVStoryStateSubsystem> StoryStateSubsystem;

    UPROPERTY()
    TObjectPtr<UVHVSaveSubsystem> SaveSubsystem;

#if !UE_BUILD_SHIPPING
    /** Rejects nested N/M input while the synchronous whole-quest transition is dispatching delegates. */
    bool bWholeQuestSkipInProgress = false;
#endif
};
