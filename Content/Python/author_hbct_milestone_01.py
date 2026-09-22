"""Author the first production HBCT story milestone.

This idempotent editor script owns only production content below
/Game/VHV_Stuff/HBCT, HBCT-labelled actors in Lvl_Village_Blockout, and the
production quest defaults on BP_ThirdPersonPlayerController.
"""

import traceback
import unreal


ROOT = "/Game/VHV_Stuff/HBCT"
ACTIVITY_DIR = ROOT + "/Activities"
CONVERSATION_DIR = ROOT + "/Dialogue"
AMBIENT_DIR = ROOT + "/Ambient"
QUEST_DIR = ROOT + "/Quests"
MAP_PATH = "/Game/VHV_Stuff/Maps/Lvl_Village_Blockout"
NPC_BP_PATH = "/Game/VHV_Stuff/NPC/BP_VHVNPCBase"
CONTROLLER_BP_PATH = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"
LOG = "[HBCT Milestone 01]"


def struct(struct_type, **properties):
    value = struct_type()
    value.set_editor_properties(properties)
    return value


def tag(name):
    value = unreal.GameplayTag()
    if not value.import_text(name):
        raise RuntimeError("Gameplay Tag is not registered: {}".format(name))
    return value


def ensure_directory(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        if not unreal.EditorAssetLibrary.make_directory(path):
            raise RuntimeError("Could not create {}".format(path))


def load_or_create(directory, name, asset_type):
    ensure_directory(directory)
    path = "{}/{}".format(directory, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset or asset.get_class() != asset_type.static_class():
            actual = asset.get_class().get_name() if asset else "unloadable"
            raise RuntimeError("{} is {}; expected {}".format(
                path, actual, asset_type.static_class().get_name()))
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", asset_type)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, directory, asset_type, factory)
        if not asset:
            raise RuntimeError("Could not create {}".format(path))
    asset.modify()
    return asset


def save_asset(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Could not save {}".format(asset.get_path_name()))


def effect(flag_name):
    return struct(
        unreal.VHVStoryEffect,
        effect_type=unreal.VHVStoryEffectType.SET_FLAG,
        state_tag=tag(flag_name),
        value=0)


def teaching(title, content, takeaways, category=None, media_texture=None, media_caption=""):
    if category is None:
        category = unreal.TextbookTeachingCategory.LESSON
    return struct(
        unreal.TeachingContent,
        category=category,
        title=title,
        content=content,
        key_takeaways=takeaways,
        media=[],
        media_texture=media_texture,
        media_caption=media_caption)


def attempt_policy(first_hint, final_explanation, max_attempts=2):
    return struct(
        unreal.TextbookAttemptPolicy,
        enabled=True,
        max_attempts=max_attempts,
        first_incorrect_hint=first_hint,
        final_incorrect_explanation=final_explanation)


def question(question_id, prompt, options, correct_index):
    return struct(
        unreal.QuestionData,
        question_id=question_id,
        question_text=prompt,
        options=[
            struct(unreal.QuestionOption, option_text=text, is_correct=index == correct_index)
            for index, text in enumerate(options)
        ])


def observation_media(media_id, description):
    return struct(
        unreal.TextbookMediaReference,
        media_id=media_id,
        media_type=unreal.TextbookMediaType.DIAGRAM,
        description=description,
        required=False)


def evidence_card(text, correct):
    return struct(unreal.ObservationEvidenceCard, text=text, correct=correct)


def takeaway_card(title, text):
    return struct(unreal.ObservationTakeawayCard, title=title, text=text)


def evidence_tagging(stage1_prompt, evidence_cards, stage2_prompt, obstacle_cards, takeaways):
    return struct(
        unreal.ObservationEvidenceTaggingConfig,
        use_evidence_tagging=True,
        stage1_prompt=stage1_prompt,
        evidence_cards=evidence_cards,
        stage2_prompt=stage2_prompt,
        obstacle_cards=obstacle_cards,
        takeaway_cards=takeaways)


def activity(tag_name, title, activity_type, prompt, teaching_data, **properties):
    values = {
        "activity_tag": tag(tag_name),
        "activity_title": title,
        "activity_type": activity_type,
        "narrative_context": "",
        "prompt_text": prompt,
        "correct_feedback": "That is the key idea.",
        "incorrect_feedback": "Look for what helps the person find their own reason and next step.",
        "partial_feedback": "You identified part of the idea.",
        "teaching": teaching_data,
        "hints": [],
        "media": [],
        "require_correct_answer_to_advance": False,
        "success_effects": [],
        "failure_effects": [],
    }
    values.update(properties)
    return struct(unreal.TextbookActivityData, **values)


def build_activities():
    observation = unreal.TextbookActivityType.OBSERVATION
    single = unreal.TextbookActivityType.SINGLE_CHOICE
    matching = unreal.TextbookActivityType.MATCHING
    why_behavior_media = unreal.load_asset(
        "/Game/VHV_Stuff/UI/Teaching/Media/T_UI_HBCT_WhyBehaviorChange")
    if not why_behavior_media:
        raise RuntimeError("Missing Why Behavior Change teaching media")
    return [
        activity(
            "VHV.Activity.HBCT.Intro.MarketObservation",
            "Market Observation",
            observation,
            "Tag the evidence you noticed in the market.",
            teaching(
                "Knowledge Is Not Always Action",
                "People may understand a health risk and still repeat a familiar behavior. Habits, convenience, feelings, confidence, and social expectations can all become obstacles.",
                ["Notice the behavior.", "Listen for awareness of risk.", "Consider the obstacle before giving advice."]),
            narrative_context="Recall the short scenes you observed in the market.",
            evidence_tagging=evidence_tagging(
                "What unhealthy behaviors did you notice?",
                [
                    evidence_card("Sugary drinks", True),
                    evidence_card("Fried or junk food", True),
                    evidence_card("Smoking", True),
                    evidence_card("Taking a motorcycle for a very short trip", True),
                    evidence_card("Choosing unhealthy food despite saying they want better health", True),
                    evidence_card("Buying fresh vegetables for dinner", False),
                    evidence_card("Walking home after shopping", False),
                ],
                "What might make someone continue an unhealthy behavior even when they know the risk?",
                [
                    evidence_card("Habit", True),
                    evidence_card("Convenience", True),
                    evidence_card("Low readiness to change", True),
                    evidence_card("Social pressure", True),
                    evidence_card("Low confidence", True),
                    evidence_card("Having plenty of free time", False),
                ],
                [
                    takeaway_card(
                        "KNOWING IS NOT ALWAYS DOING",
                        "Understanding a health risk does not automatically change behavior."),
                    takeaway_card(
                        "CHANGE HAS OBSTACLES",
                        "Habits, convenience, readiness, confidence and social influences can make change difficult."),
                    takeaway_card(
                        "VHVs HELP TURN INTENTION INTO ACTION",
                        "Support, guidance and behavior-change techniques can help people build sustainable healthier behavior."),
                ])),
        activity(
            "VHV.Activity.HBCT.Intro.IntroLesson",
            "Why Behavior Change Is Difficult",
            observation,
            "Review what helps a person move from knowing to doing.",
            teaching(
                "WHY BEHAVIOR CHANGE IS DIFFICULT",
                "",
                [],
                unreal.TextbookTeachingCategory.LESSON,
                why_behavior_media)),
        activity(
            "VHV.Activity.HBCT.Motivation.TechniqueTitle",
            "TECHNIQUE 1",
            observation,
            "Continue to begin the first behavior-change technique.",
            teaching(
                "TECHNIQUE 1 — BUILDING MOTIVATION",
                "Before someone changes behavior, something has to move inside them first.",
                ["Discover what matters.", "Explore obstacles.", "Offer choices and make a small plan."],
                unreal.TextbookTeachingCategory.TECHNIQUE)),
        activity(
            "VHV.Activity.HBCT.Motivation.PrasertObservation",
            "Observe Uncle Prasert",
            observation,
            "What health-risk habits, signs of low readiness, and important life factors did you notice?",
            teaching(
                "Observe Before Advising",
                "Prasert's sweet drinks, snack packets, television time, little movement, tiredness, and 'tomorrow' language suggest risk and low readiness. The first task is to understand what might matter enough to make change worth the effort.",
                ["Notice habits without judging.", "Listen for readiness.", "Look for personally important reasons to change."]),
            media=[
                observation_media("HBCT_PRASERT_HABITS", "Sweet drinks, snack packets, television, and little movement."),
                observation_media("HBCT_PRASERT_READINESS", "'I am too tired' and 'I will start tomorrow' signal hesitation."),
            ]),
        activity(
            "VHV.Activity.HBCT.Motivation.MotivationLesson",
            "What Is Motivation Building?",
            observation,
            "Consider how a VHV can help without taking away the person's decision.",
            teaching(
                "WHAT IS MOTIVATION BUILDING?",
                "Help the person move from hesitation toward action without taking away their freedom to decide.",
                [
                    "INFORM AND SUPPORT — Give useful information, encouragement, and support.",
                    "GUIDE, DO NOT FORCE — Do not simply command or force a decision.",
                    "ASK WHAT MATTERS — Connect health change to an important life goal.",
                    "APPRECIATE — Recognize intention and effort through supportive conversation.",
                ],
                unreal.TextbookTeachingCategory.KEY_IDEA)),
        activity(
            "VHV.Activity.HBCT.Motivation.DemoReflection",
            "Reflect on the Demonstration",
            single,
            "Which conversation better helps the person become ready to change?",
            teaching(
                "Readiness Grows Through Partnership",
                "The motivational conversation protects freedom to decide, stimulates an internal desire to act, and strengthens the relationship between adviser and person.",
                ["Ask before directing.", "Respect choice.", "Build a working relationship."],
                unreal.TextbookTeachingCategory.REFLECTION),
            question=question(
                "HBCT_MOTIVATION_DEMO",
                "Which conversation better helps the person become ready to change?",
                [
                    "The directive conversation that tells the person exactly what to do.",
                    "The motivational conversation that asks what matters and how health affects it.",
                    "Both conversations are equally effective because they mention health.",
                ], 1),
            correct_feedback="Exactly. Asking what matters helps motivation come from the person's own goals instead of from external pressure.",
            attempt_policy=attempt_policy(
                "Think about which approach allows the person to express what matters to them and make their own decision.",
                "Motivational conversation is more effective because it involves the person in their own health problem, helps them connect change to something personally important, and supports their freedom to decide. Simply directing them what to do can create resistance.")),
        activity(
            "VHV.Activity.HBCT.Motivation.Methods",
            "Appreciate, Offer Choices, Make a Plan",
            matching,
            "Match each example to the motivation-building method it demonstrates.",
            teaching(
                "MOTIVATION-BUILDING METHODS",
                "Use these methods where they help the person reflect, choose, and act.",
                [
                    "APPRECIATE — Recognize intention, effort, and successful methods.",
                    "ASK — Help the person reflect on what matters and how current behavior affects it.",
                    "OFFER CHOICES — Give feasible options the person can own.",
                    "MAKE A SMALL PLAN — Agree on a clear, realistic next action.",
                ],
                unreal.TextbookTeachingCategory.LESSON),
            matching_pairs=[
                struct(unreal.MatchingPair, left_text="What is important in your life?", right_text="Motivational question"),
                struct(unreal.MatchingPair, left_text="Losing two kilograms took determination.", right_text="Appreciate effort"),
                struct(unreal.MatchingPair, left_text="Walk to the market instead of using transport.", right_text="Small feasible plan"),
            ],
            correct_feedback="Correct. Appreciation, choices, and a small plan help the person own the change."),
        activity(
            "VHV.Activity.HBCT.Motivation.MaliWhatMatters",
            "Find What Matters to Mali",
            single,
            "Which detail gives Mali a personal reason to reduce sugar?",
            teaching(
                "A Personal Anchor",
                "Mali wants to remain strong, walk independently, and attend her grandchild's graduation. That future matters to her and can become her reason for change.",
                ["Use the person's own hopes as the anchor."]),
            question=question(
                "HBCT_MALI_MOTIVATION",
                "Which detail gives Mali a personal reason to reduce sugar?",
                [
                    "The VHV says that sugar is unhealthy.",
                    "She wants to stay independent and attend her grandchild's graduation.",
                    "Her blood glucose result has a number on it.",
                ], 1),
            attempt_policy=attempt_policy(
                "Look for the future Mali says she personally wants.",
                "Mali's wish to stay independent and attend her grandchild's graduation is the personal goal that can support change.")),
        activity(
            "VHV.Activity.HBCT.Motivation.MaliObstacle",
            "Understand Mali's Obstacle",
            single,
            "What is the main obstacle behind Mali accepting sweets and iced milk tea?",
            teaching(
                "Listen Beneath the Habit",
                "Mali worries that refusing treats from her children will hurt their feelings. Empathy makes room to discuss the social obstacle rather than blaming her.",
                ["Name the obstacle with empathy.", "Do not confuse concern for family with unwillingness to change."]),
            question=question(
                "HBCT_MALI_OBSTACLE",
                "What is the main obstacle behind Mali accepting sweets and iced milk tea?",
                [
                    "She has never heard that sugar can affect health.",
                    "She is worried that refusing her children's gifts will hurt their feelings.",
                    "There are no alternatives available in the village.",
                ], 1),
            attempt_policy=attempt_policy(
                "Listen for the feeling that makes refusing the treats difficult.",
                "Mali understands the health risk, but she worries that refusing her children's gifts will hurt their feelings. The obstacle is social and emotional.")),
        activity(
            "VHV.Activity.HBCT.Motivation.SupportiveInformation",
            "Offer Supportive Information",
            single,
            "Which response offers useful choices without ordering Mali?",
            teaching(
                "Offer Feasible Choices",
                "Useful information becomes supportive when it fits the obstacle and leaves the decision with Mali.",
                ["Explain the health reason to her children.", "Ask for less-sweet snacks.", "Ask for unsweetened herbal drinks."]),
            question=question(
                "HBCT_MALI_SUPPORT",
                "Which response offers useful choices without ordering Mali?",
                [
                    "Tell your children never to bring you anything again.",
                    "You could explain the health reason, then ask for less-sweet snacks or unsweetened herbal drinks. Which feels possible?",
                    "You must refuse every drink from now on.",
                ], 1),
            correct_feedback="Correct. The response gives relevant options and lets Mali choose.",
            attempt_policy=attempt_policy(
                "Choose the response that offers realistic alternatives and leaves the decision with Mali.",
                "Supportive information gives feasible choices that fit Mali's obstacle. Orders remove ownership and may create resistance.")),
        activity(
            "VHV.Activity.HBCT.Motivation.MaliPlanSummary",
            "Summarize Mali's Plan",
            single,
            "Which summary best links what matters, the obstacle, and a feasible next step?",
            teaching(
                "A Clear, Owned Plan",
                "A useful summary repeats the person's own reason, acknowledges the obstacle, and names the small action they chose.",
                ["What matters: strength and independence.", "Obstacle: concern for her children's feelings.", "Next step: explain why and request a lower-sugar alternative."]),
            question=question(
                "HBCT_MALI_PLAN",
                "Which summary best links what matters, the obstacle, and a feasible next step?",
                [
                    "You need to stop sugar because I said so.",
                    "You want to stay strong for your grandchild's graduation. You will explain that to your children and ask for unsweetened herbal drinks first.",
                    "Try to be healthier whenever you can.",
                ], 1),
            attempt_policy=attempt_policy(
                "Choose the summary that links Mali's own goal to the specific step she selected.",
                "A useful summary names what matters, acknowledges the obstacle, and repeats a clear next action that Mali chose herself.")),
        activity(
            "VHV.Activity.HBCT.Motivation.QuestComplete",
            "QUEST COMPLETE",
            observation,
            "Continue to close Building Motivation.",
            teaching(
                "QUEST COMPLETE — BUILDING MOTIVATION",
                "You observed readiness, discovered a personal reason, explored an obstacle, offered useful choices, and summarized a feasible plan.",
                ["Guide rather than command.", "Find what matters.", "End with a small plan the person owns."])),
        activity(
            "VHV.Activity.HBCT.Motivation.GoalSettingTeaser",
            "NEXT TECHNIQUE",
            observation,
            "Continue to preview the next technique.",
            teaching(
                "TECHNIQUE 2 — GOAL-SETTING FOR HEALTH BEHAVIOR CHANGE",
                "Coming next: turn motivation into a clear health-behavior goal.",
                ["Technique 2 is a teaser only in this milestone."])),
    ]


def configure_level(level):
    topic = struct(
        unreal.TopicData,
        topic_id="HBCT_M01_BUILDING_MOTIVATION",
        topic_title="Arrival, Observation, and Building Motivation",
        narrative_introduction="Observe village life, then practice helping a person discover their own reason for change.",
        activities=build_activities())
    day = struct(
        unreal.DayData,
        day_number=1,
        day_title="First Day in the Village",
        narrative_role="Production HBCT prologue and Technique 1",
        introduction_text="Meet the Instructor and begin by observing real life.",
        topics=[topic],
        completion_summary="Building Motivation complete.",
        completion_takeaways=[
            "Motivation comes from what matters personally.",
            "A VHV guides rather than commands.",
            "A small feasible plan turns readiness toward action.",
        ])
    level.set_editor_properties({
        "level_number": 1,
        "level_title": "HBCT — Building Motivation",
        "level_description": "Production prologue, market observation, and Technique 1 content.",
        "day_data": day,
    })


def text_node(node_id, speaker_name, line, next_id="", speaker_tag="", checkpoint="", presentation=None):
    values = {
        "node_id": node_id,
        "node_type": unreal.VHVDialogueNodeType.TEXT,
        "speaker_name": speaker_name,
        "text": line,
        "choices": [],
        "next_node_id": next_id,
        "is_checkpoint": bool(checkpoint),
        "completion_effects": [],
    }
    if presentation is not None:
        values["presentation"] = presentation
    if speaker_tag:
        values["speaker_tag"] = tag(speaker_tag)
    if checkpoint:
        values["checkpoint_tag"] = tag(checkpoint)
    return struct(unreal.DialogueNode, **values)


def learning_node(node_id, activity_tag, next_id=""):
    return struct(
        unreal.DialogueNode,
        node_id=node_id,
        node_type=unreal.VHVDialogueNodeType.LEARNING_ACTIVITY,
        speaker_name="",
        text="",
        choices=[],
        next_node_id=next_id,
        is_checkpoint=False,
        linked_activity=struct(
            unreal.TextbookActivityReference,
            activity_tag=tag(activity_tag),
            topic_id="HBCT_M01_BUILDING_MOTIVATION",
            required_for_progress=True),
        completion_effects=[])


def set_conversation(asset, conversation_id, nodes):
    asset.set_editor_property("conversation", struct(
        unreal.DialogueConversation,
        conversation_id=conversation_id,
        start_node_id=nodes[0].get_editor_property("node_id"),
        nodes=nodes))


def create_conversations():
    specs = {}

    specs["DA_Conversation_HBCT_Intro"] = (
        "HBCT_Intro_Instructor",
        [
            text_node("Intro_01", "Instructor", "Welcome. I'm glad you're here. In this village, we don't just treat illness—we help people build better lives.", "Intro_02", "VHV.Participant.Instructor"),
            text_node("Intro_02", "Player", "I'm ready to learn.", "Intro_03"),
            text_node("Intro_03", "Instructor", "Good. Then let's begin by observing real life first. Before we talk about changing behavior, you need to see why change is difficult.", speaker_tag="VHV.Participant.Instructor", checkpoint="VHV.Checkpoint.HBCT.Intro.InstructorMet"),
        ])

    specs["DA_Conversation_HBCT_MarketDebrief"] = (
        "HBCT_Intro_MarketDebrief",
        [
            text_node("Market_01", "Instructor", "Tell me what you noticed at the market.", "Market_Observe", "VHV.Participant.Instructor"),
            learning_node("Market_Observe", "VHV.Activity.HBCT.Intro.MarketObservation", "Market_02"),
            text_node("Market_02", "Instructor", "You've seen it now. Most people are not ignorant. Many already know what harms their health. The problem is not only knowledge—it's action.", "Market_03", "VHV.Participant.Instructor"),
            text_node("Market_03", "Player", "So behavior change is harder than just giving advice?", "Market_04"),
            text_node("Market_04", "Instructor", "Exactly. Old habits are familiar. New habits take effort, confidence, support, and personal responsibility. That's where a VHV becomes important.", speaker_tag="VHV.Participant.Instructor", checkpoint="VHV.Checkpoint.HBCT.Intro.MarketDebrief"),
        ])

    specs["DA_Conversation_HBCT_IntroLesson"] = (
        "HBCT_Intro_LessonSequence",
        [
            learning_node("IntroLesson_01", "VHV.Activity.HBCT.Intro.IntroLesson", "IntroLesson_02"),
            text_node("IntroLesson_02", "Instructor", "Before someone changes behavior, something has to move inside them first. That is where our first technique begins.", "IntroLesson_03", "VHV.Participant.Instructor"),
            learning_node("IntroLesson_03", "VHV.Activity.HBCT.Motivation.TechniqueTitle"),
        ])

    specs["DA_Conversation_HBCT_LearnMotivation"] = (
        "HBCT_Motivation_Learn",
        [
            learning_node("Learn_01", "VHV.Activity.HBCT.Motivation.PrasertObservation", "Learn_02"),
            text_node("Learn_02", "Player", "So I shouldn't just lecture him?", "Learn_03",
                      presentation=unreal.VHVDialoguePresentation.THOUGHT),
            text_node("Learn_03", "Instructor", "Right. Telling someone what to do is not the same as helping them want to do it.", "Learn_04", "VHV.Participant.Instructor"),
            learning_node("Learn_04", "VHV.Activity.HBCT.Motivation.MotivationLesson"),
        ])

    specs["DA_Conversation_HBCT_MaliMotivation"] = (
        "HBCT_Motivation_MaliMotivation",
        [
            text_node("MaliMot_01", "Player", "Aunt Mali, your health check shows that your blood glucose is beginning to rise. May we talk about the sweet foods and drinks you enjoy?", "MaliMot_02"),
            text_node("MaliMot_02", "Aunt Mali", "I know I should be careful. I do love sweets and iced milk tea.", "MaliMot_03", "VHV.Participant.AuntMali"),
            text_node("MaliMot_03", "Player", "If you can reduce sugar, how would you like your health to be five years from now?", "MaliMot_04"),
            text_node("MaliMot_04", "Aunt Mali", "I want to remain strong and walk independently. Most of all, I want to attend my grandchild's graduation.", "MaliMot_05", "VHV.Participant.AuntMali"),
            learning_node("MaliMot_05", "VHV.Activity.HBCT.Motivation.MaliWhatMatters", "MaliMot_06"),
            text_node("MaliMot_06", "Player", "Staying strong for that graduation gives us something important to work toward.", speaker_tag="", checkpoint="VHV.Checkpoint.HBCT.Motivation.MaliMotivation"),
        ])

    specs["DA_Conversation_HBCT_MaliObstacle"] = (
        "HBCT_Motivation_MaliObstacle",
        [
            text_node("MaliObs_01", "Player", "What makes reducing sweets difficult at home?", "MaliObs_02"),
            text_node("MaliObs_02", "Aunt Mali", "My children bring me sweets and iced milk tea. They mean well, and I worry that refusing them will hurt their feelings.", "MaliObs_03", "VHV.Participant.AuntMali"),
            text_node("MaliObs_03", "Player", "You value their kindness. It makes sense that you do not want a health change to feel like rejecting their care.", "MaliObs_04"),
            learning_node("MaliObs_04", "VHV.Activity.HBCT.Motivation.MaliObstacle", "MaliObs_05"),
            text_node("MaliObs_05", "Aunt Mali", "Yes. If they understood why, perhaps we could find another way.", speaker_tag="VHV.Participant.AuntMali", checkpoint="VHV.Checkpoint.HBCT.Motivation.MaliObstacle"),
        ])

    specs["DA_Conversation_HBCT_MaliSummary"] = (
        "HBCT_Motivation_MaliSummary",
        [
            text_node("MaliPlan_01", "Player", "You could explain the health reason to your children, ask for less-sweet snacks, or ask for unsweetened herbal drinks. Which feels possible first?", "MaliPlan_02"),
            text_node("MaliPlan_02", "Aunt Mali", "I will explain that I want to stay strong for the graduation, and ask them to bring unsweetened herbal drinks first.", "MaliPlan_03", "VHV.Participant.AuntMali"),
            learning_node("MaliPlan_03", "VHV.Activity.HBCT.Motivation.MaliPlanSummary", "MaliPlan_04"),
            text_node("MaliPlan_04", "Player", "That is a clear first step, and it respects both your health and your family's care.", checkpoint="VHV.Checkpoint.HBCT.Motivation.MaliPlan"),
        ])

    specs["DA_Conversation_HBCT_FinalDebrief"] = (
        "HBCT_Motivation_FinalDebrief",
        [
            text_node("Final_01", "Instructor", "Motivation grows from what matters personally. A VHV guides rather than commands.", "Final_02", "VHV.Participant.Instructor"),
            text_node("Final_02", "Instructor", "Discover the person's important life goal, identify obstacles, offer useful choices, and finish with a small feasible plan.", "Final_03", "VHV.Participant.Instructor"),
            text_node("Final_03", "Instructor", "The key is not to push people. The key is to help them discover why change matters to them.", "Final_04", "VHV.Participant.Instructor"),
            learning_node("Final_04", "VHV.Activity.HBCT.Motivation.QuestComplete", "Final_05"),
            learning_node("Final_05", "VHV.Activity.HBCT.Motivation.GoalSettingTeaser"),
        ])

    assets = {}
    for name, (conversation_id, nodes) in specs.items():
        asset = load_or_create(CONVERSATION_DIR, name, unreal.VHVConversationDataAsset)
        set_conversation(asset, conversation_id, nodes)
        assets[name] = asset
    return assets


def ambient_participant(slot_id, speaker_name):
    return struct(unreal.VHVAmbientParticipantDefinition, slot_id=slot_id, speaker_name=speaker_name)


def ambient_line(slot_id, line, duration=0.0, thought=False, delay=0.15, look_at=False):
    return struct(
        unreal.VHVAmbientSpeechLine,
        speaker_slot=slot_id,
        text=line,
        speech_type=(unreal.VHVAmbientSpeechType.THOUGHT if thought else unreal.VHVAmbientSpeechType.SPEECH),
        delay_before=delay,
        display_duration=duration,
        keep_previous_bubble_visible=False,
        look_at_other_participant=look_at)


def create_ambient_assets():
    specs = {
        "DA_Ambient_HBCT_MarketObservation": {
            "id": "HBCT_Market_Observation",
            "participants": [
                ambient_participant("Buyer", "MARKET SHOPPER"),
                ambient_participant("Vendor", "MARKET VENDOR"),
                ambient_participant("Smoker", "MIDDLE-AGED VILLAGER"),
                ambient_participant("SweetTea", "VILLAGER"),
                ambient_participant("Rider", "VILLAGER"),
                ambient_participant("JunkFood", "VILLAGER"),
            ],
            "lines": [
                ambient_line("Buyer", "One sweet iced drink—and a bag of fried snacks, please.", 3.0),
                ambient_line("Vendor", "Here you are. Extra syrup, as usual.", 2.5),
                ambient_line("Smoker", "I know I should stop... just not today.", 3.0, thought=True),
                ambient_line("SweetTea", "The clinic says my blood sugar is high, but I still want the sweet milk tea.", 4.0),
                ambient_line("Rider", "The stall is only down the road, but taking the motorcycle is easier.", 3.8, thought=True),
                ambient_line("JunkFood", "I really do want to be healthier... I'll take two snack bags anyway.", 3.8),
            ],
            "show_names": False,
        },
        "DA_Ambient_HBCT_PrasertObservation": {
            "id": "HBCT_Motivation_PrasertObservation",
            "participants": [ambient_participant("Prasert", "UNCLE PRASERT")],
            "lines": [
                ambient_line("Prasert", "Sweet tea, snacks, and television... that's enough for today.", 3.8),
                ambient_line("Prasert", "I'm too tired to exercise.", 2.8, thought=True),
                ambient_line("Prasert", "I'll start tomorrow.", 2.5),
            ],
            "show_names": True,
        },
        "DA_Ambient_HBCT_BadGoodDemo": {
            "id": "HBCT_Motivation_BadGoodDemo",
            "participants": [
                ambient_participant("DirectiveVHV", "VHV"),
                ambient_participant("DirectivePatient", "VILLAGER"),
                ambient_participant("MotivationalVHV", "VHV"),
                ambient_participant("MotivationalPatient", "VILLAGER"),
            ],
            "lines": [
                ambient_line("DirectiveVHV", "You must stop eating junk food.", 2.8),
                ambient_line("DirectiveVHV", "You need to exercise.", 2.4),
                ambient_line("DirectivePatient", "...All right.", 2.2),
                ambient_line("MotivationalVHV", "What is important in your life?", 2.8),
                ambient_line("MotivationalVHV", "How does your current health affect that?", 3.0),
                ambient_line("MotivationalPatient", "I want enough energy to work and play with my grandchildren. I suppose that gives me a reason to begin.", 4.8),
            ],
            "show_names": True,
        },
        "DA_Ambient_HBCT_SaengAppreciation": {
            "id": "HBCT_Motivation_SaengAppreciation",
            "participants": [
                ambient_participant("Saeng", "AUNT SAENG"),
                ambient_participant("VHV", "VHV"),
            ],
            "lines": [
                ambient_line("Saeng", "My weight has gone down by 2 kilograms.", 3.0, look_at=True),
                ambient_line("VHV", "Wow, it isn't easy to lose weight. How did you do it?", 3.6, look_at=True),
                ambient_line("VHV", "What have you been doing? It shows your determination.", 3.6, look_at=True),
                ambient_line("Saeng", "I walk with my neighbor after breakfast and choose smaller portions. Talking about it helps me see what worked.", 4.6, look_at=True),
            ],
            "show_names": True,
        },
    }
    assets = {}
    for name, data in specs.items():
        asset = load_or_create(AMBIENT_DIR, name, unreal.VHVAmbientConversationData)
        asset.set_editor_properties({
            "conversation_id": data["id"],
            "participants": data["participants"],
            "lines": data["lines"],
            "show_speaker_name": data["show_names"],
            "completion_effects": [],
        })
        assets[name] = asset
    return assets


def objective(objective_id, text, objective_type, flag="", **properties):
    values = {
        "objective_id": objective_id,
        "objective_text": text,
        "objective_type": objective_type,
        "completion_effects": [effect(flag)] if flag else [],
    }
    values.update(properties)
    return struct(unreal.VHVQuestObjectiveDefinition, **values)


def configure_arc(arc, level, conversations):
    q = unreal.VHVQuestObjectiveType
    start_action = tag("VHV.WorldAction.StartConversation")
    intro = [
        objective("O01_MeetInstructor", "Talk to the Instructor", q.TALK,
                  "VHV.Story.Flag.HBCT.Intro.InstructorMet",
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_Intro"],
                  entry_node_id="Intro_01", auto_start=False),
        objective("O02_ReachMarket", "Reach the market", q.REACH_LOCATION,
                  location_tag=tag("VHV.Location.HBCT.Market")),
        objective("O03_ObserveMarket", "Observe everyday choices in the market", q.WORLD_ACTION,
                  "VHV.Story.Flag.HBCT.Intro.MarketObserved",
                  world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.MarketObservation"),
                  world_action_tag=start_action),
        objective("O04_ReportToInstructor", "Report your observations to the Instructor", q.TALK,
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_MarketDebrief"],
                  entry_node_id="Market_01", auto_start=False),
        objective("O05_IntroLesson", "Learn why behavior change is difficult", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_IntroLesson"],
                  entry_node_id="IntroLesson_01", auto_start=True),
    ]
    motivation = [
        objective("O01_ReachPrasertHouse", "Go to Uncle Prasert's house", q.REACH_LOCATION,
                  location_tag=tag("VHV.Location.HBCT.PrasertHouse")),
        objective("O02_ObservePrasert", "Observe Uncle Prasert's habits and readiness", q.WORLD_ACTION,
                  "VHV.Story.Flag.HBCT.Motivation.PrasertObserved",
                  world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.Motivation.Prasert"),
                  world_action_tag=start_action),
        objective("O03_LearnMotivation", "Learn what motivation building means", q.CONVERSATION,
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_LearnMotivation"],
                  entry_node_id="Learn_01", auto_start=True),
        objective("O04_ObserveBadGoodDemo", "Observe directive and motivational conversations", q.WORLD_ACTION,
                  world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.Motivation.BadGoodDemo"),
                  world_action_tag=start_action),
        objective("O05_ReflectOnDemo", "Reflect on which conversation builds readiness", q.LEARNING_ACTIVITY,
                  activity_tag=tag("VHV.Activity.HBCT.Motivation.DemoReflection"), auto_start=True),
        objective("O06_VisitSaeng", "Visit Aunt Saeng", q.REACH_LOCATION,
                  location_tag=tag("VHV.Location.HBCT.SaengHouse")),
        objective("O07_ObserveAppreciation", "Observe appreciation and reflective questions", q.WORLD_ACTION,
                  "VHV.Story.Flag.HBCT.Motivation.SaengObserved",
                  world_action_receiver_tag=tag("VHV.WorldReceiver.HBCT.Motivation.Saeng"),
                  world_action_tag=start_action),
        objective("O08_LearnMotivationMethods", "Practice appreciation, choices, and small plans", q.LEARNING_ACTIVITY,
                  activity_tag=tag("VHV.Activity.HBCT.Motivation.Methods"), auto_start=True),
        objective("O09_ReachMaliHouse", "Go to Aunt Mali's house", q.REACH_LOCATION,
                  location_tag=tag("VHV.Location.HBCT.MaliHouse")),
        objective("O10_FindWhatMatters", "Discover what matters to Aunt Mali", q.TALK,
                  "VHV.Story.Flag.HBCT.Motivation.MaliMotivationFound",
                  participant_tag=tag("VHV.Participant.AuntMali"),
                  conversation=conversations["DA_Conversation_HBCT_MaliMotivation"],
                  entry_node_id="MaliMot_01", auto_start=False),
        objective("O11_FindObstacle", "Explore Aunt Mali's obstacle", q.TALK,
                  "VHV.Story.Flag.HBCT.Motivation.MaliObstacleFound",
                  participant_tag=tag("VHV.Participant.AuntMali"),
                  conversation=conversations["DA_Conversation_HBCT_MaliObstacle"],
                  entry_node_id="MaliObs_01", auto_start=False),
        objective("O12_OfferSupportiveInformation", "Offer useful choices without giving orders", q.LEARNING_ACTIVITY,
                  activity_tag=tag("VHV.Activity.HBCT.Motivation.SupportiveInformation"), auto_start=True),
        objective("O13_SummarizePlan", "Summarize Aunt Mali's feasible plan", q.TALK,
                  participant_tag=tag("VHV.Participant.AuntMali"),
                  conversation=conversations["DA_Conversation_HBCT_MaliSummary"],
                  entry_node_id="MaliPlan_01", auto_start=False),
        objective("O14_FinalDebrief", "Return to the Instructor for a final debrief", q.TALK,
                  "VHV.Story.Flag.HBCT.Motivation.Completed",
                  participant_tag=tag("VHV.Participant.Instructor"),
                  conversation=conversations["DA_Conversation_HBCT_FinalDebrief"],
                  entry_node_id="Final_01", auto_start=False),
    ]
    quests = [
        struct(unreal.VHVQuestDefinition,
               quest_id="Q_HBCT_00_INTRO",
               quest_title="Prologue — Arrival and Observation",
               quest_description="Meet the Instructor and observe why health knowledge does not always become action.",
               category=unreal.VHVQuestCategory.MAIN_STORY,
               objectives=intro,
               auto_track=True,
               auto_start_next_quest=True),
        struct(unreal.VHVQuestDefinition,
               quest_id="Q_HBCT_01_MOTIVATION",
               quest_title="Technique 1 — Building Motivation",
               quest_description="Help people discover personal reasons, obstacles, choices, and a feasible first plan.",
               category=unreal.VHVQuestCategory.MAIN_STORY,
               objectives=motivation,
               auto_track=True,
               auto_start_next_quest=False),
    ]
    arc.set_editor_properties({
        "quest_arc_id": "HBCT",
        "quest_arc_title": "Health Behavior Change Techniques",
        "quest_arc_description": "Production HBCT story arc. This milestone contains the prologue and Building Motivation.",
        "associated_level_data": level,
        "quests": quests,
        "auto_start_next_quest": True,
    })


def editor_actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def find_actor(label):
    for actor in editor_actors().get_all_level_actors():
        if actor.get_actor_label() == label:
            return actor
    return None


def spawn_actor(actor_class, label, location, rotation=None):
    actor = find_actor(label)
    if not actor:
        actor = editor_actors().spawn_actor_from_class(
            actor_class, unreal.Vector(*location), rotation or unreal.Rotator())
        if not actor:
            raise RuntimeError("Could not spawn {}".format(label))
        actor.set_actor_label(label)
    actor.modify()
    actor.set_actor_location(unreal.Vector(*location), False, True)
    actor.set_actor_rotation(rotation or unreal.Rotator(), False)
    return actor


def spawn_npc(npc_class, label, location, yaw, participant_tag=""):
    actor = spawn_actor(npc_class, label, location, unreal.Rotator(0.0, yaw, 0.0))
    component = actor.get_quest_participant_component()
    component.modify()
    component.set_editor_property("quest_participation_enabled", bool(participant_tag))
    component.set_editor_property("participant_tag", tag(participant_tag) if participant_tag else unreal.GameplayTag())
    return actor


def bind(slot_id, npc):
    return struct(unreal.VHVAmbientParticipantBinding, slot_id=slot_id, npc=npc)


def configure_ambient_actor(label, location, asset, receiver_tag, bindings):
    actor = spawn_actor(unreal.VHVAmbientConversationActor, label, location)
    actor.set_editor_properties({
        "conversation_data": asset,
        "participants": bindings,
        "auto_start_on_begin_play": False,
        "play_once": True,
        "player_leave_policy": unreal.VHVAmbientConversationLeavePolicy.CANCEL,
        "observation_radius": 1800.0,
    })
    receiver = actor.get_editor_property("world_action_receiver")
    receiver.modify()
    receiver.set_editor_properties({
        "world_actions_enabled": True,
        "receiver_tag": tag(receiver_tag),
    })
    return actor


def configure_location(label, location, location_tag):
    actor = spawn_actor(unreal.VHVQuestLocationVolume, label, location)
    actor.set_editor_properties({
        "location_tag": tag(location_tag),
        "enabled": True,
        "trigger_world_action": False,
    })
    box = actor.get_editor_property("box_component")
    box.set_box_extent(unreal.Vector(260.0, 260.0, 220.0), True)
    return actor


def create_blockout_prop(label, location, scale):
    actor = spawn_actor(unreal.StaticMeshActor, label, location)
    component = actor.get_editor_property("static_mesh_component")
    component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    actor.set_actor_scale3d(unreal.Vector(*scale))
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    return actor


def remove_development_ambient_actor():
    for actor in list(editor_actors().get_all_level_actors()):
        if actor.get_class() != unreal.VHVAmbientConversationActor.static_class():
            continue
        if actor.get_actor_label().startswith("HBCT_"):
            continue
        data = actor.get_editor_property("conversation_data")
        if data and data.get_path_name().startswith("/Game/VHV_Stuff/Dev/AmbientSpeech/"):
            unreal.log("{} Removing development ambient actor {} from production map".format(LOG, actor.get_actor_label()))
            editor_actors().destroy_actor(actor)


def configure_map(ambient_assets):
    if not unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH):
        raise RuntimeError("Could not load {}".format(MAP_PATH))
    remove_development_ambient_actor()

    npc_bp = unreal.EditorAssetLibrary.load_asset(NPC_BP_PATH)
    if not npc_bp:
        raise RuntimeError("Missing {}".format(NPC_BP_PATH))
    npc_class = npc_bp.generated_class()

    instructor = find_actor("HBCT_Intro_Instructor")
    if not instructor:
        candidates = [a for a in editor_actors().get_all_level_actors()
                      if a.get_class() == npc_class and not a.get_actor_label().startswith("HBCT_")]
        if candidates:
            instructor = candidates[0]
            instructor.set_actor_label("HBCT_Intro_Instructor")
    if instructor:
        instructor.set_actor_location(unreal.Vector(-1710.0, -1800.0, 100.0), False, True)
        instructor.set_actor_rotation(unreal.Rotator(0.0, -90.0, 0.0), False)
        participant = instructor.get_quest_participant_component()
        participant.set_editor_properties({
            "quest_participation_enabled": True,
            "participant_tag": tag("VHV.Participant.Instructor"),
        })
    else:
        instructor = spawn_npc(npc_class, "HBCT_Intro_Instructor", (-1710.0, -1800.0, 100.0), -90.0, "VHV.Participant.Instructor")
    spawn_actor(unreal.TargetPoint, "HBCT_OpeningCameraHook", (-1750.0, -2000.0, 180.0), unreal.Rotator(-8.0, 90.0, 0.0))

    market_positions = {
        "Buyer": (-950.0, -1650.0, 100.0, 0.0, "HBCT_Market_SugarDrinkShopper"),
        "Vendor": (-760.0, -1650.0, 100.0, 180.0, "HBCT_Market_Vendor"),
        "Smoker": (-1010.0, -1320.0, 100.0, 20.0, "HBCT_Market_Smoker"),
        "SweetTea": (-760.0, -1300.0, 100.0, 160.0, "HBCT_Market_SweetTeaVillager"),
        "Rider": (-520.0, -1510.0, 100.0, 90.0, "HBCT_Market_Motorbike"),
        "JunkFood": (-520.0, -1250.0, 100.0, -90.0, "HBCT_Market_JunkFoodVillager"),
    }
    market_npcs = {}
    for slot, (x, y, z, yaw, label) in market_positions.items():
        market_npcs[slot] = spawn_npc(npc_class, label, (x, y, z), yaw)
    configure_location("HBCT_Location_Market", (-780.0, -1470.0, 100.0), "VHV.Location.HBCT.Market")
    configure_ambient_actor(
        "HBCT_Market_SugarDrinkScene", (-780.0, -1470.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_MarketObservation"],
        "VHV.WorldReceiver.HBCT.MarketObservation",
        [bind(slot, market_npcs[slot]) for slot in ("Buyer", "Vendor", "Smoker", "SweetTea", "Rider", "JunkFood")])

    prasert = spawn_npc(npc_class, "HBCT_Motivation_Prasert", (0.0, -1120.0, 100.0), 150.0, "VHV.Participant.UnclePrasert")
    configure_location("HBCT_Location_PrasertHouse", (-80.0, -1300.0, 100.0), "VHV.Location.HBCT.PrasertHouse")
    configure_ambient_actor(
        "HBCT_Motivation_PrasertScene", (0.0, -1120.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_PrasertObservation"],
        "VHV.WorldReceiver.HBCT.Motivation.Prasert",
        [bind("Prasert", prasert)])
    create_blockout_prop("HBCT_Prop_Prasert_TV", (110.0, -1030.0, 70.0), (0.65, 0.12, 0.45))
    create_blockout_prop("HBCT_Prop_Prasert_SweetDrink", (-50.0, -1060.0, 55.0), (0.08, 0.08, 0.25))
    create_blockout_prop("HBCT_Prop_Prasert_SnackPackets", (-15.0, -1035.0, 48.0), (0.25, 0.16, 0.06))

    directive_vhv = spawn_npc(npc_class, "HBCT_Motivation_DirectiveVHV", (390.0, -760.0, 100.0), 0.0)
    directive_patient = spawn_npc(npc_class, "HBCT_Motivation_DirectivePatient", (560.0, -760.0, 100.0), 180.0)
    motivational_vhv = spawn_npc(npc_class, "HBCT_Motivation_MotivationalVHV", (390.0, -480.0, 100.0), 0.0)
    motivational_patient = spawn_npc(npc_class, "HBCT_Motivation_MotivationalPatient", (560.0, -480.0, 100.0), 180.0)
    configure_ambient_actor(
        "HBCT_Motivation_BadGoodDemo", (475.0, -620.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_BadGoodDemo"],
        "VHV.WorldReceiver.HBCT.Motivation.BadGoodDemo",
        [bind("DirectiveVHV", directive_vhv), bind("DirectivePatient", directive_patient),
         bind("MotivationalVHV", motivational_vhv), bind("MotivationalPatient", motivational_patient)])
    create_blockout_prop("HBCT_Motivation_BadDemo", (475.0, -850.0, 20.0), (0.9, 0.06, 0.2))
    create_blockout_prop("HBCT_Motivation_GoodDemo", (475.0, -390.0, 20.0), (0.9, 0.06, 0.2))

    saeng = spawn_npc(npc_class, "HBCT_Motivation_Saeng", (1010.0, -120.0, 100.0), 180.0, "VHV.Participant.AuntSaeng")
    saeng_vhv = spawn_npc(npc_class, "HBCT_Motivation_SaengVHV", (820.0, -120.0, 100.0), 0.0)
    configure_location("HBCT_Location_SaengHouse", (910.0, -300.0, 100.0), "VHV.Location.HBCT.SaengHouse")
    configure_ambient_actor(
        "HBCT_Motivation_SaengConversation", (910.0, -120.0, 100.0),
        ambient_assets["DA_Ambient_HBCT_SaengAppreciation"],
        "VHV.WorldReceiver.HBCT.Motivation.Saeng",
        [bind("Saeng", saeng), bind("VHV", saeng_vhv)])

    spawn_npc(npc_class, "HBCT_Motivation_Mali", (1450.0, 650.0, 100.0), 180.0, "VHV.Participant.AuntMali")
    configure_location("HBCT_Location_MaliHouse", (1260.0, 650.0, 100.0), "VHV.Location.HBCT.MaliHouse")
    create_blockout_prop("HBCT_Prop_Mali_IcedMilkTea", (1510.0, 600.0, 55.0), (0.08, 0.08, 0.25))
    create_blockout_prop("HBCT_Prop_Mali_Sweets", (1540.0, 635.0, 48.0), (0.28, 0.18, 0.06))

    if not unreal.EditorLevelLibrary.save_current_level():
        raise RuntimeError("Could not save {}".format(MAP_PATH))
    return instructor


def configure_player_controller(level, arc):
    bp = unreal.EditorAssetLibrary.load_asset(CONTROLLER_BP_PATH)
    if not bp:
        raise RuntimeError("Missing {}".format(CONTROLLER_BP_PATH))
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.modify()
    cdo.set_editor_properties({
        "use_quest_flow": True,
        "starting_level_data": level,
        "starting_quest_arc": arc,
    })
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save_asset(bp)


def validate_map_and_defaults(level, arc):
    required_labels = {
        "HBCT_Intro_Instructor",
        "HBCT_OpeningCameraHook",
        "HBCT_Market_SugarDrinkScene",
        "HBCT_Market_Smoker",
        "HBCT_Market_Motorbike",
        "HBCT_Motivation_Prasert",
        "HBCT_Motivation_BadDemo",
        "HBCT_Motivation_GoodDemo",
        "HBCT_Motivation_Saeng",
        "HBCT_Motivation_Mali",
        "HBCT_Location_Market",
        "HBCT_Location_PrasertHouse",
        "HBCT_Location_SaengHouse",
        "HBCT_Location_MaliHouse",
    }
    actors = editor_actors().get_all_level_actors()
    labels = {actor.get_actor_label() for actor in actors}
    missing = sorted(required_labels - labels)
    if missing:
        raise RuntimeError("Production map is missing actors: {}".format(missing))

    participant_tags = {}
    for actor in actors:
        if not isinstance(actor, unreal.VHVNPCCharacter):
            continue
        component = actor.get_quest_participant_component()
        if not component.get_editor_property("quest_participation_enabled"):
            continue
        value = str(unreal.GameplayTagLibrary.get_tag_name(
            component.get_editor_property("participant_tag")))
        if not value:
            raise RuntimeError("Enabled quest NPC {} has no participant tag".format(actor.get_actor_label()))
        if value in participant_tags:
            raise RuntimeError("Duplicate production participant tag {} on {} and {}".format(
                value, participant_tags[value], actor.get_actor_label()))
        participant_tags[value] = actor.get_actor_label()
    expected_participants = {
        "VHV.Participant.Instructor",
        "VHV.Participant.UnclePrasert",
        "VHV.Participant.AuntSaeng",
        "VHV.Participant.AuntMali",
    }
    if set(participant_tags) != expected_participants:
        raise RuntimeError("Unexpected production quest participant set: {}".format(participant_tags))

    validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    for actor in actors:
        if not actor.get_actor_label().startswith("HBCT_"):
            continue
        result, errors, warnings = validator.is_object_valid(actor, unreal.DataValidationUsecase.MANUAL)
        if result == unreal.DataValidationResult.INVALID:
            raise RuntimeError("Map actor validation rejected {}: {}".format(actor.get_actor_label(), errors))
        if warnings:
            unreal.log_warning("{} Map validation warnings for {}: {}".format(LOG, actor.get_actor_label(), warnings))

    bp = unreal.EditorAssetLibrary.load_asset(CONTROLLER_BP_PATH)
    cdo = unreal.get_default_object(bp.generated_class())
    if (not cdo.get_editor_property("use_quest_flow")
            or cdo.get_editor_property("starting_level_data") != level
            or cdo.get_editor_property("starting_quest_arc") != arc):
        raise RuntimeError("Production player-controller quest defaults are not configured")


def validate_assets(assets):
    validator = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    for asset in assets:
        result, errors, warnings = validator.is_object_valid(asset, unreal.DataValidationUsecase.MANUAL)
        if result == unreal.DataValidationResult.INVALID:
            raise RuntimeError("Validation rejected {}: {}".format(asset.get_path_name(), errors))
        if warnings:
            unreal.log_warning("{} Validation warnings for {}: {}".format(LOG, asset.get_path_name(), warnings))


def validate_arc_runtime(arc):
    game_instance = unreal.new_object(unreal.GameInstance)
    subsystem = unreal.new_object(unreal.VHVQuestSubsystem, outer=game_instance)
    if not subsystem.register_quest_arc(arc):
        raise RuntimeError("QuestSubsystem rejected DA_QuestArc_HBCT")


def validate_content_integrity(level, conversations, arc):
    topics = level.get_editor_property("day_data").get_editor_property("topics")
    authored_activities = topics[0].get_editor_property("activities") if len(topics) == 1 else []
    activity_ids = {
        str(unreal.GameplayTagLibrary.get_tag_name(item.get_editor_property("activity_tag"))).split(".")[-1]
        for item in authored_activities
    }
    if len(authored_activities) != 13 or len(activity_ids) != 13:
        raise RuntimeError("HBCT milestone must contain 13 uniquely addressed activities")

    placeholder = "HBCT production learning content."
    activities_by_id = {}
    for item in authored_activities:
        activity_id = str(unreal.GameplayTagLibrary.get_tag_name(
            item.get_editor_property("activity_tag"))).split(".")[-1]
        activities_by_id[activity_id] = item
        teaching_data = item.get_editor_property("teaching")
        visible_copy = [
            str(item.get_editor_property("narrative_context")),
            str(item.get_editor_property("prompt_text")),
            str(teaching_data.get_editor_property("title")),
            str(teaching_data.get_editor_property("content")),
        ] + [str(value) for value in teaching_data.get_editor_property("key_takeaways")]
        if any(placeholder in value for value in visible_copy):
            raise RuntimeError("Production placeholder remains in activity {}".format(activity_id))

        if item.get_editor_property("activity_type") == unreal.TextbookActivityType.SINGLE_CHOICE:
            policy = item.get_editor_property("attempt_policy")
            if (not policy.get_editor_property("enabled")
                    or policy.get_editor_property("max_attempts") != 2
                    or not str(policy.get_editor_property("first_incorrect_hint"))
                    or not str(policy.get_editor_property("final_incorrect_explanation"))):
                raise RuntimeError("Assessed HBCT choice {} lacks the two-attempt policy".format(activity_id))

    expected_categories = {
        "IntroLesson": unreal.TextbookTeachingCategory.LESSON,
        "TechniqueTitle": unreal.TextbookTeachingCategory.TECHNIQUE,
        "MotivationLesson": unreal.TextbookTeachingCategory.KEY_IDEA,
        "DemoReflection": unreal.TextbookTeachingCategory.REFLECTION,
        "Methods": unreal.TextbookTeachingCategory.LESSON,
    }
    for activity_id, expected_category in expected_categories.items():
        actual = activities_by_id[activity_id].get_editor_property("teaching").get_editor_property("category")
        if actual != expected_category:
            raise RuntimeError("{} has teaching category {}; expected {}".format(
                activity_id, actual, expected_category))

    market_evidence = activities_by_id["MarketObservation"].get_editor_property("evidence_tagging")
    if (not market_evidence.get_editor_property("use_evidence_tagging")
            or len(market_evidence.get_editor_property("evidence_cards")) != 7
            or len(market_evidence.get_editor_property("obstacle_cards")) != 6
            or len(market_evidence.get_editor_property("takeaway_cards")) != 3):
        raise RuntimeError("MarketObservation evidence-tagging content is incomplete")
    if activities_by_id["MarketObservation"].get_editor_property("media"):
        raise RuntimeError("MarketObservation must not expose answer media before evidence tagging")
    if activities_by_id["IntroLesson"].get_editor_property("evidence_tagging").get_editor_property("use_evidence_tagging"):
        raise RuntimeError("Passive Observation unexpectedly enables evidence tagging")

    intro_teaching = activities_by_id["IntroLesson"].get_editor_property("teaching")
    intro_media = intro_teaching.get_editor_property("media_texture")
    if (not intro_media
            or intro_media.get_path_name() != "/Game/VHV_Stuff/UI/Teaching/Media/T_UI_HBCT_WhyBehaviorChange.T_UI_HBCT_WhyBehaviorChange"
            or str(intro_teaching.get_editor_property("content")).strip()
            or intro_teaching.get_editor_property("key_takeaways")):
        raise RuntimeError("IntroLesson must use only the authored Why Behavior Change media body")

    for asset in conversations.values():
        conversation = asset.get_editor_property("conversation")
        nodes = conversation.get_editor_property("nodes")
        node_ids = {str(node.get_editor_property("node_id")) for node in nodes}
        if str(conversation.get_editor_property("start_node_id")) not in node_ids:
            raise RuntimeError("{} has an invalid start node".format(asset.get_name()))
        for node in nodes:
            next_id = str(node.get_editor_property("next_node_id"))
            if next_id and next_id not in node_ids:
                raise RuntimeError("{} node {} points to missing node {}".format(
                    asset.get_name(), node.get_editor_property("node_id"), next_id))
            if node.get_editor_property("node_type") == unreal.VHVDialogueNodeType.LEARNING_ACTIVITY:
                linked = node.get_editor_property("linked_activity").get_editor_property("activity_tag")
                linked_id = str(unreal.GameplayTagLibrary.get_tag_name(linked)).split(".")[-1]
                if linked_id not in activity_ids:
                    raise RuntimeError("{} links missing activity {}".format(asset.get_name(), linked_id))

    learning_conversation = conversations["DA_Conversation_HBCT_LearnMotivation"].get_editor_property("conversation")
    thought_nodes = [node for node in learning_conversation.get_editor_property("nodes")
                     if str(node.get_editor_property("node_id")) == "Learn_02"]
    if (len(thought_nodes) != 1
            or thought_nodes[0].get_editor_property("presentation") != unreal.VHVDialoguePresentation.THOUGHT):
        raise RuntimeError("Learn_02 must be explicitly authored as a player Thought")

    quests = arc.get_editor_property("quests")
    expected = {
        "Q_HBCT_00_INTRO": [
            "O01_MeetInstructor", "O02_ReachMarket", "O03_ObserveMarket",
            "O04_ReportToInstructor", "O05_IntroLesson"],
        "Q_HBCT_01_MOTIVATION": [
            "O01_ReachPrasertHouse", "O02_ObservePrasert", "O03_LearnMotivation",
            "O04_ObserveBadGoodDemo", "O05_ReflectOnDemo", "O06_VisitSaeng",
            "O07_ObserveAppreciation", "O08_LearnMotivationMethods", "O09_ReachMaliHouse",
            "O10_FindWhatMatters", "O11_FindObstacle", "O12_OfferSupportiveInformation",
            "O13_SummarizePlan", "O14_FinalDebrief"],
    }
    if len(quests) != 2:
        raise RuntimeError("HBCT milestone must contain exactly two quests")
    for quest in quests:
        quest_id = str(quest.get_editor_property("quest_id"))
        objective_ids = [str(item.get_editor_property("objective_id"))
                         for item in quest.get_editor_property("objectives")]
        if objective_ids != expected.get(quest_id):
            raise RuntimeError("Unexpected objective sequence for {}: {}".format(quest_id, objective_ids))


def validate_attempt_runtime(level):
    game_instance = unreal.new_object(unreal.GameInstance)
    subsystem = unreal.new_object(unreal.VHVTextbookSubsystem, outer=game_instance)
    subsystem.start_journey(level)

    if not subsystem.start_activity_by_id("DemoReflection"):
        raise RuntimeError("Could not start DemoReflection for retry verification")
    subsystem.submit_answer(False, False)
    state = subsystem.get_runtime_state()
    if state.get_editor_property("current_phase") != unreal.LearningPhase.HINT or state.get_editor_property("attempt_count") != 1:
        raise RuntimeError("First wrong answer did not enter Hint at attempt 1")
    subsystem.retry_current_activity_after_hint()
    if subsystem.get_current_phase() != unreal.LearningPhase.ASK:
        raise RuntimeError("Hint acknowledgement did not return to Ask")
    subsystem.submit_answer(False, False)
    state = subsystem.get_runtime_state()
    if state.get_editor_property("current_phase") != unreal.LearningPhase.FEEDBACK or state.get_editor_property("attempt_count") != 2:
        raise RuntimeError("Second wrong answer did not enter terminal Feedback at attempt 2")

    if not subsystem.start_activity_by_id("DemoReflection"):
        raise RuntimeError("Could not restart DemoReflection")
    subsystem.submit_answer(False, False)
    subsystem.retry_current_activity_after_hint()
    subsystem.submit_answer(True, False)
    state = subsystem.get_runtime_state()
    if (state.get_editor_property("current_phase") != unreal.LearningPhase.FEEDBACK
            or not state.get_editor_property("answer_correct")
            or state.get_editor_property("attempt_count") != 2):
        raise RuntimeError("Correct retry did not enter normal positive Feedback")


def validate_observation_runtime(level):
    game_instance = unreal.new_object(unreal.GameInstance)
    subsystem = unreal.new_object(unreal.VHVTextbookSubsystem, outer=game_instance)
    subsystem.start_journey(level)
    subsystem.set_progression_mode(unreal.VHVTextbookProgressionMode.QUEST_MANAGED)

    if not subsystem.start_activity_by_id("IntroLesson") or not subsystem.submit_observation():
        raise RuntimeError("Passive Observation completion path failed")
    passive_state = subsystem.get_runtime_state()
    if (passive_state.get_editor_property("current_phase") != unreal.LearningPhase.FEEDBACK
            or not passive_state.get_editor_property("activity_active")):
        raise RuntimeError("Passive Observation behavior changed")

    if not subsystem.start_activity_by_id("MarketObservation") or not subsystem.submit_observation():
        raise RuntimeError("Evidence-tagging Observation completion path failed")
    evidence_state = subsystem.get_runtime_state()
    if (evidence_state.get_editor_property("activity_active")
            or not evidence_state.get_editor_property("activity_completed")):
        raise RuntimeError("Evidence-tagging Observation did not complete directly")
    if subsystem.submit_observation():
        raise RuntimeError("Evidence-tagging Observation completed more than once")


def run():
    for directory in (ROOT, ACTIVITY_DIR, CONVERSATION_DIR, AMBIENT_DIR, QUEST_DIR):
        ensure_directory(directory)
    level = load_or_create(ACTIVITY_DIR, "DA_Level_HBCT", unreal.VHVLevelData)
    arc = load_or_create(QUEST_DIR, "DA_QuestArc_HBCT", unreal.VHVQuestArcData)
    configure_level(level)
    conversations = create_conversations()
    ambient_assets = create_ambient_assets()
    configure_arc(arc, level, conversations)

    all_assets = [level] + list(conversations.values()) + list(ambient_assets.values()) + [arc]
    validate_content_integrity(level, conversations, arc)
    validate_attempt_runtime(level)
    validate_observation_runtime(level)
    validate_arc_runtime(arc)
    validate_assets(all_assets)
    for asset in all_assets:
        save_asset(asset)
    configure_map(ambient_assets)
    configure_player_controller(level, arc)
    validate_map_and_defaults(level, arc)
    unreal.log("{} Authored {} activities, {} conversations, {} ambient sequences, 2 quests, and the production level bindings.".format(
        LOG, len(build_activities()), len(conversations), len(ambient_assets)))


if __name__ == "__main__":
    try:
        run()
    except Exception:
        unreal.log_error("{} Failed:\n{}".format(LOG, traceback.format_exc()))
        raise
