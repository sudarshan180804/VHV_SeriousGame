# VHV Authoring Guide

## Purpose

Use this guide to implement the single Health Behaviour Change Technique (HBCT) curriculum unit after its storyboard is approved. VHV is a modular storyboard sandbox: quests sequence learner goals, dialogue and activities present content, Story State carries decisions forward, and NPC/world components stage the scene.

```text
Storyboard beat
  -> choose a quest objective
  -> reference participants, locations, actions, or activities with Gameplay Tags
  -> author dialogue/activity content
  -> add Story State only when the result affects a later beat
  -> test the beat in PIE
```

## Storyboard Beat Map

| Storyboard beat | Existing VHV implementation |
|---|---|
| Talk to an NPC | `Talk` objective, Participant dropdown, conversation asset; player interacts with that NPC. |
| Run an automatically opened conversation | `Conversation` objective, conversation asset, optional entry node, `Auto Start` enabled. |
| Require a general interaction | `Interact` objective and Participant dropdown. It completes when that participant reports interaction. |
| NPC walks to a marked place | `NPCAction` + `MoveToTarget`, Participant dropdown, Behavior Target dropdown, and a placed `VHVNPCBehaviorTarget`. |
| NPC waits | `NPCAction` + `Wait`, Participant dropdown, and wait duration. |
| NPC returns to its home transform | `NPCAction` + `ReturnToPost`. |
| NPC performs a gesture | `NPCAction` + `PlayAction`, Participant dropdown, NPC Action dropdown, and an Action Set montage entry. |
| NPC returns to normal patrol ownership | `NPCAction` + `ReleaseToPatrol`. |
| Player enters an area | `ReachLocation` objective plus a placed and resized `VHVQuestLocationVolume` with the same Location tag. |
| A board, door, light, or prop changes | `WorldAction` objective with matching World Receiver and World Action tags. |
| Start a learning exercise | `LearningActivity` objective with an Activity dropdown. Enable `Auto Start` for immediate launch; otherwise a matching participant interaction can request activation. |
| Complete from another gameplay event | `CustomEvent` objective and a matching custom-event notification from existing gameplay logic. |
| Branch dialogue using an earlier result | Put a Story State condition on the later dialogue node or choice. |
| Record a learner choice | Put Story State effects on the selected dialogue option. |
| Record a correct/incorrect activity result | Put effects in the activity's `SuccessEffects` or `FailureEffects`. |
| Delay an objective until state is ready | Add objective `ActivationConditions`. The quest reevaluates when Story State changes. |
| Persist a stable checkpoint | Call the Save subsystem's `SaveProgress`; load on the same map with `LoadProgress`. |

## Quest Authoring

Quest arcs contain ordered quests; quests contain ordered objectives. Each objective has a freeform `ObjectiveID`, player-facing `ObjectiveText`, one objective type, optional activation conditions, and completion effects.

| Objective type | Authoring and completion |
|---|---|
| `Talk` | Requires a conversation and participant. Starts through matching NPC interaction and completes when the conversation completes. |
| `Conversation` | Requires a conversation; participant is optional. Can auto-start and completes with the conversation. |
| `LearningActivity` | Requires an Activity reference. Can auto-start or be launched by a matching participant interaction; completes when that activity completes. |
| `Interact` | Requires a Participant reference; completes on matching participant interaction. |
| `ReachLocation` | Requires a Location reference; completes when the quest subsystem receives that location arrival. |
| `CustomEvent` | Requires a Custom Event reference; completes on the matching event notification. |
| `NPCAction` | Dispatches one quest-owned NPC command and completes only when its matching command succeeds. |
| `WorldAction` | Dispatches one receiver/action request and completes only when its matching request succeeds. |

Use objectives for meaningful learner goals or visible staging steps:

- Good: "Speak with the instructor," "Practice reflective listening," "Observe the demonstration," "Enter the clinic."
- Good when sequencing matters: "Instructor moves to the teaching spot," then "Instructor explains."
- Avoid one objective per dialogue line, camera beat, facial reaction, or minor animation.

`ActivationConditions` block an objective before it activates. `CompletionEffects` apply once through normal completion before the next objective activates. Objective completion then advances the quest, refreshes the tracker, and may start the next quest according to the authored auto-start settings.

## Dialogue and Choices

A conversation asset contains a `ConversationID`, `StartNodeID`, and ordered nodes. Node IDs and next-node IDs are local freeform graph identifiers.

Node types currently present:

- `Text`: displays speaker/text and advances through `NextNodeID`.
- `Choice`: displays authored options.
- `LearningActivity`: launches its linked textbook activity.
- `Unknown`: exists in the enum but should not be used for production content.

For each node:

- Use `SpeakerTag` for a catalogued participant and author the visible `SpeakerName` separately.
- `ActivationConditions` are checked before display. A failing node is skipped through its existing `NextNodeID`; skipped nodes apply no effects. Cycles and missing nodes stop traversal with an error.
- `CompletionEffects` apply once when the node is actually completed, before traversal to the next node.
- A checkpoint node stores its effective Checkpoint ID for exit/resume and Save/Load.
- Exiting with Q commits resume/checkpoint state but does not complete the current node or apply its effects.

Choice options support:

- `AvailabilityConditions`: unavailable options are filtered by the existing dialogue UI flow.
- `SelectionEffects`: apply once after the option is selected and before following its `NextNodeID`.
- `OptionID` and `NextNodeID`: local freeform IDs.

`DialogueChoice` also exists as a learning activity type. It points to an existing conversation and choice node; it does not define a second choice system.

## Learning Activities

Activity data lives in level/topic data and currently supports:

| Type | Authored content |
|---|---|
| `SingleChoice` | Question plus one correct option. |
| `MultiChoice` | Question with multiple correct options. |
| `Ordering` | Items and an exact correct order; current runtime allows up to three submitted attempts. |
| `Matching` | Left/right pairs; current runtime allows up to three submitted attempts. |
| `Observation` | Observation media; submission is treated as completion rather than a correctness judgment. |
| `DialogueChoice` | Reference to a conversation choice node. |

The learning flow uses Ask, Hint, Feedback, and Teach phases. `SuccessEffects` and `FailureEffects` run once per definitive submitted result. Partial multi-choice/matching results do not apply either array. Observation and DialogueChoice activities do not apply correctness-result arrays; choice selection effects belong to the dialogue option.

For quest-controlled curriculum progression, reference the activity from a `LearningActivity` objective. Keep display text, question IDs, answer text, ordering item IDs, and other activity-local data freeform.

## Story State

Story State contains flags and integer counters. Conditions read state; effects change it.

| Tool | Use it for | Example |
|---|---|---|
| Flag | A durable yes/no fact. | `VHV.Story.Flag.MetInstructor` |
| Counter | A durable numeric value. | `VHV.Story.Counter.ActivityAttempts` |
| Condition | Gate a quest objective, dialogue node, or dialogue choice. | Show feedback dialogue only when `NeedsFeedback` is set. |
| Effect | Record an objective completion, node completion, choice, or evaluated activity result. | Set `UsedGoodTechnique` after the learner chooses the strong response. |

Guidelines:

- Create state only when information affects a later quest, node, choice, activity, or ending.
- Do not create a flag for every dialogue line or presentation beat.
- Prefer a flag for a durable fact and a counter when quantity or repeated attempts matters.
- Use `VHV.Story.Flag.*` tags for flag operations and `VHV.Story.Counter.*` tags for counter operations.
- Empty condition sets pass. Multiple conditions use the authored `All` or `Any` match mode.

## NPC Authoring

The reusable native base is `AVHVNPCCharacter`; the project also contains `BP_VHVNPCBase`. Create production NPCs as Blueprint children/configurations, such as `BP_Instructor` and `BP_Patient01`. New C++ NPC subclasses should not be needed for normal storyboard content.

Expected setup:

1. Create or duplicate a child of `BP_VHVNPCBase`.
2. Set the Quest Participant component's Participant dropdown.
3. Assign the Dialogue component's default conversation when direct interaction should open dialogue.
4. If patrol is needed, place/configure a patrol route, assign it to the Patrol component, and choose whether patrol auto-starts.
5. If gestures are needed, create/assign a `UVHVNPCActionSet` on the Presentation component.
6. Use the same Participant tag from quest objectives that address this NPC.

### Patrol and interaction

- Patrol routes contain actor waypoints, optional wait durations, and `Once` or `Loop` mode.
- Interaction can face the NPC toward the player before dialogue.
- Talking/engaging pauses normal patrol behavior. The existing interaction flow restores NPC behavior when dialogue ends.

### Quest commands

Quest commands currently support `MoveToTarget`, `Wait`, `ReturnToPost`, `ReleaseToPatrol`, and `PlayAction`. Quest ownership pauses patrol and remains in force across commands. `ReleaseToPatrol` is the explicit ownership release.

### Presentation actions

