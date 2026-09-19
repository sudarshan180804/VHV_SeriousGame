#pragma once

#include "CoreMinimal.h"
#include "Core/VHVAuthoringReferences.h"
#include "GameFramework/Actor.h"
#include "VHVNPCBehaviorTarget.generated.h"

class USceneComponent;

UCLASS(Blueprintable)
class VHV_API AVHVNPCBehaviorTarget : public AActor
{
	GENERATED_BODY()

public:
	AVHVNPCBehaviorTarget();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|NPC|Quest", meta = (Categories = "VHV.BehaviorTarget"))
	FGameplayTag TargetTag;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|NPC|Quest", meta = (AdvancedDisplay, DisplayName = "Target ID (Legacy)"))
	FName TargetID;

	UFUNCTION(BlueprintPure, Category = "VHV|NPC|Quest")
	FName GetEffectiveTargetID() const { return VHVAuthoringReferences::ResolveID(TargetTag, TargetID, TEXT("VHV.BehaviorTarget")); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(VisibleAnywhere, Category = "VHV|NPC|Quest", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;
};
