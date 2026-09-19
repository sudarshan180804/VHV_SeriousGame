#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VHVAuthoringReferences.generated.h"

namespace VHVAuthoringReferences
{
    VHV_API FName ResolveTagLeaf(const FGameplayTag& Tag);
    VHV_API FName ResolveID(const FGameplayTag& Tag, FName LegacyID, const TCHAR* ExpectedCategory);
    VHV_API FString ResolveStringID(const FGameplayTag& Tag, const FString& LegacyID, const TCHAR* ExpectedCategory);
    VHV_API bool HasConflict(const FGameplayTag& Tag, FName LegacyID, const TCHAR* ExpectedCategory);
    VHV_API bool HasConflict(const FGameplayTag& Tag, const FString& LegacyID, const TCHAR* ExpectedCategory);
    VHV_API bool IsConcreteTagInCategory(const FGameplayTag& Tag, const TCHAR* ExpectedCategory);
    VHV_API bool IsValidReferenceTag(const FGameplayTag& Tag, const TCHAR* ExpectedCategory);
}

UCLASS()
class VHV_API UVHVAuthoringReferenceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "VHV|Authoring References")
    static FName ResolveParticipantID(FGameplayTag ParticipantTag, FName LegacyParticipantID);

    UFUNCTION(BlueprintPure, Category = "VHV|Authoring References")
    static FName ResolveLocationID(FGameplayTag LocationTag, FName LegacyLocationID);

    UFUNCTION(BlueprintPure, Category = "VHV|Authoring References")
    static FName ResolveBehaviorTargetID(FGameplayTag BehaviorTargetTag, FName LegacyBehaviorTargetID);

    UFUNCTION(BlueprintPure, Category = "VHV|Authoring References")
    static FName ResolveWorldReceiverID(FGameplayTag WorldReceiverTag, FName LegacyWorldReceiverID);

    UFUNCTION(BlueprintPure, Category = "VHV|Authoring References")
    static FName ResolveWorldActionID(FGameplayTag WorldActionTag, FName LegacyWorldActionID);

    UFUNCTION(BlueprintPure, Category = "VHV|Authoring References")
    static FName ResolveNPCActionID(FGameplayTag NPCActionTag, FName LegacyNPCActionID);

    UFUNCTION(BlueprintPure, Category = "VHV|Authoring References")
    static FName ResolveActivityID(FGameplayTag ActivityTag, FName LegacyActivityID);

    UFUNCTION(BlueprintPure, Category = "VHV|Authoring References")
    static FName ResolveStoryStateID(FGameplayTag StoryStateTag, FName LegacyStoryStateID);
};
