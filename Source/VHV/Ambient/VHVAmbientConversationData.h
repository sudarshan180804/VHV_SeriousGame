#pragma once

#include "Ambient/VHVAmbientSpeechTypes.h"
#include "Engine/DataAsset.h"
#include "VHVAmbientConversationData.generated.h"

UCLASS(BlueprintType)
class VHV_API UVHVAmbientConversationData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Conversation")
    FName ConversationID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Conversation")
    TArray<FVHVAmbientParticipantDefinition> Participants;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Conversation")
    TArray<FVHVAmbientSpeechLine> Lines;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Conversation")
    bool bShowSpeakerName = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ambient Conversation|Story State")
    TArray<FVHVStoryEffect> CompletionEffects;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    const FVHVAmbientParticipantDefinition* FindParticipant(FName SlotID) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

