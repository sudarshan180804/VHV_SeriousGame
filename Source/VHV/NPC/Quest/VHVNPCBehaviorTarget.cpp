#include "NPC/Quest/VHVNPCBehaviorTarget.h"

#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "VHV.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

AVHVNPCBehaviorTarget::AVHVNPCBehaviorTarget()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AVHVNPCBehaviorTarget::BeginPlay()
{
	Super::BeginPlay();
	if (GetEffectiveTargetID().IsNone())
	{
		UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Behavior target '%s' has an empty Target ID."), *GetNameSafe(this));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UVHVQuestSubsystem* QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>())
			{
				QuestSubsystem->RegisterNPCBehaviorTarget(this);
			}
		}
	}
}

void AVHVNPCBehaviorTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UVHVQuestSubsystem* QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>())
			{
				QuestSubsystem->UnregisterNPCBehaviorTarget(this);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

#if WITH_EDITOR
EDataValidationResult AVHVNPCBehaviorTarget::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (GetEffectiveTargetID().IsNone())
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("NPC behavior target '%s' has no effective Target ID."), *GetNameSafe(this))));
		Result = EDataValidationResult::Invalid;
	}
	if (!VHVAuthoringReferences::IsValidReferenceTag(TargetTag, TEXT("VHV.BehaviorTarget")))
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("NPC behavior target '%s' uses tag '%s', which must be a concrete tag beneath VHV.BehaviorTarget."), *GetNameSafe(this), *TargetTag.ToString())));
		Result = EDataValidationResult::Invalid;
	}
	if (VHVAuthoringReferences::HasConflict(TargetTag, TargetID, TEXT("VHV.BehaviorTarget")))
	{
		Context.AddWarning(FText::FromString(FString::Printf(TEXT("NPC behavior target '%s' has tag '%s', which resolves to '%s', while legacy TargetID is '%s'; the tag wins."), *GetNameSafe(this), *TargetTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(TargetTag).ToString(), *TargetID.ToString())));
	}
	return Result;
}
#endif
