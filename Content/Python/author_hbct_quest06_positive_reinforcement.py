"""Author Q_HBCT_06_POSITIVE_REINFORCEMENT production content (UE 5.7)."""

import traceback
import unreal
import author_hbct_milestone_01 as hbct
import author_objective_location_tracking as tracking

ROOT = "/Game/VHV_Stuff/HBCT"
LEVEL_PATH = ROOT + "/Activities/DA_Level_HBCT"
ARC_PATH = ROOT + "/Quests/DA_QuestArc_HBCT"
CONVERSATION_DIR = ROOT + "/Dialogue"
AMBIENT_DIR = ROOT + "/Ambient"
MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout"
NPC_PATH = "/Game/VHV_Stuff/NPC/BP_VHVNPCBase"
QUEST_ID = "Q_HBCT_06_POSITIVE_REINFORCEMENT"
TOPIC_ID = "HBCT_M01_POSITIVE_REINFORCEMENT"
PREFIX = "HBCT_PR_"
FOLDER = "07_PositiveReinforcement"
LOG = "[HBCT Quest 06]"
FLAG = "VHV.Story.Flag.HBCT.PositiveReinforcement."
LOCATION = "VHV.Location.HBCT.PositiveReinforcement."


def struct(kind, **properties):
    value = kind()
    value.set_editor_properties(properties)
    return value


def tag(name):
    value = unreal.GameplayTag()
    if not value.import_text(name):
        raise RuntimeError("Gameplay tag is not registered: {}".format(name))
    return value


def flag_condition(name, is_set=True):
    return struct(unreal.VHVStoryCondition,
                  condition_type=(unreal.VHVStoryConditionType.FLAG_SET if is_set
                                  else unreal.VHVStoryConditionType.FLAG_NOT_SET),
                  state_tag=tag(name))


def all_flags(*names):
    return struct(unreal.VHVStoryConditionSet,
                  match_mode=unreal.VHVStoryConditionMatch.ALL,
                  conditions=[flag_condition(name) for name in names])


def flag_not_set(name):
    return struct(unreal.VHVStoryConditionSet,
                  match_mode=unreal.VHVStoryConditionMatch.ALL,
                  conditions=[flag_condition(name, False)])


def objective(objective_id, text, objective_type, **properties):
    values = {"objective_id": objective_id, "objective_text": text,
              "objective_type": objective_type, "completion_effects": []}
    values.update(properties)
    return struct(unreal.VHVQuestObjectiveDefinition, **values)


def contextual(participant, conversation, entry, conditions):
    return struct(unreal.VHVQuestContextualConversation,
                  participant_tag=tag(participant), conversation=conversation,
                  entry_node_id=entry, availability_conditions=conditions)


def npc_move(participant, destination, speed=0.0):
    return struct(unreal.VHVQuestNPCMoveRequest,
                  participant_tag=tag(participant),
                  destination_location_tag=tag(destination),
                  face_destination_rotation=True, move_speed_override=speed)


def move_stage(moves, receiver=""):
    return struct(unreal.VHVQuestNPCMoveStage, moves=moves,
                  ambient_conversation_receiver_tag=(tag(receiver) if receiver else unreal.GameplayTag()))


def assessed(activity_tag, title, activity_type, prompt, **properties):
    values = {
        "correct_feedback": properties.pop("correct_feedback", "Correct."),
        "incorrect_feedback": properties.pop("incorrect_feedback", "Review what happened and try again."),
        "partial_feedback": "", "require_correct_answer_to_advance": True,
    }
    values.update(properties)
    return hbct.activity(activity_tag, title, activity_type, prompt,
                         unreal.TeachingContent(), **values)


def build_activities():
    single = unreal.TextbookActivityType.SINGLE_CHOICE
    matching = unreal.TextbookActivityType.MATCHING
    ordering = unreal.TextbookActivityType.ORDERING
    return [
        assessed("VHV.Activity.HBCT.PositiveReinforcement.NoticeSuccessQuestion",
          "Notice Success", single,
          "Which person do you think felt most encouraged to repeat the behaviour?",
          question=hbct.question("HBCT_PR_NOTICE",
            "Which person do you think felt most encouraged to repeat the behaviour?",
            ["Only Uncle Chai", "The people whose successful behaviour was noticed",
             "Only someone receiving a prize", "Everyone equally"], 1),
          attempt_policy=hbct.attempt_policy(
            "Compare what followed each successful behaviour.",
            "Prompt acknowledgement made the healthy behaviour feel worth repeating."),
          correct_feedback="The successful behaviour that was noticed received immediate social reinforcement.",
          success_effects=[hbct.effect(FLAG+"PRSuccessQuestionCompleted")]),
        assessed("VHV.Activity.HBCT.PositiveReinforcement.ReinforcementTypeMatching",
          "Match Reinforcement Types", matching,
          "Match each form to the health-fair example.",
          matching_pairs=[
            struct(unreal.MatchingPair, left_text="Social", right_text="Excellent work!"),
            struct(unreal.MatchingPair, left_text="Material", right_text="Basket of fruit"),
            struct(unreal.MatchingPair, left_text="Activity", right_text="Relax at the health park"),
            struct(unreal.MatchingPair, left_text="Token", right_text="30 walking points"),
            struct(unreal.MatchingPair, left_text="Feedback", right_text="Improved health-check result")],
          correct_feedback="Reinforcement can be social, material, an activity, a token, or feedback.",
          success_effects=[hbct.effect(FLAG+"PRTypesMatched")]),
        assessed("VHV.Activity.HBCT.PositiveReinforcement.PreferenceMatching",
          "Choose Meaningful Reinforcement", matching,
          "Match each person to what they value.",
          matching_pairs=[
            struct(unreal.MatchingPair, left_text="NPC A: values recognition", right_text="Praise"),
            struct(unreal.MatchingPair, left_text="NPC B: enjoys quiet time", right_text="Preferred activity"),
            struct(unreal.MatchingPair, left_text="NPC C: enjoys milestones", right_text="Token system")],
          correct_feedback="A reward reinforces behaviour only when it is meaningful to that person.",
          success_effects=[hbct.effect(FLAG+"PRPreferencesMatched")]),
        assessed("VHV.Activity.HBCT.PositiveReinforcement.TimingOrdering",
          "Order Reinforcement Timing", ordering,
          "Put effective reinforcement in order.",
          ordering_items=hbct.ordering_items([
            "Desired behaviour occurs", "Reinforcement immediately follows",
            "Repeat consistently when appropriate"]),
          correct_order=["1", "2", "3"],
          correct_feedback="The consequence follows the behaviour promptly and is used consistently when appropriate.",
          success_effects=[hbct.effect(FLAG+"PRTimingOrdered")]),
        assessed("VHV.Activity.HBCT.PositiveReinforcement.ChaiProgressResponse",
          "Notice Chai's Progress", single, "How should you respond?",
          question=hbct.question("HBCT_PR_CHAI_PROGRESS", "How should you respond?", [
            "You need to do much better.", "You failed whenever you ate sweets.",
            "Behaviour change takes time. Even trying to reduce sweets is already a very good start."], 2),
          attempt_policy=hbct.attempt_policy(
            "Recognize Chai's genuine progress.",
            "Positive reinforcement notices progress without demanding perfection."),
          correct_feedback="SOCIAL REINFORCEMENT - PRAISE. You recognized genuine progress.",
          success_effects=[hbct.effect(FLAG+"PRChaiProgressReinforced")]),
        assessed("VHV.Activity.HBCT.PositiveReinforcement.ChaiExerciseResponse",
          "Reinforce Exercise", single, "How should you respond to Chai's walking?",
          question=hbct.question("HBCT_PR_CHAI_EXERCISE", "How should you respond?", [
            "That does not count unless you walk every day.",
            "That is a good start. Keep going. You can gradually add more walking.",
            "You must begin a perfect exercise routine immediately."], 1),
          attempt_policy=hbct.attempt_policy(
            "Notice the healthy behaviour already happening.",
            "Reinforcing a good start supports the next achievable step."),
          correct_feedback="You recognized progress rather than judging it all-or-nothing.",
          success_effects=[hbct.effect(FLAG+"PRChaiExerciseReinforced")]),
        assessed("VHV.Activity.HBCT.PositiveReinforcement.SelfRewardChoice",
          "Choose a Self-Reward", single,
          "Chai says quiet time at the health park is what he enjoys. What fits him?",
          question=hbct.question("HBCT_PR_SELF_REWARD", "Choose a reward matching Chai's preference.", [
            "A prize he does not want", "Relax at the health park after completing his weekly plan",
            "Praise before he completes the plan"], 1),
          attempt_policy=hbct.attempt_policy(
            "Use the preference Chai just stated.",
            "Self-reinforcement works when it follows success and matters to the person."),
          correct_feedback="This is meaningful because Chai values quiet time at the park.",
          success_effects=[hbct.effect(FLAG+"PRChaiSelfRewardChosen")]),
    ]


