#include "NPC/Components/VHVNPCPresentationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "NPC/Presentation/VHVNPCActionSet.h"
#include "VHV.h"

UVHVNPCPresentationComponent::UVHVNPCPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UVHVNPCPresentationComponent::PlayAction(const FName ActionID)
{
    if (ActionID.IsNone() || !ActionSet)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Presentation action '%s' rejected on '%s': invalid Action ID or missing Action Set."),
            *ActionID.ToString(), *GetNameSafe(GetOwner()));
        return false;
    }

    FVHVNPCPresentationAction Action;
    if (!ActionSet->GetAction(ActionID, Action) || !Action.Montage)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Presentation action '%s' is not valid in Action Set '%s' for '%s'."),
            *ActionID.ToString(), *GetNameSafe(ActionSet), *GetNameSafe(GetOwner()));
        return false;
    }

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance)
    {
        UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Presentation action '%s' failed on '%s': no AnimInstance."),
            *ActionID.ToString(), *GetNameSafe(GetOwner()));
        return false;
    }

    if (!CurrentActionID.IsNone())
    {
        StopCurrentAction();
    }

    CurrentActionID = ActionID;
    CurrentMontage = Action.Montage;
    ActivePlaybackSerial = ++PlaybackSerial;
    const float Duration = AnimInstance->Montage_Play(Action.Montage, Action.PlayRate);
    if (Duration <= 0.0f)
    {
        FinishCurrentAction(false, false);
        UE_LOG(LogVHV, Warning, TEXT("[VHVNPC] Presentation action '%s' failed to play on '%s'."),
            *ActionID.ToString(), *GetNameSafe(GetOwner()));
        return false;
    }

    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UVHVNPCPresentationComponent::HandleMontageEnded, ActivePlaybackSerial);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, Action.Montage);
    return true;
}

void UVHVNPCPresentationComponent::StopCurrentAction()
{
    StopCurrentActionInternal(true);
}

void UVHVNPCPresentationComponent::StopCurrentActionSilently()
{
    StopCurrentActionInternal(false);
}

bool UVHVNPCPresentationComponent::IsActionPlaying() const
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    return !CurrentActionID.IsNone() && CurrentMontage && AnimInstance && AnimInstance->Montage_IsPlaying(CurrentMontage);
}

FName UVHVNPCPresentationComponent::GetCurrentActionID() const
{
    return CurrentActionID;
}

void UVHVNPCPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopCurrentActionSilently();
    Super::EndPlay(EndPlayReason);
}

void UVHVNPCPresentationComponent::StopCurrentActionInternal(const bool bBroadcastCompletion)
{
    if (CurrentActionID.IsNone())
    {
        return;
    }

    UAnimMontage* MontageToStop = CurrentMontage;
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UAnimInstance* AnimInstance = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
    FinishCurrentAction(false, bBroadcastCompletion);
    if (AnimInstance && MontageToStop)
    {
        AnimInstance->Montage_Stop(0.2f, MontageToStop);
    }
}

void UVHVNPCPresentationComponent::FinishCurrentAction(const bool bSuccess, const bool bBroadcastCompletion)
{
    if (CurrentActionID.IsNone())
    {
        return;
    }

    const FName CompletedActionID = CurrentActionID;
    CurrentActionID = NAME_None;
    CurrentMontage = nullptr;
    ActivePlaybackSerial = 0;
    if (bBroadcastCompletion)
    {
        OnPresentationActionCompleted.Broadcast(CompletedActionID, bSuccess);
    }
}

void UVHVNPCPresentationComponent::HandleMontageEnded(
    UAnimMontage* Montage,
    const bool bInterrupted,
    const uint32 CompletedPlaybackSerial)
{
    if (!CurrentMontage || Montage != CurrentMontage || CompletedPlaybackSerial != ActivePlaybackSerial)
    {
        return;
    }
    FinishCurrentAction(!bInterrupted, true);
}
