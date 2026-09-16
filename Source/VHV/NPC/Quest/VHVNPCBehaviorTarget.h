#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VHVNPCBehaviorTarget.generated.h"

class USceneComponent;

UCLASS(Blueprintable)
class VHV_API AVHVNPCBehaviorTarget : public AActor
{
	GENERATED_BODY()

public:
	AVHVNPCBehaviorTarget();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|NPC|Quest")
	FName TargetID;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(VisibleAnywhere, Category = "VHV|NPC|Quest", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;
};
