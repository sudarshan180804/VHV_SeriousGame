import unreal


ASSET_PATH = "/Game/VHV_Stuff/Dev/AmbientSpeech"


def participant(slot, name):
    value = unreal.VHVAmbientParticipantDefinition()
    value.set_editor_property("slot_id", slot)
    value.set_editor_property("speaker_name", name)
    return value


def line(slot, text, speech_type, duration, look_at=False):
    value = unreal.VHVAmbientSpeechLine()
    value.set_editor_property("speaker_slot", slot)
    value.set_editor_property("text", text)
    value.set_editor_property("speech_type", speech_type)
    value.set_editor_property("display_duration", duration)
    value.set_editor_property("look_at_other_participant", look_at)
    return value


def create_asset(name, conversation_id, participants, lines, show_names):
    full_path = f"{ASSET_PATH}/{name}"
    existing = unreal.EditorAssetLibrary.load_asset(full_path) if unreal.EditorAssetLibrary.does_asset_exist(full_path) else None
    if existing:
        asset = existing
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.VHVAmbientConversationData)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, ASSET_PATH, unreal.VHVAmbientConversationData, factory)
    if not asset:
        raise RuntimeError(f"Could not create {full_path}")
    asset.set_editor_property("conversation_id", conversation_id)
    asset.set_editor_property("participants", participants)
    asset.set_editor_property("lines", lines)
    asset.set_editor_property("show_speaker_name", show_names)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    return asset


speech = unreal.VHVAmbientSpeechType.SPEECH
thought = unreal.VHVAmbientSpeechType.THOUGHT

two_npc_asset = create_asset(
    "DA_AmbientTest_TwoNPC",
    "AmbientTest_TwoNPC",
    [participant("A", "AUNT MALI"), participant("B", "MARKET VENDOR")],
    [
        line("A", "Hot today, isn't it?", speech, 2.5, True),
        line("B", "Very. I've been drinking sweet tea all morning.", speech, 3.0, True),
        line("A", "Didn't the health worker ask you to reduce sugar?", speech, 3.0, True),
        line("B", "I know... but changing habits is harder than knowing what to do.", speech, 4.0, True),
    ],
    True,
)

thought_asset = create_asset(
    "DA_AmbientTest_Thought",
    "AmbientTest_Thought",
    [participant("A", "AUNT MALI")],
    [line("A", "That ice cream looks good... maybe just one.", thought, 3.5)],
    False,
)

assert len(two_npc_asset.get_editor_property("participants")) == 2
assert len(two_npc_asset.get_editor_property("lines")) == 4
assert len(thought_asset.get_editor_property("participants")) == 1
assert len(thought_asset.get_editor_property("lines")) == 1
assert thought_asset.get_editor_property("lines")[0].get_editor_property("speech_type") == thought

unreal.log("Created VHV ambient speech development assets.")
