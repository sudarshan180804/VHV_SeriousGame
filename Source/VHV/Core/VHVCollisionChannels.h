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
// We use GameTraceChannel2 for interaction traces and GameTraceChannel3 for interactable objects.
// ============================================================

#define VHV_INTERACTION_TRACE_CHANNEL ECC_GameTraceChannel2
#define VHV_INTERACTABLE_CHANNEL ECC_GameTraceChannel3
