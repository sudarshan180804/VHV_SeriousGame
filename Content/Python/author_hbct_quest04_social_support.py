"""Author production Quest 4: Social Support.

This idempotent script owns only the Q_HBCT_04_SOCIAL_SUPPORT definition,
Quest-4 records inside the existing HBCT level data, assets named for Social
Support, and map actors whose labels begin with HBCT_SS_. It preserves every
Quest 0-3 objective and every unrelated map actor.
"""

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
QUEST_ID = "Q_HBCT_04_SOCIAL_SUPPORT"
TOPIC_ID = "HBCT_M04_SOCIAL_SUPPORT"
PREFIX = "HBCT_SS_"
LOG = "[HBCT Social Support]"


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


def dialogue_choice(option_id, text, next_node_id, correct=False, effects=None):
    return struct(
        unreal.DialogueChoiceOption,
        option_id=option_id,
        option_text=text,
        next_node_id=next_node_id,
        is_default=correct,
        selection_effects=effects or [])


def contextual(participant_tag, conversation, entry_node, unavailable_after_flag):
    return struct(
        unreal.VHVQuestContextualConversation,
        participant_tag=tag(participant_tag),
        conversation=conversation,
        entry_node_id=entry_node,
        availability_conditions=flag_not_set(unavailable_after_flag))


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


def build_activities():
    single = unreal.TextbookActivityType.SINGLE_CHOICE
    dialogue = unreal.TextbookActivityType.DIALOGUE_CHOICE
    matching = unreal.TextbookActivityType.MATCHING
    observation = unreal.TextbookActivityType.OBSERVATION
    empty_teaching = unreal.TeachingContent()
    support_options = ["Emotional", "Informational", "Instrumental", "Appraisal"]
    room_specs = [
        ("RoomEmotional", "Emotional", 0,
         "Emotional support includes encouragement, understanding, and caring.",
         "VHV.Story.Flag.HBCT.SocialSupport.SSRoomEmotionalComplete"),
        ("RoomInformational", "Informational", 1,
         "Informational support provides useful advice or health information.",
         "VHV.Story.Flag.HBCT.SocialSupport.SSRoomInformationalComplete"),
        ("RoomInstrumental", "Instrumental", 2,
         "Instrumental support provides practical help that makes the behaviour easier to carry out.",
         "VHV.Story.Flag.HBCT.SocialSupport.SSRoomInstrumentalComplete"),
        ("RoomAppraisal", "Appraisal", 3,
         "Appraisal support gives feedback that helps someone recognise their progress.",
         "VHV.Story.Flag.HBCT.SocialSupport.SSRoomAppraisalComplete"),
    ]
    room_activities = [
        hbct.activity(
            "VHV.Activity.HBCT.SocialSupport." + suffix,
            "Identify {} Support".format(label), single,
            "What kind of support did you see?", empty_teaching,
            question=hbct.question(
                "HBCT_SOCIAL_SUPPORT_" + suffix.upper(),
                "What kind of support did you see?",
                support_options,
                correct_index),
            attempt_policy=hbct.attempt_policy(
                "Look at what the supporter actually did: encouragement, advice, practical help, or feedback about progress.",
                "Review what the supporter did, then return to the room and try again."),
            correct_feedback=feedback,
            incorrect_feedback="Review what the supporter did, then try again.",
            partial_feedback="",
            success_effects=[effect(flag_name)])
        for suffix, label, correct_index, feedback, flag_name in room_specs
    ]
    return room_activities + [
        hbct.activity(
            "VHV.Activity.HBCT.SocialSupport.ChaiEmotionalResponse",
            "Support Uncle Chai", dialogue,
            "Respond to Uncle Chai's discouragement.", empty_teaching,
            dialogue_choice=struct(
                unreal.DialogueChoiceActivityReference,
                conversation_id="HBCT_SocialSupport_ChaiIntro",
                choice_node_id="SSChai_Choice",
                required_for_progress=True),
            incorrect_feedback="",
            partial_feedback=""),
        hbct.activity(
            "VHV.Activity.HBCT.SocialSupport.Network",
            "Build Uncle Chai's Support Network", matching,
            "Connect each supporter to a realistic role.", empty_teaching,
            presentation_style=unreal.VHVActivityPresentationStyle.SOCIAL_SUPPORT_NETWORK,
            matching_pairs=[
                struct(unreal.MatchingPair, left_text="Chai's Wife", right_text="Prepare healthier meals"),
                struct(unreal.MatchingPair, left_text="Chai's Daughter", right_text="Encourage / remind progress"),
                struct(unreal.MatchingPair, left_text="Walking Neighbor", right_text="Join evening walking"),
                struct(unreal.MatchingPair, left_text="Clinic Nurse / VHV", right_text="Provide health information"),
            ],
            correct_feedback="",
            incorrect_feedback="",
            partial_feedback="",
            success_effects=[effect("VHV.Story.Flag.HBCT.SocialSupport.SSNetworkComplete")]),
        hbct.activity(
            "VHV.Activity.HBCT.SocialSupport.AppraisalResponse",
            "Recognise Uncle Chai's Progress", dialogue,
            "Respond to Uncle Chai's progress.", empty_teaching,
            dialogue_choice=struct(
                unreal.DialogueChoiceActivityReference,
                conversation_id="HBCT_SocialSupport_OneMonthLater",
                choice_node_id="SSAppraisal_Choice",
                required_for_progress=True),
            incorrect_feedback="",
            partial_feedback=""),
        hbct.activity(
            "VHV.Activity.HBCT.SocialSupport.NetworkPayoff",
            "Social Support Complete", observation,
            "Review the support network you built.", empty_teaching,
            presentation_style=unreal.VHVActivityPresentationStyle.SOCIAL_SUPPORT_NETWORK_READ_ONLY,
            correct_feedback="",
            incorrect_feedback="",
            partial_feedback=""),
    ]


def configure_level_data(level):
    day = level.get_editor_property("day_data")
    topics = list(day.get_editor_property("topics"))
    topics = [topic for topic in topics
              if str(topic.get_editor_property("topic_id")) != TOPIC_ID]
    topics.append(struct(
        unreal.TopicData,
        topic_id=TOPIC_ID,
        topic_title="Technique 4 — Social Support",
        narrative_introduction=(
            "Build a practical network that helps Uncle Chai maintain healthier behaviour."),
        activities=build_activities()))
    day.set_editor_property("topics", topics)
    day.set_editor_property(
        "completion_summary",
        "Building Motivation, Goal Setting, Role Model, and Social Support complete.")
    takeaways = list(day.get_editor_property("completion_takeaways"))
    takeaway = "Good support is the right help, from the right person, at the right time."
    if takeaway not in [str(item) for item in takeaways]:
        takeaways.append(takeaway)
    day.set_editor_property("completion_takeaways", takeaways)
    level.set_editor_property("day_data", day)


