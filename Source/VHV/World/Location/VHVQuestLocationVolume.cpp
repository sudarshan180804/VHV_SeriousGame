#include "World/Location/VHVQuestLocationVolume.h"

#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "VHV.h"
#include "VHVCharacter.h"

#if WITH_EDITOR
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
    return Result;
}
#endif
