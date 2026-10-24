#include "Debug/VHVDeveloperSubsystem.h"

#include "Ambient/VHVAmbientConversationActor.h"
#include "Ambient/VHVAmbientConversationData.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "NPC/VHVNPCCharacter.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "Save/VHVSaveSubsystem.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "UI/VHVUIManagerComponent.h"
#include "VHV.h"
#include "World/Systems/VHVWorldActionSubsystem.h"

#if !UE_BUILD_SHIPPING
namespace
{
    const TCHAR* BoolText(const bool bValue)
    {
        return bValue ? TEXT("true") : TEXT("false");
    }

    FString DiagnosticActorLabel(const AActor* Actor)
    {
        if (!Actor)
        {
            return TEXT("None");
        }
#if WITH_EDITOR
        return Actor->GetActorLabel();
#else
        return Actor->GetName();
#endif
    }

    bool IsNativeMeshVisible(const AVHVNPCCharacter* NPC)
    {
        const USkeletalMeshComponent* Mesh = NPC ? NPC->GetMesh() : nullptr;
        return IsValid(NPC) && !NPC->IsHidden() && IsValid(Mesh)
            && Mesh->IsRegistered() && Mesh->IsActive() && Mesh->IsVisible()
            && !Mesh->bHiddenInGame && Mesh->IsRenderStateCreated()
            && Mesh->GetSkeletalMeshAsset() != nullptr;
    }

    FString ExpectedMeshPathForSlot(const FName SlotID)
    {
        return SlotID == TEXT("Model")
            ? TEXT("/Game/Character/saeng/Saeng.Saeng")
            : TEXT("/Game/Character/somchai/SOMCHAI.SOMCHAI");
    }

    struct FTimelineCastDiagnostic : TSharedFromThis<FTimelineCastDiagnostic>
    {
        explicit FTimelineCastDiagnostic(UWorld* InWorld, const bool bInQuitWhenDone)
            : World(InWorld), bQuitWhenDone(bInQuitWhenDone)
        {
        }

        void Start()
        {
            static const TCHAR* OrderedLabels[] = {
                TEXT("HBCT_RoleModel_TimelineFindsModelScene"),
                TEXT("HBCT_RoleModel_TimelineRepetitionScene"),
                TEXT("HBCT_RoleModel_TimelineBeforeScene"),
                TEXT("HBCT_RoleModel_TimelineFirstAttemptScene"),
                TEXT("HBCT_RoleModel_TimelineNewRoutineScene")
            };

            TMap<FString, AVHVAmbientConversationActor*> ByLabel;
            for (TActorIterator<AVHVAmbientConversationActor> It(World.Get()); It; ++It)
            {
                AVHVAmbientConversationActor* Scene = *It;
                const FString Label = DiagnosticActorLabel(Scene);
                if (Label.StartsWith(TEXT("HBCT_RoleModel_Timeline")))
                {
                    Scene->StopConversation();
                    Scene->ResetConversation();
                    ByLabel.Add(Label, Scene);
                }
            }
            for (const TCHAR* Label : OrderedLabels)
            {
                if (AVHVAmbientConversationActor** Found = ByLabel.Find(Label))
                {
                    Scenes.Add(*Found);
                }
                else
                {
                    UE_LOG(LogVHV, Error, TEXT("[VHVTimelineCastDiagnostic] Missing scene '%s'."), Label);
                    bAllPassed = false;
                }
            }

            if (APawn* Pawn = World.IsValid() && World->GetFirstPlayerController()
                    ? World->GetFirstPlayerController()->GetPawn() : nullptr)
            {
                OriginalPawnTransform = Pawn->GetActorTransform();
                bHasOriginalPawnTransform = true;
            }

            UE_LOG(LogVHV, Log,
                TEXT("[VHVTimelineCastDiagnostic] BEGIN World='%s' SceneCount=%d (Timeline 2 and 4 execute first; completion effects disabled)."),
                *GetNameSafe(World.Get()), Scenes.Num());
            StartNextScene();
            FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateSP(AsShared(), &FTimelineCastDiagnostic::Tick), 0.1f);
        }

