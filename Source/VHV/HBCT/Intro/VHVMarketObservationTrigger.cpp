#include "HBCT/Intro/VHVMarketObservationTrigger.h"

#include "Ambient/VHVAmbientConversationActor.h"
#include "Ambient/VHVAmbientConversationData.h"
#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "VHV.h"
#include "VHVCharacter.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
    const FName MarketObservationCounter(TEXT("VHV.Story.Counter.HBCT.Intro.MarketObservations"));
    const FString MarketObservationFlagPrefix(TEXT("VHV.Story.Flag.HBCT.Intro.MarketObservation."));

    const FName VendorObservation(TEXT("Market_VendorConversation"));
    const FName SmokerObservation(TEXT("Market_Smoker"));
    const FName SweetDrinkObservation(TEXT("Market_SweetDrink"));
    const FName MotorbikeObservation(TEXT("Market_Motorbike"));
    const FName JunkFoodObservation(TEXT("Market_JunkFood"));

    const FName BuyerSlot(TEXT("Buyer"));
    const FName VendorSlot(TEXT("Vendor"));
    const FName SmokerSlot(TEXT("Smoker"));
    const FName SweetTeaSlot(TEXT("SweetTea"));
    const FName RiderSlot(TEXT("Rider"));
    const FName JunkFoodSlot(TEXT("JunkFood"));
}

AVHVMarketObservationTrigger::AVHVMarketObservationTrigger()
{
    PrimaryActorTick.bCanEverTick = false;

    BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("ObservationVolume"));
    SetRootComponent(BoxComponent);
    BoxComponent->SetBoxExtent(FVector(200.0, 200.0, 120.0));
    BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
    BoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    BoxComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    BoxComponent->SetGenerateOverlapEvents(true);
}

const TArray<FName>& AVHVMarketObservationTrigger::GetRequiredObservationIDs()
{
    static const TArray<FName> IDs = {
        VendorObservation,
        SmokerObservation,
        SweetDrinkObservation,
        MotorbikeObservation,
        JunkFoodObservation,
    };
    return IDs;
}

void AVHVMarketObservationTrigger::ResetObservationProgress(UVHVStoryStateSubsystem* StoryState)
{
    if (!StoryState) return;
    for (const FName ID : GetRequiredObservationIDs())
    {
        StoryState->ClearFlag(FName(*(MarketObservationFlagPrefix + ID.ToString())));
    }
    StoryState->SetCounter(MarketObservationCounter, 0);
}

void AVHVMarketObservationTrigger::BeginPlay()
{
    Super::BeginPlay();
    BoxComponent->OnComponentBeginOverlap.AddUniqueDynamic(
        this, &AVHVMarketObservationTrigger::HandleBoxBeginOverlap);

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>();
        StoryStateSubsystem = GameInstance->GetSubsystem<UVHVStoryStateSubsystem>();
    }
    if (ConversationActor)
    {
        ConversationActor->OnConversationFinished.AddUniqueDynamic(
            this, &AVHVMarketObservationTrigger::HandleConversationFinished);
    }
}

void AVHVMarketObservationTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    BoxComponent->OnComponentBeginOverlap.RemoveDynamic(
        this, &AVHVMarketObservationTrigger::HandleBoxBeginOverlap);
    if (ConversationActor)
    {
        ConversationActor->OnConversationFinished.RemoveDynamic(
            this, &AVHVMarketObservationTrigger::HandleConversationFinished);
    }
    bAwaitingConversationCompletion = false;
    Super::EndPlay(EndPlayReason);
}

void AVHVMarketObservationTrigger::HandleBoxBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    const int32 OtherBodyIndex,
    const bool bFromSweep,
    const FHitResult& SweepResult)
{
    (void)OverlappedComponent;
    (void)OtherComponent;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;

    if (!Cast<AVHVCharacter>(OtherActor) || bAwaitingConversationCompletion
        || !ConversationActor || !StoryStateSubsystem || !IsIntendedObjectiveActive()
        || StoryStateSubsystem->HasFlag(GetCompletionFlagID()))
    {
        return;
    }

    int32 FirstLineIndex = INDEX_NONE;
    int32 LineCount = 0;
    if (!ResolveLineRange(FirstLineIndex, LineCount))
    {
        UE_LOG(LogVHV, Error,
            TEXT("[Q0 Market Observation] Trigger '%s' could not resolve content for '%s'."),
            *GetName(), *ObservationID.ToString());
        return;
    }

    if (ConversationActor->StartConversationRange(FirstLineIndex, LineCount))
    {
        bAwaitingConversationCompletion = true;
    }
}

