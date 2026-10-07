"""Idempotent production authoring for the central objective-location tracker.

Owns only IA_LocateObjective, its X mapping, the beam material, the player
controller action reference, and tracking metadata on Quest 1-6 objectives.
"""

import traceback
import unreal


LOG = "[VHV Objective Tracking]"
ARC_PATH = "/Game/VHV_Stuff/HBCT/Quests/DA_QuestArc_HBCT"
INPUT_ACTION_PATH = "/Game/Input/Actions/IA_LocateObjective"
INPUT_CONTEXT_PATH = "/Game/Input/IMC_Default"
PLAYER_CONTROLLER_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"
MATERIAL_DIR = "/Game/VHV_Stuff/UI/Tracking"
MATERIAL_PATH = MATERIAL_DIR + "/M_VHV_ObjectiveTrackingBeam"
MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout"


TRACKING_LOCATIONS = {
    "Q_HBCT_01_MOTIVATION": {
        "O01_ReachPrasertHouse": "VHV.Location.HBCT.PrasertHouse",
        "O02_ObservePrasert": "VHV.Location.HBCT.PrasertHouse",
        "O04_ObserveBadGoodDemo": "VHV.Location.HBCT.BadGoodDemo",
        "O06_VisitSaeng": "VHV.Location.HBCT.SaengHouse",
        "O07_ObserveAppreciation": "VHV.Location.HBCT.SaengHouse",
        "O09_ReachMaliHouse": "VHV.Location.HBCT.MaliHouse",
        "O10_FindWhatMatters": "VHV.Location.HBCT.MaliHouse",
        "O11_FindObstacle": "VHV.Location.HBCT.MaliHouse",
        "O13_SummarizePlan": "VHV.Location.HBCT.MaliHouse",
        "O14_FinalDebrief": "VHV.Location.HBCT.VillageEntrance",
    },
    "Q_HBCT_02_GOAL_SETTING": {
        "O01_ReachHealthPost": "VHV.Location.HBCT.HealthPost",
        "O02_ObserveGoalConversation": "VHV.Location.HBCT.GoalSetting.InitialObservation",
        "O04_LearnGoalSetting": "VHV.Location.HBCT.HealthPost",
        "O05_ComparePatients": "VHV.Location.HBCT.GoalSetting.ClearGoal",
        "O06_ReflectOnDirection": "VHV.Location.HBCT.HealthPost",
        "O07_ObserveGoalMethods": "VHV.Location.HBCT.GoalSetting.SelfSet",
        "O08_IdentifyGoalMethods": "VHV.Location.HBCT.HealthPost",
        "O09_EvaluateGoals": "VHV.Location.HBCT.GoalSetting.GoodGoalBoard",
        "O10_ReturnToMali": "VHV.Location.HBCT.GoalSetting.MaliCheckIn",
        "O15_ReviewGoalPlan": "VHV.Location.HBCT.GoalSetting.MaliPlanReview",
        "O15B_RoadsidePractice": "VHV.Location.HBCT.GoalSetting.RoadsidePractice",
        "O16_FinalDebrief": "VHV.Location.HBCT.GoalSetting.FinalInstructor",
    },
    "Q_HBCT_03_ROLE_MODEL": {
        "O01_ReachExercisePark": "VHV.Location.HBCT.RoleModel.Entry",
        "O02_DiscoverRoleModels": "VHV.Location.HBCT.RoleModel.ExerciseObservation",
        "O03_ClassifyModels": "VHV.Location.HBCT.ExercisePark",
        "O04_MeetSomchai": "VHV.Location.HBCT.RoleModel.SomchaiIntro",
        "O06_InspectCandidates": "VHV.Location.HBCT.RoleModel.AthleteCandidate",
        "O09_SeeChangeOverTime": "VHV.Location.HBCT.RoleModel.Timeline.Before",
        "O11_PrepareDemonstration": "VHV.Location.HBCT.ExercisePark",
        "O12_ObserveSaeng": "VHV.Location.HBCT.RoleModel.SaengDemo",
        "O13_SupportImitation": "VHV.Location.HBCT.RoleModel.SomchaiDemo",
        "O14_PracticeTogether": "VHV.Location.HBCT.RoleModel.SaengDemo",
        "O16_FinalDebrief": "VHV.Location.HBCT.ExercisePark",
    },
    "Q_HBCT_04_SOCIAL_SUPPORT": {
        "O01_ObserveVillageSupport": "VHV.Location.HBCT.SocialSupport.SSMealScene",
        "O02_ReflectOnSupport": "VHV.Location.HBCT.SocialSupport.SSInstructorCatchup",
        "O03_ExploreSupportHouse": "VHV.Location.HBCT.SocialSupport.SSRoomEmotional",
        "O04_MeetUncleChai": "VHV.Location.HBCT.SocialSupport.SSChaiHome",
        "O05_FindChaiSupporters": "VHV.Location.HBCT.SocialSupport.SSChaiWifeRecruit",
        "O06_BuildSupportNetwork": "VHV.Location.HBCT.SocialSupport.SSNetworkBoard",
        "O07_WatchSupportInAction": "VHV.Location.HBCT.SocialSupport.SSPlanInAction",
        "O08_WalkWithChai": "VHV.Location.HBCT.SocialSupport.SSChaiWalkStart",
        "O09_SupportDisruption": "VHV.Location.HBCT.SocialSupport.SSDisruption",
        "O10_FindBackupSupport": "VHV.Location.HBCT.SocialSupport.SSBackupWife",
        "O11_OneMonthLater": "VHV.Location.HBCT.SocialSupport.SSOneMonthLater",
        "O12_ProvideAppraisalSupport": "VHV.Location.HBCT.SocialSupport.SSOneMonthLater",
        "O13_FinalDebrief": "VHV.Location.HBCT.SocialSupport.SSFinalDebrief",
    },
    "Q_HBCT_05_SELF_MONITORING": {
        "O01_CheckInWithChai": "VHV.Location.HBCT.SelfMonitoring.SMChaiHome",
        "O02_LearnWhyRecordsMatter": "VHV.Location.HBCT.SelfMonitoring.SMInstructorLesson",
        "O03_IdentifyTargetBehavior": "VHV.Location.HBCT.SelfMonitoring.SMTargetBehaviorArea",
        "O04_BuildMonitoringProcess": "VHV.Location.HBCT.SelfMonitoring.SMMonitoringProcess",
        "O05_ChooseMonitoringMethod": "VHV.Location.HBCT.SelfMonitoring.SMMethodStation",
        "O06_PrepareChaiTracker": "VHV.Location.HBCT.SelfMonitoring.SMTracker",
        "O07_RecordBreakfast": "VHV.Location.HBCT.SelfMonitoring.SMBreakfast",
        "O08_RecordAfternoon": "VHV.Location.HBCT.SelfMonitoring.SMAfternoon",
        "O09_RecordEveningWalk": "VHV.Location.HBCT.SelfMonitoring.SMChaiWalkStart",
        "O10_ReviewFirstDay": "VHV.Location.HBCT.SelfMonitoring.SMFirstDayReview",
        "O11_OneWeekLater": "VHV.Location.HBCT.SelfMonitoring.SMOneWeekLater",
        "O12_FindBehaviorPatterns": "VHV.Location.HBCT.SelfMonitoring.SMPatternArea",
        "O13_AdjustThePlan": "VHV.Location.HBCT.SelfMonitoring.SMAdjustment",
        "O14_PracticeAdjustedRoutine": "VHV.Location.HBCT.SelfMonitoring.SMAdjustedWalkStart",
        "O15_BuildProgressGraph": "VHV.Location.HBCT.SelfMonitoring.SMProgressGraph",
        "O16_SetSelfReminder": "VHV.Location.HBCT.SelfMonitoring.SMSelfReminder",
        "O17_WatchChaiSelfMonitor": "VHV.Location.HBCT.SelfMonitoring.SMSelfMonitorPayoff",
        "O18_FinalDebrief": "VHV.Location.HBCT.SelfMonitoring.SMFinalDebrief",
    },
    "Q_HBCT_06_POSITIVE_REINFORCEMENT": {
        "O01_NoticeSuccess": "VHV.Location.HBCT.PositiveReinforcement.PRHealthParkIntro",
        "O02_UnderstandReinforcement": "VHV.Location.HBCT.PositiveReinforcement.PRInstructorLesson",
        "O03_ExploreHealthFair": "VHV.Location.HBCT.PositiveReinforcement.PRHealthFairCenter",
        "O05_ChooseMeaningfulReinforcement": "VHV.Location.HBCT.PositiveReinforcement.PRPreferenceArea",
        "O06_TestReinforcementTiming": "VHV.Location.HBCT.PositiveReinforcement.PRTimingArea",
        "O08_CheckInWithChai": "VHV.Location.HBCT.PositiveReinforcement.PRChaiHome",
        "O12_JoinWalkingChallenge": "VHV.Location.HBCT.PositiveReinforcement.PRWalkingChallengeBoard",
        "O14_SeeFeedbackReinforcement": "VHV.Location.HBCT.PositiveReinforcement.PRHealthPostFeedback",
        "O15_FinalDebrief": "VHV.Location.HBCT.PositiveReinforcement.PRFinalDebrief",
    },
}


