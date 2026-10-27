#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VHVFrontEndSubsystem.generated.h"

UENUM()
enum class EVHVFrontEndLaunchMode : uint8
{
    None,
    NewGame,
    TechniquePractice
};

/** Transient front-end travel requests plus small, user-specific menu preferences. */
UCLASS()
class VHV_API UVHVFrontEndSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    void PrepareNewGame();
    bool PrepareTechniquePractice(FName QuestID);
    EVHVFrontEndLaunchMode ConsumeLaunchRequest(FName& OutTechniqueQuestID);

    bool IsTechniquePracticeActive() const { return bTechniquePracticeActive; }
    void SetTechniquePracticeActive(bool bActive) { bTechniquePracticeActive = bActive; }

    bool ShouldShowTutorialOnNewGame() const { return bShowTutorialOnNewGame; }
    void SetShowTutorialOnNewGame(bool bEnabled);
    bool HasCompletedTutorial() const { return bTutorialCompleted; }
    void MarkTutorialCompleted();

private:
    void SavePreferences() const;

    EVHVFrontEndLaunchMode PendingLaunchMode = EVHVFrontEndLaunchMode::None;
    FName PendingTechniqueQuestID;
    bool bTechniquePracticeActive = false;
    bool bShowTutorialOnNewGame = true;
    bool bTutorialCompleted = false;
};
