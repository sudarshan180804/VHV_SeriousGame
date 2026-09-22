#include "World/Components/VHVWorldActionReceiverComponent.h"

#include "World/Systems/VHVWorldActionSubsystem.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UVHVWorldActionReceiverComponent::UVHVWorldActionReceiverComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UVHVWorldActionReceiverComponent::BeginPlay()
{
    Super::BeginPlay();
    if (IsWorldActionRoutingEnabled())
    {
        RegisterWithWorldActionSubsystem();
    }
}

void UVHVWorldActionReceiverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UnregisterFromWorldActionSubsystem();
    Super::EndPlay(EndPlayReason);
}

EVHVWorldActionExecutionResult UVHVWorldActionReceiverComponent::HandleWorldAction_Implementation(
    const FGuid RequestID,
    const FName ActionID)
{
    DispatchingRequestID = RequestID;
    DispatchingResult = EVHVWorldActionExecutionResult::Rejected;
    bDispatchingWorldAction = true;
    bDispatchResponseProvided = false;
    OnWorldActionRequested.Broadcast(RequestID, ActionID);
    bDispatchingWorldAction = false;
    DispatchingRequestID.Invalidate();
    return DispatchingResult;
}

EVHVWorldActionExecutionResult UVHVWorldActionReceiverComponent::ExecuteWorldAction(
    const FGuid RequestID,
    const FName ActionID)
{
    if (!IsWorldActionRoutingEnabled())
    {
        return EVHVWorldActionExecutionResult::Rejected;
    }
    return HandleWorldAction(RequestID, ActionID);
}

bool UVHVWorldActionReceiverComponent::FinishWorldAction(const FGuid RequestID, const bool bSuccess)
{
    UWorld* World = GetWorld();
    UVHVWorldActionSubsystem* Subsystem = World ? World->GetSubsystem<UVHVWorldActionSubsystem>() : nullptr;
    return Subsystem && Subsystem->CompleteWorldActionForReceiver(RequestID, bSuccess, this);
}

bool UVHVWorldActionReceiverComponent::SetWorldActionExecutionResult(
    const FGuid RequestID,
    const EVHVWorldActionExecutionResult Result)
{
    if (!bDispatchingWorldAction || bDispatchResponseProvided || RequestID != DispatchingRequestID)
    {
        return false;
    }
    DispatchingResult = Result;
    bDispatchResponseProvided = true;
    return true;
}

void UVHVWorldActionReceiverComponent::RegisterWithWorldActionSubsystem()
{
    if (!IsWorldActionRoutingEnabled())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        if (UVHVWorldActionSubsystem* Subsystem = World->GetSubsystem<UVHVWorldActionSubsystem>())
        {
            Subsystem->RegisterReceiver(this);
        }
    }
}

void UVHVWorldActionReceiverComponent::UnregisterFromWorldActionSubsystem()
{
    if (UWorld* World = GetWorld())
    {
        if (UVHVWorldActionSubsystem* Subsystem = World->GetSubsystem<UVHVWorldActionSubsystem>())
        {
            Subsystem->UnregisterReceiver(this);
        }
    }
}

#if WITH_EDITOR
EDataValidationResult UVHVWorldActionReceiverComponent::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);
    if (!IsWorldActionRoutingEnabled())
    {
        return Result;
    }

    if (GetEffectiveReceiverID().IsNone())
    {
        Context.AddError(FText::FromString(FString::Printf(TEXT("World Action receiver on '%s' has no effective Receiver ID."), *GetNameSafe(GetOwner()))));
        Result = EDataValidationResult::Invalid;
    }
    if (!VHVAuthoringReferences::IsValidReferenceTag(ReceiverTag, TEXT("VHV.WorldReceiver")))
    {
        Context.AddError(FText::FromString(FString::Printf(TEXT("World Action receiver on '%s' uses tag '%s', which must be a concrete tag beneath VHV.WorldReceiver."), *GetNameSafe(GetOwner()), *ReceiverTag.ToString())));
        Result = EDataValidationResult::Invalid;
    }
    if (VHVAuthoringReferences::HasConflict(ReceiverTag, ReceiverID, TEXT("VHV.WorldReceiver")))
    {
        Context.AddWarning(FText::FromString(FString::Printf(TEXT("World Action receiver on '%s' has tag '%s', which resolves to '%s', while legacy ReceiverID is '%s'; the tag wins."), *GetNameSafe(GetOwner()), *ReceiverTag.ToString(), *VHVAuthoringReferences::ResolveTagLeaf(ReceiverTag).ToString(), *ReceiverID.ToString())));
    }
    return Result;
}
#endif