def create_conversations():
    text = hbct.text_node
    learn = hbct.learning_node
    choice = hbct.choice_node
    specs = {}

    specs["DA_Conversation_HBCT_SocialSupport_InstructorReflection"] = (
        "HBCT_SocialSupport_InstructorReflection", [
            text("SSReflect_01", "Player", "They were all helping in different ways.", "SSReflect_02"),
            text("SSReflect_02", "Instructor", "Exactly.", "SSReflect_03", "VHV.Participant.Instructor"),
            text("SSReflect_03", "Instructor", "Support isn't only encouragement.", "SSReflect_04", "VHV.Participant.Instructor"),
            text("SSReflect_04", "Instructor", "Sometimes it is information.", "SSReflect_05", "VHV.Participant.Instructor"),
            text("SSReflect_05", "Instructor", "Sometimes it is practical help.", "SSReflect_06", "VHV.Participant.Instructor"),
            text("SSReflect_06", "Instructor", "And sometimes it simply means making sure someone doesn't feel alone.", "SSReflect_07", "VHV.Participant.Instructor"),
            text("SSReflect_07", "Player", "So the kind of support matters.", "SSReflect_08"),
            text("SSReflect_08", "Instructor", "Very much.", "SSReflect_09", "VHV.Participant.Instructor"),
            text("SSReflect_09", "Instructor", "Come with me. I want you to recognise the difference yourself.", speaker_tag="VHV.Participant.Instructor"),
        ])

    specs["DA_Conversation_HBCT_SocialSupport_ChaiIntro"] = (
        "HBCT_SocialSupport_ChaiIntro", [
            text("SSChai_01", "Uncle Chai", "I know what they keep telling me.", "SSChai_02", "VHV.Participant.UncleChai"),
            text("SSChai_02", "Uncle Chai", "Eat better. Walk more. Watch my sugar.", "SSChai_03", "VHV.Participant.UncleChai"),
            text("SSChai_03", "Uncle Chai", "I understand it.", "SSChai_04", "VHV.Participant.UncleChai"),
            text("SSChai_04", "Uncle Chai", "But some days I just get tired of thinking about all of it.", "SSChai_05", "VHV.Participant.UncleChai"),
            text("SSChai_05", "Uncle Chai", "Sometimes I feel discouraged. I have to control everything I eat.", "SSChai_Activity", "VHV.Participant.UncleChai"),
            learn("SSChai_Activity", "VHV.Activity.HBCT.SocialSupport.ChaiEmotionalResponse", "SSChai_Choice", TOPIC_ID),
            choice("SSChai_Choice", "Player", "How should you respond?", [
                dialogue_choice("A", "You just need more discipline.", "SSChai_WrongA"),
                dialogue_choice("B", "Then maybe stop trying for a while.", "SSChai_WrongB"),
                dialogue_choice("C", "It isn't easy, but you're doing well. You don't have to face it alone.", "SSChai_Correct", True),
            ]),
            text("SSChai_WrongA", "Uncle Chai", "I have been trying. Being told to try harder does not make it feel easier.", "SSChai_Activity", "VHV.Participant.UncleChai"),
            text("SSChai_WrongB", "Uncle Chai", "If I stop completely, I worry I will never start again.", "SSChai_Activity", "VHV.Participant.UncleChai"),
            text("SSChai_Correct", "Player", "EMOTIONAL SUPPORT — encouragement, understanding and caring.", "SSChai_06"),
            text("SSChai_06", "Uncle Chai", "My wife tries to help.", "SSChai_07", "VHV.Participant.UncleChai"),
            text("SSChai_07", "Uncle Chai", "My daughter checks on me.", "SSChai_08", "VHV.Participant.UncleChai"),
            text("SSChai_08", "Uncle Chai", "My neighbour walks most evenings.", "SSChai_09", "VHV.Participant.UncleChai"),
            text("SSChai_09", "Uncle Chai", "And I still visit the clinic.", "SSChai_10", "VHV.Participant.UncleChai"),
            text("SSChai_10", "Player", "Maybe we can make those people part of the plan."),
        ])

    supporter_specs = [
        ("DA_Conversation_HBCT_SocialSupport_ChaiWife", "HBCT_SocialSupport_ChaiWife", "SSWife", "Chai's Wife", "VHV.Participant.ChaiWife", [
            ("Player", "Chai is trying to improve his food and exercise habits."),
            ("Player", "What kind of help would be realistic for you?"),
            ("Chai's Wife", "I can help with meals."),
            ("Chai's Wife", "Especially dinner."),
            ("Chai's Wife", "Mornings are usually too busy, but I can make sure we have healthier food ready in the evening."),
            ("Player", "That would make his plan easier to follow."),
        ], "VHV.Story.Flag.HBCT.SocialSupport.SSWifeRecruited"),
        ("DA_Conversation_HBCT_SocialSupport_ChaiDaughter", "HBCT_SocialSupport_ChaiDaughter", "SSDaughter", "Chai's Daughter", "VHV.Participant.ChaiDaughter", [
            ("Player", "Your father is trying to keep up with his health plan."),
            ("Chai's Daughter", "I can't come over every day."),
            ("Chai's Daughter", "But I can call him."),
            ("Chai's Daughter", "If he knows I'm checking in, he'll probably keep at it."),
            ("Player", "A regular check-in could help."),
        ], "VHV.Story.Flag.HBCT.SocialSupport.SSDaughterRecruited"),
        ("DA_Conversation_HBCT_SocialSupport_ChaiNeighbor", "HBCT_SocialSupport_ChaiNeighbor", "SSNeighbor", "Walking Neighbor", "VHV.Participant.ChaiWalkingNeighbor", [
            ("Player", "Chai is trying to walk more regularly."),
            ("Walking Neighbor", "I walk most evenings anyway."),
            ("Walking Neighbor", "He can come with me."),
            ("Walking Neighbor", "If he's not outside, I'll knock on his gate."),
            ("Player", "Perfect."),
        ], "VHV.Story.Flag.HBCT.SocialSupport.SSNeighborRecruited"),
        ("DA_Conversation_HBCT_SocialSupport_ChaiNurse", "HBCT_SocialSupport_ChaiNurse", "SSNurse", "Clinic Nurse", "VHV.Participant.ChaiClinicNurse", [
            ("Player", "Chai understands that he needs to change, but he still gets confused about what is safe and useful."),
            ("Clinic Nurse", "I can help him with that."),
            ("Clinic Nurse", "We can review his food choices and his walking plan when he checks in."),
            ("Clinic Nurse", "He doesn't need to guess."),
        ], "VHV.Story.Flag.HBCT.SocialSupport.SSNurseRecruited"),
    ]
    for asset_name, conversation_id, prefix, speaker_name, speaker_tag, lines, completion_flag in supporter_specs:
        nodes = []
        for index, (line_speaker, line_text) in enumerate(lines, 1):
            node_id = "{}_{:02d}".format(prefix, index)
            next_id = "{}_{:02d}".format(prefix, index + 1) if index < len(lines) else ""
            node_effects = [effect(completion_flag)] if index == len(lines) else []
            nodes.append(text(
                node_id, line_speaker, line_text, next_id,
                speaker_tag if line_speaker == speaker_name else "",
                completion_effects=node_effects))
        specs[asset_name] = (conversation_id, nodes)

    specs["DA_Conversation_HBCT_SocialSupport_BackupWife"] = (
        "HBCT_SocialSupport_BackupWife", [
            text("SSBackup_01", "Player", "His walking partner can't come tonight.", "SSBackup_02"),
            text("SSBackup_02", "Player", "Chai is already thinking about skipping.", "SSBackup_03"),
            text("SSBackup_03", "Chai's Wife", "I can't do his full walk every evening.", "SSBackup_04", "VHV.Participant.ChaiWife"),
            text("SSBackup_04", "Chai's Wife", "But tonight I can walk with him for ten minutes after dinner.", "SSBackup_05", "VHV.Participant.ChaiWife"),
            text("SSBackup_05", "Player", "That's enough. The important thing is that the plan doesn't disappear because one person is unavailable.", "SSBackup_06"),
            text("SSBackup_06", "Chai's Wife", "Then let's go.", speaker_tag="VHV.Participant.ChaiWife",
                 completion_effects=[effect("VHV.Story.Flag.HBCT.SocialSupport.SSBackupAgreed")]),
        ])

    specs["DA_Conversation_HBCT_SocialSupport_OneMonthLater"] = (
        "HBCT_SocialSupport_OneMonthLater", [
            text("SSMonth_01", "Uncle Chai", "I've kept walking.", "SSMonth_02", "VHV.Participant.UncleChai"),
            text("SSMonth_02", "Uncle Chai", "Not every day was easy.", "SSMonth_03", "VHV.Participant.UncleChai"),
            text("SSMonth_03", "Uncle Chai", "But somebody usually helped me keep going.", "SSMonth_04", "VHV.Participant.UncleChai"),
            text("SSMonth_04", "Uncle Chai", "My blood sugar was 180 before.", "SSMonth_05", "VHV.Participant.UncleChai"),
            text("SSMonth_05", "Uncle Chai", "Now it's 140.", "SSMonth_06", "VHV.Participant.UncleChai"),
            text("SSMonth_06", "Uncle Chai", "And I've lost two kilograms.", "SSMonth_07", "VHV.Participant.UncleChai"),
            text("SSMonth_07", "Uncle Chai", "I'm eating more vegetables too.", "SSMonth_08", "VHV.Participant.UncleChai"),
            text("SSMonth_08", "Player", "That's a lot of change.", "SSMonth_09"),
            text("SSMonth_09", "Uncle Chai", "I didn't do it by myself.", speaker_tag="VHV.Participant.UncleChai"),
            learn("SSAppraisal_Activity", "VHV.Activity.HBCT.SocialSupport.AppraisalResponse", "SSAppraisal_Choice", TOPIC_ID),
            choice("SSAppraisal_Choice", "Player", "How should you respond to Chai's progress?", [
                dialogue_choice("A", "Good, but 140 is still too high.", "SSAppraisal_WrongA"),
                dialogue_choice("B", "Excellent. Your effort is making a difference. Keep going.", "SSAppraisal_Correct", True,
                                [effect("VHV.Story.Flag.HBCT.SocialSupport.SSAppraisalComplete")]),
                dialogue_choice("C", "Maybe you just got lucky this month.", "SSAppraisal_WrongC"),
            ]),
            text("SSAppraisal_WrongA", "Uncle Chai", "I know I still have work to do. I was hoping the progress mattered too.", "SSAppraisal_Activity", "VHV.Participant.UncleChai"),
            text("SSAppraisal_WrongC", "Uncle Chai", "It did not feel like luck. I worked at it every week.", "SSAppraisal_Activity", "VHV.Participant.UncleChai"),
            text("SSAppraisal_Correct", "Player", "APPRAISAL / FEEDBACK SUPPORT — feedback helps people recognise meaningful progress.", "SSAppraisal_Thanks"),
            text("SSAppraisal_Thanks", "Uncle Chai", "Thank you.", "SSAppraisal_End", "VHV.Participant.UncleChai"),
            text("SSAppraisal_End", "Uncle Chai", "Seeing the improvement makes me want to continue.", speaker_tag="VHV.Participant.UncleChai"),
        ])

    specs["DA_Conversation_HBCT_SocialSupport_FinalDebrief"] = (
        "HBCT_SocialSupport_FinalDebrief", [
            text("SSFinal_01", "Instructor", "When you first met Chai, he already knew that he should change.", "SSFinal_02", "VHV.Participant.Instructor"),
            text("SSFinal_02", "Player", "But knowing wasn't enough.", "SSFinal_03"),
            text("SSFinal_03", "Instructor", "Exactly.", "SSFinal_04", "VHV.Participant.Instructor"),
            text("SSFinal_04", "Instructor", "One person may inspire change.", "SSFinal_05", "VHV.Participant.Instructor"),
            text("SSFinal_05", "Instructor", "But keeping change going often takes a whole network.", "SSFinal_06", "VHV.Participant.Instructor"),
            text("SSFinal_06", "Player", "So the VHV doesn't have to provide every kind of support alone?", "SSFinal_07"),
            text("SSFinal_07", "Instructor", "Exactly.", "SSFinal_08", "VHV.Participant.Instructor"),
            text("SSFinal_08", "Instructor", "A good VHV helps the person build the right support around them.", "SSFinal_09", "VHV.Participant.Instructor"),
            text("SSFinal_09", "Player", "And the network needs to fit the person's real life.", "SSFinal_10"),
            text("SSFinal_10", "Instructor", "Yes.", "SSFinal_11", "VHV.Participant.Instructor"),
            text("SSFinal_11", "Instructor", "The right help.", "SSFinal_12", "VHV.Participant.Instructor"),
            text("SSFinal_12", "Instructor", "From the right person.", "SSFinal_13", "VHV.Participant.Instructor"),
            text("SSFinal_13", "Instructor", "At the right time.", "SSFinal_Network", "VHV.Participant.Instructor"),
            learn("SSFinal_Network", "VHV.Activity.HBCT.SocialSupport.NetworkPayoff", topic_id=TOPIC_ID),
        ])

    assets = {}
    for name, (conversation_id, nodes) in specs.items():
        asset = hbct.load_or_create(CONVERSATION_DIR, name, unreal.VHVConversationDataAsset)
        hbct.set_conversation(asset, conversation_id, nodes)
        assets[name] = asset
    return assets


