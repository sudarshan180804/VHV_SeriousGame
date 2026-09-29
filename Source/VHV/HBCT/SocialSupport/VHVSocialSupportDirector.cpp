#include "HBCT/SocialSupport/VHVSocialSupportDirector.h"

#include "Ambient/VHVAmbientConversationActor.h"
#include "Core/VHVAuthoringReferences.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "NPC/Components/VHVNPCDialogueComponent.h"
#include "NPC/Components/VHVNPCInteractionComponent.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "UI/VHVUIManagerComponent.h"
#include "World/Location/VHVQuestLocationVolume.h"
#include "TimerManager.h"
#include "VHV.h"

namespace
{
    const FName SocialSupportQuestID(TEXT("Q_HBCT_04_SOCIAL_SUPPORT"));
    const FName MeetUncleChaiObjectiveID(TEXT("O04_MeetUncleChai"));
    const FName UncleChaiParticipantID(TEXT("UncleChai"));
    const FName ChaiHomeLocationID(TEXT("SSChaiHome"));
    const FName ChaiIntroReadyFlagID(TEXT("SSChaiIntroReady"));

    const FName RoomEmotionalActivityTag(TEXT("VHV.Activity.HBCT.SocialSupport.RoomEmotional"));
    const FName RoomInformationalActivityTag(TEXT("VHV.Activity.HBCT.SocialSupport.RoomInformational"));
    const FName RoomInstrumentalActivityTag(TEXT("VHV.Activity.HBCT.SocialSupport.RoomInstrumental"));
    const FName RoomAppraisalActivityTag(TEXT("VHV.Activity.HBCT.SocialSupport.RoomAppraisal"));
}

AVHVSocialSupportDirector::AVHVSocialSupportDirector()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot")));
}

void AVHVSocialSupportDirector::BeginPlay()
{
    Super::BeginPlay();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimerForNextTick(
            FTimerDelegate::CreateWeakLambda(this, [this]() { InitializeRuntimeBindings(); }));
    }
}

void AVHVSocialSupportDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (QuestSubsystem)
    {
        QuestSubsystem->OnObjectiveChanged.RemoveDynamic(this, &AVHVSocialSupportDirector::HandleObjectiveChanged);
        QuestSubsystem->OnObjectiveCompleted.RemoveDynamic(this, &AVHVSocialSupportDirector::HandleObjectiveCompleted);
    }
    if (StoryStateSubsystem)
    {
        StoryStateSubsystem->OnStoryFlagChanged.RemoveDynamic(this, &AVHVSocialSupportDirector::HandleStoryFlagChanged);
    }
    if (WalkingObservationScene)
    {
        WalkingObservationScene->OnConversationStarted.RemoveDynamic(this, &AVHVSocialSupportDirector::HandleWalkingObservationStarted);
    }
    if (EmotionalRoomScene) EmotionalRoomScene->OnConversationFinished.RemoveDynamic(this, &AVHVSocialSupportDirector::HandleEmotionalRoomFinished);
    if (InformationalRoomScene) InformationalRoomScene->OnConversationFinished.RemoveDynamic(this, &AVHVSocialSupportDirector::HandleInformationalRoomFinished);
    if (InstrumentalRoomScene) InstrumentalRoomScene->OnConversationFinished.RemoveDynamic(this, &AVHVSocialSupportDirector::HandleInstrumentalRoomFinished);
    if (AppraisalRoomScene) AppraisalRoomScene->OnConversationFinished.RemoveDynamic(this, &AVHVSocialSupportDirector::HandleAppraisalRoomFinished);
    Super::EndPlay(EndPlayReason);
}

void AVHVSocialSupportDirector::InitializeRuntimeBindings()
{
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    QuestSubsystem = GameInstance ? GameInstance->GetSubsystem<UVHVQuestSubsystem>() : nullptr;
    StoryStateSubsystem = GameInstance ? GameInstance->GetSubsystem<UVHVStoryStateSubsystem>() : nullptr;
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    UIManager = PC ? PC->FindComponentByClass<UVHVUIManagerComponent>() : nullptr;

    if (QuestSubsystem)
    {
        QuestSubsystem->OnObjectiveChanged.AddUniqueDynamic(this, &AVHVSocialSupportDirector::HandleObjectiveChanged);
        QuestSubsystem->OnObjectiveCompleted.AddUniqueDynamic(this, &AVHVSocialSupportDirector::HandleObjectiveCompleted);
    }
    if (StoryStateSubsystem)
    {
        StoryStateSubsystem->OnStoryFlagChanged.AddUniqueDynamic(this, &AVHVSocialSupportDirector::HandleStoryFlagChanged);
    }
    if (WalkingObservationScene)
    {
        WalkingObservationScene->OnConversationStarted.AddUniqueDynamic(this, &AVHVSocialSupportDirector::HandleWalkingObservationStarted);
    }
    if (EmotionalRoomScene) EmotionalRoomScene->OnConversationFinished.AddUniqueDynamic(this, &AVHVSocialSupportDirector::HandleEmotionalRoomFinished);
    if (InformationalRoomScene) InformationalRoomScene->OnConversationFinished.AddUniqueDynamic(this, &AVHVSocialSupportDirector::HandleInformationalRoomFinished);
    if (InstrumentalRoomScene) InstrumentalRoomScene->OnConversationFinished.AddUniqueDynamic(this, &AVHVSocialSupportDirector::HandleInstrumentalRoomFinished);
    if (AppraisalRoomScene) AppraisalRoomScene->OnConversationFinished.AddUniqueDynamic(this, &AVHVSocialSupportDirector::HandleAppraisalRoomFinished);
    if (IsActiveObjective(MeetUncleChaiObjectiveID))
    {
        StageUncleChaiForIntroduction();
    }
    RefreshQuestPresentation();
}

