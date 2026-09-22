#include "NPC/Components/VHVAmbientSpeechComponent.h"

#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/Ambient/VHVAmbientSpeechStyle.h"
#include "UI/Ambient/VHVAmbientSpeechWidget.h"
#include "Ambient/VHVAmbientSpeechLog.h"

UVHVAmbientSpeechComponent::UVHVAmbientSpeechComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    // Set this on the component archetype so an immediate ambient auto-start does not
    // depend on this component's BeginPlay running before the coordinator's callback.
    SetWidgetClass(UVHVAmbientSpeechWidget::StaticClass());
    SetWidgetSpace(EWidgetSpace::Screen);
    SetDrawAtDesiredSize(true);
    SetPivot(FVector2D(0.5f, 1.0f));
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetWindowFocusable(false);
    SetVisibility(false);
}

void UVHVAmbientSpeechComponent::BeginPlay()
{
    Super::BeginPlay();
    EnsureSpeechWidget();
    ApplyAnchor();
    SetVisibility(false);
}

void UVHVAmbientSpeechComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HideTimerHandle);
    SpeechWidget = nullptr;
    Super::EndPlay(EndPlayReason);
}

bool UVHVAmbientSpeechComponent::ShowBubble(const FText SpeakerName, const FText Text,
    const EVHVAmbientSpeechType SpeechType, const bool bShowSpeakerName)
{
    if (!EnsureSpeechWidget()) return false;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(HideTimerHandle);
    bBubbleActive = true;
    SetVisibility(true);
    SetComponentTickEnabled(true);
    SpeechWidget->ShowBubble(SpeakerName, Text, SpeechType, bShowSpeakerName);
    const float InitialOpacity = CalculateDistanceOpacity();
    SpeechWidget->SetDistanceOpacity(InitialOpacity);
    FVector2D ScreenPosition;
    const bool bProjected = GetAnchorScreenPosition(ScreenPosition);
    UE_LOG(LogVHVAmbientSpeech, Log,
        TEXT("Speech component on '%s' presented bubble: componentVisible=%s, active=%s, opacity=%.2f, projected=%s, screen=(%.0f, %.0f), world=%s."),
        *GetNameSafe(GetOwner()), IsVisible() ? TEXT("true") : TEXT("false"),
        bBubbleActive ? TEXT("true") : TEXT("false"), InitialOpacity,
        bProjected ? TEXT("true") : TEXT("false"), ScreenPosition.X, ScreenPosition.Y,
        *GetComponentLocation().ToCompactString());
    return true;
}

bool UVHVAmbientSpeechComponent::EnsureSpeechWidget()
{
    if (!GetWidgetClass())
    {
        SetWidgetClass(UVHVAmbientSpeechWidget::StaticClass());
    }
    if (!SpeechWidget)
    {
        InitWidget();
        SpeechWidget = Cast<UVHVAmbientSpeechWidget>(GetUserWidgetObject());
    }
    if (SpeechWidget)
    {
        // Build and cache the Slate tree before ShowBubble mutates it. Otherwise the
        // first screen-space render can rebuild the tree and reset its root to collapsed,
        // which loses the only line in a one-line ambient conversation.
        SpeechWidget->TakeWidget();
    }
    return SpeechWidget != nullptr;
}

void UVHVAmbientSpeechComponent::HideBubble(const bool bImmediate)
{
    if (!bBubbleActive && !bImmediate) return;
    bBubbleActive = false;
    if (SpeechWidget) SpeechWidget->HideBubble(bImmediate);
    if (!GetWorld() || bImmediate)
    {
        FinishHide();
        return;
    }
    GetWorld()->GetTimerManager().SetTimer(HideTimerHandle, this,
        &UVHVAmbientSpeechComponent::FinishHide, VHVAmbientSpeechStyle::DisappearDuration, false);
}

void UVHVAmbientSpeechComponent::SetSeparationOffset(const float OffsetY)
{
    if (SpeechWidget) SpeechWidget->SetSeparationOffset(FMath::Clamp(OffsetY, -64.0f, 64.0f));
}

bool UVHVAmbientSpeechComponent::GetAnchorScreenPosition(FVector2D& OutScreenPosition) const
{
    APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    return PC && PC->ProjectWorldLocationToScreen(GetComponentLocation(), OutScreenPosition, true);
}

void UVHVAmbientSpeechComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!SpeechWidget || !IsVisible()) return;
    if (bBubbleActive)
    {
        SpeechWidget->SetDistanceOpacity(CalculateDistanceOpacity());
    }
    SpeechWidget->TickPresentation(DeltaTime);
}

void UVHVAmbientSpeechComponent::ApplyAnchor()
{
    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (USkeletalMeshComponent* Mesh = Character->GetMesh())
        {
            const FName ResolvedSocket = ResolveHeadSocket(Mesh);
            if (!ResolvedSocket.IsNone())
            {
                AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, ResolvedSocket);
                SetRelativeLocation(FVector(BubbleHorizontalOffset, 0.0f, HeadSocketVerticalOffset));
                return;
            }
        }
    }
    SetRelativeLocation(FVector(BubbleHorizontalOffset, 0.0f, BubbleHeightOffset));
}

FName UVHVAmbientSpeechComponent::ResolveHeadSocket(const USkeletalMeshComponent* Mesh) const
{
    if (!Mesh) return NAME_None;
    if (!AnchorSocketName.IsNone() && Mesh->DoesSocketExist(AnchorSocketName)) return AnchorSocketName;
    if (!bPreferHeadSocket) return NAME_None;

    static const FName HeadCandidates[] =
    {
        TEXT("head"),
        TEXT("HeadSocket"),
        TEXT("head_socket")
    };
    for (const FName Candidate : HeadCandidates)
    {
        if (Mesh->DoesSocketExist(Candidate)) return Candidate;
    }
    return NAME_None;
}

void UVHVAmbientSpeechComponent::FinishHide()
{
    SetVisibility(false);
    SetComponentTickEnabled(false);
}

float UVHVAmbientSpeechComponent::CalculateDistanceOpacity() const
{
    const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    const APlayerCameraManager* Camera = PC ? PC->PlayerCameraManager : nullptr;
    if (!Camera) return 0.0f;
    const float Distance = FVector::Distance(Camera->GetCameraLocation(), GetComponentLocation());
    if (Distance >= MaxVisibleDistance || (MinVisibleDistance > 0.0f && Distance < MinVisibleDistance)) return 0.0f;
    float Opacity = Distance <= FadeStartDistance
        ? 1.0f
        : 1.0f - FMath::Clamp((Distance - FadeStartDistance) / FMath::Max(1.0f, MaxVisibleDistance - FadeStartDistance), 0.0f, 1.0f);

    if (bHideSignificantlyOffscreen)
    {
        FVector2D Screen;
        int32 SizeX = 0;
        int32 SizeY = 0;
        if (!PC->ProjectWorldLocationToScreen(GetComponentLocation(), Screen, true)) return 0.0f;
        PC->GetViewportSize(SizeX, SizeY);
        const float MarginX = SizeX * 0.08f;
        const float MarginY = SizeY * 0.10f;
        if (Screen.X < -MarginX || Screen.X > SizeX + MarginX || Screen.Y < -MarginY || Screen.Y > SizeY + MarginY)
        {
            return 0.0f;
        }
    }
    return Opacity;
}
