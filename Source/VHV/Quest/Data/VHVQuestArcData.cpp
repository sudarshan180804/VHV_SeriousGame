#include "Quest/Data/VHVQuestArcData.h"

FPrimaryAssetId UVHVQuestArcData::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("VHVQuestArc"), QuestArcID.IsNone() ? GetFName() : QuestArcID);
}