FName AVHVSocialSupportDirector::ResolveFlag(const FGameplayTag& Flag) const
{
    return VHVAuthoringReferences::ResolveID(Flag, NAME_None, TEXT("VHV.Story.Flag"));
}

int32 AVHVSocialSupportDirector::CountSetFlags(const TArray<FGameplayTag>& Flags) const
{
    if (!StoryStateSubsystem) return 0;
    int32 Count = 0;
    for (const FGameplayTag& Flag : Flags)
    {
        if (StoryStateSubsystem->HasFlag(ResolveFlag(Flag))) ++Count;
    }
    return Count;
}

bool AVHVSocialSupportDirector::IsActiveObjective(const FName ObjectiveID) const
{
    if (!QuestSubsystem || QuestSubsystem->GetRuntimeState().ActiveQuestID != SocialSupportQuestID)
    {
        return false;
    }
    FVHVQuestObjectiveDefinition Objective;
    return QuestSubsystem->GetCurrentObjective(Objective) && Objective.ObjectiveID == ObjectiveID;
}

void AVHVSocialSupportDirector::RefreshQuestPresentation()
{
    if (!UIManager || !QuestSubsystem
        || QuestSubsystem->GetRuntimeState().ActiveQuestID != SocialSupportQuestID)
    {
        if (UIManager) UIManager->HideSocialSupportProgress();
        return;
    }

    FVHVQuestObjectiveDefinition Objective;
    if (!QuestSubsystem->GetCurrentObjective(Objective)) return;
    if (Objective.ObjectiveID == FName(TEXT("O01_ObserveVillageSupport")))
    {
        UIManager->ShowSocialSupportObservationProgress(CountSetFlags(ObservationFlags));
    }
    else if (Objective.ObjectiveID == FName(TEXT("O05_FindChaiSupporters")))
    {
        UIManager->ShowSocialSupportSupporterProgress(CountSetFlags(SupporterFlags));
    }
    else
    {
        UIManager->HideSocialSupportProgress();
    }
}

void AVHVSocialSupportDirector::HandleObjectiveChanged(const FName QuestID, const FName ObjectiveID)
{
    (void)QuestID;
    (void)ObjectiveID;
    RefreshQuestPresentation();
}

void AVHVSocialSupportDirector::HandleObjectiveCompleted(const FName QuestID, const FName ObjectiveID)
{
    if (QuestID != SocialSupportQuestID) return;
    if (ObjectiveID == FName(TEXT("O03_ExploreSupportHouse")))
    {
        StageUncleChaiForIntroduction();
    }
    if (!UIManager) return;
    if (ObjectiveID == FName(TEXT("O02_ReflectOnSupport")))
    {
        UIManager->ShowSupportTypeReveal();
    }
    else if (ObjectiveID == FName(TEXT("O03_ExploreSupportHouse")) && RoomsCompleteScene)
    {
        RoomsCompleteScene->StartConversation();
    }
    else if (ObjectiveID == FName(TEXT("O06_BuildSupportNetwork")) && NetworkReadyScene)
    {
        NetworkReadyScene->StartConversation();
    }
}

void AVHVSocialSupportDirector::HandleStoryFlagChanged(const FName FlagID, const bool bValue)
{
    (void)FlagID;
    (void)bValue;
    RefreshQuestPresentation();
}

void AVHVSocialSupportDirector::HandleWalkingObservationStarted()
{
    if (!IsActiveObjective(FName(TEXT("O01_ObserveVillageSupport"))) || !QuestSubsystem) return;
    if (ObservationFlags.IsValidIndex(1) && StoryStateSubsystem)
    {
        StoryStateSubsystem->SetFlag(ResolveFlag(ObservationFlags[1]));
    }
    QuestSubsystem->RequestNPCMove(
        VHVAuthoringReferences::ResolveTagLeaf(ObservationWalkerTag),
        VHVAuthoringReferences::ResolveTagLeaf(ObservationWalkerDestinationTag), true, 180.0f);
    QuestSubsystem->RequestNPCMove(
        VHVAuthoringReferences::ResolveTagLeaf(ObservationPartnerTag),
        VHVAuthoringReferences::ResolveTagLeaf(ObservationPartnerDestinationTag), true, 180.0f);
}

