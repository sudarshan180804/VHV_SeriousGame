"""Safe, reusable integration helper for profile-driven VHV Mixamo NPCs.

Typical editor-console usage:
    import vhv_mixamo_npc_integration as vhv_mixamo
    vhv_mixamo.apply_mixamo_npc_animation(
        skeletal_mesh="/Game/Characters/Uncle/Uncle_Mixamo",
        animation_profile="/Game/Characters/Uncle/DA_Uncle_Animations",
        actor_label="HBCT_SS_NPC_UncleChai",
        compatible_anim_blueprint="/Game/Characters/Uncle/ABP_VHV_Mixamo_NPC_UncleSkeleton",
        mesh_alignment={"location": (0, 0, -96), "rotation": (0, -90, 0)},
        level="/Game/VHV_Stuff/Maps/Lvl_Village_Main",
        save=True)
"""

import unreal


def _asset(value, expected_type, label):
    result = unreal.EditorAssetLibrary.load_asset(value) if isinstance(value, str) else value
    if not result or not isinstance(result, expected_type):
        raise ValueError("%s is missing or has the wrong type: %s" % (label, value))
    return result


def _vector(value, fallback):
    if value is None:
        return fallback
    if isinstance(value, unreal.Vector):
        return value
    return unreal.Vector(float(value[0]), float(value[1]), float(value[2]))


def _rotator(value, fallback):
    if value is None:
        return fallback
    if isinstance(value, unreal.Rotator):
        return value
    return unreal.Rotator(
        pitch=float(value[0]), yaw=float(value[1]), roll=float(value[2]))


def _snapshot_transform(actor):
    return {
        "location": actor.get_actor_location(),
        "rotation": actor.get_actor_rotation(),
        "scale": actor.get_actor_scale3d(),
    }


def _nearly_equal_vector(a, b, tolerance=0.001):
    return (abs(a.x - b.x) <= tolerance and abs(a.y - b.y) <= tolerance
            and abs(a.z - b.z) <= tolerance)


def _nearly_equal_rotator(a, b, tolerance=0.001):
    return (abs(a.pitch - b.pitch) <= tolerance and abs(a.yaw - b.yaw) <= tolerance
            and abs(a.roll - b.roll) <= tolerance)


def _find_actor(label):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    matches = [actor for actor in actors if actor.get_actor_label() == label]
    if len(matches) != 1:
        raise RuntimeError("Expected exactly one actor labelled '%s'; found %d" % (label, len(matches)))
    return matches[0]


def validate_mixamo_compatibility(skeletal_mesh, animation_profile, compatible_anim_blueprint):
    """Return validated assets, or raise before making any actor changes."""
    mesh = _asset(skeletal_mesh, unreal.SkeletalMesh, "Skeletal Mesh")
    profile = _asset(animation_profile, unreal.VHVNPCAnimationProfile, "Animation Profile")
    anim_bp = _asset(compatible_anim_blueprint, unreal.AnimBlueprint, "Animation Blueprint")
    skeleton = mesh.get_editor_property("skeleton")
    if not skeleton:
        raise ValueError("Skeletal Mesh has no skeleton")
    if not profile.has_all_required_animations():
        raise ValueError("Animation Profile must contain a Standing sequence")
    if not profile.is_compatible_with_skeleton(skeleton):
        raise ValueError("One or more assigned profile animations use a different skeleton than the mesh")
    if anim_bp.get_editor_property("target_skeleton") != skeleton:
        raise ValueError(
            "Animation Blueprint target skeleton differs from the mesh. Create a skeleton-specific "
            "AnimBP facade with parent class VHVMixamoNPCAnimInstance; do not force skeletons together.")
    generated_class = anim_bp.generated_class()
    if not generated_class:
        raise ValueError("Animation Blueprint has no generated class")
    return mesh, profile, anim_bp, generated_class