def configure_level(level):
    day = level.get_editor_property("day_data")
    topics = [topic for topic in list(day.get_editor_property("topics"))
              if str(topic.get_editor_property("topic_id")) != TOPIC_ID]
    topics.append(struct(unreal.TopicData, topic_id=TOPIC_ID,
        topic_title="Technique 6 - Positive Reinforcement",
        narrative_introduction="Notice progress and help make healthy behaviour worth repeating.",
        activities=build_activities()))
    day.set_editor_property("topics", topics)
    day.set_editor_property("completion_summary",
        "Positive reinforcement follows healthy behaviour with something meaningful that supports repetition.")
    level.set_editor_property("day_data", day)


def create_conversations():
    text, learn = hbct.text_node, hbct.learning_node
    specs = {
      "DA_Conversation_HBCT_PositiveReinforcement_Lesson": ("HBCT_PR_Lesson", [
        text("PRL_01", "Instructor", "When a desired behaviour happens, something meaningful can follow it and make that behaviour more likely to happen again.", "PRL_02", "VHV.Participant.Instructor"),
        text("PRL_02", "Player", "So reinforcement comes after the behaviour?", "PRL_03"),
        text("PRL_03", "Instructor", "Exactly. It is not simply giving people prizes.", speaker_tag="VHV.Participant.Instructor",
             completion_effects=[hbct.effect(FLAG+"PRConceptUnderstood")])]),
      "DA_Conversation_HBCT_PositiveReinforcement_PreferencePraise": ("HBCT_PR_PreferencePraise", [
        text("PRPP_01", "Villager", "Prizes do not matter much to me. I remember when someone notices my effort.", speaker_tag="VHV.Participant.PRPreferencePraise",
             completion_effects=[hbct.effect(FLAG+"PRPreferencePraiseObserved")])]),
      "DA_Conversation_HBCT_PositiveReinforcement_PreferenceActivity": ("HBCT_PR_PreferenceActivity", [
        text("PRPA_01", "Villager", "Quiet time, fishing, or relaxing at the park is what I look forward to.", speaker_tag="VHV.Participant.PRPreferenceActivity",
             completion_effects=[hbct.effect(FLAG+"PRPreferenceActivityObserved")])]),
      "DA_Conversation_HBCT_PositiveReinforcement_PreferenceToken": ("HBCT_PR_PreferenceToken", [
        text("PRPT_01", "Villager", "I like collecting points and reaching the next milestone.", speaker_tag="VHV.Participant.PRPreferenceToken",
             completion_effects=[hbct.effect(FLAG+"PRPreferenceTokenObserved")])]),
      "DA_Conversation_HBCT_PositiveReinforcement_Preferences": ("HBCT_PR_Preferences", [
        text("PRPM_01", "Instructor", "A reward only reinforces behaviour if it is meaningful to that person.", "PRPM_A", "VHV.Participant.Instructor"),
        learn("PRPM_A", "VHV.Activity.HBCT.PositiveReinforcement.PreferenceMatching", topic_id=TOPIC_ID)]),
      "DA_Conversation_HBCT_PositiveReinforcement_ChaiCheckIn": ("HBCT_PR_ChaiCheckIn", [
        text("PRC_01", "Player", "How have you been doing with food and exercise?", "PRC_02"),
        text("PRC_02", "Uncle Chai", "I'm trying, but not perfectly. Sometimes I still eat sweets. And some days I feel lazy about walking.", speaker_tag="VHV.Participant.UncleChai",
             completion_effects=[hbct.effect(FLAG+"PRChaiCheckInComplete")])]),
      "DA_Conversation_HBCT_PositiveReinforcement_Exercise": ("HBCT_PR_Exercise", [
        text("PRE_01", "Player", "Have you been walking?", "PRE_02"),
        text("PRE_02", "Uncle Chai", "Sometimes. I walk to the market and around the house.", "PRE_A", "VHV.Participant.UncleChai"),
        learn("PRE_A", "VHV.Activity.HBCT.PositiveReinforcement.ChaiExerciseResponse", topic_id=TOPIC_ID)]),
      "DA_Conversation_HBCT_PositiveReinforcement_SelfReward": ("HBCT_PR_SelfReward", [
        text("PRR_01", "Player", "If you complete your plan for a week, choose something you enjoy as a reward.", "PRR_02"),
        text("PRR_02", "Uncle Chai", "I do not care much about prizes. I enjoy sitting quietly in the health park after a walk.", "PRR_A", "VHV.Participant.UncleChai"),
        learn("PRR_A", "VHV.Activity.HBCT.PositiveReinforcement.SelfRewardChoice", topic_id=TOPIC_ID)]),
      "DA_Conversation_HBCT_PositiveReinforcement_FinalDebrief": ("HBCT_PR_FinalDebrief", [
        text("PRD_01", "Instructor", "People often focus on mistakes. But progress also needs to be noticed.", "PRD_02", "VHV.Participant.Instructor"),
        text("PRD_02", "Instructor", "Desired behaviour. Notice it. Respond immediately. Use something meaningful. Help the behaviour happen again.", speaker_tag="VHV.Participant.Instructor",
             completion_effects=[hbct.effect(FLAG+"PRCompleted")])]),
    }
    assets = {}
    for name, (conversation_id, nodes) in specs.items():
        asset = hbct.load_or_create(CONVERSATION_DIR, name, unreal.VHVConversationDataAsset)
        hbct.set_conversation(asset, conversation_id, nodes)
        assets[name] = asset
    return assets


def ambient_spec(conversation_id, participants, lines, effects):
    return {"conversation_id": conversation_id,
            "participants": [hbct.ambient_participant(slot, display) for slot, display in participants],
            "lines": [hbct.ambient_line(slot, line, duration) for slot, line, duration in lines],
            "show_speaker_name": False,
            "completion_effects": [hbct.effect(name) for name in effects]}