void AVHVSocialSupportDirector::ShowRoomClassification(
    const FName ActivityTagName,
    const FGameplayTag& CompletionFlag)
{
    if (!UIManager || !StoryStateSubsystem || !IsActiveObjective(FName(TEXT("O03_ExploreSupportHouse")))) return;
    const FName FlagID = ResolveFlag(CompletionFlag);
    if (!FlagID.IsNone() && !StoryStateSubsystem->HasFlag(FlagID))
    {
        FTextbookActivityReference Reference;
        Reference.ActivityTag = FGameplayTag::RequestGameplayTag(ActivityTagName);
        if (!UIManager->StartQuestManagedLearningActivity(Reference))
        {
            UE_LOG(LogVHV, Warning,
                TEXT("[VHVQuest] Could not start Support House SingleChoice activity '%s'."),
                *ActivityTagName.ToString());
        }
    }
}

void AVHVSocialSupportDirector::HandleEmotionalRoomFinished(const bool bSuccess)
{
    if (bSuccess) ShowRoomClassification(RoomEmotionalActivityTag, EmotionalRoomFlag);
}

void AVHVSocialSupportDirector::HandleInformationalRoomFinished(const bool bSuccess)
{
    if (bSuccess) ShowRoomClassification(RoomInformationalActivityTag, InformationalRoomFlag);
}

void AVHVSocialSupportDirector::HandleInstrumentalRoomFinished(const bool bSuccess)
{
    if (bSuccess) ShowRoomClassification(RoomInstrumentalActivityTag, InstrumentalRoomFlag);
}

void AVHVSocialSupportDirector::HandleAppraisalRoomFinished(const bool bSuccess)
{
    if (bSuccess) ShowRoomClassification(RoomAppraisalActivityTag, AppraisalRoomFlag);
}

bool AVHVSocialSupportDirector::StageUncleChaiForIntroduction()
{
    UWorld* World = GetWorld();
    if (!World || !StoryStateSubsystem || !QuestSubsystem)
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHV Interaction] UncleChai suppressed: Quest 4 staging services unavailable."));
        return false;
    }

    AActor* UncleChai = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        const UVHVQuestParticipantComponent* Participant =
            It->FindComponentByClass<UVHVQuestParticipantComponent>();
        if (Participant && Participant->GetEffectiveParticipantID() == UncleChaiParticipantID)
        {
            if (UncleChai)
            {
                UE_LOG(LogVHV, Warning,
                    TEXT("[VHV Interaction] UncleChai suppressed: duplicate participant resolution."));
                return false;
            }
            UncleChai = *It;
        }
    }

    AVHVQuestLocationVolume* ChaiHome = nullptr;
    for (TActorIterator<AVHVQuestLocationVolume> It(World); It; ++It)
    {
        if (It->GetEffectiveLocationID() == ChaiHomeLocationID
            && It->RequiredActiveQuestID == SocialSupportQuestID
            && It->RequiredActiveObjectiveID == MeetUncleChaiObjectiveID)
        {
            ChaiHome = *It;
            break;
        }
    }

    if (!UncleChai || !ChaiHome)
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHV Interaction] UncleChai suppressed: %s not found."),
            !UncleChai ? TEXT("persistent participant") : TEXT("O04 ChaiIntro trigger"));
        return false;
    }
    const UVHVNPCInteractionComponent* Interaction =
        UncleChai->FindComponentByClass<UVHVNPCInteractionComponent>();
    const UVHVNPCDialogueComponent* Dialogue =
        UncleChai->FindComponentByClass<UVHVNPCDialogueComponent>();
    if (!Interaction || !Interaction->bInteractionEnabled || !Dialogue)
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHV Interaction] UncleChai suppressed: required interaction/dialogue component unavailable."));
        return false;
    }

    QuestSubsystem->CancelNPCQuestCommand(UncleChaiParticipantID);
    const FVector HomeLocation = ChaiHome->GetActorLocation();
    const bool bPlaced = UncleChai->SetActorLocationAndRotation(
        HomeLocation, ChaiHome->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);
    if (!bPlaced || FVector::DistSquared2D(UncleChai->GetActorLocation(), HomeLocation) > 1.0f)
    {
        UE_LOG(LogVHV, Warning,
            TEXT("[VHV Interaction] UncleChai suppressed: could not stage participant at SSChaiHome."));
        return false;
    }

    StoryStateSubsystem->SetFlag(ChaiIntroReadyFlagID);
    UE_LOG(LogVHV, Log,
        TEXT("[VHVQuest] O04 UncleChai ready for contextual interaction."));
    return true;
}