        bool Tick(float)
        {
            if (bFinished)
            {
                return false;
            }
            AVHVAmbientConversationActor* Scene = CurrentScene.Get();
            if (!Scene)
            {
                FinishCurrentScene(false, TEXT("scene instance became invalid"));
                return !bFinished;
            }

            const double Elapsed = FPlatformTime::Seconds() - SceneStartSeconds;
            const int32 CurrentLine = Scene->GetCurrentLineIndex();
            if (CurrentLine != LastLoggedLine && Scene->ConversationData
                && Scene->ConversationData->Lines.IsValidIndex(CurrentLine))
            {
                LastLoggedLine = CurrentLine;
                HighestLineSeen = FMath::Max(HighestLineSeen, CurrentLine);
                const FName ExpectedSlot = Scene->ConversationData->Lines[CurrentLine].SpeakerSlot;
                AVHVNPCCharacter* ExpectedSpeaker = Scene->GetResolvedParticipant(ExpectedSlot);
                AVHVNPCCharacter* ActualSpeaker = Scene->GetCurrentSpeaker();
                const bool bSpeakerMatches = IsValid(ExpectedSpeaker) && ActualSpeaker == ExpectedSpeaker;
                bCurrentPassed &= bSpeakerMatches;
                UE_LOG(LogVHV, Log,
                    TEXT("[VHVTimelineCastDiagnostic] SPEAKER Scene='%s' Line=%d Slot='%s' Expected='%s' Actual='%s' BubbleAnchorMatch=%s."),
                    *DiagnosticActorLabel(Scene), CurrentLine, *ExpectedSlot.ToString(),
                    *DiagnosticActorLabel(ExpectedSpeaker), *DiagnosticActorLabel(ActualSpeaker), BoolText(bSpeakerMatches));
            }

            if (Elapsed >= 0.75 && !bLoggedStableSample)
            {
                bLoggedStableSample = true;
                const bool bVisible = ValidateAndLogScene(TEXT("STABLE"), true);
                bSawStableVisibleCast |= bVisible;
                bCurrentPassed &= bVisible;
            }

            if (Scene->IsPlaying())
            {
                if (Elapsed > 30.0)
                {
                    Scene->StopConversation();
                    FinishCurrentScene(false, TEXT("bounded playback timeout"));
                }
                return !bFinished;
            }

            const int32 ExpectedLastLine = Scene->ConversationData
                ? Scene->ConversationData->Lines.Num() - 1 : INDEX_NONE;
            const bool bPlayedAllLines = ExpectedLastLine >= 0 && HighestLineSeen == ExpectedLastLine;
            const bool bCleanupValid = ValidateCleanup(Scene);
            bCurrentPassed &= bPlayedAllLines && bSawStableVisibleCast && bCleanupValid;
            FinishCurrentScene(bCurrentPassed, bPlayedAllLines
                ? TEXT("completed") : TEXT("conversation ended before all authored lines"));
            return !bFinished;
        }

        void StartNextScene()
        {
            ++SceneIndex;
            if (!Scenes.IsValidIndex(SceneIndex))
            {
                FinishAll();
                return;
            }

            AVHVAmbientConversationActor* Scene = Scenes[SceneIndex].Get();
            CurrentScene = Scene;
            bCurrentPassed = IsValid(Scene);
            bSawStableVisibleCast = false;
            bLoggedStableSample = false;
            HighestLineSeen = INDEX_NONE;
            LastLoggedLine = INDEX_NONE;
            SceneStartSeconds = FPlatformTime::Seconds();
            if (!Scene || !Scene->ConversationData)
            {
                FinishCurrentScene(false, TEXT("missing scene or conversation data"));
                return;
            }

            SavedLeavePolicy = Scene->PlayerLeavePolicy;
            Scene->PlayerLeavePolicy = EVHVAmbientConversationLeavePolicy::Continue;
            FocusPlayerOnScene(Scene);
            Scene->ResetConversation();
            const bool bStarted = Scene->StartConversationRange(0, Scene->ConversationData->Lines.Num());
            bCurrentPassed &= bStarted;
            UE_LOG(LogVHV, Log,
                TEXT("[VHVTimelineCastDiagnostic] START Scene='%s' Conversation='%s' Bindings=%d Started=%s."),
                *DiagnosticActorLabel(Scene), *Scene->ConversationData->ConversationID.ToString(),
                Scene->Participants.Num(), BoolText(bStarted));
            if (bStarted)
            {
                bCurrentPassed &= ValidateAndLogScene(TEXT("START"), false);
            }
            else
            {
                FinishCurrentScene(false, TEXT("StartConversationRange rejected the runtime cast"));
            }
        }

