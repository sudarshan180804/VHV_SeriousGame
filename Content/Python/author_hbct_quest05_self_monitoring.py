"""Author production Quest 5: Self-Monitoring / Self-Reminder.

This idempotent script owns only Q_HBCT_05_SELF_MONITORING, its HBCT topic
records and assets, and map actors labelled HBCT_SM_. Quest 0-4 definitions
and every unrelated map actor are preserved; Quest 4 receives only the link
that starts Quest 5.
"""

import os
import traceback
import unreal

import author_hbct_milestone_01 as hbct
import author_objective_location_tracking as objective_tracking


ROOT = "/Game/VHV_Stuff/HBCT"
ACTIVITY_DIR = ROOT + "/Activities"
CONVERSATION_DIR = ROOT + "/Dialogue"
AMBIENT_DIR = ROOT + "/Ambient"
QUEST_DIR = ROOT + "/Quests"
LEVEL_PATH = ACTIVITY_DIR + "/DA_Level_HBCT"
ARC_PATH = QUEST_DIR + "/DA_QuestArc_HBCT"
MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout"
NPC_BP_PATH = "/Game/VHV_Stuff/NPC/BP_VHVNPCBase"
QUEST_ID = "Q_HBCT_05_SELF_MONITORING"
TOPIC_ID = "HBCT_M05_SELF_MONITORING"
PREFIX = "HBCT_SM_"
LOG = "[HBCT Self Monitoring]"


def struct(struct_type, **properties):
    return hbct.struct(struct_type, **properties)


def tag(name):
    return hbct.tag(name)


def effect(name):
    return hbct.effect(name)


def flag_condition(name, is_set=True):
    return struct(
        unreal.VHVStoryCondition,
        condition_type=(unreal.VHVStoryConditionType.FLAG_SET
                        if is_set else unreal.VHVStoryConditionType.FLAG_NOT_SET),
        state_tag=tag(name))


def condition_set(conditions):
    return struct(
        unreal.VHVStoryConditionSet,
        match_mode=unreal.VHVStoryConditionMatch.ALL,
        conditions=conditions)


def all_flags(*names):
    return condition_set([flag_condition(name) for name in names])


def flag_not_set(name):
    return condition_set([flag_condition(name, False)])


def objective(objective_id, text, objective_type, **properties):
    values = {
        "objective_id": objective_id,
        "objective_text": text,
        "objective_type": objective_type,
        "completion_effects": [],
    }
    values.update(properties)
    return struct(unreal.VHVQuestObjectiveDefinition, **values)


def npc_move(participant, destination, face=True, speed=0.0):
    return struct(
        unreal.VHVQuestNPCMoveRequest,
        participant_tag=tag(participant),
        destination_location_tag=tag(destination),
        face_destination_rotation=face,
        move_speed_override=speed)


def move_stage(moves, receiver=""):
    return struct(
        unreal.VHVQuestNPCMoveStage,
        moves=moves,
        ambient_conversation_receiver_tag=(tag(receiver) if receiver else unreal.GameplayTag()))


def assessed_activity(activity_tag, title, activity_type, prompt, **properties):
    values = {
        "correct_feedback": properties.pop("correct_feedback", "Correct."),
        "incorrect_feedback": properties.pop("incorrect_feedback", "Review the evidence and try again."),
        "partial_feedback": "",
        "require_correct_answer_to_advance": True,
    }
    values.update(properties)
    return hbct.activity(
        activity_tag, title, activity_type, prompt, unreal.TeachingContent(), **values)


