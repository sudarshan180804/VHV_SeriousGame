#include "NPC/Animation/VHVMixamoNPCAnimInstance.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "NPC/VHVNPCCharacter.h"

namespace
{
	struct FVHVMixamoNPCAnimNode final : FAnimNode_Base
	{
		FAnimNode_SequencePlayer_Standalone StandingPlayer;
		FAnimNode_SequencePlayer_Standalone WalkingPlayer;
		FAnimNode_SequencePlayer_Standalone RunningPlayer;
		FAnimNode_SequencePlayer_Standalone SittingPlayer;
		FAnimNode_SequencePlayer_Standalone SpecialActionPlayer;
		FAnimNode_TwoWayBlend StandingWalkBlend;
		FAnimNode_TwoWayBlend WalkRunBlend;
		FAnimNode_TwoWayBlend PostureBlend;
		FAnimNode_TwoWayBlend SpecialActionBlend;

		float MovementAlpha = 0.0f;
		float RunningAlpha = 0.0f;
		float PostureAlpha = 0.0f;
		float SpecialActionAlpha = 0.0f;
		bool bWasSpecialActionRequested = false;
		bool bConfigured = false;

		void LinkNodes()
		{
			StandingWalkBlend.A.SetLinkNode(&StandingPlayer);
			StandingWalkBlend.B.SetLinkNode(&WalkingPlayer);
			WalkRunBlend.A.SetLinkNode(&StandingWalkBlend);
			WalkRunBlend.B.SetLinkNode(&RunningPlayer);
			PostureBlend.A.SetLinkNode(&WalkRunBlend);
			PostureBlend.B.SetLinkNode(&SittingPlayer);
			SpecialActionBlend.A.SetLinkNode(&PostureBlend);
			SpecialActionBlend.B.SetLinkNode(&SpecialActionPlayer);
			StandingWalkBlend.AlphaInputType = EAnimAlphaInputType::Float;
			WalkRunBlend.AlphaInputType = EAnimAlphaInputType::Float;
			PostureBlend.AlphaInputType = EAnimAlphaInputType::Float;
			SpecialActionBlend.AlphaInputType = EAnimAlphaInputType::Float;
		}