        bool ValidateAndLogScene(const TCHAR* Phase, const bool bCheckRendered)
        {
            AVHVAmbientConversationActor* Scene = CurrentScene.Get();
            if (!Scene || !Scene->ConversationData)
            {
                return false;
            }

            bool bPass = Scene->AreResolvedParticipantsVisible();
            const bool bExpectsTwo = DiagnosticActorLabel(Scene).Contains(TEXT("FindsModel"))
                || DiagnosticActorLabel(Scene).Contains(TEXT("Repetition"));
            bPass &= Scene->Participants.Num() == (bExpectsTwo ? 2 : 1);

            for (const FVHVAmbientParticipantBinding& Binding : Scene->Participants)
            {
                AVHVNPCCharacter* Resolved = Scene->GetResolvedParticipant(Binding.SlotID);
                USkeletalMeshComponent* Mesh = Resolved ? Resolved->GetMesh() : nullptr;
                const UVHVQuestParticipantComponent* Participant = Resolved
                    ? Resolved->GetQuestParticipantComponent() : nullptr;
                const FName ParticipantID = Participant ? Participant->GetEffectiveParticipantID() : NAME_None;
                const FString MeshPath = Mesh && Mesh->GetSkeletalMeshAsset()
                    ? Mesh->GetSkeletalMeshAsset()->GetPathName() : TEXT("None");
                const FString ExpectedMeshPath = ExpectedMeshPathForSlot(Binding.SlotID);
                const bool bCorrectInstance = IsValid(Resolved) && Resolved == Binding.NPC;
                const bool bNativeVisible = IsNativeMeshVisible(Resolved);
                const bool bMeshCorrect = MeshPath == ExpectedMeshPath;
                const bool bRendered = !bCheckRendered || (Mesh && Mesh->WasRecentlyRendered(1.0f));
                bPass &= bCorrectInstance && bNativeVisible && bMeshCorrect && bRendered;

                UE_LOG(LogVHV, Log,
                    TEXT("[VHVTimelineCastDiagnostic] %s Scene='%s' Slot='%s' Actor='%s' ParticipantID='%s' Instance='%s' Valid=%s Active=%s Location=%s ActorHidden=%s MeshVisible=%s MeshHiddenInGame=%s Registered=%s RenderState=%s RecentlyRendered=%s Mesh='%s' AnimClass='%s' Materials=%d ComponentLocation=%s BoundsOrigin=%s BoundsExtent=%s Result=%s."),
                    Phase, *DiagnosticActorLabel(Scene), *Binding.SlotID.ToString(),
                    *DiagnosticActorLabel(Binding.NPC), *ParticipantID.ToString(), *GetPathNameSafe(Resolved),
                    BoolText(IsValid(Resolved)), BoolText(Mesh && Mesh->IsActive()),
                    Resolved ? *Resolved->GetActorLocation().ToCompactString() : TEXT("None"),
                    BoolText(Resolved && Resolved->IsHidden()), BoolText(Mesh && Mesh->IsVisible()),
                    BoolText(Mesh && Mesh->bHiddenInGame), BoolText(Mesh && Mesh->IsRegistered()),
                    BoolText(Mesh && Mesh->IsRenderStateCreated()), BoolText(Mesh && Mesh->WasRecentlyRendered(1.0f)),
                    *MeshPath, Mesh ? *GetNameSafe(Mesh->GetAnimClass()) : TEXT("None"),
                    Mesh ? Mesh->GetNumMaterials() : 0,
                    Mesh ? *Mesh->GetComponentLocation().ToCompactString() : TEXT("None"),
                    Mesh ? *Mesh->Bounds.Origin.ToCompactString() : TEXT("None"),
                    Mesh ? *Mesh->Bounds.BoxExtent.ToCompactString() : TEXT("None"),
                    BoolText(bCorrectInstance && bNativeVisible && bMeshCorrect && bRendered));
            }

            int32 VisibleSomchai = 0;
            int32 VisibleSaeng = 0;
            TArray<FString> VisibleSomchaiActors;
            TArray<FString> VisibleSaengActors;
            CountVisibleIdentityActors(VisibleSomchai, VisibleSaeng, VisibleSomchaiActors, VisibleSaengActors);
            const bool bUnique = VisibleSomchai == 1 && VisibleSaeng <= 1
                && (!bExpectsTwo || VisibleSaeng == 1);
            bPass &= bUnique;
            UE_LOG(LogVHV, Log,
                TEXT("[VHVTimelineCastDiagnostic] %s Scene='%s' VisibleSomchai=%d [%s] VisibleSaeng=%d [%s] RequiredTogether=%s UniqueIdentityCast=%s."),
                Phase, *DiagnosticActorLabel(Scene), VisibleSomchai, *FString::Join(VisibleSomchaiActors, TEXT(",")),
                VisibleSaeng, *FString::Join(VisibleSaengActors, TEXT(",")), BoolText(bExpectsTwo), BoolText(bUnique));
            return bPass;
        }

