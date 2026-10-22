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
		FAnimNode_SequencePlayer_Standalone SittingPlayer;
		FAnimNode_TwoWayBlend StandingWalkBlend;
		FAnimNode_TwoWayBlend PostureBlend;

		float MovementAlpha = 0.0f;
		float PostureAlpha = 0.0f;
		bool bConfigured = false;

		void LinkNodes()
		{
			StandingWalkBlend.A.SetLinkNode(&StandingPlayer);
			StandingWalkBlend.B.SetLinkNode(&WalkingPlayer);
			PostureBlend.A.SetLinkNode(&StandingWalkBlend);
			PostureBlend.B.SetLinkNode(&SittingPlayer);
			StandingWalkBlend.AlphaInputType = EAnimAlphaInputType::Float;
			PostureBlend.AlphaInputType = EAnimAlphaInputType::Float;
		}

		void Configure(
			UAnimSequence* Standing,
			UAnimSequence* Walking,
			UAnimSequence* Sitting,
			const float GroundSpeed,
			const float IdleWalkThreshold,
			const float WalkPlaybackSpeed,
			const float TransitionBlendTime,
			const EVHVNPCPosture Posture,
			const float DeltaSeconds)
		{
			SetSequenceIfChanged(StandingPlayer, Standing);
			SetSequenceIfChanged(WalkingPlayer, Walking);
			SetSequenceIfChanged(SittingPlayer, Sitting);

			StandingPlayer.SetLoopAnimation(true);
			StandingPlayer.SetPlayRate(1.0f);
			WalkingPlayer.SetLoopAnimation(true);
			WalkingPlayer.SetPlayRate(FMath::Max(WalkPlaybackSpeed, UE_SMALL_NUMBER));
			SittingPlayer.SetLoopAnimation(true);
			SittingPlayer.SetPlayRate(1.0f);

			const float TargetMovementAlpha = GroundSpeed > IdleWalkThreshold ? 1.0f : 0.0f;
			const float TargetPostureAlpha = Posture == EVHVNPCPosture::Sitting ? 1.0f : 0.0f;
			if (!bConfigured || TransitionBlendTime <= UE_SMALL_NUMBER)
			{
				MovementAlpha = TargetMovementAlpha;
				PostureAlpha = TargetPostureAlpha;
				bConfigured = true;
			}
			else
			{
				const float BlendSpeed = 1.0f / TransitionBlendTime;
				MovementAlpha = FMath::FInterpConstantTo(
					MovementAlpha, TargetMovementAlpha, DeltaSeconds, BlendSpeed);
				PostureAlpha = FMath::FInterpConstantTo(
					PostureAlpha, TargetPostureAlpha, DeltaSeconds, BlendSpeed);
			}

			StandingWalkBlend.Alpha = MovementAlpha;
			PostureBlend.Alpha = PostureAlpha;
		}

		virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override
		{
			LinkNodes();
			StandingPlayer.SetAccumulatedTime(0.0f);
			WalkingPlayer.SetAccumulatedTime(0.0f);
			SittingPlayer.SetAccumulatedTime(0.0f);
			PostureBlend.Initialize_AnyThread(Context);
		}

		virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override
		{
			PostureBlend.CacheBones_AnyThread(Context);
		}

		virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override
		{
			PostureBlend.Update_AnyThread(Context);
		}

		virtual void Evaluate_AnyThread(FPoseContext& Output) override
		{
			PostureBlend.Evaluate_AnyThread(Output);
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
		UAnimSequence* Sitting = nullptr;
		float IdleWalkThreshold = 5.0f;
		float WalkPlaybackSpeed = 1.0f;
		float TransitionBlendTime = 0.2f;
		if (AnimationProfile)
		{
			Standing = AnimationProfile->StandingIdleAnimation;
			Walking = AnimationProfile->WalkingAnimation;
			Sitting = AnimationProfile->SittingIdleAnimation;
			IdleWalkThreshold = AnimationProfile->IdleWalkThreshold;
			WalkPlaybackSpeed = AnimationProfile->WalkPlaybackSpeed;
			TransitionBlendTime = AnimationProfile->TransitionBlendTime;
		}

		RootNode.Configure(
			Standing,
			Walking,
			Sitting,
			Instance->GroundSpeed,
			IdleWalkThreshold,
			WalkPlaybackSpeed,
			TransitionBlendTime,
			Instance->RequestedPosture,
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
	CurrentAnimationState = RequestedPosture == EVHVNPCPosture::Sitting
		? EVHVMixamoAnimationState::SittingIdle
		: (GroundSpeed > Threshold
			? EVHVMixamoAnimationState::Walking
			: EVHVMixamoAnimationState::StandingIdle);

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
	}
}

bool UVHVMixamoNPCAnimInstance::ValidateProfileForCurrentMesh() const
{
	const USkeletalMeshComponent* MeshComponent = GetSkelMeshComponent();
	const USkeletalMesh* Mesh = MeshComponent ? MeshComponent->GetSkeletalMeshAsset() : nullptr;
	return AnimationProfile && Mesh
		&& AnimationProfile->IsCompatibleWithSkeleton(Mesh->GetSkeleton());
}
