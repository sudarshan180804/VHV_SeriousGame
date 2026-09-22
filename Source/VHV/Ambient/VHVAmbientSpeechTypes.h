#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Story/Types/VHVStoryStateTypes.h"
#include "VHVAmbientSpeechTypes.generated.h"

UENUM(BlueprintType)
enum class EVHVAmbientSpeechType : uint8
{
    Speech UMETA(DisplayName = "Speech"),
    Thought UMETA(DisplayName = "Thought")
};

UENUM(BlueprintType)
enum class EVHVAmbientConversationLeavePolicy : uint8
{
    Continue UMETA(DisplayName = "Continue"),
    Pause UMETA(DisplayName = "Pause"),
    Cancel UMETA(DisplayName = "Cancel")
};

USTRUCT(BlueprintType)
struct VHV_API FVHVAmbientParticipantDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Participant")
    FName SlotID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Participant")
    FText SpeakerName;
};

USTRUCT(BlueprintType)
struct VHV_API FVHVAmbientSpeechLine
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line")
    FName SpeakerSlot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line", meta = (MultiLine = "true"))
    FText Text;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line")
    EVHVAmbientSpeechType SpeechType = EVHVAmbientSpeechType::Speech;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0", Units = "Seconds"))
    float DelayBefore = 0.0f;

    /** Values at or below zero use the automatic text-length timing rule. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (Units = "Seconds"))
    float DisplayDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    bool bKeepPreviousBubbleVisible = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
    bool bLookAtOtherParticipant = false;

    /** Optional montage/action from the speaker's existing VHV NPC Action Set. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (DisplayName = "NPC Action", Categories = "VHV.NPCAction"))
    FGameplayTag NPCActionTag;

    /** Optional semantic presentation hint for Blueprint extensions. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (Categories = "VHV.Presentation"))
    FGameplayTag PresentationTag;
};