        bool ValidateCleanup(AVHVAmbientConversationActor* Scene)
        {
            bool bPass = true;
            for (const FVHVAmbientParticipantBinding& Binding : Scene->Participants)
            {
                const AVHVNPCCharacter* NPC = Binding.NPC;
                const bool bHidden = IsValid(NPC) && NPC->IsHidden();
                bPass &= bHidden;
                UE_LOG(LogVHV, Log,
                    TEXT("[VHVTimelineCastDiagnostic] CLEANUP Scene='%s' Slot='%s' Actor='%s' ActorHidden=%s."),
                    *DiagnosticActorLabel(Scene), *Binding.SlotID.ToString(),
                    *DiagnosticActorLabel(NPC), BoolText(bHidden));
            }

            int32 VisibleSomchai = 0;
            int32 VisibleSaeng = 0;
            TArray<FString> SomchaiActors;
            TArray<FString> SaengActors;
            CountVisibleIdentityActors(VisibleSomchai, VisibleSaeng, SomchaiActors, SaengActors);
            const bool bNoDuplicates = VisibleSomchai <= 1 && VisibleSaeng <= 1;
            bPass &= bNoDuplicates;
            UE_LOG(LogVHV, Log,
                TEXT("[VHVTimelineCastDiagnostic] CLEANUP Scene='%s' VisibleSomchai=%d VisibleSaeng=%d NoDuplicates=%s."),
                *DiagnosticActorLabel(Scene), VisibleSomchai, VisibleSaeng, BoolText(bNoDuplicates));
            return bPass;
        }

        void CountVisibleIdentityActors(int32& OutSomchai, int32& OutSaeng,
            TArray<FString>& OutSomchaiActors, TArray<FString>& OutSaengActors) const
        {
            OutSomchai = 0;
            OutSaeng = 0;
            for (TActorIterator<AVHVNPCCharacter> It(World.Get()); It; ++It)
            {
                AVHVNPCCharacter* NPC = *It;
                const USkeletalMeshComponent* Mesh = NPC->GetMesh();
                if (!IsNativeMeshVisible(NPC) || !Mesh || !Mesh->GetSkeletalMeshAsset())
                {
                    continue;
                }
                const FString Label = DiagnosticActorLabel(NPC);
                const bool bTimelineActor = Label.StartsWith(TEXT("HBCT_RoleModel_Timeline"));
                const bool bSomchaiIdentity = Label == TEXT("HBCT_RoleModel_UncleSomchai")
                    || (bTimelineActor && Label.EndsWith(TEXT("_Target")));
                const bool bSaengIdentity = Label == TEXT("HBCT_Motivation_Saeng")
                    || (bTimelineActor && Label.EndsWith(TEXT("_Model")));
                if (bSomchaiIdentity)
                {
                    ++OutSomchai;
                    OutSomchaiActors.Add(Label);
                }
                else if (bSaengIdentity)
                {
                    ++OutSaeng;
                    OutSaengActors.Add(Label);
                }
            }
        }

        void FocusPlayerOnScene(AVHVAmbientConversationActor* Scene) const
        {
            APlayerController* Controller = World.IsValid() ? World->GetFirstPlayerController() : nullptr;
            APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
            if (!Pawn || Scene->Participants.IsEmpty())
            {
                return;
            }
            FVector Center = FVector::ZeroVector;
            int32 Count = 0;
            for (const FVHVAmbientParticipantBinding& Binding : Scene->Participants)
            {
                if (IsValid(Binding.NPC))
                {
                    Center += Binding.NPC->GetActorLocation();
                    ++Count;
                }
            }
            if (Count == 0)
            {
                return;
            }
            Center /= static_cast<float>(Count);
            const FVector ViewLocation = Center + FVector(-650.0f, 0.0f, 80.0f);
            Pawn->SetActorLocation(ViewLocation, false, nullptr, ETeleportType::TeleportPhysics);
            const FRotator ViewRotation = (Center + FVector(0.0f, 0.0f, 80.0f) - Pawn->GetPawnViewLocation()).Rotation();
            Controller->SetControlRotation(ViewRotation);
        }

