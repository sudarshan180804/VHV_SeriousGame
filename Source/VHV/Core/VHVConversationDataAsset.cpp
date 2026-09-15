#include "Core/VHVConversationDataAsset.h"

UVHVConversationDataAsset::UVHVConversationDataAsset()
{
}

FPrimaryAssetId UVHVConversationDataAsset::GetPrimaryAssetId() const
{
    const FPrimaryAssetType PrimaryAssetType = TEXT("VHVConversation");

    if (!Conversation.ConversationID.IsEmpty())
    {
        return FPrimaryAssetId(PrimaryAssetType, FName(*Conversation.ConversationID));
    }

    return FPrimaryAssetId(PrimaryAssetType, GetFName());
}
