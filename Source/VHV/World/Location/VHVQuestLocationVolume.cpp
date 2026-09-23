#include "World/Location/VHVQuestLocationVolume.h"

#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "Story/Systems/VHVStoryStateSubsystem.h"
#include "VHV.h"
#include "VHVCharacter.h"
#include "World/Components/VHVWorldActionReceiverComponent.h"
#include "World/Systems/VHVWorldActionSubsystem.h"

#if WITH_EDITOR
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/DataValidation.h"
#endif

AVHVQuestLocationVolume::AVHVQuestLocationVolume()
{
    PrimaryActorTick.bCanEverTick = false;

    BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("LocationTrigger"));
    SetRootComponent(BoxComponent);
    BoxComponent->SetBoxExtent(FVector(200.0f));
    BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BoxComponent->SetCollisionObjectType(ECC_WorldDynamic);
    BoxComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    BoxComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    BoxComponent->SetGenerateOverlapEvents(true);

#if WITH_EDITORONLY_DATA
    // These properties affect only the editor wireframe generated for the
    // collision shape. HiddenInGame remains true, so no runtime geometry is added.
    BoxComponent->ShapeColor = FColor(0, 220, 255, 255);
    BoxComponent->bDrawOnlyIfSelected = false;
    BoxComponent->SetLineThickness(6.0f);

    EditorTriggerSprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorTriggerSprite"));
    if (EditorTriggerSprite)
    {
        static ConstructorHelpers::FObjectFinderOptional<UTexture2D> TriggerSprite(
            TEXT("/Engine/EditorResources/S_Trigger"));
        EditorTriggerSprite->SetupAttachment(BoxComponent);
        EditorTriggerSprite->Sprite = TriggerSprite.Get();
        EditorTriggerSprite->SetRelativeScale3D(FVector(0.75f));
        EditorTriggerSprite->bIsScreenSizeScaled = true;
        EditorTriggerSprite->SetIsVisualizationComponent(true);
        EditorTriggerSprite->SetHiddenInGame(true);
        EditorTriggerSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    EditorTriggerLabel = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("EditorTriggerLabel"));
    if (EditorTriggerLabel)
    {
        EditorTriggerLabel->SetupAttachment(BoxComponent);
        EditorTriggerLabel->SetHorizontalAlignment(EHTA_Center);
        EditorTriggerLabel->SetVerticalAlignment(EVRTA_TextBottom);
        EditorTriggerLabel->SetTextRenderColor(FColor(0, 220, 255, 255));
        EditorTriggerLabel->SetWorldSize(32.0f);
        EditorTriggerLabel->bAlwaysRenderAsText = true;
        EditorTriggerLabel->SetIsVisualizationComponent(true);
        EditorTriggerLabel->SetHiddenInGame(true);
        EditorTriggerLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
#endif

    BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AVHVQuestLocationVolume::HandleBoxBeginOverlap);
    BoxComponent->OnComponentEndOverlap.AddDynamic(this, &AVHVQuestLocationVolume::HandleBoxEndOverlap);
}

void AVHVQuestLocationVolume::BeginPlay()
{
    Super::BeginPlay();

    if (GetEffectiveLocationID().IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVLocation] Quest location volume '%s' has no LocationID and will not notify quests."), *GetName());
    }

    if (const UGameInstance* GameInstance = GetGameInstance())
    {
        QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>();
        StoryStateSubsystem = GameInstance->GetSubsystem<UVHVStoryStateSubsystem>();
    }
}