        void FinishCurrentScene(const bool bPassed, const TCHAR* Reason)
        {
            AVHVAmbientConversationActor* Scene = CurrentScene.Get();
            if (Scene)
            {
                Scene->PlayerLeavePolicy = SavedLeavePolicy;
            }
            bAllPassed &= bPassed;
            if (bPassed)
            {
                UE_LOG(LogVHV, Log,
                    TEXT("[VHVTimelineCastDiagnostic] RESULT Scene='%s' Passed=true Reason='%s'."),
                    *DiagnosticActorLabel(Scene), Reason);
            }
            else
            {
                UE_LOG(LogVHV, Error,
                    TEXT("[VHVTimelineCastDiagnostic] RESULT Scene='%s' Passed=false Reason='%s'."),
                    *DiagnosticActorLabel(Scene), Reason);
            }
            CurrentScene.Reset();
            StartNextScene();
        }

        void FinishAll()
        {
            if (bHasOriginalPawnTransform)
            {
                if (APawn* Pawn = World.IsValid() && World->GetFirstPlayerController()
                        ? World->GetFirstPlayerController()->GetPawn() : nullptr)
                {
                    Pawn->SetActorTransform(OriginalPawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
                }
            }
            bFinished = true;
            if (bAllPassed)
            {
                UE_LOG(LogVHV, Log,
                    TEXT("[VHVTimelineCastDiagnostic] FINAL Passed=true TimelinesTested=%d. No completion effects or save writes were performed."),
                    Scenes.Num());
            }
            else
            {
                UE_LOG(LogVHV, Error,
                    TEXT("[VHVTimelineCastDiagnostic] FINAL Passed=false TimelinesTested=%d. No completion effects or save writes were performed."),
                    Scenes.Num());
            }
            if (bQuitWhenDone)
            {
                FPlatformMisc::RequestExit(false);
            }
        }

        TWeakObjectPtr<UWorld> World;
        TArray<TWeakObjectPtr<AVHVAmbientConversationActor>> Scenes;
        TWeakObjectPtr<AVHVAmbientConversationActor> CurrentScene;
        int32 SceneIndex = INDEX_NONE;
        int32 HighestLineSeen = INDEX_NONE;
        int32 LastLoggedLine = INDEX_NONE;
        double SceneStartSeconds = 0.0;
        EVHVAmbientConversationLeavePolicy SavedLeavePolicy = EVHVAmbientConversationLeavePolicy::Cancel;
        FTransform OriginalPawnTransform;
        bool bHasOriginalPawnTransform = false;
        bool bCurrentPassed = true;
        bool bAllPassed = true;
        bool bSawStableVisibleCast = false;
        bool bLoggedStableSample = false;
        bool bFinished = false;
        bool bQuitWhenDone = false;
    };

    TSharedPtr<FTimelineCastDiagnostic> ActiveTimelineCastDiagnostic;

