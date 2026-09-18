#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Save/VHVSaveTypes.h"
#include "VHVSaveGame.generated.h"

UCLASS()
class VHV_API UVHVSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "VHV|Save")
    int32 SaveVersion = 1;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "VHV|Save")
    FString SavedMapName;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "VHV|Save")
    FVHVQuestSaveState QuestState;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "VHV|Save")
    FVHVStoryStateSaveState StoryState;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "VHV|Save")
    FVHVTextbookSaveState TextbookState;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "VHV|Save")
    TArray<FVHVDialogueCheckpointSaveState> DialogueCheckpoints;
};
