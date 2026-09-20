#include "Save/VHVSaveSubsystem.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "Save/VHVSaveGame.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "Textbook/Systems/VHVTextbookSubsystem.h"
#include "UI/VHVUIManagerComponent.h"
#include "VHV.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    constexpr int32 CurrentSaveVersion = 1;
    constexpr int32 SaveUserIndex = 0;
}

void UVHVSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    StoryStateSubsystem = Collection.InitializeDependency<UVHVStoryStateSubsystem>();
    TextbookSubsystem = Collection.InitializeDependency<UVHVTextbookSubsystem>();
    QuestSubsystem = Collection.InitializeDependency<UVHVQuestSubsystem>();
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UVHVSaveSubsystem::HandlePostLoadMap);
}

void UVHVSaveSubsystem::Deinitialize()
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
    bLoadTravelPending = false;
    PendingLoadMapName.Reset();
    QuestSubsystem = nullptr;
    StoryStateSubsystem = nullptr;
    TextbookSubsystem = nullptr;
    Super::Deinitialize();
}

bool UVHVSaveSubsystem::SaveProgress()
{
    bool bSuccess = false;
    if (!IsStableState(TEXT("save")) || !QuestSubsystem || !StoryStateSubsystem || !TextbookSubsystem)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVSave] Save failed because required runtime systems are unavailable or busy."));
        OnSaveCompleted.Broadcast(false);
        return false;
    }

    UVHVSaveGame* SaveGame = Cast<UVHVSaveGame>(UGameplayStatics::CreateSaveGameObject(UVHVSaveGame::StaticClass()));
    if (SaveGame)
    {
        SaveGame->SaveVersion = CurrentSaveVersion;
        SaveGame->SavedMapName = GetCurrentMapName();
        QuestSubsystem->ExportSaveState(SaveGame->QuestState);
        StoryStateSubsystem->ExportSaveState(SaveGame->StoryState);
        TextbookSubsystem->ExportSaveState(SaveGame->TextbookState);
        if (const UVHVUIManagerComponent* UIManager = GetUIManager())
        {
            UIManager->ExportDialogueCheckpointSaveState(SaveGame->DialogueCheckpoints);
        }
        bSuccess = UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, SaveUserIndex);
    }

    if (bSuccess)
    {
        UE_LOG(LogVHV, Log, TEXT("[VHVSave] Save to slot '%s' succeeded."), *SlotName);
    }
    else
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVSave] Save to slot '%s' failed."), *SlotName);
    }
    OnSaveCompleted.Broadcast(bSuccess);
    return bSuccess;
}

bool UVHVSaveSubsystem::LoadProgress()
{
    if (!IsStableState(TEXT("load")) || !QuestSubsystem || !StoryStateSubsystem || !TextbookSubsystem)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVSave] Load failed because required runtime systems are unavailable or busy."));
        OnLoadCompleted.Broadcast(false);
        return false;
    }

    UVHVSaveGame* SaveGame = Cast<UVHVSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, SaveUserIndex));
    if (!SaveGame)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVSave] No valid VHV save could be loaded from slot '%s'."), *SlotName);
        OnLoadCompleted.Broadcast(false);
        return false;
    }
    if (SaveGame->SaveVersion != CurrentSaveVersion)
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVSave] Slot '%s' uses unsupported save version %d; expected %d."), *SlotName, SaveGame->SaveVersion, CurrentSaveVersion);
        OnLoadCompleted.Broadcast(false);
        return false;
    }

    const FString CurrentMapName = GetCurrentMapName();
    if (SaveGame->SavedMapName.IsEmpty() || SaveGame->SavedMapName != CurrentMapName)
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVSave] Slot '%s' belongs to map '%s', but the current map is '%s'. Load the saved map before restoring progress."),
            *SlotName, *SaveGame->SavedMapName, *CurrentMapName);
        OnLoadCompleted.Broadcast(false);
        return false;
    }

    UVHVUIManagerComponent* UIManager = GetUIManager();
    const bool bValid = StoryStateSubsystem->ValidateSaveState(SaveGame->StoryState)
        && TextbookSubsystem->ValidateSaveState(SaveGame->TextbookState)
        && QuestSubsystem->ValidateSaveState(SaveGame->QuestState)
        && (!UIManager || UIManager->ValidateDialogueCheckpointSaveState(SaveGame->DialogueCheckpoints));
    if (!bValid)
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVSave] Slot '%s' contains invalid or incompatible runtime data."), *SlotName);
        OnLoadCompleted.Broadcast(false);
        return false;
    }

    bool bSuccess = StoryStateSubsystem->ImportSaveState(SaveGame->StoryState, false)
        && TextbookSubsystem->ImportSaveState(SaveGame->TextbookState);
    if (bSuccess && UIManager)
    {
        bSuccess = UIManager->ImportDialogueCheckpointSaveState(SaveGame->DialogueCheckpoints);
    }
    if (bSuccess)
    {
        bSuccess = QuestSubsystem->ImportSaveState(SaveGame->QuestState);
    }

    if (bSuccess)
    {
        UE_LOG(LogVHV, Log, TEXT("[VHVSave] Load from slot '%s' succeeded."), *SlotName);
    }
    else
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVSave] Load from slot '%s' failed."), *SlotName);
    }
    OnLoadCompleted.Broadcast(bSuccess);
    return bSuccess;
}

