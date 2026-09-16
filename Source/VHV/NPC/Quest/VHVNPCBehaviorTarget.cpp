#include "NPC/Quest/VHVNPCBehaviorTarget.h"

#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Quest/Systems/VHVQuestSubsystem.h"
#include "VHV.h"

AVHVNPCBehaviorTarget::AVHVNPCBehaviorTarget()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AVHVNPCBehaviorTarget::BeginPlay()
{
	Super::BeginPlay();
	if (TargetID.IsNone())
	{
		UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Behavior target '%s' has an empty Target ID."), *GetNameSafe(this));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UVHVQuestSubsystem* QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>())
			{
				QuestSubsystem->RegisterNPCBehaviorTarget(this);
			}
		}
	}
}

void AVHVNPCBehaviorTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			if (UVHVQuestSubsystem* QuestSubsystem = GameInstance->GetSubsystem<UVHVQuestSubsystem>())
			{
				QuestSubsystem->UnregisterNPCBehaviorTarget(this);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}