def create_ambient_assets():
    specs = {
      "DA_Ambient_HBCT_PositiveReinforcement_NoticeSuccess": ambient_spec("HBCT_PR_NoticeSuccess",
        [("Walker","Walker"),("Friend","Friend"),("Child","Child"),("Parent","Parent"),("Chai","Uncle Chai")],
        [("Friend","You did the whole route today!",2.4),("Walker","I did!",1.5),
         ("Parent","You chose water instead of another sweet drink. Good choice.",2.8),("Child","Water is fine.",1.6),
         ("Chai","My walking goal is finished. I'll head home now.",2.5)],
        [FLAG+"PRSuccessWalkerSeen",FLAG+"PRSuccessChildSeen",FLAG+"PRSuccessChaiSeen"]),
      "DA_Ambient_HBCT_PositiveReinforcement_HealthFairSocial": ambient_spec("HBCT_PR_HealthFairSocial",
        [("VHV","VHV"),("Guest","Villager")],[("VHV","Excellent! You kept going even though it was difficult.",3.0)],
        [FLAG+"PRSocialStationSeen"]),
      "DA_Ambient_HBCT_PositiveReinforcement_HealthFairMaterial": ambient_spec("HBCT_PR_HealthFairMaterial",
        [("VHV","VHV"),("Guest","Villager")],[("VHV","You reached your milestone. This fruit is a practical way to mark it.",3.1),("Guest","That will be useful at home.",2.0)],
        [FLAG+"PRMaterialStationSeen"]),
      "DA_Ambient_HBCT_PositiveReinforcement_HealthFairActivity": ambient_spec("HBCT_PR_HealthFairActivity",
        [("Guest","Villager")],[("Guest","I finished my walking goal. I'm going to relax at the park for a while.",3.1)],
        [FLAG+"PRActivityStationSeen"]),
      "DA_Ambient_HBCT_PositiveReinforcement_HealthFairToken": ambient_spec("HBCT_PR_HealthFairToken",
        [("Head","Village Head"),("Guest","Villager")],[("Head","One completed walk earns one point. Thirty points earns a small reward.",3.2),("Guest","I'll mark today's point.",1.9)],
        [FLAG+"PRTokenStationSeen"]),
      "DA_Ambient_HBCT_PositiveReinforcement_HealthFairFeedback": ambient_spec("HBCT_PR_HealthFairFeedback",
        [("Nurse","Nurse"),("Guest","Villager")],[("Nurse","Your recent health-check result has improved.",2.6),("Guest","Seeing that makes me want to continue.",2.5)],
        [FLAG+"PRFeedbackStationSeen"]),
      "DA_Ambient_HBCT_PositiveReinforcement_Timing": ambient_spec("HBCT_PR_Timing",
        [("Instructor","Instructor"),("Demo","Villager")],
        [("Instructor","Excellent work exercising!",1.9),("Instructor","But they haven't done it yet.",2.2),
         ("Demo","TWO DAYS LATER...",1.6),("Instructor","Good job walking on Tuesday.",2.1),
         ("Instructor","Better than nothing, but the connection is weaker.",2.7),
         ("Demo","Thirty minutes complete.",1.8),("Instructor","Excellent - you completed all 30 minutes today.",2.8),
         ("Instructor","Now the reinforcement is clearly connected to the behaviour.",2.8)],
        [FLAG+"PRTooEarlySeen",FLAG+"PRTooLateSeen",FLAG+"PRImmediateSeen"]),
      "DA_Ambient_HBCT_PositiveReinforcement_WalkingChallenge": ambient_spec("HBCT_PR_WalkingChallenge",
        [("Head","Village Head"),("Chai","Uncle Chai")],
        [("Head","Reach 30 points and there's a small reward.",2.6),("Chai","That actually sounds fun.",2.1)],
        [FLAG+"PRChallengeWalkComplete",FLAG+"PRWalkingChallengeJoined"]),
      "DA_Ambient_HBCT_PositiveReinforcement_FeedbackResult": ambient_spec("HBCT_PR_FeedbackResult",
        [("Nurse","Nurse"),("Chai","Uncle Chai")],
        [("Nurse","Your recent result has improved gradually. Your calendar also shows more regular walking and fewer sweet drinks.",3.5),
         ("Chai","So it really is making a difference.",2.4),
         ("Nurse","Seeing progress can help make the healthy behaviour worth continuing.",3.0)],
        [FLAG+"PRFeedbackReinforcementSeen"]),
    }
    assets = {}
    for name, data in specs.items():
        asset = hbct.load_or_create(AMBIENT_DIR, name, unreal.VHVAmbientConversationData)
        asset.set_editor_properties(data)
        assets[name] = asset
    return assets