# Ordered preference only; gameplay remains non-linear wherever the authored
# triggers already allow it. Each entry is skipped as soon as its condition passes.
MULTI_TRACKING_DESTINATIONS = {
    "Q_HBCT_03_ROLE_MODEL": {
        "O02_DiscoverRoleModels": [
            ("VHV.Location.HBCT.RoleModel.ExerciseObservation", "flag", "VHV.Story.Flag.HBCT.RoleModel.ExerciseObserved", 0),
            ("VHV.Location.HBCT.RoleModel.SimilarExperience", "flag", "VHV.Story.Flag.HBCT.RoleModel.SimilarExperienceObserved", 0),
            ("VHV.Location.HBCT.RoleModel.Family", "flag", "VHV.Story.Flag.HBCT.RoleModel.FamilyObserved", 0),
            ("VHV.Location.HBCT.RoleModel.Symbolic", "flag", "VHV.Story.Flag.HBCT.RoleModel.SymbolicObserved", 0),
        ],
        "O06_InspectCandidates": [
            ("VHV.Location.HBCT.RoleModel.AthleteCandidate", "flag", "VHV.Story.Flag.HBCT.RoleModel.CandidateAthleteInspected", 0),
            ("VHV.Location.HBCT.RoleModel.SaengCandidate", "flag", "VHV.Story.Flag.HBCT.RoleModel.CandidateSaengInspected", 0),
            ("VHV.Location.HBCT.RoleModel.SymbolicCandidate", "flag", "VHV.Story.Flag.HBCT.RoleModel.CandidateSymbolicInspected", 0),
        ],
        "O09_SeeChangeOverTime": [
            ("VHV.Location.HBCT.RoleModel.Timeline.Before", "counter", "VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved", 1),
            ("VHV.Location.HBCT.RoleModel.Timeline.FindsModel", "counter", "VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved", 2),
            ("VHV.Location.HBCT.RoleModel.Timeline.FirstAttempt", "counter", "VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved", 3),
            ("VHV.Location.HBCT.RoleModel.Timeline.Repetition", "counter", "VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved", 4),
            ("VHV.Location.HBCT.RoleModel.Timeline.NewRoutine", "counter", "VHV.Story.Counter.HBCT.RoleModel.TimelineStationsObserved", 5),
        ],
    },
    "Q_HBCT_04_SOCIAL_SUPPORT": {
        "O01_ObserveVillageSupport": [
            ("VHV.Location.HBCT.SocialSupport.SSMealScene", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSObservedMeal", 0),
            ("VHV.Location.HBCT.SocialSupport.SSWalkingPartnerScene", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSObservedWalkingPartner", 0),
            ("VHV.Location.HBCT.SocialSupport.SSEncouragementScene", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSObservedEncouragement", 0),
            ("VHV.Location.HBCT.SocialSupport.SSHealthGuidanceScene", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSObservedGuidance", 0),
        ],
        "O03_ExploreSupportHouse": [
            ("VHV.Location.HBCT.SocialSupport.SSRoomEmotional", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSRoomEmotionalComplete", 0),
            ("VHV.Location.HBCT.SocialSupport.SSRoomInformational", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSRoomInformationalComplete", 0),
            ("VHV.Location.HBCT.SocialSupport.SSRoomInstrumental", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSRoomInstrumentalComplete", 0),
            ("VHV.Location.HBCT.SocialSupport.SSRoomAppraisal", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSRoomAppraisalComplete", 0),
        ],
        "O05_FindChaiSupporters": [
            ("VHV.Location.HBCT.SocialSupport.SSChaiWifeRecruit", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSWifeRecruited", 0),
            ("VHV.Location.HBCT.SocialSupport.SSChaiDaughterRecruit", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSDaughterRecruited", 0),
            ("VHV.Location.HBCT.SocialSupport.SSChaiNeighborRecruit", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSNeighborRecruited", 0),
            ("VHV.Location.HBCT.SocialSupport.SSChaiNurseRecruit", "flag", "VHV.Story.Flag.HBCT.SocialSupport.SSNurseRecruited", 0),
        ],
    },
    "Q_HBCT_06_POSITIVE_REINFORCEMENT": {
        "O03_ExploreHealthFair": [
            ("VHV.Location.HBCT.PositiveReinforcement.PRHealthFairSocial", "flag", "VHV.Story.Flag.HBCT.PositiveReinforcement.PRSocialStationSeen", 0),
            ("VHV.Location.HBCT.PositiveReinforcement.PRHealthFairMaterial", "flag", "VHV.Story.Flag.HBCT.PositiveReinforcement.PRMaterialStationSeen", 0),
            ("VHV.Location.HBCT.PositiveReinforcement.PRHealthFairActivity", "flag", "VHV.Story.Flag.HBCT.PositiveReinforcement.PRActivityStationSeen", 0),
            ("VHV.Location.HBCT.PositiveReinforcement.PRHealthFairToken", "flag", "VHV.Story.Flag.HBCT.PositiveReinforcement.PRTokenStationSeen", 0),
            ("VHV.Location.HBCT.PositiveReinforcement.PRHealthFairFeedback", "flag", "VHV.Story.Flag.HBCT.PositiveReinforcement.PRFeedbackStationSeen", 0),
        ],
        "O05_ChooseMeaningfulReinforcement": [
            ("VHV.Location.HBCT.PositiveReinforcement.PRPreferencePraise", "flag", "VHV.Story.Flag.HBCT.PositiveReinforcement.PRPreferencePraiseObserved", 0),
            ("VHV.Location.HBCT.PositiveReinforcement.PRPreferenceActivity", "flag", "VHV.Story.Flag.HBCT.PositiveReinforcement.PRPreferenceActivityObserved", 0),
            ("VHV.Location.HBCT.PositiveReinforcement.PRPreferenceToken", "flag", "VHV.Story.Flag.HBCT.PositiveReinforcement.PRPreferenceTokenObserved", 0),
        ],
    },
}


ROLE_MODEL_COMPLETION_FLAGS = {
    "DA_Ambient_HBCT_RoleModel_Exercise": "VHV.Story.Flag.HBCT.RoleModel.ExerciseObserved",
    "DA_Ambient_HBCT_RoleModel_SimilarExperience": "VHV.Story.Flag.HBCT.RoleModel.SimilarExperienceObserved",
    "DA_Ambient_HBCT_RoleModel_Family": "VHV.Story.Flag.HBCT.RoleModel.FamilyObserved",
    "DA_Ambient_HBCT_RoleModel_Symbolic": "VHV.Story.Flag.HBCT.RoleModel.SymbolicObserved",
    "DA_Ambient_HBCT_RoleModel_SymbolicCandidate": "VHV.Story.Flag.HBCT.RoleModel.CandidateSymbolicInspected",
}

# The saved map currently lacks these Quest 5 semantic anchors. They are created
# only when missing, at the production staging coordinates, with trigger behavior
# disabled. A later full Quest 5 authoring pass can replace them with its normal
# objective-gated volumes without changing the semantic IDs or tracker metadata.
MISSING_LOCATION_ANCHORS = {
    "SMInstructorLesson": ((1200, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMInstructorLesson"),
    "SMTargetBehaviorArea": ((2200, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMTargetBehaviorArea"),
    "SMMonitoringProcess": ((3300, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMMonitoringProcess"),
    "SMMethodStation": ((4400, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMMethodStation"),
    "SMTracker": ((5400, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMTracker"),
    "SMBreakfast": ((6400, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMBreakfast"),
    "SMAfternoon": ((7400, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMAfternoon"),
    "SMChaiWalkStart": ((8400, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMChaiWalkStart"),
    "SMFirstDayReview": ((11000, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMFirstDayReview"),
    "SMOneWeekLater": ((12000, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMOneWeekLater"),
    "SMPatternArea": ((13200, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMPatternArea"),
    "SMAdjustment": ((14300, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMAdjustment"),
    "SMAdjustedWalkStart": ((15300, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMAdjustedWalkStart"),
    "SMProgressGraph": ((17500, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMProgressGraph"),
    "SMSelfReminder": ((18500, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMSelfReminder"),
    "SMSelfMonitorPayoff": ((19500, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMSelfMonitorPayoff"),
    "SMFinalDebrief": ((20700, 900, 100), "VHV.Location.HBCT.SelfMonitoring.SMFinalDebrief"),
}


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def gameplay_tag(name):
    value = unreal.GameplayTag()
    if not value.import_text(name):
        raise RuntimeError("Gameplay Tag is not registered: {}".format(name))
    return value


def tracking_destination(location_name, condition_kind, state_name, compare_value):
    condition_type = (unreal.VHVStoryConditionType.FLAG_SET
                      if condition_kind == "flag"
                      else unreal.VHVStoryConditionType.COUNTER_GREATER_OR_EQUAL)
    condition = unreal.VHVStoryCondition(
        condition_type=condition_type,
        state_tag=gameplay_tag(state_name),
        compare_value=compare_value)
    return unreal.VHVQuestTrackingDestination(
        destination_location_tag=gameplay_tag(location_name),
        completion_conditions=unreal.VHVStoryConditionSet(
            match_mode=unreal.VHVStoryConditionMatch.ALL,
            conditions=[condition]))


def load_or_create_input_action():
    action = unreal.EditorAssetLibrary.load_asset(INPUT_ACTION_PATH)
    if not action:
        factory_type = getattr(unreal, "InputAction_Factory", None)
        if not factory_type:
            factory_type = getattr(unreal, "InputActionFactory", None)
        if not factory_type:
            raise RuntimeError("Enhanced Input action factory is unavailable")
        action = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "IA_LocateObjective", "/Game/Input/Actions",
            unreal.InputAction, factory_type())
    if not action:
        raise RuntimeError("Could not create IA_LocateObjective")
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    save(action)
    return action


def configure_input(action):
    context = unreal.EditorAssetLibrary.load_asset(INPUT_CONTEXT_PATH)
    if not context:
        raise RuntimeError("Missing {}".format(INPUT_CONTEXT_PATH))
    context.modify()
    context.unmap_all_keys_from_action(action)
    x_key = unreal.Key()
    x_key.set_editor_property("key_name", "X")
    context.map_key(action, x_key)
    save(context)

    controller_bp = unreal.EditorAssetLibrary.load_asset(PLAYER_CONTROLLER_PATH)
    if not controller_bp or not controller_bp.generated_class():
        raise RuntimeError("Missing {}".format(PLAYER_CONTROLLER_PATH))
    controller_cdo = unreal.get_default_object(controller_bp.generated_class())
    controller_cdo.modify()
    controller_cdo.set_editor_property("locate_objective_action", action)
    save(controller_bp)


def load_or_create_beam_material():
    ensure_directory(MATERIAL_DIR)
    material = unreal.EditorAssetLibrary.load_asset(MATERIAL_PATH)
    if not material:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_VHV_ObjectiveTrackingBeam", MATERIAL_DIR,
            unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("Could not create objective tracking beam material")

    material.modify()
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)

    color = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant3Vector, -260, -40)
    color.set_editor_property("constant", unreal.LinearColor(0.02, 4.0, 8.0, 1.0))
    opacity = unreal.MaterialEditingLibrary.create_material_expression(
        material, unreal.MaterialExpressionConstant, -260, 100)
    opacity.set_editor_property("r", 0.42)
    unreal.MaterialEditingLibrary.connect_material_property(
        color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(material)
    save(material)
    return material


def apply_tracking_metadata_to_objectives(quest_id, objectives):
    """Shared source-of-truth helper used by the Quest 3-6 authoring scripts."""
    authored = TRACKING_LOCATIONS.get(quest_id, {})
    found = set()
    for objective in objectives:
        objective_id = str(objective.get_editor_property("objective_id"))
        location_name = authored.get(objective_id)
        if not location_name:
            continue
        multi_specs = MULTI_TRACKING_DESTINATIONS.get(quest_id, {}).get(objective_id, [])
        objective.set_editor_properties({
            "enable_location_tracking": True,
            "tracking_location_tag": gameplay_tag(location_name),
            "tracking_destinations": [tracking_destination(*spec) for spec in multi_specs],
        })
        found.add(objective_id)
    if found != set(authored):
        raise RuntimeError("{} tracking objectives missing: {}".format(
            quest_id, sorted(set(authored) - found)))


def configure_quest_metadata():
    arc = unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    if not arc:
        raise RuntimeError("Missing {}".format(ARC_PATH))
    arc.modify()
    quests = list(arc.get_editor_property("quests"))
    found = set()
    for quest in quests:
        quest_id = str(quest.get_editor_property("quest_id"))
        authored = TRACKING_LOCATIONS.get(quest_id)
        if not authored:
            continue
        objectives = list(quest.get_editor_property("objectives"))
        apply_tracking_metadata_to_objectives(quest_id, objectives)
        found.update((quest_id, objective_id) for objective_id in authored)
        quest.set_editor_property("objectives", objectives)
    expected = {(quest_id, objective_id)
                for quest_id, objectives in TRACKING_LOCATIONS.items()
                for objective_id in objectives}
    if found != expected:
        raise RuntimeError("Tracking metadata mismatch; missing={}".format(
            sorted(expected - found)))
    arc.set_editor_property("quests", quests)
    save(arc)
    unreal.log("{} Updated tracking metadata on {} objectives.".format(LOG, len(found)))


def validate_tracking_locations():
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    required_ids = {
        location_name.rsplit(".", 1)[-1]
        for objectives in TRACKING_LOCATIONS.values()
        for location_name in objectives.values()
    }
    required_ids.update(
        location_name.rsplit(".", 1)[-1]
        for objectives in MULTI_TRACKING_DESTINATIONS.values()
        for destinations in objectives.values()
        for location_name, _, _, _ in destinations)
    matches = {location_id: [] for location_id in required_ids}
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actor_subsystem.get_all_level_actors():
        if not isinstance(actor, unreal.VHVQuestLocationVolume):
            continue
        location_id = str(actor.get_effective_location_id())
        if location_id in matches:
            matches[location_id].append(actor)
    invalid = {location_id: [actor.get_actor_label() for actor in actors]
               for location_id, actors in matches.items() if len(actors) != 1}
    if invalid:
        raise RuntimeError("Tracking locations must resolve exactly once: {}".format(invalid))
    unreal.log("{} Validated {} semantic tracking locations in the village map.".format(
        LOG, len(required_ids)))
    for quest_id in ("Q_HBCT_01_MOTIVATION", "Q_HBCT_02_GOAL_SETTING"):
        for objective_id, tag_name in TRACKING_LOCATIONS[quest_id].items():
            location_id = tag_name.rsplit(".", 1)[-1]
            actor = matches[location_id][0]
            position = actor.get_actor_location()
            unreal.log("{} {} {} -> {} -> {} @ ({:.1f}, {:.1f}, {:.1f})".format(
                LOG, quest_id, objective_id, tag_name, actor.get_actor_label(),
                position.x, position.y, position.z))


def ensure_quest12_tracking_anchors():
    """Repair only semantic metadata/anchors needed by Quest 1/2 tracking."""
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    by_label = {actor.get_actor_label(): actor for actor in actors}
    changed = False

    # These two existing objective triggers previously shared MaliCheckIn. Give
    # the later review trigger its already-defined, stable semantic destination.
    plan_review = by_label.get("HBCT_Trigger_GoalSetting_MaliPlanReview")
    if not isinstance(plan_review, unreal.VHVQuestLocationVolume):
        raise RuntimeError("Missing HBCT_Trigger_GoalSetting_MaliPlanReview")
    desired_review_tag = gameplay_tag("VHV.Location.HBCT.GoalSetting.MaliPlanReview")
    if str(plan_review.get_effective_location_id()) != "MaliPlanReview":
        plan_review.modify()
        plan_review.set_editor_property("location_tag", desired_review_tag)
        changed = True

    def ensure_anchor(label, source_label, tag_name):
        nonlocal changed
        existing = by_label.get(label)
        if existing:
            if not isinstance(existing, unreal.VHVQuestLocationVolume):
                raise RuntimeError("{} is not a semantic location volume".format(label))
            existing.modify()
            existing.set_editor_property("location_tag", gameplay_tag(tag_name))
            return existing
        source = by_label.get(source_label)
        if not source:
            raise RuntimeError("Missing source scene actor {} for {}".format(source_label, label))
        actor = actor_subsystem.spawn_actor_from_class(
            unreal.VHVQuestLocationVolume,
            source.get_actor_location(),
            source.get_actor_rotation())
        if not actor:
            raise RuntimeError("Could not create {}".format(label))
        actor.set_actor_label(label)
        actor.set_folder_path("03_GoalSetting/Tracking")
        actor.set_editor_properties({
            "location_tag": gameplay_tag(tag_name),
            "enabled": False,
            "trigger_world_action": False,
            "activate_current_objective": False,
            "required_active_quest_id": "",
            "required_active_objective_id": "",
        })
        actor.get_editor_property("box_component").set_box_extent(
            unreal.Vector(180.0, 180.0, 180.0), True)
        by_label[label] = actor
        changed = True
        return actor

    ensure_anchor(
        "HBCT_Location_GoalSetting_GoodGoalBoard",
        "HBCT_Location_HealthPost",
        "VHV.Location.HBCT.GoalSetting.GoodGoalBoard")
    ensure_anchor(
        "HBCT_Location_GoalSetting_RoadsidePractice",
        "HBCT_GoalSetting_RoadVillager",
        "VHV.Location.HBCT.GoalSetting.RoadsidePractice")
    ensure_anchor(
        "HBCT_Location_GoalSetting_FinalInstructor",
        "HBCT_Location_HealthPost",
        "VHV.Location.HBCT.GoalSetting.FinalInstructor")

    if changed and not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save {}".format(MAP_PATH))
    unreal.log("{} Quest 1/2 tracking anchors and semantic identities are ready.".format(LOG))


def ensure_missing_location_anchors():
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = {}
    for actor in actor_subsystem.get_all_level_actors():
        if isinstance(actor, unreal.VHVQuestLocationVolume):
            existing.setdefault(str(actor.get_effective_location_id()), []).append(actor)
    created = 0
    for location_id, (coordinates, tag_name) in MISSING_LOCATION_ANCHORS.items():
        matches = existing.get(location_id, [])
        if len(matches) > 1:
            raise RuntimeError("Duplicate semantic location {}".format(location_id))
        if matches:
            continue
        actor = actor_subsystem.spawn_actor_from_class(
            unreal.VHVQuestLocationVolume,
            unreal.Vector(*coordinates),
            unreal.Rotator(0.0, 0.0, 0.0))
        if not actor:
            raise RuntimeError("Could not create semantic anchor {}".format(location_id))
        actor.set_actor_label("HBCT_SM_Location_{}".format(location_id))
        actor.set_folder_path("06_SelfMonitoring/Locations")
        actor.set_editor_properties({
            "location_tag": gameplay_tag(tag_name),
            "enabled": False,
            "trigger_world_action": False,
            "activate_current_objective": False,
            "required_active_quest_id": "",
            "required_active_objective_id": "",
        })
        actor.get_editor_property("box_component").set_box_extent(
            unreal.Vector(50.0, 50.0, 110.0), True)
        created += 1
    if created and not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save {}".format(MAP_PATH))
    unreal.log("{} Added {} missing semantic tracking anchors without moving existing actors.".format(
        LOG, created))


def ensure_role_model_tracking_anchors():
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    by_label = {actor.get_actor_label(): actor
                for actor in actor_subsystem.get_all_level_actors()}
    changed = False

    for label, source_label, tag_name in (
            ("HBCT_Location_RoleModel_SomchaiIntro", "HBCT_RoleModel_UncleSomchai",
             "VHV.Location.HBCT.RoleModel.SomchaiIntro"),
            ("HBCT_Location_RoleModel_AthleteCandidate", "HBCT_RoleModel_YoungAthlete",
             "VHV.Location.HBCT.RoleModel.AthleteCandidate")):
        source = by_label.get(source_label)
        if not source:
            raise RuntimeError("Missing interaction actor {}".format(source_label))
        anchor = by_label.get(label)
        if anchor and not isinstance(anchor, unreal.VHVQuestLocationVolume):
            raise RuntimeError("{} is not a semantic location volume".format(label))
        if not anchor:
            anchor = actor_subsystem.spawn_actor_from_class(
                unreal.VHVQuestLocationVolume,
                source.get_actor_location(), source.get_actor_rotation())
            if not anchor:
                raise RuntimeError("Could not create {}".format(label))
            anchor.set_actor_label(label)
            anchor.set_folder_path("04_RoleModel/Locations")
            anchor.get_editor_property("box_component").set_box_extent(
                unreal.Vector(180.0, 180.0, 180.0), True)
            by_label[label] = anchor
            changed = True
        anchor.modify()
        anchor.set_editor_properties({
            "location_tag": gameplay_tag(tag_name),
            "enabled": False,
            "trigger_world_action": False,
            "activate_current_objective": False,
            "required_active_quest_id": "",
            "required_active_objective_id": "",
        })
        delta = anchor.get_actor_location() - source.get_actor_location()
        if delta.x * delta.x + delta.y * delta.y + delta.z * delta.z > 0.01:
            anchor.set_actor_location(source.get_actor_location(), False, False)
            anchor.set_actor_rotation(source.get_actor_rotation(), False)
            changed = True

    if changed and not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save {}".format(MAP_PATH))


def ensure_role_model_completion_flags():
    for asset_name, flag_name in ROLE_MODEL_COMPLETION_FLAGS.items():
        path = "/Game/VHV_Stuff/HBCT/Ambient/" + asset_name
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            raise RuntimeError("Missing {}".format(path))
        effects = list(asset.get_editor_property("completion_effects"))
        existing = {
            str(unreal.GameplayTagLibrary.get_tag_name(effect.get_editor_property("state_tag")))
            for effect in effects
        }
        if flag_name not in existing:
            effects.append(unreal.VHVStoryEffect(
                effect_type=unreal.VHVStoryEffectType.SET_FLAG,
                state_tag=gameplay_tag(flag_name)))
            asset.modify()
            asset.set_editor_property("completion_effects", effects)
            save(asset)


def run():
    action = load_or_create_input_action()
    configure_input(action)
    load_or_create_beam_material()
    configure_quest_metadata()
    ensure_quest12_tracking_anchors()
    ensure_role_model_tracking_anchors()
    ensure_role_model_completion_flags()
    ensure_missing_location_anchors()
    validate_tracking_locations()
    unreal.log("{} Authored X input, beam material, controller binding, and Quest 1-5 metadata.".format(LOG))


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