void AVHVQuestLocationVolume::HandleBoxBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!bEnabled)
    {
        return;
    }

    const FName EffectiveLocationID = GetEffectiveLocationID();
    if (EffectiveLocationID.IsNone())
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVLocation] Quest location volume '%s' ignored player entry because LocationID is NAME_None."), *GetName());
        return;
    }

    AVHVCharacter* PlayerCharacter = Cast<AVHVCharacter>(OtherActor);
    if (!PlayerCharacter || !PlayerCharacter->IsPlayerControlled() || PlayersInside.Contains(PlayerCharacter))
    {
        return;
    }

    PlayersInside.Add(PlayerCharacter);
    if (QuestSubsystem)
    {
        QuestSubsystem->NotifyLocationReached(EffectiveLocationID);
    }

    const bool bCanActivateObjective = bActivateCurrentObjective
        && (!bActivateCurrentObjectiveOnce || !bCurrentObjectiveActivatedByTrigger);
    const bool bCanTriggerWorldAction = bTriggerWorldAction
        && (!bTriggerWorldActionOnce || !bWorldActionTriggered);
    if (!bCanActivateObjective && !bCanTriggerWorldAction)
    {
        return;
    }

    const FVHVQuestArcRuntimeState QuestRuntime = QuestSubsystem
        ? QuestSubsystem->GetRuntimeState() : FVHVQuestArcRuntimeState();
    FVHVQuestObjectiveDefinition CurrentObjective;
    const bool bHasCurrentObjective = QuestSubsystem
        && QuestSubsystem->GetCurrentObjective(CurrentObjective);
    const bool bQuestGatePassed = RequiredActiveQuestID.IsNone()
        || QuestRuntime.ActiveQuestID == RequiredActiveQuestID;
    const bool bObjectiveGatePassed = RequiredActiveObjectiveID.IsNone()
        || (bHasCurrentObjective && CurrentObjective.ObjectiveID == RequiredActiveObjectiveID);
    const bool bStoryGatePassed = TriggerConditions.Conditions.IsEmpty()
        || (StoryStateSubsystem && StoryStateSubsystem->EvaluateConditionSet(TriggerConditions));
    if (!bQuestGatePassed || !bObjectiveGatePassed || !bStoryGatePassed)
    {
        return;
    }

    if (bCanActivateObjective && QuestSubsystem && QuestSubsystem->RequestCurrentObjectiveActivation())
    {
        bCurrentObjectiveActivatedByTrigger = true;
    }

    if (bCanTriggerWorldAction)
    {
        const FName ReceiverID = VHVAuthoringReferences::ResolveID(
            WorldActionReceiverTag, NAME_None, TEXT("VHV.WorldReceiver"));
        const FName ActionID = VHVAuthoringReferences::ResolveID(
            WorldActionTag, NAME_None, TEXT("VHV.WorldAction"));

        bool bStarted = bTrackWorldActionAsObjective && QuestSubsystem && QuestSubsystem->RequestExplicitWorldAction(
            ReceiverID, ActionID, RequiredActiveQuestID, RequiredActiveObjectiveID);

        if (!bStarted && !bTrackWorldActionAsObjective)
        {
            FGuid RequestID;
            UVHVWorldActionSubsystem* WorldActions = GetWorld()
                ? GetWorld()->GetSubsystem<UVHVWorldActionSubsystem>() : nullptr;
            const EVHVWorldActionExecutionResult Result = WorldActions
                ? WorldActions->RequestWorldAction(ReceiverID, ActionID, RequestID)
                : EVHVWorldActionExecutionResult::Rejected;
            bStarted = Result != EVHVWorldActionExecutionResult::Rejected;
        }

        // Ungated volumes remain available for generic, non-quest WorldActions.
        if (!bStarted && bTrackWorldActionAsObjective
            && RequiredActiveQuestID.IsNone() && RequiredActiveObjectiveID.IsNone())
        {
            FGuid RequestID;
            UVHVWorldActionSubsystem* WorldActions = GetWorld()
                ? GetWorld()->GetSubsystem<UVHVWorldActionSubsystem>() : nullptr;
            const EVHVWorldActionExecutionResult Result = WorldActions
                ? WorldActions->RequestWorldAction(ReceiverID, ActionID, RequestID)
                : EVHVWorldActionExecutionResult::Rejected;
            bStarted = Result != EVHVWorldActionExecutionResult::Rejected;
        }

        if (bStarted)
        {
            bWorldActionTriggered = true;
        }
        else
        {
            UE_LOG(LogVHV, Warning,
                TEXT("[VHVLocation] Trigger '%s' could not dispatch WorldAction '%s' to '%s'."),
                *GetName(), *ActionID.ToString(), *ReceiverID.ToString());
        }
    }
}

void AVHVQuestLocationVolume::HandleBoxEndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex)
{
    if (AVHVCharacter* PlayerCharacter = Cast<AVHVCharacter>(OtherActor))
    {
        if (!BoxComponent->IsOverlappingActor(PlayerCharacter))
        {
            PlayersInside.Remove(PlayerCharacter);
        }
    }
}

#if WITH_EDITOR
void AVHVQuestLocationVolume::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshEditorVisualization();
}

void AVHVQuestLocationVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    RefreshEditorVisualization();
}

void AVHVQuestLocationVolume::RefreshEditorVisualization()
{
#if WITH_EDITORONLY_DATA
    if (!BoxComponent)
    {
        return;
    }

    const float LabelHeight = BoxComponent->GetUnscaledBoxExtent().Z + 80.0f;
    if (EditorTriggerSprite)
    {
        EditorTriggerSprite->SetRelativeLocation(FVector(0.0f, 0.0f, LabelHeight));
    }

    if (EditorTriggerLabel)
    {
        FString DisplayLabel = GetActorLabel();
        const FString TriggerID = GetEffectiveLocationID().ToString();
        if (!TriggerID.IsEmpty() && TriggerID != TEXT("None")
            && !DisplayLabel.Contains(TriggerID, ESearchCase::IgnoreCase))
        {
            DisplayLabel += FString::Printf(TEXT("\n%s"), *TriggerID);
        }

        EditorTriggerLabel->SetText(FText::FromString(DisplayLabel));
        EditorTriggerLabel->SetRelativeLocation(FVector(0.0f, 0.0f, LabelHeight + 45.0f));
    }
#endif
}

