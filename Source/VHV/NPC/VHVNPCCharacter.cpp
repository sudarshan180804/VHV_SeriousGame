#include "NPC/VHVNPCCharacter.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NPC/Components/VHVNPCBehaviorComponent.h"
#include "NPC/Components/VHVAmbientSpeechComponent.h"
#include "NPC/Components/VHVNPCDialogueComponent.h"
#include "NPC/Components/VHVNPCInteractionComponent.h"
#include "NPC/Components/VHVNPCNameplateComponent.h"
#include "NPC/Components/VHVNPCPatrolComponent.h"
#include "NPC/Components/VHVNPCPresentationComponent.h"
#include "NPC/Components/VHVNPCQuestCommandComponent.h"
#include "NPC/Animation/VHVMixamoNPCAnimInstance.h"
#include "NPC/VHVNPCAIController.h"
#include "Player/Components/VHVInteractionComponent.h"
#include "Quest/Components/VHVQuestParticipantComponent.h"
#include "VHV/Core/VHVCollisionChannels.h"
#include "Components/SphereComponent.h"

#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

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
	PresentationComponent = CreateDefaultSubobject<UVHVNPCPresentationComponent>(TEXT("PresentationComponent"));
	QuestCommandComponent = CreateDefaultSubobject<UVHVNPCQuestCommandComponent>(TEXT("QuestCommandComponent"));
	AmbientSpeechComponent = CreateDefaultSubobject<UVHVAmbientSpeechComponent>(TEXT("AmbientSpeechComponent"));
	AmbientSpeechComponent->SetupAttachment(GetRootComponent());
	NameplateComponent = CreateDefaultSubobject<UVHVNPCNameplateComponent>(TEXT("NameplateComponent"));
	NameplateComponent->SetupAttachment(GetRootComponent());

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

void AVHVNPCCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyRequestedAnimationState(
#if WITH_EDITOR
		GetWorld() && !GetWorld()->IsGameWorld()
#else
		false
#endif
	);
}

void AVHVNPCCharacter::BeginPlay()
{
	Super::BeginPlay();
	HomeTransform = GetActorTransform();

	if (AnimationProfile)
	{
		CurrentPosture = bIsSitting ? EVHVNPCPosture::Sitting : AnimationProfile->InitialPosture;
		bSpecialAnimationActionActive = AnimationProfile->bUseSpecialActionOutsideDialogue;
		ApplyRequestedAnimationState();
	}
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

UVHVNPCPresentationComponent* AVHVNPCCharacter::GetPresentationComponent() const
{
	return PresentationComponent;
}

UVHVAmbientSpeechComponent* AVHVNPCCharacter::GetAmbientSpeechComponent() const
{
	return AmbientSpeechComponent;
}

UVHVNPCNameplateComponent* AVHVNPCCharacter::GetNameplateComponent() const
{
	return NameplateComponent;
}

UVHVNPCAnimationProfile* AVHVNPCCharacter::GetAnimationProfile() const
{
	return AnimationProfile;
}

void AVHVNPCCharacter::SetAnimationProfile(UVHVNPCAnimationProfile* NewAnimationProfile)
{
	AnimationProfile = NewAnimationProfile;
	CurrentPosture = bIsSitting
		? EVHVNPCPosture::Sitting
		: AnimationProfile ? AnimationProfile->InitialPosture : EVHVNPCPosture::Standing;
	ApplyRequestedAnimationState();
}

EVHVNPCPosture AVHVNPCCharacter::GetNPCPosture() const
{
	return CurrentPosture;
}

void AVHVNPCCharacter::SetNPCPosture(const EVHVNPCPosture NewPosture)
{
	CurrentPosture = NewPosture;
	if (UVHVMixamoNPCAnimInstance* AnimInstance = Cast<UVHVMixamoNPCAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->SetRequestedPosture(CurrentPosture);
	}
}

void AVHVNPCCharacter::SetIsSitting(const bool bNewIsSitting)
{
	bIsSitting = bNewIsSitting;
	CurrentPosture = bIsSitting
		? EVHVNPCPosture::Sitting
		: AnimationProfile ? AnimationProfile->InitialPosture : EVHVNPCPosture::Standing;
	ApplyRequestedAnimationState(
#if WITH_EDITOR
		GetWorld() && !GetWorld()->IsGameWorld()
#else
		false
#endif
	);
}

void AVHVNPCCharacter::ApplyRequestedAnimationState(const bool bPreviewInEditor)
{
	if (bIsSitting && AnimationProfile && !AnimationProfile->SittingIdleAnimation)
	{
		UE_LOG(LogAnimation, Warning,
			TEXT("VHV NPC '%s' requested Is Sitting, but profile '%s' has no compatible Sitting Idle; using Standing Idle."),
			*GetName(), *GetNameSafe(AnimationProfile));
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		return;
	}

#if WITH_EDITOR
	if (bPreviewInEditor && MeshComponent->GetAnimationMode() == EAnimationMode::AnimationBlueprint)
	{
		MeshComponent->InitAnim(true);
	}
#endif

	if (UVHVMixamoNPCAnimInstance* AnimInstance =
		Cast<UVHVMixamoNPCAnimInstance>(MeshComponent->GetAnimInstance()))
	{
		AnimInstance->SetAnimationProfile(AnimationProfile);
		AnimInstance->SetRequestedPosture(CurrentPosture);
		AnimInstance->SetSpecialActionActive(bSpecialAnimationActionActive);
	}

#if WITH_EDITOR
	if (bPreviewInEditor)
	{
		MeshComponent->TickAnimation(1.0f / 30.0f, false);
		MeshComponent->RefreshBoneTransforms();
		MeshComponent->MarkRenderDynamicDataDirty();
	}
#endif
}

#if WITH_EDITOR
void AVHVNPCCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AVHVNPCCharacter, bIsSitting)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(AVHVNPCCharacter, AnimationProfile))
	{
		CurrentPosture = bIsSitting
			? EVHVNPCPosture::Sitting
			: AnimationProfile ? AnimationProfile->InitialPosture : EVHVNPCPosture::Standing;
		ApplyRequestedAnimationState(true);
	}
}
#endif

void AVHVNPCCharacter::SetSpecialAnimationActionActive(const bool bActive)
{
	bSpecialAnimationActionActive = bActive;
	if (UVHVMixamoNPCAnimInstance* AnimInstance = Cast<UVHVMixamoNPCAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInstance->SetSpecialActionActive(bSpecialAnimationActionActive);
	}
}

bool AVHVNPCCharacter::IsSpecialAnimationActionActive() const
{
	return bSpecialAnimationActionActive;
}