		void Configure(
			UAnimSequence* Standing,
			UAnimSequence* Walking,
			UAnimSequence* Running,
			UAnimSequence* Sitting,
			UAnimSequence* SpecialAction,
			const float GroundSpeed,
			const float IdleWalkThreshold,
			const float WalkPlaybackSpeed,
			const float RunSpeedThreshold,
			const float RunPlaybackSpeed,
			const float TransitionBlendTime,
			const EVHVNPCPosture Posture,
			const bool bSpecialActionActive,
			const bool bLoopSpecialAction,
			const float SpecialActionPlaybackSpeed,
			const float DeltaSeconds)
		{
			SetSequenceIfChanged(StandingPlayer, Standing);
			SetSequenceIfChanged(WalkingPlayer, Walking ? Walking : Standing);
			SetSequenceIfChanged(RunningPlayer, Running ? Running : (Walking ? Walking : Standing));
			SetSequenceIfChanged(SittingPlayer, Sitting ? Sitting : Standing);
			SetSequenceIfChanged(SpecialActionPlayer, SpecialAction ? SpecialAction : Standing);

			StandingPlayer.SetLoopAnimation(true);
			StandingPlayer.SetPlayRate(1.0f);
			WalkingPlayer.SetLoopAnimation(true);
			WalkingPlayer.SetPlayRate(FMath::Max(WalkPlaybackSpeed, UE_SMALL_NUMBER));
			RunningPlayer.SetLoopAnimation(true);
			RunningPlayer.SetPlayRate(FMath::Max(RunPlaybackSpeed, UE_SMALL_NUMBER));
			SittingPlayer.SetLoopAnimation(true);
			SittingPlayer.SetPlayRate(1.0f);
			SpecialActionPlayer.SetLoopAnimation(bLoopSpecialAction);
			SpecialActionPlayer.SetPlayRate(FMath::Max(SpecialActionPlaybackSpeed, UE_SMALL_NUMBER));

			const float TargetMovementAlpha = Walking && GroundSpeed > IdleWalkThreshold ? 1.0f : 0.0f;
			const float TargetRunningAlpha = Running && GroundSpeed >= RunSpeedThreshold ? 1.0f : 0.0f;
			const float TargetPostureAlpha = Sitting && Posture == EVHVNPCPosture::Sitting
				&& GroundSpeed <= IdleWalkThreshold ? 1.0f : 0.0f;
			const float TargetSpecialActionAlpha = SpecialAction && bSpecialActionActive ? 1.0f : 0.0f;
			if (TargetSpecialActionAlpha > 0.0f && !bWasSpecialActionRequested)
			{
				SpecialActionPlayer.SetAccumulatedTime(0.0f);
			}
			bWasSpecialActionRequested = TargetSpecialActionAlpha > 0.0f;
			if (!bConfigured || TransitionBlendTime <= UE_SMALL_NUMBER)
			{
				MovementAlpha = TargetMovementAlpha;
				RunningAlpha = TargetRunningAlpha;
				PostureAlpha = TargetPostureAlpha;
				SpecialActionAlpha = TargetSpecialActionAlpha;
				bConfigured = true;
			}
			else
			{
				const float BlendSpeed = 1.0f / TransitionBlendTime;
				MovementAlpha = FMath::FInterpConstantTo(
					MovementAlpha, TargetMovementAlpha, DeltaSeconds, BlendSpeed);
				RunningAlpha = FMath::FInterpConstantTo(
					RunningAlpha, TargetRunningAlpha, DeltaSeconds, BlendSpeed);
				PostureAlpha = FMath::FInterpConstantTo(
					PostureAlpha, TargetPostureAlpha, DeltaSeconds, BlendSpeed);
				SpecialActionAlpha = FMath::FInterpConstantTo(
					SpecialActionAlpha, TargetSpecialActionAlpha, DeltaSeconds, BlendSpeed);
			}

			StandingWalkBlend.Alpha = MovementAlpha;
			WalkRunBlend.Alpha = RunningAlpha;
			PostureBlend.Alpha = PostureAlpha;
			SpecialActionBlend.Alpha = SpecialActionAlpha;
		}

		virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override
		{
			LinkNodes();
			StandingPlayer.SetAccumulatedTime(0.0f);
			WalkingPlayer.SetAccumulatedTime(0.0f);
			RunningPlayer.SetAccumulatedTime(0.0f);
			SittingPlayer.SetAccumulatedTime(0.0f);
			SpecialActionPlayer.SetAccumulatedTime(0.0f);
			SpecialActionBlend.Initialize_AnyThread(Context);
		}

		virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
		{
			SpecialActionBlend.CacheBones_AnyThread(Context);
		}

		virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override
		{
			SpecialActionBlend.Update_AnyThread(Context);
		}

		virtual void Evaluate_AnyThread(FPoseContext& Output) override
		{
			SpecialActionBlend.Evaluate_AnyThread(Output);
		}

	private:
		static void SetSequenceIfChanged(
			FAnimNode_SequencePlayer_Standalone& Player,
			UAnimSequence* Sequence)
		{
			if (Player.GetSequence() != Sequence)
			{
				Player.SetSequence(Sequence);
				Player.SetAccumulatedTime(0.0f);
			}
		}
	};
}

struct FVHVMixamoNPCAnimInstanceProxy final : FAnimInstanceProxy
{
	FVHVMixamoNPCAnimInstanceProxy() = default;

	explicit FVHVMixamoNPCAnimInstanceProxy(UAnimInstance* Instance)
		: FAnimInstanceProxy(Instance)
	{
	}