def configure_arc(arc, conversations):
    q = unreal.VHVQuestObjectiveType
    scene_flags = [FLAG+"PRSuccessWalkerSeen",FLAG+"PRSuccessChildSeen",FLAG+"PRSuccessChaiSeen"]
    station_flags = [FLAG+"PRSocialStationSeen",FLAG+"PRMaterialStationSeen",FLAG+"PRActivityStationSeen",FLAG+"PRTokenStationSeen",FLAG+"PRFeedbackStationSeen"]
    preference_flags = [FLAG+"PRPreferencePraiseObserved",FLAG+"PRPreferenceActivityObserved",FLAG+"PRPreferenceTokenObserved"]
    quest = struct(unreal.VHVQuestDefinition,
      quest_id=QUEST_ID, quest_title="TECHNIQUE 6: POSITIVE REINFORCEMENT",
      quest_description="Notice progress and help make healthy behaviour worth repeating.",
      category=unreal.VHVQuestCategory.MAIN_STORY, auto_track=True, auto_start_next_quest=False,
      completion_stinger=hbct.major_stinger("POSITIVE REINFORCEMENT - COMPLETE",
        "POSITIVE REINFORCEMENT", "NOTICE PROGRESS. HELP HEALTHY BEHAVIOUR HAPPEN AGAIN.", 2.6),
      objectives=[
        objective("O01_NoticeSuccess", "Observe how people respond to small health successes.", q.LEARNING_ACTIVITY,
          activity_tag=tag("VHV.Activity.HBCT.PositiveReinforcement.NoticeSuccessQuestion"), auto_start=True,
          prepare_activation_npc_readiness_before_exposure=True,
          activation_npc_readiness=[npc_move(
            "VHV.Participant.UncleChai", LOCATION+"PRSuccessChai")],
          npc_move_stage_activation_conditions=all_flags(*scene_flags),
          npc_travel_mode=unreal.VHVQuestNPCTravelMode.FREE_ROAM,
          npc_move_stages=[move_stage([npc_move("VHV.Participant.UncleChai", LOCATION+"PRSuccessChaiExit", 180.0)])]),
        objective("O02_UnderstandReinforcement", "Talk to the Instructor about positive reinforcement.", q.CONVERSATION,
          participant_tag=tag("VHV.Participant.Instructor"), location_tag=tag(LOCATION+"PRInstructorLesson"),
          ensure_participant_present_at_location=True,
          conversation=conversations["DA_Conversation_HBCT_PositiveReinforcement_Lesson"], entry_node_id="PRL_01", auto_start=False),
        objective("O03_ExploreHealthFair", "Inspect all five reinforcement stations at the health fair.", q.CUSTOM_EVENT,
          custom_event_tag=tag("VHV.CustomEvent.HBCT.PositiveReinforcement.HealthFairComplete"),
          completion_conditions=all_flags(*station_flags)),
        objective("O04_MatchReinforcementTypes", "Match the five forms of reinforcement.", q.LEARNING_ACTIVITY,
          activity_tag=tag("VHV.Activity.HBCT.PositiveReinforcement.ReinforcementTypeMatching"), auto_start=True),
        objective("O05_ChooseMeaningfulReinforcement", "Talk to the three villagers and learn what encouragement matters to each.", q.CONVERSATION,
          conversation=conversations["DA_Conversation_HBCT_PositiveReinforcement_Preferences"],
          entry_node_id="PRPM_01", auto_start=False,
          contextual_conversations=[
            contextual("VHV.Participant.PRPreferencePraise", conversations["DA_Conversation_HBCT_PositiveReinforcement_PreferencePraise"], "PRPP_01", flag_not_set(FLAG+"PRPreferencePraiseObserved")),
            contextual("VHV.Participant.PRPreferenceActivity", conversations["DA_Conversation_HBCT_PositiveReinforcement_PreferenceActivity"], "PRPA_01", flag_not_set(FLAG+"PRPreferenceActivityObserved")),
            contextual("VHV.Participant.PRPreferenceToken", conversations["DA_Conversation_HBCT_PositiveReinforcement_PreferenceToken"], "PRPT_01", flag_not_set(FLAG+"PRPreferenceTokenObserved"))],
          completion_npc_moves=[
            npc_move("VHV.Participant.Instructor", LOCATION+"PRTimingTooEarly", 180.0),
            npc_move("VHV.Participant.PRDemoVillager", LOCATION+"PRTimingImmediate", 180.0)]),
        objective("O06_TestReinforcementTiming", "Watch three examples of reinforcement timing.", q.WORLD_ACTION,
          world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.PositiveReinforcement.TimingDemo"),
          world_action_tag=tag("VHV.WorldAction.StartConversation"),
          world_action_start_policy=unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER,
          activation_npc_readiness=[
            npc_move("VHV.Participant.Instructor", LOCATION+"PRTimingTooEarly", 180.0),
            npc_move("VHV.Participant.PRDemoVillager", LOCATION+"PRTimingImmediate", 180.0)]),
        objective("O07_OrderReinforcementTiming", "Order effective reinforcement timing.", q.LEARNING_ACTIVITY,
          activity_tag=tag("VHV.Activity.HBCT.PositiveReinforcement.TimingOrdering"), auto_start=True),
        objective("O08_CheckInWithChai", "Talk to Uncle Chai about how his plan is going.", q.CONVERSATION,
          participant_tag=tag("VHV.Participant.UncleChai"), location_tag=tag(LOCATION+"PRChaiHome"),
          ensure_participant_present_at_location=True,
          conversation=conversations["DA_Conversation_HBCT_PositiveReinforcement_ChaiCheckIn"], entry_node_id="PRC_01", auto_start=False),
        objective("O09_ReinforceChaiProgress", "Choose a response that recognizes Chai's food progress.", q.LEARNING_ACTIVITY,
          activity_tag=tag("VHV.Activity.HBCT.PositiveReinforcement.ChaiProgressResponse"), auto_start=True),
        objective("O10_ReinforceExercise", "Choose a response that supports Chai's walking progress.", q.CONVERSATION,
          participant_tag=tag("VHV.Participant.UncleChai"),
          conversation=conversations["DA_Conversation_HBCT_PositiveReinforcement_Exercise"], entry_node_id="PRE_01", auto_start=True),
        objective("O11_ChooseSelfReward", "Help Uncle Chai choose a meaningful self-reward.", q.CONVERSATION,
          participant_tag=tag("VHV.Participant.UncleChai"),
          conversation=conversations["DA_Conversation_HBCT_PositiveReinforcement_SelfReward"], entry_node_id="PRR_01", auto_start=True,
          completion_npc_moves=[npc_move("VHV.Participant.UncleChai", LOCATION+"PRChallengeWalkStart", 180.0)]),
        objective("O12_JoinWalkingChallenge", "Go with Uncle Chai to the village walking challenge.", q.WORLD_ACTION,
          world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.PositiveReinforcement.WalkingChallenge"),
          world_action_tag=tag("VHV.WorldAction.StartConversation"),
          world_action_start_policy=unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER,
          activation_npc_readiness=[npc_move("VHV.Participant.UncleChai", LOCATION+"PRChallengeWalkStart", 180.0)],
          npc_travel_mode=unreal.VHVQuestNPCTravelMode.FREE_ROAM,
          npc_move_stages=[
            move_stage([npc_move("VHV.Participant.UncleChai", LOCATION+"PRChallengeWalkMid", 180.0)]),
            move_stage([npc_move("VHV.Participant.UncleChai", LOCATION+"PRChallengeWalkEnd", 180.0)])],
          complete_after_npc_move_stages=False),
        objective("O13_SeveralWeeksLater", "See how consistent reinforcement affected progress.", q.NPC_ACTION,
          npc_participant_tag=tag("VHV.Participant.UncleChai"),
          npc_command_type=unreal.VHVNPCQuestCommandType.WAIT, npc_wait_duration=0.1,
          activation_stinger=hbct.major_stinger("", "SEVERAL WEEKS LATER", "", 1.8),
          completion_effects=[hbct.effect(FLAG+"PRSeveralWeeksLaterSeen")]),
        objective("O14_SeeFeedbackReinforcement", "Visit the health post and watch Uncle Chai receive progress feedback.", q.WORLD_ACTION,
          world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.PositiveReinforcement.FeedbackResult"),
          world_action_tag=tag("VHV.WorldAction.StartConversation"),
          world_action_start_policy=unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER,
          ensure_participant_present_at_location=True,
          activation_npc_readiness=[
            npc_move("VHV.Participant.PRHealthNurse", LOCATION+"PRHealthPostFeedback"),
            npc_move("VHV.Participant.UncleChai", LOCATION+"PRHealthPostChai")]),
        objective("O15_FinalDebrief", "Talk to the Instructor to review positive reinforcement.", q.CONVERSATION,
          participant_tag=tag("VHV.Participant.Instructor"), location_tag=tag(LOCATION+"PRFinalDebrief"),
          ensure_participant_present_at_location=True,
          conversation=conversations["DA_Conversation_HBCT_PositiveReinforcement_FinalDebrief"], entry_node_id="PRD_01", auto_start=False),
      ])
    objectives = list(quest.get_editor_property("objectives"))
    tracking.apply_tracking_metadata_to_objectives(QUEST_ID, objectives)
    quest.set_editor_property("objectives", objectives)
    quests = list(arc.get_editor_property("quests"))
    quests = [item for item in quests if str(item.get_editor_property("quest_id")) != QUEST_ID]
    expected = ["Q_HBCT_00_INTRO","Q_HBCT_01_MOTIVATION","Q_HBCT_02_GOAL_SETTING",
                "Q_HBCT_03_ROLE_MODEL","Q_HBCT_04_SOCIAL_SUPPORT","Q_HBCT_05_SELF_MONITORING"]
    if [str(item.get_editor_property("quest_id")) for item in quests] != expected:
        raise RuntimeError("Unexpected Quest 0-5 order; refusing to author Quest 6")
    quests_by_id={str(item.get_editor_property("quest_id")):item for item in quests}
    quests_by_id["Q_HBCT_04_SOCIAL_SUPPORT"].set_editor_property(
      "auto_start_next_quest", True)
    quests_by_id["Q_HBCT_05_SELF_MONITORING"].set_editor_property(
      "auto_start_next_quest", True)
    quest.set_editor_property("auto_start_next_quest", False)
    quests.append(quest)
    arc.set_editor_property("quests", quests)
    arc.set_editor_property("quest_arc_description",
        "Production HBCT story arc containing the prologue and Techniques 1-6.")


def actors(): return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def world_actors(): return list(actors().get_all_level_actors())


def participant_id(actor):
    if not actor or not hasattr(actor, "get_quest_participant_component"): return ""
    component = actor.get_quest_participant_component()
    return str(component.get_effective_participant_id()) if component else ""


def find_participant(leaf):
    matches = [actor for actor in world_actors() if participant_id(actor) == leaf]
    if len(matches) > 1: raise RuntimeError("Duplicate participant {}".format(leaf))
    return matches[0] if matches else None


def spawn(kind, label, xyz, yaw=180.0, folder=FOLDER):
    actor = actors().spawn_actor_from_class(kind, unreal.Vector(*xyz), unreal.Rotator(0, yaw, 0))
    if not actor: raise RuntimeError("Could not spawn {}".format(label))
    actor.set_actor_label(label); actor.set_folder_path(folder)
    actor.set_actor_location(unreal.Vector(*xyz), False, True)
    actor.set_actor_rotation(unreal.Rotator(0, yaw, 0), False)
    return actor


