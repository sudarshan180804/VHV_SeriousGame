#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VHVSaveSubsystem.generated.h"

class UVHVQuestSubsystem;
class UVHVStoryStateSubsystem;
class UVHVTextbookSubsystem;
class UVHVUIManagerComponent;
class UVHVSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVHVSaveOperationCompleted, bool, bSuccess);

UCLASS()
class VHV_API UVHVSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "VHV|Save")
    bool SaveProgress();

    UFUNCTION(BlueprintCallable, Category = "VHV|Save")
    bool LoadProgress();

    UFUNCTION(BlueprintPure, Category = "VHV|Save")
    bool DoesSaveExist() const;

    /** Returns true only when the slot can be validated and has a gameplay map to restore. */
    UFUNCTION(BlueprintPure, Category = "VHV|Save")
    bool CanLoadProgress() const;

    /** Travels to the saved map and restores through LoadProgress after gameplay initialization. */
    UFUNCTION(BlueprintCallable, Category = "VHV|Save")
    bool LoadProgressFromMainMenu();

    /** Allows the gameplay controller to avoid starting fresh systems during a Continue travel. */
    bool IsLoadTravelPending() const { return bLoadTravelPending; }

    UFUNCTION(BlueprintCallable, Category = "VHV|Save")
    bool DeleteSave();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHV|Save")
    FString SlotName = TEXT("VHV_Progress");

    UPROPERTY(BlueprintAssignable, Category = "VHV|Save")
    FOnVHVSaveOperationCompleted OnSaveCompleted;

    UPROPERTY(BlueprintAssignable, Category = "VHV|Save")
    FOnVHVSaveOperationCompleted OnLoadCompleted;

private:
    UVHVUIManagerComponent* GetUIManager() const;
    bool IsStableState(const TCHAR* OperationName) const;
    FString GetCurrentMapName() const;
    UVHVSaveGame* LoadValidatedSaveGame() const;
    void HandlePostLoadMap(UWorld* LoadedWorld);

    UPROPERTY()
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    UPROPERTY()
    TObjectPtr<UVHVStoryStateSubsystem> StoryStateSubsystem;

    UPROPERTY()
    TObjectPtr<UVHVTextbookSubsystem> TextbookSubsystem;

    bool bLoadTravelPending = false;
    FString PendingLoadMapName;
};