    void TimelineCastDiagnosticCommand(const TArray<FString>& Args, UWorld* World)
    {
        const bool bQuitWhenDone = Args.ContainsByPredicate([](const FString& Arg)
        {
            return Arg.Equals(TEXT("quit"), ESearchCase::IgnoreCase);
        });
        if (!World || !World->IsGameWorld())
        {
            UE_LOG(LogVHV, Error,
                TEXT("[VHVTimelineCastDiagnostic] Command requires an active PIE or standalone gameplay world."));
            return;
        }
        if (ActiveTimelineCastDiagnostic.IsValid())
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVTimelineCastDiagnostic] A validation run is already active."));
            return;
        }
        ActiveTimelineCastDiagnostic = MakeShared<FTimelineCastDiagnostic>(World, bQuitWhenDone);
        ActiveTimelineCastDiagnostic->Start();
    }

    bool IsEditableTextFocused()
    {
        if (!FSlateApplication::IsInitialized())
        {
            return false;
        }

        TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
        while (FocusedWidget)
        {
            const FString WidgetType = FocusedWidget->GetTypeAsString();
            if (WidgetType.Contains(TEXT("EditableText")) || WidgetType == TEXT("SSearchBox"))
            {
                return true;
            }
            FocusedWidget = FocusedWidget->GetParentWidget();
        }
        return false;
    }

    UVHVDeveloperSubsystem* ResolveDeveloperSubsystem(UWorld* World)
    {
        UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
        UVHVDeveloperSubsystem* DeveloperSubsystem = GameInstance ? GameInstance->GetSubsystem<UVHVDeveloperSubsystem>() : nullptr;
        if (!DeveloperSubsystem)
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] No VHV Developer Subsystem is available for this world."));
        }
        return DeveloperSubsystem;
    }

    bool RequireArgumentCount(const TArray<FString>& Args, const int32 ExpectedCount, const TCHAR* Usage)
    {
        if (Args.Num() == ExpectedCount)
        {
            return true;
        }
        UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Usage: %s"), Usage);
        return false;
    }

    void StatusCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 0, TEXT("vhv.status")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->PrintStatus();
        }
    }

    void CompleteQuestCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 0, TEXT("vhv.quest.complete")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->CompleteCurrentObjective();
        }
    }

    void RestartQuestCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 0, TEXT("vhv.quest.restart")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->RestartCurrentQuest();
        }
    }

    void ActivateQuestObjectiveCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 0, TEXT("vhv.quest.activate")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->ActivateCurrentObjective();
        }
    }

    void SetFlagCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 1, TEXT("vhv.story.setflag <FlagID>")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->SetStoryFlag(FName(*Args[0]));
        }
    }

    void ClearFlagCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 1, TEXT("vhv.story.clearflag <FlagID>")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->ClearStoryFlag(FName(*Args[0]));
        }
    }

    void SetCounterCommand(const TArray<FString>& Args, UWorld* World)
    {
        int32 Value = 0;
        if (!RequireArgumentCount(Args, 2, TEXT("vhv.story.setcounter <CounterID> <Value>"))) return;
        if (!LexTryParseString(Value, *Args[1]))
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Usage: vhv.story.setcounter <CounterID> <Value> (Value must be an integer)"));
            return;
        }
        if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->SetStoryCounter(FName(*Args[0]), Value);
    }

    void AddCounterCommand(const TArray<FString>& Args, UWorld* World)
    {
        int32 Delta = 0;
        if (!RequireArgumentCount(Args, 2, TEXT("vhv.story.addcounter <CounterID> <Delta>"))) return;
        if (!LexTryParseString(Delta, *Args[1]))
        {
            UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Usage: vhv.story.addcounter <CounterID> <Delta> (Delta must be an integer)"));
            return;
        }
        if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->AddStoryCounter(FName(*Args[0]), Delta);
    }

    void WorldActionCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 2, TEXT("vhv.world.action <ReceiverID> <ActionID>")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->TriggerWorldAction(FName(*Args[0]), FName(*Args[1]));
        }
    }

    void SaveCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 0, TEXT("vhv.save")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->SaveProgress();
        }
    }

    void LoadCommand(const TArray<FString>& Args, UWorld* World)
    {
        if (RequireArgumentCount(Args, 0, TEXT("vhv.load")))
        {
            if (UVHVDeveloperSubsystem* Developer = ResolveDeveloperSubsystem(World)) Developer->LoadProgress();
        }
    }

    static FAutoConsoleCommandWithWorldAndArgs GStatusCommand(
        TEXT("vhv.status"), TEXT("Print a compact VHV runtime status snapshot."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StatusCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GCompleteQuestCommand(
        TEXT("vhv.quest.complete"), TEXT("Complete the current objective through normal quest progression."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&CompleteQuestCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GRestartQuestCommand(
        TEXT("vhv.quest.restart"), TEXT("Restart the active authored quest at its first objective."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RestartQuestCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GActivateQuestObjectiveCommand(
        TEXT("vhv.quest.activate"), TEXT("Request the active quest objective's normal presentation/interaction path."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ActivateQuestObjectiveCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GSetFlagCommand(
        TEXT("vhv.story.setflag"), TEXT("Usage: vhv.story.setflag <FlagID>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetFlagCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GClearFlagCommand(
        TEXT("vhv.story.clearflag"), TEXT("Usage: vhv.story.clearflag <FlagID>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ClearFlagCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GSetCounterCommand(
        TEXT("vhv.story.setcounter"), TEXT("Usage: vhv.story.setcounter <CounterID> <Value>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetCounterCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GAddCounterCommand(
        TEXT("vhv.story.addcounter"), TEXT("Usage: vhv.story.addcounter <CounterID> <Delta>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AddCounterCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GWorldActionCommand(
        TEXT("vhv.world.action"), TEXT("Usage: vhv.world.action <ReceiverID> <ActionID>"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&WorldActionCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GSaveCommand(
        TEXT("vhv.save"), TEXT("Save VHV progress using the existing save subsystem."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SaveCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GLoadCommand(
        TEXT("vhv.load"), TEXT("Load VHV progress using the existing save subsystem."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&LoadCommand), ECVF_Cheat);
    static FAutoConsoleCommandWithWorldAndArgs GTimelineCastDiagnosticCommand(
        TEXT("vhv.timeline.cast.validate"),
        TEXT("Development-only live validation of the five Quest 3 timeline casts. Optional argument: quit."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&TimelineCastDiagnosticCommand), ECVF_Cheat);
}
#endif

bool UVHVDeveloperSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return Super::ShouldCreateSubsystem(Outer);
#endif
}

void UVHVDeveloperSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if !UE_BUILD_SHIPPING
    QuestSubsystem = Collection.InitializeDependency<UVHVQuestSubsystem>();
    StoryStateSubsystem = Collection.InitializeDependency<UVHVStoryStateSubsystem>();
    SaveSubsystem = Collection.InitializeDependency<UVHVSaveSubsystem>();
#endif
}

void UVHVDeveloperSubsystem::Deinitialize()
{
    QuestSubsystem = nullptr;
    StoryStateSubsystem = nullptr;
    SaveSubsystem = nullptr;
    Super::Deinitialize();
}

void UVHVDeveloperSubsystem::PrintStatus() const
{
#if !UE_BUILD_SHIPPING
    const FVHVQuestArcRuntimeState QuestState = QuestSubsystem ? QuestSubsystem->GetRuntimeState() : FVHVQuestArcRuntimeState();
    FVHVQuestObjectiveDefinition Objective;
    const bool bHasObjective = QuestSubsystem && QuestSubsystem->GetCurrentObjective(Objective);
    const int32 FlagCount = StoryStateSubsystem ? StoryStateSubsystem->GetSetFlagIDs().Num() : 0;
    const int32 CounterCount = StoryStateSubsystem ? StoryStateSubsystem->GetCounterValues().Num() : 0;
    const bool bSaveExists = SaveSubsystem && SaveSubsystem->DoesSaveExist();

    UE_LOG(LogVHV, Log,
        TEXT("[VHVDeveloper] Arc=%s Quest=%s Objective=%s Waiting=%s Flags=%d Counters=%d Save=%s Text=\"%s\""),
        *QuestState.QuestArcID.ToString(),
        *QuestState.ActiveQuestID.ToString(),
        bHasObjective ? *Objective.ObjectiveID.ToString() : TEXT("None"),
        QuestSubsystem && QuestSubsystem->IsCurrentObjectiveWaitingForActivation() ? TEXT("true") : TEXT("false"),
        FlagCount,
        CounterCount,
        bSaveExists ? TEXT("yes") : TEXT("no"),
        bHasObjective ? *Objective.ObjectiveText.ToString() : TEXT(""));
#endif
}

bool UVHVDeveloperSubsystem::CompleteCurrentObjective()
{
#if !UE_BUILD_SHIPPING
    if (bWholeQuestSkipInProgress)
    {
        return false;
    }
    UVHVUIManagerComponent* UIManager = nullptr;
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PlayerController = World->GetFirstPlayerController())
        {
            UIManager = PlayerController->FindComponentByClass<UVHVUIManagerComponent>();
            if (UIManager)
            {
                if (!UIManager->PrepareForDeveloperObjectiveSkip())
                {
                    return false;
                }
            }
        }
    }
    const bool bCompleted = QuestSubsystem && QuestSubsystem->DebugCompleteCurrentObjective();
    if (UIManager)
    {
        UIManager->FinishDeveloperObjectiveSkip();
    }
    return bCompleted;
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::CompleteCurrentQuest()
{
#if !UE_BUILD_SHIPPING
    if (bWholeQuestSkipInProgress || !QuestSubsystem)
    {
        return false;
    }

    const FName CapturedQuestID = QuestSubsystem->GetRuntimeState().ActiveQuestID;
    if (CapturedQuestID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Cannot complete quest: there is no active quest."));
        return false;
    }

    TGuardValue<bool> SkipGuard(bWholeQuestSkipInProgress, true);
    UVHVUIManagerComponent* UIManager = nullptr;
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PlayerController = World->GetFirstPlayerController())
        {
            UIManager = PlayerController->FindComponentByClass<UVHVUIManagerComponent>();
            if (UIManager)
            {
                UIManager->PrepareForDeveloperQuestSkip();
            }
        }
    }

    const bool bCompleted = QuestSubsystem->DebugCompleteCurrentQuest(CapturedQuestID);
    if (UIManager)
    {
        UIManager->FinishDeveloperQuestSkip();
    }
    return bCompleted;
#else
    return false;
#endif
}

void UVHVDeveloperSubsystem::BindDeveloperInput(UInputComponent* InputComponent)
{
#if !UE_BUILD_SHIPPING
    if (InputComponent)
    {
        InputComponent->BindKey(
            EKeys::N,
            IE_Pressed,
            this,
            &UVHVDeveloperSubsystem::HandleCompleteCurrentObjectiveShortcut);
        InputComponent->BindKey(
            EKeys::M,
            IE_Pressed,
            this,
            &UVHVDeveloperSubsystem::HandleCompleteCurrentQuestShortcut);
    }
#endif
}

void UVHVDeveloperSubsystem::HandleCompleteCurrentQuestShortcut()
{
#if !UE_BUILD_SHIPPING
    if (!IsEditableTextFocused())
    {
        CompleteCurrentQuest();
    }
#endif
}

void UVHVDeveloperSubsystem::HandleCompleteCurrentObjectiveShortcut()
{
#if !UE_BUILD_SHIPPING
    if (!IsEditableTextFocused())
    {
        CompleteCurrentObjective();
    }
#endif
}

bool UVHVDeveloperSubsystem::RestartCurrentQuest()
{
#if !UE_BUILD_SHIPPING
    return QuestSubsystem && QuestSubsystem->DebugRestartCurrentQuest();
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::ActivateCurrentObjective()
{
#if !UE_BUILD_SHIPPING
    return QuestSubsystem && QuestSubsystem->RequestCurrentObjectiveActivation();
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::SetStoryFlag(const FName FlagID)
{
#if !UE_BUILD_SHIPPING
    if (!StoryStateSubsystem || FlagID.IsNone()) return false;
    StoryStateSubsystem->SetFlag(FlagID);
    return true;
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::ClearStoryFlag(const FName FlagID)
{
#if !UE_BUILD_SHIPPING
    if (!StoryStateSubsystem || FlagID.IsNone()) return false;
    StoryStateSubsystem->ClearFlag(FlagID);
    return true;
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::SetStoryCounter(const FName CounterID, const int32 Value)
{
#if !UE_BUILD_SHIPPING
    if (!StoryStateSubsystem || CounterID.IsNone()) return false;
    StoryStateSubsystem->SetCounter(CounterID, Value);
    return true;
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::AddStoryCounter(const FName CounterID, const int32 Delta)
{
#if !UE_BUILD_SHIPPING
    if (!StoryStateSubsystem || CounterID.IsNone()) return false;
    StoryStateSubsystem->AddCounter(CounterID, Delta);
    return true;
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::TriggerWorldAction(const FName ReceiverID, const FName ActionID)
{
#if !UE_BUILD_SHIPPING
    if (ReceiverID.IsNone() || ActionID.IsNone() || !GetWorld())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] Usage: vhv.world.action <ReceiverID> <ActionID>"));
        return false;
    }
    UVHVWorldActionSubsystem* WorldActionSubsystem = GetWorld()->GetSubsystem<UVHVWorldActionSubsystem>();
    if (!WorldActionSubsystem)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVDeveloper] World Action subsystem is unavailable."));
        return false;
    }

    FGuid RequestID;
    const EVHVWorldActionExecutionResult Result = WorldActionSubsystem->RequestWorldAction(ReceiverID, ActionID, RequestID);
    UE_LOG(LogVHV, Log, TEXT("[VHVDeveloper] World action Receiver=%s Action=%s Result=%s Request=%s"),
        *ReceiverID.ToString(), *ActionID.ToString(), *UEnum::GetValueAsString(Result), *RequestID.ToString());
    return Result != EVHVWorldActionExecutionResult::Rejected;
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::SaveProgress()
{
#if !UE_BUILD_SHIPPING
    return SaveSubsystem && SaveSubsystem->SaveProgress();
#else
    return false;
#endif
}

bool UVHVDeveloperSubsystem::LoadProgress()
{
#if !UE_BUILD_SHIPPING
    return SaveSubsystem && SaveSubsystem->LoadProgress();
#else
    return false;
#endif
}