def create_ambient_assets():
    participant = hbct.ambient_participant
    line = hbct.ambient_line
    specs = {
        "DA_Ambient_HBCT_SocialSupport_HealthyMeal": {
            "id": "HBCT_SocialSupport_HealthyMeal",
            "participants": [participant("Patient", "Patient"), participant("Spouse", "Spouse")],
            "lines": [
                line("Patient", "I was going to have the sweet tea.", 2.5),
                line("Spouse", "Let's have this instead. I already made some water for us.", 3.4),
                line("Patient", "All right.", 2.0),
            ],
            "effects": [effect("VHV.Story.Flag.HBCT.SocialSupport.SSObservedMeal")],
        },
        "DA_Ambient_HBCT_SocialSupport_WalkingPartner": {
            "id": "HBCT_SocialSupport_WalkingPartner",
            "participants": [participant("Partner", "Walking Partner"), participant("Villager", "Other Villager")],
            "lines": [
                line("Partner", "Come on, we said we'd walk together today.", 3.0),
                line("Villager", "I was hoping you'd forget.", 2.6),
                line("Partner", "Not a chance.", 2.1),
            ],
            "effects": [effect("VHV.Story.Flag.HBCT.SocialSupport.SSObservedWalkingPartner")],
        },
        "DA_Ambient_HBCT_SocialSupport_Encouragement": {
            "id": "HBCT_SocialSupport_Encouragement",
            "participants": [participant("Tired", "Tired Villager"), participant("Friend", "Friend")],
            "lines": [
                line("Tired", "I'm done. That's enough for today.", 2.8),
                line("Friend", "You already did ten minutes.", 2.5),
                line("Friend", "That's better than last week.", 2.5),
                line("Tired", "...You're right.", 2.2),
            ],
            "effects": [effect("VHV.Story.Flag.HBCT.SocialSupport.SSObservedEncouragement")],
        },
        "DA_Ambient_HBCT_SocialSupport_HealthGuidance": {
            "id": "HBCT_SocialSupport_HealthGuidance",
            "participants": [participant("Patient", "Patient"), participant("Nurse", "Clinic Nurse")],
            "lines": [
                line("Patient", "I'm never sure what I should change first.", 3.0),
                line("Nurse", "Start small.", 2.0),
                line("Nurse", "For your coffee, try using a little less sugar first.", 3.4),
                line("Nurse", "And keep this guide with you when you shop.", 3.2),
                line("Patient", "That makes it easier.", 2.4),
            ],
            "effects": [effect("VHV.Story.Flag.HBCT.SocialSupport.SSObservedGuidance")],
        },
        "DA_Ambient_HBCT_SocialSupport_RoomEmotional": {
            "id": "HBCT_SocialSupport_RoomEmotional",
            "participants": [participant("Patient", "Patient"), participant("Family", "Family Member")],
            "lines": [
                line("Patient", "I've only just started exercising and I'm already tired.", 3.4),
                line("Family", "Starting is the hard part.", 2.5),
                line("Family", "You're doing really well just by beginning.", 3.0),
                line("Family", "Don't give up.", 2.2),
            ], "effects": []},
        "DA_Ambient_HBCT_SocialSupport_RoomInformational": {
            "id": "HBCT_SocialSupport_RoomInformational",
            "participants": [participant("Patient", "Patient"), participant("VHV", "VHV")],
            "lines": [
                line("Patient", "I drink coffee several times a day. What should I change?", 3.5),
                line("VHV", "Start by reducing the sugar.", 2.5),
                line("VHV", "You can also choose fruit with less sugar and keep portions reasonable.", 3.8),
            ], "effects": []},
        "DA_Ambient_HBCT_SocialSupport_RoomInstrumental": {
            "id": "HBCT_SocialSupport_RoomInstrumental",
            "participants": [participant("Patient", "Patient"), participant("Family", "Family Member")],
            "lines": [
                line("Family", "I already prepared the vegetables.", 2.7),
                line("Family", "And tomorrow morning, shall we walk together?", 3.0),
                line("Patient", "That would help.", 2.3),
            ], "effects": []},
        "DA_Ambient_HBCT_SocialSupport_RoomAppraisal": {
            "id": "HBCT_SocialSupport_RoomAppraisal",
            "participants": [participant("Patient", "Patient"), participant("VHV", "VHV")],
            "lines": [
                line("VHV", "You've been exercising consistently.", 2.8),
                line("VHV", "Look at how much more often you're walking compared with last month.", 3.8),
                line("Patient", "I didn't realise I had improved that much.", 3.0),
            ], "effects": []},
        "DA_Ambient_HBCT_SocialSupport_RoomsComplete": {
            "id": "HBCT_SocialSupport_RoomsComplete",
            "participants": [participant("Instructor", "Instructor")],
            "lines": [
                line("Instructor", "Now you know what support can look like.", 3.0),
                line("Instructor", "Next, I want you to build it.", 2.7),
            ], "effects": []},
        "DA_Ambient_HBCT_SocialSupport_NetworkReady": {
            "id": "HBCT_SocialSupport_NetworkReady",
            "participants": [participant("Instructor", "Instructor")],
            "lines": [line("Instructor", "A support network works best when everyone has a clear role.", 3.8)],
            "effects": []},
        "DA_Ambient_HBCT_SocialSupport_PlanInAction": {
            "id": "HBCT_SocialSupport_PlanInAction",
            "participants": [
                participant("Chai", "Uncle Chai"), participant("Wife", "Chai's Wife"),
                participant("Daughter", "Chai's Daughter"), participant("Neighbor", "Walking Neighbor")],
            "lines": [
                line("Wife", "I made vegetables with dinner tonight.", 3.0),
                line("Chai", "That helps. When it's already there, I don't have to think so much about what to choose.", 4.4),
                line("Daughter", "Did you walk today?", 2.3),
                line("Chai", "Not yet.", 1.8),
                line("Daughter", "I'm checking again tomorrow.", 2.5),
                line("Chai", "I know you will.", 2.1),
                line("Neighbor", "Chai.", 1.6),
                line("Neighbor", "Ready?", 1.8),
                line("Chai", "Already?", 1.8),
                line("Neighbor", "We said we'd walk today.", 2.5),
                line("Chai", "All right, all right.", 2.3),
            ], "effects": []},
        "DA_Ambient_HBCT_SocialSupport_ChaiWalkAlong": {
            "id": "HBCT_SocialSupport_ChaiWalkAlong",
            "participants": [participant("Chai", "Uncle Chai"), participant("Neighbor", "Walking Neighbor")],
            "lines": [
                line("Neighbor", "Ready? We said we'd walk today.", 2.8),
                line("Chai", "I nearly talked myself out of it.", 2.8),
                line("Neighbor", "That's why I came to get you.", 2.8),
                line("Chai", "I always think the hardest part is getting started.", 3.4, delay=0.8),
                line("Neighbor", "Then don't start alone.", 2.5),
                line("Chai", "It is easier when someone is waiting for me.", 3.2, delay=0.8),
                line("Neighbor", "And if you're not outside tomorrow, I'm knocking on your gate.", 3.8),
                line("Chai", "Then I suppose I don't have an excuse.", 3.0),
                line("Chai", "Walking doesn't feel as difficult when I'm not doing it alone.", 3.6, thought=True),
            ], "effects": []},
        "DA_Ambient_HBCT_SocialSupport_Disruption": {
            "id": "HBCT_SocialSupport_Disruption",
            "participants": [participant("Chai", "Uncle Chai"), participant("Neighbor", "Walking Neighbor")],
            "lines": [
                line("Neighbor", "Sorry, Chai. I can't walk tonight.", 3.0, delay=2.0),
                line("Neighbor", "I have something I need to take care of.", 3.0),
                line("Chai", "Maybe I'll just skip today.", 2.7),
            ], "effects": []},
        "DA_Ambient_HBCT_SocialSupport_BackupWalk": {
            "id": "HBCT_SocialSupport_BackupWalk",
            "participants": [participant("Chai", "Uncle Chai"), participant("Wife", "Chai's Wife")],
            "lines": [
                line("Wife", "Ten minutes is enough for tonight.", 2.8),
                line("Chai", "I almost skipped the whole thing.", 2.8),
                line("Wife", "Doing a little is better than doing nothing.", 3.0),
                line("Chai", "You're right.", 2.0),
            ], "effects": []},
    }

    assets = {}
    for name, data in specs.items():
        asset = hbct.load_or_create(AMBIENT_DIR, name, unreal.VHVAmbientConversationData)
        asset.set_editor_properties({
            "conversation_id": data["id"],
            "participants": data["participants"],
            "lines": data["lines"],
            "show_speaker_name": True,
            "completion_effects": data["effects"],
        })
        assets[name] = asset
    return assets