	virtual void Initialize(UAnimInstance* InAnimInstance) override
	{
		FAnimInstanceProxy::Initialize(InAnimInstance);
		FAnimationInitializeContext InitializeContext(this);
		RootNode.Initialize_AnyThread(InitializeContext);
	}

	virtual void InitializeObjects(UAnimInstance* InAnimInstance) override
	{
		FAnimInstanceProxy::InitializeObjects(InAnimInstance);
		const UVHVMixamoNPCAnimInstance* Instance = CastChecked<UVHVMixamoNPCAnimInstance>(InAnimInstance);
		AnimationProfile = Instance->AnimationProfile;
	}

	virtual void ClearObjects() override
	{
		FAnimInstanceProxy::ClearObjects();
		AnimationProfile = nullptr;
	}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, const float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		const UVHVMixamoNPCAnimInstance* Instance = CastChecked<UVHVMixamoNPCAnimInstance>(InAnimInstance);
		AnimationProfile = Instance->AnimationProfile;

		UAnimSequence* Standing = nullptr;
		UAnimSequence* Walking = nullptr;
		UAnimSequence* Running = nullptr;
		UAnimSequence* Sitting = nullptr;
		UAnimSequence* SpecialAction = nullptr;
		float IdleWalkThreshold = 5.0f;
		float WalkPlaybackSpeed = 1.0f;
		float RunSpeedThreshold = 350.0f;
		float RunPlaybackSpeed = 1.0f;
		float TransitionBlendTime = 0.2f;
		bool bLoopSpecialAction = false;
		float SpecialActionPlaybackSpeed = 1.0f;
		if (AnimationProfile)
		{
			Standing = AnimationProfile->StandingIdleAnimation;
			Walking = AnimationProfile->WalkingAnimation;
			Running = AnimationProfile->RunningAnimation;
			Sitting = AnimationProfile->SittingIdleAnimation;
			SpecialAction = AnimationProfile->SpecialActionAnimation;
			IdleWalkThreshold = AnimationProfile->IdleWalkThreshold;
			WalkPlaybackSpeed = AnimationProfile->WalkPlaybackSpeed;
			RunSpeedThreshold = AnimationProfile->RunSpeedThreshold;
			RunPlaybackSpeed = AnimationProfile->RunPlaybackSpeed;
			TransitionBlendTime = AnimationProfile->TransitionBlendTime;
			bLoopSpecialAction = AnimationProfile->bLoopSpecialAction;
			SpecialActionPlaybackSpeed = AnimationProfile->SpecialActionPlaybackSpeed;
		}

		RootNode.Configure(
			Standing,
			Walking,
			Running,
			Sitting,
			SpecialAction,
			Instance->GroundSpeed,
			IdleWalkThreshold,
			WalkPlaybackSpeed,
			RunSpeedThreshold,
			RunPlaybackSpeed,
			TransitionBlendTime,
			Instance->RequestedPosture,
			Instance->bSpecialActionActive,
			bLoopSpecialAction,
			SpecialActionPlaybackSpeed,
			DeltaSeconds);
	}

	virtual void CacheBones() override
	{
		FAnimationCacheBonesContext CacheContext(this);
		RootNode.CacheBones_AnyThread(CacheContext);
	}

	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override
	{
		RootNode.Update_AnyThread(InContext);
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		RootNode.Evaluate_AnyThread(Output);
		return true;
	}

	FVHVMixamoNPCAnimNode RootNode;
	TObjectPtr<UVHVNPCAnimationProfile> AnimationProfile;
};

void UVHVMixamoNPCAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	RefreshFromOwningNPC();
}

void UVHVMixamoNPCAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	RefreshFromOwningNPC();

	const APawn* Pawn = TryGetPawnOwner();
	if (Pawn)
	{
		const FVector Velocity = Pawn->GetVelocity();
		GroundSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
	}
	else
	{
		GroundSpeed = 0.0f;
	}

	const float Threshold = AnimationProfile ? AnimationProfile->IdleWalkThreshold : 5.0f;
	CurrentAnimationState = bSpecialActionActive && AnimationProfile && AnimationProfile->SpecialActionAnimation
		? EVHVMixamoAnimationState::SpecialAction
		: AnimationProfile && AnimationProfile->RunningAnimation
			&& GroundSpeed >= AnimationProfile->RunSpeedThreshold
			? EVHVMixamoAnimationState::Running
			: GroundSpeed > Threshold && AnimationProfile && AnimationProfile->WalkingAnimation
				? EVHVMixamoAnimationState::Walking
				: RequestedPosture == EVHVNPCPosture::Sitting && AnimationProfile && AnimationProfile->SittingIdleAnimation
					? EVHVMixamoAnimationState::SittingIdle
					: EVHVMixamoAnimationState::StandingIdle;

	if (!bProfileWarningIssued && AnimationProfile && !ValidateProfileForCurrentMesh())
	{
		UE_LOG(LogAnimation, Error,
			TEXT("VHV Mixamo profile '%s' is incomplete or incompatible with mesh '%s'."),
			*GetNameSafe(AnimationProfile), *GetNameSafe(GetSkelMeshComponent()->GetSkeletalMeshAsset()));
		bProfileWarningIssued = true;
	}
}

void UVHVMixamoNPCAnimInstance::SetAnimationProfile(UVHVNPCAnimationProfile* NewProfile)
{
	if (AnimationProfile != NewProfile)
	{
		AnimationProfile = NewProfile;
		bProfileWarningIssued = false;
	}
}

UVHVNPCAnimationProfile* UVHVMixamoNPCAnimInstance::GetAnimationProfile() const
{
	return AnimationProfile;
}

void UVHVMixamoNPCAnimInstance::SetRequestedPosture(const EVHVNPCPosture NewPosture)
{
	RequestedPosture = NewPosture;
}

EVHVNPCPosture UVHVMixamoNPCAnimInstance::GetRequestedPosture() const
{
	return RequestedPosture;
}

void UVHVMixamoNPCAnimInstance::SetSpecialActionActive(const bool bActive)
{
	bSpecialActionActive = bActive;
}

bool UVHVMixamoNPCAnimInstance::IsSpecialActionActive() const
{
	return bSpecialActionActive;
}

EVHVMixamoAnimationState UVHVMixamoNPCAnimInstance::GetCurrentAnimationState() const
{
	return CurrentAnimationState;
}

float UVHVMixamoNPCAnimInstance::GetGroundSpeed() const
{
	return GroundSpeed;
}

FAnimInstanceProxy* UVHVMixamoNPCAnimInstance::CreateAnimInstanceProxy()
{
	return new FVHVMixamoNPCAnimInstanceProxy(this);
}

void UVHVMixamoNPCAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete static_cast<FVHVMixamoNPCAnimInstanceProxy*>(InProxy);
}

void UVHVMixamoNPCAnimInstance::RefreshFromOwningNPC()
{
	if (const AVHVNPCCharacter* NPC = Cast<AVHVNPCCharacter>(TryGetPawnOwner()))
	{
		SetAnimationProfile(NPC->GetAnimationProfile());
		SetRequestedPosture(NPC->GetNPCPosture());
		SetSpecialActionActive(NPC->IsSpecialAnimationActionActive());
	}
}

bool UVHVMixamoNPCAnimInstance::ValidateProfileForCurrentMesh() const
{
	const USkeletalMeshComponent* MeshComponent = GetSkelMeshComponent();
	const USkeletalMesh* Mesh = MeshComponent ? MeshComponent->GetSkeletalMeshAsset() : nullptr;
	return AnimationProfile && Mesh
		&& AnimationProfile->IsCompatibleWithSkeleton(Mesh->GetSkeleton());
}
