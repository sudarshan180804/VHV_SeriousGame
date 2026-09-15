#pragma once

#include "CoreMinimal.h"

// ============================================================
// VHV CUSTOM COLLISION CHANNELS
// ============================================================
//
// Keep these mappings synchronized with:
// Project Settings → Engine → Collision
//
// Current project configuration:
//
// GameTraceChannel1 = SoftCollision
// GameTraceChannel2 = VHV_Interaction
// GameTraceChannel3 = VHV_Interactable
//
// We use GameTraceChannel3 for interactable objects.
// ============================================================

#define VHV_INTERACTABLE_CHANNEL ECC_GameTraceChannel3