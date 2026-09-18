#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VHVNPCActionSet.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct VHV_API FVHVNPCPresentationAction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    FName ActionID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    TObjectPtr<UAnimMontage> Montage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0.01"))
    float PlayRate = 1.0f;
};

UCLASS(BlueprintType)
class VHV_API UVHVNPCActionSet : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    TArray<FVHVNPCPresentationAction> Actions;

    UFUNCTION(BlueprintPure, Category = "VHV|NPC|Presentation")
    bool GetAction(FName ActionID, FVHVNPCPresentationAction& OutAction) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