def configure_arc(arc, conversations):
    q = unreal.VHVQuestObjectiveType
    start_action = tag("VHV.WorldAction.StartConversation")
    observed = (
        "VHV.Story.Flag.HBCT.SocialSupport.SSObservedMeal",
        "VHV.Story.Flag.HBCT.SocialSupport.SSObservedWalkingPartner",
        "VHV.Story.Flag.HBCT.SocialSupport.SSObservedEncouragement",
        "VHV.Story.Flag.HBCT.SocialSupport.SSObservedGuidance")
    rooms = (
        "VHV.Story.Flag.HBCT.SocialSupport.SSRoomEmotionalComplete",
        "VHV.Story.Flag.HBCT.SocialSupport.SSRoomInformationalComplete",
        "VHV.Story.Flag.HBCT.SocialSupport.SSRoomInstrumentalComplete",
        "VHV.Story.Flag.HBCT.SocialSupport.SSRoomAppraisalComplete")
    supporters = (
        "VHV.Story.Flag.HBCT.SocialSupport.SSWifeRecruited",
        "VHV.Story.Flag.HBCT.SocialSupport.SSDaughterRecruited",
        "VHV.Story.Flag.HBCT.SocialSupport.SSNeighborRecruited",
        "VHV.Story.Flag.HBCT.SocialSupport.SSNurseRecruited")

    quest4 = struct(
        unreal.VHVQuestDefinition,
        quest_id=QUEST_ID,
        quest_title="TECHNIQUE 4: SOCIAL SUPPORT",
        quest_description="Build a functioning support network around Uncle Chai and help it adapt.",
        category=unreal.VHVQuestCategory.MAIN_STORY,
        auto_track=True,
        auto_start_next_quest=False,
        completion_stinger=hbct.major_stinger(
            "SOCIAL SUPPORT — COMPLETE", "THE RIGHT HELP", "FROM THE RIGHT PERSON • AT THE RIGHT TIME", 2.4),
        objectives=[
            objective(
                "O01_ObserveVillageSupport", "Observe how the village supports healthy behaviour", q.CUSTOM_EVENT,
                custom_event_tag=tag("VHV.CustomEvent.HBCT.SocialSupport.ObservationsComplete"),
                completion_conditions=all_flags(*observed),
                completion_npc_moves=[npc_move(
                    "VHV.Participant.Instructor",
                    "VHV.Location.HBCT.SocialSupport.SSInstructorCatchup")]),
            objective(
                "O02_ReflectOnSupport", "Talk to the Instructor about what you observed", q.CONVERSATION,
                participant_tag=tag("VHV.Participant.Instructor"),
                conversation=conversations["DA_Conversation_HBCT_SocialSupport_InstructorReflection"],
                entry_node_id="SSReflect_01", auto_start=False,
                ensure_participant_present_at_location=True,
                activation_npc_readiness=[npc_move(
                    "VHV.Participant.Instructor",
                    "VHV.Location.HBCT.SocialSupport.SSInstructorCatchup")],
                completion_npc_moves=[npc_move(
                    "VHV.Participant.Instructor",
                    "VHV.Location.HBCT.SocialSupport.SSSupportHouseInstructor")]),
            objective(
                "O03_ExploreSupportHouse", "Explore the Support House", q.CUSTOM_EVENT,
                custom_event_tag=tag("VHV.CustomEvent.HBCT.SocialSupport.RoomsComplete"),
                completion_conditions=all_flags(*rooms)),
            objective(
                "O04_MeetUncleChai", "Talk to Uncle Chai", q.CONVERSATION,
                participant_tag=tag("VHV.Participant.UncleChai"),
                location_tag=tag("VHV.Location.HBCT.SocialSupport.SSChaiHome"),
                ensure_participant_present_at_location=True,
                conversation=conversations["DA_Conversation_HBCT_SocialSupport_ChaiIntro"],
                entry_node_id="SSChai_01", auto_start=False,
                activation_conditions=all_flags(
                    "VHV.Story.Flag.HBCT.SocialSupport.SSChaiIntroReady")),
            objective(
                "O05_FindChaiSupporters", "Find people who can help Uncle Chai continue", q.CUSTOM_EVENT,
                custom_event_tag=tag("VHV.CustomEvent.HBCT.SocialSupport.SupportersComplete"),
                ensure_participant_present_at_location=True,
                activation_npc_readiness=[
                    npc_move("VHV.Participant.ChaiWife", "VHV.Location.HBCT.SocialSupport.SSChaiWifeRecruit"),
                    npc_move("VHV.Participant.ChaiDaughter", "VHV.Location.HBCT.SocialSupport.SSChaiDaughterRecruit"),
                    npc_move("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSChaiNeighborRecruit"),
                    npc_move("VHV.Participant.ChaiClinicNurse", "VHV.Location.HBCT.SocialSupport.SSChaiNurseRecruit"),
                ],
                completion_conditions=all_flags(*supporters),
                completion_npc_moves=[npc_move(
                    "VHV.Participant.Instructor",
                    "VHV.Location.HBCT.SocialSupport.SSNetworkInstructor")],
                contextual_conversations=[
                    contextual("VHV.Participant.ChaiWife", conversations["DA_Conversation_HBCT_SocialSupport_ChaiWife"], "SSWife_01", supporters[0]),
                    contextual("VHV.Participant.ChaiDaughter", conversations["DA_Conversation_HBCT_SocialSupport_ChaiDaughter"], "SSDaughter_01", supporters[1]),
                    contextual("VHV.Participant.ChaiWalkingNeighbor", conversations["DA_Conversation_HBCT_SocialSupport_ChaiNeighbor"], "SSNeighbor_01", supporters[2]),
                    contextual("VHV.Participant.ChaiClinicNurse", conversations["DA_Conversation_HBCT_SocialSupport_ChaiNurse"], "SSNurse_01", supporters[3]),
                ]),
            objective(
                "O06_BuildSupportNetwork", "Build Uncle Chai's support network", q.LEARNING_ACTIVITY,
                activity_tag=tag("VHV.Activity.HBCT.SocialSupport.Network"), auto_start=False,
                activation_npc_readiness=[npc_move(
                    "VHV.Participant.Instructor",
                    "VHV.Location.HBCT.SocialSupport.SSNetworkInstructor")],
                completion_npc_moves=[
                    npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSPlanInAction"),
                    npc_move("VHV.Participant.ChaiWife", "VHV.Location.HBCT.SocialSupport.SSPlanWife"),
                    npc_move("VHV.Participant.ChaiDaughter", "VHV.Location.HBCT.SocialSupport.SSPlanDaughter"),
                    npc_move("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSPlanNeighbor"),
                ]),
            objective(
                "O07_WatchSupportInAction", "See Uncle Chai's support plan begin", q.WORLD_ACTION,
                world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.SocialSupport.PlanInAction"),
                world_action_tag=start_action,
                world_action_start_policy=unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER,
                ensure_participant_present_at_location=True,
                activation_npc_readiness=[
                    npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSPlanInAction"),
                    npc_move("VHV.Participant.ChaiWife", "VHV.Location.HBCT.SocialSupport.SSPlanWife"),
                    npc_move("VHV.Participant.ChaiDaughter", "VHV.Location.HBCT.SocialSupport.SSPlanDaughter"),
                    npc_move("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSPlanNeighbor"),
                ],
                completion_npc_moves=[
                    npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSChaiWalkStart"),
                    npc_move("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSNeighborWalkStart"),
                ]),
            objective(
                "O08_WalkWithChai", "Walk with Uncle Chai and his neighbour", q.CUSTOM_EVENT,
                custom_event_tag=tag("VHV.CustomEvent.HBCT.SocialSupport.WalkComplete"),
                activation_npc_readiness=[
                    npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSChaiWalkStart"),
                    npc_move("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSNeighborWalkStart"),
                ],
                npc_travel_mode=unreal.VHVQuestNPCTravelMode.FREE_ROAM,
                npc_move_stages=[
                    move_stage([
                        npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSChaiWalkMid", True, 180.0),
                        npc_move("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSNeighborWalkMid", True, 180.0),
                    ], "VHV.WorldReceiver.HBCT.SocialSupport.ChaiWalkAlong"),
                    move_stage([
                        npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSChaiWalkEnd", True, 180.0),
                        npc_move("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSNeighborWalkEnd", True, 180.0),
                    ]),
                ],
                complete_after_npc_move_stages=True,
                completion_npc_moves=[
                    npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSDisruption"),
                    npc_move("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSNeighborDisruption"),
                ]),
            objective(
                "O09_SupportDisruption", "See what happens when support is unavailable", q.WORLD_ACTION,
                activation_stinger=hbct.major_stinger("", "THE NEXT EVENING", "", 1.4),
                world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.SocialSupport.Disruption"),
                world_action_tag=start_action,
                world_action_start_policy=unreal.VHVWorldActionStartPolicy.EXPLICIT_TRIGGER,
                ensure_participant_present_at_location=True,
                activation_npc_readiness=[
                    npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSDisruption"),
                    npc_move("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSNeighborDisruption"),
                ],
                completion_npc_moves=[npc_move(
                    "VHV.Participant.ChaiWife",
                    "VHV.Location.HBCT.SocialSupport.SSBackupWife")]),
            objective(
                "O10_FindBackupSupport", "Find another way to support Uncle Chai tonight", q.CUSTOM_EVENT,
                custom_event_tag=tag("VHV.CustomEvent.HBCT.SocialSupport.BackupComplete"),
                contextual_conversations=[contextual(
                    "VHV.Participant.ChaiWife",
                    conversations["DA_Conversation_HBCT_SocialSupport_BackupWife"],
                    "SSBackup_01",
                    "VHV.Story.Flag.HBCT.SocialSupport.SSBackupAgreed")],
                activation_npc_readiness=[npc_move(
                    "VHV.Participant.ChaiWife",
                    "VHV.Location.HBCT.SocialSupport.SSBackupWife")],
                npc_move_stage_activation_conditions=all_flags(
                    "VHV.Story.Flag.HBCT.SocialSupport.SSBackupAgreed"),
                npc_travel_mode=unreal.VHVQuestNPCTravelMode.FREE_ROAM,
                npc_move_stages=[
                    move_stage([
                        npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSChaiBackupStart", True, 180.0),
                        npc_move("VHV.Participant.ChaiWife", "VHV.Location.HBCT.SocialSupport.SSWifeBackupStart", True, 180.0),
                    ]),
                    move_stage([
                        npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSChaiBackupEnd", True, 180.0),
                        npc_move("VHV.Participant.ChaiWife", "VHV.Location.HBCT.SocialSupport.SSWifeBackupEnd", True, 180.0),
                    ], "VHV.WorldReceiver.HBCT.SocialSupport.BackupWalk"),
                ],
                complete_after_npc_move_stages=True,
                completion_npc_moves=[npc_move(
                    "VHV.Participant.UncleChai",
                    "VHV.Location.HBCT.SocialSupport.SSOneMonthLater")]),
            objective(
                "O11_OneMonthLater", "Return to Uncle Chai", q.CONVERSATION,
                activation_stinger=hbct.major_stinger("", "ONE MONTH LATER", "", 1.8),
                participant_tag=tag("VHV.Participant.UncleChai"),
                conversation=conversations["DA_Conversation_HBCT_SocialSupport_OneMonthLater"],
                entry_node_id="SSMonth_01", auto_start=False,
                ensure_participant_present_at_location=True,
                activation_npc_readiness=[npc_move(
                    "VHV.Participant.UncleChai",
                    "VHV.Location.HBCT.SocialSupport.SSOneMonthLater")]),
            objective(
                "O12_ProvideAppraisalSupport", "Respond to Uncle Chai's progress", q.CONVERSATION,
                participant_tag=tag("VHV.Participant.UncleChai"),
                location_tag=tag("VHV.Location.HBCT.SocialSupport.SSOneMonthLater"),
                ensure_participant_present_at_location=True,
                conversation=conversations["DA_Conversation_HBCT_SocialSupport_OneMonthLater"],
                entry_node_id="SSAppraisal_Activity", auto_start=False,
                completion_npc_moves=[
                    npc_move("VHV.Participant.Instructor", "VHV.Location.HBCT.SocialSupport.SSFinalDebrief"),
                    npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSFinalChai"),
                    npc_move("VHV.Participant.ChaiWife", "VHV.Location.HBCT.SocialSupport.SSFinalWife"),
                    npc_move("VHV.Participant.ChaiDaughter", "VHV.Location.HBCT.SocialSupport.SSFinalDaughter"),
                    npc_move("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSFinalNeighbor"),
                ]),
            objective(
                "O13_FinalDebrief", "Reflect with the Instructor", q.CONVERSATION,
                participant_tag=tag("VHV.Participant.Instructor"),
                conversation=conversations["DA_Conversation_HBCT_SocialSupport_FinalDebrief"],
                entry_node_id="SSFinal_01", auto_start=False,
                ensure_participant_present_at_location=True,
                activation_npc_readiness=[
                    npc_move("VHV.Participant.Instructor", "VHV.Location.HBCT.SocialSupport.SSFinalDebrief"),
                    npc_move("VHV.Participant.UncleChai", "VHV.Location.HBCT.SocialSupport.SSFinalChai"),
                    npc_move("VHV.Participant.ChaiWife", "VHV.Location.HBCT.SocialSupport.SSFinalWife"),
                    npc_move("VHV.Participant.ChaiDaughter", "VHV.Location.HBCT.SocialSupport.SSFinalDaughter"),
                    npc_move("VHV.Participant.ChaiWalkingNeighbor", "VHV.Location.HBCT.SocialSupport.SSFinalNeighbor"),
                ],
                completion_effects=[effect("VHV.Story.Flag.HBCT.SocialSupport.Completed")]),
        ])

    quest4_objectives = list(quest4.get_editor_property("objectives"))
    objective_tracking.apply_tracking_metadata_to_objectives(QUEST_ID, quest4_objectives)
    quest4.set_editor_property("objectives", quest4_objectives)

    quests = list(arc.get_editor_property("quests"))
    existing_ids = [str(item.get_editor_property("quest_id")) for item in quests]
    if existing_ids[:4] != [
            "Q_HBCT_00_INTRO", "Q_HBCT_01_MOTIVATION",
            "Q_HBCT_02_GOAL_SETTING", "Q_HBCT_03_ROLE_MODEL"]:
        raise RuntimeError("Unexpected Quest 0-3 ordering; refusing to rewrite the arc")
    quests = [item for item in quests if str(item.get_editor_property("quest_id")) != QUEST_ID]
    role_model = next(item for item in quests
                      if str(item.get_editor_property("quest_id")) == "Q_HBCT_03_ROLE_MODEL")
    role_model.set_editor_property("auto_start_next_quest", True)
    quests.append(quest4)
    arc.set_editor_property("quests", quests)
    arc.set_editor_property(
        "quest_arc_description",
        "Production HBCT story arc containing the prologue and Techniques 1–4.")


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find_actor(label):
    for actor in actors().get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def folder(actor, path):
    if actor:
        actor.set_folder_path(path)
    return actor


def destroy_managed_actors():
    managed = [actor for actor in actors().get_all_level_actors()
               if actor.get_actor_label().startswith(PREFIX)]
    for actor in managed:
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
    if not component:
        return ""
    return str(component.get_effective_participant_id())


