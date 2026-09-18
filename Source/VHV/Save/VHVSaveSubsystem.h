#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VHVSaveSubsystem.generated.h"

class UVHVQuestSubsystem;
class UVHVStoryStateSubsystem;
class UVHVTextbookSubsystem;
class UVHVUIManagerComponent;

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

    UPROPERTY()
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    UPROPERTY()
    TObjectPtr<UVHVStoryStateSubsystem> StoryStateSubsystem;

    UPROPERTY()
    TObjectPtr<UVHVTextbookSubsystem> TextbookSubsystem;
};
