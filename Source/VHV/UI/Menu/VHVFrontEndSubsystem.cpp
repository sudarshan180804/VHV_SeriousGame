#include "UI/Menu/VHVFrontEndSubsystem.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
    const TCHAR* FrontEndSettingsSection = TEXT("/Script/VHV.VHVFrontEndSettings");
}

void UVHVFrontEndSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    GConfig->GetBool(FrontEndSettingsSection, TEXT("ShowTutorialOnNewGame"),
        bShowTutorialOnNewGame, GGameUserSettingsIni);
    GConfig->GetBool(FrontEndSettingsSection, TEXT("TutorialCompleted"),
        bTutorialCompleted, GGameUserSettingsIni);
}

void UVHVFrontEndSubsystem::PrepareNewGame()
{
    PendingLaunchMode = EVHVFrontEndLaunchMode::NewGame;
    PendingTechniqueQuestID = NAME_None;
    bTechniquePracticeActive = false;
}

bool UVHVFrontEndSubsystem::PrepareTechniquePractice(const FName QuestID)
{
    if (QuestID.IsNone())
    {
        return false;
    }
    PendingLaunchMode = EVHVFrontEndLaunchMode::TechniquePractice;
    PendingTechniqueQuestID = QuestID;
    bTechniquePracticeActive = true;
    return true;
}

EVHVFrontEndLaunchMode UVHVFrontEndSubsystem::ConsumeLaunchRequest(FName& OutTechniqueQuestID)
{
    const EVHVFrontEndLaunchMode Result = PendingLaunchMode;
    OutTechniqueQuestID = PendingTechniqueQuestID;
    PendingLaunchMode = EVHVFrontEndLaunchMode::None;
    PendingTechniqueQuestID = NAME_None;
    return Result;
}

void UVHVFrontEndSubsystem::SetShowTutorialOnNewGame(const bool bEnabled)
{
    bShowTutorialOnNewGame = bEnabled;
    SavePreferences();
}

void UVHVFrontEndSubsystem::MarkTutorialCompleted()
{
    bTutorialCompleted = true;
    SavePreferences();
}

void UVHVFrontEndSubsystem::SavePreferences() const
{
    GConfig->SetBool(FrontEndSettingsSection, TEXT("ShowTutorialOnNewGame"),
        bShowTutorialOnNewGame, GGameUserSettingsIni);
    GConfig->SetBool(FrontEndSettingsSection, TEXT("TutorialCompleted"),
        bTutorialCompleted, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}
