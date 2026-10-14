#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VHVQ5DisplayVisibilityController.generated.h"

class AVHVTimeSkipSequence;
class UPrimitiveComponent;
class UVHVQuestSubsystem;

USTRUCT(BlueprintType)
struct VHV_API FVHVQ5ObjectiveDisplayGroup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Q5 Displays")
    FName ObjectiveID;

    /** All actors forming this objective's single logical world-space display. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Q5 Displays")
    TArray<TObjectPtr<AActor>> DisplayActors;
};

/** Runtime-only visibility ownership for Q5 informational world displays. */
UCLASS(Blueprintable)
class VHV_API AVHVQ5DisplayVisibilityController : public AActor
{
    GENERATED_BODY()

public:
    AVHVQ5DisplayVisibilityController();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Q5 Displays")
    FName RequiredQuestID = TEXT("Q_HBCT_05_SELF_MONITORING");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Q5 Displays")
    TArray<FVHVQ5ObjectiveDisplayGroup> ObjectiveDisplays;

    /** Optional Q5 sequence; used only to defer O08/O09 display reveal until blackout. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Q5 Displays")
    TObjectPtr<AVHVTimeSkipSequence> TimeSkipSequence;

    /** Additional Q5-only entry transitions that defer their target display until blackout. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Q5 Displays")
    TArray<TObjectPtr<AVHVTimeSkipSequence>> AdditionalTimeSkipSequences;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
    UPROPERTY(Transient)
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    FName PendingObjectiveID;
    TMap<TWeakObjectPtr<AActor>, bool> OriginalActorHiddenStates;
    TMap<TWeakObjectPtr<AActor>, bool> OriginalActorCollisionStates;
    TMap<TWeakObjectPtr<UPrimitiveComponent>, bool> OriginalComponentVisibilityStates;
    TMap<TWeakObjectPtr<UPrimitiveComponent>, bool> OriginalComponentHiddenStates;
    TMap<TWeakObjectPtr<UPrimitiveComponent>, TEnumAsByte<ECollisionEnabled::Type>> OriginalComponentCollisionStates;

    UFUNCTION()
    void HandleObjectiveChanged(FName QuestID, FName ObjectiveID);

    UFUNCTION()
    void HandleQuestStarted(FName QuestID);

    UFUNCTION()
    void HandleQuestCompleted(FName QuestID);

    UFUNCTION()
    void HandleTimeSkipStageActivated(FName StageID, FName ObjectiveID);

    void RefreshFromCurrentObjective();
    void ApplyObjectiveVisibility(FName ObjectiveID);
    void HideAllDisplays();
    void SetDisplayActorActive(AActor* DisplayActor, bool bActive);
    bool ShouldDeferForTimeSkip(FName ObjectiveID) const;
    void RestoreOriginalStates();
};
