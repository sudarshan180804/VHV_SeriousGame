#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VHVQuestParticipantComponent.generated.h"

UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVQuestParticipantComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UVHVQuestParticipantComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|Quest")
    FName ParticipantID;

    UFUNCTION(BlueprintCallable, Category = "VHV|Quest")
    bool NotifyInteracted();
};
