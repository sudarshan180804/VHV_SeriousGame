#include "VHVLevelData.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UVHVLevelData::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const FTopicData& Topic : DayData.Topics)
	{
		for (const FTextbookActivityData& Activity : Topic.Activities)
		{
			auto ValidateEffects = [&Context, &Result, &Activity](const TArray<FVHVStoryEffect>& Effects, const TCHAR* EffectArrayName)
			{
				for (int32 EffectIndex = 0; EffectIndex < Effects.Num(); ++EffectIndex)
				{
					if (Effects[EffectIndex].StateID.IsNone())
					{
						Context.AddError(FText::FromString(FString::Printf(
							TEXT("Activity '%s' has an invalid %s effect at index %d: StateID must not be NAME_None."),
							*Activity.ActivityID, EffectArrayName, EffectIndex)));
						Result = EDataValidationResult::Invalid;
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
