#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VHVAudioEventRow.h"
#include "VHVAudioSubsystem.generated.h"

class UDataTable;
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

private:
    bool EnsureManifestLoaded();
    void PreloadSounds();
    USoundBase* ResolveSound(FName EventID);
    const FVHVAudioEventRow* FindEvent(FName EventID);
    bool CanPlayNow(FName EventID, float CooldownSeconds);
    static FVector2D ParseRange(const FString& Text, float DefaultValue);

    UFUNCTION() void HandleQuestStarted(FName QuestID);
    UFUNCTION() void HandleQuestCompleted(FName QuestID);
    UFUNCTION() void HandleObjectiveCompleted(FName QuestID, FName ObjectiveID);
    UFUNCTION() void HandleLearningPhaseChanged(ELearningPhase NewPhase);
    UFUNCTION() void HandleActivityCompleted();

    UPROPERTY(Transient) TObjectPtr<UDataTable> EventTable;
    UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> LoadedSounds;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> WorldAttenuation;
    UPROPERTY(Transient) TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;
    UPROPERTY(Transient) TObjectPtr<UVHVTextbookSubsystem> TextbookSubsystem;

    TMap<FName, FVHVAudioEventRow> Events;
    TMap<FName, double> LastPlayTimes;
    FString LastLearningResultKey;
    bool bManifestLoadAttempted = false;
};
