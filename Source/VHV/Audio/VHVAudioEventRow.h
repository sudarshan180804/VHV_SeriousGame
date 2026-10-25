#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "VHVAudioEventRow.generated.h"

/** Manifest-backed metadata for one reusable VHV audio event. */
USTRUCT(BlueprintType)
struct VHV_API FVHVAudioEventRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString AssetID;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString UE_AssetName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Category;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Folder;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString RelativeWAVPath;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Priority;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float TargetDuration_s = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int64 ExactSamples_48kHz = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 SampleRate_Hz = 48000;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 BitDepth = 16;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString Channels;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool Looping = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString UE_SoundClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString RandomPitchRange;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString RandomVolumeRange;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float Peak_dBFS = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float RMS_dBFS = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FString TriggerDescription;
};
