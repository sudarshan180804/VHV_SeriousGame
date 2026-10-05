#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHVMainMenuWidget.generated.h"

class SBorder;
class SImage;
class STextBlock;
class UAudioComponent;
class USoundBase;
class UTexture2D;

/** Focused, self-contained presentation and input behavior for the startup menu. */
UCLASS()
class VHV_API UVHVMainMenuWidget : public UVHVUserWidgetBase
{
    GENERATED_BODY()

public:
    UVHVMainMenuWidget();

    void InitializeMenu(FName InGameplayMapName);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    enum class EMenuOption : uint8
    {
        Play,
        Continue,
        Quit,
        Count
    };

    void RefreshSaveAvailability();
    void MoveSelection(int32 Direction);
    void SetSelectedOption(int32 Index, bool bPlaySound);
    void ActivateSelectedOption();
    void ActivateOption(int32 Index);
    void ShowNewGameConfirmation();
    void HideNewGameConfirmation();
    void SetConfirmationSelection(int32 Index, bool bPlaySound);
    void StartNewGame();
    void ContinueGame();
    void QuitGame();
    void PrepareForTravel();
    void PlayFocusSound() const;
    void PlayConfirmSound() const;
    void HandleMenuHovered(int32 Index);
    FReply HandleMenuClicked(int32 Index);
    void HandleConfirmationHovered(int32 Index);
    FReply HandleConfirmationClicked(int32 Index);

    FName GameplayMapName = TEXT("/Game/VHV_Stuff/Maps/Lvl_Village_Main");
    int32 SelectedOption = 0;
    int32 ConfirmationSelection = 0;
    bool bSaveSlotExists = false;
    bool bContinueEnabled = false;
    bool bConfirmationVisible = false;
    float EntranceElapsed = 0.0f;
    TArray<float> SelectionEmphasis;
    TArray<float> ConfirmationEmphasis;

    TArray<TSharedPtr<SBorder>> MenuAccents;
    TArray<TSharedPtr<STextBlock>> MenuLabels;
    TArray<TSharedPtr<SWidget>> MenuRows;
    TArray<TSharedPtr<STextBlock>> ConfirmationLabels;
    TSharedPtr<SImage> BackgroundImage;
    TSharedPtr<SWidget> MenuPanel;
    TSharedPtr<SWidget> ConfirmationPanel;
    FSlateBrush BackgroundBrush;
    FSlateBrush AccentBrush;
    FSlateBrush ConfirmationPanelBrush;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> BackgroundTexture;

    UPROPERTY(EditDefaultsOnly, Category = "VHV|Main Menu|Audio")
    TObjectPtr<USoundBase> MenuAmbience;

    UPROPERTY(EditDefaultsOnly, Category = "VHV|Main Menu|Audio")
    TObjectPtr<USoundBase> FocusSound;

    UPROPERTY(EditDefaultsOnly, Category = "VHV|Main Menu|Audio")
    TObjectPtr<USoundBase> ConfirmSound;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> MenuAmbienceComponent;
};