def build_activities():
    single = unreal.TextbookActivityType.SINGLE_CHOICE
    matching = unreal.TextbookActivityType.MATCHING
    ordering = unreal.TextbookActivityType.ORDERING
    observation = unreal.TextbookActivityType.OBSERVATION
    return [
        hbct.activity(
            "VHV.Activity.HBCT.SelfMonitoring.MonitoringLoop",
            "SELF-MONITORING", observation,
            "Observe, record, compare, and adjust.",
            hbct.teaching(
                "SELF-MONITORING",
                "Self-monitoring helps a person see whether behaviour change is succeeding or failing, so the plan can be adjusted when necessary.",
                ["OBSERVE", "RECORD", "COMPARE", "ADJUST"]),
            correct_feedback=""),
        assessed_activity(
            "VHV.Activity.HBCT.SelfMonitoring.TargetWalkingBehavior",
            "Choose a Behaviour Target", single,
            "Which is the clearest behaviour target to monitor?",
            question=hbct.question(
                "HBCT_SM_TARGET_WALKING",
                "Which is the clearest behaviour target to monitor?",
                ["Improve his health", "Keep his blood sugar good",
                 "Walk for 30 minutes, 5 days per week", "Become healthier"], 2),
            attempt_policy=hbct.attempt_policy(
                "Choose something Chai can directly perform and count.",
                "A monitoring target must describe observable behaviour."),
            correct_feedback="A clear monitoring target describes a specific behaviour that can actually be observed and recorded.",
            success_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMTargetWalkingBehaviorChosen")]),
        assessed_activity(
            "VHV.Activity.HBCT.SelfMonitoring.TargetFoodBehavior",
            "Choose Food Behaviours", single,
            "Which food behaviour would also be practical to monitor?",
            question=hbct.question(
                "HBCT_SM_TARGET_FOOD",
                "Which food behaviour would also be practical to monitor?",
                ["Become healthier", "Add vegetables to meals and record sweet-drink intake",
                 "Lower his blood sugar", "Lose weight"], 1),
            attempt_policy=hbct.attempt_policy(
                "Choose actions that can be observed at meals.",
                "Blood sugar and weight are outcomes, while food choices are behaviours."),
            correct_feedback="Monitor the behaviour itself, not only the health outcome.",
            success_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMTargetFoodBehaviorChosen")]),
        assessed_activity(
            "VHV.Activity.HBCT.SelfMonitoring.ProcessOrdering",
            "Build the Monitoring Process", ordering,
            "Put the self-monitoring process in order.",
            ordering_items=hbct.ordering_items([
                "Choose the behaviour to observe",
                "Choose when to record it",
                "Choose a recording method",
                "Perform and record the behaviour",
                "Display the results",
                "Review what the results mean",
            ]),
            correct_order=["1", "2", "3", "4", "5", "6"],
            correct_feedback="Recording becomes useful when the person reviews what the record means.",
            success_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMProcessOrdered")]),
        assessed_activity(
            "VHV.Activity.HBCT.SelfMonitoring.ChaiMethodChoice",
            "Choose Chai's Monitoring Method", single,
            "Which method best fits Uncle Chai right now?",
            question=hbct.question(
                "HBCT_SM_METHOD_CHOICE",
                "Which method best fits Uncle Chai right now?",
                ["Notebook", "Wall calendar", "Phone reminder", "Memory alone"], 1),
            attempt_policy=hbct.attempt_policy(
                "Use what Chai said about what he will actually continue.",
                "The visible wall calendar is quick and already fits Chai's routine."),
            correct_feedback="The best monitoring method is the one the person can realistically continue using.",
            success_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMMethodChosen")]),
        assessed_activity(
            "VHV.Activity.HBCT.SelfMonitoring.MethodMatching",
            "Match Method to Person", matching,
            "Match each person to a monitoring method they can realistically use.",
            matching_pairs=[
                struct(unreal.MatchingPair, left_text="Older villager who rarely uses a phone", right_text="Wall calendar"),
                struct(unreal.MatchingPair, left_text="Person who enjoys writing meals down", right_text="Notebook"),
                struct(unreal.MatchingPair, left_text="Busy worker who always carries a phone", right_text="Phone reminder"),
            ],
            correct_feedback="There is no universally best method. The method should fit the person.",
            success_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMMethodMatched")]),
        assessed_activity(
            "VHV.Activity.HBCT.SelfMonitoring.PatternBusyDays",
            "Find the Busy-Day Pattern", single,
            "What pattern is most useful here?",
            question=hbct.question(
                "HBCT_SM_PATTERN_BUSY",
                "What pattern is most useful here?",
                ["Chai does not care about exercise",
                 "Walking is often missed on his busiest days",
                 "Walking is impossible for Chai", "His support network failed"], 1),
            attempt_policy=hbct.attempt_policy(
                "Compare the missed walks with the market schedule.",
                "Both missed walks occurred on Chai's busiest market days."),
            correct_feedback="The record links missed walking with Chai's busiest days.",
            success_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMPatternBusyDaysFound")]),
        assessed_activity(
            "VHV.Activity.HBCT.SelfMonitoring.PatternSweetDrinks",
            "Find the Sweet-Drink Pattern", single,
            "What does the record suggest about sweet drinks?",
            question=hbct.question(
                "HBCT_SM_PATTERN_SWEET",
                "What does the record suggest about sweet drinks?",
                ["They happen randomly", "They are more likely during visits and social situations",
                 "He drinks them every day", "He should stop visiting people"], 1),
            attempt_policy=hbct.attempt_policy(
                "Compare the drink marks with where Chai was on those days.",
                "The marked drinks cluster around visits and social situations."),
            correct_feedback="Sweet drinks are more likely for Chai during visits and social situations.",
            success_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMPatternSweetDrinkContextFound")]),
        assessed_activity(
            "VHV.Activity.HBCT.SelfMonitoring.PatternWalkingGoal",
            "Interpret the Walking Goal", single,
            "What does the weekly record show about the walking plan?",
            question=hbct.question(
                "HBCT_SM_PATTERN_WALK_GOAL",
                "What does the weekly record show about the walking plan?",
                ["It completely failed", "The goal was achieved despite two missed days",
                 "He should stop monitoring", "Every day must be identical"], 1),
            attempt_policy=hbct.attempt_policy(
                "Compare five successful days with the five-day goal.",
                "The weekly target allows two missed days and was still achieved."),
            correct_feedback="Chai achieved the five-day walking goal despite two missed days.",
            success_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMPatternWalkingGoalFound")]),
        assessed_activity(
            "VHV.Activity.HBCT.SelfMonitoring.PatternAdjustmentMatching",
            "Turn Patterns into Adjustments", matching,
            "Match each recorded pattern to a useful adjustment.",
            matching_pairs=[
                struct(unreal.MatchingPair,
                       left_text="Walking missed on busy market days",
                       right_text="Take a shorter walk or choose another time on busy days"),
                struct(unreal.MatchingPair,
                       left_text="Sweet drinks common during social visits",
                       right_text="Prepare/order an alternative drink during social visits"),
                struct(unreal.MatchingPair,
                       left_text="Walking goal reached five days",
                       right_text="Continue the current walking strategy and keep monitoring"),
            ],
            correct_feedback="The record shows what should change and what is already working.",
            success_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMAdjustmentCompleted")]),
        assessed_activity(
            "VHV.Activity.HBCT.SelfMonitoring.ProgressGraphOrdering",
            "Build the Progress Graph", ordering,
            "Place the four weekly walking results in order.",
            ordering_items=hbct.ordering_items([
                "Week 1 - 3 walking-goal days",
                "Week 2 - 4 walking-goal days",
                "Week 3 - 5 walking-goal days",
                "Week 4 - 5 walking-goal days",
            ]),
            correct_order=["1", "2", "3", "4"],
            correct_feedback="The graph makes Chai's increasing consistency visible: 3, 4, 5, 5.",
            success_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMProgressGraphBuilt")]),
        assessed_activity(
            "VHV.Activity.HBCT.SelfMonitoring.ReminderChoice",
            "Place a Useful Reminder", single,
            "Where should Uncle Chai place his reminder so it fits his routine?",
            question=hbct.question(
                "HBCT_SM_REMINDER",
                "Where should Uncle Chai place his reminder so it fits his routine?",
                ["Calendar beside the walking shoes", "Calendar hidden in a cupboard",
                 "Phone reminder despite Chai barely using his phone",
                 "Notebook somewhere he rarely visits"], 0),
            attempt_policy=hbct.attempt_policy(
                "Choose the cue Chai will see when the behaviour happens.",
                "A visible calendar beside the shoes connects the reminder to his walking routine."),
            correct_feedback="A useful reminder should appear where the behaviour already happens.",
            success_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMReminderConfigured")]),
    ]


def configure_level_data(level):
    day = level.get_editor_property("day_data")
    topics = [topic for topic in list(day.get_editor_property("topics"))
              if str(topic.get_editor_property("topic_id")) != TOPIC_ID]
    topics.append(struct(
        unreal.TopicData,
        topic_id=TOPIC_ID,
        topic_title="Technique 5 - Self-Monitoring",
        narrative_introduction="Make Uncle Chai's real behavior visible, find patterns, and adjust the plan.",
        activities=build_activities()))
    day.set_editor_property("topics", topics)
    day.set_editor_property(
        "completion_summary",
        "HBCT Techniques 1-5 complete through self-monitoring and adjustment.")
    takeaways = list(day.get_editor_property("completion_takeaways"))
    takeaway = "People cannot clearly improve a behaviour if they cannot clearly see the pattern."
    if takeaway not in [str(item) for item in takeaways]:
        takeaways.append(takeaway)
    day.set_editor_property("completion_takeaways", takeaways)
    level.set_editor_property("day_data", day)


def create_conversations():
    text = hbct.text_node
    learn = hbct.learning_node
    specs = {
        "DA_Conversation_HBCT_SelfMonitoring_ChaiCheckIn": (
            "HBCT_SelfMonitoring_ChaiCheckIn", [
                text("SMCheck_01", "Player", "How have things been going?", "SMCheck_02"),
                text("SMCheck_02", "Uncle Chai", "I'm trying.", "SMCheck_03", "VHV.Participant.UncleChai"),
                text("SMCheck_03", "Uncle Chai", "But some days I forget to exercise.", "SMCheck_04", "VHV.Participant.UncleChai"),
                text("SMCheck_04", "Uncle Chai", "And sometimes I eat more than I realise.", "SMCheck_05", "VHV.Participant.UncleChai"),
                text("SMCheck_05", "Player", "How many days did you walk this week?", "SMCheck_06"),
                text("SMCheck_06", "Uncle Chai", "Hmm...", "SMCheck_07", "VHV.Participant.UncleChai"),
                text("SMCheck_07", "Uncle Chai", "Maybe four?", "SMCheck_08", "VHV.Participant.UncleChai"),
                text("SMCheck_08", "Uncle Chai", "Or three.", "SMCheck_09", "VHV.Participant.UncleChai"),
                text("SMCheck_09", "Uncle Chai", "I'm not sure.", "SMCheck_10", "VHV.Participant.UncleChai"),
                text("SMCheck_10", "Player", "And how many days did you have sweet drinks?", "SMCheck_11"),
                text("SMCheck_11", "Uncle Chai", "I honestly don't remember.", "SMCheck_12", "VHV.Participant.UncleChai"),
                text("SMCheck_12", "Player", "So you feel like you're doing better...", "SMCheck_13"),
                text("SMCheck_13", "Player", "...but you can't actually tell how often these things are happening.", "SMCheck_14"),
                text("SMCheck_14", "Uncle Chai", "I suppose that's true.", speaker_tag="VHV.Participant.UncleChai"),
            ]),
        "DA_Conversation_HBCT_SelfMonitoring_InstructorLesson": (
            "HBCT_SelfMonitoring_InstructorLesson", [
                text("SMLesson_01", "Instructor", "This is common.", "SMLesson_02", "VHV.Participant.Instructor"),
                text("SMLesson_02", "Instructor", "We often think we know how consistently we're behaving.", "SMLesson_03", "VHV.Participant.Instructor"),
                text("SMLesson_03", "Instructor", "But memory gives us an impression.", "SMLesson_04", "VHV.Participant.Instructor"),
                text("SMLesson_04", "Instructor", "A record gives us evidence.", "SMLesson_Activity", "VHV.Participant.Instructor"),
                learn("SMLesson_Activity", "VHV.Activity.HBCT.SelfMonitoring.MonitoringLoop", topic_id=TOPIC_ID),
            ]),
        "DA_Conversation_HBCT_SelfMonitoring_TargetBehavior": (
            "HBCT_SelfMonitoring_TargetBehavior", [
                learn("SMTarget_Activity1", "VHV.Activity.HBCT.SelfMonitoring.TargetWalkingBehavior", "SMTarget_Activity2", TOPIC_ID),
                learn("SMTarget_Activity2", "VHV.Activity.HBCT.SelfMonitoring.TargetFoodBehavior", "SMTarget_End", TOPIC_ID),
                text("SMTarget_End", "Player", "Now we have behaviours that can actually be observed and recorded."),
            ]),
        "DA_Conversation_HBCT_SelfMonitoring_MonitoringProcess": (
            "HBCT_SelfMonitoring_MonitoringProcess", [
                learn("SMProcess_Activity", "VHV.Activity.HBCT.SelfMonitoring.ProcessOrdering", "SMProcess_01", TOPIC_ID),
                text("SMProcess_01", "Instructor", "Exactly.", "SMProcess_02", "VHV.Participant.Instructor"),
                text("SMProcess_02", "Instructor", "Recording alone isn't the whole technique.", "SMProcess_03", "VHV.Participant.Instructor"),
                text("SMProcess_03", "Instructor", "The record becomes useful when we look back at it.", speaker_tag="VHV.Participant.Instructor"),
            ]),
        "DA_Conversation_HBCT_SelfMonitoring_ChooseMethod": (
            "HBCT_SelfMonitoring_ChooseMethod", [
                learn("SMMethod_Activity1", "VHV.Activity.HBCT.SelfMonitoring.ChaiMethodChoice", "SMMethod_Activity2", TOPIC_ID),
                learn("SMMethod_Activity2", "VHV.Activity.HBCT.SelfMonitoring.MethodMatching", "SMMethod_End", TOPIC_ID),
                text("SMMethod_End", "Instructor", "There is no universally best method. It has to fit the person.", speaker_tag="VHV.Participant.Instructor"),
            ]),
        "DA_Conversation_HBCT_SelfMonitoring_FirstDayReview": (
            "HBCT_SelfMonitoring_FirstDayReview", [
                text("SMReview_01", "Uncle Chai", "So I did some things well...", "SMReview_02", "VHV.Participant.UncleChai"),
                text("SMReview_02", "Uncle Chai", "...and I still had the sweet drink.", "SMReview_03", "VHV.Participant.UncleChai"),
                text("SMReview_03", "Instructor", "Exactly.", "SMReview_04", "VHV.Participant.Instructor"),
                text("SMReview_04", "Instructor", "A record isn't there to call the whole day good or bad.", "SMReview_05", "VHV.Participant.Instructor"),
                text("SMReview_05", "Instructor", "It tells us what actually happened.", "SMReview_06", "VHV.Participant.Instructor"),
                text("SMReview_06", "Player", "And tomorrow gives us another record."),
            ]),
        "DA_Conversation_HBCT_SelfMonitoring_OneWeekLater": (
            "HBCT_SelfMonitoring_OneWeekLater", [
                text("SMWeek_01", "Uncle Chai", "I thought I was missing more days than this.", "SMWeek_02", "VHV.Participant.UncleChai"),
                text("SMWeek_02", "Instructor", "That's why we record.", speaker_tag="VHV.Participant.Instructor"),
            ]),
        "DA_Conversation_HBCT_SelfMonitoring_PatternInvestigation": (
            "HBCT_SelfMonitoring_PatternInvestigation", [
                learn("SMPattern_Activity1", "VHV.Activity.HBCT.SelfMonitoring.PatternBusyDays", "SMPattern_Activity2", TOPIC_ID),
                learn("SMPattern_Activity2", "VHV.Activity.HBCT.SelfMonitoring.PatternSweetDrinks", "SMPattern_Activity3", TOPIC_ID),
                learn("SMPattern_Activity3", "VHV.Activity.HBCT.SelfMonitoring.PatternWalkingGoal", "SMPattern_End", TOPIC_ID),
                text("SMPattern_End", "Player", "Busy days affect walking, social visits affect drinks, and the five-day walking goal was still achieved."),
            ]),
        "DA_Conversation_HBCT_SelfMonitoring_Adjustment": (
            "HBCT_SelfMonitoring_Adjustment", [
                learn("SMAdjust_Activity", "VHV.Activity.HBCT.SelfMonitoring.PatternAdjustmentMatching", "SMAdjust_01", TOPIC_ID),
                text("SMAdjust_01", "Instructor", "Now the record has done its job.", "SMAdjust_02", "VHV.Participant.Instructor"),
                text("SMAdjust_02", "Player", "It showed us what needed changing.", "SMAdjust_03"),
                text("SMAdjust_03", "Instructor", "Exactly.", "SMAdjust_04", "VHV.Participant.Instructor"),
                text("SMAdjust_04", "Instructor", "Monitoring is not about judging Chai.", "SMAdjust_05", "VHV.Participant.Instructor"),
                text("SMAdjust_05", "Instructor", "It gives us information for adjustment.", speaker_tag="VHV.Participant.Instructor"),
            ]),
        "DA_Conversation_HBCT_SelfMonitoring_ProgressGraph": (
            "HBCT_SelfMonitoring_ProgressGraph", [
                learn("SMGraph_Activity", "VHV.Activity.HBCT.SelfMonitoring.ProgressGraphOrdering", "SMGraph_01", TOPIC_ID),
                text("SMGraph_01", "Player", "You can actually see the behaviour becoming more consistent.", "SMGraph_02"),
                text("SMGraph_02", "Instructor", "And Chai can see it too.", "SMGraph_03", "VHV.Participant.Instructor"),
                text("SMGraph_03", "Uncle Chai", "It feels different when I can actually see the progress.", speaker_tag="VHV.Participant.UncleChai"),
            ]),
        "DA_Conversation_HBCT_SelfMonitoring_SelfReminder": (
            "HBCT_SelfMonitoring_SelfReminder", [
                learn("SMReminder_Activity", "VHV.Activity.HBCT.SelfMonitoring.ReminderChoice", "SMReminder_01", TOPIC_ID),
                text("SMReminder_01", "Uncle Chai", "If I see it when I put my shoes away...", "SMReminder_02", "VHV.Participant.UncleChai"),
                text("SMReminder_02", "Uncle Chai", "...I'll remember to mark the walk.", "SMReminder_03", "VHV.Participant.UncleChai"),
                text("SMReminder_03", "Instructor", "A useful reminder should appear where the behaviour already happens.", speaker_tag="VHV.Participant.Instructor"),
            ]),
        "DA_Conversation_HBCT_SelfMonitoring_FinalDebrief": (
            "HBCT_SelfMonitoring_FinalDebrief", [
                text("SMFinal_01", "Instructor", "Goals tell us where we want to go.", "SMFinal_02", "VHV.Participant.Instructor"),
                text("SMFinal_02", "Instructor", "Self-monitoring tells us where we actually are.", "SMFinal_03", "VHV.Participant.Instructor"),
                text("SMFinal_03", "Player", "So the record doesn't change the behaviour by itself.", "SMFinal_04"),
                text("SMFinal_04", "Instructor", "No.", "SMFinal_05", "VHV.Participant.Instructor"),
                text("SMFinal_05", "Instructor", "It helps the person see the pattern clearly.", "SMFinal_06", "VHV.Participant.Instructor"),
                text("SMFinal_06", "Instructor", "Then they can decide what should stay the same...", "SMFinal_07", "VHV.Participant.Instructor"),
                text("SMFinal_07", "Instructor", "...and what needs to change.", "SMFinal_08", "VHV.Participant.Instructor"),
                text("SMFinal_08", "Instructor", "OBSERVE. RECORD. SEE THE PATTERN. COMPARE. ADJUST.", "SMFinal_09", "VHV.Participant.Instructor"),
                text("SMFinal_09", "Instructor", "People cannot clearly improve a behaviour if they cannot clearly see the pattern.", speaker_tag="VHV.Participant.Instructor"),
            ]),
    }
    assets = {}
    for name, (conversation_id, nodes) in specs.items():
        asset = hbct.load_or_create(CONVERSATION_DIR, name, unreal.VHVConversationDataAsset)
        hbct.set_conversation(asset, conversation_id, nodes)
        assets[name] = asset
    return assets


def ambient_spec(conversation_id, speaker, display_name, lines, completion_flag="", thought=False):
    return {
        "id": conversation_id,
        "participants": [hbct.ambient_participant(speaker, display_name)],
        "lines": [hbct.ambient_line(speaker, value, duration, thought=thought)
                  for value, duration in lines],
        "effects": [effect(completion_flag)] if completion_flag else [],
    }


def create_ambient_assets():
    specs = {
        "DA_Ambient_HBCT_SelfMonitoring_Breakfast": ambient_spec(
            "HBCT_SelfMonitoring_Breakfast", "Chai", "Uncle Chai",
            [("I remembered the vegetables today.", 2.8),
             ("Then let's record it.", 2.5)],
            "VHV.Story.Flag.HBCT.SelfMonitoring.SMBreakfastSceneSeen"),
        "DA_Ambient_HBCT_SelfMonitoring_AfternoonDrink": {
            "id": "HBCT_SelfMonitoring_AfternoonDrink",
            "participants": [hbct.ambient_participant("Friend", "Friend"),
                             hbct.ambient_participant("Chai", "Uncle Chai")],
            "lines": [hbct.ambient_line("Friend", "Want one?", 2.0),
                      hbct.ambient_line("Chai", "All right. Just one.", 2.5)],
            "effects": [effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMAfternoonSceneSeen")],
        },
        "DA_Ambient_HBCT_SelfMonitoring_EveningWalk": ambient_spec(
            "HBCT_SelfMonitoring_EveningWalk", "Chai", "Uncle Chai",
            [("I usually think I walk longer than this.", 2.7),
             ("That's another reason to keep a record.", 2.7),
             ("So the record keeps me honest?", 2.5),
             ("It helps you see what actually happened.", 2.8)],
            "VHV.Story.Flag.HBCT.SelfMonitoring.SMWalkRouteComplete"),
        "DA_Ambient_HBCT_SelfMonitoring_AdjustedWalk": ambient_spec(
            "HBCT_SelfMonitoring_AdjustedWalk", "Chai", "Uncle Chai",
            [("Normally I'd say I was too busy and skip it.", 1.8),
             ("The record showed us exactly when that happens.", 1.8),
             ("So instead of giving up on the plan, we changed it.", 1.8),
             ("Exactly.", 1.4)]),
        "DA_Ambient_HBCT_SelfMonitoring_SelfMonitorPayoff": {
            "id": "HBCT_SelfMonitoring_SelfMonitorPayoff",
            "participants": [hbct.ambient_participant("Chai", "Uncle Chai"),
                             hbct.ambient_participant("Instructor", "Instructor")],
            "lines": [hbct.ambient_line("Chai", "Walk finished. I'll mark it now.", 2.8),
                      hbct.ambient_line("Instructor", "That's the goal.", 2.2),
                      hbct.ambient_line("Instructor", "We helped him build the system.", 2.8),
                      hbct.ambient_line("Instructor", "Now he's using it himself.", 2.8)],
            "effects": [effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMChaiSelfMonitoring")],
        },
    }

    observations = [
        ("TargetShoes", "Walking is something Chai actually does.", "SMTargetShoesObserved"),
        ("TargetVegetables", "This is also a behaviour he can repeat and record.", "SMTargetVegetablesObserved"),
        ("TargetSweetDrink", "This is another behaviour that can be observed.", "SMTargetSweetDrinkObserved"),
        ("TargetGlucose", "This is an outcome, not the behaviour itself.", "SMTargetGlucoseObserved"),
        ("TargetScale", "Weight is an outcome.", "SMTargetScaleObserved"),
        ("TargetCalendar", "This could record whether a behaviour happened.", "SMTargetCalendarObserved"),
        ("TargetNotebook", "This could record details.", "SMTargetNotebookObserved"),
        ("PatternBusyEvidence", "Wednesday and Friday are Chai's busiest market days.", "SMPatternBusyEvidenceInspected"),
        ("PatternSweetEvidence", "The sweet-drink marks line up with visits and social stops.", "SMPatternSweetEvidenceInspected"),
        ("PatternWalkingEvidence", "Five walking days are marked against a five-day goal.", "SMPatternWalkingEvidenceInspected"),
    ]
    for suffix, line_text, flag_leaf in observations:
        specs["DA_Ambient_HBCT_SelfMonitoring_" + suffix] = ambient_spec(
            "HBCT_SelfMonitoring_" + suffix, "Chai", "Uncle Chai",
            [(line_text, 2.6)], "VHV.Story.Flag.HBCT.SelfMonitoring." + flag_leaf, True)

    methods = [
        ("MethodNotebook", ["Detailed. Useful for writing meals and exercise. Requires regular writing.",
                            "I could write everything down...", "But I know myself. I might stop after a few days."],
         "SMMethodNotebookInspected"),
        ("MethodCalendar", ["Fast, visible, and easy to mark each day.", "That one I'd see every morning."],
         "SMMethodCalendarInspected"),
        ("MethodPhone", ["Useful for reminders and quick records.", "I mostly use my phone for calls."],
         "SMMethodPhoneInspected"),
    ]
    for suffix, lines, flag_leaf in methods:
        specs["DA_Ambient_HBCT_SelfMonitoring_" + suffix] = ambient_spec(
            "HBCT_SelfMonitoring_" + suffix, "Chai", "Uncle Chai",
            [(value, 2.7) for value in lines],
            "VHV.Story.Flag.HBCT.SelfMonitoring." + flag_leaf)

    records = [
        ("PrepareTracker", "The weekly wall calendar is ready.", "SMTrackerPrepared"),
        ("RecordBreakfast", "Vegetables marked. Sweet drink: none this morning.", "SMBreakfastRecorded"),
        ("RecordAfternoon", "Sweet drink marked honestly.", "SMSweetDrinkRecorded"),
        ("RecordWalk", "Thirty-minute walk marked complete.", "SMWalkRecorded"),
    ]
    for suffix, line_text, flag_leaf in records:
        specs["DA_Ambient_HBCT_SelfMonitoring_" + suffix] = ambient_spec(
            "HBCT_SelfMonitoring_" + suffix, "Chai", "Uncle Chai",
            [(line_text, 2.5)], "VHV.Story.Flag.HBCT.SelfMonitoring." + flag_leaf, True)

    assets = {}
    for name, data in specs.items():
        asset = hbct.load_or_create(AMBIENT_DIR, name, unreal.VHVAmbientConversationData)
        asset.set_editor_properties({
            "conversation_id": data["id"],
            "participants": data["participants"],
            "lines": data["lines"],
            "show_speaker_name": False,
            "completion_effects": data["effects"],
        })
        assets[name] = asset
    return assets


def configure_arc(arc, conversations):
    q = unreal.VHVQuestObjectiveType
    start_action = tag("VHV.WorldAction.StartConversation")
    target_flags = [
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMTargetShoesObserved",
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMTargetVegetablesObserved",
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMTargetSweetDrinkObserved",
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMTargetGlucoseObserved",
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMTargetScaleObserved",
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMTargetCalendarObserved",
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMTargetNotebookObserved",
    ]
    method_flags = [
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMMethodNotebookInspected",
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMMethodCalendarInspected",
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMMethodPhoneInspected",
    ]
    pattern_evidence = [
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMPatternBusyEvidenceInspected",
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMPatternSweetEvidenceInspected",
        "VHV.Story.Flag.HBCT.SelfMonitoring.SMPatternWalkingEvidenceInspected",
    ]
    quest5 = struct(
        unreal.VHVQuestDefinition,
        quest_id=QUEST_ID,
        quest_title="TECHNIQUE 5: SELF-MONITORING",
        quest_description="WHAT REALLY HAPPENED? Observe, record, compare, and adjust Uncle Chai's plan.",
        category=unreal.VHVQuestCategory.MAIN_STORY,
        auto_track=True,
        auto_start_next_quest=False,
        completion_stinger=hbct.major_stinger(
            "SELF-MONITORING - COMPLETE", "OBSERVE - RECORD - COMPARE - ADJUST",
            "PEOPLE CANNOT IMPROVE A PATTERN THEY CANNOT SEE", 2.6),
        objectives=[
            objective("O01_CheckInWithChai", "Visit Uncle Chai and find out what is going wrong", q.CONVERSATION,
                      participant_tag=tag("VHV.Participant.UncleChai"),
                      location_tag=tag("VHV.Location.HBCT.SelfMonitoring.SMChaiHome"),
                      ensure_participant_present_at_location=True,
                      required_participant_spawn_class=unreal.load_asset(NPC_BP_PATH).generated_class(),
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_ChaiCheckIn"],
                      entry_node_id="SMCheck_01", auto_start=False),
            objective("O02_LearnWhyRecordsMatter", "Talk with the Instructor", q.CONVERSATION,
                      participant_tag=tag("VHV.Participant.Instructor"),
                      ensure_participant_present_at_location=True,
                      activation_npc_readiness=[
                          npc_move("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMInstructorLesson"),
                          npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMInstructorLessonChai"),
                      ],
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_InstructorLesson"],
                      entry_node_id="SMLesson_01", auto_start=False),
            objective("O03_IdentifyTargetBehavior", "Find what Uncle Chai should actually monitor", q.CONVERSATION,
                      ensure_participant_present_at_location=True,
                      activation_npc_readiness=[npc_move(
                          "VHV.Participant.UncleChai",
                          "VHV.Location.HBCT.SelfMonitoring.SMTargetBehaviorArea")],
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_TargetBehavior"],
                      entry_node_id="SMTarget_Activity1", auto_start=True,
                      activation_conditions=all_flags(*target_flags)),
            objective("O04_BuildMonitoringProcess", "Put the self-monitoring process in order", q.CONVERSATION,
                      ensure_participant_present_at_location=True,
                      activation_npc_readiness=[npc_move(
                          "VHV.Participant.Instructor",
                          "VHV.Location.HBCT.SelfMonitoring.SMMonitoringProcess")],
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_MonitoringProcess"],
                      entry_node_id="SMProcess_Activity", auto_start=False),
            objective("O05_ChooseMonitoringMethod", "Choose a monitoring method that Uncle Chai will actually use", q.CONVERSATION,
                      ensure_participant_present_at_location=True,
                      activation_npc_readiness=[npc_move(
                          "VHV.Participant.UncleChai",
                          "VHV.Location.HBCT.SelfMonitoring.SMMethodChai")],
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_ChooseMethod"],
                      entry_node_id="SMMethod_Activity1", auto_start=True,
                      activation_conditions=all_flags(*method_flags)),
            objective("O06_PrepareChaiTracker", "Prepare Uncle Chai's weekly tracker", q.CUSTOM_EVENT,
                      custom_event_tag=tag("VHV.CustomEvent.HBCT.SelfMonitoring.TrackerPrepared"),
                      ensure_participant_present_at_location=True,
                      activation_npc_readiness=[npc_move(
                          "VHV.Participant.UncleChai",
                          "VHV.Location.HBCT.SelfMonitoring.SMTrackerChai")],
                      completion_conditions=all_flags("VHV.Story.Flag.HBCT.SelfMonitoring.SMTrackerPrepared")),
            objective("O07_RecordBreakfast", "Record Uncle Chai's breakfast", q.CUSTOM_EVENT,
                      custom_event_tag=tag("VHV.CustomEvent.HBCT.SelfMonitoring.BreakfastRecorded"),
                      ensure_participant_present_at_location=True,
                      activation_npc_readiness=[npc_move(
                          "VHV.Participant.UncleChai",
                          "VHV.Location.HBCT.SelfMonitoring.SMBreakfast")],
                      completion_conditions=all_flags("VHV.Story.Flag.HBCT.SelfMonitoring.SMBreakfastRecorded")),
            objective("O08_RecordAfternoon", "Observe and record what happens this afternoon", q.CUSTOM_EVENT,
                      custom_event_tag=tag("VHV.CustomEvent.HBCT.SelfMonitoring.AfternoonRecorded"),
                      ensure_participant_present_at_location=True,
                      activation_npc_readiness=[
                          npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMAfternoon"),
                          npc_move("VHV.Participant.SelfMonitoringFriend", "VHV.Location.HBCT.SelfMonitoring.SMAfternoonFriend"),
                      ],
                      completion_conditions=all_flags("VHV.Story.Flag.HBCT.SelfMonitoring.SMSweetDrinkRecorded")),
            objective("O09_RecordEveningWalk", "Walk with Uncle Chai and record today's exercise", q.CUSTOM_EVENT,
                      custom_event_tag=tag("VHV.CustomEvent.HBCT.SelfMonitoring.WalkRecorded"),
                      completion_conditions=all_flags("VHV.Story.Flag.HBCT.SelfMonitoring.SMWalkRecorded"),
                      npc_travel_mode=unreal.VHVQuestNPCTravelMode.FREE_ROAM,
                      npc_move_stages=[
                          move_stage([npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMChaiWalkMid", True, 180.0)],
                                     "VHV.WorldReceiver.HBCT.SelfMonitoring.EveningWalk"),
                          move_stage([npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMChaiWalkEnd", True, 180.0)]),
                      ], complete_after_npc_move_stages=False),
            objective("O10_ReviewFirstDay", "Review today's record with Uncle Chai", q.CONVERSATION,
                      participant_tag=tag("VHV.Participant.UncleChai"),
                      ensure_participant_present_at_location=True,
                      activation_npc_readiness=[
                          npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMFirstDayReview"),
                          npc_move("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMFirstDayInstructor"),
                      ],
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_FirstDayReview"],
                      entry_node_id="SMReview_01", auto_start=False),
            objective("O11_OneWeekLater", "Review Uncle Chai's first week", q.CONVERSATION,
                      activation_stinger=hbct.major_stinger("", "ONE WEEK LATER", "", 1.8),
                      participant_tag=tag("VHV.Participant.UncleChai"),
                      ensure_participant_present_at_location=True,
                      activation_npc_readiness=[
                          npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMOneWeekLater"),
                          npc_move("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMOneWeekInstructor"),
                      ],
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_OneWeekLater"],
                      entry_node_id="SMWeek_01", auto_start=False),
            objective("O12_FindBehaviorPatterns", "Study Uncle Chai's week and find the patterns", q.CONVERSATION,
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_PatternInvestigation"],
                      entry_node_id="SMPattern_Activity1", auto_start=True,
                      activation_conditions=all_flags(*pattern_evidence)),
            objective("O13_AdjustThePlan", "Use the record to improve Uncle Chai's plan", q.CONVERSATION,
                      participant_tag=tag("VHV.Participant.Instructor"),
                      location_tag=tag("VHV.Location.HBCT.SelfMonitoring.SMAdjustment"),
                      ensure_participant_present_at_location=True,
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_Adjustment"],
                      entry_node_id="SMAdjust_Activity", auto_start=False),
            objective("O14_PracticeAdjustedRoutine", "Help Uncle Chai adapt his walking plan on a busy day", q.CUSTOM_EVENT,
                      custom_event_tag=tag("VHV.CustomEvent.HBCT.SelfMonitoring.AdjustedRoutineComplete"),
                      npc_travel_mode=unreal.VHVQuestNPCTravelMode.FREE_ROAM,
                      npc_move_stages=[
                          move_stage([npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMAdjustedWalkMid", True, 180.0)],
                                     "VHV.WorldReceiver.HBCT.SelfMonitoring.AdjustedWalk"),
                          move_stage([npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMAdjustedWalkEnd", True, 180.0)]),
                      ], complete_after_npc_move_stages=True,
                      completion_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.SMAdjustedRoutinePracticed")]),
            objective("O15_BuildProgressGraph", "Turn Uncle Chai's records into a progress graph", q.CONVERSATION,
                      ensure_participant_present_at_location=True,
                      activation_npc_readiness=[
                          npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMProgressGraphChai"),
                          npc_move("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMProgressGraphInstructor"),
                      ],
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_ProgressGraph"],
                      entry_node_id="SMGraph_Activity", auto_start=False),
            objective("O16_SetSelfReminder", "Help Uncle Chai remember to record his behaviour", q.CONVERSATION,
                      participant_tag=tag("VHV.Participant.UncleChai"),
                      location_tag=tag("VHV.Location.HBCT.SelfMonitoring.SMSelfReminder"),
                      ensure_participant_present_at_location=True,
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_SelfReminder"],
                      entry_node_id="SMReminder_Activity", auto_start=False),
            objective("O17_WatchChaiSelfMonitor", "See whether Uncle Chai can use the system himself", q.WORLD_ACTION,
                      world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.SelfMonitoring.SelfMonitorPayoff"),
                      world_action_tag=tag("VHV.WorldAction.StartConversation"),
                      world_action_start_policy=unreal.VHVWorldActionStartPolicy.IMMEDIATE,
                      npc_travel_mode=unreal.VHVQuestNPCTravelMode.FREE_ROAM,
                      npc_move_stages=[move_stage([
                          npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMSelfMonitorTracker", True, 180.0)])],
                      complete_after_npc_move_stages=False),
            objective("O18_FinalDebrief", "Reflect with the Instructor", q.CONVERSATION,
                      participant_tag=tag("VHV.Participant.Instructor"),
                      ensure_participant_present_at_location=True,
                      activation_npc_readiness=[
                          npc_move("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMFinalDebrief"),
                          npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMFinalChai"),
                      ],
                      conversation=conversations["DA_Conversation_HBCT_SelfMonitoring_FinalDebrief"],
                      entry_node_id="SMFinal_01", auto_start=False,
                      completion_effects=[effect("VHV.Story.Flag.HBCT.SelfMonitoring.Completed")]),
        ])

    quest5_objectives = list(quest5.get_editor_property("objectives"))
    objective_tracking.apply_tracking_metadata_to_objectives(QUEST_ID, quest5_objectives)
    quest5.set_editor_property("objectives", quest5_objectives)

    quests = list(arc.get_editor_property("quests"))
    ids = [str(item.get_editor_property("quest_id")) for item in quests]
    expected = ["Q_HBCT_00_INTRO", "Q_HBCT_01_MOTIVATION", "Q_HBCT_02_GOAL_SETTING",
                "Q_HBCT_03_ROLE_MODEL", "Q_HBCT_04_SOCIAL_SUPPORT"]
    if ids[:5] != expected:
        raise RuntimeError("Unexpected Quest 0-4 ordering; refusing to author Quest 5")
    quests = [item for item in quests if str(item.get_editor_property("quest_id")) != QUEST_ID]
    quest4 = next(item for item in quests
                  if str(item.get_editor_property("quest_id")) == "Q_HBCT_04_SOCIAL_SUPPORT")
    quest4.set_editor_property("auto_start_next_quest", True)
    quests.append(quest5)
    arc.set_editor_property("quests", quests)
    arc.set_editor_property(
        "quest_arc_description",
        "Production HBCT story arc containing the prologue and Techniques 1-5.")


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find_actor(label):
    return next((actor for actor in actors().get_all_level_actors()
                 if actor.get_actor_label() == label), None)


def folder(actor, path):
    actor.set_folder_path(path)
    return actor


def destroy_managed_actors():
    for actor in list(actors().get_all_level_actors()):
        if actor.get_actor_label().startswith(PREFIX):
            actors().destroy_actor(actor)


def spawn_actor(actor_class, label, location, yaw=0.0, folder_path=""):
    actor = actors().spawn_actor_from_class(
        actor_class, unreal.Vector(*location), unreal.Rotator(0.0, yaw, 0.0))
    if not actor:
        raise RuntimeError("Could not spawn {}".format(label))
    actor.set_actor_label(label)
    actor.set_actor_location(unreal.Vector(*location), False, True)
    actor.set_actor_rotation(unreal.Rotator(0.0, yaw, 0.0), False)
    folder(actor, folder_path)
    return actor


def participant_id(actor):
    if not actor or not hasattr(actor, "get_quest_participant_component"):
        return ""
    component = actor.get_quest_participant_component()
    return str(component.get_effective_participant_id()) if component else ""


def find_participant(participant_leaf):
    matches = [actor for actor in actors().get_all_level_actors()
               if participant_id(actor) == participant_leaf]
    if len(matches) > 1:
        raise RuntimeError("Duplicate participant {}: {}".format(
            participant_leaf, [actor.get_actor_label() for actor in matches]))
    return matches[0] if matches else None


def configure_participant(actor, participant_tag, display_name):
    participant = actor.get_quest_participant_component()
    participant.modify()
    participant.set_editor_properties({
        "quest_participation_enabled": True,
        "participant_tag": tag(participant_tag),
    })
    interaction = actor.get_npc_interaction_component()
    interaction.modify()
    interaction.set_editor_properties({
        "default_interaction_prompt": "Talk",
        "use_quest_objective_text_as_prompt": False,
        "interaction_enabled": True,
    })
    hbct.configure_npc_nameplate(actor, display_name, True)
    return actor


def spawn_npc(npc_class, label, location, participant_tag, display_name, folder_path):
    actor = spawn_actor(npc_class, label, location, 180.0, folder_path)
    configure_participant(actor, participant_tag, display_name)
    return actor


def spawn_location(label, location, location_tag, yaw=0.0):
    actor = spawn_actor(unreal.VHVQuestLocationVolume, label, location, yaw,
                        "06_SelfMonitoring/Locations")
    actor.set_editor_properties({
        "location_tag": tag(location_tag), "enabled": False,
        "trigger_world_action": False, "activate_current_objective": False,
        "required_active_quest_id": "", "required_active_objective_id": "",
    })
    actor.get_editor_property("box_component").set_box_extent(
        unreal.Vector(50.0, 50.0, 110.0), True)
    return actor


def spawn_trigger(label, location, location_tag, objective_id,
                  extent=(280.0, 280.0, 180.0), receiver_tag="",
                  activate_objective=False, trigger_conditions=None,
                  track_as_objective=False, trigger_once=True,
                  require_interaction=False, interaction_prompt="Interact"):
    actor = spawn_actor(unreal.VHVQuestLocationVolume, label, location, 0.0,
                        "06_SelfMonitoring/Triggers")
    values = {
        "location_tag": tag(location_tag), "enabled": True,
        "trigger_world_action": bool(receiver_tag),
        "trigger_world_action_once": trigger_once,
        "track_world_action_as_objective": track_as_objective,
        "activate_current_objective": activate_objective,
        "activate_current_objective_once": True,
        "required_active_quest_id": QUEST_ID,
        "required_active_objective_id": objective_id,
        "trigger_conditions": trigger_conditions or unreal.VHVStoryConditionSet(),
        "require_player_interaction": require_interaction,
        "interaction_prompt": interaction_prompt,
    }
    if receiver_tag:
        values["world_action_receiver_tag"] = tag(receiver_tag)
        values["world_action_tag"] = tag("VHV.WorldAction.StartConversation")
    actor.set_editor_properties(values)
    actor.get_editor_property("box_component").set_box_extent(unreal.Vector(*extent), True)
    return actor


def bind(slot, npc):
    return struct(unreal.VHVAmbientParticipantBinding, slot_id=slot, npc=npc)


def spawn_ambient(label, location, asset, receiver_tag, bindings, folder_path,
                  preserve_behavior=False):
    actor = spawn_actor(unreal.VHVAmbientConversationActor, label, location, 0.0, folder_path)
    actor.set_editor_properties({
        "conversation_data": asset, "participants": bindings,
        "auto_start_on_begin_play": False, "requires_explicit_trigger": True,
        "play_once": True, "preserve_participant_behavior": preserve_behavior,
        "player_leave_policy": (unreal.VHVAmbientConversationLeavePolicy.CONTINUE
                                if preserve_behavior else unreal.VHVAmbientConversationLeavePolicy.CANCEL),
        "observation_radius": 2400.0,
    })
    receiver = actor.get_editor_property("world_action_receiver")
    receiver.modify()
    receiver.set_editor_properties({"world_actions_enabled": True, "receiver_tag": tag(receiver_tag)})
    return actor


def blockout_prop(label, location, scale, folder_path="06_SelfMonitoring/Props"):
    actor = spawn_actor(unreal.StaticMeshActor, label, location, 0.0, folder_path)
    component = actor.get_editor_property("static_mesh_component")
    component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def text_display(label, location, text, facing_yaw=180.0, size=24.0,
                 folder_path="06_SelfMonitoring/Props"):
    """Create upright text whose +X front is turned toward the expected viewer.

    Unreal Python's Rotator constructor is ordered Roll, Pitch, Yaw, unlike the
    C++ FRotator constructor. Use named fields here so a facing yaw can never be
    authored accidentally as a 180-degree pitch.
    """
    actor = spawn_actor(unreal.TextRenderActor, label, location, 0.0, folder_path)
    actor.set_actor_rotation(
        unreal.Rotator(roll=0.0, pitch=0.0, yaw=facing_yaw), False)
    component = actor.get_component_by_class(unreal.TextRenderComponent)
    component.set_editor_properties({
        "text": text, "world_size": size,
        "text_render_color": unreal.Color(245, 238, 210, 255),
        "horizontal_alignment": unreal.HorizTextAligment.EHTA_CENTER,
        "vertical_alignment": unreal.VerticalTextAligment.EVRTA_TEXT_BOTTOM,
    })
    return actor


def label_display(label, location, heading, description, folder_path):
    """Create a centered two-level prop label without mixed-size line hacks."""
    x, y, z = location
    text_display(label, (x, y, z + 20.0), heading, size=15.0,
                 folder_path=folder_path)
    text_display(label + "_Description", (x, y, z), description, size=9.0,
                 folder_path=folder_path)


def destroy_quest5_text_actors():
    for actor in list(actors().get_all_level_actors()):
        if (actor.get_actor_label().startswith(PREFIX)
                and actor.get_component_by_class(unreal.TextRenderComponent)):
            actors().destroy_actor(actor)


def create_quest5_world_text():
    """Author every Quest 5 world label with one consistent sign orientation."""
    text_display("HBCT_SM_Display_BlankCalendar", (300, 895, 180),
                 "WEEKLY CALENDAR\n-  -  -  -  -  -  -", size=18.0,
                 folder_path="06_SelfMonitoring/01_CheckIn")

    target_displays = [
        ("WalkingShoes", (2050, 700, 125), "WALKING SHOES", "An observable behaviour"),
        ("Vegetables", (2200, 700, 125), "VEGETABLES", "A repeatable behaviour"),
        ("SweetDrink", (2350, 700, 125), "SWEET DRINK", "An observable choice"),
        ("GlucoseRecord", (2050, 1100, 125), "BLOOD GLUCOSE", "An outcome"),
        ("Scale", (2200, 1100, 125), "BODY WEIGHT", "An outcome"),
        ("Calendar", (2350, 1100, 125), "CALENDAR", "Records whether it happened"),
        ("Notebook", (2500, 900, 125), "NOTEBOOK", "Records details"),
    ]
    for suffix, location, heading, description in target_displays:
        label_display("HBCT_SM_Label_" + suffix, location, heading, description,
                      "06_SelfMonitoring/03_TargetBehavior")

    method_displays = [
        ("Notebook", (4250, 700, 125), "NOTEBOOK", "Detailed - regular writing"),
        ("Calendar", (4400, 700, 125), "WALL CALENDAR", "Fast - visible - daily"),
        ("Phone", (4550, 700, 125), "PHONE REMINDER", "Quick - carried phone"),
    ]
    for suffix, location, heading, description in method_displays:
        label_display("HBCT_SM_MethodLabel_" + suffix, location, heading, description,
                      "06_SelfMonitoring/05_Method")

    tracker_text = "WALK 30 MINUTES\nGoal: 5 days per week\n\nVEGETABLES WITH MEALS\nGoal: Daily\n\nSWEET DRINK\nRecord honestly when it occurs"
    text_display("HBCT_SM_Display_PreparedTracker", (5400, 900, 130), tracker_text,
                 size=16.0, folder_path="06_SelfMonitoring/06_Tracker")
    text_display("HBCT_SM_Display_BreakfastRecord", (6550, 900, 130),
                 "TODAY\nWalking       [ ]\nVegetables    [X]\nSweet Drink   [ ]",
                 size=17.0, folder_path="06_SelfMonitoring/07_FirstDay")
    text_display("HBCT_SM_Display_AfternoonRecord", (7550, 820, 130),
                 "TODAY\nWalking       [ ]\nVegetables    [X]\nSweet Drink   [X]",
                 size=17.0, folder_path="06_SelfMonitoring/07_FirstDay")
    text_display("HBCT_SM_Display_CompleteDay", (10200, 900, 130),
                 "TODAY\nWalking       [X]\nVegetables    [X]\nSweet Drink   [X]",
                 size=17.0, folder_path="06_SelfMonitoring/08_Walk")
    text_display("HBCT_SM_Display_FirstDayReview", (11180, 900, 130),
                 "TODAY\nVegetables      [X]\n30-minute walk  [X]\nSweet drink     [X]",
                 size=17.0, folder_path="06_SelfMonitoring/07_FirstDay")

    weekly_text = ("ONE WEEK RECORD\n"
                   "MON  Walk X  Veg X  Sweet -\nTUE  Walk X  Veg X  Sweet -\n"
                   "WED  Walk -  Veg X  Sweet X\nTHU  Walk X  Veg X  Sweet -\n"
                   "FRI  Walk -  Veg -  Sweet X\nSAT  Walk X  Veg X  Sweet X\n"
                   "SUN  Walk X  Veg X  Sweet -\n\nWALKING GOAL: 5 / 5")
    text_display("HBCT_SM_Display_WeeklyCalendar", (12180, 900, 150), weekly_text,
                 size=14.0, folder_path="06_SelfMonitoring/09_WeekReview")
    text_display("HBCT_SM_Evidence_WeeklyCalendar", (13000, 750, 125), weekly_text,
                 size=12.0, folder_path="06_SelfMonitoring/10_Patterns")
    text_display("HBCT_SM_Evidence_MarketSchedule", (13200, 750, 125),
                 "MARKET SCHEDULE\nWEDNESDAY - BUSY\nFRIDAY - BUSY", size=15.0,
                 folder_path="06_SelfMonitoring/10_Patterns")
    text_display("HBCT_SM_Evidence_SweetDrinkVisits", (13400, 750, 125),
                 "SOCIAL VISITS\nWED - FRI - SAT\nSweet drinks marked", size=15.0,
                 folder_path="06_SelfMonitoring/10_Patterns")
    text_display("HBCT_SM_Evidence_WalkingGoal", (13200, 1100, 125),
                 "WALKING GOAL\n5 successful days\nGOAL: 5 / 5", size=16.0,
                 folder_path="06_SelfMonitoring/10_Patterns")
    text_display("HBCT_SM_Display_ProgressGraph", (17500, 900, 150),
                 "WALKING GOAL DAYS\nWeek 1  ###\nWeek 2  ####\nWeek 3  #####\nWeek 4  #####",
                 size=18.0, folder_path="06_SelfMonitoring/13_Graph")
    text_display("HBCT_SM_Display_ReminderCalendar", (18650, 980, 125),
                 "WALK TRACKER\nVISIBLE BESIDE SHOES", size=17.0,
                 folder_path="06_SelfMonitoring/14_Reminder")
    text_display("HBCT_SM_Display_SelfMonitorTrackerPending", (19700, 900, 125),
                 "TODAY'S WALK\nREADY FOR CHAI", size=18.0,
                 folder_path="06_SelfMonitoring/15_SelfMonitoringPayoff")
    text_display("HBCT_SM_Display_SelfMonitorTracker", (19700, 900, 125),
                 "TODAY'S WALK\nMARKED BY CHAI\nCOMPLETE", size=18.0,
                 folder_path="06_SelfMonitoring/15_SelfMonitoringPayoff")
    text_display("HBCT_SM_Display_Final", (20900, 900, 150),
                 "SELF-MONITORING\nOBSERVE\n|\nRECORD\n|\nSEE THE PATTERN\n|\nCOMPARE\n|\nADJUST",
                 size=17.0, folder_path="06_SelfMonitoring/16_Final")


def configure_map(ambient_assets):
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    destroy_managed_actors()
    npc_bp = unreal.load_asset(NPC_BP_PATH)
    if not npc_bp:
        raise RuntimeError("Missing {}".format(NPC_BP_PATH))
    npc_class = npc_bp.generated_class()

    instructor = find_participant("Instructor") or find_actor("HBCT_Intro_Instructor")
    chai = find_participant("UncleChai") or find_actor("HBCT_GoalSetting_UncleChai")
    if not instructor or not chai:
        raise RuntimeError("Persistent Instructor and Uncle Chai are required; refusing duplicates")
    configure_participant(instructor, "VHV.Participant.Instructor", "Instructor")
    configure_participant(chai, "VHV.Participant.UncleChai", "Uncle Chai")
    friend = spawn_npc(
        npc_class, "HBCT_SM_NPC_SelfMonitoringFriend", (7400, 1080, 100),
        "VHV.Participant.SelfMonitoringFriend", "Friend",
        "06_SelfMonitoring/07_FirstDay")

    # Environmental storytelling at the initial home scene.
    home_props = [
        ("HBCT_SM_Home_Notebook", (300, 760, 70), (0.7, 0.45, 0.08)),
        ("HBCT_SM_Home_Calendar", (300, 900, 105), (0.8, 0.08, 0.8)),
        ("HBCT_SM_Home_WalkingShoes", (300, 1040, 55), (0.55, 0.35, 0.18)),
        ("HBCT_SM_Home_Vegetables", (450, 760, 65), (0.45, 0.45, 0.22)),
        ("HBCT_SM_Home_SweetDrink", (450, 900, 75), (0.18, 0.18, 0.55)),
        ("HBCT_SM_Home_GlucoseRecord", (450, 1040, 95), (0.65, 0.06, 0.5)),
        ("HBCT_SM_Home_Scale", (600, 900, 55), (0.5, 0.5, 0.12)),
    ]
    for label, location, scale in home_props:
        blockout_prop(label, location, scale, "06_SelfMonitoring/01_CheckIn")

    # Readable world records and evidence; these replace any need for a custom activity UI.
    target_props = [
        ("WalkingShoes", (2050, 700, 80)),
        ("Vegetables", (2200, 700, 80)),
        ("SweetDrink", (2350, 700, 80)),
        ("GlucoseRecord", (2050, 1100, 80)),
        ("Scale", (2200, 1100, 80)),
        ("Calendar", (2350, 1100, 80)),
        ("Notebook", (2500, 900, 80)),
    ]
    for suffix, location in target_props:
        blockout_prop("HBCT_SM_Prop_" + suffix, location, (0.38, 0.38, 0.35),
                      "06_SelfMonitoring/03_TargetBehavior")

    method_props = [
        ("Notebook", (4250, 700, 80)),
        ("Calendar", (4400, 700, 80)),
        ("Phone", (4550, 700, 80)),
    ]
    for suffix, location in method_props:
        blockout_prop("HBCT_SM_Method_" + suffix, location, (0.38, 0.38, 0.35),
                      "06_SelfMonitoring/05_Method")
    blockout_prop("HBCT_SM_Prop_ReminderShoes", (18650, 820, 60), (0.55, 0.35, 0.18),
                  "06_SelfMonitoring/14_Reminder")
    create_quest5_world_text()

    # Core scene triggers.
    spawn_trigger("HBCT_SM_Trigger_ChaiCheckIn", (200, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMChaiHome", "O01_CheckInWithChai")
    spawn_trigger("HBCT_SM_Trigger_InstructorLesson", (1200, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMInstructorLesson", "O02_LearnWhyRecordsMatter")
    spawn_trigger("HBCT_SM_Trigger_TargetBehaviorArea", (2200, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMTargetBehaviorArea", "O03_IdentifyTargetBehavior")
    spawn_trigger("HBCT_SM_Trigger_MonitoringProcess", (3300, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMMonitoringProcess", "O04_BuildMonitoringProcess",
                  activate_objective=True)
    spawn_trigger("HBCT_SM_Trigger_MonitoringMethods", (4400, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMMethodStation", "O05_ChooseMonitoringMethod")
    spawn_trigger("HBCT_SM_Trigger_PrepareTracker", (5400, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMTracker", "O06_PrepareChaiTracker",
                  receiver_tag="VHV.WorldReceiver.HBCT.SelfMonitoring.PrepareTracker",
                  require_interaction=True, interaction_prompt="Prepare Tracker")
    spawn_trigger("HBCT_SM_Trigger_Breakfast", (6400, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMBreakfast", "O07_RecordBreakfast",
                  receiver_tag="VHV.WorldReceiver.HBCT.SelfMonitoring.Breakfast")
    spawn_trigger("HBCT_SM_Trigger_Afternoon", (7400, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMAfternoon", "O08_RecordAfternoon",
                  receiver_tag="VHV.WorldReceiver.HBCT.SelfMonitoring.AfternoonDrink")
    spawn_trigger("HBCT_SM_Trigger_WalkStart", (8400, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMChaiWalkStart", "O09_RecordEveningWalk")
    spawn_trigger("HBCT_SM_Trigger_FirstDayReview", (11000, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMFirstDayReview", "O10_ReviewFirstDay")
    spawn_trigger("HBCT_SM_Trigger_OneWeekLater", (12000, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMOneWeekLater", "O11_OneWeekLater")
    spawn_trigger("HBCT_SM_Trigger_PatternArea", (13200, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMPatternArea", "O12_FindBehaviorPatterns")
    spawn_trigger("HBCT_SM_Trigger_Adjustment", (14300, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMAdjustment", "O13_AdjustThePlan",
                  activate_objective=True)
    spawn_trigger("HBCT_SM_Trigger_AdjustedWalkStart", (15300, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMAdjustedWalkStart", "O14_PracticeAdjustedRoutine")
    spawn_trigger("HBCT_SM_Trigger_ProgressGraph", (17500, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMProgressGraph", "O15_BuildProgressGraph",
                  activate_objective=True)
    spawn_trigger("HBCT_SM_Trigger_SelfReminder", (18500, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMSelfReminder", "O16_SetSelfReminder",
                  activate_objective=True)
    spawn_trigger("HBCT_SM_Trigger_SelfMonitorPayoff", (19500, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMSelfMonitorPayoff", "O17_WatchChaiSelfMonitor")
    spawn_trigger("HBCT_SM_Trigger_FinalDebrief", (20700, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMFinalDebrief", "O18_FinalDebrief")

    # Small, distinct observation/record volumes around physical props.
    observation_triggers = [
        ("TargetShoes", (2050, 700, 100), "SMTargetShoesObserved", "O03_IdentifyTargetBehavior"),
        ("TargetVegetables", (2200, 700, 100), "SMTargetVegetablesObserved", "O03_IdentifyTargetBehavior"),
        ("TargetSweetDrink", (2350, 700, 100), "SMTargetSweetDrinkObserved", "O03_IdentifyTargetBehavior"),
        ("TargetGlucose", (2050, 1100, 100), "SMTargetGlucoseObserved", "O03_IdentifyTargetBehavior"),
        ("TargetScale", (2200, 1100, 100), "SMTargetScaleObserved", "O03_IdentifyTargetBehavior"),
        ("TargetCalendar", (2350, 1100, 100), "SMTargetCalendarObserved", "O03_IdentifyTargetBehavior"),
        ("TargetNotebook", (2500, 900, 100), "SMTargetNotebookObserved", "O03_IdentifyTargetBehavior"),
        ("MethodNotebook", (4250, 700, 100), "SMMethodNotebookInspected", "O05_ChooseMonitoringMethod"),
        ("MethodCalendar", (4400, 700, 100), "SMMethodCalendarInspected", "O05_ChooseMonitoringMethod"),
        ("MethodPhone", (4550, 700, 100), "SMMethodPhoneInspected", "O05_ChooseMonitoringMethod"),
        ("PatternBusyEvidence", (13100, 750, 100), "SMPatternBusyEvidenceInspected", "O12_FindBehaviorPatterns"),
        ("PatternSweetEvidence", (13400, 750, 100), "SMPatternSweetEvidenceInspected", "O12_FindBehaviorPatterns"),
        ("PatternWalkingEvidence", (13200, 1100, 100), "SMPatternWalkingEvidenceInspected", "O12_FindBehaviorPatterns"),
    ]
    observation_prompts = {
        "TargetShoes": "Inspect Walking Shoes",
        "TargetVegetables": "Inspect Vegetables",
        "TargetSweetDrink": "Inspect Sweet Drink",
        "TargetGlucose": "Inspect Blood Glucose Record",
        "TargetScale": "Inspect Body Weight Scale",
        "TargetCalendar": "Inspect Calendar",
        "TargetNotebook": "Inspect Notebook",
        "MethodNotebook": "Inspect Notebook",
        "MethodCalendar": "Inspect Wall Calendar",
        "MethodPhone": "Inspect Phone Reminder",
        "PatternBusyEvidence": "Inspect Busy-Day Evidence",
        "PatternSweetEvidence": "Inspect Sweet-Drink Evidence",
        "PatternWalkingEvidence": "Inspect Walking-Goal Evidence",
    }
    for suffix, location, flag_leaf, objective_id in observation_triggers:
        receiver = "VHV.WorldReceiver.HBCT.SelfMonitoring." + suffix
        spawn_trigger("HBCT_SM_Trigger_" + suffix, location,
                      "VHV.Location.HBCT.SelfMonitoring.SM" + suffix,
                      objective_id, extent=(60, 60, 140), receiver_tag=receiver,
                      trigger_conditions=flag_not_set(
                          "VHV.Story.Flag.HBCT.SelfMonitoring." + flag_leaf),
                      require_interaction=True,
                      interaction_prompt=observation_prompts[suffix])

    spawn_trigger("HBCT_SM_Trigger_RecordBreakfast", (6550, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMBreakfastTracker", "O07_RecordBreakfast",
                  extent=(80, 80, 140), receiver_tag="VHV.WorldReceiver.HBCT.SelfMonitoring.RecordBreakfast",
                  trigger_conditions=all_flags("VHV.Story.Flag.HBCT.SelfMonitoring.SMBreakfastSceneSeen"),
                  require_interaction=True, interaction_prompt="Record Breakfast")
    spawn_trigger("HBCT_SM_Trigger_RecordAfternoon", (7550, 820, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMAfternoonRecord", "O08_RecordAfternoon",
                  extent=(80, 80, 140), receiver_tag="VHV.WorldReceiver.HBCT.SelfMonitoring.RecordAfternoon",
                  trigger_conditions=all_flags("VHV.Story.Flag.HBCT.SelfMonitoring.SMAfternoonSceneSeen"),
                  require_interaction=True, interaction_prompt="Record Sweet Drink")
    spawn_trigger("HBCT_SM_Trigger_RecordWalk", (10200, 900, 100),
                  "VHV.Location.HBCT.SelfMonitoring.SMWalkRecord", "O09_RecordEveningWalk",
                  extent=(100, 100, 150), receiver_tag="VHV.WorldReceiver.HBCT.SelfMonitoring.RecordWalk",
                  trigger_conditions=all_flags("VHV.Story.Flag.HBCT.SelfMonitoring.SMWalkRouteComplete"),
                  require_interaction=True, interaction_prompt="Record Walk")

    # Semantic movement/staging targets not already represented by primary triggers.
    locations = [
        ("InstructorLessonChai", (1200, 1080, 100)),
        ("MethodChai", (4400, 1050, 100)), ("TrackerChai", (5400, 1080, 100)),
        ("AfternoonFriend", (7400, 1080, 100)),
        ("ChaiWalkMid", (9200, 900, 100)), ("ChaiWalkEnd", (10000, 900, 100)),
        ("FirstDayInstructor", (11000, 1080, 100)),
        ("OneWeekInstructor", (12000, 1080, 100)),
        ("AdjustedWalkMid", (15900, 900, 100)), ("AdjustedWalkEnd", (16500, 900, 100)),
        ("ProgressGraphChai", (17500, 1080, 100)),
        ("ProgressGraphInstructor", (17500, 720, 100)),
        ("SelfMonitorTracker", (19700, 900, 100)),
        ("SelfMonitorInstructor", (19500, 1100, 100)),
        ("FinalChai", (20700, 1100, 100)),
    ]
    for suffix, location in locations:
        spawn_location("HBCT_SM_Location_" + suffix, location,
                       "VHV.Location.HBCT.SelfMonitoring.SM" + suffix)

    # World-action receivers. Observation assets use Chai's nonblocking thought bubble.
    for suffix, location, _, objective_id in observation_triggers:
        asset = ambient_assets["DA_Ambient_HBCT_SelfMonitoring_" + suffix]
        spawn_ambient("HBCT_SM_Ambient_" + suffix, location, asset,
                      "VHV.WorldReceiver.HBCT.SelfMonitoring." + suffix,
                      [bind("Chai", chai)], "06_SelfMonitoring/03_TargetBehavior"
                      if objective_id == "O03_IdentifyTargetBehavior" else
                      ("06_SelfMonitoring/05_Method" if objective_id == "O05_ChooseMonitoringMethod"
                       else "06_SelfMonitoring/10_Patterns"))

    ambient_scenes = [
        ("Breakfast", (6400, 900, 100), [bind("Chai", chai)], "06_SelfMonitoring/07_FirstDay", False),
        ("AfternoonDrink", (7400, 900, 100), [bind("Friend", friend), bind("Chai", chai)], "06_SelfMonitoring/07_FirstDay", False),
        ("EveningWalk", (8400, 900, 100), [bind("Chai", chai)], "06_SelfMonitoring/08_Walk", True),
        ("AdjustedWalk", (15300, 900, 100), [bind("Chai", chai)], "06_SelfMonitoring/12_AdjustedWalk", True),
        ("SelfMonitorPayoff", (19500, 900, 100), [bind("Chai", chai), bind("Instructor", instructor)], "06_SelfMonitoring/15_SelfMonitoringPayoff", True),
        ("PrepareTracker", (5400, 900, 100), [bind("Chai", chai)], "06_SelfMonitoring/06_Tracker", False),
        ("RecordBreakfast", (6550, 900, 100), [bind("Chai", chai)], "06_SelfMonitoring/07_FirstDay", False),
        ("RecordAfternoon", (7550, 820, 100), [bind("Chai", chai)], "06_SelfMonitoring/07_FirstDay", False),
        ("RecordWalk", (10200, 900, 100), [bind("Chai", chai)], "06_SelfMonitoring/08_Walk", False),
    ]
    for suffix, location, bindings, scene_folder, preserve in ambient_scenes:
        ambient_actor = spawn_ambient(
            "HBCT_SM_Ambient_" + suffix, location,
            ambient_assets["DA_Ambient_HBCT_SelfMonitoring_" + suffix],
            "VHV.WorldReceiver.HBCT.SelfMonitoring." + suffix,
            bindings, scene_folder, preserve)
        if suffix == "SelfMonitorPayoff":
            ambient_actor.set_editor_properties({
                "requires_explicit_trigger": False,
                "actors_hidden_until_conversation_starts": [
                    find_actor("HBCT_SM_Display_SelfMonitorTracker")],
                "actors_hidden_when_conversation_starts": [
                    find_actor("HBCT_SM_Display_SelfMonitorTrackerPending")],
            })

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save {}".format(MAP_PATH))
    return {"instructor": instructor, "chai": chai, "friend": friend}


OBJECTIVE_IDS = [
    "O01_CheckInWithChai", "O02_LearnWhyRecordsMatter", "O03_IdentifyTargetBehavior",
    "O04_BuildMonitoringProcess", "O05_ChooseMonitoringMethod", "O06_PrepareChaiTracker",
    "O07_RecordBreakfast", "O08_RecordAfternoon", "O09_RecordEveningWalk",
    "O10_ReviewFirstDay", "O11_OneWeekLater", "O12_FindBehaviorPatterns",
    "O13_AdjustThePlan", "O14_PracticeAdjustedRoutine", "O15_BuildProgressGraph",
    "O16_SetSelfReminder", "O17_WatchChaiSelfMonitor", "O18_FinalDebrief",
]


def validate_authored_data(level, arc, conversations, ambient_assets):
    quests = list(arc.get_editor_property("quests"))
    ids = [str(item.get_editor_property("quest_id")) for item in quests]
    if ids[:6] != ["Q_HBCT_00_INTRO", "Q_HBCT_01_MOTIVATION", "Q_HBCT_02_GOAL_SETTING",
                   "Q_HBCT_03_ROLE_MODEL", "Q_HBCT_04_SOCIAL_SUPPORT", QUEST_ID]:
        raise RuntimeError("Quest sequence is not 0-5 in production order")
    quest4 = quests[4]
    if not quest4.get_editor_property("auto_start_next_quest"):
        raise RuntimeError("Quest 4 must link directly to Quest 5")
    quest5 = quests[5]
    objectives = list(quest5.get_editor_property("objectives"))
    if [str(item.get_editor_property("objective_id")) for item in objectives] != OBJECTIVE_IDS:
        raise RuntimeError("Quest 5 objective sequence is incorrect")

    day = level.get_editor_property("day_data")
    topic = next(item for item in day.get_editor_property("topics")
                 if str(item.get_editor_property("topic_id")) == TOPIC_ID)
    activities = list(topic.get_editor_property("activities"))
    activity_types = {
        str(unreal.GameplayTagLibrary.get_tag_name(item.get_editor_property("activity_tag"))):
            item.get_editor_property("activity_type") for item in activities
    }
    expected_types = {
        "VHV.Activity.HBCT.SelfMonitoring.MonitoringLoop": unreal.TextbookActivityType.OBSERVATION,
        "VHV.Activity.HBCT.SelfMonitoring.TargetWalkingBehavior": unreal.TextbookActivityType.SINGLE_CHOICE,
        "VHV.Activity.HBCT.SelfMonitoring.TargetFoodBehavior": unreal.TextbookActivityType.SINGLE_CHOICE,
        "VHV.Activity.HBCT.SelfMonitoring.ProcessOrdering": unreal.TextbookActivityType.ORDERING,
        "VHV.Activity.HBCT.SelfMonitoring.ChaiMethodChoice": unreal.TextbookActivityType.SINGLE_CHOICE,
        "VHV.Activity.HBCT.SelfMonitoring.MethodMatching": unreal.TextbookActivityType.MATCHING,
        "VHV.Activity.HBCT.SelfMonitoring.PatternBusyDays": unreal.TextbookActivityType.SINGLE_CHOICE,
        "VHV.Activity.HBCT.SelfMonitoring.PatternSweetDrinks": unreal.TextbookActivityType.SINGLE_CHOICE,
        "VHV.Activity.HBCT.SelfMonitoring.PatternWalkingGoal": unreal.TextbookActivityType.SINGLE_CHOICE,
        "VHV.Activity.HBCT.SelfMonitoring.PatternAdjustmentMatching": unreal.TextbookActivityType.MATCHING,
        "VHV.Activity.HBCT.SelfMonitoring.ProgressGraphOrdering": unreal.TextbookActivityType.ORDERING,
        "VHV.Activity.HBCT.SelfMonitoring.ReminderChoice": unreal.TextbookActivityType.SINGLE_CHOICE,
    }
    if activity_types != expected_types:
        raise RuntimeError("Quest 5 activity catalog/type mapping is incorrect")
    if len(conversations) != 12 or len(ambient_assets) != 22:
        raise RuntimeError("Quest 5 production asset count is incomplete")

    walk = objectives[8]
    adjusted = objectives[13]
    if len(walk.get_editor_property("npc_move_stages")) != 2:
        raise RuntimeError("O09 must have Mid and End movement stages")
    if len(adjusted.get_editor_property("npc_move_stages")) != 2:
        raise RuntimeError("O14 must have Mid and End movement stages")
    for objective_data in (walk, adjusted):
        for stage in objective_data.get_editor_property("npc_move_stages"):
            for move in stage.get_editor_property("moves"):
                if abs(float(move.get_editor_property("move_speed_override")) - 180.0) > 0.01:
                    raise RuntimeError("Quest 5 walking speed must be 180 cm/s")


def validate_map():
    all_actors = list(actors().get_all_level_actors())
    managed = [actor for actor in all_actors if actor.get_actor_label().startswith(PREFIX)]
    if not managed:
        raise RuntimeError("No Quest 5 map actors were authored")
    if any(not str(actor.get_folder_path()).startswith("06_SelfMonitoring") for actor in managed):
        raise RuntimeError("Quest 5 actors escaped the 06_SelfMonitoring hierarchy")
    for actor in managed:
        location = actor.get_actor_location()
        if location.x < 0 or location.x > 21200 or location.y < 500 or location.y > 1350:
            raise RuntimeError("{} is outside the Quest 5 staging strip: {}".format(
                actor.get_actor_label(), location))
        if actor.get_actor_label().startswith("HBCT_SM_NPC_") and abs(location.z - 100.0) > 1.0:
            raise RuntimeError("{} is not at the requested NPC floor root".format(actor.get_actor_label()))

    for participant in ("Instructor", "UncleChai", "SelfMonitoringFriend"):
        matches = [actor for actor in all_actors if participant_id(actor) == participant]
        if len(matches) != 1:
            raise RuntimeError("Expected one {}, found {}".format(participant, len(matches)))

    triggers = [actor for actor in managed if actor.get_actor_label().startswith("HBCT_SM_Trigger_")]
    for trigger in triggers:
        if str(trigger.get_editor_property("required_active_quest_id")) != QUEST_ID:
            raise RuntimeError("{} is not Quest-5-gated".format(trigger.get_actor_label()))
        if not str(trigger.get_editor_property("required_active_objective_id")):
            raise RuntimeError("{} has no objective gate".format(trigger.get_actor_label()))
    primary_centers = {
        "HBCT_SM_Trigger_ChaiCheckIn": (200, 900),
        "HBCT_SM_Trigger_InstructorLesson": (1200, 900),
        "HBCT_SM_Trigger_Breakfast": (6400, 900),
        "HBCT_SM_Trigger_Afternoon": (7400, 900),
        "HBCT_SM_Trigger_WalkStart": (8400, 900),
        "HBCT_SM_Trigger_FirstDayReview": (11000, 900),
        "HBCT_SM_Trigger_OneWeekLater": (12000, 900),
        "HBCT_SM_Trigger_AdjustedWalkStart": (15300, 900),
        "HBCT_SM_Trigger_FinalDebrief": (20700, 900),
    }
    for label, (x, y) in primary_centers.items():
        value = find_actor(label).get_actor_location()
        if abs(value.x - x) > 0.1 or abs(value.y - y) > 0.1:
            raise RuntimeError("{} primary center is incorrect".format(label))

    semantic_leaves = []
    for actor in managed:
        try:
            location_tag = str(unreal.GameplayTagLibrary.get_tag_name(
                actor.get_editor_property("location_tag")))
        except Exception:
            continue
        if location_tag:
            semantic_leaves.append(location_tag.rsplit(".", 1)[-1])
    duplicates = sorted({leaf for leaf in semantic_leaves if semantic_leaves.count(leaf) > 1})
    if duplicates:
        raise RuntimeError("Ambiguous Quest 5 semantic location leaves: {}".format(duplicates))

    for suffix in ("EveningWalk", "AdjustedWalk", "SelfMonitorPayoff"):
        actor = find_actor("HBCT_SM_Ambient_" + suffix)
        if not actor.get_editor_property("preserve_participant_behavior"):
            raise RuntimeError("{} must preserve NPC movement".format(actor.get_actor_label()))

    validate_world_text()


def validate_world_text():
    text_actors = [
        actor for actor in actors().get_all_level_actors()
        if (actor.get_actor_label().startswith(PREFIX)
            and actor.get_component_by_class(unreal.TextRenderComponent))
    ]
    if len(text_actors) != 36:
        raise RuntimeError("Expected 36 Quest 5 world-text actors, found {}".format(
            len(text_actors)))
    for actor in text_actors:
        rotation = actor.get_actor_rotation()
        if (abs(rotation.pitch) > 0.1 or abs(rotation.roll) > 0.1
                or abs(abs(rotation.yaw) - 180.0) > 0.1):
            raise RuntimeError("{} is not upright and approach-facing: {}".format(
                actor.get_actor_label(), rotation))
        if actor.get_actor_location().z < 120.0:
            raise RuntimeError("{} is too low for a vertical sign".format(
                actor.get_actor_label()))
        component = actor.get_component_by_class(unreal.TextRenderComponent)
        if component.get_editor_property("horizontal_alignment") != unreal.HorizTextAligment.EHTA_CENTER:
            raise RuntimeError("{} is not horizontally centered".format(
                actor.get_actor_label()))
        if component.get_editor_property("vertical_alignment") != unreal.VerticalTextAligment.EVRTA_TEXT_BOTTOM:
            raise RuntimeError("{} is not bottom-anchored above its prop".format(
                actor.get_actor_label()))


def refresh_quest5_world_text():
    """Refresh only HBCT_SM_ TextRender actors in the production village map."""
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    destroy_quest5_text_actors()
    create_quest5_world_text()
    validate_world_text()
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save {}".format(MAP_PATH))
    unreal.log("{} Refreshed 35 Quest 5 world-text actors only.".format(LOG))


PRESENCE_GUARANTEES = {
    "Q_HBCT_03_ROLE_MODEL": {
        "O03_ClassifyModels": [("VHV.Participant.Instructor", "VHV.Location.HBCT.ExercisePark")],
        "O06_InspectCandidates": [("VHV.Participant.AuntSaeng", "VHV.Location.HBCT.RoleModel.SaengCandidate")],
        "O11_PrepareDemonstration": [("VHV.Participant.Instructor", "VHV.Location.HBCT.ExercisePark")],
        "O12_ObserveSaeng": [
            ("VHV.Participant.AuntSaeng", "VHV.Location.HBCT.RoleModel.SaengDemo"),
            ("VHV.Participant.UncleSomchai", "VHV.Location.HBCT.RoleModel.SomchaiDemo"),
        ],
        "O13_SupportImitation": [("VHV.Participant.UncleSomchai", "VHV.Location.HBCT.RoleModel.SomchaiDemo")],
        "O16_FinalDebrief": [("VHV.Participant.Instructor", "VHV.Location.HBCT.ExercisePark")],
    },
    "Q_HBCT_04_SOCIAL_SUPPORT": {
        "O02_ReflectOnSupport": [("VHV.Participant.Instructor", "VHV.Location.HBCT.SocialSupport.SSInstructorCatchup")],
        "O04_MeetUncleChai": [("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSChaiHome")],
        "O05_FindChaiSupporters": [
            ("VHV.Participant.ChaiWife", "VHV.Location.HBCT.SocialSupport.SSChaiWifeRecruit"),
            ("VHV.Participant.ChaiDaughter", "VHV.Location.HBCT.SocialSupport.SSChaiDaughterRecruit"),
            ("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSChaiNeighborRecruit"),
            ("VHV.Participant.ChaiClinicNurse", "VHV.Location.HBCT.SocialSupport.SSChaiNurseRecruit"),
        ],
        "O07_WatchSupportInAction": [
            ("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSPlanInAction"),
            ("VHV.Participant.ChaiWife", "VHV.Location.HBCT.SocialSupport.SSPlanWife"),
            ("VHV.Participant.ChaiDaughter", "VHV.Location.HBCT.SocialSupport.SSPlanDaughter"),
            ("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSPlanNeighbor"),
        ],
        "O09_SupportDisruption": [
            ("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSDisruption"),
            ("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSNeighborDisruption"),
        ],
        "O11_OneMonthLater": [("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSOneMonthLater")],
        "O12_ProvideAppraisalSupport": [("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSOneMonthLater")],
        "O13_FinalDebrief": [
            ("VHV.Participant.Instructor", "VHV.Location.HBCT.SocialSupport.SSFinalDebrief"),
            ("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSFinalChai"),
            ("VHV.Participant.ChaiWife", "VHV.Location.HBCT.SocialSupport.SSFinalWife"),
            ("VHV.Participant.ChaiDaughter", "VHV.Location.HBCT.SocialSupport.SSFinalDaughter"),
            ("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSFinalNeighbor"),
        ],
    },
    "Q_HBCT_05_SELF_MONITORING": {
        "O01_CheckInWithChai": [("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMChaiHome")],
        "O02_LearnWhyRecordsMatter": [
            ("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMInstructorLesson"),
            ("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMInstructorLessonChai"),
        ],
        "O03_IdentifyTargetBehavior": [("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMTargetBehaviorArea")],
        "O04_BuildMonitoringProcess": [("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMMonitoringProcess")],
        "O05_ChooseMonitoringMethod": [("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMMethodChai")],
        "O06_PrepareChaiTracker": [("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMTrackerChai")],
        "O07_RecordBreakfast": [("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMBreakfast")],
        "O08_RecordAfternoon": [
            ("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMAfternoon"),
            ("VHV.Participant.SelfMonitoringFriend", "VHV.Location.HBCT.SelfMonitoring.SMAfternoonFriend"),
        ],
        "O10_ReviewFirstDay": [
            ("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMFirstDayReview"),
            ("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMFirstDayInstructor"),
        ],
        "O11_OneWeekLater": [
            ("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMOneWeekLater"),
            ("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMOneWeekInstructor"),
        ],
        "O13_AdjustThePlan": [("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMAdjustment")],
        "O15_BuildProgressGraph": [
            ("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMProgressGraphChai"),
            ("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMProgressGraphInstructor"),
        ],
        "O16_SetSelfReminder": [("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMSelfReminder")],
        "O18_FinalDebrief": [
            ("VHV.Participant.Instructor", "VHV.Location.HBCT.SelfMonitoring.SMFinalDebrief"),
            ("VHV.Participant.UncleChai", "VHV.Location.HBCT.SelfMonitoring.SMFinalChai"),
        ],
    },
}


def apply_presence_guarantees_only():
    """Persist only the stationary-objective presence metadata in the HBCT arc."""
    arc = unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    if not arc:
        raise RuntimeError("Existing HBCT quest arc is missing")
    arc.modify()
    quests = list(arc.get_editor_property("quests"))
    found = set()
    for quest in quests:
        quest_id = str(quest.get_editor_property("quest_id"))
        requirements_by_objective = PRESENCE_GUARANTEES.get(quest_id)
        if not requirements_by_objective:
            continue
        objectives = list(quest.get_editor_property("objectives"))
        for objective_data in objectives:
            objective_id = str(objective_data.get_editor_property("objective_id"))
            requirements = requirements_by_objective.get(objective_id)
            if not requirements:
                continue
            objective_data.set_editor_property("ensure_participant_present_at_location", True)
            if len(requirements) == 1:
                participant_name, location_name = requirements[0]
                objective_data.set_editor_property("participant_tag", tag(participant_name))
                objective_data.set_editor_property("location_tag", tag(location_name))
            else:
                objective_data.set_editor_property(
                    "activation_npc_readiness",
                    [npc_move(participant, location) for participant, location in requirements])
            found.add((quest_id, objective_id))
        quest.set_editor_property("objectives", objectives)
    expected = {
        (quest_id, objective_id)
        for quest_id, objectives in PRESENCE_GUARANTEES.items()
        for objective_id in objectives
    }
    if found != expected:
        raise RuntimeError("Presence guarantee objective mismatch: missing={}".format(
            sorted(expected - found)))

    excluded_walks = {
        ("Q_HBCT_03_ROLE_MODEL", "O14_PracticeTogether"),
        ("Q_HBCT_04_SOCIAL_SUPPORT", "O08_WalkWithChai"),
        ("Q_HBCT_04_SOCIAL_SUPPORT", "O10_FindBackupSupport"),
        ("Q_HBCT_05_SELF_MONITORING", "O09_RecordEveningWalk"),
        ("Q_HBCT_05_SELF_MONITORING", "O14_PracticeAdjustedRoutine"),
        ("Q_HBCT_05_SELF_MONITORING", "O17_WatchChaiSelfMonitor"),
    }
    for quest in quests:
        quest_id = str(quest.get_editor_property("quest_id"))
        for objective_data in quest.get_editor_property("objectives"):
            key = (quest_id, str(objective_data.get_editor_property("objective_id")))
            if (key in excluded_walks
                    and objective_data.get_editor_property("ensure_participant_present_at_location")):
                raise RuntimeError("Travel objective incorrectly enables presence guarantee: {}".format(key))
    arc.set_editor_property("quests", quests)
    hbct.save_asset(arc)
    unreal.log("{} Updated stationary participant-presence metadata only ({} objectives).".format(
        LOG, len(expected)))


def repair_o03_o18_progression_only():
    """Restore only missing Quest 5 O03-O18 triggers and semantic movement anchors."""
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))

    def volumes_for_location(location_leaf):
        return [actor for actor in actors().get_all_level_actors()
                if isinstance(actor, unreal.VHVQuestLocationVolume)
                and str(actor.get_effective_location_id()) == location_leaf]

    def upsert_trigger(label, location, location_leaf, objective_id,
                       extent=(280.0, 280.0, 180.0), receiver_tag="",
                       activate_objective=False, trigger_conditions=None,
                       require_interaction=False, interaction_prompt="Interact"):
        actor = find_actor(label)
        candidates = volumes_for_location(location_leaf)
        if actor and not isinstance(actor, unreal.VHVQuestLocationVolume):
            raise RuntimeError("{} exists with the wrong actor class".format(label))
        if not actor:
            if len(candidates) > 1:
                raise RuntimeError("{} resolves to multiple map volumes: {}".format(
                    location_leaf, [item.get_actor_label() for item in candidates]))
            actor = candidates[0] if candidates else None
        if not actor:
            return spawn_trigger(
                label, location,
                "VHV.Location.HBCT.SelfMonitoring." + location_leaf,
                objective_id, extent=extent, receiver_tag=receiver_tag,
                activate_objective=activate_objective,
                trigger_conditions=trigger_conditions,
                require_interaction=require_interaction,
                interaction_prompt=interaction_prompt)

        actor.modify()
        actor.set_actor_label(label)
        actor.set_actor_location(unreal.Vector(*location), False, True)
        actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
        actor.set_folder_path("06_SelfMonitoring/Triggers")
        values = {
            "location_tag": tag("VHV.Location.HBCT.SelfMonitoring." + location_leaf),
            "enabled": True,
            "trigger_world_action": bool(receiver_tag),
            "trigger_world_action_once": True,
            "track_world_action_as_objective": False,
            "activate_current_objective": activate_objective,
            "activate_current_objective_once": True,
            "required_active_quest_id": QUEST_ID,
            "required_active_objective_id": objective_id,
            "trigger_conditions": trigger_conditions or unreal.VHVStoryConditionSet(),
            "require_player_interaction": require_interaction,
            "interaction_prompt": interaction_prompt,
        }
        if receiver_tag:
            values["world_action_receiver_tag"] = tag(receiver_tag)
            values["world_action_tag"] = tag("VHV.WorldAction.StartConversation")
        actor.set_editor_properties(values)
        actor.get_editor_property("box_component").set_box_extent(
            unreal.Vector(*extent), True)
        return actor

    def upsert_location(suffix, location):
        location_leaf = "SM" + suffix
        label = "HBCT_SM_Location_" + suffix
        actor = find_actor(label)
        candidates = volumes_for_location(location_leaf)
        if actor and not isinstance(actor, unreal.VHVQuestLocationVolume):
            raise RuntimeError("{} exists with the wrong actor class".format(label))
        if not actor:
            if len(candidates) > 1:
                raise RuntimeError("{} resolves to multiple map volumes: {}".format(
                    location_leaf, [item.get_actor_label() for item in candidates]))
            actor = candidates[0] if candidates else None
        if not actor:
            return spawn_location(
                label, location,
                "VHV.Location.HBCT.SelfMonitoring." + location_leaf)
        actor.modify()
        actor.set_actor_label(label)
        actor.set_actor_location(unreal.Vector(*location), False, True)
        actor.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
        actor.set_folder_path("06_SelfMonitoring/Locations")
        actor.set_editor_properties({
            "location_tag": tag("VHV.Location.HBCT.SelfMonitoring." + location_leaf),
            "enabled": False,
            "trigger_world_action": False,
            "activate_current_objective": False,
            "required_active_quest_id": "",
            "required_active_objective_id": "",
            "require_player_interaction": False,
        })
        actor.get_editor_property("box_component").set_box_extent(
            unreal.Vector(50.0, 50.0, 110.0), True)
        return actor

    primary_triggers = [
        ("HBCT_SM_Trigger_TargetBehaviorArea", (2200, 900, 100), "SMTargetBehaviorArea", "O03_IdentifyTargetBehavior", False, ""),
        ("HBCT_SM_Trigger_MonitoringProcess", (3300, 900, 100), "SMMonitoringProcess", "O04_BuildMonitoringProcess", True, ""),
        ("HBCT_SM_Trigger_MonitoringMethods", (4400, 900, 100), "SMMethodStation", "O05_ChooseMonitoringMethod", False, ""),
        ("HBCT_SM_Trigger_PrepareTracker", (5400, 900, 100), "SMTracker", "O06_PrepareChaiTracker", False, "VHV.WorldReceiver.HBCT.SelfMonitoring.PrepareTracker"),
        ("HBCT_SM_Trigger_Breakfast", (6400, 900, 100), "SMBreakfast", "O07_RecordBreakfast", False, "VHV.WorldReceiver.HBCT.SelfMonitoring.Breakfast"),
        ("HBCT_SM_Trigger_Afternoon", (7400, 900, 100), "SMAfternoon", "O08_RecordAfternoon", False, "VHV.WorldReceiver.HBCT.SelfMonitoring.AfternoonDrink"),
        ("HBCT_SM_Trigger_WalkStart", (8400, 900, 100), "SMChaiWalkStart", "O09_RecordEveningWalk", False, ""),
        ("HBCT_SM_Trigger_FirstDayReview", (11000, 900, 100), "SMFirstDayReview", "O10_ReviewFirstDay", False, ""),
        ("HBCT_SM_Trigger_OneWeekLater", (12000, 900, 100), "SMOneWeekLater", "O11_OneWeekLater", False, ""),
        ("HBCT_SM_Trigger_PatternArea", (13200, 900, 100), "SMPatternArea", "O12_FindBehaviorPatterns", False, ""),
        ("HBCT_SM_Trigger_Adjustment", (14300, 900, 100), "SMAdjustment", "O13_AdjustThePlan", True, ""),
        ("HBCT_SM_Trigger_AdjustedWalkStart", (15300, 900, 100), "SMAdjustedWalkStart", "O14_PracticeAdjustedRoutine", False, ""),
        ("HBCT_SM_Trigger_ProgressGraph", (17500, 900, 100), "SMProgressGraph", "O15_BuildProgressGraph", True, ""),
        ("HBCT_SM_Trigger_SelfReminder", (18500, 900, 100), "SMSelfReminder", "O16_SetSelfReminder", True, ""),
        ("HBCT_SM_Trigger_SelfMonitorPayoff", (19500, 900, 100), "SMSelfMonitorPayoff", "O17_WatchChaiSelfMonitor", False, ""),
        ("HBCT_SM_Trigger_FinalDebrief", (20700, 900, 100), "SMFinalDebrief", "O18_FinalDebrief", False, ""),
    ]
    for label, location, location_leaf, objective_id, activate, receiver in primary_triggers:
        require_interaction = label == "HBCT_SM_Trigger_PrepareTracker"
        upsert_trigger(
            label, location, location_leaf, objective_id,
            receiver_tag=receiver, activate_objective=activate,
            require_interaction=require_interaction,
            interaction_prompt="Prepare Tracker" if require_interaction else "Interact")

    observation_triggers = [
        ("TargetShoes", (2050, 700, 100), "SMTargetShoesObserved", "O03_IdentifyTargetBehavior", "Inspect Walking Shoes"),
        ("TargetVegetables", (2200, 700, 100), "SMTargetVegetablesObserved", "O03_IdentifyTargetBehavior", "Inspect Vegetables"),
        ("TargetSweetDrink", (2350, 700, 100), "SMTargetSweetDrinkObserved", "O03_IdentifyTargetBehavior", "Inspect Sweet Drink"),
        ("TargetGlucose", (2050, 1100, 100), "SMTargetGlucoseObserved", "O03_IdentifyTargetBehavior", "Inspect Blood Glucose Record"),
        ("TargetScale", (2200, 1100, 100), "SMTargetScaleObserved", "O03_IdentifyTargetBehavior", "Inspect Body Weight Scale"),
        ("TargetCalendar", (2350, 1100, 100), "SMTargetCalendarObserved", "O03_IdentifyTargetBehavior", "Inspect Calendar"),
        ("TargetNotebook", (2500, 900, 100), "SMTargetNotebookObserved", "O03_IdentifyTargetBehavior", "Inspect Notebook"),
        ("MethodNotebook", (4250, 700, 100), "SMMethodNotebookInspected", "O05_ChooseMonitoringMethod", "Inspect Notebook"),
        ("MethodCalendar", (4400, 700, 100), "SMMethodCalendarInspected", "O05_ChooseMonitoringMethod", "Inspect Wall Calendar"),
        ("MethodPhone", (4550, 700, 100), "SMMethodPhoneInspected", "O05_ChooseMonitoringMethod", "Inspect Phone Reminder"),
        ("PatternBusyEvidence", (13100, 750, 100), "SMPatternBusyEvidenceInspected", "O12_FindBehaviorPatterns", "Inspect Busy-Day Evidence"),
        ("PatternSweetEvidence", (13400, 750, 100), "SMPatternSweetEvidenceInspected", "O12_FindBehaviorPatterns", "Inspect Sweet-Drink Evidence"),
        ("PatternWalkingEvidence", (13200, 1100, 100), "SMPatternWalkingEvidenceInspected", "O12_FindBehaviorPatterns", "Inspect Walking-Goal Evidence"),
    ]
    for suffix, location, flag_leaf, objective_id, prompt in observation_triggers:
        upsert_trigger(
            "HBCT_SM_Trigger_" + suffix, location, "SM" + suffix, objective_id,
            extent=(60, 60, 140),
            receiver_tag="VHV.WorldReceiver.HBCT.SelfMonitoring." + suffix,
            trigger_conditions=flag_not_set(
                "VHV.Story.Flag.HBCT.SelfMonitoring." + flag_leaf),
            require_interaction=True, interaction_prompt=prompt)

    record_triggers = [
        ("HBCT_SM_Trigger_RecordBreakfast", (6550, 900, 100), "SMBreakfastTracker", "O07_RecordBreakfast",
         (80, 80, 140), "RecordBreakfast", "SMBreakfastSceneSeen", "Record Breakfast"),
        ("HBCT_SM_Trigger_RecordAfternoon", (7550, 820, 100), "SMAfternoonRecord", "O08_RecordAfternoon",
         (80, 80, 140), "RecordAfternoon", "SMAfternoonSceneSeen", "Record Sweet Drink"),
        ("HBCT_SM_Trigger_RecordWalk", (10200, 900, 100), "SMWalkRecord", "O09_RecordEveningWalk",
         (100, 100, 150), "RecordWalk", "SMWalkRouteComplete", "Record Walk"),
    ]
    for label, location, location_leaf, objective_id, extent, receiver_leaf, gate_leaf, prompt in record_triggers:
        upsert_trigger(
            label, location, location_leaf, objective_id, extent=extent,
            receiver_tag="VHV.WorldReceiver.HBCT.SelfMonitoring." + receiver_leaf,
            trigger_conditions=all_flags(
                "VHV.Story.Flag.HBCT.SelfMonitoring." + gate_leaf),
            require_interaction=True, interaction_prompt=prompt)

    locations = [
        ("InstructorLessonChai", (1200, 1080, 100)),
        ("MethodChai", (4400, 1050, 100)), ("TrackerChai", (5400, 1080, 100)),
        ("AfternoonFriend", (7400, 1080, 100)),
        ("ChaiWalkMid", (9200, 900, 100)), ("ChaiWalkEnd", (10000, 900, 100)),
        ("FirstDayInstructor", (11000, 1080, 100)),
        ("OneWeekInstructor", (12000, 1080, 100)),
        ("AdjustedWalkMid", (15900, 900, 100)), ("AdjustedWalkEnd", (16500, 900, 100)),
        ("ProgressGraphChai", (17500, 1080, 100)),
        ("ProgressGraphInstructor", (17500, 720, 100)),
        ("SelfMonitorTracker", (19700, 900, 100)),
        ("SelfMonitorInstructor", (19500, 1100, 100)),
        ("FinalChai", (20700, 1100, 100)),
    ]
    for suffix, location in locations:
        upsert_location(suffix, location)

    required_ambient_labels = [
        "TargetShoes", "TargetVegetables", "TargetSweetDrink", "TargetGlucose",
        "TargetScale", "TargetCalendar", "TargetNotebook", "MethodNotebook",
        "MethodCalendar", "MethodPhone", "PatternBusyEvidence", "PatternSweetEvidence",
        "PatternWalkingEvidence", "PrepareTracker", "Breakfast", "AfternoonDrink",
        "EveningWalk", "AdjustedWalk", "SelfMonitorPayoff", "RecordBreakfast",
        "RecordAfternoon", "RecordWalk",
    ]
    missing_receivers = [suffix for suffix in required_ambient_labels
                         if not find_actor("HBCT_SM_Ambient_" + suffix)]
    if missing_receivers:
        raise RuntimeError("Missing existing Quest 5 world-action receivers: {}".format(missing_receivers))

    expected_locations = ([item[2] for item in primary_triggers]
                          + ["SM" + item[0] for item in observation_triggers]
                          + [item[2] for item in record_triggers]
                          + ["SM" + item[0] for item in locations])
    unresolved = []
    duplicated = []
    for location_leaf in expected_locations:
        matches = volumes_for_location(location_leaf)
        if not matches:
            unresolved.append(location_leaf)
        elif len(matches) > 1:
            duplicated.append((location_leaf, [item.get_actor_label() for item in matches]))
    if unresolved or duplicated:
        raise RuntimeError("Quest 5 location validation failed; missing={} duplicates={}".format(
            unresolved, duplicated))

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save {}".format(MAP_PATH))
    unreal.log("{} Restored O03-O18 progression triggers and semantic locations only.".format(LOG))


def repair_o17_only():
    """Repair only O17 automatic movement, post-arrival payoff, and O18 handoff prerequisites."""
    arc = unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    if not arc:
        raise RuntimeError("Existing HBCT quest arc is missing")
    arc.modify()
    quests = list(arc.get_editor_property("quests"))
    quest5 = next((quest for quest in quests
                   if str(quest.get_editor_property("quest_id")) == QUEST_ID), None)
    if not quest5:
        raise RuntimeError("{} is missing from the HBCT quest arc".format(QUEST_ID))
    objectives = list(quest5.get_editor_property("objectives"))
    o17 = next((item for item in objectives
                if str(item.get_editor_property("objective_id")) == "O17_WatchChaiSelfMonitor"), None)
    o18 = next((item for item in objectives
                if str(item.get_editor_property("objective_id")) == "O18_FinalDebrief"), None)
    if not o17 or not o18:
        raise RuntimeError("Quest 5 O17 or O18 is missing")

    old_stages = list(o17.get_editor_property("npc_move_stages"))
    old_destinations = []
    old_ambient_receivers = []
    for stage in old_stages:
        old_ambient_receivers.append(str(unreal.GameplayTagLibrary.get_tag_name(
            stage.get_editor_property("ambient_conversation_receiver_tag"))))
        for move in list(stage.get_editor_property("moves")):
            old_destinations.append(str(unreal.GameplayTagLibrary.get_tag_name(
                move.get_editor_property("destination_location_tag"))))
    unreal.log("{} O17 before repair: type={} stages={} destinations={} stage_ambient={} ensure_presence={}.".format(
        LOG, o17.get_editor_property("objective_type"), len(old_stages), old_destinations,
        old_ambient_receivers,
        o17.get_editor_property("ensure_participant_present_at_location")))

    o17.set_editor_properties({
        "objective_type": unreal.VHVQuestObjectiveType.WORLD_ACTION,
        "custom_event_tag": unreal.GameplayTag(),
        "completion_conditions": unreal.VHVStoryConditionSet(),
        "world_action_receiver_tag": tag(
            "VHV.WorldReceiver.HBCT.SelfMonitoring.SelfMonitorPayoff"),
        "world_action_tag": tag("VHV.WorldAction.StartConversation"),
        "world_action_start_policy": unreal.VHVWorldActionStartPolicy.IMMEDIATE,
        "npc_travel_mode": unreal.VHVQuestNPCTravelMode.FREE_ROAM,
        "npc_move_stages": [move_stage([
            npc_move("VHV.Participant.UncleChai",
                     "VHV.Location.HBCT.SelfMonitoring.SMSelfMonitorTracker",
                     True, 180.0)])],
        "complete_after_npc_move_stages": False,
        "ensure_participant_present_at_location": False,
    })
    quest5.set_editor_property("objectives", objectives)
    arc.set_editor_property("quests", quests)
    hbct.save_asset(arc)

    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    tracker_locations = [actor for actor in actors().get_all_level_actors()
                         if isinstance(actor, unreal.VHVQuestLocationVolume)
                         and str(actor.get_effective_location_id()) == "SMSelfMonitorTracker"]
    if len(tracker_locations) != 1:
        raise RuntimeError("SMSelfMonitorTracker must resolve exactly once; found {}".format(
            [actor.get_actor_label() for actor in tracker_locations]))

    completed_text = find_actor("HBCT_SM_Display_SelfMonitorTracker")
    if not completed_text:
        completed_text = text_display(
            "HBCT_SM_Display_SelfMonitorTracker", (19700, 900, 125),
            "TODAY'S WALK\nMARKED BY CHAI\nCOMPLETE", size=18.0,
            folder_path="06_SelfMonitoring/15_SelfMonitoringPayoff")
    completed_text.modify()
    completed_text.set_actor_location(unreal.Vector(19700.0, 900.0, 125.0), False, True)
    completed_component = completed_text.get_component_by_class(unreal.TextRenderComponent)
    completed_component.modify()
    completed_component.set_editor_property("text", "TODAY'S WALK\nMARKED BY CHAI\nCOMPLETE")

    pending_text = find_actor("HBCT_SM_Display_SelfMonitorTrackerPending")
    if not pending_text:
        pending_text = text_display(
            "HBCT_SM_Display_SelfMonitorTrackerPending", (19700, 900, 125),
            "TODAY'S WALK\nREADY FOR CHAI", size=18.0,
            folder_path="06_SelfMonitoring/15_SelfMonitoringPayoff")
    pending_text.modify()
    pending_text.set_actor_location(unreal.Vector(19700.0, 900.0, 125.0), False, True)
    pending_component = pending_text.get_component_by_class(unreal.TextRenderComponent)
    pending_component.modify()
    pending_component.set_editor_property("text", "TODAY'S WALK\nREADY FOR CHAI")

    payoff = find_actor("HBCT_SM_Ambient_SelfMonitorPayoff")
    if not payoff or not isinstance(payoff, unreal.VHVAmbientConversationActor):
        raise RuntimeError("HBCT_SM_Ambient_SelfMonitorPayoff is missing or has the wrong class")
    payoff.modify()
    payoff.set_editor_properties({
        "requires_explicit_trigger": False,
        "actors_hidden_until_conversation_starts": [completed_text],
        "actors_hidden_when_conversation_starts": [pending_text],
    })

    if not find_participant("Instructor"):
        raise RuntimeError("O18 Instructor participant is missing")
    final_locations = [actor for actor in actors().get_all_level_actors()
                       if isinstance(actor, unreal.VHVQuestLocationVolume)
                       and str(actor.get_effective_location_id()) == "SMFinalDebrief"]
    if len(final_locations) != 1 or not find_actor("HBCT_SM_Trigger_FinalDebrief"):
        raise RuntimeError("O18 SMFinalDebrief does not resolve exactly once")

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save {}".format(MAP_PATH))
    unreal.log("{} Repaired O17 as post-arrival WorldAction payoff; O18 handoff prerequisites resolve.".format(LOG))


def repair_o01_presence_only():
    """Repair only Quest 5 O01 presence metadata and its required map volume."""
    arc = unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    npc_bp = unreal.load_asset(NPC_BP_PATH)
    if not arc or not npc_bp:
        raise RuntimeError("Existing HBCT quest arc or VHV NPC Blueprint is missing")

    arc.modify()
    quests = list(arc.get_editor_property("quests"))
    quest5 = next((quest for quest in quests
                   if str(quest.get_editor_property("quest_id")) == QUEST_ID), None)
    if not quest5:
        raise RuntimeError("{} is missing from the HBCT quest arc".format(QUEST_ID))
    objectives = list(quest5.get_editor_property("objectives"))
    o01 = next((item for item in objectives
                if str(item.get_editor_property("objective_id")) == "O01_CheckInWithChai"), None)
    if not o01:
        raise RuntimeError("Quest 5 O01_CheckInWithChai is missing")
    o01.set_editor_properties({
        "participant_tag": tag("VHV.Participant.UncleChai"),
        "location_tag": tag("VHV.Location.HBCT.SelfMonitoring.SMChaiHome"),
        "ensure_participant_present_at_location": True,
        "required_participant_spawn_class": npc_bp.generated_class(),
    })
    quest5.set_editor_property("objectives", objectives)
    arc.set_editor_property("quests", quests)
    hbct.save_asset(arc)

    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    chai_matches = [actor for actor in actors().get_all_level_actors()
                    if participant_id(actor) == "UncleChai"]
    if len(chai_matches) > 1:
        raise RuntimeError("Duplicate UncleChai participants: {}".format(
            [actor.get_actor_label() for actor in chai_matches]))

    trigger = find_actor("HBCT_SM_Trigger_ChaiCheckIn")
    if trigger and not isinstance(trigger, unreal.VHVQuestLocationVolume):
        raise RuntimeError("HBCT_SM_Trigger_ChaiCheckIn has the wrong actor class")
    if not trigger:
        trigger = spawn_trigger(
            "HBCT_SM_Trigger_ChaiCheckIn", (200, 900, 100),
            "VHV.Location.HBCT.SelfMonitoring.SMChaiHome", "O01_CheckInWithChai")
    else:
        trigger.modify()
        trigger.set_actor_location(unreal.Vector(200.0, 900.0, 100.0), False, True)
        trigger.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
        trigger.set_folder_path("06_SelfMonitoring/Triggers")
        trigger.set_editor_properties({
            "location_tag": tag("VHV.Location.HBCT.SelfMonitoring.SMChaiHome"),
            "enabled": True,
            "trigger_world_action": False,
            "activate_current_objective": False,
            "required_active_quest_id": QUEST_ID,
            "required_active_objective_id": "O01_CheckInWithChai",
        })
        trigger.get_editor_property("box_component").set_box_extent(
            unreal.Vector(280.0, 280.0, 180.0), True)

    locations = [actor for actor in actors().get_all_level_actors()
                 if isinstance(actor, unreal.VHVQuestLocationVolume)
                 and str(actor.get_effective_location_id()) == "SMChaiHome"]
    if len(locations) != 1 or locations[0] != trigger:
        raise RuntimeError("SMChaiHome must resolve to exactly HBCT_SM_Trigger_ChaiCheckIn")
    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save {}".format(MAP_PATH))
    unreal.log("{} Repaired Quest 5 O01 presence metadata and required location volume only.".format(LOG))


def run():
    for directory in (ROOT, ACTIVITY_DIR, CONVERSATION_DIR, AMBIENT_DIR, QUEST_DIR):
        hbct.ensure_directory(directory)
    level = unreal.EditorAssetLibrary.load_asset(LEVEL_PATH)
    arc = unreal.EditorAssetLibrary.load_asset(ARC_PATH)
    if not level or not arc:
        raise RuntimeError("Existing HBCT level data or quest arc is missing")
    level.modify()
    arc.modify()
    configure_level_data(level)
    conversations = create_conversations()
    ambient_assets = create_ambient_assets()
    configure_arc(arc, conversations)
    validate_authored_data(level, arc, conversations, ambient_assets)
    for asset in [level] + list(conversations.values()) + list(ambient_assets.values()) + [arc]:
        hbct.save_asset(asset)
    configure_map(ambient_assets)
    validate_map()
    unreal.log("{} Authored 18 objectives, 12 activities, {} conversations, {} ambient sequences, and {} managed map actors.".format(
        LOG, len(conversations), len(ambient_assets),
        len([actor for actor in actors().get_all_level_actors()
             if actor.get_actor_label().startswith(PREFIX)])))


if __name__ == "__main__":
    try:
        if os.environ.get("VHV_Q5_O17_ONLY") == "1":
            repair_o17_only()
        elif os.environ.get("VHV_Q5_PROGRESSION_REPAIR_ONLY") == "1":
            repair_o03_o18_progression_only()
        elif os.environ.get("VHV_O01_PRESENCE_ONLY") == "1":
            repair_o01_presence_only()
        elif os.environ.get("VHV_PRESENCE_GUARANTEE_ONLY") == "1":
            apply_presence_guarantees_only()
        elif os.environ.get("VHV_Q5_TEXT_ONLY") == "1":
            refresh_quest5_world_text()
        else:
            run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
