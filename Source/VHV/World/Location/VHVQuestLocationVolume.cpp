#include "World/Location/VHVQuestLocationVolume.h"

#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "VHV.h"
#include "VHVCharacter.h"

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

    if (LocationID.IsNone())
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

    if (LocationID.IsNone())
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
        QuestSubsystem->NotifyLocationReached(LocationID);
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