def apply_mixamo_npc_animation(
        skeletal_mesh,
        animation_profile,
        actor_label,
        compatible_anim_blueprint,
        mesh_alignment=None,
        level=None,
        save=False):
    """Replace only an existing VHV NPC's visual/animation configuration.

    mesh_alignment is optional and may contain location, rotation, and scale tuples.
    Actor transform, capsule dimensions, and all gameplay components are verified unchanged.
    """
    mesh, profile, anim_bp, anim_class = validate_mixamo_compatibility(
        skeletal_mesh, animation_profile, compatible_anim_blueprint)
    if level:
        if not unreal.EditorLevelLibrary.load_level(level):
            raise RuntimeError("Could not load level: " + level)
    actor = _find_actor(actor_label)
    if not isinstance(actor, unreal.VHVNPCCharacter):
        raise TypeError("Actor is not a BP_VHVNPCBase/VHVNPCCharacter instance: " + actor_label)

    actor_before = _snapshot_transform(actor)
    capsule = actor.get_editor_property("capsule_component")
    capsule_before = (capsule.get_unscaled_capsule_radius(), capsule.get_unscaled_capsule_half_height())
    gameplay_before = {
        "participant": actor.get_quest_participant_component(),
        "dialogue": actor.get_dialogue_component(),
        "behavior": actor.get_behavior_component(),
        "interaction": actor.get_npc_interaction_component(),
        "patrol": actor.get_patrol_component(),
    }

    component = actor.get_editor_property("mesh")
    component.set_skinned_asset_and_update(mesh)
    component.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_BLUEPRINT)
    component.set_anim_instance_class(anim_class)
    actor.set_animation_profile(profile)
    actor.set_npc_posture(unreal.VHVNPCPosture.STANDING)

    if mesh_alignment:
        current_location = component.get_editor_property("relative_location")
        current_rotation = component.get_editor_property("relative_rotation")
        current_scale = component.get_editor_property("relative_scale3d")
        component.set_relative_location(
            _vector(mesh_alignment.get("location"), current_location), False, False)
        component.set_relative_rotation(
            _rotator(mesh_alignment.get("rotation"), current_rotation), False, False)
        component.set_relative_scale3d(
            _vector(mesh_alignment.get("scale"), current_scale))

    actor_after = _snapshot_transform(actor)
    capsule_after = (capsule.get_unscaled_capsule_radius(), capsule.get_unscaled_capsule_half_height())
    gameplay_after = {
        "participant": actor.get_quest_participant_component(),
        "dialogue": actor.get_dialogue_component(),
        "behavior": actor.get_behavior_component(),
        "interaction": actor.get_npc_interaction_component(),
        "patrol": actor.get_patrol_component(),
    }
    transform_preserved = (
        _nearly_equal_vector(actor_before["location"], actor_after["location"])
        and _nearly_equal_vector(actor_before["scale"], actor_after["scale"])
        and _nearly_equal_rotator(actor_before["rotation"], actor_after["rotation"]))
    gameplay_preserved = all(gameplay_before[key] == gameplay_after[key] for key in gameplay_before)
    if not transform_preserved or capsule_before != capsule_after or not gameplay_preserved:
        raise RuntimeError("Safety verification failed; actor transform, capsule, or gameplay component changed")

    if save:
        unreal.EditorLevelLibrary.save_current_level()

    report = {
        "actor": actor.get_path_name(),
        "mesh": mesh.get_path_name(),
        "profile": profile.get_path_name(),
        "anim_blueprint": anim_bp.get_path_name(),
        "skeleton": mesh.get_editor_property("skeleton").get_path_name(),
        "actor_transform_preserved": transform_preserved,
        "capsule_preserved": capsule_before == capsule_after,
        "gameplay_components_preserved": gameplay_preserved,
        "saved": bool(save),
    }
    unreal.log_warning("VHV_MIXAMO_INTEGRATION=" + str(report))
    return report
