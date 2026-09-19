#include "Debug/VHVDeveloperSubsystem.h"

#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "Save/VHVSaveSubsystem.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "VHV.h"
#include "World/Systems/VHVWorldActionSubsystem.h"

#if !UE_BUILD_SHIPPING
namespace
{
    UVHVDeveloperSubsystem* ResolveDeveloperSubsystem(UWorld* World)
    {
        UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
        UVHVDeveloperSubsystem* DeveloperSubsystem = GameInstance ? GameInstance->GetSubsystem<UVHVDeveloperSubsystem>() : nullptr;
        if (!DeveloperSubsystem)
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] No VHV Developer Subsystem is available for this world."));
        }
        return DeveloperSubsystem;
    }

    bool RequireArgumentCount(const TArray<FString>& Args, const int32 ExpectedCount, const TCHAR* Usage)
    {
        if (Args.Num() == ExpectedCount)
        {
            return true;
        }
        UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Usage: %s"), Usage);
        return false;
    }

    void StatusCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 0, TEXT("vhv.status")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->PrintStatus();
        }
    }

    void CompleteQuestCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 0, TEXT("vhv.quest.complete")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->CompleteCurrentObjective();
        }
    }

    void RestartQuestCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 0, TEXT("vhv.quest.restart")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->RestartCurrentQuest();
        }
    }

    void SetFlagCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 1, TEXT("vhv.story.setflag <FlagID>")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->SetStoryFlag(FName(*Args[0]));
        }
    }

    void ClearFlagCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 1, TEXT("vhv.story.clearflag <FlagID>")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->ClearStoryFlag(FName(*Args[0]));
        }
    }

    void SetCounterCommand(const TArray<FString>& Args, UWorld* World)
    {
        int32 Value = 0;
        if (!RequireArgumentCount(Args, 2, TEXT("vhv.story.setcounter <CounterID> <Value>"))) return;
        if (!LexTryParseString(Value, *Args[1]))
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Usage: vhv.story.setcounter <CounterID> <Value> (Value must be an integer)"));
            return;
        }
        if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->SetStoryCounter(FName(*Args[0]), Value);
    }

    void AddCounterCommand(const TArray<FString>& Args, UWorld* World)
    {
        int32 Delta = 0;
        if (!RequireArgumentCount(Args, 2, TEXT("vhv.story.addcounter <CounterID> <Delta>"))) return;
        if (!LexTryParseString(Delta, *Args[1]))
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Usage: vhv.story.addcounter <CounterID> <Delta> (Delta must be an integer)"));
            return;
        }
        if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->AddStoryCounter(FName(*Args[0]), Delta);
    }

    void WorldActionCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 2, TEXT("vhv.world.action <ReceiverID> <ActionID>")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->TriggerWorldAction(FName(*Args[0]), FName(*Args[1]));
        }
    }

    void SaveCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 0, TEXT("vhv.save")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->SaveProgress();
        }
    }

    void LoadCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 0, TEXT("vhv.load")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->LoadProgress();
        }
    }

    static FAutoConsoleCommandWithWorldAndArgs GStatusCommand(
        TEXT("vhv.status"), TEXT("Print a compact VHV runtime status snapshot."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StatusCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GCompleteQuestCommand(
        TEXT("vhv.quest.complete"), TEXT("Complete the current objective through normal quest progression."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CompleteQuestCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GRestartQuestCommand(
        TEXT("vhv.quest.restart"), TEXT("Restart the active authored quest at its first objective."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RestartQuestCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GSetFlagCommand(
        TEXT("vhv.story.setflag"), TEXT("Usage: vhv.story.setflag <FlagID>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetFlagCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GClearFlagCommand(
        TEXT("vhv.story.clearflag"), TEXT("Usage: vhv.story.clearflag <FlagID>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ClearFlagCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GSetCounterCommand(
        TEXT("vhv.story.setcounter"), TEXT("Usage: vhv.story.setcounter <CounterID> <Value>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetCounterCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GAddCounterCommand(
        TEXT("vhv.story.addcounter"), TEXT("Usage: vhv.story.addcounter <CounterID> <Delta>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AddCounterCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GWorldActionCommand(
        TEXT("vhv.world.action"), TEXT("Usage: vhv.world.action <ReceiverID> <ActionID>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WorldActionCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GSaveCommand(
        TEXT("vhv.save"), TEXT("Save VHV progress using the existing save subsystem."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SaveCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GLoadCommand(
        TEXT("vhv.load"), TEXT("Load VHV progress using the existing save subsystem."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LoadCommand), ECVF_Cheat);
}
#endif

bool UVHVDeveloperSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return Super::ShouldCreateSubsystem(Outer);
#endif
}

void UVHVDeveloperSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
    QuestSubsystem = Collection.InitializeDependency<UVHVQuestSubsystem>();
    StoryStateSubsystem = Collection.InitializeDependency<UVHVStoryStateSubsystem>();
    SaveSubsystem = Collection.InitializeDependency<UVHVSaveSubsystem>();
#endif
}

void UVHVDeveloperSubsystem::Deinitialize()
{
    QuestSubsystem = nullptr;
    StoryStateSubsystem = nullptr;
    SaveSubsystem = nullptr;
    Super::Deinitialize();
}

void UVHVDeveloperSubsystem::PrintStatus() const
{
#if !UE_BUILD_SHIPPING
    const FVHVQuestArcRuntimeState QuestState = QuestSubsystem ? QuestSubsystem->GetRuntimeState() : FVHVQuestArcRuntimeState();
    FVHVQuestObjectiveDefinition Objective;
    const bool bHasObjective = QuestSubsystem && QuestSubsystem->GetCurrentObjective(Objective);
    const int32 FlagCount = StoryStateSubsystem ? StoryStateSubsystem->GetSetFlagIDs().Num() : 0;
    const int32 CounterCount = StoryStateSubsystem ? StoryStateSubsystem->GetCounterValues().Num() : 0;
    const bool bSaveExists = SaveSubsystem && SaveSubsystem->DoesSaveExist();

    UE_LOG(LogVHV, Log,
        TEXT("[VHVDeveloper] Arc=%s Quest=%s Objective=%s Waiting=%s Flags=%d Counters=%d Save=%s Text=\"%s\""),
        *QuestState.QuestArcID.ToString(),
        *QuestState.ActiveQuestID.ToString(),
        bHasObjective ? *Objective.ObjectiveID.ToString() : TEXT("None"),
        QuestSubsystem && QuestSubsystem->IsCurrentObjectiveWaitingForActivation() ? TEXT("true") : TEXT("false"),
        FlagCount,
        CounterCount,
        bSaveExists ? TEXT("yes") : TEXT("no"),
        bHasObjective ? *Objective.ObjectiveText.ToString() : TEXT(""));
#endif
}

bool UVHVDeveloperSubsystem::CompleteCurrentObjective()
{
#if !UE_BUILD_SHIPPING
    return QuestSubsystem && QuestSubsystem->DebugCompleteCurrentObjective();
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::RestartCurrentQuest()
{
#if !UE_BUILD_SHIPPING
    return QuestSubsystem && QuestSubsystem->DebugRestartCurrentQuest();
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::SetStoryFlag(const FName FlagID)
{
#if !UE_BUILD_SHIPPING
    if (!StoryStateSubsystem || FlagID.IsNone()) return false;
    StoryStateSubsystem->SetFlag(FlagID);
    return true;
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::ClearStoryFlag(const FName FlagID)
{
#if !UE_BUILD_SHIPPING
    if (!StoryStateSubsystem || FlagID.IsNone()) return false;
    StoryStateSubsystem->ClearFlag(FlagID);
    return true;
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::SetStoryCounter(const FName CounterID, const int32 Value)
{
#if !UE_BUILD_SHIPPING
    if (!StoryStateSubsystem || CounterID.IsNone()) return false;
    StoryStateSubsystem->SetCounter(CounterID, Value);
    return true;
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::AddStoryCounter(const FName CounterID, const int32 Delta)
{
#if !UE_BUILD_SHIPPING
    if (!StoryStateSubsystem || CounterID.IsNone()) return false;
    StoryStateSubsystem->AddCounter(CounterID, Delta);
    return true;
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::TriggerWorldAction(const FName ReceiverID, const FName ActionID)
{
#if !UE_BUILD_SHIPPING
    if (ReceiverID.IsNone() || ActionID.IsNone() || !GetWorld())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Usage: vhv.world.action <ReceiverID> <ActionID>"));
        return false;
    }
    UVHVWorldActionSubsystem* WorldActionSubsystem = GetWorld()->GetSubsystem<UVHVWorldActionSubsystem>();
    if (!WorldActionSubsystem)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] World Action subsystem is unavailable."));
        return false;
    }

    FGuid RequestID;
    const EVHVWorldActionExecutionResult Result = WorldActionSubsystem->RequestWorldAction(ReceiverID, ActionID, RequestID);
    UE_LOG(LogVHV, Log, TEXT("[VHVDeveloper] World action Receiver=%s Action=%s Result=%s Request=%s"),
        *ReceiverID.ToString(), *ActionID.ToString(), *UEnum::GetValueAsString(Result), *RequestID.ToString());
    return Result != EVHVWorldActionExecutionResult::Rejected;
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::SaveProgress()
{
#if !UE_BUILD_SHIPPING
    return SaveSubsystem && SaveSubsystem->SaveProgress();
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::LoadProgress()
{
#if !UE_BUILD_SHIPPING
    return SaveSubsystem && SaveSubsystem->LoadProgress();
#else
    return false;
#endif
}
