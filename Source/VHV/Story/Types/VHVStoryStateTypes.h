#pragma once

#include "CoreMinimal.h"
#include "Core/VHVAuthoringReferences.h"
#include "VHVStoryStateTypes.generated.h"

UENUM(BlueprintType)
enum class EVHVStoryConditionType : uint8
{
    FlagSet,
    FlagNotSet,
    CounterEqual,
    CounterGreaterOrEqual,
    CounterLessOrEqual
};

UENUM(BlueprintType)
enum class EVHVStoryConditionMatch : uint8
{
    All,
    Any
};

USTRUCT(BlueprintType)
struct VHV_API FVHVStoryCondition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State")
    EVHVStoryConditionType ConditionType = EVHVStoryConditionType::FlagSet;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State", meta = (DisplayName = "Story State", Categories = "VHV.Story"))
    FGameplayTag StateTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "Story State", meta = (DisplayName = "State ID (Legacy)"))
    FName StateID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State", meta = (EditCondition = "ConditionType == EVHVStoryConditionType::CounterEqual || ConditionType == EVHVStoryConditionType::CounterGreaterOrEqual || ConditionType == EVHVStoryConditionType::CounterLessOrEqual", EditConditionHides))
    int32 CompareValue = 0;

    FName GetEffectiveStateID() const
    {
        const TCHAR* ExpectedCategory = ConditionType == EVHVStoryConditionType::FlagSet || ConditionType == EVHVStoryConditionType::FlagNotSet
            ? TEXT("VHV.Story.Flag")
            : TEXT("VHV.Story.Counter");
        return VHVAuthoringReferences::ResolveID(StateTag, StateID, ExpectedCategory);
    }
};

USTRUCT(BlueprintType)
struct VHV_API FVHVStoryConditionSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State")
    EVHVStoryConditionMatch MatchMode = EVHVStoryConditionMatch::All;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State")
    TArray<FVHVStoryCondition> Conditions;
};

UENUM(BlueprintType)
enum class EVHVStoryEffectType : uint8
{
    SetFlag,
    ClearFlag,
    SetCounter,
    AddCounter
};

USTRUCT(BlueprintType)
struct VHV_API FVHVStoryEffect
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State")
    EVHVStoryEffectType EffectType = EVHVStoryEffectType::SetFlag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State", meta = (DisplayName = "Story State", Categories = "VHV.Story"))
    FGameplayTag StateTag;

    // Legacy serialized fallback. Hidden from authoring; do not remove until old assets are fully migrated.
    UPROPERTY(BlueprintReadOnly, Category = "Story State", meta = (DisplayName = "State ID (Legacy)"))
    FName StateID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Story State", meta = (EditCondition = "EffectType == EVHVStoryEffectType::SetCounter || EffectType == EVHVStoryEffectType::AddCounter", EditConditionHides))
    int32 Value = 0;

    FName GetEffectiveStateID() const
    {
        const TCHAR* ExpectedCategory = EffectType == EVHVStoryEffectType::SetFlag || EffectType == EVHVStoryEffectType::ClearFlag
            ? TEXT("VHV.Story.Flag")
            : TEXT("VHV.Story.Counter");
        return VHVAuthoringReferences::ResolveID(StateTag, StateID, ExpectedCategory);
    }
};