An Action Set maps an NPC Action tag to one montage and an optional play rate. Different NPCs may map the same logical tag to different montages.

```text
DA_InstructorActions
  VHV.NPCAction.Wave -> instructor wave montage

DA_PatientActions
  VHV.NPCAction.Wave -> patient wave montage
```

Only one presentation montage is active at a time. Natural completion reports success; interruption reports failure to the quest-command layer. Dialogue remains authoritative when it interrupts a presentation action.

## World Actions

Add `VHVWorldActionReceiverComponent` to the target Blueprint and select its World Receiver tag. Implement the existing **Handle World Action** Blueprint event for received action IDs.

- Return `Completed` for synchronous success.
- Return `StartedAsync` when work continues, then call `FinishWorldAction(RequestID, bSuccess)` exactly once.
- Return `Rejected` for unsupported requests.

A quest `WorldAction` objective uses matching World Receiver and World Action dropdowns. The objective completes only after the exact request reports success. The system does not discover actors by scan; receivers register with the World Action subsystem.

## Location Triggers

Place `VHVQuestLocationVolume`, resize its Box component, select its Location tag, and leave `Enabled` true. Create a `ReachLocation` objective using the same tag.

```text
Placed volume: VHV.Location.Clinic
Quest objective: ReachLocation -> VHV.Location.Clinic
```

The volume detects the player pawn, notifies once per entry, and may notify again after the player exits and re-enters. The quest subsystem decides whether the current objective matches and should complete.

## Save / Load

`UVHVSaveSubsystem` uses slot `VHV_Progress`, save version 1, and exposes `SaveProgress`, `LoadProgress`, `DoesSaveExist`, `DeleteSave`, `OnSaveCompleted`, and `OnLoadCompleted`.

Saved data includes:

- Quest arc/quest/objective progress and completed IDs.
- Story flags and counters.
- Meaningful textbook progress.
- Dialogue checkpoint/resume state.
- Current map name.

Load requires the player to already be on the saved map. Save and load are rejected during active dialogue, an active learning activity, or an executing NPC/World Action objective. The system does not save widgets, temporary input state, AI movement, world transforms, or open modal state.

Restore order is Story State silently, textbook state, dialogue checkpoints, then quest state. The restored active objective is activated through the existing path against the coherent restored state.

## Quest Tracker

`WBP_QuestTracker` displays the tracked active quest title and current objective text. It listens for quest/tracking updates and hides when no active tracked quest exists. Author concise `QuestTitle` and `ObjectiveText`; do not put implementation IDs in learner-facing text.

## Gameplay Tag Catalog

The centralized dropdown catalog is:

`Config/DefaultGameplayTags.ini`

Current categories:

| Category | Shared reference |
|---|---|
| `VHV.Participant` | NPC/participant identity |
| `VHV.Location` | Location objectives and volumes |
| `VHV.BehaviorTarget` | NPC movement targets |
| `VHV.NPCAction` | Presentation montage actions |
| `VHV.WorldReceiver` | World Action receiver identity |
| `VHV.WorldAction` | World Action request identity |
| `VHV.Activity` | Learning activities referenced across assets |
| `VHV.Story.Flag` | Story flags |
| `VHV.Story.Counter` | Story counters |
| `VHV.CustomEvent` | Cross-system custom quest events |
| `VHV.Checkpoint` | Dialogue checkpoints |

Authoring rule:

```text
Record-local definition ID -> freeform
Shared catalog identity/reference -> filtered Gameplay Tag dropdown
```

Examples of freeform definition/local IDs are `QuestID`, `ObjectiveID`, `ConversationID`, `NodeID`, `NextNodeID`, and choice `OptionID`. Catalog-backed activity and Action Set entries use their matching tag because other assets reference those identities.

To add a shared option:

1. Add one `+GameplayTagList` entry under the existing settings section in `DefaultGameplayTags.ini`.
2. Put it under the correct hierarchy, for example `VHV.Location.HealthPost`.
3. Reload/restart the Editor if the tag list is already cached.
4. Select the new option from every matching dropdown; do not retype it into each asset.

New tag fields take precedence. Advanced legacy name fields remain for existing assets; leave them empty for new tagged content. If both are set differently, validation warns and the tag wins. Runtime and SaveGame continue using the resolved stable name.

## Naming Conventions

