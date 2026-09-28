#pragma once

#include "Components/WidgetComponent.h"
#include "VHVNPCNameplateComponent.generated.h"

class USkeletalMeshComponent;
class UVHVNPCNameplateWidget;

/** Reusable screen-space name label for named NPCs. */
UCLASS(ClassGroup=(VHV), meta=(BlueprintSpawnableComponent))
class VHV_API UVHVNPCNameplateComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UVHVNPCNameplateComponent();

	/** Player-facing authored name. Empty names remain hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Nameplate")
	FText NameplateDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Nameplate")
	bool bShowNameplate = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Nameplate|Anchor")
	bool bPreferHeadSocket = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Nameplate|Anchor")
	FName AnchorSocketName;

	/** World-up padding above either the resolved head socket or the capsule top fallback. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Nameplate|Anchor", meta = (Units = "Centimeters"))
	float NameplateHeadPadding = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Nameplate|Visibility", meta = (ClampMin = "0.0", Units = "Centimeters"))
	float NameplateMaxVisibleDistance = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VHV|NPC|Nameplate|Visibility", meta = (ClampMin = "0.05", Units = "Seconds"))
	float VisibilityRefreshInterval = 0.12f;

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Nameplate")
	void RefreshNameplate();

	UFUNCTION(BlueprintCallable, Category = "VHV|NPC|Nameplate")
	void SetNameplateDisplayName(const FText& InDisplayName);

protected:
	virtual void InitWidget() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVHVNPCNameplateWidget> NameplateWidget;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> AnchorMesh;

	FName ResolvedHeadSocket;
	float VisibilityRefreshElapsed = 0.0f;

	bool EnsureNameplateWidget();
	void ResolveAnchor();
	void UpdateAnchorLocation();
	void UpdateNameplateVisibility();
	bool ShouldShowNameplate() const;
	FName ResolveHeadSocket(const USkeletalMeshComponent* Mesh) const;
};