void AVHVMarketObservationTrigger::HandleConversationFinished(const bool bSuccess)
{
    if (!bAwaitingConversationCompletion) return;
    bAwaitingConversationCompletion = false;
    if (!bSuccess || !StoryStateSubsystem || !QuestSubsystem || !IsIntendedObjectiveActive()
        || StoryStateSubsystem->HasFlag(GetCompletionFlagID()))
    {
        return;
    }

    StoryStateSubsystem->SetFlag(GetCompletionFlagID());
    const int32 CompletedCount = CountCompletedObservations();
    StoryStateSubsystem->SetCounter(MarketObservationCounter, CompletedCount);

    UE_LOG(LogVHV, Log,
        TEXT("[Q0 Market Observation] Completed '%s' (%d/%d)."),
        *ObservationID.ToString(), CompletedCount, GetRequiredObservationIDs().Num());

    if (CompletedCount == GetRequiredObservationIDs().Num())
    {
        if (!QuestSubsystem->CompleteCurrentObjective())
        {
            UE_LOG(LogVHV, Error,
                TEXT("[Q0 Market Observation] All observations completed, but objective completion was rejected."));
        }
    }
}

bool AVHVMarketObservationTrigger::IsIntendedObjectiveActive() const
{
    if (!QuestSubsystem || QuestSubsystem->IsCurrentObjectiveWaitingForActivation()) return false;
    const FVHVQuestArcRuntimeState Runtime = QuestSubsystem->GetRuntimeState();
    if (Runtime.ActiveQuestID != RequiredQuestID) return false;

    FVHVQuestObjectiveDefinition Objective;
    return QuestSubsystem->GetCurrentObjective(Objective)
        && Objective.ObjectiveID == RequiredObjectiveID;
}

bool AVHVMarketObservationTrigger::ResolveLineRange(int32& OutFirstLineIndex, int32& OutLineCount) const
{
    OutFirstLineIndex = INDEX_NONE;
    OutLineCount = 0;
    const UVHVAmbientConversationData* Data = ConversationActor ? ConversationActor->ConversationData : nullptr;
    if (!Data) return false;

    if (ObservationID == VendorObservation)
    {
        for (int32 Index = 0; Index + 1 < Data->Lines.Num(); ++Index)
        {
            if (Data->Lines[Index].SpeakerSlot == BuyerSlot
                && Data->Lines[Index + 1].SpeakerSlot == VendorSlot)
            {
                OutFirstLineIndex = Index;
                OutLineCount = 2;
                return true;
            }
        }
        return false;
    }

    FName RequiredSlot = NAME_None;
    if (ObservationID == SmokerObservation) RequiredSlot = SmokerSlot;
    else if (ObservationID == SweetDrinkObservation) RequiredSlot = SweetTeaSlot;
    else if (ObservationID == MotorbikeObservation) RequiredSlot = RiderSlot;
    else if (ObservationID == JunkFoodObservation) RequiredSlot = JunkFoodSlot;
    if (RequiredSlot.IsNone()) return false;

    const int32 Index = Data->Lines.IndexOfByPredicate([RequiredSlot](const FVHVAmbientSpeechLine& Line)
    {
        return Line.SpeakerSlot == RequiredSlot;
    });
    if (Index == INDEX_NONE) return false;
    OutFirstLineIndex = Index;
    OutLineCount = 1;
    return true;
}

FName AVHVMarketObservationTrigger::GetCompletionFlagID() const
{
    return ObservationID.IsNone()
        ? NAME_None
        : FName(*(MarketObservationFlagPrefix + ObservationID.ToString()));
}

int32 AVHVMarketObservationTrigger::CountCompletedObservations() const
{
    if (!StoryStateSubsystem) return 0;
    int32 Count = 0;
    for (const FName ID : GetRequiredObservationIDs())
    {
        Count += StoryStateSubsystem->HasFlag(FName(*(MarketObservationFlagPrefix + ID.ToString()))) ? 1 : 0;
    }
    return Count;
}

#if WITH_EDITOR
EDataValidationResult AVHVMarketObservationTrigger::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    if (Result == EDataValidationResult::NotValidated)
    {
        Result = EDataValidationResult::Valid;
    }

    if (!GetRequiredObservationIDs().Contains(ObservationID))
    {
        Context.AddError(FText::FromString(TEXT("Market observation trigger has an unsupported Observation ID.")));
        Result = EDataValidationResult::Invalid;
    }
    if (!IsValid(ConversationActor))
    {
        Context.AddError(FText::FromString(TEXT("Market observation trigger has no conversation actor.")));
        Result = EDataValidationResult::Invalid;
    }
    else
    {
        int32 FirstLineIndex = INDEX_NONE;
        int32 LineCount = 0;
        if (!ResolveLineRange(FirstLineIndex, LineCount))
        {
            Context.AddError(FText::FromString(TEXT("Market observation trigger cannot resolve its authored speaker line(s).")));
            Result = EDataValidationResult::Invalid;
        }
    }
    if (RequiredQuestID != FName(TEXT("Q_HBCT_00_INTRO"))
        || RequiredObjectiveID != FName(TEXT("O03_ObserveMarket")))
    {
        Context.AddError(FText::FromString(TEXT("Market observation trigger must be gated to Q0/O03.")));
        Result = EDataValidationResult::Invalid;
    }
    if (BoxComponent->GetCollisionEnabled() != ECollisionEnabled::QueryOnly
        || BoxComponent->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Overlap)
    {
        Context.AddError(FText::FromString(TEXT("Market observation trigger must be QueryOnly and overlap Pawns.")));
        Result = EDataValidationResult::Invalid;
    }
    return Result;
}
#endif
