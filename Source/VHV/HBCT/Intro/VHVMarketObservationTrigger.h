#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VHVMarketObservationTrigger.generated.h"

class AVHVAmbientConversationActor;
class UBoxComponent;
class UPrimitiveComponent;
class UVHVQuestSubsystem;
class UVHVStoryStateSubsystem;
struct FHitResult;

/** Q0-only proximity trigger for one independently completed market observation. */
UCLASS()
class VHV_API AVHVMarketObservationTrigger : public AActor
{
    GENERATED_BODY()

public:
    AVHVMarketObservationTrigger();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|Q0 Market Observation")
    TObjectPtr<UBoxComponent> BoxComponent;

    /** Stable Q0 observation ID (for example Market_Smoker). */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|Q0 Market Observation")
    FName ObservationID;

    /** The existing six-participant market scene; its authored participant bindings are reused. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|Q0 Market Observation")
    TObjectPtr<AVHVAmbientConversationActor> ConversationActor;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|Q0 Market Observation|Gating")
    FName RequiredQuestID = FName(TEXT("Q_HBCT_00_INTRO"));

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|Q0 Market Observation|Gating")
    FName RequiredObjectiveID = FName(TEXT("O03_ObserveMarket"));

    /** Clear only the persisted Q0 market-observation progress used by these triggers. */
    static void ResetObservationProgress(UVHVStoryStateSubsystem* StoryState);

    static const TArray<FName>& GetRequiredObservationIDs();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
    UFUNCTION()
    void HandleBoxBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleConversationFinished(bool bSuccess);

    bool IsIntendedObjectiveActive() const;
    bool ResolveLineRange(int32& OutFirstLineIndex, int32& OutLineCount) const;
    FName GetCompletionFlagID() const;
    int32 CountCompletedObservations() const;

    UPROPERTY(Transient)
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UVHVStoryStateSubsystem> StoryStateSubsystem;

    bool bAwaitingConversationCompletion = false;
};
