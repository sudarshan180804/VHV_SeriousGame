#include "Audio/VHVAudioSubsystem.h"

#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "Sound/SoundAttenuation.h"
#include "Textbook/Systems/VHVTextbookSubsystem.h"
#include "VHV.h"

namespace
{
    const TCHAR* AudioRoot = TEXT("/Game/Audio/VHV/SFX");
    constexpr int32 ExpectedEventCount = 19;
}

void UVHVAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    QuestSubsystem = Collection.InitializeDependency<UVHVQuestSubsystem>();
    TextbookSubsystem = Collection.InitializeDependency<UVHVTextbookSubsystem>();

    if (QuestSubsystem)
    {
        QuestSubsystem->OnQuestStarted.AddDynamic(this, &UVHVAudioSubsystem::HandleQuestStarted);
        QuestSubsystem->OnQuestCompleted.AddDynamic(this, &UVHVAudioSubsystem::HandleQuestCompleted);
        QuestSubsystem->OnObjectiveCompleted.AddDynamic(this, &UVHVAudioSubsystem::HandleObjectiveCompleted);
    }
    if (TextbookSubsystem)
    {
        TextbookSubsystem->OnLearningPhaseChanged.AddDynamic(this, &UVHVAudioSubsystem::HandleLearningPhaseChanged);
        TextbookSubsystem->OnActivityCompleted.AddDynamic(this, &UVHVAudioSubsystem::HandleActivityCompleted);
    }

    WorldAttenuation = NewObject<USoundAttenuation>(this, TEXT("VHVFootstepAttenuation"));
    if (WorldAttenuation)
    {
        WorldAttenuation->Attenuation.bAttenuate = true;
        WorldAttenuation->Attenuation.bSpatialize = true;
        WorldAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
        WorldAttenuation->Attenuation.AttenuationShapeExtents = FVector(120.0f);
        WorldAttenuation->Attenuation.FalloffDistance = 900.0f;
    }

    if (EnsureManifestLoaded())
    {
        PreloadSounds();
    }
}

void UVHVAudioSubsystem::Deinitialize()
{
    if (QuestSubsystem)
    {
        QuestSubsystem->OnQuestStarted.RemoveAll(this);
        QuestSubsystem->OnQuestCompleted.RemoveAll(this);
        QuestSubsystem->OnObjectiveCompleted.RemoveAll(this);
    }
    if (TextbookSubsystem)
    {
        TextbookSubsystem->OnLearningPhaseChanged.RemoveAll(this);
        TextbookSubsystem->OnActivityCompleted.RemoveAll(this);
    }
    Events.Empty();
    LoadedSounds.Empty();
    LastPlayTimes.Empty();
    EventTable = nullptr;
    WorldAttenuation = nullptr;
    QuestSubsystem = nullptr;
    TextbookSubsystem = nullptr;
    Super::Deinitialize();
}

bool UVHVAudioSubsystem::EnsureManifestLoaded()
{
    if (bManifestLoadAttempted)
    {
        return Events.Num() == ExpectedEventCount;
    }
    bManifestLoadAttempted = true;

    EventTable = LoadObject<UDataTable>(nullptr,
        TEXT("/Game/Audio/VHV/DT_VHV_AudioEvents.DT_VHV_AudioEvents"));
    if (!EventTable)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVAudio] The 19-event audio table is unavailable; audio will remain silent."));
        return false;
    }

    TArray<FVHVAudioEventRow*> Rows;
    EventTable->GetAllRows(TEXT("VHVAudioSubsystem"), Rows);
    for (const FVHVAudioEventRow* Row : Rows)
    {
        if (Row && !Row->UE_AssetName.IsEmpty())
        {
            Events.Add(FName(*Row->UE_AssetName), *Row);
        }
    }
    if (Events.Num() != ExpectedEventCount)
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHVAudio] Expected %d one-shot events but loaded %d; unavailable IDs will stay silent."),
            ExpectedEventCount, Events.Num());
        return false;
    }
    UE_LOG(LogVHV, Log, TEXT("[VHVAudio] Loaded the 19-event one-shot SFX manifest."));
    return true;
}

void UVHVAudioSubsystem::PreloadSounds()
{
    for (const TPair<FName, FVHVAudioEventRow>& Event : Events)
    {
        const FString Path = FString::Printf(TEXT("%s/%s.%s"), AudioRoot,
            *Event.Value.UE_AssetName, *Event.Value.UE_AssetName);
        if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path))
        {
            LoadedSounds.Add(Event.Key, Sound);
        }
    }
    UE_LOG(LogVHV, Log, TEXT("[VHVAudio] Preloaded %d/%d short SoundWaves."),
        LoadedSounds.Num(), Events.Num());
}

const FVHVAudioEventRow* UVHVAudioSubsystem::FindEvent(const FName EventID)
{
    EnsureManifestLoaded();
    return Events.Find(EventID);
}

USoundBase* UVHVAudioSubsystem::ResolveSound(const FName EventID)
{
    if (const TObjectPtr<USoundBase>* Cached = LoadedSounds.Find(EventID))
    {
        return Cached->Get();
    }
    const FVHVAudioEventRow* Row = FindEvent(EventID);
    if (!Row)
    {
        return nullptr;
    }
    const FString Path = FString::Printf(TEXT("%s/%s.%s"), AudioRoot,
        *Row->UE_AssetName, *Row->UE_AssetName);
    if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path))
    {
        LoadedSounds.Add(EventID, Sound);
        return Sound;
    }
    return nullptr;
}