def configure_participant(actor, participant_tag, display_name):
    actor.get_quest_participant_component().set_editor_properties({
        "quest_participation_enabled": True, "participant_tag": tag(participant_tag)})
    actor.get_npc_interaction_component().set_editor_properties({
        "default_interaction_prompt": "Talk", "use_quest_objective_text_as_prompt": False,
        "interaction_enabled": True})
    hbct.configure_npc_nameplate(actor, display_name, True)
    return actor


def spawn_npc(kind, label, xyz, participant_tag, display_name):
    return configure_participant(spawn(kind, label, xyz, 180, FOLDER+"/Participants"), participant_tag, display_name)


def location(label, xyz, leaf, enabled=False, objective_id="", extent=(55,55,110), folder=None):
    actor = spawn(unreal.VHVQuestLocationVolume, label, xyz, 0, folder or FOLDER+"/Locations")
    actor.set_editor_properties({"location_tag": tag(LOCATION+leaf), "enabled": enabled,
      "trigger_world_action": False, "activate_current_objective": False,
      "required_active_quest_id": QUEST_ID if enabled else "",
      "required_active_objective_id": objective_id})
    actor.get_editor_property("box_component").set_box_extent(unreal.Vector(*extent), True)
    return actor


def world_trigger(label, xyz, leaf, objective_id, receiver, interaction,
                  track_as_objective=False, extent=(120,120,140), prompt="Observe"):
    actor = location(label, xyz, leaf, True, objective_id, extent, FOLDER+"/Triggers")
    actor.set_editor_properties({"trigger_world_action": True, "trigger_world_action_once": True,
      "track_world_action_as_objective": track_as_objective,
      "world_action_receiver_tag": tag(receiver),
      "world_action_tag": tag("VHV.WorldAction.StartConversation"),
      "require_player_interaction": interaction, "interaction_prompt": prompt})
    return actor


def objective_activation_trigger(label, xyz, leaf, objective_id, conditions,
                                 extent=(360,360,150), interaction=False,
                                 prompt="Continue"):
    actor = location(label, xyz, leaf, True, objective_id, extent, FOLDER+"/Triggers")
    actor.set_editor_properties({
      "activate_current_objective": True,
      "activate_current_objective_once": True,
      "trigger_conditions": conditions,
      "require_player_interaction": interaction,
      "interaction_prompt": prompt})
    return actor


def bind(slot, npc): return struct(unreal.VHVAmbientParticipantBinding, slot_id=slot, npc=npc)


def ambient(label, xyz, asset, receiver, bindings, folder):
    actor = spawn(unreal.VHVAmbientConversationActor, label, xyz, 0, folder)
    actor.set_editor_properties({"conversation_data": asset, "participants": bindings,
      "auto_start_on_begin_play": False, "requires_explicit_trigger": True,
      "play_once": True, "preserve_participant_behavior": True,
      "player_leave_policy": unreal.VHVAmbientConversationLeavePolicy.CONTINUE,
      "observation_radius": 2400.0})
    receiver_component = actor.get_editor_property("world_action_receiver")
    receiver_component.modify()
    receiver_component.set_editor_properties({
      "world_actions_enabled": True, "receiver_tag": tag(receiver)})
    return actor


def prop(label, xyz, scale, folder):
    actor = spawn(unreal.StaticMeshActor, label, xyz, 0, folder)
    actor.set_actor_scale3d(unreal.Vector(*scale)); return actor


def text_display(label, xyz, text, size, folder):
    actor = spawn(unreal.TextRenderActor, label, xyz, 0, folder)
    actor.set_actor_rotation(
      unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0), False)
    actor.get_component_by_class(unreal.TextRenderComponent).set_editor_properties({
      "text": text, "world_size": size,
      "text_render_color": unreal.Color(245, 238, 210, 255),
      "horizontal_alignment": unreal.HorizTextAligment.EHTA_CENTER,
      "vertical_alignment": unreal.VerticalTextAligment.EVRTA_TEXT_BOTTOM})
    return actor


