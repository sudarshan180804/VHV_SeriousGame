#include "Core/VHVAuthoringReferences.h"

namespace VHVAuthoringReferences
{
FName ResolveTagLeaf(const FGameplayTag& Tag)
{
    if (!Tag.IsValid())
    {
        return NAME_None;
    }

    const FString TagString = Tag.ToString();
    int32 LastSeparatorIndex = INDEX_NONE;
    if (TagString.FindLastChar(TEXT('.'), LastSeparatorIndex))
    {
        return FName(*TagString.RightChop(LastSeparatorIndex + 1));
    }

    return Tag.GetTagName();
}

FName ResolveID(const FGameplayTag& Tag, const FName LegacyID, const TCHAR* ExpectedCategory)
{
    return IsConcreteTagInCategory(Tag, ExpectedCategory) ? ResolveTagLeaf(Tag) : LegacyID;
}

FString ResolveStringID(const FGameplayTag& Tag, const FString& LegacyID, const TCHAR* ExpectedCategory)
{
    if (!IsConcreteTagInCategory(Tag, ExpectedCategory))
    {
        return LegacyID;
    }

    return ResolveTagLeaf(Tag).ToString();
}

bool HasConflict(const FGameplayTag& Tag, const FName LegacyID, const TCHAR* ExpectedCategory)
{
    return !LegacyID.IsNone()
        && IsConcreteTagInCategory(Tag, ExpectedCategory)
        && ResolveTagLeaf(Tag) != LegacyID;
}

bool HasConflict(const FGameplayTag& Tag, const FString& LegacyID, const TCHAR* ExpectedCategory)
{
    return !LegacyID.IsEmpty()
        && IsConcreteTagInCategory(Tag, ExpectedCategory)
        && ResolveTagLeaf(Tag).ToString() != LegacyID;
}

bool IsConcreteTagInCategory(const FGameplayTag& Tag, const TCHAR* ExpectedCategory)
{
    if (!Tag.IsValid() || ExpectedCategory == nullptr || ExpectedCategory[0] == TEXT('\0'))
    {
        return false;
    }

    const FString TagString = Tag.ToString();
    const FString CategoryPrefix = FString(ExpectedCategory) + TEXT(".");
    return TagString.Len() > CategoryPrefix.Len()
        && TagString.StartsWith(CategoryPrefix, ESearchCase::CaseSensitive);
}

bool IsValidReferenceTag(const FGameplayTag& Tag, const TCHAR* ExpectedCategory)
{
    return !Tag.IsValid() || IsConcreteTagInCategory(Tag, ExpectedCategory);
}
}

FName UVHVAuthoringReferenceLibrary::ResolveParticipantID(const FGameplayTag ParticipantTag, const FName LegacyParticipantID)
{
    return VHVAuthoringReferences::ResolveID(ParticipantTag, LegacyParticipantID, TEXT("VHV.Participant"));
}

FName UVHVAuthoringReferenceLibrary::ResolveLocationID(const FGameplayTag LocationTag, const FName LegacyLocationID)
{
    return VHVAuthoringReferences::ResolveID(LocationTag, LegacyLocationID, TEXT("VHV.Location"));
}

FName UVHVAuthoringReferenceLibrary::ResolveBehaviorTargetID(const FGameplayTag BehaviorTargetTag, const FName LegacyBehaviorTargetID)
{
    return VHVAuthoringReferences::ResolveID(BehaviorTargetTag, LegacyBehaviorTargetID, TEXT("VHV.BehaviorTarget"));
}

FName UVHVAuthoringReferenceLibrary::ResolveWorldReceiverID(const FGameplayTag WorldReceiverTag, const FName LegacyWorldReceiverID)
{
    return VHVAuthoringReferences::ResolveID(WorldReceiverTag, LegacyWorldReceiverID, TEXT("VHV.WorldReceiver"));
}

FName UVHVAuthoringReferenceLibrary::ResolveWorldActionID(const FGameplayTag WorldActionTag, const FName LegacyWorldActionID)
{
    return VHVAuthoringReferences::ResolveID(WorldActionTag, LegacyWorldActionID, TEXT("VHV.WorldAction"));
}

FName UVHVAuthoringReferenceLibrary::ResolveNPCActionID(const FGameplayTag NPCActionTag, const FName LegacyNPCActionID)
{
    return VHVAuthoringReferences::ResolveID(NPCActionTag, LegacyNPCActionID, TEXT("VHV.NPCAction"));
}

FName UVHVAuthoringReferenceLibrary::ResolveActivityID(const FGameplayTag ActivityTag, const FName LegacyActivityID)
{
    return VHVAuthoringReferences::ResolveID(ActivityTag, LegacyActivityID, TEXT("VHV.Activity"));
}

FName UVHVAuthoringReferenceLibrary::ResolveStoryStateID(const FGameplayTag StoryStateTag, const FName LegacyStoryStateID)
{
    if (VHVAuthoringReferences::IsConcreteTagInCategory(StoryStateTag, TEXT("VHV.Story.Flag")))
    {
        return VHVAuthoringReferences::ResolveID(StoryStateTag, LegacyStoryStateID, TEXT("VHV.Story.Flag"));
    }

    return VHVAuthoringReferences::ResolveID(StoryStateTag, LegacyStoryStateID, TEXT("VHV.Story.Counter"));
}
