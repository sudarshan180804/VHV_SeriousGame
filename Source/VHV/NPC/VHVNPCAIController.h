#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "VHVNPCAIController.generated.h"

UCLASS()
class VHV_API AVHVNPCAIController : public AAIController
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|AI")
	void StopForInteraction();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|AI")
	void FaceActor(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|AI")
	void ClearInteractionFocus();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|AI")
	void ResumeBehavior();
};
