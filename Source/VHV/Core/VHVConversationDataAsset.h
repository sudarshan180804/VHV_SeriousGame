#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/VHVDialogueTypes.h"
#include "VHVConversationDataAsset.generated.h"

UCLASS(BlueprintType)
class VHV_API UVHVConversationDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UVHVConversationDataAsset();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Conversation")
    FDialogueConversation Conversation;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
