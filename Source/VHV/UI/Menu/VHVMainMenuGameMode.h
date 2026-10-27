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
    FName GameplayMapName = TEXT("/Game/VHV_Stuff/Maps/Lvl_Village_Main");

    UPROPERTY(Transient)
    TObjectPtr<UVHVMainMenuWidget> MainMenuWidget;
};