| Content | Convention/example |
|---|---|
| Unit | `HBCT` |
| Quest | `HBCT_Q01` |
| Objectives | `HBCT_Q01_OBJ_01`, `HBCT_Q01_OBJ_02` |
| Dialogue asset | `DA_Dialogue_HBCT_Intro` |
| Level/activity data | `DA_Level_HBCT` |
| Action Set | `DA_InstructorActions` |
| NPC Blueprints | `BP_Instructor`, `BP_Patient01` |
| Behavior targets | Select `VHV.BehaviorTarget.*` |
| Locations | Select `VHV.Location.*` |

Do not rename existing smoke-test assets while creating production content.

## Content Organization

Use this production structure:

```text
Content/VHV_Stuff/HBCT/
    Quests/
    Dialogue/
    Activities/
    NPCs/
    Animations/
    World/
    UI/
```

Keep `Content/VHV_Stuff/Dev/QuestSmoke/` as development/test content. Do not place production curriculum assets there or make production assets depend on smoke-test content.

## HBCT Production Workspace Setup

Run `Content/Python/setup_hbct_content.py` once from Unreal Editor's **Tools > Execute Python Script** workflow before production storyboard authoring. The script is idempotent: running it again preserves existing folders and assets without resetting, renaming, replacing, or deleting content.

It prepares:

```text
/Game/VHV_Stuff/HBCT/
    Quests/
    Dialogue/
    Activities/
    NPCs/
    Animations/
    World/
        Locations/
        BehaviorTargets/
        WorldActions/
    UI/
    Maps/
```

When the native classes are available to Unreal Python, the script also creates empty `DA_QuestArc_HBCT`, `DA_Level_HBCT`, and `DA_InstructorActions` DataAssets if they are missing. It does not populate storyboard content. `Content/VHV_Stuff/Dev/QuestSmoke/` remains test-only and is never touched by this setup.

## PIE Debugging Workflow

**COMPILED - RUNTIME VERIFICATION PENDING**

Developer Tools exist in Editor/Development builds and are unavailable in Shipping. Commands accept resolved runtime IDs; for tag-authored content, use the full tag name.

| Command | Purpose |
|---|---|
| `vhv.status` | Log active arc, quest, objective ID/text, activation wait state, Story State counts, and save existence. |
| `vhv.quest.complete` | Force the active objective through normal completion/progression. |
| `vhv.quest.restart` | Reset the active authored quest to its first objective without map reload. |
| `vhv.story.setflag <FlagID>` | Set a flag; example: `vhv.story.setflag VHV.Story.Flag.NeedsFeedback`. |
| `vhv.story.clearflag <FlagID>` | Clear a flag. |
| `vhv.story.setcounter <CounterID> <Value>` | Set a counter to an integer. |
| `vhv.story.addcounter <CounterID> <Delta>` | Add an integer delta. |
| `vhv.world.action <ReceiverID> <ActionID>` | Send a debug World Action request. |
| `vhv.save` | Call the existing Save subsystem. |
| `vhv.load` | Call the existing Load subsystem. |

Suggested beat test:

```text
Start PIE
  -> vhv.status
  -> play the beat normally
  -> inspect Output Log
  -> use Story State commands to test blocked branches
  -> use vhv.quest.complete only to reach later beats quickly
  -> test save/load only from a stable gameplay state
```

Forced completion uses normal completion effects and progression. Restart clears that quest's completed-objective progress but does not reset Story State. Treat these commands as test conveniences, not player-facing behavior.

## Production Authoring Checklist

- Place production assets under `Content/VHV_Stuff/HBCT/`.
- Add missing shared catalog tags once, then use dropdowns consistently.
- Give quests/objectives concise freeform IDs and useful learner-facing text.
- Confirm every participant, location, target, action, receiver, and activity reference matches its defining component/asset.
- Add Story State only for facts needed later.
- Ensure conditional dialogue nodes have a valid `NextNodeID` skip path.
- Ensure asynchronous World Actions always finish their request.
- Use `ReleaseToPatrol` when the storyboard returns an NPC to normal behavior.
- Save only at stable points outside dialogue, activities, and executing action objectives.
- Validate assets before PIE and inspect warnings for tag/legacy conflicts.

## Verification Status

### VERIFIED previously

- NPC patrol/movement/dialogue interruption.
- Quest NPC commands.
- Story State basics and quest integration.
- Activity Story State effects.
- Dialogue Story State conditions/effects.
- World Actions synchronous/asynchronous.
- Location triggers.
- Save/Load basic restoration.
- NPC presentation Wave montage.

### COMPILED BUT RUNTIME VERIFICATION PENDING

- Developer console tools.
- Gameplay Tag dropdown authoring conversion.
