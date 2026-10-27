#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VHVAudioEventRow.h"
#include "VHVAudioSubsystem.generated.h"

class UDataTable;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class UVHVQuestSubsystem;
class UVHVTextbookSubsystem;
enum class ELearningPhase : uint8;

/** Central routing for the deliberately small VHV one-shot SFX library. */
UCLASS()
class VHV_API UVHVAudioSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Plays a non-spatial one-shot. Unknown event IDs are intentional quiet no-ops. */
    UFUNCTION(BlueprintCallable, Category="VHV|Audio")
    bool PlayEvent(FName EventID);

    /** Plays a lightweight positional one-shot, used only for player footsteps. */
    UFUNCTION(BlueprintCallable, Category="VHV|Audio")
    bool PlayEventAtLocation(FName EventID, FVector Location);

    /** Starts the single looping front-end track, fading any gameplay track first. */
    UFUNCTION(BlueprintCallable, Category="VHV|Audio|Music")
    void PlayMainMenuMusic();

    /** Starts the single looping village track, fading any front-end track first. */
    UFUNCTION(BlueprintCallable, Category="VHV|Audio|Music")
    void PlayGameplayMusic();

    UFUNCTION(BlueprintCallable, Category="VHV|Audio|Music")
    void StopMusic(float FadeSeconds = 0.8f);

    UFUNCTION(BlueprintCallable, Category="VHV|Audio|Music")
    void SetMusicVolume(float NormalizedVolume);

    UFUNCTION(BlueprintPure, Category="VHV|Audio|Music")
    float GetMusicVolume() const { return MusicVolume; }

private:
    bool EnsureManifestLoaded();
    void PreloadSounds();
    USoundBase* ResolveSound(FName EventID);
    const FVHVAudioEventRow* FindEvent(FName EventID);
    bool CanPlayNow(FName EventID, float CooldownSeconds);
    static FVector2D ParseRange(const FString& Text, float DefaultValue);
    void PlayMusic(USoundBase* Sound, FName TrackID);
    void SaveMusicVolume() const;

    UFUNCTION() void HandleQuestCompleted(FName QuestID);
    UFUNCTION() void HandleObjectiveCompleted(FName QuestID, FName ObjectiveID);
    UFUNCTION() void HandleLearningPhaseChanged(ELearningPhase NewPhase);
    UFUNCTION() void HandleActivityCompleted();

    UPROPERTY(Transient) TObjectPtr<UDataTable> EventTable;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> LoadedSounds;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> WorldAttenuation;
    UPROPERTY(Transient) TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;
    UPROPERTY(Transient) TObjectPtr<UVHVTextbookSubsystem> TextbookSubsystem;
    UPROPERTY(Transient) TObjectPtr<USoundBase> MainMenuMusic;
    UPROPERTY(Transient) TObjectPtr<USoundBase> GameplayMusic;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> MusicComponent;

    TMap<FName, FVHVAudioEventRow> Events;
    TMap<FName, double> LastPlayTimes;
    FString LastLearningResultKey;
    FName ActiveMusicTrack;
    float MusicVolume = 0.70f;
    bool bManifestLoadAttempted = false;
};