EDataValidationResult AVHVQuestLocationVolume::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    if (GetEffectiveLocationID().IsNone())
    {
        Context.AddError(FText::FromString(FString::Printf(TEXT("Quest location volume '%s' has no effective Location ID."), *GetNameSafe(this))));
        Result = EDataValidationResult::Invalid;
    }
    if (!VHVAuthoringReferences::IsValidReferenceTag(LocationTag, TEXT("VHV.Location")))
    {
        Context.AddError(FText::FromString(FString::Printf(TEXT("Quest location volume '%s' uses tag '%s', which must be a concrete tag beneath VHV.Location."), *GetNameSafe(this), *LocationTag.ToString())));
        Result = EDataValidationResult::Invalid;
    }
    if (VHVAuthoringReferences::HasConflict(LocationTag, LocationID, TEXT("VHV.Location")))
    {
        Context.AddWarning(FText::FromString(FString::Printf(TEXT("Quest location volume '%s' has tag '%s', which resolves to '%s', while legacy LocationID is '%s'; the tag wins."), *GetNameSafe(this), *LocationTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(LocationTag).ToString(), *LocationID.ToString())));
    }
    if ((bTriggerWorldAction || bActivateCurrentObjective)
        && (!RequiredActiveQuestID.IsNone() || !RequiredActiveObjectiveID.IsNone())
        && (RequiredActiveQuestID.IsNone() || RequiredActiveObjectiveID.IsNone()))
    {
        Context.AddWarning(FText::FromString(TEXT("Quest-gated story triggers should identify both the required active quest and objective.")));
    }
    if (bActivateCurrentObjective && RequiredActiveQuestID.IsNone() && RequiredActiveObjectiveID.IsNone())
    {
        Context.AddWarning(FText::FromString(TEXT("Objective-activation triggers should be gated to a specific quest objective.")));
    }
    for (const FVHVStoryCondition& Condition : TriggerConditions.Conditions)
    {
        const bool bFlagCondition = Condition.ConditionType == EVHVStoryConditionType::FlagSet
            || Condition.ConditionType == EVHVStoryConditionType::FlagNotSet;
        const TCHAR* ExpectedCategory = bFlagCondition ? TEXT("VHV.Story.Flag") : TEXT("VHV.Story.Counter");
        if (!VHVAuthoringReferences::IsValidReferenceTag(Condition.StateTag, ExpectedCategory))
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("Story trigger condition tag '%s' must be a concrete tag beneath %s."),
                *Condition.StateTag.ToString(), ExpectedCategory)));
            Result = EDataValidationResult::Invalid;
        }
    }
    if (bTriggerWorldAction)
    {
        if (!VHVAuthoringReferences::IsValidReferenceTag(WorldActionReceiverTag, TEXT("VHV.WorldReceiver")))
        {
            Context.AddError(FText::FromString(TEXT("Location-triggered World Action requires a concrete VHV.WorldReceiver tag.")));
            Result = EDataValidationResult::Invalid;
        }
        if (!VHVAuthoringReferences::IsValidReferenceTag(WorldActionTag, TEXT("VHV.WorldAction")))
        {
            Context.AddError(FText::FromString(TEXT("Location-triggered World Action requires a concrete VHV.WorldAction tag.")));
            Result = EDataValidationResult::Invalid;
        }
        const FName ReceiverID = VHVAuthoringReferences::ResolveID(
            WorldActionReceiverTag, NAME_None, TEXT("VHV.WorldReceiver"));
        bool bFoundReceiver = false;
        if (UWorld* World = GetWorld())
        {
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                if (const UVHVWorldActionReceiverComponent* Receiver = It->FindComponentByClass<UVHVWorldActionReceiverComponent>();
                    Receiver && Receiver->GetEffectiveReceiverID() == ReceiverID)
                {
                    bFoundReceiver = true;
                    break;
                }
            }
        }
        if (!bFoundReceiver)
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("World Action trigger '%s' has no matching receiver '%s' in this world."),
                *GetNameSafe(this), *ReceiverID.ToString())));
            Result = EDataValidationResult::Invalid;
        }
    }
    return Result;
}
#endif
