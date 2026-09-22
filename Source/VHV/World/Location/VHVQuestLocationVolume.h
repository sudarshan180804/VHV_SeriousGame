#pragma once

#include "CoreMinimal.h"
#include "Core/VHVAuthoringReferences.h"
#include "GameFramework/Actor.h"
#include "VHVQuestLocationVolume.generated.h"

class AVHVCharacter;
class UBoxComponent;
class UPrimitiveComponent;
class UVHVQuestSubsystem;
struct FHitResult;

UCLASS()
class VHV_API AVHVQuestLocationVolume : public AActor
{
    GENERATED_BODY()

public:
    AVHVQuestLocationVolume();

protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

    UFUNCTION()
    void HandleBoxBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleBoxEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|Location")
    TObjectPtr<UBoxComponent> BoxComponent;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|Location", meta = (DisplayName = "Location", Categories = "VHV.Location"))
    FGameplayTag LocationTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "VHV|Location", meta = (DisplayName = "Location ID (Legacy)"))
    FName LocationID;

    UFUNCTION(BlueprintPure, Category = "VHV|Location")
    FName GetEffectiveLocationID() const { return VHVAuthoringReferences::ResolveID(LocationTag, LocationID, TEXT("VHV.Location")); }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHV|Location")
    bool bEnabled = true;

    /** Optionally dispatch a generic WorldAction when the player enters this existing location trigger. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Location|World Action")
    bool bTriggerWorldAction = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Location|World Action", meta = (DisplayName = "World Receiver", Categories = "VHV.WorldReceiver", EditCondition = "bTriggerWorldAction", EditConditionHides))
    FGameplayTag WorldActionReceiverTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Location|World Action", meta = (DisplayName = "World Action", Categories = "VHV.WorldAction", EditCondition = "bTriggerWorldAction", EditConditionHides))
    FGameplayTag WorldActionTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Location|World Action", meta = (EditCondition = "bTriggerWorldAction", EditConditionHides))
    bool bTriggerWorldActionOnce = true;

private:
    UPROPERTY(Transient)
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    TSet<TWeakObjectPtr<AVHVCharacter>> PlayersInside;
    bool bWorldActionTriggered = false;
};