FVector2D UVHVAudioSubsystem::ParseRange(const FString& Text, const float DefaultValue)
{
    FString Left;
    FString Right;
    if (Text.Split(TEXT("-"), &Left, &Right))
    {
        return FVector2D(FCString::Atof(*Left), FCString::Atof(*Right));
    }
    const float Value = Text.IsEmpty() ? DefaultValue : FCString::Atof(*Text);
    return FVector2D(Value, Value);
}

bool UVHVAudioSubsystem::CanPlayNow(const FName EventID, const float CooldownSeconds)
{
    if (CooldownSeconds <= 0.0f)
    {
        return true;
    }
    const double Now = FPlatformTime::Seconds();
    if (const double* Previous = LastPlayTimes.Find(EventID);
        Previous && Now - *Previous < CooldownSeconds)
    {
        return false;
    }
    LastPlayTimes.Add(EventID, Now);
    return true;
}

bool UVHVAudioSubsystem::PlayEvent(const FName EventID)
{
    const FVHVAudioEventRow* Row = FindEvent(EventID);
    USoundBase* Sound = ResolveSound(EventID);
    UWorld* World = GetWorld();
    if (!Row || !Sound || !World)
    {
        return false;
    }

    const float Cooldown = EventID == TEXT("DIA_BubbleAppear") ? 0.02f : 0.0f;
    if (!CanPlayNow(EventID, Cooldown))
    {
        return false;
    }

    const FVector2D Pitch = ParseRange(Row->RandomPitchRange, 1.0f);
    const FVector2D Volume = ParseRange(Row->RandomVolumeRange, 1.0f);
    UGameplayStatics::PlaySound2D(World, Sound,
        FMath::FRandRange(Volume.X, Volume.Y),
        FMath::FRandRange(Pitch.X, Pitch.Y), 0.0f, nullptr, nullptr, true);
    return true;
}

bool UVHVAudioSubsystem::PlayEventAtLocation(const FName EventID, const FVector Location)
{
    const FVHVAudioEventRow* Row = FindEvent(EventID);
    USoundBase* Sound = ResolveSound(EventID);
    UWorld* World = GetWorld();
    if (!Row || !Sound || !World || !CanPlayNow(EventID, 0.18f))
    {
        return false;
    }

    const FVector2D Pitch = ParseRange(Row->RandomPitchRange, 1.0f);
    const FVector2D Volume = ParseRange(Row->RandomVolumeRange, 1.0f);
    UGameplayStatics::PlaySoundAtLocation(World, Sound, Location, FRotator::ZeroRotator,
        FMath::FRandRange(Volume.X, Volume.Y),
        FMath::FRandRange(Pitch.X, Pitch.Y), 0.0f, WorldAttenuation, nullptr, nullptr);
    return true;
}

void UVHVAudioSubsystem::HandleQuestStarted(const FName QuestID)
{
    PlayEvent(TEXT("QST_QuestStarted"));
}

void UVHVAudioSubsystem::HandleQuestCompleted(const FName QuestID)
{
    PlayEvent(TEXT("QST_QuestComplete"));
}

void UVHVAudioSubsystem::HandleObjectiveCompleted(const FName QuestID, const FName ObjectiveID)
{
    if (QuestID != TEXT("Q_HBCT_05_SELF_MONITORING"))
    {
        return;
    }
    if (ObjectiveID == TEXT("O07_RecordBreakfast")
        || ObjectiveID == TEXT("O08_RecordAfternoon")
        || ObjectiveID == TEXT("O09_RecordEveningWalk"))
    {
        PlayEvent(TEXT("HBCT_Q5_TrackerEntry"));
    }
}

void UVHVAudioSubsystem::HandleLearningPhaseChanged(const ELearningPhase NewPhase)
{
    if (!TextbookSubsystem || !TextbookSubsystem->IsActivityActive())
    {
        return;
    }

    const FTextbookRuntimeState State = TextbookSubsystem->GetRuntimeState();
    if (!State.bAnswerSubmitted
        || (NewPhase != ELearningPhase::Hint
            && NewPhase != ELearningPhase::Feedback
            && NewPhase != ELearningPhase::Teach))
    {
        return;
    }

    const FTextbookActivityData Activity = TextbookSubsystem->GetCurrentActivity();
    const FString ResultKey = FString::Printf(TEXT("%s:%d"),
        *Activity.GetEffectiveActivityID(), State.AttemptCount);
    if (ResultKey == LastLearningResultKey)
    {
        return;
    }
    LastLearningResultKey = ResultKey;
    PlayEvent(State.bAnswerCorrect ? TEXT("EDU_AnswerCorrect") : TEXT("EDU_AnswerIncorrect"));
}

void UVHVAudioSubsystem::HandleActivityCompleted()
{
    // Completion is the sole sound for actions that complete without a separate result phase.
    PlayEvent(TEXT("EDU_PracticeCompleted"));
}