def configure_map(ambient_assets):
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH): raise RuntimeError("Could not load map")
    for actor in list(world_actors()):
        if actor.get_actor_label().startswith(PREFIX): actors().destroy_actor(actor)
    npc_asset = unreal.load_asset(NPC_PATH)
    if not npc_asset: raise RuntimeError("Missing NPC blueprint")
    npc_class = npc_asset.generated_class()
    instructor = find_participant("Instructor"); chai = find_participant("UncleChai")
    if not instructor or not chai: raise RuntimeError("Persistent Instructor and Uncle Chai are required")
    configure_participant(instructor, "VHV.Participant.Instructor", "Instructor")
    configure_participant(chai, "VHV.Participant.UncleChai", "Uncle Chai")

    specs = [
      ("SuccessWalker",(100,-220,50),"PRSuccessWalker","Walker"),
      ("SuccessWalkerFriend",(100,-70,50),"PRSuccessWalkerFriend","Friend"),
      ("WaterChild",(100,180,50),"PRWaterChild","Child"),
      ("WaterParent",(100,330,50),"PRWaterParent","Parent"),
      ("HealthFairExerciseVillager",(1750,-420,50),"PRHealthFairExerciseVillager","Exercise Villager"),
      ("HealthFairVHV",(1900,-300,50),"PRHealthFairVHV","VHV"),
      ("MaterialVillager",(2050,-420,50),"PRMaterialVillager","Villager"),
      ("ActivityVillager",(2350,-20,50),"PRActivityVillager","Villager"),
      ("TokenVillager",(2050,380,50),"PRTokenVillager","Walker"),
      ("FeedbackVillager",(1750,380,50),"PRFeedbackVillager","Villager"),
      ("PreferencePraise",(3300,-250,50),"PRPreferencePraise","NPC A"),
      ("PreferenceActivity",(3400,0,50),"PRPreferenceActivity","NPC B"),
      ("PreferenceToken",(3500,250,50),"PRPreferenceToken","NPC C"),
      ("DemoVillager",(4600,150,50),"PRDemoVillager","Villager"),
      ("VillageHead",(8450,180,50),"PRVillageHead","Village Head"),
      ("HealthNurse",(9600,-20,50),"PRHealthNurse","Nurse")]
    p = {key:spawn_npc(npc_class, PREFIX+"NPC_"+key, xyz, "VHV.Participant."+leaf, display)
         for key,xyz,leaf,display in specs}

    anchors = {
      "PRSuccessWalker":(100,-220,50),"PRSuccessChild":(100,180,50),"PRSuccessChai":(250,-20,50),
      "PRSuccessChaiExit":(500,-20,50),"PRInstructorIntro":(-200,-20,50),
      "PRInstructorLesson":(900,-20,50),"PRHealthFairCenter":(2000,-20,50),
      "PRPreferencePraise":(3300,-250,50),
      "PRPreferenceActivity":(3400,0,50),"PRPreferenceToken":(3500,250,50),
      "PRTimingTooEarly":(4400,-250,50),"PRTimingTooLate":(4600,0,50),
      "PRTimingImmediate":(4800,250,50),"PRChaiHome":(6000,-20,50),
      "PRChallengeWalkStart":(7100,-20,50),"PRChallengeWalkMid":(7700,100,50),
      "PRChallengeWalkEnd":(8300,-20,50),
      "PRHealthPostChai":(9800,-20,50),"PRFinalDebrief":(10700,-20,50)}
    for leaf,xyz in anchors.items(): location(PREFIX+"Location_"+leaf, xyz, leaf)

    objective_activation_trigger(
      PREFIX+"Trigger_PreferenceMatching", (3400,-20,50), "PRPreferenceArea",
      "O05_ChooseMeaningfulReinforcement",
      all_flags(FLAG+"PRPreferencePraiseObserved",
                FLAG+"PRPreferenceActivityObserved",
                FLAG+"PRPreferenceTokenObserved"),
      interaction=True, prompt="Match Reinforcement Preferences")

    world_trigger(PREFIX+"Trigger_HealthParkIntro",(-50,-20,50),"PRHealthParkIntro","O01_NoticeSuccess",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.NoticeSuccess",False,False,(300,300,150))
    ambient(PREFIX+"Ambient_NoticeSuccess",(-50,-20,50),ambient_assets["DA_Ambient_HBCT_PositiveReinforcement_NoticeSuccess"],
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.NoticeSuccess",
      [bind("Walker",p["SuccessWalker"]),bind("Friend",p["SuccessWalkerFriend"]),
       bind("Child",p["WaterChild"]),bind("Parent",p["WaterParent"]),bind("Chai",chai)],FOLDER+"/01_NoticeSuccess")

    station_specs = [
      ("Social",(1750,-420,50),"PRHealthFairSocial","HealthFairVHV","HealthFairExerciseVillager","VHV"),
      ("Material",(2050,-420,50),"PRHealthFairMaterial","HealthFairVHV","MaterialVillager","VHV"),
      ("Activity",(2350,-20,50),"PRHealthFairActivity",None,"ActivityVillager",""),
      ("Token",(2050,380,50),"PRHealthFairToken","VillageHead","TokenVillager","Head"),
      ("Feedback",(1750,380,50),"PRHealthFairFeedback","HealthNurse","FeedbackVillager","Nurse")]
    for suffix,xyz,leaf,host,guest,host_slot in station_specs:
        receiver="VHV.WorldReceiver.HBCT.PositiveReinforcement.HealthFair"+suffix
        world_trigger(PREFIX+"Trigger_HealthFair"+suffix,xyz,leaf,"O03_ExploreHealthFair",receiver,True,False,
                      (120,120,140),"Observe {} Reinforcement".format(suffix))
        bindings=[bind("Guest",p[guest])]
        if host: bindings.append(bind(host_slot,p[host]))
        ambient(PREFIX+"Ambient_HealthFair"+suffix,xyz,
          ambient_assets["DA_Ambient_HBCT_PositiveReinforcement_HealthFair"+suffix],receiver,bindings,FOLDER+"/03_HealthFair")
        text_display(PREFIX+"Text_HealthFair"+suffix,(xyz[0],xyz[1],200),suffix.upper()+" REINFORCEMENT",18,FOLDER+"/03_HealthFair")
    prop(PREFIX+"Prop_MaterialFruit",(2120,-420,75),(.35,.35,.22),FOLDER+"/03_HealthFair")
    prop(PREFIX+"Prop_TokenBoard",(2200,380,120),(1.0,.10,1.0),FOLDER+"/03_HealthFair")
    text_display(PREFIX+"Text_TokenBoard",(2200,365,235),
      "VILLAGE WALKING PROGRAM\n1 WALK = STAR 1 POINT\n30 POINTS = SMALL REWARD",15,FOLDER+"/03_HealthFair")

    world_trigger(PREFIX+"Trigger_TimingArea",(4600,-20,50),"PRTimingArea","O06_TestReinforcementTiming",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.TimingDemo",False,True,(300,300,150))
    ambient(PREFIX+"Ambient_Timing",(4600,-20,50),ambient_assets["DA_Ambient_HBCT_PositiveReinforcement_Timing"],
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.TimingDemo",
      [bind("Instructor",instructor),bind("Demo",p["DemoVillager"])],FOLDER+"/06_Timing")
    text_display(PREFIX+"Text_TimingEarly",(4400,-250,180),"TOO EARLY",16,FOLDER+"/06_Timing")
    text_display(PREFIX+"Text_TimingLate",(4600,0,180),"TWO DAYS LATER",16,FOLDER+"/06_Timing")
    text_display(PREFIX+"Text_TimingImmediate",(4800,250,180),"IMMEDIATE",16,FOLDER+"/06_Timing")

    ambient(PREFIX+"Ambient_WalkingChallenge",(8450,-20,50),ambient_assets["DA_Ambient_HBCT_PositiveReinforcement_WalkingChallenge"],
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.WalkingChallenge",
      [bind("Head",p["VillageHead"]),bind("Chai",chai)],FOLDER+"/12_Challenge")
    world_trigger(PREFIX+"Trigger_WalkingChallenge",(8450,-20,50),"PRWalkingChallengeBoard","O12_JoinWalkingChallenge",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.WalkingChallenge",True,True,
      (180,180,150),"Join Walking Challenge")
    prop(PREFIX+"Prop_WalkingChallengeBoard",(8450,-20,130),(1.7,.12,1.35),FOLDER+"/12_Challenge")
    text_display(PREFIX+"Text_WalkingChallengeBoard",(8450,-35,290),
      "VILLAGE WALKING CHALLENGE\n1 WALK = STAR +1 POINT\n30 POINTS = SMALL REWARD",17,FOLDER+"/12_Challenge")

    world_trigger(PREFIX+"Trigger_HealthPostFeedback",(9600,-20,50),"PRHealthPostFeedback","O14_SeeFeedbackReinforcement",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.FeedbackResult",False,True,(260,260,150))
    ambient(PREFIX+"Ambient_FeedbackResult",(9600,-20,50),ambient_assets["DA_Ambient_HBCT_PositiveReinforcement_FeedbackResult"],
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.FeedbackResult",
      [bind("Nurse",p["HealthNurse"]),bind("Chai",chai)],FOLDER+"/14_Feedback")
    prop(PREFIX+"Prop_HealthResult",(9450,160,120),(1.15,.10,.9),FOLDER+"/14_Feedback")
    text_display(PREFIX+"Text_HealthResult",(9450,145,225),"RECENT RESULT\nGRADUAL IMPROVEMENT",16,FOLDER+"/14_Feedback")
    prop(PREFIX+"Prop_MonitoringCalendar",(9650,160,120),(1.15,.10,.9),FOLDER+"/14_Feedback")
    text_display(PREFIX+"Text_MonitoringCalendar",(9650,145,225),"MORE REGULAR WALKS\nFEWER SWEET DRINKS",16,FOLDER+"/14_Feedback")
    if not unreal.EditorLevelLibrary.save_current_level(): raise RuntimeError("Could not save map")


OBJECTIVES = ["O01_NoticeSuccess","O02_UnderstandReinforcement","O03_ExploreHealthFair",
 "O04_MatchReinforcementTypes","O05_ChooseMeaningfulReinforcement","O06_TestReinforcementTiming",
 "O07_OrderReinforcementTiming","O08_CheckInWithChai","O09_ReinforceChaiProgress",
 "O10_ReinforceExercise","O11_ChooseSelfReward","O12_JoinWalkingChallenge",
 "O13_SeveralWeeksLater","O14_SeeFeedbackReinforcement","O15_FinalDebrief"]


def tag_name(value):
    return str(unreal.GameplayTagLibrary.get_tag_name(value))


def condition_tags(condition_set):
    return [tag_name(item.get_editor_property("state_tag"))
            for item in condition_set.get_editor_property("conditions")]


def linked_activity_tags(conversation_asset):
    nodes=conversation_asset.get_editor_property("conversation").get_editor_property("nodes")
    return [tag_name(node.get_editor_property("linked_activity").get_editor_property("activity_tag"))
            for node in nodes
            if node.get_editor_property("node_type")==unreal.VHVDialogueNodeType.LEARNING_ACTIVITY]


def validate(level, arc, conversations, ambient_assets):
    quests=list(arc.get_editor_property("quests"))
    quests_by_id={str(item.get_editor_property("quest_id")):item for item in quests}
    if not quests_by_id["Q_HBCT_04_SOCIAL_SUPPORT"].get_editor_property("auto_start_next_quest"):
        raise RuntimeError("Quest 4 must auto-start Quest 5")
    if not quests_by_id["Q_HBCT_05_SELF_MONITORING"].get_editor_property("auto_start_next_quest"):
        raise RuntimeError("Quest 5 must auto-start Quest 6")
    matches=[item for item in quests if str(item.get_editor_property("quest_id"))==QUEST_ID]
    if len(matches)!=1: raise RuntimeError("Expected exactly one Quest 6 definition")
    quest=matches[0]
    objectives=list(quest.get_editor_property("objectives"))
    if [str(o.get_editor_property("objective_id")) for o in objectives]!=OBJECTIVES: raise RuntimeError("Objective order incorrect")
    if quest.get_editor_property("auto_start_next_quest"): raise RuntimeError("Quest 6 must stop")

    topics=[t for t in level.get_editor_property("day_data").get_editor_property("topics")
            if str(t.get_editor_property("topic_id"))==TOPIC_ID]
    if len(topics)!=1: raise RuntimeError("Expected exactly one Quest 6 textbook topic")
    activities=list(topics[0].get_editor_property("activities"))
    if len(activities)!=7: raise RuntimeError("Expected seven activities")
    activity_ids=[tag_name(item.get_editor_property("activity_tag")) for item in activities]
    if len(set(activity_ids))!=7: raise RuntimeError("Quest 6 activity IDs must be unique")

    by_id={str(item.get_editor_property("objective_id")):item for item in objectives}
    scene_flags={FLAG+"PRSuccessWalkerSeen",FLAG+"PRSuccessChildSeen",FLAG+"PRSuccessChaiSeen"}
    o01=by_id["O01_NoticeSuccess"]
    if condition_tags(o01.get_editor_property("activation_conditions")):
        raise RuntimeError("O01 may not wait on flags produced by its own world sequence")
    if (not o01.get_editor_property("auto_start")
        or o01.get_editor_property("ensure_participant_present_at_location")
        or not o01.get_editor_property("prepare_activation_npc_readiness_before_exposure")):
        raise RuntimeError("O01 must use one-time readiness preparation, not stationary presence")
    readiness=list(o01.get_editor_property("activation_npc_readiness"))
    if (len(readiness)!=1
        or tag_name(readiness[0].get_editor_property("participant_tag"))!="VHV.Participant.UncleChai"
        or tag_name(readiness[0].get_editor_property("destination_location_tag"))!=LOCATION+"PRSuccessChai"):
        raise RuntimeError("O01 must prepare the existing Uncle Chai at PRSuccessChai")
    if set(condition_tags(o01.get_editor_property("npc_move_stage_activation_conditions")))!=scene_flags:
        raise RuntimeError("O01 Chai exit must wait for all three vignettes")

    station_flags=[FLAG+"PRSocialStationSeen",FLAG+"PRMaterialStationSeen",
                   FLAG+"PRActivityStationSeen",FLAG+"PRTokenStationSeen",
                   FLAG+"PRFeedbackStationSeen"]
    if condition_tags(by_id["O03_ExploreHealthFair"].get_editor_property("completion_conditions"))!=station_flags:
        raise RuntimeError("O03 completion must require the five distinct station flags")
    preference_flags=[FLAG+"PRPreferencePraiseObserved",FLAG+"PRPreferenceActivityObserved",
                      FLAG+"PRPreferenceTokenObserved"]
    contextuals=list(by_id["O05_ChooseMeaningfulReinforcement"].get_editor_property("contextual_conversations"))
    if len(contextuals)!=3:
        raise RuntimeError("O05 requires three contextual villager conversations")
    expected_contextuals=[
      ("VHV.Participant.PRPreferencePraise",preference_flags[0]),
      ("VHV.Participant.PRPreferenceActivity",preference_flags[1]),
      ("VHV.Participant.PRPreferenceToken",preference_flags[2])]
    for contextual_data,(expected_participant,expected_flag) in zip(contextuals,expected_contextuals):
        conditions=list(contextual_data.get_editor_property("availability_conditions").get_editor_property("conditions"))
        if (tag_name(contextual_data.get_editor_property("participant_tag"))!=expected_participant
            or len(conditions)!=1
            or conditions[0].get_editor_property("condition_type")!=unreal.VHVStoryConditionType.FLAG_NOT_SET
            or tag_name(conditions[0].get_editor_property("state_tag"))!=expected_flag):
            raise RuntimeError("O05 contextual interaction is not independently one-shot: "+expected_participant)
    if linked_activity_tags(conversations["DA_Conversation_HBCT_PositiveReinforcement_Preferences"])!=[
            "VHV.Activity.HBCT.PositiveReinforcement.PreferenceMatching"]:
        raise RuntimeError("O05 follow-up must contain PreferenceMatching exactly once")
    if linked_activity_tags(conversations["DA_Conversation_HBCT_PositiveReinforcement_Exercise"])!=[
            "VHV.Activity.HBCT.PositiveReinforcement.ChaiExerciseResponse"]:
        raise RuntimeError("O10 must contain ChaiExerciseResponse exactly once")
    if linked_activity_tags(conversations["DA_Conversation_HBCT_PositiveReinforcement_SelfReward"])!=[
            "VHV.Activity.HBCT.PositiveReinforcement.SelfRewardChoice"]:
        raise RuntimeError("O11 must contain SelfRewardChoice exactly once")

    for objective_id, expected_specs in (
            ("O03_ExploreHealthFair",tracking.MULTI_TRACKING_DESTINATIONS[QUEST_ID]["O03_ExploreHealthFair"]),
            ("O05_ChooseMeaningfulReinforcement",tracking.MULTI_TRACKING_DESTINATIONS[QUEST_ID]["O05_ChooseMeaningfulReinforcement"])):
        destinations=list(by_id[objective_id].get_editor_property("tracking_destinations"))
        actual=[(tag_name(item.get_editor_property("destination_location_tag")),
                 condition_tags(item.get_editor_property("completion_conditions")))
                for item in destinations]
        expected=[(spec[0],[spec[2]]) for spec in expected_specs]
        if actual!=expected:
            raise RuntimeError("{} multi-destination tracking is incomplete".format(objective_id))

    managed=[a for a in world_actors() if a.get_actor_label().startswith(PREFIX)]
    labels={a.get_actor_label():a for a in managed}
    station_specs=[
      ("Social","VHV.WorldReceiver.HBCT.PositiveReinforcement.HealthFairSocial",station_flags[0]),
      ("Material","VHV.WorldReceiver.HBCT.PositiveReinforcement.HealthFairMaterial",station_flags[1]),
      ("Activity","VHV.WorldReceiver.HBCT.PositiveReinforcement.HealthFairActivity",station_flags[2]),
      ("Token","VHV.WorldReceiver.HBCT.PositiveReinforcement.HealthFairToken",station_flags[3]),
      ("Feedback","VHV.WorldReceiver.HBCT.PositiveReinforcement.HealthFairFeedback",station_flags[4])]
    for suffix,receiver,completion_flag in station_specs:
        station=labels.get(PREFIX+"Trigger_HealthFair"+suffix)
        if (not station
            or str(station.get_editor_property("required_active_objective_id"))!="O03_ExploreHealthFair"
            or not station.get_editor_property("require_player_interaction")
            or station.get_editor_property("track_world_action_as_objective")
            or tag_name(station.get_editor_property("world_action_receiver_tag"))!=receiver):
            raise RuntimeError("Invalid O03 station trigger: "+suffix)
        effects=ambient_assets["DA_Ambient_HBCT_PositiveReinforcement_HealthFair"+suffix].get_editor_property("completion_effects")
        if [tag_name(effect.get_editor_property("state_tag")) for effect in effects]!=[completion_flag]:
            raise RuntimeError("Incorrect O03 station completion flag: "+suffix)

    semantic={}
    for actor in world_actors():
        if isinstance(actor,unreal.VHVQuestLocationVolume):
            leaf=str(actor.get_effective_location_id())
            if leaf.startswith("PR"): semantic.setdefault(leaf,[]).append(actor.get_actor_label())
    bad={leaf:names for leaf,names in semantic.items() if len(names)!=1}
    if bad: raise RuntimeError("Ambiguous location leaves: {}".format(bad))
    required_tracking=list(tracking.TRACKING_LOCATIONS[QUEST_ID].values())
    required_tracking.extend(spec[0]
      for destinations in tracking.MULTI_TRACKING_DESTINATIONS[QUEST_ID].values()
      for spec in destinations)
    for location_tag in required_tracking:
        leaf=location_tag.rsplit(".",1)[-1]
        if len(semantic.get(leaf,[]))!=1: raise RuntimeError("Tracking location does not resolve once: "+leaf)

    for actor in managed:
        if isinstance(actor,unreal.TextRenderActor):
            rotation=actor.get_actor_rotation()
            if abs(rotation.pitch)>.1 or abs(rotation.roll)>.1 or abs(abs(rotation.yaw)-180)>.1:
                raise RuntimeError("Invalid TextRender orientation: "+actor.get_actor_label())
    for leaf in ("Instructor","UncleChai"):
        if len([a for a in world_actors() if participant_id(a)==leaf])!=1: raise RuntimeError("Duplicate "+leaf)
    owned_participants=["PRPreferencePraise","PRPreferenceActivity","PRPreferenceToken",
                        "PRDemoVillager","PRVillageHead","PRHealthNurse"]
    for leaf in owned_participants:
        if len([a for a in world_actors() if participant_id(a)==leaf])!=1:
            raise RuntimeError("Expected exactly one Quest 6 participant: "+leaf)

    expected_receivers={
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.NoticeSuccess",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.HealthFairSocial",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.HealthFairMaterial",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.HealthFairActivity",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.HealthFairToken",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.HealthFairFeedback",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.TimingDemo",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.WalkingChallenge",
      "VHV.WorldReceiver.HBCT.PositiveReinforcement.FeedbackResult"}
    receiver_counts={name:0 for name in expected_receivers}
    for actor in world_actors():
        if not isinstance(actor,unreal.VHVAmbientConversationActor): continue
        receiver=actor.get_editor_property("world_action_receiver")
        receiver_id=tag_name(receiver.get_editor_property("receiver_tag"))
        if receiver_id in receiver_counts:
            receiver_counts[receiver_id]+=1
            if not actor.get_editor_property("play_once"):
                raise RuntimeError("Quest 6 ambient receiver must be one-shot: "+receiver_id)
    invalid_receivers={name:count for name,count in receiver_counts.items() if count!=1}
    if invalid_receivers: raise RuntimeError("Quest 6 receivers must resolve once: {}".format(invalid_receivers))

    preference_prompt=labels.get(PREFIX+"Trigger_PreferenceMatching")
    intro_trigger=labels.get(PREFIX+"Trigger_HealthParkIntro")
    if (not intro_trigger
        or str(intro_trigger.get_editor_property("required_active_objective_id"))!="O01_NoticeSuccess"
        or not intro_trigger.get_editor_property("trigger_world_action")
        or intro_trigger.get_editor_property("require_player_interaction")
        or tag_name(intro_trigger.get_editor_property("world_action_receiver_tag"))!=
            "VHV.WorldReceiver.HBCT.PositiveReinforcement.NoticeSuccess"):
        raise RuntimeError("O01 HealthParkIntro trigger is missing or incorrectly gated")
    notice_effects=ambient_assets["DA_Ambient_HBCT_PositiveReinforcement_NoticeSuccess"].get_editor_property("completion_effects")
    if {tag_name(effect.get_editor_property("state_tag")) for effect in notice_effects}!=scene_flags:
        raise RuntimeError("O01 NoticeSuccess ambient must produce all three vignette flags")
    for leaf in ("PRSuccessChai","PRSuccessChaiExit"):
        if len(semantic.get(leaf,[]))!=1:
            raise RuntimeError("O01 Chai location must resolve exactly once: "+leaf)
    if (not preference_prompt or not preference_prompt.get_editor_property("require_player_interaction")
        or condition_tags(preference_prompt.get_editor_property("trigger_conditions"))!=preference_flags):
        raise RuntimeError("O05 follow-up must become interactable without exit/re-entry")

    o06=by_id["O06_TestReinforcementTiming"]
    if len(o06.get_editor_property("activation_npc_readiness"))!=2:
        raise RuntimeError("O06 requires Instructor and demo participant readiness")
    o12=by_id["O12_JoinWalkingChallenge"]
    if o12.get_editor_property("ensure_participant_present_at_location"): raise RuntimeError("O12 may not teleport Chai")
    if o12.get_editor_property("world_action_start_policy")!=unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER:
        raise RuntimeError("O12 payoff may not start remotely")
    stages=list(o12.get_editor_property("npc_move_stages"))
    if len(stages)!=2: raise RuntimeError("O12 requires Mid and End stages")
    expected_stage_tags=[LOCATION+"PRChallengeWalkMid",LOCATION+"PRChallengeWalkEnd"]
    for stage,expected_tag in zip(stages,expected_stage_tags):
        moves=list(stage.get_editor_property("moves"))
        if (len(moves)!=1
            or tag_name(moves[0].get_editor_property("destination_location_tag"))!=expected_tag
            or abs(float(moves[0].get_editor_property("move_speed_override"))-180.0)>.01):
            raise RuntimeError("O12 travel stage is incorrect: "+expected_tag)
    challenge=labels.get(PREFIX+"Trigger_WalkingChallenge")
    if (not challenge or not challenge.get_editor_property("require_player_interaction")
        or not challenge.get_editor_property("track_world_action_as_objective")
        or str(challenge.get_editor_property("required_active_objective_id"))!="O12_JoinWalkingChallenge"):
        raise RuntimeError("O12 destination payoff trigger is invalid")

    o13=by_id["O13_SeveralWeeksLater"]
    if (o13.get_editor_property("npc_command_type")!=unreal.VHVNPCQuestCommandType.WAIT
        or float(o13.get_editor_property("npc_wait_duration"))<=0.0):
        raise RuntimeError("O13 must remain an automatic finite WAIT transition")
    feedback=labels.get(PREFIX+"Trigger_HealthPostFeedback")
    if (not feedback or not feedback.get_editor_property("track_world_action_as_objective")
        or str(feedback.get_editor_property("required_active_objective_id"))!="O14_SeeFeedbackReinforcement"):
        raise RuntimeError("O14 feedback receiver/trigger is missing")


def run():
    level=unreal.EditorAssetLibrary.load_asset(LEVEL_PATH); arc=unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    if not level or not arc: raise RuntimeError("Missing production HBCT data")
    conversations=create_conversations(); ambient_assets=create_ambient_assets()
    configure_level(level); configure_arc(arc,conversations); configure_map(ambient_assets)
    hbct.save_asset(level); hbct.save_asset(arc)
    for asset in list(conversations.values())+list(ambient_assets.values()): hbct.save_asset(asset)
    validate(level,arc,conversations,ambient_assets)
    unreal.log("{} Authored and validated Quest 6".format(LOG))


if __name__=="__main__":
    try: run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG,traceback.format_exc())); raise