bool UVHVSaveSubsystem::DoesSaveExist() const
{
    return UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex);
}

bool UVHVSaveSubsystem::CanLoadProgress() const
{
    return LoadValidatedSaveGame() != nullptr;
}

bool UVHVSaveSubsystem::LoadProgressFromMainMenu()
{
    const UVHVSaveGame* SaveGame = LoadValidatedSaveGame();
    if (!SaveGame)
    {
        OnLoadCompleted.Broadcast(false);
        return false;
    }

    PendingLoadMapName = SaveGame->SavedMapName;
    bLoadTravelPending = true;
    UGameplayStatics::OpenLevel(this, FName(*PendingLoadMapName));
    return true;
}

bool UVHVSaveSubsystem::DeleteSave()
{
    if (!DoesSaveExist())
    {
        return true;
    }
    const bool bSuccess = UGameplayStatics::DeleteGameInSlot(SlotName, SaveUserIndex);
    if (bSuccess)
    {
        UE_LOG(LogVHV, Log, TEXT("[VHVSave] Delete of slot '%s' succeeded."), *SlotName);
    }
    else
    {
        UE_LOG(LogVHV, Error, TEXT("[VHVSave] Delete of slot '%s' failed."), *SlotName);
    }
    return bSuccess;
}

UVHVUIManagerComponent* UVHVSaveSubsystem::GetUIManager() const
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    return PlayerController ? PlayerController->FindComponentByClass<UVHVUIManagerComponent>() : nullptr;
}

bool UVHVSaveSubsystem::IsStableState(const TCHAR* OperationName) const
{
    if (const UVHVUIManagerComponent* UIManager = GetUIManager(); UIManager && UIManager->IsConversationSessionActive())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVSave] Cannot %s while a dialogue session is active."), OperationName);
        return false;
    }
    if (TextbookSubsystem && TextbookSubsystem->IsActivityActive())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVSave] Cannot %s while a learning activity is active."), OperationName);
        return false;
    }
    if (QuestSubsystem && QuestSubsystem->IsTransientObjectiveExecutionActive())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVSave] Cannot %s while an NPC or World Action objective is executing."), OperationName);
        return false;
    }
    return true;
}

FString UVHVSaveSubsystem::GetCurrentMapName() const
{
    return GetWorld() ? UGameplayStatics::GetCurrentLevelName(GetWorld(), true) : FString();
}

UVHVSaveGame* UVHVSaveSubsystem::LoadValidatedSaveGame() const
{
    if (!DoesSaveExist())
    {
        return nullptr;
    }

    UVHVSaveGame* SaveGame = Cast<UVHVSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, SaveUserIndex));
    if (!SaveGame || SaveGame->SaveVersion != CurrentSaveVersion || SaveGame->SavedMapName.IsEmpty()
        || !StoryStateSubsystem || !TextbookSubsystem || !QuestSubsystem)
    {
        return nullptr;
    }

    return StoryStateSubsystem->ValidateSaveState(SaveGame->StoryState)
        && TextbookSubsystem->ValidateSaveState(SaveGame->TextbookState)
        && QuestSubsystem->ValidateSaveState(SaveGame->QuestState)
        ? SaveGame
        : nullptr;
}

void UVHVSaveSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
    if (!bLoadTravelPending || !LoadedWorld)
    {
        return;
    }

    const FString LoadedMapName = UGameplayStatics::GetCurrentLevelName(LoadedWorld, true);
    if (LoadedMapName != PendingLoadMapName)
    {
        return;
    }

    bLoadTravelPending = false;
    PendingLoadMapName.Reset();
    LoadedWorld->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            LoadProgress();
        }));
}
