#include "Core/VHVAuthoringReferences.h"

#if WITH_EDITOR
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#endif

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

#if WITH_EDITOR
bool UVHVAuthoringReferenceLibrary::ReadSerializedEnumProperty(
    const FString& ScriptStructPath,
    const FString& SerializedStruct,
    const FName PropertyName,
    FString& OutDeclaredEnumType,
    int64& OutNumericValue,
    FString& OutError)
{
    OutDeclaredEnumType.Reset();
    OutNumericValue = 0;
    OutError.Reset();

    UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, *ScriptStructPath);
    if (!ScriptStruct)
    {
        OutError = FString::Printf(TEXT("Script struct was not loaded: %s"), *ScriptStructPath);
        return false;
    }

    const FProperty* Property = FindFProperty<FProperty>(ScriptStruct, PropertyName);
    if (!Property)
    {
        OutError = FString::Printf(
            TEXT("Property '%s' was not found on %s"), *PropertyName.ToString(), *ScriptStructPath);
        return false;
    }

    FStructOnScope StructCopy(ScriptStruct);
    if (!ScriptStruct->ImportText(
            *SerializedStruct,
            StructCopy.GetStructMemory(),
            nullptr,
            PPF_None,
            GWarn,
            ScriptStruct->GetName()))
    {
        OutError = FString::Printf(TEXT("Native import failed for %s"), *ScriptStructPath);
        return false;
    }

    const void* ValueAddress = Property->ContainerPtrToValuePtr<void>(StructCopy.GetStructMemory());
    if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
    {
        const FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
        const UEnum* Enum = EnumProperty->GetEnum();
        if (!UnderlyingProperty || !Enum)
        {
            OutError = FString::Printf(
                TEXT("Enum property '%s' has no underlying numeric property or UEnum"),
                *PropertyName.ToString());
            return false;
        }

        OutDeclaredEnumType = FString::Printf(
            TEXT("%s[%s]"), *Enum->GetPathName(), *UnderlyingProperty->GetClass()->GetName());

        const bool bUnsignedUnderlying =
            CastField<FByteProperty>(UnderlyingProperty) != nullptr
            || CastField<FUInt16Property>(UnderlyingProperty) != nullptr
            || CastField<FUInt32Property>(UnderlyingProperty) != nullptr
            || CastField<FUInt64Property>(UnderlyingProperty) != nullptr;
        const bool bSignedUnderlying =
            CastField<FInt8Property>(UnderlyingProperty) != nullptr
            || CastField<FInt16Property>(UnderlyingProperty) != nullptr
            || CastField<FIntProperty>(UnderlyingProperty) != nullptr
            || CastField<FInt64Property>(UnderlyingProperty) != nullptr;

        if (bUnsignedUnderlying)
        {
            const uint64 UnsignedValue = UnderlyingProperty->GetUnsignedIntPropertyValue(ValueAddress);
            if (UnsignedValue > static_cast<uint64>(MAX_int64))
            {
                OutError = FString::Printf(
                    TEXT("Enum property '%s' exceeds the supported signed 64-bit snapshot range"),
                    *PropertyName.ToString());
                return false;
            }
            OutNumericValue = static_cast<int64>(UnsignedValue);
        }
        else if (bSignedUnderlying)
        {
            OutNumericValue = UnderlyingProperty->GetSignedIntPropertyValue(ValueAddress);
        }
        else
        {
            OutError = FString::Printf(
                TEXT("Enum property '%s' uses unsupported underlying numeric type %s"),
                *PropertyName.ToString(), *UnderlyingProperty->GetClass()->GetName());
            return false;
        }
        return true;
    }

    if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
    {
        const UEnum* Enum = ByteProperty->Enum;
        if (!Enum)
        {
            OutError = FString::Printf(
                TEXT("Byte property '%s' is not enum-backed"), *PropertyName.ToString());
            return false;
        }

        OutDeclaredEnumType = FString::Printf(
            TEXT("%s[%s]"), *Enum->GetPathName(), *ByteProperty->GetClass()->GetName());
        OutNumericValue = static_cast<int64>(ByteProperty->GetPropertyValue(ValueAddress));
        return true;
    }

    OutError = FString::Printf(
        TEXT("Property '%s' uses unsupported reflected type %s; expected FEnumProperty or enum-backed FByteProperty"),
        *PropertyName.ToString(), *Property->GetClass()->GetName());
    return false;
}
#endif
