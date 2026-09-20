#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VHVMainMenuGameMode.generated.h"

class UVHVMainMenuWidget;

/** Lightweight startup world owner. It intentionally creates no gameplay pawn or quest controller. */
UCLASS()
class VHV_API AVHVMainMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AVHVMainMenuGameMode();

protected:
    virtual void BeginPlay() override;

private:
    void RestoreMenuFocus();

    UPROPERTY(EditDefaultsOnly, Category = "VHV|Main Menu")
    TSubclassOf<UVHVMainMenuWidget> MainMenuWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "VHV|Main Menu")
    FName GameplayMapName = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson");

    UPROPERTY(Transient)
    TObjectPtr<UVHVMainMenuWidget> MainMenuWidget;
};
