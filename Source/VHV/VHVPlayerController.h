// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "VHVPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UVHVUIManagerComponent;
class UVHVLevelData;
class UVHVQuestArcData;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class AVHVPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AVHVPlayerController();

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Dialogue input actions */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Dialogue")
	TObjectPtr<UInputAction> DialogueAdvanceAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Dialogue")
	TObjectPtr<UInputAction> DialogueConfirmAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Dialogue")
	TObjectPtr<UInputAction> DialogueChoiceUpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Dialogue")
	TObjectPtr<UInputAction> DialogueChoiceDownAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|Dialogue")
	TObjectPtr<UInputAction> DialogueExitAction;

	/** Player UI manager component (owns HUD widgets) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VHV|UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UVHVUIManagerComponent> UIManagerComponent;

	/** Starting level data for textbook initialization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHV|Textbook")
	TSoftObjectPtr<UVHVLevelData> StartingLevelData;

	/** Enables the opt-in quest orchestration path. Legacy textbook startup remains the default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHV|Quest")
	bool bUseQuestFlow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VHV|Quest", meta = (EditCondition = "bUseQuestFlow"))
	TSoftObjectPtr<UVHVQuestArcData> StartingQuestArc;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

};
