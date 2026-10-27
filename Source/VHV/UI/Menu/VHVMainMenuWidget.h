#pragma once

#include "CoreMinimal.h"
#include "UI/VHVUserWidgetBase.h"
#include "VHVMainMenuWidget.generated.h"

class SBorder;
class SImage;
class SSlider;
class STextBlock;
class UTexture2D;

/** Native, packaged-game front end built on the existing VHV save/quest/audio systems. */
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
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
    enum class EMenuOption : uint8 { NewGame, Continue, Techniques, Tutorial, Settings, Quit, Count };
    enum class EPage : uint8 { Main, Techniques, Tutorial, Settings };

    void RefreshSaveAvailability();
    void ShowPage(EPage Page);
    void MoveSelection(int32 Direction);
    void SetSelectedOption(int32 Index, bool bPlaySound);
    void ActivateSelectedOption();
    void ActivateOption(int32 Index);
    void ShowNewGameConfirmation();
    void HideNewGameConfirmation();
    void SetConfirmationSelection(int32 Index, bool bPlaySound);
    void StartNewGame();
    void BeginNewGameTravel();
    void ContinueGame();
    void QuitGame();
    void PrepareForTravel();
    void PlayFocusSound() const;
    void PlayConfirmSound() const;

    void SetTechniqueSelection(int32 Index);
    void StartSelectedTechnique();
    void SetTutorialPage(int32 PageIndex);
    void CloseTutorial(bool bStartGame);
    void RefreshSettingsLabels();
    void HandleMusicVolumeChanged(float Value);
    void ToggleTutorialPreference();

    void HandleMenuHovered(int32 Index);
    FReply HandleMenuClicked(int32 Index);
    void HandleConfirmationHovered(int32 Index);
    FReply HandleConfirmationClicked(int32 Index);
    FReply HandleTechniqueClicked(int32 Index);
    FReply HandleBackClicked();
    FReply HandleTutorialPrevious();
    FReply HandleTutorialNext();
    FReply HandleTutorialSkip();
    FReply HandleTutorialPreferenceClicked();

    FName GameplayMapName = TEXT("/Game/VHV_Stuff/Maps/Lvl_Village_Main");
    EPage CurrentPage = EPage::Main;
    int32 SelectedOption = 0;
    int32 ConfirmationSelection = 0;
    int32 TechniqueSelection = 0;
    int32 TutorialPageIndex = 0;
    int32 SettingsSelection = 0;
    bool bSaveSlotExists = false;
    bool bContinueEnabled = false;
    bool bConfirmationVisible = false;
    bool bTutorialLaunchingNewGame = false;
    float EntranceElapsed = 0.0f;
    TArray<float> SelectionEmphasis;
    TArray<float> ConfirmationEmphasis;
    TArray<FName> TechniqueQuestIDs;

    TArray<TSharedPtr<SBorder>> MenuAccents;
    TArray<TSharedPtr<STextBlock>> MenuLabels;
    TArray<TSharedPtr<SWidget>> MenuRows;
    TArray<TSharedPtr<STextBlock>> ConfirmationLabels;
    TArray<TSharedPtr<STextBlock>> TechniqueLabels;
    TSharedPtr<SImage> BackgroundImage;
    TSharedPtr<SWidget> MenuPanel;
    TSharedPtr<SWidget> ConfirmationPanel;
    TSharedPtr<SWidget> TechniquesPanel;
    TSharedPtr<SWidget> TutorialPanel;
    TSharedPtr<SWidget> SettingsPanel;
    TSharedPtr<STextBlock> TutorialTitle;
    TSharedPtr<STextBlock> TutorialBody;
    TSharedPtr<STextBlock> TutorialImageLabel;
    TSharedPtr<SImage> TutorialImage;
    TSharedPtr<STextBlock> TutorialNextLabel;
    TSharedPtr<STextBlock> MusicVolumeValue;
    TSharedPtr<STextBlock> TutorialPreferenceValue;
    TSharedPtr<SSlider> MusicSlider;

    FSlateBrush BackgroundBrush;
    FSlateBrush AccentBrush;
    FSlateBrush PanelBrush;
    FSlateBrush PlaceholderBrush;
    FSlateBrush TutorialImageBrush;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> BackgroundTexture;

    /** Import final images at these defaults; the widget picks them up without logic changes. */
    UPROPERTY(EditDefaultsOnly, Category = "VHV|Tutorial")
    TSoftObjectPtr<UTexture2D> NavigationTutorialImage;

    UPROPERTY(EditDefaultsOnly, Category = "VHV|Tutorial")
    TSoftObjectPtr<UTexture2D> InteractionTutorialImage;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> ActiveTutorialTexture;
};
