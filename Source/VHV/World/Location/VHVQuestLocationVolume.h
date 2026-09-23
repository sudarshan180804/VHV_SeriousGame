#pragma once

#include "CoreMinimal.h"
#include "Core/VHVAuthoringReferences.h"
#include "GameFramework/Actor.h"
#include "Story/Types/VHVStoryStateTypes.h"
#include "VHVQuestLocationVolume.generated.h"

class AVHVCharacter;
class UBillboardComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UTextRenderComponent;
class UVHVQuestSubsystem;
class UVHVStoryStateSubsystem;
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
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
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

    /** Route the WorldAction through the active Explicit Trigger objective so async completion advances it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Location|World Action", meta = (EditCondition = "bTriggerWorldAction", EditConditionHides))
    bool bTrackWorldActionAsObjective = true;

    /** Ask the QuestSubsystem to open the active Conversation or LearningActivity on entry. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Location|Objective Activation")
    bool bActivateCurrentObjective = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Location|Objective Activation", meta = (EditCondition = "bActivateCurrentObjective", EditConditionHides))
    bool bActivateCurrentObjectiveOnce = true;

    /** Optional quest gate. When set, only this active quest may dispatch the WorldAction. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Location|Story Trigger|Gating")
    FName RequiredActiveQuestID;

    /** Optional objective gate. When set, only this active objective may dispatch the WorldAction. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Location|Story Trigger|Gating")
    FName RequiredActiveObjectiveID;

    /** Optional Story State conditions evaluated when the player enters. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Location|Story Trigger|Gating")
    FVHVStoryConditionSet TriggerConditions;

private:
#if WITH_EDITORONLY_DATA
    /** Screen-scaled editor handle that makes large trigger volumes easy to pick. Never cooked. */
    UPROPERTY()
    TObjectPtr<UBillboardComponent> EditorTriggerSprite;

    /** Editor-only identifier displayed above the trigger. Never cooked. */
    UPROPERTY()
    TObjectPtr<UTextRenderComponent> EditorTriggerLabel;
#endif

#if WITH_EDITOR
    void RefreshEditorVisualization();
#endif

    UPROPERTY(Transient)
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    UPROPERTY(Transient)
    TObjectPtr<UVHVStoryStateSubsystem> StoryStateSubsystem;

    TSet<TWeakObjectPtr<AVHVCharacter>> PlayersInside;
    bool bWorldActionTriggered = false;
    bool bCurrentObjectiveActivatedByTrigger = false;
};
