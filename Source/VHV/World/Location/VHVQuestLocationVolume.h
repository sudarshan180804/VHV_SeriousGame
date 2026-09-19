#pragma once

#include "CoreMinimal.h"
#include "Core/VHVAuthoringReferences.h"
#include "GameFramework/Actor.h"
#include "VHVQuestLocationVolume.generated.h"

class AVHVCharacter;
class UBoxComponent;
class UPrimitiveComponent;
class UVHVQuestSubsystem;
struct FHitResult;

UCLASS()
class VHV_API AVHVQuestLocationVolume : public AActor
{
    GENERATED_BODY()

public:
    AVHVQuestLocationVolume();

protected:
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

    UFUNCTION()
    void HandleBoxBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void HandleBoxEndOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex);

public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|Location")
    TObjectPtr<UBoxComponent> BoxComponent;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|Location", meta = (Categories = "VHV.Location"))
    FGameplayTag LocationTag;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "VHV|Location", meta = (AdvancedDisplay, DisplayName = "Location ID (Legacy)"))
    FName LocationID;

    UFUNCTION(BlueprintPure, Category = "VHV|Location")
    FName GetEffectiveLocationID() const { return VHVAuthoringReferences::ResolveID(LocationTag, LocationID, TEXT("VHV.Location")); }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHV|Location")
    bool bEnabled = true;

private:
    UPROPERTY(Transient)
    TObjectPtr<UVHVQuestSubsystem> QuestSubsystem;

    TSet<TWeakObjectPtr<AVHVCharacter>> PlayersInside;
};
