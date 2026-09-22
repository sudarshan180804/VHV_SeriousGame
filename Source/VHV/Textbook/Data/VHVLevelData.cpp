#include "VHVLevelData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UVHVLevelData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TSet<FString> ActivityIDs;

	for (const FTopicData& Topic : DayData.Topics)
	{
		for (const FTextbookActivityData& Activity : Topic.Activities)
		{
			const FString EffectiveActivityID = Activity.GetEffectiveActivityID();
			if (EffectiveActivityID.IsEmpty())
			{
				Context.AddError(FText::FromString(TEXT("A textbook activity has no effective Activity ID.")));
				Result = EDataValidationResult::Invalid;
			}
			else if (ActivityIDs.Contains(EffectiveActivityID))
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("Level Data '%s' has duplicate effective Activity ID '%s'."), *GetName(), *EffectiveActivityID)));
				Result = EDataValidationResult::Invalid;
			}
			ActivityIDs.Add(EffectiveActivityID);
			if (!VHVAuthoringReferences::IsValidReferenceTag(Activity.ActivityTag, TEXT("VHV.Activity")))
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("Activity '%s' uses tag '%s', which must be a concrete tag beneath VHV.Activity."), *EffectiveActivityID, *Activity.ActivityTag.ToString())));
				Result = EDataValidationResult::Invalid;
			}
			if (VHVAuthoringReferences::HasConflict(Activity.ActivityTag, Activity.ActivityID, TEXT("VHV.Activity")))
			{
				Context.AddWarning(FText::FromString(FString::Printf(TEXT("Activity '%s' has tag '%s', which resolves to '%s', while legacy ActivityID is '%s'; the tag wins."), *EffectiveActivityID, *Activity.ActivityTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Activity.ActivityTag).ToString(), *Activity.ActivityID)));
			}

			if (Activity.AttemptPolicy.bEnabled)
			{
				const bool bChoiceActivity = Activity.ActivityType == ETextbookActivityType::SingleChoice
					|| Activity.ActivityType == ETextbookActivityType::MultiChoice;
				if (!bChoiceActivity)
				{
					Context.AddError(FText::FromString(FString::Printf(
						TEXT("Activity '%s' enables AttemptPolicy but is not SingleChoice or MultiChoice."),
						*EffectiveActivityID)));
					Result = EDataValidationResult::Invalid;
				}
				if (Activity.AttemptPolicy.MaxAttempts < 1)
				{
					Context.AddError(FText::FromString(FString::Printf(
						TEXT("Activity '%s' has AttemptPolicy.MaxAttempts below 1."), *EffectiveActivityID)));
					Result = EDataValidationResult::Invalid;
				}
			}

			auto ValidateEffects = [&Context, &Result, &Activity](const TArray<FVHVStoryEffect>& Effects, const TCHAR* EffectArrayName)
			{
				for (int32 EffectIndex = 0; EffectIndex < Effects.Num(); ++EffectIndex)
				{
					const FVHVStoryEffect& Effect = Effects[EffectIndex];
					if (Effect.GetEffectiveStateID().IsNone())
					{
						Context.AddError(FText::FromString(FString::Printf(
							TEXT("Activity '%s' has an invalid %s effect at index %d: StateID must not be NAME_None."),
							*Activity.GetEffectiveActivityID(), EffectArrayName, EffectIndex)));
						Result = EDataValidationResult::Invalid;
					}
					const bool bFlag = Effect.EffectType == EVHVStoryEffectType::SetFlag || Effect.EffectType == EVHVStoryEffectType::ClearFlag;
					const TCHAR* ExpectedCategory = bFlag ? TEXT("VHV.Story.Flag") : TEXT("VHV.Story.Counter");
					if (!VHVAuthoringReferences::IsValidReferenceTag(Effect.StateTag, ExpectedCategory))
					{
						Context.AddError(FText::FromString(FString::Printf(TEXT("Activity '%s' %s effect at index %d uses tag '%s', which must be a concrete tag beneath %s."), *Activity.GetEffectiveActivityID(), EffectArrayName, EffectIndex, *Effect.StateTag.ToString(), ExpectedCategory)));
						Result = EDataValidationResult::Invalid;
					}
					if (VHVAuthoringReferences::HasConflict(Effect.StateTag, Effect.StateID, ExpectedCategory))
					{
						Context.AddWarning(FText::FromString(FString::Printf(TEXT("Activity '%s' %s effect at index %d has tag '%s', which resolves to '%s', while legacy StateID is '%s'; the tag wins."), *Activity.GetEffectiveActivityID(), EffectArrayName, EffectIndex, *Effect.StateTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(Effect.StateTag).ToString(), *Effect.StateID.ToString())));
					}
				}
			};

			ValidateEffects(Activity.SuccessEffects, TEXT("SuccessEffects"));
			ValidateEffects(Activity.FailureEffects, TEXT("FailureEffects"));
		}
	}

	return Result;
}
#endif
