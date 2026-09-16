#include "NPC/VHVNPCCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NPC/Components/VHVNPCBehaviorComponent.h"
#include "NPC/Components/VHVNPCDialogueComponent.h"
#include "NPC/Components/VHVNPCInteractionComponent.h"
#include "NPC/Components/VHVNPCPatrolComponent.h"
#include "NPC/Components/VHVNPCQuestCommandComponent.h"
#include "NPC/VHVNPCAIController.h"
#include "Player/Components/VHVInteractionComponent.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "VHV/Core/VHVCollisionChannels.h"
#include "Components/SphereComponent.h"

AVHVNPCCharacter::AVHVNPCCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	MovementComponent->MaxWalkSpeed = 300.0f;
	MovementComponent->BrakingDecelerationWalking = 2000.0f;

	AIControllerClass = AVHVNPCAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	InteractionComponent = CreateDefaultSubobject<UVHVInteractionComponent>(TEXT("InteractionComponent"));
	QuestParticipantComponent = CreateDefaultSubobject<UVHVQuestParticipantComponent>(TEXT("QuestParticipantComponent"));
	BehaviorComponent = CreateDefaultSubobject<UVHVNPCBehaviorComponent>(TEXT("BehaviorComponent"));
	NPCInteractionComponent = CreateDefaultSubobject<UVHVNPCInteractionComponent>(TEXT("NPCInteractionComponent"));
	DialogueComponent = CreateDefaultSubobject<UVHVNPCDialogueComponent>(TEXT("DialogueComponent"));
	PatrolComponent = CreateDefaultSubobject<UVHVNPCPatrolComponent>(TEXT("PatrolComponent"));
	QuestCommandComponent = CreateDefaultSubobject<UVHVNPCQuestCommandComponent>(TEXT("QuestCommandComponent"));

	InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
	InteractionCollision->SetupAttachment(GetRootComponent());
	InteractionCollision->InitSphereRadius(100.0f);
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionCollision->SetCollisionObjectType(VHV_INTERACTABLE_CHANNEL);
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionCollision->SetCollisionResponseToChannel(VHV_INTERACTION_TRACE_CHANNEL, ECR_Block);
	InteractionCollision->SetGenerateOverlapEvents(false);
	InteractionCollision->CanCharacterStepUpOn = ECB_No;
}

void AVHVNPCCharacter::BeginPlay()
{
	Super::BeginPlay();
	HomeTransform = GetActorTransform();
}

FTransform AVHVNPCCharacter::GetHomeTransform() const
{
	return HomeTransform;
}

void AVHVNPCCharacter::SetHomeTransform(const FTransform& NewHomeTransform)
{
	HomeTransform = NewHomeTransform;
}

UVHVInteractionComponent* AVHVNPCCharacter::GetInteractionComponent() const
{
	return InteractionComponent;
}

UVHVQuestParticipantComponent* AVHVNPCCharacter::GetQuestParticipantComponent() const
{
	return QuestParticipantComponent;
}

UVHVNPCBehaviorComponent* AVHVNPCCharacter::GetBehaviorComponent() const
{
	return BehaviorComponent;
}

UVHVNPCInteractionComponent* AVHVNPCCharacter::GetNPCInteractionComponent() const
{
	return NPCInteractionComponent;
}

UVHVNPCDialogueComponent* AVHVNPCCharacter::GetDialogueComponent() const
{
	return DialogueComponent;
}

USphereComponent* AVHVNPCCharacter::GetInteractionCollision() const
{
	return InteractionCollision;
}

UVHVNPCPatrolComponent* AVHVNPCCharacter::GetPatrolComponent() const
{
	return PatrolComponent;
}

UVHVNPCQuestCommandComponent* AVHVNPCCharacter::GetQuestCommandComponent() const
{
	return QuestCommandComponent;
}
