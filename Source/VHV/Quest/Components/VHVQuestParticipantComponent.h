#pragma once

#include "CoreMinimal.h"
#include "Core/VHVAuthoringReferences.h"
#include "Components/ActorComponent.h"
#include "VHVQuestParticipantComponent.generated.h"

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVQuestParticipantComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVHVQuestParticipantComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Quest", meta = (DisplayName = "Participant", Categories = "VHV.Participant"))
    FGameplayTag ParticipantTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "VHV|Quest", meta = (DisplayName = "Participant ID (Legacy)"))
    FName ParticipantID;

    UFUNCTION(BlueprintPure, Category = "VHV|Quest")
    FName GetEffectiveParticipantID() const { return VHVAuthoringReferences::ResolveID(ParticipantTag, ParticipantID, TEXT("VHV.Participant")); }

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool NotifyInteracted();

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