def find_participant(participant_leaf):
    matches = [actor for actor in actors().get_all_level_actors()
               if participant_id(actor) == participant_leaf]
    if len(matches) > 1:
        raise RuntimeError("Duplicate persistent participant {}: {}".format(
            participant_leaf, [actor.get_actor_label() for actor in matches]))
    return matches[0] if matches else None


def configure_participant(actor, participant_tag, display_name):
    component = actor.get_quest_participant_component()
    component.modify()
    component.set_editor_properties({
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
    dialogue = actor.get_dialogue_component()
    dialogue.modify()
    dialogue.set_editor_property("default_conversation", None)
    hbct.configure_npc_nameplate(actor, display_name, bool(display_name))
    return actor


def spawn_npc(npc_class, label, location, yaw=0.0, participant_tag="",
              display_name="", folder_path=""):
    actor = spawn_actor(npc_class, label, location, yaw, folder_path)
    component = actor.get_quest_participant_component()
    component.modify()
    component.set_editor_properties({
        "quest_participation_enabled": bool(participant_tag),
        "participant_tag": tag(participant_tag) if participant_tag else unreal.GameplayTag(),
    })
    if participant_tag:
        configure_participant(actor, participant_tag, display_name)
    else:
        hbct.configure_npc_nameplate(actor, display_name, bool(display_name))
    return actor


def spawn_location(label, location, location_tag, yaw=0.0):
    actor = spawn_actor(
        unreal.VHVQuestLocationVolume, label, location, yaw,
        "05_SocialSupport/Locations")
    actor.set_editor_properties({
        "location_tag": tag(location_tag),
        "enabled": False,
        "trigger_world_action": False,
        "activate_current_objective": False,
        "required_active_quest_id": "",
        "required_active_objective_id": "",
    })
    actor.get_editor_property("box_component").set_box_extent(
        unreal.Vector(60.0, 60.0, 120.0), True)
    return actor


def spawn_trigger(label, location, location_tag, objective_id, extent=(280.0, 280.0, 180.0),
                  receiver_tag="", activate_objective=False, trigger_conditions=None,
                  track_as_objective=False, trigger_once=True):
    actor = spawn_actor(
        unreal.VHVQuestLocationVolume, label, location, 0.0,
        "05_SocialSupport/Triggers")
    values = {
        "location_tag": tag(location_tag),
        "enabled": True,
        "trigger_world_action": bool(receiver_tag),
        "trigger_world_action_once": trigger_once,
        "track_world_action_as_objective": track_as_objective,
        "activate_current_objective": activate_objective,
        "activate_current_objective_once": True,
        "required_active_quest_id": QUEST_ID,
        "required_active_objective_id": objective_id,
        "trigger_conditions": trigger_conditions or unreal.VHVStoryConditionSet(),
    }
    if receiver_tag:
        values["world_action_receiver_tag"] = tag(receiver_tag)
        values["world_action_tag"] = tag("VHV.WorldAction.StartConversation")
    actor.set_editor_properties(values)
    actor.get_editor_property("box_component").set_box_extent(
        unreal.Vector(*extent), True)
    return actor


def spawn_ambient(label, location, asset, receiver_tag, bindings, folder_path,
                  preserve_behavior=False, requires_explicit_trigger=True):
    actor = spawn_actor(
        unreal.VHVAmbientConversationActor, label, location, 0.0, folder_path)
    actor.set_editor_properties({
        "conversation_data": asset,
        "participants": bindings,
        "auto_start_on_begin_play": False,
        "requires_explicit_trigger": requires_explicit_trigger,
        "play_once": True,
        "preserve_participant_behavior": preserve_behavior,
        "player_leave_policy": (unreal.VHVAmbientConversationLeavePolicy.CONTINUE
                                if preserve_behavior else unreal.VHVAmbientConversationLeavePolicy.CANCEL),
        "observation_radius": 2200.0,
    })
    receiver = actor.get_editor_property("world_action_receiver")
    receiver.modify()
    receiver.set_editor_properties({
        "world_actions_enabled": True,
        "receiver_tag": tag(receiver_tag),
    })
    return actor


def bind(slot, npc):
    return struct(unreal.VHVAmbientParticipantBinding, slot_id=slot, npc=npc)


def blockout_prop(label, location, scale, folder_path):
    actor = spawn_actor(unreal.StaticMeshActor, label, location, 0.0, folder_path)
    component = actor.get_editor_property("static_mesh_component")
    component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def configure_map(ambient_assets):
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    destroy_managed_actors()

    npc_bp = unreal.load_asset(NPC_BP_PATH)
    if not npc_bp:
        raise RuntimeError("Missing {}".format(NPC_BP_PATH))
    npc_class = npc_bp.generated_class()

    instructor = find_participant("Instructor")
    if not instructor:
        instructor = find_actor("HBCT_Intro_Instructor")
    if not instructor:
        raise RuntimeError("Persistent Instructor was not found; refusing to create a duplicate")
    configure_participant(instructor, "VHV.Participant.Instructor", "Instructor")

    chai = find_participant("UncleChai")
    if not chai:
        chai = find_actor("HBCT_GoalSetting_UncleChai")
    if not chai:
        raise RuntimeError("Existing Uncle Chai was not found; refusing to create a duplicate")
    configure_participant(chai, "VHV.Participant.UncleChai", "Uncle Chai")

    obs_folder = "05_SocialSupport/01_Observation"
    inst_folder = "05_SocialSupport/02_Instructor"
    house_folder = "05_SocialSupport/03_SupportHouse"
    chai_folder = "05_SocialSupport/04_Chai"
    supporter_folder = "05_SocialSupport/05_Supporters"
    network_folder = "05_SocialSupport/06_Network"
    walk_folder = "05_SocialSupport/07_Walk"
    disruption_folder = "05_SocialSupport/08_Disruption"
    month_folder = "05_SocialSupport/09_TimeJump"
    final_folder = "05_SocialSupport/10_Final"

    meal_patient = spawn_npc(npc_class, "HBCT_SS_NPC_MealPatient", (100, 1800, 100), 90, folder_path=obs_folder)
    meal_spouse = spawn_npc(npc_class, "HBCT_SS_NPC_MealSpouse", (100, 1980, 100), -90, folder_path=obs_folder)
    obs_walker = spawn_npc(
        npc_class, "HBCT_SS_NPC_ObservationWalker", (1100, 1800, 100), 0,
        "VHV.Participant.SocialSupport.SSObservationWalker", "", obs_folder)
    obs_partner = spawn_npc(
        npc_class, "HBCT_SS_NPC_ObservationWalkingPartner", (1100, 1980, 100), 0,
        "VHV.Participant.SocialSupport.SSObservationWalkingPartner", "", obs_folder)
    tired = spawn_npc(npc_class, "HBCT_SS_NPC_TiredVillager", (2100, 1800, 100), 90, folder_path=obs_folder)
    friend = spawn_npc(npc_class, "HBCT_SS_NPC_EncouragingFriend", (2100, 1980, 100), -90, folder_path=obs_folder)
    guidance_nurse = spawn_npc(npc_class, "HBCT_SS_NPC_GuidanceNurse", (3100, 1800, 100), 90, folder_path=obs_folder)
    guidance_patient = spawn_npc(npc_class, "HBCT_SS_NPC_GuidancePatient", (3100, 1980, 100), -90, folder_path=obs_folder)

    emotional_patient = spawn_npc(npc_class, "HBCT_SS_NPC_RoomEmotionalPatient", (5200, 1450, 100), 90, folder_path=house_folder)
    emotional_family = spawn_npc(npc_class, "HBCT_SS_NPC_RoomEmotionalFamily", (5200, 1630, 100), -90, folder_path=house_folder)
    info_vhv = spawn_npc(npc_class, "HBCT_SS_NPC_RoomInformationalVHV", (6000, 1450, 100), 90, folder_path=house_folder)
    info_patient = spawn_npc(npc_class, "HBCT_SS_NPC_RoomInformationalPatient", (6000, 1630, 100), -90, folder_path=house_folder)
    instrumental_patient = spawn_npc(npc_class, "HBCT_SS_NPC_RoomInstrumentalPatient", (5200, 2250, 100), 90, folder_path=house_folder)
    instrumental_family = spawn_npc(npc_class, "HBCT_SS_NPC_RoomInstrumentalFamily", (5200, 2430, 100), -90, folder_path=house_folder)
    appraisal_patient = spawn_npc(npc_class, "HBCT_SS_NPC_RoomAppraisalPatient", (6000, 2250, 100), 90, folder_path=house_folder)
    appraisal_vhv = spawn_npc(npc_class, "HBCT_SS_NPC_RoomAppraisalVHV", (6000, 2430, 100), -90, folder_path=house_folder)

    wife = spawn_npc(npc_class, "HBCT_SS_NPC_ChaiWife", (8100, 1350, 100), 90,
                     "VHV.Participant.ChaiWife", "Chai's Wife", supporter_folder)
    daughter = spawn_npc(npc_class, "HBCT_SS_NPC_ChaiDaughter", (8100, 2250, 100), -90,
                         "VHV.Participant.ChaiDaughter", "Chai's Daughter", supporter_folder)
    neighbor = spawn_npc(npc_class, "HBCT_SS_NPC_ChaiWalkingNeighbor", (9000, 1350, 100), 90,
                         "VHV.Participant.ChaiWalkingNeighbor", "Walking Neighbor", supporter_folder)
    nurse = spawn_npc(npc_class, "HBCT_SS_NPC_ChaiClinicNurse", (9000, 2250, 100), -90,
                      "VHV.Participant.ChaiClinicNurse", "Clinic Nurse", supporter_folder)

    ambient = {}
    ambient["meal"] = spawn_ambient(
        "HBCT_SS_Ambient_ObserveMeal", (100, 1890, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_HealthyMeal"],
        "VHV.WorldReceiver.HBCT.SocialSupport.ObserveMeal",
        [bind("Patient", meal_patient), bind("Spouse", meal_spouse)], obs_folder)
    ambient["walking"] = spawn_ambient(
        "HBCT_SS_Ambient_ObserveWalkingPartner", (1100, 1890, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_WalkingPartner"],
        "VHV.WorldReceiver.HBCT.SocialSupport.ObserveWalkingPartner",
        [bind("Partner", obs_partner), bind("Villager", obs_walker)], obs_folder, True)
    ambient["encouragement"] = spawn_ambient(
        "HBCT_SS_Ambient_ObserveEncouragement", (2100, 1890, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_Encouragement"],
        "VHV.WorldReceiver.HBCT.SocialSupport.ObserveEncouragement",
        [bind("Tired", tired), bind("Friend", friend)], obs_folder)
    ambient["guidance"] = spawn_ambient(
        "HBCT_SS_Ambient_ObserveHealthGuidance", (3100, 1890, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_HealthGuidance"],
        "VHV.WorldReceiver.HBCT.SocialSupport.ObserveHealthGuidance",
        [bind("Patient", guidance_patient), bind("Nurse", guidance_nurse)], obs_folder)
    ambient["room_emotional"] = spawn_ambient(
        "HBCT_SS_Ambient_RoomEmotional", (5200, 1540, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_RoomEmotional"],
        "VHV.WorldReceiver.HBCT.SocialSupport.RoomEmotional",
        [bind("Patient", emotional_patient), bind("Family", emotional_family)], house_folder)
    ambient["room_informational"] = spawn_ambient(
        "HBCT_SS_Ambient_RoomInformational", (6000, 1540, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_RoomInformational"],
        "VHV.WorldReceiver.HBCT.SocialSupport.RoomInformational",
        [bind("Patient", info_patient), bind("VHV", info_vhv)], house_folder)
    ambient["room_instrumental"] = spawn_ambient(
        "HBCT_SS_Ambient_RoomInstrumental", (5200, 2340, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_RoomInstrumental"],
        "VHV.WorldReceiver.HBCT.SocialSupport.RoomInstrumental",
        [bind("Patient", instrumental_patient), bind("Family", instrumental_family)], house_folder)
    ambient["room_appraisal"] = spawn_ambient(
        "HBCT_SS_Ambient_RoomAppraisal", (6000, 2340, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_RoomAppraisal"],
        "VHV.WorldReceiver.HBCT.SocialSupport.RoomAppraisal",
        [bind("Patient", appraisal_patient), bind("VHV", appraisal_vhv)], house_folder)
    ambient["rooms_complete"] = spawn_ambient(
        "HBCT_SS_Ambient_RoomsComplete", (6400, 1800, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_RoomsComplete"],
        "VHV.WorldReceiver.HBCT.SocialSupport.RoomsComplete",
        [bind("Instructor", instructor)], inst_folder,
        requires_explicit_trigger=False)
    ambient["network_ready"] = spawn_ambient(
        "HBCT_SS_Ambient_NetworkReady", (10000, 1980, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_NetworkReady"],
        "VHV.WorldReceiver.HBCT.SocialSupport.NetworkReady",
        [bind("Instructor", instructor)], network_folder,
        requires_explicit_trigger=False)
    ambient["plan"] = spawn_ambient(
        "HBCT_SS_Ambient_PlanInAction", (10800, 1800, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_PlanInAction"],
        "VHV.WorldReceiver.HBCT.SocialSupport.PlanInAction",
        [bind("Chai", chai), bind("Wife", wife), bind("Daughter", daughter), bind("Neighbor", neighbor)], chai_folder)
    ambient["walk"] = spawn_ambient(
        "HBCT_SS_Ambient_ChaiWalkAlong", (12400, 1800, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_ChaiWalkAlong"],
        "VHV.WorldReceiver.HBCT.SocialSupport.ChaiWalkAlong",
        [bind("Chai", chai), bind("Neighbor", neighbor)], walk_folder, True,
        requires_explicit_trigger=False)
    ambient["disruption"] = spawn_ambient(
        "HBCT_SS_Ambient_Disruption", (14000, 1890, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_Disruption"],
        "VHV.WorldReceiver.HBCT.SocialSupport.Disruption",
        [bind("Chai", chai), bind("Neighbor", neighbor)], disruption_folder)
    ambient["backup"] = spawn_ambient(
        "HBCT_SS_Ambient_BackupWalk", (15950, 1800, 100),
        ambient_assets["DA_Ambient_HBCT_SocialSupport_BackupWalk"],
        "VHV.WorldReceiver.HBCT.SocialSupport.BackupWalk",
        [bind("Chai", chai), bind("Wife", wife)], disruption_folder, True,
        requires_explicit_trigger=False)

    observation_specs = [
        ("HBCT_SS_Trigger_ObserveMeal", (100, 1800, 100), "VHV.Location.HBCT.SocialSupport.SSMealScene", "VHV.WorldReceiver.HBCT.SocialSupport.ObserveMeal", "VHV.Story.Flag.HBCT.SocialSupport.SSObservedMeal"),
        ("HBCT_SS_Trigger_ObserveWalkingPartner", (1100, 1800, 100), "VHV.Location.HBCT.SocialSupport.SSWalkingPartnerScene", "VHV.WorldReceiver.HBCT.SocialSupport.ObserveWalkingPartner", "VHV.Story.Flag.HBCT.SocialSupport.SSObservedWalkingPartner"),
        ("HBCT_SS_Trigger_ObserveEncouragement", (2100, 1800, 100), "VHV.Location.HBCT.SocialSupport.SSEncouragementScene", "VHV.WorldReceiver.HBCT.SocialSupport.ObserveEncouragement", "VHV.Story.Flag.HBCT.SocialSupport.SSObservedEncouragement"),
        ("HBCT_SS_Trigger_ObserveHealthGuidance", (3100, 1800, 100), "VHV.Location.HBCT.SocialSupport.SSHealthGuidanceScene", "VHV.WorldReceiver.HBCT.SocialSupport.ObserveHealthGuidance", "VHV.Story.Flag.HBCT.SocialSupport.SSObservedGuidance"),
    ]
    for label, location, location_tag, receiver, completed_flag in observation_specs:
        spawn_trigger(label, location, location_tag, "O01_ObserveVillageSupport",
                      receiver_tag=receiver, trigger_conditions=flag_not_set(completed_flag))

    room_specs = [
        ("HBCT_SS_Trigger_RoomEmotional", (5200, 1450, 100), "VHV.Location.HBCT.SocialSupport.SSRoomEmotional", "VHV.WorldReceiver.HBCT.SocialSupport.RoomEmotional", "VHV.Story.Flag.HBCT.SocialSupport.SSRoomEmotionalComplete"),
        ("HBCT_SS_Trigger_RoomInformational", (6000, 1450, 100), "VHV.Location.HBCT.SocialSupport.SSRoomInformational", "VHV.WorldReceiver.HBCT.SocialSupport.RoomInformational", "VHV.Story.Flag.HBCT.SocialSupport.SSRoomInformationalComplete"),
        ("HBCT_SS_Trigger_RoomInstrumental", (5200, 2250, 100), "VHV.Location.HBCT.SocialSupport.SSRoomInstrumental", "VHV.WorldReceiver.HBCT.SocialSupport.RoomInstrumental", "VHV.Story.Flag.HBCT.SocialSupport.SSRoomInstrumentalComplete"),
        ("HBCT_SS_Trigger_RoomAppraisal", (6000, 2250, 100), "VHV.Location.HBCT.SocialSupport.SSRoomAppraisal", "VHV.WorldReceiver.HBCT.SocialSupport.RoomAppraisal", "VHV.Story.Flag.HBCT.SocialSupport.SSRoomAppraisalComplete"),
    ]
    for label, location, location_tag, receiver, completed_flag in room_specs:
        spawn_trigger(label, location, location_tag, "O03_ExploreSupportHouse",
                      extent=(240, 240, 180), receiver_tag=receiver,
                      trigger_conditions=flag_not_set(completed_flag), trigger_once=False)

    spawn_trigger("HBCT_SS_Trigger_InstructorCatchup", (4100, 1800, 100),
                  "VHV.Location.HBCT.SocialSupport.SSInstructorCatchup", "O02_ReflectOnSupport")
    spawn_trigger("HBCT_SS_Trigger_ChaiIntro", (7200, 1800, 100),
                  "VHV.Location.HBCT.SocialSupport.SSChaiHome", "O04_MeetUncleChai")
    spawn_trigger("HBCT_SS_Trigger_ChaiWife", (8100, 1350, 100),
                  "VHV.Location.HBCT.SocialSupport.SSChaiWifeRecruit", "O05_FindChaiSupporters")
    spawn_trigger("HBCT_SS_Trigger_ChaiDaughter", (8100, 2250, 100),
                  "VHV.Location.HBCT.SocialSupport.SSChaiDaughterRecruit", "O05_FindChaiSupporters")
    spawn_trigger("HBCT_SS_Trigger_ChaiNeighbor", (9000, 1350, 100),
                  "VHV.Location.HBCT.SocialSupport.SSChaiNeighborRecruit", "O05_FindChaiSupporters")
    spawn_trigger("HBCT_SS_Trigger_ChaiNurse", (9000, 2250, 100),
                  "VHV.Location.HBCT.SocialSupport.SSChaiNurseRecruit", "O05_FindChaiSupporters")
    spawn_trigger("HBCT_SS_Trigger_NetworkBoard", (10000, 1800, 100),
                  "VHV.Location.HBCT.SocialSupport.SSNetworkBoard", "O06_BuildSupportNetwork",
                  activate_objective=True)
    spawn_trigger("HBCT_SS_Trigger_PlanInAction", (10800, 1800, 100),
                  "VHV.Location.HBCT.SocialSupport.SSPlanInAction", "O07_WatchSupportInAction",
                  receiver_tag="VHV.WorldReceiver.HBCT.SocialSupport.PlanInAction",
                  track_as_objective=True)
    spawn_trigger("HBCT_SS_Trigger_WalkStart", (11600, 1700, 100),
                  "VHV.Location.HBCT.SocialSupport.SSWalkStartTrigger", "O08_WalkWithChai")
    spawn_trigger("HBCT_SS_Trigger_Disruption", (14000, 1800, 100),
                  "VHV.Location.HBCT.SocialSupport.SSDisruption", "O09_SupportDisruption",
                  receiver_tag="VHV.WorldReceiver.HBCT.SocialSupport.Disruption",
                  track_as_objective=True)
    spawn_trigger("HBCT_SS_Trigger_BackupWife", (14800, 1800, 100),
                  "VHV.Location.HBCT.SocialSupport.SSBackupWife", "O10_FindBackupSupport")
    spawn_trigger("HBCT_SS_Trigger_OneMonthLater", (17200, 1800, 100),
                  "VHV.Location.HBCT.SocialSupport.SSOneMonthLater", "O11_OneMonthLater")
    spawn_trigger("HBCT_SS_Trigger_FinalDebrief", (18200, 1800, 100),
                  "VHV.Location.HBCT.SocialSupport.SSFinalDebrief", "O13_FinalDebrief")

    location_specs = [
        ("HBCT_SS_Location_ObservationWalkEnd", (1550, 1800, 100), "VHV.Location.HBCT.SocialSupport.SSObservationWalkEnd"),
        ("HBCT_SS_Location_ObservationPartnerWalkEnd", (1550, 1980, 100), "VHV.Location.HBCT.SocialSupport.SSObservationPartnerWalkEnd"),
        ("HBCT_SS_Location_SupportHouseInstructor", (6400, 1800, 100), "VHV.Location.HBCT.SocialSupport.SSSupportHouseInstructor"),
        ("HBCT_SS_Location_NetworkInstructor", (10000, 1980, 100), "VHV.Location.HBCT.SocialSupport.SSNetworkInstructor"),
        ("HBCT_SS_Location_PlanWife", (10800, 1600, 100), "VHV.Location.HBCT.SocialSupport.SSPlanWife"),
        ("HBCT_SS_Location_PlanDaughter", (10600, 1950, 100), "VHV.Location.HBCT.SocialSupport.SSPlanDaughter"),
        ("HBCT_SS_Location_PlanNeighbor", (11000, 1950, 100), "VHV.Location.HBCT.SocialSupport.SSPlanNeighbor"),
        ("HBCT_SS_Location_ChaiWalkStart", (11600, 1700, 100), "VHV.Location.HBCT.SocialSupport.SSChaiWalkStart"),
        ("HBCT_SS_Location_NeighborWalkStart", (11600, 1900, 100), "VHV.Location.HBCT.SocialSupport.SSNeighborWalkStart"),
        ("HBCT_SS_Location_ChaiWalkMid", (12400, 1700, 100), "VHV.Location.HBCT.SocialSupport.SSChaiWalkMid"),
        ("HBCT_SS_Location_NeighborWalkMid", (12400, 1900, 100), "VHV.Location.HBCT.SocialSupport.SSNeighborWalkMid"),
        ("HBCT_SS_Location_ChaiWalkEnd", (13200, 1700, 100), "VHV.Location.HBCT.SocialSupport.SSChaiWalkEnd"),
        ("HBCT_SS_Location_NeighborWalkEnd", (13200, 1900, 100), "VHV.Location.HBCT.SocialSupport.SSNeighborWalkEnd"),
        ("HBCT_SS_Location_NeighborDisruption", (14000, 1980, 100), "VHV.Location.HBCT.SocialSupport.SSNeighborDisruption"),
        ("HBCT_SS_Location_ChaiBackupStart", (15600, 1700, 100), "VHV.Location.HBCT.SocialSupport.SSChaiBackupStart"),
        ("HBCT_SS_Location_WifeBackupStart", (15600, 1900, 100), "VHV.Location.HBCT.SocialSupport.SSWifeBackupStart"),
        ("HBCT_SS_Location_ChaiBackupEnd", (16300, 1700, 100), "VHV.Location.HBCT.SocialSupport.SSChaiBackupEnd"),
        ("HBCT_SS_Location_WifeBackupEnd", (16300, 1900, 100), "VHV.Location.HBCT.SocialSupport.SSWifeBackupEnd"),
        ("HBCT_SS_Location_FinalWife", (18050, 2050, 100), "VHV.Location.HBCT.SocialSupport.SSFinalWife"),
        ("HBCT_SS_Location_FinalDaughter", (18200, 2100, 100), "VHV.Location.HBCT.SocialSupport.SSFinalDaughter"),
        ("HBCT_SS_Location_FinalNeighbor", (18350, 2050, 100), "VHV.Location.HBCT.SocialSupport.SSFinalNeighbor"),
        ("HBCT_SS_Location_FinalChai", (18200, 1600, 100), "VHV.Location.HBCT.SocialSupport.SSFinalChai"),
    ]
    locations = {label: spawn_location(label, position, location_tag)
                 for label, position, location_tag in location_specs}

    blockout_prop("HBCT_SS_Prop_MealTable", (220, 1890, 55), (0.45, 0.32, 0.10), obs_folder)
    blockout_prop("HBCT_SS_Prop_SugaryDrink", (80, 1870, 70), (0.07, 0.07, 0.22), obs_folder)
    blockout_prop("HBCT_SS_Prop_HealthyDrink", (120, 1870, 70), (0.07, 0.07, 0.22), obs_folder)
    blockout_prop("HBCT_SS_Prop_RoomVegetables", (5310, 2360, 60), (0.32, 0.22, 0.08), house_folder)
    blockout_prop("HBCT_SS_Prop_RoomWalkingShoes", (5100, 2380, 45), (0.25, 0.12, 0.06), house_folder)
    blockout_prop("HBCT_SS_Prop_RoomProgressBoard", (6100, 2350, 125), (0.06, 0.50, 0.38), house_folder)
    blockout_prop("HBCT_SS_Prop_OneMonthShoes", (17100, 1950, 45), (0.25, 0.12, 0.06), month_folder)
    blockout_prop("HBCT_SS_Prop_OneMonthCalendar", (17300, 1980, 125), (0.06, 0.48, 0.38), month_folder)
    blockout_prop("HBCT_SS_Prop_OneMonthHealthyMeal", (17120, 1650, 60), (0.35, 0.25, 0.08), month_folder)
    blockout_prop("HBCT_SS_Prop_OneMonthProgressSheet", (17320, 1650, 110), (0.05, 0.38, 0.28), month_folder)

    director = spawn_actor(
        unreal.VHVSocialSupportDirector, "HBCT_SS_Director", (10000, 1800, 100), 0.0,
        network_folder)
    director.set_editor_properties({
        "observation_flags": [
            tag("VHV.Story.Flag.HBCT.SocialSupport.SSObservedMeal"),
            tag("VHV.Story.Flag.HBCT.SocialSupport.SSObservedWalkingPartner"),
            tag("VHV.Story.Flag.HBCT.SocialSupport.SSObservedEncouragement"),
            tag("VHV.Story.Flag.HBCT.SocialSupport.SSObservedGuidance"),
        ],
        "supporter_flags": [
            tag("VHV.Story.Flag.HBCT.SocialSupport.SSWifeRecruited"),
            tag("VHV.Story.Flag.HBCT.SocialSupport.SSDaughterRecruited"),
            tag("VHV.Story.Flag.HBCT.SocialSupport.SSNeighborRecruited"),
            tag("VHV.Story.Flag.HBCT.SocialSupport.SSNurseRecruited"),
        ],
        "walking_observation_scene": ambient["walking"],
        "observation_walker_tag": tag("VHV.Participant.SocialSupport.SSObservationWalker"),
        "observation_partner_tag": tag("VHV.Participant.SocialSupport.SSObservationWalkingPartner"),
        "observation_walker_destination_tag": tag("VHV.Location.HBCT.SocialSupport.SSObservationWalkEnd"),
        "observation_partner_destination_tag": tag("VHV.Location.HBCT.SocialSupport.SSObservationPartnerWalkEnd"),
        "emotional_room_scene": ambient["room_emotional"],
        "informational_room_scene": ambient["room_informational"],
        "instrumental_room_scene": ambient["room_instrumental"],
        "appraisal_room_scene": ambient["room_appraisal"],
        "emotional_room_flag": tag("VHV.Story.Flag.HBCT.SocialSupport.SSRoomEmotionalComplete"),
        "informational_room_flag": tag("VHV.Story.Flag.HBCT.SocialSupport.SSRoomInformationalComplete"),
        "instrumental_room_flag": tag("VHV.Story.Flag.HBCT.SocialSupport.SSRoomInstrumentalComplete"),
        "appraisal_room_flag": tag("VHV.Story.Flag.HBCT.SocialSupport.SSRoomAppraisalComplete"),
        "rooms_complete_scene": ambient["rooms_complete"],
        "network_ready_scene": ambient["network_ready"],
    })

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save {}".format(MAP_PATH))

    return {
        "instructor": instructor,
        "chai": chai,
        "wife": wife,
        "daughter": daughter,
        "neighbor": neighbor,
        "nurse": nurse,
        "ambient": ambient,
        "locations": locations,
    }


OBJECTIVE_IDS = [
    "O01_ObserveVillageSupport",
    "O02_ReflectOnSupport",
    "O03_ExploreSupportHouse",
    "O04_MeetUncleChai",
    "O05_FindChaiSupporters",
    "O06_BuildSupportNetwork",
    "O07_WatchSupportInAction",
    "O08_WalkWithChai",
    "O09_SupportDisruption",
    "O10_FindBackupSupport",
    "O11_OneMonthLater",
    "O12_ProvideAppraisalSupport",
    "O13_FinalDebrief",
]

TRIGGER_NAMES = [
    "HBCT_SS_Trigger_ObserveMeal",
    "HBCT_SS_Trigger_ObserveWalkingPartner",
    "HBCT_SS_Trigger_ObserveEncouragement",
    "HBCT_SS_Trigger_ObserveHealthGuidance",
    "HBCT_SS_Trigger_InstructorCatchup",
    "HBCT_SS_Trigger_RoomEmotional",
    "HBCT_SS_Trigger_RoomInformational",
    "HBCT_SS_Trigger_RoomInstrumental",
    "HBCT_SS_Trigger_RoomAppraisal",
    "HBCT_SS_Trigger_ChaiIntro",
    "HBCT_SS_Trigger_ChaiWife",
    "HBCT_SS_Trigger_ChaiDaughter",
    "HBCT_SS_Trigger_ChaiNeighbor",
    "HBCT_SS_Trigger_ChaiNurse",
    "HBCT_SS_Trigger_NetworkBoard",
    "HBCT_SS_Trigger_PlanInAction",
    "HBCT_SS_Trigger_WalkStart",
    "HBCT_SS_Trigger_Disruption",
    "HBCT_SS_Trigger_BackupWife",
    "HBCT_SS_Trigger_OneMonthLater",
    "HBCT_SS_Trigger_FinalDebrief",
]


def quest_definition(arc, quest_id):
    return next((item for item in arc.get_editor_property("quests")
                 if str(item.get_editor_property("quest_id")) == quest_id), None)


def objective_definition(quest, objective_id):
    return next((item for item in quest.get_editor_property("objectives")
                 if str(item.get_editor_property("objective_id")) == objective_id), None)


def validate_authored_data(level, conversations, ambient_assets, arc):
    quest4 = quest_definition(arc, QUEST_ID)
    if not quest4:
        raise RuntimeError("Quest 4 definition was not added")
    actual_ids = [str(item.get_editor_property("objective_id"))
                  for item in quest4.get_editor_property("objectives")]
    if actual_ids != OBJECTIVE_IDS:
        raise RuntimeError("Quest 4 objective sequence is incorrect: {}".format(actual_ids))

    quests = list(arc.get_editor_property("quests"))
    if [str(item.get_editor_property("quest_id")) for item in quests[:4]] != [
            "Q_HBCT_00_INTRO", "Q_HBCT_01_MOTIVATION",
            "Q_HBCT_02_GOAL_SETTING", "Q_HBCT_03_ROLE_MODEL"]:
        raise RuntimeError("Quest 0-3 ordering changed")

    o01 = objective_definition(quest4, "O01_ObserveVillageSupport")
    o03 = objective_definition(quest4, "O03_ExploreSupportHouse")
    o05 = objective_definition(quest4, "O05_FindChaiSupporters")
    if len(o01.get_editor_property("completion_conditions").get_editor_property("conditions")) != 4:
        raise RuntimeError("O01 must require four independently persisted observations")
    if len(o03.get_editor_property("completion_conditions").get_editor_property("conditions")) != 4:
        raise RuntimeError("O03 must require four independently persisted room classifications")
    if len(o05.get_editor_property("completion_conditions").get_editor_property("conditions")) != 4:
        raise RuntimeError("O05 must require four independently persisted supporters")
    if len(o05.get_editor_property("contextual_conversations")) != 4:
        raise RuntimeError("O05 must expose four non-linear contextual conversations")

    o04 = objective_definition(quest4, "O04_MeetUncleChai")
    o04_conditions = list(o04.get_editor_property(
        "activation_conditions").get_editor_property("conditions"))
    if len(o04_conditions) != 1 or len(o04.get_editor_property("activation_npc_readiness")) != 0:
        raise RuntimeError("O04 must wait on the persisted Quest-4 Chai staging condition")
    if str(unreal.GameplayTagLibrary.get_tag_name(
            o04.get_editor_property("participant_tag"))) != "VHV.Participant.UncleChai":
        raise RuntimeError("O04 must target the existing UncleChai participant")
    if o04.get_editor_property("auto_start"):
        raise RuntimeError("O04 must remain a contextual E interaction")

    o08 = objective_definition(quest4, "O08_WalkWithChai")
    readiness = list(o08.get_editor_property("activation_npc_readiness"))
    stages = list(o08.get_editor_property("npc_move_stages"))
    if len(readiness) != 2 or len(stages) != 2:
        raise RuntimeError("O08 must have two readiness moves and two paired travel stages")
    for stage in stages:
        moves = list(stage.get_editor_property("moves"))
        if len(moves) != 2:
            raise RuntimeError("Every O08 stage must start two walkers together")
        if any(abs(float(move.get_editor_property("move_speed_override")) - 180.0) > 0.01
               for move in moves):
            raise RuntimeError("O08 walk speed must be 180 cm/s")
    if not o08.get_editor_property("complete_after_npc_move_stages"):
        raise RuntimeError("O08 must complete only after both End arrivals")

    o10 = objective_definition(quest4, "O10_FindBackupSupport")
    if len(o10.get_editor_property("contextual_conversations")) != 1:
        raise RuntimeError("O10 backup support must be solved through Chai's Wife in the world")
    if len(o10.get_editor_property("npc_move_stages")) != 2:
        raise RuntimeError("O10 must stage Chai and Wife, then run the paired backup walk")

    topics = list(level.get_editor_property("day_data").get_editor_property("topics"))
    social_topics = [item for item in topics
                     if str(item.get_editor_property("topic_id")) == TOPIC_ID]
    if len(social_topics) != 1:
        raise RuntimeError("Expected exactly one Quest 4 textbook topic")
    activities = list(social_topics[0].get_editor_property("activities"))
    activities_by_tag = {
        str(unreal.GameplayTagLibrary.get_tag_name(item.get_editor_property("activity_tag"))): item
        for item in activities
    }
    room_answers = {
        "VHV.Activity.HBCT.SocialSupport.RoomEmotional": 0,
        "VHV.Activity.HBCT.SocialSupport.RoomInformational": 1,
        "VHV.Activity.HBCT.SocialSupport.RoomInstrumental": 2,
        "VHV.Activity.HBCT.SocialSupport.RoomAppraisal": 3,
    }
    expected_options = ["Emotional", "Informational", "Instrumental", "Appraisal"]
    for activity_tag, correct_index in room_answers.items():
        room_activity = activities_by_tag.get(activity_tag)
        if not room_activity or room_activity.get_editor_property("activity_type") != unreal.TextbookActivityType.SINGLE_CHOICE:
            raise RuntimeError("{} must be an existing SingleChoice activity".format(activity_tag))
        question = room_activity.get_editor_property("question")
        options = list(question.get_editor_property("options"))
        if [str(option.get_editor_property("option_text")) for option in options] != expected_options:
            raise RuntimeError("{} has incorrect support-type options".format(activity_tag))
        correct = [index for index, option in enumerate(options)
                   if option.get_editor_property("is_correct")]
        if correct != [correct_index]:
            raise RuntimeError("{} has incorrect answer mapping".format(activity_tag))
        if len(room_activity.get_editor_property("success_effects")) != 1:
            raise RuntimeError("{} must set exactly one room completion flag".format(activity_tag))
    network = next(item for item in activities
                   if item.get_editor_property("presentation_style") ==
                   unreal.VHVActivityPresentationStyle.SOCIAL_SUPPORT_NETWORK)
    pairs = [(str(item.get_editor_property("left_text")),
              str(item.get_editor_property("right_text")))
             for item in network.get_editor_property("matching_pairs")]
    expected_pairs = [
        ("Chai's Wife", "Prepare healthier meals"),
        ("Chai's Daughter", "Encourage / remind progress"),
        ("Walking Neighbor", "Join evening walking"),
        ("Clinic Nurse / VHV", "Provide health information"),
    ]
    if pairs != expected_pairs:
        raise RuntimeError("Support Network mappings changed: {}".format(pairs))

    month = conversations["DA_Conversation_HBCT_SocialSupport_OneMonthLater"]
    month_text = " ".join(str(node.get_editor_property("text"))
                          for node in month.get_editor_property("conversation").get_editor_property("nodes"))
    if "180" not in month_text or "140" not in month_text or "two kilograms" not in month_text:
        raise RuntimeError("One-month payoff values must remain 180 to 140 and two kilograms")
    if len(conversations) != 9 or len(ambient_assets) != 14:
        raise RuntimeError("Unexpected Quest 4 asset counts")


def validate_map(authored):
    all_actors = actors().get_all_level_actors()
    labels = [actor.get_actor_label() for actor in all_actors]
    missing = [label for label in TRIGGER_NAMES if label not in labels]
    if missing:
        raise RuntimeError("Missing Quest 4 triggers: {}".format(missing))

    triggers = [find_actor(label) for label in TRIGGER_NAMES]
    for trigger in triggers:
        if str(trigger.get_editor_property("required_active_quest_id")) != QUEST_ID:
            raise RuntimeError("{} is not Quest-4-gated".format(trigger.get_actor_label()))
        if not str(trigger.get_editor_property("required_active_objective_id")):
            raise RuntimeError("{} has no objective gate".format(trigger.get_actor_label()))
        extent = trigger.get_editor_property("box_component").get_unscaled_box_extent()
        if extent.x < 200.0 or extent.y < 200.0 or extent.z < 140.0:
            raise RuntimeError("{} trigger extent is too small".format(trigger.get_actor_label()))

    for room_label in (
            "HBCT_SS_Trigger_RoomEmotional", "HBCT_SS_Trigger_RoomInformational",
            "HBCT_SS_Trigger_RoomInstrumental", "HBCT_SS_Trigger_RoomAppraisal"):
        if find_actor(room_label).get_editor_property("trigger_world_action_once"):
            raise RuntimeError("{} must allow a room retry after an incorrect activity".format(room_label))

    chai_intro = find_actor("HBCT_SS_Trigger_ChaiIntro")
    chai_intro_location = chai_intro.get_actor_location()
    if abs(chai_intro_location.x - 7200.0) > 0.1 or abs(chai_intro_location.y - 1800.0) > 0.1:
        raise RuntimeError("ChaiIntro trigger moved away from the authored O04 staging point")

    center_pairs = {
        "HBCT_SS_Trigger_ObserveMeal": "HBCT_SS_NPC_MealPatient",
        "HBCT_SS_Trigger_ObserveWalkingPartner": "HBCT_SS_NPC_ObservationWalker",
        "HBCT_SS_Trigger_ObserveEncouragement": "HBCT_SS_NPC_TiredVillager",
        "HBCT_SS_Trigger_ObserveHealthGuidance": "HBCT_SS_NPC_GuidanceNurse",
        "HBCT_SS_Trigger_RoomEmotional": "HBCT_SS_NPC_RoomEmotionalPatient",
        "HBCT_SS_Trigger_RoomInformational": "HBCT_SS_NPC_RoomInformationalVHV",
        "HBCT_SS_Trigger_RoomInstrumental": "HBCT_SS_NPC_RoomInstrumentalPatient",
        "HBCT_SS_Trigger_RoomAppraisal": "HBCT_SS_NPC_RoomAppraisalPatient",
        "HBCT_SS_Trigger_ChaiWife": "HBCT_SS_NPC_ChaiWife",
        "HBCT_SS_Trigger_ChaiDaughter": "HBCT_SS_NPC_ChaiDaughter",
        "HBCT_SS_Trigger_ChaiNeighbor": "HBCT_SS_NPC_ChaiWalkingNeighbor",
        "HBCT_SS_Trigger_ChaiNurse": "HBCT_SS_NPC_ChaiClinicNurse",
    }
    for trigger_label, npc_label in center_pairs.items():
        trigger_location = find_actor(trigger_label).get_actor_location()
        npc_location = find_actor(npc_label).get_actor_location()
        if abs(trigger_location.x - npc_location.x) > 0.1 or abs(trigger_location.y - npc_location.y) > 0.1:
            raise RuntimeError("{} is not centered on {}".format(npc_label, trigger_label))

    managed = [actor for actor in all_actors if actor.get_actor_label().startswith(PREFIX)]
    for actor in managed:
        location = actor.get_actor_location()
        if abs(location.x) < 0.1 and abs(location.y) < 0.1:
            raise RuntimeError("{} was placed at world origin".format(actor.get_actor_label()))
        if location.x < -100.0 or location.x > 18600.0 or location.y < 900.0 or location.y > 2700.0:
            raise RuntimeError("{} is outside the Quest 4 staging strip: {}".format(
                actor.get_actor_label(), location))

    participant_tags = [
        "UncleChai", "ChaiWife", "ChaiDaughter",
        "ChaiWalkingNeighbor", "ChaiClinicNurse"]
    for participant in participant_tags:
        matches = [actor for actor in all_actors if participant_id(actor) == participant]
        if len(matches) != 1:
            raise RuntimeError("Expected one {}, found {}".format(participant, len(matches)))

    chai = next(actor for actor in all_actors if participant_id(actor) == "UncleChai")
    if not chai.get_npc_interaction_component() or not chai.get_dialogue_component():
        raise RuntimeError("Existing UncleChai lacks the production interaction/dialogue components")
    if not chai.get_npc_interaction_component().get_editor_property("interaction_enabled"):
        raise RuntimeError("Existing UncleChai interaction is disabled")

    semantic_leaves = []
    for actor in managed:
        if hasattr(actor, "get_editor_property"):
            try:
                location_tag = str(actor.get_editor_property("location_tag"))
            except Exception:
                continue
            if location_tag:
                semantic_leaves.append(location_tag.rsplit(".", 1)[-1])
    duplicates = sorted({leaf for leaf in semantic_leaves if semantic_leaves.count(leaf) > 1})
    if duplicates:
        raise RuntimeError("Ambiguous Quest 4 location leaves: {}".format(duplicates))

    for scene_key in ("walking", "walk", "backup"):
        scene = authored["ambient"][scene_key]
        if not scene.get_editor_property("preserve_participant_behavior"):
            raise RuntimeError("{} must preserve movement during speech".format(scene.get_actor_label()))


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
    validate_authored_data(level, conversations, ambient_assets, arc)

    for asset in [level] + list(conversations.values()) + list(ambient_assets.values()) + [arc]:
        hbct.save_asset(asset)
    authored = configure_map(ambient_assets)
    validate_map(authored)
    unreal.log("{} Authored 13 objectives, 8 activities, {} conversations, {} ambient sequences, and {} managed map actors.".format(
        LOG, len(conversations), len(ambient_assets),
        len([actor for actor in actors().get_all_level_actors()
             if actor.get_actor_label().startswith(PREFIX)])))


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
